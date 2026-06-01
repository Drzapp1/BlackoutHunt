# Blackout Hunt

Blackout Hunt is a direct-IP multiplayer horror hunt prototype for Unreal Engine 5.

One player hosts a listen server and friends join with `host-ip-or-domain:7777`. Players can connect across LAN, a game-created hotspot, or the internet when the host is reachable on UDP port `7777`. The current implementation generates its levels at runtime and uses imported CC0 PBR materials for the main environment surfaces and set dressing.

## Beta Release

Current beta target: `0.7.0` Windows classroom build. See `Docs\BETA_RELEASE_NOTES_0.7.0.md` for tester scope, known limits, and validation notes.

## Current Gameplay

- 2-12 players.
- Host can queue one to three Teachers, Survivors, Hall Monitors, or Auto roles in the lobby.
- Survivors answer active station questions with `1-4`, then hold `E` on breakers and solved side-objective stations to unlock the exit.
- Survivors can hide in lockers, drop decoys, pull one-use map alarms, manage stamina, and scavenge batteries that restore flashlight, stamina, and nerve.
- Anyone can toggle room lighting circuits and open central security shutters from terminals.
- The Teacher can capture visible Survivors with `Mouse1`, search occupied lockers with `E`, use a heartbeat scan with `Q`, and trigger a blackout surge with `R` during the hunt phase.
- Caught Survivors can return as Hall Monitors when the round still has active Survivors. Hall Monitors look like the Teacher, cannot capture, place alarm traps with `G`, send real hints with `Q`, and send false hints with `R`. In Physics Classroom, monitors still count toward the 70/50 mastery goals and their tools stay locked until they meet the answer-team contribution target.
- Survivor fear rises when the Teacher is close, when questions are answered wrong, or when scares/scans/cold-calls hit; wrong answers can add a temporary detention mark that makes movement noisier and Teacher scans more dangerous.
- Survivors who camp in one place too long build extra dread, then eventually give the Teacher a restless-breathing noise alert unless they complete a short meaningful movement burst.
- Correct answers clear detention marks, steady nerves, and give a small stamina/flashlight relief boost before the physical task.
- Between stages, the train intermission has optional passenger activities in the social car: snack and drink stations restore stamina, flashlight, and nerves, while reflex arcade and memory-card minigames award small role-appropriate shop points for good timing.
- CCTV cameras can expose Survivor movement to the Teacher as short, imprecise motion pings; holding a security monitor upgrades that to periodic precise locks only while the Teacher stays at the monitor. Survivors can counter cameras by breaking line of sight, going prone, hiding in lockers, or opening the matching security shutter circuit, while Hall Monitors can spoof false CCTV motion.
- The host can switch infection mode, slow/party pacing, Teacher count, and objective intensity from the Escape menu.
- Capturing every Survivor or running out the hunt timer gives the Teacher the win.
- One Survivor escaping gives Survivors the win.

## Menu

The game now opens on a native menu with account login, direct-IP Host/Join, separate host/port joining, a saved classroom join list, Live Classroom, online lobby Host/Find/Join, an internet tunnel helper, local loopback test, game hotspot control, Quit, and Escape-menu support while in a match. The menu also includes a host-only Classroom tab with lesson presets, printable manual question set generation, a live roster/progress board, Physics question focus/complexity controls, and a separate projector window. The console commands remain available as a fallback.
While a hosted game is still in the lobby, the listen-server host can open Escape and queue each connected player as Teacher, Survivor, Hall Monitor, or Auto before starting. Students are prompted for an in-game lobby name before joining if no useful local profile name exists, and that name appears in the host roster. Every connected player must ready up for live classroom play; the host can soft-kick stuck or misjoined human players from the roster. Students see local display/audio/profile controls only; host-only session, classroom, role, tunnel, and admin controls stay on the host machine.

## Performance Modes

- `Low 4GB`: 45 FPS cap, dynamic resolution, small texture pool, lower shadow/reflection cost, no volumetric fog, and software/integrated-GPU friendly renderer settings while preserving the same dark horror tonemapping/AO baseline as higher modes.
- `High 16GB`: 120 FPS cap, 1080p/100% render scale target, larger streaming pool, full-quality meshes/materials, and dynamic resolution only when needed.
- `Ultra`: uncapped, no dynamic resolution, full streaming pool, max view/shadow/post settings, and no intentional graphics restrictions.
- Startup graphics scan local CPU/RAM/GPU information and defaults to the safest matching preset. On a new install, software, unknown, integrated, or low-VRAM GPUs start in `Low 4GB` and apply 1280x720 windowed mode unless the local user already saved a resolution. The Settings panel includes Auto, adaptive on/off, adaptive FPS goal, render scale, texture quality, shadows, effects, persistent resolution, and FPS-limit controls. Adaptive graphics is on by default and adjusts render scale plus high-cost shadow/effects work against the selected FPS goal.

For machines without a dedicated GPU, keep `Auto` enabled or use `Low 4GB` plus the 720p windowed button. Packaged Windows classroom builds default to D3D11 and also include `Launch-BlackoutHunt-DX11.cmd` plus `Launch-BlackoutHunt-DX11-Low.cmd`; the low launcher forces D3D11, 1280x720 windowed mode, and the VirtualBox-safe path for VM or lab-PC validation. If Windows still reports that a D3D11-compatible GPU is required, the machine is not exposing Direct3D feature level 11 / Shader Model 5 to Unreal; update the graphics driver, avoid Remote Desktop, or use a different GPU/VM configuration. Headless server/testing can still use Unreal's `-nullrhi` path when rendering is not needed.

## Controls

- `WASD`: move
- Mouse: look
- Arrow keys: keyboard look/turn fallback
- `Enter`: ready in lobby
- `B`: host classroom board
- `F10`: host-only force-start test shortcut when explicitly enabled
- `F`: flashlight
- `E`: interact / hold repair / exit locker
- `1-4`: answer the active station question you are looking at; Physics Classroom Hall Monitors use this to unlock monitor tools
- `M` or `I`: HUD map
- `V`: cycle reticle style
- `F1-F6`: use owned train/shop powerups
- `Space`: jump / hold to bunny hop
- `Shift`: sprint
- `Left Ctrl`: crouch / sprint roll
- `Left Alt`: prone / sprint slide; combine with `Space` while moving for a dive
- `Mouse1`: Teacher capture
- `Q`: Teacher heartbeat scan / Hall Monitor real hint
- `R`: Teacher blackout / Hall Monitor false hint
- `G`: Survivor decoy / Hall Monitor trap
- `H`: late-spectator team support
- `T`, `Y`, `U`: late-spectator next-round role request for Teacher, Survivor, or Hall Monitor
- `Escape`: menu

The HUD shows a center crosshair and an `E` prompt when you are aiming at a usable interactable.
If an action is blocked by role, phase, distance, or cooldown, the HUD now shows a centered status message explaining why.

## Current Map

Three runtime levels are currently implemented:

- `Facility`: expanded abandoned facility with concrete/plaster/tile materials, denser floor seams, wall trim, grime, hazard striping, pipes, warning signs, clutter, extra lockers, and extra breaker locations.
- `Substation`: larger utility/substation map sized for 12 players, with concrete, plaster, diamond-plate, rusty metal, painted metal, warning placards, transformer lanes, control rooms, perimeter routes, shutters, terminals, batteries, lockers, circuit lighting, and industrial clutter.
- `Foggrounds`: large outdoor nighttime facility perimeter with heavy fog presets, fenced service-road lanes, sheds, generator yards, watch posts, trees, rocks, two exits, and dense 12-player objective coverage.

Imported art assets and licenses are tracked in `Docs\ASSETS.md`.

## Network Commands

The menu is the intended path, but the console still supports:

- Host: `HostGame`
- Host substation: `HostSubstationGame`
- Host foggrounds: `HostFoggroundsGame`
- Join LAN, hotspot, public IP, DNS host, or copied join code: `JoinGame host.example.com:7777` or `JoinGame BH1:...`
- Host via the configured online subsystem: `HostOnlineGame`
- Host substation via the configured online subsystem: `HostOnlineSubstationGame`
- Host foggrounds via the configured online subsystem: `HostOnlineFoggroundsGame`
- Find online sessions/lobbies: `FindOnlineGames`
- Join a found online session by index: `JoinOnlineGame 0`
- Clear the current online session: `DestroyOnlineSession`
- Start a router-free UDP internet tunnel helper: `StartInternetTunnel`
- Open tunnel setup in the browser: `OpenInternetTunnelSetup`
- Stop the tunnel helper if the game launched it: `StopInternetTunnel`
- Create a dedicated Windows game hotspot: `CreateGameHotspot`
- Stop the game hotspot created by this session: `StopGameHotspot`
- Host test start when explicitly enabled: `ForceStartRound`
- Host classroom board: `ToggleClassroomBoard`

Default Unreal listen-server port is UDP `7777`. Direct-IP internet hosting requires the host machine to be reachable on that port, usually through router port forwarding or a directly reachable network. For router-free ad-hoc hosting, use the internet tunnel helper, enter the relay allocation once, then use `COPY JOIN CODE` so players paste a single `BH1:...` value. The online lobby path uses the configured Unreal OnlineSubsystem. The project defaults to `OnlineSubsystemNull` for classroom/local testing; EOS and Steam profiles are packaged separately with `.\Tools\Package-Windows-EOS.ps1` or `.\Tools\Package-Windows-Steam.ps1` after filling the matching local values file. See `Docs\ONLINE_SERVICES.md`.

For no-account online play, use the guest/direct-IP/Playit path instead of EOS or Steam. Package it with `.\Tools\Package-Windows-NoAccount.ps1`; clients launch the same build, choose Guest, paste the host's `BH1:...` join code, and join without any external account. See `Docs\NO_ACCOUNT_ONLINE.md`.

## Account Commands

- Use local guest profile: `AccountGuest`
- Login with Google: `LoginGoogle`
- Login with Microsoft: `LoginMicrosoft`
- Check pending browser login: `AccountPollLogin`
- Sync progress: `AccountSync`
- Sign out: `AccountSignOut`

Account progress is saved locally under `Saved\Account` and can sync through the backend scaffold in `Tools\AccountBackend`. See `Docs\ACCOUNTS.md`.

## Classroom Deployment

The project defaults to a classroom-safe profile: local username/password profiles are enabled, external account login/backend sync are disabled, and session/admin controls are restricted to the listen-server host machine. Students can still play the in-game Teacher role when the host assigns it, but that role does not grant host/admin controls.

Use one of the `LIVE CLASSROOM` map buttons from the Play menu for the school-safe Physics Classroom flow: Facility, Substation, or Foggrounds, selected lesson preset, classroom board, join address, and host roster. Live Classroom binds the host to `127.0.0.1` and uses the configured Playit endpoint by default, which avoids the Windows Firewall public/private networks prompt on locked-down school PCs. Direct LAN remains available through Host LAN or an IT-managed build that sets `bClassroomLoopbackOnlyHost=False` and pre-authorizes UDP `7777`. The host can save/apply lesson presets, generate a local 12-question printable manual set from the selected preset, or adjust question focus, complexity, mastery targets, duration, bots, and scare intensity from the Classroom tab. Force-start stays disabled; the host removes blockers with the roster Kick button.

For live classroom packaging and operation, see `Docs\CLASSROOM_DEPLOYMENT.md`. The primary release command is:

- `.\Tools\Package-Windows-Classroom.ps1`

## Local Helpers

- Build editor target: `.\Tools\Build-Editor.ps1`
- Export authored map seeds from the runtime generator: `.\Tools\Export-AuthoredMaps.ps1 -BuildFirst` (see `Docs\AUTHORED_MAP_CONVERSION.md`)
- Package Windows build: `.\Tools\Package-Windows.ps1`
- Package classroom Windows build: `.\Tools\Package-Windows-Classroom.ps1`
- Package no-account guest online Windows build: `.\Tools\Package-Windows-NoAccount.ps1`
- Create EOS local values template: `.\Tools\New-EOSLocalValues.ps1`
- Package EOS-profile Windows build: `.\Tools\Package-Windows-EOS.ps1`
- Verify EOS-profile Windows package: `.\Tools\Verify-EOSPackage.ps1`
- Package Steam-profile Windows build: `.\Tools\Package-Windows-Steam.ps1`
- Verify Steam-profile Windows package: `.\Tools\Verify-SteamPackage.ps1`
- Package Linux build from Windows: `.\Tools\Package-Linux.ps1`
- Package classroom Linux build groundwork: `.\Tools\Package-Linux-Classroom.ps1`
- Package Linux build from a native Linux UE install: `./Tools/Package-Linux.sh`
- Check UE 5.7 Linux platform files: `./Tools/Check-Unreal-Linux-Platform.sh`
- Install UE 5.7 Linux platform files through Heroic/Legendary: `./Tools/Install-Unreal-Linux-Platform.sh`
- Package Linux build from this Linux machine using the Windows UE install through Wine: `./Tools/Package-Linux-Wine.sh`
- Run the packaged Windows build on Linux through Wine/DXVK: `./Tools/Run-Windows-Build-Wine.sh`
- Create a classroom preflight/support bundle: `.\Tools\New-ClassroomSupportBundle.ps1`

## Tuning

Common gameplay values are now centralized in `Config\DefaultGame.ini` under `[/Script/BlackoutHunt.BHGameSettings]`. See `Docs\TUNING.md`.

Comfort defaults for captions, reduced flash, reduced camera shake, reduced jumpscares, and high-contrast HUD are also in `Config\DefaultGame.ini`; players can override them from the Settings tab.

Facility production criteria are tracked in `Docs\FACILITY_VERTICAL_SLICE.md`. Code split targets are tracked in `Docs\MAINTAINABILITY.md`, with a local snapshot tool at `.\Tools\New-CodeHealthSnapshot.ps1`.

## Packaged Build

The packaged Windows build is written to:

- `Builds\Windows\BlackoutHunt.exe`

The Steam-profile Windows package is written to:

- `Builds\WindowsSteam\BlackoutHunt.exe`

The EOS-profile Windows package is written to:

- `Builds\WindowsEOS\BlackoutHunt.exe`

The packaged Linux build is written to:

- `Builds\Linux`

Linux packaging requires the Unreal Engine Linux target platform files. From Windows, enable the Linux optional component for UE 5.7 in the Epic Games Launcher before running `.\Tools\Package-Linux.ps1`. From this Linux machine, run `./Tools/Check-Unreal-Linux-Platform.sh` to inspect the current UE install, then run `./Tools/Install-Unreal-Linux-Platform.sh` after logging into Epic through Heroic/Legendary, and package with `./Tools/Package-Linux-Wine.sh`. From a native Linux Unreal Engine 5.7 install, run `./Tools/Package-Linux.sh` and set `UE_ROOT` if it is not at `/run/media/adamrosta/T7/UE_5.7`. See `Docs\LINUX_BUILD.md`.

Until the Linux platform files are available, the current Windows package can be run on Linux with Wine and DXVK:

- `./Tools/Run-Windows-Build-Wine.sh`

The top-level `Builds\Windows\BlackoutHunt.exe` is a small launcher stub; the Wine helper runs `Builds\Windows\BlackoutHunt\Binaries\Win64\BlackoutHunt.exe` directly and forces D3D11 through DXVK. The current package was not cooked with Windows Vulkan shader formats, so `-vulkan` is not expected to work until a future Windows package is cooked with `VulkanTargetedShaderFormats`.

Windows runtime logs are written under:

- `Builds\Windows\BlackoutHunt\Saved\Logs`

Linux runtime logs are written under:

- `Builds\Linux\BlackoutHunt\Saved\Logs`

## Twinmotion

Twinmotion is installed at `D:\Twinmotion2026.1`. The project has Datasmith import support enabled for the editor; see `Docs\TWINMOTION.md` for the visual pass workflow.
