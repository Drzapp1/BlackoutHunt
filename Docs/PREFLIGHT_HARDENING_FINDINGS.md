# Blackout Hunt — Pre-Flight Hardening Findings & Patch Manifest

Generated during a live-readiness hardening pass (0.6.0-beta.1, 32-player classroom). A second agent was
concurrently rewriting the core gameplay files, so fixes are split: **non-contended files were patched
directly**; **fixes that belong in the co-agent's hot files are listed here to apply/verify**.

Hot/contended files (co-agent actively rewriting — do not edit blind, verify my edits survived):
`BHGameMode.cpp/.h`, `BHGameInstance.cpp/.h`, `BHPlayerController.cpp/.h`, `BHPlayerState.cpp/.h`,
`BHObjectiveStation.cpp`, `BHCharacter.cpp/.h`, `BHGameState.cpp/.h`, `BHHUD.*`, `SBHMainMenu.*`,
`BHTrainBonusQuestionTerminal.cpp`, `BHTutorialDirector.*`, `BHBotController.*`, `BHGameModeBotServices.cpp`.

---

## A. Fixes I APPLIED in contended files — VERIFY THESE SURVIVED (re-apply if clobbered)

| Bug | File(s) | What to confirm is present |
|-----|---------|----------------------------|
| 8 RevisionStats info-leak | `BHPlayerState.cpp` GetLifetimeReplicatedProps | `RevisionStats` and `RevisionReviewQueue` are `DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)` (not plain DOREPLIFETIME) |
| 9 reconnect token (secure reconnect) | `BHGameInstance.h/.cpp`, `BHPlayerState.h`, `BHPlayerController.h/.cpp`, `BHGameMode.h/.cpp`, `BHNativeProofTests.cpp` | See "Bug 9 components" below — the whole token chain must be intact |
| 10 re-host race | `BHGameInstance.h/.cpp` | `bPendingOnlineHostCancel` member + teardown in `OnCreate/OnStartOnlineSessionComplete` + set in `LeaveOnlineSessionIfActive` |
| 11 bonus-terminal answer leak | `BHTrainBonusQuestionTerminal.cpp` LoadQuestion | choice-rotation block (rotates `ServerQuestion`/`Question` choices; correct index only in `ServerQuestion`) before the `CorrectChoiceIndex=0` scrub |
| 12 captured survivor keeps completing objective | `BHObjectiveStation.cpp` Tick worker-prune | prune drops workers where `!WorkerPS->IsAliveSurvivor()`, not just `!IsValid()` |
| 17 negative-modulo | `BHGameMode.cpp` SelectPromptLine | `static_cast<uint32>(Salt) % ...` (not `FMath::Abs(Salt) %`) |

**Bug 9 components (must all be present):**
- `BHGameInstance.h`: `FString ReconnectToken;` in `FBHTravelPlayerProgress`; `ClientReconnectToken` member + `SetClientReconnectToken`/`GetClientReconnectToken`.
- `BHPlayerState.h`: server-side `FString ReconnectToken;` (non-replicated).
- `BHPlayerController.h/.cpp`: `UFUNCTION(Client,Reliable) ClientReceiveReconnectToken` + impl storing it in the GameInstance.
- `BHGameMode.cpp`: `InitNewPlayer` override parsing `?BHReconnect=`; PostLogin issues a fresh `FGuid` token when empty + pushes via `ClientReceiveReconnectToken`. `#include "Misc/Guid.h"`.
- `BHGameInstance.cpp`: `JoinGame` appends `?BHReconnect=<token>`; `PersistTravelPlayerState` stores it; `TryGetReconnectProgress` matches by **token** (not display name); `ClearReconnectMark` matches by token.
- `BHNativeProofTests.cpp`: `ReconnectRestoresRoleWithinGrace` test uses tokens + asserts a same-named imposter without the token is **rejected**.

---

## B. Contended-file fixes — NOW APPLIED (2026-05-31, after "no other agent on repo")

The High/Med items below were applied directly once the co-agent stood down. Re-verified by a clean
`BlackoutHuntEditor` build + the full `Automation RunTests BlackoutHunt` suite (see Verification status).

### ✅ Bug 21 — `ServerSetSprinting` missing `CanAct()` gate  (High, cheat) — APPLIED
`BHCharacter.cpp` `ServerSetSprinting_Implementation`: added `if (!CanAct()) { return; }` as the first
line (mirrors `ServerSetProne`). A client can no longer sprint while input-frozen/captured/in the
final-escape cutscene.

### ✅ Bug 22 — Input-freeze / jumpscare lock not enforced server-side  (High, cheat) — APPLIED
`BHCharacter.cpp` authoritative `Tick`: added a central enforcement block. When `bPlayerInputFrozen`
(or `bHunterInputFrozen` for hunters) is set and the pawn is in normal play (`!bOutOfPlay`,
`!bHiddenInLocker`), the server `StopMovementImmediately()` + `DisableMovement()`s the CMC and
re-asserts it every tick (guarded on `MovementMode != MOVE_None`, so it's cheap and survives an
overlapping jumpscare restore). On unfreeze it restores `MOVE_Walking` + `ApplyMovementSpecialState()`
only if the pawn is back to `CanAct()` (so capture/locker that took over mid-freeze keep ownership).
New server-only member `bMovementFrozenByServer` (reset in `ResetRoleWarmupStateForRoundStart`).
Centralised in the character (reacts to the GameState flags) rather than touching every freeze caller.

### ✅ Bug 20 — `CutLightsForJumpscare` untracked restore timers  (Low) — APPLIED
`BHGameMode.cpp`: both jumpscare restore timers (the light re-power lambda in `CutLightsForJumpscare`
and the movement/input un-freeze lambda above it) now use `BindWeakLambda(this, ...)` instead of a bare
`BindLambda`, so `EndPlay`'s `ClearAllTimersForObject(this)` cancels them on level transition and the
weak-`this` guard prevents a fire after the game mode is gone.

### ✅ Bug 23 — `bProneInputHeld` never replicated to authority  (Med, functional) — APPLIED
Added `UFUNCTION(Server, Reliable) ServerSetProneInputHeld(bool)` (mirrors `ServerSetSprinting`); called
from `StartProne`(true)/`StopProne`(false). The impl mirrors `StopProne`'s local rule (releasing mid-slide
clears `bSpecialMoveEndsProne`). `FinishSpecialMoveAuthority` now sees the real prone-hold for remote
clients, so slide→"hold to stay prone" works for everyone, not just the listen-server host.

### Bot-AI performance — 3 High (only when bots fill empty slots) — STILL DEFERRED (operational mitigation stands)
`BHBotController.cpp` + `BHGameModeBotServices.cpp`:
1. Per-think full-`TActorIterator` scans multiplied per objective candidate (`CountNearbyAllies`/`GetThreatPressureForLocation`, ~1064/1369) — O(bots × objectives × all-actors), 4×/s.
2. `GetBotApproachPoint` (`BHGameModeBotServices.cpp` ~475) does up to **80 synchronous `FindPathToLocationSynchronously`** + 80 actor scans per call, every think while moving.
3. Policy-brain `MoveTowardActor`/`MoveTowardLocation` re-issue `MoveToLocation` every think with no "same goal, already moving" guard (the StateTree brain has `MinCommitInterval`; the default policy brain doesn't).
Plus **Med**: captured (not destroyed) survivors aren't released from `BotObjectiveClaims`/`BotTargetCooldowns` (`IsBotTargetStillUseful` returns true for any `ABHCharacter`).
**Operational mitigation for the live session: run a full 32-human class with bot-fill OFF (0 bots) — none of these trigger.** Fixes: cache the approach point per (bot,target) for a few seconds + cap to one sync path; iterate `PlayerArray` not `TActorIterator`; add a MoveTo re-issue guard; release claims/cooldowns on capture.

---

## C. Fixes APPLIED in non-contended files (safe, done)
- **Bug 5** privacy: `BHFeedbackSubsystem.cpp` — classroom mode anonymizes identity in off-machine feedback + honest privacy text.
- **Bug 6** answer-option distinctness: `BHRevisionQuestionBank.cpp` (both validators) + `BHGameModeRevisionTests.cpp` (new assertion) + stale count string.
- **Bug 7** button-press SFX multicast: `BHButtonModule.cpp/.h`.
- **Bug 15** credential `.bak`/`.tmp` leak: `BHAccountSubsystem.cpp` (`ForgetLocalCredential` + `ResetLocalClassroomData`).
- **Bug 18** lighting-circuit grief: `BHPowerSwitch.cpp/.h` — phase gate + 1s anti-spam cooldown.
- **Bug 19** security-circuit grief: `BHSecurityTerminal.cpp/.h` — phase gate + 1s anti-spam cooldown.
- **Bug 14** sliding-gate grief (2026-05-31): `BHSlidingGate.cpp/.h` — added `CanInteract` (alive + non-Lobby/results phase) + 1s `LastToggleServerTime` cooldown, identical to the power/security pattern. Closes "any alive player flaps any gate any time".

## D. Low / deferred (documented, low priority)
- **13** Locker `Occupant` replicates the occupant's identity (not just a bool) to all clients — needs a `bool bOccupied` + client-side "am I the occupant" derive. (`BHLocker.cpp`)
- **16** `SaveCustomPreset` non-atomic write (recoverable via backup-on-next-save). (`BHLessonPreset.cpp`)
- **24** Non-swept low-capsule restore can clip in tight geometry. (`BHCharacter.cpp` ~3195)
- **25** Prone-toggle capture-evasion refresh has no cooldown. (`BHCharacter.cpp` ~5998)

## E. Operational mitigations for the live session (no code needed)
- **Bot-fill OFF (0 bots)** with a full class → neutralizes all bot-perf risk.
- DX11 pre-menu crash on basic-display/RDP GPUs is engine-level — **pre-audit lab GPUs** (documented in `Builds/Windows/GPU-TROUBLESHOOTING.txt`).
- Feedback ships data off-machine to a Google endpoint; classroom mode now anonymizes identity, but `bEnableFeedback=False` fully disables it if desired.

## F. Verified-correct (audited, no bug) — for confidence
Hall-monitor contribution gate (frozen per round, displayed==enforced); capture authority + double-capture
guard; breaker concurrent-repair; mastery thresholds 70/50 server-authoritative; spaced-repetition both
directions; anti-gaming mastery decay; captions default-on; missing-audio fallbacks; 32-pawn
replication/relevancy + net rates + 30Hz tick; EndRound re-entrancy + capture-vs-escape determinism;
objective-station answer index bounds + `CorrectAnswerIndex` `COND_Never`; all host/admin/tester Server RPCs
gated by `RequireHostAdmin`; spectator/captured can't act; economy/powerup overspend/underflow/charge caps;
shutter-crush safety; CCTV reveal recipients/cooldowns; jumpscare null/stale/spawn-resolver safety;
reduced-flash/jumpscare/shake accessibility enforced client-side; persistence atomic-write + backup-aware load.

## G. Wave-4 (network + diagram) findings & the Bug 9 CORRECTION

- **CRITICAL — Bug 9 was incomplete and had REGRESSED reconnect; now corrected.** The token was appended only
  in `UBHGameInstance::JoinGame` (Exec console only). The menu JOIN button uses `ABHPlayerController::JoinGame`,
  which traveled with no token — so after the Bug 9 change, `TryGetReconnectProgress` (now token-keyed) always
  bailed and a dropped student lost role/points. **FIXED** by appending `?BHReconnect=<token>` in
  `ABHPlayerController::JoinGame` too (`BHPlayerController.cpp` ~1635). NOTE: this one edit is in a *contended*
  file — made deliberately because it corrects a self-introduced regression that would otherwise break reconnect
  live. **VERIFIED present 2026-05-31** (`BHPlayerController.cpp` ~1648-1656 appends `?BHReconnect=%s` from
  `GetClientReconnectToken()`; `ClientReceiveReconnectToken_Implementation` intact) — the whole Bug 9 chain survived.
- **Bug-net-3 (High) — stale tunnel reported "ready" / oldest address. FIXED (non-contended `BHNetworkSupport.cpp`):**
  `StopInternetTunnel` now deletes the agent log; `TryExtractTunnelAddressFromLog` now returns the NEWEST
  allocation, not the first. Residual (documented, not fixed): an agent that *crashes* without a game-stop, or an
  externally-run playit that dies, can still leave a stale log — harder to disambiguate from external playit.
- **Bug-net-2 (High, partly mitigated) — TODO (contended):** after a connection-failure bounce the host address
  isn't remembered; JOIN re-suggests the student's own LAN IP. Mitigated for the default classroom because the
  Playit endpoint is in `ClassroomJoinEndpoints` (pickable from the JOIN LIST). Full fix: persist a
  `LastJoinAddress` and prefill it (`SBHMainMenu`/`BHGameInstance`).
- **Bug-net-4 (Med) — APPLIED 2026-05-31 (`BHGameInstance.cpp` `HandleEngineNetworkFailure`):** added an
  `ENetworkFailure::FailureReceived` case → "The host turned you away. The class may be full (32 players max) or
  the round may have locked - ask the teacher, then reopen JOIN and try again." (honest: covers full *and* locked).
- **Bug-net-5 (Low/Med) — TODO (contended):** no GameInstance-side watchdog on `bOnlineSessionBusy`; if an EOS/
  Steam completion delegate never fires on a locked-down network it latches true and blocks all online ops. Add a
  timer fallback that clears it. (EOS/Steam lobby path only; direct-IP/tunnel classroom path unaffected.)
- **Bug 26 (Med) — APPLIED 2026-05-31 (`BHHUD.cpp` `ResolveDiagramTexture` + `BHHUD.h`):** added a
  `TSet<FString> MissingDiagramTexturePaths` sentinel. A missing path is now recorded once and short-circuited;
  only a genuine load populates the weak-ptr cache. No more per-HUD-frame synchronous `LoadObject` on a missing
  teacher-JSON diagram path. Narrow trigger (built-in bank never sets image paths); render math already hardened.
- **Bugs 27/28/29 (Low cosmetic, diagram):** MomentBeam givens in inconsistent label slots; IVGraph point plotted
  slightly off the ohmic line; EnergyChain box gap can go negative at extreme low res. Cosmetic only.

## H. Wave-5 (long-session / 60-min stability) — essentially clean
Audited unbounded growth, resource leaks, audio-thread safety, float accumulation. Codebase is well-disciplined
(prune loops + Reset on logout/round-reset across every player/actor-keyed container; spawned actors carry finite
LifeSpan; render targets/MIDs reused not churned). Two Low/latent findings only:
- **Bug-ls-1 (Low):** `PlaytestTelemetryPlayerTagsById` (`BHGameInstance.cpp` ~1182) is the lone player-keyed map
  never pruned on logout — grows ~1 small entry per unique PlayerId (reconnects/bots). ~tens of KB over hours;
  negligible. Polish fix: prune in `Logout` like `SpectatorEncouragementTimes`.
- **Bug-ls-2 (Low):** `BHSynthComponent` reads `BaseFrequency/Volume/...` on the audio thread while `Configure()`
  writes on the game thread (non-atomic). In practice configured once before `Start()`; tear-free for aligned
  floats on x64 → at worst a 1-frame glitch. Polish fix: make the shared scalars `std::atomic<float>`.
Conclusion: **no High/Med long-session crash/leak/growth bug** — the 60-minute host session is stable.

## I. Wave-6 (packaging / cook / deployment safety) — checked vs the actual cooked manifest
- **APPLIED (non-contended) — SmartScreen/AV deployment gap (Med):** added a "Windows SmartScreen And Antivirus"
  section to `Docs/CLASSROOM_DEPLOYMENT.md` (Mark-of-the-Web unblock, SmartScreen click-through, AV allow-list,
  pre-stage on lab image, signing recommendation). The unsigned exe + bundled `playit.exe` + zip is a likely
  first-contact failure on locked-down school PCs.
- **Feedback endpoint embedded in pak (Med, operational/known tradeoff):** `DefaultGame.ini:146`
  `FeedbackBackendBaseUrl` (Google Apps Script `/exec`) ships to every student — an unauthenticated capability URL
  (anyone with the pak can POST to the owner's Sheet/inbox; no rate-limit). Intended live path; operator can set
  `bEnableFeedback=False` to disable. Decide acceptability.
- **ContainersHouseCH not cooked (Low):** `BHGameMode.cpp:5346/5422` runtime-loads `/Game/ContainersHouseCH/`
  cladding for procedural Facility/Substation, but it's not in AlwaysCook → bare-cube walls + per-wall soft-load
  WARNING spam in the shipped build (graceful, non-fatal; may trip the runtime-log gate). This is the *pending* v2
  kit (per project notes, not shipping yet) → either cook it when ready (`+DirectoriesToAlwaysCook=...ContainersHouseCH`)
  or gate the clad call (`bIndustrialLevel`) off until then. Verify the smoke/log gate isn't already flagging it.
- **Stray `SecurityToken` in cooked `DefaultEngine.ini:123` (Low):** AndroidFileServer token ships in cleartext
  config; inert on Win64 Shipping but the secret-scanner (filename-based) misses it. Recommend deleting the line/section.
- **Unfenced plugins (Low):** `DatasmithContent` + `GeometryScripting` in `.uproject` have no `Editor` allowlist →
  cook into the student client as likely dead weight. Add `"TargetAllowList":["Editor"]` or remove to shrink the pak.
- **`-BHAutomation` compiled into Shipping (Low, contended `BHGameInstance.cpp:140`):** a student could pass
  `-BHAutomation=1 -BHAutoHost=...` to the shipping exe to auto-host/skip-ready. Minor under the classroom threat
  model (host-only admin); gate behind a build flag if undesired. (This is also *why* tested==shipped holds.)
- **Verifier coverage (defense-in-depth):** `Verify-ClassroomPackage.ps1` required-cook list omits dirs the C++
  string-loads (SecurityCameras/Meshes|Materials, Art/Diagrams) — currently OK by transitive refs / CVar-gating, but
  the verifier can't catch a future "silent placeholder" regression. Consider asserting every string-loaded dir is
  AlwaysCook'd or has a documented fallback.
- **OK:** editor-only plugins correctly fenced; no EOS/Steam creds in shipped config; tested artifact == shipped
  (smoke runs the Shipping exe); SecurityCameras content cooks transitively (CCTV ships fine).

## J. Soak-discovered HIGH bug (2026-05-31) — BHStaticBlockField replication exceeds max bunch
The full cook+soak gate (Development, 2 clients) surfaced a **pre-existing, High-severity multiplayer bug**
that unit tests can't catch:

- **Symptom (host log):** `Ensure condition failed: !IsBunchTooLarge(Connection, Bunch)` /
  `Attempted to send bunch exceeding max allowed size. BunchSize=152269, MaximumSize=65536. Actor: BHStaticBlockField`.
- **Cause:** `ABHStaticBlockField::StaticBlockSpecs` is a `DOREPLIFETIME` `TArray<FBHStaticBlockSpec>` (~55 B/spec).
  A procedural facility is ~1800 blocks → ~100-152 KB serialized as one property → one bunch over the 64 KB
  cap. `UChannel::SendBunch` (`DataChannel.cpp:1259`) does `Bunch->SetError()` and drops it → the spec array
  **never reaches clients**, on every join.
- **Live scope = REAL:** shipping default `bUseAuthoredLevels=false` (`BHGameSettings.cpp:182`) → procedural
  facility path → `BHGameMode` spawns/fills `BHStaticBlockField` (`~8803/8849`). So in a live 32-player class,
  every student would fail to receive the procedural level geometry. (The baked-level path, if
  `bUseAuthoredLevels` is flipped on with baked maps for all live levels, sidesteps this entirely.)
- **Second, related bug (same actor):** client logs spam `AttachTo: '...BHStaticBlockField.Root' is not
  static, cannot attach '...StaticBlock_*' which is static. Aborting.` — the Root is the default (movable)
  `USceneComponent` but the generated ISM components are `SetMobility(Static)`; attaching a Static child to a
  non-Static parent aborts. Root should be `EComponentMobility::Static` (or build blocks before/with matching
  mobility). Affects host and client.
- **Fix options (decision needed):** (a) ship baked (`bUseAuthoredLevels=true`) if all live levels are baked →
  bug moot; (b) keep procedural and fix replication — regenerate on clients from a compact seed (best
  bandwidth, biggest refactor), FastArraySerializer, or chunked transfer; plus set Root mobility Static.
- **NOT a regression from the hardening/feature work** — `BHStaticBlockField` was untouched this session.

## Verification status — GREEN (2026-05-31, including the contended-file batch)

**Run 1 (non-contended patches):** clean build + full suite, EXIT 0, zero failures. Covers bugs
5,6,7,8,9+net-1,10,11,12,15,17,18,19,net-3.

**Run 2 (2026-05-31, after the co-agent stood down — the section-B contended batch):** clean
`BlackoutHuntEditor` build **Result: Succeeded** (incremental, 64.9 s, zero build errors) + full
`Automation RunTests BlackoutHunt` suite **TEST COMPLETE. EXIT CODE: 0**, zero failed tests. The two
touched tests pass again (`ReconnectRestoresRoleWithinGrace` = Success, `RevisionDiagramAnswerSafety` =
Success). So the newly-applied fixes **21, 22, 20, 23, 14, net-4, 26** all compile and are regression-free,
and net-1 was confirmed present.

**Remaining (not applied this pass):** net-2 (persist `LastJoinAddress`), net-5 (online-busy watchdog —
EOS/Steam path only, classroom uses direct-IP/tunnel so unaffected), bot-perf ×3 (operational mitigation:
run 0 bots for a full class), and the section-D Lows (13, 16, 24, 25).

**Full cook+soak `Run-StabilityGate.ps1` not yet run this pass.** Build+unit-tests are the right gate for
these code changes (all GREEN). The full gate adds a ~30-min Win64 cook + 25-min normal soak + 10-min
degraded soak. ⚠️ Watch: its runtime-log gate fails at ≥12 soft-load warnings, and the procedural Facility
runtime-loads uncooked `/Game/ContainersHouseCH/` cladding (section I) — so the gate may trip on that known,
non-fatal item. To get clean signal on everything else, either cook the kit
(`+DirectoriesToAlwaysCook=...ContainersHouseCH`) or run with `-RuntimeLogAllowedPattern "ContainersHouseCH"`.
