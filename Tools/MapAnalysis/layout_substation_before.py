# Copyright (c) 2026 Adam Rosta. All Rights Reserved.
# The CURRENTLY-SHIPPED Substation geometry, transcribed from
# BHGameMode::BuildSubstationLevel() (BHGameMode.cpp ~9485) for baseline diagnosis.
# This is read-only reference: the redesign lives in layout_substation.py.

from bh_mapcheck import Rect


def build():
    solids = []

    # ---- perimeter (AddMapContainment(6100,4600)) ----
    solids += [
        Rect(0, -4600, 122, 0.35), Rect(0, 4600, 122, 0.35),
        Rect(-6100, 0, 0.35, 92), Rect(6100, 0, 0.35, 92),
    ]

    # ---- interior collision walls (the `Walls` array) ----
    walls = [
        # vertical dividers (run along Y)
        (-4500, -4150, 0.30, 9.0), (-4500, -2400, 0.30, 20.0), (-4500, 0, 0.30, 22.0), (-4500, 2250, 0.30, 17.0), (-4500, 4300, 0.30, 6.0),
        (-2500, -4300, 0.30, 6.0), (-2500, -2705, 0.30, 7.9), (-2500, -1745, 0.30, 6.9), (-2500, 0, 0.30, 22.0), (-2500, 2250, 0.30, 17.0), (-2500, 4300, 0.30, 6.0),
        (-500, -4150, 0.30, 9.0), (-500, -2950, 0.30, 9.0), (-500, 2950, 0.30, 9.0), (-500, 4150, 0.30, 9.0),
        (1800, -4300, 0.30, 6.0), (1800, -2250, 0.30, 17.0), (1800, 0, 0.30, 22.0), (1800, 1745, 0.30, 6.9), (1800, 2705, 0.30, 7.9), (1800, 4300, 0.30, 6.0),
        (3900, -4300, 0.30, 6.0), (3900, -2250, 0.30, 17.0), (3900, -450, 0.30, 13.0), (3900, 1225, 0.30, 14.5), (3900, 2675, 0.30, 8.5), (3900, 4300, 0.30, 6.0),
        # horizontal dividers (run along X), X[-4500,6100] only -> west gallery open top-to-bottom
        (-4075, -2500, 8.5, 0.30), (-2500, -2500, 17.0, 0.30), (-375, -2500, 19.5, 0.30), (1232.5, -2500, 6.65, 0.30), (2117.5, -2500, 6.65, 0.30), (3775, -2500, 10.5, 0.30), (5900, -2500, 4.0, 0.30),
        (-4075, 0, 8.5, 0.30), (-2750, 0, 12.0, 0.30), (2425, 0, 5.5, 0.30), (3250, 0, 5.0, 0.30), (5900, 0, 4.0, 0.30),
        (-4075, 2500, 8.5, 0.30), (-2930, 2500, 8.4, 0.30), (-1970, 2500, 6.4, 0.30), (-375, 2500, 19.5, 0.30), (1675, 2500, 15.5, 0.30), (3775, 2500, 10.5, 0.30), (5900, 2500, 4.0, 0.30),
    ]
    solids += [Rect(x, y, sx, sy) for (x, y, sx, sy) in walls]

    # ---- freestanding cover ----
    cover = [
        (-3500, -1900, 2.2, 0.9), (-3500, 1900, 2.2, 0.9), (2850, -1900, 2.2, 0.9), (2850, 1900, 2.2, 0.9),
        (-1500, -700, 0.9, 2.4), (1100, 700, 0.9, 2.4),
    ]
    solids += [Rect(x, y, sx, sy) for (x, y, sx, sy) in cover]

    # ---- central transformer banks (2 rows x 4 cols, scale 1.4x3.0) ----
    for ty in (-1450, 1450):
        for tx in (-2000, -900, 300, 1400):
            solids.append(Rect(tx, ty, 1.4, 3.0))

    # ---- squeeze pockets (5 walls each), at (-1900, +/-3350) ----
    for (px, py) in ((-1900, -3350), (-1900, 3350)):
        solids += [
            Rect(px + 150, py, 0.4, 3.4),       # back
            Rect(px, py - 150, 3.0, 0.4),       # side
            Rect(px, py + 150, 3.0, 0.4),       # side
            Rect(px - 150, py - 75, 0.4, 1.1),  # front stub
            Rect(px - 150, py + 75, 0.4, 1.1),  # front stub
        ]

    # ---- doors (openable -> passable gaps) ----
    doors = [
        (-4500, -3550), (-4500, -1250), (-4500, 1250), (-2500, 1250), (-500, -3550), (-500, 3550),
        (1800, -1250), (3900, -1250), (3900, 2100),
        (-3500, -2500), (-1500, -2500), (750, -2500), (-3500, 0), (2850, 0), (-3500, 2500), (-1500, 2500), (750, 2500),
    ]

    # ---- crawl gates (survivor-passable corridors): (cx,cy,yaw,length,width) ----
    crawls = [
        (-2500, -2200, 0, 650, 200),
        (1800, 2200, 0, 650, 200),
        (-2400, 2500, 90, 650, 200),
        (1675, -2500, 90, 650, 200),
    ]

    # ---- reachability targets ----
    targets = []
    breakers = [(-5650, -3400), (-5650, 3400), (-1500, -3700), (2850, -3700), (750, 3700), (2850, 3700), (5300, -1700), (5300, 1700)]
    for i, (x, y) in enumerate(breakers):
        targets.append({"kind": "breaker", "x": x, "y": y, "label": f"#{i} (beat order)"})

    stations = [
        (-5650, 0), (-5650, -1700), (-5650, 1700), (-3500, -1250), (-3500, 1250), (-3500, -3550), (-3500, 3550),
        (-1200, 0), (1100, 0), (-350, -1900), (-350, 1900), (2850, -1250), (2850, 1250), (750, -3700), (-1500, 3700), (4250, 0),
    ]
    for x, y in stations:
        targets.append({"kind": "station", "x": x, "y": y, "label": ""})

    for circuit in range(1, 9):
        targets.append({"kind": "switch", "x": -5900 + circuit * 1300, "y": -4450, "label": f"circuit {circuit}"})

    targets.append({"kind": "exit", "x": 5600, "y": 0, "label": "platform"})
    targets.append({"kind": "hunter", "x": -5650, "y": -1250, "label": "hunter spawn"})

    spawns = [
        (4150, -1300), (4400, -1300), (4650, -1300), (4150, -450), (4400, -450), (4650, -450),
        (4150, 450), (4400, 450), (4650, 450), (4150, 1300), (4400, 1300), (4650, 1300),
    ]

    return {
        "name": "Substation (BEFORE / shipped)",
        "bounds": (-6300, 6300, -4800, 4800),
        "solids": solids,
        "doors": doors,
        "crawls": crawls,
        "targets": targets,
        "spawns": spawns,
    }
