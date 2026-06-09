"""
Top-down analysis + render of the Substation level as built by
ABHGameMode::BuildSubstationLevel() in Source/BlackoutHunt/BHGameMode.cpp.

Scale convention: SpawnBlock scale 1.0 == 100 world units (base mesh is the
100^3 /Engine/BasicShapes/Cube). Block center is (cx,cy); half-extent = scale*50.

Renders a PNG and runs a coarse flood-fill connectivity check to find
dead-end / single-entrance regions. Pure analysis -- changes nothing in-game.

Run with the psim venv python:  P:\psim_venv\Scripts\python.exe substation_map.py
"""
import sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle
from collections import deque

# ---------------------------------------------------------------- geometry ----
BX, BY = 6100.0, 4600.0  # half-extents of the outer shell

def rect(cx, cy, sx, sy):
    """center + block-scale -> (x0,y0,x1,y1) world rect."""
    return (cx - sx * 50, cy - sy * 50, cx + sx * 50, cy + sy * 50)

# Perimeter walls (collide)
PERIM = [
    rect(0, -4600, 122, 0.35), rect(0, 4600, 122, 0.35),
    rect(-6100, 0, 0.35, 92), rect(6100, 0, 0.35, 92),
]

# Main interior walls  (Walls[] in the builder)
MAIN_WALLS = [
    rect(-4400, -2500, 34, 0.30), rect(-900, -2500, 24, 0.30), rect(3200, -2500, 40, 0.30),
    rect(-4200, 0, 36, 0.30), rect(100, 0, 28, 0.30), rect(4300, 0, 28, 0.30),
    rect(-4100, 2500, 38, 0.30), rect(250, 2500, 34, 0.30), rect(4450, 2500, 30, 0.30),
    rect(-4500, -3400, 0.30, 24), rect(-4500, 1200, 0.30, 34),
    rect(-2500, -1200, 0.30, 26), rect(-2500, 3400, 0.30, 24),
    rect(-500, -3400, 0.30, 24), rect(-500, 1250, 0.30, 33),
    rect(1800, -1400, 0.30, 32), rect(1800, 3600, 0.30, 20),
    rect(3900, -3400, 0.30, 24), rect(3900, 1300, 0.30, 32),
]
# Extra partial walls (ExtraWalls[])
EXTRA_WALLS = [
    rect(-2000, -3500, 0.30, 13), rect(1300, -3500, 0.30, 13),
    rect(-2000, 3500, 0.30, 13), rect(1300, 3500, 0.30, 13),
    rect(-3300, -1900, 11, 0.30), rect(3000, 1900, 11, 0.30),
]

DOORS = [
    (-2400, -2500), (750, -2500), (-1850, 0), (2200, 0), (-1825, 2500), (2450, 2500),
    (-4500, -1350), (-2500, 1150), (-500, -1300), (1800, 1400), (3900, -1250),
]
SHUTTERS = [(-900, 0), (1800, 0), (3900, 0), (-2500, 2500)]

# Transformer bays: checkerboard steel boxes 110 x 350 units
TRANSFORMERS = []
for r in range(-3, 4):
    for c in range(-2, 3):
        if (r + c) % 2 == 0:
            TRANSFORMERS.append(rect(r * 950.0, c * 780.0, 1.1, 3.5))

BREAKERS = [(-5550, -3650), (-5400, 3650), (-850, -3800), (850, 3800),
            (3300, -3750), (5450, 3300), (5200, 350)]
OBJECTIVES = [
    (-5850, -1650), (-5850, 1650), (-1200, -3650), (1650, 3650), (5350, -1700),
    (5300, 2100), (-3600, -1250), (-3600, 1250), (-1200, -1250), (-1200, 1250),
    (1450, -1250), (1450, 1250), (3700, -1250), (3700, 1250), (-5550, -3650),
    (-5400, 3650), (-2500, -3650), (-2500, 3650), (-500, -3650), (-500, 3650),
    (1800, -3650), (1800, 3650), (5350, 350), (3900, 3650), (3900, -3650),
    (5450, -3300), (-5850, 0), (5350, 3900),
]
LOCKERS = [
    (-5750, -4050), (-5000, -4050), (-3100, -4050), (-1800, -4050), (-250, -4050),
    (1250, -4050), (3000, -4050), (5000, -4050), (-5750, 4050), (-5000, 4050),
    (-3100, 4050), (-1800, 4050), (-250, 4050), (1250, 4050), (3000, 4050),
    (5000, 4050), (-5850, -900), (-5850, 900), (5850, -900), (5850, 900),
    (2200, -400), (2200, 720), (-3100, -640), (-3100, 760),
]
CLUTTER = [
    (-5200, -3300), (-3600, -3300), (-1200, -3500), (1200, -3500), (3450, -3300),
    (5200, -2700), (-5200, 3200), (-3300, 3400), (-900, 3500), (1550, 3500),
    (3900, 3300), (5300, 2300),
    (-5700, -4100), (-5700, 4100), (5700, -4100), (5700, 4100),
]
SURV_SPAWNS = [
    (4860, -1420), (5120, -1040), (5380, -660), (4860, -240), (5120, 180),
    (5380, 580), (4860, 980), (5120, 1380), (4560, -980), (4560, 980),
    (5480, -1260), (5480, 1260),
]
HUNTER_SPAWN = (-5600, -1200)
EXIT_GATE = (5600, 0)

# crawl ducts (cx,cy,yaw,lenScale) -> approximate footprint (collides, with a
# prone-only interior). Treated here as cover, not a hard block, for flood-fill.
DUCTS = [
    (-3400, -3350, 0, 4.8), (1500, -3350, 0, 4.6), (-3400, 3350, 0, 4.8),
    (600, 3350, 0, 4.6), (-5650, 0, 90, 4.8), (-5650, -3000, 0, 3.6),
    (-5650, 3000, 0, 3.6), (4300, -3350, 0, 4.6),
]
SQUEEZE = [(-1900, -3350, 0), (2700, -3350, 180), (-1900, 3350, 0), (2700, 3350, 180)]

# Station footprint (subway exit station, +X gate). Approx blocks that collide
# and crowd the east spawn rooms. dir=+1, gate at 5600.
def sp(toward, along):
    return (5600 + toward, along)
# Walkable floor slabs (platform/concourse/track-bed) -- drawn, NOT blocking.
STATION_FLOORS = [
    rect(*sp(-360, 0), 10.2, 37.0), rect(*sp(-1040, 0), 6.2, 22.0),
    rect(*sp(340, 0), 4.1, 40.0),
]
# Real vertical obstacles: train cars, portal side walls, track back wall.
STATION_OBSTACLES = [
    rect(*sp(500, 0), 0.22, 41.0),
    rect(*sp(120, -1220), 0.62, 10.4), rect(*sp(120, 1220), 0.62, 10.4),
    rect(*sp(-650, -1980), 9.2, 0.22), rect(*sp(-650, 1980), 9.2, 0.22),
]
STATION_RECTS = STATION_FLOORS + STATION_OBSTACLES
STATION_PILLARS = [sp(-620, -1350 + i * 900) for i in range(4)]

# all hard blockers for flood-fill (station floors are walkable -> excluded)
HARD = PERIM + MAIN_WALLS + EXTRA_WALLS + TRANSFORMERS + STATION_OBSTACLES

# ---------------------------------------------------------- connectivity ------
CELL = 50.0
NX = int(BX * 2 / CELL)
NY = int(BY * 2 / CELL)

def w2g(x, y):
    return int((x + BX) / CELL), int((y + BY) / CELL)

blocked = np.zeros((NX, NY), dtype=bool)
for (x0, y0, x1, y1) in HARD:
    gx0, gy0 = w2g(min(x0, x1), min(y0, y1))
    gx1, gy1 = w2g(max(x0, x1), max(y0, y1))
    blocked[max(0, gx0):min(NX, gx1 + 1), max(0, gy0):min(NY, gy1 + 1)] = True

# carve door gaps (doors are walk-through actors) -- punch a 130u opening
for (dx, dy) in DOORS + SHUTTERS:
    gx, gy = w2g(dx, dy)
    blocked[max(0, gx - 2):gx + 3, max(0, gy - 2):gy + 3] = False

# flood fill from hunter spawn
start = w2g(*HUNTER_SPAWN)
reach = np.zeros((NX, NY), dtype=bool)
if not blocked[start]:
    q = deque([start])
    reach[start] = True
    while q:
        cx, cy = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < NX and 0 <= ny < NY and not blocked[nx, ny] and not reach[nx, ny]:
                reach[nx, ny] = True
                q.append((nx, ny))

open_cells = int((~blocked).sum())
reach_cells = int(reach.sum())
unreachable = int((~blocked & ~reach).sum())
print(f"open cells={open_cells}  reachable_from_hunter={reach_cells}  "
      f"unreachable_open={unreachable} ({100*unreachable/max(1,open_cells):.1f}%)")

# spawn reachability
for name, pt in [("hunter", HUNTER_SPAWN), ("exit", EXIT_GATE)] + \
        [(f"surv{i}", p) for i, p in enumerate(SURV_SPAWNS)]:
    gx, gy = w2g(*pt)
    print(f"  {name:8s} {pt} blocked={blocked[gx,gy]} reachable={reach[gx,gy]}")

# ---------------------------------------------- per-room entrance analysis ----
# Grid cell boundaries used by the builder's interior walls.
COLS = [-6100, -4500, -2500, -500, 1800, 3900, 6100]
ROWS = [-4600, -2500, 0, 2500, 4600]
MIN_RUN = 3          # >=150u contiguous opening counts as a usable entrance
PERIM_MARGIN = 150   # ignore openings hugging the outer shell (thin slivers)

def edge_open_run(fixed_axis, fixed_val, lo, hi):
    """max contiguous run (in cells) of open cells straddling a border line."""
    best = run = 0
    if fixed_axis == "x":  # vertical border at x=fixed_val, scan y in [lo,hi]
        gx = int((fixed_val + BX) / CELL)
        for y in range(lo + PERIM_MARGIN, hi - PERIM_MARGIN, int(CELL)):
            gy = int((y + BY) / CELL)
            a = 0 <= gx - 1 < NX and not blocked[gx - 1, gy]
            b = 0 <= gx < NX and not blocked[gx, gy]
            c = 0 <= gx + 1 < NX and not blocked[gx + 1, gy]
            run = run + 1 if (a and c and (b or True)) else 0
            best = max(best, run)
    else:                  # horizontal border at y=fixed_val, scan x in [lo,hi]
        gy = int((fixed_val + BY) / CELL)
        for x in range(lo + PERIM_MARGIN, hi - PERIM_MARGIN, int(CELL)):
            gx = int((x + BX) / CELL)
            a = 0 <= gy - 1 < NY and not blocked[gx, gy - 1]
            c = 0 <= gy + 1 < NY and not blocked[gx, gy + 1]
            run = run + 1 if (a and c) else 0
            best = max(best, run)
    return best

print("\n--- per-room entrance count (cells with <=1 entrance get you cornered) ---")
single = []
for ci in range(len(COLS) - 1):
    for ri in range(len(ROWS) - 1):
        x0, x1 = COLS[ci], COLS[ci + 1]
        y0, y1 = ROWS[ri], ROWS[ri + 1]
        ent = 0
        sides = []
        if ci > 0 and edge_open_run("x", x0, y0, y1) >= MIN_RUN:
            ent += 1; sides.append("W")
        if ci < len(COLS) - 2 and edge_open_run("x", x1, y0, y1) >= MIN_RUN:
            ent += 1; sides.append("E")
        if ri > 0 and edge_open_run("y", y0, x0, x1) >= MIN_RUN:
            ent += 1; sides.append("S")
        if ri < len(ROWS) - 2 and edge_open_run("y", y1, x0, x1) >= MIN_RUN:
            ent += 1; sides.append("N")
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        tag = f"C{ci}R{ri} center=({cx:.0f},{cy:.0f})"
        if ent <= 1:
            single.append((tag, ent, sides))
        print(f"  {tag:32s} entrances={ent} {sides}")
print(f"\n  *** {len(single)} rooms with <=1 entrance:")
for tag, ent, sides in single:
    print(f"      {tag}  entrances={ent} {sides}")

# ------------------------------------------------------------------ render ----
fig, ax = plt.subplots(figsize=(20, 15))
ax.set_facecolor("#0c0e10")

# reachable-area shading
img = np.zeros((NY, NX, 3))
img[(~blocked).T] = (0.10, 0.13, 0.15)
img[(reach).T] = (0.14, 0.20, 0.24)
ax.imshow(img, extent=[-BX, BX, -BY, BY], origin="lower", zorder=0)

def draw(rects, color, z=2, alpha=1.0):
    for (x0, y0, x1, y1) in rects:
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, color=color, zorder=z, alpha=alpha))

draw(PERIM, "#3a3f44", 2)
draw(MAIN_WALLS, "#6b7378", 3)
draw(EXTRA_WALLS, "#8a6b3a", 3)
draw(TRANSFORMERS, "#5a4030", 3, 0.95)
draw(STATION_RECTS, "#264", 3, 0.85)

def scatter(pts, color, marker, size, label, z=5):
    if not pts:
        return
    xs, ys = zip(*[(p[0], p[1]) for p in pts])
    ax.scatter(xs, ys, c=color, marker=marker, s=size, label=label, zorder=z,
               edgecolors="black", linewidths=0.4)

scatter(DOORS, "#39d", "s", 70, f"doors ({len(DOORS)})")
scatter(SHUTTERS, "#6cf", "D", 70, f"shutters ({len(SHUTTERS)})")
scatter(BREAKERS, "#fd3", "^", 130, f"breakers ({len(BREAKERS)})")
scatter(OBJECTIVES, "#f80", "o", 55, f"objectives ({len(OBJECTIVES)})")
scatter(LOCKERS, "#9af", "p", 45, f"lockers ({len(LOCKERS)})")
scatter(CLUTTER, "#a76", "X", 60, f"clutter props ({len(CLUTTER)})")
scatter([(c, d) for (c, d, *_2) in DUCTS], "#5c8", "_", 200, f"crawl ducts ({len(DUCTS)})")
scatter([(c, d) for (c, d, *_2) in SQUEEZE], "#5c8", "|", 200, f"squeeze ({len(SQUEEZE)})")
scatter(STATION_PILLARS, "#284", "h", 60, "station pillars")
scatter(SURV_SPAWNS, "#2f6", "*", 220, f"survivor spawns ({len(SURV_SPAWNS)})")
scatter([HUNTER_SPAWN], "#f24", "X", 320, "hunter spawn")
scatter([EXIT_GATE], "#0f8", "*", 480, "EXIT gate")

# grid lines used as room dividers, for reference
for gx in (-4500, -2500, -500, 1800, 3900):
    ax.axvline(gx, color="#444", lw=0.4, ls=":")
for gy in (-2500, 0, 2500):
    ax.axhline(gy, color="#444", lw=0.4, ls=":")

ax.set_xlim(-BX - 200, BX + 200)
ax.set_ylim(-BY - 200, BY + 200)
ax.set_aspect("equal")
ax.set_title("Substation -- CURRENT layout (top-down, +X = east / exit side)", color="w", fontsize=15)
ax.tick_params(colors="w")
leg = ax.legend(loc="upper left", fontsize=8, ncol=2, framealpha=0.85)
out = sys.argv[1] if len(sys.argv) > 1 else "substation_current.png"
plt.savefig(out, dpi=80, bbox_inches="tight", facecolor="#0c0e10")
print("wrote", out)
