"""Verify an exported authored BlackoutHunt .umap actually persisted its actors.

Loads /Game/BlackoutHunt/Maps/<Level> in a headless editor and reports an actor-class
breakdown, so we can confirm the authoring-bake AStaticMeshActors + the discovery-tracked
gameplay actors (breakers, doors, exit gates, objective stations, flicker lights) + the
APlayerStarts actually serialized into the saved package (the historically risky step in
Docs/AUTHORED_MAP_CONVERSION.md step 3).

Run:
  & "D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "D:\\BlackoutHunt\\BlackoutHunt.uproject" `
      -ExecutePythonScript="D:\\BlackoutHunt\\Tools\\VerifyAuthoredMap.py Facility" `
      -unattended -nop4 -nosplash -stdout -nopause
"""

import sys

import unreal


def log(msg):
    unreal.log("[BH VerifyMap] " + str(msg))


# Discovery collects these by pointer (must be present); the rest are read by world iteration.
TRACKED = [
    "BHBreaker", "BHDoor", "BHExitGate", "BHObjectiveStation",
    "BHEscapeStationManager", "BHFlickerLight",
]
SUPPORT = [
    "BHLocker", "BHBatteryPickup", "BHPanicAlarm", "BHPowerSwitch",
    "BHSecurityShutter", "BHSecurityTerminal", "BHSecurityMonitor", "BHSecurityCamera",
    "BHCCTVZone", "BHLevelMarker", "PlayerStart", "StaticMeshActor",
]


def main():
    level = sys.argv[1] if len(sys.argv) > 1 else "Facility"
    asset_path = "/Game/BlackoutHunt/Maps/{}".format(level)

    if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log_error("[BH VerifyMap] MISSING: {} -- run the export commandlet first.".format(asset_path))
        return

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les.load_level(asset_path)

    actors = eas.get_all_level_actors()
    counts = {}
    hunter_starts = 0
    for a in actors:
        cls = a.get_class().get_name()
        counts[cls] = counts.get(cls, 0) + 1
        if cls == "PlayerStart":
            try:
                if a.get_editor_property("player_start_tag") == "Hunter":
                    hunter_starts += 1
            except Exception:
                pass

    log("==== {} actor breakdown ({} total) ====".format(level, len(actors)))
    for cls in sorted(counts):
        log("  {:<28} {}".format(cls, counts[cls]))

    log("---- discovery-tracked (must be > 0) ----")
    ok = True
    for cls in TRACKED:
        n = counts.get(cls, 0)
        flag = "" if n > 0 or cls in ("BHEscapeStationManager",) else "  <-- MISSING"
        if n == 0 and cls not in ("BHEscapeStationManager",):
            ok = False
        log("  {:<28} {}{}".format(cls, n, flag))
    log("  PlayerStart(survivor+hunter)  {}  (hunter-tagged: {})".format(counts.get("PlayerStart", 0), hunter_starts))
    log("  StaticMeshActor (baked geo)   {}".format(counts.get("StaticMeshActor", 0)))

    if counts.get("StaticMeshActor", 0) == 0:
        unreal.log_error("[BH VerifyMap] No StaticMeshActor baked -- authoring bake did not run or did not serialize.")
        ok = False
    if hunter_starts != 1:
        unreal.log_warning("[BH VerifyMap] Expected exactly 1 Hunter-tagged PlayerStart, found {}.".format(hunter_starts))

    log("RESULT: {}".format("PASS" if ok else "FAIL"))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("[BH VerifyMap] FAILED: " + str(exc))
finally:
    unreal.SystemLibrary.quit_editor()
