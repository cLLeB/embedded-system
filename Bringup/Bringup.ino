// =====================================================================
//  Bringup.ino - Smart Socket, simple version
//
//  Runs on the Stage 1 wiring: sensor on A0, buttons on D2/D3, relay on
//  D7, buzzer on D8. NO SCREEN NEEDED. Everything is reported over the
//  USB cable instead, so you can build and test before the LCD backpack
//  arrives.
//
//  One file. No libraries to install. Nothing to configure.
//
//  TO USE:
//    1. Upload.
//    2. Tools -> Serial Monitor.
//    3. Set the speed box (bottom right) to 9600 baud.
//
//  The full layered firmware lives in ../SmartSocket/. This is the plain
//  version: same behaviour, no EEPROM learning, no screen.
// =====================================================================

// ---------------------------------------------------------------------
//  PINS - these match the Stage 1 wiring exactly.
// ---------------------------------------------------------------------
const uint8_t PIN_SENSOR     = A0;  // R2 "OUT"
const uint8_t PIN_BTN_NEXT   = 2;   // square one
const uint8_t PIN_BTN_ACTION = 3;   // square two
const uint8_t PIN_RELAY      = 7;   // R1 "IN"
const uint8_t PIN_BUZZER     = 8;   // via resistor to L2 long leg

// Most cheap relay boards switch on when the pin goes LOW. If yours turns
// out backwards - it clicks OFF at startup instead of ON - change this to
// false and re-upload. Nothing else needs to change.
const bool RELAY_ACTIVE_LOW = true;

// ---------------------------------------------------------------------
//  NUMBERS YOU MIGHT TUNE
// ---------------------------------------------------------------------
const int  SENSOR_MV_PER_AMP = 185;    // ACS712 5A model. See "CALIBRATING" below.

// Measurements are averaged over a whole number of 50 Hz mains cycles (20 ms
// each). Mains hum couples into the sensor's leads - its open screw terminals
// are effectively a small antenna - and over a whole cycle the positive and
// negative halves cancel exactly. Average over an arbitrary window instead and
// a residue survives, which drifts and looks like a real current.
//
// MUST stay a whole multiple of 20 ms. Longer is quieter but blocks the button
// polling for that long, so 40 ms is the compromise.
const unsigned int AVG_WINDOW_MS = 40;   // 2 mains cycles
const unsigned int CAL_WINDOW_MS = 500;  // 25 cycles, for the startup zero

// ---------------------------------------------------------------------------
//  THE LOAD IS A STAIRCASE, AND EVERY THRESHOLD BELOW SITS BETWEEN TWO STEPS.
//
//  The "device" is BANK_RESISTORS identical resistors in parallel across one
//  18650, and the charge is made to taper by pulling them out. Measured on this
//  kit: 110 ohm +/-1%, so each one draws 3.7 V / 110 = 33.6 mA and the current
//  can only ever land on a multiple of it:
//
//    resistors in |  1    2    3    4    5    6    7    8    9   10   11
//    current (mA) | 34   67  101  134  168  202  235  269  302  336  370
//
//  One ADC count is 5000 mV / 1024 / 185 mV-per-A = 26 mA - most of a whole
//  step. So a threshold placed ON a step is a coin toss, and the offset drift
//  seen on this hardware is itself several steps wide. Each number below is
//  placed near the middle of a gap. Check the table before changing any of them.
// ---------------------------------------------------------------------------
const int  BANK_RESISTORS    = 11;     // how many are in the load bank
const long PER_RESISTOR_MA   = 34;     // 3.7 V / 110 ohm; only used before a peak exists

const long NOISE_FLOOR_MA    = 60;     // below this, call it zero (under step 2)
const long PLUG_DETECT_MA    = 120;    // above this, something is plugged in (steps 3|4)
// Below the noise floor on purpose: only a reading of exactly zero counts as an
// unplug. Was 80 mA, which sat 13 mA under the 3-resistor step - so the demo's
// own end state was one count of drift away from being read as "unplugged",
// which abandons the session and goes quietly back to READY.
const long UNPLUG_MA         = 50;     // below this, it has been unplugged
const long TAPER_FLOOR_MA    = 115;    // cutoff level never goes below this (steps 3|4)
// 250, not 300: the full bank is 370 mA nominal, but a cell at 3.4 V or a sensor
// reading 15% low puts it near 300. Falling under this does not just mis-tune the
// demo, it takes the trickle branch - power cut at 60 s with no taper and no 90 s
// confirmation, which from the outside looks like a real cutoff.
const long MIN_PEAK_MA       = 250;    // a real charge pulls at least this much
const long OVERCURRENT_MA    = 4500;   // fault: sensor maxes out at 5000
// 49%, matching the full firmware in ../SmartSocket (38% learned x 130% margin).
// Puts the threshold at 181 mA with a full bank - almost exactly halfway between
// step 5 (168) and step 6 (202), the furthest from both it can sit.
const int  TAPER_PCT         = 49;     // cut when current falls under this % of peak

const unsigned long SAMPLE_MS        = 250;      // how often we measure
const unsigned long SETTLE_MS        = 5000;     // ignore the first 5s after plug-in
const unsigned long MIN_CHARGE_MS    = 60000UL;  // never cut off in the first minute
const unsigned long TAPER_CONFIRM_MS = 90000UL;  // current must stay low this long
const unsigned long UNPLUG_CONFIRM_MS= 3000;     // load must be gone this long
const unsigned long PRINT_MS         = 1000;     // how often we report
// Asymmetric on purpose. A press is accepted quickly so taps feel instant,
// but a release must be stable for much longer before it counts.
//
// Cheap tactile switches on a breadboard chatter: contact drops out for a few
// milliseconds at a time while you are holding them down. With a symmetric
// 30 ms filter every dropout read as a release and restarted the long-press
// timer, so a 1.5 s hold could take ten seconds of wrestling to register - or
// never register at all. Ignoring any gap shorter than RELEASE_FILTER_MS makes
// a hold accumulate properly through the chatter.
const unsigned long PRESS_FILTER_MS   = 25;
const unsigned long RELEASE_FILTER_MS = 120;
const unsigned long LONG_PRESS_MS     = 1500;

// ---------------------------------------------------------------------
//  STATE
// ---------------------------------------------------------------------
enum State {
  READY,       // power on, nothing plugged in
  SETTLE,      // load just appeared, waiting for it to steady
  CHARGING,    // measuring, watching for the drop
  CUT,         // full - power cut, waiting for the user
  MANUAL_OFF,  // user forced power off
  FAULT        // overcurrent
};

State state = READY;

int  zeroCounts   = 512;  // measured at startup
long currentMa    = 0;
long peakMa       = 0;
long thresholdMa  = 0;

unsigned long stateSince   = 0;  // when we entered the current state
unsigned long chargeStart  = 0;  // when CHARGING began
unsigned long lowSince     = 0;  // when current first dropped below threshold (0 = not low)
unsigned long goneSince    = 0;  // when the load first vanished (0 = still there)
unsigned long lastSample   = 0;
unsigned long lastPrint    = 0;
unsigned long cutoffCount  = 0;

// ---------------------------------------------------------------------
//  RELAY
// ---------------------------------------------------------------------
void powerOn() {
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? LOW : HIGH);
}

void powerOff() {
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_LOW ? HIGH : LOW);
}

// ---------------------------------------------------------------------
//  BUZZER
//
//  Driven with tone() rather than a plain HIGH. A PASSIVE buzzer is just a
//  coil and needs a switching signal - a steady voltage does nothing but
//  click. An ACTIVE buzzer has its own oscillator, but still sounds when fed
//  a square wave. So tone() drives both types, whereas digitalWrite only
//  drives active ones. The two look identical from the outside, so this
//  removes the guesswork.
//
//  tone() uses Timer2. millis() uses Timer0, so there is no clash.
// ---------------------------------------------------------------------
// Measured on the actual buzzer with BuzzerTest.ino: 1500 Hz was clearly the
// loudest, 2400 Hz was faint, and 3000 Hz and above were inaudible. A passive
// buzzer is a resonant device - a few hundred Hz off its peak is the difference
// between a clear beep and nothing at all. Do not "round this to a nicer
// number" without re-running BuzzerTest.
const unsigned int BUZZER_TONE_HZ = 1500;

void beep(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    tone(PIN_BUZZER, BUZZER_TONE_HZ);
    delay(onMs);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
    if (i < times - 1) delay(offMs);
  }
}

// ---------------------------------------------------------------------
//  BUTTONS
//
//  Wired to GND and using the chip's internal pull-up resistors, so a
//  pressed button reads LOW. That is why there are no resistors on them.
// ---------------------------------------------------------------------
// poll() is a member rather than a free function on purpose. The Arduino IDE
// auto-generates prototypes for free functions and inserts them ABOVE this
// struct, so a free `pollButton(Button&, ...)` fails to compile with
// "'Button' was not declared in this scope". Members are immune to that.
struct Button {
  uint8_t pin;
  bool    down;       // the debounced state we act on
  bool    longFired;  // long press already reported for this hold
  bool    pending;    // raw disagrees with `down`, waiting out the filter
  unsigned long edgeAt;   // when raw first started disagreeing
  unsigned long downAt;   // when the debounced press began

  // Adopt whatever the pin reads right now as the starting state, and mark any
  // already-held button as having spent its long press. Without this, a button
  // that is stuck LOW at boot fires a long press 1.5 s later all by itself -
  // which looks exactly like a mysterious spontaneous power-off.
  void begin(unsigned long now) {
    down = (digitalRead(pin) == LOW);
    longFired = down;
    pending = false;
    edgeAt = now;
    downAt = now;
  }

  bool stuck() const { return digitalRead(pin) == LOW; }

  // Returns 1 on release-after-short-press (a tap), 2 the moment a long
  // press is reached, 0 otherwise.
  int poll(unsigned long now) {
    bool raw = (digitalRead(pin) == LOW);

    if (raw == down) {
      pending = false;          // settled again, forget any part-formed edge
    } else {
      if (!pending) {
        pending = true;
        edgeAt = now;
      }
      unsigned long needed = raw ? PRESS_FILTER_MS : RELEASE_FILTER_MS;
      if (now - edgeAt >= needed) {
        pending = false;
        down = raw;
        if (down) {
          downAt = now;
          longFired = false;
        } else if (!longFired) {
          return 1;             // a tap: released before the long-press point
        }
      }
    }

    if (down && !longFired && (now - downAt >= LONG_PRESS_MS)) {
      longFired = true;
      return 2;
    }
    return 0;
  }
};

Button btnNext   = { PIN_BTN_NEXT,   false, false, false, 0, 0 };
Button btnAction = { PIN_BTN_ACTION, false, false, false, 0, 0 };

// ---------------------------------------------------------------------
//  CURRENT MEASUREMENT
//
//  The sensor sits at half its supply with no current flowing, and moves
//  up or down from there depending on which way the current runs. So we
//  measure that resting point once at startup (with the relay open, so it
//  really is zero) and everything after is the difference from it.
// ---------------------------------------------------------------------
// Samples flat out for windowMs, however many readings that turns out to be.
// Timing the window rather than counting samples is the whole point: it is what
// guarantees a whole number of mains cycles regardless of how fast analogRead
// happens to be.
int readRawAverage(unsigned int windowMs) {
  unsigned long total = 0;
  unsigned long count = 0;
  unsigned long window = (unsigned long)windowMs * 1000UL;
  unsigned long start = micros();

  while (micros() - start < window) {
    total += analogRead(PIN_SENSOR);
    count++;
  }
  if (count == 0) return analogRead(PIN_SENSOR);  // cannot happen, but never divide by zero
  return (int)(total / count);
}

long readCurrentMa() {
  int raw = readRawAverage(AVG_WINDOW_MS);
  int diff = raw - zeroCounts;
  if (diff < 0) diff = -diff;  // direction does not matter, only size

  // counts -> millivolts -> milliamps, in two steps so nothing overflows
  long mv = (long)diff * 5000L / 1024L;
  long ma = mv * 1000L / (long)SENSOR_MV_PER_AMP;

  if (ma < NOISE_FLOOR_MA) ma = 0;  // too small to tell from noise
  return ma;
}

// ---------------------------------------------------------------------
//  REPORTING
// ---------------------------------------------------------------------
const char *stateName() {
  switch (state) {
    case READY:      return "READY";
    case SETTLE:     return "SETTLE";
    case CHARGING:   return "CHARGING";
    case CUT:        return "FULL - POWER CUT";
    case MANUAL_OFF: return "MANUAL OFF";
    case FAULT:      return "! OVERCURRENT !";
  }
  return "?";
}

// How many resistors the reading implies are still in the bank.
//
// Worked out from the session peak rather than from PER_RESISTOR_MA, so it stays
// right even when the sensor calibration or the cell voltage is off - both of
// those scale the whole staircase, and dividing by the peak cancels them out.
//
// This is the number that makes the demo debuggable: if the serial line says 5
// while there are 7 resistors in the board, the reading is wrong, not the logic.
int resistorsLeft(long ma) {
  long stepMa = (peakMa > 0) ? (peakMa / BANK_RESISTORS) : PER_RESISTOR_MA;
  if (stepMa <= 0) return 0;
  return (int)((ma + stepMa / 2) / stepMa);
}

void printAmps(long ma) {
  Serial.print(ma / 1000);
  Serial.print('.');
  long frac = (ma % 1000) / 10;
  if (frac < 10) Serial.print('0');
  Serial.print(frac);
  Serial.print('A');
}

void report(unsigned long now) {
  Serial.print('[');
  Serial.print(stateName());
  Serial.print("]  now ");
  printAmps(currentMa);

  if (state == CHARGING || state == SETTLE) {
    Serial.print("  (~");
    Serial.print(resistorsLeft(currentMa));
    Serial.print(" of ");
    Serial.print(BANK_RESISTORS);
    Serial.print(" resistors)");
  }

  if (state == CHARGING) {
    Serial.print("   peak ");
    printAmps(peakMa);
    Serial.print("   cut below ");
    printAmps(thresholdMa);
    Serial.print("   running ");
    Serial.print((now - chargeStart) / 1000);
    Serial.print('s');

    // The trickle branch cuts power at 60 s with no taper and no confirmation,
    // which is indistinguishable from a real cutoff unless it is called out here.
    if (peakMa < MIN_PEAK_MA) {
      Serial.print(F("   *** PEAK TOO LOW - will be cut as a trickle at 60s."
                     " Add resistors. ***"));
    }
    if (lowSince != 0) {
      Serial.print("   LOW for ");
      Serial.print((now - lowSince) / 1000);
      Serial.print("s of ");
      Serial.print(TAPER_CONFIRM_MS / 1000);
      Serial.print('s');
    }
  }
  Serial.println();
}

void enter(State s, unsigned long now) {
  state = s;
  stateSince = now;
}

// ---------------------------------------------------------------------
//  SETUP
// ---------------------------------------------------------------------
void setup() {
  // Relay first, before anything else, so a reset never leaves it closed.
  pinMode(PIN_RELAY, OUTPUT);
  powerOff();

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_BTN_NEXT,   INPUT_PULLUP);
  pinMode(PIN_BTN_ACTION, INPUT_PULLUP);

  Serial.begin(9600);
  while (!Serial) { }  // harmless on an Uno, needed on some clones

  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F(" Smart Socket - simple version"));
  Serial.println(F("================================================"));
  Serial.println(F(" A0 sensor   D2 button1   D3 button2"));
  Serial.println(F(" D7 relay    D8 buzzer"));
  Serial.println();

  // Close the relay FIRST, then measure zero.
  //
  // The relay coil draws ~70 mA, and that current flows through the shared
  // ground wiring on the breadboard. The small voltage drop it creates shifts
  // the sensor's ground relative to the Arduino's, and the ADC reads that shift
  // as current. Calibrating with the coil de-energised and then running with it
  // energised bakes that offset into every later reading - it showed up as a
  // steady phantom 0.10-0.12 A.
  //
  // Nothing is plugged in at this point, so the relay closing is harmless and
  // "socket live, waiting" is the normal resting state anyway. Measuring here
  // means the zero is taken in exactly the condition it will be used in.
  powerOn();
  delay(300);   // let the coil current and the rail settle

  Serial.print(F("Calibrating sensor zero... "));
  zeroCounts = readRawAverage(CAL_WINDOW_MS);
  Serial.print(F("done. resting value = "));
  Serial.println(zeroCounts);

  if (zeroCounts < 400 || zeroCounts > 620) {
    Serial.println(F("*** WARNING: expected about 512."));
    Serial.println(F("*** Check A0, and the sensor's VCC and GND wires."));
  }

  Serial.println();
  // Nothing should be pressed at boot. Anything reading LOW here is wired
  // wrong, not being held by the user.
  Serial.println();
  Serial.print(F("Button1 (D2): "));
  Serial.println(btnNext.stuck()   ? F("*** STUCK PRESSED - MISWIRED ***") : F("ok, released"));
  Serial.print(F("Button2 (D3): "));
  Serial.println(btnAction.stuck() ? F("*** STUCK PRESSED - MISWIRED ***") : F("ok, released"));

  if (btnNext.stuck() || btnAction.stuck()) {
    Serial.println(F("  A stuck button means its two wires reach legs that are"));
    Serial.println(F("  joined INSIDE the switch. Rotate the switch a quarter"));
    Serial.println(F("  turn and re-seat it, then restart."));
  }

  btnNext.begin(millis());
  btnAction.begin(millis());

  Serial.println();
  // The demo in one place, worked out from the constants rather than written down,
  // so it can never disagree with them.
  {
    const int cutAt  = (TAPER_PCT * BANK_RESISTORS - 1) / 100;  // highest count that cuts
    const int target = cutAt - 1;                               // one step of slack
    Serial.print(F("Load bank: "));
    Serial.print(BANK_RESISTORS);
    Serial.print(F(" x 110 ohm = "));
    printAmps((long)BANK_RESISTORS * PER_RESISTOR_MA);
    Serial.println();
    Serial.print(F("Demo: start with all "));
    Serial.print(BANK_RESISTORS);
    Serial.print(F(" in, wait 1 min, then pull down to "));
    Serial.print(target);
    Serial.println(F(" and keep your hands off for 90 s."));
    Serial.print(F("  cuts at "));
    Serial.print(cutAt);
    Serial.print(F(" or fewer; never go below 3 - at 2 it reads as unplugged."));
    Serial.println();
  }
  Serial.println();
  Serial.println(F("Button1 = cycle info   Button2 = tap to reset, hold 2s for on/off"));
  Serial.println(F("Or type into the box above:  n = button1   a = button2 tap   h = button2 hold"));
  Serial.println(F("                            z = re-zero the sensor"));
  Serial.println(F("------------------------------------------------"));

  // Relay is already closed - it was closed before calibration, above.
  beep(1, 250, 0);
  enter(READY, millis());
}

// ---------------------------------------------------------------------
//  BUTTON ACTIONS
// ---------------------------------------------------------------------
// A tap restores power from ANY off state, including MANUAL_OFF.
//
// MANUAL_OFF used to need a second 1.5 s hold to escape, which made a
// marginal button contact a trap: the hold that turned it off would register,
// but the hold to turn it back on kept restarting its timer on every contact
// bounce, leaving no way back. A tap is a far easier gesture to land, and it
// is still deliberate - which is all the cutoff logic actually requires.
void onActionTap(unsigned long now) {
  if (state == CUT || state == FAULT || state == MANUAL_OFF) {
    Serial.println(F(">> re-armed"));
    powerOn();
    beep(1, 200, 0);
    peakMa = 0;
    lowSince = 0;
    goneSince = 0;
    enter(READY, now);
  } else {
    Serial.println(F(">> (nothing to reset)"));
  }
}

void onActionHold(unsigned long now) {
  if (state == MANUAL_OFF) {
    Serial.println(F(">> power back ON"));
    powerOn();
    beep(1, 200, 0);
    peakMa = 0;
    lowSince = 0;
    goneSince = 0;
    enter(READY, now);
  } else {
    Serial.println(F(">> forced power OFF"));
    powerOff();
    beep(2, 200, 120);
    enter(MANUAL_OFF, now);
  }
}

// ---------------------------------------------------------------------
//  THE STATE MACHINE - one step, called every SAMPLE_MS
// ---------------------------------------------------------------------
void step(unsigned long now) {
  currentMa = readCurrentMa();

  // Overcurrent overrides everything, from any state.
  if (currentMa > OVERCURRENT_MA && state != FAULT) {
    powerOff();
    beep(5, 250, 120);
    Serial.println(F("!! OVERCURRENT - power cut. Check for a short circuit."));
    enter(FAULT, now);
    return;
  }

  switch (state) {

    case READY:
      if (currentMa >= PLUG_DETECT_MA) {
        Serial.println(F(">> load detected"));
        beep(1, 200, 0);
        peakMa = 0;
        lowSince = 0;
        goneSince = 0;
        chargeStart = now;
        enter(SETTLE, now);
      }
      break;

    case SETTLE:
      // Chargers spike and negotiate for the first few seconds. Sampling
      // the peak during that would poison every threshold after it.
      if (now - stateSince >= SETTLE_MS) {
        Serial.println(F(">> charging"));
        peakMa = currentMa;
        enter(CHARGING, now);
      }
      break;

    case CHARGING: {
      if (currentMa > peakMa) peakMa = currentMa;

      thresholdMa = peakMa * TAPER_PCT / 100;
      if (thresholdMa < TAPER_FLOOR_MA) thresholdMa = TAPER_FLOOR_MA;

      // Has it been unplugged?
      if (currentMa < UNPLUG_MA) {
        if (goneSince == 0) goneSince = now;
        if (now - goneSince >= UNPLUG_CONFIRM_MS) {
          Serial.println(F(">> unplugged"));
          peakMa = 0;
          lowSince = 0;
          goneSince = 0;
          enter(READY, now);
          break;
        }
      } else {
        goneSince = 0;
      }

      // Has the draw tapered off? Two guards before we believe it:
      // the session must have run a minute, and it must have pulled a
      // real charging current at some point - otherwise a device that was
      // already full would look "tapered" from the moment it was plugged in.
      bool oldEnough = (now - chargeStart >= MIN_CHARGE_MS);
      bool realCharge = (peakMa >= MIN_PEAK_MA);

      if (oldEnough && realCharge && currentMa > 0 && currentMa < thresholdMa) {
        if (lowSince == 0) {
          lowSince = now;
          Serial.println(F(">> current dropped - starting 90s confirmation"));
        }
        if (now - lowSince >= TAPER_CONFIRM_MS) {
          powerOff();
          cutoffCount++;
          Serial.println();
          Serial.println(F("****************************************"));
          Serial.println(F("  FULL - POWER CUT"));
          Serial.print  (F("  cutoff #"));
          Serial.println(cutoffCount);
          Serial.println(F("  press button2 to re-arm"));
          Serial.println(F("****************************************"));
          Serial.println();
          beep(3, 300, 150);
          delay(200);
          beep(3, 300, 150);
          enter(CUT, now);
        }
      } else {
        if (lowSince != 0) {
          Serial.println(F(">> current came back up - confirmation cancelled"));
        }
        lowSince = 0;
      }
      break;
    }

    case CUT:
    case MANUAL_OFF:
    case FAULT:
      // Deliberately does nothing. It will not re-arm on its own -
      // auto-rearming would restart the exact trickle charging this
      // whole project exists to prevent.
      break;
  }
}

// ---------------------------------------------------------------------
//  LOOP
// ---------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // Buttons are polled every pass so taps are never missed.
  // Keyboard equivalents of the buttons, typed into the Serial Monitor. These
  // exist so a faulty button can never leave you with no way to drive the
  // socket - and so the state machine can be exercised independently of the
  // button wiring while that wiring is still being debugged.
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'n' || c == 'N') {
      Serial.print(F(">> [key] button1. cutoffs so far: "));
      Serial.println(cutoffCount);
    } else if (c == 'a' || c == 'A') {
      Serial.println(F(">> [key] button2 tap"));
      onActionTap(now);
    } else if (c == 'h' || c == 'H') {
      Serial.println(F(">> [key] button2 hold"));
      onActionHold(now);
    } else if (c == 'z' || c == 'Z') {
      // Re-measure the zero point without restarting. Useful for telling a
      // fixed offset apart from genuine wander: if a phantom reading vanishes
      // after 'z' and stays away, it was an offset; if it creeps back, it is
      // drift or a loose connection.
      Serial.print(F(">> [key] re-zeroing (keep the load disconnected)... "));
      zeroCounts = readRawAverage(CAL_WINDOW_MS);
      Serial.print(F("resting value = "));
      Serial.println(zeroCounts);
    }
  }

  int a = btnAction.poll(now);
  if (a == 1) onActionTap(now);
  if (a == 2) onActionHold(now);

  int n = btnNext.poll(now);
  if (n == 1) {
    Serial.print(F(">> button1 works. cutoffs so far: "));
    Serial.println(cutoffCount);
  }

  if (now - lastSample >= SAMPLE_MS) {
    lastSample = now;
    step(now);
  }

  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    report(now);
  }
}

// =====================================================================
//  CALIBRATING
//
//  With a known load connected, compare the reported amps against a
//  multimeter. If the reading is consistently too HIGH, raise
//  SENSOR_MV_PER_AMP (try 190, 195). Too LOW, lower it (180, 175).
//  Re-upload after each change.
//
//  If it reports current with nothing connected, unplug the USB and plug
//  it back in - the zero point is measured fresh at every startup.
// =====================================================================
