#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Smart Socket with Adaptive Time-of-Use Scheduling  --  Group 38
Parametric enclosure generator for FreeCAD.

Run headless:
    "C:\\Program Files\\FreeCAD 1.1\\bin\\freecadcmd.exe" smart_socket_enclosure.py

Outputs into ./stl :  THREE parts to print, nothing else
    SmartSocket_Base.stl        print as-is, floor on the bed, no supports
    SmartSocket_Lid.stl         print as-is, already flipped, no supports
    SmartSocket_Shelf.stl       print as-is, flat, no supports
    SmartSocket_FitCheck.stl    optional test coupon, not counted in the total
    SmartSocket_Assembly.stl    visual check only, do NOT slice this
The rocker switch snaps straight into the base's front wall - no bezel part.

Coordinates
    X = width (left-right), Y = depth (front-back), Z = up.
    Global origin = outer front-left-bottom corner of the base.
    Interior coords go through IX()/IY()/IZ().
    "Front" (Y=0) is the edge nearest you at the bench.

Design rules used throughout
    * Anything whose dimensions are known from a published spec gets an exact
      feature.  Anything that varies between vendors gets a tolerant feature
      (corner brackets with slack, oversized openings, or a replaceable plate).
    * Both large parts print flat with zero support material.
"""

import os
import sys
import math

import FreeCAD as App
import Part

HERE = os.path.dirname(os.path.abspath(__file__))

# Two builds off one design.  SLIM only thins the shell - every component
# position, hole pattern and internal clearance is identical in both.
#   standard :  set nothing            -> stl/        ~284 g
#   slim     :  SMART_SOCKET_SLIM=1    -> stl_slim/   ~243 g
SLIM = os.environ.get("SMART_SOCKET_SLIM", "0") == "1"
_TAG = "_slim" if SLIM else ""

OUT_DIR = os.path.join(HERE, "stl" + _TAG)
LOG_PATH = os.path.join(HERE, "build%s.log" % _TAG)

# freecadcmd swallows stdout, so mirror everything into build.log
_LOG = open(LOG_PATH, "w")


def say(msg=""):
    sys.stdout.write(msg + "\n")
    _LOG.write(msg + "\n")
    _LOG.flush()

# =============================================================================
# 1. PRINT PROCESS
# =============================================================================
FIT = 0.40          # per-side clearance, snug push fit
FIT_MODULE = 0.80   # per-side clearance for COTS modules of uncertain size
LIP_OVER = 1.5      # how far a retaining lip reaches over a PCB edge

# =============================================================================
# 2. SHELL
# =============================================================================
WALL = 2.0 if SLIM else 2.4     # 5 or 6 x 0.4 mm extrusions
FLOOR = 2.4 if SLIM else 3.0    # thicker than the walls: it carries the bosses
LID_T = 2.2 if SLIM else 2.6
SHELF_T = 2.4 if SLIM else 2.8  # floor of the mains deck.  This is the 230 V
                                # barrier, so it never gets lightening holes -
                                # only the plate itself is thinned.
CORNER_R = 3.0

# --- the socket sets the height of the upper deck --------------------------
# Measured 30 Jul as "3 cm" with the plate face-down.  If that 30 mm actually
# included the faceplate, the true body is nearer 20 mm and MAINS_H can drop
# by 10 mm (saves ~25 g).  Erring high is the safe direction: too shallow and
# the lid will not close.
SOCKET_BODY_DEPTH = 30.0
SOCKET_BODY_CLR = 2.0

# --- two decks -------------------------------------------------------------
# 230 V lives entirely ABOVE the shelf, low voltage entirely BELOW and in front
# of it.  The separation is printed geometry, not a partition bolted in later.
LOWER_H = 22.0                  # floor top -> shelf underside.  The breadboard
                                # is 10 mm, leaving 12 mm for jumper wires.
MAINS_H = SOCKET_BODY_DEPTH + SOCKET_BODY_CLR    # shelf top -> lid underside
IN_H = LOWER_H + SHELF_T + MAINS_H

IN_W = 166.0                    # interior width (X) - set by the 162 mm breadboard
FRONT_D = 58.0                  # LV zone, FULL height: Arduino on the floor,
                                # LCD hanging off the lid beside it
MAINS_D = 114.0                 # socket 86 + a 22.6 mm strip for relay/ACS712.
                                # Driven by the relay: 16 mm wide plus 3 mm of
                                # bracket each side.  Any less and the relay
                                # ends up under the socket body.
IN_D = FRONT_D + MAINS_D

# Interior Z of the shelf, and of the mains deck floor
SHELF_Z0 = LOWER_H
SHELF_Z1 = LOWER_H + SHELF_T

OUT_W = IN_W + 2 * WALL
OUT_D = IN_D + 2 * WALL
BASE_H = FLOOR + IN_H
TOTAL_H = BASE_H + LID_T

# =============================================================================
# 3. COMPONENTS
#     [SPEC]    published, trusted to 0.1 mm  -> exact feature
#     [COMMON]  usual value, vendors vary     -> tolerant feature
#     [VARIES]  cannot be pinned down         -> replaceable / oversized
# =============================================================================

# --- Arduino Uno R3 -------------------------------------------------- [SPEC]
UNO_L, UNO_W = 68.6, 53.4
UNO_PCB_T = 1.6
UNO_HOLES = [(14.0, 2.5), (15.3, 50.8), (66.1, 35.6), (66.1, 7.6)]
UNO_TALL = 12.0                       # tallest thing on the board
UNO_STANDOFF_H = 5.0                  # clears the solder tails underneath
UNO_BOSS_OD = 7.0
UNO_PILOT_D = 2.5                     # M3 self-tapping into PLA
# How far the pilot may sink into the floor. Always leave >= 1.2 mm of solid
# floor underneath, or a screw driven hard breaks out of the bottom.
UNO_PILOT_SINK = min(2.0, FLOOR - 1.2)

# --- 16x2 HD44780 LCD ------------------------------------------------ [SPEC]
LCD_L, LCD_W = 80.0, 36.0
LCD_HOLE_DX, LCD_HOLE_DY = 75.0, 31.0
# Window is sized to the ACTIVE display area (character block is 64.5 x 16.4),
# NOT to the full metal bezel.  A bezel-sized 72 x 26 window would run straight
# through the mounting bosses, which sit only 34 mm from centre.
LCD_WIN_L, LCD_WIN_W = 67.0, 23.0
LCD_WIN_CHAMFER = 1.5                 # 45 deg relief on the outer face: stops
                                      # the 4 mm deep window vignetting the
                                      # display, and prints support-free
LCD_BOSS_H = 3.0
LCD_BOSS_OD = 6.0
LCD_PILOT_D = 2.4
LCD_PILOT_SINK = min(1.5, LID_T - 1.0)   # keep >= 1.0 mm of lid above it
# The PCF8574 I2C backpack arrived 30 Jul and is soldered to the 16-pin header
# on the back of the LCD.  It roughly doubles how far the module hangs below
# the lid: 16-pin header + backpack PCB + its trimpot and 4-pin plug.
LCD_BACKPACK_H = 12.0
LCD_STACK = LCD_BOSS_H + 1.6 + 9.0 + LCD_BACKPACK_H

# --- 1-channel relay module -------------------------------- [MEASURED 30 Jul]
RELAY_L, RELAY_W = 40.0, 16.0
RELAY_BRACKET_H = 6.0
RELAY_LIP_Z = 2.2
RELAY_TALL = 19.0

# --- ACS712 5A current sensor ------------------------------ [MEASURED 30 Jul]
# Mounted turned 90 deg (long axis along Y) so it tucks beside the relay
# instead of eating another 12 mm of depth behind it.
ACS_L, ACS_W = 30.0, 12.0
ACS_BRACKET_H = 5.0
ACS_LIP_Z = 2.2

# --- Full-size 830-point breadboard ------------------------ [MEASURED 30 Jul]
BB_L, BB_W, BB_H = 162.0, 54.0, 10.0

# --- Single 18650 holder ----------------------------------- [MEASURED 30 Jul]
BAT_L, BAT_W, BAT_H = 75.0, 20.0, 19.0
BAT_BRACKET_H = 10.0                  # no lip: the cell has to come out

# --- Mains socket faceplate --------------------------------- [PARTLY MEASURED]
# Plate outline measured 30 Jul.  Body footprint and fixing centres are NOT
# known, so neither is trusted:
#   - the through-opening is cut generously, sized off the plate not the body
#   - the fixing screws get SLOTS with a rib underneath, so the socket's own
#     screws self-tap wherever they land.  Any centre from 90 to 150 mm works,
#     which covers the 120.6 mm BS standard this plate size implies.
# Confirmed a standard BS 4662 2-gang plate (146 x 86, fixing centres 120.6 mm)
# from the web + the FOCUS PC/H40BR photo.  Opening widened to 140 x 80 so a
# full-size 2-gang body drops through with clearance; the plate still lands on
# a 3 mm rim, and the fixing tabs still bracket the 120.6 mm screw centres.
SOCKET_PLATE_L, SOCKET_PLATE_W = 146.0, 86.0
SOCKET_FIX_CENTRES = 120.6                     # BS 4662 2-gang, horizontal
SOCKET_OPEN_L, SOCKET_OPEN_W = 138.0, 78.0     # 4 mm rim; 2 mm clearance on a
                                               # max 2-gang body (~134 x 74)
SOCKET_OPEN_R = 3.0
SOCKET_SLOT_W = 2.8                   # narrower than an M3.5: the screw taps it
SOCKET_SLOT_INNER = 45.0              # tab inner end, from socket centre
SOCKET_SLOT_LEN = 25.0                # tab spans 45..70 mm: outer end embeds
                                      # 1 mm into the rim past the 138 mm
                                      # opening (half-width 69), so it stays
                                      # anchored, not floating.  Slot cut
                                      # 47..68 brackets the 60.3 mm screw.
SOCKET_RIB_W = 9.0
SOCKET_RIB_H = 6.0

# --- incoming mains cord ------------------------------------------ [COMMON]
CABLE_D = 8.0                         # 3-core 1.0 mm2 flex
CABLE_TIE_W, CABLE_TIE_H = 3.5, 2.2   # strain-relief tie slots

# --- Passive buzzer ------------------------------------------------ [COMMON]
BUZZ_D, BUZZ_H = 12.0, 9.5
BUZZ_COLLAR_H = 6.0

# --- 12 mm tactile buttons ----------------------------------------- [COMMON]
BTN_BODY = 12.0
BTN_PLUNGER_HOLE_D = 8.0
BTN_COLLAR_H = 5.0

# --- Rocker switch ------------------------------------ [IDENTIFIED 30 Jul]
# KCD1-101, two spade terminals, identified from the coin-scale photos.
# It is a PANEL-MOUNT switch: it carries its OWN snap clips, so it clicks
# straight into a hole in the base's front wall - no separate bezel to print.
# Mounted PORTRAIT (taller than wide) so the moulded O / I read upright.
#   cutout   : 13.4 x 19.6 mm  (13.2 x 19.4 nominal + 0.1/side clearance)
#   panel    : the clips grab a 1.5 mm panel, so the wall is recessed to that
#              thickness over a small area around the hole.
SW_OPEN_W, SW_OPEN_H = 13.4, 19.6     # exact switch cutout in the front wall
SW_PANEL_T = 1.5                      # local wall thickness the clips snap onto
SW_RECESS_W, SW_RECESS_H = 24.0, 30.0 # thinned/recessed area around the cutout

# =============================================================================
# 4. INTERIOR LAYOUT (interior coords, origin = interior front-left)
#    Every position here is verified by check_layout() before anything is built.
# =============================================================================
# --- MAINS DECK: on the shelf, Y >= PARTITION_Y1, Z >= SHELF_Z1 -------------
PARTITION_Y0 = FRONT_D                # front face of the mains compartment
PARTITION_Y1 = FRONT_D + WALL

# Socket plate, centred in X.  Pushed to the back so a 16 mm relay still fits
# in the strip in front of it.
SOCKET_CX = IN_W / 2.0                # 83.0
SOCKET_CY = 126.0                     # plate spans Y 83..169, 3 mm off the back

# Relay and ACS712 share the strip between the partition and the socket plate.
RELAY_X, RELAY_Y = 12.0, 64.0         # 40 x 16 -> X 12..52,  Y 64..80
ACS_X, ACS_Y = 64.0, 64.0             # 30 x 12 -> X 64..94,  Y 64..76

# Incoming mains cord: back wall, above the shelf, away from the wire pass-slot
CABLE_CX = 130.0
CABLE_CZ = SHELF_Z1 + 12.0

# The ONLY route between the two decks.  Carries the six low-voltage signal
# wires (relay VCC/GND/IN, ACS712 VCC/GND/OUT) and nothing else.
PASS_X, PASS_Y = 148.0, 64.0
PASS_L, PASS_W = 16.0, 6.0

# --- LOWER DECK: under the shelf, LV only ----------------------------------
BB_X, BB_Y = 2.0, 66.0                # 162 x 54 -> X 2..164, Y 66..120

# The shelf is a SEPARATE printed plate.  Moulding it into the base would put a
# 166 x 110 mm ceiling 22 mm up in mid-air - the one thing that would force
# support material.  It drops onto these posts instead and screws down, which
# also means you can lift it out to reach the wiring underneath.
SHELF_POST_D = 9.0
SHELF_POST_PILOT = 2.5
SHELF_CLEAR_D = 3.4
# Posts can only stand where the breadboard is not, i.e. Y < 66 or Y > 120.
# The two at Y=163 are pulled inboard in X so they clear the lid-screw bosses.
SHELF_POSTS = [(5.0, 61.0), (83.0, 61.0), (161.0, 61.0),
               (5.0, 126.0), (83.0, 126.0), (161.0, 126.0),
               (40.0, 160.0), (126.0, 160.0)]
# The centre pair is plain support - a screw there could not be reached past
# the breadboard.  The rest take M3s.
SHELF_SCREW_POSTS = [p for p in SHELF_POSTS if abs(p[0] - 83.0) > 1.0]

# --- FRONT ZONE: full height, no shelf above -------------------------------
UNO_X, UNO_Y = 9.0, 2.5               # connector edge faces the LEFT wall.
                                      # X=9 keeps its standoffs clear of the
                                      # front-left lid boss by 0.3 mm.
BAT_X, BAT_Y = 86.0, 30.0             # 75 x 20, right of the Arduino, clear of
                                      # the rocker keepout, 12 mm under the LCD

# Rocker moves to the FRONT wall: the lower deck is only 22 mm tall, too short
# for a 24 mm opening, and the front zone is the only full-height wall left.
SW_CX = 120.0                         # rocker centre, front wall, interior X
SW_CZ = 15.0                          # rocker centre, interior Z
SW_KEEPOUT_DEPTH = 26.0               # clear volume needed behind the switch

# --- lid screw bosses --------------------------------------------------------
BOSS_OD = 8.0
BOSS_INSET = 3.2
BOSS_PILOT_D = 2.6
BOSS_PILOT_DEPTH = 16.0
LID_SCREW_CLEAR_D = 3.4
LID_CSK_D = 6.2
LID_CSK_DEPTH = min(1.4, LID_T - 1.0)    # keep >= 1.0 mm under the screw head

# All six run down the LEFT and RIGHT edges.  None sits behind the socket
# plate, whose 146 mm width would otherwise foul the countersunk heads.
BOSSES = [
    (BOSS_INSET, BOSS_INSET),
    (IN_W - BOSS_INSET, BOSS_INSET),
    (BOSS_INSET, IN_D - BOSS_INSET),
    (IN_W - BOSS_INSET, IN_D - BOSS_INSET),
    # Mid-span pair stops the long lid bowing.  NOT at IN_D/2 - that lands in
    # the middle of the breadboard, and these bosses run floor to lid.
    (BOSS_INSET, 136.0),
    (IN_W - BOSS_INSET, 136.0),
]

# =============================================================================
# 5. LID FEATURES (interior coords)
# =============================================================================
# Front zone only.  Everything from Y = 78 back is the socket plate.
LCD_CX, LCD_CY = 122.0, 30.0          # 80 x 36 -> X 82..162, Y 12..48
BTN1_C = (20.0, 20.0)                 # both sit over the Arduino, which tops
BTN2_C = (48.0, 20.0)                 # out 26 mm below the lid
BUZZ_C = (34.0, 44.0)

# No lid vents.  The only unused lid area is directly over the mains
# compartment, and a slot there is a finger-sized route to 230 V.
VENT_ROWS_Y = []
VENT_COLS_X = []
VENT_SLOT_L = 36.0
VENT_SLOT_W = 3.0

TEXT_DEPTH = 0.8
TEXT_STROKE = 1.6
LABELS = [
    ("GROUP 38",              (83.0, 70.0), 12.0, 1.8),
    ("SMART SOCKET",          (83.0, 58.0),  7.0, 1.2),
    ("UNPLUG BEFORE OPENING", (83.0, 50.0),  4.0, 0.8),
    ("MODE",                  (20.0, 11.0),  5.0, 1.0),
    ("SET",                   (48.0, 11.0),  5.0, 1.0),
]

# Wall vents stay BELOW the shelf, so they only ever open into the
# low-voltage deck.  Nothing vents into the mains compartment.
WALL_VENT_L, WALL_VENT_W = 14.0, 3.0
WALL_VENT_Z = [8.0, 16.0]

# =============================================================================
# helpers
# =============================================================================
V = App.Vector


def IX(x):
    return WALL + x


def IY(y):
    return WALL + y


def IZ(z):
    return FLOOR + z


def rounded_box(w, d, h, r, origin=(0.0, 0.0, 0.0)):
    b = Part.makeBox(w, d, h, V(*origin))
    if r <= 0:
        return b
    vertical = [e for e in b.Edges
                if abs(e.Vertexes[0].Point.z - e.Vertexes[1].Point.z) > 1e-6]
    return b.makeFillet(r, vertical)


def cyl(d, h, x, y, z):
    return Part.makeCylinder(d / 2.0, h, V(x, y, z))


def rect_wire(cx, cy, w, d, z):
    """Closed rectangular wire, for lofting tapered cuts."""
    hw, hd = w / 2.0, d / 2.0
    pts = [V(cx - hw, cy - hd, z), V(cx + hw, cy - hd, z),
           V(cx + hw, cy + hd, z), V(cx - hw, cy + hd, z),
           V(cx - hw, cy - hd, z)]
    return Part.makePolygon(pts)


def capsule(length, width, height, cx, cy, z):
    """Rounded-end slot lying along X."""
    r = width / 2.0
    straight = max(length - width, 0.0)
    body = Part.makeBox(straight, width, height, V(cx - straight / 2.0, cy - r, z))
    return body.fuse(cyl(width, height, cx - straight / 2.0, cy, z)) \
               .fuse(cyl(width, height, cx + straight / 2.0, cy, z))


def fuse_all(solids):
    out = solids[0]
    for s in solids[1:]:
        out = out.fuse(s)
    return out


def bracket_set(x0, y0, length, width, height, clr,
                lip_z=None, arm=8.0, thick=2.2):
    """
    Four L-shaped corner brackets forming a tolerant pocket for a PCB module.

    The module only has to land inside the corners, so a couple of millimetres
    of vendor variation costs nothing.  `lip_z` adds a ledge that reaches over
    the PCB edge to hold it down; leave it None for anything removable.
    """
    px0, py0 = x0 - clr, y0 - clr
    px1, py1 = x0 + length + clr, y0 + width + clr
    solids = []
    for (cx, cy, sx, sy) in ((px0, py0, 1, 1), (px1, py0, -1, 1),
                             (px0, py1, 1, -1), (px1, py1, -1, -1)):
        wall_x = cx - thick if sx > 0 else cx
        wall_y = cy - thick if sy > 0 else cy
        # Each arm is extended back by `thick` so the two arms of an L overlap
        # in the corner square.  If they only touched along the corner line the
        # result is non-manifold: one edge shared by four triangles.
        arm_x = wall_x if sx > 0 else cx - arm
        arm_y = wall_y if sy > 0 else cy - arm
        solids.append(Part.makeBox(thick, arm + thick, height, V(wall_x, arm_y, 0)))
        solids.append(Part.makeBox(arm + thick, thick, height, V(arm_x, wall_y, 0)))
        if lip_z is not None:
            # overlap the lip 0.4 mm into the arm so it fuses as a volume
            lip_x = cx - 0.4 if sx > 0 else cx - LIP_OVER
            lip_y = cy - 0.4 if sy > 0 else cy - LIP_OVER
            solids.append(Part.makeBox(LIP_OVER + 0.4, arm + thick, height - lip_z,
                                       V(lip_x, arm_y, lip_z)))
            solids.append(Part.makeBox(arm + thick, LIP_OVER + 0.4, height - lip_z,
                                       V(arm_x, lip_y, lip_z)))
    return fuse_all(solids)


# =============================================================================
# stroke font - deterministic, needs no font file and no Draft workbench
# =============================================================================
GLYPHS = {
    "A": [[(0, 0), (0.5, 1), (1, 0)], [(0.22, 0.38), (0.78, 0.38)]],
    "B": [[(0, 0), (0, 1), (0.7, 1), (0.95, 0.85), (0.7, 0.55), (0, 0.55)],
          [(0.7, 0.55), (1, 0.35), (0.7, 0), (0, 0)]],
    "C": [[(1, 0.85), (0.75, 1), (0.25, 1), (0, 0.75),
           (0, 0.25), (0.25, 0), (0.75, 0), (1, 0.15)]],
    "D": [[(0, 0), (0, 1), (0.6, 1), (1, 0.7), (1, 0.3), (0.6, 0), (0, 0)]],
    "E": [[(1, 1), (0, 1), (0, 0), (1, 0)], [(0, 0.5), (0.8, 0.5)]],
    "F": [[(1, 1), (0, 1), (0, 0)], [(0, 0.52), (0.8, 0.52)]],
    "G": [[(1, 0.85), (0.75, 1), (0.25, 1), (0, 0.75), (0, 0.25),
           (0.25, 0), (0.75, 0), (1, 0.18), (1, 0.45), (0.55, 0.45)]],
    "H": [[(0, 1), (0, 0)], [(1, 1), (1, 0)], [(0, 0.5), (1, 0.5)]],
    "I": [[(0.5, 1), (0.5, 0)], [(0.15, 1), (0.85, 1)], [(0.15, 0), (0.85, 0)]],
    "K": [[(0, 1), (0, 0)], [(1, 1), (0, 0.45)], [(0.35, 0.63), (1, 0)]],
    "L": [[(0, 1), (0, 0), (1, 0)]],
    "M": [[(0, 0), (0, 1), (0.5, 0.42), (1, 1), (1, 0)]],
    "N": [[(0, 0), (0, 1), (1, 0), (1, 1)]],
    "O": [[(0.25, 1), (0.75, 1), (1, 0.75), (1, 0.25), (0.75, 0),
           (0.25, 0), (0, 0.25), (0, 0.75), (0.25, 1)]],
    "P": [[(0, 0), (0, 1), (0.72, 1), (1, 0.82), (1, 0.63),
           (0.72, 0.45), (0, 0.45)]],
    "R": [[(0, 0), (0, 1), (0.72, 1), (1, 0.82), (1, 0.63),
           (0.72, 0.45), (0, 0.45)], [(0.5, 0.45), (1, 0)]],
    "S": [[(1, 0.85), (0.75, 1), (0.25, 1), (0, 0.82), (0, 0.66),
           (0.25, 0.52), (0.75, 0.52), (1, 0.36), (1, 0.16),
           (0.75, 0), (0.25, 0), (0, 0.15)]],
    "T": [[(0, 1), (1, 1)], [(0.5, 1), (0.5, 0)]],
    "U": [[(0, 1), (0, 0.25), (0.25, 0), (0.75, 0), (1, 0.25), (1, 1)]],
    "V": [[(0, 1), (0.5, 0), (1, 1)]],
    "W": [[(0, 1), (0.22, 0), (0.5, 0.6), (0.78, 0), (1, 1)]],
    "Y": [[(0, 1), (0.5, 0.5), (1, 1)], [(0.5, 0.5), (0.5, 0)]],
    "0": [[(0.25, 1), (0.75, 1), (1, 0.75), (1, 0.25), (0.75, 0),
           (0.25, 0), (0, 0.25), (0, 0.75), (0.25, 1)]],
    "1": [[(0.2, 0.8), (0.5, 1), (0.5, 0)], [(0.2, 0), (0.8, 0)]],
    "2": [[(0, 0.85), (0.25, 1), (0.75, 1), (1, 0.8), (0.9, 0.55),
           (0, 0), (1, 0)]],
    "3": [[(0, 0.88), (0.25, 1), (0.75, 1), (1, 0.8), (0.75, 0.55), (0.38, 0.55)],
          [(0.75, 0.55), (1, 0.32), (0.75, 0), (0.25, 0), (0, 0.12)]],
    "4": [[(0.75, 0), (0.75, 1), (0, 0.32), (1, 0.32)]],
    "5": [[(1, 1), (0, 1), (0, 0.58), (0.7, 0.62), (1, 0.4),
           (0.8, 0.05), (0.3, 0), (0, 0.12)]],
    "6": [[(0.9, 0.95), (0.55, 1), (0.15, 0.75), (0, 0.35), (0.2, 0.03),
           (0.6, 0), (0.95, 0.2), (0.85, 0.5), (0.5, 0.62), (0.12, 0.5)]],
    "7": [[(0, 1), (1, 1), (0.35, 0)]],
    "8": [[(0.28, 0.55), (0, 0.74), (0.25, 1), (0.75, 1), (1, 0.74),
           (0.72, 0.55), (0.28, 0.55), (0, 0.3), (0.25, 0), (0.75, 0),
           (1, 0.3), (0.72, 0.55)]],
    "9": [[(0.1, 0.05), (0.45, 0), (0.85, 0.25), (1, 0.65), (0.8, 0.97),
           (0.4, 1), (0.05, 0.8), (0.15, 0.5), (0.5, 0.38), (0.88, 0.5)]],
    " ": [],
}

GLYPH_W = 0.62
GLYPH_ADV = 0.90


def _stroke(p0, p1, w, h):
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    L = math.hypot(dx, dy)
    if L < 1e-9:
        return None
    b = Part.makeBox(L, w, h, V(0, -w / 2.0, 0))
    b.rotate(V(0, 0, 0), V(0, 0, 1), math.degrees(math.atan2(dy, dx)))
    b.translate(V(p0[0], p0[1], 0))
    return b


def text_solids(text, cx, cy, height, depth, stroke_w, z_bottom):
    """Engraved text as a list of solids, centred on (cx, cy)."""
    text = text.upper()
    adv = GLYPH_ADV * height
    total_w = len(text) * adv - (adv - GLYPH_W * height)
    x0 = cx - total_w / 2.0
    y0 = cy - height / 2.0
    solids = []
    for i, ch in enumerate(text):
        polys = GLYPHS.get(ch)
        if not polys:
            if ch not in GLYPHS:
                raise KeyError("stroke font has no glyph for %r" % ch)
            continue
        ox = x0 + i * adv
        for poly in polys:
            pts = [(ox + px * GLYPH_W * height, y0 + py * height) for px, py in poly]
            for j in range(len(pts) - 1):
                s = _stroke(pts[j], pts[j + 1], stroke_w, depth)
                if s:
                    solids.append(s)
            for p in pts:
                solids.append(cyl(stroke_w, depth, p[0], p[1], 0))
    for s in solids:
        s.translate(V(0, 0, z_bottom))
    return solids


# =============================================================================
# BASE
# =============================================================================
def build_base():
    body = rounded_box(OUT_W, OUT_D, BASE_H, CORNER_R)
    cavity = rounded_box(IN_W, IN_D, IN_H + 1.0, max(CORNER_R - WALL, 0.3),
                         origin=(WALL, WALL, FLOOR))
    body = body.cut(cavity)

    add, cut = [], []

    # --- lid screw bosses ----------------------------------------------------
    for (bx, by) in BOSSES:
        add.append(cyl(BOSS_OD, IN_H, IX(bx), IY(by), IZ(0)))
        cut.append(cyl(BOSS_PILOT_D, BOSS_PILOT_DEPTH,
                       IX(bx), IY(by), IZ(IN_H) - BOSS_PILOT_DEPTH))

    # --- Arduino standoffs (exact, from the published drawing) ---------------
    for (hx, hy) in UNO_HOLES:
        px, py = IX(UNO_X + hx), IY(UNO_Y + hy)
        add.append(cyl(UNO_BOSS_OD, UNO_STANDOFF_H, px, py, IZ(0)))
        cut.append(cyl(UNO_PILOT_D, UNO_STANDOFF_H + UNO_PILOT_SINK,
                       px, py, IZ(0) - UNO_PILOT_SINK))

    # --- shelf posts ---------------------------------------------------------
    # These carry the whole mains deck.  The shelf is a separate plate; see the
    # note by SHELF_POSTS for why it is not moulded into the base.
    for (px, py) in SHELF_POSTS:
        add.append(cyl(SHELF_POST_D, SHELF_Z0, IX(px), IY(py), IZ(0)))
    for (px, py) in SHELF_SCREW_POSTS:
        cut.append(cyl(SHELF_POST_PILOT, 14.0,
                       IX(px), IY(py), IZ(SHELF_Z0) - 12.0))

    # --- battery bracket (front zone, on the floor) --------------------------
    b = bracket_set(IX(BAT_X), IY(BAT_Y), BAT_L, BAT_W, BAT_BRACKET_H,
                    FIT_MODULE, lip_z=None, arm=9.0, thick=2.4)
    b.translate(V(0, 0, IZ(0)))
    add.append(b)

    # --- breadboard end stops (the side walls locate it in X) ----------------
    for ry in (BB_Y - 1.6, BB_Y + BB_W + 0.4):
        add.append(Part.makeBox(118.0, 1.6, 3.0, V(IX(26.0), IY(ry), IZ(0))))

    # --- Arduino connector opening, LEFT wall --------------------------------
    # Deliberately one wide opening.  The PCB outline and its mounting holes
    # are exact, but the USB / barrel-jack positions along that edge are not
    # worth betting a 350 g print on.  This clears both whatever they are.
    cut.append(Part.makeBox(WALL + 4.0, UNO_W + 2.0, 15.0,
                            V(-2.0, IY(UNO_Y - 1.0), IZ(UNO_STANDOFF_H - 0.5))))

    # --- rocker switch, FRONT wall - snaps straight in, no bezel -------------
    # The KCD1-101 carries its own clips.  We recess the inner face around the
    # hole so the wall is SW_PANEL_T thick where the clips grip, then cut the
    # exact switch hole through it.
    recess_d = WALL - SW_PANEL_T + 0.1
    cut.append(Part.makeBox(SW_RECESS_W, recess_d, SW_RECESS_H,
                            V(IX(SW_CX) - SW_RECESS_W / 2.0,
                              SW_PANEL_T,
                              IZ(SW_CZ) - SW_RECESS_H / 2.0)))
    cut.append(Part.makeBox(SW_OPEN_W, WALL + 4.0, SW_OPEN_H,
                            V(IX(SW_CX) - SW_OPEN_W / 2.0,
                              -2.0,
                              IZ(SW_CZ) - SW_OPEN_H / 2.0)))

    # --- incoming mains cord, BACK wall, above the shelf ---------------------
    cut.append(Part.makeCylinder(CABLE_D / 2.0, WALL + 4.0,
                                 V(IX(CABLE_CX), OUT_D - WALL - 2.0, IZ(CABLE_CZ)),
                                 V(0, 1, 0)))
    # Strain relief: a tie through these two holes, cinched round the cord
    # inside, stops any pull ever reaching the terminals.
    for dx in (-9.0, 9.0):
        cut.append(Part.makeCylinder(CABLE_TIE_W / 2.0, WALL + 4.0,
                                     V(IX(CABLE_CX + dx), OUT_D - WALL - 2.0,
                                       IZ(CABLE_CZ)),
                                     V(0, 1, 0)))

    # --- wall vents ----------------------------------------------------------
    # Z is clamped below the shelf, so these only ever open into the
    # low-voltage deck.  Nothing vents into the mains compartment.
    for i in range(10):
        x = 20.0 + 16.0 * i
        if x > IN_W - 18.0:
            continue
        if min(abs(x - bx) for (bx, _) in BOSSES) < 12.0:
            continue
        for zc in WALL_VENT_Z:
            # after the rotation below, WALL_VENT_W is the vertical extent
            if zc + WALL_VENT_W / 2.0 > SHELF_Z0 - 1.0:
                continue
            for is_front, y_at in ((True, -2.0), (False, IN_D - WALL - 2.0)):
                if is_front and abs(x - SW_CX) < SW_OPEN_W / 2.0 + 8.0:
                    continue          # do not clip the switch opening
                s = capsule(WALL_VENT_L, WALL_VENT_W, WALL + 4.0,
                            IX(x), 0.0, 0.0)
                s.rotate(V(IX(x), 0.0, 0.0), V(1, 0, 0), 90.0)
                s.translate(V(0.0, IY(y_at), IZ(zc)))
                cut.append(s)

    # --- floor lightening pockets --------------------------------------------
    # Pocketed from the INSIDE only, so the outer face stays flat and the first
    # layer prints clean.  Keeps well clear of anything that sits ON the floor
    # (the breadboard is not on standoffs) and of every post, boss and bracket.
    pocket_d = min(1.6, FLOOR - 1.2)
    keepouts = [(BB_X, BB_Y, BB_L, BB_W),
                (UNO_X, UNO_Y, UNO_L, UNO_W),
                (BAT_X, BAT_Y, BAT_L, BAT_W),
                (SW_CX - 22.0, 0.0, 44.0, SW_KEEPOUT_DEPTH)]
    for (px, py) in SHELF_POSTS:
        keepouts.append((px - 8.0, py - 8.0, 16.0, 16.0))
    for (bx, by) in BOSSES:
        keepouts.append((bx - 8.0, by - 8.0, 16.0, 16.0))

    pitch, cell = 22.0, 17.0
    pockets = []
    gy = 6.0
    while gy + cell < IN_D - 6.0:
        gx = 6.0
        while gx + cell < IN_W - 6.0:
            clash = False
            for (kx, ky, kw, kd) in keepouts:
                if (gx - 3.0 < kx + kw and kx < gx + cell + 3.0 and
                        gy - 3.0 < ky + kd and ky < gy + cell + 3.0):
                    clash = True
                    break
            if not clash:
                pockets.append(Part.makeBox(cell, cell, pocket_d + 1.0,
                                            V(IX(gx), IY(gy), IZ(0) - pocket_d)))
            gx += pitch
        gy += pitch
    say("   floor pockets: %d x %.0f mm, %.1f mm deep"
        % (len(pockets), cell, pocket_d))
    for s in pockets:
        body = body.cut(s)

    for s in add:
        body = body.fuse(s)
    for s in cut:
        body = body.cut(s)
    return body.removeSplitter()


# =============================================================================
# SHELF  -  separate plate, prints flat, partition wall rises off it
#           local coords: origin at its own front-left, plate bottom at Z=0
# =============================================================================
SH_W = IN_W - 0.6
SH_D = IN_D - FRONT_D - 0.6
SH_X0 = 0.3                           # interior X of the shelf's left edge
SH_Y0 = FRONT_D + 0.3                 # interior Y of the shelf's front edge


def _sh(x, y):
    """Interior coords -> shelf-local coords."""
    return x - SH_X0, y - SH_Y0


def build_shelf():
    body = Part.makeBox(SH_W, SH_D, SHELF_T)
    add, cut = [], []

    # Partition: the front wall of the mains compartment.  Vertical off a flat
    # plate, so it prints with no support.
    add.append(Part.makeBox(SH_W, WALL, MAINS_H, V(0.0, 0.0, SHELF_T)))

    # Clearance for the lid-screw bosses that pass through this region
    for (bx, by) in BOSSES:
        if by <= FRONT_D:
            continue
        lx, ly = _sh(bx, by)
        cut.append(cyl(BOSS_OD + 2.0, SHELF_T + 2.0, lx, ly, -1.0))

    # Screws down into the posts
    for (px, py) in SHELF_SCREW_POSTS:
        lx, ly = _sh(px, py)
        cut.append(cyl(SHELF_CLEAR_D, SHELF_T + 2.0, lx, ly, -1.0))

    # Relay and ACS712 sit on top, in the strip in front of the socket
    for (mx, my, ml, mw, mh, lz, arm, th) in (
        (RELAY_X, RELAY_Y, RELAY_L, RELAY_W, RELAY_BRACKET_H, RELAY_LIP_Z, 8.0, 2.2),
        (ACS_X, ACS_Y, ACS_L, ACS_W, ACS_BRACKET_H, ACS_LIP_Z, 6.0, 2.2),
    ):
        lx, ly = _sh(mx, my)
        b = bracket_set(lx, ly, ml, mw, mh, FIT_MODULE, lip_z=lz, arm=arm, thick=th)
        b.translate(V(0, 0, SHELF_T))
        add.append(b)

    # The only route between decks: six low-voltage signal wires.  The raised
    # collar keeps the creepage distance up where the wires cross.
    lx, ly = _sh(PASS_X, PASS_Y)
    add.append(Part.makeBox(PASS_L + 5.0, PASS_W + 5.0, SHELF_T + 4.0,
                            V(lx - 2.5, ly - 2.5, 0.0)))
    cut.append(Part.makeBox(PASS_L, PASS_W, SHELF_T + 6.0, V(lx, ly, -1.0)))

    for s in add:
        body = body.fuse(s)
    for s in cut:
        body = body.cut(s)
    return body.removeSplitter()


# =============================================================================
# LID  (built assembled: underside at Z=0, outer face at Z=LID_T)
# =============================================================================
def build_lid():
    body = rounded_box(OUT_W, OUT_D, LID_T, CORNER_R)
    add, cut = [], []

    for (bx, by) in BOSSES:
        px, py = IX(bx), IY(by)
        cut.append(cyl(LID_SCREW_CLEAR_D, LID_T + 4.0, px, py, -2.0))
        # Cone must be NARROW at the bottom and WIDE at the outer face, so the
        # screw head sinks flush. makeCone takes radius-at-base first.
        cut.append(Part.makeCone(LID_SCREW_CLEAR_D / 2.0, LID_CSK_D / 2.0,
                                 LID_CSK_DEPTH,
                                 V(px, py, LID_T - LID_CSK_DEPTH)))

    # --- mains socket opening + fixing tabs ----------------------------------
    # The body drops through a generous opening.  The socket's own fixing
    # screws land inside that opening with nothing to bite, so two tabs bridge
    # back across it, each carrying a slot narrower than an M3.5 - the screw
    # taps its own thread wherever along the slot it happens to land, covering
    # fixing centres from 90 to 150 mm.
    #
    # If the socket body turns out to foul a tab, snip it out: it is 2.6 mm of
    # PLA and takes ten seconds with side cutters.  Deliberately a recoverable
    # failure, not a scrapped lid.
    so = Part.makeBox(SOCKET_OPEN_L, SOCKET_OPEN_W, LID_T + 4.0,
                      V(IX(SOCKET_CX) - SOCKET_OPEN_L / 2.0,
                        IY(SOCKET_CY) - SOCKET_OPEN_W / 2.0, -2.0))
    for sgn in (-1, 1):
        xa = IX(SOCKET_CX) + sgn * SOCKET_SLOT_INNER
        xb = IX(SOCKET_CX) + sgn * (SOCKET_SLOT_INNER + SOCKET_SLOT_LEN)
        xa, xb = min(xa, xb), max(xa, xb)
        ya = IY(SOCKET_CY) - SOCKET_RIB_W / 2.0
        tab = Part.makeBox(xb - xa, SOCKET_RIB_W, LID_T + SOCKET_RIB_H,
                           V(xa, ya, -SOCKET_RIB_H))
        so = so.cut(tab)              # keep the tab out of the opening cut
        add.append(tab)
        cut.append(Part.makeBox(xb - xa - 4.0, SOCKET_SLOT_W,
                                LID_T + SOCKET_RIB_H + 2.0,
                                V(xa + 2.0, IY(SOCKET_CY) - SOCKET_SLOT_W / 2.0,
                                  -SOCKET_RIB_H - 1.0)))
    cut.append(so)

    # --- LCD window + exact mounting bosses ----------------------------------
    cut.append(Part.makeBox(LCD_WIN_L, LCD_WIN_W, LID_T + 4.0,
                            V(IX(LCD_CX) - LCD_WIN_L / 2.0,
                              IY(LCD_CY) - LCD_WIN_W / 2.0, -2.0)))
    # 45 deg chamfer opening out toward the outer face.  Printed outer-face-down
    # this is widest at the bed and narrows going up: self-supporting.
    ch = LCD_WIN_CHAMFER
    cut.append(Part.makeLoft(
        [rect_wire(IX(LCD_CX), IY(LCD_CY), LCD_WIN_L, LCD_WIN_W, LID_T - ch),
         rect_wire(IX(LCD_CX), IY(LCD_CY), LCD_WIN_L + 2 * (ch + 1.0),
                   LCD_WIN_W + 2 * (ch + 1.0), LID_T + 1.0)], True))
    for sx in (-1, 1):
        for sy in (-1, 1):
            px = IX(LCD_CX) + sx * LCD_HOLE_DX / 2.0
            py = IY(LCD_CY) + sy * LCD_HOLE_DY / 2.0
            add.append(cyl(LCD_BOSS_OD, LCD_BOSS_H, px, py, -LCD_BOSS_H))
            cut.append(cyl(LCD_PILOT_D, LCD_BOSS_H + LCD_PILOT_SINK,
                           px, py, -LCD_BOSS_H))

    # --- tactile buttons -----------------------------------------------------
    for (bcx, bcy) in (BTN1_C, BTN2_C):
        px, py = IX(bcx), IY(bcy)
        pocket = BTN_BODY + 2 * FIT
        collar = pocket + 4.0
        add.append(Part.makeBox(collar, collar, BTN_COLLAR_H,
                                V(px - collar / 2.0, py - collar / 2.0,
                                  -BTN_COLLAR_H)))
        cut.append(Part.makeBox(pocket, pocket, BTN_COLLAR_H + 0.01,
                                V(px - pocket / 2.0, py - pocket / 2.0,
                                  -BTN_COLLAR_H)))
        cut.append(cyl(BTN_PLUNGER_HOLE_D, LID_T + 4.0, px, py, -2.0))

    # --- buzzer --------------------------------------------------------------
    bx, by = IX(BUZZ_C[0]), IY(BUZZ_C[1])
    bpocket = BUZZ_D + 2 * FIT
    add.append(cyl(bpocket + 4.0, BUZZ_COLLAR_H, bx, by, -BUZZ_COLLAR_H))
    cut.append(cyl(bpocket, BUZZ_COLLAR_H + 0.01, bx, by, -BUZZ_COLLAR_H))
    cut.append(cyl(2.2, LID_T + 4.0, bx, by, -2.0))
    for i in range(6):
        a = math.radians(60 * i)
        cut.append(cyl(2.2, LID_T + 4.0,
                       bx + 4.0 * math.cos(a), by + 4.0 * math.sin(a), -2.0))

    # --- vents ---------------------------------------------------------------
    for vy in VENT_ROWS_Y:
        for vx in VENT_COLS_X:
            cut.append(capsule(VENT_SLOT_L, VENT_SLOT_W, LID_T + 4.0,
                               IX(vx + VENT_SLOT_L / 2.0), IY(vy), -2.0))

    # --- engraving on the outer face -----------------------------------------
    for text, (cx, cy), h, sw in LABELS:
        cut.extend(text_solids(text, IX(cx), IY(cy), h, TEXT_DEPTH + 0.5,
                               sw, LID_T - TEXT_DEPTH))

    for s in add:
        body = body.fuse(s)
    for s in cut:
        body = body.cut(s)
    return body.removeSplitter()


# =============================================================================
# FIT-CHECK COUPON
# =============================================================================
CP_W, CP_D, CP_T = 150.0, 140.0, 1.6


def build_coupon():
    plate = rounded_box(CP_W, CP_D, CP_T, 4.0)
    add, cut = [], []

    # (A) Arduino hole pattern - the single most important fit
    for (hx, hy) in UNO_HOLES:
        px, py = 6.0 + hx, 6.0 + hy
        add.append(cyl(UNO_BOSS_OD, UNO_STANDOFF_H, px, py, CP_T))
        cut.append(cyl(UNO_PILOT_D, UNO_STANDOFF_H + CP_T + 1.0, px, py, -0.5))

    # (B) relay and (C) ACS712 pockets
    for (mx, my, ml, mw, mh, lz, arm) in (
        (84.0, 8.0, RELAY_L, RELAY_W, RELAY_BRACKET_H, RELAY_LIP_Z, 8.0),
        (84.0, 44.0, ACS_L, ACS_W, ACS_BRACKET_H, ACS_LIP_Z, 6.0),
    ):
        b = bracket_set(mx, my, ml, mw, mh, FIT_MODULE, lip_z=lz, arm=arm)
        b.translate(V(0, 0, CP_T))
        add.append(b)

    # (D) LCD hole pattern + window.  ly is chosen so the lower bosses clear
    # the ACS bracket arms above them - they are neighbours only on this plate.
    lx, ly = 6.0, 62.0
    for sx in (-1, 1):
        for sy in (-1, 1):
            px = lx + LCD_L / 2.0 + sx * LCD_HOLE_DX / 2.0
            py = ly + LCD_W / 2.0 + sy * LCD_HOLE_DY / 2.0
            add.append(cyl(LCD_BOSS_OD, LCD_BOSS_H, px, py, CP_T))
            cut.append(cyl(LCD_PILOT_D, LCD_BOSS_H + CP_T + 1.0, px, py, -0.5))
    cut.append(Part.makeBox(LCD_WIN_L, LCD_WIN_W, CP_T + 2.0,
                            V(lx + (LCD_L - LCD_WIN_L) / 2.0,
                              ly + (LCD_W - LCD_WIN_W) / 2.0, -1.0)))

    # (E) button collar
    bpx, bpy = 100.0, 82.0
    pocket = BTN_BODY + 2 * FIT
    collar = pocket + 4.0
    add.append(Part.makeBox(collar, collar, BTN_COLLAR_H,
                            V(bpx - collar / 2.0, bpy - collar / 2.0, CP_T)))
    cut.append(Part.makeBox(pocket, pocket, BTN_COLLAR_H + 0.01,
                            V(bpx - pocket / 2.0, bpy - pocket / 2.0, CP_T)))
    cut.append(cyl(BTN_PLUNGER_HOLE_D, CP_T + 2.0, bpx, bpy, -1.0))

    # (F) buzzer collar
    zpx, zpy = 130.0, 88.0
    bpk = BUZZ_D + 2 * FIT
    add.append(cyl(bpk + 4.0, BUZZ_COLLAR_H, zpx, zpy, CP_T))
    cut.append(cyl(bpk, BUZZ_COLLAR_H + 0.01, zpx, zpy, CP_T))
    cut.append(cyl(2.2, CP_T + 2.0, zpx, zpy, -1.0))

    # (G) battery holder brackets
    b = bracket_set(20.0, 108.0, BAT_L, BAT_W, BAT_BRACKET_H, FIT_MODULE,
                    lip_z=None, arm=9.0, thick=2.4)
    b.translate(V(0, 0, CP_T))
    add.append(b)

    # (H) screw boss + countersink test
    add.append(cyl(BOSS_OD, 10.0, 132.0, 120.0, CP_T))
    cut.append(cyl(BOSS_PILOT_D, 12.0, 132.0, 120.0, CP_T))
    cut.append(cyl(LID_SCREW_CLEAR_D, CP_T + 2.0, 114.0, 120.0, -1.0))
    cut.append(Part.makeCone(LID_SCREW_CLEAR_D / 2.0, LID_CSK_D / 2.0,
                             LID_CSK_DEPTH, V(114.0, 120.0, CP_T - LID_CSK_DEPTH)))

    # lightening windows in the dead areas
    for (wx, wy, ww, wd) in ((30, 16, 34, 34), (96, 14, 26, 14),
                             (34, 114, 52, 12), (108, 96, 36, 20),
                             (6, 92, 34, 12), (108, 60, 34, 14)):
        cut.append(Part.makeBox(ww, wd, CP_T + 2.0, V(wx, wy, -1.0)))

    cut.extend(text_solids("GROUP 38", 62.0, 132.0, 6.0, 1.2, 1.2, CP_T - 0.7))

    for s in add:
        plate = plate.fuse(s)
    for s in cut:
        plate = plate.cut(s)
    return plate.removeSplitter()


# =============================================================================
# layout verification - refuses to build a box that cannot work
# =============================================================================
def check_layout():
    errs = []
    # name, x, y, length, width, height, pad
    #   pad = how far the mounting feature reaches past the module outline.
    #   Brackets stick out by clearance + wall thickness; the Arduino only has
    #   screw standoffs, whose widest point barely clears the PCB edge.
    br_pad = FIT_MODULE + 2.2
    bat_pad = FIT_MODULE + 2.4
    uno_pad = UNO_BOSS_OD / 2.0 - (UNO_L - UNO_HOLES[2][0])
    # Each module now sits on one of three decks, so only same-deck modules can
    # possibly collide and the overlap test is scoped accordingly.
    #   front : full-height zone at the front, Y < FRONT_D.  Low voltage.
    #   lower : under the shelf.  Low voltage.
    #   mains : on top of the shelf.  230 V.  Nothing else may enter it.
    rs = [
        # breadboard pad is 0: the side walls locate it in X and its only
        # added features are two end-stop ribs, checked separately below
        ("breadboard", "lower", BB_X, BB_Y, BB_L, BB_W, BB_H, 0.0),
        ("arduino", "front", UNO_X, UNO_Y, UNO_L, UNO_W,
         UNO_STANDOFF_H + UNO_PCB_T + UNO_TALL, max(uno_pad, 0.0)),
        ("battery", "front", BAT_X, BAT_Y, BAT_L, BAT_W, BAT_H, bat_pad),
        ("relay", "mains", RELAY_X, RELAY_Y, RELAY_L, RELAY_W, RELAY_TALL, br_pad),
        ("acs712", "mains", ACS_X, ACS_Y, ACS_L, ACS_W, 14.0, br_pad),
    ]
    WIRE_GAP = 2.0              # room to actually get a jumper in there

    DECK_Y = {"front": (0.0, FRONT_D),
              "lower": (FRONT_D, IN_D),
              "mains": (FRONT_D + WALL, IN_D)}
    DECK_H = {"front": IN_H - 3.0,
              "lower": LOWER_H - 1.0,
              "mains": MAINS_H - 2.0}

    for i in range(len(rs)):
        for j in range(i + 1, len(rs)):
            n1, k1, x1, y1, w1, d1, _, p1 = rs[i]
            n2, k2, x2, y2, w2, d2, _, p2 = rs[j]
            if k1 != k2:
                continue
            g = p1 + p2 + WIRE_GAP
            gap_x = max(x2 - (x1 + w1), x1 - (x2 + w2))
            gap_y = max(y2 - (y1 + d1), y1 - (y2 + d2))
            if max(gap_x, gap_y) < g:
                errs.append("TOO CLOSE: %s / %s - best gap %.1f mm, need %.1f"
                            % (n1, n2, max(gap_x, gap_y), g))

    for name, kind, x, y, w, d, h, pad in rs:
        dy0, dy1 = DECK_Y[kind]
        if x - pad < 0 or x + w + pad > IN_W:
            errs.append("OUT OF BOX: %s spans X %.1f..%.1f of %.1f"
                        % (name, x - pad, x + w + pad, IN_W))
        if y - pad < dy0 or y + d + pad > dy1:
            errs.append("OFF DECK: %s is on the %s deck, spans Y %.1f..%.1f, "
                        "deck is %.1f..%.1f"
                        % (name, kind, y - pad, y + d + pad, dy0, dy1))
        if h > DECK_H[kind]:
            errs.append("TOO TALL: %s is %.1f, the %s deck allows %.1f"
                        % (name, h, kind, DECK_H[kind]))
        for (bx, by) in BOSSES:
            cx = max(x - pad, min(bx, x + w + pad))
            cy = max(y - pad, min(by, y + d + pad))
            if math.hypot(cx - bx, cy - by) < BOSS_OD / 2.0 + 0.5:
                errs.append("BOSS CLASH: %s vs lid boss (%.1f, %.1f)"
                            % (name, bx, by))

    # clear volume behind the rocker switch - now in the FRONT wall
    sx0 = SW_CX - SW_OPEN_W / 2.0 - 4.0
    sx1 = SW_CX + SW_OPEN_W / 2.0 + 4.0
    for name, kind, x, y, w, d, _, pad in rs:
        if kind != "front":
            continue
        if x - pad < sx1 and sx0 < x + w + pad and y - pad < SW_KEEPOUT_DEPTH:
            errs.append("ROCKER CLASH: %s intrudes behind the switch" % name)
    if not (0 < SW_CX - SW_OPEN_W / 2.0 and SW_CX + SW_OPEN_W / 2.0 < IN_W):
        errs.append("rocker opening runs off the front wall")

    # --- mains socket --------------------------------------------------------
    so_x0 = SOCKET_CX - SOCKET_PLATE_L / 2.0
    so_x1 = SOCKET_CX + SOCKET_PLATE_L / 2.0
    so_y0 = SOCKET_CY - SOCKET_PLATE_W / 2.0
    so_y1 = SOCKET_CY + SOCKET_PLATE_W / 2.0
    if so_x0 < 1.0 or so_x1 > IN_W - 1.0 or so_y1 > IN_D - 1.0:
        errs.append("socket plate runs off the lid")
    if so_y0 < FRONT_D + WALL + 1.0:
        errs.append("socket plate overhangs the mains partition")
    # A countersunk lid screw hidden under the plate could never be reached.
    for (bx, by) in BOSSES:
        if (so_x0 - LID_CSK_D / 2.0 < bx < so_x1 + LID_CSK_D / 2.0 and
                so_y0 - LID_CSK_D / 2.0 < by < so_y1 + LID_CSK_D / 2.0):
            errs.append("lid screw (%.1f, %.1f) is buried under the socket plate"
                        % (bx, by))
    # relay and ACS712 live in the strip between partition and socket body
    for nm, ry, rd in (("relay", RELAY_Y, RELAY_W), ("acs712", ACS_Y, ACS_W)):
        if ry + rd > so_y0 - 1.0:
            errs.append("%s runs under the socket body (strip is %.1f mm deep)"
                        % (nm, so_y0 - (FRONT_D + WALL)))
    if MAINS_H - SOCKET_BODY_DEPTH < 1.0:
        errs.append("socket body bottoms out on the shelf")
    if SOCKET_SLOT_INNER + SOCKET_SLOT_LEN > SOCKET_PLATE_L / 2.0 - 2.0:
        errs.append("socket fixing slot runs past the edge of the plate")
    if SOCKET_OPEN_L >= SOCKET_PLATE_L - 6.0 or SOCKET_OPEN_W >= SOCKET_PLATE_W - 6.0:
        errs.append("socket opening leaves no rim for the plate to sit on")

    # --- shelf ---------------------------------------------------------------
    for (px, py) in SHELF_POSTS:
        if not (FRONT_D <= py <= IN_D):
            errs.append("shelf post (%.1f, %.1f) is not under the shelf"
                        % (px, py))
        if (BB_X - SHELF_POST_D / 2.0 < px < BB_X + BB_L + SHELF_POST_D / 2.0 and
                BB_Y - SHELF_POST_D / 2.0 < py < BB_Y + BB_W + SHELF_POST_D / 2.0):
            errs.append("shelf post (%.1f, %.1f) lands on the breadboard"
                        % (px, py))
        for (bx, by) in BOSSES:
            if math.hypot(px - bx, py - by) < (SHELF_POST_D + BOSS_OD) / 2.0 + 0.5:
                errs.append("shelf post (%.1f, %.1f) fouls lid boss (%.1f, %.1f)"
                            % (px, py, bx, by))
    for nm, mx, my, ml, mw in (("relay", RELAY_X, RELAY_Y, RELAY_L, RELAY_W),
                               ("acs712", ACS_X, ACS_Y, ACS_L, ACS_W)):
        if (mx - 3.0 < PASS_X + PASS_L and PASS_X < mx + ml + 3.0 and
                my - 3.0 < PASS_Y + PASS_W and PASS_Y < my + mw + 3.0):
            errs.append("wire pass-slot is buried under the %s" % nm)

    # LCD mounting bosses must not eat into the viewing window
    gap_x = (LCD_HOLE_DX - LCD_BOSS_OD) / 2.0 - LCD_WIN_L / 2.0
    gap_y = (LCD_HOLE_DY - LCD_BOSS_OD) / 2.0 - LCD_WIN_W / 2.0
    if gap_x < 0.5 or gap_y < 0.5:
        errs.append("LCD bosses cut into the window (clear %.2f x %.2f mm)"
                    % (gap_x, gap_y))
    if LCD_WIN_L < 64.5 + 1.0 or LCD_WIN_W < 16.4 + 1.0:
        errs.append("LCD window %.1f x %.1f is smaller than the 64.5 x 16.4 "
                    "character area" % (LCD_WIN_L, LCD_WIN_W))

    # lid features must not collide with each other
    lid_items = [("socket", SOCKET_CX, SOCKET_CY, SOCKET_PLATE_L, SOCKET_PLATE_W),
                 ("lcd", LCD_CX, LCD_CY, LCD_L, LCD_W),
                 ("button1", BTN1_C[0], BTN1_C[1],
                  BTN_BODY + 2 * FIT + 4.0, BTN_BODY + 2 * FIT + 4.0),
                 ("button2", BTN2_C[0], BTN2_C[1],
                  BTN_BODY + 2 * FIT + 4.0, BTN_BODY + 2 * FIT + 4.0),
                 ("buzzer", BUZZ_C[0], BUZZ_C[1],
                  BUZZ_D + 2 * FIT + 4.0, BUZZ_D + 2 * FIT + 4.0)]
    for i in range(len(lid_items)):
        for j in range(i + 1, len(lid_items)):
            n1, cx1, cy1, w1, d1 = lid_items[i]
            n2, cx2, cy2, w2, d2 = lid_items[j]
            if (abs(cx1 - cx2) < (w1 + w2) / 2.0 + 2.0 and
                    abs(cy1 - cy2) < (d1 + d2) / 2.0 + 2.0):
                errs.append("LID CLASH: %s and %s" % (n1, n2))
    for n, cx, cy, w, d in lid_items:
        if cx - w / 2.0 < 2.0 or cx + w / 2.0 > IN_W - 2.0 or \
           cy - d / 2.0 < 2.0 or cy + d / 2.0 > IN_D - 2.0:
            errs.append("LID OFF EDGE: %s" % n)
        for (bx, by) in BOSSES:
            if (abs(cx - bx) < w / 2.0 + BOSS_OD / 2.0 and
                    abs(cy - by) < d / 2.0 + BOSS_OD / 2.0):
                errs.append("LID CLASH: %s vs screw boss (%.1f, %.1f)"
                            % (n, bx, by))

    # the two big wall openings must leave a solid corner web between them
    usb_y_end = UNO_Y - 1.0 + UNO_W + 2.0
    web_front = SW_CX - SW_OPEN_W / 2.0 - 0.0
    if usb_y_end > FRONT_D:
        errs.append("USB opening (ends Y=%.1f) runs past the front zone (%.1f)"
                    % (usb_y_end, FRONT_D))
    if UNO_Y - 1.0 < 0:
        errs.append("USB opening runs off the left wall")
    if web_front < 15.0:
        errs.append("front-left corner web too thin: %.1f mm" % web_front)
    if SW_CZ - SW_OPEN_H / 2.0 < 3.0 or SW_CZ + SW_OPEN_H / 2.0 > IN_H - 4.0:
        errs.append("rocker opening too close to the floor or the lid")

    # the breadboard's two end-stop ribs must miss the front/back lid bosses
    for rib_y in (BB_Y - 1.6, BB_Y + BB_W + 0.4 + 1.6):
        for (bx, by) in BOSSES:
            if 26.0 <= bx <= 144.0 and abs(rib_y - by) < BOSS_OD / 2.0 + 0.5:
                errs.append("breadboard rib at Y=%.1f clashes with lid boss "
                            "at (%.1f, %.1f)" % (rib_y, bx, by))

    # The LCD hangs off the lid into the full-height front zone.  With the I2C
    # backpack fitted it reaches a long way down, so check it against whatever
    # actually stands underneath it.
    lcd_bottom = IN_H - LCD_STACK
    if lcd_bottom < 12.0:
        errs.append("LCD hangs to %.1f mm above the floor - too low" % lcd_bottom)
    for nm, mx, my, ml, mw, mh in (
            ("battery", BAT_X, BAT_Y, BAT_L, BAT_W, BAT_H),
            ("arduino", UNO_X, UNO_Y, UNO_L, UNO_W,
             UNO_STANDOFF_H + UNO_PCB_T + UNO_TALL)):
        if (abs(LCD_CX - (mx + ml / 2.0)) < (LCD_L + ml) / 2.0 and
                abs(LCD_CY - (my + mw / 2.0)) < (LCD_W + mw) / 2.0):
            if lcd_bottom < mh + 4.0:
                errs.append("LCD fouls the %s: %.1f mm clear, want 4+"
                            % (nm, lcd_bottom - mh))

    # button collars hang down too, and both buttons sit over the Arduino
    uno_top = UNO_STANDOFF_H + UNO_PCB_T + UNO_TALL
    if IN_H - BTN_COLLAR_H < uno_top + 4.0:
        errs.append("button collars foul the Arduino: %.1f mm clear"
                    % (IN_H - BTN_COLLAR_H - uno_top))

    # the Arduino connector opening must sit inside the left wall
    if UNO_Y - 1.0 < 0 or UNO_Y + UNO_W + 1.0 > FRONT_D:
        errs.append("Arduino connector opening runs off the left wall")

    return errs


# =============================================================================
def export(shape, name):
    import MeshPart
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)
    m = MeshPart.meshFromShape(Shape=shape, LinearDeflection=0.04,
                               AngularDeflection=0.20, Relative=False)
    m.write(os.path.join(OUT_DIR, name))
    bb = shape.BoundBox
    say("  %-30s %6.1f x %6.1f x %5.1f mm  %7.1f cm3  %6d tri  solid=%s"
          % (name, bb.XLength, bb.YLength, bb.ZLength,
             shape.Volume / 1000.0, m.CountFacets, shape.isValid()))
    return shape.Volume / 1000.0


def main():
    say("=" * 78)
    say("Smart Socket enclosure  -  Group 38")
    say("=" * 78)

    errs = check_layout()
    if errs:
        say("\nLAYOUT ERRORS - nothing was written:")
        for e in errs:
            say("   !! " + e)
        sys.exit(1)
    say("Layout check PASS: no overlaps, no boss clashes, nothing too tall,")
    say("               LCD/button clearance ok, switch volume clear.\n")

    base = build_base()
    lid = build_lid()
    shelf = build_shelf()
    coupon = build_coupon()

    lid_print = lid.copy()
    lid_print.rotate(V(0, 0, 0), V(1, 0, 0), 180.0)
    bb = lid_print.BoundBox
    lid_print.translate(V(0, -bb.YMin, -bb.ZMin))

    lid_asm = lid.copy()
    lid_asm.translate(V(0, 0, BASE_H))
    shelf_asm = shelf.copy()
    shelf_asm.translate(V(IX(SH_X0), IY(SH_Y0), IZ(SHELF_Z0)))
    asm = base.fuse(shelf_asm).fuse(lid_asm)

    say("Exports:")
    v1 = export(base, "SmartSocket_Base.stl")
    v2 = export(lid_print, "SmartSocket_Lid.stl")
    v5 = export(shelf, "SmartSocket_Shelf.stl")
    v4 = export(coupon, "SmartSocket_FitCheck.stl")
    export(asm, "SmartSocket_Assembly.stl")

    printed = v1 + v2 + v5
    say("")
    say("Assembled outer size : %.1f x %.1f x %.1f mm" % (OUT_W, OUT_D, TOTAL_H))
    say("Interior clear       : %.1f x %.1f x %.1f mm" % (IN_W, IN_D, IN_H))
    say("  front zone (LV)    : Y 0..%.1f, full height" % FRONT_D)
    say("  lower deck (LV)    : Y %.1f..%.1f, Z 0..%.1f" % (FRONT_D, IN_D, LOWER_H))
    say("  mains deck (230 V) : Y %.1f..%.1f, Z %.1f..%.1f"
        % (FRONT_D + WALL, IN_D, SHELF_Z1, IN_H))
    say("")
    say("Base                 : %6.1f cm3  (~%3.0f g)" % (v1, v1 * 1.24))
    say("Lid                  : %6.1f cm3  (~%3.0f g)" % (v2, v2 * 1.24))
    say("Shelf                : %6.1f cm3  (~%3.0f g)" % (v5, v5 * 1.24))
    say("-" * 46)
    say("TOTAL TO PRINT (3)   : %6.1f cm3  (~%3.0f g)  = %.0f GHS at 2.50/g"
        % (printed, printed * 1.24, printed * 1.24 * 2.50))
    say("Fit-check coupon     : %6.1f cm3  (~%3.0f g)  optional, not printed"
        % (v4, v4 * 1.24))
    say("=" * 78)


import traceback
try:
    main()
except Exception:
    say("\nBUILD FAILED:\n" + traceback.format_exc())
    sys.exit(1)
