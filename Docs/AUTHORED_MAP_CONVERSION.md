# Authored Map Conversion

How to convert the runtime-generated Facility / Substation / Foggrounds blockouts into authored `.umap`
levels on the Windows editor machine. The C++ pipeline (export commandlet, discovery, routing) is already
in place; this is the editor-side execution. Background: `Docs/FACILITY_VERTICAL_SLICE.md` ("Authored Map
Pipeline").

This work **cannot be done on the Linux dev box** — it needs a compiled `BlackoutHuntEditor` and the
editor GUI for the art/lighting/nav pass.

> **Update:** the export now bakes the level into a real, editable, lit `.umap` automatically (no more
> invisible block-field seed). See `Docs/AUTHORED_MAP_BAKE.md` for the code-driven bake, the verified
> Facility result, the in-editor polish punch-list, and the v2 modular-shell plan. The steps below remain
> the reference for manual editor authoring on top of the bake.

## 0. One-shot seeding

```powershell
# From repo root, on the Windows editor machine
.\Tools\Export-AuthoredMaps.ps1 -BuildFirst
```

This builds the editor, then runs `BHExportLevel` for all three levels, writing
`Content\BlackoutHunt\Maps\{Facility,Substation,Foggrounds}.umap`. The rest of this doc is the per-step
detail, the parity gaps to close in-editor, and the acceptance/enable/rollback steps.

## 1. Build + test

```powershell
.\Tools\Build-Editor.ps1
# Recommended before relying on the new code paths:
#   Automation RunTests BlackoutHunt   (includes BlackoutHunt.Level.AuthoredDiscovery / .TravelRoutingFallback)
.\Tools\Run-StabilityGate.ps1
```

## 2. Export the seeds

`UBHExportLevelCommandlet` (`-run=BHExportLevel`) makes a blank world, spawns a transient `ABHGameMode`,
runs `ABHGameMode::BuildLevelForExport(<Level>)` (geometry + gameplay actors only — no timers/atmosphere),
drops one `ABHLevelMarker`, and saves `/Game/BlackoutHunt/Maps/<Level>`.

```powershell
.\Tools\Export-AuthoredMaps.ps1                       # all three (editor already built)
# or one level:
#   <Engine>\Binaries\Win64\UnrealEditor-Cmd.exe <proj>\BlackoutHunt.uproject -run=BHExportLevel -Level=Facility -stdout -unattended -nopause -nosplash
```

Note the commandlet's log line: `Built <Level>: persistent level holds N actors before save.`

## 3. Verify the `.umap` actually persisted the actors (the risky step)

Saving commandlet-spawned actors into a package is the one part that could silently fail. Before doing any
art work:

1. Open `Content/BlackoutHunt/Maps/<Level>` in the editor.
2. World Outliner actor count should roughly match the commandlet's `N actors` log line.
3. Spot-check that these tracked types are present: `ABHBreaker`, `ABHDoor`, `ABHExitGate`,
   `ABHObjectiveStation`, `ABHEscapeStationManager`, `ABHFlickerLight`, and exactly one `ABHLevelMarker`
   (check its `LevelName`/`StageIndex` in Details).
4. If actors are missing, the save did not serialize them — do not proceed; revisit
   `BHExportLevelCommandlet.cpp` (it marks the world package dirty and uses
   `UEditorLoadingAndSavingUtils::SaveMap`). Re-export after fixing.

The map also needs `APlayerStart` actors for spawns — the generator uses bare coordinates, so the export
does **not** create PlayerStarts. Add them in step 4.

## 4. Per-map authoring to reach play-parity

Discovery (`ABHGameMode::DiscoverAuthoredLevel`) collects the pointer-tracked actors (breakers, doors,
exit gates, objective stations, escape managers, flicker lights) and reads spawns from `APlayerStart`. It
does **not** re-run the code-only wiring the generator does. Reproduce these in-editor:

### All maps

- **Survivor/teacher spawns:** add `APlayerStart` actors at the survivor spawn points; tag one
  `PlayerStartTag = "Hunter"` for the teacher (else the teacher uses the marker transform). Discovery logs
  a warning if none are placed.
- **Light circuits:** the generator assigns each `ABHFlickerLight` a circuit ID. Set the matching circuit
  on each placed light (`Configure(...)`) so `ToggleLightCircuit` / breaker blackouts affect the right
  lights.
- **Power switches:** place `ABHPowerSwitch` actors and set their circuit IDs to match the lights.
- **Security:** place `ABHSecurityCamera` + a matching `ABHCCTVZone`, `ABHSecurityTerminal`,
  `ABHSecurityShutter`, `ABHSecurityMonitor`; set each to the map's CCTV circuit
  (Facility 100, Substation 200, Foggrounds 300). The generator's `SpawnCCTVZoneForCamera` is not run.
- **Atmosphere:** the code mood/detail/horror passes are not reproduced. Author equivalent
  PostProcessVolume + `ExponentialHeightFog` + lighting (the `ABHLevelMarker.DefaultFogPreset` only seeds
  the runtime fog value; authored maps own their look).
- **RequiredBreakers:** discovery clamps `RequiredBreakers` down to the number of placed breakers if you
  place fewer than the configured 6, so the round stays winnable. Place the intended number of breakers.

### Facility (StageIndex 0, CCTV circuit 100)

- Lights on circuits 1–6 (+ a central circuit 0); 4 shutters + 2 terminals + 3 cameras/zones on circuit 100.
- Optional: the hidden teacher mirror-trap node — place one `ABHObjectiveStation` near `(-5125,-3825)` and
  call its mirror-trap configuration if you want that mechanic (the generator does this; discovery just
  collects whatever stations exist).

### Substation (StageIndex 1, CCTV circuit 200)

- Lights on circuits 1–8 (two per circuit); 8 power switches on circuits 1–8; 4 shutters + 2 terminals +
  2 cameras/zones on circuit 200; 1 monitor.

### Foggrounds (StageIndex 2, CCTV circuit 300)

- Place `ABHSlidingGate` lift gates as standalone actors (they're interaction-found, like in the generator
  — no array registration needed), plus trees/boulders for cover. 1 shutter + 1 terminal + 2 cameras/zones
  on circuit 300; scare switches as desired. Foggrounds is the final stage — the final-station escape
  structure is only built by the runtime final-station path; author it in-map if needed.

## 5. Art / lighting / nav acceptance gate

Per map, meet the goals in `Docs/FACILITY_VERTICAL_SLICE.md` (Goals + Validation Viewpoints):

1. Replace tinted blocks with the mesh kit in place (SmartBasicInterfaces / Twinmotion-Datasmith — see
   `Docs/TWINMOTION.md`); keep gameplay-actor transforms.
2. Bake lighting (route identity by color: storage amber, lab cyan, ward violet, utility white, exits green).
3. Add a `NavMeshBoundsVolume` + build paths; PIE-test that a bot can reach every objective/exit.
4. Add room sound-occlusion volumes.
5. Set the `ABHLevelMarker`'s `bRebuildRuntimeNavigation = false` (it now ships baked nav). Save.
6. Capture the screenshot baseline from the doc's validation viewpoints.

## 6. Enable + package (only after all target maps pass the gate)

```ini
; Config/DefaultGame.ini
[/Script/BlackoutHunt.BHGameSettings]
bUseAuthoredLevels=True

[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/Game/BlackoutHunt/Maps")
```

Then update `Docs/ASSETS.md`, and:

```powershell
.\Tools\Verify-ClassroomPackage.ps1
.\Tools\Package-Windows-Classroom.ps1
```

In the packaged build, confirm each level loads the authored map (authored meshes, not tinted blocks) and
that stage-to-stage travel + escape work.

## 7. Rollback

Set `bUseAuthoredLevels=False` — every travel site immediately resolves back to `/Engine/Maps/Entry` and
the procedural generator runs exactly as before (the resolver returns the identical string). Optionally
remove the cook dir line and the `.umap` files. No code revert needed.
