> ## ⚠ THIS FILE IS OUT OF DATE — DO NOT PRINT FROM IT
>
> It describes the earlier single-deck box with bezel plates and an 18650
> bracket. The enclosure was redesigned for the 230 V build: two decks split by
> a screwed-down shelf, the socket faceplate mounted in the lid, and the rocker
> snapped straight into the base's front wall with no bezel.
>
> **Print three files from `stl_slim/`: `SmartSocket_Base.stl`,
> `SmartSocket_Lid.stl`, `SmartSocket_Shelf.stl`.** `SwitchPlates` no longer
> exists in that folder.
>
> The authoritative sources are `build_slim.log` (what was actually exported)
> and the docstring at the top of `smart_socket_enclosure.py`. Assembly steps
> are in `../docs/FINAL_BUILD.md` PART 10.

# Smart Socket Enclosure — Group 38

Parametric enclosure for the Arduino Uno "Smart Socket with Adaptive
Time-of-Use Scheduling" build. Everything is generated from
[smart_socket_enclosure.py](smart_socket_enclosure.py) — the STLs are outputs,
never edit them by hand.

Needs a **180 × 180 mm bed** or larger.

---

## 0. Two versions — pick one at the print shop

Both come from the same script and the **same verified design**. `slim` only
thins the outer shell; every component position, hole pattern and internal
clearance is identical, confirmed by measuring both sets of meshes.

| | `stl/` — standard | `stl_slim/` — slim |
|---|---|---|
| Wall / floor / lid | 2.4 / 3.0 / 2.6 mm | 2.0 / 2.6 / 2.2 mm |
| Assembled size | 172.8 × 158.8 × 45.6 mm | 172.0 × 158.0 × 44.8 mm |
| **Interior (identical)** | **168 × 154 × 40 mm** | **168 × 154 × 40 mm** |
| Base + lid + bezels | **284 g** | **243 g** |
| At 2.50 GHS/g | **~710 GHS** | **~608 GHS** |

**Slim saves about 100 cedis and gives up nothing you can feel** — 2.0 mm walls
are still five extrusion lines thick, which is plenty for a box that sits on a
bench. Interior height stays at 40 mm in both, because that gap is what keeps
the LCD hanging off the lid clear of the jumper wires standing up on the
breadboard. That is the one dimension not worth trading for filament.

Those weights are solid model volume. With 20 % infill the slicer will report
roughly 15 % less — **ask the printer for his slicer's quote before he starts.**

---

## 1. Print these

Pick **one** folder — `stl/` or `stl_slim/` — and take all three files from it.

| File | What | Standard | Slim | Orientation |
|---|---|---|---|---|
| `SmartSocket_Base.stl` | Tray | ~195 g | ~168 g | As-is, floor on the bed |
| `SmartSocket_Lid.stl` | Top panel | ~80 g | ~67 g | As-is, **already flipped** |
| `SmartSocket_SwitchPlates.stl` | 3 rocker bezels | ~9 g | ~8 g | As-is, flat |

`SmartSocket_FitCheck.stl` is an optional ~39 g test coupon (section 2). Print
it first if you can afford it; skip it if you can't.

`SmartSocket_Assembly.stl` is for looking at only — **do not slice it.** It is
the base and lid fused into one solid, which is exactly what a printer means
when he asks for the model "in parts".

**No part needs support material.** Both large parts print flat. Don't rotate
anything in the slicer; the lid is already exported outer-face-down so its
bosses point up and the engraving lands on the build plate (which is what makes
it come out crisp).

### Slicer settings

- Layer height **0.2 mm**, nozzle 0.4 mm
- **4 perimeters** — walls come out fully solid in either version
- 5 top / 5 bottom layers, 20 % infill
- **Brim on the base** — it's a big flat part and the corners like to lift

---

## 2. Print the fit-check coupon first

This is the whole point of the coupon: it is 39 g and about 1½ hours, against
275 g and most of a day for the real thing. It carries every fit on the
enclosure at the exact modelled sizes:

| On the coupon | Check |
|---|---|
| 4 tall bosses in a rectangle | Arduino Uno drops on, all 4 holes line up |
| Large L-bracket pocket | Relay module sits inside and the lips hold it down |
| Small L-bracket pocket | ACS712 sits inside |
| Long L-bracket pocket | 18650 holder drops in and lifts back out |
| 4 short bosses + window | LCD screws on and all 32 characters are visible |
| Square collar + round hole | Tactile button body seats, plunger pokes through |
| Round collar + 7 small holes | Buzzer seats |
| Boss with a pilot hole | An M3 screw bites without splitting the boss |
| Countersunk hole | An M3 countersunk head sits flush |

If anything is tight or loose, change the matching constant in the script
(section 3, each one is labelled) and re-run. Don't file the real print.

---

## 3. Where everything goes

Origin is the **inside front-left corner** of the tray; "front" is the edge
nearest you at the bench, the one with the two rows of vent slots.

| Part | X (mm) | Y (mm) | How it is held |
|---|---|---|---|
| Breadboard (830 pt) | 1.5 – 166.5 | 10 – 65 | Side walls + 2 ribs; use its own adhesive backing |
| Arduino Uno | 1.5 – 70.1 | 71 – 124.4 | 4 × M3 into the floor standoffs |
| Relay module | 76.5 – 126.5 | 71 – 97 | Snaps under the bracket lips |
| ACS712 | 135 – 148 | 71 – 102 | Snaps under the bracket lips (turned 90°) |
| 18650 holder | 76.5 – 154.5 | 111 – 132 | Drops into brackets, no lips — cell stays swappable |
| Rocker switch | back wall, 45 mm from the left | | Bezel plate |

**The Arduino's USB and barrel jack face the left wall** and come out through
the wide opening there — you can reflash without opening the box.

On the lid: LCD at the front-left, MODE and SET at the front-right, buzzer
between them, vents and the `GROUP 38` engraving across the back.

---

## 4. Hardware you need

- **6 × M3 × 12 countersunk** — lid to base
- **4 × M3 × 8 pan head** — Arduino to the floor standoffs
- **4 × M3 × 6 pan head** — LCD to the lid bosses
- Hot glue — the two buttons, the buzzer, and the switch bezel

Self-tapping M3 is ideal, but ordinary M3 machine screws cut their own thread
in PLA perfectly well. All pilot holes are 2.4–2.6 mm.

---

## 5. Assembly order

1. Try the rocker in each of the three bezel plates; keep the one that fits
   (`small`, `standard`, or file the `blank` to shape). Press it into the
   back-wall opening from the outside, dab of glue.
2. Screw the Arduino to the four floor standoffs.
3. Press the relay and the ACS712 down into their bracket pockets until the
   lips catch the PCB edges. Drop in the battery holder.
4. Stick the breadboard down between the two ribs.
5. Fit the LCD to the lid bosses. Hot-glue the two buttons and the buzzer into
   their collars from underneath.
6. **Wire the lid parts with ~150 mm of slack** so the lid can be lifted off
   and set beside the box without pulling anything loose.
7. Wire everything else, then close with the six countersunk screws.

---

## 6. Regenerating after a change

Every dimension is a named constant at the top of the script. Change it and
re-run:

```bash
"C:/Program Files/FreeCAD 1.1/bin/freecadcmd.exe" smart_socket_enclosure.py
```

For the slim version, set `SMART_SOCKET_SLIM=1` first — same script, one design:

```bash
SMART_SOCKET_SLIM=1 "C:/Program Files/FreeCAD 1.1/bin/freecadcmd.exe" smart_socket_enclosure.py
```

Output goes to `stl/` (or `stl_slim/`) and a report to `build.log`
(or `build_slim.log`). The script runs a layout
check **before** it writes anything — overlapping modules, a part fouling a
screw boss, something too tall for the interior, the LCD bosses eating into the
window, no clear volume behind the rocker — and refuses to export if any of
those fail. If you move a component and it complains, believe it.

---

## 7. Design notes — why things are the way they are

**Dimensions are graded by confidence, and the mounting method follows.**
The Arduino outline and its four mounting holes, and the LCD's 75 × 31 hole
pattern, are published values — those get exact bosses, and they land within
0.001 mm in the exported STL. Relay, ACS712 and battery-holder outlines vary
between vendors, so those get L-shaped corner brackets with 0.8 mm of slack:
the module only has to land inside the corners, so being 1.5 mm off nominal
costs nothing.

**The Arduino connector opening is one wide 55 × 15 mm slot, not two tight
cutouts.** The board's position is exact because the standoffs fix it, but the
USB and barrel-jack positions *along* that edge are not something worth betting
a 275 g print on. One opening clears both regardless.

**The rocker switch is on a replaceable bezel.** It's the one part whose size
couldn't be pinned down — the common families (KCD11, KCD1-101, KCD3) need very
different cutouts. So the back wall gets a generous 30 × 24 mm opening and a
2 g bezel plate carries the actual cutout. Wrong guess costs ten minutes, not
the enclosure.

**The LCD window is 67 × 23 mm — sized to the active display area, not the
metal bezel.** A bezel-sized 72 × 26 window would run straight through the
mounting bosses, which sit only 34.5 mm from centre. The window has a 45°
chamfer on the outer face so the 4 mm depth doesn't vignette the display at an
angle; printed face-down that chamfer is self-supporting.

**Height is 45.6 mm against the old model's 75 mm.** The old enclosure was
~378 g and had no internal mounting features at all — just four corner posts
for the lid, with every board left to rattle around loose. It also had a
52 × 52 mm hole in the lid with nothing behind it, and the lid was fused to the
base in one STL so it couldn't be printed without cutting it up first.

**Footprint is set by the 165 mm breadboard**, which alone is 46 % of the floor.
The back strip packs the other four modules at about 70 % density — tighter
than that and there's no room to get a jumper in.
