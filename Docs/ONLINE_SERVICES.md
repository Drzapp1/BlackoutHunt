# Online Services

Blackout Hunt now has two network paths:

- Direct IP: `HostGame` / `JoinGame host:7777`. This still needs UDP `7777` to reach the host.
- Online sessions: `HostOnlineGame`, `HostOnlineSubstationGame`, `FindOnlineGames`, and `JoinOnlineGame <index>`. This uses Unreal's configured OnlineSubsystem.
- Internet tunnel fallback: `StartInternetTunnel` verifies and launches the bundled Playit agent on Windows, writes the agent log path, and opens tunnel setup. Create a Custom UDP tunnel to `127.0.0.1:7777`, then copy a `BH1:...` join code from the menu or let clients join the allocation address through the normal direct-IP path. The beta.4 classroom package also ships the owned endpoint `blackouthunt.playit.plus:24761` in the saved join list.

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

Enable `OnlineSubsystemSteam`, set `DefaultPlatformService=Steam`, add the Steam App ID config, and use Steam's lobby/socket support. The C++ session path is provider-agnostic, so the same `HostOnline*`, `FindOnlineGames`, and `JoinOnlineGame` commands remain the entry points.
