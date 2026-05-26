# Blackout Hunt 0.2.0-beta.5 Release Notes

## Scope

This beta is for Windows classroom testing. One teacher machine hosts Live Classroom and student machines join through the owned Playit endpoint:

```text
blackouthunt.playit.plus:24761
```

Public internet matchmaking is still out of scope. The checked-in build uses `OnlineSubsystemNull`; EOS or Steam lobby/relay setup is deferred to a later beta.

Fedora/Wine and native Linux are secondary compatibility tracks and do not gate this Windows classroom beta.

## Tester Entry Points

- Extract the packaged Windows classroom zip and start `BlackoutHunt.exe`. The package includes app-local Windows runtime DLLs, so classroom testers should not need administrator rights or a separate VC++ Redistributable install.
- The teacher uses a `LIVE CLASSROOM` map button for Facility, Substation, or Foggrounds.
- Students enter an in-game lobby name when prompted, then use the saved classroom join list or type `blackouthunt.playit.plus:24761`.
- Live Classroom binds the listen host to `127.0.0.1` by default and publishes the configured Playit endpoint, reducing Windows Firewall prompts on locked-down school PCs.
- Direct LAN remains available through `Host LAN`, normal direct-IP commands, or an IT-managed build with `bClassroomLoopbackOnlyHost=False`.

## Changes Since beta.4

- Ships `0.2.0-beta.5` and supersedes `0.2.0-beta.4`.
- Adds the train intermission and final escape pacing layer: recap, bonus question, shop, station stop, stage timers, hunter release delay, anti-camp pressure, and final escape cutscene timing.
- Adds train-specific actors for doors, displays, tunnel motion, bonus question terminals, and intermission management.
- Adds the final-station escape manager and related classroom/game-state tracking.
- Adds a StateTree-backed hunter atmosphere track, bot StateTree nodes, and a checked-in hunter atmosphere StateTree asset.
- Adds the atmosphere director, CCTV zones, security cameras, and monitor surfaces for stronger scare and surveillance beats.
- Adds powerup component, library, and shop terminal support for classroom/shop intermission rewards.
- Expands jumpscare variant support with runtime selection, Fab asset soft-path planning, fallback visuals, and menu test commands.
- Expands the host test-command menu with scrollable/collapsible groups for jumpscare variants, atmosphere probes, and round/train commands.
- Keeps Live Classroom tunnel-first by default with loopback hosting, Playit endpoint publishing, disabled game-target Unreal Trace, and disabled UDP Messaging sockets.
- Updates Hall Monitor classroom scoring so caught students still count toward class and individual mastery while tools stay gated behind answer-team contribution.
- Adds startup graphics detection and adaptive graphics controls for safer low-spec classroom defaults.
- Adds native proof, revision flow, jumpscare variant, network, and automation coverage for the new gameplay paths.

## Classroom Defaults

- Classroom mode is enabled.
- Host force-start is disabled.
- Student admin/session controls are disabled.
- External Google/Microsoft account login and backend sync are disabled.
- Windows hotspot helper is disabled by default because it can require administrator networking permission.
- Live Classroom binds to `127.0.0.1` by default and uses the configured Playit endpoint.
- Windows classroom graphics default to D3D11. The package also includes `Launch-BlackoutHunt-DX11.cmd` and `Launch-BlackoutHunt-DX11-Low.cmd` for older lab machines.
- Local classroom profiles remain available and can be reset from the Account panel.

## Known Limits

- The Playit endpoint depends on the teacher-owned tunnel/agent staying configured and online.
- Online session Host/Find/Join is for local/development validation only until EOS or Steam is configured.
- The current maps are runtime-generated prototype levels rather than authored production maps.
- Unreal still requires a GPU/driver exposing Direct3D feature level 11.0 / Shader Model 5.0; Microsoft Basic Display Adapter, Remote Desktop without hardware acceleration, some VMs, and pre-DX11 GPUs can still fail before the menu.
- The SCP096 prototype jumpscare visual is included for beta classroom testing; broader distribution still needs a final source-asset audit.
- Fab Free Customizable Jumpscares are configured as soft-path variants, but testers need the assets imported at the documented project paths for those exact visuals.
- Native Linux and Fedora/Wine builds remain best-effort compatibility tracks unless the local Unreal install includes the required Linux toolchain.
- Broad physical-device validation remains a follow-up after VM classroom validation.

## Required Beta Validation

- Build editor target.
- Package the Shipping classroom Windows build.
- Confirm package verification passes, including app-local runtime DLL checks and the Windows executable icon.
- Confirm a clean Windows standard user can extract and launch the package without administrator rights or VC++ Redistributable installation.
- Run `BlackoutHunt.Network.NormalizeJoinAddress`, `BlackoutHunt.Automation.CommandLine`, classroom min-player automation, native proof, revision, and jumpscare variant tests.
- Validate host plus two Windows student clients with automation flags.
- Validate default, D3D11, low/windowed, VirtualBox-safe, repeated launch/quit, cold boot, and a 60-minute menu/classroom idle soak.
- Validate Playit join through `blackouthunt.playit.plus:24761` where internet access is available.
- Validate Live Classroom roster names, all-player ready gate, kick flow, question controls, board window, train intermissions, bonus questions, shop flow, and final escape.
- Confirm remote students cannot use admin/session/tunnel controls and can only change local display/audio/profile settings.
- Confirm caught Hall Monitors still contribute to classroom mastery and unlock monitor tools only after the answer-team contribution target.
- Confirm late joiners during Hunt enter as survivor spectators only.
- Confirm the distributed package contains no saved account data, logs, crashes, backend secrets, or debug symbols.

## Feedback

Record tester reports with:

- build version: `0.2.0-beta.5`
- map and mode
- host/client count
- machine specs and graphics preset
- exact join path used: saved Playit endpoint, direct LAN, typed direct IP, or invite code
- lobby names used by host/students
- steps to reproduce
- packaged runtime log when safe to share
