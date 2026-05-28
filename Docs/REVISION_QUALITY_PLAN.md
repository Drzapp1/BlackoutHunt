# Revision Quality Improvements — Design & Plan

Goal: make students **actually revise and learn** in BlackoutHunt instead of guessing or
messing around. This document is the design of record for the change set on branch
`feature/train-intermission-final-station`.

The four failure modes the teacher reported, and the levers chosen (all "recommended" options):

| Failure mode | Lever |
| --- | --- |
| Spam-guessing answers (25% MC odds, 0.45s cooldown, same question stays loaded) | Anti-gaming: correction lockout + escalation + reload-on-wrong + decay |
| Ignoring explanations | Force engagement: wrong answer holds answering for a few seconds while the correction shows |
| Avoiding stations / messing around on the train | Incentives: bonus-terminal answers now build mastery and count toward escape |
| Mastery caps too fast (~5 lucky answers = 100%) | Demonstrated + durable mastery: slower diff-weighted gain, decay on miss, review-gated, asymptotic cap |

Design constraints from the teacher:
- **Firm but safe.** No mechanic may pin a player in place during a chase. The lockouts only
  delay *answer submission*; the player can always walk away from the terminal.
- **Keep the 70% class / 50% individual exit thresholds**, tune the gain curve so a focused
  session still reaches them; expose the knobs in `TUNING.md`.
- **JSON content with C++ fallback.** The C++ bank stays as the built-in default; add an export
  command + a round-trip test that proves the JSON path reproduces the C++ bank exactly.

---

## Pillar 1 — Demonstrated + durable mastery (`BHGameMode::RecordRevisionAnswer`)

Today: a correct answer adds a flat `+24 * weight` to the topic mastery (so ~5 correct = 100%),
and `MasteryPercent = 100 * (correct+corrections) / attempts` — a naive ratio that a lucky
guesser maxes quickly. The per-topic floats and `MasteryPercent` are two **different scales**
with different readers, which is also desync-prone.

New model (single writer, no new replicated fields):

1. **Diff-weighted gain with diminishing returns** on a correct answer for the answered topic:
   ```
   DiffMult  = clamp(Question.MasteryWeight, 1.0, 1.5)      // Easy 1.0 / Med 1.2 / Hard 1.5
   Headroom  = max(1 - (TopicMastery/100) * 0.6, 0.25)      // approaching the cap slows down
   Gain      = 15.0 * DiffMult * Headroom
   ```
   This makes the climb to ~70% reachable (~6–7 easy corrects, fewer with harder questions),
   but 90–100% is asymptotic — you cannot trivially "cap" a topic, and harder questions
   (higher `DiffMult`) are what push past the plateau, so breadth/difficulty is rewarded.

2. **Decay on a miss** for the answered topic (this is what kills spam-guessing):
   ```
   MissDiffMult = Easy 1.2 / Med 1.0 / Hard 0.8             // careless easy misses hurt most
   Decay        = 7.0 * MissDiffMult
   ```
   At 25% MC odds, 4 blind guesses ≈ +1 correct and −3 wrong ⇒ net-negative mastery. Knowing
   the material (mostly correct) still climbs steadily.

3. **Review-gated ceiling.** A topic is clamped to ≤ 80% while the student still has an
   unresolved missed question (review-queue entry) in that topic. You must clear your own
   mistakes to fully master a topic — ties the spaced-repetition loop directly to mastery.

4. **Unified `MasteryPercent`** = mean of the **enabled** topic masteries (`RevisionTopicMask`).
   Removes the two-scale desync; every existing reader (HUD, exit gate, report, board, travel
   snapshot) keeps working and gets a more meaningful number. `Attempts`, `CorrectAnswers`,
   `CorrectionsCompleted`, `ContributionCount`, `HintCount` are still tracked for the board/report.

5. **Incentive alignment.** A correct answer to a *previously-missed* question (`bCorrection`)
   pays half shop points (you get full points by knowing it first time) but still grants full
   mastery + clears the review (recovery is real learning).

6. **New parameter `bCountsAsContribution`** (default `true`). Team-station answers count as a
   contribution (the hall-monitor tool gate); bonus-terminal answers pass `false`.

## Pillar 2/3 — Station anti-gaming (`BHObjectiveStation`)

- New server-only members (not replicated): `CorrectionHoldUntil`, `ConsecutiveWrongAtStation`.
- **Correction lockout.** After a wrong answer, answering is blocked for
  `clamp(3 + 1.5*(consecutiveWrong-1), 3, 9)` seconds; the feedback shows the correction and a
  "retry in Ns" prompt. Player can still move/flee — only the answer input waits.
- **Reload-on-wrong (anti-echo).** In revision mode, a wrong answer no longer leaves the same
  question on screen to be brute-forced. The missed question is enqueued for spaced review, and
  a fresh adaptive question (eased difficulty, since last answer was wrong) loads. The student
  can't just read the revealed answer and re-enter it.
- `bCorrection` for a correct answer is now driven by `bRevisionReviewQuestion` (the loaded
  question really came from the review queue), which is more accurate than the old
  per-character `PendingCorrectionCharacters` heuristic.
- Existing wrong-answer pressure (fear/dread/detention/noise) is retained and the detention
  mark scales with consecutive wrongs.

## Pillar 3/4 — Bonus terminal (`BHTrainBonusQuestionTerminal`)

- Adds the same correction lockout (4s) on top of the existing 0.35s anti-spam cooldown.
- Calls `RecordRevisionAnswer(..., bCountsAsContribution=false)` so bonus answers **build topic
  mastery + overall mastery** (revising on the train genuinely helps the class escape) but do
  not satisfy the team-station contribution gate. The terminal's own duplicate review-queue and
  telemetry writes are removed (the unified path now owns them) to avoid double counting.

## Pillar 5 — Editable question content (JSON + C++ fallback)

- `BHRevisionQuestionBank::GetQuestions()` builds from JSON when an override file exists and
  parses & passes structural validation; otherwise falls back to the compiled C++ bank.
  Override path: `<Project>/Saved/ClassroomPresets/QuestionBank.json` (same dir as
  `LessonPresets.json`). A corrupt file is backed up (`.invalid-<ts>`) and the fallback is used,
  mirroring `BHLessonPreset`.
- JSON schema: explicit `id`, `topic`, `topicName`, `subtopic`, `difficulty`, `type`,
  `diagramType`, `prompt`, `choices[4]`, `correctChoiceIndex`, `hint`, `correctionPrompt`,
  `explanation`, `formula`, `numericAnswer`, `numericTolerance`, optional `masteryWeight`
  (derived from difficulty if absent). Enums round-trip as strings.
- `Validate()` is relaxed to **structural** validation (well-formed, unique ids, 4 choices, ≥1
  question per enabled topic) so teacher content of any size is accepted. The strict built-in
  distribution check (320 / 80-per-topic / 24-32-24 / fixed type counts) moves to a test-only
  `ValidateBuiltInDistribution()` that guards the shipped content.
- New host command **`ExportQuestionBankTemplate`** writes the full current bank (all 320
  questions) to the override path as an editable starting point.
- A new automation test serializes the built-in bank → JSON string → parses back and asserts
  every field + the count match, proving "migrate all" fidelity without hand-transcribing.

## Tests (`BHGameModeRevisionTests.cpp`)
- Mastery model: a 1-correct-of-4 guesser ends net-negative/flat; a consistent correct-answerer
  climbs; topic is review-gated to ≤80% while a miss is queued; `MasteryPercent` = mean of
  enabled topics.
- JSON round-trip fidelity (serialize built-in → parse → equal).
- Relaxed `Validate` accepts a small hand-built bank; `ValidateBuiltInDistribution` still 320.
- Existing review-queue + tuning tests updated for the relaxed `Validate`.

## Out of scope / non-goals
- No new replicated `FBHPlayerRevisionStats` fields (avoids touching travel serialization and
  the classroom-board dirty signature).
- No change to default exit thresholds (70/50) or to the team-answering composition.
- No hard mid-chase gates.
