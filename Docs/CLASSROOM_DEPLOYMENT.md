# Classroom Deployment

This guide is for live classroom use where one teacher machine hosts Blackout Hunt and students join from school PCs.

## Classroom Defaults

- The project defaults to classroom mode in `Config/DefaultGame.ini`.
- External Google/Microsoft login and backend sync are disabled by default.
- Local username/password profiles remain available on each machine.
- Network hosting, direct-IP join, and the Playit tunnel fallback remain available without requiring administrator rights.
- Session/admin controls are host-machine-only. Students can still play the in-game Teacher role when assigned, but that role does not grant admin controls.
- Host force-start is disabled for classroom releases. Keep `bAllowHostForceStart=False` for live classes.
- The Live Classroom menu button hosts Facility with the current Physics Classroom defaults: 10 minutes, adaptive questions, all topics, and Horror scare intensity.
- The host can adjust Physics question focus, complexity mix, class/individual mastery targets, and scare intensity from the Classroom tab after Live Classroom starts.
- Scare intensity levels are `Off`, `Low`, `Horror`, and `Chaos`. `Horror` is the default live-classroom level and is tuned for frequent automatic scares.
- Every connected player must ready up before a normal live round starts. The listen-server host can soft-kick stuck or misjoined human players from the lobby roster; kicked students return to the main menu and may rejoin.
- The host can open a projector-friendly classroom board from the Classroom tab or `B`. It shows phase, timer, the preferred join address, readiness, role mix, objective progress, and revision mastery without showing student locations or answer keys.

## Teacher-Hosted LAN

1. Extract the packaged Windows classroom zip and start `BlackoutHunt.exe` on the teacher machine. The classroom package includes app-local Windows runtime DLLs, so normal game launch should not require administrator rights or a separate VC++ Redistributable install.
2. Choose `LIVE CLASSROOM` from the Play menu for the one-click live lesson flow, or host a normal map if you are not running Physics Classroom.
3. The game shows the preferred classroom join address when the listen server is open. For the owned beta tunnel this is `blackouthunt.playit.plus:24761`; direct LAN still uses the teacher machine IP and UDP `7777`.
4. Students enter an in-game lobby name if prompted, then choose the saved classroom endpoint or type the teacher address, for example `blackouthunt.playit.plus:24761` or `192.168.1.20:7777`.
5. If no student reaches the host, the classroom preflight starts the verified bundled Playit agent and changes status to either `LAN blocked, tunnel ready` when a usable tunnel address is detected or `network setup required` when teacher setup is still needed.
6. Set the Physics question focus, complexity, mastery targets, and scare intensity from the Classroom tab if the default lesson profile needs changing.
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

## Tunnel Fallback

Use the Playit tunnel helper only from the host machine.

1. Host the match with `LIVE CLASSROOM`.
2. If direct LAN does not receive a student client, the game starts the verified bundled Playit agent automatically from the host machine.
3. If the status says `network setup required`, use the opened Playit page to create or select a Custom UDP tunnel to `127.0.0.1:7777`.
4. When the game detects a usable tunnel allocation in the agent log, the host status changes to `LAN blocked, tunnel ready` and shows the join address. For the owned beta tunnel, students can use `blackouthunt.playit.plus:24761` from the saved join list. You can also put another allocation host/port in the menu and copy a `BH1:...` join code.

The game verifies the bundled `playit.exe` hash before launching it. If verification fails, the helper is not launched.

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
- On a clean Windows machine, confirm a standard user can extract the zip and launch `BlackoutHunt.exe` without administrator rights or VC++ Redistributable installation.
- Host a LAN lobby from the teacher machine.
- Join with at least two student clients.
- If direct LAN is blocked, confirm the Playit fallback reaches `LAN blocked, tunnel ready` or gives a clear `network setup required` status with agent log path.
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
