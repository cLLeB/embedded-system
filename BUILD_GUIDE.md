# Build Guide — Smart Socket

Every step, in order, start to finish. Do them in order. Do not skip ahead.

**Everything here uses only the parts on your list. Nothing extra to buy. No mains. No
cutting. Nothing dangerous.**

---

# PART 0 — What you are building

Your parts list has a **Li-ion battery, a case, and an SPDT switch**. That battery pack is
your power source. It stands in for the wall.

| The idea | Your build |
|---|---|
| Wall power | Li-ion battery + SPDT switch |
| Plugged-in device | A load on the breadboard |
| Device charging (drawing power) | Load pulling current |
| Battery full (power drops) | You reduce the current |
| Cut the power | Relay opens |
| Show status + beep | LCD + buzzer |

The firmware is identical either way. It measures current, spots the drop, waits to be sure,
cuts the relay, beeps, and shows `FULL - POWER CUT`. The only difference is that a battery
supplies the current instead of a wall socket.

Highest voltage anywhere in this project: **3.7 volts**. It cannot hurt you.

---

# PART 1 — Parts checklist

All from your kit. Lay them out and tick them off.

| # | Part | What it looks like |
|---|------|--------------------|
| 1 | Arduino Uno | Blue board, USB socket on one end |
| 2 | ACS712 current sensor (5A) | Small board, 2 green screw terminals one side, 3 pins the other |
| 3 | Relay module (1 channel) | Board with a blue box on it, 3 screw terminals, 3 pins |
| 4 | 16x2 I2C LCD | Screen with a small board soldered on the back, 4 pins |
| 5 | Tactile buttons x2 | Tiny square buttons, 4 legs |
| 6 | Active buzzer | Small black cylinder, 2 legs |
| 7 | Resistors — **about 11 of the same value**, 100Ω is ideal | See Part 9 |
| 8 | Breadboard | White block full of holes |
| 9 | Jumper wires | Male-to-male |
| 10 | Li-ion battery (18650) + holder | Blue cylinder in a plastic case with red and black wires |
| 11 | SPDT switch | The small black rocker switch |
| 12 | USB cable | Arduino to your PC |

The Arduino is powered from your PC over USB. The battery only powers the load.

---

# PART 2 — Set up the software

**Step 2.1** — Open Arduino IDE.

**Step 2.2** — **File** → **Open**.

**Step 2.3** — Go to:
```
C:\Users\kyere\Documents\codes\embedded systems\SmartSocket\SmartSocket.ino
```
Click it, click **Open**.

**Step 2.4** — **Tools** → **Board** → **Arduino AVR Boards** → **Arduino Uno**.

**Step 2.5** — Near the top of the code, find:
```cpp
#define SENSOR_MODE_AC 0
```
Check it says **`0`**. It is already set for you. Change nothing.

**Step 2.6** — Click the **tick** button (top left). Wait.

**Step 2.7** — At the bottom you should see `Sketch uses 11130 bytes`. It worked.

Orange or red errors? Go to Part 12.

---

# PART 3 — Your breadboard

- The **long rows** marked `+` and `−` are **power rails**. Every hole along one long row is
  joined together.
- In the **middle**, each **vertical column of 5 holes** is joined together.
- The **centre gap** separates left from right.

Two wires in the same 5-hole column are connected. In different columns, they are not.

---

# PART 4 — Power rails

**Keep the Arduino unplugged for Parts 4 to 7.**

**Step 4.1** — Unplug the Arduino. No lights on.

**Step 4.2** — Arduino **`5V`** → breadboard **`+` rail**.

**Step 4.3** — Arduino **`GND`** → breadboard **`−` rail**.

---

# PART 5 — LCD

The small board on the back of your LCD has 4 pins: `GND`, `VCC`, `SDA`, `SCL`.

**Step 5.1** — LCD `GND` → **`−` rail**

**Step 5.2** — LCD `VCC` → **`+` rail**

**Step 5.3** — LCD `SDA` → Arduino **`A4`**

**Step 5.4** — LCD `SCL` → Arduino **`A5`**

`A4` and `A5` only. No other pins work for this.

---

# PART 6 — Buttons, buzzer, relay

## Buttons

A tactile button's 4 legs are joined in pairs inside. **Always use two legs diagonally
opposite each other.** Two legs on the same side is a permanent short.

**Step 6.1** — Push button 1 into the breadboard **across the centre gap** — 2 legs each
side.

**Step 6.2** — Push button 2 in the same way, a few columns along.

**Step 6.3** — Button 1: one leg → Arduino **`D2`**

**Step 6.4** — Button 1: **diagonally opposite** leg → **`−` rail**

**Step 6.5** — Button 2: one leg → Arduino **`D3`**

**Step 6.6** — Button 2: **diagonally opposite** leg → **`−` rail**

Button 1 = **NEXT**. Button 2 = **ACTION**.

No resistors on the buttons.

## Buzzer

One leg is **longer**. The long leg is `+`.

**Step 6.7** — Put the buzzer in the breadboard.

**Step 6.8** — Put a **100Ω resistor** so one end shares a column with the buzzer's **long
leg**.

**Step 6.9** — Resistor's other end → Arduino **`D8`**

**Step 6.10** — Buzzer **short leg** → **`−` rail**

## Relay — pins only

**Step 6.11** — Relay `VCC` → **`+` rail**

**Step 6.12** — Relay `GND` → **`−` rail**

**Step 6.13** — Relay `IN` → Arduino **`D7`**

Leave the relay's 3 **screw terminals** empty for now.

---

# PART 7 — Current sensor

**Step 7.1** — ACS712 `VCC` → **`+` rail**

**Step 7.2** — ACS712 `GND` → **`−` rail**

**Step 7.3** — ACS712 `OUT` → Arduino **`A0`**

Leave the 2 **screw terminals** empty for now.

---

# PART 8 — Test everything before the load

**Step 8.1** — Check against this table:

| Arduino pin | Goes to |
|---|---|
| `5V` | `+` rail |
| `GND` | `−` rail |
| `A0` | ACS712 `OUT` |
| `A4` | LCD `SDA` |
| `A5` | LCD `SCL` |
| `D2` | Button 1 |
| `D3` | Button 2 |
| `D7` | Relay `IN` |
| `D8` | 100Ω resistor → buzzer long leg |

| `+` rail | `−` rail |
|---|---|
| LCD `VCC` | LCD `GND` |
| Relay `VCC` | Relay `GND` |
| ACS712 `VCC` | ACS712 `GND` |
| | Button 1 diagonal leg |
| | Button 2 diagonal leg |
| | Buzzer short leg |

**Step 8.2** — Plug the Arduino into your PC.

**Step 8.3** — **Tools** → **Port** → select the COM port. Unsure which? Unplug the Arduino,
look at the list, plug it back in, pick the one that reappeared.

**Step 8.4** — Click the **arrow** button to upload. Wait for `Done uploading`.

**Step 8.5** — The LCD shows:
```
Smart Socket
Calibrating...
```
then:
```
READY      0.00A
00:00:00  pk0.00
```

**Step 8.6** — You hear the relay **click** once at startup.

**Step 8.7** — Press **Button 1 (NEXT)**. Screen changes to `Cut at / Taper`. Press again
for `Cutoffs / Saved`. Press again to return.

**Step 8.8** — Hold **Button 2 (ACTION)** for 2 seconds. Screen shows `MANUAL POWER OFF`,
relay clicks.

**Step 8.9** — Hold Button 2 again for 2 seconds. Back to `READY`, relay clicks.

### Blank LCD?

1. Turn the small **blue screw** on the LCD's back board slowly. That is the contrast.
2. Still nothing? Open `SmartSocket\src\core\Config.h`, change `LcdI2cAddress = 0x27` to
   `0x3F`, upload again.

**Everything in Part 8 must work before Part 9.**

---

# PART 9 — Build the load

> **Follow `docs/STAGE2.md` instead of this part.** It was written against the
> resistors as actually measured — **110 Ω, not 100 Ω** — and it returns the load
> straight to the battery rather than through the Arduino's `−` rail. Both matter.
> Parts 1–8 below are still correct; this part and Part 10 are kept for the
> reasoning only.

## Why 11 resistors

Your ACS712 is the **5A** model. It cannot see small currents — anything under about
**0.06A** is lost in its own noise. Your load must pull at least **0.35A** or the sensor is
blind to it.

One 18650 gives **3.7V**. Ohm's law:

- One 100Ω resistor across 3.7V = **0.037A**. Far too small to see.
- Put **10 of them side by side (in parallel)** = **0.37A**. The sensor sees that clearly.

Each resistor still only handles 0.037A and stays cool. Ten together add up to a load the
sensor can measure. (The 11th resistor is the one already on your buzzer.)

**If you don't have ten 100Ω resistors:** any value from 100Ω to 220Ω works. Just use more
of them for higher values — about 10 for 100Ω, 15 for 150Ω, 22 for 220Ω. Do not use
anything below 100Ω; it will get hot.

## Build the resistor bank

**Step 9.1** — Unplug the Arduino.

**Step 9.2** — Pick two empty columns on the breadboard, a few holes apart. Call them
**column A** and **column B**.

**Step 9.3** — Push a resistor in so **one leg is in column A and the other is in column B**.

**Step 9.4** — Do that **nine more times**. Ten resistors, all side by side, every one
bridging column A to column B.

They should look like a row of little fences. All ten are now in parallel.

## Wire the battery circuit

**Step 9.5** — Put the 18650 in its holder. **Red wire is `+`. Black wire is `−`.**

**Step 9.6** — Battery **red (+)** → **one blade** of the rocker switch. The switch in
this kit has **two spade blades, not three pins** — there is no middle pin, and either
blade can be either side. The blades do not fit a breadboard; strip 15 mm of a jumper,
wrap it round the blade and tape it. Or leave the switch out of the first run entirely
and use the cell as the on/off, which is what `docs/STAGE2.md` does.

**Step 9.7** — **The other blade** of the rocker switch → **relay screw terminal `COM`**.
Tighten.

**Step 9.8** — Jumper wire from **relay `NO`** → **ACS712 screw terminal 1**. Tighten.

**Step 9.9** — Jumper wire from **ACS712 screw terminal 2** → **column A** on the
breadboard.

**Step 9.10** — Jumper wire from **column B** → **battery black (−)**, directly.

**Step 9.11** — **Not** through the breadboard `−` rail. The ACS712's screw terminals are
isolated from its `VCC`/`GND` pins, so the load needs no shared ground with the Uno —
and keeping 0.37 A out of the Arduino's ground rail removes the voltage shift that
showed up earlier in this build as a phantom 0.10–0.12 A reading.

The current path is now:
```
battery + → switch → relay → ACS712 → resistor bank → battery −
```

**Step 9.12** — Set the SPDT switch to **OFF**.

---

# PART 10 — Run the demo

**Step 10.1** — Switch the SPDT switch **OFF**.

**Step 10.2** — Plug the Arduino into your PC.

**Step 10.3** — Wait for `READY   0.00A`. It must read **0.00A** — nothing is flowing yet.

**Step 10.4** — Now flick the SPDT switch **ON**.

**Step 10.5** — You hear **one short beep**. The LCD shows:
```
SETTLE     0.37A
```
and after 5 seconds:
```
CHARGING   0.37A
00:00:03  pk0.37
```

**Your "device" is now charging.** The resistors may feel slightly warm. That is normal.

**Step 10.6** — Press **Button 1 (NEXT)** to see the Detail screen:
```
Cut at     0.14A
Taper 30%    n=0
```
That `Cut at` number is the level the socket will cut off below. It worked it out from your
load by itself.

**Step 10.7** — Press Button 1 again to return to the Status screen.

## Simulate the battery filling up

**Step 10.8** — Wait **at least 1 minute** with all ten resistors in. The socket will not cut
off before then, on purpose.

**Step 10.9** — Now **pull out resistors one at a time.** Watch the current drop on the LCD:

| Resistors left | LCD shows about |
|---|---|
| 10 | 0.37A |
| 8 | 0.30A |
| 6 | 0.22A |
| 4 | 0.15A |
| **3** | **0.11A** |

**Step 10.10** — Stop at **3 resistors**. The LCD reads about `0.11A`, which is below the
`Cut at` figure.

**Do not pull out more than 3.** Below about 0.08A the socket decides the device has been
unplugged and just goes back to `READY`.

**Step 10.11** — Leave it alone for **90 seconds**. Do not touch anything.

**Step 10.12** — You hear **three beeps, twice**, the relay clicks, and the LCD shows:
```
FULL - POWER CUT
Press ACTION
```

**The project works.** The relay has cut the power. The resistors are cold. Current is zero.

**Step 10.13** — Press **Button 1 (NEXT)** twice to see the Stats screen:
```
Cutoffs        1
Saved     0h01m
```

**Step 10.14** — Press **Button 2 (ACTION)** once to re-arm. Back to `READY`.

## What just happened

1. Current was high → `CHARGING`
2. You dropped the current → the socket saw it
3. It waited 90 seconds to be sure it was not a glitch
4. It cut the relay and beeped → `FULL - POWER CUT`
5. It remembered the cutoff in its EEPROM — the count survives unplugging

That is the whole product.

## Do it again

**Step 10.15** — Put all ten resistors back. Switch the SPDT switch OFF, then ON.

**Step 10.16** — Repeat. On the Detail screen you will see `Taper` change slightly each
time. That is the adaptive learning — it is tuning itself to your load.

---

# PART 11 — Checking the readings

**Step 11.1** — With all 10 resistors in and the switch ON, the LCD should read about
`0.37A`.

**Step 11.2** — If it is consistently off, open `SmartSocket\src\core\Config.h`:
```cpp
const int32_t SensorMvPerAmp = 185;
```
- Reads **too high** → raise it (190, 195)
- Reads **too low** → lower it (180, 175)

Upload again after each change.

**Step 11.3** — If it shows current with the switch OFF: switch off, unplug the Arduino,
plug it back in. It measures its own zero point every time it starts.

---

# PART 12 — Troubleshooting

| Problem | Do this |
|---|---|
| Compile error mentions `LiquidCrystal_I2C` | **Tools** → **Manage Libraries** → search `LiquidCrystal I2C` → install the one by **Frank de Brabander** |
| Upload fails, port not found | **Tools** → **Port** → pick the COM port. Unplug/replug to see which appears |
| LCD backlit but no text | Turn the blue screw on the LCD's back board |
| LCD dead | Check `VCC`/`GND`. Then change `LcdI2cAddress` to `0x3F` in `Config.h` |
| Relay clicks backwards | `SmartSocket\src\hal\HalPins.h`, change `RelayActiveLow = true` to `false` |
| Buttons dead | You used two legs on the same side. Use **diagonally opposite** legs |
| Buzzer silent or always on | Legs are backwards. Long leg → resistor, short leg → `−` rail |
| Never leaves `READY` when switch is ON | Load too small. Add more resistors. You need at least 0.35A |
| Resistors hot | Your resistors are below 100Ω. Use 100Ω or higher |
| Goes to `READY` instead of `FULL` | You pulled out too many resistors. Leave 3 in |
| Cuts off too early | `Config.h`, change `TaperConfirmMs` from `90000` to `180000` |
| Never cuts off | Did you wait a full 90 seconds without touching it? Is the current below the `Cut at` figure? |
| `! OVERCURRENT !` | Short circuit. Switch off. Check the battery is not wired straight across |
| Current reads 0.00A with switch ON | Check the SPDT middle pin, the relay `COM`/`NO` screws, and both ACS712 screws |

---

# PART 13 — Optional: run the Arduino from the battery too

Only if you want it running with no PC.

One 18650 is **3.7V** — **not enough** to power an Arduino Uno. You need **two cells in
series** (7.4V) in a 2-cell holder. If you only have one cell, skip this part and keep the
USB cable plugged in. That is perfectly fine.

With two cells:

**Step 13.1** — Battery **red (+)** → middle pin of the SPDT switch

**Step 13.2** — One outer pin of the switch → Arduino **`VIN`**

**Step 13.3** — Battery **black (−)** → Arduino **`GND`**

**Never connect the battery and the USB cable at the same time.**

---

# Quick reference

## Control side (breadboard, 5V from Arduino)

| Arduino pin | Connects to |
|---|---|
| `5V` | `+` rail |
| `GND` | `−` rail |
| `A0` | ACS712 `OUT` |
| `A4` | LCD `SDA` |
| `A5` | LCD `SCL` |
| `D2` | Button 1 (NEXT) |
| `D3` | Button 2 (ACTION) |
| `D7` | Relay `IN` |
| `D8` | 100Ω resistor → buzzer long leg |

| `+` rail | `−` rail |
|---|---|
| LCD `VCC` | LCD `GND` |
| Relay `VCC` | Relay `GND` |
| ACS712 `VCC` | ACS712 `GND` |
| | Button 1 diagonal leg |
| | Button 2 diagonal leg |
| | Buzzer short leg |
| | Battery black (−) |
| | Resistor bank column B |

## Load side (3.7V battery)

```
battery + → rocker blade 1        (2 blades, no middle pin - or omit for the first run)
rocker blade 2 → relay COM
relay NO → ACS712 terminal 1
ACS712 terminal 2 → resistor bank (node A)
resistor bank (node B) → battery −     NOT the − rail
```

This loop never touches the Arduino's rails. See `docs/STAGE2.md`.

## The demo

1. All **11** resistors in, power ON → `CHARGING 0.37A`
2. Wait 1 minute
3. Pull resistors out until **4** remain → about `0.13A` (cut-below is `0.18A`)
4. Wait 90 seconds, hands off
5. Three beeps → `FULL - POWER CUT`
6. Tap **ACTION** to re-arm

Never go below 3 resistors — at 2 the socket reads it as unplugged and returns to
`READY`.

## Buttons

- **Button 1 (NEXT)** — change screen
- **Button 2 (ACTION)** — tap to reset after cutoff; hold 2s to force power off/on
