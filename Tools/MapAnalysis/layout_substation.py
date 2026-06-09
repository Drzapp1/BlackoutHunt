# Copyright (c) 2026 Adam Rosta. All Rights Reserved.
# REDESIGNED Substation flow. Authored here as the single source of truth, verified by
# bh_mapcheck, then code-genned into BHGameMode::BuildSubstationLevel() (emit_cpp) so the
# C++ never drifts from the verified model.
#
# Design goals (the shipped map was topologically fine but read as a sealed maze and sent
# the "Power" compass beat to the far hunter-den corner):
#   * Open E-W spine at Y~0 the whole map width -> the compass bearing to power matches a run.
#   * 5 vertical "ribs" + 2 horizontal "band" walls, every room with >=2 WIDE (>=400u) arches
#     -> loops, no dead-ends, reads open.
#   * Doors only on the hall<->band crossings (slammable chokepoints / Teacher-interrupt
#     counterplay), spine + hall arches left open.
#   * Breakers ordered NEAR->FAR from the east spawn so the first "Power" beat is a fair,
#     close objective and danger ramps west toward the hunter (last breakers near its den).
#   * Crawls pierce the band walls at the rib columns -- a hall<->band flank OFFSET from the
#     centre door, so a chased survivor slips across where the hunter covers the door.

from bh_mapcheck import Rect

# ---- macro grid ----
VX = [-4500, -2500, -500, 1800, 3900]           # vertical rib X lines
BANDY = [-2500, 2500]                            # horizontal band wall Y lines
RIB_GAPS_Y = [(0, 520), (-1700, 460), (1700, 460), (-3400, 460), (3400, 460)]  # (centre,width)
BAND_GAP_X = [(-3500, 460), (-1500, 460), (650, 460), (2850, 460), (4800, 520)] # door columns
WEST_X = -4500                                   # west gallery is open (no band walls) west of here
THICK = 0.30
WALLTOP = 3.25                                   # SpawnBlock z-scale for interior walls

# crawl flanks: (cx, cy, wall_along_x, label) -- pierce a band wall INSIDE a room, offset ~700u
# from that room's centre door, so it is a true parallel hall<->band route the hunter can't
# cover while guarding the door. X chosen in the solid span between door gaps.
CRAWLS = [
    (-2100, 2500, True, "hall<->N band, off the -1500 door"),
    (1300, 2500, True, "hall<->N band, off the 650 door"),
    (2300, 2500, True, "hall<->N band, off the 2850 door"),
    (-2100, -2500, True, "hall<->S band, off the -1500 door"),
    (2300, -2500, True, "hall<->S band, off the 2850 door"),
]
CRAWL_LEN = 6.5

# breakers, PRE-SORTED near->far from the spawn centroid (4400,0) so the beat ramps outward
BREAKERS = [
    (2850, 1900), (2850, -1900),     # east-mid rooms  (first beats, fair + close)
    (-350, 1900), (-350, -1900),     # central hall flanks
    (-1500, 3400), (-1500, -3400),   # band rooms
    (-5650, 3400), (-5650, -3400),   # west gallery (last, near the hunter den)
]

STATIONS = [
    (-5650, 0, "Valve"), (-5650, -1700, "Terminal"), (-5650, 1700, "Antenna"),  # west gallery
    (-3500, -1250, "Evidence"), (-3500, 1250, "Valve"),
    (-3500, -3500, "Terminal"), (-3500, 3500, "Antenna"),
    (-1200, 0, "Evidence"), (1100, 0, "Valve"),                                  # spine
    (-350, -1900, "Terminal"), (-350, 1900, "Antenna"),
    (2850, -1250, "Evidence"), (2850, 1250, "Valve"),
    (750, -3500, "Terminal"), (-1500, 3500, "Antenna"),
    (4250, 0, "Evidence"),                                                       # muster foyer
]

SPAWNS = [
    (4150, -1300), (4400, -1300), (4650, -1300), (4150, -450), (4400, -450), (4650, -450),
    (4150, 450), (4400, 450), (4650, 450), (4150, 1300), (4400, 1300), (4650, 1300),
]
HUNTER = (-5650, -1250)
EXIT = (5600, 0)
SWITCHES = [(-5900 + c * 1300, -4450) for c in range(1, 9)]


def _segments(orient, fixed, lo, hi, gaps, thick=THICK):
    """Emit (cx,cy,sx,sy) wall segments along a line with gaps. orient 'V': constant X=fixed,
    spanning Y in [lo,hi]. orient 'H': constant Y=fixed, spanning X in [lo,hi]."""
    out = []
    cur = lo
    for (gc, gw) in sorted(gaps):
        seg_hi = gc - gw / 2.0
        if seg_hi - cur > 60:
            c = (cur + seg_hi) / 2.0
            length = (seg_hi - cur) / 100.0
            out.append((fixed, c, thick, length) if orient == 'V' else (c, fixed, length, thick))
        cur = gc + gw / 2.0
    if hi - cur > 60:
        c = (cur + hi) / 2.0
        length = (hi - cur) / 100.0
        out.append((fixed, c, thick, length) if orient == 'V' else (c, fixed, length, thick))
    return out


def walls():
    w = []
    for x in VX:
        w += _segments('V', x, -4500, 4500, RIB_GAPS_Y)
    for y in BANDY:
        w += _segments('H', y, WEST_X, 6100, BAND_GAP_X)
    return w


def build():
    solids = []
    # perimeter
    solids += [Rect(0, -4600, 122, 0.35), Rect(0, 4600, 122, 0.35),
               Rect(-6100, 0, 0.35, 92), Rect(6100, 0, 0.35, 92)]
    # interior ribs + bands
    wall_specs = walls()
    solids += [Rect(x, y, sx, sy) for (x, y, sx, sy) in wall_specs]
    # central transformer banks (hall cover; at |Y|=1450 so they never block the Y~0 spine)
    for ty in (-1450, 1450):
        for tx in (-2000, -900, 300, 1400):
            solids.append(Rect(tx, ty, 1.4, 3.0))
    # a little freestanding cover in the big flanking rooms (off the spine + doorways)
    for (x, y, sx, sy) in [(-3500, -1900, 2.2, 0.9), (-3500, 1900, 2.2, 0.9),
                           (2850, -1900, 2.2, 0.9), (2850, 1900, 2.2, 0.9)]:
        solids.append(Rect(x, y, sx, sy))
    # squeeze hidey-pockets
    for (px, py) in ((-1900, -3350), (1100, 3350)):
        solids += [Rect(px + 150, py, 0.4, 3.4), Rect(px, py - 150, 3.0, 0.4), Rect(px, py + 150, 3.0, 0.4),
                   Rect(px - 150, py - 75, 0.4, 1.1), Rect(px - 150, py + 75, 0.4, 1.1)]

    doors = [(gx, by) for by in BANDY for (gx, _gw) in BAND_GAP_X]   # hall<->band chokepoints
    crawls = [(cx, cy, 90 if wax else 0, int(CRAWL_LEN * 100), 200) for (cx, cy, wax, _l) in CRAWLS]

    targets = []
    for i, (x, y) in enumerate(BREAKERS):
        targets.append({"kind": "breaker", "x": x, "y": y, "label": f"#{i} d={int((x-4400)**2+(y)**2)**0.5}"})
    for (x, y, _t) in STATIONS:
        targets.append({"kind": "station", "x": x, "y": y, "label": ""})
    for i, (x, y) in enumerate(SWITCHES):
        targets.append({"kind": "switch", "x": x, "y": y, "label": f"circuit {i+1}"})
    targets.append({"kind": "exit", "x": EXIT[0], "y": EXIT[1], "label": "platform"})
    targets.append({"kind": "hunter", "x": HUNTER[0], "y": HUNTER[1], "label": "hunter spawn"})

    return {
        "name": "Substation (AFTER / redesign)",
        "bounds": (-6300, 6300, -4800, 4800),
        "solids": solids, "doors": doors, "crawls": crawls,
        "targets": targets, "spawns": SPAWNS,
    }


# ----------------------------------------------------------------------------------------
# C++ code-gen: emit the exact literals to paste into BuildSubstationLevel()
# ----------------------------------------------------------------------------------------
def emit_cpp():
    def _f(v):
        s = f"{v:.2f}".rstrip("0").rstrip(".")
        if s in ("", "-0"):
            s = "0"
        if "." not in s:
            s += ".0"   # C++ float literals need a decimal point: '4500f' is invalid, '4500.0f' is fine
        return s

    def fv(x, y, z):
        return f"FVector({_f(x)}f, {_f(y)}f, {_f(z)}f)"

    print("\n// ===================== GENERATED by Tools/MapAnalysis/layout_substation.py =====================")
    print("// ---- Walls (ribs + band walls); z-center via CenterZForBlockBottom in the spawn loop ----")
    print("\tconst TArray<TPair<FVector, FVector>> Walls = {")
    rib_lines = []
    for x in VX:
        segs = _segments('V', x, -4500, 4500, RIB_GAPS_Y)
        rib_lines.append("\t\t" + ", ".join("{" + fv(sx_, sy_, 175.0) + ", " + fv(tx, ty, WALLTOP) + "}"
                                             for (sx_, sy_, tx, ty) in segs))
    for y in BANDY:
        # the C++ band wall ALSO needs a physical gap at each crawl column: SpawnCrawlGate only
        # caps the top with a lintel, so the prone duct needs an actual hole below it. (The
        # analyzer keeps the wall solid here and models the crawl as a prone-only carve, which is
        # the faithful in-game behaviour -- standing can't pass the lintel, only prone can.)
        crawl_gaps = [(cx, 240) for (cx, cy, wax, _l) in CRAWLS if wax and cy == y]
        segs = _segments('H', y, WEST_X, 6100, BAND_GAP_X + crawl_gaps)
        rib_lines.append("\t\t" + ", ".join("{" + fv(cx, cy, 175.0) + ", " + fv(sx, sy, WALLTOP) + "}"
                                             for (cx, cy, sx, sy) in segs))
    print(",\n".join(rib_lines))
    print("\t};")

    print("\n\tconst TArray<TPair<FVector, FRotator>> Doors = {")
    door_strs = []
    for by in BANDY:
        for (gx, _gw) in BAND_GAP_X:
            door_strs.append("{" + fv(gx, by, 120.0) + ", FRotator(0.0f, 90.0f, 0.0f)}")  # band walls run along X
    print("\t\t" + ", ".join(door_strs))
    print("\t};")

    print("\n\t// Breakers PRE-SORTED near->far from spawn (4400,0): first 'Power' beat is the closest.")
    print("\tconst TArray<TPair<FVector, FRotator>> Breakers = {")
    print("\t\t" + ", ".join("{" + fv(x, y, 80.0) + ", FRotator(0.0f, 0.0f, 0.0f)}" for (x, y) in BREAKERS))
    print("\t};")

    print("\n\t// Crawl flanks: pierce a band wall at a rib column (offset from the centre door).")
    for (cx, cy, wax, label) in CRAWLS:
        yaw = 90.0 if wax else 0.0
        print(f"\tSpawnCrawlGate({cx:.1f}f, {cy:.1f}f, {yaw}f, {str(wax).lower()}, {CRAWL_LEN}f);  // {label}")

    print("\n\tconst TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {")
    print("\t\t" + ",\n\t\t".join("{" + fv(x, y, 95.0) + f", EBHObjectiveStationType::{t}" + "}"
                                   for (x, y, t) in STATIONS))
    print("\t};")
    print("// ===================== END GENERATED =====================\n")
