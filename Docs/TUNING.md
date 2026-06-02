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
ReconnectGraceSeconds=120.0
InteractDistance=550.0
CaptureDistance=220.0
FlashlightDrainPerSecond=0.17
BlackoutFlashlightDrainMultiplier=3.0
BlackoutFlashlightStrengthScale=0.15
BlackoutFlashlightWeakenSeconds=6.0
BlackoutFlashlightEffectRadius=2600.0
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
- `ReconnectGraceSeconds`: how long after a student drops mid-round they can rejoin straight back into their role and progress (revision stats, points, powerups, and life state are restored) before falling back to a late-join spectator. Defaults to `120`. Set to `0` to disable mid-round reconnect restore. Reconnect is matched by lobby display name, so a returning student should use the same name.
- `InteractDistance`: range for doors, lockers, breakers, batteries, switches, terminals, and exit.
- `CaptureDistance`: Teacher capture range.
- `FlashlightDrainPerSecond`: battery drain while flashlight is on. `0.17` gives roughly 10 minutes from full charge.
- `BlackoutFlashlightDrainMultiplier`: while caught in a Teacher blackout, a nearby student's flashlight battery drains `FlashlightDrainPerSecond` times this factor. `1.0` disables the battery surge.
- `BlackoutFlashlightStrengthScale`: the beam strength a blackout collapses a nearby student's flashlight toward, violently flickering around it (also kills its ability to stagger the Teacher). `0.15` is a near-dead 15% beam; `1.0` disables the dimming.
- `BlackoutFlashlightWeakenSeconds`: how long the blackout weakening (battery + beam) lasts on a student. Each Blackout Surge upgrade charge adds two seconds.
- `BlackoutFlashlightEffectRadius`: radius (cm) around the Teacher's blackout that weakens students' flashlights (fallback when no lights were killed; otherwise the killed-light spread + margin is used).
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

## Jumpscare Feel

Each jumpscare is described by a `FBHJumpscareVariant` (configured in `JumpscareVariants` or auto-discovered from the Whisper art root). Beyond the existing shake/flash fields, three impact knobs drive the moment of contact:

- `ImpactFOVPunch` (deg, 0–30, default `14`): how far the field of view snaps inward on contact before easing back. `0` disables the FOV punch for that variant.
- `ImpactHitStopSeconds` (0–0.25, default `0.07`): a brief client-local slow-motion "hitstop" on the hit. Only the scared player feels it. Suppressed for players with reduced jumpscares.
- `ImpactRumbleIntensity` (0–1, default `0.85`): gamepad rumble strength on the hit. Suppressed for players with reduced jumpscares.
- `ImpactStinger` (optional sound): an extra audio layer (e.g. a low sub-boom or sharp transient) played alongside the main impact scream. The scream is always pitch-randomized, and on a close-range impact it is also doubled with a pitched-down "roar" layer for low-end weight.

Project-wide audio ducking (optional, on `BHGameSettings`):

- `JumpscareDuckSoundMix`: a `SoundMix` asset pushed for the scare's duration to duck ambient audio under the impact. Leave unset to disable (no SoundMix ships by default — author one with the desired class adjustments to enable ducking).
- `JumpscareDuckSeconds` (0.1–4.0, default `1.4`): how long the duck mix stays pushed.

Comfort scaling still applies to everything above: `bDefaultReducedFlash` quarters the flash and skips the black-blink, `bDefaultReducedCameraShake` quarters the FOV punch / flinch / jitter, and `bDefaultReducedJumpscares` softens the close-up and skips the hitstop and rumble.

### Approach variety and dread

Monster-charge scares now roll one of four approaches with an even mix, never repeating the previous one back-to-back:

- **Head-On** — spawns in view ahead and charges (the classic).
- **Behind** — spawns out of view behind the player; the reveal lands on contact.
- **Already There** — spawns close and already in view, then lunges almost immediately.
- **Ceiling Drop** — spawns roughly overhead and descends onto the player.

The chosen variant is also de-duplicated against the previous scare. Separately, the director occasionally fires a **fake-out** (a faint whisper/footstep behind the player plus a brief light flicker that resolves to nothing) to build dread; it has its own 25s cooldown and does not consume the monster cooldown. These behaviors are code-side and scale with the existing revision scare-intensity setting (intensity `0` disables scares entirely).

## Revision Review Loop (Spaced Repetition)

When a revision participant (Survivor or Hall Monitor) answers a question incorrectly, that exact question is added to a per-player review queue. The next time that player reaches an objective station or the train bonus terminal, the queued question is re-asked first — framed in the HUD as a "SECOND CHANCE" review — and it stays queued until answered correctly. This turns each miss into a corrected, re-tested item instead of a one-off, and it feeds the existing correction/mastery scoring (a corrected review counts toward mastery the same as any other correction).

- The queue is server-authoritative and per-player; it resets with the player's revision stats at round/stage reset.
- It is a *priority*, not a hard gate: round and objective completion are never blocked by a non-empty queue.
- The backlog is capped (currently `8`, the `BHMaxRevisionReviewQueue` constant in `BHPlayerState.cpp`) so a long session re-tests recent misses without the queue growing without bound. Change the constant and rebuild to tune the cap.

## Mastery Scoring — "Demonstrated and Durable"

Mastery is designed so students earn it by genuinely revising, not by guessing. All of the
constants below live in `ABHGameMode::RecordRevisionAnswer` (`BHGameMode.cpp`); change and
rebuild to tune. See `Docs/REVISION_QUALITY_PLAN.md` for the rationale.

- **Per-topic gain on a correct answer** = `15.0 * DiffMult * Headroom`, where `DiffMult` is the
  question's `MasteryWeight` (Easy `1.0` / Medium `1.2` / Hard `1.5`) and
  `Headroom = max(1 - (topicMastery/100) * 0.6, 0.25)`. The headroom term gives **diminishing
  returns** — a topic climbs to ~70% in roughly 6–7 easy correct answers (fewer with harder
  questions) but 90–100% is asymptotic, so a topic can no longer be "capped" by a handful of
  lucky answers. Harder questions move the needle most, which rewards attempting them.
- **Decay on a wrong answer** = `7.0 * MissDiffMult` subtracted from the topic (Easy `1.2` /
  Medium `1.0` / Hard `0.8` — a careless easy miss costs the most). At 25% multiple-choice odds,
  blind guessing trends *negative*; knowing the material still climbs steadily.
- **Review-gated ceiling:** a topic is held at **≤ 80%** while the student still has an
  unresolved missed question in it (an entry in the spaced-repetition queue above). Clearing
  your own mistakes is required to fully master a topic.
- **Overall `MasteryPercent`** is the mean of the **enabled** topics' mastery (`RevisionTopicMask`).
  A class focused on one topic is scored only on that topic. This is the number the exit gate
  and HUD use; the default escape thresholds (class 70% / individual 50%) are unchanged and
  remain configurable per lesson preset (lower them if a class is short on time).
- **Points:** a first-time-correct answer pays full shop points; recovering a previously-missed
  question (a correction) pays **half** points but still grants full mastery — so knowing it the
  first time is the most rewarding, while recovery is still worthwhile.

## Anti-Gaming Answer Locks

To stop spam-guessing and to make students read corrections, a wrong answer now applies a short
**correction hold** before the next answer can be submitted. The hold only delays *input* — the
player is never frozen and can always walk away from the station and return.

- **Objective stations** (`BHObjectiveStation.cpp`): after a wrong answer the station holds
  resubmission for `clamp(3 + 1.5 * (consecutiveWrong - 1), 3, 9)` seconds and **reloads a fresh
  eased question** (the missed one goes to the spaced-repetition queue) so the just-revealed
  answer cannot simply be re-entered. The detention mark also scales with consecutive misses.
- **Train bonus terminals** (`BHTrainBonusQuestionTerminal.cpp`): a flat `4`-second correction
  hold on top of the existing `0.35`s anti-spam cooldown. Bonus answers now build topic and
  overall mastery (and so count toward escape) but do **not** satisfy the team-station
  contribution gate that unlocks Hall Monitor tools.
