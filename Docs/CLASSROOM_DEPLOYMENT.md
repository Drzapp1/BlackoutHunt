# Classroom Deployment

This guide is for live classroom use where one teacher machine hosts Blackout Hunt and students join from school PCs.

## Classroom Defaults

- The project defaults to classroom mode in `Config/DefaultGame.ini`.
- External Google/Microsoft login and backend sync are disabled by default.
- Local username/password profiles remain available on each machine.
- Network hosting, direct-IP join, and the Playit tunnel fallback remain available without requiring administrator rights.
- The classroom package disables Unreal UDP Messaging and game-target Unreal Trace so the game does not open engine diagnostic sockets at launch.
- Live Classroom hosts bind to `127.0.0.1` by default through `bClassroomLoopbackOnlyHost=True`, then publish the configured Playit endpoint. This avoids the Windows Defender Firewall public/private networks prompt on locked-down school PCs.
- Session/admin controls are host-machine-only. Students can still play the in-game Teacher role when assigned, but that role does not grant admin controls.
- Host force-start is disabled for classroom releases. Keep `bAllowHostForceStart=False` for live classes.
- The Live Classroom menu buttons let the host choose Facility, Substation, or Foggrounds with the selected lesson preset. Built-in presets include `Electricity easy`, `Mixed exam prep`, `Low scare`, and `Hard mode`.
- The host can adjust Physics question focus, complexity mix, class/individual mastery targets, and scare intensity from the Classroom tab after Live Classroom starts.
- The Classroom tab can generate a local 12-question printable manual set from the selected lesson preset, including hints and a teacher answer key.
- Scare intensity levels are `Off`, `Low`, `Horror`, and `Chaos`. `Horror` is the default live-classroom level and is tuned for frequent automatic scares.
- Every connected player must ready up before a normal live round starts. The listen-server host can soft-kick stuck or misjoined human players from the lobby roster; kicked students return to the main menu and may rejoin.
- Caught students who return as Hall Monitors still count toward Physics Classroom participation, class mastery, and individual mastery. Their trap and hint tools stay locked until they meet the answer-team contribution target.
- The host can open a projector-friendly classroom board from the Classroom tab or `B`. It shows phase, timer, the preferred join address, readiness, role mix, objective progress, and revision mastery without showing student locations or answer keys.

## Teacher-Hosted Classroom

1. Extract the packaged Windows classroom zip and start `BlackoutHunt.exe` on the teacher machine. The classroom package includes app-local Windows runtime DLLs, so normal game launch should not require administrator rights or a separate VC++ Redistributable install.
2. Choose the `LIVE CLASSROOM` button for the map you want from the Play menu, or host a normal map if you are not running Physics Classroom. Before students join, open the Classroom tab and check Host Preflight for `Ready`, `Needs tunnel`, or `Missing endpoint`.
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

## Playtest Telemetry Heatmaps

Host machines can explicitly export anonymous playtest telemetry from the Classroom tab with `Export Heatmap`, or from the console with `ExportPlaytestTelemetry`. The export writes CSV files under:

```text
Saved\PlaytestTelemetry
```

The event CSV is designed for spreadsheet heatmaps or quick map overlays. It records event type, map/stage/round phase, world location, anonymous session-local player tags, role labels, question/topic metadata, and counts. Useful rows include captures, exit route choices, escapes, objective starts/completions/stalls, unused lockers, CCTV detections, scare cues, jumpscares, Teacher scans, wrong-answer noise, battery pickups, and flashlight starvation.

Telemetry is local, teacher/developer-controlled, and not exported automatically. The anonymous tags are generated from runtime-only player/object IDs and reset with the local telemetry session; the export does not write player names, IP addresses, online account IDs, credentials, answer keys, or local account data.

## Tunnel Fallback

Use the Playit tunnel helper only from the host machine.

1. Host the match with `LIVE CLASSROOM`.
2. The game starts the verified bundled Playit agent automatically from the host machine when classroom tunnel support is enabled.
3. If the status says `network setup required`, use the opened Playit page to create or select a Custom UDP tunnel to `127.0.0.1:7777`.
4. When the game detects a usable tunnel allocation in the agent log, the host status shows the join address. For the owned beta tunnel, students can use `blackouthunt.playit.plus:24761` from the saved join list. You can also put another allocation host/port in the menu and copy a `BH1:...` join code.

The game verifies the bundled `playit.exe` hash before launching it. If verification fails, the helper is not launched.

## Direct LAN and Firewall Prompts

The school-safe Live Classroom path is tunnel-only by default. It binds the listen server to `127.0.0.1`, so Windows should not ask a standard student or teacher account to allow inbound public/private network access.

Direct LAN hosting still exists through `Host LAN`, normal direct-IP host commands, or by setting `bClassroomLoopbackOnlyHost=False` in an IT-managed build. Those paths listen on UDP `7777` on the teacher machine network interface and can trigger the Windows Firewall consent prompt. On school PCs, do not rely on the game to create that firewall rule; have IT pre-authorize the executable/port or use the default Playit classroom endpoint.

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

The host-only Classroom tab includes Host Preflight. It is designed for a standard teacher account without administrator access. It shows the beta version, map/classroom mode, Playit endpoint, join-code readiness, loopback/direct-LAN state, admin-access expectation, tunnel and hotspot permissions, online subsystem, graphics/RHI status, package root, runtime executable folder, runtime log path, support bundle folder, support-bundle tool state, and deployment guide path. Buttons let the host refresh the summary, copy the classroom join code, start the verified Playit helper, open logs/package/support folders, create a support bundle when the tool is present, and open this guide. Remote student clients do not get these host setup controls.

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

Linux groundwork is available through:

```powershell
.\Tools\Package-Linux-Classroom.ps1
```

Native Linux validation is still a separate release task.

## Release Checklist

- Build editor target: `.\Tools\Build-Editor.ps1`
- Package classroom Windows build: `.\Tools\Package-Windows-Classroom.ps1`
- Confirm package verification passes.
- Create and review a classroom preflight/support bundle: `.\Tools\New-ClassroomSupportBundle.ps1`
- On a clean Windows machine, confirm a standard user can extract the zip and launch `BlackoutHunt.exe` without administrator rights or VC++ Redistributable installation.
- On low-spec or lab machines, confirm `Launch-BlackoutHunt-DX11.cmd` and `Launch-BlackoutHunt-DX11-Low.cmd` start the game. If the D3D11-compatible GPU error appears, confirm the machine is not using Microsoft Basic Display Adapter, hidden hardware acceleration through Remote Desktop, or a VM graphics path without Direct3D feature level 11.0.
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
