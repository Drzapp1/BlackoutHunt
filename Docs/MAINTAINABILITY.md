# Maintainability Pass

This pass records the current ownership boundaries before large source files are split. Keep behavior changes small until the beta branch is stable.

## Extraction Targets

- `BHGameMode.cpp`: round flow, runtime map generation, classroom/revision flow, scare director bridge, train/final escape, and reporting should move into focused helpers over time.
- `SBHMainMenu.cpp`: split into Play, Classroom, Settings, Guide, Character, Account, Network, and Test panels once the player-facing controls stop moving.
- Runtime map generation: keep shared block/material helpers centralized, then move Facility, Substation, Foggrounds, and Train into separate builders.
- Classroom support: keep package verification, preflight reports, and support bundle rules in tools/docs instead of duplicating them in release notes.

## Rules For Future Splits

- Move one responsibility at a time and keep public behavior identical.
- Add focused tests or automation markers before moving shared round-flow code.
- Prefer small helpers with project vocabulary over generic frameworks.
- Keep generated/package artifacts out of support bundles unless a tool explicitly whitelists them.
- Build the editor after every code split and run package verification after deployment/tooling changes.

## Current Guardrail

Run:

```powershell
.\Tools\New-CodeHealthSnapshot.ps1
```

The report highlights the largest source files and the files that should be split first.

## Completed Splits

- Task 9: moved `ABHGameMode` bot services into `Source/BlackoutHunt/BHGameModeBotServices.cpp`, including host bot count/difficulty commands, bot soak/hunt entry points, roster lifecycle, tactical memory/status reports, nav checks, objective claims, and target cooldown cleanup.
- Task 9: moved `ABHGameMode` host classroom/setup controls into `Source/BlackoutHunt/BHGameModeHostControls.cpp`, including host-admin gating, force start, roster role assignment, spectator support, soft kicks, map/fog votes, avatar selection, and round option toggles.
