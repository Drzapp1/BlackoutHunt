# Online Services

Blackout Hunt now has two network paths:

- Direct IP: `HostGame` / `JoinGame host:7777`. This still needs UDP `7777` to reach the host.
- Online sessions: `HostOnlineGame`, `HostOnlineSubstationGame`, `FindOnlineGames`, and `JoinOnlineGame <index>`. This uses Unreal's configured OnlineSubsystem.
- Internet tunnel fallback: `StartInternetTunnel` verifies and launches the bundled Playit agent on Windows, writes the agent log path, and opens tunnel setup. Create a Custom UDP tunnel to `127.0.0.1:7777`, then copy a `BH1:...` join code from the menu or let clients join the allocation address through the normal direct-IP path. The beta.6 classroom package also ships the owned endpoint `blackouthunt.playit.plus:24761` in the saved join list.

The checked-in default is:

```ini
[OnlineSubsystem]
DefaultPlatformService=Null
```

`OnlineSubsystemNull` is for local and development validation. It does not provide real internet relay. For internet play without router setup, switch the subsystem to a provider that supports lobbies and P2P relay/NAT traversal, then keep using the same game menu buttons.

The tunnel fallback exists for ad-hoc play when EOS or Steam is not configured yet. It avoids router forwarding and CGNAT problems, but it is an external tunnel service rather than a native game lobby provider. The menu's `COPY JOIN CODE` button wraps the normalized allocation address into a bounded `BH1:...` code so players can paste one value instead of splitting host and port. For a shipped build, EOS or Steam remains the cleaner default because players can use the normal lobby browser instead of sharing tunnel addresses.

In classroom mode, tunnel helpers are host-machine-only. Remote student clients can join sessions, but they cannot start/stop helper processes, destroy online sessions, or run host/admin network setup. The Windows hotspot helper is disabled by default for the classroom beta because it can require administrator networking permission.

The packaged classroom join list is local configuration, not a public matchmaking browser. It can contain stable owned endpoints such as `blackouthunt.playit.plus:24761`; it does not discover arbitrary active lobbies or replace EOS/Steam.

The Windows tunnel helper only launches the bundled `playit.exe` from the packaged output or `ThirdParty/Playit`, never an arbitrary executable from `PATH`. Before launch, the game checks the helper size and SHA-256 hash listed in `ThirdParty/Playit/README.md`. The agent is started with a local log path under `Saved/Logs/BlackoutHuntPlayit.log`; the classroom preflight reads that log for a usable tunnel allocation and reports either `LAN blocked, tunnel ready` or `network setup required`. Do not post that log publicly if it contains account or agent setup details.

Join input is intentionally strict. The game accepts direct hosts, `host:port`, bracketed IPv6, `BH1:...` codes, and `blackouthunt://join/...` invite links. Malformed ports, unsupported URI schemes, path/query fragments, and unsafe host characters are rejected before `ClientTravel`.

## EOS Deployment Notes

Enable `OnlineSubsystemEOS` and `SocketSubsystemEOS`, then configure EOS credentials in `DefaultEngine.ini` or platform-specific config. Do not commit private client secrets for a shipped product.

Minimum shape:

```ini
[OnlineSubsystem]
DefaultPlatformService=EOS

[/Script/OnlineSubsystemEOS.EOSSettings]
DefaultArtifactName=BlackoutHunt
bUseEAS=true
bUseEOSConnect=true
+Artifacts=(ArtifactName="BlackoutHunt",ClientId="<client-id>",ClientSecret="<client-secret>",ProductId="<product-id>",SandboxId="<sandbox-id>",DeploymentId="<deployment-id>",ClientEncryptionKey="<encryption-key>")

[/Script/Engine.Engine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOS",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")

[SocketSubsystemEOS]
RelayControl=AllowRelays
```

EOS still needs a valid login flow for the local user before online sessions can be created or joined. Once configured, the existing online-session menu path advertises lobbies and resolves the join address through EOS.

## Steam Deployment Notes

Steam is wired as a separate Windows package profile so classroom builds stay on `OnlineSubsystemNull` plus Playit/direct-IP unless the Steam package script is used.

The value-drop file is:

```ini
; Config\Steam\SteamValues.local.ini
[Steam]
SteamAppId=480
IncludeSteamAppIdTxtForLocalTesting=true
```

Copy `Config\Steam\SteamValues.example.ini` to `Config\Steam\SteamValues.local.ini`, then replace `SteamAppId` with the real numeric Steam App ID when one exists. Keep `IncludeSteamAppIdTxtForLocalTesting=false` for Steam depot/upload packages.

Build a development Steam smoke package with:

```powershell
.\Tools\Package-Windows-Steam.ps1 -Configuration Development
```

Build a real Steam shipping candidate with:

```powershell
.\Tools\Package-Windows-Steam.ps1 -Configuration Shipping
```

`Tools\Package-Windows-Steam.ps1` temporarily appends Steam OnlineSubsystem settings to `Config\DefaultEngine.ini`, sets the compile-time Steam shipping App ID from the values file, archives to `Builds\WindowsSteam`, optionally writes `steam_appid.txt` for local development testing, verifies the package, then restores `DefaultEngine.ini`.

Shipping packages reject App ID `480` unless `-AllowSpacewarShipping` is passed for an explicit smoke test. Steam depot packages must not include `steam_appid.txt`; verify that mode with:

```powershell
.\Tools\Verify-SteamPackage.ps1 -PackageRoot .\Builds\WindowsSteam -ForSteamDepot
```

Steam online lobbies require the Steam client to be running and the signed-in account to own the configured App ID. If Steam is selected but not ready, the game requests Steam auto-login once and asks the player to press Host/Find/Join again after Steam reports ready. The same menu commands remain the entry points: `HostOnlineGame`, `FindOnlineGames`, and `JoinOnlineGame`.
