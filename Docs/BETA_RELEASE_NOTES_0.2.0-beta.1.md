# Blackout Hunt 0.2.0-beta.1 Release Notes

## Scope

This beta is for Windows classroom/LAN testing. One teacher or host machine runs a listen server and students join over LAN, direct IP, hotspot, or an optional Playit tunnel fallback on UDP `7777`.

Public internet matchmaking is not part of this beta. The checked-in build still uses `OnlineSubsystemNull`; EOS or Steam lobby/relay setup is deferred to a later beta.

Linux packaging and native Linux validation are deferred.

## Tester Entry Points

- Extract the packaged Windows classroom zip and start `BlackoutHunt.exe`. The package is built with app-local Windows runtime DLLs so classroom testers should not need administrator rights or a separate VC++ Redistributable install for normal launch.
- Use `LIVE CLASSROOM` for the classroom flow.
- Use direct host/join for normal LAN sessions.
- Use Playit only as a host-side tunnel fallback when LAN/direct IP is blocked.

## Classroom Defaults

- Classroom mode is enabled.
- Host force-start is disabled.
- Student admin/session controls are disabled.
- External Google/Microsoft account login and backend sync are disabled.
- Local classroom profiles remain available and can be reset from the Account panel.

## Known Limits

- Online session Host/Find/Join is for local/development validation only until EOS or Steam is configured.
- Internet play without router setup depends on the external Playit tunnel helper.
- Windows hotspot creation can still require administrator rights because that is a Windows networking permission, separate from launching the game.
- The current maps are runtime-generated prototype levels rather than authored production maps.
- The classroom beta excludes unaudited prototype character/jumpscare source assets and uses procedural or CC0 fallback visuals where needed.
- Multiplayer validation still needs to be completed on separate physical machines before broad distribution.

## Required Beta Validation

- Build editor target.
- Package the Shipping classroom build.
- Confirm package verification passes, including the app-local runtime DLL check.
- Confirm a clean Windows standard user can extract and launch the package without a VC++ installer step.
- Run the join-address automation test.
- Validate a host plus at least two clients on separate machines.
- Complete full rounds on Facility, Substation, and Foggrounds.
- Validate Live Classroom roster, ready gate, kick flow, question controls, and board window.
- Confirm remote students cannot use admin/session/tunnel/hotspot controls.
- Check Low 4GB mode, 720p windowed mode, and D3D11 fallback.
- Confirm the distributed package contains no saved account data, logs, crashes, backend secrets, or debug symbols.

## Feedback

Record tester reports with:

- build version: `0.2.0-beta.1`
- map and mode
- host/client count
- machine specs and graphics preset
- exact join path used: LAN, direct IP, hotspot, or Playit
- steps to reproduce
- packaged runtime log when safe to share
