"""
PROPOSED Substation layout (bold open-up redesign). Designed here and verified
against the same metrics as substation_map.py BEFORE porting to C++:
  - every room has >=2 entrances (no cornering)
  - fully reachable, with loops
  - far lower object density + a long open central transformer hall

Walls are generated from an explicit per-gridline "openings" spec so the
geometry is exact and portable to BHGameMode.cpp.
Run: P:\\psim_venv\\Scripts\\python.exe substation_proposed.py proposed.png
"""
import sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from collections import deque

BX, BY = 6100.0, 4600.0

def rect(cx, cy, sx, sy):
    return (cx - sx * 50, cy - sy * 50, cx + sx * 50, cy + sy * 50)

# ---- wall generator from openings -------------------------------------------
# openings: list of (center, width). Wall fills the closed gaps between them.
# Narrow openings (<= DOOR_MAX) get a door actor; wide ones are open arches.
DOOR_MAX = 320

def gen_walls(axis, line, lo, hi, openings):
    rects, doors = [], []
    pts = sorted(openings)
    cur = lo
    for (oc, ow) in pts:
        o0, o1 = oc - ow / 2, oc + ow / 2
        if o0 > cur:
            rects.append((axis, line, cur, o0))
        cur = max(cur, o1)
        if ow <= DOOR_MAX:
            doors.append((axis, line, oc))
    if cur < hi:
        rects.append((axis, line, cur, hi))
    out, specs = [], []
    for (ax, ln, a, b) in rects:
        c, L = (a + b) / 2, (b - a)
        if ax == "x":
            out.append(rect(ln, c, 0.30, L / 100)); specs.append((ln, c, 0.30, round(L / 100, 2)))
        else:
            out.append(rect(c, ln, L / 100, 0.30)); specs.append((c, ln, round(L / 100, 2), 0.30))
    door_pts = [(ln, oc, 0.0) if ax == "x" else (oc, ln, 90.0) for (ax, ln, oc) in doors]
    return out, door_pts, specs

# Vertical gridlines (X), span Y. West gallery C0 (X<-4500) stays open.
VSPEC = {
    -4500: [(-3550, 300), (-1250, 300), (1250, 300), (3550, 900)],   # C0 gallery <-> C1 (4 ways)
    -2500: [(-3550, 900), (-1250, 300), (1250, 300), (3550, 900)],   # C1 <-> central, bands open
     -500: [(-3550, 300), (0, 5000), (3550, 300)],                   # central hall: wide open
     1800: [(-3550, 900), (-1250, 300), (1250, 300), (3550, 900)],   # central <-> C4
     3900: [(-3550, 900), (-1250, 300), (350, 300), (2100, 300), (3550, 900)],  # C4 <-> C5 muster
}
# Horizontal gridlines (Y), span X[-4500,6100] (leave west gallery open).
HSPEC = {
    -2500: [(-3500, 300), (-1500, 300), (750, 300), (2850, 800), (5000, 1400)],
        0: [(-3500, 300), (0, 4300), (2850, 300), (4600, 2200)],     # central hall + muster foyer open N-S
     2500: [(-3500, 300), (-1500, 300), (750, 300), (2850, 800), (5000, 1400)],
}

MAIN_WALLS, DOORS, WALL_SPECS = [], [], []
for x, ops in VSPEC.items():
    w, d, s = gen_walls("x", x, -4600, 4600, ops)
    MAIN_WALLS += w; DOORS += d; WALL_SPECS += s
for y, ops in HSPEC.items():
    w, d, s = gen_walls("y", y, -4500, 6100, ops)
    MAIN_WALLS += w; DOORS += d; WALL_SPECS += s

PERIM = [rect(0, -4600, 122, 0.35), rect(0, 4600, 122, 0.35),
         rect(-6100, 0, 0.35, 92), rect(6100, 0, 0.35, 92)]

# Central transformer hall: 8 boxes flanking a clear central cross (long E-W
# sightline kept at |Y|<900; N-S gaps between boxes).
TRANSFORMERS = [rect(x, y, 1.4, 3.0)
                for y in (-1450, 1450) for x in (-2000, -900, 300, 1400)]

SHUTTERS = [(-2500, -1250), (1800, 1250), (3900, 350)]   # gameplay shutters on doorways

BREAKERS = [(-5650, -3400), (-5650, 3400), (-1500, -3700), (750, 3700),
            (2850, -3700), (2850, 3700), (5300, -1700), (5300, 1700)]
OBJECTIVES = [
    (-5650, 0), (-5650, -1700), (-5650, 1700),          # west gallery (3)
    (-3500, -1250), (-3500, 1250), (-3500, -3550), (-3500, 3550),  # C1 rooms (4)
    (-1200, 0), (1100, 0), (-350, -1900), (-350, 1900),  # central hall (4)
    (2850, -1250), (2850, 1250),                          # C4 rooms (2)
    (750, -3700), (-1500, 3700),                          # bands (2)
    (4250, 0),                                            # muster (1)
]
LOCKERS = [
    (-5850, -3550), (-5850, -700), (-5850, 700), (-5850, 3550),  # gallery
    (-3500, -2300), (-3500, 2300), (-1500, -3550), (750, 3550),
    (2850, -2300), (2850, 2300), (-2300, 0), (1600, 0),
    (4100, -1500), (4100, 1500), (-700, -3550), (-700, 3550),
]
CLUTTER = [
    (-3500, -3550), (-3500, 3550), (-1500, -3550), (1300, -3550),
    (-1500, 3550), (1300, 3550), (2850, -3550), (2850, 3550),
    (-350, -1450), (-350, 1450),
]
SURV_SPAWNS = [(x, y) for x in (4150, 4400, 4650) for y in (-1300, -450, 450, 1300)]
HUNTER_SPAWN = (-5650, -1250)
EXIT_GATE = (5600, 0)

# Crawl-through-wall shortcuts: (cx, cy, tunnel-axis). Each tunnels through an interior divider connecting two
# rooms (prone-only; lintel blocks standing). lenScale 6.5 -> ~325u each side of the wall.
CRAWLS = [(-2500, -2200, 'x'), (1800, 2200, 'x'), (-2400, 2500, 'y'), (1675, -2500, 'y')]
CRAWL_HALF = 325
SQUEEZE = [(-1900, -3350, 0), (-1900, 3350, 0)]

# station (shared builder), gate at 5600 dir +1
def sp(t, a):
    return (5600 + t, a)
STATION_FLOORS = [rect(*sp(-360, 0), 10.2, 37.0), rect(*sp(-1040, 0), 6.2, 22.0), rect(*sp(340, 0), 4.1, 40.0)]
STATION_OBSTACLES = [rect(*sp(500, 0), 0.22, 41.0),
                     rect(*sp(120, -1220), 0.62, 10.4), rect(*sp(120, 1220), 0.62, 10.4),
                     rect(*sp(-650, -1980), 9.2, 0.22), rect(*sp(-650, 1980), 9.2, 0.22)]
STATION_PILLARS = [sp(-620, -1350 + i * 900) for i in range(4)]

# freestanding cover blocks added during implementation (small, must not seal a room)
COVER = [rect(-3500, -1900, 2.2, 0.9), rect(-3500, 1900, 2.2, 0.9),
         rect(2850, -1900, 2.2, 0.9), rect(2850, 1900, 2.2, 0.9),
         rect(-1500, -700, 0.9, 2.4), rect(1100, 700, 0.9, 2.4)]
# crawl-duct side walls (155u tall, collide) are real standing obstacles -- include them so the entrance
# metric proves the ducts don't seal a room. Side walls sit at +/-100 perpendicular, ~325u each side of the wall.
DUCT_WALLS = []
for (cx, cy, axis) in CRAWLS:
    if axis == 'x':
        DUCT_WALLS += [rect(cx, cy - 100, 6.5, 0.12), rect(cx, cy + 100, 6.5, 0.12)]
    else:
        DUCT_WALLS += [rect(cx - 100, cy, 0.12, 6.5), rect(cx + 100, cy, 0.12, 6.5)]
HARD = PERIM + MAIN_WALLS + TRANSFORMERS + STATION_OBSTACLES + COVER + DUCT_WALLS

# -------------------------------------------------------------- flood fill ----
CELL = 50.0
NX, NY = int(BX * 2 / CELL), int(BY * 2 / CELL)
def w2g(x, y): return int((x + BX) / CELL), int((y + BY) / CELL)
blocked = np.zeros((NX, NY), dtype=bool)
for (x0, y0, x1, y1) in HARD:
    gx0, gy0 = w2g(min(x0, x1), min(y0, y1)); gx1, gy1 = w2g(max(x0, x1), max(y0, y1))
    blocked[max(0, gx0):min(NX, gx1 + 1), max(0, gy0):min(NY, gy1 + 1)] = True
for pt in DOORS + SHUTTERS:
    dx, dy = pt[0], pt[1]
    gx, gy = w2g(dx, dy); blocked[max(0, gx - 2):gx + 3, max(0, gy - 2):gy + 3] = False
start = w2g(*HUNTER_SPAWN)
reach = np.zeros((NX, NY), dtype=bool)
q = deque([start]); reach[start] = True
while q:
    cx, cy = q.popleft()
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        nx, ny = cx + dx, cy + dy
        if 0 <= nx < NX and 0 <= ny < NY and not blocked[nx, ny] and not reach[nx, ny]:
            reach[nx, ny] = True; q.append((nx, ny))
oc = int((~blocked).sum()); rc = int(reach.sum()); un = oc - rc
print(f"open={oc} reachable={rc} unreachable_open={un} ({100*un/oc:.1f}%)")
for nm, pt in [("hunter", HUNTER_SPAWN), ("exit", EXIT_GATE)] + [(f"surv{i}", p) for i, p in enumerate(SURV_SPAWNS)]:
    gx, gy = w2g(*pt); print(f"  {nm:8s}{pt} blk={blocked[gx,gy]} reach={reach[gx,gy]}")

# -------------------------------------------------- per-room entrance count ----
COLS = [-6100, -4500, -2500, -500, 1800, 3900, 6100]
ROWS = [-4600, -2500, 0, 2500, 4600]
MIN_RUN, MARGIN = 3, 150
def run_open(axis, val, lo, hi):
    best = run = 0
    if axis == "x":
        gx = int((val + BX) / CELL)
        for y in range(lo + MARGIN, hi - MARGIN, int(CELL)):
            gy = int((y + BY) / CELL)
            ok = (0 <= gx - 1 < NX and not blocked[gx-1, gy]) and (0 <= gx+1 < NX and not blocked[gx+1, gy])
            run = run + 1 if ok else 0; best = max(best, run)
    else:
        gy = int((val + BY) / CELL)
        for x in range(lo + MARGIN, hi - MARGIN, int(CELL)):
            gx = int((x + BX) / CELL)
            ok = (0 <= gy-1 < NY and not blocked[gx, gy-1]) and (0 <= gy+1 < NY and not blocked[gx, gy+1])
            run = run + 1 if ok else 0; best = max(best, run)
    return best
print("\n--- per-room entrances ---")
bad = []
for ci in range(len(COLS) - 1):
    for ri in range(len(ROWS) - 1):
        x0, x1, y0, y1 = COLS[ci], COLS[ci+1], ROWS[ri], ROWS[ri+1]
        ent, sides = 0, []
        if ci > 0 and run_open("x", x0, y0, y1) >= MIN_RUN: ent += 1; sides.append("W")
        if ci < len(COLS)-2 and run_open("x", x1, y0, y1) >= MIN_RUN: ent += 1; sides.append("E")
        if ri > 0 and run_open("y", y0, x0, x1) >= MIN_RUN: ent += 1; sides.append("S")
        if ri < len(ROWS)-2 and run_open("y", y1, x0, x1) >= MIN_RUN: ent += 1; sides.append("N")
        tag = f"C{ci}R{ri} ({(x0+x1)//2},{(y0+y1)//2})"
        print(f"  {tag:24s} entrances={ent} {sides}")
        if ent <= 1: bad.append(tag)
print(f"\n  *** rooms with <=1 entrance: {len(bad)} {bad}")
print(f"  counts: objectives={len(OBJECTIVES)} transformers={len(TRANSFORMERS)} "
      f"lockers={len(LOCKERS)} clutter={len(CLUTTER)} crawls={len(CRAWLS)} doors={len(DOORS)}")

# crawl shortcuts must each connect two DISTINCT rooms (else it's a useless mid-room duct)
def room_of(x, y):
    ci = next((i for i in range(len(COLS) - 1) if COLS[i] <= x < COLS[i + 1]), -1)
    ri = next((i for i in range(len(ROWS) - 1) if ROWS[i] <= y < ROWS[i + 1]), -1)
    return f"C{ci}R{ri}"
print("\n--- crawl-through-wall shortcuts (each must join two DISTINCT rooms) ---")
crawl_ok = True
for (cx, cy, axis) in CRAWLS:
    if axis == 'x':
        a, b = room_of(cx - CRAWL_HALF, cy), room_of(cx + CRAWL_HALF, cy)
    else:
        a, b = room_of(cx, cy - CRAWL_HALF), room_of(cx, cy + CRAWL_HALF)
    ok = a != b
    crawl_ok = crawl_ok and ok
    print(f"  crawl ({cx:>5},{cy:>5}) {axis}: {a} <-> {b}   {'OK' if ok else '*** FAIL same room'}")
print(f"  *** all crawls connect distinct rooms: {crawl_ok}")

# ------------------------------------------------------------------ render ----
fig, ax = plt.subplots(figsize=(20, 15)); ax.set_facecolor("#0c0e10")
img = np.zeros((NY, NX, 3)); img[(~blocked).T] = (0.10, 0.13, 0.15); img[reach.T] = (0.14, 0.21, 0.25)
ax.imshow(img, extent=[-BX, BX, -BY, BY], origin="lower", zorder=0)
def draw(rs, c, z=2, a=1.0):
    for (x0, y0, x1, y1) in rs: ax.add_patch(Rectangle((x0, y0), x1-x0, y1-y0, color=c, zorder=z, alpha=a))
draw(PERIM, "#3a3f44"); draw(MAIN_WALLS, "#6b7378", 3)
draw(TRANSFORMERS, "#5a4030", 3, .95); draw(STATION_FLOORS, "#1c3326", 2, .8); draw(STATION_OBSTACLES, "#264", 3, .85)
def sc(pts, c, m, s, lab, z=5):
    if pts: xs, ys = zip(*[(p[0], p[1]) for p in pts]); ax.scatter(xs, ys, c=c, marker=m, s=s, label=lab, zorder=z, edgecolors="k", linewidths=.4)
sc(DOORS, "#39d", "s", 70, f"doors ({len(DOORS)})"); sc(SHUTTERS, "#6cf", "D", 70, f"shutters ({len(SHUTTERS)})")
sc(BREAKERS, "#fd3", "^", 130, f"breakers ({len(BREAKERS)})"); sc(OBJECTIVES, "#f80", "o", 60, f"objectives ({len(OBJECTIVES)})")
sc(LOCKERS, "#9af", "p", 50, f"lockers ({len(LOCKERS)})"); sc(CLUTTER, "#a76", "X", 60, f"clutter ({len(CLUTTER)})")
for (cx, cy, axis) in CRAWLS:  # draw each crawl as a green tunnel crossing its divider wall
    if axis == 'x':
        ax.plot([cx - CRAWL_HALF, cx + CRAWL_HALF], [cy, cy], color="#3f9", lw=6, alpha=.85, zorder=4, solid_capstyle="round")
    else:
        ax.plot([cx, cx], [cy - CRAWL_HALF, cy + CRAWL_HALF], color="#3f9", lw=6, alpha=.85, zorder=4, solid_capstyle="round")
ax.plot([], [], color="#3f9", lw=6, label=f"crawl shortcuts ({len(CRAWLS)})")
sc([(c, d) for (c, d, *_2) in SQUEEZE], "#5c8", "|", 220, f"squeeze ({len(SQUEEZE)})")
sc(STATION_PILLARS, "#284", "h", 60, "pillars"); sc(SURV_SPAWNS, "#2f6", "*", 220, f"spawns ({len(SURV_SPAWNS)})")
sc([HUNTER_SPAWN], "#f24", "X", 320, "hunter"); sc([EXIT_GATE], "#0f8", "*", 480, "EXIT")
for gx in COLS[1:-1]: ax.axvline(gx, color="#444", lw=.4, ls=":")
for gy in ROWS[1:-1]: ax.axhline(gy, color="#444", lw=.4, ls=":")
ax.set_xlim(-BX-200, BX+200); ax.set_ylim(-BY-200, BY+200); ax.set_aspect("equal")
ax.set_title("Substation -- PROPOSED layout (bold open-up)", color="w", fontsize=15)
ax.tick_params(colors="w"); ax.legend(loc="upper left", fontsize=8, ncol=2, framealpha=.85)
out = sys.argv[1] if len(sys.argv) > 1 else "substation_proposed.png"
plt.savefig(out, dpi=80, bbox_inches="tight", facecolor="#0c0e10"); print("wrote", out)

# ---------------------------------------------------------- C++ emitter -------
if "--cpp" in sys.argv:
    print("\n// ==== WALLS (center@175, scale; height 3.25) ====")
    for (cx, cy, sx, sy) in WALL_SPECS:
        print(f"\t\t{{FVector({cx:.0f}.0f, {cy:.0f}.0f, 175.0f), FVector({sx:g}f, {sy:g}f, 3.25f)}},")
    print("\n// ==== DOORS ====")
    for (dx, dy, yaw) in DOORS:
        rot = "FRotator::ZeroRotator" if yaw == 0.0 else f"FRotator(0.0f, {yaw:.0f}.0f, 0.0f)"
        print(f"\t\t{{FVector({dx:.0f}.0f, {dy:.0f}.0f, 120.0f), {rot}}},")
    print("\n// ==== BREAKERS / OBJECTIVES / LOCKERS / CLUTTER / SPAWNS counts ====")
    print(f"// breakers={len(BREAKERS)} objectives={len(OBJECTIVES)} lockers={len(LOCKERS)} "
          f"clutter={len(CLUTTER)} spawns={len(SURV_SPAWNS)} transformers={len(TRANSFORMERS)}")
