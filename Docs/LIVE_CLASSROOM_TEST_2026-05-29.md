# Live Classroom Test Handoff - 2026-05-29

Use this file as the first handoff document for the first in-classroom live test planned for Friday, May 29, 2026.

The purpose of this document is to avoid re-explaining context in a new Codex chat. A prompt like this should be enough:

```text
Work on task LT2 in Docs/LIVE_CLASSROOM_TEST_2026-05-29.md. Read the task, follow the repository AGENTS.md rules, inspect the referenced code/docs first, implement only the requested scope, and run the listed validation where practical.
```

## Current Snapshot

- Project: Blackout Hunt, Unreal Engine 5.7 direct-IP multiplayer classroom horror hunt prototype.
- Current beta target: `0.2.0-beta.6`.
- Primary live-test package: Windows classroom build.
- Planned test date: Friday, May 29, 2026.
- Teacher-hosted classroom path: `LIVE CLASSROOM` map button, loopback listen host, Playit endpoint for student joins.
- Preferred join endpoint documented for 0.2.0-beta.6:

```text
blackouthunt.playit.plus:24761
```

- Original verified packaged tree:

```text
Builds\Windows
```

- Current classroom archive:

```text
Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip
```

- SHA-256:

```text
9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7
```

- `.\Tools\Verify-ClassroomPackage.ps1` passed against `Builds\Windows` on May 28, 2026.
- LT9 recheck on May 28, 2026 confirmed the 0.2.0-beta.6 archive hash and selected the archive, not the current staged `Builds\Windows` folder, as the distribution source.
- Do not copy or distribute the current local `Builds\Windows` folder unless a new classroom package is deliberately rebuilt and verified; its shipping executable no longer matches the 0.2.0-beta.6 archive.
- Release notes say Unreal automation for `Automation RunTests BlackoutHunt` passed: 32 succeeded, 0 failed.
- Automation warnings seen in the 0.2.0-beta.6 report were from optional/fallback asset paths and a deliberate host-admin rejection warning, not failing tests.
- Release notes also say the quick stability-gate soak was not completed, and broad physical-device validation remains a follow-up.

## Critical School Constraint

The school Windows machines may block interactive console access. Do not rely on command prompt, PowerShell, Unreal console commands, or `.cmd` launchers on student machines.

Batch files are not fully ruled out yet. `.bat` may be possible on these machines, but treat it as unverified until tested on the actual school image. Technically, `.bat` files usually run through `cmd.exe`, so a policy that blocks `cmd.exe` can still block `.bat` even if file association behavior looks different in Explorer.

This means the existing packaged helper files are not good classroom fallback instructions:

```text
Launch-BlackoutHunt-DX11.cmd
Launch-BlackoutHunt-DX11-Low.cmd
```

They may still exist in the package, but do not treat them as the classroom fallback unless they are tested on the actual school machines. If `cmd.exe` or console execution is blocked, these `.cmd` launchers are likely unusable.

For the actual classroom machines, prefer:

- double-clicking `BlackoutHunt.exe`
- in-game Settings controls for `Low 4GB`, FPS cap, render scale, and 720p/windowed mode
- a tested `.bat` fallback only if the school image allows it
- a future non-console shortcut or launcher path if task LT2 is implemented

Do not tell students to run terminal commands, `.cmd`, PowerShell, Unreal console commands, or developer scripts during the live class. Do not use `.bat` during class unless it has already been tested successfully on the school machines.

## Recommended Live-Test Posture

For tomorrow's test, reliability matters more than adding new gameplay.

Use the existing 0.2.0-beta.6 classroom package unless there is a deliberate reason to rebuild. The worktree had source, docs, tools, config, plugins, and imported content changes after the packaged 0.2.0-beta.6 archive was created. If a new build is made, rerun classroom packaging and verification instead of assuming the old validation still applies.

Recommended first live round:

- Map: Facility.
- Mode: `LIVE CLASSROOM`.
- Lesson preset: `Electricity easy` or `Low scare`.
- Scare intensity: low for round one.
- Teacher count: one.
- Bots: as low as practical for the first live run.
- Host board: open with `B` and move to projector/display.
- Backup: generate the local 12-question manual set before students join.

## Stable Task Index

Do not renumber these task IDs. Append new tasks at the end if needed.

| Task | Title | Best owner | Why it matters |
| --- | --- | --- | --- |
| LT1 | Freeze And Distribute The Known Good Beta.6 Package | Operator | Avoids invalidating package verification right before class. |
| LT2 | Confirm Or Replace Console-Dependent Low-Spec Fallback | Codex/dev | School machines may block command prompt launchers; `.bat` needs proof before relying on it. |
| LT3 | Rehearse Teacher Host Plus Two Student Joins | Operator | Real network/hardware is the biggest remaining risk. |
| LT4 | Validate Teacher Host Preflight And Support Bundle | Operator or Codex/dev | Confirms endpoint, logs, package paths, and diagnostics before students wait. |
| LT5 | Run A Short Classroom Soak | Operator | Covers the 0.2.0-beta.6 validation gap called out in release notes. |
| LT6 | Prepare Low-Spec Student Machine Flow With Tested Launchers | Operator or Codex/dev | Reduces failure on older lab PCs without relying on unverified `.cmd` or `.bat` files. |
| LT7 | Lock A Simple First-Round Lesson Plan | Operator | Keeps the first test focused on whether the flow works. |
| LT8 | Collect Safe Feedback And Logs After The Test | Operator | Makes post-test fixes concrete without exposing student data. |
| LT9 | Only Rebuild If A Specific Blocker Is Fixed | Codex/dev | Prevents risky last-minute churn. |

## Task LT1 - Freeze And Distribute The Known Good Beta.6 Package

High-value outcome: students and the teacher use the same package that already passed package verification.

Current state:

- `Builds\Windows` passed `.\Tools\Verify-ClassroomPackage.ps1`.
- Archive exists at `Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip`.
- The zip hash is documented above.
- The worktree contains many unrelated dirty files and imported content. Treat them as user work unless the user explicitly asks to alter them.

Implementation guidance:

- Do not rebuild just because source files are dirty.
- Do not replace the classroom zip without rerunning packaging and verification.
- If copying to USB/shared storage, copy the `.zip` and `.zip.sha256` together.
- Do not distribute `Saved\Account`, `Saved\Logs`, `Saved\Crashes`, backend data, or local credential files.

Acceptance criteria:

- The distributed archive name and hash match this document.
- Teacher and student machines all extract from the same archive.
- No one runs from a developer checkout or stale `Builds\Windows` folder copied before the verified archive.

Validation:

```powershell
Get-FileHash Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip -Algorithm SHA256
.\Tools\Verify-ClassroomPackage.ps1
```

These commands are for the development machine, not student machines.

## Task LT2 - Confirm Or Replace Console-Dependent Low-Spec Fallback

High-value outcome: low-spec or older school PCs have a proven double-click way to launch the game with safe graphics settings.

Current state:

- The package includes `Launch-BlackoutHunt-DX11.cmd` and `Launch-BlackoutHunt-DX11-Low.cmd`.
- School Windows machines may block console execution, so those `.cmd` launchers are risky until tested on the school image.
- `.bat` may be possible, but it must be proven on the actual locked-down machines before it becomes the classroom fallback.
- The root launcher `BlackoutHunt.exe` is still the normal double-click entry point.
- In-game performance modes exist: `Low 4GB`, `High 16GB`, and `Ultra`.
- README says packaged classroom builds default to D3D11 and include `.cmd` fallbacks, but that advice is not sufficient for this school environment until launcher policy is tested.

Implementation guidance:

- Avoid terminal, PowerShell, and Unreal console-command workflows for classroom students.
- Test whether `.bat` files launch on the school image before implementing or documenting a `.bat` fallback.
- Prefer one of these safer approaches:
  - if `.bat` is allowed, add a simple packaged `.bat` low-spec launcher and validate it on a school machine
  - add a packaged Windows shortcut `.lnk` that launches the game with low-spec arguments without opening a console
  - add a non-console helper executable if the project already has a safe pattern for that
  - make the default first-run classroom graphics profile conservative enough that students can reach Settings from `BlackoutHunt.exe`
  - add a clear in-game Settings affordance for `Low 4GB` and 720p/windowed that does not require command-line flags
- If creating `.lnk` files during packaging, do it in `Tools\Package-Windows-Classroom.ps1` or a closely related packaging helper, and verify it survives zip/extract.
- If adding `.bat`, keep the file double-clickable, avoid requiring typed commands, and document that it is school-image dependent.
- If changing defaults in `Config\DefaultGame.ini`, update `Docs\TUNING.md` and relevant classroom docs.
- Do not require administrator rights.
- Do not assume PowerShell is available on school machines either.

Inspect first:

- `README.md`
- `Docs\CLASSROOM_DEPLOYMENT.md`
- `Docs\TUNING.md`
- `Config\DefaultGame.ini`
- `Tools\Package-Windows-Classroom.ps1`
- `Tools\Verify-ClassroomPackage.ps1`
- menu/settings code under `Source\BlackoutHunt`, especially graphics/settings UI paths

Acceptance criteria:

- A student can double-click a file in the extracted package and get either normal launch or low-spec launch.
- If the chosen fallback is `.bat`, it has been tested successfully on the actual school Windows image.
- Instructions no longer rely on untested `.cmd` or `.bat` files for locked-down classroom machines.
- The fallback does not need admin rights.
- Package verification still passes.

Validation:

```powershell
.\Tools\Build-Editor.ps1
.\Tools\Package-Windows-Classroom.ps1
.\Tools\Verify-ClassroomPackage.ps1
```

Manual validation:

- Extract the zip on a clean Windows account.
- Double-click the normal launcher.
- Double-click the low-spec fallback if implemented, including `.bat` if that is the chosen path.
- Confirm the game reaches the menu and Settings can show/apply the low profile.

LT2 development note from May 28:

- A packaged `.lnk` fallback was investigated but rejected for tomorrow's distribution because Windows shortcut creation kept absolute package paths after the folder was moved. That is not safe for a zip that will be extracted to arbitrary school-machine folders.
- A non-console Windows GUI helper executable is the viable replacement path for a future package. The prototype launched `BlackoutHunt.exe` from the same folder with `-d3d11 -BHVirtualBoxSafe -ResX=1280 -ResY=720 -WINDOWED`, showed no console subsystem, and smoke-tested successfully against a fake local `BlackoutHunt.exe`.
- Do not mix that helper into the May 29 frozen archive unless a real launch blocker is confirmed and a new archive, sidecar hash, and package verification pass are produced. For the current frozen distribution, the student-safe path remains normal double-click `BlackoutHunt.exe`, then in-game Settings for `Low 4GB` / 720p windowed on machines that reach the menu.

## Task LT3 - Rehearse Teacher Host Plus Two Student Joins

High-value outcome: confirms the real school network, locked-down Windows policy, and Playit endpoint before a room of students is waiting.

Current state:

- Live Classroom binds the host to `127.0.0.1` by default and publishes the configured Playit endpoint.
- The intended endpoint is `blackouthunt.playit.plus:24761`.
- This path is designed to avoid Windows Firewall prompts on locked-down school PCs.

Operator steps:

1. Extract the verified 0.2.0-beta.6 classroom zip on the teacher machine.
2. Start `BlackoutHunt.exe` by double-clicking it.
3. Host `LIVE CLASSROOM` Facility.
4. Open the Classroom tab and check Host Preflight.
5. Confirm the preferred join address is `blackouthunt.playit.plus:24761`.
6. On two student machines, start `BlackoutHunt.exe`.
7. Enter lobby names when prompted.
8. Join using the saved classroom endpoint or by typing `blackouthunt.playit.plus:24761`.
9. Confirm both students appear on the host roster with the expected names.
10. Ready both students.
11. Assign one Teacher if needed.
12. Start a short round.

Acceptance criteria:

- Teacher host reaches Live Classroom without admin rights.
- Student machines launch without admin rights.
- No Windows Firewall public/private networks prompt appears.
- Two student clients join through Playit.
- Roster names appear correctly.
- Ready gate works.
- Host can kick a stuck/unready student.
- Students cannot see host-only admin, tunnel, role assignment, preflight, or classroom settings controls.

Validation:

- No developer commands required on student machines.
- If safe, create a support bundle afterward on the teacher/dev machine.

## Task LT4 - Validate Teacher Host Preflight And Support Bundle

High-value outcome: the teacher can see setup problems and collect diagnostics without exposing student data.

Current state:

- Beta.6 includes a host-only Classroom Preflight panel.
- `Tools\New-ClassroomSupportBundle.ps1` creates `PREFLIGHT.md` and a zip under `Builds\Support`.
- Classroom docs say the bundle avoids saved account data, backend `.env` files, and backend data.

Operator steps:

1. Start the package on the teacher machine.
2. Host Live Classroom.
3. Open Classroom tab.
4. Review Host Preflight.
5. Confirm labels make sense for:
   - beta version
   - map/classroom mode
   - Playit endpoint
   - join-code readiness
   - loopback/direct-LAN state
   - tunnel/helper state
   - graphics/RHI status
   - package root
   - runtime log path
   - support bundle path
6. Create a support bundle from the teacher/dev machine if needed.

Acceptance criteria:

- Host Preflight does not expose unsafe account data, secrets, or student-private info.
- Host can identify the join endpoint before students join.
- Host can find logs/support folder if a failure happens.
- Remote student clients cannot access these controls.

Validation:

```powershell
.\Tools\New-ClassroomSupportBundle.ps1
```

This command is for the development or teacher support machine, not student machines.

LT4 validation result from the dev machine on May 28, 2026:

- Frozen package to distribute is `Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip`, with its sidecar `.zip.sha256` in the same folder.
- SHA-256 was rechecked on that distribution copy and matches the documented sidecar value: `9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7`.
- `Builds\Archives` no longer contains the 0.2.0-beta.6 zip in this checkout; use the verified distribution-folder copy above.
- `.\Tools\Verify-ClassroomPackage.ps1` passed against `Builds\Windows`.
- Archive inspection found the expected root launcher, notices, DX11 fallback files, and Win64 Shipping executable in the zip.
- `.\Tools\New-ClassroomSupportBundle.ps1` created `Builds\Support\BlackoutHunt-ClassroomSupport-20260528-174602.zip` and `Builds\Support\classroom-support-20260528-174602\PREFLIGHT.md`; its internal package verification passed.
- The support bundle zip was checked for saved account, crash, backend data, `.env`, secret, credential, token, profile, and progress path matches; none were found.
- `PREFLIGHT.md` reports `ProjectVersion: 0.2.0-beta.6`, `ClassroomJoinEndpoints: blackouthunt.playit.plus:24761`, `bClassroomLoopbackOnlyHost: True`, `bAllowStudentTeacherAdminControls: False`, `DefaultGraphicsRHI: DefaultGraphicsRHI_DX11`, `DefaultPlatformService: Null`, `live_classroom_admin_required: False`, and `firewall_prompt_expected: No for Live Classroom loopback hosting`.
- Host-only boundaries were reviewed in `BHPlayerController` and `SBHMainMenu`: Host Preflight and its file/support/join-code actions require a local standalone or listen-server controller, and the Classroom Preflight panel is collapsed for remote client controllers.
- Packaged Live Classroom smoke was run with one host and two local clients. The successful marker set reached `HOST_LISTENING`, `JOIN_ADDRESS:blackouthunt.playit.plus:24761`, two client `JOINED` markers, `READY_SET`, and `ROUND_STARTED`. The smoke wrapper ended before writing its final clean-exit result block, so do not count this as the LT5 soak or a full clean-exit smoke pass.
- Before students enter the room, still do the real teacher-machine visual check: double-click `BlackoutHunt.exe`, host `LIVE CLASSROOM` Facility, open the Classroom tab, refresh Host Preflight, confirm the endpoint is `blackouthunt.playit.plus:24761`, and confirm no student/private account data is displayed.

## Task LT5 - Run A Short Classroom Soak

High-value outcome: covers the release-note gap that the quick stability-gate soak was not completed.

Current state:

- Beta.6 package verification passed.
- Automation passed.
- Release notes explicitly say the quick stability-gate soak was not completed.

Operator steps:

1. Launch the packaged game on the teacher machine.
2. Host Live Classroom Facility.
3. Leave the lobby/menu open for 30-60 minutes if time allows.
4. If possible, keep one or two student clients connected for part of the soak.
5. Watch for crashes, disconnects, GPU errors, Playit endpoint loss, audio runaway, or severe frame drops.

Acceptance criteria:

- Teacher machine stays open for at least 30 minutes.
- Join endpoint remains visible/usable.
- No crash occurs.
- If clients are connected, they remain connected or reconnect cleanly.

Validation:

- Review packaged runtime logs after the soak if safe.
- Do not share logs publicly if they contain local setup details.

LT5 dev-machine result from May 28, 2026:

- The verified distribution archive was extracted to `Builds\Validation\LT5-ExtractedPackage-20260528-180037` and used for the soak; no rebuild was made.
- A manual packaged soak ran `LiveFacility` with one host and two local clients from `Saved\PackagedClassroomSmoke\lt5-manual-20260528-180232`.
- Startup markers reached `HOST_LISTENING`, `JOIN_ADDRESS:blackouthunt.playit.plus:24761`, two client `JOINED` markers, `READY_SET`, and `ROUND_STARTED`.
- Host plus both clients stayed alive for `30.9` minutes. They were manually stopped after the LT5 threshold; no packaged process remained.
- `.\Tools\Test-RuntimeLogs.ps1 -Path Saved\PackagedClassroomSmoke\lt5-manual-20260528-180232` passed after the soak.
- `BHAutoQuitSeconds=1800` did not hold the earlier wrapper-driven run open, so the completed soak intentionally omitted auto-quit and used manual stop after the threshold.
- Current dirty `.\Tools\Verify-ClassroomPackage.ps1` rerun against the extracted archive fails because the in-progress asset policy now forbids cooked SCP096 prototype content. Treat that as a package-policy blocker for any new/revised package decision, not as an LT5 runtime soak failure.

## Task LT6 - Prepare Low-Spec Student Machine Flow With Tested Launchers

High-value outcome: older lab PCs have a realistic path that does not rely on unverified `.cmd` or `.bat` files.

Current state:

- The game has `Low 4GB` mode, dynamic resolution, FPS caps, and a 720p/windowed option.
- The packaged `.cmd` launchers are not usable if school blocks console execution.
- `.bat` may work, but it needs a quick proof on the real school image before students depend on it.
- Unreal still requires Direct3D feature level 11.0 / Shader Model 5.0. Some machines may fail before the menu if they expose only Microsoft Basic Display Adapter, Remote Desktop software graphics, unsupported VM graphics, or pre-DX11 GPUs.

Prepared 0.2.0-beta.6 package status for May 29:

- Archive hash was rechecked for the LT1 archive and matches the documented SHA-256: `9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7`.
- Practical package verification against `Builds\Windows` passed during this LT6 pass before later unrelated verifier-policy changes were observed in the dirty worktree.
- Earlier dirty asset-policy and verifier-script edits are not enough to invalidate the frozen package. LT9 supersedes this with an archive-only distribution decision: distribute the exact verified 0.2.0-beta.6 zip and do not mix in current dirty worktree outputs.
- Do not add a sidecar launcher, shortcut, or rebuilt package for tomorrow unless a real launch blocker is confirmed and the new package is revalidated.
- The verified archive contains `BlackoutHunt.exe`, `Launch-BlackoutHunt-DX11.cmd`, and `Launch-BlackoutHunt-DX11-Low.cmd`. It does not contain a `.bat` or `.lnk` low-spec fallback.
- Treat the packaged `.cmd` launchers as developer/IT fallback files only until the actual school Windows image proves console execution is allowed. Both `.cmd` files run through `cmd.exe`, so a console-blocking policy can block them.
- The practical tested launch path for students is the normal double-click `BlackoutHunt.exe` path from the verified archive, then in-game Settings for weak machines that reach the menu.
- The in-game Settings panel includes `Low 4GB`, adaptive graphics, FPS goal controls, and `720 W` resolution. The Classroom workflow also exposes quick `Low 4GB` and `720p Windowed` controls for school PCs.

Operator flow for tomorrow:

1. Extract the LT1 archive on each student machine and start only by double-clicking `BlackoutHunt.exe`.
2. If the game reaches the menu on a weak, unknown, integrated-GPU, or stuttering machine, open Settings and choose `Low 4GB`, keep adaptive graphics on, set FPS goal to `30` or `45`, and choose `720 W` / 720p windowed before joining.
3. If a machine already looks smooth at the menu, leave Auto graphics alone for the first round and avoid spending class time tuning it.
4. Do not ask students to run `.cmd`, `.bat`, PowerShell, terminal commands, or Unreal console commands during class. Use a `.cmd` or `.bat` fallback only if the teacher/IT has already proven that exact file type works on the school image before students arrive.
5. Keep a quick paper or whiteboard list with machine ID, status, and action: `Green = menu reached`, `Amber = menu reached after Low 4GB/720p`, `Red = failed before menu`.
6. If a machine fails before the menu, record the visible error in a short phrase and move that student to another machine or pair them with another student.
7. If a machine shows the D3D11-compatible GPU error, do not spend class time debugging it unless a driver/hardware acceleration fix is obvious. Microsoft Basic Display Adapter, Remote Desktop software graphics, unsupported VM graphics, and pre-DX11 GPUs are likely no-go for this package.

Implementation follow-up:

- Work on LT2 after the test if normal double-click is not reliable enough.
- Consider defaulting classroom packages more conservatively if many lab machines struggle before students can reach Settings.
- If `.cmd` or `.bat` works on the real school image, record that in LT8 feedback with the exact file used. If not, the next build should prefer a non-console `.lnk` or helper executable path rather than a batch fallback.

Acceptance criteria:

- The teacher has a tested fallback plan that does not depend on unverified console launchers.
- Weak machines that can reach the menu are switched to low settings.
- Machines that cannot expose D3D11 feature level 11 are identified quickly rather than consuming class time.

## Task LT7 - Lock A Simple First-Round Lesson Plan

High-value outcome: the first live test measures the classroom flow instead of exposing students to every unfinished system at once.

Locked first round for May 29:

- Package: `Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip`.
- SHA-256: `9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7`.
- Teacher and students should all extract from that same zip. Do not run from a developer checkout or a stale copied `Builds\Windows` folder.
- Entry point: double-click `BlackoutHunt.exe`.
- Map and mode: `LIVE CLASSROOM` Facility.
- Lesson preset: `Electricity easy`.
- Scare intensity: low.
- Teacher count: one.
- Bots: none for the first attempt unless the lobby cannot start; if needed, add only the minimum practical number.
- Duration: default.
- Host board: open with `B` and move it to the projector/display before readying the lobby.
- Offline backup: use the 12-question set below if the network, endpoint, or lobby flow fails.

Teacher setup checklist:

1. Extract the verified zip on the teacher machine.
2. Start `BlackoutHunt.exe` by double-clicking it.
3. Open `LIVE CLASSROOM`.
4. Select Facility.
5. Select `Electricity easy`.
6. Set scare intensity to low if the preset does not already do so.
7. Confirm the join endpoint shown to students is `blackouthunt.playit.plus:24761`.
8. Open the classroom board with `B`.
9. Keep host-only controls, preflight details, tunnel controls, force-start/test controls, and admin actions on the teacher machine only.

Student-facing script:

1. "Double-click `BlackoutHunt.exe`."
2. "Enter your lobby name when asked."
3. "Join using the saved classroom endpoint, or type `blackouthunt.playit.plus:24761`."
4. "Wait in the lobby until your name appears on the classroom board."
5. "Press Enter when I ask the class to ready up."
6. "During the round, move with WASD, look with the mouse, answer stations with 1-4, and hold E when the center prompt asks you to interact."
7. "Use F for flashlight if you need it."

Do not mention terminal commands, `.cmd` launchers, PowerShell, Unreal console commands, force-start shortcuts, tunnel setup, or admin/test controls to students during the live round.

Round stop conditions:

- If more than three student machines cannot reach the menu quickly, stop onboarding and pair affected students with working machines.
- If the Playit endpoint or lobby join is still failing after about five minutes, stop the network attempt and switch to the offline backup.
- If the round starts but students are confused by controls, keep the test short and collect feedback instead of adding more systems.

Offline 12-question electricity backup:

| # | Question | A | B | C | D | Answer |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Which material usually lets electricity flow easily? | Rubber | Copper | Glass | Wood | B |
| 2 | Which material is usually an electrical insulator? | Copper | Aluminum | Rubber | Steel | C |
| 3 | What must a simple circuit have for a bulb to stay lit? | A closed path | A broken wire | No battery | Only plastic parts | A |
| 4 | What does a battery provide in a simple circuit? | A source of electrical energy | A way to block current | A type of switch | A sound sensor | A |
| 5 | What does a switch do? | Changes electricity into sound | Opens or closes a circuit | Stores heat | Measures mass | B |
| 6 | In a lit bulb, electrical energy changes mostly into what? | Light and heat | Gravity | Magnetism only | Water | A |
| 7 | What is electric current? | The flow of electric charge | The color of a wire | The weight of a battery | The shape of a plug | A |
| 8 | In a series circuit, how many main paths does current have? | One | Two equal paths | Four paths | No path | A |
| 9 | In a parallel circuit, what happens if one branch breaks? | Every branch must stop | Other branches can still work | The battery disappears | The wires turn into insulators | B |
| 10 | What is the job of a fuse or circuit breaker? | Make wires longer | Protect the circuit by opening it when current is too high | Make electricity free | Turn light into sound | B |
| 11 | Which is the safest rule near wall outlets? | Use wet hands | Push metal objects into sockets | Keep liquids and metal objects away | Pull plugs by the cord hard | C |
| 12 | What is static electricity? | A buildup of electric charge | A type of battery only | A broken switch | A wire color | A |

Acceptance criteria:

- The first round has one locked map, preset, scare level, teacher count, and package.
- Students know only the minimum controls needed for round one.
- The teacher can run the session without exposing admin/test controls.
- If the network fails, the manual question set still lets the class activity continue.

## Task LT8 - Collect Safe Feedback And Logs After The Test

High-value outcome: post-test fixes are based on concrete observations, not memory.

Prepared capture sheet:

```text
Docs\LIVE_CLASSROOM_FEEDBACK_2026-05-29.md
```

Use that file immediately after the live run. Keep it teacher-local until any lobby names, reports, support bundles, or logs have been reviewed for privacy.

Collect:

- build version: `0.2.0-beta.6`
- archive name and hash if known
- map and mode
- host/client count
- machine specs where failures happened
- graphics preset used
- exact join path used
- lobby names if safe and local
- lesson preset/manual set used
- what students got stuck on
- whether `.cmd` or `.bat` launchers worked or were blocked
- crashes, disconnects, GPU errors, or stuck lobby states
- runtime log from the teacher machine if safe

Do not collect or share:

- saved account data
- passwords
- backend secrets
- private student account/profile data
- unnecessary IP/account details
- raw logs if they contain sensitive local setup details

Acceptance criteria:

- There is enough evidence to turn failures into specific follow-up tasks.
- Feedback preserves student privacy.
- Support bundle is created if technical failures occur.

Post-test workflow:

1. Fill the package anchor, session summary, launch matrix, join matrix, and student-friction sections in `Docs\LIVE_CLASSROOM_FEEDBACK_2026-05-29.md`.
2. For each crash, disconnect, GPU error, blocked launcher, stuck lobby, or role/admin visibility problem, add one row under `Technical Failures` with a local machine label and reproduction steps.
3. If technical failures occurred on the teacher/dev machine, run `.\Tools\New-ClassroomSupportBundle.ps1` after the session and record the bundle path in the feedback sheet.
4. If package runtime logs exist, run `.\Tools\Test-RuntimeLogs.ps1 -Path .\Builds\Windows\BlackoutHunt\Saved\Logs` and record only the sanitized result unless raw logs have been reviewed.
5. Convert each distinct failure into a follow-up task tied to an evidence ID such as `LT8-F1`.

## Task LT9 - Only Rebuild If A Specific Blocker Is Fixed

High-value outcome: avoids last-minute changes invalidating the known package.

Current state:

- The verified classroom package was built early May 28, 2026.
- The source tree contains later changes after that package time.
- A rebuild may be correct if fixing a real blocker, especially LT2, but it must be treated as a new release candidate.

LT9 decision for May 29:

- Do not rebuild for the live classroom test unless a new, reproduced blocker appears.
- Distribute `Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip` plus `Builds\Distribution\LiveClassroom-2026-05-29\BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip.sha256`.
- Expected SHA-256: `9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7`.
- Do not distribute the current local `Builds\Windows` folder; a May 28 LT9 recheck found that `BlackoutHunt\Binaries\Win64\BlackoutHunt-Win64-Shipping.exe` has drifted from the archived 0.2.0-beta.6 executable.
- Practical archive checks completed: hash matches the sidecar and this document, the zip can be opened and read, no saved account/log/crash/class report/telemetry/credential/debug paths were found in the archive entry list, packaged `playit.exe` hash matches the expected classroom helper hash, and required 0.2.0-beta.6 manifest assets are present.
- `.\Tools\Verify-ClassroomPackage.ps1` is not currently a reliable rerun target until its dirty in-progress syntax error near line 216 is resolved; do not change the package decision based on that unrelated tool edit.

Rebuild only for:

- a confirmed launch blocker
- a confirmed join/preflight blocker
- a tested low-spec launch fix that is needed before class
- a crash fix reproduced from the packaged 0.2.0-beta.6 build

Do not rebuild for:

- new maps
- new scares
- new imported assets
- broad visual polish
- EOS/Steam/online-service changes
- Linux/Wine work
- direct LAN/hotspot changes unless IT specifically requires it

Validation after any rebuild:

```powershell
.\Tools\Build-Editor.ps1
.\Tools\Package-Windows-Classroom.ps1
.\Tools\Verify-ClassroomPackage.ps1
.\Tools\New-ClassroomSupportBundle.ps1
```

Manual validation after any rebuild:

- clean extract
- double-click `BlackoutHunt.exe`
- host Live Classroom Facility
- join with two student machines
- verify no Firewall prompt
- verify student/admin visibility boundaries
- verify the low-spec launch path if LT2 changed launch behavior, including `.bat` if used

Acceptance criteria:

- New package has a new timestamped archive and hash.
- Old and new package names are not confused.
- Release notes or a short handoff note states exactly why the rebuild was made.

## References For New Chats

Read these before changing code or packaging:

- `README.md`
- `Docs\CLASSROOM_DEPLOYMENT.md`
- `Docs\BETA_RELEASE_NOTES_0.2.0-beta.6.md`
- `Docs\TUNING.md`
- `Docs\ASSETS.md`
- `Docs\CODEX_GAME_IMPROVEMENT_TASKS.md`

For any task touching visuals, audio, package content, cook paths, or runtime references, check `Docs\ASSETS.md` and search `Content\` first.

For any task touching host controls, classroom flow, accounts, logs, reports, or networking, preserve host-only/admin boundaries and do not expose private student/account/network data.
