# Substation flow redesign + Facility crawl network — map fix

**Date:** 2026-06-09  ·  **Scope:** `ABHGameMode::BuildSubstationLevel()` and `ABHGameMode::BuildBackroomsFacility()` in `Source/BlackoutHunt/BHGameMode.cpp` (both maps are **procedural** — there are no baked `.umap`s, so this C++ *is* the map).

## The problem (as reported)
> "Substation really needs a fixed map. Rooms are closed, the 'power' checkpoint is unreachable, crawl spaces useless. Move to Facility and add more usable crawlspaces that give a real advantage."

## Diagnosis (verified, not guessed)
I rebuilt a connectivity analyzer (`Tools/MapAnalysis/bh_mapcheck.py`) — the original `substation_proposed.py` that once "verified 0 closed rooms" had been **deleted**, so the claim was never re-checked. It models the collision geometry as 2D rectangles, flood-fills from the survivor spawns, and asserts every breaker / station / power switch / exit is reachable.

**Finding:** the shipped Substation was *not* topologically broken — it scored **34/34 reachable even with every door treated as shut**. The real bugs were experiential:

1. **"Power unreachable":** the HUD "Power" compass beat (`BHGameMode.cpp` ~12070) points at the **first unrepaired breaker in array order**, and that was `(-5650,-3400)` — the far SW corner of the **west cable gallery, the hunter's own spawn room**, a full map-width from the east survivor spawn. The bearing pointed straight through walls into the monster's den, so it *read* as "no way to it."
2. **"Rooms closed":** ~17 closed doors + a grid of near-identical concrete cells with short wall segments made an over-connected map *feel* sealed.
3. **"Crawls useless":** all 4 Substation crawl gates dumped survivors **into the wide-open central hall** (the most exposed kill-zone), not away from danger.

## What changed

### Substation — bolder flow redesign (authored + verified in `Tools/MapAnalysis/layout_substation.py`, code-genned into C++ to avoid hand-transcription drift)
- **Open E-W spine:** 5 vertical "ribs" + 2 horizontal "band" walls with a continuous open corridor at **Y≈0 across the whole map**, so the Power bearing matches a real run. Every room has ≥2 wide (≥400u) arches → loops, no dead-ends. West cable gallery stays open top-to-bottom.
- **Power beat fixed:** the 8 breakers are now **pre-sorted near→far from the east spawn**, so the *first* "Power" beat is a fair, close objective `(2850,1900)` and danger ramps westward; the last breakers sit out near the hunter den (climax), never as beat #0.
- **Doors trimmed 17→10:** doors now only on the hall↔band crossings (slammable chokepoints / Teacher-interrupt counterplay); the spine + hall arches are open.
- **Crawls relocated:** 5 crawl gates now pierce the **band walls inside a room, ~700u offset from that room's centre door** — a true parallel hall↔band flank the hunter can't cover while guarding the door (a real escape, not a dump into the open hall).
- Repositioned the 3 circuit-gated shutters + 2 terminals onto the open rib hall-arches (they used to land on old doorways that are now solid wall).

**Verification:** `python bh_mapcheck.py substation` → **34/34 reachable** in all three modes (doors-closed/no-crawl, standing/doors-open, full), door audit clean. See `Tools/MapAnalysis/substation_before.png` vs `substation_after.png`.

### Facility (Backrooms) — denser, more advantageous crawl network
- **Crawl tunnels 16 → 26.** Crawl tunnels are *additive* prone passages (they only ever add a route, never seal the open hall), so densifying is **zero-risk for connectivity**.
- The **10 new tunnels are longer** (lenScale ~6.0–6.5) so the standing hunter's detour around them is bigger — a real shortcut/escape advantage, not just a hidey-hole. A prone survivor inside is uncapturable (crawl-space immunity).
- **Crawl-volume edge margin fix:** Facility's shelter volume Y half-extent was `95` (< the `±100` side walls), leaving a 5u sliver outside the immunity volume; bumped to `105` to match Substation's no-edge-margin standard.

## How to build & verify
```
# build (serial — the box hits PCH OOM at higher parallelism)
D:\UE_5.7\Engine\Build\BatchFiles\Build.bat BlackoutHuntEditor Win64 Development -Project="D:\BlackoutHunt\BlackoutHunt.uproject" -WaitMutex -MaxParallelActions=1
# re-run the connectivity proof at any time
cd D:\BlackoutHunt\Tools\MapAnalysis && python bh_mapcheck.py substation
```
In-game: spawn Substation, confirm the "Power" marker points at a *nearby* breaker and you can walk a roughly-straight spine toward it; confirm the band crawl-flanks let you slip a chase; in Facility confirm the denser long tunnels.

## Residual notes / possible follow-ups (need playtest)
- **Facility "barrier" crawls:** I deliberately did *not* add wall-barriers-with-crawl-only-gaps in Facility (the Substation pattern). They give the strongest "real advantage" but carry a small risk of sealing the procedural hall, which I can't validate without a playtest. Easy follow-up once someone can play it.
- **Foggrounds** has the same `95`→`105` crawl-volume edge sliver (`BHGameMode.cpp` ~6535); left untouched (out of scope), trivial to fix.
- A handful of Substation set-dressing props (some lockers) still use legacy coordinates; none block connectivity (verified), but a cosmetic pass could re-seat any that clip a new wall.

## Files
- `Source/BlackoutHunt/BHGameMode.cpp` — `BuildSubstationLevel()` + `BuildBackroomsFacility()`.
- `Tools/MapAnalysis/bh_mapcheck.py` — analyzer (flood-fill + PNG, stdlib only).
- `Tools/MapAnalysis/layout_substation.py` — verified Substation model + C++ code-gen (source of truth).
- `Tools/MapAnalysis/layout_substation_before.py` — shipped layout, for the before/after comparison.
