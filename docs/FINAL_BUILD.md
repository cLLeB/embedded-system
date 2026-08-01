# Final build — mains smart socket, step by step

This replaces `BUILD_GUIDE.md` and `docs/STAGE2.md`. Those describe the
low-voltage bench rig (18650 + resistor bank) that was used to prove the
firmware. This is the finished product: a plug, a wall socket, and the relay
cutting real wall power when the charging device fills up.

Do the steps in order. Do not skip ahead.

---

## Your parts, by position in the photo

**Row 1, left to right:**

| # | Item | Used? |
|---|---|---|
| 1 | 18650 battery holder | **No** — bench rig only |
| 2 | 40-pin male header strip | **No** |
| 3 | Small 4-pin LED module | **No** |
| 4 | Breadboard | Yes |
| 5 | Bag of resistors | Yes — **one** 110 Ω only |
| 6 | Black rocker switch | Yes — mains master switch |
| 7 | ACS712 current sensor | Yes |
| 8 | Arduino Uno | Yes |
| 9 | 16x2 LCD with I2C backpack | Yes |

**Row 2:** plug, socket faceplate, relay. All three used, all three rewired.

---

## How to read the breadboard positions

Hold the board so the printed numbers read normally — **0 on the left, 60 on
the right**.

- **UPPER BLOCK** = the five rows between the centre channel and the **top**
  pair of rails.
- **LOWER BLOCK** = the five rows between the centre channel and the **bottom**
  pair of rails.
- **`+` RAIL** = on the top pair, the hole row next to the **red** line (the
  upper of the two).
- **`−` RAIL** = on the top pair, the hole row next to the **blue** line (the
  lower of the two).

Row letters are deliberately **not** used. Different boards letter them in
opposite directions — this one runs A–E across the upper block and F–J across
the lower block, which is the reverse of many. Block plus column is
unambiguous on any board.

Positions are written as **column N, upper** or **column N, lower**. Any of the
five rows in that block works: all five holes in one column of one block are the
same electrical point.

Your rails are **split at the middle notch, around column 30**. Everything in
this build stays in the **left half, columns 0–30**. Nothing needs bridging.

All five holes in one column of one block are the same electrical point. So if
your board's letters happen to run the other way round, ignore my letters and
use the column plus the block — it makes no electrical difference. The only
place the row matters is the two buttons, and there the instruction says which
side of the gap.

The bottom pair of rails is **not used at all**.

---

## Safety — read once, obey throughout

1. Nothing goes into the wall until STEP 34.
2. Once it is in the wall, the lid is on and no bare metal is reachable.
3. Every mains joint is inside a screw terminal or a connector strip. No
   twisted-and-taped joints. No mains on the breadboard, ever.
4. The Arduino's USB source is the PC or a power bank — **never** the socket the
   relay switches.
5. The relay switches **LIVE**, never neutral.

**This build has no earth.** Your plug lead is 2-core. The socket's earth
terminal stays empty and must never be linked to neutral. Only
**double-insulated (Class II)** appliances may be plugged in — look for the ⧠
double-square symbol on the charger label, or a two-pin figure-8 lead.

---

# PART 1 — Gather

### STEP 1 — Get these

- An assortment of **M3 self-tapping screws, 8 / 12 / 16 / 20 mm**, plus six
  M3 countersunk around 16 mm for the lid. See PART 10 for what goes where
- Hot glue gun
- A small mains connector strip (chocolate block)
- About **10 female-to-male jumper wires** — the LCD, relay and ACS712 all have
  male header pins, and in the finished box they sit in brackets, not in the
  breadboard
- A **Class II laptop charger** (⧠ symbol) for the demo. A phone charger draws
  about 0.09 A on 230 V and the ACS712-5A cannot see it
- A **5 V USB phone charger** and a **USB-A-to-USB-B cable** — this is what powers
  the Arduino in the finished product, instead of your laptop. See PART 9B

### STEP 2 — Clear the bench

Unplug the Arduino from the PC. Unplug the mains plug from everything. Pull
every wire off the socket faceplate and the relay. Pull the buzzer, the
resistor and both buttons out of the breadboard.

You now have bare parts. The Arduino stays unplugged until STEP 21.

---

# PART 2 — Breadboard

### STEP 3 — Orient the board

Turn the breadboard so the printed numbers read normally, 0 on the left.
Confirm the top pair of rails has the red line above the blue line.

### STEP 4 — Buzzer

Push the buzzer into the **lower block** so one leg is in **column 5** and the
other in **column 8**.

It is a passive buzzer. Polarity does not matter — either leg either way.

The body is about 12 mm across, so it covers roughly columns 4–9 and three rows
deep. Everything below reaches into the row furthest from it, next to the
centre channel.

### STEP 5 — Resistor

Take **one** 110 Ω resistor (bands: brown – brown – black – black – brown).
Bend both legs down. One leg into **column 8, lower block**, the other into
**column 12, lower block**.

Column 8 is where the buzzer's second leg already is, so the resistor is now in
series with the buzzer.

### STEP 6 — Button 1 (NEXT)

Push button 1 into the board **across the centre channel**, so its legs land in
**column 18 and column 20** — two legs in the upper block, two in the lower
block.

Press it flat. It should not rock.

### STEP 7 — Button 2 (ACTION)

Same again, legs in **column 25 and column 27**.

**The pair rule.** A tactile button's four legs are joined in pairs inside the
body, and **the joined pair is the two legs on the same side of the channel**.
So the two wires you attach in STEPS 12–15 must come from **opposite sides of
the channel** — one above, one below. Two wires from the same side is a
permanent short, and the pin will read as stuck-pressed.

---

# PART 3 — Wires on the breadboard

Nine male-to-male jumpers. Arduino still unplugged.

### STEP 8 — Power in

Uno **`5V`** → **`+` rail, hole nearest column 3**

### STEP 9 — Ground in

Uno **`GND`** → **`−` rail, hole nearest column 3**

### STEP 10 — Buzzer drive

Uno **digital 8** → **column 12, lower block**

### STEP 11 — Buzzer return

**column 5, lower block** → **`−` rail**

Use the row furthest from the buzzer body, and any hole in the left half of the
rail — the rail is one electrical point, so column 5 or column 13 makes no
difference. Pick whichever the buzzer body is not sitting over.

### STEP 12 — Button 1 signal

Uno **digital 2** → **column 18, UPPER block**

### STEP 13 — Button 1 ground

**column 20, LOWER block** → **`−` rail**

### STEP 14 — Button 2 signal

Uno **digital 3** → **column 25, UPPER block**

### STEP 15 — Button 2 ground

**column 27, LOWER block** → **`−` rail**

**One wire above the channel, one below.** See the pair rule under STEP 7. Both
wires on the same side ties the pin permanently to ground.

---

# PART 4 — The three modules

Female end on the module pin, male end into the breadboard or the Uno.

### STEP 16 — LCD (4 wires)

| LCD backpack pin | Goes to |
|---|---|
| `GND` | `−` rail, hole nearest column 10 |
| `VCC` | `+` rail, hole nearest column 10 |
| `SDA` | Uno **`A4`** |
| `SCL` | Uno **`A5`** |

`A4` and `A5` are the Uno's only I2C pins. Nothing else works.

### STEP 17 — Relay (3 wires)

| Relay pin | Goes to |
|---|---|
| `GND` | `−` rail, hole nearest column 15 |
| `VCC` | `+` rail, hole nearest column 15 |
| `IN` | Uno **`D7`** |

Leave the relay's three **screw** terminals empty.

### STEP 18 — ACS712 (3 wires)

| ACS712 pin | Goes to |
|---|---|
| `GND` | `−` rail, hole nearest column 20 |
| `VCC` | `+` rail, hole nearest column 20 |
| `OUT` | Uno **`A0`** |

Leave the ACS712's two **screw** terminals empty.

### STEP 19 — Check it against this table

| Uno pin | Goes to |
|---|---|
| `5V` | `+` rail |
| `GND` | `−` rail |
| `A0` | ACS712 `OUT` |
| `A4` | LCD `SDA` |
| `A5` | LCD `SCL` |
| `D2` | column 18, upper |
| `D3` | column 25, upper |
| `D7` | Relay `IN` |
| `D8` | column 12, lower |

| On the `+` rail | On the `−` rail |
|---|---|
| Uno `5V` | Uno `GND` |
| LCD `VCC` | LCD `GND` |
| Relay `VCC` | Relay `GND` |
| ACS712 `VCC` | ACS712 `GND` |
| | column 5, lower (buzzer) |
| | column 20, lower (button 1) |
| | column 27, lower (button 2) |

Everything in columns 0–30. Nothing in the bottom rails.

---

# PART 5 — Firmware

The code is already changed and verified for this build — AC mode on, passive
buzzer driven with `tone()`, taper default set for a real charger. 109/109 core
tests pass and it compiles at 37% flash. **You edit nothing.**

### STEP 20 — Upload

1. Arduino IDE → **File → Open** →
   `C:\Users\kyere\Documents\codes\embedded systems\SmartSocket\SmartSocket.ino`
2. **Tools → Board → Arduino AVR Boards → Arduino Uno**
3. Plug the Arduino into the PC with the USB cable
4. **Tools → Port** → pick the COM port that appears
5. Click the **arrow** button. Wait for `Done uploading`

---

# PART 6 — Bench test. No mains yet.

### STEP 21 — Watch the boot

The LCD shows:

```
Smart Socket
Calibrating...
```

then:

```
READY      0.00A
00:00:00  pk0.00
```

**Backlight on but no characters?** Almost always contrast, and it is worth
knowing what to look for.

Turn the small blue screw on the LCD's back board **slowly through its whole
range, both directions**, and watch for a row of solid dark blocks.

- **Blocks appear at some setting** → the panel is fine. Back the screw off
  until they just fade, and the text will be there
- **No blocks anywhere in the range** → the panel is not being driven at all.
  Check LCD `VCC` on the `+` rail and `GND` on the `−` rail

The I2C address needs no attention: `HalDisplay::begin()` probes `0x27`, and
falls back to `0x3F` if only that one answers the bus. Those are the only two
addresses these backpacks appear at, because the address jumpers ship open — a
PCF8574 lands on `0x27`, a PCF8574A on `0x3F`, and the boards are not marked.

### STEP 22 — Listen for the relay

The relay opens first, then **closes** a moment later once calibration is done.
So you hear a click, and the relay LED finishes **ON**.

That is correct and deliberate. `SocketController::enter(State_Idle)` calls
`relay_.setClosed(true)`, because current is the only way to notice a device
being plugged in — an open relay would make the socket blind.

If the LED is **off** at `READY`, the module is active-HIGH: open
`SmartSocket\src\hal\HalPins.h`, change `RelayActiveLow = true` to `false`, and
upload again.

### STEP 23 — Button 1

Tap it. The screen changes to `Cut at / Taper`. Tap again for `Cutoffs /
Saved`. Tap again to return.

Nothing happens? You used two legs on the same side of the gap. Redo STEP 6.

### STEP 24 — Button 2

Hold it for 2 seconds. Screen shows `MANUAL POWER OFF`, relay clicks. Hold
again for 2 seconds — back to `READY`, relay clicks.

### STEP 25 — Buzzer

The socket **chirps once at power-on**, while `Calibrating...` is on screen. A
short, clear beep.

That chirp exists because nothing else would prove the buzzer works: all three
patterns the socket plays in normal use — plug detected, cutoff, fault — need a
real load, so a dead buzzer would stay hidden until the one moment it has to be
heard. Now every boot tests it.

**Silent?**

1. Check the chain: `D8` → col 12 lower → resistor → col 8 lower → buzzer →
   col 5 lower → `−` rail
2. If the wiring is right, the element's resonant frequency is probably not
   1500 Hz. Upload `BuzzerTest/BuzzerTest.ino`, open **Serial Monitor at 9600**,
   note which tone is loudest, put that number into `BuzzerToneHz` in
   `src/hal/HalIo.h`, and re-upload `SmartSocket.ino`
3. If only the steady-HIGH step of `BuzzerTest` sounds, it is an active buzzer
   after all — revert `HalBuzzer::setSounding` in `HalIo.cpp` to a plain
   `digitalWrite`

**Every one of STEPS 21–25 must work before you touch mains.**

---

# PART 7 — Mains

### STEP 26 — Power down

Unplug the Arduino from the PC. The plug stays out of the wall.

### STEP 27 — Find the LIVE core

Your lead is 2-core white, so both wires look identical. The plug is the only
thing that can tell them apart.

Open the plug. Hold it **face towards you, earth pin at the top**. The
**right-hand** pin is `L` (live). The left is `N` (neutral).

See which wire lands on `L`. **Mark it** — tape, marker, anything.

Switch the wrong one and you switch neutral: the socket's live pin stays live
while the LCD says `POWER CUT`. It looks like it works and it is not safe.

### STEP 28 — Move the relay wire from `NC` to `NO`

You currently have a wire in `COM` and a wire in `NC`. `NC` is backwards.

- **`NC`** is live when the relay is off. A crashed Arduino, a pulled USB lead
  or a dead 5 V rail would all leave the socket permanently live — and the
  firmware opens the relay at boot, so `NC` would switch the socket *on*.
- **`NO`** is dead until the firmware closes the relay. Anything going wrong
  falls back to power off.

Loosen `NC`, move that wire to `NO`. Leave `NC` empty. `COM` stays where it is.

### STEP 29 — Live into the rocker

Plug **LIVE** (the wire you marked) → **rocker blade 1**.

The blades do not fit a breadboard. Strip 15 mm, wrap it tightly round the
blade, tape over it. A spade crimp is better if you have one. Either blade can
be either side.

### STEP 30 — Rocker into the relay

**Rocker blade 2** → relay **`COM`** screw.

### STEP 31 — Relay into the sensor

Relay **`NO`** screw → **ACS712 screw 1**.

Either ACS712 screw can be either side. The firmware uses the size of the
reading, not its direction.

### STEP 32 — Sensor into the socket

**ACS712 screw 2** → socket faceplate **LIVE** terminal.

### STEP 33 — Neutral straight through

Plug **NEUTRAL** → socket faceplate **NEUTRAL** terminal, using the connector
strip.

Socket **EARTH** terminal: leave it **empty**. Write
`NO EARTH — CLASS II ONLY` on the faceplate.

### STEP 34 — Tug every joint

Five mains connections:

```
plug LIVE      -> rocker blade 1
rocker blade 2 -> relay COM
relay NO       -> ACS712 screw 1
ACS712 screw 2 -> socket LIVE
plug NEUTRAL   -> socket NEUTRAL
```

Every screw must be tightened onto **bare metal, not insulation**. Pull each
wire. If it moves, it is not connected. A loose screw is the number one reason
this reads `0.00A` later.

Trace the chain once with your finger, out loud.

---

# PART 8 — First live test

### STEP 35 — Rocker OFF

### STEP 36 — Arduino to the PC

Wait for `READY 0.00A`.

### STEP 37 — Plug into the wall

### STEP 38 — Rocker ON

The LCD must still read **`0.00A`** — nothing is in the socket yet.

Anything else: rocker off, unplug from the wall, go back to STEP 34.

### STEP 39 — Plug in the laptop charger

Class II charger, laptop attached, screen on, battery below about 90%.

You should hear a short beep, see `SETTLE`, then after 5 seconds `CHARGING`
with a live current.

### STEP 40 — Read the current. This is the number everything depends on.

| Reading | Meaning |
|---|---|
| **0.13 A or more** | Good. Go on |
| 0.07 – 0.12 A | Marginal — the socket may not separate charging from full |
| 0.00 A | The mains chain is open. Redo `COM`, `NO`, both ACS712 screws |

The thresholds are sized for **230 V mains**, where every current is 2–3× smaller
than on the old battery rig. A 35 W laptop charger draws 0.15 A. One ADC count is
26 mA, so the whole charging signal is about six counts — see the note at the top
of the threshold block in `Config.h` before changing any of them.

### STEP 41 — Check the threshold

Tap **Button 1** for the Detail screen. `Cut at` is the level it will cut
below. It worked that out from your load by itself.

### STEP 42 — Calibrate, only if the reading is clearly wrong

`SmartSocket\src\core\Config.h`, line `SensorMvPerAmp = 185`:

- Reads too high → raise it (190, 195)
- Reads too low → lower it (180, 175)

Upload again after each change.

---

# PART 9 — The demo

### STEP 43 — Real version, about 15 minutes

Start with the laptop at about 95%. Let it fill. At 100% the charger's draw
falls to whatever the laptop needs to run, the socket sees the drop, waits 90
seconds, then:

```
FULL - POWER CUT
ACTION=check now
```

Three beeps, twice. The relay clicks. Power stays off.

### It re-checks by itself

The socket's only sensor sits **downstream of the relay**, so while power is cut
it is blind — it cannot tell an empty socket from a full laptop from a phone at
5%. Rather than wait for a button press, which is the only input a blind socket
has left, it closes the contacts for **8 seconds** every so often and looks:

- **A real charging current appears** → something has changed, resume charging
- **Little or nothing** → open again and wait

How long it waits depends on what was drawing when power was cut:

| At cutoff | Re-checks every |
|---|---|
| Something still drawing — a device is sitting there, full | **15 min** |
| Nothing drawing — the socket is probably empty | **60 s** |

So plugging something new in starts it within a minute, with no button press.

This is **not** auto-rearming, which would restart the exact trickle charging
this product exists to prevent. Auto-rearming leaves the relay closed; this
closes it for seconds at a time and only stays closed on positive evidence. An
8-second probe every 15 minutes is a 0.9% duty cycle.

**ACTION still works** — tap it to re-check immediately instead of waiting. It
is no longer required.

### STEP 44 — Fast version, about 3 minutes

Put a 4-way extension into the smart socket.

1. Laptop charger **and** a phone charger with a phone on it. Total ≈ 0.5 A
2. Wait **1 full minute**. The firmware refuses to cut before then
3. Unplug the laptop charger. Current drops to ≈ 0.09 A, well under `Cut at`
4. Hands off for 90 seconds. Any bounce back up restarts the 90 s
5. Three beeps, relay clicks, `FULL - POWER CUT`

### STEP 45 — Show the stats

Tap **Button 1** twice: `Cutoffs 1 / Saved 0h01m`.

Tap **Button 2** once to re-arm. Run it again and watch `Taper` change slightly
on the Detail screen. That is the adaptive learning tuning itself to your load.

---

# PART 9B — Powering the finished product

The Arduino cannot stay tethered to your laptop. Here is what it actually needs.

## The upload is one-time

Uploading writes the firmware into the Arduino's **flash memory**, which is
permanent. It survives being unplugged, and it survives being powered from
something else. Once STEP 20 is done you never need the PC again.

After that, the USB cable is carrying **power only**.

## Power it from a 5 V USB phone charger

Any ordinary phone charger plus a USB-A-to-USB-B cable (the square-ended
printer-style plug that fits the Uno). Plug the charger into **a different wall
outlet**, or a multi-way adapter alongside the smart socket's own plug.

That is the whole answer. The enclosure already supports it: the Arduino's
connector edge faces the **left wall**, so the USB socket is reachable with the
box closed.

## Never power it from the socket the relay switches

This one is not a preference, it is a deadlock:

```
relay opens  ->  socket dead  ->  Arduino loses power
             ->  relay cannot be commanded closed  ->  socket stays dead
```

The socket would cut power once and never come back, because the thing that
decides to come back died with it. Both outlets on your twin socket are fed from
the same terminals, so **neither** of them can power the Arduino.

## Why not the battery

The parts list includes an 18650 and a holder, and they are genuinely not usable
here:

- **One cell is 3.7 V.** The Uno needs 7–12 V on `VIN`, or a regulated 5 V on the
  `5V` pin. 3.7 V is too low for either.
- **Two in series is 7.4 V** and would run — `BUILD_GUIDE.md` PART 13 covers it —
  but you have a single-cell holder, and a socket that is supposed to sit on the
  wall permanently would go flat in a day or two.

A mains appliance runs off mains. Leave the holder out.

## Never both at once

USB power and a battery on `VIN` at the same time is how Uno regulators die.
One source only.

---

# PART 10 — Into the enclosure

## What to print

**Three files, all from `enclosure/stl_slim/`:**

| File | Weight |
|---|---|
| `SmartSocket_Base.stl` | ~217 g |
| `SmartSocket_Lid.stl` | ~52 g |
| `SmartSocket_Shelf.stl` | ~71 g |

**Total ~340 g, about 849 GHS at 2.50/g.** Ask the printer for his slicer's
quote at 20% infill — it will be less than that solid-volume figure.

**Do not print** `SmartSocket_Assembly.stl` — it is the parts fused into one
solid, for looking at only. `SmartSocket_FitCheck.stl` is an optional 39 g test
coupon.

All three print flat, as exported, **no supports, do not rotate anything**. The
lid is already flipped. Layer 0.2 mm, 4 perimeters, brim on the base.

Assembled size is **170 × 176 × 61 mm**.

## How the box is arranged

The design keeps 230 V and low voltage in separate compartments, using printed
geometry rather than a partition bolted in later.

| Zone | Height | What lives there |
|---|---|---|
| **Front zone** — LV | full height | Arduino, and the LCD/buttons/buzzer on the lid above it |
| **Lower deck** — LV | Z 0–22 mm | Breadboard, under the shelf |
| **Mains deck** — 230 V | Z 24–56 mm | Socket faceplate, relay, ACS712, on top of the shelf |

**The shelf is the barrier.** Exactly one route crosses it: a 16 × 6 mm slot
carrying **six low-voltage wires and nothing else** — relay `VCC`/`GND`/`IN` and
ACS712 `VCC`/`GND`/`OUT`. No mains wire goes through it.

Two things that changed from the earlier design, so ignore anything that says
otherwise:

- **The socket faceplate mounts into the lid**, not loose on a lead. The lid has
  the opening and slotted fixing holes; the socket's own screws self-tap
  wherever they land, so any fixing centre from 90 to 150 mm works.
- **The rocker snaps straight into the base's front wall.** There are no bezel
  plates any more.

## Screws

All M3 self-tapping into printed pilot holes. Ordinary M3 machine screws cut
their own thread in PLA perfectly well.

| Where | Count | Head |
|---|---|---|
| Lid to base | 6 | countersunk, ~16 mm |
| Shelf to its posts | 6 | pan |
| Arduino to floor standoffs | 4 | pan, ~8 mm |
| LCD to lid bosses | 4 | pan, ~6 mm |

**Buy an assortment of M3 self-tappers in 8 / 12 / 16 / 20 mm.** Cheap, and it
removes the guesswork on lengths.

Also: hot glue, for the two buttons, the buzzer and the rocker.

## Assembly order

Rocker off, plug out of the wall, Arduino unplugged for all of this.

### STEP 46 — Rocker into the front wall

It snaps into the opening in the **base's front wall** from outside. Dab of glue
behind it once it clicks in.

### STEP 47 — Arduino

Four M3 into the floor standoffs in the front zone. **Its connector edge faces
the LEFT wall** so you can reflash without opening the box.

### STEP 48 — Breadboard

Peel the backing and stick it down on the lower deck floor, behind the Arduino.

### STEP 49 — Wire the lower deck now, before the shelf goes on

Once the shelf is screwed down you cannot reach under it without taking it off
again. Do all the breadboard wiring, then feed the **six** relay and ACS712
signal wires up through the pass-slot, leaving plenty of slack on the deck side.

### STEP 50 — Shelf

Drop it onto its eight posts and screw down the six outer ones. The middle pair
is plain support — a screw there could not be reached past the breadboard.

### STEP 51 — Relay and ACS712 onto the shelf

Relay to the left, ACS712 to its right, both in the strip in front of where the
socket will sit. Connect the six signal wires you fed up in STEP 49.

### STEP 52 — Mains cord

In through the back wall, above the shelf. **Tie a knot inside** so a pull on
the lead cannot pull on a screw terminal.

### STEP 53 — Lid parts

LCD onto its four bosses. Hot-glue the two buttons and the buzzer into their
collars from underneath. Move them off the breadboard and wire them back to the
same columns with **150 mm of slack**, so the lid lifts off and sits beside the
box without pulling anything loose.

### STEP 54 — Socket into the lid

From the outside. Its own screws self-tap into the slotted holes.

### STEP 55 — Close up

Wire the mains side to the socket's terminals, then six countersunk M3 down the
left and right edges.

### STEP 56 — Repeat STEPS 35–40

One last live check with the box closed.

## If something is wrong

| What you see | What it means |
|---|---|
| `0.00 A` with a charger plugged in | Open mains chain. Redo `COM`, `NO`, both ACS712 screws |
| Never leaves `READY` | Load under 0.12 A. Bigger charger |
| Cuts at exactly 60 s with no taper | Peak was under 0.25 A, so it took the trickle path. Bigger charger |
| Goes to `READY` instead of `FULL` | Current fell under 0.05 A and read as unplugged. Leave a small load on |
| Cuts too early | `TaperConfirmMs` 90000 → 180000 in `Config.h` |
| Buzzer silent | Trace `D8` → col 12 lower → resistor → col 8 lower → buzzer → col 5 lower → `−` rail |
| A button does nothing at all | Two different faults look identical here. Either the wires are not in the columns the legs actually occupy, or they are both on the same side of the channel — a pin held permanently low fires one `LongPress` at 1.5 s and then nothing, and `loop()` ignores `LongPress` on NEXT. Read the legs, then swap the `D2`/`D3` jumpers at the Uno end to tell pin from button |
| LCD lit but blank | Blue screw on the backpack. Turn it the whole way, both directions, watching for dark blocks |
| LCD dead, no backlight | `VCC`/`GND` on the wrong rails |
| Relay backwards | `RelayActiveLow = false` in `HalPins.h` |
| `! OVERCURRENT !` | Rocker off, unplug from the wall, recheck every mains joint |
