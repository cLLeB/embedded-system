# Fitting the build into the enclosure

You have the three printed parts. This is the order to put everything into them,
and the two places where the printed geometry does not match what the wiring
needs.

This continues from `docs/FINAL_BUILD.md`. That document gets you a working
socket lying on the bench; this one gets it into the box. **Everything in
`FINAL_BUILD.md` STEPS 1–45 must already work before you start here.** Boxing a
build that has never run means every fault you meet afterwards has two possible
causes.

Where the two documents disagree, this one is right — see "Corrections to
FINAL_BUILD" at the end.

---

## Orientation — read this first, everything below depends on it

Put the **base** on the bench, open side up, and turn it so the **rectangular
switch cutout is on the wall nearest you**.

That is now fixed for the rest of the document:

| Wall | How you know it | What goes through it |
|---|---|---|
| **FRONT** (nearest you) | 13.4 × 19.6 mm rectangular cutout, low down | Rocker switch |
| **LEFT** | one wide slot, ~15 mm tall, near the front corner | Arduino USB — **5 V only** |
| **BACK** | round 8 mm hole high up, with two small holes either side | Mains cord, 230 V |
| **RIGHT** | plain, apart from vent slots | nothing |

Inside you are looking at:

- **Front zone** — the full-height open area along the front wall, about 58 mm
  deep. Four short posts for the Arduino, and a rectangle of four L-brackets to
  their right (the 18650 bay, unused).
- **Eight round posts**, 22 mm tall, filling the rest of the box. The shelf
  lands on these.
- **Six tall pillars** up the left and right edges, running floor to rim. Those
  are the lid screw bosses — do not confuse them with the shelf posts. Shelf
  posts stop at 22 mm; bosses go all the way up.

The **shelf** has a 2 mm wall standing up along one long edge. **That wall faces
the front.** It is the barrier between 230 V and everything else.

The **lid**, seen from the underside, has: a large rectangular opening with two
small bridging tabs (the socket), a smaller rectangular window with four 6 mm
bosses around it (the LCD), two square collars and one round collar with a ring
of small holes (buttons and buzzer).

---

## The zones, and why they matter

| Zone | Height above the floor | Contents | Voltage |
|---|---|---|---|
| Front zone | 0 – 56 mm, full height | Arduino; LCD, buttons, buzzer on the lid above | 5 V |
| Lower deck | 0 – 22 mm, under the shelf | Breadboard | 5 V |
| Mains deck | 24 – 56 mm, on the shelf | Socket, relay, ACS712 | **230 V** |

One route crosses between them: the raised collar with a 16 × 6 mm slot near the
back right of the shelf. Six wires go through it — relay `VCC`/`GND`/`IN` and
ACS712 `VCC`/`GND`/`OUT` — and nothing else.

---

## Buy list

**Screws.** All M3, all self-tapping into printed pilot holes. Ordinary M3
machine screws cut their own thread in PLA.

| Where | Count | Type |
|---|---|---|
| Lid to base | 6 | **M3 × 16 countersunk** |
| Shelf to posts | 6 | M3 × 12 pan |
| Arduino to standoffs | 4 | M3 × 8 pan |
| LCD to lid bosses | 4 | M3 × 6 pan |

An 8/12/16/20 mm M3 self-tapper assortment covers all of it except the
countersunk six.

**Also.** Hot glue gun · a soldering iron (six joints, no way round it) ·
**about 300 mm of 6 mm sleeving or heat-shrink** for STEP 8 · six small cable
ties · a chocolate-block connector strip · a multimeter · an 8 mm drill bit or a
round file.

---

# PART A — Before anything goes in

### STEP 0 — Lengthen the wires, before you touch the box

The bench build has everything within arm's reach of everything else. The boxed
build does not: the lid becomes a separate object carrying the socket, the LCD,
both buttons and the buzzer, joined to the base by twelve wires. If those wires
are short you will be working on live-capable terminals with the lid held up in
one hand.

Cut and make these up **first**, on the bench, before anything goes into the
enclosure.

| Wires | Count | Length | Note |
|---|---|---|---|
| LCD `VCC` `GND` `SDA` `SCL` | 4 | **250 mm** | female to the backpack, male to the board |
| Button MODE, button SET | 4 | **250 mm** | soldered to the legs, STEP 18 |
| Buzzer | 2 | **250 mm** | soldered, STEP 18 |
| ACS712 out → socket `L` | 1 | **250 mm** | mains |
| Neutral block → socket `N` | 1 | **250 mm** | mains |
| Relay + ACS712 signals | 6 | **150 mm** | these cross the shelf, not the lid |
| Cord `L` → rocker, rocker → relay `COM` | 2 | **250 mm** | mains, **sleeved**, STEP 8 |

250 mm is what lets the lid lie flat on the bench beside the base with nothing
under tension. That is the position you will do all the fault-finding in.

### STEP 1 — Deburr and dry-fit

Clip off any brim or stringing. Run a knife round the inside of every pilot hole
and the socket opening.

Check three fits, with nothing wired:

1. **Lid onto base.** It should drop on with the six countersunk holes centred
   over the six pillars. If it rocks, a corner has lifted during printing — sand
   the base rim flat.
2. **Shelf onto its posts, wall to the front.** The four large holes near its
   corners should drop over the four tall lid bosses; the six small holes should
   land on six of the eight posts. The two posts at the middle of the box get no
   screw — they are plain support.
3. **Socket faceplate into the lid, from the OUTSIDE.** The body drops through
   the big opening; the plate lands on the 4 mm rim. Its two fixing screws should
   line up somewhere along the two slotted tabs.

### The two tabs across the socket opening

Two 9 mm bridges span the opening, standing 6 mm proud of the lid's inner face,
each with a 2.8 mm slot through it. **They are the socket's fixing points.** The
plate's screws sit about 60 mm either side of centre, which lands them in the
middle of a 138 mm hole with nothing to bite; the tabs put plastic back exactly
there. The slot is a slot and not a hole so that any fixing centre from 90 to
150 mm works — the screw taps its own thread wherever it lands.

**If the socket will not seat, check the side you are fitting from first.** The
plate is 146 × 86 mm and the opening is 138 × 78 mm, so it cannot pass through
from the inside. It goes in from the **outer** face, body first.

Fitted the right way round, the two slots should line up with the two brass
fixing bosses on the back of the plate. If they do, the geometry is correct and
anything still stopping it is height, not position.

**If it still will not seat from the outside**, the brass boss tubes on the back
of the plate are landing on the tabs, which stand 6 mm proud of the inner face.
This is an anticipated failure, not a scrapped lid:

1. Sit the socket in from the outside and measure the gap between plate and lid.
   That is how much tab has to go — 6 mm is the whole projection.
2. Side cutters, then a flat file, until the stumps are **flush with the lid's
   inner face.** Do not cut into the lid itself; keep the 2.2 mm of lid
   thickness with the slot still running through it.
3. You have lost most of the thread depth, so do not rely on self-tapping. The
   socket's screws go through the plate lugs, through the slot, and onto a
   **washer and nut on the inside.** If the plate's own screws are too short to
   come through, use M3 × 20.

**Screws and nuts, not glue.** Pulling a plug out of a socket applies real force,
and a glued plate comes out with the plug eventually.

### STEP 2 — Prove the electronics one more time

With everything still on the bench as `FINAL_BUILD.md` left it, power the
Arduino from USB and confirm all five: boot chirp, `READY 0.00A`, relay clicks
closed, Button 1 cycles screens, Button 2 held 2 s cuts power.

Then **unplug the Arduino and unplug the mains lead from the wall.** They stay
unplugged until PART E.

### STEP 3 — Take the lid parts off the breadboard

Pull **both buttons** and the **buzzer** out of the breadboard. They are moving
to the lid.

**Leave the 110 Ω resistor where it is** — column 8 to column 12, lower block.
The buzzer comes back to **column 8**, on the far side of that resistor.

---

# PART B — The base

### STEP 4 — Rocker into the front wall

The switch has its own snap clips and the wall is recessed to 1.5 mm behind the
cutout so they grip. Push it in **from the outside** until both clips click.

Mount it **portrait** — taller than wide — so the moulded **O** is at the bottom
and **I** at the top.

Add a dab of hot glue on the inside afterwards. A switch that pushes back
through under thumb pressure would leave a live blade loose in the box.

### STEP 5 — Arduino onto its standoffs

Four 5 mm standoffs in the front zone, on the left. The hole pattern is
asymmetric and only fits one way: **the connector edge faces the LEFT wall.**

Sight through the left-wall slot before you screw down — you should see the USB
socket and the barrel jack looking straight out of it. If you see the far edge of
the board instead, turn it 180°.

Four M3 × 8 pan screws, finger tight plus a nudge. Do not crank them; you are
cutting thread in PLA 2.4 mm thick.

*Three of four holes lining up is normal on clone boards. Use three.*

### STEP 6 — Breadboard onto the lower deck

Two low ribs run across the floor behind the front zone. They set the
breadboard's depth; the side walls set its width.

Peel the adhesive backing and press it down between the ribs, **numbers reading
normally, column 0 to the left.** Once it is stuck it does not come up cleanly,
so line it up dry first.

### STEP 7 — Rewire the lower deck, now, before the shelf goes on

Once the shelf is screwed down you cannot reach under it. Rebuild the breadboard
exactly as `FINAL_BUILD.md` PART 3 and PART 4 describe, with three changes:

- **No buttons and no buzzer on the board.** Their wires arrive later from the
  lid.
- The **six module wires** (relay `VCC`/`GND`/`IN`, ACS712 `VCC`/`GND`/`OUT`) go
  in at the breadboard end now, with the far ends loose and **at least 150 mm**
  spare. They get threaded up through the shelf in STEP 11.
- The **ten lid wires** (LCD 4, buttons 4, buzzer 2) go in at the breadboard and
  Arduino end now, loose ends up, **250 mm** spare each.

Check against this before you go on:

| Uno pin | Goes to | Fitted now? |
|---|---|---|
| `5V` | `+` rail | yes |
| `GND` | `−` rail | yes |
| `A0` | ACS712 `OUT` | long tail, loose |
| `A4` | LCD `SDA` | long tail, loose |
| `A5` | LCD `SCL` | long tail, loose |
| `D2` | Button 1 | long tail, loose |
| `D3` | Button 2 | long tail, loose |
| `D7` | Relay `IN` | long tail, loose |
| `D8` | column 12, lower | yes |

and on the board: buzzer chain is `D8` → col 12 → **110 Ω** → col 8 → *(loose
tail up to the lid buzzer)*, and the buzzer's return tail goes to the `−` rail.

Route every loose tail toward the **front** of the box. Nothing should lie where
the shelf posts are.

---

# PART C — The barrier, and the one modification you have to make

### STEP 8 — Cut the mains crossing for the rocker

**Read this whole step before you drill.**

The mains cord enters the back wall, above the shelf. The rocker is in the front
wall, below it. There is no route between them: the shelf's partition seals the
mains compartment at the front, and the only hole in the barrier is the six-wire
pass-slot, which mains must not use. The printed parts have no path for the two
wires the rocker needs.

**Take the shelf out of the box and lay it on the bench.**

Drill an **8 mm hole through the upstanding partition wall**, positioned:

- **110 mm from the end of the shelf that will sit against the left wall**
  (the shelf is 166 mm long, so this is right of centre)
- **20 mm up from the top face of the shelf plate**

That spot is clear on both sides: on the mains side the relay ends at 52 mm, the
ACS712 at 94 mm and the pass-slot collar starts at 145 mm; on the front-zone
side the LCD's rear edge stops 10 mm short of the partition.

Deburr both faces. **Fit a grommet if you have one**; if not, line the hole with
a 15 mm length of sleeving glued in place. Bare PLA edges and mains insulation
should not be left to rub.

**The two rocker wires must be sleeved for their whole run inside the box.**
6 mm heat-shrink or PVC sleeving over the pair, from the hole to the switch. That
sleeve is the second layer of insulation that replaces the compartment wall you
just breached.

**What this costs.** The design's rule is that 230 V never leaves the mains deck.
This breaks it for two wires. In exchange they are sleeved, they hug the right
wall, they stay 80 mm from the Arduino, they never cross the lower deck and they
never pass over the breadboard. That is how a mains flex crosses a low-voltage
area in a commercial appliance, and it is the only option that does not mean
reprinting.

**If you will not accept mains in the low-voltage zone:** leave the rocker out,
blank the front cutout with a printed plug or a strip of PLA and hot glue, and
use the wall socket's own switch as the isolator. You lose the local isolator, so
"unplug from the wall before opening" becomes the only rule protecting you —
which is exactly what the lid already says.

### STEP 9 — Relay and ACS712 onto the shelf

Both clip into L-brackets on the shelf's top face, in the strip in front of where
the socket will sit. Brackets with a **lip** hold the board down; push one corner
under the lip and press the opposite corner in.

- **Relay** — the larger pocket, on the **left**. Its three screw terminals
  should face **forward**, toward the partition, so a screwdriver reaches them
  with the lid off.
- **ACS712** — the smaller pocket to its right, long axis running front-to-back.
  Its two screw terminals face forward for the same reason.

Leave all five screw terminals empty for now.

### STEP 10 — Six wires down through the pass-slot

Connect the six signal wires to the two modules' header pins:

| Module pin | Colour convention |
|---|---|
| Relay `VCC`, ACS712 `VCC` | red |
| Relay `GND`, ACS712 `GND` | black |
| Relay `IN`, ACS712 `OUT` | anything else |

Feed all six free ends **down** through the raised collar's slot, near the back
right of the shelf. Leave a loop of slack on the shelf side so the modules can be
lifted out later without pulling anything.

### STEP 11 — Shelf down

Lower the shelf onto its eight posts, **partition wall to the front**, guiding
the six wires down through the slot as it goes and keeping every low-voltage tail
from PART B clear of the underside.

The four large holes clear the tall lid bosses. Six M3 × 12 pan screws into the
six posts that have holes above them. The two middle posts get nothing.

**Do not overtighten.** These six screws carry the whole mains deck; a stripped
post cannot be re-cut.

Now join the six wires to the breadboard and Arduino, per the table in STEP 7.

---

# PART D — Mains

Plug out of the wall. Arduino unplugged. Rocker off.

### STEP 12 — Cord in, strain relief on

Feed the mains cord in through the round hole in the **back wall**, above the
shelf. Bring in **150 mm** of cord — enough to reach the far end of the deck.

Thread a cable tie through the two small holes beside the entry, round the cord,
and cinch it hard. **Pull the cord from outside.** If it moves at all, tighten
again. That tie is the only thing standing between a tug on the lead and a screw
terminal.

Identify **LIVE** before you go further — `FINAL_BUILD.md` STEP 27. It is the
core landing on the plug's right-hand pin with the plug face toward you and the
earth pin up. Mark it.

### STEP 13 — The chain

Five joints, in this order. Every one goes into a screw terminal or a
chocolate-block, onto bare copper, never onto insulation.

```
cord LIVE  ──┐
             │  (sleeved pair, through the STEP 8 hole,
             │   down the RIGHT wall, along the FRONT wall)
             └──►  rocker blade 1
                   rocker blade 2  ──►  back up through the same hole
                                        │
                                        ▼
                                   relay COM
                                   relay NO   ──►  ACS712 screw 1
                                                   ACS712 screw 2  ──►  ┐
                                                                        │
cord NEUTRAL ──► chocolate block ──────────────────────────────────────►│
                                                                        ▼
                                                             flying leads to the
                                                             socket in the lid
```

Working notes:

- **Rocker blades do not take a screw.** Strip 15 mm, wrap tight round the blade,
  heat-shrink over it. A spade crimp is better. Either blade either side.
- **Relay `NO`, never `NC`.** `NC` is live whenever the Arduino is not driving
  the relay, which includes every crash and every unplugged USB lead. `NO` fails
  to power-off. Leave `NC` empty.
- **Either ACS712 screw can be either side.** The firmware uses the size of the
  reading, not its direction.
- **Route the sleeved rocker pair round the walls**, not across the middle: out
  of the hole, right to the right-hand wall, down to the floor, forward along the
  right wall, then left along the front wall to the switch. Cable-tie it at three
  points. It must not lie on the battery bay, the Arduino, or any lid wire.

### STEP 14 — Two flying leads up to the lid

The socket lives in the lid, so the last two mains connections cross the hinge
line that this box does not have.

| From | To | Length |
|---|---|---|
| ACS712 screw 2 | socket **LIVE** | **250 mm** |
| Neutral block | socket **NEUTRAL** | **250 mm** |

250 mm lets the lid lie flat on the bench beside the base with nothing under
tension. Anything shorter and you will be working on live-capable terminals held
up in one hand.

Socket **EARTH** stays empty. Write `NO EARTH — CLASS II ONLY` on the plate.

**Dress both leads forward, into the strip in front of the socket.** The socket
body is 30 mm deep in a 32 mm compartment — there is no room under it.

### STEP 15 — Tug test

Pull every one of the five joints plus the two flying leads. Anything that moves
is not connected. A loose screw here is the number one cause of `0.00 A` later.

Trace the chain with a finger, out loud, once.

---

# PART E — The lid

### STEP 16 — LCD

Four M3 × 6 pan screws into the four 6 mm bosses around the window. The display
faces out through the window; the backpack, with its blue contrast screw, faces
into the box.

The window is cut to the **active display area**, not the metal bezel, so the
bezel overlaps the opening on all sides. That is correct.

### STEP 17 — Buttons and buzzer into their collars

Each has a printed pocket on the lid underside. The body seats up inside the
pocket; only the plunger comes through the hole in the outer face.

**Test-fit before gluing.** Press the switch into its pocket and look at the
outer face:

- Plunger stands slightly proud → correct
- Plunger sits below the surface → put a 1–2 mm shim behind the switch (a washer,
  or a hardened blob of hot glue on the pocket floor) to raise it. You cannot
  press what you cannot reach

The engraved labels tell you which is which:

| Engraved | Is | Wired to |
|---|---|---|
| **MODE** | Button 1, "NEXT" in the firmware docs | `D2` |
| **SET** | Button 2, "ACTION" | `D3` |

Buzzer into the round collar with the ring of sound holes. Polarity does not
matter — it is passive.

Hot-glue all three from underneath once the fit is right.

### STEP 18 — Solder six joints

This is the step that wants an iron. Six joints, fifteen minutes.

| From | To |
|---|---|
| Button MODE, a leg on **one** face | Uno `D2` tail |
| Button MODE, a leg on the **opposite** face | `−` rail tail |
| Button SET, a leg on **one** face | Uno `D3` tail |
| Button SET, a leg on the **opposite** face | `−` rail tail |
| Buzzer, either leg | **column 8, lower block** tail |
| Buzzer, other leg | `−` rail tail |

**Opposite faces, always.** A tactile switch's four legs are joined in pairs
inside the body, and the joined pair is the two on the same face. Two wires from
one face ties the pin permanently to ground: you get one spurious long-press at
1.5 s and then nothing, which looks exactly like a dead pin.

**Column 8, not column 12.** Column 12 is where `D8` arrives; the 110 Ω resistor
sits between 12 and 8. Wiring the buzzer to 12 shorts the resistor out and drives
the element straight off the pin.

*No iron at all?* A female Dupont end pushes onto the 12 mm buttons' flat legs
and grips well enough with tape over it. It will not grip the buzzer's thin round
leads. If a button dies a week after assembly, this is why.

### STEP 19 — Socket into the lid

From the **outside**. Body through the opening, plate onto the rim, its own two
screws self-tapping into the slotted tabs wherever they land.

Connect the two 250 mm flying leads from STEP 14 to the socket's `L` and `N`
terminals. Earth empty.

### STEP 20 — Connect the ten low-voltage lid wires

LCD `SDA` → `A4`, `SCL` → `A5`, `VCC` → `+` rail, `GND` → `−` rail, plus the six
button and buzzer tails from STEP 18.

They drop into the **front zone**, over the Arduino. They must not go anywhere
near the mains deck. Bundle them with a cable tie and leave the loop long enough
that the lid lies flat beside the base.

---

# PART F — Test, then close

### STEP 21 — Low-voltage test, lid beside the box

Mains still unplugged. Rocker off. Power the Arduino from USB and repeat
`FINAL_BUILD.md` STEPS 21–25:

| Check | Expected |
|---|---|
| Boot | one chirp, `Smart Socket / Calibrating...` |
| Settles to | `READY 0.00A` |
| Relay | clicks, LED finishes **on** |
| **MODE** tap | screen cycles Detail → Stats → back |
| **SET** held 2 s | `MANUAL POWER OFF`, relay clicks |

Everything here is a fault you just created — a solder joint, a wire in the wrong
column, a button on one face. Fix it now, with the box open.

### STEP 22 — Live test, lid loose

Lid resting in place but **not screwed down**. Rocker off. Plug into the wall,
then switch the rocker on.

`0.00 A` with nothing in the socket. Anything else: rocker off, unplug from the
wall, go back to STEP 15.

Then plug in the Class II laptop charger and confirm you get **0.13 A or more**.
`FINAL_BUILD.md` STEP 40 has the full reading table.

### STEP 23 — Close it

Rocker off. **Unplug from the wall.**

Check before the lid goes down:

- [ ] No wire trapped on the base rim
- [ ] The sleeved rocker pair is tied down and touches no low-voltage wire
- [ ] Nothing low-voltage has strayed above the shelf
- [ ] No bare copper anywhere outside a terminal
- [ ] Both flying leads have slack, and neither is pinched under the socket body

Six M3 × 16 countersunk screws down the left and right edges. The heads sit flush
in the cones. Snug, then stop.

### STEP 24 — Final live check

Repeat STEP 22 with the box closed. Then run the demo — `FINAL_BUILD.md` STEP 43
for the real 15-minute version, STEP 44 for the 3-minute one.

### STEP 25 — Power the finished product

The Arduino runs from **a 5 V phone charger into the left-wall USB slot**, plugged
into a **different outlet**.

Never the socket the relay switches. That is a deadlock, not a preference: the
relay opens, the socket dies, the Arduino dies with it, and nothing is left alive
to close the relay again. Both outlets on the twin socket are fed from the same
terminals, so neither will do.

The firmware is in flash and stays there. The USB lead is carrying power only
from here on — and the left-wall slot means you can still reflash without opening
the box.

---

## Corrections to FINAL_BUILD.md

| Where | Says | Should say |
|---|---|---|
| STEP 53 table | Buzzer to **column 12**, lower | **Column 8**. Column 12 is `D8`; the 110 Ω resistor spans 12–8, and wiring to 12 bypasses it |
| PART 10, STEP 46 | Rocker snaps in, dab of glue | True, but its two mains wires have no route to the mains deck. See STEP 8 above |
| PART 10 | Buttons called NEXT / ACTION | The lid is engraved **MODE** / **SET**. Same buttons, `D2` and `D3` |

The buzzer line is fixed in `FINAL_BUILD.md`. The other two are cross-references.

---

## If it does not work

Faults introduced by boxing, on top of `FINAL_BUILD.md`'s table:

| What you see | Almost certainly |
|---|---|
| A button does nothing | Both wires soldered to the same face of the switch |
| Buzzer much louder or distorted | Wired to column 12 — the 110 Ω resistor is shorted out |
| LCD blank, backlight on | Contrast. The blue screw is now facing into the box — reach it with the lid off |
| `0.00 A` after it worked on the bench | A flying lead pulled out of the socket terminal while closing the lid |
| Intermittent reset when the relay clicks | A lid wire lying across the sleeved mains pair. Re-route and tie |
| Relay never clicks after boxing | One of the six pass-slot wires pinched under the shelf |
| Screw spins forever | Post stripped. Move to a different post, or fill with a matchstick and glue |

## Room left inside

The 18650 bay in the front zone stays empty — a single cell cannot run an Uno.

If you fit the **HC-05** later (`docs/HANDOFF.md` §2), put it on the **lower
deck behind the breadboard**, not in the battery bay: the bay is now beside the
sleeved mains run. There is clear floor there, and it keeps the radio's four
wires entirely in the low-voltage half of the box.

Remember its `TX`/`RX` share pins 0 and 1 with the USB uploader — unplug both
before every reflash.
