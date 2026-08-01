# Stage 2 — the load, the battery, and the first real cutoff

Stage 1 is done: sensor, relay, buzzer and both buttons all work. Stage 2 adds the
thing being "charged", and gets the relay to cut it off for real.

Highest voltage anywhere: **3.7 V**. It cannot hurt you.

**The one real rule:** the battery's red wire must never touch its black wire, and
no resistor leg on one rail may touch a leg on the other rail. That is a short
across the cell. The cell gets hot, fast. Everything below is arranged so this
cannot happen by accident, as long as you keep the legs straight.

---

## What you are adding

Only **one** new thing goes into the breadboard: the resistor bank. Everything
else is four wires between screw terminals.

```
battery +  ->  relay COM screw
relay NO   ->  ACS712 screw 1
ACS712 screw 2  ->  LOAD RAIL A
11 resistors across the two load rails
LOAD RAIL B  ->  battery -
```

**None of this touches the Arduino's `+`/`−` rails.** That is different from
`BUILD_GUIDE.md` Part 9 on purpose. The ACS712's two screw terminals are
electrically separate from its `VCC`/`GND`/`OUT` pins, so the battery loop needs
no shared ground with the Uno — and keeping 0.37 A out of the Arduino's ground
rail is what removes the phantom 0.10–0.12 A reading you saw in Stage 1.

---

# PART A — Get ready

### Step A.1 — Kill the power
Unplug the USB cable from the Arduino. No lights.

### Step A.2 — Take the cell out
Lift the blue 18650 out of its holder. Set it on the far side of the table. It
stays out until Step D.2.

### Step A.3 — Count out 11 resistors
From the bag in your photo, take **11 resistors**. Check all 11 have the **same
colour bands: brown – brown – black – black – brown**. That is 110 Ω.

You should still have plenty left in the bag. The one already wired to the buzzer
on `D8` stays where it is — do not steal it.

---

# PART B — Choose the two load rails

The long strips down the top and bottom edges of the breadboard are the **rails**.
There are four of them: two along the top edge, two along the bottom edge.

### Step B.1 — Find the pair Stage 1 is using
Look at where the wire from the Arduino's **`5V`** pin lands. The rail it goes
into, and its partner right next to it, are the **Stage 1 pair**. They have your
LCD, relay and sensor `VCC`/`GND` wires in them.

**Do not touch that pair for the rest of Stage 2.**

### Step B.2 — Use the pair at the opposite edge
If Stage 1 is on the bottom edge, your load rails are the **two strips along the
top edge**. If Stage 1 is on the top, use the bottom two.

Give them names, because they are not power rails any more:

- **LOAD RAIL A** — the strip closer to the lettered rows
- **LOAD RAIL B** — the strip on the outer edge of the board

Ignore the red and blue printing. Those two strips are now just "two long
connected points".

### Step B.3 — Check they are completely empty
Run your eye along the whole length of both. **No jumper, no resistor leg,
nothing.** If anything at all is plugged into either strip, pull it out or move
the bank to the other edge.

### Step B.4 — Remember which half
Your breadboard's rails are **split in the middle**, around column 30. The left
half of a rail and the right half are **not** connected.

**Pick the right-hand half — columns 33 to 63 — and keep every single thing in
this stage inside it.**

### Step B.5 — Never bridge the two pairs
Some tutorials tell you to run a jumper from `+` to `+` across the board. **Do
not.** That would put the battery onto the Arduino's 5 V rail.

---

# PART C — Build the resistor bank

Good news: **within one rail strip, every hole is the same electrical point.** So
the exact holes do not matter — only that one leg is in RAIL A and the other is
in RAIL B, and that you stay in the right-hand half.

### Step C.1 — Bend the first resistor
Hold a resistor and bend both legs straight down, about 8 mm apart — just wide
enough to span from RAIL A across to RAIL B.

### Step C.2 — Plug it in above column 40
Push it in so:
- **one leg → LOAD RAIL A**, around **column 40**
- **the other leg → LOAD RAIL B**, straight across from it

It sits like a tiny bridge across the two strips. Push until the legs feel
gripped.

### Step C.3 — Add ten more, working to the right
Do the same again at column 41, 42, 43 … up to column 50. **Eleven resistors,
side by side, all crossing the same two strips.** They look like a row of little
fences.

### Step C.4 — The safety check
Look along the row from the end. Every resistor's two legs go **straight down**,
one into each strip.

**No leg may lean over and touch a leg from the other strip.** If two legs from
opposite strips touch, that is a dead short across the battery. Straighten
anything that leans.

(The firmware would catch it — it cuts the relay above 4.5 A within a quarter of a
second — but do not make it prove that.)

### Step C.5 — Count them
Point at each one and count out loud. **Eleven.** Every threshold in the firmware
is positioned against that number, so 10 or 12 will change what you see later.

---

# PART D — The four wires

Screw terminals are the green blocks with screws on top: **3 on the relay**
(marked `COM`, `NO`, `NC`) and **2 on the ACS712** (the small blue board in your
photo, with the green block on its right-hand edge).

For every screw: loosen it, push the bare metal of the wire fully in, tighten
down onto the **metal, not the plastic**. Then tug the wire gently. If it moves,
it is not connected. **Loose screws are the number one reason this stage reads
`0.00A`.**

### Step D.1 — Free the battery wires
In your photo the holder's red and black wires end in a **black barrel plug**.
You do not need that plug — one 3.7 V cell cannot run an Uno anyway (that needs
two cells; see `BUILD_GUIDE.md` Part 13).

Either **unscrew the plug's plastic collar** if it is the screw-terminal kind and
pull the wires out, or **cut the plug off** and strip about 10 mm of insulation
off each wire.

Then, on the **black** wire only: twist the loose strands tightly together,
clockwise, into one stiff point. That is what will go into a breadboard hole.

### Step D.2 — Wire 1: battery red (+) → relay `COM`
Battery **red** wire into the relay's **`COM`** screw. Tighten. Tug it.

`COM` is the middle screw of the three on most relay boards, but go by the
printed label, not the position.

### Step D.3 — Wire 2: relay `NO` → ACS712 screw 1
Take a **male-to-male jumper** (the coloured ones in your photo). Put one end in
the relay's **`NO`** screw and the other in **either** of the ACS712's two
screws. Tighten both.

`NO` means "normally open" — that gap is what the relay closes and opens. That is
the whole product in one wire.

### Step D.4 — Wire 3: ACS712 screw 2 → LOAD RAIL A
Another male-to-male jumper. One end in the ACS712's **other** screw. The other
end into **LOAD RAIL A, around column 60** — the same strip as your eleven
resistor legs, but far to the right of them so you can pull resistors without
knocking it out.

Which of the two ACS712 screws you used for which wire does not matter. The
firmware uses the size of the reading, not its direction.

### Step D.5 — Wire 4: LOAD RAIL B → battery black (−)
Push the twisted **black** battery wire directly into **LOAD RAIL B, around
column 60**. Straight down, firmly.

If it will not stay, twist the strands tighter, or tape the wire down to the
table so it is not pulling sideways on the hole.

### Step D.6 — Trace the ring with your finger
Say it out loud as you go:

> red → `COM` … `NO` → sensor … sensor → RAIL A … eleven resistors … RAIL B →
> black.

Four wires, eleven resistors, one closed ring. Nothing from this ring reaches the
Arduino's rails.

### Step D.7 — Last look at the Stage 1 pair
Check nothing you just did landed in the Stage 1 rails, and that no wire runs
between the two rail pairs.

---

# PART E — Run it

### Step E.1 — Upload the new firmware
**The numbers changed today** for your 110 Ω resistors. The old ones assumed
100 Ω and would have misread this demo.

1. Plug the USB back in. **Leave the cell out.**
2. Arduino IDE → open `Bringup/Bringup.ino` → upload.
3. **Tools → Serial Monitor**, and set the speed box at the bottom right to
   **9600**.

### Step E.2 — Read the startup report
You should see:

```
Calibrating sensor zero... done. resting value = 512
Button1 (D2): ok, released
Button2 (D3): ok, released
Load bank: 11 x 110 ohm = 0.37A
Demo: start with all 11 in, wait 1 min, then pull down to 4 and keep your hands off for 90 s.
  cuts at 5 or fewer; never go below 3 - at 2 it reads as unplugged.
[READY]  now 0.00A
```

The board is telling you the demo itself, worked out from its own constants.

**It must say `0.00A` with the cell out.** If it does not, type `z` and press
Enter to re-measure zero. If a reading still creeps back, one of your load wires
is touching the Arduino's rails.

### Step E.3 — Drop the cell in
Red is `+`. Put the 18650 into its holder.

Within a second or two:

```
>> load detected
[SETTLE]    now 0.37A  (~11 of 11 resistors)
```

and five seconds later:

```
>> charging
[CHARGING]  now 0.37A  (~11 of 11 resistors)   peak 0.37A   cut below 0.18A   running 6s
```

**`(~11 of 11 resistors)` is the number that matters.** The board is counting your
resistors from the current alone. If it says 11 and you built 11, the sensor and
the maths agree and the rest will work. If it says 8 or 14, stop and fix the
reading (table below).

The resistors will get warm. 0.12 W each in a ¼ W part. Normal.

### Step E.4 — Wait one full minute
The firmware refuses to cut off in the first 60 seconds. That guard is what stops
an already-full phone being cut the instant you plug it in.

### Step E.5 — Pull down to 4
Now pull out **seven** resistors — the **right-hand seven** — leaving the
**leftmost four** in place. Take them out one at a time and watch the count fall:

| Resistors left | Reads about | Serial says |
|---|---|---|
| 11 | 0.37 A | `~11 of 11` |
| 8 | 0.27 A | `~8 of 11` |
| 6 | 0.20 A | `~6 of 11` |
| **4** | **0.13 A** | `~4 of 11` |

`cut below` is **0.18 A**, so at four you are clearly under it and the clock
starts:

```
>> current dropped - starting 90s confirmation
[CHARGING]  now 0.13A  (~4 of 11 resistors) ... LOW for 12s of 90s
```

**Stop at 4. Never go below 3.** At two resistors (0.07 A) the reading is close
enough to zero that the socket decides you unplugged the device, and it goes
quietly back to `READY`. Not a fault — just the wrong answer, and it ends the run.

### Step E.6 — Hands off for 90 seconds
Do not touch the board. Every time the current pops back above 0.18 A the 90
seconds starts again — that is the guard that stops a momentary dip cutting a real
laptop off mid-charge.

Then:

```
****************************************
  FULL - POWER CUT
  cutoff #1
  press button2 to re-arm
****************************************
```

Three beeps, twice. The relay clicks. The current goes to zero and the resistors
go cold.

**That is the entire product working.**

### Step E.7 — Do it again
Tap **Button 2** to re-arm. Put all eleven resistors back. It will do the whole
thing again.

---

## If something is wrong

| What you see | What it means |
|---|---|
| `0.00A` with the cell in | The ring is open. Nine times out of ten it is a screw tightened onto insulation. Redo `COM`, `NO` and both ACS712 screws |
| Resistor count off by 2 or more | Sensor calibration. In `Bringup.ino` change `SENSOR_MV_PER_AMP`: reading too high → raise it (190, 195); too low → lower it (180, 175). Re-upload |
| `*** PEAK TOO LOW ***` | Fewer than about 8 resistors are actually conducting. It will cut at 60 s as a "trickle", which looks like a real cutoff but skips the taper. Reseat every leg |
| Never leaves `READY` | Under 0.12 A, so under four resistors' worth is getting through. Check the bank and the screws |
| Goes back to `READY` mid-run | You went below three resistors, or a leg fell out |
| A reading with the cell out | Type `z`. If it creeps back, a load wire is touching the Arduino's rails |
| Nothing happens after 90 s | Did the current stay under `cut below` the whole time? Any bounce upward restarts the clock |
| `!! OVERCURRENT` | The battery is shorted. **Take the cell out now**, then look for two legs touching across the rails |
| Cell gets hot | Same thing. Cell out immediately |

---

## Later: the rocker switch

Your rocker has **two spade blades, not three pins** — plain on/off, no middle
pin, either blade can be either side. The blades do not fit a breadboard, so it is
not worth fighting on the first run.

Once the cutoff works, put the switch between the battery and the relay:

```
battery +  ->  rocker blade 1
rocker blade 2  ->  relay COM
```

To land a wire on a blade: strip 15 mm, wrap it tightly around the blade, tape
over it.

The enclosure already has a 30 × 24 mm opening in the back wall and three
interchangeable bezel plates for this switch — `enclosure/README.md` section 5.

---

## What changed in the firmware today

Your resistors measured **110 Ω**, not the 100 Ω the original guide assumed. With
11 in parallel the load can only sit on multiples of 33.6 mA:

```
resistors in |  1    2    3    4    5    6    7    8    9   10   11
current (mA) | 34   67  101  134  168  202  235  269  302  336  370
```

One ADC count is 26 mA — most of a whole step. So a threshold sitting *on* a step
is a coin toss, and two of them were:

| Constant | Was | Now | Why |
|---|---|---|---|
| taper threshold | 39% of peak | 49% of peak | 39% put the cut 10 mA from the 4-resistor step; 49% sits midway between steps 5 and 6 |
| `UnplugMa` | 80 mA | 50 mA | 80 mA was 13 mA under the 3-resistor step — the demo's own end state was one count of drift from reading as "unplugged" |
| `TaperFloorMa` | 100 mA | 115 mA | 100 mA landed exactly on the 3-resistor step (101 mA) |
| `PlugDetectMa` | 150 mA | 120 mA | Between steps 3 and 4 instead of on step 5 |
| `MinSessionPeakMa` | 300 mA | 250 mA | A full bank is 370 mA nominal, but a low cell or a 15%-low calibration lands near 300 — and dropping under it silently switches to the trickle path |

Changed in both `Bringup/Bringup.ino` and `SmartSocket/src/core/Config.h` so the
two rigs behave identically. Core suite 109/109; `Bringup` compiles at 28% flash.
