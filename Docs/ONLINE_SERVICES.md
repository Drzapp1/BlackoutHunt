# Online Services

Blackout Hunt now has three network paths:

- Direct IP: `HostGame` / `JoinGame host:7777`. This still needs UDP `7777` to reach the host.
- Online sessions: `HostOnlineGame`, `HostOnlineSubstationGame`, `FindOnlineGames`, and `JoinOnlineGame <index>`. This uses Unreal's configured OnlineSubsystem.
- Internet tunnel fallback: `StartInternetTunnel` verifies and launches the bundled Playit agent on Windows, writes the agent log path, and opens tunnel setup. Create a Custom UDP tunnel to `127.0.0.1:7777`, then copy a `BH1:...` join code from the menu or let clients join the allocation address through the normal direct-IP path. The 0.5.0-beta.1 classroom package also ships the owned endpoint `blackouthunt.playit.plus:24761` in the saved join list.

The checked-in default is:

```ini
[OnlineSubsystem]
DefaultPlatformService=Null
```

`OnlineSubsystemNull` is for local and development validation. It does not provide real internet relay. For internet play without router setup, switch the subsystem to a provider that supports lobbies and P2P relay/NAT traversal, then keep using the same game menu buttons.

The tunnel fallback exists for ad-hoc play when EOS or Steam is not configured yet. It avoids router forwarding and CGNAT problems, but it is an external tunnel service rather than a native game lobby provider. The menu's `COPY JOIN CODE` button wraps the normalized allocation address into a bounded `BH1:...` code so players can paste one value instead of splitting host and port. For a shipped build, EOS or Steam remains the cleaner default because players can use the normal lobby browser instead of sharing tunnel addresses.

In classroom mode, tunnel helpers are host-machine-only. Remote student clients can join sessions, but they cannot start/stop helper processes, destroy online sessions, or run host/admin network setup. The Windows hotspot helper is disabled by default for the classroom beta because it can require administrator networking permission.

The packaged classroom join list is local configuration, not a public matchmaking browser. It can contain stable owned endpoints such as `blackouthunt.playit.plus:24761`; it does not discover arbitrary active lobbies or replace EOS/Steam.

## No-Account Guest Path

Use this when clients should not need Steam, Epic, Google, Microsoft, or any other account:

```powershell
.\Tools\Package-Windows-NoAccount.ps1 -Configuration Shipping
```

That wrapper uses the classroom-safe package profile and writes a `NO-ACCOUNT-ONLINE.txt` quickstart into `Builds\Windows`. Hosts use Guest plus Live Classroom/direct host and share a `BH1:...` join code from the Playit/direct-IP path. Clients choose Guest, paste the code, and join. This is the easiest account-free path today, but it is not public matchmaking.

The Windows tunnel helper only launches the bundled `playit.exe` from the packaged output or `ThirdParty/Playit`, never an arbitrary executable from `PATH`. Before launch, the game checks the helper size and SHA-256 hash listed in `ThirdParty/Playit/README.md`. The agent is started with a local log path under `Saved/Logs/BlackoutHuntPlayit.log`; the classroom preflight reads that log for a usable tunnel allocation and reports either `LAN blocked, tunnel ready` or `network setup required`. Do not post that log publicly if it contains account or agent setup details.

Join input is intentionally strict. The game accepts direct hosts, `host:port`, bracketed IPv6, `BH1:...` codes, and `blackouthunt://join/...` invite links. Malformed ports, unsupported URI schemes, path/query fragments, and unsafe host characters are rejected before `ClientTravel`.

## EOS Deployment Notes

EOS is wired as a separate Windows package profile so classroom builds stay on `OnlineSubsystemNull` plus Playit/direct-IP unless the EOS package script is used. This is the preferred no-Steam-fee path for public internet lobby testing.

The value-drop file is:

```ini
; Config\EOS\EOSValues.local.ini
[EOS]
ArtifactName=BlackoutHunt
LoginMode=EpicAccount
ProductId=<product-id>
SandboxId=<sandbox-id>
DeploymentId=<deployment-id>
ClientId=<client-id>
ClientSecret=<client-secret>
ClientEncryptionKey=<client-encryption-key>
UseEAS=true
UseEOSConnect=true
UseEOSRTC=false
EnableOverlay=false
EnableSocialOverlay=false
PreferPersistentAuth=true
CacheDir=BlackoutHuntEOS
```

Run `.\Tools\New-EOSLocalValues.ps1`, or copy `Config\EOS\EOSValues.example.ini` to `Config\EOS\EOSValues.local.ini`, then fill values from the EOS Developer Portal product, sandbox, deployment, and client settings. The helper generates a valid 64-hex-character `ClientEncryptionKey` when one is not provided. `Config\EOS\EOSValues.local.ini` is ignored by git and must not be committed or shipped as a loose file.

You can also initialize the values file through the package script:

```powershell
.\Tools\Package-Windows-EOS.ps1 -InitLocalValues
```

That command creates the local values file only when it is missing. It leaves an existing `Config\EOS\EOSValues.local.ini` unchanged so filled credentials are not accidentally overwritten.

Check the local EOS values without cooking:

```powershell
.\Tools\Package-Windows-EOS.ps1 -ValidateOnly
```

Build a development EOS package with:

```powershell
.\Tools\Package-Windows-EOS.ps1 -Configuration Development
```

Build a shipping EOS package with:

```powershell
.\Tools\Package-Windows-EOS.ps1 -Configuration Shipping
```

`Tools\Package-Windows-EOS.ps1` validates the IDs and encryption key before cooking, temporarily appends EOS OnlineSubsystem settings to `Config\DefaultEngine.ini`, archives to `Builds\WindowsEOS`, verifies that the EOS runtime is staged, checks that local credential files were not copied into the package, then restores `DefaultEngine.ini`.

The generated temporary config shape is:

```ini
[/Script/Engine.Engine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOS",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="BeaconNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

[OnlineSubsystem]
DefaultPlatformService=EOS

[/Script/OnlineSubsystemEOS.EOSSettings]
DefaultArtifactName=BlackoutHunt
bUseEAS=true
bUseEOSConnect=true
bPreferPersistentAuth=true
+Artifacts=(ArtifactName="BlackoutHunt",ClientId="<client-id>",ClientSecret="<client-secret>",ProductId="<product-id>",SandboxId="<sandbox-id>",DeploymentId="<deployment-id>",ClientEncryptionKey="<client-encryption-key>")

[SocketSubsystemEOS]
RelayControl=AllowRelays
```

EOS still needs a valid login flow for the local user before online sessions can be created or joined. The package profile enables persistent Epic Account Services auth by default. When the game reports that EOS sign-in is starting, complete the account prompt, return to the game, then press Host/Find/Join again. Once configured, the existing online-session menu path advertises lobbies and resolves the join address through EOS.

EOS Device ID can support account-free EOS Game Services login in the raw EOS SDK, but this repo currently uses UE 5.7's legacy `OnlineSubsystemEOS` generic session path. That path does not expose a complete Device ID AutoLogin flow for the existing Host/Find/Join code without additional plugin-level glue. Until that work is added, use `LoginMode=EpicAccount` for EOS lobbies and `.\Tools\Package-Windows-NoAccount.ps1` for account-free play.

## Steam Deployment Notes

Steam is wired as a separate Windows package profile so classroom builds stay on `OnlineSubsystemNull` plus Playit/direct-IP unless the Steam package script is used. Prefer EOS when you want public lobbies without Steam Direct/App ID costs.

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
