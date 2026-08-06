# Handover

You have inherited a working smart socket and an Android app that has never met
the radio it was written for. This is what is done, what is not, and the order
to finish it in.

If you are an AI assistant picking this up: read `CLAUDE.md` first. It has the
build commands, the architecture rule, the measured hardware facts, and the six
bugs that already cost days here.

---

## 1. What already works

**On real 230 V mains, confirmed on hardware.** A laptop charged to 95%, the
socket saw the current taper, cut the wall and latched. `Cutoffs` incremented,
which is the difference between a real cutoff and the trickle-rejection path
that looks identical from the outside.

| | State |
|---|---|
| Firmware | **Done.** 129 tests, 42% flash / 47% RAM as fitted |
| Telemetry over UART | **Done and proven on hardware.** `S,...` lines confirmed once a second in the Serial Monitor at 9600 |
| Android app | **Done.** 27 tests. Battery cutoff now runs on the Application, so it survives the app being swiped away |
| Windows app | **Done.** 30 tests. Self-contained `.exe`, tray-resident, cuts at the laptop's own battery percentage |
| App ↔ real socket | **Never tested.** The HC-05 is wired; this is the one open step |
| Enclosure | Designed, verified, **not printed** |

Both clients run today with no hardware at all: open either and choose
**"Open demo"**, which drives every screen from the socket's own state machine
in software.

Built installers land in `dist/`:

| File | Install it by |
|---|---|
| `SmartSocket-1.0.apk` | Copying to the phone and tapping it. Allow "unknown sources" |
| `SmartSocket-1.0-windows.exe` | Double-clicking. Self-contained — nothing to install first |

The APK is signed with `android/keystore/smartsocket.jks`. **That keystore and
its `keystore.properties` are gitignored, and losing them means never being able
to update an installed copy of the app.** Back them up somewhere off this
machine.

---

## 2. Do these in order

### Step 1 — Flash the current firmware and prove the link over USB

**Before touching the HC-05.** The radio uses the same UART, so if this works,
the module is plug-and-go.

1. Open `SmartSocket/SmartSocket.ino`, Board → Arduino Uno, upload.
   It should report **15218 bytes (47%)**.
2. **Tools → Serial Monitor, 9600 baud.**
3. You should see a line every second:

   ```
   S,1,0,0,0,0,3,45230,1
   ```

   That is `S,<state>,<mA>,<peak>,<threshold>,<elapsedMs>,<cutoffs>,<savedMs>,<relay>`.
   State `1` is `READY` — the ordinals are the `SocketState` enum in
   `src/core/Types.h`.
4. Type **`C`** and press Enter. The relay should click, the LCD should read
   `FULL - POWER CUT`, and the state ordinal should change to `4`.
5. Type **`R`**. It should re-arm to `1`.

If that works the whole telemetry path is proven and nothing below is guesswork.

### Step 2 — Wire the HC-05

**The module on hand is a ZS-040**, six pins, with a small button. Its own label
settles the two questions that matter:

- **`Power: 3.6--6V`** — so `VCC` goes to the Uno's **5 V**. It has an onboard
  regulator.
- **`LEVEL: 3.3V`** — so `RXD` is a **3.3 V input**. The divider below is not
  optional.

Pin order, printed beside each pin. **The button is at the `EN` end**, which is
the reliable way to tell which end is which:

```
EN      <- button beside this one.   leave empty
VCC                                  + rail
GND                                  - rail
TXD                                  Uno pin 0 (RX), direct
RXD                                  divider tap
STATE                                leave empty
```

**Four wires, and one of them needs resistors.**

```
HC-05 VCC  ->  Uno 5V
HC-05 GND  ->  Uno GND
HC-05 TXD  ->  Uno pin 0 (RX)      direct
Uno pin 1 (TX)  ->  divider  ->  HC-05 RXD
```

**The divider is not optional.** The Uno drives 5 V into a pin the board itself
labels 3.3 V. Feeding it 5 V works right up until it does not, and then the
module is dead.

Build it from three of the 110 Ω resistors already in the kit, in the **upper
block**, which is empty across columns 11–17:

```
Uno pin 1 ----[110R]----+----[110R]----[110R]---- GND
   col 11       11-13   |    13-15       15 - rail
                        |
                     col 13 ---- HC-05 RXD
```

That is 5 V x 220/330 = **3.33 V**. Any pair in a roughly 1:2 ratio works;
these are the parts on hand.

The other direction needs nothing: the HC-05's 3.3 V output clears the Uno's
input threshold.

**Pins 0 and 1 are also the USB uploader's.** Unplug the HC-05's TX and RX
wires before every upload, or the upload fails.

**Do not hold the module's button while powering up.** That line is the module's
`KEY` input; holding it enters AT command mode at 38400 and the module then
looks dead. Normal power-up is data mode at 9600, which is `config::TelemetryBaud`.

### Step 3 — Pair, then connect

1. Power the socket. The HC-05's LED blinks fast when unpaired.
2. On the phone, **Android Settings → Bluetooth → pair with `HC-05`**. Code is
   **`1234`** or **`0000`**.
3. Open the app, allow the Bluetooth permission, and pick `HC-05` from the list.

**The app lists bonded devices only — it does not run discovery.** If it is not
paired in Android's settings, it will not appear.

Expect this to be the step that needs debugging, because it is the one path
nobody has run. Baud is 9600 on both sides (`config::TelemetryBaud` and the
HC-05 default). If the module was reconfigured by a previous owner, that is the
first thing to check.

**The app now tells you which failure it is.** It does not report a connection
until the socket has answered: it sends `?` and waits for one line it can parse.
Three outcomes, and they mean different things:

| The app says | It means |
|---|---|
| "Could not connect" | The RFCOMM socket never opened. Not paired, out of range, or unpowered |
| "never sent anything" | Connected fine, but silence. **`TXD` → pin 0**, or the socket is off |
| "cannot read" what it sends | Bytes are arriving but nothing parses. **Baud rate** — the firmware uses 9600 |

### Step 4 — Print and assemble the enclosure

Three files from `enclosure/stl_slim/`: **Base, Lid, Shelf.** ~340 g.
Assembly is `docs/FINAL_BUILD.md` PART 10.

Needs: M3 self-tappers (8/12/16/20 mm assortment), six M3x16 countersunk, hot
glue, and **a soldering iron** — the buttons and buzzer move onto the lid and
need six joints.

The HC-05 fits in the 18650 bay, which is dead space in the mains build.

---

## 3. What is left in the app

**One thing, and it is the one that needs the module in the room.**

| | Notes |
|---|---|
| Real-hardware testing of `BluetoothTransport` | Never run against an HC-05 |

Everything else is built. Since the handover was written:

- **A connection handshake.** An open RFCOMM socket is not a Smart Socket — the
  app now sends `?` and waits for a line it can parse before saying "connected",
  and distinguishes silence from unreadable bytes. See the table in Step 3
- **Automatic reconnection**, doubling from 2 s to a one-minute ceiling for
  about eight minutes, then giving up with a notification. The dashboard stays
  put and the foreground service stays up while it works
- **History export** as a CSV through the share sheet
- **27 JVM unit tests** — `cd android; .\gradlew.bat :app:testDebugUnitTest`

The app's architecture is what made that possible without hardware:
`SocketTransport` has two implementations, and `MockTransport` runs the real
sequence at 20x. **Build against the mock, then swap.** That is why it exists.

---

## 4. Things that will waste your time if you do not know them

- **`src/core/` must never include `Arduino.h`.** That is what lets 129 tests
  run on a PC. `run_tests.ps1` compiles core only, so a violation fails loudly.
- **Do not hardcode milliamp literals in tests.** They silently stop testing
  what they name when thresholds move. Derive from `config::`.
- **The socket cannot see a phone charging.** 20 mA on 230 V, inside the
  ACS712-5A's noise. This is not fixable in firmware — it is why the app has a
  battery-percentage cutoff.
- **Unplug the HC-05's TX/RX before every upload.**
- **The relay contacts stuck once.** A `Wire` lockup froze `loop()` with the
  relay closed; the timeout and watchdog now prevent it, and ten manual-off
  cycles passed afterwards. If "power cut" ever shows over a live outlet again,
  the firmware will now say `! RELAY STUCK !` instead of lying.

---

## 5. Safety

**230 V, and this build has no earth** — the plug lead is 2-core. Only
double-insulated (Class II) appliances. The relay switches **live**, never
neutral, and the load is on `NO` so any failure falls back to power off. Mains
never touches the breadboard.

Full rules: `docs/FINAL_BUILD.md`, "Safety".
