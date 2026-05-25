# Blackout Hunt 0.2.0-beta.2 Release Notes

## Scope

This beta is for Windows classroom/LAN testing. One non-admin teacher machine hosts Live Classroom and student machines join on UDP `7777` by direct LAN when available, with a guided Playit fallback when inbound LAN traffic is blocked.

Public internet matchmaking is not part of this beta. The checked-in build still uses `OnlineSubsystemNull`; EOS or Steam lobby/relay setup is deferred to a later beta.

Fedora/Wine and native Linux are secondary compatibility tracks and do not gate this Windows classroom beta.

## Tester Entry Points

- Extract the packaged Windows classroom zip and start `BlackoutHunt.exe`. The package includes app-local Windows runtime DLLs, so classroom testers should not need administrator rights or a separate VC++ Redistributable install.
- Use `LIVE CLASSROOM` for the classroom flow.
- Students join by direct LAN address first, for example `192.168.1.20:7777`.
- If direct LAN is blocked, the host preflight starts the verified bundled Playit agent and reports `LAN blocked, tunnel ready` when a usable allocation is detected or `network setup required` when teacher setup is still needed.

## Classroom Defaults

- Classroom mode is enabled.
- Host force-start is disabled.
- Student admin/session controls are disabled.
- External Google/Microsoft account login and backend sync are disabled.
- Windows hotspot helper is disabled by default because it can require administrator networking permission.
- Local classroom profiles remain available and can be reset from the Account panel.

## Stability Changes Since beta.1

- Added hidden lifecycle automation flags for packaged Shipping validation: `-BHAutomation=1`, `-BHAutoHost=...`, `-BHAutoJoin=...`, `-BHAutoReady=1`, `-BHAutoQuitSeconds=...`, and `-BHAutomationTag=...`.
- Added stable automation log markers for boot, menu ready, hosting, joining, ready, round start, clean quit, and failure reasons. Packaged Shipping writes these to `Saved\Logs\BlackoutHuntAutomation.log`.
- Added `-BHVirtualBoxSafe` plus automatic VirtualBox GPU detection for low/windowed validation.
- Added graceful clean quit for menu quit, automation quit, and OS window close.
- Added classroom network preflight with LAN-first status and Playit fallback status.

## Known Limits

- Online session Host/Find/Join is for local/development validation only until EOS or Steam is configured.
- Playit fallback may still require the teacher to claim/select a tunnel in the browser before a usable join address exists.
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
- Validate Live Classroom roster, all-player ready gate, kick flow, question controls, and board window.
- Confirm remote students cannot use admin/session/tunnel controls.
- Confirm the distributed package contains no saved account data, logs, crashes, backend secrets, or debug symbols.

## Feedback

Record tester reports with:

- build version: `0.2.0-beta.2`
- map and mode
- host/client count
- machine specs and graphics preset
- exact join path used: LAN, direct IP, or Playit
- steps to reproduce
- packaged runtime log when safe to share
