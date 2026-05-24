# Blackout Hunt Setup

## Engine

Install Unreal Engine 5.7 or newer through Epic Games Launcher. Prefer an install on `D:` if the launcher allows it.

After installation, right-click `BlackoutHunt.uproject` and choose **Generate Visual Studio project files**, then open the project in Unreal Editor.

If the project asks to rebuild missing modules, accept. The first launch will compile the C++ gameplay module.

For the current `D:\UE_5.7` install, command-line builds use `-notinstalledengine`; the helper scripts already include that flag.

## Network Test

Host machine:

1. Launch the game/editor.
2. Use the menu to host `Facility`, `Substation`, or `Foggrounds`. Console fallback: `HostGame`, `HostSubstationGame`, or `HostFoggroundsGame`.
3. Allow Windows Firewall inbound access if prompted.

Client machine:

1. Connect to the same LAN, game hotspot, or reachable network.
2. Launch the game/editor.
3. Open the console and run `JoinGame 192.168.x.x:7777`, replacing the IP with the host's local IP.

Internet/direct-IP play:

1. Host as above.
2. Make sure UDP port `7777` reaches the host machine. For most home networks this means forwarding UDP `7777` on the router to the host PC.
3. Clients join with `JoinGame public-ip-or-domain:7777`.

Internet tunnel play, no router setup:

1. On the host, click `START INTERNET TUNNEL` in the menu or run `StartInternetTunnel`.
2. The bundled Playit agent starts on Windows and opens the tunnel setup page. Claim the agent if prompted, then create or select a Custom UDP tunnel with local address `127.0.0.1` and local port `7777`.
3. Host `Facility`, `Substation`, or `Foggrounds` normally.
4. Put the tunnel allocation host and port into the menu's `HOST / IP / CODE` and `PORT` fields, then click `COPY JOIN CODE`.
5. Send that `BH1:...` code to players. They paste it into `HOST / IP / CODE` and click `JOIN GAME`. The console also accepts `JoinGame BH1:...`.
6. Use `STOP INTERNET TUNNEL` or `StopInternetTunnel` when finished if the game launched the agent.

Online lobby play:

1. Use `HOST ONLINE FACILITY`, `HOST ONLINE SUBSTATION`, or `HOST ONLINE FOGGROUNDS` in the menu. Console fallbacks: `HostOnlineGame`, `HostOnlineSubstationGame`, or `HostOnlineFoggroundsGame`.
2. Clients use `FIND ONLINE SESSIONS`, then `JOIN FIRST ONLINE SESSION`. Console fallbacks: `FindOnlineGames`, then `JoinOnlineGame 0`.
3. The project defaults to `OnlineSubsystemNull`, which is useful for local/dev validation. For internet NAT traversal or relay, configure EOS or Steam as described in `D:\MainGame\Docs\ONLINE_SERVICES.md`.

Game hotspot fallback:

1. If a LAN or public Wi-Fi blocks peer-to-peer traffic between clients, click `CREATE GAME HOTSPOT` in the menu or run `CreateGameHotspot`.
2. Windows creates a dedicated `BlackoutHunt-*` SSID with a generated passphrase when the Wi-Fi adapter and permissions allow it. Running as administrator may be required.
3. Other players connect to that SSID, then join the host address shown in the menu on port `7777`.
4. Use `STOP GAME HOTSPOT` or `StopGameHotspot` when finished.

The runtime currently generates three large procedural levels in the default engine entry map, so no hand-authored `.umap` is required for the first compile.

The packaged game now starts on a menu. Use Host or Join there first; the console commands remain available as fallback.

Gameplay actions are role/phase gated:

- In lobby, press `Enter` on every connected player to assign roles and start prep.
- The listen-server host can open Escape in the lobby and queue players as Hunter, Survivor, Fake, or Auto before the round starts.
- The Escape menu also lets players cycle avatar shape/color, vote or set the next level, vote Foggrounds fog intensity, let the host override fog presets for the next round, change Hunter count, objective intensity, infection mode, slow/party pacing, graphics preset, resolution, and FPS cap.
- Survivors can hide during prep after roles are assigned.
- Breaker repair, side-objective stations, panic alarms, hunter heartbeat scan, hunter blackout, fake seeker traps/hints, hunter capture, and exit gameplay unlock when Hunt begins.
- Each round now randomizes the active breaker route, some starting door states, and some powered light circuits. The HUD and Escape menu show the round seed and current objective text.
- Loud repair, station work, sprint, scan, alarm, trap, and decoy events ping the Hunter with directional noise. The round director can also trigger short local scare cues near Survivors during Hunt.
- Survivor stamina limits chase duration. Fear rises near the Hunter or after scans/scares and slows stamina recovery. Batteries now restore flashlight charge, stamina, and nerve.
- For quick local testing, the listen-server host can press `F10`, use `ForceStartRound`, or use the menu's Host Start Test Round button to skip directly to Hunt.

Packaged executable:

`D:\MainGame\Builds\Windows\BlackoutHunt.exe`

## Next Editor Pass

Once Unreal Editor opens successfully:

1. Create a real `Content/Maps/L_Facility.umap`.
2. Use Twinmotion plus Datasmith for the first authored facility art pass. See `D:\MainGame\Docs\TWINMOTION.md`.
3. Move the runtime layout into the map as authored geometry or Blueprints.
4. Replace the current procedural hums/decoy tones with authored free sounds for doors, breaker repair, heartbeat scan, decoy pulses, and capture feedback.
