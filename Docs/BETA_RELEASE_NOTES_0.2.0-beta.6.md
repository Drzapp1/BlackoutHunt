# Blackout Hunt 0.2.0-beta.6 Release Notes

## Scope

This beta is for classroom testing. The primary package is the Windows classroom build. One teacher machine hosts Live Classroom and student machines join through the owned Playit endpoint:

```text
blackouthunt.playit.plus:24761
```

Public internet matchmaking is still out of scope for the classroom package. The default classroom build uses `OnlineSubsystemNull`; the Steam profile is prepared as a separate package path for development smoke testing.

## Tester Entry Points

- Windows: extract the packaged Windows classroom zip and start `BlackoutHunt.exe`.
- The package includes app-local Windows runtime DLLs, so classroom testers should not need administrator rights or a separate VC++ Redistributable install.
- The teacher uses a `LIVE CLASSROOM` map button for Facility, Substation, or Foggrounds.
- Students enter an in-game lobby name when prompted, then use the saved classroom join list or type `blackouthunt.playit.plus:24761`.
- Live Classroom binds the listen host to `127.0.0.1` by default and publishes the configured Playit endpoint, reducing Windows Firewall prompts on locked-down school PCs.
- Direct LAN remains available through `Host LAN`, normal direct-IP commands, or an IT-managed build with `bClassroomLoopbackOnlyHost=False`.

## Changes Since beta.5

- Ships `0.2.0-beta.6` and supersedes `0.2.0-beta.5`.
- Adds a host-only Classroom Preflight panel with version, endpoint, loopback, tunnel, graphics/RHI, package, log, support-bundle, and deployment-guide actions.
- Adds lesson presets, host-saved classroom setup snapshots, selected-preset launch options, and local printable 12-question manual sets.
- Adds anonymous host-controlled playtest telemetry exports for heatmap-style review of captures, objectives, lockers, CCTV, scares, batteries, and escape routes.
- Expands role warmup, spectator encouragement, spectator role preference, and classroom-safe host controls while keeping student admin/session controls hidden.
- Adds survivor anti-camp pressure, restless-breathing Teacher alerts, prone/crawl state, special movement tuning, and Teacher capture windup/counterplay timing.
- Adds gameplay audio identity cues for footsteps, flashlight clicks, CCTV static, breaker hum/completion, locker knocks, Teacher proximity, and power-loss beats with caption support.
- Expands CCTV/security gameplay with motion reveals, monitor-based precise locks, Hall Monitor spoofing, and the Dead CCTV round modifier.
- Expands the train intermission with snack/drink restore stations, reflex and memory-card activity stations, activity point rewards, shop/economy coverage, and final station tester routes.
- Improves Facility production readability with optional SmartBasicInterfaces runtime mesh props, station signal dressing, route identity, static block batching, breakable glass, and surface patches.
- Adds runtime jumpscare variant discovery/resolution tests and guarded soft-reference playback for optional Fab/Epic jumpscare assets.
- Adds local cosmetic unlock/selection persistence for avatar, shirt color, headwear, and gear choices without changing classroom balance.
- Adds Steam-profile packaging and verification scripts while keeping the classroom package on the safer Null subsystem path.
- Tightens package verification for third-party notices, forbidden source/demo content, saved account data, saved logs, playtest telemetry, and credential-like files.

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
- Online session Host/Find/Join is for local/development validation only until EOS or the Steam profile is configured and tested with owned accounts.
- The current maps are runtime-generated prototype levels rather than authored production maps.
- Unreal still requires a GPU/driver exposing Direct3D feature level 11.0 / Shader Model 5.0; Microsoft Basic Display Adapter, Remote Desktop without hardware acceleration, some VMs, and pre-DX11 GPUs can still fail before the menu.
- The SCP096 prototype jumpscare visual is included for beta classroom testing; broader distribution still needs a final source-asset audit.
- Optional Fab/Epic jumpscare and imported audio visuals depend on locally entitled imported assets; runtime code keeps guarded fallbacks when optional content is absent.
- Native Linux and Fedora/Wine builds remain best-effort compatibility tracks. No Linux beta.6 archive is attached to this prerelease.
- Broad physical-device validation remains a follow-up after local package verification.

## Required Beta Validation

- Build editor target.
- Package the Shipping classroom Windows build.
- Confirm package verification passes, including app-local runtime DLL checks, third-party notices, forbidden staged paths, and cook policy.
- Confirm archive checksum and zip listing integrity.
- Confirm a clean Windows standard user can extract and launch the package without administrator rights or VC++ Redistributable installation.
- Run `BlackoutHunt.Network.NormalizeJoinAddress`, `BlackoutHunt.Automation.CommandLine`, classroom min-player automation, native proof, revision, train economy, lesson preset, jumpscare spawn resolver, and jumpscare variant tests.
- Validate host plus two Windows student clients with automation flags.
- Validate default, D3D11, low/windowed, VirtualBox-safe, repeated launch/quit, cold boot, and a 60-minute menu/classroom idle soak.
- Validate Playit join through `blackouthunt.playit.plus:24761` where internet access is available.
- Validate Live Classroom roster names, all-player ready gate, kick flow, lesson presets, manual question generation, classroom preflight, support bundle, classroom board, train intermissions, activity stations, shop flow, and final escape.
- Confirm remote students cannot use admin/session/tunnel/preflight controls and can only change local display/audio/profile settings.
- Confirm caught Hall Monitors still contribute to classroom mastery and unlock monitor tools only after the answer-team contribution target.
- Confirm late joiners during Hunt enter as survivor spectators only.
- Confirm the distributed package contains no saved account data, logs, crashes, backend secrets, heatmap exports, or debug symbols.

## Attached Packages

- `BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip`
- `BlackoutHunt-0.2.0-beta.6-Windows-Classroom-20260528-035104.zip.sha256`

## Build Notes

- Editor build passed with `.\Tools\Build-Editor.ps1` on 2026-05-28.
- Windows Shipping classroom package passed `.\Tools\Package-Windows-Classroom.ps1` on 2026-05-28.
- Package verification passed for `Builds\Windows`.
- Archive listing passed with `tar -tf`.
- Unreal automation passed with `Automation RunTests BlackoutHunt`: 32 succeeded, 0 failed. Report path: `Saved\Automation\beta6-20260528-035447\Report`.
- Windows package SHA-256: `9de74b70bda6b17693cfd325333d1f41b6765ae569db2b642a9e008b6d0bccf7`.
- Quick stability-gate soak was not completed in this pass. The initial `BlackoutHunt.*` wildcard filter did not match tests in Unreal's command-line runner, so the gate stopped before packaged soaks; the prefix-filter automation rerun above passed.
- Compiler warning noted: Visual Studio 2026 toolchain is newer than Unreal's preferred compiler version.
- Packaging warnings noted: disabled local Marketplace plugin copies were discovered and prioritized over matching engine plugin copies; they remained disabled for the classroom package.
- Packaging warning noted: the `/Game/Free_Jumpscares` redirect still uses deprecated `MatchSubstring`; replace it with `MatchWildcard` in a later cleanup.

## Feedback

Record tester reports with:

- build version: `0.2.0-beta.6`
- map and mode
- host/client count
- machine specs and graphics preset
- exact join path used: saved Playit endpoint, direct LAN, typed direct IP, or invite code
- lobby names used by host/students
- lesson preset or manual question set used
- steps to reproduce
- packaged runtime log when safe to share
