# SPDX-License-Identifier: CC-BY-4.0
# Copyright (c) 2026 Kymopsis
"""
Radar Auto-Lock enclosure, parametric (CadQuery).

    pip install cadquery
    python enclosure.py            # writes the STL files next to this script

Coordinate system: X = left/right, Y = front (0, faces you) to back, Z = up.
All sizes in millimetres. Change the parameters below and run again.
"""

from pathlib import Path

import cadquery as cq

# ---------------------------------------------------------------- parameters

WALL = 2.0            # case walls
FLOOR = 5.5           # case floor (holds the dovetail groove)
LID_T = 2.0           # lid plate
CLR = 0.4             # general fit clearance

# ESP32-S3 N16R8 dual USB-C board (Kymopsis KYM-DT-ESP32S3): PCB incl. antenna
BOARD_L = 64.0        # USB-C end to antenna end
BOARD_W = 28.6
PCB_T = 1.6
BOARD_TOP_PARTS = 3.6  # tallest part on top (USB-C shells, RF shield)
ANTENNA_OVERHANG = 5.7  # antenna tab without pins at the far end
WIRE_SPACE = 24.0     # under the board: header pins + Dupont housings

# 60 GHz radar module, PCB antenna (2T2R, 25 x 31.5 mm)
RADAR_W = 31.5        # along X
RADAR_H = 25.0        # along Z (board stands upright, antennas face front)
RADAR_T = 5.2
RADOME = 1.5          # plastic in front of the antennas, ~half a wavelength in PLA/PETG
RADAR_TO_BOARD = 15.0  # free space behind the radar for its header and wires

# Monitor clip
CLIP_GAPS = (6, 12, 20)   # monitor/laptop-lid thickness at the top edge
CLIP_LEG = 30.0       # rear leg length
CLIP_LIP = 4.0        # front lip hanging over the bezel
CLIP_T = 2.6          # leg thickness (flexes a little, print in PETG if you can)

# Dovetail: groove under the case, tongue on the clip/foot (slides in from the side)
DT_NARROW = 9.0
DT_WIDE = 13.0
DT_H = 3.0
DT_Y = 12.0           # centre line, measured from the front face
DT_FIT = 0.25

# ---------------------------------------------------------------- derived

IN_X = BOARD_L + 1.0
POCKET_Y = RADOME + RADAR_T + CLR             # radar slot ends here
BOARD_Y0 = POCKET_Y + RADAR_TO_BOARD          # board front edge
IN_Y_END = BOARD_Y0 + BOARD_W + CLR           # inner back face

OUT_X = IN_X + 2 * WALL
OUT_Y = IN_Y_END + WALL
PCB_Z = FLOOR + WIRE_SPACE                    # underside of the PCB
CASE_H = PCB_Z + PCB_T + BOARD_TOP_PARTS + 2.0  # top rim of the case walls

X0 = WALL                                     # inner left wall
X1 = WALL + IN_X                              # inner right wall (USB side)
BOARD_X1 = X1 - 0.3                           # USB end of the board
BOARD_X0 = BOARD_X1 - BOARD_L
CX = OUT_X / 2

RADAR_Z0 = FLOOR + 2.0                        # radar rests on a 2 mm ledge
RADAR_Z1 = RADAR_Z0 + RADAR_H


def box(x0, y0, z0, x1, y1, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def dovetail_profile(narrow, wide, h, length, grow=0.0):
    """Trapezoid prism along X. Narrow side at z=0, wide side at z=+h."""
    n, w = narrow / 2 + grow, wide / 2 + grow
    return (
        cq.Workplane("YZ")
        .polyline([(-n, 0), (n, 0), (w, h), (-w, h)])
        .close()
        .extrude(length)
    )


# ---------------------------------------------------------------- case

def make_case():
    case = box(0, 0, 0, OUT_X, OUT_Y, CASE_H)
    case = case.cut(box(X0, WALL, FLOOR, X1, IN_Y_END, CASE_H + 1))

    # Radar window: thin the front wall to RADOME over the antenna area.
    rw, rh = RADAR_W + 1.0, RADAR_H + 1.0
    case = case.cut(box(CX - rw / 2, RADOME, RADAR_Z0 - 0.5, CX + rw / 2, WALL + 0.01, RADAR_Z0 + rh - 0.5))

    # Radar ledge and two retaining ribs behind the radar edges.
    case = case.union(box(CX - RADAR_W / 2 - 1, WALL - 0.01, FLOOR - 0.01, CX + RADAR_W / 2 + 1, POCKET_Y, RADAR_Z0))
    for sx in (-1, 1):
        xa = CX + sx * (RADAR_W / 2 - 3.0)
        xb = CX + sx * (RADAR_W / 2 + 1.0)
        case = case.union(box(min(xa, xb), POCKET_Y, FLOOR - 0.01, max(xa, xb), POCKET_Y + 2.5, RADAR_Z1))
    # Side guides so the radar cannot slide left/right.
    for sx in (-1, 1):
        xa = CX + sx * (RADAR_W / 2 + CLR / 2)
        xb = CX + sx * (RADAR_W / 2 + 1.6)
        case = case.union(box(min(xa, xb), WALL - 0.01, FLOOR - 0.01, max(xa, xb), POCKET_Y + 0.01, RADAR_Z1))

    # Board supports: a block under the pin-free antenna tab, a ledge at the USB end.
    tab_w = 15.0
    ycen = BOARD_Y0 + BOARD_W / 2
    case = case.union(box(X0 - 0.01, ycen - tab_w / 2, FLOOR - 0.01, BOARD_X0 + ANTENNA_OVERHANG - 0.8, ycen + tab_w / 2, PCB_Z))
    case = case.union(box(BOARD_X1 - 1.4, ycen - 6, FLOOR - 0.01, X1 + 0.01, ycen + 6, PCB_Z))
    # Back stop on both ends so the board cannot slide backwards/forwards.
    for ya, yb in ((BOARD_Y0 - 1.6, BOARD_Y0 - CLR / 2), ):
        case = case.union(box(X0 - 0.01, ya, PCB_Z - 3, BOARD_X0 + 3.0, yb, PCB_Z + PCB_T + 1.0))

    # USB-C opening in the right wall, both ports (plug overmolds need ~12 x 7 mm each).
    usb_w, usb_h = 26.0, 8.0
    usb_zc = PCB_Z + PCB_T + 1.6
    case = case.cut(box(X1 - 0.1, ycen - usb_w / 2, usb_zc - usb_h / 2, OUT_X + 1, ycen + usb_w / 2, usb_zc + usb_h / 2))

    # Dovetail groove in the underside, open at both ends (prints as a short bridge).
    groove = dovetail_profile(DT_NARROW, DT_WIDE, DT_H + 0.2, OUT_X + 2, grow=DT_FIT).translate((-1, DT_Y, -0.01))
    case = case.cut(groove)
    return case


# ---------------------------------------------------------------- lid

def make_lid():
    lid = box(0, 0, 0, OUT_X, OUT_Y, LID_T)
    # Inner lip, friction fit.
    lip_c = 0.2
    lip = box(X0 + lip_c, WALL + lip_c, -2.0, X1 - lip_c, IN_Y_END - lip_c, 0.01)
    lip = lip.cut(box(X0 + lip_c + 1.4, WALL + lip_c + 1.4, -2.1, X1 - lip_c - 1.4, IN_Y_END - lip_c - 1.4, 0.02))
    lid = lid.union(lip)
    # Keep the lip clear of the USB-C connectors at the right wall.
    ycen = BOARD_Y0 + BOARD_W / 2
    lid = lid.cut(box(X1 - 3.0, ycen - 14.0, -2.5, X1 + 0.5, ycen + 14.0, 0.0))
    # Small ridges on the long sides of the lip for a snug friction fit.
    for y, dy in ((WALL + lip_c, -0.35), (IN_Y_END - lip_c, 0.35)):
        for x in (OUT_X * 0.3, OUT_X * 0.7):
            ya, yb = sorted((y, y + dy))
            lid = lid.union(box(x - 2.5, ya - 0.01 * (dy > 0), -1.8, x + 2.5, yb + 0.01 * (dy < 0), -0.6))

    # Hold-downs: press the board onto its supports and keep the radar seated.
    lid_under = 0.0  # lid underside in lid coordinates; case rim = CASE_H
    to_pcb = CASE_H - (PCB_Z + PCB_T)              # drop from rim to PCB top
    to_usb = CASE_H - (PCB_Z + PCB_T + 3.3)         # drop to the USB-C shells
    lid = lid.union(box(BOARD_X0 + 0.8, ycen - 6, -to_pcb + 0.2, BOARD_X0 + ANTENNA_OVERHANG - 1.0, ycen + 6, lid_under + 0.01))
    lid = lid.union(box(BOARD_X1 - 6.5, ycen - 11, -to_usb + 0.2, BOARD_X1 - 1.0, ycen - 7.5, lid_under + 0.01))
    lid = lid.union(box(BOARD_X1 - 6.5, ycen + 7.5, -to_usb + 0.2, BOARD_X1 - 1.0, ycen + 11, lid_under + 0.01))
    to_radar = CASE_H - RADAR_Z1
    lid = lid.union(box(CX - 8, WALL + 0.6, -to_radar + 0.3, CX + 8, POCKET_Y - 0.6, lid_under + 0.01))

    # Window over the BOOT/RST buttons and the RGB LED (USB end of the board).
    lid = lid.cut(box(BOARD_X1 - 46.0, BOARD_Y0 + 5.0, -3, BOARD_X1 - 7.5, BOARD_Y0 + BOARD_W - 5.0, LID_T + 1))
    # Vent slots over the wiring area.
    for i in range(4):
        x = X0 + 8 + i * 6.0
        lid = lid.cut(box(x, WALL + 4, -3, x + 2.4, BOARD_Y0 - 3, LID_T + 1))
    return lid


# ---------------------------------------------------------------- clip

def make_clip(gap):
    plate_t = 3.0
    plate_x = OUT_X - 10.0
    y_leg = CLIP_T + gap                        # front face of the rear leg
    plate_y = max(DT_Y + DT_WIDE / 2 + 4.0, y_leg + CLIP_T)
    x0 = (OUT_X - plate_x) / 2

    clip = box(x0, 0, -plate_t, x0 + plate_x, plate_y, 0)
    # Dovetail groove from the top face, open at both ends.
    tongue = dovetail_profile(DT_NARROW, DT_WIDE, DT_H, plate_x).translate((x0, DT_Y, -0.01))
    clip = clip.union(tongue)

    # Front lip over the bezel.
    clip = clip.union(box(x0, 0, -plate_t - CLIP_LIP, x0 + plate_x, CLIP_T, -plate_t + 0.01))

    # Rear leg, leaning 4 degrees toward the monitor for grip, with a rounded foot.
    import math
    lean = math.radians(4.0)
    pivot_y, pivot_z = y_leg, -plate_t
    leg = box(x0 + 6, y_leg, -plate_t - CLIP_LEG, x0 + plate_x - 6, y_leg + CLIP_T, -plate_t + 0.01)
    leg = leg.rotate((0, pivot_y, pivot_z), (1, pivot_y, pivot_z), -4.0)
    clip = clip.union(leg)
    foot_y = pivot_y + CLIP_T / 2 * math.cos(lean) - CLIP_LEG * math.sin(lean)
    foot_z = pivot_z - CLIP_LEG * math.cos(lean) - CLIP_T / 2 * math.sin(lean)
    foot = (
        cq.Workplane("YZ")
        .circle(CLIP_T * 0.8)
        .extrude(plate_x - 12)
        .translate((x0 + 6, foot_y, foot_z))
    )
    clip = clip.union(foot)
    return clip


def make_desk_foot(tilt_deg=6.0):
    """Stand for the desk (below the monitor) instead of the clip. Tilts the radar up."""
    import math
    depth = OUT_Y + 6
    top = box(0, 0, -3.0, OUT_X, depth, 0)
    top = top.union(dovetail_profile(DT_NARROW, DT_WIDE, DT_H, OUT_X).translate((0, DT_Y, -0.01)))
    # Raise the front: rotate about the back edge.
    top = top.rotate((0, depth, 0), (1, depth, 0), -tilt_deg)
    rise = depth * math.sin(math.radians(tilt_deg))
    filler = (
        cq.Workplane("YZ")
        .polyline([(0, -8.0), (depth, -8.0), (depth, -2.0), (0, rise - 2.0)])
        .close()
        .extrude(OUT_X)
    )
    foot = top.union(filler)
    foot = foot.cut(box(-1, -5, -30, OUT_X + 1, depth + 5, -8.0))  # flat bottom on the bed
    return foot


# ---------------------------------------------------------------- export

def main():
    out = Path(__file__).resolve().parent
    parts = {
        "case.stl": make_case(),
        # Lid printed upside down: flip so the flat top sits on the bed.
        "lid.stl": make_lid().rotate((0, 0, 0), (1, 0, 0), 180),
        "desk-foot.stl": make_desk_foot(),
    }
    for g in CLIP_GAPS:
        # Print the clip standing on its end: the side profile lies on the bed, no supports.
        parts[f"clip-{g}mm.stl"] = make_clip(g).rotate((0, 0, 0), (0, 1, 0), 90)
    for name, shape in parts.items():
        cq.exporters.export(shape, str(out / name), tolerance=0.02, angularTolerance=0.1)
        bb = shape.val().BoundingBox()
        print(f"{name:16s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:6.1f} mm")
    print(f"case outside: {OUT_X:.1f} x {OUT_Y:.1f} x {CASE_H + LID_T:.1f} mm")


if __name__ == "__main__":
    main()
