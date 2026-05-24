# Blackout Hunt Tuning

Small gameplay changes now live in:

`D:\MainGame\Config\DefaultGame.ini`

Edit the `[/Script/BlackoutHunt.BHGameSettings]` section for common tuning:

```ini
[/Script/BlackoutHunt.BHGameSettings]
MinPlayers=2
MaxPlayers=12
PrepSeconds=45
HuntSeconds=900
RequiredBreakers=6
bAllowHostForceStart=False
InteractDistance=550.0
CaptureDistance=220.0
FlashlightDrainPerSecond=0.17
ScanCooldownSeconds=25.0
DecoyCooldownSeconds=10.0
BatteryRefillAmount=45.0
```

## What These Control

- `MinPlayers`: players needed before normal ready-up can start the round.
- `MaxPlayers`: join limit.
- `PrepSeconds`: survivor setup time after roles are assigned.
- `HuntSeconds`: hunt phase length.
- `RequiredBreakers`: repaired breakers required before the exit unlocks.
- `bAllowHostForceStart`: enables the host-only `F10` / `ForceStartRound` test shortcut when explicitly turned on for supervised test sessions. Classroom releases keep it disabled.
- `InteractDistance`: range for doors, lockers, breakers, batteries, switches, terminals, and exit.
- `CaptureDistance`: Teacher capture range.
- `FlashlightDrainPerSecond`: battery drain while flashlight is on. `0.17` gives roughly 10 minutes from full charge.
- `ScanCooldownSeconds`: Teacher heartbeat scan cooldown.
- `DecoyCooldownSeconds`: survivor decoy cooldown.
- `BatteryRefillAmount`: flashlight battery refill amount from pickups.

After changing these defaults, rebuild/package to bake them into the distributable build. For editor testing, restarting PIE or the editor is usually enough.
