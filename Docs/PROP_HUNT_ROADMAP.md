# Prop Hunt — Roadmap & Design Specification

> Status: **living document**. Created 2026-06-09 on branch `feat/boot-menu-aaa`.
> Audience: the developer(s) building this out (you + Claude). Written to be executable: every phase lists
> file-level tasks, CVars, replication fields, edge cases, and acceptance criteria so work can start cold.
>
> This is the full plan to take Prop Hunt from the current **v1 vertical slice** (already in the tree, builds green,
> `BlackoutHunt.PropHunt.Library` passes) to a **fully working, enjoyable, polished** mode for casual multiplayer with
> friends. It is deliberately exhaustive — small additions included — so nothing is lost between sessions.

---

## 0. How to read this document

- **§1–§5** are orientation: the vision, the locked decisions, the current state, the ideal match, the architecture.
- **§6** is the **phase plan** — the order of work (P0…P9), each phase shippable on its own.
- **§7** is the **subsystem reference** — deep specs you consult while implementing a phase.
- **§8–§12** are testing, risks, sequencing, and appendices (full CVar/input/telemetry/asset tables).
- Anything marked **(v1 done)** already exists. **(stub)** exists but is inert/partial. **(new)** is not started.
- Tunables are named `bh.PropHunt*` and listed in the master table in §7.12. Treat every number as a CVar default.

---

## 1. Vision & design pillars

**One-line pitch:** flip BlackoutHunt's hide-and-seek into a *prop hunt* — hiders disguise as the furniture, one (then
more) seeker hunts them through prop-stuffed arenas; a tense hide window, a frantic seek window, laughs all round.

**Design pillars (in priority order):**
1. **Readable & fair.** A round always ends; props are always findable (auto-taunts + reveal pulse); the seeker is
   always punished for flailing (wrong-hit penalty). No degenerate "everyone hides forever" stalls.
2. **Casual fun for a friend group.** Built for 2–10 people messing around (per your call). Forgiving, funny,
   low-stakes. Not a horror experience — **no random jumpscares interrupting a hidden prop** (it doesn't make sense).
3. **Reversible & additive.** Prop Hunt is opt-in (`bh.PropHunt`/`?BHPropHunt=1`). With it off the base game is
   byte-for-byte unchanged. Every prop-hunt code path is gated. We never fork or break the classroom mode.
4. **Reuse the engine we have.** Roles, capture, replication, runtime nav, the low-poly avatar, the disguise actor,
   the HUD canvas, the audio cue bus — all already exist. Prefer wiring over inventing.
5. **Multiplayer-correct first.** Server-authoritative round state and disguise; client-only camera/cosmetics. Solid
   under 10 players with bots filling empties. (Bots that actually *play* prop hunt are a later phase.)

**Explicit non-goals (for now):** ranked/competitive matchmaking; persistent prop-hunt progression economy; a custom
map editor; cross-play; mobile. These can graduate from §11 "Open questions / future" if they ever matter.

---

## 2. Locked decisions (from the design Q&A)

These are settled. The rest of the doc assumes them.

| Topic | Decision |
|---|---|
| Question nodes / objectives | **Removed.** Prop-hunt arenas have **no** question stations, breakers, or escape gates. Props win by *surviving*, not by tasks. |
| Round structure | **Classic blind hide → seek.** A hide window where the seeker is **frozen AND screen-blacked**, then released to seek. Durations are CVars. |
| Maps | **Wire up the existing imported packs** (ContainersHouse warehouse, RuinedCrypt, the horror-template levels) as arenas, stripped of objectives. |
| Camera | **Props can toggle 1st/3rd person**; the seeker stays 1st person. |
| Build priority | **Multiplayer with friends first** (replication-solid, host/lobby toggle, 2–10 players). |
| Seeker kit | **All four:** heartbeat scan/sonar, melee swing to catch, wrong-hit penalty, periodic auto reveal pulse. |
| Taunts | **Both** — forced auto taunts on a shrinking timer **and** optional manual taunts for bonus points (risk/reward). |
| Match & roles | **Rotating starting seeker across best-of-N rounds + infection.** A different player starts as seeker each round; caught props **join the seekers** mid-round; last prop standing wins the round. |
| Atmosphere | **Casual, light.** Keep ambient lighting / flashlight / sound; **no random jumpscares** during prop hunt. Brand-flavoured but playful. |

---

## 3. Current state — the v1 vertical slice (what exists today)

v1 was built to ride the *existing* classroom Hunt round flow with minimal, guarded edits. It proves the core
(disguise + taunt + win-flip) but is **not** yet the standalone hide→seek loop above. Inventory:

**New files (v1 done):**
- `Source/BlackoutHunt/BHPropHuntLibrary.h` — pure, header-only inline helpers (no world/CVar reads), unit-tested:
  - `ResolvePropHuntRound(aliveProps, escapedProps, aliveSeekers, bTimeExpired) → EBHRoundPhase` (whole win table).
  - `TauntIntervalSeconds(elapsedFrac, base, min)` — shrinking taunt cadence.
  - `SeekerMissSlowSeconds(consecutiveMisses, base, perExtra, max)` — **exists but not yet wired** to the capture path.
  - `FallbackProps()` / `FallbackPropForSalt(salt)` — engine basic-shape catalogue for "disguise with nothing in view".
- `Source/BlackoutHunt/BHPropHuntLibraryTests.cpp` — automation test `BlackoutHunt.PropHunt.Library` (passing).
- `Source/BlackoutHunt/BHGameModePropHunt.cpp` — a new `ABHGameMode` partial: the `bh.PropHunt*` CVars, the free
  accessor `BHIsPropHuntCVarEnabled()`, `IsPropHuntMode()`, `CountPropHuntProps()`, `RefreshPropHuntGameState()`,
  `BeginPropHuntHunt()`, `TickPropHunt()`, `ForcePropHuntTaunt()`.

**Touched files (v1 done, all `bPropHuntMode`/`IsPropHuntProp()`-gated):**
- `BHGameMode.h/.cpp` — parse `?BHPropHunt=1` / `bh.PropHunt` in `BuildRuntimeFacility` → `bPropHuntMode` (mutually
  exclusive with practice/test/train/revision); push to GameState; `StartPrepPhase`/`StartHuntPhase` hooks; the
  `TickRoundTimer` Hunt-branch per-second `TickPropHunt()` + the **timeout flip** to `SurvivorsWin`; the
  `NotifySurvivorCaptured` `!bPropHuntMode` guard that keeps caught props **out** instead of Hall-Monitor-converted.
- `BHGameState.h/.cpp` — replicated `bPropHuntMode`, `PropHuntPropsRemaining`, `PropHuntPropsTotal`,
  `PropHuntNextTauntServerTime` + `SetPropHuntState(...)`.
- `BHCharacter.h/.cpp` — the disguise: `PropDisguiseMesh` (capsule-anchored, world-fixed, owner-no-see, no collision);
  replicated `bDisguisedAsProp` + mesh/material/scale/yaw (shared `OnRep_PropDisguise`) + `bPropLockedInPlace`
  (`OnRep_PropLockedInPlace`); input handlers `TogglePropDisguise` (Z), `TogglePropLockInPlace` (MMB),
  `RotatePropLeft/Right` (`[` `]`); server RPCs `ServerSetPropDisguise/ServerSetPropLocked/ServerRotateProp`
  with server-side asset validation + scale clamp; `ApplyPropDisguiseVisuals()` (hides `RoleModelRoot`, shows the prop,
  mesh-bounds floor alignment) and the un-disguise restore via `ApplyAvatarStyle()`; `ClearPropDisguiseAuthority()`
  wired into `MarkCaptured`/`MarkEscaped`/`ResetRoleWarmupStateForRoundStart`.
- `BHHUD.h/.cpp` — `DrawPropHuntOverlay()` (top-centre "PROP HUNT", props-left X/Y, taunt countdown, role prompt),
  gated on `GameState->bPropHuntMode`.
- `Config/DefaultInput.ini` — `PropDisguise=Z`, `PropLock=MiddleMouseButton`, `PropRotateLeft=[`, `PropRotateRight=]`.

**What v1 already does well:** Seeker=Hunter, Props=Survivors; disguise-as-looked-at-prop with capsule still
capturable; shrinking auto-taunt; props-left HUD; timeout=props win; caught props stay out; clean replication; no
regressions (full suite: only the 2 pre-existing reds).

**What v1 is missing vs the target (the gap this roadmap closes):**
1. No distinct **Hide phase** (props can disguise any time; seeker is released immediately). **(P1)**
2. No **seeker freeze + screen-black** during hiding. **(P1)**
3. No **rotating starting seeker** and no **infection** (caught→seeker). v1 caught props go straight to spectate. **(P1)**
4. No **best-of-N match** wrapper / role rotation / cumulative scoring / MVP. **(P6)**
5. No **3rd-person prop camera** or 1st/3rd toggle. **(P2)**
6. Runs on the **classroom maps** with their geometry; the imported **prop-rich arenas aren't wired**. **(P3 — done: ContainersHouse + RuinedCrypt wired via the arena loader; editor spawn-bake optional)**
7. Seeker kit is just the raw Hunter kit; no **reveal pulse**, **wrong-hit penalty wiring**, or prop-tuned scan. **(P4)**
8. No **manual taunt** (only auto). **(P5)**
9. No prop-hunt **audio** (taunt horn, found sting, release klaxon, hide-phase bed). **(P5/P7)**
10. **Atmosphere not yet suppressed** — the dread/jumpscare director still ticks; must be silenced for prop hunt. **(P1)**
11. **First-person disguise = no self-view** of your own prop (v1 limitation). Fixed by the 3rd-person cam. **(P2)**
12. Disguise has **no solidity option**, no idle bob, no "peek" — easy to spot a floating, dead-still duplicate. **(P2)**
13. **Bots don't play** prop hunt (bot props just stand as their default body; bot seeker uses Hunter AI). **(P8)**

---

## 4. Target experience — a narrated ideal match

So everyone shares the same mental model of "done":

1. **Lobby (train lobby reused).** Host flips a "Prop Hunt" toggle in the host menu (or `bh.PropHunt 1`). Players see a
   "PROP HUNT — best of 3" banner and a short rules card. Host sets round count, hide/seek seconds, arena rotation.
   Host presses Start.
2. **Travel to Arena 1** (e.g., the ContainersHouse warehouse — packed with crates, barrels, pallets, shelves).
3. **Hide phase (~30s).** Roles assigned: one **starting seeker** (rotates each round), everyone else a **prop**. The
   seeker is teleported to a holding spot, **frozen and screen-blacked** with a ticking countdown ("HIDING: 27…").
   Props scatter, look at furniture, press **Z** to become it, nudge into a corner, press **MMB** to lock dead-still,
   `[`/`]` to face it right. A prop can flip to **3rd person** (toggle key) to check their silhouette, then back to 1st.
4. **Seek phase (~4 min).** A klaxon; the seeker's screen clears; "SEEK!" The seeker prowls with a flashlight, uses the
   **Q heartbeat sonar** to ping nearby props through walls (cooldown), and swings melee to catch one. Swinging at a
   *real* prop or whiffing triggers the **wrong-hit penalty** (brief slow + a clang). Every ~30s (tightening) the
   **auto-taunt** forces every hidden prop to emit a sound at their location; a bold prop can also **manual-taunt** for
   bonus points, deliberately giving away position. Periodically (more often late) a **reveal pulse** briefly outlines
   all props — the fairness valve.
5. **Infection.** When a prop is caught it doesn't just spectate — it **joins the seeker team** (re-coloured, gets the
   seeker kit), so the hunt accelerates. The HUD shows "PROPS LEFT 3/8".
6. **Round end.** Either the **last prop** is caught (seekers win the round) or the timer expires with ≥1 prop alive
   (the surviving props win the round). A scoreboard pops: per-player points (survival time, catches, taunt bonus),
   the round winner, and "MVP". 8-second intermission.
7. **Next round.** The **starting seeker rotates** (round-robin, or "last round's top prop becomes the seeker"). New
   arena (rotation list). Repeat for best-of-N.
8. **Match end.** Final scoreboard, cumulative winner, a victory line, return to lobby.

Everything below makes that real, in order.

---

## 5. Architecture overview

**Where prop-hunt logic lives:** keep growing the `ABHGameMode` partial **`BHGameModePropHunt.cpp`** for round/match
orchestration (server-authoritative), the pure helpers in **`BHPropHuntLibrary.h`**, the per-pawn disguise + camera on
**`ABHCharacter`**, the replicated mode/round state on **`ABHGameState`**, and the readout on **`ABHHUD`**. This keeps
prop hunt a cohesive, mostly-self-contained slice that the rest of the game ignores when `bPropHuntMode == false`.

**The central new concept: a Prop-Hunt round state machine** layered *over* `EBHRoundPhase`, because the base
`EBHRoundPhase` (Lobby/Prep/Hunt/…) doesn't have a "hide" sub-phase. Two options, pick in P1:
- **(A) New replicated `EBHPropHuntPhase`** {None, Hiding, Seeking, RoundEnd} on `ABHGameState`, advanced by the
  GameMode, with `EBHRoundPhase` held at `Hunt` the whole time. **Preferred** — minimal blast radius, the base enum and
  all its consumers (HUD phase text, win checks) keep working; prop-hunt HUD/logic reads the sub-phase.
- (B) Extend `EBHRoundPhase` with `PropHide`/`PropSeek`. Rejected: touches every switch on the enum (HUD, banners,
  travel, tests) — violates pillar 3.

So: **`EBHPropHuntPhase` is the spine of P1.** The GameMode owns the timers; the GameState replicates the phase +
deadlines; the character and HUD react.

**Authority model (unchanged, restated):** all round/role/disguise *state* is set on the server and replicated; the
*camera*, *screen-black*, *outlines*, *audio*, and *HUD* are client-local reactions to replicated state. Inputs go
client→`Server*`RPC→`*Authority` with `HasAuthority()` + `IsPropHuntProp()`/role gates, exactly like the rest of the game.

---

## 6. Phase plan

Each phase is independently shippable and leaves the build green + tests passing. Rough sizing: **S** ≈ ½ day, **M** ≈
1–2 days, **L** ≈ 3–5 days (solo + Claude). Order is dependency-driven.

### P0 — Foundations & decoupling (S)  *(do first)*
**Goal:** make prop hunt a clean standalone mode that ignores classroom scaffolding, before adding the new loop.
- **Strip objectives in prop-hunt rounds.** In the procedural builders / authored-level path, when `bPropHuntMode`,
  skip spawning `ABHObjectiveStation`, `ABHBreaker`, `ABHExitGate`, `ABHEscapeStationManager`, question terminals.
  Concretely: guard the relevant `Spawn*`/registration calls in `BuildFacilityLevel`/`BuildBackroomsFacility`/etc., or
  (cleaner) add an early `if (bPropHuntMode) { BuildPropHuntArena(); return; }` dispatch in `BuildRuntimeFacility` that
  builds only geometry + spawns + nav (no objective actors). This is also the seam P3's arena loader plugs into.
- **Silence the atmosphere/jumpscare director for prop hunt.** Gate the dread/presence/monster-charge director so it
  does nothing when `bPropHuntMode`: early-return in `TickDirector`/`TriggerScareEvent`/`UpdatePresenceDirector`/
  `TriggerMonsterChargeJumpscare` on `bPropHuntMode`, OR simply don't `StartDirectorTimer()` for prop hunt. Keep
  ambient emitters + flickers (mood) but kill scripted scares. (Pillar 2; your "jumpscaring a prop makes no sense".)
- **Disable exit-unlock / objective HUD lines** under prop hunt (the prop-hunt overlay already replaces them; ensure
  `BuildHudActionLine`/`DetailLine`/`AuxLine` return empty or are skipped when `bPropHuntMode`).
- **Round-timer guard:** confirm `CountAliveSurvivors()`/`CountEscapedSurvivors()` resolution still behaves once
  objectives are gone (it does — props win on timeout; all-caught = seeker win). Add a regression test.
- **Acceptance:** start `bh.PropHunt 1` on Facility → no stations/breakers/exits spawn, no jumpscares fire, the
  prop-hunt overlay shows, a round resolves on timeout (props win) and on all-caught (seeker win). Build + suite green.

### P1 — Round state machine: Hide → Seek, rotating seeker, infection (L)  *(the heart)*
**Goal:** replace "ride the classroom Hunt" with the real prop-hunt round loop.
- **Add `EBHPropHuntPhase` {None, Hiding, Seeking, RoundEnd}** to `BHTypes.h`; replicate on `ABHGameState`
  (`PropHuntPhase`, `PropHuntPhaseEndServerTime`) via `SetPropHuntPhase(phase, endTime)`.
- **GameMode round driver** in `BHGameModePropHunt.cpp`:
  - On round start: assign **1 starting seeker** (the rotation pick — §7.2) + the rest props; enter **Hiding**, set
    `PropHuntPhaseEndServerTime = now + bh.PropHuntHideSeconds` (default 30).
  - Hide phase: **freeze + isolate the seeker** — teleport to a `HunterSpawn`/holding volume, `DisableMovement`,
    `bCaptureDisabled = true` (reuse `ABHGameState::bHunterInputFrozen`), and tell the client to **black the screen**
    (new replicated flag or reuse the cutscene state). Props are free to move/disguise (capture is off globally).
  - On hide timer end: enter **Seeking**, release the seeker (un-freeze, un-black, klaxon), set
    `PropHuntPhaseEndServerTime = now + bh.PropHuntSeekSeconds` (default 240), enable capture, start the taunt +
    reveal-pulse cadences (driven from `TickPropHunt`, keyed off `PropHuntPhase == Seeking`).
  - Seeking resolution each `TickRoundTimer` second: `BHPropHunt::ResolvePropHuntRound(...)` (already exists) →
    enter **RoundEnd** with the result. All-caught → seekers win; timer 0 with props alive → props win.
- **Infection (caught → seeker).** Replace the v1 "caught prop spectates" with: on catch, convert the prop's
  `PlayerRole` to `Hunter` (seeker), re-`RestartPlayer`, drop their disguise, give them the seeker kit, broadcast
  "X was found and joined the hunt!". Keep counting **props remaining = alive Survivors** (so the win math is
  unchanged). The last alive prop is the winner. (This is the "let caught props join hunters" you chose, replacing the
  v1 `bCanReturnAsFakeHunter` suppression with a deliberate seeker conversion — gated to prop hunt.)
- **Rotating starting seeker.** Track per-match seeker history; pick the next starting seeker each round (§7.2). Store
  on the GameMode (and persist across the round travel like the train economy does).
- **HUD:** phase-aware overlay — big "HIDING 0:27" with a hide hint for props / "you'll be released in…" for the
  seeker; then "SEEK! 3:58" + props-left. Screen-black overlay for the frozen seeker.
- **Acceptance:** full round: hide (seeker blind/frozen, props disguise) → seek (released, hunts) → a prop caught
  becomes a seeker → last prop or timer ends the round → RoundEnd state. Works listen-server + 1 remote client.

### P2 — 3rd-person camera + prop-pawn polish (M)
**Goal:** props can see and sell their disguise; the prop *feels* like a prop.
- **Camera:** add `USpringArmComponent PropCameraBoom` to `ABHCharacter` (capsule-attached, `bUsePawnControlRotation`,
  arm length `bh.PropHunt3pDistance` ≈ 300, probe collision on, slight lag). A **client-local** `bPropThirdPerson`
  flips the active view: 1st = `Camera` on the capsule at eye height (current); 3rd = `Camera` attached to the boom
  socket (orbit). Toggle action **`PropCameraToggle`** (new input — §7.13). When 3rd-person, set
  `PropDisguiseMesh` owner-**see** (so the owner sees their own prop); when 1st-person, owner-**no-see** (so it doesn't
  fill the screen). Seeker stays 1st-person; the toggle is inert for non-props. Default props to 3rd person on disguise.
- **Prop solidity (optional collision).** Add a CVar `bh.PropHuntSolidProps` (default on): when a prop locks, give the
  disguise a light blocking collision matched to its bounds so the seeker can bump it (and the prop can't overlap into
  walls). Off = pure cosmetic (current). Keep the *capsule* as the capture target regardless.
- **Idle life.** A tiny breathing bob / sway on an unlocked disguise (purely cosmetic, client-side, scaled by movement)
  so a walking prop reads as alive; a locked prop is perfectly still. Helps the seeker tell "moved recently".
- **Size clamp & validity.** Reject disguising as absurdly large/small meshes (bounds outside `bh.PropHuntMin/MaxSize`)
  and as non-solid/decorative tiny bits; snackable list per map later (§7.9). Show "too big to become" feedback.
- **Self-view affordances:** a faint outline of your own prop in 1st person (so you know where your hitbox is), and a
  "you look like: Crate" label (already partially in the HUD copy).
- **Acceptance:** disguise → see yourself in 3rd person, toggle to 1st and back, lock and the prop is solid & still,
  oversized meshes are refused. No regressions to the seeker's 1st-person.

### P3 — Map import: arena loader + wiring the packs (L) **(done — code complete 2026-06-09; editor steps pending, see TODO box)**
**Goal:** play prop hunt in the prop-rich imported environments, not just the classroom maps.

> **STATUS (what landed):** the full runtime arena path needs NO editor work: `BHPropHunt::FArenaSpec` registry
> (BHPropHuntLibrary.h) → arena names are first-class level tokens (`NormalizeBHLevelName` passthrough) → travel
> resolves them via `BHResolvePropHuntArenaPackage` (authored `Arena_<Name>` bake preferred, raw pack demo fallback)
> → `BuildRuntimeFacility` dispatches `TryBuildPropHuntArena()` for a loaded arena package (also FORCES prop hunt on,
> so `open /Game/ContainersHouseCH/Maps/Map_ContainersHouse_Demo` alone works) → geometry-bounds scan (sky-sphere
> excluded) drives Halton floor-trace spawn scatter (lowest-standable-wins, so roofs don't shadow interiors; placed
> PlayerStarts + a "Hunter"-tagged start always win), bounds-sized runtime nav, perimeter containment, bounds-based
> void-recovery Z. `?BHArena=<name>` implies+routes prop hunt; `?BHPropHunt=1` now rides every travel URL the mode
> builds (URL-option-only sessions used to silently drop the mode on round 2 — fixed). Client-side
> `EnsurePropHuntArenaExposureGuard` clamps eye adaptation on packs that ship no exposure pass (wide 0.03–2.0 band).
> Registered arenas: **ContainersHouse** (demo map in every cook list + Verify-EOSPackage guard), **RuinedCrypt**
> (registry-ready; NOT cooked yet — editor/dev builds only, keeps the package small until playtested).
> Tests: `BlackoutHunt.PropHunt.ArenaTravel` + `.ArenaScatter` (+ registry/Halton/hold-pick rows in `.Library`).
>
> **TODO (needs the interactive editor / a human):**
> 1. *(optional but recommended)* Run `Tools/Setup-PropHuntArena.py` (headless one-liner in its header) to bake
>    `Arena_ContainersHouse`/`Arena_RuinedCrypt` with hand-tunable PlayerStarts; then add the baked map(s) to the
>    cook `-map=` lists (commented pointers already sit in each Package-* script) + `Verify-EOSPackage.ps1`.
> 2. Per-map checklist (§7.9) in the editor: eyeball spawn spread, lighting, collision holes; nudge PlayerStarts.
> 3. Human playtest on ContainersHouse (hide spots, sightlines, round timing) — record findings here.
- **Arena loader (the import mechanism).** Most pack demo maps have geometry + lights + (sometimes) PlayerStarts but
  **no** BlackoutHunt markers/spawns/nav and **no** `ABHLevelMarker`. Add a prop-hunt arena path:
  - Recognise an **arena travel option** `?BHArena=<LogicalName>` (or reuse `?BHLevel=`), resolved to a package via a
    new **arena registry** (a small table: logical name → `/Game/<Pack>/Maps/<Map>` package + spawn strategy + a few
    tuning hints). Travel there with `?BHPropHunt=1`.
  - In `BuildRuntimeFacility`, when `bPropHuntMode` **and** the loaded level is a registered arena (not a procedural
    BlackoutHunt level), **skip generation** and instead `BuildPropHuntArena()`:
    1. **Spawns:** collect existing `APlayerStart`s; if too few, **scatter spawns on the navmesh** (sample reachable
       points spread across the bounds — reuse the bot patrol-point sampler `GetRandomBotPatrolPoint`/nav projection).
       Pick one as the seeker holding spot (farthest-from-centroid or a dedicated tagged start).
    2. **Navmesh:** build a **runtime nav volume** over the level bounds (the game already does this via
       `BuildRuntimeNavigation`/`RuntimeNavBounds`; size it to the level's geometric bounds).
    3. **Containment & void recovery:** add kill-Z / boundary blockers (`AddMapContainment`) sized to the level so a
       prop can't escape the playable area; register void recovery.
    4. **Lighting/mood:** light arenas may need a runtime exposure clamp (reuse `ClampIndoorAutoExposure`) or a fill
       light if pitch black; dark arenas are fine (seeker has a flashlight).
  - This makes "import a map" = **add a row to the arena registry + (optionally) place a couple of tagged spawns** in
    the editor. No re-authoring of the pack.
- **First arenas to wire (priority order):**
  1. `ContainersHouseCH` → `Content/ContainersHouseCH/Maps/Map_ContainersHouse_Demo` (or `_Overview`). Warehouse,
     crates/barrels/shelves — *the* prop-hunt arena. **Do this one first as the proof.**
  2. `RuinedCrypt` → `Content/RuinedCrypt/Demo/Maps/RuinedCrypt_01/RuinedCrypt_01_P` — atmospheric, lots of debris/urns.
  3. `Horror_Template`/`HorrorTemplate` demo/showcase levels — furnished rooms, good clutter.
  4. (Stretch) `BFHorror/Maps/Demo_Level`, `ResidentHorrorV1/Maps/Map_MechanicMap`, `FirstPersonHorrorKit/Levels/MainLevel`.
- **Cooking:** add the chosen arena `.umap`s + their pack content to the cook `-map=`/`DirectoriesToAlwaysCook` lists
  (the same lists the 0.7.1 cook-map-list fix touched) so they ship in packaged builds. Update `Verify-EOSPackage`'s
  map guard.
- **Per-map checklist** (repeat for each arena): §7.9.
- **Acceptance:** `?BHPropHunt=1?BHArena=ContainersHouse` (from console/host) travels into the warehouse, spawns the
  seeker + props on valid nav, a full hide→seek round plays, nobody falls through the world. Cooked build loads it too.

### P4 — Seeker kit (M) **(done — 2026-06-09)**
**Goal:** the four seeker tools you picked, prop-tuned.

> **STATUS:** all four tools live. **Sonar:** Q branches in `UseScanAuthority` → `UsePropHuntSonarAuthority`
> (through-wall ping of every alive prop in `bh.PropHuntScanRadius`=1500 on `bh.PropHuntScanCooldown`=8s; the ping is
> LOUD at the seeker's own spot — info flows both ways; markers delivered via `ClientReceivePropHuntSonar`).
> **Melee catch:** the existing capture swing (unchanged). **Wrong-hit penalty:** wired into BOTH whiff branches of
> `ResolveTeacherCaptureAttackAuthority` (no-target + timed-dodge) via `ApplyPropHuntMissPenaltyAuthority` —
> escalating self-slow (`SeekerMissSlowSeconds` curve, `bh.PropHuntMissSlow*`, factor `bh.PropHuntMissSlowFactor`=0.55,
> owner-only-replicated until-time folded into `RefreshMovementSpeedFromState`), reset on a landed catch.
> **Reveal pulse:** `ForcePropHuntRevealPulse` on the TauntIntervalSeconds lerp (`bh.PropHuntPulseBase`=45 →
> `PulseMin`=12), range-unlimited, all seekers get markers + low-alpha amber flash; props get a warning toast.
> **Seeker HUD:** sonar ready/cooldown line, through-wall red markers with metres (project + fade), pulse flash.
> SFX for clang/horn land in P5's audio pass. Tuning values are first-guess — playtest then record here.
- **Heartbeat scan / sonar (Q).** Reuse `UseScanAuthority`, but in prop hunt make it a **prop sonar**: on use, briefly
  (client cue) reveal disguised props within `bh.PropHuntScanRadius` (through walls) as pulsing outlines/markers, on a
  `bh.PropHuntScanCooldown`. Server picks the targets (alive props in radius), multicasts a reveal cue to the seeker.
- **Melee swing to catch (Mouse1).** Reuse the capture swing. A hit on a prop's capsule within range = catch → infection.
- **Wrong-hit penalty.** Wire the existing `BHPropHunt::SeekerMissSlowSeconds(...)` helper: track consecutive misses on
  the seeker; on a whiff (no prop caught), apply a brief movement slow + a clang SFX + a small screen shake; reset the
  miss counter on a successful catch. Tunables `bh.PropHuntMissSlowBase/PerMiss/Max`. (Also discourages prop-bashing.)
- **Auto reveal pulse.** On a cadence that **tightens as the seek timer runs down** (`bh.PropHuntPulseBase/Min`), the
  server multicasts a short "all props outline" cue to seekers. Guarantees endgame closure even with a passive seeker.
- **Seeker HUD:** scan cooldown ring, "props left", a directional "last taunt heard" arrow (reuse the noise/threat
  arrow), pulse flash.
- **Acceptance:** seeker can ping, catch, gets slowed for flailing, and the pulse reliably surfaces the last prop.

### P5 — Taunts (auto + manual) + first audio pass (M) **(done — 2026-06-09)**
**Goal:** the flush-out mechanic with risk/reward, and the sounds that sell it.

> **STATUS:** **Manual taunt** = `PropTaunt` on **T** (role-gated; spectator T conflict is benign) →
> `ABHGameMode::HandlePropHuntManualTaunt`: alive-prop + Seek-phase + `bh.PropHuntManualTauntCooldown` (8s) checks,
> emits the loud-noise + atmosphere stimulus + a HIGHER-pitched horn (so regulars can tell bold from forced), awards
> `bh.PropHuntTauntBonusPoints` (25) into the new replicated `ABHPlayerState::PropHuntScore`, and **restarts the
> auto-taunt clock** (one bold prop buys everyone a quiet window). HUD: score rides the props-left line; taunt key in
> the prop prompt. **Audio first pass** through `BroadcastGameplayAudioCue` (all paths in BHGameModePropHunt.cpp,
> first-guess SoundsOfHorror stingers — SWAP AFTER AN EAR PASS): release klaxon (global), auto/manual taunt horn
> (spatial 52 m, walk-the-sound-down), found sting (global), wrong-hit clang (spatial at the whiffing seeker — free
> intel for nearby props), reveal-pulse whoosh (seekers only), props/seekers win stings via the idempotent
> `EndRound` seam. Hide-phase bed deferred to P7 (needs loop management).
- **Auto taunt (v1 done, refine).** Keep the shrinking-interval forced taunt; on fire, every alive prop emits a noise
  at their location (already) **and** plays a **taunt horn** SFX audible to the seeker, plus a brief self-outline.
- **Manual taunt (new).** Prop input **`PropTaunt`** (new key): on press, emit the taunt cue at your location and earn
  `bh.PropHuntTauntBonusPoints`; short personal cooldown so it's a deliberate risk. Optional: a small wheel of
  taunt lines/sounds (reuse the emote-wheel UI). Manual taunt resets the *auto* timer slightly (so taunting "pays" the
  fairness debt).
- **Audio (first pass, reuse `SoundsOfHorror` + existing cue bus `BroadcastGameplayAudioCue`):**
  - Hide-phase bed (calm tick), **release klaxon**, taunt horn, **found** sting, wrong-hit clang, round-win/lose stings.
  - All routed through the existing audio-cue multicast so they're networked + positional where relevant.
- **Acceptance:** auto + manual taunts both fire with sound; manual taunt scores + risks; the seeker can localise taunts.

### P6 — Match flow, scoring, MVP, host/lobby UI (L)  *(multiplayer-first payoff)* **(done — 2026-06-09)**
**Goal:** a real best-of-N match with rotation, a scoreboard, and host control — the "with friends" experience.

> **STATUS:** the full loop is live. **Match wrapper:** rounds-played persists in `UBHGameInstance` across the
> per-round ServerTravel (like the train run); per-player `PropHuntScore` (replicated) + `PropHuntTimesSeeker`
> persist via `FBHTravelPlayerProgress`; GameState replicates `PropHuntRoundIndex/Count` + `bPropHuntMatchComplete`.
> Best-of-N = `?BHPropHuntRounds=` (carried on every hop) else `bh.PropHuntRoundCount` (3). **Rotation:** AssignRoles
> branches to `ChoosePropHuntStartingSeekers` — fewest-seeks-first, ties to lowest score (pure helper
> `PickStartingSeekerIndex`, unit-tested); bot-Teacher substitution + FakeHunter assignment are gated OFF in prop
> hunt; `bh.PropHuntSeekers` (1) picks N. **Scoring:** survival trickle (`bh.PropHuntPointsPerSecondAlive`=1/s in
> TickPropHunt), catch (`bh.PropHuntPointsPerCatch`=75), survive-the-timer bonus (`bh.PropHuntSurviveBonus`=100),
> starting-seeker base (`bh.PropHuntSeekerBasePoints`=25), manual-taunt bonus (P5). **Round end** (idempotent
> EndRound seam): round-MVP banner = biggest score DELTA (snapshot at hide start), round counter++, arena rotation
> via `?BHMapRoute=` (arena names are route tokens), and after the final round: champion broadcast + FINAL STANDINGS
> board + travel BACK TO THE TRAIN LOBBY with the same arena queued for a one-click rematch (match auto-zeroes on
> lobby arrival). **Scoreboard:** round-end board in DrawPropHuntOverlay, sorted by score, champion crowned on the
> final board, "ROUND i/N" readout while playing. **Host UI:** main-menu "Prop Hunt" section (Warehouse / Ruined
> Crypt / Facility) → `HostPropHuntForMenu` boards the lobby train with `?BHPropHunt=1?BHFirstLevel=<arena>`; the
> existing ready-up/force-start departure carries the mode automatically. Round count via `bh.PropHuntRoundCount`
> or `?BHPropHuntRounds=` (a host-menu picker for it can ride a later polish pass).
- **Match wrapper** (server, in `BHGameModePropHunt.cpp` + GameState): `PropHuntRoundIndex`, `PropHuntRoundCount`
  (best-of-N, CVar/host-set), per-player cumulative score, round-winner history. On RoundEnd → intermission (~8s,
  reuse the post-round travel timer) → rotate starting seeker → next arena (rotation list) → next round; after N →
  **MatchEnd** scoreboard → return to lobby.
- **Scoring (proposal — tune freely):** prop **survival** = `bh.PropHuntPointsPerSecondAlive` per second hidden;
  **last-prop bonus**; manual-taunt bonus; seeker **catch** = `bh.PropHuntPointsPerCatch`; **starting-seeker** small
  base so the rotation isn't a penalty. **MVP** = top score of the round. Cumulative match winner = highest total.
- **Replicated scoreboard** on `ABHPlayerState` (reuse `HunterPoints`/`QuestionPoints` or add prop-hunt fields) +
  a `DrawPropHuntScoreboard()` (RoundEnd + MatchEnd; reuse the leaderboard/board-text builders).
- **Host/lobby UI (the toggle you'll actually use):** in the existing host menu (`BHGameModeHostControls.cpp` +
  the menu Slate), add a **"Prop Hunt"** game-mode toggle, **round count**, **hide/seek seconds**, **arena rotation**
  pickers, and **seeker count** (default 1, scale option). The host start path appends `?BHPropHunt=1` (+ arena) to the
  lobby→first-arena travel so it's a one-click thing, not a console command. (Console `bh.PropHunt` stays as the dev path.)
- **Acceptance:** host picks Prop Hunt + best-of-3 + 2 arenas from the menu, presses start; the group plays 3 rounds
  with rotating seekers, a scoreboard between rounds, and a final winner.

### P7 — Polish, feel, accessibility, anti-grief (M, ongoing) **(core items done — 2026-06-09; grab-bag stays open)**
**Goal:** the difference between "works" and "enjoyable & polished". Grab-bag, do continuously.

> **STATUS (landed):** **idle bob** — UpdatePropDisguiseIdleBob (Tick, non-dedicated): a ~1 cm breath at rest
> swelling to ~3.5 cm while moving, riding PropDisguiseBaseZ; a LOCKED prop snaps to rest and stays dead-still
> (the tell the seeker learns). **Solidity** — `bh.PropHuntSolidProps` (1): a locked disguise gets real blocking
> collision (camera channel ignored), applied in lockstep on server + clients (SetPropLockedAuthority / OnRep /
> ApplyPropDisguiseVisuals), dropped BEFORE walking resumes on unlock. **Anti-strobe** — `bh.PropHuntReDisguiseCooldown`
> (1.5s) between disguise SWAPS (first disguise always free). **Seeker-hold no-hide zone** —
> `bh.PropHuntHoldNoHideRadius` (700) blocks disguising near the blind seeker's holding spot during the hide.
> **Telemetry** — full ph_* set now: ph_round_start / ph_hide_end / ph_sonar / ph_pulse / ph_taunt_manual /
> ph_catch / ph_round_end / ph_match_end. FOUND toasts + score feedback were in P5/P6.
> **Still open (needs assets/editor or later passes):** disguise poof/shatter VFX, hide-phase audio bed,
> spectate-cam for the caught-before-infected moment, colour-blind outline palette (markers are shape+text
> already), reduced-flash variant of the seeker screen-black (it is FUNCTIONAL anti-peek, so a dim variant must
> stay opaque enough to not leak positions).
- **Feel/VFX:** disguise "poof" particle + sound on transform; a subtle dust/​settle when a prop locks; a catch
  "shatter" of the prop; reveal-pulse outline material; seeker footstep/heartbeat audio ramp near props.
- **HUD/UX (small but high-impact):** clear phase banners; per-prop "FOUND!" toast with name; props-left pips; seeker
  scan-cooldown ring; manual-taunt cooldown ring; a tiny "you are: 3rd person / 1st person" indicator; round/match
  counter ("Round 2/3"); a between-round scoreboard with sort + MVP crown; spectate-cam for caught-then-infected players
  before they respawn as seeker; colour-blind-safe outline colours; the "Menu Size" scaler already covers text.
- **Accessibility/comfort:** respect reduced-camera-shake for the wrong-hit shake; the screen-black hide phase honours a
  "reduced flash" option (dim instead of full black); 3rd-person is the comfortable default for motion-sensitive players.
- **Anti-grief / failsafes:**
  - **Stuck prop recovery:** if a prop wedges in geometry (disguise solidity), the `O` reset / a nav-reproject snaps
    them to the nearest reachable point (reuse the void-recovery + tree-stuck patterns).
  - **AFK seeker:** if the seeker is idle, the reveal-pulse cadence guarantees the round still ends on the timer; add an
    optional AFK nudge.
  - **Spawn-camping the seeker hold:** props can't disguise inside/too near the seeker holding volume during hide.
  - **Hider out-of-bounds:** containment + kill-Z reposition (P3).
  - **Re-disguise spam:** small cooldown on `Z` so you can't strobe meshes.
  - **Reconnect:** a disconnected prop's pawn is cleaned up; reconnect mid-match rejoins as a spectator→next round
    (reuse the reconnect-token-across-travel system).
- **Telemetry (reuse `RecordPlaytestTelemetryMarker`):** `ph_round_start/ph_hide_end/ph_catch/ph_taunt_manual/`
  `ph_round_end/ph_match_end` with arena, durations, counts — so you can see what's fun and tune.
- **Acceptance:** a friend session feels clean: clear feedback, no one stuck, sensible defaults, nothing baffling.

### P8 — Bots that play prop hunt (M)  *(after the human game is good)* **(done — 2026-06-09)**
**Goal:** fill empty slots so 2–3 friends still get a lively round.

> **STATUS:** **Bot vision gate** (the keystone): `CanSeeCharacter` treats a STILL disguised prop as furniture —
> invisible beyond ~3.3 m however clear the LOS; a MOVING prop is spottable to 14 m. Bot seekers therefore hunt by
> the taunt/pulse noise stimuli they already consume instead of wallhacking the pawn list. **Bot props:** a 2 Hz
> `ServicePropHuntBots` (bot-mode rounds only) lets each bot wander 30–65% of the hide on its survivor brain, then
> `BotPickNearbyPropDisguiseAuthority` copies the NEAREST size-clamped world mesh (engine-shape fallback; same
> no-hide-zone rule as humans) and locks; after an auto-taunt ~35% of locked bots take a 3.5–7 s relocation scurry
> and re-disguise wherever they end up. **Bot seekers:** the stock hunter brain roams/chases/swings; the service
> fires the prop sonar on a human-ish cadence (cooldown + 2–6 s jitter), which doubles as an audible tell.
> **Infection:** caught bot props convert through the same HandlePropHuntCapture SetRole+RestartPlayer path as
> humans. Difficulty scaling beyond the existing bot-difficulty knobs is deferred until a playtest says it matters.
- **Bot props:** on Hiding, a bot picks a nearby valid prop, disguises, navigates to a hiding spot, locks. On auto-taunt
  it may relocate. Reuse the bot stimulus/intent system (`EBHBotIntent::Hide`, patrol points, nav).
- **Bot seeker:** roams via nav, uses scan on a cadence, investigates taunt noises (the bots already consume noise
  stimuli), swings when close. Difficulty scales scan frequency + accuracy.
- **Bot infection:** a caught bot prop converts to a bot seeker (same as humans).
- **Acceptance:** a 2-human + 6-bot round is fun and finishes; bots hide and seek believably at Normal difficulty.

### P9 — Ship & harden (S)
- Cook the arenas (P3) into a Shipping EOS Windows build; run `Run-PackagedClassroomSmoke` equivalent for prop hunt
  (a packaged 3-client prop-hunt smoke). Update the cook map lists + `Verify-EOSPackage`.
- Final automation suite green; a real **in-engine human playtest** with the friend group (the one thing v1 hasn't had).
- Write release notes; tag a pre-release.

---

## 7. Subsystem reference (deep specs)

### 7.1 Round & match state machine
- **`EBHPropHuntPhase`** {None, Hiding, Seeking, RoundEnd} — replicated on GameState with `PropHuntPhaseEndServerTime`.
- **Transitions:** `None → (round start) → Hiding → (hide timer) → Seeking → (all-caught | seek timer) → RoundEnd →
  (intermission) → Hiding (next round) | MatchEnd`.
- **Timers:** all driven from the existing 1 Hz `TickRoundTimer` (guarded by `bPropHuntMode`) + the phase end times; no
  new timer handles needed except optionally the reveal-pulse/taunt cadence (can also ride the 1 Hz tick).
- **Match fields (GameState):** `PropHuntRoundIndex`, `PropHuntRoundCount`, `PropHuntStartingSeekerId`,
  `PropHuntPropsRemaining/Total` (v1 done). **Per-player (PlayerState):** `PropHuntScore`, `PropHuntRoundsWon`,
  `bPropHuntWasInfected`.

### 7.2 Roles, seeker rotation, infection
- **Roles map to existing enum:** seeker = `EBHPlayerRole::Hunter`, prop = `EBHPlayerRole::Survivor`. Don't add a new
  role unless needed (keeps avatar/HUD/capture code working).
- **Starting-seeker rotation:** maintain `TArray<FUniqueNetId/PlayerId> PropHuntSeekerHistory`. Next starting seeker =
  the player who has seeked the fewest times, tiebreak by lowest cumulative score (gives trailing players a turn), or
  simple round-robin by join order. **Recommendation:** round-robin by a stable player id; skip players who left.
- **Seeker count:** default 1; optional `bh.PropHuntSeekers` or scale `= max(1, floor(players / bh.PropHuntPlayersPerSeeker))`.
- **Infection:** caught prop → `SetRole(Hunter)` + `RestartPlayer` + grant seeker kit + clear disguise + broadcast. The
  *starting* seeker is tracked separately so scoring can reward survival vs catches correctly.

### 7.3 Disguise system (current + improvements)
- **Current (v1):** capsule-anchored, world-fixed, owner-no-see `PropDisguiseMesh`; replicated mesh/material/scale/yaw;
  server-validated asset load; mesh-bounds floor alignment; `ApplyAvatarStyle()` restore. **Keep.**
- **Add:** owner-see toggle tied to camera mode (P2); optional locked-collision solidity (P2); idle bob (P2); size/
  validity clamp + per-map whitelist (P2/§7.9); re-disguise cooldown (P7); disguise "poof" VFX/SFX (P7); a small health
  pool option so a prop takes 2 hits (off by default — your group can decide).

### 7.4 Camera system (P2 detail)
- `PropCameraBoom` (SpringArm) created in the `ABHCharacter` ctor, deactivated by default; `bPropThirdPerson`
  client-local; `ApplyPropCameraMode()` reparents `Camera` (boom socket vs capsule eye) + flips `PropDisguiseMesh`
  owner-see; gated to props; default 3rd-person on first disguise. Honour the POV anim layer (the boom can still take
  the additive shake/lean from `UpdatePOVAnimation`, or suppress it in 3rd person for comfort). Seeker untouched.

### 7.5 Seeker tools (P4 detail)
- **Scan/sonar:** `bh.PropHuntScanRadius` (≈1500), `bh.PropHuntScanCooldown` (≈8s), reveal duration ≈2s, through-wall
  outline. **Melee:** reuse capture range/angle. **Wrong-hit:** `SeekerMissSlowSeconds(misses, 0.6, 0.35, 2.0)` (defaults
  match the unit test), slow applied via a temporary `MaxWalkSpeed` scale; clang SFX; reset on catch. **Reveal pulse:**
  `bh.PropHuntPulseBase` (≈45s) → `bh.PropHuntPulseMin` (≈12s) over the seek timer; ≈0.6s outline flash.

### 7.6 Taunt system (P5 detail)
- **Auto:** `TauntIntervalSeconds(elapsedFrac, bh.PropHuntTauntBase=30, bh.PropHuntTauntMin=10)` (v1 done) + horn SFX +
  self-outline. **Manual:** `PropTaunt` key, `bh.PropHuntTauntBonusPoints` (≈25), personal cooldown ≈8s; emits the same
  cue; nudges the auto timer. Optional taunt-line wheel (reuse emote wheel).

### 7.7 Scoring & rewards (P6 detail)
- Props: `bh.PropHuntPointsPerSecondAlive` (≈1/s) + last-prop bonus (≈100) + manual-taunt bonus. Seekers:
  `bh.PropHuntPointsPerCatch` (≈75) + starting-seeker base (≈25). MVP = round top score. Match winner = cumulative top.
  Optionally feed the existing cosmetic-achievement system (a "Master of Disguise" / "Bloodhound" egg) — cosmetic only.

### 7.8 Maps & arena registry (P3 detail)
- **Arena registry** (a static table or a small data asset): `{ LogicalName, PackagePath, SpawnStrategy
  (PlayerStarts|ScatterNav|Tagged), HoldingSpotHint, ExposureClampOn, RecommendedPlayers, Notes }`.
- **SpawnStrategy:** prefer existing `APlayerStart`s; else scatter on nav; tag one as the seeker hold or pick farthest.
- **Wire order:** ContainersHouse → RuinedCrypt → Horror templates → others.

### 7.9 Per-map arena checklist (repeat for each)
1. Add a registry row (package path + spawn strategy).
2. Open in editor: confirm collision on hide-able props; note oversized/tiny meshes for the size clamp; eyeball lighting.
3. Place 8–10 `APlayerStart`s spread across the play space (or rely on nav-scatter); tag/choose a seeker holding spot.
4. Confirm a runtime navmesh covers the whole space (adjust the runtime nav bounds size).
5. Add containment/kill-Z to the boundary; test you can't escape.
6. Optional: an exposure clamp / fill light if too dark, or it's fine with the flashlight.
7. Add the `.umap` + pack content to the cook lists; verify it loads in a cooked build.
8. Playtest: hide spots feel varied, sightlines are fair, no fall-through, round timing feels right.

### 7.10 HUD / UX inventory (small additions included)
Phase banner (HIDING/SEEK!/ROUND OVER) · big countdown · props-left X/Y pips · seeker scan-cooldown ring · manual-taunt
cooldown ring · "you are: Crate / 1st·3rd person" · last-taunt direction arrow · reveal-pulse flash · FOUND! toasts ·
round/match counter · between-round scoreboard + MVP crown · screen-black hide overlay for the seeker · infected
"you're a seeker now" banner · colour-blind outline palette · honour HUD Text/Widget Size + Menu Size scalers.

### 7.11 Audio inventory
Hide-phase calm bed · release klaxon · auto/manual taunt horn (+ optional voice lines) · found sting · wrong-hit clang ·
prop-poof transform · prop-shatter on catch · round win/lose stings · seeker heartbeat ramp near props · all via
`BroadcastGameplayAudioCue` / multicast, positional where it matters. Source from existing `SoundsOfHorror` + engine.

### 7.12 CVar master table (defaults; all `ECVF_Default`, read on the game thread)
| CVar | Default | Meaning |
|---|---|---|
| `bh.PropHunt` | 0 | Master opt-in toggle (v1 done). |
| `bh.PropHuntHideSeconds` | 30 | Hide-phase length (seeker frozen + blind). **(P1)** |
| `bh.PropHuntSeekSeconds` | 240 | Seek-phase length. **(P1)** |
| `bh.PropHuntRoundCount` | 3 | Best-of-N rounds per match. **(P6)** |
| `bh.PropHuntSeekers` | 1 | Starting seeker count (or 0 = auto-scale). **(P1/P6)** |
| `bh.PropHuntPlayersPerSeeker` | 6 | Auto-scale divisor when seekers=0. **(P6)** |
| `bh.PropHuntTauntBase` | 30 | Auto-taunt interval at round start (v1 done). |
| `bh.PropHuntTauntMin` | 10 | Auto-taunt interval at round end (v1 done). |
| `bh.PropHuntTauntBonusPoints` | 25 | Manual-taunt reward. **(P5)** |
| `bh.PropHuntScanRadius` | 1500 | Sonar reveal radius. **(P4)** |
| `bh.PropHuntScanCooldown` | 8 | Sonar cooldown. **(P4)** |
| `bh.PropHuntPulseBase` | 45 | Reveal-pulse interval early. **(P4)** |
| `bh.PropHuntPulseMin` | 12 | Reveal-pulse interval late. **(P4)** |
| `bh.PropHuntMissSlowBase` | 0.6 | Wrong-hit slow base (s) — matches the unit test. **(P4)** |
| `bh.PropHuntMissSlowPerMiss` | 0.35 | Wrong-hit slow per extra miss. **(P4)** |
| `bh.PropHuntMissSlowMax` | 2.0 | Wrong-hit slow cap. **(P4)** |
| `bh.PropHunt3pDistance` | 300 | 3rd-person spring-arm length. **(P2)** |
| `bh.PropHuntArenaSpawns` | 12 | Target spawn count on arena maps (placed PlayerStarts first, scatter fills). **(P3 done)** |
| `bh.PropHuntSolidProps` | 1 | Locked disguises get blocking collision. **(P2)** |
| `bh.PropHuntMinSize` / `MaxSize` | tune | Disguise bounds clamp. **(P2)** |
| `bh.PropHuntPointsPerSecondAlive` | 1 | Prop survival scoring. **(P6)** |
| `bh.PropHuntPointsPerCatch` | 75 | Seeker catch scoring. **(P6)** |
| `bh.PropHuntReDisguiseCooldown` | 1.5 | Anti-strobe on Z. **(P7)** |

### 7.13 Input bindings (current + new)
Current (v1): `PropDisguise=Z`, `PropLock=MiddleMouseButton`, `PropRotateLeft=[`, `PropRotateRight=]`.
New: `PropCameraToggle` (1st/3rd — pick a free key/mouse-thumb), `PropTaunt` (manual taunt — free key). All gated by
`IsPropHuntProp()` so they're inert in every other mode/role. Seeker reuses `Scan=Q`, `Capture=Mouse1`.

### 7.14 Replication summary
GameState: `bPropHuntMode`, `PropHuntPropsRemaining/Total`, `PropHuntNextTauntServerTime` (v1) + `PropHuntPhase`,
`PropHuntPhaseEndServerTime`, `PropHuntRoundIndex/Count`, `PropHuntStartingSeekerId` (new). Character: disguise fields +
`bPropLockedInPlace` (v1). PlayerState: `PropHuntScore`, `PropHuntRoundsWon` (new). Camera/screen-black/outlines/audio
are client-local reactions to replicated state.

### 7.15 Edge cases & failsafes (checklist)
Seeker disconnects mid-seek → props win (existing `CountAliveHunters()<=0`). Last prop disconnects → round resolves.
Everyone hides perfectly → auto-taunt + pulse force closure. Prop locks inside a wall → solidity prevents / nav-reproject
recovers. Prop in seeker hold during hide → blocked. Re-disguise spam → cooldown. Captured-then-infected scoring is
attributed correctly. Reconnect across the round travel → spectator → next round. Arena with no PlayerStarts →
nav-scatter spawns. Pitch-black arena → exposure clamp / flashlight. Mode left on across `ServerTravel` (CVar global) is
intended.

---

## 8. Testing strategy
- **Automation (headless, fast):** extend `BHPropHuntLibraryTests.cpp` and add a `BHGameModePropHuntTests.cpp`:
  - Pure: round-resolution table (done), taunt interval, miss slow, pulse cadence, scoring math, seeker-rotation pick.
  - World-based (`FBHScopedAutomationWorld` + `SpawnActor<ABHGameMode>`): phase transitions Hide→Seek→RoundEnd, infection
    role conversion, props-remaining counting, arena spawn-scatter returns valid points, timeout=props win.
  - Run via `UnrealEditor-Cmd … -ExecCmds="Automation RunTests BlackoutHunt.PropHunt; Quit"`.
- **Regression:** full `BlackoutHunt` suite must stay green except the documented pre-existing reds (cosmetic/crawl).
- **Manual playtest checklist (the friend session):** per arena — hide window feels right; seeker release is punchy;
  disguise + 3rd-person reads well; scan/pulse/taunt all land; infection accelerates fun; scoreboard is clear; nobody
  stuck/fell through; best-of-3 with rotation completes; defaults feel good. Capture telemetry, then tune CVars.

## 9. Risks & mitigations
- **Pack maps lack nav/spawns/collision** → arena loader (nav-scatter + runtime nav + containment); per-map checklist;
  start with the one known-good warehouse. **Risk: some props have no collision** → size/validity clamp + whitelist.
- **Cooking the packs bloats the build / missing-asset cook bugs** → reuse the 0.7.1 cook-map-list discipline + verify.
- **Disguise solidity causes stuck pawns** → default solid but with nav-reproject recovery + a non-solid fallback CVar.
- **Client-authoritative disguise asset path** (cosmetic) → already server-validated; keep paths in `/Game`/`/Engine`.
- **Screen-black hide phase + reconnect timing** → drive off replicated phase + deadline, not a one-shot RPC.
- **3rd-person + the additive POV camera layer fighting** → suppress/relax POV shake in 3rd person.
- **Scope creep** → phases are independently shippable; stop at any P and still have a playable mode.

## 10. Milestones & suggested sequencing
1. **M1 "Real loop"** = P0 + P1 → hide→seek→infection on a classroom map. *(biggest single win)*
2. **M2 "Looks right"** = P2 → 3rd-person + prop polish.
3. **M3 "Real arenas"** = P3 → ContainersHouse + 1–2 more wired.
4. **M4 "Real game"** = P4 + P5 → seeker kit + taunts + first audio.
5. **M5 "With friends"** = P6 → match flow, scoring, host UI. *(the target experience)*
6. **M6 "Polished"** = P7 → feel/UX/anti-grief, continuous.
7. **M7 "Solo-friendly"** = P8 → bots.
8. **M8 "Shipped"** = P9 → cook, smoke, human playtest, release notes.

## 11. Open questions / future (revisit when they matter)
- Prop **health** (1 vs 2 hits)? Default 1; trivially a CVar.
- **Seeker-side props** (decoys the seeker can place)? Probably no.
- **Custom taunt lines/voices**? Nice-to-have (emote-wheel reuse).
- **Persistent prop-hunt cosmetics/achievements**? Cosmetic-only, low priority.
- **Map voting** in the lobby for arenas (reuse the existing map-vote system)? Easy add in P6.
- **Spectator prop-cam** for the dead-before-infected moment? Small P7 add.
- **Dedicated server** prop hunt (vs listen-server)? Should "just work"; verify in P9.

## 12. Appendix — file inventory (current + planned)
- **Pure/logic:** `BHPropHuntLibrary.h` (v1) — add: pulse cadence, scoring, rotation helpers.
- **GameMode partial:** `BHGameModePropHunt.cpp` (v1) — add: phase driver, infection, match wrapper, seeker tools, arena
  builder hooks. **Tests:** `BHPropHuntLibraryTests.cpp` (v1) + new `BHGameModePropHuntTests.cpp`.
- **Types/state:** `BHTypes.h` (add `EBHPropHuntPhase`), `BHGameState.h/.cpp` (phase + match fields),
  `BHPlayerState.h/.cpp` (score fields).
- **Pawn:** `BHCharacter.h/.cpp` — disguise (v1) + camera boom/toggle, manual taunt, solidity, idle bob, seeker-tool hooks.
- **HUD:** `BHHUD.h/.cpp` — `DrawPropHuntOverlay` (v1) + phase banners, scoreboard, scan/taunt rings, screen-black.
- **Maps:** arena registry (new, in `BHGameModePropHunt.cpp` or a data asset) + cook-list + `Verify-EOSPackage` updates.
- **Config:** `DefaultInput.ini` (v1 + camera/taunt actions); CVars live in `BHGameModePropHunt.cpp`.
- **Host UI:** `BHGameModeHostControls.cpp` + menu Slate — prop-hunt toggle/options.

---

*End of roadmap. Keep this updated as phases land — flip **(new)→(done)**, record tuned CVar values, and append
playtest findings so the next session starts where this one ended.*
