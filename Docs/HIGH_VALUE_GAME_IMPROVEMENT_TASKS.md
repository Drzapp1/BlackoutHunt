# Blackout Hunt High-Value Improvement Tasks

Use this file when you want a new Codex chat to work on the most valuable next improvements without spending time rediscovering project context.

Recommended prompt format:

```text
Work on high-value task HV1 in Docs/HIGH_VALUE_GAME_IMPROVEMENT_TASKS.md. Read the task, inspect the listed files first, preserve classroom safety, implement the change, and run practical validation.
```

High-persistence prompt for `HV1`:

```text
Work on high-value task HV1 in Docs/HIGH_VALUE_GAME_IMPROVEMENT_TASKS.md.

Treat this as an end-to-end engineering task, not a first-pass script sketch. Stay with it until there is a robust, documented packaged classroom smoke-test workflow, or until you hit a concrete blocker that cannot be resolved locally.

Required workflow:

1. Start by running git status and reading AGENTS.md, README.md, Docs/HIGH_VALUE_GAME_IMPROVEMENT_TASKS.md, the full HV1 section, Docs/SETUP.md, Docs/CLASSROOM_DEPLOYMENT.md, Tools/Run-StabilityGate.ps1, Tools/Verify-ClassroomPackage.ps1, Source/BlackoutHunt/BHAutomationSupport.*, Source/BlackoutHunt/BHAutomationSupportTests.cpp, Source/BlackoutHunt/BHPlayerController.*, and any directly relevant automation startup code in BHGameMode/BHGameState.
2. Summarize the current automation/package behavior and identify the smallest implementation that genuinely validates one packaged host plus two packaged clients.
3. Implement the workflow. Prefer extending Tools/Run-StabilityGate.ps1 or adding a focused Tools script. Use C++ changes only when the existing automation flags/markers are insufficient.
4. The implementation must handle missing package/executable paths, port conflicts where practical, process startup timeouts, client join timeout, ready/round-start marker timeout, process cleanup, host/client exit codes, log collection, and clear failure summaries.
5. The test must be local/package-safe: no external accounts, no public matchmaking, no Playit dependency, no admin rights, no firewall-required LAN setup, no unsafe logs or private saved data in output.
6. Do not stop after the first working path. Run a robustness pass: review timeout values, orphan-process cleanup, log parsing, error messages, no-package behavior, and repeated-run behavior. Tighten anything brittle.
7. Run a polish pass: make the command discoverable, outputs readable, paths explicit, and docs sufficient for a future agent or tester to run it without this chat.
8. Run practical validation. If a packaged build exists, run the new smoke test. If the package is missing, run the script's missing-package path and any unit/build validations that are practical. Run Build-Editor if C++ changed. Run automation support tests if parser behavior changed. Run package verification if package verification changed.
9. If validation fails, diagnose and fix, then rerun the relevant validation. Repeat until it passes or until the remaining blocker is external and clearly documented.
10. Before final response, inspect git diff for your touched files and make sure you did not revert unrelated dirty work.

Acceptance bar:

- There is a concrete command that attempts a packaged host plus two packaged clients.
- It reports host/client process IDs or launch records, log paths, relevant automation markers, and pass/fail status.
- It cleans up child processes on success, failure, timeout, and interruption as well as PowerShell reasonably allows.
- It fails clearly for missing package, missing executable, join/ready/round-start timeout, process crash, missing log marker, or severe runtime log error if that check is implemented.
- Documentation includes the exact command, expected output shape, and what to do when the package is missing.

Do not end with only a plan. Implement, validate, harden, document, and then summarize changed files plus validation results.
```

Task IDs in this file are stable. Do not renumber existing `HV` tasks. If a task is completed or superseded, mark its status in place and append new tasks with new IDs.

## Why These Tasks

The repo already has many 0.5.0-beta.1 systems implemented or partially implemented: classroom preflight, lesson presets, warmup, audio identity cues, CCTV gameplay, train intermission, local cosmetic progression, spectator support, heatmap telemetry export, and multiple round modifiers. The highest-value work now is not simply adding more mechanics. It is making the classroom beta reliable, easy to operate, easy to understand in the first round, legally/package safe, and measurable through repeatable validation.

These tasks are intended to be higher value than generic content additions because they reduce the main risks for a live classroom playtest:

- The packaged build must run on real school machines.
- The teacher must know what to do without developer help.
- Students must understand the first round quickly.
- Network, reconnect, warmup, train, and reset flows must survive messy classroom behavior.
- Risky assets and saved/private data must not ship accidentally.
- Playtest data should produce actionable tuning notes.

## Quick Index

| Task | Title | Primary outcome | First files to inspect | Key validation |
| --- | --- | --- | --- | --- |
| HV1 | Packaged Classroom End-To-End Smoke Test | One command can test host plus clients in the packaged build. | `Tools/Run-StabilityGate.ps1`, `BHAutomationSupport.*`, `BHPlayerController.*`, `Docs/SETUP.md` | Packaged host/client automation run. |
| HV2 | Low-Spec Lab Machine Readiness Pass | Weak school PCs have a safer launch and graphics path. | `BHPlayerController.*`, `DefaultScalability.ini`, package scripts, classroom docs | Build editor; packaged low/D3D11 launch smoke test. |
| HV3 | Round Flow Recovery And Rejoin Hardening | Disconnects, late joins, warmup, train, and reset paths behave predictably. | `BHGameMode.*`, `BHGameModeTrainFlow.cpp`, `BHPlayerController.*`, `BHPlayerState.*` | Hosted round, late join, train, and reset smoke test. |
| HV4 | In-Game Teacher Runbook | The host gets a step-by-step class operation checklist in the UI. | `SBHMainMenu.cpp`, preflight, lesson preset, board, report/export code | Host/student menu visibility review; build editor. |
| HV5 | First-Round Objective Guidance Pass | New students know the next useful action during the first minutes. | `BHHUD.*`, `BHGameState.*`, `BHGameMode::BuildObjectiveBeats`, objective code | Manual role/phase HUD review; build editor. |
| HV6 | Crash And Log Gate | Validation fails on crashes, ensures, asset spam, and severe runtime errors. | `Tools/Verify-ClassroomPackage.ps1`, `Tools/Run-StabilityGate.ps1`, packaged logs | Run gate against clean and intentionally bad logs. |
| HV7 | Jumpscare And Licensed Asset Risk Cleanup | Classroom packages avoid risky prototype/source-pack content. | `Docs/ASSETS.md`, `DefaultGame.ini`, jumpscare code, verify script | Package verification; asset path review. |
| HV8 | Classroom Report Quality Pass | Reports become useful teaching summaries, not just raw exports. | `BHGameMode.cpp`, `BHGameInstance.*`, `BHLessonPreset.*`, classroom docs | Generate sample reports; privacy review. |
| HV9 | Map Playtest Evidence Report | Heatmap telemetry becomes actionable map and lesson tuning notes. | telemetry export code, `Tools`, classroom docs | Export CSV; generate Markdown or HTML report. |
| HV10 | Input And Control Conflict Audit | Advertised controls work, do not conflict, and explain blocked actions. | `DefaultInput.ini`, `BHPlayerController.*`, `BHHUD.*`, `README.md`, `Docs/SETUP.md` | Manual/control smoke test; docs review. |

## Current Project Context

- Project root: `D:\BlackoutHunt`
- Engine target: Unreal Engine 5.7
- Current beta target in docs: `0.6.0`
- Primary release path: Windows classroom package
- Core gameplay implementation: native C++ under `Source/BlackoutHunt`
- UI implementation: Slate, especially `SBHMainMenu`, `BHHUD`, and `SBHClassroomBoard`
- Important package/tooling paths:
  - `Tools/Build-Editor.ps1`
  - `Tools/Package-Windows-Classroom.ps1`
  - `Tools/Verify-ClassroomPackage.ps1`
  - `Tools/Run-StabilityGate.ps1`
  - `Tools/New-ClassroomSupportBundle.ps1`
- Important docs:
  - `README.md`
  - `Docs/CODEX_GAME_IMPROVEMENT_TASKS.md`
  - `Docs/ASSETS.md`
  - `Docs/CLASSROOM_DEPLOYMENT.md`
  - `Docs/TUNING.md`
  - `Docs/MAINTAINABILITY.md`
  - `Docs/SETUP.md`
  - `Docs/BETA_RELEASE_NOTES_0.5.0-beta.1.md`

## Global Rules For All HV Tasks

- Run `git status --short` before editing and do not revert unrelated dirty work.
- Read the files listed in the task before designing the implementation.
- Keep host/admin controls host-only. Student clients must not see tunnel, support bundle, force-start, role assignment, package path, account data, or diagnostics controls.
- Do not expose private profile/account data, credentials, backend tokens, IPs, unsafe logs, answer keys, or saved classroom data in student UI, support bundles, reports, or telemetry.
- If a task touches visuals, audio, runtime assets, cook/stage rules, or package verification, read `Docs/ASSETS.md` and search `Content/` before adding placeholders.
- Prefer guarded soft references and fallbacks for optional imported assets.
- Preserve comfort/accessibility behavior for jumpscares, flash, camera shake, captions, and high-contrast HUD.
- Keep new gameplay logic server-authoritative and replicated explicitly where clients need to know state.
- For docs-only changes, focused review and `git status --short -- <file>` is usually enough.

## Recommended Order

If no other priority is given, work in this order:

1. `HV1` Packaged Classroom End-To-End Smoke Test
2. `HV2` Low-Spec Lab Machine Readiness Pass
3. `HV3` Round Flow Recovery And Rejoin Hardening
4. `HV4` In-Game Teacher Runbook
5. `HV5` First-Round Objective Guidance Pass
6. `HV6` Crash And Log Gate
7. `HV7` Jumpscare And Licensed Asset Risk Cleanup
8. `HV8` Classroom Report Quality Pass
9. `HV9` Map Playtest Evidence Report
10. `HV10` Input And Control Conflict Audit

The first five improve the chance that a real classroom session succeeds. The later tasks reduce shipping risk, improve post-session value, and tighten polish.

## Task HV1 - Packaged Classroom End-To-End Smoke Test

Build a repeatable validation path that launches one packaged host plus two packaged clients, exercises Live Classroom, and exits cleanly. The goal is to validate the actual classroom package, not just editor PIE or isolated unit tests.

High-value outcome: a single command should catch the highest-risk classroom failure modes: packaged launch failure, host setup failure, client join failure, ready gate issues, round start failure, train/intermission travel failure, and unsafe logs after exit.

Current state:

- `Docs/SETUP.md` documents automation flags such as `-BHAutomation=1`, `-BHAutoHost=LiveClassroom|Facility|Substation|Foggrounds`, `-BHAutoJoin=<host:port-or-code>`, `-BHAutoReady=1`, `-BHAutoQuitSeconds=<seconds>`, `-BHAutomationTag=<id>`, and `-BHVirtualBoxSafe`.
- Packaged Shipping writes automation markers to `Saved\Logs\BlackoutHuntAutomation.log`.
- `Tools/Run-StabilityGate.ps1` exists and 0.5.0-beta.1 notes mention that the quick stability-gate soak was not completed.
- `BHAutomationSupport.*` parses automation command-line settings.
- `BHPlayerController` owns automation startup behavior.
- Live Classroom uses loopback plus Playit defaults for school-safe hosting, but an automated local package smoke test should not require external internet.

Implementation guidance:

- Prefer extending `Tools/Run-StabilityGate.ps1` or adding a sibling script under `Tools` rather than embedding process orchestration in gameplay code.
- Use packaged executables under `Builds\Windows` when present. If missing, fail with a clear message that points to `Tools/Package-Windows-Classroom.ps1`.
- Start one host process with an automation tag and a deterministic map/mode.
- Start two client processes that join the host through loopback/direct local address or an automation-safe join code.
- Use automation flags to ready clients and quit after a bounded time.
- Collect exit codes and log paths for host and clients.
- Parse logs for expected automation markers so the test proves more than "the process existed".
- Avoid requiring administrator access, firewall prompts, external accounts, Steam/EOS, or public internet.
- Keep any generated logs or reports under `Saved` or `Builds\Support`; do not stage them into a distributable package.
- If gameplay automation needs small improvements, keep them generic and hidden behind `-BHAutomation=1`.

Inspect first:

- `Tools/Run-StabilityGate.ps1`
- `Tools/Package-Windows-Classroom.ps1`
- `Tools/Verify-ClassroomPackage.ps1`
- `Source/BlackoutHunt/BHAutomationSupport.*`
- `Source/BlackoutHunt/BHAutomationSupportTests.cpp`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Docs/SETUP.md`
- `Docs/CLASSROOM_DEPLOYMENT.md`
- `Docs/BETA_RELEASE_NOTES_0.5.0-beta.1.md`

Acceptance criteria:

- A documented command launches one packaged host and at least two packaged clients.
- Clients join, ready up, and reach a started gameplay state or a clearly defined automation milestone.
- The run exits without orphaned game processes.
- The tool reports host/client log locations and the automation markers found.
- Failures are actionable: package missing, executable missing, join failed, ready gate failed, round did not start, process crashed, or log marker missing.
- The workflow does not require external accounts, public matchmaking, admin rights, or a Playit tunnel.

Validation:

- Run the new or updated smoke script against an existing packaged build when practical.
- Run `.\Tools\Build-Editor.ps1` if C++ automation code changes.
- Run targeted automation tests for `BHAutomationSupport` if parsing changes.
- Run `.\Tools\Verify-ClassroomPackage.ps1` if package verification behavior changes.
- Update `Docs/SETUP.md` or `Docs/CLASSROOM_DEPLOYMENT.md` with the exact command and expected outputs.

## Task HV2 - Low-Spec Lab Machine Readiness Pass

Harden startup and graphics defaults for weak classroom machines. The game should take the safest route on integrated GPUs, older lab desktops, VirtualBox-style environments, and machines that need D3D11 plus 720p/Low 4GB settings.

High-value outcome: classroom sessions fail if a meaningful fraction of student machines cannot launch or maintain usable performance. This task reduces that risk before adding more content.

Current state:

- README documents Low 4GB, High 16GB, Ultra, adaptive graphics, D3D11 classroom launchers, and 720p windowed guidance.
- The classroom package defaults to D3D11 and includes `Launch-BlackoutHunt-DX11.cmd` plus `Launch-BlackoutHunt-DX11-Low.cmd`.
- `BHPlayerController` has graphics preset, adaptive graphics, hardware scan, and VirtualBox-safe handling.
- `Config/DefaultScalability.ini` and `Config/DefaultGame.ini` contain relevant defaults.
- Beta.6 known limits still include GPU/driver issues before the menu.

Implementation guidance:

- Focus on startup resilience and clear user-facing status, not visual upgrades.
- Confirm the Low 4GB preset actually applies render scale, FPS cap, texture pool, shadows, effects, and dynamic resolution as documented.
- Review hardware detection for integrated/software/unknown GPUs and make fallback choices conservative.
- Make first-launch behavior safe when no settings file exists.
- Ensure VirtualBox-safe automation and low/windowed launch arguments do not conflict with normal classroom defaults.
- If adding log or UI status, phrase it for teachers and students, not engine developers.
- Avoid changing Ultra/High behavior unless necessary.
- Keep any command-line override documented and package-safe.

Inspect first:

- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHGameSettings.*`
- `Config/DefaultScalability.ini`
- `Config/DefaultGame.ini`
- `Config/DefaultEngine.ini`
- `Tools/Package-Windows-Classroom.ps1`
- `Docs/CLASSROOM_DEPLOYMENT.md`
- `Docs/SETUP.md`
- `README.md`

Acceptance criteria:

- New installs default to a safe graphics path for low-spec classroom machines.
- The packaged D3D11 and D3D11-Low launchers still map to expected settings.
- Integrated/unknown GPU detection chooses conservative settings without hiding settings from capable machines.
- The menu/preflight or logs make the active preset/RHI/resolution state clear enough for support.
- Changes do not require admin rights, external dependencies, or manual config editing.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes.
- Package or inspect package scripts if launchers/config staging change.
- Smoke test normal launch, D3D11 launch, D3D11-Low launch, and `-BHVirtualBoxSafe` if practical.
- Update README/classroom docs if user-visible startup or graphics behavior changes.

## Task HV3 - Round Flow Recovery And Rejoin Hardening

Make messy multiplayer classroom behavior predictable: late joins, disconnects, reconnects, Escape menu usage, role warmup ending, train travel, final escape, and round reset should leave no stale role, stat, report, cooldown, or UI state behind.

High-value outcome: live classroom sessions often involve students joining late, disconnecting, restarting, or needing the host to recover the session. Robust flow recovery is more valuable than another feature when the goal is classroom testing.

Current state:

- Late joiners during active rounds enter as survivor spectators.
- Spectators have safe support and next-round role preference tools.
- Role warmup exists and resets practice state before the live hunt.
- Train intermission and final escape travel exist.
- Classroom reports, local XP/progression, heatmap telemetry, and train resources exist.
- Host-only controls, soft kicks, ready gate, and role assignment exist.

Implementation guidance:

- Start by mapping state transitions: Lobby, Prep/warmup, Hunt, Intermission, FinalEscape, SurvivorsWin, HunterWin, travel reset.
- Write down what must reset at each transition: life state, role, desired role, spectator state, readiness, warmup props, traps, decoys, cooldowns, fear, dread, detention marks, objective progress, train resources, telemetry keys, report rows, camera reveals, and UI messages.
- Fix one concrete failure path at a time. Avoid broad rewrites of `BHGameMode`.
- Preserve late-join spectator restrictions. Late joiners should not gain hidden locations, answer keys, or host controls.
- Keep host recovery controls host-only.
- When in doubt, prefer returning a confused player to a safe spectator/lobby state over letting them affect the active round incorrectly.

Inspect first:

- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHGameModeTrainFlow.cpp`
- `Source/BlackoutHunt/BHGameModeHostControls.cpp`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHPlayerState.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHTrainIntermissionManager.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/SBHClassroomBoard.*`

Acceptance criteria:

- A late join during Hunt becomes a spectator with only safe spectator actions.
- A spectator preference can be approved by the host next lobby and does not auto-grant a role during an active round.
- Ending role warmup clears practice-only effects and starts the live hunt with clean resources/cooldowns/objective state.
- Train travel does not carry stale captured/escaped/warmup-only state into the next stage.
- Final escape and round reset return players to a predictable next state.
- Any fixed failure path has targeted validation or at least a documented manual smoke procedure.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes.
- Manually smoke test host plus at least one client if practical: late join during Hunt, spectator support, role preference, warmup end, train travel, and reset.
- Run targeted automation tests if you add or adjust flow tests.
- Review report/progression output to confirm warmup or spectator-only actions do not pollute classroom stats.

## Task HV4 - In-Game Teacher Runbook

Add a host-only "Run Class" checklist panel that guides a teacher through the live classroom workflow from preflight to post-round export.

High-value outcome: the game already has many classroom features. The teacher needs an operator workflow, not a pile of buttons. A runbook reduces setup mistakes and support burden.

Current state:

- The Classroom tab includes Host Preflight, lesson presets, manual 12-question generation, classroom board/projector, roster, ready gate, role assignment, support bundle, heatmap export, and classroom settings.
- Remote students should only see local display/audio/profile controls and normal gameplay role controls.
- `Docs/CLASSROOM_DEPLOYMENT.md` already contains a teacher-hosted classroom sequence.

Implementation guidance:

- Put this where the host already works: likely the Classroom tab or a host-only panel in `SBHMainMenu`.
- The panel should be a checklist, not another wall of explanation.
- Suggested steps:
  - Check Host Preflight.
  - Choose map and lesson preset.
  - Generate backup 12-question set if needed.
  - Start or verify tunnel/join code.
  - Confirm students joined with lobby names.
  - Assign or approve roles.
  - Confirm ready gate.
  - Start role warmup.
  - Start hunt.
  - Open classroom board/projector.
  - After round, review train recap/report and export support/telemetry only when needed.
- Steps should show status where the game can infer it. Examples: ready/not ready, preset selected, endpoint present, players joined, all ready, board available.
- Actions that open folders, support bundles, tunnel helpers, role assignment, and exports must remain host-only.
- Keep text short and teacher-facing.

Inspect first:

- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/SBHMainMenu.h`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHGameModeHostControls.cpp`
- `Source/BlackoutHunt/SBHClassroomBoard.*`
- `Source/BlackoutHunt/BHLessonPreset.*`
- `Docs/CLASSROOM_DEPLOYMENT.md`

Acceptance criteria:

- Host has a visible, ordered classroom checklist before and during Live Classroom setup.
- Checklist status reflects at least preflight, player count/ready state, selected lesson/map, and post-round export availability where practical.
- Checklist actions reuse existing host-only commands instead of duplicating logic.
- Remote students cannot access runbook-only host actions.
- Documentation points teachers to the in-game runbook.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes.
- Manually inspect host menu and remote student menu paths for visibility boundaries.
- Verify the runbook does not expose answer keys, account data, unsafe logs, tunnel internals beyond intended join endpoint, or admin/test controls to students.
- Update `Docs/CLASSROOM_DEPLOYMENT.md` if the operator flow changes.

## Task HV5 - First-Round Objective Guidance Pass

Make the first two minutes of live play easier to understand through short, role-aware next-action guidance. This should reduce confusion without turning into a tutorial page or revealing hidden information.

High-value outcome: new students should know what to do in their first round: where to go, what to interact with, why an action is blocked, and what their role is for.

Current state:

- HUD already shows objective text, role/status, blocked-action messages, objective beats, CCTV reveal markers, train/final escape messaging, and role-specific text.
- Role warmup exists, but students can still enter live Hunt with little mental model.
- `BHGameMode::BuildObjectiveBeats` already creates replicated navigation beats for objectives, exits, train, and danger.
- `BHHUD` has substantial role and phase-specific rendering.

Implementation guidance:

- Focus on early Hunt and first objective contact.
- Add or refine short next-action lines by role:
  - Survivor: nearest active task/breaker, answer prompt, hold E after answer, exit locked/unlocked state.
  - Teacher: scan/capture/search reminder, camera/monitor hint if relevant, no perfect location reveal.
  - Hall Monitor: contribution gate, trap/hint/false marker once unlocked, no capture.
  - Spectator: safe encouragement and next-round role request.
  - Train passenger: current train phase and next door/shop/bonus action.
- Do not expose hidden player locations, answer keys, host controls, or test/admin information.
- Prefer short, changing guidance over persistent long text.
- Respect high-contrast HUD.
- Avoid adding constant center-screen text during chases.

Inspect first:

- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/BHGameState.*`
- `Source/BlackoutHunt/BHGameMode.cpp`, especially `BuildObjectiveBeats`
- `Source/BlackoutHunt/BHObjectiveStation.*`
- `Source/BlackoutHunt/BHBreaker.*`
- `Source/BlackoutHunt/BHExitGate.*`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/SBHClassroomBoard.*`

Acceptance criteria:

- In Lobby, Prep/warmup, early Hunt, Train, and Final Escape, each role has a concise next-useful-action prompt.
- Common blocked actions explain why they are blocked: wrong role, wrong phase, contribution gate, cooldown, distance, line of sight, resource, or host-only restriction.
- Student clients do not see host/admin/test controls.
- Guidance remains readable with high-contrast HUD enabled.
- Guidance does not disclose hidden Teacher/Survivor information.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes.
- Manually review HUD output for Survivor, Teacher, Hall Monitor, Spectator, and Train phases.
- If classroom board labels are touched, verify board output remains projector-safe and does not reveal answer keys or hidden positions.

## Task HV6 - Crash And Log Gate

Extend validation tooling so a package or stability run fails on serious runtime log problems: crashes, ensures, fatal errors, missing required assets, missing DLLs, net driver failures, repeated soft-load spam, and automation marker failures.

High-value outcome: the team should not ship a package that technically built but generated fatal runtime warnings, missing asset spam, or failed automation markers.

Current state:

- `Tools/Verify-ClassroomPackage.ps1` audits package files and forbidden staged content.
- `Tools/Run-StabilityGate.ps1` exists for broader stability checks.
- Packaged runtime logs are under `Builds\Windows\BlackoutHunt\Saved\Logs`.
- Automation markers can be written to `BlackoutHuntAutomation.log`.
- Beta.6 build notes mention package verification passed but quick stability-gate soak was not completed.

Implementation guidance:

- Keep package file verification separate from runtime log verification, but allow the stability gate to call both.
- Parse logs with a clear allowlist/denylist strategy.
- Deny obvious severe patterns such as `Fatal error`, `ensure`, `Unhandled Exception`, `LowLevelFatalError`, `Assertion failed`, missing required DLLs, failed map travel, failed net driver, and automation failure markers.
- Be careful with benign Unreal warnings. Avoid failing on every `Warning:` line unless proven harmful.
- Summarize log findings with file path, line number if practical, and first matching line.
- Do not collect or print secrets, account data, Playit setup logs, or unsafe saved data.
- Add an escape hatch only if it is explicit and documented for developers, not classroom release defaults.

Inspect first:

- `Tools/Verify-ClassroomPackage.ps1`
- `Tools/Run-StabilityGate.ps1`
- `Tools/New-ClassroomSupportBundle.ps1`
- `Docs/CLASSROOM_DEPLOYMENT.md`
- `Docs/SETUP.md`
- `Source/BlackoutHunt/BHAutomationSupport.*`

Acceptance criteria:

- A tool can scan packaged runtime logs and fail on severe runtime errors.
- Output identifies which log file and pattern caused failure.
- Automation marker absence is reported clearly when an automation run was expected.
- Benign warnings do not create excessive false positives.
- The gate does not expose private saved data or unsafe logs in normal output.

Validation:

- Run the gate against current logs if present.
- Test parser behavior with a small copied/sample log containing one intentional severe line and one benign warning.
- Run package verification if the verification script changes.
- Update docs with the command and interpretation of failures.

## Task HV7 - Jumpscare And Licensed Asset Risk Cleanup

Reduce classroom package risk by isolating or removing risky prototype, IP-derived, unclear-license, or source-pack content from normal distribution while preserving safe fallback scares.

High-value outcome: asset and licensing mistakes can block distribution even when gameplay works. This task keeps the classroom package legally boring and operationally safe.

Current state:

- `Docs/ASSETS.md` documents package-safe paths, risky paths, never-cook paths, and current jumpscare decisions.
- SCP096 prototype content is included only for classroom beta testing and broader distribution is blocked until source-license and underlying-IP evidence is recorded.
- Hider/Hunter character roots are excluded from classroom packages.
- Free Customizable Jumpscares source/demo roots are excluded; migrated runtime subfolders are used.
- `Tools/Verify-ClassroomPackage.ps1` audits forbidden paths, source archives, risky content, saved data, and credential-like files.
- `BHJumpscareVariantLibrary`, `BHJumpscareMonster`, and `BHJumpscarePresentation` handle optional/fallback jumpscare assets.

Implementation guidance:

- Start with an audit. Do not delete user-imported content unless explicitly requested; change cook/stage/runtime references instead.
- Separate "available locally for development" from "allowed in classroom package".
- Make default classroom scare variants use package-safe in-house/generated/cleared assets or procedural fallbacks.
- Keep risky variants opt-in, development-only, or blocked from package unless license evidence is documented.
- Ensure missing optional assets degrade gracefully with no crash, assert, or startup failure.
- If changing cook paths, update both `Docs/ASSETS.md` and package verification.
- Never stage raw source archives, source files, downloaded pack roots, demo maps, or unverified external actor/object data.

Inspect first:

- `Docs/ASSETS.md`
- `Docs/THIRD_PARTY_NOTICES.txt`
- `Config/DefaultGame.ini`
- `Source/BlackoutHunt/BHJumpscareVariantLibrary.*`
- `Source/BlackoutHunt/BHJumpscareMonster.*`
- `Source/BlackoutHunt/BHJumpscarePresentation.*`
- `Source/BlackoutHunt/BHJumpscareVariantTests.cpp`
- `Source/BlackoutHunt/BHJumpscareSpawnResolverTests.cpp`
- `Tools/Verify-ClassroomPackage.ps1`
- Relevant `Content/BlackoutHunt/Art/Jumpscares` paths

Acceptance criteria:

- Classroom package defaults do not rely on assets with unresolved distribution risk.
- Optional risky assets are excluded from classroom cook/stage paths or clearly gated for local/dev use only.
- Runtime still has visible/safe fallback scares when optional assets are absent.
- Package verification catches known forbidden jumpscare/source-pack paths.
- Asset docs and notices match the implemented cook/stage behavior.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes.
- Run jumpscare-related automation tests if touched.
- Run `.\Tools\Verify-ClassroomPackage.ps1` for package/cook/stage changes.
- Review `Docs/ASSETS.md` and `Docs/THIRD_PARTY_NOTICES.txt` for consistency.

## Task HV8 - Classroom Report Quality Pass

Turn classroom reports into teacher-useful summaries: weak topics, common misconceptions, students needing follow-up, suggested next preset, and printable post-round recap.

High-value outcome: the game becomes more useful as a teaching tool. A teacher should be able to finish a session and know what to review next.

Current state:

- Classroom performance export exists.
- Train recap builders exist in `BHGameInstance`.
- Lesson presets and manual question set generation exist.
- Revision questions include topic, difficulty, question type, hints, explanations, diagrams, and adaptive recommendations.
- Heatmap telemetry export exists separately and must remain anonymous/local.

Implementation guidance:

- Keep reports local and teacher-controlled.
- Avoid exposing private account data, credentials, IPs, or hidden gameplay data.
- Decide whether the report is Markdown, CSV plus Markdown, or a simple HTML file. Prefer a format teachers can open without extra tools.
- Include actionable sections:
  - Session summary: map, preset, topic focus, difficulty mix, class/individual thresholds, duration.
  - Class mastery: class score, individual threshold count, weak topics.
  - Common misconceptions: most missed question topics/subtopics and explanations.
  - Follow-up: recommended next preset/topic/difficulty.
  - Gameplay-classroom notes: objectives completed, train recap, final escape if relevant.
- Do not include answer keys in student-facing or projector-facing output unless clearly teacher-local.
- Keep raw export compatibility if existing workflows depend on it.

Inspect first:

- `Source/BlackoutHunt/BHGameMode.cpp`, especially classroom performance export
- `Source/BlackoutHunt/BHGameInstance.*`
- `Source/BlackoutHunt/BHLessonPreset.*`
- `Source/BlackoutHunt/BHRevisionQuestionBank.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Docs/CLASSROOM_DEPLOYMENT.md`
- Existing saved report paths under `Saved\ClassReports` behavior

Acceptance criteria:

- A teacher-local report summarizes mastery, weak topics, and recommended next action.
- Report avoids private student/account/network data.
- Report includes enough context to connect results to map, preset, topic, difficulty, and round.
- Existing classroom exports are not broken.
- If answer keys or explanations are included, they are clearly teacher-local and not shown to students.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes.
- Generate a sample report from a test/classroom round if practical.
- Review output manually for privacy, usefulness, and readability.
- Update classroom docs with report location and intended use.

## Task HV9 - Map Playtest Evidence Report

Use existing anonymous heatmap telemetry to generate actionable map and lesson tuning notes. The output should help answer: where are players captured, where do objectives stall, which lockers are unused, which routes are confusing, and where do batteries run dry?

High-value outcome: Facility, Substation, and Foggrounds should improve from evidence rather than subjective guesses.

Current state:

- Host-controlled anonymous playtest telemetry export exists.
- Docs say useful rows include captures, exit route choices, escapes, objective starts/completions/stalls, unused lockers, CCTV detections, scare cues, jumpscares, Teacher scans, wrong-answer noise, battery pickups, and flashlight starvation.
- Exports are local and must not include names, IPs, online account IDs, credentials, answer keys, or local account data.
- There is not yet a clear report that turns CSV rows into design actions.

Implementation guidance:

- Prefer a `Tools` script that reads exported CSV files and writes Markdown or HTML under a local output folder.
- Keep the first version analytical and simple; do not build a full interactive map viewer unless necessary.
- Group by map, phase, event type, and rough coordinate buckets.
- Generate sections such as:
  - Top capture zones
  - Objective stall zones
  - Unused lockers near active routes
  - Common exit route choices
  - CCTV detection hotspots
  - Scare frequency by zone
  - Battery starvation or flashlight depletion zones
  - Wrong-answer clusters by topic/difficulty when available without answer keys
- Include suggested follow-up actions, such as "add route sign near X", "move locker near Y", "reduce camera coverage near Z", or "review Electricity medium questions".
- Do not collect or print player names, account data, IPs, credentials, or answer keys.

Inspect first:

- `Source/BlackoutHunt/BHGameMode.*`, telemetry marker calls
- `Source/BlackoutHunt/BHGameInstance.*`, telemetry storage/export
- `Source/BlackoutHunt/BHObjectiveStation.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`, export action
- `Docs/CLASSROOM_DEPLOYMENT.md`
- `Tools/`

Acceptance criteria:

- A local tool reads one or more telemetry CSV exports and writes a teacher/developer-friendly report.
- The report contains at least capture, objective, locker, and route/exit sections when data exists.
- The report states when there is insufficient data instead of inventing conclusions.
- Output contains no private student/account/network data.
- Docs explain how to run it after exporting heatmap telemetry.

Validation:

- Run the tool against a sample or real telemetry export if available.
- If no telemetry file exists, test the "no data" path.
- Review output for privacy and practical usefulness.
- Update docs with command, input folder, and output path.

## Task HV10 - Input And Control Conflict Audit

Audit advertised controls against actual bindings and gameplay behavior. Every visible control should work, avoid classroom-host conflicts, and give useful blocked-action feedback.

High-value outcome: control mismatches create immediate tester distrust. This is a relatively cheap polish pass with high classroom value.

Current state:

- README lists controls for ready, board, flashlight, interact, answer keys, movement, Teacher tools, Hall Monitor tools, and Escape menu.
- `Docs/SETUP.md` documents automation flags and setup behavior.
- `Config/DefaultInput.ini` owns input bindings.
- `BHPlayerController` binds many actions, including spectator support and role preferences.
- `BHHUD` renders blocked-action feedback and role-specific panels.
- Existing work has touched potential conflicts such as host force-start, test shortcuts, final station shortcuts, spectator keys, and special movement.

Implementation guidance:

- Create a control matrix from docs, config, and code.
- Verify each advertised control:
  - `WASD`, mouse, arrows
  - `Enter` ready
  - `B` classroom board
  - `F10` host-only/test-only behavior
  - `F` flashlight
  - `E` interact/hold/locker exit
  - `1-4` answers
  - `Space`, `Shift`, `Left Ctrl`, prone/special movement if bound
  - `Mouse1` Teacher capture
  - `Q`, `R`, `G` role tools
  - `H`, `T`, `Y`, `U` spectator support/preferences if documented
  - `Escape` menu
- Confirm controls behave by role and phase.
- Make blocked feedback short and specific.
- Keep host/admin/test shortcuts hidden from normal students.
- Update docs when code is correct but docs are stale; update code when docs are correct but behavior is wrong.

Inspect first:

- `Config/DefaultInput.ini`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `README.md`
- `Docs/SETUP.md`
- `Docs/CLASSROOM_DEPLOYMENT.md`

Acceptance criteria:

- README and setup docs match actual input bindings.
- Each role has correct controls and blocked feedback for unavailable actions.
- Student clients cannot trigger host/admin/test actions.
- No high-value action shares an unsafe conflict in normal classroom play.
- Any changed binding is documented and does not break automation flags.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ or input config changes when practical.
- Manually smoke test each role's controls in at least Lobby/Prep/Hunt.
- Review docs for stale or contradictory controls.
- If automation keys or command-line behavior changes, run relevant automation support tests.
