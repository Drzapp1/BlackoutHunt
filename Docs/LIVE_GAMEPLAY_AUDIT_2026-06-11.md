# Live-Gameplay Audit — 2026-06-11 (branch feat/boot-menu-aaa @ 1097679)

> **STATUS UPDATE (same day, fix pass):** every finding below was addressed in the 2026-06-11 fix waves
> unless listed here. Fixed: C1/C2, H1-H8, the reconnect family (plus hunter-demote-on-restore,
> bot-mark skips, token-only mark clearing, exact departure roster), all station/revision items
> (+ review-queue topic-mask skip), all bot items (+ retry pacing, target-side blackout term,
> exit-gate FinalEscape fallback, over-capacity bot-seat admit), all horror items (+ circuit-0
> light-cut replay), train/world/config items, and the prop-hunt set (+ promoted-seeker credit,
> locker release on promote, hide-phase banner, presence seek clock, menu-gated T/V). The two
> long-red baseline tests (cosmetic gating, crawl gate) were stale tests — repaired; crawl
> sheltering is now geometric. An adversarial review wave over the fixes found and fixed 2 further
> regressions (circuit-0 light strand, EM mapping still mappable on TF/distractor rows).
> **Deliberately NOT fixed** (documented decisions): prop disguise fit/depenetration validation and
> the capsule-vs-mesh hitbox tell (need playtest/design input), RuinedCrypt cook wiring (needs the
> arena bake), jukebox tracks (missing CC0 audio assets, not code), true free-camera spectating
> (lives on feat/catch-pressure), movement-prediction architecture items (sprint-toggle/special-move
> rubber-banding — engineering disproportionate without tunnel-play evidence), whisper zero-variant
> warning visibility in Shipping (root cause — the NeverCook line — removed instead), per-window
> light-cut overlap extending a local cut's darkness a few seconds (accepted, end-state correct).
> Suite: 89 tests green at the time of the fix commits (was 66/69).

Full-codebase sweep for anything that would affect live play: 11 subsystem reviews (game-mode
lifecycle, roles/catch, prop hunt, character/movement, controller/HUD, replication/authority,
bots, interactables, revision bank, train lobby, horror/atmosphere, config wiring), plus a fresh
editor build, the full native test suite, and session-log review. Every finding below was
verified against the code (file:line); top findings were independently confirmed by two reviewers.

Build: editor compiles clean. Tests: 66/69 pass — the 3 reds are the known pre-existing baseline
(Account.CosmeticAchievementGating, Movement.CrawlSpaceRoleAndProfileGate, Train.Economy).
Train.Economy was diagnosed: stale test string — commit 19d21b7 reworded the shop summary
"Max charges %d" -> "Max x%d" (BHPowerupShopTerminal.cpp:258) and BHTrainEconomyTests.cpp:66
still expects "Max charges". One-line test fix; purchase logic itself is sound.

---

## CRITICAL — prop hunt is broken for humans as shipped

### C1. Hide->seek transition wipes the entire hide phase
`BHGameMode.cpp:14615` — `StartHuntPhase()` calls `ResetRoleWarmupForLiveRoundStart()`
unconditionally, BEFORE the `bPropHuntMode` branch at 14633. That reset (14442-14458) calls
`ResetRoleWarmupStateForRoundStart()` on every pawn — which explicitly `ClearPropDisguiseAuthority()`
(BHCharacter.cpp:2946) — then `RestartPlayer()`, which **destroys the pawn and respawns it at a
spawn point** (BHGameMode.cpp:1494-1499). Prop hunt rides Prep as its HIDE phase
(BHGameModePropHunt.cpp:397), so at the exact moment the seeker is released every hidden prop is
undisguised, unlocked, and teleported back to the spawn cluster. The hide phase is meaningless.
Bots masked it: `ServicePropHuntBots` re-disguises bots 2-5s into the seek, so bot smoke runs
"work". Zero test coverage of the Prep->Hunt seam.
**Fix shape:** gate the reset — `if (!bPropHuntMode) { ResetRoleWarmupForLiveRoundStart(); }`
(prop hunt has no warmup state to reset; each round already starts from a fresh map travel).

### C2. T and V are dead keys — manual taunt and prop-camera toggle never fire
DefaultInput.ini binds `CycleCrosshair=V` (line 94) and `SpectatorQueueTeacher=T` (line 98) on the
PlayerController (BHPlayerController.cpp:1608,1612) and `PropCameraToggle=V` / `PropTaunt=T`
(lines 115/118) on the pawn (BHCharacter.cpp:2586-2587). The ini comments claim they "co-fire ...
both handlers gate by role" — false under UE input semantics: the controller's InputComponent is
processed before the pawn's and `bConsumeInput` defaults to true (zero overrides in Source), so the
controller consumes the press regardless of what its handler does. A live prop pressing T gets the
spectator-queue RPC (and the toast "Only late-join spectators can request a next-round role");
V cycles the crosshair. Props can never manually taunt (dead scoring mechanic) and never see their
own disguise in 3rd person. Same engine mechanism the repo already documented once for the O key.
**Fix shape:** move the two controller binds to other keys, or bind them with `bConsumeInput=false`.

---

## HIGH

### H1. First-round lockout: `?BHAutoPrep=1` starts Prep 1.0s after host load; slow-loading clients
spend round 1 as spectators
`BHGameModeTrainFlow.cpp:66` appends `?BHAutoPrep=1` on every lobby->first-hunt departure;
`BHGameMode.cpp:1143-1158` fires `StartPrepPhase()` 1.0s after the HOST's BeginPlay with no
client-count gate. Travel is non-seamless, so each client re-logs-in only after its own map load;
`PostLogin` then sees `bCanJoinRound = (RoundPhase == Lobby)` (BHGameMode.cpp:1218) — false in
Prep/Hunt — and parks them Spectator/Captured (1304-1305). No reconnect mark exists to rescue them
(Logout marks only when phase != Lobby; lobby teardown is phase Lobby — BHGameMode.cpp:1414-1419),
and no mid-round role-assign path exists. Any student whose map load runs >(host load + ~1s) misses
round 1; if ALL remote clients miss it in a revision lobby, the host becomes the lone survivor and
the round instantly resolves. Introduced post-0.8.1; never human-tested.
**Fix shape:** hold AutoPrep until the persisted travel roster has re-logged-in (or timeout ~30-60s),
or let `bCanJoinRound` include Prep and assign roles to Prep joiners.

### H2. Prop-hunt taunt/pulse cadence normalized by the wrong clock — pressure curve starts 73% spent
`BHGameModePropHunt.cpp:503` (also 322, 529-532): `Elapsed = (HuntSeconds - RemainingTime) / HuntSeconds`,
but the seek clock is `bh.PropHuntSeekSeconds` (240; set at 410) while `HuntSeconds` stays at the
classroom 900. At seek start Elapsed = 660/900 = 0.73, so forced taunts run ~15s->10s instead of
30s->10s and reveal pulses ~21s->12s instead of 45s->12s — roughly double the designed prop exposure
all round, seeker-favored, untunable via cvars. **Fix:** normalize by the seek length.

### H3. Revision once-per-node cap relaxation is dead code — the "unwinnable bot-lobby" fix never landed
`ABHGameMode::ShouldEnforceRevisionNodeAnswerCap`/`RevisionNodeAnswerCapApplies`
(BHGameMode.cpp:3541-3559) have **zero call sites**; the station gate is unconditional
(BHObjectiveStation.cpp:1036-1046). Each node needs 3 distinct correct answers; <3 human students
=> nodes can never unlock — except that survivor BOTS do fill node quotas (no IsABot gate on the
quota increment, BHObjectiveStation.cpp:1112), directly contradicting the design comment at
BHGameMode.cpp:3556 ("bots are excluded ... cannot fill a node's quota"). Two bugs currently cancel
in bot-filled lobbies: solo+bots is winnable only because bots answer for the class. Wrong answers
also consume the slot (id recorded before grading). Decide the intended rule, then wire
`ShouldEnforceRevisionNodeAnswerCap()` into the station gate and gate (or keep) bot quota credit.

### H4. Eye-adaptation "stare at wall -> scene crushes to black" is live on all shipped indoor maps
The documented two-path fix (BHGameMode.h:60-64) names `ABHPlayerController::ClampIndoorAutoExposure`
as the runtime repair — **that function does not exist anywhere in the repo**. The export-time half
(`AddMoodPass`, BHGameMode.cpp:11040-11046, AutoExposureMaxBrightness 0.20) landed 2026-06-09, but
the authored maps were baked 2026-06-01/02 with the old 0.95 ceiling, and the live authored-map path
never re-runs AddMoodPass. The only runtime guard (`EnsurePropHuntArenaExposureGuard`,
BHPlayerController.cpp:5776-5780) explicitly skips Facility/Substation/Foggrounds/Tutorial.
**Fix:** implement the runtime clamp as documented (client-side PP tweak on map load), per the
baked-atmosphere rule: never re-export the maps for this.

### H5. Numpad 5-9/0 don't type calculation answers — they're tester keys
BHCharacter.cpp:2600-2605 binds NumPad5..9/0 to Tester* functions (duplicated on the controller,
BHPlayerController.cpp:1638-1643, which consumes first); only top-row 5-9/0 reach NumericEntry*
(2616-2621). NumPad1-4 work (answer handlers double as digits), so typing "150" on the numpad
enters "1" and silently drops the rest. For a host in a Test Round the keys instead trigger tester
actions mid-question. **Fix:** route numpad digits to NumericEntry* when the keypad is focused
(mirroring 1-4), and/or move tester keys off the numpad.

### H6. Listen-host jumpscare hit-stop dilates the whole server
`ClientPlayHorrorCue` (BHPlayerController.cpp:9881-9898) calls `SetGlobalTimeDilation(0.12)` for the
"jolt". On the listen host that dilates the authoritative world — `AWorldSettings::TimeDilation`
replicates — so all 10 players hiccup (~70-80ms + latency) whenever the HOST is scared. The teacher
auto-jolt (BHGameMode.cpp:11840-11860, ~135s cadence @13%) makes this periodic in classroom listen
servers where the teacher hosts. Also: the restore lambda skips restore if the PC died mid-window.
**Fix:** skip the dilation unless `GetNetMode() == NM_Client || NM_Standalone`; make restore
unconditional.

### H7. Bot Teachers erase the swing slow-down (and prop-hunt whiff stumble) — dodge counterplay gutted
Humans get the 0.54x speed clamp for the whole 0.62s axe swing via `RefreshMovementSpeedFromState`
(BHCharacter.cpp:4836) plus a per-tick clamp explicitly gated `IsPlayerControlled()`
(BHCharacter.cpp:2250). The bot controller raw-writes `MaxWalkSpeed` every 0.25s think
(BHBotController.cpp:154-160, 1725: sprint 1150), so a bot Teacher is back to full sprint ~0.25s
into a swing (human Teachers are capped at 621 throughout). Same erasure for the prop-hunt
wrong-hit slow. Bot mode prefers a bot Teacher, so the taught "slide/roll the swing" counterplay is
much weaker exactly where most solo players meet it. **Fix:** route bot speed through the same
state-multiplier pipeline (or drop the IsPlayerControlled gate on the tick clamp).

### H8. Bots are inert during FinalEscape (campaign climax)
`BHBotController.cpp:824-832`: Think early-returns for every phase except Hunt (Prep gets patrol).
`EBHRoundPhase::FinalEscape` falls into the return — bot survivors never attempt the final escape
and a bot Teacher poses zero threat during it. Default solo campaign (5 bots) reaches this state
every run. **Fix:** treat FinalEscape like Hunt in the bot brain (flee-to-escape / hunt behavior).

---

## MEDIUM

### Reconnect / lifecycle family
- **Stale reconnect marks defer a decided HunterWin up to ~2 min.** Every ServerTravel teardown in a
  non-Lobby phase marks ALL players for reconnect (BHGameMode.cpp:1414-1427); `ClearReconnectMark`'s
  only call site is the reconnect branch of PostLogin (1296-1299), so normally-landing players keep
  marks for the full 120s grace. `CountReconnectableAliveSurvivors` (BHGameInstance.cpp:1235-1264)
  counts players who are actually present, so the all-caught HunterWin tick defers silently
  (BHGameMode.cpp:15052-15059) after every train hop / mid-round quit. Round timer is the backstop.
  **Fix:** clear the mark on any successful login, and skip marks for currently-connected players.
- **Combat-log escape exploit.** Logout snapshots role/lifestate/points (no pawn location);
  rejoining within 120s restores Alive at a FRESH spawn (BHGameMode.cpp:1261-1266, 1348) — cornered
  survivors can plug-pull to teleport away; the new PlayerState's PlayerId also resets the
  once-per-node answer cap. The round is even held open for them (15052-15059). 15-16yo players
  will find this. **Fix shape:** persist and restore pawn location (or restore as Captured if the
  Teacher was within X uu at disconnect).
- **Round-N snapshot restores into round N+1.** Marks survive EndRound (~60-75s transition < 120s
  grace), so a round-N Teacher who rejoins during round N+1 comes back as a second hunter
  (PostLogin reconnect branch applies in any non-Lobby phase, 1234-1251). **Fix:** invalidate marks
  at EndRound/AssignRoles.
- **Lone-hunter disconnect ends the round instantly — no reconnect grace for hunters.**
  BHGameMode.cpp:15062-15067 (and Logout mirror): survivor branch defers on grace, hunter branch
  resolves SurvivorsWin within 1s. A Teacher Wi-Fi blip kills the round. Also: a hunter leaving
  during Prep produces a Hunt that self-resolves 1s in (no hunter re-guarantee at StartHuntPhase).
- **Prop-hunt reconnect loses PropHuntScore/TimesSeeker.** The mid-round reconnect restore
  (BHGameMode.cpp:1261-1277) omits both fields even though FBHTravelPlayerProgress carries them
  (BHGameInstance.cpp:1051-1052) — corrupts match standings and seeker rotation.
- **RemoveOneBot pops the newest bot regardless of role/phase** (BHGameModeBotServices.cpp:810-834,
  triggered by PostLogin roster clamp 1358) — if it's the bot Teacher, next tick = unearned
  SurvivorsWin. **Fix:** prefer non-hunter bots; never trim the live hunter mid-round.

### Stations / interactables
- **Answer race grades against a swapped question.** `ServerSubmitAnswer` carries only an index
  (BHCharacter.cpp:4930; station SubmitAnswer BHObjectiveStation.cpp:909); any teammate's answer
  reloads the shared replicated question (1152-1153, 1260-1262) which replicates at 4Hz
  (BHInteractableActor.cpp:15) — in-flight answers grade against the NEW question and burn the
  sender's once-per-node attempt. **Fix:** echo `RevisionQuestionStep` in the RPC; reject stale.
- **Correction hold is per-station, not per-player** (BHObjectiveStation.h:323-324, cpp:1017-1024):
  one wrong answer locks the whole team out of that node 3-9s; griefable indefinitely. Contrast the
  deliberately per-player answer throttle right above it.
- **CCTV zone stripes are wrong on every client.** Zone extent is configured server-side only
  (BHCCTVZone.cpp:107-125; no replicated extent — BHCCTVZone.h:76-91), clients keep the ctor
  1200x650 while real detection is up to ~30% longer (Foggrounds 5200/52deg). Survivors get pinged
  standing visibly outside the painted area. **Fix:** replicate the extent, apply in OnRep.
- **SecurityShutter direct interact lacks the throttle + phase gate its siblings have**
  (BHSecurityShutter.cpp:126-173 vs BHSecurityTerminal.cpp:99-104) — circuit-wide spam toggling.
- **BHDoor has no toggle throttle** (BHDoor.cpp:48-105): E-spam = unlimited free swing interrupts
  (380uu, 1.2s recovery each) + Teacher noise-feed flooding in doorway standoffs.
- **Numeric keypad path skips the once-per-node bookkeeping** (SubmitNumericAnswer,
  BHObjectiveStation.cpp:1284-1388) — enables solo node farming at Calculation nodes and the
  review-echo loop below.

### Revision pedagogy/economy integrity
- **Review echo:** a missed question is immediately re-served at the same node (the miss is enqueued
  then peeked back — BHGameMode.cpp:4016, BHObjectiveStation.cpp:1766) while the correction text
  containing the answer is on everyone's screen (1263). Teammates (MC) or the same student (keypad,
  via the numeric bypass) bank full/half mastery off the revealed answer. The code comment at
  1250-1251 documents the opposite intent.
- **Host "Topics" mask is dead:** the "every node is available in revision now" loop re-adds all
  stations after the mask filter (BHGameMode.cpp:11609-11617); masked-off topics still get asked
  but can't raise the class MasteryPercent (mean over enabled topics, 4030) — wasted attempts.
- **Pinned question set with topic gaps** silently serves out-of-set questions and drops that
  node's target to 1 (BHObjectiveStation.cpp:1929-1945; no coverage validation on save/load).
- **EM-spectrum diagram band clicks mis-map on 12 questions** (BHHUD.cpp:3938-3946 maps a band to
  the FIRST choice containing its name): ordering/matching/TF questions submit arbitrary or wrong
  answers on an exploratory click, burning the attempt. Restrict clickable regions to single-band
  identify questions (or map by exact-label match only).
- **Adaptive spotlight targets the just-locked-out student** (BHObjectiveStation.cpp:1049, 1750-1767)
  — next question tuned for someone who can't answer it; others who answer the re-surfaced review
  get bCorrection half-points for a question they never missed.

### Horror / presentation
- **Overlapping global light-cuts restore a stale all-off snapshot** (CutLightsForJumpscare,
  BHGameMode.cpp:13901-13962; reveal charges cut with radius 0 for ~10.25s, 13476/13486): two scares
  inside the window leave the whole map dark until a breaker/exit event re-powers. **Fix:** refcount
  cuts or have the restore re-derive from current desired state.
- **Scare-intensity 0 (revision "scares off") is bypassed** by the teacher auto-jolt
  (BHGameMode.cpp:11842-11860), super-chain stage cues (3382-3474, no SensoryScale), and the
  counter-trap/scare-relay rewards (12884/12921/13032) — opted-out classes still get full
  creature + input lock. Per-player ReducedJumpscares is intact.
- **Host close-up "ghost monster" replicates to bystanders** (BHPlayerController.cpp:9802-9805
  spawns ABHJumpscareMonster with bReplicates=true in the authoritative world when the victim is
  the listen host) — everyone nearby sees a floating creature at the host's face. Spawn it
  bReplicates=false for the cosmetic close-up.
- **Scare lock release stomps the question-cursor input mode** (ReleaseJumpscareInput,
  BHPlayerController.cpp:9518-9521 vs SetQuestionCursorMode 7159-7163): scared mid-mouse-answer =>
  frozen look + invisible cursor until Tab twice. Related: **pawn destroy/swap strands GameAndUI
  cursor mode** — nothing clears it on RestartPlayer/conversion (BHCharacter EndPlay doesn't), and
  C1's hunt-start RestartPlayer makes "in mouse-answer mode when Hunt starts" a common case in ALL
  modes: stuck visible cursor + Tab inverted until Escape-menu round-trip. Clear cursor/emote modes
  in controller OnUnPossess/OnPossess.
- **Prop hunt: presence whisper not suppressed** (UpdatePresenceDirector has no bPropHuntMode gate,
  BHGameMode.cpp:11770 vs 11777-11782) — replicated drone SFX spawns behind hiding props and can
  beacon them to the seeker (contradicts the mode's own "no scares on props" rationale).

### Bots (fairness/fun)
- **Light-blind vision both ways** (CanSeeCharacter, BHBotController.cpp:1913-1949 — no
  blackout/fog/light term): bot survivors ignore the Teacher's blackout power entirely; bot Teachers
  spot lights-off survivors at full 2800uu in pitch dark/fog. The prop-hunt disguise gate proves the
  pattern exists; classic mode never got one.
- **Locker wallhack strongest on Normal/Easy** (FindSuspiciousLocker reads real occupancy,
  BHBotController.cpp:2091-2095; empty-locker bluffs only on Hard) — "bot walks to a locker" is a
  guaranteed find tell.
- **Threat scoring reads true hunter positions through walls** (GetThreatPressureForLocation,
  BHBotController.cpp:2167-2181) + team-wide sighting telepathy — ambushes never work on bots.
- **Bots can't operate doors at all** (zero door references in BHBotController.cpp; doors block) —
  closing a door permanently breaks a bot chase / locks bot survivors out of objectives.
- **Revision bots camp spent nodes** (no memory of node rejection; HandleStationAnswer ignores the
  result, BHBotController.cpp:2374) — they retry a node forever and exclusive-claim it.
- **Prop-hunt bot props drop noise decoys at their own hiding spot** (survivor flee brain still
  runs; BotDropDecoyOrTrap 1291-1298 has no prop-hunt gate) — audible self-beacon.

### Train / lobby
- **Doors-close auto-board yanks lounge sitters** — aboard-check X band is the OLD interior
  (+/-3720, BHTrainIntermissionManager.cpp:523-526) but the tube was extended to +/-5250 with lounge
  cars at +/-4500 (BHGameMode.cpp:8540, 8904): everyone in a lounge is teleported mid-train at every
  doors-close; chair-locked sitters arrive movement-frozen until they press Jump. Widen the band.

### Settings / limits
- **Bot-count UI hardcodes 11 in four places** (SBHMainMenu.cpp:7147, 10961 "Bots %d/11";
  BHPlayerController.cpp:2946, 3822) while the server clamp is MaxPlayers-1 = 31
  (BHGameModeBotServices.cpp:211) — the BHGameSettings.h:28-32 comment claims this exact bug was
  fixed; the menu/controller sites never converted. Stepper can't pass 11; Fill spawns 31 ("31/11").
- **Uncommitted tunnel-health check can report a dead tunnel as healthy**
  (BHNetworkSupport.cpp:666-693, working tree): 90s window gates only file mtime while the verdict
  is `Contains()` over the whole accumulating log — markers from a previous session keep matching
  while the agent spams reconnect errors. Host announces "students join via the classroom endpoint"
  while joins fail. Scan only recent lines (or a timestamped marker). New logic has no test coverage
  (the helpers are anonymous-namespace statics).

---

## LOW (verified, brief)
- Post-round Teacher swing still docks 25% points / awards +40 in a decided round (resolve path has
  no phase check — BHCharacter.cpp:9501-9506, BHGameMode.cpp:1710-1714); persists via travel.
- Reconnect inside the 8s win window double-fires ClientRecordRoundResult => double XP (1291-1294).
- Mid-round late joiners are frozen VISIBLE statues at survivor spawns (normal pawn, CanAct false,
  blocks ECC_Pawn; HUD says "You're spectating. Watch how survivors...") — no engine spectator used;
  multiple joiners stack. True-spectate exists only on feat/catch-pressure.
- Slide ending under low geometry restores the standing capsule unswept (BHCharacter.cpp:4646-4660,
  4880-4884; the slide validates at prone height 34) — wedge/jitter under desks/lips; crawl volumes
  self-heal, generic furniture doesn't. Force prone when !CanStandFromProne().
- ServerSetSprinting swallows the RELEASE while CanAct() is false (locker) => stale sprint flag =>
  phantom roll-out + noise on locker exit (BHCharacter.cpp:9104-9114 vs 2798-2811). Process false.
- Sprint/special-move speed changes live outside saved-move prediction => rubber-banding on sprint
  toggle/exhaustion and during rolls for remote clients, scales with ping (3564-3573, 4326).
- Silent slide-stop crouch can't stick for remote clients (server Crouch() reverted by the client's
  next move flags, 4662-4666) — feature is host-only.
- Special-move cooldown meter always reads "ready" on remote clients (3166-3170; not replicated).
- Seat-locked players' held movement keys fight the server freeze (~RTT jitter; 3242-3252) — the
  prop-lock path got an OnRep fix (7544), seats didn't.
- Remote players' flashlight beams render pitch-flattened (no RemoteViewPitch use; 6972).
- Hunter-side: monitor "false ping" decoys etc. fine; survivor bots are never fooled by the Hall
  Monitor (role-omniscient IsAliveHunter check) — the Monitor's bluff does nothing to bots.
- ConnectionTimeout=45 is SHORTER than the engine's 60 default while the ini comment says
  "lengthen the base timeouts" (DefaultEngine.ini:76-82 vs BaseEngine.ini) — tunnel stalls 45-60s
  drop where stock UE survives.
- F11 atmosphere-console bind is unreachable (bF11TogglesFullscreen=True consumes it first);
  Pause/Backslash alternates work.
- PowerSwitch/SecurityTerminal labels not replicated — remote clients see default prompts.
- electricity_match_03 is unfailable by drag (both pieces are the identical string, bank :301).
- No cross-station dedup: two same-type nodes can serve the identical question; double mastery.
- Slot machine can display negative credits for one spin (re-buy check before deduct, :181-186).
- Chess/TTT/othello/C4 PvP have no turn timer (blackjack does); chess PvP lacks the no-legal-move
  end the AI path has — AFK opponent soft-stalls a table (recoverable by walking away).
- Jukebox playlist is EMPTY: all four track paths fail to resolve — even SW_LofiTrain, which the
  code comment says ships, is absent from Content (BHTrainJukebox.cpp:23-28; Content/...Audio has
  only EerieLobbyLoop/MenuClick/TerrifiedScreamFaint). Dead lobby interactable; degrades silently.
- Whisper jumpscare folder is in DirectoriesToNeverCook (DefaultGame.ini:49) while code+HEAD commit
  assume it ships — currently moot (folder absent) but a workflow trap once art is imported.
- Behind-you scare payoff still fires on a player caught mid-hold (spectator gets the slam).
- Manual taunt (once T works) requires no disguise: undisguised runner farms ~187 pts/min vs the
  60/min survival trickle — playtest eye.
- Prop disguise has no fit/placement validation: near-max props interpenetrate walls (position leak
  to adjacent rooms, or bury-the-mesh near-invisible hiding); full-size invisible capsule on small
  props = shoulder-bump tell + air-tags (genre problem; 16cm min size invites it).
- Prop-hunt capture with a null controller on the caught prop ends the round HunterWin with props
  still hidden (BHGameModePropHunt.cpp:475-486) — degrade to "skip infection" instead.
- Catch-pressure cvar surface (bh.RecaptureTax*/bh.Detention*/bh.ClassLifelines*/bh.ClimbBack*) is
  declared with live-sounding help but has zero consumers on this branch (BHGameMode.cpp:3634-3744;
  live penalty hardcodes 0.25 at :1710) — host tuning silently no-ops. (The feature lives un-merged
  in the feat/catch-pressure worktree.)
- RuinedCrypt arena is menu-offered but not in the EOS cook map list — packaged builds silently
  fall back to Facility (button copy admits editor-only; fallback is silent in-game).
- `[SystemSettings]` pins r.Streaming.PoolSize etc. above scalability priority — sg.TextureQuality
  buckets can't move them (mitigated: the game's own settings UI re-sets via console priority).

## Repo hygiene (not gameplay, worth knowing)
- `D:\BlackoutHunt\BlackoutHunt\` is a STALE nested clone (own .git, HEAD at 0.7.1-era ea6a9b9,
  origin Drzapp1/BlackoutHunt). Untracked, outside the cook path, but a strong
  wrong-checkout/wrong-script footgun. Delete or move out of the repo.
- ~25 `*.bak-20260524-*` files in Source/ (not compiled; clutter).
- Uncommitted working tree: BHNetworkSupport.cpp tunnel-health logic (see Medium), DefaultEngine.ini
  whitespace, Package-Windows-EOS.ps1 `-Fast` switch (8 parallel actions + iterative cook — fine for
  playtest cooks; keep default path for ship).
- Session logs show only benign errors (EOS OAuth dev noise, missing profiler DLLs). The missing
  FreeAnimationLibrary clips are KNOWN and fallback-handled in code (BHCharacter.cpp:742-744).

## Suggested fix order
1. **Before the prop-hunt playtest:** C1, C2, H2 (mode is unplayable/unbalanced without them), plus
   the prop-hunt reconnect score loss if time permits.
2. **Before the next classroom session:** H1 (first-round lockout), H3 (+bot-quota decision),
   H4 (exposure crush), H5 (numpad), H6 (host hit-stop).
3. **Bot-mode quality pass:** H7, H8, RemoveOneBot, doors, vision gates.
4. **Reconnect family** (stale marks, combat-log, round-N restore, hunter grace) as one coherent PR.
5. Station/interactable mediums; horror mediums; train auto-board; UI clamps; then lows
   opportunistically. The Train.Economy test red is a one-line fix any time.
