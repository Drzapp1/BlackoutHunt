# Classroom Deployment

This guide is for live classroom use where one teacher machine hosts Blackout Hunt and students join from school PCs.

## Classroom Defaults

- The project defaults to classroom mode in `Config/DefaultGame.ini`.
- The default join limit is 32 players (`MaxPlayers`), sized for a full class plus the teacher host. Students who try to join a full lobby are returned to the menu with a "class is full" message; the host can also soft-kick stuck players to free a slot.
- External Google/Microsoft login and backend sync are disabled by default.
- Local username/password profiles remain available on each machine.
- Network hosting, direct-IP join, and the Playit tunnel fallback remain available without requiring administrator rights.
- The classroom package disables Unreal UDP Messaging and game-target Unreal Trace so the game does not open engine diagnostic sockets at launch.
- Live Classroom hosts now bind **all interfaces by default** (`bClassroomLoopbackOnlyHost=False`) and publish the configured Playit endpoint, so students can join by **LAN IP** (same room/switch) **or** the **Playit tunnel / BH1 code** (off-LAN). The tradeoff is a one-time Windows Defender Firewall "Allow" on the host PC. Set `bClassroomLoopbackOnlyHost=True` to revert to loopback-only (`127.0.0.1`, tunnel-mandatory, no LAN fallback, no firewall prompt) for a strict tunnel-only deployment.
- Session/admin controls are host-machine-only. Students can still play the in-game Teacher role when assigned, but that role does not grant admin controls.
- Host force-start is disabled for classroom releases. Keep `bAllowHostForceStart=False` for live classes.
- The Live Classroom menu buttons let the host choose Facility, Substation, or Foggrounds with the selected lesson preset. Built-in presets include `Electricity easy`, `Mixed exam prep`, `Low scare`, and `Hard mode`.
- The host can adjust Physics question focus, complexity mix, class/individual mastery targets, and scare intensity from the Classroom tab after Live Classroom starts.
- The Classroom tab can generate a local 12-question printable manual set from the selected lesson preset, including hints and a teacher answer key.
- Scare intensity levels are `Off`, `Low`, `Horror`, and `Chaos`. `Horror` is the default live-classroom level and is tuned for frequent automatic scares.
- Every connected player must ready up before a normal live round starts. The listen-server host can soft-kick stuck or misjoined human players from the lobby roster; kicked students return to the main menu and may rejoin.
- Students are automatically re-tested on questions they get wrong. A missed question is re-asked (shown as a "SECOND CHANCE" review) at the student's next objective station or train bonus terminal, and keeps coming back until they answer it correctly. This is a priority, not a blocker, so it never stalls a round.
- Caught students who return as Hall Monitors still count toward Physics Classroom participation, class mastery, and individual mastery. Their trap and hint tools stay locked until they meet the answer-team contribution target.
- Mastery is earned by genuinely revising, not guessing. Each topic climbs with correct answers (harder questions count more, with diminishing returns near the top) and dips on a wrong answer, so blind multiple-choice guessing trends downward rather than upward. A topic is also held below full mastery until the student clears the questions they previously missed. See `Docs/TUNING.md` for the exact knobs.
- A wrong answer briefly locks resubmission so students read the correction first, and objective stations then load a fresh question instead of leaving the revealed answer on screen to be re-entered. The lock only delays answering — it never freezes a student who needs to flee.
- Train bonus questions now build topic and overall mastery (so revising on the train genuinely helps the class reach the escape threshold), but they do not count toward the answer-team contribution that unlocks Hall Monitor tools — that still requires team objective stations.
- The host can open a projector-friendly classroom board from the Classroom tab or `B`. It shows phase, timer, the preferred join address, readiness, role mix, objective progress, and revision mastery without showing student locations or answer keys.

## Teacher-Hosted Classroom

1. Extract the packaged Windows classroom zip and start `BlackoutHunt.exe` on the teacher machine. The classroom package includes app-local Windows runtime DLLs, so normal game launch should not require administrator rights or a separate VC++ Redistributable install.
2. Choose the `LIVE CLASSROOM` button for the map you want from the Play menu, or host a normal map if you are not running Physics Classroom. Before students join, open the Classroom tab and follow the host-only `Run Class` checklist for preflight, map/preset, join code, roster/ready, warmup, board, and post-round export status.
3. The game shows the preferred classroom join address when the listen server is open. For the owned beta tunnel this is `blackouthunt.playit.plus:24761`.
4. Students enter an in-game lobby name if prompted, then choose the saved classroom endpoint or type `blackouthunt.playit.plus:24761`.
5. If the Playit tunnel is not ready, the classroom preflight starts the verified bundled Playit agent and reports either the configured classroom endpoint or a clear setup-required status.
6. Apply a Lesson Preset from the Classroom tab, generate a 12-question manual set if you want a printable/offline backup, or set the Physics question focus, complexity, mastery targets, duration, map, bots, and scare intensity manually if the default lesson profile needs changing.
7. Use the host machine to assign roles, ask every student to ready up, and kick stuck/misjoined blockers from the roster if needed.
8. The Live Classroom path opens the Classroom workflow automatically. You can also open the Classroom tab or press `B` on the host to launch the separate board window, then move that window to the projector/display.

Students may:

- join and leave sessions
- ready up
- vote maps
- change avatar/profile, display, audio, and graphics settings on their own machine
- use abilities for the gameplay role assigned by the host

Students may not:

- assign roles
- kick other players
- force-start rounds
- change match/classroom settings
- use the `Run Class` checklist actions
- run bot/debug commands
- start or stop tunnel helpers
- trigger targeted/admin scares
- destroy online sessions

## Lesson Presets

Lesson presets are host-only and local to the teacher machine. The Classroom tab can save the current classroom setup under a teacher-provided name, then apply it before starting a later Live Classroom or while still in the live lobby. Applying a preset during an active round only selects it for the next setup; it does not rewrite the running round.

Custom presets are stored under:

```text
Saved\ClassroomPresets\LessonPresets.json
```

Preset loading validates stale values against current topic, difficulty, mastery, duration, scare, map, and bot limits. Built-in presets remain available if the local JSON file is missing or unreadable.

The `Generate 12Q` button writes a teacher-local Markdown question set from the selected preset topic and difficulty mix, with prompts, choices, hints, formulas, and an answer key under:

```text
Saved\ClassroomPresets\QuestionSets
```

## Editing the Question Bank

The 368 built-in physics questions can be replaced with your own by dropping a `QuestionBank.json` file (no rebuild needed). The fastest start is the `bh.ExportQuestionBank` console command, which writes the full built-in bank to `Saved\ClassroomPresets\QuestionBank.json` as an editable starting point. See `Docs/QUESTION_BANK_EDITING.md` for the format and rules, and `Docs/QuestionBank.example.json` for a worked example.

## Classroom Reports

At the end of a Physics Classroom round, the host writes a local classroom report automatically. The host can also use `Export Report` from the Classroom tab, or the `ExportRevisionReport` console command, to write the current report on demand.

Reports are saved under:

```text
Saved\ClassReports
```

Each export keeps the existing CSV files for spreadsheet workflows and also writes a teacher-local `*_teacher_recap.md` file. The Markdown recap summarizes the session settings, map, preset, topic focus, difficulty mix, mastery targets, weak topics, common misconceptions, students needing follow-up by lobby display name, a suggested next preset/focus, objective progress, and a printable class recap section.

The teacher recap may include question text, answer explanations, and lobby display names. Keep it on the host machine and review it before projecting or sharing. Reports do not include IP addresses, online account IDs, credentials, heatmap locations, saved account data, or tunnel setup details.

## Playtest Telemetry Heatmaps

Host machines can explicitly export anonymous playtest telemetry from the Classroom tab with `Export Heatmap`, or from the console with `ExportPlaytestTelemetry`. The export writes CSV files under:

```text
Saved\PlaytestTelemetry
```

The event CSV is designed for spreadsheet heatmaps or quick map overlays. It records event type, map/stage/round phase, world location, anonymous session-local player tags, role labels, question/topic metadata, and counts. Useful rows include captures, exit route choices, escapes, objective starts/completions/stalls, unused lockers, CCTV detections, scare cues, jumpscares, Teacher scans, wrong-answer noise, battery pickups, and flashlight starvation.

Telemetry is local, teacher/developer-controlled, and not exported automatically. The anonymous tags are generated from runtime-only player/object IDs and reset with the local telemetry session; the export does not write player names, IP addresses, online account IDs, credentials, answer keys, or local account data.

After exporting heatmap telemetry, generate a map playtest evidence report with:

```powershell
.\Tools\New-PlaytestEvidenceReport.ps1
```

By default the tool reads telemetry event CSV files from `Saved\PlaytestTelemetry` and writes a Markdown report under:

```text
Saved\PlaytestEvidence
```

Use `-InputPath <file-or-folder>` to analyze a specific export folder, `-OutputRoot <folder>` to choose another local report folder, `-BucketSize 1000` to tune rough world-coordinate grouping, and `-Top 8` to adjust how many clusters appear per section. The report summarizes capture zones, objective stalls, locker use, route/exit choices, CCTV hotspots, scare frequency, battery starvation, and wrong-answer topic/difficulty clusters when those rows exist. If an export has too little data, the report says so instead of inventing conclusions. It intentionally omits session IDs, player tags, account/network data, selected answer text, and answer keys.

## Tunnel Fallback

Use the Playit tunnel helper only from the host machine.

1. Host the match with `LIVE CLASSROOM`.
2. The game starts the verified bundled Playit agent automatically from the host machine when classroom tunnel support is enabled.
3. If the status says `network setup required`, use the opened Playit page to create or select a Custom UDP tunnel to `127.0.0.1:7777`.
4. When the game detects a usable tunnel allocation in the agent log, the host status shows the join address. For the owned beta tunnel, students can use `blackouthunt.playit.plus:24761` from the saved join list. You can also put another allocation host/port in the menu and copy a `BH1:...` join code.

The game verifies the bundled `playit.exe` hash before launching it. If verification fails, the helper is not launched.

## Direct LAN and Firewall Prompts

Live Classroom now binds all network interfaces by default (`bClassroomLoopbackOnlyHost=False`), so it listens on UDP `7777` on the teacher machine's interfaces — letting same-LAN students join by IP without an IT-managed build — and can trigger a one-time Windows Firewall consent prompt. On locked-down school PCs, have IT pre-authorize the executable / UDP `7777`. If direct LAN is **not** needed (everyone joins over the Playit tunnel), set `bClassroomLoopbackOnlyHost=True` to bind `127.0.0.1` only: no firewall prompt, but the tunnel is then the sole way in (no LAN fallback — if the school blocks Playit/UDP nobody can join).

Direct LAN hosting is also available through `Host LAN` and normal direct-IP host commands, which likewise listen on UDP `7777`. On school PCs, do not rely on the game to create the firewall rule; have IT pre-authorize the executable/port or use the Playit classroom endpoint.

## Low-Spec And Locked-Down Machines

The primary student path is simply to double-click `BlackoutHunt.exe`. On weak hardware this is usually enough:

- When the game detects integrated or software graphics, it automatically applies a conservative graphics preset and 1280x720 windowed mode on first launch, so a machine that reaches the menu is already in a safe profile. Students can then leave Auto graphics on, or pick `Low 4GB` / `720p Windowed` from in-game Settings (also surfaced as quick controls in the Classroom workflow).
- Do not ask students to run `.cmd`, `.bat`, PowerShell, or console commands. Locked-down school accounts often block console execution, which can make the packaged `Launch-BlackoutHunt-DX11*.cmd` files unusable. Treat those `.cmd` files as IT/developer fallbacks only, until console execution is proven on the actual school image.
- The only failure in-game Settings cannot fix is a machine that cannot reach the menu because the engine fails to select a usable renderer at default settings. For that case, an optional non-console launcher that forces DX11/windowed/720p at process start is provided as source under `Tools\LowSpecLauncher`. It must be built and validated on the real school image before being added to a classroom package (see its README); it is not auto-staged into releases.
- Machines that expose only Microsoft Basic Display Adapter, Remote Desktop software graphics, an unsupported VM graphics path, or a pre-DX11 GPU cannot meet Unreal's Direct3D feature level 11.0 requirement and should be identified quickly rather than debugged during class.

## Windows SmartScreen And Antivirus

The release is an **unsigned** executable distributed as a zip, and it bundles `playit.exe` and opens UDP sockets — all common triggers for Windows SmartScreen and school antivirus/endpoint protection on locked-down lab images. Plan for this **before** class, because it is a likely first-contact failure:

- **Mark-of-the-Web:** a zip downloaded from the internet is flagged. Right-click the **zip** → Properties → tick **Unblock** → Apply, *then* extract. (Unblocking after extraction does not always propagate to the inner files.) Files copied from a USB stick or a local network share are usually not flagged.
- **SmartScreen "Windows protected your PC":** on first launch of the unsigned `BlackoutHunt.exe`, click **More info → Run anyway**. A standard locked-down student account may not be allowed to click through this at all.
- **Antivirus / endpoint protection:** the unsigned `BlackoutHunt.exe` and the bundled `playit.exe` can be quarantined. Ask IT to **allow-list the extracted build folder** (or the two executables) on the lab image ahead of time.
- **Recommended for managed labs:** have IT **pre-stage the extracted, unblocked build on each machine's image** so students never see SmartScreen or AV prompts, rather than relying on each student to click through them. Verify the exact distributed build launches cleanly on the real school image during the pre-flight, on a standard (non-admin) student account.
- Code-signing the release (Authenticode) would remove the SmartScreen/AV friction entirely and is recommended if a certificate is available; the current packaging does not sign the binary.

## Windows Hotspot

The classroom beta does not use the Windows hotspot helper by default because locked-down student/teacher machines often require administrator networking permission. Use IT-managed Wi-Fi/LAN or the Playit fallback instead.

## Local Profiles

Students can create a local username/password profile on each machine.

- Usernames are sanitized and limited to 32 characters.
- Passwords must be 8-128 characters.
- Profiles and progress stay on the local machine.
- Backend sync is disabled in classroom builds.
- Use `AccountResetLocalClassroomData` from the console or the Account panel Reset button to clear local profile, progress, and saved credentials on shared machines.

Do not distribute a packaged build that already contains `Saved\Account`, `Saved\Logs`, or `Saved\Crashes`.

## Classroom Preflight And Support Bundle

The host-only Classroom tab includes Host Preflight. It is designed for a standard teacher account without administrator access. It shows the beta version, map/classroom mode, Playit endpoint, join-code readiness, loopback/direct-LAN state, admin-access expectation, tunnel and hotspot permissions, online subsystem, graphics/RHI status, active graphics preset, resolution state, package root, runtime executable folder, runtime log path, support bundle folder, support-bundle tool state, and deployment guide path. Buttons let the host refresh the summary, copy the classroom join code, start the verified Playit helper, open logs/package/support folders, create a support bundle when the tool is present, and open this guide. Remote student clients do not get these host setup controls.

For locked-down school PCs, `Admin access` should read that administrator rights are not required for the Live Classroom defaults. If it mentions direct LAN or firewall policy, use the Playit/loopback classroom path or ask IT for a managed build instead of trying to elevate the teacher account.

Before a live class or after a tester report, create a safe diagnostic bundle with:

```powershell
.\Tools\New-ClassroomSupportBundle.ps1
```

The tool writes a `PREFLIGHT.md` report and a zip under `Builds\Support`. It records project version, classroom settings, school-account readiness, join endpoint, loopback/tunnel/hotspot settings, RHI and online subsystem settings, package verification output, Playit hash, package inventory, selected config files, and the latest safe logs. It does not collect `Saved\Account`, backend `.env` files, or `Tools\AccountBackend\data`.

## Classroom Packaging

Build the primary classroom package with:

```powershell
.\Tools\Package-Windows-Classroom.ps1
```

This creates a clean Shipping classroom package under:

```text
Builds\Windows
```

The script runs `Tools\Verify-ClassroomPackage.ps1` after packaging. Verification fails if the package contains:

- debug symbols
- saved account/profile/progress/credential files
- saved logs or crash dumps
- backend `.env` or backend data secrets
- a missing or hash-mismatched `playit.exe`
- missing app-local Windows runtime DLLs beside the root launcher or packaged Win64 Shipping executable

After packaging, run the local packaged classroom smoke test:

```powershell
.\Tools\Run-PackagedClassroomSmoke.ps1
```

This launches one packaged Live Classroom host and two packaged clients on local loopback, auto-readies them, waits for the host/client join, ready, round-start, and clean-quit automation markers, and writes a report under `Saved\PackagedClassroomSmoke\<timestamp>\SMOKE_REPORT.md`. The run does not use external accounts, public matchmaking, administrator rights, or the Playit tunnel; automation-only Live Classroom startup skips the tunnel helper and clients join `127.0.0.1:7777` directly. If `Builds\Windows` is missing, build it first with `.\Tools\Package-Windows-Classroom.ps1`.

Regression note (2026-06): Development-config packages (`.\Tools\Package-Windows.ps1` without `-Classroom`) crashed at first boot, before `HOST_LISTENING`, with `EXCEPTION_ACCESS_VIOLATION` in whatever allocation-heavy code ran first (whisper jumpscare asset discovery in one binary, physics-scene registration in another). The root cause was NOT the code on the stack: the local `D:\UE_5.7` engine drop's precompiled `UnrealGame\Development` artifacts were extracted while the engine source tree was still being written (2026-05-19), so engine object files and engine headers disagree about class layouts (verified: `UCharacterMovementComponent`'s constructor writes 4 bytes past the size its own UClass allocates, corrupting the heap at the first character CDO). Shipping artifacts are internally consistent, which is why every Shipping smoke passed. Cure: re-extract or rebuild the engine drop's Development `UnrealGame` artifacts before trusting any Development package from this machine; a quick canary is booting the packaged Development exe with `-ansimalloc` (boots) versus default (crashes) — that signature means allocator-metadata corruption from the layout mismatch, not a game bug.

Separately, the whisper jumpscare discovery (`BHDiscoverWhisperAssets`) now queries on-disk asset-registry state only (`bIncludeOnlyOnDiskAssets`): the previous default also enumerated live in-memory UObjects during the initial map load (the menu's test panel resolves variants at boot), which is unnecessary for imported-art discovery and was the first victim of the corruption above. Boot-time registry scans should stay on-disk-only.

After any packaged manual or automated run, scan the runtime logs:

```powershell
.\Tools\Test-RuntimeLogs.ps1 -Path .\Builds\Windows\BlackoutHunt\Saved\Logs
```

The gate fails on crashes, ensures, missing runtime DLLs, severe asset/map load failures, net driver or travel failures, repeated soft-load warning spam, automation failure markers, and missing required automation markers when `-ExpectAutomationMarkers` is used. Output is intentionally limited to a rule name, path, line number, and sanitized first matching line. `-AllowedFindingPattern` is a developer-only escape hatch for reviewed exceptions and should not be used for release validation.

To run the broader stability gate against an existing classroom package, including classroom package verification and runtime log checks, use:

```powershell
.\Tools\Run-StabilityGate.ps1 -SkipPackage -ExistingPackageRoot .\Builds\Windows -Configuration Shipping
```

Linux groundwork is available through:

```powershell
.\Tools\Package-Linux-Classroom.ps1
```

Native Linux validation is still a separate release task.

## Release Checklist

- Build editor target: `.\Tools\Build-Editor.ps1`
- Package classroom Windows build: `.\Tools\Package-Windows-Classroom.ps1`
- Confirm package verification passes.
- Run packaged host plus two client smoke test: `.\Tools\Run-PackagedClassroomSmoke.ps1`.
- Scan packaged runtime logs after smoke/manual runs: `.\Tools\Test-RuntimeLogs.ps1 -Path .\Builds\Windows\BlackoutHunt\Saved\Logs`.
- Create and review a classroom preflight/support bundle: `.\Tools\New-ClassroomSupportBundle.ps1`
- On a clean Windows machine, confirm a standard user can extract the zip and launch `BlackoutHunt.exe` without administrator rights or VC++ Redistributable installation.
- On low-spec or lab machines, first confirm a standard user can double-click `BlackoutHunt.exe` and reach the menu, then switch weak machines to `Low 4GB` and 1280x720 windowed mode from in-game Settings. Treat `Launch-BlackoutHunt-DX11.cmd` and `Launch-BlackoutHunt-DX11-Low.cmd` as IT/developer fallbacks until the real school Windows image proves `.cmd` execution is allowed; they are not a classroom instruction for locked-down student accounts. If the D3D11-compatible GPU error appears, confirm the machine is not using Microsoft Basic Display Adapter, hidden hardware acceleration through Remote Desktop, or a VM graphics path without Direct3D feature level 11.0.
- Host a Live Classroom lobby from the teacher machine and confirm the join address is the configured Playit endpoint.
- Join with at least two student clients through the saved classroom endpoint.
- Confirm the classroom path does not show a Windows Firewall public/private networks prompt on a standard school account.
- For IT-managed direct LAN builds only, confirm UDP `7777` is pre-authorized before testing LAN joins.
- Confirm a remote student cannot run admin commands.
- Confirm Live Classroom shows the join address, classroom board, and role roster.
- Confirm students appear in the roster by their in-game lobby names.
- Confirm the host can change question focus, complexity, mastery targets, and scare intensity, and students cannot.
- Confirm all connected players must ready before the live round starts.
- Confirm late joiners during an active round enter as survivor spectators only until the next lobby.
- Confirm the host can assign roles, soft-kick an unready student, and start a normal round once remaining players are ready.
- Confirm the host can open the Classroom board window with `B` and move it to the projector/display.
- Create, login, and reset a local profile.
- Test Facility, Substation, and Physics Classroom flows.
- Review runtime logs before distribution and do not share logs containing account or tunnel setup details.
- For validation automation, use `-BHAutomation=1` with `-BHAutoHost=LiveClassroom`, `-BHAutoJoin=<host:port-or-code>`, `-BHAutoReady=1`, `-BHAutoQuitSeconds=<seconds>`, and optional `-BHVirtualBoxSafe`. Packaged Shipping writes automation markers to `Saved\Logs\BlackoutHuntAutomation.log`.
