# Blackout Hunt — Deep Bug Hunt Findings (2026-06-04)

*A full-codebase audit of the native C++ gameplay module (`Source/BlackoutHunt`, ~120 files). Goal: find every defect — crashes/stability, multiplayer authority & replication, gameplay/education logic — and fix the unambiguously-safe ones.*

Branch: **`bugfix/deep-audit-2026-06-04`** (off `claude/dazzling-hamilton-68Lji`, the 0.8.1 live-play branch).
Method: **static analysis only** (no engine build — the project's own build logs show OOM/git-hang cooks, so the window was spent reading, not fighting a 30-min cook). Every fix below is reasoned to compile and to be behavior-safe, but **none has been compiled or run** — see [Caveats](#caveats).

---

## Headline

**This codebase is exceptionally defensive and well-engineered.** Across 120 source files I (and a fleet of 15 parallel deep-auditor passes + my own line read of the core) found **no crash worse than a latent out-of-range index, and no authority / anti-cheat hole.** The server-authoritative model is applied consistently: every client action goes `input → ServerXxx RPC → XxxAuthority()` with role/phase/distance/line-of-sight/cooldown re-validation; every host/admin action funnels through `RequireHostAdmin`; answer-bearing fields are `COND_Never`/owner-only; timers and delegates are cleared in `EndPlay`; weak pointers are validated; divisors are floored; enum-indexed tables are clamped.

The most consequential defects are a small set of **round-outcome (win/loss) logic bugs** — the round-end resolution in `Logout` and the infection-capture path credited the Teacher a win the class had actually earned (a survivor had already escaped). Beyond those: **latent crashes (out-of-range inputs), an unwinnable single-breaker map, late-join replication-ordering glitches, and education-bank robustness gaps** — plus a long tail of low-severity polish/perf/UX items.

| | Count |
|---|---|
| **Fixed & committed** | 10 issues (7 commits) |
| **Reported, not fixed** (recommended) | ~30 items, all Low / Low-Medium |
| Highest severity found | **High** (one win/loss bug, fixed) — no Critical, no crash-on-normal-play |

---

## Coverage

**Personally deep-read (authority/replication/lifetime core):** `BHGameState`, `BHPlayerState`, `BHGameInstance` (travel/reconnect/online sessions), `BHNetworkSupport`, `BHCharacter` RPC/authority surface (capture, scan, hunter power, decoy, interaction, answer, powerup), `BHGameModeHostControls` (full), `BHGameMode` win/loss (`NotifySurvivorCaptured`/`Escaped`), `BHPlayerController` Server-RPC bridge, `BHDoor`/`BHSlidingGate`/`BHFlickerLight`, plus the revision-bank select/parse/validate paths.

**Audited by 15 parallel read-only deep-auditor passes (each read its files in full):** interactables/objectives · security/CCTV · train/final-escape · bot AI · atmosphere/horror/jumpscare · main menu Slate (all 11.5k lines) · HUD/diagram/classroom-board · revision/lessons/tutorial/feedback · powerups/cosmetics/account+crypto · level/props/movement assets · settings/types/commandlets/automation · **`BHGameMode.cpp` lifecycle/director/capture-escape half (1–7200)** · **`BHGameMode.cpp` level-gen/revision/reporting half (7000–end)** · **`BHPlayerController.cpp` in full (all 8.7k lines)** · **GameMode BotServices + TrainFlow partials + all 10 automation test files**.

**Coverage is now effectively complete.** A final wave closed the original blind spot — both halves of the 14k-line `BHGameMode.cpp`, the full `BHPlayerController.cpp`, the BotServices/TrainFlow partials, and the test suite were all read in full. The test suite was specifically checked for wrong-expected assertions that could mask a regression — none found.

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

### 7. Round credited to the Teacher on disconnect when a survivor already escaped — *High (win/loss)*
`Source/BlackoutHunt/BHGameMode.cpp::Logout` (~1322). "Evacuate-together" means an escaped survivor (LifeState `Escaped`, so not counted as alive) has already won for the class. If the *last alive* survivor then disconnects (and isn't reconnectable), `CountAliveSurvivors() <= 0` fired `EndRound(HunterWin)` — discarding the escapee's win. Every other terminal check disambiguates with `CountEscapedSurvivors()` (Hunt tick :13317, standard capture :1606); only `Logout` omitted it. Now `CountEscapedSurvivors() > 0 ? SurvivorsWin : HunterWin`. **Commit `eab726b`.**

### 8. Same wrong-winner in the infection-mode capture path — *Medium (win/loss)*
`BHGameMode.cpp::NotifySurvivorCaptured` infection branch (~1527). If a survivor already escaped and the last inside survivor is captured-and-infected, the round ended `HunterWin`. Same fix as #7. **Commit `eab726b`.**

### 9. Single-breaker maps are unwinnable — *Medium (gameplay logic)*
`BHGameMode.cpp::PrepareRoundDirector` (~9907). `ActiveBreakerCount = FMath::Clamp(X, 2, FMath::Max(1, BreakerActors.Num()))` — when a map has ≤1 breaker, the clamp is `Clamp(X, 2, 1)` (min>max), which UE resolves to the *min* (2). With one physical breaker `BreakersCompleted` maxes at 1 and never reaches the required 2, so the exit never unlocks. Shipped procedural maps spawn 7–8 breakers (unaffected); authored maps with ≤1 breaker, or partial spawn failures, hit it. Wrapped in `FMath::Min(BreakerActors.Num(), …)` — a no-op where the clamp result is already ≤ N. **Commit `eab726b`.**

### 10. `ServerSetReady` missing the lobby-RPC flood-guard — *Low (authority)*
`Source/BlackoutHunt/BHPlayerController.cpp::ServerSetReady_Implementation` (~7407). The one spammable lobby RPC missing `AllowLobbyActionRpc()`; a modified client could toggle ready/unready unbounded (repeated `AreAllReady()` scans + reliable churn). Added the guard to match the other lobby RPCs. **Commit `bf34adc`.**

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

### Final-wave additions (GameMode / PlayerController / partials)
- **Listen-server host hit-stop dilates the *shared* world** — `BHPlayerController.cpp:8565`. `ClientPlayHorrorCue` uses `UGameplayStatics::SetGlobalTimeDilation` for the impact hit-stop; on the listen-server host (which is a client of its own world) this briefly slows the simulation for *every* student when the host is the scare target. Self-heals via a restore timer. The clean fix has tradeoffs (gating to `NM_Client` removes the host's own hit-stop; a camera/anim-only jolt avoids touching global dilation) — left for your call.
- **~40 Server RPCs deref `GetWorld()` unchecked** — `BHPlayerController.cpp` (7409, 7436, 7444, …) do `GetWorld()->GetAuthGameMode<>()` with no null guard, while two siblings (`ServerJumpscareTest`, `ServerAtmosphereTest`) do guard. Effectively unreachable on the authority, but inconsistent. Recommend the guarded form everywhere.
- **`BroadcastStatus`/`SpawnAmbient`/director scare iterators deref `GetWorld()` unchecked** — `BHGameMode.cpp:12419, 9850, 10853, 12320`. Same latent defensive gap as above; recommend an early `if (!GetWorld()) return;`.
- **Solo live round with zero hunters ends instantly as SurvivorsWin** — `BHGameMode.cpp::AssignRoles` (~13198). The hunter-guarantee only runs for `Players.Num() >= 2`, so a host who sets `MinPlayers == 1` and starts a *live* (non-practice) round solo as a survivor gets an immediate `SurvivorsWin` on the first Hunt tick. Practice/Test are the intended solo modes. Recommend spawning a bot/teacher (or holding in Lobby) when a live round would start with zero hunters.
- **`GetSpawnTransformFor` uses `INDEX_NONE` in spawn-offset math** — `BHGameMode.cpp:13714-13731`. If `PlayerArray.IndexOfByKey(BHPS)` returns -1 (a login/travel timing window), the negative index flows into the hunter-spawn offset math (no OOB — the survivor branch's `IsValidIndex(-1…)` falls back, and `BHResolveSpawnLocation` uses `FMath::Abs`), so it's at worst a slightly-wrong spawn offset. Recommend `const int32 SafeIndex = FMath::Max(0, PlayerIndex);`.
- **`SetBotCount` clears `bBotMode` mid-round while live bots remain** — `BHGameModeBotServices.cpp:222`. Setting bot count to 0 mid-Hunt flips `bBotMode=false` (and the GameState) but the cull is deferred to round end, so `RefreshBotRoster` then early-returns and the live-bot count is never re-clamped if a human joins/leaves. Internally inconsistent, not a crash. Recommend deferring the `bBotMode` flip (like the count) when mid-round.
- **Lesson-preset bot cap is a hardcoded `11`** — `BHLessonPreset.cpp:930` vs the GameMode's `MaxPlayers - 1` cap. Diverges if `MaxPlayers != 12`. Recommend deriving both from `UBHGameSettings::MaxPlayers`.
- **`StartBotSoak` re-runs `RequireHostAdmin` via `ForceBotHunt`** — `BHGameModeBotServices.cpp:326` (harmless redundant authority check after the caller already gated). Optional cleanup.

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
- **Coverage is now effectively complete** — the original blind spot (full `BHGameMode.cpp`, `BHPlayerController.cpp`, the partials, the test suite) was closed by a final 4-pass wave. The only thing not done is a re-read of the procedural level-*geometry* builders' raw numeric constants (cosmetic), which can't harbor the bug classes hunted here.
- The `bUseAuthoredLevels` default is documented inconsistently (ini `False` in §28 vs "on by default" narrative in §33) — worth confirming, as it changes whether the door/gate late-join path and the single-breaker case (runtime-spawned vs authored actors) are hit in shipped configs.
- Recommended verification once you can build: the **wrong-winner fixes (#7, #8)** want a targeted test or a 2-client "escape then disconnect / infect" check; **#9** wants a 1-breaker-map round; the late-join **door/gate (#4, #5)** want a 2-client join-after-open check. None of these have existing automated coverage.

---

*Generated by an automated deep audit (1 orchestrator + 15 parallel read-only auditor passes) on 2026-06-04. Branch `bugfix/deep-audit-2026-06-04`.*

---
---

# Pass 2 — deeper, cross-cutting audit (2026-06-05)

A second pass at **2× the depth**, hunting *specialized lenses that cut across files* (the gap where pass-1's per-file reads are weakest) rather than re-sweeping each file: physics-answer correctness, exhaustive state-machine soft-locks, systematic replication-ordering, adversarial RPC tracing, timer/lifetime, float/NaN/determinism, cross-system data-flow (producer→consumer), long-session resource growth, the config files, and deep re-reads of the director/movement/ObjectiveStation/bot bodies — plus an **adversarial review of the pass-1 fixes**.

## Pass-2 headline

The pass-1 verdict holds and is reinforced: **no new crash-on-normal-play, no authority/anti-cheat hole, no win/loss-breaking logic bug.** The two most valuable pass-2 outcomes are *confirmations*:

- **All 376 built-in physics questions are physically correct** — two independent examiner passes re-derived every calculation within tolerance, checked every marked MC/TF answer, verified units/magnitudes, and confirmed explanations match their prompts and diagrams don't leak answers. For a teaching tool this was the single biggest untested surface; it's clean.
- **Adversarial RPC tracing of all 67 Server RPCs found zero exploitable gaps** (economy, answers, abilities, escape, match-control all re-validate role+phase+ownership+cooldown+distance+currency server-side), and the **timer/delegate/async surface has zero use-after-free** (every callback is weak-bound or `this`-bound with a matching clear).
- **All 5 pass-1 fix commits independently reviewed SOUND** — each fixes a real bug, none regresses or mis-compiles.

| | Count |
|---|---|
| **Pass-2 fixed & committed** | 3 (StartHuntPhase ticker, final-leg auto-board, Tester melee visuals) |
| **Pass-2 reported, not fixed** | ~35 items (a few Medium, mostly Low) |
| New crash/Critical/exploit | **0** |

## Important shipped-config note (resolves a pass-1 caveat)

`Config/DefaultGame.ini` sets **`bUseAuthoredLevels=True`** (shipped) — the runtime block-field spec array (~152 KB) exceeds the 64 KB net-bunch cap, so shipped builds load baked `.umap`s with level-*placed* actors. This means the pass-1 **door/gate (#4/#5)** and **single-breaker (#9)** fixes primarily protect the *runtime-generated / non-authored / test* path and future authored maps — their real-world severity in the shipped authored-map config is lower than rated (level-placed doors/gates don't hit the OnRep-before-BeginPlay path; shipped maps have 6–10 breakers). The fixes remain correct and were verified to introduce no regression for level-placed actors. Also confirmed: **EOS/Steam secret handling is correct** — `EOSValues.local.ini` (real credentials) is gitignored *and* in `[Staging] DisallowedConfigFiles`, so secrets are neither committed nor cooked; only empty `.example.ini` templates are tracked. And the engine RPC-DoS detection is intentionally disabled (template), so the C++ `AllowLobbyActionRpc` throttle is the only RPC protection — which is exactly why the pass-1 `ServerSetReady` flood-guard mattered.

## Pass-2 fixed & committed

- **`StartHuntPhase` didn't arm the round ticker** (`BHGameMode.cpp` ~12952) — it relied on the looping `RoundTimerHandle` from `StartPrepPhase`; the sibling `StartHuntPhaseImmediately` arms its own. If any Prep path cleared that handle, the live Hunt would have no ticker and `TickRoundTimer`'s win/loss/time logic would never run (hang). Arm it explicitly (idempotent). **Commit `4f7dab2`.**
- **Final-leg Recap→StationStop skipped auto-board** (`BHTrainIntermissionManager.cpp` ~322) — unlike every other boarding transition, the final results-hold shortcut didn't call `AutoBoardPlayers()`, so a straggler could be stranded in the tunnel. **Commit `4f7dab2`.**
- **Teacher axe swing didn't animate for Testers** (`BHCharacter.cpp` 5483/5529) — capture gates on `IsAliveHunter()` (includes Tester) but the visuals checked `role==Hunter` exactly, so a swinging Tester saw no animation/trail/flash. Use `IsAliveHunter()`. **Commit `b725d78`.**

## Pass-2 reported, not fixed (with file:line + recommendation)

Triaged conservatively — these are real but either Low severity, risk regressing a working system if changed without a build, or are tuning/UX judgments. The notable **Medium** ones are starred.

**State machine / round flow**
- ★ **`TickRoundTimer` has no `FinalEscape` arm** (`BHGameMode.cpp:13310-13340`) — during FinalEscape the looping ticker does nothing; if all survivors leave the alive set by a *non-event* path (e.g. an admin role change) only the expiry timer resolves it. Recommend a `FinalEscape` backstop mirroring the Hunt arm (`CountEscapedSurvivors()>0 ? SurvivorsWin : HunterWin`). Defensive (the event handlers cover the normal paths).
- **`IsAliveSurvivor()` and `IsAliveHunter()` both count a `Tester`** (`BHPlayerState.cpp:86-94`) — a Tester is on both teams, so neither team-wipe win can fire. Masked today by `bTestMode` early-returns; risk only if a Tester role leaks into a scored round. Cross-system audit confirmed it can't currently mis-credit a win. Risky to change (predicates used everywhere) → report.
- **`EBHFinalEscapeState::Locked` is never set** (`BHTypes.h` + all `SetFinalEscapeState` sites) — dead state; the display already uses `Inactive`. Remove or wire it.
- **No-manager final-escape fallback skips the cutscene + hunter-freeze/anti-camp** (`BHGameMode.cpp:247-266`) — only reachable on a final map with no `ABHEscapeStationManager` (shipped maps have one). Assert/log when missing, or give the fallback a minimal cutscene.
- **Revision exit can be gated to a forced timer-loss by one AFK hall-monitor** (`BHGameMode.cpp:4105-4109`) — a non-participating FakeHunter keeps `StudentsWithoutContribution>0`. Bounded (re-checks each tick; timer ends it). Recommend excluding AFK/unreachable monitors from the gate.

**Long-session stability**
- ★ **`TravelPlayerProgress` grows unbounded** (`BHGameInstance.cpp:988`, `.h:278`) — one entry per distinct player-identity for the whole process; never pruned (siblings `QuestionAttemptHistory`/`PlaytestTelemetryEvents` are capped). Over a multi-hour class with rejoins/lost-tokens/name-changes it accumulates. *Not fixed:* a naive cap risks evicting a current player's banked-progress entry (regressing travel-restore). Recommend pruning pending-reconnect marks aged well past the grace window, plus a generous cap that evicts only non-pending, non-current entries.
- **`UBHAccountSubsystem::Progress.XP/RoundsPlayed` use non-saturating `int32` add** (`BHAccountSubsystem.cpp:1015/1035`) — overflow needs ~7.8M rounds (unreachable). Tidiness: mirror `BHSaturatingAddPoints`.

**Determinism / reproducibility (not live-MP desync — server computes & replicates)**
- ★ **`RoundSeed = FMath::Rand()`** (`BHGameMode.cpp:9867`) — the global, process-local RNG advanced by every other `FRand` call. Downstream derivation is deterministic per-seed, so server/clients agree in a live match, but a *logged* seed can never reproduce a round (replay/QA tooling). Recommend a stable seed source (hash of match/level/stage, or an explicit override).
- ★ **Revision question selection mixes in `FMath::RandRange`** (`BHObjectiveStation.cpp:1888`) — the `LocationSeed` adds `+ FMath::RandRange(0,100000)`, defeating per-seed reproducibility of *which* question shows; the sibling standard-Hunt path (line 1984) omits it. Recommend dropping the random term and folding in `RoundSeed` for per-round variety that's still reproducible.

**Bot AI (behavior, not crashes — and `bUseStateTreeAI=False` by default, so the StateTree-path items are dormant in shipped config)**
- ★ **Survivor bot's own hunter-sighting is clobbered each `Think`** (`BHBotController.cpp:781`) — `GetBotWorldMemorySnapshot` resets `LocalMemory`, dropping a hunter seen via async perception until the throttled (2 s) global re-broadcast; the flee/hide logic briefly reads "no hunter." Bounded by the 2 s re-broadcast. Recommend merging (max-timestamp) instead of overwriting.
- ★ **Bots share omniscient hearing/sight via global stimuli** (`BHBotController.cpp:1037`, `BHGameModeBotServices.cpp:107-154`) — `GetLatestBotNoiseLocation` has no per-bot distance/LOS gate, so a Teacher reacts to noise across walls. Long-standing coordination design; recommend gating consumption by distance/reachability.
- **`InvestigateLastSeen`/`Chase`/`UseScan` fall back to world-origin `(0,0,0)`** (`BHBotController.cpp:435,426,453`) — StateTree-only (dormant). Guard the fallback with the sentinel check.
- **Distance penalised twice (policy scorer vs `AddCandidate`)** (`BHBotController.cpp:1405` + `BHBotPolicySubsystem.cpp:181`) — only when the external weights file is present (else the subsystem self-disables). Apply distance in one layer.
- **StateTree watchdog: permanent fallback + false-positive on stationary intents** (`BHBotController.cpp:479-482,654-672`) — StateTree-only (dormant). Re-arm after healthy ticks; refresh `LastDecisionTime` on the commit-skip path.
- **Ambush built from clobbered/own-only last-seen** (`1059`); **patrol picks the *farthest* point** (`1543`, ping-pongs the map); **Flee/Bait pass the Teacher as `Target`** (routes through the go-to cooldown filter, `1389`); **sentinel mismatch `-999`/`-9999`** (`238` vs `902`) — all Low, AI-feel.

**Movement / anti-camp (server-authoritative; tuning-sensitive → report)**
- ★ **Anti-camp move-burst wipes on a single 0.35 s sub-threshold dip; the 150 u/s threshold excludes walking** (`BHCharacter.cpp:6146,6153`) — a careful, constantly-moving explorer can be treated as camping. Recommend decaying the burst gradually and/or basing "meaningful movement" on accumulated displacement, and lowering the threshold.
- ★ **Special-move forward-clearance sweep is positioned at standing-capsule height for slide/dive** (`BHCharacter.cpp:3168-3174`) — a low obstacle below the shrunk probe passes validation, then the move slams into it; the floor/drop check mixes prone and standing half-heights. Geometry-sensitive → report.
- **Prone has no auto-stand fallback** (`3450`) — under a permanently-low ceiling `SetProneAuthority(false)` always fails ("No room to stand"); add a crouch-stand/nudge escape.
- **Sprint exhaustion threshold differs (client 0.08 vs server 0.05)** (`3005/3524` vs `6496`) — a 5–8 % stutter band; unify the constant.
- Low: crouch speed hardcoded 205 ignoring role/map (`3525`); IsInTeacherBlackout boundary server/client edge desync (`2545`); blackout flicker floor clamp overrides config `0` (`2572`); locker-exit carries anti-camp idle debt (`6186`).

**Atmosphere director**
- ★ **Presence "decay" is a flat per-tick drop, not per-second** (`BHGameMode.cpp:10419`) — the director interval is mode-dependent (7 s normal vs 4 s revision-i3), so presence bleeds off at different real-world rates. Scale by `DeltaSeconds`.
- Low: cue substitution can pick a *more expensive* cue (`BHAtmosphereDirector.cpp:715`); substituted `LightCut` loses its `LockSeconds` (not classified heavy, `677`); self-mutating tie-break (`580`); weighted-variant pick uses `<=` (tiny first-candidate bias, `BHGameMode.cpp:2301`).

**Education node / replication**
- **Team-vote tally hard-capped at 4 choices** (`BHObjectiveStation.cpp:1072`) — latent if a drag/ordering question ever ships >4 candidates (the node would be unresolvable by team vote). Size the tally to `QuestionChoices.Num()`.
- **`MajorityNeeded` counts present-but-silent bystanders** (`BHObjectiveStation.cpp:1081`) — can wedge a discussion (no hard soft-lock; leaving the radius frees it).
- **`QuestionExplanation` replicates to all clients pre-answer** (`BHObjectiveStation.cpp:519`) — for MC the explanation often paraphrases the answer, partially defeating the `COND_Never` answer-hiding. Likely by-design (also shown in feedback); gate behind solved-state if treated as a leak.
- `SubmitNumericAnswer` hardcodes the revision flag `true` (`1440`) — non-issue today (Calculation only exists in revision mode); defensive: compute `BHGS->bRevisionMode`.

**RPC hardening (non-exploitable)**
- `ServerSetProneInputHeld` lacks a `CanAct()` gate and a throttle (`BHCharacter.cpp:6526`); `ServerSetFlashlight` is unthrottled (`6307`) — both effects are inert/edge-triggered. Add for consistency only.

**Config / misc**
- `AndroidFileServer SecurityToken` is committed in `DefaultEngine.ini:142` but `bIncludeInShipping=False` (inert in the Windows game) — rotate if the editor file-server is ever used.
- Polish (from the fix review): the door back-out duplicates `ApplyDoorState`'s `+90°` literal — extract a shared `DoorOpenYawOffset` constant so they can't drift.

## Pass-2 verified-clean (the deep lenses that came back empty)

Physics answers (all 376), adversarial RPC (all 67, no exploit), timer/delegate/async lifetime (no UAF), cross-system data-flow (RoundSeed, contribution gate `[1,6]` everywhere, mastery single-sourced, breaker counts, blackout window, final-escape timings, powerup charges/cooldowns, role/life-state — all consistent), NaN/Inf (no `Acos`/`Asin`, no raw `/Size()`, every replicated meter divisor-guarded/clamped), grading (rotated-correct on every path: MC/standard/drag/diagram-click), anti-gaming hold, spaced-repetition queue, contribution credit (train-bonus correctly excluded), mirror-trap exclusion, the budget/cooldown ledger math, scare target-scoring directions, the intensity-0 "subtle only" contract, and EOS/Steam secret handling.

*Pass 2 generated by 1 orchestrator + 14 parallel read-only auditor passes + direct config/verification reads on 2026-06-05.*

---
---

# Pass 3 — un-audited ground: cook/assets, packaging gate, input, tutorial/HUD (2026-06-05)

Passes 1–2 covered the C++ exhaustively. Pass 3 went after what they *didn't* touch: cook/asset **licensing** correctness, the classroom-package **verifier**, **input-binding** completeness, the **tutorial/HUD** logic, the **scalability/module/plugin** config, and an independent **physics re-derivation**. This pass found the C++/gameplay still clean, but surfaced a cluster of **packaging/licensing/governance** issues that matter before public distribution.

## Pass-3 fixed & committed

- **Hunter-AI StateTree was being cooked out of the package** (`Config/DefaultGame.ini`) — `/Game/BlackoutHunt/AI/ST_BH_HunterAtmosphere` is referenced only via a `TSoftObjectPtr` in the `UBHGameSettings` CDO and no `.umap` hard-references it, so `/Game/BlackoutHunt/AI` (project-owned, on disk) was absent from the AlwaysCook list → a packaged build with `bUseStateTreeAI=True` silently falls back to the C++ bot policy (the StateTree brain works in-editor only). Added the AlwaysCook entry (safe: project asset, no licensing concern; inert today because the flag defaults False). **Commit `bb566a3`.** *(Corroborated independently by the cook-asset and completeness auditors.)*

## Pass-3 — licensing / cook governance (report; author decision + a test cook required)

These are **High-severity for distribution** (shipping unlicensed third-party content is a legal/redistribution risk for a classroom tool) but they are **governance decisions + build-risk**, not blind-fixable: editing the cook list can break the cook (proven below), so they need the author's intent call and a verified cook. The C++ already degrades gracefully (cosmetic fallbacks) when these assets are absent.

- ★ **SmartBasicInterfaces ships its `Demo/` + `Maps/` subtrees, license pending.** The whole pack is AlwaysCook (`DefaultGame.ini:29`); `ASSETS.md:58` *claims* `Demo`/`Maps` are excluded via `DirectoriesToNeverCook`, **but no such NeverCook entry exists** — and `DefaultGame.ini:44-48` explains why it can't: the runtime breaker/station meshes pull sounds/materials from `Demo/Sounds` and `Demo/SCContent/Materials`, so `NeverCook`-ing `Demo` orphans those deps and *fails the cook*. So the doc is wrong and the demo/template content (highest-IP-risk part of a Fab pack) ships. **Resolution = record the license** (the pack's pending license-evidence gate), or migrate only the needed subassets out of `Demo/`. Do **not** add the NeverCook entry the doc implies — it breaks the cook.
- ★ **`/Game/ContainersHouseCH`** — runtime soft-loaded by `CladAuthoredWall` (`BHGameMode.cpp:5578/5583`), **not cooked, not in NeverCook, not in the verifier, no `ASSETS.md` row**. In the package the wall cladding is absent (authored walls render as the bare shell — cosmetic). Licensing risk if ever staged (transitive map pull). Decide: license + AlwaysCook (`/StaticMesh`,`/Materials`) **or** NeverCook + add a verifier rule. A test cook is needed to confirm no authored `.umap` hard-references a clad wall (which would make NeverCook break that map's cook).
- ★ **`/Game/BFHorror/DemoAssets`** — same pattern (the `BluePanel` block material falls back here, `BHBlockActor.cpp:90`; falls back to `VisualTint` when absent). NeverCook + verifier, or drop the fallback.
- ★ **Verifier coverage is narrower than `ASSETS.md` promises** (`Tools/Verify-ClassroomPackage.ps1`) — it guards SCP096/Hider/Hunter/Whisper/Free_Jumpscares/Downloaded/SourceFiles, but **not** `ContainersHouseCH`, `BFHorror`, or the other imported source-pack roots present in `Content/` (`FirstPersonHorrorKit`, `Horror_Template`, `RuinedCrypt`, …) and their `__ExternalActors__`/`__ExternalObjects__` data. `ASSETS.md:57` says the verifier "fails packages that stage raw source folders" — the actual coverage is a fixed denylist, so an un-enumerated pack could stage and pass. Recommend an **allowlist model** (fail any `Content/<Root>/` not on the approved-cook list) or extend the denylist + external-actor/object rules.
- **SCP096 Teacher scare degrades to no creature in the package** — `TriggerRealScp096Scare` (`BHGameMode.cpp:11458`, reachable from the Teacher scare picker) loads the correctly-`NeverCook`'d SCP096 mesh/anim/material (`BHJumpscareVariantLibrary.cpp:365-370`). Licensing is *safe* (not shipped), but the explicit Teacher trigger plays only shake/flash/audio with no creature visual in the package. Recommend gating it to a cooked proxy variant when SCP096 isn't present.
- **`ResidentHorrorV1/Demo/Meshes` clutter props** (`SM_Computer/trashbin/ammoBox`, `BHGameMode.cpp:9017-9021`) — only `/Audio` is cooked; the code comment assumes they cook "transitively via the baked map," unverified. Tinted-cube fallback when absent. Make the cook explicit or accept the fallback.
- **`SecurityCameras/Meshes` + `/Materials`** uncooked (only `/Sounds` cooked) — but this is **by-design** (native prop fallback, `ASSETS.md:137` says don't cook until licensed). Confirmed intended.

## Pass-3 — other reported items

- ★ **`DefaultScalability.ini`: `r.StaticMeshLODDistanceScale` is inverted across buckets** — `1.45 → 1.15 → 1.00 → 1.00` for `[ViewDistanceQuality@0..3]`, i.e. the *lowest* quality bucket (weakest classroom PC) keeps the *most* expensive mesh LODs, while every sibling CVar increases with quality (`r.ViewDistanceScale` 0.55→1.40, `foliage.LODDistanceScale` 0.65→1.00). Hurts the exact low-end hardware the bucket targets. Recommend mirroring the foliage progression (≈0.65, 0.85, 1.00, 1.00). *Perf-tuning value → reported, not edited blind.*
- **Six enabled-but-unused plugins** (`.uproject`: GameplayAbilities, CommonUI, SmartObjects, PCG, GeometryScripting, DatasmithImporter) — no C++/Build.cs usage; add editor/cook cost + package size (possibly a factor in the OOM cooks). Verify no Blueprint/asset references them, then disable with a test build. *(Not edited blind — a referenced-but-disabled plugin breaks the cook.)*
- **`/Game/BlackoutHunt/Art/Diagrams/M_BH_DiagramRT` not cooked** — the train bonus-terminal screen material; only used when `bh.Diagrams.TerminalRT` is on (off by default, the diagram folds into prompt text), so inert in shipped config. Add to AlwaysCook if that CVar is ever enabled.
- **Tutorial UX (all Low, guided-flow polish):** the Counterplay objective beat keeps a labeled "STUDENT" marker pinned to the AI student *while it's hidden in a locker*, contradicting the "students vanish in lockers" lesson (`BHTutorialDirector.cpp:745`); the Capture step advances on a single swing into empty air (`1426`); the Crawl step accepts any prone state, not proximity to the taught duct (`1020`); survivor vitals (FEAR/DREAD/TEACHER) render in the Teacher/Monitor tutorials where they sit dead at 0 (`BHHUD.cpp:1143`); concept-caption gating is question-*type*-based rather than value-based (no leak today, latent).
- **Docs stale:** `ASSETS.md:42` says the authored Maps namespace is "not yet present and not yet in the cook list" — it's present and AlwaysCook'd (`DefaultGame.ini:28`); `ASSETS.md:58` claims `Demo`/`Maps` are NeverCook-excluded (they aren't). `Tab` (question cursor) and `N` (node marker) are real bindings absent from the controls tables.
- **Stray duplicate tree** `D:\BlackoutHunt\BlackoutHunt\` (a second `.uproject` + `Config/`, untracked in git) — not the canonical project; worth removing so tools/cooks don't pick the wrong root.

## Pass-3 verified-clean

Input bindings (every documented control bound; no dead/missing/conflicting/ungated; host/tester/admin/spectator inputs all server-gated), the tutorial state machine (no soft-locks — all 30+ transitions have timeout fallbacks; correct action-detection hooks and server-authority), HUD scales/role-gating/answer-safety, the classroom-package verifier's *implemented* checks (playit.exe SHA-256, EOS/Steam local-INI exclusion via `[Staging]`+filename, and an extensive forbidden-file scanner for pdbs/Saved-data/secrets/source-archives/risky-roots), `BlackoutHunt.Build.cs` module deps (all map to real usage; nothing missing), the `DefaultScalability.ini` bucket *structure* (complete, Lumen off at low buckets), and an independent re-derivation of a Forces/Match/Order physics sample (all correct).

*Pass 3 generated by 1 orchestrator + 3 parallel auditors + direct config/build/asset reads on 2026-06-05. Branch holds 14 fixes across 11 commits.*
