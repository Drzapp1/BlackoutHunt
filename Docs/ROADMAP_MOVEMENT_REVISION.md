# Roadmap — CS2-style movement + Monitor revision enforcement + HUD counters

Status: PLANNED (design locked with the project owner 2026-06-08). Not yet implemented.

This roadmap covers three requested features. Design decisions in **bold** are owner-confirmed.

---

## Workstream 1 — CS2-style air movement (air-strafe + bhop)

### Goal
Air movement with a **high skill ceiling that is genuinely difficult, like CS2/Source**: you can
**build speed above sprint by syncing mouse + strafe (rewarding), but a hard cap keeps it from being broken**.
**All roles** get it (survivor, Teacher, Monitor) so chase balance is preserved.

### Current state (baseline)
- Standard replicated `UCharacterMovementComponent`. `AirControl=0.16` (weak steering),
  `FallingLateralFriction=0.12` (momentum preserved in air), `JumpZVelocity=395`, `MaxAcceleration=4200`,
  `GroundFriction=7.25`. `BHCharacter.cpp:1691-1699`.
- `MoveForward/MoveRight` (`BHCharacter.cpp:3035-3063`) feed `AddMovementInput` in air too, but UE's default
  air physics give no skill-based speed gain.
- Bhop scaffolding exists: `TryBHopJump` (`:3146`), jump buffer `BHBHopJumpBufferSeconds=0.16` (`:72`),
  `LastBHopJumpInputTime`, `bBHopJumpQueued`, fired from `StartJump`/`Landed`. Today it only *preserves*
  speed — no air-accel, no friction-skip on a clean hop.

### Approach
The correct, network-safe way is a **custom movement component** `UBHCharacterMovementComponent : UCharacterMovementComponent`,
set via `ObjectInitializer.SetDefaultSubobjectClass<...>(CharacterMovementComponentName)` in the `ABHCharacter`
constructor. Override the falling-phase velocity calc to inject the Quake/Source `Accelerate` formula:

1. **Air-strafe accel (the skill mechanic).** In the air, replace default lateral accel with:
   - `wishdir` from strafe/forward input (the classic strafe is: hold one strafe key + look into it).
   - `addspeed = min(wishspeed, AirSpeedCap) - dot(velocity2D, wishdir)`; if `<= 0`, no accel this tick.
   - `accel = min(AirAccelerate * wishspeed * dt, addspeed)`; `velocity2D += accel * wishdir`.
   - The tight **`AirSpeedCap`** (per-tick wish cap) is *the* difficulty knob — low cap = CS2-like, only
     skilled mouse+strafe sync gains speed.
2. **Hard ceiling.** Clamp total horizontal air speed to **`MaxAirSpeed = ~1.6× sprint`** ("faster than
   sprint, rewarding, not broken"). Tunable.
3. **Bhop.** On a frame-perfect landing jump (the existing 0.16s buffer), **skip the ground-friction tick**
   so accumulated air speed carries into the next hop instead of being scrubbed. Manual timing only (no
   auto-bhop) to keep the ceiling high.
4. Keep the special-move system (roll/slide/dive) gating intact — those still suppress air-accel while active.

### Tunables (CVars, so we can tune live without rebuilds)
- `bh.AirAccelerate` (default ~12) — air accel aggressiveness.
- `bh.AirSpeedCap` (default ~30 u/s equiv) — per-tick wish cap; **the skill knob** (lower = harder).
- `bh.MaxAirSpeedMult` (default ~1.6) — hard ceiling as a multiple of role sprint speed.
- `bh.BhopFrictionGraceMs` (default ~16) — window after landing where a buffered jump skips ground friction.
- `bh.AirControl` retune (raise from 0.16) for the redirect feel.

### Risks / mitigations
- **Chase balance:** survivors out-bhopping the Teacher. Mitigated by *everyone* getting it + `MaxAirSpeedMult`
  cap + the difficulty (most players won't master it). Teacher can be given a slightly higher cap later if needed.
- **Network prediction:** must live in the CMC (predicted + server-authoritative) — NOT in Tick — or it desyncs.
  This is why a custom CMC subclass is the foundation, not a velocity poke in `ABHCharacter::Tick`.
- **Anti-cheat:** server re-simulates the same `Accelerate`, so a hacked client can't exceed the cap server-side.

### Phasing
1. Add `UBHCharacterMovementComponent`, wire it into `ABHCharacter` ctor (no behavior change yet). Build green.
2. Implement air-accel + `MaxAirSpeed` cap behind CVars (default off → on). Tune `AirSpeedCap` for the ceiling.
3. Bhop friction-skip on clean landing. Tune buffer.
4. Verify in a listen-server + a remote client (prediction parity); update `docs/MOVEMENT.md`.

---

## Workstream 2 — Monitors must finish revision even if everyone is caught

### Goal
The Teacher must **not** be able to catch everyone and skip straight to the next level without the class doing
revision. **Decision: grace window, then end + large dock.**

### Current state (the exploit)
- Caught survivors become **Monitors (`FakeHunter`)** and are revision participants who answer questions at
  ObjectiveStations to earn *contributions* (`BHGameMode.cpp:1696-1715`, `3523-3526`, `3688-3698`).
- A per-round **contribution target (8–12)** already exists (`GetRevisionMinimumContributionTarget`, `:3506`),
  but only gates *tools/exit*, not round-end.
- **Exploit:** `NotifySurvivorCaptured` (`:1718-1729`) and `TickRoundTimer` (`:14503-14529`) call `EndRound`
  the instant `CountAliveSurvivors()<=0` — **no monitor-revision check**. No penalty exists for unfinished revision.

### Approach (confirmed: grace window, then end + dock)
1. When the last survivor is caught in revision mode, **do NOT end immediately.** Enter an **all-caught grace
   window (~60–90s, CVar)**: survivors are all down, monitors keep answering.
2. End the round early **as soon as monitors hit the contribution target** (don't waste class time), OR when the
   grace window / main round timer expires — whichever first.
3. On end, **large point dock** for any monitor below target (CVar; e.g., **−50% of round-earned question points,
   or a flat large amount** — tune to "large"). Finishing in time = no dock.
4. All `EndRound`-on-all-caught paths route through one server-side predicate
   `CanEndRoundEarlyInRevision()` so the win/loss can't be raced by client timing.

### Anchors
- `BHGameMode.cpp:1718-1729` (NotifySurvivorCaptured), `:14503-14529` (TickRoundTimer all-caught),
  `:14537-14539` (timer-expiry → dock site), new predicate near `:4294-4323` (mirrors `CanUnlockRevisionExit`).
- New replicated GameState field for the grace-window deadline (drives the everyone-visible countdown, WS3).

### Risks / mitigations
- **Threat-free grind:** mitigated by the bounded grace window + the dock (teacher can't tank the hunt for a free
  no-pressure answer session indefinitely).
- **In-flight answers at travel:** ensure the final answer RPC lands before `ResetRoundByTravel` (delay travel a
  beat if an answer is processing).
- **Tunables:** `bh.AllCaughtGraceSeconds`, `bh.MonitorUnfinishedDockPct` / `bh.MonitorUnfinishedDockFlat`.

---

## Workstream 3 — HUD / board counters

### Goal (confirmed)
- **Per-monitor revision progress** (e.g. `Revision 5/8`) → **on the Monitor's own HUD** (top corner), monitors only.
- **The same monitor progress also on the big classroom board** (the host's `B` board) so the room can see it.
- **A countdown until the round ends, visible to EVERYONE, from the start of the round.**

### Current state
- `BHHUD::DrawHUD` corners: top-left = timer/exit/actions (a `T-x:xx` already exists, `:1042`); top-right =
  role/tools. No progress/deadline counter concept. Data available: `BHGS->RemainingTime`,
  `BHGS->RevisionContributionTarget`, `BHPS->RevisionStats.ContributionCount`.
- Classroom board = `ToggleClassroomBoard`/`ShowClassroomBoard` (a separate widget).

### Approach
1. **Monitor HUD counter:** new `DrawTopCornerCounter` (`BHHUD.cpp`, top-right above role readout), gated to
   `FakeHunter`, shows `ContributionCount / RevisionContributionTarget`; turns amber/red as the grace deadline nears.
2. **Classroom board:** add a monitors-progress section (each monitor's `x/target`) to the board widget so the
   host's `B` screen shows the class's revision state.
3. **Everyone-visible countdown:** ensure the round countdown is shown to **all roles from round start** (promote
   the existing `T-x:xx`), and during the all-caught grace window it switches to the **grace deadline** countdown
   (driven by the WS2 replicated deadline field).

### Risks
- Don't leak strategic info: full per-station remaining counts stay monitor/board-only; the global countdown is
  just time (fine for all).
- Top-right overlap with the role readout — stack/shift or use a compact pill.

---

## Suggested order
1. **WS2 + WS3 countdown** first (gameplay-correctness + the everyone-visible timer) — highest pedagogical value,
   lower risk.
2. **WS3 monitor/board counters.**
3. **WS1 movement** last (biggest/most delicate; custom CMC + prediction parity testing).

Each workstream: implement → build green (close the running shell first; LFS-bypass commit) → adversarial review →
playtest in the shell.
