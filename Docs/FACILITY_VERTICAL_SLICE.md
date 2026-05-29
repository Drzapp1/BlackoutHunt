# Facility Vertical Slice

The current Facility is still a runtime blockout. Treat this checklist as the acceptance gate before it becomes the production reference map.

## Goals

- Every quadrant has a distinct landmark read from the central hub within 3 seconds.
- Objective silhouettes are readable from doorway distance, not only at interaction range.
- Lockers sit near loop exits, not only in dead ends.
- Both subway exits are findable from the center without opening the HUD map.
- Lighting uses color and shadow to label route identity: storage amber, lab cyan, ward violet, utility white, exits green.
- Chases have at least two break-line choices before a capture range commitment.
- Physics Classroom stations keep team clusters visible without blocking the main chase route.

## Current Runtime Pass

- Central Facility route stripes now point toward both exits and major destination clusters.
- Landmark floor pads identify storage, lab, ward, and utility/classroom lanes.
- A C++ vertical-slice pass adds hub-readable route chevrons, glyph overhead signs, route gateways, quadrant beacons, objective silhouettes, objective-type glyph plaques, and color-coded route lights.
- Exit routes now have extended green chevrons, runway dots, platform gateway frames, door leaf silhouettes, and card-reader props so both subway exits read from the hub and spawn-side lane.
- Hub route screens and small card/drawer dressing give the central hub and nearby room thresholds a more authored, less blockout-only read while staying nonblocking.
- Additional loop-side lockers now sit on the main north/south chase routes instead of only at room ends, with floor split cues marking nearby break-line choices.
- Imported props from the cooked SmartBasicInterfaces pack dress storage, lab, ward, utility, and exit lanes with cabinets, panels, generator sections, light fixtures, door frames, card readers, alarm panels, power buttons, and gas cans.
- Existing locker, battery, station, and exit placement remains unchanged for balance stability.

## Runtime Asset Paths

These imported assets are used by `ABHGameMode::AddFacilityVerticalSlicePass()` through `ABHRuntimeMeshPropActor`:

- `/Game/SmartBasicInterfaces/Meshes/SM_cabinet.SM_cabinet`
- `/Game/SmartBasicInterfaces/Materials/MI_Cabinet.MI_Cabinet`
- `/Game/SmartBasicInterfaces/Meshes/SM_panel.SM_panel`
- `/Game/SmartBasicInterfaces/Materials/MI_Panel.MI_Panel`
- `/Game/SmartBasicInterfaces/Meshes/SM_generatorFront.SM_generatorFront`
- `/Game/SmartBasicInterfaces/Materials/MI_GeneratorFront.MI_GeneratorFront`
- `/Game/SmartBasicInterfaces/Meshes/SM_generatorSection.SM_generatorSection`
- `/Game/SmartBasicInterfaces/Materials/MI_GeneratorSection.MI_GeneratorSection`
- `/Game/SmartBasicInterfaces/Meshes/SM_light.SM_light`
- `/Game/SmartBasicInterfaces/Materials/MI_EmissiveLight.MI_EmissiveLight`
- `/Game/SmartBasicInterfaces/Meshes/SM_doubledoorframe.SM_doubledoorframe`
- `/Game/SmartBasicInterfaces/Materials/MI_DoubleDoorFrame.MI_DoubleDoorFrame`
- `/Game/SmartBasicInterfaces/Meshes/SM_cardreader.SM_cardreader`
- `/Game/SmartBasicInterfaces/Materials/MI_CardReader.MI_CardReader`
- `/Game/SmartBasicInterfaces/Meshes/SM_alarm.SM_alarm`
- `/Game/SmartBasicInterfaces/Materials/MI_EmissiveRed.MI_EmissiveRed`
- `/Game/SmartBasicInterfaces/Meshes/SM_powerbtn.SM_powerbtn`
- `/Game/SmartBasicInterfaces/Materials/MI_PowerBTN.MI_PowerBTN`
- `/Game/SmartBasicInterfaces/Meshes/SM_gascan.SM_gascan`
- `/Game/SmartBasicInterfaces/Materials/MI_GasCan.MI_GasCan`
- `/Game/SmartBasicInterfaces/Meshes/SM_doubledoor01.SM_doubledoor01`
- `/Game/SmartBasicInterfaces/Meshes/SM_doubledoor02.SM_doubledoor02`
- `/Game/SmartBasicInterfaces/Materials/MI_DoubleDoor.MI_DoubleDoor`
- `/Game/SmartBasicInterfaces/Meshes/SM_cabinetdrawer.SM_cabinetdrawer`
- `/Game/SmartBasicInterfaces/Materials/MI_CabinetDrawer.MI_CabinetDrawer`
- `/Game/SmartBasicInterfaces/Meshes/SM_card.SM_card`
- `/Game/SmartBasicInterfaces/Materials/MI_Card01.MI_Card01`
- `/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel1.MI_ScreenPanel1`
- `/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel2.MI_ScreenPanel2`
- `/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel3.MI_ScreenPanel3`

If an imported mesh is missing, the spawner uses a tinted block fallback with the same collision setting. If a material is missing, the imported mesh keeps its default material or the fallback block uses the supplied route tint.

## Validation Viewpoints

Use these runtime positions as the manual review baseline after launching Facility:

- Spawn-side lane: around `(4400, 0, 160)`, looking west toward the hub and exit language.
- Central hub: around `(0, 0, 160)`, checking that `S`, `L`, `W`, `U`, and green `E` route signs are readable within 3 seconds.
- Storage/lab thresholds: around `(-2150, -1185, 160)` and `(2450, -1185, 160)`, checking amber/cyan gateways, nearby route screens, and station glyph plaques from doorway distance.
- Ward/utility thresholds: around `(-2550, 1185, 160)` and `(250, 1185, 160)`, checking violet/white gateways, objective glyph plaques, and route callouts.
- Subway platforms: around `(-4680, 0, 160)` and `(4680, 0, 160)`, checking green runway dots, platform gateway frames, door leaf silhouettes, and card-reader props.
- Chase loop checks: around `(-2225, -1800, 160)` and `(2225, 1800, 160)`, checking that loop lockers have visible split cues without blocking paths.

## Next Authored Pass

- Move stable room shapes into authored geometry or reusable room Blueprints.
- Replace runtime block chevrons/signs with embedded floor decals, authored signage, and room Blueprint lighting props once the route language is approved in playtests.
- Add room-local sound occlusion volumes before expanding objective count.
- Verify one survivor can kite from each objective to a locker without crossing another objective's interaction prompt.
- Capture screenshots from spawn, center hub, each objective cluster, each exit, and two chase loops.

## Authored Map Pipeline

The runtime generator stays the default. An authored map is opt-in and is detected at `BeginPlay` by a
single placed marker, so the procedural levels are completely unaffected until a real `.umap` ships.

> Editor-side execution runbook (build → export → verify → author → enable → rollback), with the per-map
> authoring spec, lives in `Docs/AUTHORED_MAP_CONVERSION.md`. This section is the architecture/contract.

### How the game mode chooses authored vs. generated

- `ABHGameMode::DiscoverAuthoredLevel()` runs once at the top of `BuildRuntimeFacility()` (after the
  travel options are parsed, before any geometry is generated).
- It looks for a single `ABHLevelMarker` (`Source/BlackoutHunt/BHLevelMarker.h`) placed in the loaded
  level. No marker → it returns false and the runtime block generator runs exactly as before.
- With a marker present, it collects the actor types the game mode tracks by pointer
  (`ABHBreaker`, `ABHDoor`, `ABHExitGate`, `ABHObjectiveStation`, `ABHEscapeStationManager`,
  `ABHFlickerLight`) via `TActorIterator`, then skips generation.
- Everything else the game already finds by world iteration at its use sites — lockers, battery pickups,
  panic alarms, security cameras + CCTV zones, security terminals/monitors, power switches, shutters —
  so those just need to be placed in the level; no registration call is required.

### What designers place in an authored level

- Exactly one `ABHLevelMarker`. Set `LevelName` (Facility/Substation/Foggrounds), `DefaultFogPreset`,
  and `StageIndex`. Leave `bRebuildRuntimeNavigation = true` until the map ships a baked
  `NavMeshBoundsVolume` + `RecastNavMesh`, then set it false.
- `APlayerStart` actors for survivor spawns. One `APlayerStart` with `PlayerStartTag = "Hunter"` becomes
  the teacher/hunter spawn (falls back to the marker transform if none is tagged).
- All tracked gameplay actors: breakers, doors, exit gates (one per subway platform), objective stations
  (set each station's type/`Configure` value in-editor), and any escape-station managers.
- The shutters/terminals/cameras/switches/lockers/batteries/alarms, placed for cover, light circuits, and
  loops. Configure light circuit IDs on `ABHFlickerLight` and `ABHPowerSwitch` to match.

### Known authoring nuances (follow-ups, not blockers)

- The generator's hidden teacher-mirror-trap station (`ConfigureTeacherMirrorTrapNode()`) is created but
  intentionally not added to `ObjectiveStations`. Discovery collects every `ABHObjectiveStation`, so an
  authored mirror-trap node must be a separate configured node the designer wants counted, or excluded by
  not placing one.
- CCTV zone ↔ camera wiring and circuit IDs are configured per-camera in-editor; the generator's
  `SpawnCCTVZoneForCamera()` helper is not run in authored mode.
- Station-signal exit indicators (the subway "signal" blocks) are cosmetic and currently generated only by
  the runtime subway-station builder; authored maps should use authored signage instead.

### Travel routing

- `bUseAuthoredLevels` (`Config/DefaultGame.ini`, `[/Script/BlackoutHunt.BHGameSettings]`, default
  `False`) gates routing. When true, the shared resolver `BHResolveLevelMapPackage()` (declared in
  `BHGameMode.h`, used by `ABHGameMode::ResolveTravelMapForLevel()` too) prefers
  `/Game/BlackoutHunt/Maps/<Level>` when that package exists and falls back to `/Engine/Maps/Entry`. It
  never reroutes the `TrainIntermission` level.
- All gameplay-level travel sites are now routed through this resolver: the `BHGameMode` stage-progression
  + `BuildTravelOptionsForLevel` + tester foggrounds paths, every `BHPlayerController` host/practice/test/
  classroom/bot-launch entry, `BHGameInstance::OpenListenLevel`, and the `BHGameModeBotServices` bot-soak
  travel. Sites that intentionally stay on `/Engine/Maps/Entry` (return-to-menu, host-kick client travels,
  and the online advertised `SETTING_MAPNAME`) are left unchanged.
- With `bUseAuthoredLevels=False` (default) the resolver returns the exact same `/Engine/Maps/Entry`
  string, so routing is behavior-identical for the current procedural builds. Enable the flag only after
  all target maps exist and travel is validated on Windows.

### Seeding an authored `.umap` (export commandlet — implemented)

Rather than re-place the tuned blockout by hand, export it once and author meshes/lighting on top so the
balanced coordinates are preserved.

- `UBHExportLevelCommandlet` (`Source/BlackoutHunt/BHExportLevelCommandlet.{h,cpp}`, editor-only) creates a
  blank editor world, spawns a transient `ABHGameMode`, runs `ABHGameMode::BuildLevelForExport(Level)`
  (geometry + gameplay actors only — no timers/atmosphere/round state), drops an `ABHLevelMarker`, and
  saves `/Game/BlackoutHunt/Maps/<Level>` via `UEditorLoadingAndSavingUtils::SaveMap`.
- `BuildLevelForExport` dispatches to the per-level builders, which are now uniform: `BuildFacilityLevel()`
  was extracted from the inline Facility geometry so Facility/Substation/Foggrounds are all standalone
  builders (also advances ROADMAP Chunk 7).
- Run all three: `pwsh Tools/Export-AuthoredMaps.ps1 -BuildFirst`
  (or one: `UnrealEditor-Cmd.exe BlackoutHunt.uproject -run=BHExportLevel -Level=Facility`).
- Then per map: open it, replace tinted blocks with the modular mesh kit in place (see Twinmotion/Datasmith
  in `Docs/TWINMOTION.md`), add baked lighting, a `NavMeshBoundsVolume`, and sound occlusion volumes, then
  set the marker's `bRebuildRuntimeNavigation = false`.

**Cannot be validated on Linux** (no editor toolchain here). The riskiest step is actor persistence —
actors spawned via `World->SpawnActor` into the `GEditor->NewMap()` world must serialize into the saved
package; the commandlet marks the world package dirty and uses `SaveMap`, but confirm the saved `.umap`
contains the spawned actors on the Windows editor before relying on it.

### Packaging authored maps (do when maps exist)

The cook config is intentionally **not** changed yet (today is the first live classroom test; an unverified
cook change is avoided). When the first authored `.umap` is committed:

1. Add `+DirectoriesToAlwaysCook=(Path="/Game/BlackoutHunt/Maps")` to `Config/DefaultGame.ini`
   `[/Script/UnrealEd.ProjectPackagingSettings]`.
2. Record the maps + cook policy in `Docs/ASSETS.md`.
3. Run `Tools/Verify-ClassroomPackage.ps1` and a classroom package build on Windows before release.
