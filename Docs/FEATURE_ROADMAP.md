# Blackout Hunt — Feature Roadmap (achievements & beyond)

Separate from the beta-polish `Docs/ROADMAP.md`, which stays the source of truth for classroom-reliability work.
This tracks the post-achievements feature work, **re-sequenced classroom-first** per the owner's call: the
teaching-value items lead (they also align with the beta roadmap's classroom focus), with the cosmetic polish
alongside.

Discipline: **one concern per commit, build-validated** (`Tools/Build-Editor.ps1`); cosmetic / additive /
reversible bias; anything touching scoring or server-authoritative play is called out.

Status: `[ ]` todo · `[~]` in progress · `[x]` done

> **Progress (autonomous run, 2026-06-07):** Phase 1 **done** (educational achievements + per-topic mastery; the
> teacher report was already comprehensive, so no work was needed there). Phase 2 progress bars **done**
> (newly-unlocked indicator deferred — doing it right needs the Awards panel to rebuild on tab-open). Phase 3
> **done** (rank-from-XP + stats profile card). Phase 4: momentum flow-chain **cvars done**; head-bone hats +
> nameplate-flair placement polish deferred — they need in-engine visual verification, so they're owner-playtest
> items rather than blind guesses.

---

## Phase 1 — Classroom / teaching value (first)
- [ ] **Question-grading hook** — one place that fires when an answer is graded (who / which topic / correct?),
  covering the standard, revision, and team paths in `BHObjectiveStation`. Foundation for everything below.
- [ ] **Per-topic mastery, surfaced to the student** — track correct/total per physics topic in the account;
  show "Forces 82% · Waves 60% · …" in the menu (and end-of-round where it fits).
- [ ] **Educational achievements** — *Honor Roll* (5 correct in a row) + *Polymath* (a correct answer in all
  four topics in one session), built on the hook. Rewards: a tint + a title.
- [ ] **Richer end-of-class teacher report** — topic breakdown / most-missed, where the report is assembled.

## Phase 2 — Finish the achievements UI
- [ ] **Progress bars in the Awards tab** — Veteran 18/25, Graduate 31/50, Tourist 2/4, On a Roll 1/3.
- [ ] **"Newly unlocked" indicator** + an unread count on the Awards tab button.

## Phase 3 — Stats / Profile
- [ ] **Rank / level from XP** — a school-themed ladder (Freshman → … → Valedictorian).
- [ ] **Stats panel** — lifetime rounds, wins, escapes, captures, best streak, favourite role; a section in the
  Awards tab.

## Phase 4 — Fun & polish
- [ ] **More easter eggs** — established cosmetic-only, `bh.EasterEggs`-gated style.
- [ ] **Momentum flow-chain tuning** + cvar knobs (the one balance-sensitive feature).
- [ ] **Pixel-accurate hats** — head-bone attachment for the procedural headwear.
- [ ] **Nameplate flair placement polish** — after the first playtest.

---

## Notes
- Phase 1's grading hook is shared by the educational achievements and per-topic mastery — built once.
- Visual-placement values that can't be verified in-engine (hats, flair, roof teleport) are flagged for the
  owner's playtest as one-number tweaks.
