# Map analysis / layout verification

Top-down modelling + connectivity checks for the runtime-generated levels in
`Source/BlackoutHunt/BHGameMode.cpp`. Pure analysis — these scripts never touch
the game; they mirror the builder geometry so a layout can be designed and
verified (reachability, per-room entrance counts, object density) *before*
porting numbers into the C++ builder.

Scale convention: `SpawnBlock` scale `1.0 == 100` world units (base mesh is the
100³ `/Engine/BasicShapes/Cube`); block half-extent = `scale * 50`.

## Scripts
- `substation_map.py` — models the **current** `BuildSubstationLevel()` and
  reports the problems it has (renders `substation_current.png`).
- `substation_proposed.py` — the **open-up redesign** that shipped: generates the
  wall/door layout from an explicit per-gridline "openings" spec, verifies it
  (0 single-entrance rooms, 0% unreachable), renders `substation_proposed.png`,
  and with `--cpp` prints ready-to-paste `Walls`/`Doors` specs.

## Run
```
P:\psim_venv\Scripts\python.exe substation_proposed.py out.png        # verify + render
P:\psim_venv\Scripts\python.exe substation_proposed.py out.png --cpp  # also emit C++
```
(`P:\psim_venv` has matplotlib/numpy; the system Python 3.14 does not.)

## What the redesign fixed (2026-06)
The old Substation grid had **14 of 24 rooms with ≤1 entrance** (2 with none) —
the "easy to get cornered" complaint — plus 28 objectives / 17 transformer boxes
/ 24 lockers crammed in, and survivor spawns sitting on the messy station
platform. The new layout: continuous west gallery, open central transformer
hall, every room 2–3 looped entrances, density roughly halved, and a clean
spawn muster foyer west of the platform.

**Crawl shortcuts (follow-up):** the 4 crawl ducts were free-standing mid-room
("sitting ducks" — the hunter just camps the one mouth). They now tunnel
*through* interior dividers (lintel-capped gap + duct + volume, the Foggrounds
pattern) so each connects two distinct rooms — a survivor-only escape the
Teacher can't follow (ejected + the prone hider is uncapturable). The verifier
asserts every crawl joins two distinct rooms and that the duct side walls don't
drop any room below 2 standing entrances.
