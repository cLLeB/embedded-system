# Smart Socket — project context

Arduino Uno socket that cuts 230 V wall power when a charging device's current
tapers off, so it never trickle-charges. Project 38.

**Working on real mains.** Confirmed on hardware: a laptop charged to 95%, the
socket detected the taper, cut power and latched.

---

## Commands

```powershell
# 129 core tests, native, no hardware needed. Finds MSVC via vswhere.
powershell -ExecutionPolicy Bypass -File test\run_tests.ps1

# Compile-verify for the Uno without opening the IDE.
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" `
    compile --fqbn arduino:avr:uno SmartSocket

# Regenerate the enclosure STLs (slim set).
$env:SMART_SOCKET_SLIM=1
& "C:\Program Files\FreeCAD 1.1\bin\freecadcmd.exe" enclosure\smart_socket_enclosure.py

# Android: 27 unit tests, then the installable APK.
cd android; .\gradlew.bat test assembleRelease

# Windows desktop: 30 core tests, then the self-contained exe.
cd desktop
& "C:\Program Files\dotnet\dotnet.exe" test tests\SmartSocket.Core.Tests\SmartSocket.Core.Tests.csproj
& "C:\Program Files\dotnet\dotnet.exe" publish src\SmartSocket.App\SmartSocket.App.csproj -c Release -o dist
```

Current budget: **42% flash, 47% RAM** as shipped, **47% / 50%** with every
peripheral enabled. There is no `arduino-cli` on PATH; the Arduino IDE bundles
the one above.

## Fitted peripherals

The top of `SmartSocket.ino` has `HAS_BUTTONS`, `HAS_BUZZER` and
`HAS_BLUETOOTH`, all **0** — the buttons, buzzer and HC-05 are not wired yet.
Set each to 1 as the part goes in. Sensor + relay + LCD is the whole
safety-critical path; the rest is how a person hears about it and answers back.

With them at 0 the socket still cuts power on its own and still probes its way
back. The LCD cycles its three screens on a timer instead of on the NEXT button,
and the USB Serial Monitor (9600, line ending **Newline**) is the control panel:
`C` cut, `R` re-arm, `P` probe now, `?` status. Those are the same four commands
the phone sends, through the same `onRemote()` path as the buttons.

---

## The architecture rule

**`src/core/` must never include `Arduino.h`.**

That is what lets the whole algorithm run natively on a PC against fakes — a
three-hour charge session runs in microseconds, so the 90-second confirmation
window costs nothing to test. `run_tests.ps1` compiles `src/core` **only**, so if
anyone adds an Arduino include to the core the tests stop compiling immediately
rather than at migration time.

`src/hal/` is the only place `Arduino.h` appears. The core reaches hardware
through `src/core/Interfaces.h`. Swapping the current sensor, the display or the
relay is one file in `src/hal/`.

---

## Measured hardware facts

These are measured on the actual kit, not assumed. Every threshold in
`src/core/Config.h` is sized against them, and the comments there explain each one.

| | Value |
|---|---|
| One ADC count | **26 mA** (5 V / 1024 / 185 mV-per-A) |
| Noise pedestal, empty socket | **under 0.02 A** — better than one count, because the AC RMS window averages ~500 samples |
| Laptop charging | **0.13–0.16 A** |
| Laptop at 95%, still running | **0.06–0.10 A** |
| **Phone charging** | **0.02 A — indistinguishable from an empty socket** |

**The socket cannot detect a phone.** 20 mA is 0.4% of the ACS712-**5A**'s range.
No threshold separates a signal that size from the noise. This is the specified
sensor's resolution, not an algorithm fault. See `docs/MOBILE_APP.md` §2 for how
the Android app is meant to solve it.

**The Bluetooth module is not an HC-05.** It is sold as one, on a ZS-040 board
with the button, but it is an **HM-10 class BLE part** (CC2541, service `FFE0`,
characteristic `FFE1`). Windows reported a GATT service on it, Classic pairing
never produced a serial port, and the desktop app identified the profile on
connection. So both apps carry a **BLE/GATT transport** as well as the Classic
one and pick per device — `BleSerialProfiles` in each app resolves which
characteristic carries the bytes, from a table plus a notify+write fallback.

**Each client cuts using the battery only it can see.** The sensor handles the
general case; the two apps handle their own host device:

| Device | What cuts the power |
|---|---|
| Laptop | The socket's current taper, **or** the Windows app at an exact percentage |
| The phone running the Android app | That app, at its battery limit |
| Any other device | The socket's current sensing alone — and it cannot see a phone |

Both cutoffs can be live at once without conflict: whichever fires first sends
`C`, and the firmware ignores a command for a state it is already in.

---

## Things that have already gone wrong here

Each of these cost hours. The fixes are in the code with comments explaining why.

- **Thresholds were sized for the old 3.7 V battery rig.** On mains every current
  is 2–3× smaller. At the old values a normally-charging laptop fell under
  `MinSessionPeakMa` and was cut at 60 s as "already full" — a false cutoff that
  looks identical to a real one, except `Cutoffs` does not increment.
- **Tests hardcoded milliamp literals.** They silently stopped testing what they
  named when the thresholds were rescaled. Derive from `config::` constants.
- **The zero offset was calibrated once at boot, with the relay open.** The socket
  runs with the relay closed, and the coil's 70 mA through the shared 5 V rail
  shifts the ACS712's midpoint. It also drifts with temperature. It is now
  re-derived every 60 ms from the sample mean, which over whole mains cycles is
  the offset whether a load is present or not.
- **`Wire` blocks forever if a device holds SDA low.** The relay switches 230 V
  centimetres from unshielded I2C leads. When `loop()` stops, pin 7 holds its last
  level — so a hang mid-charge leaves the relay **closed**. There is now a 25 ms
  Wire timeout and a 4 s watchdog.
- **`setClosed(false)` is a command, not a measurement.** Contacts weld. The
  socket now keeps sampling while cut and raises `RELAY STUCK` rather than
  displaying `FULL - POWER CUT` over a live outlet.
- **The test rig held current constant after the relay opened**, which is
  physically impossible — the ACS712 is downstream of the relay. `Rig::run` now
  models that.

---

## Docs

| File | What |
|---|---|
| `README.md` | Overview, wiring, known limits |
| `docs/FINAL_BUILD.md` | **The current build guide**, step by step |
| `docs/ENCLOSURE_ASSEMBLY.md` | Fitting the build into the printed box. Supersedes `FINAL_BUILD.md` PART 10 |
| `docs/HANDOFF.md` | **Start here.** What is done, what is not, and the order to finish it |
| `android/README.md` | The Android app: how to build it, and why it is shaped that way |
| `desktop/README.md` | The Windows app: build, publish, and why the laptop cutoff differs from the phone's |
| `docs/MOBILE_APP.md` | The app's original spec. Kept for the reasoning; **it is built** |
| `enclosure/build_slim.log` | Authoritative list of what to print |
| `BUILD_GUIDE.md`, `docs/STAGE2.md` | **Superseded.** The old low-voltage bench rig |
| `enclosure/README.md` | **Stale**, banner at the top says so |

---

## Safety

This is a 230 V mains build with **no earth** — the plug lead is 2-core, so only
double-insulated (Class II) appliances may be plugged in. The relay switches
**live**, never neutral, and the load sits on `NO` so any failure falls back to
power off. Mains never touches the breadboard.
