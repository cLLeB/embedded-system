# Smart Socket — Windows app

The desktop companion. Same socket, same wire format, same gold-on-black as the
phone app, plus the one thing a laptop can do that a phone cannot: cut power at
**its own** battery percentage.

---

## Why this exists when the socket already works

The socket cuts power on its own. It watches the charge current taper and opens
the relay when it falls far enough below the peak — no app, no phone, no PC.
That is the whole safety-critical path and it is finished.

What it cannot do is know *what percentage* that taper happened at. On a laptop
the taper lands wherever the charger decides, typically the high nineties.

If what you want is to stop at **80%** for the sake of the battery's lifespan,
only Windows knows when that is. So this app asks Windows and sends the cut.

Both cutoffs run at once and do not fight: whichever condition is met first
cuts, and the firmware ignores a `C` for a state it is already in.

| Device | What cuts the power |
|---|---|
| Laptop | The socket's current sensing, **or** this app at your exact percentage |
| The phone running the Android app | That app, at its battery limit |
| Anything else | The socket's current sensing only |

---

## Build and run

Needs the .NET 8 SDK. Nothing else — no Visual Studio.

```powershell
# All 30 core tests. No hardware, no UI, runs in under a second.
dotnet test tests\SmartSocket.Core.Tests\SmartSocket.Core.Tests.csproj

# Debug run.
dotnet run --project src\SmartSocket.App

# The thing you hand to somebody: one self-contained .exe, no runtime needed.
dotnet publish src\SmartSocket.App\SmartSocket.App.csproj -c Release -o dist
```

The published `dist\SmartSocket.exe` is about 68 MB because it carries the .NET
runtime and WPF inside it. That is the deliberate trade: nothing to install, no
"please install .NET 8" step in front of somebody marking the project.

---

## The architecture rule, again

**`SmartSocket.Core` must never reference anything Windows.**

It targets plain `net8.0`, not `net8.0-windows`, so the compiler enforces it —
the same trick as the firmware's `src/core` never including `Arduino.h`, and the
Android app's data layer never importing Android.

That is what lets the wire format, the charge session, the reconnect schedule
and the CSV export be tested with no window, no COM port and no socket.

```
desktop/
  src/SmartSocket.Core/     plain net8.0. Parser, session, reconnect, CSV.
  src/SmartSocket.App/      net8.0-windows. WPF, serial, tray, Win32 battery.
  tests/SmartSocket.Core.Tests/   30 tests, ported case for case from Kotlin.
```

The tests are deliberately ports of the Android app's tests, assertion for
assertion. Two clients that disagree about the wire format would show different
numbers for the same socket, and the tests are what stop that happening
quietly.

---

## Connecting

Both routes to the socket are a COM port at 9600, so there is one transport:

- **USB cable** — the Uno enumerates directly through its CDC/CH340 driver.
- **HC-05** — pair it in Windows Bluetooth settings first. Windows then exposes
  an *outgoing* virtual COM port for it, which is the one to pick.

**An open port is not a Smart Socket.** Windows will happily open a port
belonging to a printer or a GPS dongle, so connecting is not finished until the
socket has answered a `?` with a line the app can parse. The three ways that can
fail are reported separately because they have different fixes:

| The app says | What is wrong |
|---|---|
| "already open in another program" | Close the Arduino IDE's Serial Monitor |
| "opened but never sent anything" | Wrong port, or the socket is not powered |
| "sending data this app cannot read" | Baud rate. The firmware uses 9600 |

Opening a COM port asserts DTR, which **resets the Uno**. That is wanted — it
puts the firmware in a known state — but it is why the handshake tolerates
several seconds of silence before giving up.

---

## Closing the window does not stop it

The window is a view, not the app. Closing it hides it to the tray, because a
socket being watched is the normal state and a window on screen is not. The tray
icon carries the current state in its tooltip, offers **Cut** and **Re-arm**
directly, and **Quit** is the only thing that actually stops it.

This is the desktop counterpart of the phone app's foreground service, and it
exists for the same reason: a cutoff an hour away is no use if it only fires
while somebody is looking at the screen.

Balloon notifications fire on cutoff, on a stuck relay, on a lost link, and when
the app itself cut at your battery limit.

---

## What it stores, and where

`%APPDATA%\SmartSocket\`

| File | What |
|---|---|
| `settings.json` | Battery limit, whether to cut at it, last port used |
| `history.csv` | Completed charges, in the same four-field encoding the Android app writes |

The history encoding is shared deliberately: a file from the phone can be
dropped in here and read, and the exported spreadsheet has identical columns
from both apps, so exports can be concatenated.

---

## Known limits

- **Windows only.** WPF and `GetSystemPowerStatus` are both Win32. The `Core`
  project is portable; nothing above it is.
- **A desktop PC has no battery**, so the percentage cutoff has nothing to act
  on. The app says so and disables the control rather than showing 0%.
- **Reconnect gives up after about eight minutes** (`ReconnectPolicy`), rather
  than retrying forever. A tray icon claiming to watch a socket switched off at
  the wall is worse than an honest "lost it".
- If Windows will not report the battery percentage it returns 255, which the
  app treats as "unknown" and refuses to act on. Guessing there would cut power
  to a machine whose charge it does not know.
