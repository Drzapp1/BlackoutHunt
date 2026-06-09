# Blackout Hunt — Bug Hunt & Release-Readiness Findings (2026-06-08)

*A targeted hunt of the **new work since the last full audit (2026-06-05)** plus a release-readiness pass for the classroom Windows build (v0.8.x / 0.9). Goal: find every defect in the June additions, re-confirm the previously-clean core didn't regress, and verify the build is fit to ship.*

**Branch:** `feature/easter-eggs-2026-06-06`  ·  **Project:** `D:\BlackoutHunt` (UE 5.7)
**Method:** **Static analysis, read-only** — 1 orchestrator (personal deep-read of the 3 train minigames + crash/disk forensics) + **6 parallel read-only auditor passes** (achievements/cosmetics, movement/camera, easter-eggs/roof/train, UI-theme/menu/gating, cook/licensing, core regression). **No engine build or cook was run** — the project is under **active concurrent edit + build by other agents** (last build 00:41, source files rewritten through 00:36–00:41), so a second build would corrupt theirs. Every fix below is reasoned to compile and be behaviour-safe but is **not compiled/verified** here.
**Scope:** the surface added/changed since 2026-06-05 (commits `2437114`..`f0d0685`): train minigames (chess/TTT/blackjack), "functional sit", forward-somersault camera, achievements/cosmetics/UI-theme/per-part avatar colour, easter-egg roof system — plus cook/licensing governance and a core-regression re-check. The pre-June core was exhaustively audited in `BUG_HUNT_FINDINGS_2026-06-04.md` (5 passes); known/fixed items there are not re-reported.

---

## Headline

**The June work is well-engineered and the build is close to release-ready. No Critical. No High gameplay/crash/anti-cheat bug. No classroom host/student safety leak. No win/loss or replication regression.** The server-authoritative discipline of the core extends cleanly into the new systems (every minigame/sit/cosmetic action re-validates on the server; cosmetics are local-only with no gameplay effect; new private replication stays `COND_OwnerOnly`).

The blockers that remain for a **public** classroom release are **not gameplay bugs** — they are (1) one **licensing-attribution** gap (fixed here), (2) **environment/disk** pressure that is corrupting cook assets, and (3) pre-existing **cook-governance** decisions only the owner can make. The functional defects found are a tight cluster around the brand-new "functional sit" feature being under-integrated with the movement state machine, plus chess/TTT lacking the seat-pruning that blackjack already has — all **Medium, self-healing or cosmetic, none round-breaking.**

| | Count |
|---|---|
| **Fixed (this pass)** | 1 (Kenney CC0 attribution — distribution blocker; cold file + `.bak`) |
| **High** | 1 (the fixed one) |
| **Medium** | 9 (3 = the "sit" cluster · minigame seats · materials · plugins · **disk/DDC** · audio assets · licensing-governance) |
| **Low** | ~23 (cosmetic / perf / defense-in-depth / docs) |
| **New Critical / crash-on-normal-play / anti-cheat / classroom-leak** | **0** |

> **Why so few edits applied:** per the owner's direction (static-only, edit-in-place-with-`.bak`, report the rest) and the live concurrent-edit reality, **almost every code file carrying a finding is in the actively-edited/hot set** (`BHCharacter.cpp`, `BHGameMode.cpp`, `BHPlayerController.cpp`, the three minigame tables, `SBHMainMenu.cpp`, `BHAccountSubsystem.cpp` — all rewritten 00:24–00:41 or in the dirty tree). Editing those blind would clobber an agent mid-build. So the one fix applied is in a **cold** file; everything else is reported **with a concrete, ready-to-apply patch** for the system's owner to drop in during a coordinated window.

---

## Fixed this pass

### H1. Kenney roof props are cooked but missing from the shipped `THIRD_PARTY_NOTICES.txt` — *High (distribution)*
`Docs/THIRD_PARTY_NOTICES.txt` (no Kenney block) vs `Config/DefaultGame.ini:30` (`Content/BlackoutHunt/Art/Roof` is AlwaysCook'd) and `Docs/ASSETS.md:70,74-87`.
The June train-roof easter egg ships Kenney **Space Kit** meshes (telescope, satellite dish/antenna, rocks, meteor, deck stool). They cook into the package, but the notices file copied into the build (`Tools/Package-Windows.ps1:223`) had **no Kenney attribution**, and the verifier's required-snippet list (`Tools/Verify-ClassroomPackage.ps1:149-159`) doesn't include "Kenney", so it passed silently. CC0 is permissive but **still requires attribution** — shipping without it is a license-compliance gap.
**Fix applied:** added a Kenney CC0 block to `Docs/THIRD_PARTY_NOTICES.txt` (Space Kit + Nature Kit, CC0, kenney.nl, source pages). **Backup:** `Docs/THIRD_PARTY_NOTICES.txt.bak-bughunt-20260608`. Reversible by restoring the `.bak` or `git checkout -- Docs/THIRD_PARTY_NOTICES.txt`.
*Recommended follow-up (not done — avoid touching the verifier mid-flight): add `"Kenney"` to `$requiredNoticeSnippets` in `Verify-ClassroomPackage.ps1` so this can't silently regress.*

---

## Medium — reported with patches

### The "functional sit" cluster (new feature, `f0d0685`) — under-integrated with the movement state machine
All three are in `BHCharacter.cpp` (**hot** — report-only). They share a root cause: `bSeated` was added without reconciling it against capture/escape/round-reset, the hidden/freeze path, or prone/special-move. None corrupt win/loss, authority, or replication; all are visible state desyncs that **self-heal on the player's first movement input**.

- **M1. `bSeated` is never cleared by capture / escape / round-reset → respawn visually seated** — `BHCharacter.cpp:2695` (`ResetRoleWarmupStateForRoundStart`), `:2594` (`MarkCaptured`), `:2617` (`MarkEscaped`).
  `ResetRoleWarmupStateForRoundStart` resets every other movement flag but omits `bSeated`; capture/escape don't clear it either. A player seated when captured or when a new round/leg starts comes back **alive and able to walk but replicated as seated** (-22 cm eye height + legless dropped 3rd-person avatar) until they tap a movement key.
  **Patch:** add `bSeated = false;` to `ResetRoleWarmupStateForRoundStart` (with the other resets ~2729) and set `bSeated = false;` in `MarkCaptured`/`MarkEscaped` alongside `DisableMovement()`.
  Confidence: High · Intentional: no.

- **M2. `ApplyHiddenState()` force-sets `MOVE_Walking` for any alive pawn, silently un-freezing a seated player** — `BHCharacter.cpp:6202`.
  Called from `OnRep_HiddenInLocker`/`OnRep_OutOfPlay`/life-state/capture/escape, it unconditionally restores `MOVE_Walking` for an alive in-play pawn — re-enabling movement under a seated visual without clearing `bSeated` (same desync as M1, reachable from any locker/life-state OnRep near a seated player, host/owning-client path).
  **Patch:** guard the restore — `if (bAlive && !bSeated) GetCharacterMovement()->SetMovementMode(MOVE_Walking);` (let the stand path own movement while seated). Fixing M1's lifecycle + this guard together closes it.
  Confidence: Med · Intentional: no.

- **M3. Sit coexists with prone / special-move → pose desync** — `BHCharacter.cpp:7242` (`SetSeatedAuthority`) vs `:4060` (`SetProneAuthority`).
  `SetSeatedAuthority` gates only on `CanAct()` (not `IsProne()`/`IsSpecialMoveActive()`); `SetProneAuthority` rejects during a special move but not when seated. So a player can be **both prone and seated**; standing via a move key restores `MOVE_Walking` but leaves `MovementSpecialState==Prone` (lowered pose stuck) until they separately press Space. Player keeps full control — cosmetic/pose desync, no soft-lock.
  **Patch:** in `SetSeatedAuthority`, `if (bNewSeated && (IsProne() || IsSpecialMoveActive())) return;` (mirrors the prone guard).
  Confidence: High (coexistence) / Med (impact) · Intentional: no.

### M4. Chess & Tic-Tac-Toe never free an abandoned seat — the table becomes unusable for the rest of the lobby/intermission
`BHTrainChessTable.cpp` / `BHTrainTicTacToeTable.cpp` (**hot** — report-only). Confirmed by grep: `SetActiveMinigameTable(nullptr)` is **only** ever called by **blackjack's** `PruneSeats`. Chess/TTT have **no** seat-pruning, no "leave seat", and at `GameOver` they keep the same `WhitePlayerId/BlackPlayerId` (`XPlayerId/OPlayerId`), so:
- A player who sits (incl. vs-AI) and walks away or disconnects keeps the seat **forever** (for the life of the table actor — i.e. the whole lobby, which has no auto-advance).
- A new player can never claim that seat ("Both seats are taken") and can't rematch a finished game (only a *seated* player can). One abandoned game kills the table until ServerTravel rebuilds it.
Blackjack is immune (it prunes by owner-validity + distance in `Tick`, and has a 45 s turn timeout).
**Patch:** mirror blackjack's prune. Add a server `Tick` occupancy check: for each seated id, if the owning `ABHPlayerState`'s pawn is null or beyond a range (or `GetActiveMinigameTable() != this`), free that seat (id → -1, name cleared, `SetActiveMinigameTable(nullptr)`), and if a seat empties mid-`Playing`, reset `Phase` to `Idle`/end the game. Also allow an unseated player to claim a seat at a `GameOver` table.
Confidence: High · Intentional: no (inconsistent with blackjack).

### M5. New train-furniture materials referenced but not present → tables render untextured
`Source/BlackoutHunt/BHPropVisuals.cpp:84/90/96` load `M_BH_Wood`/`M_BH_Carpet`/`M_BH_Felt`; none exist under `Content/BlackoutHunt/Art/Materials/` (only the source texture `T_BH_Wood066_color.uasset` is imported). Safe `BasicMaterial()` fallback (no crash), but the chess/blackjack/TTT tables render with untextured primitive bodies. **In-progress import gap.**
**Action:** finish the import (create the three materials from the CC0 ambientCG sources already in `ThirdParty/`) or accept the cosmetic fallback for ship.
Confidence: High · Intentional: no (mid-import).

### M6. Six enabled-but-unused heavy plugins still enabled — cook cost / OOM-cook suspect
`BlackoutHunt.uproject:90-108`: `SmartObjects`, `GameplayAbilities`, `CommonUI`, `PCG`, `GeometryScripting`, `DatasmithImporter` — no `Build.cs` usage; add editor/cook cost + package size and were suspected in the prior OOM cooks. Flagged in Pass-3, still open.
**Action (owner, test required):** verify no Blueprint/asset references each, then disable with a **test build** (a referenced-but-disabled plugin breaks the cook — do not disable blind).
Confidence: Med · Intentional: maybe (left enabled defensively).

### M7. C: drive at **99%** (14 GB free), D: at 95% → DDC "Insufficient Storage (507)" is corrupting cook assets — **release-cook risk** *(environment, not code)*
Forensics on `Saved/Crashes/`: the 22:00 / 22:34 sessions logged `LogDerivedDataCache: ... Insufficient Storage (507)` from ZenLocal **and** handled audio-asset corruption ensures (M8) in the same window. This is the same disk pressure behind the documented prior **OOM cooks**. At 99% on C: (the DDC/temp drive) a release cook can fail or silently produce corrupt assets.
**Action (do before any release cook):** free C: space (clear the UE DDC / `Saved/`/temp; the project's own `Saved/` holds GBs of logs/crashes/webcache), then cook from a clean DDC. This gates the cook itself.
Confidence: High · Intentional: no.

### M8. Three audio assets hit `FStreamedAudioChunkSeekTable::Parse` ensures — likely DDC-corruption-induced
`SW_EerieLobbyLoop`, `SW_MenuClick`, `Flashlight_On` logged *handled* (non-fatal) seektable-parse ensures during the disk-full window (`Saved/Crashes/...D21D4304.../`, `...8F5F9D70.../`). Not present in the latest runs (`run-shell4.log`, `run-movetut3.log` are clean), consistent with transient DDC corruption. No crash, but a corrupt cooked seektable can mean a **silent / glitched** menu click, lobby ambience, or flashlight sound in the package.
**Action:** after M7, re-import/re-save those three SoundWaves and confirm they play, then verify in a clean cook.
Confidence: Med · Intentional: no.

### M9. Pass-3 cook-governance blockers remain OPEN — owner licensing decision + a test cook
Confirmed still open against live files (the LOD-inversion item from Pass-3 is **RESOLVED** — `DefaultScalability.ini` now rises 0.65→1.00):
- **SmartBasicInterfaces** ships its `Demo/`+`Maps/` whole-pack (`Config/DefaultGame.ini:38`), license-evidence still unrecorded. The breaker/station meshes pull deps from `Demo/Sounds`+`Demo/SCContent/Materials`, so it **cannot** simply be NeverCook'd (`:53-57` documents why — breaks the cook). Resolution = record the license/entitlement + add a notices block, **or** migrate only the needed sub-assets out of `Demo/`.
- **`/Game/ContainersHouseCH`** (soft-loaded by `CladAuthoredWall`, `BHGameMode.cpp:5700/5705`) and **`/Game/BFHorror/DemoAssets`** (`BHBlockActor.cpp:90`) — uncooked, not in NeverCook, not in the verifier, no `ASSETS.md` row. Cosmetic-absent today (safe fallbacks). Decide: license + AlwaysCook, **or** NeverCook + a verifier rule. Needs a test cook to confirm no authored `.umap` hard-references them.
- **Verifier is a denylist, not an allowlist** (`Verify-ClassroomPackage.ps1:47-83`): un-enumerated source-pack roots (`ContainersHouseCH`, `BFHorror`, `FirstPersonHorrorKit`, `Horror_Template`, `RuinedCrypt`, `ResidentHorrorV1`, …) could stage and PASS. Mitigated today because the classroom cook uses a **fixed map list** (`Package-Windows.ps1:185`), not `-cookall`, so they only stage if a cooked map hard-refs them. Recommend an **allowlist** model: fail any `Content/<Root>/` not on the approved list.
Confidence: High · Intentional: pending owner decision.

---

## Low — reported (file:line · one-line fix)

**Minigames**
- **L2. Blackjack dealer hole card replicated in cleartext** — `BHTrainBlackjackTable.h:148` (`DealerCards` replicates the hole too; only HUD-masked via `bDealerHoleHidden`). A modified client can peek. Trivial stakes (chips never convert to real points — economy is sandboxed). Fix (optional): keep the hole server-only until reveal.
- **L4. Minigame tables run cosmetic refresh on the server every tick** — chess/TTT `RefreshBoardVisuals`, blackjack `RefreshTableText`. Listen-server host has a viewer so it's minor; wasted on a dedicated server. Fix: early-out `RefreshBoardVisuals`/`RefreshTableText` on `NM_DedicatedServer`.
- **L19. Chess header comment stale** — `BHTrainChessTable.h:69-71` says "25-int board / `From*25+To`"; the code is 6×6=36 (`BHChessCells`) and consistent. Doc-only.

**Sit / movement (all `BHCharacter.cpp`, hot)**
- **L5. Sit is not phase/role-gated** — `:7242`. A survivor can sit mid-Hunt; a Teacher can sit. *Not* an anti-camp exploit (verified: `MOVE_None` fails the move-burst gate, idle pressure keeps building the loud "restless breathing" tell). Fix if "lobby relax pose" is the intent: gate `bNewSeated` on `RoundPhase != Hunt/FinalEscape`.
- **L6. `ServerSetSeated` unthrottled reliable RPC** — `:7237`. Cheap (early-out on match); consistent with the other unthrottled movement RPCs. Add the lightweight throttle for parity.
- **L12. Seated freeze defeated for 1 frame during a cutscene/final-escape lock** — `:7265`. Stand-up has no `CanAct()` gate; the authority Tick re-asserts the lock next frame (`:1791-1804`), so not exploitable. Fix (defense): only restore `MOVE_Walking` on un-seat if `CanAct()`.

**Achievements / cosmetics / persistence**
- **L7. Reconnect-into-resolved-round re-grants a round's cosmetic XP once** — `BHGameMode.cpp:1211` + `BHAccountSubsystem.cpp:1140`. Bounded to one extra grant in a narrow drop-at-EndRound window; XP only gates cosmetics. Fix: make `RecordRoundResult` idempotent per round (or skip the re-send when reconnecting into a `*Win` phase).
- **L8. `RecordRoundResult` win/escape counters non-saturating** — `BHAccountSubsystem.cpp:1157/1162/1168` (plain `++`, unlike the saturated `RoundsPlayed`/`XP` beside them). Overflow unreachable (~2.1B rounds). Mirror the `if (X < MAX_int32) ++X;` guard.
- **L9. JSON numeric `double→int32` cast without range guard** — `BHAccountSubsystem.cpp:1950/1958/1979` (+ `JsonInt` `:202`). A hand-edited/corrupt `progress.json` is implementation-defined on cast; only sink is the Awards-tab mastery tallies (never gameplay). Clamp parsed counts to `[0, MAX_int32]`.
- **L10. `ServerSetAvatarColor` status toast mislabels prestige/black colours** — `BHPlayerController.cpp:8024` prints `Clamp(idx,0,7)+1`, so any tint ≥8 reports "8". Text-only; replicated value is correct. Print `ColorIndex+1` / the item name.
- **L11. Title/Emblem flair not server-verified against unlocks (cosmetic spoof)** — `BHPlayerController.cpp:8070/8088`. RPCs are flood-guarded + index-clamped (no OOB) but don't confirm the player earned the achievement-gated title/emblem. Purely cosmetic; consistent with the "cosmetics are client-local" posture, but the header says "achievement-gated". Validate server-side or soften the comment.

**UI / menu / HUD**
- **L13. Avatar preview re-captures the scene every frame while the menu is open** — `SBHMainMenu.cpp:2690→8830`. `Tick` calls `UpdateAvatarPreviewMesh()` unconditionally on every tab; change-guards skip the mesh/material reloads but the trailing `CaptureScene()` (512×768 RT) fires every frame regardless (defeats `bCaptureEveryFrame=false`). Fix: only update from `Tick` on the Character tab, and/or early-out the capture when no change flag fired.
- **L14. `GetStatusText` sticky account/network message masks per-click feedback** *(known, pass-1)* — `SBHMainMenu.cpp:7406-7432`. Theme/recolour/role-assign/kick toasts hidden behind the sticky line. Clear the sticky once consumed or timestamp a fresh `StatusText` to outrank it.
- **L15. Host action-list capabilities baked at `Construct`** *(known, pass-1)* — `SBHMainMenu.cpp:5315`. "START HUNT NOW" can linger/absent if the phase changes with the menu open. **Host-side only — no student exposure.** Drive via live `Can*` `TAttribute` getters like the rest of the menu.

**Easter-eggs / roof / train**
- **L1. Roof-light breaker replicates to all students when the HOST flips it (listen server)** — `BHCharacter.cpp:3516-3528` via `BHTrainRoofBreaker.cpp:52`. The host's Client-RPC runs on the authority and spawns `bReplicates=true` `ABHTrainServiceLight`s, breaking the "purely per-player local" contract (host only). Cosmetic. Fix: `RoofLight->SetReplicates(false);` right after each spawn in `SetRoofServiceLightsLocal`.
- **L3. Tunnel-motion actor ticks cosmetic work on a dedicated server** — `BHTrainTunnelMotionActor.cpp:89-157` (`BeginPlay` early-returns on `NM_DedicatedServer` but never disables tick; sibling new actors got the guard). Fix: `if (GetNetMode()==NM_DedicatedServer){ SetActorTickEnabled(false); return; }` in `BeginPlay`. (Near-zero impact in the listen-server classroom config.)
- **L16. Both roof hatches grant `roof_rider` + a "climbed onto the roof" toast — including the DOWN hatch** — `BHTrainRoofHatch.cpp:57`. Misleading on the descend hatch (grant is idempotent, so not an exploit). Gate the grant/toast on a roof-bound target or a `bAwardsRoofRider` flag.
- **L17. Completionist roll-up omits `roof_parkour`** — `BHAccountSubsystem.cpp:1263-1266`. `EASTER_EGGS.md` says "every hidden award" but the array lists only spelunker/honorary_faculty/codebreaker/roof_rider. Add `roof_parkour` or fix the doc.
- **L18. `ABHTrainServiceLight` ticks forever after boot (client perf)** — `BHTrainServiceLight.cpp:108-147` (0.05 s, ~2% "breathe" sine across ~15-45 fixtures). Server tick correctly disabled. Optional: disable tick or raise interval once `bBootComplete`.

**Config / docs**
- **L20. `DefaultScalability.ini` header comment contradicts its now-correct values** — `:1-4` narrates the old inverted `1.45→1.00` scheme; values at `:7,12,17,22` are the corrected `0.65→1.00`. Update the comment.
- **L21. Version drift in docs** — load-bearing strings are correct at **0.8.1** (`Config/DefaultGame.ini:5`, online build-id `BHGameInstance.cpp:42`, newest notes `BETA_RELEASE_NOTES_0.8.1.md`), so **online matchmaking is fine**. Stale docs: `README.md:11` (0.7.2), `AGENTS.md:25,32` (0.7.0), `Docs/BLACKOUT_HUNT_COMPLETE_OVERVIEW.md:7,611,636,637` (0.7.0). Bump the three docs to 0.8.1 (AGENTS.md self-mandates keeping the overview current).
- **L22. `ASSETS.md` imported-asset inventory stale** — `:22-28` lists only the original 7 ambientCG sets; June added Wood066/Carpet012/Fabric030/Leather030/Marble016 sources + a cooked `T_BH_Wood066_color`. Low risk (blanket CC0). Add them.
- **L23. `AndroidFileServer SecurityToken` committed** — `DefaultEngine.ini:142`, but `bIncludeInShipping=False` (`:143`) so inert in the Windows game. Rotate if the editor file-server is ever used. *(Pre-existing.)*

---

## Release-readiness assessment — classroom Windows 0.8.x / 0.9

**Gameplay/runtime: GO.** No Critical, no crash-on-normal-play, no anti-cheat hole, no win/loss/replication regression, no host/student safety leak. The crash dumps contain **no shipping-runtime crash** — only an editor save/shutdown Slate assert (dev-only) and DDC-induced audio ensures (M8). The new minigames are crash-safe; the new movement tech (flow-chain/somersault/bunny-hop — intentional) can't produce infinite speed, clip geometry, or break a round.

**Must clear before a public cook (P0):**
1. **M7 — free C: disk + clean DDC** (currently 99%; gates the cook itself / caused asset corruption).
2. **M8 — re-verify the 3 audio assets** play after a clean cook.
3. **M9 — owner licensing decision** on SmartBasicInterfaces / ContainersHouseCH / BFHorror + a **test cook** (or accept cosmetic-absent for the latter two and record the SmartBasicInterfaces license).
4. **H1 — Kenney attribution** — ✅ done.

**Should clear for quality (P1, low-risk, owner of each system applies):** the sit cluster (M1–M3), chess/TTT seat-pruning (M4), finish/accept the train materials (M5), roof-light host replication (L1).

**Verdict:** **fit for an internal/classroom playtest now**; for a **public** distribution, clear the four P0 items (three are environment/governance, one is done).

---

## Intentional "fun" — confirmed and PRESERVED (do not "fix")

Verified cosmetic-only / round-safe / advantage-free; left untouched:
- **Survivor "flow chain" / momentum speedrun tech** + **bunny-hop chaining** — bounded speed (set-not-multiply, hard link cap), engine-swept (no clip-through), survivor-only, never aids a Hunter.
- **Dodge-roll forward somersault** — clamped 0..360°, applied to the final POV only (can't corrupt aim or clamp to ±90°).
- **All easter eggs** (`bh.EasterEggs`): physicist greetings, locker graffiti, presence whispers, Konami, and the **entire train-roof hang-out** (hatches, per-player breaker, starfield, decorations, **parkour course + Roof Runner**). Parkour gate is not cheeseable and grants its XP exactly once; N/O reset always recovers a stuck player; no one is stranded at departure.
- The **train minigames** themselves (chess/TTT/blackjack) — the fun is intended; only the seat-pruning (M4) and the polish items above are defects.

---

## Notable verified-clean (release confidence)

- **Classroom safety (the #1 concern): clean.** Every host/admin affordance in the new tabs (role assign, kick, force-start, tunnel, hotspot, classroom board, lesson presets, network admin) is gated on the listen-server host via live `Can*`/visibility getters; a remote student (`NM_Client`) gets none. Roster data shown to all is already-visible lobby data, not PII. Targeted scare is correctly limited to an alive Hunter/authority. `BHUnlockAllCosmetics` is `#if UE_BUILD_SHIPPING`-stubbed.
- **Anti-cheat / authority: clean.** All minigame, sit, special-move, and cosmetic actions go input → Server RPC → `*Authority()` with role/phase/ownership/clamp re-validation. Achievements are server-detected → owner-only Client RPC → local account write; the server never trusts a client claim. New Server RPCs (`ServerSetAvatarSlotColors`/`ServerSetTitle`/`ServerSetEmblem`/`ServerSetSeated`/`ServerStartSpecialMove`) funnel through the flood guard / authority + `CanAct()`.
- **Win/loss + replication: no regression.** Evacuate-together `CountEscapedSurvivors()` tie-break intact at every terminal site; reconnect-grace defer + no-hunters→SurvivorsWin present; achievement hooks are additive and run before/around `EndRound` without altering the decision. All new replicated state has correct COND (private meters + academic data stay `COND_OwnerOnly`; cosmetics replicate-to-all by design); `Super::` called in every `GetLifetimeReplicatedProps`. The prior `BHObjectiveStation` team-vote `[4]` soft-lock is **confirmed fixed** (now a TMap, individual-graded).
- **Minigames crash-safe.** Indices validated (`IsValidIndex`/`Clamp`); chess is 6×6=36 throughout; AI move-lists are provably non-empty before `RandRange`; negamax/minimax terminate; blackjack reshuffles a 4-deck shoe (no exhaustion) and chips never convert to real points (no economy exploit).
- **Persistence/crypto.** AES-256-CBC + encrypt-then-HMAC (constant-time compare), salted+iterated hashing, atomic `.tmp`/`.bak` rotation, forward-compat save lock; all four avatar palette tables byte-identical + append-only with a consistent +1/-1 mapping; JSON round-trip handles missing keys + old saves.
- **Secrets.** Real EOS values (`Config/EOS/EOSValues.local.ini`) are gitignored **and** staging-excluded (`[Staging] DisallowedConfigFiles`) **and** unused in the classroom Null profile; only `.example.ini` are tracked.

---

## Roadmap (for return + the active agents)

**P0 — before a public classroom cook (mostly environment/owner, not code):**
- [ ] **M7** free C: disk (99%→) + clear UE DDC, then cook from clean DDC. *(Blocks the cook.)*
- [ ] **M9** owner call on SmartBasicInterfaces license + ContainersHouseCH/BFHorror cook policy → **test cook** to confirm.
- [ ] **M8** re-import/verify `SW_EerieLobbyLoop`, `SW_MenuClick`, `Flashlight_On` after M7.
- [x] **H1** Kenney attribution — done (`.bak` left).
- [ ] *(follow-up)* add `"Kenney"` to the verifier's required-notice snippets; consider the allowlist verifier model (M9).

**P1 — quality, low-risk, the owning agent applies during a coordinated window (patches above):**
- [ ] **M1+M2+M3** the sit cluster — fix `bSeated` lifecycle + the two guards together (`BHCharacter.cpp`).
- [ ] **M4** chess/TTT seat-pruning (mirror blackjack) — `BHTrainChessTable.cpp` / `BHTrainTicTacToeTable.cpp`.
- [ ] **M5** finish or accept the `M_BH_Wood/Carpet/Felt` materials.
- [ ] **M6** test-disable the 6 unused plugins (verify refs first; affects cook size/OOM).
- [ ] **L1** roof-light breaker `SetReplicates(false)` on host.

**P2 — polish / defense / cosmetic / docs:** the Low list (L2–L23). Safe cold-file doc fixes (L20–L22) can be batched; the rest ride along when their owning file is next touched.

---

## Caveats

- **Nothing here was compiled or run.** Static analysis only (concurrent builds made a second build unsafe). Patches are reasoned correct; verify with `Tools/Build-Editor.ps1` + targeted automation in a coordinated window. Header-touching fixes (none applied here) would need a UHT regen.
- **A concurrent automated writer is actively editing this repo** (source rewritten through 00:41 on 2026-06-08). The hot files (`BHCharacter.cpp`, `BHGameMode.cpp`, `BHPlayerController.cpp`, the 3 minigame tables, `SBHMainMenu.cpp`, `BHAccountSubsystem.cpp`) were reported-not-edited to avoid clobbering in-flight work. Re-confirm a finding against live code before patching (the active agent may have already addressed some).
- **Line numbers** are from the live tree at audit time and may drift as the concurrent agent edits.
- The one applied edit (`THIRD_PARTY_NOTICES.txt`) is reversible via `Docs/THIRD_PARTY_NOTICES.txt.bak-bughunt-20260608` or git.

---

*Generated by an automated bug hunt (1 orchestrator + 6 parallel read-only auditors + crash/disk forensics) on 2026-06-08, scoped to the post-2026-06-05 surface + release readiness. Companion to `BUG_HUNT_FINDINGS_2026-06-04.md` (the pre-June core audit).*
