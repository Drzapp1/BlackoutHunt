# Blackout Hunt 0.6.0 Release Notes — Authored Map Pipeline

## Headline

`0.6.0` is a **major** beta. It lands the **Authored Map Pipeline**: the
foundation that lets Blackout Hunt move from runtime-generated prototype levels toward
hand-authored production maps, without disturbing the shipping procedural game. This is
the largest structural change since the classroom betas began — roughly 8,000 lines
across ~97 source/tooling files since `0.5.0-beta.1`.

The pipeline is **opt-in and behavior-identical by default**: `bUseAuthoredLevels`
(`Config/DefaultGame.ini`) defaults to `False`, no authored `.umap` is cooked yet, and
the runtime generator still builds Facility, Substation, and Foggrounds exactly as in
`0.5.0-beta.1`. This beta ships the *machinery*; authored maps themselves come in a
follow-up once a real `.umap` is authored and travel-validated on Windows.

## Scope

This beta remains a classroom build. One teacher machine hosts Live Classroom and student
machines join through the owned Playit endpoint:

```text
blackouthunt.playit.plus:24761
```

Public internet matchmaking is still out of scope. The default classroom build uses
`OnlineSubsystemNull`; the Steam profile remains a separate development-only package path.

## Major Change: Authored Map Pipeline

- **Opt-in authored levels.** `ABHGameMode::DiscoverAuthoredLevel()` runs once at the top
  of `BuildRuntimeFacility()`. A single placed `ABHLevelMarker` switches a level from
  procedural generation to authored geometry; with no marker, the runtime generator runs
  exactly as before. Tracked actors (breakers, doors, exit gates, objective stations,
  escape managers, flicker lights) are collected by `TActorIterator`; lockers, batteries,
  alarms, cameras/CCTV, terminals/monitors, switches, and shutters are found by world
  iteration at their use sites, so designers only have to place them.
- **Export commandlet.** `UBHExportLevelCommandlet` (editor-only) seeds an authored
  `/Game/BlackoutHunt/Maps/<Level>` `.umap` from the tuned runtime blockout via
  `ABHGameMode::BuildLevelForExport()`, so the balanced coordinates are preserved and
  artists replace tinted blocks with the mesh kit in place rather than re-laying the level
  by hand. Driven by `Tools/Export-AuthoredMaps.ps1` (`-BuildFirst` to compile first).
- **Uniform per-level builders.** `BuildFacilityLevel()` was extracted from the inline
  Facility geometry, so Facility, Substation, and Foggrounds are now standalone builders
  (also advances the ROADMAP map-builder refactor).
- **Shared travel resolver.** `BHResolveLevelMapPackage()` prefers
  `/Game/BlackoutHunt/Maps/<Level>` when present and falls back to `/Engine/Maps/Entry`.
  Every gameplay-level travel site (game-mode stage progression, host/practice/test/
  classroom/bot-launch entries, `OpenListenLevel`, bot-soak travel) routes through it.
  With `bUseAuthoredLevels=False` it returns the identical `/Engine/Maps/Entry` string, so
  routing is byte-for-byte unchanged for the current procedural builds.
- **Runbook + contract.** `Docs/FACILITY_VERTICAL_SLICE.md` documents the architecture
  contract and the per-map authoring spec / enable / rollback runbook.

## Other Changes Since 0.5.0-beta.1

- **Revision quality.** Anti-gaming locks, durable mastery, and mastery-bearing bonus
  question terminals so repeated/guessed answers cannot farm mastery.
- **Editable question bank (Pillar 5).** The revision question bank can be authored/edited
  from external JSON, with a fidelity test guarding parity against the compiled defaults.
  See `Docs/QUESTION_BANK_EDITING.md` and `Docs/QuestionBank.example.json`.
- **Data-driven visual physics questions.** Ray-diagram and graph-reading questions with
  structured diagram data (angles, labels, shapes) instead of prose-only items.
- **Foggrounds atmosphere.** A visible moon and eerie moonlight shafts through the fog,
  flashlight atmosphere tuning, and a subtle ambient moon glow (skylight volumetric
  `0.10 -> 0.25`).
- **Low-spec launcher.** A non-console low-spec launcher source for locked-down school
  PCs (`Tools/LowSpecLauncher`).
- **Stability hardening.** Guarded unchecked `GetWorld()` in breaker/power-switch
  interactions, hardened bot perception against null-pawn dereferences, and a fix for the
  black security-monitor feed.

## Build Provenance & Parity (read before testing)

This release was cut on the **Windows** dual-boot (UE 5.7 + MSVC), so the two platform
packages differ in currency:

- **Windows — freshly cooked at release time and fully current.** Classroom Shipping, cooked
  on the Windows UE 5.7 / MSVC toolchain on **2026-05-29** from the exact `v0.6.0`
  commit via `Tools/Package-Windows-Classroom.ps1`. This package contains **every** change in
  this release — the Authored Map Pipeline, the revision-quality and editable-question-bank
  work, the data-driven visual questions, the Foggrounds atmosphere pass, the feedback
  subsystem, and the three MSVC build fixes needed to compile the pipeline (see Build Notes).
  There is no trailing-commit gap.
- **Linux — the prior `0.5.0-beta.1` Classroom Shipping cook** (`Tools/Package-Linux-Wine.sh`,
  **2026-05-28**), attached as a compatibility package. It is **one minor version behind**: it
  does **not** contain the `0.6.0` Authored Map Pipeline or the other changes above. The
  Linux cross-toolchain lives only on the Linux boot and cannot be driven from this Windows
  session, so a Linux package at exact `0.6.0` parity must be re-cooked there:

```text
Linux parity re-cook : CLASSROOM=1 ./Tools/Package-Linux-Wine.sh   (on the Linux boot)
```

## Known Limits

- Maps are still the runtime-generated prototype levels. The authored pipeline ships but
  `bUseAuthoredLevels=False` and no authored `.umap` is cooked yet.
- The cook config is intentionally unchanged (no
  `+DirectoriesToAlwaysCook=(Path="/Game/BlackoutHunt/Maps")` yet) until the first authored
  map is committed and verified on Windows.
- The attached **Linux** package is the prior `0.5.0-beta.1` cook and trails this release by one
  minor version (see Build Provenance & Parity); the Windows package is fully current.
- Online session Host/Find/Join is local/development validation only until EOS or the
  Steam profile is configured with owned accounts.
- The Playit endpoint depends on the teacher-owned tunnel/agent staying online.
- Unreal still requires a GPU/driver exposing Direct3D feature level 11.0 / Shader Model
  5.0; Microsoft Basic Display Adapter, RDP without hardware acceleration, some VMs, and
  pre-DX11 GPUs can fail before the menu.
- Native Linux / Fedora-Wine builds remain a best-effort compatibility track; Windows
  classroom validation remains the release gate.

## Attached Packages

- `BlackoutHunt-0.6.0-beta.1-Windows-Classroom-20260529-180518.zip` — fresh `0.6.0-beta.1`, primary
- `BlackoutHunt-0.6.0-beta.1-Windows-Classroom-20260529-180518.zip.sha256`
- `BlackoutHunt-0.5.0-beta.1-Linux-Classroom-20260528-125358.zip` — prior `0.5.0-beta.1`, compatibility (see parity note)
- `BlackoutHunt-0.5.0-beta.1-Linux-Classroom-20260528-125358.zip.sha256`

> Note: the attached Windows zip keeps its original `0.6.0-beta.1` cook label (both the filename and the in-game version string), because that is the exact binary that was cooked and verified. The repository is now versioned `0.6.0`, so the next Windows cook will be labelled `0.6.0`.

## Build Notes

- Release commit: tagged `v0.6.0` on branch `feature/authored-map-pipeline`. `main` is
  intentionally left untouched (this beta is a prerelease off the feature branch).
- Windows: Classroom Shipping (`-distribution`) freshly cooked on **2026-05-29** with
  `.\Tools\Package-Windows-Classroom.ps1` on the Windows UE 5.7 / MSVC toolchain; package
  verification passed.
- Three MSVC build fixes were required to cook the Authored Map Pipeline on Windows (it had
  previously only been written/validated against the Linux cross-compiler):
  - `BHGameMode::DiscoverAuthoredLevel` — `FURL::GetOption()` returns `const TCHAR*`, so it is
    now wrapped in `FString(...)` before `.IsEmpty()`.
  - `BHExportLevelCommandlet` — dropped the redundant call to the non-exported
    `FEditorFileUtils::ResetLevelFilenames()` (`UEditorEngine::NewMap()` already resets the
    level filename), which was an unresolved external on MSVC.
  - `Config/DefaultGame.ini` — removed the `SmartBasicInterfaces/Demo` (+`/Maps`) `NeverCook`
    entries; the always-cooked runtime props (`SM_light`, `SM_alarm`, `BP_LightSwitch`, …)
    reference sounds/materials under `Demo/`, so excluding it failed the cook.
- Linux: prior `0.5.0-beta.1` Classroom Shipping cook (2026-05-28,
  `CLASSROOM=1 ./Tools/Package-Linux-Wine.sh`, UE 5.7 via Wine + Linux cross-toolchain v26
  clang-20.1.8); one minor behind — see Build Provenance & Parity.
- Windows package SHA-256: `97c39e0516ac4ff8fb08ecef7cd0bb666adf1ce5c7d35a50323d1f49cafe77b6`
- Linux package SHA-256: `c066cc32cd89f2041cf01d193e521c2e25986fdf99d15adcf8c79713779adb15`

## Feedback

Record tester reports with:

- build version: `0.6.0-beta.1`
- platform and exact package (note the Windows parity caveat above)
- map and mode
- host/client count
- machine specs and graphics preset
- exact join path used: saved Playit endpoint, direct LAN, typed direct IP, or invite code
- lobby names used by host/students
- lesson preset or manual/JSON question set used
- steps to reproduce
- packaged runtime log when safe to share
