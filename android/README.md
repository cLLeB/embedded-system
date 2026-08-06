# Smart Socket — Android app

Talks to the socket over Bluetooth Classic (SPP), shows what it is doing, and
tells it when to cut power.

**Runs with no hardware.** The connect screen has a demo that drives every screen
from a simulated socket, so the app can be developed and shown before an HC-05
exists.

---

## Build

```powershell
cd android
.\gradlew.bat assembleDebug
```

The APK lands in `app/build/outputs/apk/debug/app-debug.apk`.

Toolchain: JDK 17+, Gradle 8.11.1, AGP 8.9.1, Kotlin 2.0.21, compileSdk 35,
minSdk 26. `local.properties` points at the SDK.

---

## What it does

| | |
|---|---|
| **Live status** | State, current, peak, and how close the current is to the cutoff threshold |
| **Control** | Cut power, re-arm, probe now — the same three commands the buttons give |
| **Charge limit** | Stop charging **this phone** at 80 / 90 / 100% |
| **Notifications** | Tells you when power was cut, even with the app closed |
| **History** | Every cutoff, with peak, taper point and duration, plus a chart |
| **Export** | The history as a CSV, out through the share sheet |
| **Reconnects itself** | Rebuilds a dropped link for about eight minutes before giving up |
| **Demo mode** | A simulated socket, for when the real one is in another room |

### The charge limit is the point

A phone charging on 230 V draws about **20 mA**, which is inside the ACS712-5A's
noise. The socket genuinely cannot tell a charging phone from an empty outlet —
see "Known limits" in the root README.

But the phone knows its own battery. So the app watches `BatteryManager`, and
when the level crosses the limit it sends `C`. The socket stops having to
*infer* the charge state because it is *told*.

An 80% limit is also what battery-longevity practice recommends, which is the
premise of the whole project.

---

## Architecture

```
data/
  SocketStatus.kt     the model, and SocketState mirroring Types.h ordinals
  StatusParser.kt     decodes one "S,..." line, or returns null
  SocketTransport.kt  the interface
  BluetoothTransport  real link, RFCOMM to an HC-05
  MockTransport       a socket that exists only in software
  ReconnectPolicy.kt  how long to wait before the nth retry
  HistoryStore.kt     the flat file
  HistoryCsv.kt       history rendered for a spreadsheet
  HistoryExport.kt    that CSV into a share intent
ui/
  Theme.kt            the palette
  Components.kt       the shared pieces
  *Screen.kt          splash, onboarding, connect, dashboard
SocketViewModel.kt    state, battery watching, command dispatch
```

### Tests

```powershell
cd android
.\gradlew.bat :app:testDebugUnitTest
```

27 plain JVM tests, no emulator and no Robolectric. Everything worth testing —
the wire format, the reconnect schedule, the export — was written with no
Android in it so that it could be tested this way, which is the same split that
lets the firmware's `src/core` run on a PC.

**`SocketTransport` has two implementations for the same reason the firmware has
`ICurrentSensor`:** so the thing above it can be built and tested without the
hardware below it. `MockTransport` is not a stub returning canned values — it
runs the real sequence (settle, charge, taper, cut, wait, probe) at 20× speed,
which also makes states that are slow to reach on real hardware reachable in
seconds.

### Parsing is strict on purpose

`StatusParser` returns `null` for anything it does not fully understand. A
Bluetooth link drops mid-line every time the phone moves out of range, so partial
and corrupt lines are the normal case — and a half-parsed line would show a
number that never existed.

### An open socket is not a Smart Socket

`BluetoothTransport.connect` does not report success when the RFCOMM socket
opens. It sends `?` and waits for one line it can parse, up to three times.

Every bonded device that speaks SPP accepts a connection, and a swapped TX wire
or a module left at the wrong baud rate connects *perfectly* and then says
nothing at all. Without the handshake that arrives as a dashboard full of zeroes
with no clue which of the two it was. With it, the two cases are told apart by
whether any bytes arrived before the timeout:

| What happened | What the user is told |
|---|---|
| No bytes at all | Check `TXD` → Arduino pin 0, and that the socket is powered |
| Bytes, but nothing parses | Almost always the baud rate — the firmware uses 9600 |

### Reconnecting

A dropped link is rebuilt on a schedule that doubles from 2 s to a one-minute
ceiling, for twelve attempts — about eight minutes. Two cases pull in opposite
directions: a radio glitch wants a retry immediately, and someone who walked
into another room wants one much later. The ceiling serves the second without
making the first wait.

It **gives up** rather than retrying forever, and posts a notification when it
does. A foreground notification the user cannot dismiss, sitting over a socket
that has been switched off at the wall, is worse than an honest "lost it".

`LinkState.Reconnecting` is a separate state from `Connecting` because the two
mean opposite things to the UI: a reconnect must **not** throw the user back to
the device picker and must **not** release the foreground service. The dashboard
keeps the last readings on screen, labelled as the last ones, and disables the
command buttons — a button that looks like it worked and did not is worse than a
dead one on a device that switches mains.

---

## Design

**Gold on black.** Three golds, four blacks, and warm off-white rather than pure
white — `#FFFFFF` against gold reads cold and makes the gold look green.

**Dynamic colour is deliberately disabled.** On Android 12+ `dynamicDarkColorScheme`
derives the palette from the user's wallpaper, which usually lands on Material's
lavender-purple default. That would repaint the app in someone else's colours.

**No ripples.** Material's default ripple is a grey wash that reads as a smear on
gold; the buttons signal touch with colour instead.

The one chart is the taper bar on the dashboard — the whole product is "watch
this fall past that line", and a number alone does not show how close it is.

---

## Bluetooth notes

- **Classic SPP, not BLE.** An HC-05 is a Bluetooth 2.0 device. This will never
  work on iOS, which blocks SPP without MFi certification
- **Pair in Android's settings first.** The app lists bonded devices; it does not
  run discovery. Default HC-05 code is `1234` or `0000`
- **Android 12+ needs runtime `BLUETOOTH_CONNECT` / `BLUETOOTH_SCAN`.** Below 31
  the legacy manifest permissions are granted at install, so the request is
  skipped rather than failed — most tutorials predate this split and get it wrong

---

## Staying alive in the background

`SocketRepository` is held by the **Application**, not a ViewModel. That is the
whole trick: a link owned by a ViewModel dies with the screen, and can only
notify someone already looking at it.

`SocketService` is a foreground service that keeps the process alive while a
connection is up. It does not own the link — it exists so Android does not kill
the process, and so there is something to look at while it runs. Started on
connect, stopped on disconnect, because a notification the user cannot dismiss
should only be there while it is buying something.

Android 14 requires a foreground service to declare a type and hold the matching
permission; `connectedDevice` is the honest one here.

`SocketRepository` is also the only place that watches for **transitions**
rather than state — a cutoff notification has to fire on the edge into `Cutoff`,
once, not on every status line that happens to say `Cutoff`.

## History without Room

`HistoryStore` writes comma-separated lines to a file in `filesDir`, newest
first, capped at 60.

Room would bring the KSP plugin, a schema, a DAO and migrations for a table with
four integer columns that will never hold more than 60 rows. The cost is all
ceremony. Reads are one `readLines`, writes one `writeText`, and the format is
legible if anyone wants to look at it.

The chart is bars, not a line: each charge is a discrete event, and a line
between them would imply values in the gaps that were never measured.

**Export** writes the same rows to the cache directory as a CSV and hands it out
through a share intent, so it can go to email, Drive, or anything else with no
storage permission on any Android version. Two time columns: an ISO one for a
human and a millisecond one for a spreadsheet that will not parse it. Rows run
**oldest first**, the reverse of the screen — the newest charge belongs at the
top of a list, but a column a spreadsheet is going to plot has to run forwards.

## Not built yet

- **Connecting to real hardware has never been tested.** The HC-05 is here and
  wired but the first end-to-end run has not happened. Everything else runs
