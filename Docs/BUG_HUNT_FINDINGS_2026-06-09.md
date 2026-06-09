# Blackout Hunt — Live-Play Hardening Bug Hunt & Fix Pass (2026-06-09)

*An aggressive, depth-first hunt for anything that could **compromise or affect live play**, plus general bug-hunting, ahead of a fresh Windows **Classroom** prerelease build. Unlike the 06-08 pass (static, report-only), this pass **applies** the confirmed live-play fixes and **cooks/ships** the result.*

**Build branch:** `fix/live-play-hardening` · **Cooked from:** isolated git worktree (`D:\BH_build`) — a frozen snapshot of the live tree, so the fixes + cook are decoupled from the other agents still editing `D:\BlackoutHunt`.
**Engine:** UE 5.7 · **Target:** Windows Classroom (Shipping), listen-server host + student `NM_Client`.

## Method

Six parallel read-only auditors, each with a focused live-play mandate, run against the live tree; findings cross-checked against the actual code and against the prior audits (`BUG_HUNT_FINDINGS_2026-06-08.md`, `-06-04.md`) so known/fixed items are not re-reported:

1. **Anti-cheat / replication / authority** — server-RPC validation, `HasAuthority` gates, `COND` leaks, the new `BHAntiCheatSubsystem`.
2. **Movement exploits** — the new `BHCharacterMovementComponent` (air-strafe), flow-chain/bunny-hop/somersault/sit, speed/clip/OOB/stamina/desync.
3. **Crash / stability** — null derefs, OOB, casts, lifetime/dangling, late-join/disconnect/reconnect races, the new actors.
4. **Tutorial/dev/test leak into live play + host/student safety + the new boot/BSOD menu.**
5. **Match flow / round state** — win-loss, phase transitions, capture/escape/evacuate, role assignment, reconnect-grace, seat/minigame lifecycle.
6. **The ~13 brand-new untracked `BHTrain*` social actors + `BHCollectable`** — authority, replication, crash, seat-occupancy.

## Headline verdict

**No Critical. No High. No crash-on-normal-play, no anti-cheat hole, no host/student safety leak, no win-loss/replication regression, no round-breaking soft-lock.** The codebase remains exceptionally server-authoritative and defensive; the new movement tech and new social actors follow the established safe patterns. The previously-open round-affecting item (06-08 **M4**, chess/TTT abandoned seats) is **confirmed fixed** by the concurrent work, and Othello/Connect-Four ship the same correct prune.

The only live-play-relevant defects found were a small, well-understood cluster — all **fixed in this pass** (below).

## Fixed this pass (applied + cooked)

All in `Source/BlackoutHunt/`, tagged `[live-play-hardening]` in-code.

### Sit-cluster M1/M2/M3 — seated state desync (carried over from 06-08, confirmed still open by 3 independent auditors)
`bSeated`/`bSeatLocked` were added to the sit/chair feature without reconciling against the capture/escape/round-reset lifecycle or the hidden-state path. None corrupt authority, win-loss, or replication; all are visual desyncs (lowered eye height + legless 3rd-person avatar) that self-heal on first movement input — but they are avoidable.

- **M1 — `BHCharacter.cpp`:** clear `bSeated`/`bSeatLocked` in `ResetRoleWarmupStateForRoundStart`, `MarkCaptured`, and `MarkEscaped`. A player seated when captured/escaped or when the round/leg resets no longer comes back replicated-as-seated.
- **M2 — `BHCharacter::ApplyHiddenState`:** guard the alive-restore as `if (bAlive && !bSeated)` so a locker/life-state `OnRep` near a seated pawn no longer silently un-freezes it to `MOVE_Walking` under the seated visual. The stand path owns movement while seated.
- **M3 — `BHCharacter::SetSeatedAuthority`:** reject `bNewSeated` when `IsProne() || IsSpecialMoveActive()` (mirrors the prone guard), so a player can no longer be both prone/mid-roll and seated and end up with a stuck pose.

### Anti-cheat enforce footgun — teleport detection could kick legitimate students (MEDIUM)
`BHAntiCheatSubsystem.cpp` — the position-jump "teleport" heuristic accrued toward the kick budget, but the intended exemption hook `NotifyTrustedTeleport()` has **zero call sites**. The game legitimately teleports players via `ETeleportType::TeleportPhysics` (locker exit, stuck-zone reset, train-interior reset, final-escape relocation, roof reset, chair-sit). In **enforce mode (`bh.AntiCheat 2`)** that would auto-kick legitimate students for normal play — the exact classroom-safety failure the subsystem exists to prevent.
**Fix:** set `BHACWeightTeleport = 0.0f` (teleport becomes **log-only**, never contributes to the kick budget) until every engine-teleport site calls `NotifyTrustedTeleport`. **Speed** and **Fly** enforcement (the real movement cheats) are unchanged. Anti-cheat is off by default (`bh.AntiCheat=0`), so the shipped classroom default is unaffected; this removes the hazard for any host who enables enforce mode.

## Carried-over / not actioned (low value or owner-decision; not live-play-blocking)

- **LOW — dedicated-server cosmetic tick waste** across the new social actors (Aquarium/ToyTrain ~33 Hz, etc.). Near-zero in the shipped **listen-server** classroom config; cosmetic-only. Optional `NM_DedicatedServer` tick guards.
- **LOW — Dartboard** never clears `ActiveMinigameTable` (sticky HUD within 7.5 m); self-heals on walk-away, non-exclusive, no stranding.
- **LOW — doc drift** (`MOVEMENT.md` chain-window default, header comments) — non-functional.
- **06-08 governance items (M6 unused plugins, M9 licensing/cook policy)** — owner decisions, out of scope for a code hardening pass.

## Verified-clean highlights (live-play confidence)

- **Anti-cheat/authority:** every host/admin/tester RPC re-validates host authority server-side (`RequireHostAdmin`/`IsHostAdminController`, local-controller check on the server); every gameplay RPC re-checks role/phase/ownership/distance and clamps client input; hidden answer data is `COND_Never`, private meters `COND_OwnerOnly`. A student `NM_Client` cannot forge state, peek answers, or trigger host actions.
- **Movement:** the new air-strafe is Source-correct AirAccelerate with a hard speed ceiling, set-not-multiply, runs in the predicted+authoritative `CalcVelocity` (reconciles); flow-chain/bunny-hop/roll are role-gated, link-capped, stamina-charged, swept (no clip/OOB). No `LaunchCharacter` anywhere.
- **Round/match machine:** uniform evacuate-together win-loss tie-break at every terminal site; re-entry-guarded `EndRound`; reconnect-grace defer consistent; FinalEscape has armed fallbacks; role assignment guarantees ≥1 survivor (and ≥1 hunter with ≥2 players); objective votes are individual-graded and prune leavers.
- **Tutorial leak:** `bTutorialCaptureImmune` is server-only, set only on the tutorial student, and cleared three ways incl. the round-start reset (`5d1ed26`) and every `ServerTravel`. Tester/dev shortcuts are host-gated server-side; dev unlock is `#if !UE_BUILD_SHIPPING` + local credential.
- **Boot/BSOD intro:** honors reduced-flash/reduced-shake comfort prefs (loaded before the console shows); never shown to a joining student or the live-classroom host; no black-screen/focus deadlock.
- **New social actors + `BHCollectable`:** authority-gated, correct `COND`/`Super::`, bounds-guarded, no PII leak, no use-after-destroy; Othello prunes seats correctly (no M4 repeat).

## Cook / environment

- **LFS:** the local `.git/lfs/tmp` directory was NTFS-corrupted (blocked `git status`/commit); repaired by renaming the corrupt entry aside and creating a fresh `tmp`. Working-tree assets verified intact.
- **Disk (06-08 M7):** freed ~35 GB (old `Builds/`, stale `Saved/` cook/test artifacts) before cooking from the worktree, to avoid the DDC "Insufficient Storage" asset corruption seen in the 06-08 window.

---
*Companion to `BUG_HUNT_FINDINGS_2026-06-08.md` (static report-only) and `BUG_HUNT_FINDINGS_2026-06-04.md` (pre-June core). This pass applied + shipped the fixes.*
