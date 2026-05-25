# Blackout Hunt 0.2.0-beta.3 Release Notes

## Scope

This beta is for Windows classroom testing. One teacher machine hosts Live Classroom and student machines join on UDP `7777` by direct LAN when available or by the owned Playit endpoint:

```text
blackouthunt.playit.plus:24761
```

Public internet matchmaking is still out of scope. The checked-in build uses `OnlineSubsystemNull`; EOS or Steam lobby/relay setup is deferred to a later beta.

Fedora/Wine and native Linux are secondary compatibility tracks and do not gate this Windows classroom beta.

## Tester Entry Points

- Extract the packaged Windows classroom zip and start `BlackoutHunt.exe`. The package includes app-local Windows runtime DLLs, so classroom testers should not need administrator rights or a separate VC++ Redistributable install.
- The teacher uses `LIVE CLASSROOM`.
- Students enter an in-game lobby name when prompted, then use the saved classroom join list or type `blackouthunt.playit.plus:24761`.
- Direct LAN still works when the teacher IP and UDP `7777` are reachable.
- If a teacher-created Playit tunnel is unavailable, the host preflight reports `network setup required` and points to the agent/setup state.

## Changes Since beta.2

- Ships `0.2.0-beta.3` and supersedes `0.2.0-beta.2`.
- Adds the owned classroom Playit endpoint as the default saved join endpoint.
- Shows the preferred public join address on the Classroom board instead of falling back to a local IP when a public/tunnel endpoint is configured.
- Requires a useful local in-game lobby name before joining, then syncs that name to the replicated player roster.
- Adds a visible `READY FOR ROUND` action for connected students in the menu.
- Hides host-only role, session, classroom, tunnel, and admin controls from remote students.
- Clarifies that graphics presets, 720p, and frame cap controls are local to the current machine.
- Allows late joins during an active round as survivor spectators only until the next lobby.
- Preserves strict join-address parsing for Playit domains, direct IP, DNS hostnames, IPv6, and `BH1:` invite codes.

## Classroom Defaults

- Classroom mode is enabled.
- Host force-start is disabled.
- Student admin/session controls are disabled.
- External Google/Microsoft account login and backend sync are disabled.
- Windows hotspot helper is disabled by default because it can require administrator networking permission.
- Local classroom profiles remain available and can be reset from the Account panel.

## Known Limits

- The Playit endpoint depends on the teacher-owned tunnel/agent staying configured and online.
- Online session Host/Find/Join is for local/development validation only until EOS or Steam is configured.
- The current maps are runtime-generated prototype levels rather than authored production maps.
- The classroom beta excludes unaudited prototype character/jumpscare source assets and uses procedural or CC0 fallback visuals where needed.
- Broad physical-device validation remains a follow-up after VM classroom validation.

## Required Beta Validation

- Build editor target.
- Package the Shipping classroom build.
- Confirm package verification passes, including app-local runtime DLL checks.
- Confirm a clean Windows standard user can extract and launch the package without administrator rights or VC++ Redistributable installation.
- Run `BlackoutHunt.Network.NormalizeJoinAddress` and `BlackoutHunt.Automation.CommandLine`.
- Validate host plus two Windows student clients with automation flags.
- Validate default, D3D11, low/windowed, VirtualBox-safe, repeated launch/quit, cold boot, and a 60-minute menu/classroom idle soak.
- Validate Playit join through `blackouthunt.playit.plus:24761` where internet access is available.
- Validate Live Classroom roster names, all-player ready gate, kick flow, question controls, and board window.
- Confirm remote students cannot use admin/session/tunnel controls and can only change local display/audio/profile settings.
- Confirm late joiners during Hunt enter as survivor spectators only.
- Confirm the distributed package contains no saved account data, logs, crashes, backend secrets, or debug symbols.

## Feedback

Record tester reports with:

- build version: `0.2.0-beta.3`
- map and mode
- host/client count
- machine specs and graphics preset
- exact join path used: saved Playit endpoint, direct LAN, typed direct IP, or invite code
- lobby names used by host/students
- steps to reproduce
- packaged runtime log when safe to share
