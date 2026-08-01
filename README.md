# Smart Socket with Adaptive Time-of-Use Scheduling

**Project #38** — Arduino Uno. Cuts wall power when a charging device reaches full charge,
so it never trickle-charges and the battery lasts longer.

Status: **working on real 230 V mains.** 123/123 core tests passing, 41% flash / 40% RAM on
the Uno. Confirmed on hardware: a laptop charged to 95%, the socket detected the taper, cut
wall power and latched.

**Build it by following [`docs/FINAL_BUILD.md`](docs/FINAL_BUILD.md)** — that is the current
guide, step by step from bare parts to a working socket. `BUILD_GUIDE.md` and
`docs/STAGE2.md` describe the earlier low-voltage bench rig (18650 + resistor bank) that was
used to prove the firmware, and are kept for the reasoning only.

---

## Open it in Arduino IDE

1. **File → Open…** → select `SmartSocket/SmartSocket.ino`
2. **Tools → Board →** Arduino AVR Boards → **Arduino Uno**
3. **Sketch → Verify**

That is the whole migration. No project files to convert, no paths to fix. The `src/`
subfolder compiles automatically — that is a standard Arduino sketch layout, not a trick.

### The one dependency

**LiquidCrystal_I2C** (already installed on this machine, v1.1.2).

On any other machine: **Tools → Manage Libraries…** → search `LiquidCrystal I2C` → install
the one by *Frank de Brabander*.

Everything else — the algorithm, the state machine, the maths — is in this repo. No other
libraries.

---

## Wiring

| Uno pin | Connects to | Notes |
|---------|-------------|-------|
| **A0** | ACS712 `OUT` | analog signal |
| **5V** | ACS712 `VCC`, LCD `VCC`, relay `VCC` | |
| **GND** | ACS712 `GND`, LCD `GND`, relay `GND` | all grounds common |
| **A4** | LCD `SDA` | fixed I2C pin on the Uno |
| **A5** | LCD `SCL` | fixed I2C pin on the Uno |
| **D2** | Button 1 → other leg to **GND** | NEXT (cycle screens) |
| **D3** | Button 2 → other leg to **GND** | ACTION (reset / hold to force off) |
| **D7** | Relay `IN` | |
| **D8** | Buzzer `+` | buzzer `−` to GND |

**Buttons need no resistors.** They use the Uno's internal pull-ups, so each button just
bridges its pin to GND.

The **load** passes through the ACS712's screw terminals (in series) and through the relay's
COM/NO contacts.

### If the relay behaves backwards

Most cheap 1-channel boards are active-LOW, which is the default. If yours clicks the wrong
way, set `RelayActiveLow = false` in `SmartSocket/src/hal/HalPins.h`.

### If the LCD stays blank

The backpack address is usually `0x27`, occasionally `0x3F`. Change `LcdI2cAddress` in
`SmartSocket/src/core/Config.h`. If it is lit but shows nothing, turn the contrast
potentiometer on the back of the backpack.

---

## AC or DC — read this before wiring

Near the top of `SmartSocket.ino`:

```cpp
#define SENSOR_MODE_AC 1   // 1 = 230 V mains (true RMS)   0 = low-voltage DC bench test
```

This is **not** a cosmetic setting. AC current is a sine wave that averages to roughly zero
regardless of load, so the two modes use genuinely different maths:

- **AC (1)** — samples across whole 50 Hz cycles and computes true RMS. Correct for Ghana
  mains. **This is the default, and what the finished socket runs.**
- **DC (0)** — averages samples. For the old low-voltage bench rig only.

Run DC mode on mains and the socket reads an empty socket while a laptop charges. Run AC
mode on DC and it reads noise. **Match it to what you actually wired.**

## Safety

**230 V AC can kill you.** The mains side of this project is a genuine shock and fire
hazard, and no amount of firmware changes that.

- Bench-test on **low-voltage DC** first (`SENSOR_MODE_AC 0`).
- Mains work belongs in a proper enclosure, with adequate spacing, under supervision.
- Never work on it live.

The firmware fails safe — the relay is opened before anything else at boot and on any fault
— but firmware cannot make mains wiring safe.

---

## Using it

On boot the LCD shows `Calibrating...` for a moment while the sensor's zero point is
measured. Then it sits at `READY` with power on, waiting.

Plug a device in → short beep → `CHARGING`, with live current and elapsed time. When the
device fills up and its draw tapers off, the socket confirms the drop is real, cuts power,
beeps three times, and shows:

```
FULL - POWER CUT
Press ACTION
```

It **stays** cut. It will not re-arm on its own — auto-rearming would restart the exact
trickle charging this project exists to prevent. Press ACTION to re-arm.

If you re-arm without unplugging, the socket notices the device is still full (it never
draws a real charging current) and cuts it again after about a minute, rather than
trickle-charging it forever.

**Buttons**

| Action | Effect |
|--------|--------|
| **NEXT**, tap | Cycle screens: Status → Detail → Stats |
| **ACTION**, tap | Clear a cutoff or fault, re-arm |
| **ACTION**, hold 1.5 s | Force power off / back on |

**Screens**

```
Status              Detail              Stats
CHARGING   1.24A    Cut at     0.60A    Cutoffs        7
02:15:33  pk2.00    Taper 30%    n=7    Saved     12h30m
```

---

## What "adaptive" means here

The title says *time-of-use*, but the parts list has no RTC — the board cannot know the
clock time, let alone electricity tariff hours. The brief's description never mentions
tariffs either. It describes exactly one behaviour: measure the power, catch the drop that
means full, cut the wall.

So the *time of use* being adapted to is **the device's**, and *adaptive* means it learns:

- **Within a session** — the cutoff threshold is a fraction of the peak current this device
  actually drew, so it scales itself to a phone (~0.5 A) or a laptop (~2 A) with nothing to
  configure.
- **Across sessions** — after each successful cutoff it records where the taper really
  happened and blends it into a learned value in EEPROM. Over a few charges it converges on
  *your* device's real taper point.

Full reasoning in `docs/superpowers/specs/2026-07-17-smart-socket-design.md`.

---

## How the code is laid out

```
SmartSocket/
  SmartSocket.ino        wires the hardware to the logic; setup() and loop()
  src/core/              the logic. No Arduino.h anywhere. Tested on the PC.
    ChargeAnalyzer       the full-charge algorithm
    SocketController     the state machine that owns the relay
    CurrentMath          ADC counts -> milliamps, integer RMS
    UiPresenter          state -> two 16-char lines
    ProfileCodec         EEPROM record framing + CRC
    ButtonDebouncer, BuzzerDriver, Format, Config, Types, Interfaces
  src/hal/               the hardware. The only place Arduino.h appears.
test/                    95 tests + fakes. Arduino IDE never sees this folder.
docs/                    design spec
```

The core talks to hardware only through interfaces (`src/core/Interfaces.h`). That is what
makes two useful things possible at once: hardware can be swapped by touching one file in
`src/hal/`, and the whole algorithm can be tested with a fake clock — a three-hour charge
session runs in microseconds, so the 90-second confirmation window costs nothing to test.

## Running the tests

```powershell
powershell -ExecutionPolicy Bypass -File test\run_tests.ps1
```

Needs Visual Studio Build Tools with the C++ workload (already installed here). No test
framework to install.

The script compiles `src/core` **only** — never `src/hal`. That is deliberate: if anyone
ever adds an Arduino include to the core, the tests stop compiling and the mistake surfaces
immediately instead of at migration time.

## Tuning

Everything adjustable is in `src/core/Config.h`. The two that matter:

| Constant | Default | Meaning |
|----------|---------|---------|
| `TaperConfirmMs` | 90 s | How long current must stay low before cutting. **Lower is dangerous** — it is the guard against cutting a laptop's power on a momentary dip. Raising it only wastes a little energy. |
| `TaperRatioDefaultPct` | 30 % | Starting cutoff point as a fraction of peak, before anything is learned. |
| `TaperMarginPct` | 130 % | How far the cutoff threshold sits above the *learned* ratio. **Do not set this to 100.** The margin is what stops the learning from eating itself — see the comment on it in `Config.h`. |

## Known limits — all measured, not assumed

- **It cannot see a phone.** Measured on this hardware: a phone charging draws **0.02 A** on
  230 V, which is the same as an empty socket reads. 20 mA is 0.4% of the ACS712-**5A**'s
  range and 0.76 of one ADC count, so no threshold can separate the signal from the noise.
  Laptop-class loads (0.13–0.16 A) work reliably. This is the specified sensor's resolution,
  not an algorithm fault — the same firmware on a 1 A part, or a shunt and INA219, resolves
  both. `ICurrentSensor` exists so that swap costs one file.
- **Resolution is better than the datasheet arithmetic suggests.** One ADC count is 26 mA,
  but the AC RMS window averages ~500 samples, and the measured noise pedestal on an empty
  socket is **under 0.02 A**. Averaging drives quantisation noise below a single count.
- **The adaptive learning is currently inert.** `TaperFloorMa` (100 mA) is larger than the
  peak-derived threshold at these currents, so the floor decides every cutoff and the learned
  ratio never changes the outcome. The learning is correct and tested; it just has no room to
  act at 230 V with this sensor. On the 3.7 V rig, where peaks were 370 mA, it governed.
- **One profile.** The socket learns one device profile, not one per device. Alternating
  devices makes the learned ratio drift between them. The within-session peak-scaling still
  handles both correctly; only the cross-session learning blurs.
- **The relay is the weak part.** A switch-mode charger's inrush is tens of amps for a few
  milliseconds, and a 10 A relay switching into that repeatedly can micro-weld its contacts.
  The firmware now detects this (see below) rather than claiming to have cut power it hasn't.

## What makes it "smart" beyond the cutoff

- **Automatic recovery.** The sensor is downstream of the relay, so an open relay leaves the
  socket blind — it cannot tell an empty socket from a full laptop. Rather than demand a
  button press, it closes the contacts for 8 s every 15 min (device still present) or 60 s
  (socket empty) and resumes only on a real charging current. Not auto-rearming: it stays
  closed only on positive evidence.
- **Relay verification.** `setClosed(false)` is a command, not a measurement. The socket keeps
  sampling while cut, and if current is still flowing 3 s later it raises `! RELAY STUCK !`
  instead of displaying `FULL - POWER CUT` over a live outlet.
- **It cannot silently freeze.** `Wire` has a 25 ms timeout so I2C noise from the relay
  cannot lock the bus, and a 4 s watchdog resets the chip if `loop()` ever stops — on reset
  the relay is opened before anything else. A hang used to leave the relay wherever it was,
  which on a mains socket means power stuck **on**.
- **The zero offset is tracked, not calibrated once.** The ACS712's offset drifts with
  temperature and rail loading; frozen at boot it walked far enough in two minutes to fake a
  0.10 A load on an empty socket. Each 60 ms window now re-derives it from the sample mean,
  which over whole mains cycles is the offset whether a load is present or not.
