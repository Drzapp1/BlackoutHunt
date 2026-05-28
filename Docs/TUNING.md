# Blackout Hunt Tuning

Small gameplay changes now live in:

`Config\DefaultGame.ini`

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
AntiCampGraceSeconds=60.0
AntiCampWarningSeconds=50.0
AntiCampRequiredMoveSeconds=5.0
AntiCampRequiredMoveDistance=650.0
AntiCampMovementSpeedThreshold=150.0
AntiCampPressureDreadPerSecond=2.6
AntiCampPressureFearPerSecond=0.55
AntiCampAlertDelaySeconds=18.0
AntiCampAlertCooldownSeconds=18.0
bDefaultReducedJumpscares=False
bDefaultReducedFlash=False
bDefaultReducedCameraShake=False
bDefaultCaptions=True
bDefaultHighContrastHud=False
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
- `AntiCampGraceSeconds`: seconds a Survivor can go without a completed meaningful movement burst before dread pressure starts.
- `AntiCampWarningSeconds`: seconds before the warning prompt tells a still Survivor to move.
- `AntiCampRequiredMoveSeconds`: continuous movement time required to refresh anti-camp safety.
- `AntiCampRequiredMoveDistance`: actual ground distance required during the movement burst, so tiny shuffles do not count.
- `AntiCampMovementSpeedThreshold`: minimum 2D speed for movement to count toward anti-camp safety.
- `AntiCampPressureDreadPerSecond`: dread added per second after the grace period.
- `AntiCampPressureFearPerSecond`: fear added per second as the hunter-alert threshold approaches.
- `AntiCampAlertDelaySeconds`: extra seconds after dread pressure starts before restless breathing alerts the Teacher.
- `AntiCampAlertCooldownSeconds`: minimum seconds between repeated anti-camp breathing alerts.
- `bDefaultReducedJumpscares`: default local preference for softer close-up scare presentation.
- `bDefaultReducedFlash`: default local preference for softer flash overlays.
- `bDefaultReducedCameraShake`: default local preference for softer camera shake and jitter.
- `bDefaultCaptions`: default local preference for cue captions.
- `bDefaultHighContrastHud`: default local preference for stronger HUD text and meter contrast.

After changing these defaults, rebuild/package to bake them into the distributable build. For editor testing, restarting PIE or the editor is usually enough.
