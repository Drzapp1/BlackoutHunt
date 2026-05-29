# Blackout Hunt 0.3.0-beta.1 Release Notes

## Scope

This beta is for Windows classroom testing. One teacher machine hosts Live Classroom and student machines join on UDP `7777` by direct LAN when available or by the owned Playit endpoint:

```text
blackouthunt.playit.plus:24761
```

Public internet matchmaking is still out of scope. The checked-in build uses `OnlineSubsystemNull`; EOS or Steam lobby/relay setup is deferred to a later beta.

Fedora/Wine and native Linux are secondary compatibility tracks and do not gate this Windows classroom beta.

## Tester Entry Points

- Extract the packaged Windows classroom zip and start `BlackoutHunt.exe`. The package includes app-local Windows runtime DLLs, so classroom testers should not need administrator rights or a separate VC++ Redistributable install.
- The teacher uses a `LIVE CLASSROOM` map button for Facility, Substation, or Foggrounds.
- Students enter an in-game lobby name when prompted, then use the saved classroom join list or type `blackouthunt.playit.plus:24761`.
- Direct LAN still works when the teacher IP and UDP `7777` are reachable.
- If a teacher-created Playit tunnel is unavailable, the host preflight reports `network setup required` and points to the agent/setup state.

## Changes Since 0.2.2-beta.1

- Ships `0.3.0-beta.1` and supersedes `0.2.2-beta.1`.
- Adds the AI-generated Windows executable icon as `Build/Windows/Application.ico`, with the source PNG tracked beside it.
- Adds the `-BHAutoMinPlayers` automation flag so multi-client classroom validation can wait for a target ready-player count.
- Closes gameplay menu and classroom board UI when leaving the lobby, with a `ROUND_UI_CLOSED` automation marker for validation.
- Logs the classroom preflight join address for automation and tester diagnostics.
- Routes packaged-build Derived Data Cache output to `Builds/DerivedDataCache` and uses the installed local fallback DDC profile.
- Keeps Playit download/setup pages user-driven instead of opening them automatically from the game.
- Doubles the Physics classroom question bank to 320 IGCSE-style revision prompts with balanced topic, difficulty, and question-type coverage.
- Lets teachers pick Facility, Substation, or Foggrounds directly from the Live Classroom menu flow.
- Restores the SCP096 prototype visual for the jumpscare actor, with procedural fallback pieces still available when the asset is missing.
- Restores explicit survivor HUD readouts for flashlight battery and teacher proximity.
- Adds many more revision objective modules on every runtime map, with the densest coverage pass on Foggrounds, and randomizes revision node fill across the map.
- Switches Windows classroom packages to D3D11 by default and writes DX11/DX11-low helper launchers plus a GPU troubleshooting note into the package root.
- Fixes menu tab widget index mapping for the native menu.

## Classroom Defaults

- Classroom mode is enabled.
- Host force-start is disabled.
- Student admin/session controls are disabled.
- External Google/Microsoft account login and backend sync are disabled.
- Windows hotspot helper is disabled by default because it can require administrator networking permission.
- Windows classroom graphics default to D3D11. The package also includes `Launch-BlackoutHunt-DX11.cmd` and `Launch-BlackoutHunt-DX11-Low.cmd` for older lab machines.
- Local classroom profiles remain available and can be reset from the Account panel.

## Known Limits

- The Playit endpoint depends on the teacher-owned tunnel/agent staying configured and online.
- Online session Host/Find/Join is for local/development validation only until EOS or Steam is configured.
- The current maps are runtime-generated prototype levels rather than authored production maps.
- Unreal still requires a GPU/driver exposing Direct3D feature level 11.0 / Shader Model 5.0; Microsoft Basic Display Adapter, Remote Desktop without hardware acceleration, some VMs, and pre-DX11 GPUs can still fail before the menu.
- The SCP096 prototype jumpscare visual is included for beta classroom testing; broader distribution still needs a final source-asset audit.
- Broad physical-device validation remains a follow-up after VM classroom validation.

## Required Beta Validation

- Build editor target.
- Package the Shipping classroom build.
- Confirm package verification passes, including app-local runtime DLL checks and the Windows executable icon.
- Confirm a clean Windows standard user can extract and launch the package without administrator rights or VC++ Redistributable installation.
- Run `BlackoutHunt.Network.NormalizeJoinAddress`, `BlackoutHunt.Automation.CommandLine`, and classroom min-player automation.
- Validate host plus two Windows student clients with automation flags.
- Validate default, D3D11, low/windowed, VirtualBox-safe, repeated launch/quit, cold boot, and a 60-minute menu/classroom idle soak.
- Validate Playit join through `blackouthunt.playit.plus:24761` where internet access is available.
- Validate Live Classroom roster names, all-player ready gate, kick flow, question controls, and board window.
- Confirm remote students cannot use admin/session/tunnel controls and can only change local display/audio/profile settings.
- Confirm late joiners during Hunt enter as survivor spectators only.
- Confirm the distributed package contains no saved account data, logs, crashes, backend secrets, or debug symbols.

## Feedback

Record tester reports with:

- build version: `0.3.0-beta.1`
- map and mode
- host/client count
- machine specs and graphics preset
- exact join path used: saved Playit endpoint, direct LAN, typed direct IP, or invite code
- lobby names used by host/students
- steps to reproduce
- packaged runtime log when safe to share
