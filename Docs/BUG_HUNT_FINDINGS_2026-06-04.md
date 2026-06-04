# Blackout Hunt — Deep Bug Hunt Findings (2026-06-04)

*A full-codebase audit of the native C++ gameplay module (`Source/BlackoutHunt`, ~120 files). Goal: find every defect — crashes/stability, multiplayer authority & replication, gameplay/education logic — and fix the unambiguously-safe ones.*

Branch: **`bugfix/deep-audit-2026-06-04`** (off `claude/dazzling-hamilton-68Lji`, the 0.8.1 live-play branch).
Method: **static analysis only** (no engine build — the project's own build logs show OOM/git-hang cooks, so the window was spent reading, not fighting a 30-min cook). Every fix below is reasoned to compile and to be behavior-safe, but **none has been compiled or run** — see [Caveats](#caveats).

---

## Headline

**This codebase is exceptionally defensive and well-engineered.** Across 120 source files I (and a fleet of 11 parallel deep-auditor passes) found **no high-severity crash, no authority/anti-cheat hole, and no win/loss-breaking logic bug.** The server-authoritative model is applied consistently: every client action goes `input → ServerXxx RPC → XxxAuthority()` with role/phase/distance/line-of-sight/cooldown re-validation; every host/admin action funnels through `RequireHostAdmin`; answer-bearing fields are `COND_Never`/owner-only; timers and delegates are cleared in `EndPlay`; weak pointers are validated; divisors are floored; enum-indexed tables are clamped.

The defects that exist are a small number of **latent crashes (out-of-range inputs), late-join replication-ordering glitches, and education-bank robustness gaps** — plus a long tail of low-severity polish/perf/UX items.

| | Count |
|---|---|
| **Fixed & committed** | 6 issues (3 commits) |
| **Reported, not fixed** (recommended) | ~24 items, all Low / Low-Medium |
| High / Critical found | **0** |

---

## Coverage

**Personally deep-read (authority/replication/lifetime core):** `BHGameState`, `BHPlayerState`, `BHGameInstance` (travel/reconnect/online sessions), `BHNetworkSupport`, `BHCharacter` RPC/authority surface (capture, scan, hunter power, decoy, interaction, answer, powerup), `BHGameModeHostControls` (full), `BHGameMode` win/loss (`NotifySurvivorCaptured`/`Escaped`), `BHPlayerController` Server-RPC bridge, `BHDoor`/`BHSlidingGate`/`BHFlickerLight`, plus the revision-bank select/parse/validate paths.

**Audited by 11 parallel read-only deep-auditor passes (each read its files in full):** interactables/objectives · security/CCTV · train/final-escape · bot AI · atmosphere/horror/jumpscare · main menu Slate (all 11.5k lines) · HUD/diagram/classroom-board · revision/lessons/tutorial/feedback · powerups/cosmetics/account+crypto · level/props/movement assets · settings/types/commandlets/automation.

**Not exhaustively line-read by me personally** (covered by the agents + spot reads): the full 14k-line `BHGameMode.cpp` director/level-gen body, full `BHPlayerController.cpp`, the BotServices/TrainFlow partials, and the automation test suite. No defect was found in the parts that were read; the unread remainder is lower-risk (cosmetic/generation code) but is the main residual blind spot.

---

## Fixed & committed

### 1. Question-bank seed modulo could index out of bounds (`FMath::Abs(INT32_MIN)` UB) — *Medium*
`Source/BlackoutHunt/BHRevisionQuestionBank.cpp` — `SelectQuestion` (~1070), `SelectQuestionByDifficulty` (~1102), `SelectDragQuestion` (~1134).
`FMath::Abs(INT32_MIN)` is undefined and in practice stays `INT32_MIN`; `INT32_MIN % N` is negative, so `Candidates[ChosenIndex]` / `Pool[...]` reads before the array. Reachable: `FBHLessonPresetStore::BuildManualQuestionSet` builds `QuestionSeed` via unsigned addition that can wrap to exactly `0x80000000` and passes it straight in. Switched all three to unsigned modulo — the exact guard the author already uses in `BHGameModeHostControls.cpp::SelectHostControlPromptLine`. **Commit `47d66e1`.**

### 2. Teacher override-bank parse didn't floor mastery weight / tolerance — *Medium (education logic)*
`BHRevisionQuestionBank.cpp::ParseQuestionsFromJson` (~1414, ~1437).
A runtime `Saved/ClassroomPresets/QuestionBank.json` is screened by `ValidateQuestionSet`, which checks structure (4 choices, valid index, distinct, topics present) but **not** `masteryWeight <= 0` or `numericTolerance < 0`. A teacher bank with `masteryWeight: 0` makes correct answers build no mastery (soft-locks the class below the exit threshold); negative reduces it. A negative tolerance can never satisfy `|value-answer| <= tolerance`. Now floored on parse (`>0` / `>=0`) to match the built-in `AddSpecs` path. **Commit `47d66e1`.**

### 3. Showcase question builder didn't clamp `CorrectChoiceIndex` — *Low (latent)*
`BHRevisionQuestionBank.cpp::BuildNewTypeShowcase` (~773). Every other builder clamps; this stored the raw index. All 8 current callers pass `0`, so no live defect, but a future entry ≥4 would OOB-index `Choices[]` during grading. Clamped like `AddSpecs`. **Commit `47d66e1`.**

### 4. Doors render ~180° wrong for late-joining clients — *Medium (replication ordering)*
`Source/BlackoutHunt/BHDoor.{h,cpp}`.
For a dynamically-replicated door, `OnRep_Open` can fire **before** `BeginPlay`. If a client joins after a survivor opened the door, `OnRep_Open` ran with `ClosedRotation` still at its zero default and rotated the actor to `(0,90,0)`; then `BeginPlay` captured *that* corrupted pose as "closed," leaving the door offset by ~180° (wrong visual **and** wrong collision box) on that client. Fixed by deferring `ApplyDoorState` until `BeginPlay` captures the baseline, and backing the open offset out of the replicated spawn transform so the true closed rotation is recovered even when the door arrives already-open. Authored/level-placed doors are unaffected. **Commit `02c1197`.**

### 5. Sliding gates lift to 2× height for late-joining clients — *Low (replication ordering)*
`Source/BlackoutHunt/BHSlidingGate.{h,cpp}`. Same `OnRep`-before-`BeginPlay` hazard; the gate baked the lift height into its "closed" mesh position. Fixed with the same deferral gate (no offset back-out needed — component-relative transforms aren't in the spawn bunch, so the mesh starts at the authored closed location once OnRep is deferred). **Commit `02c1197`.**

### 6. Captured/benched bots don't release their objective claim — *Low (AI)*
`Source/BlackoutHunt/BHBotController.cpp::Think` (~751). A bot captured mid-objective keeps its pawn, so `EndPlay` never runs and its time-boxed claim (~13 s) lingered; the rest of the bot team treated the station/breaker as taken and avoided it. Now releases the claim on the not-alive early-out, mirroring `EndPlay`. **Commit `dafc7b9`.**

---

## Reported — not fixed (recommended)

These are real but **Low / Low-Medium** severity, and several involve changing code that currently behaves correctly — left for your judgment rather than changed blind (un-compilable). Each has a concrete recommended fix.

### Networking / replication
- **`BHFlickerLight` burst timing is cross-machine-time-fragile** — `BHFlickerLight.cpp:86,239`. `FlickerBurstEndTime` is a *server* `GetTimeSeconds()` value compared against the *client's* unsynchronised `GetTimeSeconds()`. **Visible behaviour is currently correct** because the server's authoritative `EndFlickerBurst` timer + `FlushNetDormancy` ends the burst on time; the client-local early-clear just never fires usefully for remote clients (their `RemainingSeconds` is a large over-estimate). Recommend deriving the client countdown purely from the replicated bool + a duration (not from an absolute cross-machine timestamp). *Not fixed: working today; change risks the one path that is correct.*
- **`AppendMaxPlayersOption` substring guard** — `BHGameSettings.cpp:184`. `if (!Options.Contains(TEXT("MaxPlayers=")))` would be fooled by any colliding option key, silently re-introducing the engine's 16-player cap the function exists to prevent. No colliding key exists today. Recommend a boundary check (`?MaxPlayers=` / `&MaxPlayers=`) or `FParse::Value`.

### Gameplay / education logic
- **Atmosphere `ResolveTarget` preferred-branch liveness mismatch** — `BHAtmosphereDirector.cpp:374-383`. The preferred branch returns the target if `!PreferredPS || LifeState==Alive`; the scan branch correctly requires `IsAliveSurvivor() && !IsHiddenInLocker()`. A non-survivor (or null-PS) passed as the preferred target would be scared. Harmless today (the scheduler passes survivors). Recommend aligning the preferred branch to `PreferredPS && PreferredPS->IsAliveSurvivor() && !PreferredTarget->IsHiddenInLocker()`.
- **Revision team-vote tally hard-capped at 4 choices** — `BHObjectiveStation.cpp:~1072`. `int32 ChoiceVotes[4]` / loops to `< 4`. If a revision question ever has its correct answer rotate to index ≥4 (the data model tolerates non-4 choice counts; only the validator enforces 4 today), the node can never be voted through (soft-lock). Recommend sizing the tally to `QuestionChoices.Num()`.
- **Train bonus terminal: dead correction-hold + throttle-before-validate** — `BHTrainBonusQuestionTerminal.cpp:243,322,335`. The "Retry in Ns / read the correction" path is unreachable (a wrong answer adds the player to `PlayersAnsweredCurrentQuestion`, and one-answer-per-question then blocks the retry the messaging promises); the per-player throttle timestamp is written before the request is validated. Cosmetic/messaging only. Recommend only adding to the answered-set on a *correct* answer (or dropping the hold + its copy), and moving the throttle write after the guards.
- **Atmosphere CCTV-glitch cooldown is global, not per-player** — `BHAtmosphereDirector.cpp:89-108`. `LastCCTVGlitchTime` is director-wide, so one survivor's CCTV cue suppresses everyone's for 8 s despite per-player budgets. Recommend moving the throttle into `FBHPlayerScareMemory`.
- **Bot scoring double-counts personality/difficulty** — `BHBotController.cpp:1419-1447` + `BHBotPolicySubsystem.cpp:178-249` apply personality bonuses and difficulty noise in *both* layers (and even disagree: Bold gets `Flee` in one, `Chase` in the other). Skews tuning (Easy gets two independent noise terms). Recommend owning the shaping in one layer.

### Stability / lifetime (latent — no live trigger found)
- **`HandleSurvivorThreat` dereferences `Threat`/`BotCharacter` with no null check** — `BHBotController.cpp:2201-2220`. **Dead code** (no caller); would crash if ever wired up with a null threat. Recommend adding a guard or deleting it (also dead: `FindSurvivorObjective`, `HasUsableNavLocation`).
- **`FBHScopedAutomationWorld` world not rooted** — `BHAutomationTestWorld.cpp:43-55`. No `AddToRoot()`/FGCObject anchor; relies on the package/context reference surviving an in-test `CollectGarbage`. Test-only. Recommend rooting the world for the harness lifetime.
- **`BHNoiseDecoy` has no `EndPlay` timer clear** — `BHNoiseDecoy.cpp:75`. Repeating noise timer relies solely on actor-destruction auto-clear (fine in the normal lifespan path). Defensive: add `ClearTimer` in `EndPlay`.
- **`ABHRuntimeMeshPropActor` loads assets in its constructor + has no `BeginPlay` re-apply** — `BHRuntimeMeshPropActor.cpp:31`. `LoadObject` from a constructor (vs `FObjectFinder`) runs per-spawn/CDO; and unlike `ABHBlockActor` it never re-applies serialized visuals on the host, so a *map-placed* instance would show constructor defaults. Spawned-at-runtime props (the normal path) are fine. Recommend `FObjectFinder` + a `BeginPlay` that calls `ApplyPropVisuals`.

### Security (low / nits)
- **Account login poll never times out** — `BHAccountSubsystem.cpp:1233,1494`. If OAuth is abandoned and the backend keeps replying `pending`, the repeating poll timer runs forever (not a hang — bounded HTTP per call — but an unbounded background poll). Recommend an attempt/deadline cap.
- **Credential MAC compared non-constant-time** — `BHAccountSubsystem.cpp:506`. `FString::operator!=` short-circuits. Marginal (machine-bound key, local attacker, local timing). Recommend a constant-time byte compare. *(The crypto is otherwise correct: AES-256-CBC, random per-save IV, encrypt-then-MAC verified before decrypt, salted+iterated password hashing, crash-safe atomic `.bak`/`.tmp` rotation.)*

### Perf / UX (low)
- **`SBHMainMenu` per-click status masked by sticky messages** — `SBHMainMenu.cpp:6506-6531`. After a host/sign-in, `GetStatusText()` keeps returning the sticky account/network message, hiding most per-button results (kick/role/vote feedback). Recommend clearing the sticky message once consumed or letting a fresh `StatusText` outrank it.
- **`SBHMainMenu` host action-list capability computed at `Construct`** — `SBHMainMenu.cpp:4509-4540`. `bHostAdmin`/`bCanEndWarmup` are baked at build time, so "START HUNT NOW" can linger/absent if the phase changes while the menu stays open (host-side only; no student exposure). Recommend `TAttribute` getters like the rest of the menu.
- **`ABHExitGate` ticks cosmetic visuals on the dedicated server** — `BHExitGate.cpp:20-21,75`. Per-frame light pulse + dynamic-material churn with no viewers. Recommend early-out when `NM_DedicatedServer`.
- **`BHJumpscareMonster` synchronously loads the photoreal mesh/anim/material on the dedicated server** — `BHJumpscareMonster.cpp:1086-1176`, which it never renders. Recommend early-out on `NM_DedicatedServer` (or async-load).
- **`BHLocker` Tester sees a non-functional "Tester Hide" prompt on an empty locker** — `BHLocker.cpp:75,108`. `CanInteract` is true but `BeginInteract` falls through all branches. Recommend implementing it or gating `CanInteract`.
- **EMSpectrum band tables are three independent literals of length 7** — `BHDiagramRenderer.cpp:290,303,697`. Safe today; edit one without the others and the loop reads past the shorter array. Recommend `UE_ARRAY_COUNT` + `static_assert`.
- **`EBHWarmupStep` is a flags enum without `ENUM_CLASS_FLAGS`** — `BHTypes.h:68-75`. Always hand-cast today; add the macro to prevent a future un-cast `|`.
- Misc low-confidence defensive nits from the CCTV pass (`Tan(half-cone)` domain if a designer authors a >170° cone directly; `GetUniqueID()`-seeded fuzz can collide across actor lifetimes) — `BHSecurityCamera.cpp:690`, `BHSecurityMonitor.cpp:649`.

---

## Notable verified-clean areas

So the report isn't read as "everything is broken," these high-risk areas were specifically traced and found correct:

- **Anti-cheat / authority:** capture (`IsTeacherCaptureCandidateAuthority` — alive-survivor, vertical/range/aim-arc/LoS/locker-grace all server-checked), scan, hunter power, decoy, interaction (server distance **+ line-of-sight** re-gate against wall-hacks), answer submission, and powerup use are all server-authoritative with role+phase+cooldown+stamina validation. Host/admin/tester/jumpscare-test RPCs all re-gate through `RequireHostAdmin` (with a denial-reply throttle against RPC spam). Students cannot reach teacher/admin/test affordances.
- **Replication:** owner-only conditions on private meters (Stamina/Fear/Dread) and academic records (RevisionStats/ReviewQueue/Warmup); answer fields never replicated; `Super::` called in every `GetLifetimeReplicatedProps`/`BeginPlay`/`EndPlay`.
- **Travel/reconnect:** keyed on a secret server-issued token (not a spoofable display name); a 120 s grace uses a process-wide monotonic clock that survives ServerTravel; a same-named joiner can't inherit another student's points/role.
- **Win/loss:** evacuate-together semantics; the last survivor being caught still yields SurvivorsWin if anyone already escaped; final-escape timeout credits a partial evacuation correctly.
- **Crypto/accounts, diagram renderer (answer-safety + all div/NaN/OOB guarded), bot AI (difficulty correctness right-way-round, no stale-pointer/iterator-invalidation), train economy (no double-spend/free-buy), block-field batching (no key collision, full-teardown rebuild avoids stale instance indices), synth audio, jumpscare selection (no infinite "never repeat" loop in scope).**

---

## Caveats

- **Nothing here was compiled or run.** Fixes are reasoned to be correct and syntactically valid, but the user opted for static analysis. **The door/gate header changes add a member and need a normal build (UHT regen) to take effect** — they will not show up in a hot-swapped exe without a rebuild.
- Per `AGENTS.md`, after building, the touched systems map to: `Build-Editor.ps1` (C++), and the revision/bot/objective automation suites (`BlackoutHunt.*`). A focused run of the revision and train-economy tests would cover commits 1 and 6; there is no automated coverage for the late-join door/gate ordering (commits 4–5) — those want a 2-client late-join smoke check.
- Residual blind spot: the unread remainder of `BHGameMode.cpp`/`BHPlayerController.cpp` bodies and the test suite. Nothing read there was defective; a follow-up pass could close it.
- The `bUseAuthoredLevels` default is documented inconsistently (ini `False` in §28 vs "on by default" narrative in §33) — worth confirming, as it changes whether the door/gate late-join path (runtime-spawned actors) is hit in shipped configs.

---

*Generated by an automated deep audit (1 orchestrator + 11 parallel read-only auditor passes) on 2026-06-04.*
