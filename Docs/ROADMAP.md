# Blackout Hunt Roadmap

This roadmap turns the current beta polish review into executable chunks. The goal is to improve classroom reliability, first-run clarity, and maintainability before adding more gameplay surface area.

## Current Priorities

1. Fix low-risk usability defects that can confuse teachers or testers.
2. Make classroom hosting easier to verify before students join.
3. Reduce player-facing ambiguity in HUD/menu labels.
4. Track larger authored-map and architecture work without mixing it into risky last-minute beta fixes.

## Chunk 1 - Input And Test Safety

- Remove the `F10` collision between host force-start and test-only final-station shortcuts.
- Keep tester shortcuts available through explicit test keys and menu buttons.
- Update visible control text so it matches the actual bindings.
- Add or update automation coverage for any behavior that can regress.

## Chunk 2 - Documentation And Package Hygiene

- Replace stale `D:\MainGame` examples with project-relative paths or the current `D:\BlackoutHunt` root.
- Keep release notes accurate without editing unrelated in-progress release-script changes.
- Keep package verification focused on runtime DLLs, Playit, forbidden saved data, and support-bundle hygiene.

## Chunk 3 - Classroom Preflight And Support Bundle

- Add a host-facing classroom preflight summary: version, endpoint, loopback mode, tunnel/hotspot permissions, online subsystem, graphics state, and log paths.
- Add a support bundle/export command that writes a teacher-shareable diagnostic summary and selected safe logs.
- Surface the preflight/support actions in the menu where classroom hosts already work.

## Chunk 4 - HUD And Menu Clarity

- Replace opaque HUD abbreviations where space allows.
- Keep role-specific information prominent and reduce test-only noise in normal classroom play.
- Consolidate advanced/dev/test menu actions behind clear advanced sections.
- Keep the start screen visual direction, but make downstream choices more task-oriented.

## Chunk 5 - Accessibility And Comfort

- Add classroom-safe toggles for reduced jumpscares, reduced flash, reduced camera shake, subtitles/captions, and high-contrast HUD.
- Ensure scare intensity affects both event frequency and sensory intensity, not just content selection.
- Make settings persistent and visible before hosting a classroom.
- Status: implemented as local persistent Settings options plus scare-intensity sensory scaling.

## Chunk 6 - Facility Vertical Slice

- Do one production-style pass on Facility before broadening map count.
- Prioritize landmarks, route readability, objective silhouettes, lighting composition, locker/escape loops, and sound occlusion.
- Use the current runtime map as a blockout only; migrate stable layouts into authored or data-driven assets when practical.
- Status: runtime route-readability scaffold added; authored production criteria are tracked in `Docs/FACILITY_VERTICAL_SLICE.md`.
- Status: authored-map foundation landed — `ABHLevelMarker` + `ABHGameMode::DiscoverAuthoredLevel()` let the game mode run on a hand-authored `.umap` (placed actors discovered instead of generated), gated so the procedural levels are unchanged. `bUseAuthoredLevels` (default off) + `ResolveTravelMapForLevel()` route GameMode travel to `/Game/BlackoutHunt/Maps/<Level>` when present.
- Status: export pipeline landed — `BuildFacilityLevel()` extracted so all three maps are uniform standalone builders; public `ABHGameMode::BuildLevelForExport()` + `UBHExportLevelCommandlet` + `Tools/Export-AuthoredMaps.ps1` seed `/Game/BlackoutHunt/Maps/<Level>.umap` from the tuned blockout. Travel routing completed across every gameplay-level launch site via the shared `BHResolveLevelMapPackage()` helper (behavior-identical while the flag is off). **Not built/validated on Linux** — needs a Windows editor build + commandlet run; the cook-dir change is deferred until the first authored map exists. Remaining: run the export on Windows, then the art/lighting/nav pass per map. See the "Authored Map Pipeline" section in `Docs/FACILITY_VERTICAL_SLICE.md`.

## Chunk 7 - Maintainability

- Split `BHGameMode.cpp` responsibilities into smaller units: map generation, round flow, revision/classroom logic, scare director, bot services, train/final escape flow, and reporting.
- Split `SBHMainMenu.cpp` into panels or helper widgets once player-facing behavior is stable.
- Replace repeated hard-coded strings and colors with small local helper APIs where it reduces real duplication.
- Status: split plan and code-health snapshot tool added; broad file moves are intentionally deferred until the beta branch is less noisy.

## Validation Gate

For each code chunk:

- Build editor target when practical.
- Run targeted automation tests for touched behavior.
- Run `Tools/Verify-ClassroomPackage.ps1` after package-impacting changes.
- Update this roadmap only when a chunk is complete or intentionally deferred.
