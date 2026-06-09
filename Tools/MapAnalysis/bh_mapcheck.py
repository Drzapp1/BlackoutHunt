# Copyright (c) 2026 Adam Rosta. All Rights Reserved.
# BlackoutHunt map connectivity analyzer + C++ literal code-gen.
#
# Why this exists: BHGameMode::BuildSubstationLevel() is authored as hand-typed FVector
# coordinate arrays. The original "OPEN-UP redesign" was verified by a Tools/MapAnalysis
# script that has since been DELETED, so the "0 closed rooms" claim was never re-checked --
# and in fact the "Power" compass beat points at the first unrepaired breaker, which can sit
# in a room no path reaches. This rebuilds the verification, drift-proof:
#
#   * model the collision geometry as 2D solid rectangles (SpawnBlock scale * 50 == half-extent),
#   * carve passable gaps where doors / crawl tunnels are,
#   * flood-fill from the survivor spawns,
#   * ASSERT every breaker / objective station / power switch / exit gate is reachable,
#   * render a top-down PNG (stdlib only -- no matplotlib/Pillow dependency),
#   * and CODE-GEN the exact C++ array literals so the map is authored from this verified model
#     instead of by hand (no transcription drift).
#
# Usage:
#   python bh_mapcheck.py substation-before    # diagnose the CURRENT shipped layout
#   python bh_mapcheck.py substation            # check the redesigned layout + emit C++
#
# Geometry convention (matches ABHGameMode::SpawnBlock): a block at center (cx,cy) with
# "scale" (sx,sy) occupies [cx - sx*50, cx + sx*50] x [cy - sy*50, cy + sy*50].

import sys
import math
import struct
import zlib

CELL = 40.0  # grid cell size in world units


# --------------------------------------------------------------------------------------
# geometry primitives
# --------------------------------------------------------------------------------------
class Rect:
    """Axis-aligned solid rectangle in world XY, given as a SpawnBlock center+scale."""
    __slots__ = ("cx", "cy", "hx", "hy")

    def __init__(self, cx, cy, sx, sy):
        self.cx = float(cx)
        self.cy = float(cy)
        self.hx = abs(float(sx)) * 50.0
        self.hy = abs(float(sy)) * 50.0

    def contains(self, x, y):
        return abs(x - self.cx) <= self.hx and abs(y - self.cy) <= self.hy


def oriented_corridor_cells(cx, cy, yaw_deg, length, width, grid):
    """Yield (col,row) cells covered by an oriented rectangle (a crawl tunnel footprint)."""
    rad = math.radians(yaw_deg)
    cs, sn = math.cos(rad), math.sin(rad)
    hl, hw = length * 0.5, width * 0.5
    # bounding box of the oriented rect, then test each cell in local space
    reach = math.hypot(hl, hw)
    for col in grid.cols_in(cx - reach, cx + reach):
        for row in grid.rows_in(cy - reach, cy + reach):
            x, y = grid.cell_center(col, row)
            dx, dy = x - cx, y - cy
            lx = dx * cs + dy * sn
            ly = -dx * sn + dy * cs
            if abs(lx) <= hl and abs(ly) <= hw:
                yield col, row


# --------------------------------------------------------------------------------------
# grid + flood fill
# --------------------------------------------------------------------------------------
class Grid:
    def __init__(self, minx, maxx, miny, maxy):
        self.minx, self.maxx, self.miny, self.maxy = minx, maxx, miny, maxy
        self.ncol = int(math.ceil((maxx - minx) / CELL))
        self.nrow = int(math.ceil((maxy - miny) / CELL))
        # state per cell: 0 = open(void/passable), 1 = solid, 2 = carved-passable (door/crawl)
        self.solid = bytearray(self.ncol * self.nrow)

    def idx(self, col, row):
        return row * self.ncol + col

    def cell_center(self, col, row):
        return (self.minx + (col + 0.5) * CELL, self.miny + (row + 0.5) * CELL)

    def cols_in(self, x0, x1):
        c0 = max(0, int((x0 - self.minx) / CELL))
        c1 = min(self.ncol - 1, int((x1 - self.minx) / CELL))
        return range(c0, c1 + 1)

    def rows_in(self, y0, y1):
        r0 = max(0, int((y0 - self.miny) / CELL))
        r1 = min(self.nrow - 1, int((y1 - self.miny) / CELL))
        return range(r0, r1 + 1)

    def cell_at(self, x, y):
        col = int((x - self.minx) / CELL)
        row = int((y - self.miny) / CELL)
        if 0 <= col < self.ncol and 0 <= row < self.nrow:
            return col, row
        return None

    def mark_solid(self, rect):
        for col in self.cols_in(rect.cx - rect.hx, rect.cx + rect.hx):
            for row in self.rows_in(rect.cy - rect.hy, rect.cy + rect.hy):
                self.solid[self.idx(col, row)] = 1

    def carve_box(self, cx, cy, hx, hy):
        """Force a rectangular region passable (a door/opening), overriding solids."""
        for col in self.cols_in(cx - hx, cx + hx):
            for row in self.rows_in(cy - hy, cy + hy):
                self.solid[self.idx(col, row)] = 2

    def carve_corridor(self, cx, cy, yaw_deg, length, width):
        for col, row in oriented_corridor_cells(cx, cy, yaw_deg, length, width, self):
            self.solid[self.idx(col, row)] = 2

    def is_passable(self, col, row):
        return self.solid[self.idx(col, row)] != 1

    def flood(self, seeds):
        from collections import deque
        seen = bytearray(self.ncol * self.nrow)
        dq = deque()
        for (x, y) in seeds:
            cell = self.nearest_passable(x, y, 200.0)
            if cell:
                col, row = cell
                if not seen[self.idx(col, row)]:
                    seen[self.idx(col, row)] = 1
                    dq.append((col, row))
        while dq:
            col, row = dq.popleft()
            for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nc, nr = col + dc, row + dr
                if 0 <= nc < self.ncol and 0 <= nr < self.nrow:
                    i = self.idx(nc, nr)
                    if not seen[i] and self.solid[i] != 1:
                        seen[i] = 1
                        dq.append((nc, nr))
        return seen

    def nearest_passable(self, x, y, max_r):
        """Find the nearest passable cell to a world point (objectives sit ON the floor,
        sometimes a hair inside a prop, so we search a small radius)."""
        base = self.cell_at(x, y)
        if base is None:
            return None
        bc, br = base
        max_cells = int(max_r / CELL) + 1
        best = None
        best_d = 1e18
        for dc in range(-max_cells, max_cells + 1):
            for dr in range(-max_cells, max_cells + 1):
                nc, nr = bc + dc, br + dr
                if 0 <= nc < self.ncol and 0 <= nr < self.nrow and self.solid[self.idx(nc, nr)] != 1:
                    d = dc * dc + dr * dr
                    if d < best_d:
                        best_d = d
                        best = (nc, nr)
        return best


# --------------------------------------------------------------------------------------
# stdlib PNG writer (RGB, no deps)
# --------------------------------------------------------------------------------------
def write_png(path, width, height, rgb_rows):
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))

    raw = bytearray()
    for row in rgb_rows:
        raw.append(0)  # filter type 0
        raw.extend(row)
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def render(grid, reachable, targets, passages, path, scale=3):
    """Render top-down. Solid=near-black, reachable floor=grey, unreachable floor=dark red.
    Targets colored; unreachable targets ringed bright red. Y is flipped so +Y is up."""
    W, H = grid.ncol * scale, grid.nrow * scale
    # base image
    img = [[(16, 16, 20) for _ in range(W)] for _ in range(H)]

    def putcell(col, row, rgb, sz=1):
        # world row -> image row (flip vertically)
        irow0 = (grid.nrow - 1 - row) * scale
        icol0 = col * scale
        for yy in range(-sz + 1, sz):
            for xx in range(-sz + 1, sz):
                ix, iy = icol0 + xx + scale // 2, irow0 + yy + scale // 2
                if 0 <= ix < W and 0 <= iy < H:
                    img[iy][ix] = rgb

    for row in range(grid.nrow):
        for col in range(grid.ncol):
            s = grid.solid[grid.idx(col, row)]
            if s == 1:
                color = (40, 42, 50)          # solid wall
            else:
                if reachable[grid.idx(col, row)]:
                    color = (70, 90, 80) if s == 2 else (52, 58, 66)  # carved passage vs floor
                else:
                    color = (90, 24, 24)       # UNREACHABLE void -> dark red
            irow0 = (grid.nrow - 1 - row) * scale
            icol0 = col * scale
            for yy in range(scale):
                for xx in range(scale):
                    img[irow0 + yy][icol0 + xx] = color

    # passages (doors=white, crawls=cyan)
    for (kind, x, y) in passages:
        cell = grid.cell_at(x, y)
        if cell:
            putcell(cell[0], cell[1], (235, 235, 245) if kind == "door" else (40, 200, 230), sz=2)

    # targets
    palette = {
        "breaker": (250, 215, 40), "station": (60, 200, 220), "switch": (250, 150, 40),
        "exit": (60, 230, 90), "hunter": (220, 70, 220), "spawn": (240, 240, 240),
    }
    for t in targets:
        cell = grid.cell_at(t["x"], t["y"])
        if not cell:
            continue
        col = palette.get(t["kind"], (255, 255, 255))
        putcell(cell[0], cell[1], col, sz=3)
        if not t.get("reachable", True):
            # bright red ring around unreachable target
            for ang in range(0, 360, 30):
                rx = t["x"] + 90 * math.cos(math.radians(ang))
                ry = t["y"] + 90 * math.sin(math.radians(ang))
                rc = grid.cell_at(rx, ry)
                if rc:
                    putcell(rc[0], rc[1], (255, 0, 0), sz=2)

    rows = [bytes(bytearray().join(struct.pack("BBB", *img[r][c]) for c in range(W))) for r in range(H)]
    write_png(path, W, H, rows)


# --------------------------------------------------------------------------------------
# analysis driver
# --------------------------------------------------------------------------------------
def door_audit(layout):
    """A door only opens a room if it sits in a GAP between wall segments. If a wall solid
    covers the door footprint, opening it just reveals wall -> the room stays sealed. This
    catches the 'rooms are closed' coordinate-drift bug the reachability flood can mask
    (because the flood carves doors passable unconditionally)."""
    g = Grid(layout["bounds"][0], layout["bounds"][1], layout["bounds"][2], layout["bounds"][3])
    for r in layout["solids"]:
        g.mark_solid(r)
    embedded = []
    for (cx, cy) in layout.get("doors", []):
        cell = g.cell_at(cx, cy)
        if cell and g.solid[g.idx(cell[0], cell[1])] == 1:
            embedded.append((cx, cy))
            continue
        # measure the standing gap: scan +/- across both axes until a solid is hit
        def gap(axis):
            span = 0
            for s in (-1, 1):
                d = CELL
                while d < 400:
                    x = cx + (d * s if axis == 0 else 0)
                    y = cy + (d * s if axis == 1 else 0)
                    c = g.cell_at(x, y)
                    if not c or g.solid[g.idx(c[0], c[1])] == 1:
                        break
                    d += CELL
                span += d
            return span
        narrow = min(gap(0), gap(1))
        if narrow < 110:  # capsule diameter ~84; under ~110 is a squeeze, not a clean door
            embedded.append((cx, cy, f"narrow {int(narrow)}u"))
    return embedded


def reach_count(layout, use_doors, use_crawls):
    g = Grid(layout["bounds"][0], layout["bounds"][1], layout["bounds"][2], layout["bounds"][3])
    for r in layout["solids"]:
        g.mark_solid(r)
    if use_doors:
        for (cx, cy) in layout.get("doors", []):
            g.carve_box(cx, cy, 95.0, 95.0)
    if use_crawls:
        for (cx, cy, yaw, length, width) in layout.get("crawls", []):
            g.carve_corridor(cx, cy, yaw, length, width)
    reachable = g.flood(layout["spawns"])
    ok = 0
    for t in layout["targets"]:
        cell = g.nearest_passable(t["x"], t["y"], 170.0)
        if cell and reachable[g.idx(cell[0], cell[1])]:
            ok += 1
    return ok, len(layout["targets"])


def analyze(layout, png_path):
    g = Grid(layout["bounds"][0], layout["bounds"][1], layout["bounds"][2], layout["bounds"][3])

    for r in layout["solids"]:
        g.mark_solid(r)
    passages = []
    for (cx, cy) in layout.get("doors", []):
        g.carve_box(cx, cy, 95.0, 95.0)
        passages.append(("door", cx, cy))
    for (cx, cy, yaw, length, width) in layout.get("crawls", []):
        g.carve_corridor(cx, cy, yaw, length, width)
        passages.append(("crawl", cx, cy))

    reachable = g.flood(layout["spawns"])

    targets = list(layout["targets"])
    fails = []
    for t in targets:
        cell = g.nearest_passable(t["x"], t["y"], 170.0)
        ok = bool(cell) and bool(reachable[g.idx(cell[0], cell[1])])
        t["reachable"] = ok
        if not ok:
            fails.append(t)

    render(g, reachable, targets + [{"kind": "spawn", "x": s[0], "y": s[1]} for s in layout["spawns"]],
           passages, png_path)

    # report
    print(f"== {layout['name']} ==")
    print(f"grid {g.ncol}x{g.nrow} cells @ {int(CELL)}u   solids={len(layout['solids'])}   "
          f"doors={len(layout.get('doors', []))}   crawls={len(layout.get('crawls', []))}")
    by_kind = {}
    for t in targets:
        by_kind.setdefault(t["kind"], [0, 0])
        by_kind[t["kind"]][0] += 1
        by_kind[t["kind"]][1] += 1 if t["reachable"] else 0
    for kind, (tot, ok) in sorted(by_kind.items()):
        flag = "" if ok == tot else "   <-- UNREACHABLE"
        print(f"  {kind:9s}: {ok}/{tot} reachable{flag}")
    if fails:
        print("  FAILURES (no path from spawn):")
        for t in fails:
            print(f"    - {t['kind']:9s} '{t.get('label','')}' at ({t['x']:.0f},{t['y']:.0f})")
    else:
        print("  ALL TARGETS REACHABLE.")

    # connectivity modes: what's reachable as the player actually encounters it
    od, ot = reach_count(layout, use_doors=False, use_crawls=False)
    sd, st = reach_count(layout, use_doors=True, use_crawls=False)
    print(f"  modes: doors-closed,no-crawl {od}/{ot} | standing(doors open) {sd}/{st} | full {len([t for t in targets if t['reachable']])}/{len(targets)}")

    embedded = door_audit(layout)
    if embedded:
        print(f"  DOOR AUDIT: {len(embedded)} door(s) embedded-in-wall / too-narrow:")
        for e in embedded:
            print(f"    - {e}")
    else:
        print("  DOOR AUDIT: every door sits in a clean standing gap.")
    print(f"  wrote {png_path}")
    return len(fails) == 0


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "substation-before"
    if which == "substation-before":
        import layout_substation_before as L
        analyze(L.build(), "substation_before.png")
    elif which == "substation":
        import layout_substation as L
        ok = analyze(L.build(), "substation_after.png")
        if "--emit" in sys.argv:
            L.emit_cpp()
        sys.exit(0 if ok else 1)
    else:
        print(f"unknown target {which!r}")
        sys.exit(2)
