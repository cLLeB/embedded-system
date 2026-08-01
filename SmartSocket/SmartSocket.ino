// =====================================================================
//  Smart Socket with Adaptive Time-of-Use Scheduling
//
//  This file is only the composition root: it constructs the hardware objects
//  and injects them into the logic in src/core/, which contains no Arduino
//  code and is unit-tested on a PC. Read src/core/SocketController.cpp for the
//  state machine and src/core/ChargeAnalyzer.cpp for the algorithm.
//
//  SETUP: see README.md. Requires the LiquidCrystal_I2C library.
// =====================================================================

#include <avr/wdt.h>

#include "src/core/ButtonDebouncer.h"
#include "src/core/BuzzerDriver.h"
#include "src/core/Config.h"
#include "src/core/SocketController.h"
#include "src/core/Types.h"
#include "src/core/UiPresenter.h"
#include "src/hal/HalCurrent.h"
#include "src/hal/HalDisplay.h"
#include "src/hal/HalIo.h"
#include "src/hal/HalPins.h"
#include "src/hal/HalProfileStore.h"

using namespace smartsocket;

// ---------------------------------------------------------------------
//  SENSOR MODE  -- the one setting you are most likely to change.
//
//    1 = AC true RMS. For the 230 V mains build: plug -> rocker -> relay ->
//        ACS712 -> wall socket. THIS IS WHAT THIS PROJECT NOW RUNS.
//    0 = DC mean.     For the old low-voltage bench rig (18650 + resistor bank),
//        kept so that rig can still be rebuilt.
//
//  This is not cosmetic. AC current is a sine wave that averages to about zero
//  whatever the load, so DC mode on mains reads an empty socket while a laptop
//  charges. Match it to what is actually wired.
//
//  (The #ifndef guard only exists so both paths can be build-tested; in the IDE
//  just change the number.)
// ---------------------------------------------------------------------
#ifndef SENSOR_MODE_AC
#define SENSOR_MODE_AC 1
#endif

// --- hardware ---
HalClock g_clock;
HalRelay g_relay(pins::Relay, pins::RelayActiveLow);
HalBuzzer g_buzzer(pins::Buzzer);
HalButtons g_buttons(pins::ButtonNext, pins::ButtonAction);
HalDisplay g_display(config::LcdI2cAddress, config::LcdColumns, config::LcdRows);
HalProfileStore g_store(0);

#if SENSOR_MODE_AC
AcRmsCurrentSensor g_sensor(pins::CurrentSensor);
#else
DcCurrentSensor g_sensor(pins::CurrentSensor);
#endif

// --- logic (no hardware knowledge) ---
SocketController g_controller(g_clock, g_sensor, g_relay, g_store);
ButtonBank g_buttonBank(g_buttons);
BuzzerDriver g_buzzerDriver(g_buzzer);
UiPresenter g_ui;

static Millis g_lastLcdMs = 0;

static void refreshDisplay(bool force) {
  const Millis now = g_clock.now();
  if (!force && (now - g_lastLcdMs) < config::LcdRefreshMs) {
    return;
  }
  g_lastLcdMs = now;

  char line0[LineBufferSize];
  char line1[LineBufferSize];
  g_ui.render(g_controller.status(), line0, line1);
  g_display.showLine(0, line0);
  g_display.showLine(1, line1);
}

void setup() {
  // Relay first, and de-energized: never leave the load powered while the rest of
  // the system is still coming up.
  g_relay.begin();
  g_buzzer.begin();
  g_buttons.begin();
  g_display.begin();

  g_display.showLine(0, "Smart Socket    ");
  g_display.showLine(1, "Calibrating...  ");

  // Power-on chirp. It is the only thing that proves the buzzer works, because
  // all three patterns the socket plays in normal use - plug detected, cutoff,
  // fault - need a real load. Without this a dead or wrongly-driven buzzer stays
  // hidden until the one moment it has to be heard.
  g_buzzer.setSounding(true);
  delay(config::BootChirpMs);
  g_buzzer.setSounding(false);

  // Measures the sensor's zero point with the relay open, loads the learned
  // profile, then arms the socket.
  g_controller.begin();

  refreshDisplay(true);

  // LAST THING IN setup(), so a slow start cannot trip it.
  //
  // When loop() stops running, pin 7 holds its last level - so a hang mid-charge
  // or mid-probe leaves the relay CLOSED. Power stuck on, display frozen on
  // whatever it last drew. On a socket whose whole purpose is cutting power, a
  // lockup that fails ON is the worst failure there is, and this build has
  // already demonstrated one.
  //
  // The watchdog turns that into a 4 second glitch: the chip resets, and
  // HalRelay::begin() opens the relay before anything else runs.
  //
  // 4 s, not the shortest available, on purpose. A watchdog reset re-enters the
  // bootloader, and older bootloaders neither clear the reset flag nor disable
  // the timer - with a short period that loops forever and bricks the board
  // until it is reflashed. Any bootloader finishes well inside 4 s.
  wdt_enable(WDTO_4S);
}

void loop() {
  // Every path through loop() must reach here. The AC RMS window blocks for
  // 60 ms and the I2C writes can now stall for up to 25 ms before timing out,
  // which together are two orders of magnitude inside the 4 s budget.
  wdt_reset();

  const Millis now = g_clock.now();

  g_buttonBank.update(now);

  if (g_buttonBank.event(Button_Next) == ButtonEvent_ShortPress) {
    g_ui.nextScreen();
    refreshDisplay(true);
  }

  const ButtonEvent action = g_buttonBank.event(Button_Action);
  if (action != ButtonEvent_None) {
    g_controller.onButton(Button_Action, action);
    refreshDisplay(true);
  }

  g_controller.update();

  BuzzerPattern alert;
  if (g_controller.takeAlert(alert)) {
    if (alert == Buzz_Silent) {
      g_buzzerDriver.silence(now);
    } else {
      g_buzzerDriver.play(alert, now);
    }
  }
  g_buzzerDriver.update(now);

  refreshDisplay(false);
}
