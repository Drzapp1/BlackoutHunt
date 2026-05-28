# Blackout Hunt Setup

## Engine

Install Unreal Engine 5.7 or newer through Epic Games Launcher. Prefer an install on `D:` if the launcher allows it.

After installation, right-click `BlackoutHunt.uproject` and choose **Generate Visual Studio project files**, then open the project in Unreal Editor.

If the project asks to rebuild missing modules, accept. The first launch will compile the C++ gameplay module.

For the current `D:\UE_5.7` install, command-line builds use `-notinstalledengine`; the helper scripts already include that flag.

## Network Test

Host machine:

1. Launch the game/editor.
2. Use the menu to host `LIVE CLASSROOM`, `Facility`, `Substation`, or `Foggrounds`. Console fallback: `HostGame`, `HostSubstationGame`, or `HostFoggroundsGame`.
3. For classroom beta use, use Live Classroom with the configured Playit endpoint. The default classroom host binds to `127.0.0.1`, so it should not require the Windows Firewall public/private networks prompt.

Client machine:

1. Connect to the school internet/LAN as normal.
2. Launch the game/editor.
3. Enter an in-game lobby name if prompted.
4. Use the menu's saved classroom endpoint or type `blackouthunt.playit.plus:24761`.

Direct LAN classroom testing:

1. Use `Host LAN`, normal direct-IP host commands, or set `bClassroomLoopbackOnlyHost=False` in an IT-managed build.
2. Make sure UDP port `7777` is pre-authorized by school IT for the host executable.
3. Clients can then join with `JoinGame 192.168.x.x:7777`, replacing the IP with the host's local IP.

Internet/direct-IP play:

1. Host as above.
2. Make sure UDP port `7777` reaches the host machine. For most home networks this means forwarding UDP `7777` on the router to the host PC.
3. Clients join with `JoinGame public-ip-or-domain:7777`.

Internet tunnel play, no router setup:

1. On the host, click `START INTERNET TUNNEL` in the menu or run `StartInternetTunnel`.
2. The bundled Playit agent starts on Windows only after the game verifies the packaged `playit.exe` hash, then opens the tunnel setup page. Claim the agent if prompted, then create or select a Custom UDP tunnel with local address `127.0.0.1` and local port `7777`.
3. Host `Facility`, `Substation`, or `Foggrounds` normally.
4. For the owned classroom beta tunnel, students can join `blackouthunt.playit.plus:24761` from the saved join list.
5. For a different tunnel allocation, put the tunnel allocation host and port into the menu's `HOST / IP / CODE` and `PORT` fields, then click `COPY JOIN CODE`.
6. Send that `BH1:...` code to players. They paste it into `HOST / IP / CODE` and click `JOIN GAME`. The console also accepts `JoinGame BH1:...`.
7. Use `STOP INTERNET TUNNEL` or `StopInternetTunnel` when finished if the game launched the agent.

No-account online play:

1. Package with `.\Tools\Package-Windows-NoAccount.ps1 -Configuration Shipping`.
2. Host launches `Builds\Windows\BlackoutHunt.exe`, chooses `Guest` if prompted, hosts a map, and starts the internet tunnel if players are not on the same LAN.
3. Host uses `COPY JOIN CODE` and sends the `BH1:...` code to players.
4. Clients launch the same package, choose `Guest`, paste the `BH1:...` code into `HOST / IP / CODE`, and click `JOIN GAME`.
5. This path uses direct-IP/Playit joining, not EOS/Steam public matchmaking.

Online lobby play:

1. Use `HOST ONLINE FACILITY`, `HOST ONLINE SUBSTATION`, or `HOST ONLINE FOGGROUNDS` in the menu. Console fallbacks: `HostOnlineGame`, `HostOnlineSubstationGame`, or `HostOnlineFoggroundsGame`.
2. Clients use `FIND ONLINE SESSIONS`, then `JOIN FIRST ONLINE SESSION`. Console fallbacks: `FindOnlineGames`, then `JoinOnlineGame 0`.
3. The project defaults to `OnlineSubsystemNull`, which is useful for local/dev validation. For EOS internet lobby testing, run `.\Tools\New-EOSLocalValues.ps1`, fill `Config\EOS\EOSValues.local.ini` from the EOS Developer Portal, check it with `.\Tools\Package-Windows-EOS.ps1 -ValidateOnly`, then package with `.\Tools\Package-Windows-EOS.ps1 -Configuration Development`.
4. EOS lobby play uses the Epic account sign-in path by default. If the menu reports that EOS sign-in is starting, complete the browser/account prompt, return to the game, then press the online host/find/join button again.
5. Steam remains available for development smoke tests through `Config\Steam\SteamValues.local.ini` and `.\Tools\Package-Windows-Steam.ps1 -Configuration Development`, but real Steam candidates need a real Steam App ID.

Game hotspot fallback:

The classroom beta disables the Windows hotspot helper by default because it can require administrator networking permission. Prefer normal LAN/Wi-Fi or the Playit tunnel fallback.

The runtime currently generates three large procedural levels in the default engine entry map, so no hand-authored `.umap` is required for the first compile.

The packaged game now starts on a menu. Use Host or Join there first; the console commands remain available as fallback.

Keyboard and mouse controls are listed in the Escape menu's Controls panel. The default bindings are:

- `WASD`, mouse, and arrow keys for movement and look.
- `Enter` ready, `Escape` menu, `B` host classroom board, `M`/`I` HUD map, and `V` reticle style.
- `F` flashlight, `E` interact/hold/locker exit, `1-4` or `Numpad 1-4` answer choices, and `F1-F6` owned train/shop powerups.
- `Space` jump/bunny hop, `Left Shift` sprint, `Left Ctrl` crouch/roll, and `Left Alt` prone/slide/dive setup.
- `Mouse1` Teacher capture, `Q` Teacher scan or Hall Monitor real hint, `R` Teacher blackout or Hall Monitor false marker, and `G` Survivor decoy or Hall Monitor trap.
- `H` late-spectator support and `T`/`Y`/`U` late-spectator next-round role requests for Teacher/Survivor/Hall Monitor.
- `F10` is a host-only test shortcut when force-start is explicitly enabled; live classroom releases keep it disabled by default.

Gameplay actions are role/phase gated:

- In lobby, press `Enter` on every connected player to assign roles and start prep.
- The listen-server host can open Escape in the lobby and queue players as Hunter, Survivor, Fake, or Auto before the round starts.
- The Escape menu lets students cycle avatar shape/color, vote maps, ready up, and change local graphics/resolution/FPS/audio only on their own machine. Host-only session, classroom, role, tunnel, and admin controls stay on the host machine.
- Survivors can hide during prep after roles are assigned.
- Breaker repair, side-objective stations, panic alarms, hunter heartbeat scan, hunter blackout, fake seeker traps/hints, hunter capture, and exit gameplay unlock when Hunt begins. In Physics Classroom, Hall Monitors still count toward the 70/50 mastery goals, and their traps/hints also require the student to meet the answer-team contribution target.
- Each round now randomizes the active breaker route, some starting door states, and some powered light circuits. The HUD and Escape menu show the round seed and current objective text.
- Loud repair, station work, sprint, scan, alarm, trap, and decoy events ping the Hunter with directional noise. The round director can also trigger short local scare cues near Survivors during Hunt.
- Survivor stamina limits chase duration. Fear rises near the Hunter or after scans/scares and slows stamina recovery. Batteries now restore flashlight charge, stamina, and nerve.
- For quick local testing, the listen-server host can press `F10`, use `ForceStartRound`, or use the menu's Host Start Test Round button to skip directly to Hunt.

Jumpscare variant testing:

- Start a Test Round from the menu, or use a host-admin listen server.
- Press `Escape`, open the `Round` tab, then use the expanded `Test Commands` panel. It is scrollable and has collapsible groups for jumpscare variants, atmosphere probes, and round/train commands.
- Under `Jumpscare Variants`, use `All Jumpscare Variants` to queue every variant, or click an individual entry such as `ProxyRed`, `ProxyCyan`, `FabMonster01`, `FabMonster02`, or `FabMonster03`.
- Under `Atmosphere Probes`, click ambient, monster charge, blackout, CCTV, footstep noise, or bot memory checks without opening the console.
- If the Fab assets have not been imported to the configured `/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares` paths, the commands still fire and fall back to procedural proxy visuals where needed.

Packaged executable:

`Builds\Windows\BlackoutHunt.exe`

Steam-profile Windows packages are written to:

`Builds\WindowsSteam\BlackoutHunt.exe`

EOS-profile Windows packages are written to:

`Builds\WindowsEOS\BlackoutHunt.exe`

Classroom Windows packages include app-local runtime DLLs beside the root launcher and Shipping executable. A clean standard Windows user should be able to extract the zip and launch the game without administrator rights or a separate VC++ Redistributable install.

Classroom packages default to D3D11 for broader school-PC compatibility. On first launch, Auto graphics sends software, unknown, integrated, or low-VRAM GPUs to `Low 4GB` and 1280x720 windowed mode unless the local user already saved a resolution. If the normal launcher fails on graphics startup, try `Launch-BlackoutHunt-DX11.cmd` or `Launch-BlackoutHunt-DX11-Low.cmd` from the package root; the low launcher forces D3D11, 1280x720 windowed mode, and the VirtualBox-safe path. If Windows still says a D3D11-compatible GPU is required, install the real GPU driver and make sure the machine exposes Direct3D feature level 11.0 / Shader Model 5.0; Microsoft Basic Display Adapter, Remote Desktop, some VirtualBox setups, and very old GPUs are not enough for this UE5 package.

Validation automation flags are hidden and inert unless `-BHAutomation=1` is present. Supported flags are `-BHAutoHost=LiveClassroom|LiveFacility|LiveSubstation|LiveFoggrounds|Facility|Substation|Foggrounds`, `-BHAutoJoin=<host:port-or-code>`, `-BHAutoReady=1`, `-BHAutoMinPlayers=<count>`, `-BHAutoQuitSeconds=<seconds>`, `-BHAutomationTag=<id>`, and `-BHVirtualBoxSafe`. Packaged Shipping writes markers to `Saved\Logs\BlackoutHuntAutomation.log`.

Packaged classroom smoke test:

```powershell
.\Tools\Run-PackagedClassroomSmoke.ps1
```

The smoke test uses the existing package under `Builds\Windows`, starts one Live Classroom host plus two local clients on `127.0.0.1:7777`, auto-readies all three players, waits for `HOST_LISTENING`, `JOINED`, `READY_SET`, `ROUND_STARTED`, and `CLEAN_QUIT` markers, scans the run logs for severe runtime failures, and then cleans up the launched processes. It writes isolated logs and `SMOKE_REPORT.md` under `Saved\PackagedClassroomSmoke\<timestamp>` using `-UserDir`, so the distributable package is not polluted with saved logs.

Expected console output includes the package root, executable path, host/client process IDs, log paths, marker log paths, and a final pass/fail line. If the package is missing, the script fails before launch and tells you to run:

```powershell
.\Tools\Package-Windows-Classroom.ps1
```

Runtime log validation is also available as a standalone gate:

```powershell
.\Tools\Test-RuntimeLogs.ps1 -Path .\Builds\Windows\BlackoutHunt\Saved\Logs
```

For an automation run with a known tag, require the expected markers:

```powershell
.\Tools\Test-RuntimeLogs.ps1 -Path .\Saved\Logs\BlackoutHuntAutomation.log -ExpectAutomationMarkers -AutomationTag classroom-smoke -RequiredAutomationMarker BH_AUTOMATION_BOOT,HOST_LISTENING,ROUND_STARTED,CLEAN_QUIT
```

Failures are reported as `[rule] path:line: first matching line` and cover crashes, ensures, missing DLLs, severe asset/map load failures, net/travel failures, repeated soft-load spam, automation failure markers, and missing required automation markers. Use `-AllowedFindingPattern` only for reviewed developer exceptions; do not use it for classroom release validation.

## Next Editor Pass

Once Unreal Editor opens successfully:

1. Create a real `Content/Maps/L_Facility.umap`.
2. Use Twinmotion plus Datasmith for the first authored facility art pass. See `Docs\TWINMOTION.md`.
3. Move the runtime layout into the map as authored geometry or Blueprints.
4. Replace the current procedural hums/decoy tones with authored free sounds for doors, breaker repair, heartbeat scan, decoy pulses, and capture feedback.
