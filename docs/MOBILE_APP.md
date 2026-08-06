# Mobile app — specification and plan

> **Status: built.** The app lives in [`android/`](../android) — see
> [`android/README.md`](../android/README.md) for how to build it and what it
> does. The firmware side (`ITelemetry` / `HalTelemetry`) is in and tested.
>
> What remains untested is the only part that needs hardware: **connecting to a
> real HC-05.** Everything else runs today, against `MockTransport`.
>
> This document is kept as the reasoning behind the design, not as a to-do list.

Target: **Android**, native.

---

## 1. Read this first: the Uno has no radio

There is no WiFi and no Bluetooth on an Arduino Uno. Not disabled, not unused —
physically absent. **An app cannot talk to this socket until a radio is added.**

The kit contains no radio either. Every component was checked against the kit
photographs: Uno, LCD + PCF8574 backpack, relay module, ACS712, breadboard,
tactile buttons, rocker switch, 18650 + holder, header strip, jumpers, resistors,
and a KY-016 RGB LED module. Nothing with an antenna.

| Option | Cost (GHS) | Notes |
|---|---|---|
| **HC-05** — Bluetooth Classic, SPP | 40–60 | **Recommended.** Android-native, simplest stack |
| HC-06 | 40–60 | Slave only. Fine here — the socket is never the master |
| HM-10 / HC-08 | 60–90 | **BLE, not Classic.** Different Android code entirely (GATT, not a socket). Works on iOS |
| ESP-01 / ESP8266 | 40–60 | WiFi. Needs 3.3 V and level shifting |
| Replace the Uno with an ESP32 | 80–120 | WiFi *and* BLE built in. See below |

**iOS is why Classic is the right default.** Apple blocks Bluetooth SPP without
MFi certification, so an HC-05 is Android-only. That matches the target.

### The ESP32 option is smaller than it sounds — but not yet

`src/core/` is portable C++ with no `Arduino.h` anywhere, which is why 123 tests
run natively on a PC. Only `src/hal/` knows about the hardware. Porting means
rewriting five small files, not the project — and it would bring far more RAM and
flash, a much better ADC, and no pairing.

**Do not do it before the demo.** The enclosure is dimensioned around the Uno's
footprint and mounting holes.

---

## 2. What the app should do

### Tier 1 — read-only. Most of the value, least of the work.

- Live status: state, current, session time, peak
- **Notification when power is cut** — "Your laptop is charged." This is the
  actual point of a smart socket; the rest is decoration
- Charge history and graphs. The Uno has no clock and 1 KB of RAM; the phone has
  both. Let the socket stream and the phone remember
- Lifetime stats: cutoff count, time saved

### Tier 2 — control

- Force power off / on, and re-arm after a cutoff, without walking to the box
- Trigger a probe immediately instead of waiting out the interval
- Change thresholds live — taper confirm time, taper ratio, plug-detect level —
  without re-flashing

### Tier 3 — the one that actually matters

**Solve the phone-detection problem in software.**

The socket cannot see a phone charging. Measured: 0.02 A on 230 V, which is the
same as an empty socket reads, 0.76 of one ADC count. No threshold separates a
signal that size from the noise. See the "Known limits" section of the README.

**But the phone knows its own battery percentage.** Android exposes it through
`BatteryManager` in about five lines.

So: the app watches its own battery, and when it crosses a limit the user sets —
80%, 90%, 100% — it sends `C` over Bluetooth and the socket cuts power.

The socket stops having to *infer* the charge state, because it is told. An 80%
limit is also exactly what battery-longevity practice recommends, which is the
entire premise of the project.

**Build this first after the plumbing works.** It turns the hardest unsolved
problem into a feature.

---

## 3. Wire protocol

Line-based ASCII over UART at 9600. Deliberately dumb: no JSON parser on a part
with 1.2 KB of free RAM.

### Socket → app, one line per sample interval

```
S,<state>,<mA>,<peakMa>,<elapsedMs>,<cutoffs>,<savedMs>\n
```

`<state>` is the `SocketState` enum ordinal from `src/core/Types.h`. Send the
ordinal, not a string — the app has the names, the socket has 19 KB of flash left
and no reason to spend it on text.

### App → socket

| Command | Meaning | |
|---|---|---|
| `C\n` | Cut power now | built |
| `R\n` | Re-arm | built |
| `P\n` | Probe now | built |
| `?\n` | Send one status line immediately | built |
| `T<seconds>\n` | Set taper confirm time | **not built** |

`HalTelemetry::pump` accepts **single letters only**; anything longer is dropped.
`T<seconds>` was designed here and deliberately not implemented — live-editable
cutoff thresholds on a device switching 230 V is a bigger decision than it looks,
and the socket learns its own profile anyway. Adding it means teaching `pump` to
parse an argument, and making the threshold mutable in `Config.h`.

Unknown commands must be ignored, not faulted. A phone that reconnects mid-line
will send garbage.

---

## 4. Firmware work needed

**None of this exists yet.**

1. `ITelemetry` in `src/core/Interfaces.h` — `void publish(const SocketStatus&)`
   and a command-poll. **The core must not learn that Bluetooth exists**; it talks
   to hardware only through interfaces, which is what keeps it testable with fakes.
2. `src/hal/HalTelemetry.h/.cpp` — the UART implementation.
3. A fake in `test/Fakes.h`, and tests that assert commands are honoured and
   malformed input is ignored.

Budget: roughly 1.5 KB flash, ~100 bytes RAM. There is 19 KB and 1.2 KB spare.

### Two hardware gotchas

- **Hardware `Serial` is on pins 0/1, which the USB uploader also uses.** Unplug
  the HC-05 to upload. This is normal and worth documenting on the enclosure.
- **Do not use `SoftwareSerial`.** It disables interrupts while transmitting, and
  this firmware has a 60 ms blocking RMS sample window and `tone()` on Timer2. Use
  the hardware UART.

---

## 5. Android stack

| Layer | Choice |
|---|---|
| Language | Kotlin |
| UI | Jetpack Compose |
| Bluetooth | `android.bluetooth.BluetoothSocket`, SPP UUID `00001101-0000-1000-8000-00805F9B34FB` |
| Async | Coroutines + `Flow` over the input stream |
| Storage | Room, for charge history |
| Charts | Compose Canvas, or MPAndroidChart |
| Background | Foreground service + `NotificationManager` |

### Two things that will bite

- **Android 12+ requires runtime `BLUETOOTH_CONNECT` and `BLUETOOTH_SCAN`
  permissions**, not manifest-only. Most tutorials predate this and will not work.
- **Battery optimisation kills background services.** "Notify me when charging
  finishes" needs a foreground service with a persistent notification, and a
  request for battery-optimisation exemption.

---

## 6. Build order

1. **Firmware telemetry + the `C` command.** Small, testable, immediately useful.
   Verify with the Arduino IDE's Serial Monitor before any app exists
2. **Bare-bones app**: connect, show live status, one button to cut power
3. **The battery-percentage cutoff** (Tier 3)
4. Notifications, history, charts, live settings

Steps 1 and 2 are an evening each. A polished app with pairing, a foreground
service, permissions handled properly, history and charts is days.
