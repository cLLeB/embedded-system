# Smart Socket with Adaptive Time-of-Use Scheduling — Design

**Date:** 2026-07-17
**Project #38.** Target: Arduino Uno. Market context: Ghana (230 V / 50 Hz mains).

## 1. The brief, and what it actually asks for

> Construct an eco-friendly smart socket that protects battery lifespans of plugged
> electronics (like laptops or phones). It measures the power drawn; when it detects the
> plugged device's power usage drops significantly (signaling 100% full charge), it
> completely cuts wall power to prevent trickle-charging wear.

**Components:** Arduino Uno, ACS712 (5 A), Relay Module (1-ch), 16x2 I2C LCD, Tactile
Buttons, Active Buzzer, Resistors, Breadboard, Li-ion batteries with case and SPDT switch.

### Interpreting "Adaptive Time-of-Use Scheduling"

The title says "time-of-use", but the description never mentions tariffs, peak hours, or
electricity pricing, and **the component list contains no RTC** (confirmed absent from the
physical kit). Without an RTC or network, the board cannot know wall-clock time.

**Resolution (user-confirmed):** "time-of-use" means the **plugged device's** use, not the
utility's tariff clock. "Adaptive" means the socket learns each device's real charging
profile and adapts its cutoff point, instead of using a fixed timer. This needs no RTC and
matches the description exactly.

**Explicitly out of scope (YAGNI):** tariff windows, peak/off-peak deferral, WiFi, app,
cloud, multi-socket, energy billing.

## 2. Goals, in the user's stated priority order

1. **It must work.** Correct taper detection, no false cutoffs mid-charge.
2. **Migration must be easy.** Drops into Arduino IDE with no restructuring.

Everything below is subordinate to those two.

## 3. Architecture

The single most important decision: **decision logic contains no `Arduino.h`.**

```
SmartSocket/                  <- open THIS folder in Arduino IDE
  SmartSocket.ino             <- composition root: wires HAL -> core, setup(), loop()
  src/
    core/                     <- portable C++. No Arduino.h. Native-testable.
      Types.h                 enums + small value structs
      Config.h                every tunable constant (no magic numbers)
      Interfaces.h            IClock, ICurrentSensor, IRelay, IDisplay,
                              IBuzzer, IButtonSource, IProfileStore
      Format.h/.cpp           AVR-safe number/time -> text (no printf/float bloat)
      ButtonDebouncer.h/.cpp  raw pin level -> Short/Long press events
      BuzzerDriver.h/.cpp     non-blocking beep patterns
      ChargeAnalyzer.h/.cpp   THE ALGORITHM: taper detection
      SocketController.h/.cpp THE STATE MACHINE
      UiPresenter.h/.cpp      state -> two 16-char LCD lines
    hal/                      <- Arduino-only. Guarded by #if defined(ARDUINO)
      HalClock, HalCurrent (AC-RMS + DC), HalRelay, HalDisplay,
      HalButtons, HalBuzzer, HalProfileStore (EEPROM)
test/                         <- native MSVC tests + fakes. Never seen by Arduino IDE.
```

Arduino IDE compiles a sketch's `src/` subfolder **recursively** (supported since 1.6.10),
so this layout builds with zero configuration: open the folder, press Verify.

The `core` <-> `hal` split is what makes both goals achievable simultaneously. Core depends
only on interfaces, so:

- **Testing without hardware:** fakes implement the interfaces. A `FakeClock` lets a
  3-hour charge session execute in microseconds — no `delay()`, no waiting.
- **Migration:** hardware code is quarantined in `hal/`. Swapping the LCD or sensor touches
  one file and never touches the algorithm.

### Dependency rule

`core` never includes `hal`. `hal` implements `core`'s interfaces. The `.ino` is the only
place that knows about both — it constructs concrete HAL objects and injects them.

## 4. Current measurement

The ACS712 outputs a voltage centered at Vcc/2, swinging ±185 mV per amp (5 A model).

**Two engines behind one `ICurrentSensor` interface:**

- **`DcCurrentSensor`** — averages N samples. For low-voltage DC bench testing.
- **`AcRmsCurrentSensor`** — mains is a sine wave; averaging it yields ~zero regardless of
  load. Must compute **true RMS**: sample over whole 50 Hz cycles (60 ms = 3 cycles),
  subtract the zero offset, square, accumulate, sqrt, scale. The window must stay a whole
  multiple of 20 ms or partial-cycle bias makes the reading wander with phase. Three cycles
  rather than five because this loop blocks, and every millisecond in it is one `loop()`
  cannot poll the buttons.

Ghana mains is the real deployment target, so AC-RMS is the default; DC exists so the
project can be exercised safely on the bench.

**Zero-offset auto-calibration:** at boot, with the relay **open** (guaranteed no current),
average the ADC to find the true zero point. This corrects sensor and supply variation and
is far more accurate than assuming exactly 512.

**Honest accuracy note:** 10-bit ADC over 5 V = 4.89 mV/LSB; at 185 mV/A that is
**~26 mA per LSB**. Thresholds below ~50 mA are inside the noise floor and cannot be
trusted. All thresholds are set well above this. This is a real limitation of the specified
parts, not a bug.

## 5. The algorithm (ChargeAnalyzer)

Chargers are constant-current/constant-voltage: current sits high while filling, then
tapers as the battery approaches full, dropping to a trickle at ~100%.

1. **Smooth** raw readings with an EMA to reject noise.
2. **Track peak** current seen during the session — this is the adaptive baseline.
3. **Threshold** = `max(peak × effectiveTaperPct, TAPER_FLOOR_MA)`, where
   `effectiveTaperPct = clamp(learnedTaperPct × TAPER_MARGIN_PCT)`. See the learning
   section below for why the margin exists.
4. **Confirm before acting.** Current must stay below the threshold *continuously* for
   `TAPER_CONFIRM_MS` (90 s), and the session must have run at least `MIN_CHARGE_MS`
   (60 s). This is the guard against the failure that would ruin the product: cutting power
   during a momentary dip mid-charge.
5. **Distinguish unplug from full.** Both are low current. Below `UNPLUG_MA` → device
   removed, return to Idle. Below taper but above unplug → genuinely full, cut power.
6. **Catch the already-full device.** If the peak never reaches `MIN_SESSION_PEAK_MA`
   (300 mA) by `MIN_CHARGE_MS`, this was never a charge — it is a full device left plugged
   in while the socket was re-armed. Its own trickle would otherwise become the session
   peak, putting the threshold permanently out of reach. Cut it.

### The learning (what makes it "adaptive")

Two layers:

- **Within-session:** the threshold derives from the observed peak, so it self-scales to a
  phone (~500 mA) or a laptop (~2 A) with no configuration.
- **Across sessions (EEPROM):** on each successful cutoff, record
  `actualCutoffCurrent / peak`. Blend into a stored `learnedTaperPct` via EMA, clamped to
  10–60 %. The socket converges on *this* device's real taper point over repeated use.
  Sessions that never reached `MIN_SESSION_PEAK_MA` are excluded — their ratio is ~100 %,
  which is an artefact, not a device characteristic.

#### Why the threshold sits above the learned ratio

The cutoff threshold is the learned ratio **times `TAPER_MARGIN_PCT` (130 %)**, not the
learned ratio itself. This is load-bearing, and the first version of this design got it
wrong.

The learned value comes from the current at which a cutoff happened — which is, by
construction, always *below* the threshold that triggered it. Feed that straight back and
the threshold ratchets downward every session, converging onto the device's trickle
current. At that point `current < threshold` can never be true again and **the socket
silently stops cutting off forever**, with nothing on the display to indicate it. It took
about six charges of the same device.

Holding the threshold a fixed margin above the learned value turns an absorbing fixed point
into a stable one: the learned ratio settles on the device's true taper, and the threshold
settles just above it, still reachable. Covered by
`test_ChargeAnalyzer.cpp: learning_converges_and_keeps_working_over_many_sessions`, which
runs 40 consecutive sessions — a single-session test cannot see this class of bug.

EEPROM record is versioned and CRC-16 protected; a failed CRC falls back to defaults rather
than acting on garbage.

## 6. State machine (SocketController)

```
Calibrating --(zero offset captured, relay open)--> Idle
Idle        --current > PLUG_MA sustained-------> Settling   (relay closed, listening)
Settling    --after SETTLE_MS--------------------> Charging   (ignores inrush transients)
Charging    --analyzer confirms taper-----------> Cutoff     (relay OPEN, latched, alarm)
Charging    --current ~0 sustained--------------> Idle       (unplugged)
Charging    --current > OVERCURRENT_MA----------> Fault      (relay OPEN, alarm)
Cutoff      --button ACTION short---------------> Idle       (relay closed, re-arm)
Fault       --button ACTION long----------------> Idle
any         --button ACTION long----------------> ManualOff  (user override)
```

**Cutoff latches.** It does not auto-rearm — auto-rearming would re-start the trickle
charging the product exists to prevent. Only a human clears it.

## 7. Hardware map (Arduino Uno)

| Pin | Role | Notes |
|-----|------|-------|
| A0 | ACS712 OUT | analog |
| A4 / A5 | I2C SDA / SCL | fixed on Uno; LCD backpack |
| D2 | Button NEXT | `INPUT_PULLUP`, button to GND |
| D3 | Button ACTION | `INPUT_PULLUP`, button to GND |
| D7 | Relay IN | active-LOW configurable in Config.h |
| D8 | Active buzzer | drive HIGH = sound |

Buttons use internal pull-ups, so no external resistors are needed for them.

## 8. UI

Screens cycle with NEXT:

- **Status** — state + live current; elapsed time + session peak
- **Detail** — cutoff threshold, learned ratio, session count
- **Stats** — lifetime cutoffs, total time saved

Cutoff and Fault take over the display regardless of screen, because they are the states
the user must act on.

Buzzer (active type — on/off only, no `tone()`): 1 short beep on plug detect, 3 beeps ×2 on
cutoff, continuous slow beep on fault. All non-blocking.

## 9. Error handling

- EEPROM CRC failure → defaults, no crash.
- Sensor reading implausible (beyond ACS712 range) → Fault, relay open.
- Overcurrent > 4.5 A (ACS712-5A limit) → immediate Fault, relay open. Checked in
  `SocketController::update()` against the **raw** reading, before the state switch — so it
  applies in every state the relay can be closed in, and trips within one sample. Checking
  it inside the analyzer would leave an over-limit load connected for the several seconds it
  takes to cross Idle and Settling; checking the smoothed value would wait for the EMA to
  climb. A protection limit must not be filtered, and must not be reachable only from one
  state.
- Relay defaults to **open** on boot until calibration completes — fail safe, never fail
  energized.

## 10. Testing

Native MSVC tests against fakes, covering: taper detection with realistic charge curves,
the mid-charge-dip false-positive guard, unplug vs. full disambiguation, overcurrent,
state transitions, button debounce/long-press, EEPROM CRC round-trip, and UI rendering
(exactly 16 chars, no overflow).

Firmware is additionally compiled for real AVR via the bundled `arduino-cli` to prove it
fits in the Uno's 32 KB flash / 2 KB SRAM.

## 11. Safety

The mains path is genuinely lethal. 230 V AC through an ACS712 on a breadboard is a shock
and fire hazard. Bench-test on low-voltage DC. Any mains work requires a proper enclosure,
adequate creepage/clearance, and supervision. The firmware fails safe (relay open), but
firmware cannot make mains wiring safe.
