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
