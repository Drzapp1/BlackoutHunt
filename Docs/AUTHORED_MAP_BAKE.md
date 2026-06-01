# Authored Map Bake (code-driven, editable .umap)

This is the upgrade to the [Authored Map Conversion](AUTHORED_MAP_CONVERSION.md) pipeline that turns the
runtime block-array levels into **full, editable, visual `.umap` files** entirely from code, so the
balanced layout is preserved exactly and an artist only has to *polish* in-editor (not rebuild from
scratch).

Background: the levels (Facility / Substation / Foggrounds) are generated in C++ from `SpawnBlock(...)`
arrays into a single `ABHStaticBlockField`, whose instanced geometry is only built at `BeginPlay` — so the
old export `.umap` contained **no selectable geometry** in the editor (just spec data). The bake fixes that.

## What the bake does

`ABHGameMode::BuildLevelForExport()` (editor-only, run by `UBHExportLevelCommandlet`) now sets a
`bAuthoringExport` flag for the duration of the build. While set, the two geometry chokepoints re-route to
**real, hard-referenced `AStaticMeshActor`s** instead of the runtime block field / replicated prop actors:

- `AddStaticBlock(...)` → `SpawnAuthoredBlockActor(...)`: spawns an `AStaticMeshActor` using
  `/Engine/BasicShapes/Cube` scaled to the block, with the real `M_BH_*` PBR material resolved from the
  block's `EBHBlockMaterial` (Concrete / Plaster / Tiles / DiamondPlate / PaintedMetal / RustedMetal /
  WarningSign), **Static** mobility, collision/navigation matching the runtime field exactly. Containment
  blockers stay collision-only + `SetActorHiddenInGame(true)`. Runtime-only `FogSheet` cues are skipped
  (the authored map ships real `ExponentialHeightFog`).
- `SpawnRuntimeMeshProp(...)` → `SpawnAuthoredMeshActor(...)`: spawns an `AStaticMeshActor` with a **hard
  `UStaticMesh*` reference** (so the mesh is a hard map cook dependency and is editable in-editor), with a
  block fallback if the mesh is missing. This bakes the SmartBasicInterfaces industrial props (cabinets,
  panels, generators, light fixtures, doorframes, card readers, alarms, gas cans, …) as real actors.

`BuildLevelForExport` also calls `SpawnAuthoredPlayerStarts()` (12 survivor `APlayerStart`s + 1 tagged
`Hunter`), which `DiscoverAuthoredLevel` requires. All gameplay actors (breakers, doors, exit gates,
objective stations, flicker lights, lockers, batteries, alarms, power switches, security shutters/terminals/
monitors/cameras + CCTV zones) are spawned by the same builder, so their tuned coordinates and circuit IDs
are preserved with **zero divergence**.

Lighting: the ~34 `ABHFlickerLight`s the builder spawns call `Configure()`, which applies intensity/colour/
visibility to their point-light components immediately, so they serialize already-lit and circuit-correct
(circuit 0 = always-lit hub; 1–6 darkenable by the 6 power switches; CCTV circuit 100). No extra always-on
lights are added — that would defeat the blackout mechanic.

**The runtime generator is completely unchanged.** `bAuthoringExport` is `WITH_EDITOR`-only and is set
*only* inside `BuildLevelForExport`; in a cooked/Shipping build the field does not exist and every runtime
read compiles out. `BuildRuntimeFacility` and friends are byte-for-byte identical.

## Run it

```powershell
# Build the editor once, then export (writes Content/BlackoutHunt/Maps/Facility.umap)
.\Tools\Build-Editor.ps1
.\Tools\Export-AuthoredMaps.ps1 -Levels Facility -BuildFirst   # or all three
# raw single level:
#   <Engine>\Binaries\Win64\UnrealEditor-Cmd.exe <proj>\BlackoutHunt.uproject -run=BHExportLevel -Level=Facility -stdout -unattended -nopause -nosplash

# Verify the actors actually serialized into the package (re-opens the .umap, dumps an actor breakdown):
#   UnrealEditor-Cmd.exe <proj>\BlackoutHunt.uproject -ExecutePythonScript="<proj>\Tools\VerifyAuthoredMap.py Facility" -unattended -nop4 -nosplash -stdout -nopause
```

## Verified Facility result

`Content/BlackoutHunt/Maps/Facility.umap` (~4 MB) re-opens cleanly with the expected actors:

- **~1790 baked `StaticMeshActor`s** (real, editable shell geometry + props), grouped into Outliner folders
  `Authored/Shell`, `Authored/Decals_Signage`, `Authored/Containment`, `Authored/Props`.
- Discovery-tracked: 9 breakers · 19 doors · 2 exit gates · **30** objective stations (the hidden
  mirror-trap node is correctly excluded from the count) · 34 flicker lights.
- 13 `PlayerStart` (12 survivor + 1 `Hunter`-tagged) · 44 lockers · 12 batteries · 6 alarms · 6 power
  switches · 4 shutters · 2 terminals · 1 monitor · 3 cameras · 3 CCTV zones · `ExponentialHeightFog` +
  `PostProcessVolume` + `BHLevelMarker`.

### Correctness fixes folded in

- **Mirror-trap exclusion (critical):** `DiscoverAuthoredLevel` now skips `IsTeacherMirrorTrapNode()`
  stations when collecting `ObjectiveStations`, matching the generator (30, not 31). Without this an
  authored round counts an objective that can never complete → the exit never unlocks (unwinnable).
  Regression test: `BlackoutHunt.Level.AuthoredDiscoveryExcludesMirrorTrap`.
- **No baked nav volume:** `BuildRuntimeNavigation` is a no-op during the export bake, so the `.umap` does
  not ship a stale Movable `NavMeshBoundsVolume`/`RecastNavMesh`. The marker keeps
  `bRebuildRuntimeNavigation = true`, so the runtime rebuilds nav fresh on load (bots path exactly as they
  do on the proven runtime Facility — same coordinates, same 12000×9600×800 bounds).
- **`ABHBlockActor::BeginPlay`** re-applies visuals/state from serialized properties so map-placed dynamic
  blocks render correctly on a listen-server host (they previously showed flat, untinted base material).
- **SaveMap fix:** the export commandlet strips dynamic-material overrides (replaces each
  `UMaterialInstanceDynamic` with its parent) before `SaveMap`, because some actors create CDO-owned MIDs
  that `SaveMap` rejects as "illegal reference to private object". Runtime re-creates the MIDs at
  `BeginPlay`, so visuals are unchanged.

## In-editor polish punch-list (the "you polish" handoff)

Open `Content/BlackoutHunt/Maps/Facility.umap`. The map is a correct, lit, navigable, fully-dressed starting
point. Recommended polish, by priority:

1. **Merge / instance the shell for perf.** ~1790 separate `StaticMeshActor`s lose the runtime's instancing.
   Select the `Authored/Shell` folder → **Tools ▸ Merge Actors** (or convert to ISM / set up HLOD) before
   shipping, so the map isn't ~1790 draw-call actors. Keep a pre-merge copy for further edits.
2. **Re-colour the route cues.** Decorative `Tinted` blocks bake with a neutral material (per-instance tint
   is a runtime MID that can't serialize). Re-apply route identity (storage amber / lab cyan / ward violet /
   utility white / exits green) with decals or material instances on the `Authored/Decals_Signage` folder.
3. **Swap the cube shell for the modular industrial kit** — see *v2* below (or let the code do it).
4. **Bake lighting.** The flicker lights are dynamic (Movable) for the blackout mechanic; add a baked
   ambient/skylight pass and a Lightmass build for the static shell if you want baked GI. Keep the flicker
   lights dynamic.
5. **Bake navigation + flip the marker.** Add a hand-tuned `NavMeshBoundsVolume`, build paths, confirm a bot
   reaches every objective/exit (`?BHRunBotNavCheck=1`), then set the `BHLevelMarker`'s
   `bRebuildRuntimeNavigation = false`.
6. **Room sound-occlusion volumes** per room before expanding objective count.
7. Note: the bake freezes **one** horror-variation layout (the per-round clutter/glass RNG is captured at
   export). Hand-add/remove clutter rather than expecting run-to-run variety.
8. **Crawl-space gates are safe to delete while remeshing.** Each `ABHCrawlSpaceVolume` (the trigger that
   ejects standing pawns *and the Teacher* from a crawl run — the low geometry alone only blocks standing
   capsules) is recorded on the `BHLevelMarker` (`CrawlGates`) at export. If a merge/remesh pass removes the
   volume actors, `DiscoverAuthoredLevel` rebuilds them from the marker on load (only when the map has none),
   so a prone Teacher can never follow survivors through. Leave the marker's `CrawlGates` array intact.

## v2 — ContainersHouseCH modular industrial cladding (IMPLEMENTED)

`ABHGameMode::CladAuthoredWall` lays **ContainersHouseCH `SM_Wall_Metal_6m` panels** (material
`MI_CH_Walls_Metal`) over every wall-shaped blockout block on the **industrial levels (Facility +
Substation)**, tiled along the wall and on **both faces**, as **non-collision visual cladding**. It is
additive and fail-safe: the cube wall keeps collision + navigation, so a missing kit or a wrong panel
orientation can only fail to *add* metal — it can never break the map (you still get the clean v1 concrete
shell). Foggrounds is skipped (outdoor). Result: Facility 1793 → **2175** baked mesh actors (+382 metal
panels in the `Authored/MetalCladding` folder), Substation 570 → 922 (+352). The panels are 8.5 cm proud of
each wall face and scaled to the wall's length/height (≈600 cm tiles, height-scaled from the native 230 cm).

> Because the panels are **hard-referenced** by the saved map, ContainersHouseCH cooks automatically with
> the `.umap` once `/Game/BlackoutHunt/Maps` is cooked — no separate `+DirectoriesToAlwaysCook` entry is
> needed. **License gate:** record ContainersHouseCH license evidence in `Docs/ASSETS.md` before shipping it
> (same gate as SmartBasicInterfaces).

**Verify the look in-editor** (I can't render headlessly): open `Facility.umap`, hide/show the
`Authored/MetalCladding` folder to A/B it. If panels are offset or back-faced, the placement constants in
`CladAuthoredWall` (yaw per face / pivot end / 8.5 cm proud offset) are the knobs — tell me what's off and
I'll correct them. Catalog ground truth for tuning is in `Tools/MeshCatalog.json`:

- `SM_Wall_Metal_{1,2,3,4,6}m`: thin (8.5 cm) corrugated metal panels, length = the metre token, **height
  only 230 cm** (Facility ceiling is 335 cm → scale Z ≈ 1.46, or add a header strip). Pivot at one bottom
  end, length runs along local −Y.
- `SM_Roof_Metal_{3,4,6}m`: flat ~8.5 cm panels (usable as ceiling cladding).
- `SM_Base_*` ("floor") are **3.7 m-deep container understructures**, *not* flat floor tiles — keep the
  `M_BH_Concrete` floor instead.
- ContainersHouseCH is **not yet in any cook list** and needs a license-evidence gate recorded in
  `Docs/ASSETS.md` before it can ship.

Plan: clad the existing cube **collision** walls/ceiling with these meshes as **non-collision visual
cladding** (the proven cube keeps collision/nav, so tiling imperfections are cosmetic), tile by the catalog
dimensions, scale to height. This pass is orientation-sensitive and is best done with a visual check
(open the map / screenshot), so it is staged as a follow-up rather than shipped blind.

## Enable + ship (only after all target maps are baked and validated)

Both edits are required together; doing one without the other silently falls back to the procedural
generator with no error:

```ini
; Config/DefaultGame.ini
[/Script/BlackoutHunt.BHGameSettings]
bUseAuthoredLevels=True

[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/Game/BlackoutHunt/Maps")
+DirectoriesToAlwaysCook=(Path="/Game/ContainersHouseCH")   ; only if v2 modular shell is used
```

Then update `Docs/ASSETS.md` (+ record the ContainersHouseCH license gate if used), flip the
`BlackoutHunt.Level.TravelRoutingFallback` test expectation, run `Tools/Verify-ClassroomPackage.ps1` and a
classroom package build on Windows, and confirm each level loads the authored map (authored meshes, not the
runtime blockout) with stage-to-stage travel + escape working.

**Rollback:** set `bUseAuthoredLevels=False` — every travel site resolves back to `/Engine/Maps/Entry` and
the procedural generator runs exactly as before. No code revert needed.
