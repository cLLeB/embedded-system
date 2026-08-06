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
#include "src/hal/HalTelemetry.h"

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

// ---------------------------------------------------------------------
//  FITTED PERIPHERALS  -- set each to 1 as the part actually goes in.
//
//  The socket is a complete product with none of these: the sensor, the relay
//  and the LCD are the whole safety-critical path, and cutting power at full
//  charge needs nothing else. Buttons, buzzer and radio are how a person hears
//  about it and talks back, so each is compiled in only once it exists.
//
//  What each 0 costs, and what stands in for it:
//
//    HAS_BUTTONS 0 - no NEXT, so the three LCD screens rotate on a timer
//                    (config::UiAutoCycleMs); no ACTION, so re-arm / cut / probe
//                    come over the serial link instead, which is why the
//                    telemetry object is built either way. Nothing is lost: the
//                    cutoff itself is automatic and the recovery probe is on a
//                    timer, so an untouched socket still does its whole job.
//
//    HAS_BUZZER  0 - cutoffs and faults are silent. They are still on the LCD
//                    and still in the telemetry, so nothing is unreported, but a
//                    fault you are not in the room for will not call you.
//
//    HAS_BLUETOOTH 0 - the same telemetry goes out of the USB port instead of
//                    the HC-05, at config::TelemetryBaud. Open the Arduino IDE's
//                    Serial Monitor at 9600 with line ending "Newline" and it is
//                    the control panel: C, R, P, ? one letter per line. A short
//                    human banner is printed at boot; it is compiled out when the
//                    radio is fitted, so the phone only ever sees clean S,... lines.
//
//  WHEN THE HC-05 GOES ON: it lands on pins 0/1, the same UART the USB port
//  uses. Uploads fail with it wired. Unplug its TX/RX for every upload.
// ---------------------------------------------------------------------
#ifndef HAS_BUTTONS
#define HAS_BUTTONS 0
#endif

#ifndef HAS_BUZZER
#define HAS_BUZZER 0
#endif

#ifndef HAS_BLUETOOTH
#define HAS_BLUETOOTH 1
#endif

// --- hardware ---
HalClock g_clock;
HalRelay g_relay(pins::Relay, pins::RelayActiveLow);
HalDisplay g_display(config::LcdI2cAddress, config::LcdColumns, config::LcdRows);
HalProfileStore g_store(0);
HalTelemetry g_telemetry(config::TelemetryBaud);

#if HAS_BUZZER
HalBuzzer g_buzzer(pins::Buzzer);
#endif

#if HAS_BUTTONS
HalButtons g_buttons(pins::ButtonNext, pins::ButtonAction);
#endif

#if SENSOR_MODE_AC
AcRmsCurrentSensor g_sensor(pins::CurrentSensor);
#else
DcCurrentSensor g_sensor(pins::CurrentSensor);
#endif

// --- logic (no hardware knowledge) ---
SocketController g_controller(g_clock, g_sensor, g_relay, g_store);
UiPresenter g_ui;

#if HAS_BUTTONS
ButtonBank g_buttonBank(g_buttons);
#endif

#if HAS_BUZZER
BuzzerDriver g_buzzerDriver(g_buzzer);
#endif

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
#if HAS_BUZZER
  g_buzzer.begin();
#endif
#if HAS_BUTTONS
  g_buttons.begin();
#endif
  g_display.begin();
  g_telemetry.begin();

  g_display.showLine(0, "Smart Socket    ");
  g_display.showLine(1, "Calibrating...  ");

#if HAS_BUZZER
  // Power-on chirp. It is the only thing that proves the buzzer works, because
  // all three patterns the socket plays in normal use - plug detected, cutoff,
  // fault - need a real load. Without this a dead or wrongly-driven buzzer stays
  // hidden until the one moment it has to be heard.
  g_buzzer.setSounding(true);
  delay(config::BootChirpMs);
  g_buzzer.setSounding(false);
#endif

#if !HAS_BLUETOOTH
  // Printed once, before any status line, so whoever opens the Serial Monitor
  // knows the link is two-way. Compiled out when the HC-05 is fitted: the phone
  // parses S,... lines positionally and would have to be taught to skip this.
  Serial.println(F("Smart Socket ready."));
  Serial.println(F("C=cut R=rearm P=probe ?=status"));
#endif

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

#if HAS_BUTTONS
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
#else
  // NO SLIDESHOW. With no NEXT button fitted the display stays on the status
  // screen and does not rotate.
  //
  // It used to cycle through status, detail and stats on a timer, on the
  // reasoning that otherwise the other two would be unreachable. That was
  // reasoning about the code rather than about the person standing at the
  // socket: the threshold, the taper ratio and the lifetime cutoff count are
  // diagnostics, and rotating them through the one line that says whether mains
  // is live made the display worse for everybody. Whoever needs them can read
  // them in the app, or fit the buttons and press NEXT.
  //
  // The detail and stats screens are still built and still tested - they are
  // one button press away the moment HAS_BUTTONS becomes 1.
#endif

  // Remote commands are read before update() so a "cut now" from the phone acts
  // on this tick rather than the next, and routed through the controller so they
  // obey exactly the same transitions the buttons do.
  //
  // With no radio and no ACTION button this is the only way to talk to the
  // socket, which is why it is never compiled out: the USB port is the panel.
  const RemoteCommand remote = g_telemetry.takeCommand();
  if (remote != Remote_None) {
    g_controller.onRemote(remote);
    refreshDisplay(true);
  }

  // The client's own battery level, for the screen only. Taken every tick
  // rather than only when a command arrives, because it changes on its own
  // schedule and the display should follow it without being asked.
  const int16_t battery = g_telemetry.takeBatteryPercent();
  if (battery >= 0) {
    g_ui.setBatteryPercent(battery);
    refreshDisplay(true);
  }

  const int16_t limit = g_telemetry.takeBatteryLimit();
  if (limit >= 0) {
    g_ui.setBatteryLimit(limit);
    refreshDisplay(true);
  }

  // The one fact the sensor cannot supply. A phone charging draws about 20 mA,
  // below a single ADC count, so without this the display says "Ready - waiting
  // for something to be plugged in" over an outlet that is charging one.
  const int8_t charging = g_telemetry.takeClientCharging();
  if (charging >= 0) {
    g_ui.setClientCharging(charging == 1);
    refreshDisplay(true);
  }

  g_controller.update();

  g_telemetry.publish(g_controller.status(), now);

#if HAS_BUZZER
  BuzzerPattern alert;
  if (g_controller.takeAlert(alert)) {
    if (alert == Buzz_Silent) {
      g_buzzerDriver.silence(now);
    } else {
      g_buzzerDriver.play(alert, now);
    }
  }
  g_buzzerDriver.update(now);
#endif

  refreshDisplay(false);
}
