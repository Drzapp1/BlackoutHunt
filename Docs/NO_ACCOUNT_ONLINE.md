# No-Account Online Play

Use this path when you want players to join without Steam, Epic, Google, Microsoft, or any other account.

This is not public matchmaking. It is direct Unreal listen-server play through one of these routes:

- LAN/direct IP: clients join `host-or-ip:7777`.
- Playit tunnel: host starts the bundled tunnel helper, then shares a `BH1:...` join code.
- Classroom endpoint: clients join the saved classroom endpoint when that owned tunnel is configured and online.

## Package

```powershell
.\Tools\Package-Windows-NoAccount.ps1 -Configuration Shipping
```

The package is written to:

```text
Builds\Windows\BlackoutHunt.exe
```

## Host

1. Launch `Builds\Windows\BlackoutHunt.exe`.
2. Choose `Guest` if the account panel appears.
3. Use `LIVE CLASSROOM`, or host a normal map and click `START INTERNET TUNNEL` if players are not on the same LAN.
4. If Playit setup opens, create or select a Custom UDP tunnel to `127.0.0.1:7777`.
5. Use `COPY JOIN CODE` and send the `BH1:...` code to players.

## Client

1. Launch the same package.
2. Choose `Guest` if the account panel appears.
3. Paste the `BH1:...` code into `HOST / IP / CODE`.
4. Click `JOIN GAME`.

## EOS Device ID Note

Epic Online Services supports Device ID pseudo-accounts for account-free EOS Game Services login, but the current UE 5.7 legacy `OnlineSubsystemEOS` path in this project does not expose a complete Device ID AutoLogin setup for the existing generic session code. Until that glue is added, EOS packages use Epic account sign-in and this no-account path uses direct-IP/Playit joining.
