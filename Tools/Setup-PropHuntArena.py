# Copyright (c) 2026 Adam Rosta. All Rights Reserved.
# This source code is proprietary and confidential.
# Unauthorized copying or distribution is strictly prohibited.
#
# Setup-PropHuntArena.py -- bake an authored prop-hunt arena map from an imported pack demo map.
#
# WHAT IT DOES (per arena row in ARENAS below):
#   1. Loads the source pack demo map (e.g. /Game/ContainersHouseCH/Maps/Map_ContainersHouse_Demo).
#   2. Computes the colliding-geometry bounds (sky spheres / oversized backdrops excluded).
#   3. Floor-traces a deterministic Halton scatter and places SPAWN_TARGET APlayerStarts on standable,
#      headroom-checked floor points (the same algorithm the runtime arena loader uses, so the authored
#      spawns and the runtime fallback agree). Existing PlayerStarts are kept and count toward the target.
#   4. Tags the PlayerStart farthest from the spawn centroid "Hunter" (the seeker's hide-phase holding spot)
#      unless one is already tagged.
#   5. Saves the result AS A NEW MAP at the arena's authored package (/Game/BlackoutHunt/Maps/Arena_<Name>),
#      leaving the imported pack untouched. The runtime prefers the authored bake automatically
#      (BHResolvePropHuntArenaPackage), so no code change is needed after running this.
#
# The runtime works WITHOUT this script (it scatters spawns itself on the raw demo map); run it when you want
# hand-tunable spawns -- after it runs you can nudge/add/delete the PlayerStarts in the editor and re-save.
#
# HOW TO RUN (pick one):
#   A) Headless (editor closed), from PowerShell:
#        $u = & "D:\BlackoutHunt\Tools\Find-Unreal.ps1"
#        $cmd = Join-Path (Split-Path $u.Editor) "UnrealEditor-Cmd.exe"
#        & $cmd "D:\BlackoutHunt\BlackoutHunt.uproject" -run=pythonscript `
#            -script="D:/BlackoutHunt/Tools/Setup-PropHuntArena.py" -stdout -unattended -nosplash
#   B) In the editor: Tools > Execute Python Script... > pick this file (or paste into the Output Log's
#      Python console). Requires the "Python Editor Script Plugin" (on by default in UE 5.x).
#
# AFTER RUNNING:
#   - Add the new Arena_*.umap to the cook -map= lists in Tools/Package-*.ps1/.sh (a commented entry is
#     already there next to the ContainersHouse demo map) and to Tools/Verify-EOSPackage.ps1 if you want the
#     cooked-map guard to require it.
#   - Open the map once and eyeball the spawns (Docs/PROP_HUNT_ROADMAP.md section 7.9 per-map checklist).

import unreal

# ----------------------------------------------------------------------------------------------------------
# Config -- keep in sync with BHPropHunt::ArenaSpecs in Source/BlackoutHunt/BHPropHuntLibrary.h.
# ----------------------------------------------------------------------------------------------------------
ARENAS = [
    {
        "logical": "ContainersHouse",
        "source": "/Game/ContainersHouseCH/Maps/Map_ContainersHouse_Demo",
        "dest": "/Game/BlackoutHunt/Maps/Arena_ContainersHouse",
    },
    {
        "logical": "RuinedCrypt",
        "source": "/Game/RuinedCrypt/Demo/Maps/RuinedCrypt_01/RuinedCrypt_01_P",
        "dest": "/Game/BlackoutHunt/Maps/Arena_RuinedCrypt",
    },
]

SPAWN_TARGET = 12          # total PlayerStarts to aim for (matches bh.PropHuntArenaSpawns)
EDGE_MARGIN = 250.0        # cm clear of the bounds edge
MIN_SPACING = 500.0        # cm between spawns
CAPSULE_RADIUS = 42.0
CAPSULE_HALF_HEIGHT = 98.0
MAX_COMPONENT_EXTENT = 50000.0   # skip sky spheres / backdrops when computing bounds
MAX_FLOOR_HOPS = 6
MAX_CANDIDATES = 256
STANDABLE_NORMAL_Z = 0.72


def halton(index, base):
    result, fraction, remaining = 0.0, 1.0 / base, max(0, index)
    while remaining > 0:
        result += fraction * (remaining % base)
        remaining //= base
        fraction /= base
    return result


def log(msg):
    unreal.log("[Setup-PropHuntArena] {}".format(msg))


def compute_bounds(world):
    """Colliding static-geometry bounds, mirroring ABHGameMode::ComputePropHuntArenaBounds."""
    bounds_min = None
    bounds_max = None
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        origin, extent = actor.get_actor_bounds(only_colliding_components=True)
        m = max(extent.x, extent.y, extent.z)
        if m > MAX_COMPONENT_EXTENT or m <= 2.0:
            continue
        lo = unreal.Vector(origin.x - extent.x, origin.y - extent.y, origin.z - extent.z)
        hi = unreal.Vector(origin.x + extent.x, origin.y + extent.y, origin.z + extent.z)
        if bounds_min is None:
            bounds_min, bounds_max = lo, hi
        else:
            bounds_min = unreal.Vector(min(bounds_min.x, lo.x), min(bounds_min.y, lo.y), min(bounds_min.z, lo.z))
            bounds_max = unreal.Vector(max(bounds_max.x, hi.x), max(bounds_max.y, hi.y), max(bounds_max.z, hi.z))
    return bounds_min, bounds_max


def trace_standable_points(world, x, y, top_z, bottom_z):
    """All standable (normal.z >= 0.72) surfaces under (x, y), top-down through the geometry."""
    points = []
    start_z = top_z
    for _ in range(MAX_FLOOR_HOPS):
        if start_z <= bottom_z:
            break
        hit = unreal.SystemLibrary.line_trace_single(
            world,
            unreal.Vector(x, y, start_z),
            unreal.Vector(x, y, bottom_z),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,  # Visibility
            False, [], unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            break
        broken = unreal.GameplayStatics.break_hit_result(hit)
        impact = broken[5]   # impact_point (see GameplayStatics.break_hit_result docs)
        normal = broken[6]   # impact_normal
        if normal.z >= STANDABLE_NORMAL_Z:
            points.append(unreal.Vector(impact.x, impact.y, impact.z))
        start_z = impact.z - 30.0
    return points


def has_clearance(world, point):
    """Pawn-capsule headroom check at a candidate spawn centre (box probe -- python has no capsule overlap)."""
    half = unreal.Vector(CAPSULE_RADIUS, CAPSULE_RADIUS, CAPSULE_HALF_HEIGHT)
    hits = unreal.SystemLibrary.box_overlap_actors(
        world, point, half, [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1], None, [])  # WorldStatic
    return len(hits) == 0


def setup_arena(arena):
    log("=== {} : {} -> {} ===".format(arena["logical"], arena["source"], arena["dest"]))
    if not unreal.EditorAssetLibrary.does_asset_exist(arena["source"]):
        log("SKIP: source map does not exist: {}".format(arena["source"]))
        return False

    world = unreal.EditorLoadingAndSavingUtils.load_map(arena["source"])
    if world is None:
        log("SKIP: could not load {}".format(arena["source"]))
        return False

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    bounds_min, bounds_max = compute_bounds(world)
    if bounds_min is None:
        log("SKIP: no colliding static geometry found (check the map's collision).")
        return False
    log("Bounds min={} max={}".format(bounds_min, bounds_max))

    existing_starts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart)
    spawn_points = [s.get_actor_location() for s in existing_starts]
    has_hunter_tag = any(str(s.get_editor_property("player_start_tag")) == "Hunter" for s in existing_starts)
    log("Existing PlayerStarts: {} (hunter-tagged: {})".format(len(existing_starts), has_hunter_tag))

    span_x = max(100.0, (bounds_max.x - bounds_min.x) - 2.0 * EDGE_MARGIN)
    span_y = max(100.0, (bounds_max.y - bounds_min.y) - 2.0 * EDGE_MARGIN)
    top_z = bounds_max.z + 500.0
    bottom_z = bounds_min.z - 500.0

    new_starts = []
    candidate = 0
    while len(spawn_points) < SPAWN_TARGET and candidate < MAX_CANDIDATES:
        u, v = halton(1 + candidate, 2), halton(1 + candidate, 3)
        candidate += 1
        x = bounds_min.x + EDGE_MARGIN + u * span_x
        y = bounds_min.y + EDGE_MARGIN + v * span_y
        if any((p.x - x) ** 2 + (p.y - y) ** 2 < MIN_SPACING ** 2 for p in spawn_points):
            continue
        standable = trace_standable_points(world, x, y, top_z, bottom_z)
        placed = False
        for floor_point in reversed(standable):  # lowest first: interior floor beats the roof
            centre = unreal.Vector(floor_point.x, floor_point.y, floor_point.z + CAPSULE_HALF_HEIGHT + 12.0)
            if not has_clearance(world, centre):
                continue
            start = actor_subsystem.spawn_actor_from_class(unreal.PlayerStart, centre, unreal.Rotator(0.0, 0.0, 0.0))
            if start:
                start.set_folder_path("PropHunt/Spawns")
                spawn_points.append(centre)
                new_starts.append(start)
                placed = True
            break
        if not placed:
            continue
    log("Placed {} new PlayerStarts ({} total).".format(len(new_starts), len(spawn_points)))
    if len(spawn_points) == 0:
        log("SKIP: no standable spawn points found; not saving.")
        return False

    # Tag the start farthest from the centroid as the seeker hold, unless the map already has one.
    if not has_hunter_tag:
        all_starts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart)
        if all_starts:
            cx = sum(s.get_actor_location().x for s in all_starts) / len(all_starts)
            cy = sum(s.get_actor_location().y for s in all_starts) / len(all_starts)
            farthest = max(all_starts, key=lambda s: (s.get_actor_location().x - cx) ** 2 + (s.get_actor_location().y - cy) ** 2)
            farthest.set_editor_property("player_start_tag", unreal.Name("Hunter"))
            log("Tagged the farthest PlayerStart 'Hunter' at {}.".format(farthest.get_actor_location()))

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, arena["dest"])
    log("Saved {} -> {}".format(arena["dest"], "OK" if ok else "FAILED"))
    return ok


def main():
    results = {}
    for arena in ARENAS:
        try:
            results[arena["logical"]] = setup_arena(arena)
        except Exception as err:  # keep going; report at the end
            unreal.log_error("[Setup-PropHuntArena] {} failed: {}".format(arena["logical"], err))
            results[arena["logical"]] = False
    log("Summary: " + ", ".join("{}={}".format(k, "OK" if v else "FAILED") for k, v in results.items()))


if __name__ == "__main__":
    main()
