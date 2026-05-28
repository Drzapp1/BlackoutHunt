# Blackout Hunt Tuning

Small gameplay changes now live in:

`Config\DefaultGame.ini`

Edit the `[/Script/BlackoutHunt.BHGameSettings]` section for common tuning:

```ini
[/Script/BlackoutHunt.BHGameSettings]
MinPlayers=2
MaxPlayers=32
PrepSeconds=45
HuntSeconds=900
RequiredBreakers=6
bAllowHostForceStart=False
InteractDistance=550.0
CaptureDistance=220.0
FlashlightDrainPerSecond=0.17
ScanCooldownSeconds=25.0
DecoyCooldownSeconds=10.0
HunterSprintDrainMultiplierMax=0.85
HunterStaminaRecoveryMultiplier=1.75
TeacherAxeStaminaCost=5.0
TeacherAxeMinStamina=2.0
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
- `MaxPlayers`: join limit. Defaults to `32` so a full class can join one host. Players who join above this limit are returned to the menu with a "class is full" message. Raising it further increases replication load; validate with a higher-count packaged soak before going beyond ~32.
- `PrepSeconds`: survivor setup time after roles are assigned.
- `HuntSeconds`: hunt phase length.
- `RequiredBreakers`: repaired breakers required before the exit unlocks.
- `bAllowHostForceStart`: enables the host-only `F10` / `ForceStartRound` test shortcut when explicitly turned on for supervised test sessions. Classroom releases keep it disabled.
- `InteractDistance`: range for doors, lockers, breakers, batteries, switches, terminals, and exit.
- `CaptureDistance`: Teacher capture range.
- `FlashlightDrainPerSecond`: battery drain while flashlight is on. `0.17` gives roughly 10 minutes from full charge.
- `ScanCooldownSeconds`: Teacher heartbeat scan cooldown.
- `DecoyCooldownSeconds`: survivor decoy cooldown.
- `HunterSprintDrainMultiplierMax`: maximum Teacher sprint stamina drain multiplier after role movement tuning is applied. Existing movement data assets are capped by this value.
- `HunterStaminaRecoveryMultiplier`: Teacher stamina recovery multiplier while grounded and not sprinting.
- `TeacherAxeStaminaCost`: stamina spent when the Teacher starts an axe swing.
- `TeacherAxeMinStamina`: minimum stamina needed before the Teacher can start an axe swing.
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
- `bDefaultReducedFlash`: default local preference for softer flash overlays. When enabled it softens the screen-flash horror cues, the power-out flicker-light strobe bursts, and the jumpscare monster's pulsing eye/core lights (the rapid oscillation is muted toward a steady level, so each effect still reads but is not a strobe hazard).
- `bDefaultReducedCameraShake`: default local preference for softer camera shake and jitter.
- `bDefaultCaptions`: default local preference for cue captions.
- `bDefaultHighContrastHud`: default local preference for stronger HUD text and meter contrast.

After changing these defaults, rebuild/package to bake them into the distributable build. For editor testing, restarting PIE or the editor is usually enough.

## Revision Review Loop (Spaced Repetition)

When a revision participant (Survivor or Hall Monitor) answers a question incorrectly, that exact question is added to a per-player review queue. The next time that player reaches an objective station or the train bonus terminal, the queued question is re-asked first — framed in the HUD as a "SECOND CHANCE" review — and it stays queued until answered correctly. This turns each miss into a corrected, re-tested item instead of a one-off, and it feeds the existing correction/mastery scoring (a corrected review counts toward mastery the same as any other correction).

- The queue is server-authoritative and per-player; it resets with the player's revision stats at round/stage reset.
- It is a *priority*, not a hard gate: round and objective completion are never blocked by a non-empty queue.
- The backlog is capped (currently `8`, the `BHMaxRevisionReviewQueue` constant in `BHPlayerState.cpp`) so a long session re-tests recent misses without the queue growing without bound. Change the constant and rebuild to tune the cap.
