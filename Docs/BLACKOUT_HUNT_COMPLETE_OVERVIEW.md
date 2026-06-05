# Blackout Hunt — Complete Project Overview

*A full, ground-up explanation of the game, its systems, and the repository. Written to bring a new reader fully up to speed on every part of the project.*

> **Maintenance contract:** This is the canonical repo-context document — read it first instead of re-scanning the whole repo, and keep it current. Whenever you change a system, a config default, a tool, a test, or the version, update the affected section in the **same** change. For authoritative live numbers, defer to `Config/DefaultGame.ini`, `Source/BlackoutHunt/BHTypes.h`, and `Source/BlackoutHunt/BHGameSettings.cpp` (this doc paraphrases them). *Last substantive update: **2026-06-01** — the 0.7.0 release landed: interactive/visual physics questions, the three-role guided tutorial, the jumpscare overhaul, authored maps on by default, 2K structural textures, and an EOS Windows build; version bumped to **`0.7.0`** across the repo (§33).*

Engine: **Unreal Engine 5.7** · Language: **C++** (native gameplay, Slate UI) · Project version: **0.7.0** · Module: `BlackoutHunt` · Platforms: Windows (primary), Linux (parity build), macOS (groundwork).

---

## Table of Contents

1. [What the game is](#1-what-the-game-is)
2. [The core concept & roles](#2-the-core-concept--roles)
3. [Full game flow](#3-full-game-flow)
4. [Survival mechanics](#4-survival-mechanics)
5. [Movement & special moves](#5-movement--special-moves)
6. [Abilities by role](#6-abilities-by-role)
7. [Objectives & interactables](#7-objectives--interactables)
8. [The education layer — Physics Classroom revision](#8-the-education-layer--physics-classroom-revision)
9. [Visual questions & the diagram system](#9-visual-questions--the-diagram-system)
10. [The atmosphere / horror director](#10-the-atmosphere--horror-director)
11. [Jumpscares](#11-jumpscares)
12. [CCTV & security systems](#12-cctv--security-systems)
13. [Lighting, environment & level construction](#13-lighting-environment--level-construction)
14. [Audio](#14-audio)
15. [Train intermission](#15-train-intermission)
16. [Final escape](#16-final-escape)
17. [Powerups & the shop economy](#17-powerups--the-shop-economy)
18. [Bot AI](#18-bot-ai)
19. [Maps](#19-maps)
20. [Authored-map pipeline](#20-authored-map-pipeline)
21. [Multiplayer & networking](#21-multiplayer--networking)
22. [Accounts & progression](#22-accounts--progression)
23. [Classroom deployment](#23-classroom-deployment)
24. [Feedback & telemetry](#24-feedback--telemetry)
25. [UI — menu, classroom board, HUD](#25-ui--menu-classroom-board-hud)
26. [Performance modes & accessibility](#26-performance-modes--accessibility)
27. [Code architecture](#27-code-architecture)
28. [Configuration & tuning reference](#28-configuration--tuning-reference)
29. [Build, packaging & tooling](#29-build-packaging--tooling)
30. [Automated tests](#30-automated-tests)
31. [Version history & roadmap](#31-version-history--roadmap)
32. [Assets & licensing](#32-assets--licensing)
33. [Known caveats & inconsistencies](#33-known-caveats--inconsistencies)

---

## 1. What the game is

**Blackout Hunt** is a **direct-IP multiplayer asymmetric horror game** that doubles as a **classroom physics-revision tool**. It blends *Dead by Daylight*-style "one hunter vs. many survivors" play with spaced-repetition study mechanics.

- One player (the **Teacher / Hunter**) stalks the others through a dark facility.
- The other players (**Survivors**) must answer physics questions at stations, repair the building's power (breakers), and escape — all while being hunted in the dark.
- It supports **2–12 players** in normal play (the shipped classroom config raises the hard cap to **32** for a full class).
- It runs over **LAN, a host-created Wi-Fi hotspot, the public internet** (port-forward or a Playit tunnel), or **online services** (EOS / Steam).
- Levels are **generated procedurally in C++ at runtime** (no hand-authored map needed to play), with an opt-in pipeline to bake them into hand-editable maps.

The signature twist is the education layer: in **Physics Classroom mode**, every objective is an IGCSE-style physics question, learning literally gates the exit (the class must hit mastery targets to unlock escape), and a spaced-repetition system re-surfaces questions students get wrong until they get them right. It is designed to be safe and practical to run on locked-down school PCs.

**One-sentence pitch:** *Survive a hunter in the dark by answering physics questions and restoring power to escape — a genuinely playable multiplayer horror game that is also a real classroom revision tool.*

---

## 2. The core concept & roles

The match is **asymmetric**: one side hunts, the other survives and studies. A single C++ pawn class (`ABHCharacter`) is used for every role; behavior branches on the player's assigned role. In code the role enum (`EBHPlayerRole`) is:

| Code name | Player-facing name | Count | Purpose |
|-----------|--------------------|-------|---------|
| `Hunter` | **Teacher** | usually 1 (host can set 1–3) | The monster. Captures survivors, scares, cuts power. |
| `Survivor` | **Survivor / Student** | the majority | Answers questions, repairs power, escapes. Has all the survival meters. |
| `FakeHunter` | **Hall Monitor** | dynamic | A *captured* survivor who returns to help — looks like the Teacher, can't capture, lays traps and sends hints. |
| `Tester` | Tester | test-only | Counts as both an alive survivor and an alive hunter; used for solo testing. |
| `Spectator` | Spectator | late joiners | Supports the team, requests a role for next round. |
| `Unassigned` | — | lobby | Pre-assignment state. |

**Win conditions:**
- **Survivors win** if at least one survivor escapes (through the exit gate, or by boarding the final-escape train).
- **Teacher wins** by capturing every survivor, or by running out the hunt timer.

A captured survivor is **not eliminated** while other survivors remain — they return as a **Hall Monitor** (the `FakeHunter` role), keeping caught players in the game as a support role rather than benching them. The Teacher gets +40 "hunter points" per capture; the captured survivor takes a 25% question-point penalty.

---

## 3. Full game flow

The whole match is a state machine, `EBHRoundPhase`: `Lobby → Prep → Hunt → Intermission (train) → FinalEscape → SurvivorsWin / HunterWin`. The flow is driven server-authoritatively by `ABHGameMode`.

### 3.1 Boot & main menu
The game opens on a **native Slate main menu** (not a level) with account login, host/join options, a Live Classroom flow, settings, and an in-match escape menu. Startup scans the local CPU/RAM/GPU and picks the safest graphics preset.

### 3.2 Lobby (`EBHRoundPhase::Lobby`)
- A host opens a **listen server** (UDP 7777) and players join via `host:7777`, a saved classroom endpoint, a `BH1:…` join code, or an online session.
- Students are prompted for a display name if they don't have a useful local profile name; that name shows in the host roster.
- The host assigns each connected player a role (Teacher / Survivor / Hall Monitor / Auto) and can **soft-kick** stuck or misjoined players.
- **Everyone must ready up** (Enter). When ≥ `MinPlayers` are present and all are ready, the round starts. (Force-start is disabled in classroom builds.)

### 3.3 Prep / "Role Warmup" (`EBHRoundPhase::Prep`, default **45 s**)
- Roles are assigned (`AssignRoles`): the configured number of Teachers (clamped 1..N-1, bots preferred for the Teacher slot unless a human chose it), captured-eligible players become Hall Monitors, everyone else is a Survivor — always guaranteeing at least one survivor.
- Survivors can **practice** objectives with no scoring; decoys/traps placed in warmup are cleared when the real hunt begins.

### 3.4 Hunt (`EBHRoundPhase::Hunt`, default **900 s** / 15 min)
The active round:
- Survivors answer station questions (`1-4` or typed numeric), then **hold E** to do the physical task; they repair breakers (default **6 required**); they manage flashlight/stamina/fear; they hide, drop decoys, pull alarms.
- The exit unlocks only when breakers **and** side objectives are complete (and, in revision mode, when mastery + contribution gates pass).
- The Teacher hunts: capture (`Mouse1`), heartbeat scan (`Q`), blackout surge (`R`), search lockers.
- The **atmosphere director** runs continuously, choosing scares, jumpscares, whispers, and "cold-call" pressure events based on each survivor's fear/dread and proximity to the Teacher.
- The round ends if all survivors are captured (Teacher win), the timer expires (Teacher win), or a survivor escapes (Survivors win — see Final Escape on the final stage).

### 3.5 Train intermission (`EBHRoundPhase::Intermission`)
Between hunt stages, players board a train. Phases: **Arrival → Recap → BonusQuestion → Shop → StationStop → Departing**. Players review missed questions, answer bonus questions for extra points, spend points in the shop, and use snack/drink/minigame stations. Doors lock and a moving-tunnel illusion plays while in transit. (Full detail in §15.)

### 3.6 Final escape (`EBHRoundPhase::FinalEscape`)
On the **final stage**, instead of an instant win the first time a survivor reaches the exit, a scripted climax triggers: a ~6.5 s cutscene (everyone frozen), then the evacuation-train doors open and survivors have **120 s** to reach a green door. Teachers are held back ~3.5 s, then released, and are penalized for camping the doors. (Full detail in §16.)

### 3.7 Round end & travel
On `EndRound`, the result phase is set, every client is told the result (driving XP/cosmetics and the end-of-round survey), a 60 s revision review is armed in classroom mode (and the class report auto-exports), and the game ServerTravels to reset/advance after ~8 s. Stages advance 0→1→2; after the final recap, the persistent run resets and travel returns to Facility.

**Mid-round reconnect:** a player who drops can rejoin within `ReconnectGraceSeconds` (**120 s**) and have their exact role, life state, points, revision stats, and powerups restored (matched by stable net id, else by display name). This survives ServerTravel via a process-wide monotonic clock.

---

## 4. Survival mechanics

Survivors carry four interacting meters (replicated on `ABHCharacter`); they only run during the Hunt while the player is an alive survivor.

### 4.1 Flashlight
- Battery 0–100, drains at **0.17/sec** while on (~10 minutes from full). Toggle with `F`; blocked when dead, warns at ≤20.
- Refilled by batteries (+45) and the Light Boost powerup (+82). Infinite for Testers/test mode. Suppressed on the intermission train.
- The beam's intensity, cone, volumetrics, and flicker are cosmetic and react to battery, fear/dread, and fog.

### 4.2 Stamina (0–100)
- Drains while sprinting (**20/sec** × role/powerup/fear multipliers) and on every special move; recovers at **14/sec** when grounded and not sprinting (with a brief lock after landing/exhaustion).
- Gates sprint (needs >8%), special moves, and the Teacher's capture swing.

### 4.3 Fear (0–100)
- Rises near the Teacher (scaled by distance, ~3.5 + 14×proximity per second; ×1.62 if hidden in a locker, ×1.75 if the Teacher has line of sight), on wrong answers, and from scares/scans/cold-calls. A heartbeat scan adds +16 (or +24 if you carry a detention mark).
- Decays when no Teacher is near.
- High fear **degrades** the survivor: slower walk/sprint, more stamina drain, slower recovery, and **louder footsteps** (up to ~+70% noise). Past thresholds it forces panic-breathing noise pings that reveal you.

### 4.4 Dread & the anti-camp system
- Dread rises from Teacher proximity, ambient stress (presence level, objective progress, darkness), long hiding, and detention marks.
- **Anti-camp:** you must keep moving. Within a **60 s** grace window you have to accumulate **5 s** of real movement covering **650 units** above a speed threshold. At 50 s you're warned ("Keep moving…"). Past 60 s idle, dread/fear pressure ramps; after a further 18 s delay, every 18 s your character gives a **"restless breathing"** noise that reveals your position to the Teacher and bots. Meeting the move requirement refunds a little dread. This stops survivors from turtling in a corner forever.

### 4.5 Detention mark
- A wrong answer applies a **detention mark** (duration scales with consecutive wrong answers, e.g. ~38 s + 8 s per streak step). While marked you move noisier, accrue extra dread, and the Teacher's heartbeat scan is **heavily biased toward you** (and hits you for double fear).
- Cleared by a correct answer (which also calms fear/dread and gives a small stamina/flashlight boost before the physical task).

---

## 5. Movement & special moves

Movement is server-authoritative and role-tuned. Base walk/sprint speeds differ by role (Survivor 360/900; Teacher 315/1150 but with higher stamina cost and noise; Hall Monitor in between).

**Special moves** (each costs stamina, has a cooldown, emits noise, and is validated for space — forward clearance, floor probe, slope ≤48°, drop height):

| Move | Input | Duration | Stamina | Cooldown | Notes |
|------|-------|----------|---------|----------|-------|
| **Roll** | sprint + crouch (`Shift`+`Ctrl`) | 0.55 s | 12 | 1.25 s | dodge |
| **Slide** | sprint + prone (`Shift`+`Alt`) | 0.75 s | 16 | 1.60 s | ends prone |
| **Dive** | prone + jump while moving | 0.65 s | 22 | 2.40 s | only move usable from prone |
| **Prone** | `Alt` | toggle | — | — | shrinks capsule, cuts speed (120), lowers visibility (×0.55) and noise (×0.38) |
| **Crouch** | `Ctrl` | toggle | — | — | speed 205; stand needs headroom |
| **Bunny-hop** | hold `Space` | — | — | — | buffered re-jump within 0.16 s of landing |

Special moves grant a brief **i-frame window** for dodging the Teacher's capture swing (see §6). Going prone roughly halves CCTV detection range and lowers your sight line.

A `UBHMovementAnimInstance` drives animation state; if no animation blueprint is bound, the character falls back to single-node animation or a fully **procedural cube-skeleton** animation (bob, breathing, limb swing) so it works even with no imported character art. Movement numbers live in `UBHMovementTuningAsset` / `UBHGameSettings`, with an identical hardcoded fallback table in the character.

---

## 6. Abilities by role

### 6.1 Teacher (Hunter)
- **Capture (`Mouse1`)** — a 3-phase swing: ~0.30 s windup (move speed cut to 0.54×), then resolve, then recovery. Costs 5 stamina. Outcomes:
  - **Hit** → captures the best candidate in front (within 220 + forgiveness, aim/LoS checked).
  - **Flashlight stagger** — a survivor shining their flashlight in the Teacher's face (within ~980, good aim, enough battery) **cancels the swing**, loses 22 battery, and staggers the Teacher (1.35 s recovery).
  - **Timed evasion** — a survivor mid-roll/slide/dive or just-proned dodges the swing.
  - **Door slam** — a survivor slamming a door near the Teacher interrupts the swing (1.2 s recovery).
  - **Miss** → ~1.05 s recovery.
- **Heartbeat scan (`Q`)** — cooldown ~25 s (shorter with the Scan Focus passive), range ~3000 (wider with Scan Focus). Spikes fear on all survivors in range, pings the nearest back to the Teacher (with a "Hidden" flag), and with Patrol Intel reports recent-noise direction. Detention-marked survivors are weighted heavily as targets.
- **Blackout surge (`R`)** — cuts the lights in an area (wider/faster with the Blackout Surge passive), adding terror and dread.
- **Locker handling** — captures hidden survivors through the normal capture flow.

### 6.2 Survivor
- **Answer questions** — `1-4` for multiple choice; for calculation questions, type a numeric answer and press Enter (tolerance-checked).
- **Interact (`E`)** — repair breakers (hold), take batteries, use the exit, operate terminals/switches. The server re-validates distance and **line of sight** (anti-wallhack).
- **Hide in locker (`E`)** — forces flashlight off, adds dread; press E again to exit (noisy if you're scared).
- **Drop decoy (`G`)** — spawns a noise decoy ahead to bait the Teacher/bots; 10 s cooldown.
- **Pull alarm** — one-shot panic alarm (deliberately loud; adds fear/dread to the puller but can be tactical).

### 6.3 Hall Monitor (FakeHunter)
Cannot capture. Looks like the Teacher. Tools (gated by a contribution requirement in revision mode):
- **Real hint (`Q`)** — tells every Teacher the nearest survivor's compass direction + distance.
- **False corridor marker (`R`)** — sends Teachers a fake survivor location to mislead them; can also spoof false CCTV motion at a monitor.
- **Trap (`G`)** — deploys an alarm trap that survivors can dodge until it arms.

---

## 7. Objectives & interactables

All interactables implement `IBHInteractableInterface` (`CanInteract`, `BeginInteract`/`EndInteract`, label + a rich prompt struct with hold time, progress, risk/noise/danger flags). Most derive from `ABHInteractableActor`.

| Actor | What it is | Key behavior |
|-------|-----------|--------------|
| **Breaker** | Power-restore objective | Hold E to repair (6 s solo; **multiple repairers speed it up**); 6 required by default; emits noise; gauge/levers/lights animate; completing it counts toward exit unlock. |
| **Objective station** | The question node | Answer then hold E to finish; four types (Valve/Terminal/Antenna/Evidence) map to the four physics topics. See §8. |
| **Exit gate** | Escape point | Color-coded beacon: dark (inactive) → amber-red (locked) → pulsing green (escape-ready). Shows *why* it's locked (power/tasks remaining, or class/your mastery, or contribution). |
| **Locker** | Hiding spot | Survivor hides; Teacher captures hidden survivors; occupied indicator glows red. |
| **Noise decoy** | Thrown bait | 8 s lifespan; procedural-audio pulse; lures the Teacher/bots. |
| **Panic alarm** | One-shot pull | Deliberately loud; alerts the Teacher; single use. |
| **Battery pickup** | Consumable | +45 flashlight, +32 stamina, −18 fear; one use. |
| **Door** | Swinging door | Walk-through; **slamming it shut near the Teacher interrupts a capture swing**. |
| **Sliding gate** | Lift gate | Raises ~265 units; used on Foggrounds. |
| **Crawl-space volume** | Low passage | Only prone/sliding/diving survivors may enter; anyone standing (or the Teacher) is physically ejected. |
| **Alarm trap** | Hall Monitor tool | Proximity trap; arms after ~1.15 s; spikes fear and reveals a survivor, then self-destructs. |
| **Student scare switch** | Survivor counter-scare | Triggers a class counter-scare on a cooldown (45–90 s); severity 1–4. |
| **Powerup shop terminal** | Train shop | Buy powerups with question points (survivors) or capture points (teachers); only during Shop/StationStop. |
| **Breakable glass pane** | Hazard | Cracks then shatters from damage/physics/interaction; **shattering is loud** — feeds both the Teacher's AI and the horror director. |
| **Button module + receiver relay** | Reusable puzzle wiring | A button (press/hold/toggle/one-shot) that drives one or more receivers via an interface — decouples buttons from what they control for designers. |

---

## 8. The education layer — Physics Classroom revision

This is what sets Blackout Hunt apart from a normal horror game. When revision mode (`EBHRevisionMode::PhysicsClassroom`) is on, every objective station is a physics question and **learning gates the win**.

### 8.1 Question bank (`FBHRevisionQuestionBank`)
- **376 built-in IGCSE-style questions** on this branch (validated distribution: **94 per topic**, ~28 Easy / 38 Medium / 28 Hard each). (An older test references 368/92; the bank grew to 376.)
- **4 topics** (`EBHPhysicsTopic`): Forces & Motion, Electricity, Waves, Energy.
- **3 difficulties** (`EBHQuestionDifficulty`): Easy, Medium, Hard (mastery weight 1.0 / 1.2 / 1.5).
- **7 question types** (`EBHQuestionType`): MultipleChoice, TrueFalse, Calculation, FormulaFill ("IGCSE Skill"), GraphReading, DragDropMatching, Ordering.
- A question (`FBHRevisionQuestion`) carries: id, topic, subtopic, difficulty, type, diagram type + diagram params, prompt, answer payload (choices / correct index / formula / numeric answer + tolerance), hint, correction prompt, explanation, mastery weight.
- **Answer-bearing fields are server-only** (`COND_Never` replication) so a modified client can't read the answer off the wire; choices are rotated by a location-seeded shuffle to defeat memorization.
- **Editable by teachers (Pillar 5):** drop a `Saved/ClassroomPresets/QuestionBank.json` to replace the bank with no rebuild (runtime-read, structurally validated; invalid files are backed up and the built-in bank is used). Export a starting template with the `bh.Revision.ExportQuestionBank` console command.

### 8.2 Mastery model — "demonstrated and durable" (in `ABHGameMode::RecordRevisionAnswer`)
Per-topic mastery is 0–100:
- **Correct:** `+15 × difficultyWeight × headroom`, where `headroom = max(1 − mastery/100 × 0.6, 0.25)` — gains shrink as you approach mastery, so luck can't cap a topic.
- **Wrong:** `−7 × missWeight` (Easy miss 1.2, Medium 1.0, Hard 0.8 — careless easy misses cost the most). Blind 25%-chance guessing trends *negative*.
- **Review ceiling:** a topic is capped at **80%** while you still have an unresolved missed question queued in it.
- **Overall mastery** = mean of *enabled* topics (so a single-topic lesson isn't penalized).

### 8.3 Spaced-repetition review queue (on `ABHPlayerState`)
- A wrong answer enqueues that exact question id (queue capped at 8, dedup-moves-to-back).
- It re-surfaces ("SECOND CHANCE") at the next station or train bonus terminal until answered correctly, which dequeues it. A correct review counts as a "correction" (full mastery, half points).

### 8.4 Anti-gaming locks
- After a wrong answer, the station holds resubmission for `3 + 1.5×(streak−1)` seconds (clamped 3–9) and reloads a fresh question so the just-revealed answer can't be re-entered. The hold never pins you — you can walk away.
- Train bonus terminals add a flat ~4 s correction hold and **build mastery but do not satisfy** the Hall Monitor contribution gate.

### 8.5 Exit gating & team answers
- The exit unlocks only when **class average ≥ class threshold** (default 70%), **your mastery ≥ individual threshold** (default 50%), **and** you've met a contribution count (**5** on the first stage, **6** on later stages — `ComputeLiveRevisionContributionTarget`, frozen per round). The HUD, exit gate, and classroom board now display this *exact enforced* value; a prior `[1,4]` display clamp made the gate look satisfied at 4 while it still required 5–6, so the exit and Hall Monitor tools never unlocked when the UI said they should.
- **Team-answer stations:** the answer team is the players **physically present at the node** (`BuildRevisionAnswerTeam`, rebuilt on every vote). A lone student — or a Hall Monitor working a node alone — can resolve it themselves; a cluster votes 1-4 and a majority resolves the question for the whole team, crediting all voters. A teammate who wanders off can't soft-lock the node. (Previously the team was a class-wide committee picked by lowest contribution, which could leave a present player permanently stuck at "Vote recorded (1/5)" because the other members never came to that node — the main cause of "the Hall Monitor can't complete questions".)

### 8.6 Lesson presets (`FBHLessonPreset`)
A teacher configures: topic mask, difficulty mix (Balanced/Easy/Hard/Adaptive), class & individual thresholds, round length, scare intensity (0–3), map, bot count/difficulty, and comfort toggles. Built-in presets: **Electricity easy**, **Mixed exam prep**, **Low scare**, **Hard mode**. Presets serialize into URL options that launch the round; they can also generate a **printable 12-question worksheet** as a markdown/offline backup.

---

## 9. Visual questions & the diagram system

The "visual questions" overhaul (the current branch's focus) makes questions **require reading a diagram**. A shared, pure, canvas-agnostic renderer `FBHDiagramRenderer` draws the same picture in three places: the player's HUD question panel, the train bonus terminal (render-to-texture), and an offline PNG-export commandlet.

**Key design rule:** diagrams show the *givens*, never the *answer*. This is enforced by construction and verified by an automated answer-safety test. Editorial "concept captions" (e.g. "gradient = velocity") that *would* give away an identify/recall answer are shown only on question types where they can't (Calculation/DragDrop/Ordering), and suppressed on MC/TF/graph-reading.

### 9.1 The 21 diagram types (`EBHDiagramType`)
`None` plus: **MotionGraph, VelocityGraph, ForceArrows, SpringGraph, MomentBeam, Circuit, IVGraph, StaticCharge, Wave, EMSpectrum, RayDiagram, Sankey, EnergyChain** (original set), then appended (serialized by name, append-only): **Lens, Transformer, MagneticField, InclinedPlane, PressureColumn, EnergyBars, ParticleModel**.

Examples of what they draw and how data maps in:
- **ForceArrows** — horizontal vectors whose lengths scale to ValueA/ValueB; variant 1 adds a vertical free-body pair.
- **Circuit** — battery loop; ShapeVariant 0/1/2/3 = single / series-2 / parallel-2 / series-3 resistor.
- **IVGraph** — current-voltage curve; variant 0/1/2 = ohmic / filament / diode; optional plotted point from ValueA/B vs C/D.
- **Wave** — animated sine; ValueA = amplitude fraction, ValueB = cycles.
- **EMSpectrum** — 7 colored bands; variant 1..7 brackets the highlighted band.
- **Transformer** — coil turn counts reflect ValueA (Np) / ValueB (Ns).
- **RayDiagram / InclinedPlane** — angle from `AngleOrShape`.
- **EnergyBars** — up to 4 bars sized by ValueA..D.

### 9.2 ShapeVariant
`FBHDiagramParams::ShapeVariant` is a per-type integer selecting *which* case a diagram type renders, so one type covers many questions (0 = historical default). Conventions are documented per type in `BHTypes.h`.

### 9.3 Diagram parameters (`FBHDiagramParams`)
`ValueA..D` (numeric magnitudes → arrow lengths, bar heights, turn ratios, plotted points), `LabelA..D` (annotations), `XAxis`/`YAxis` (graph axis captions), `AngleOrShape`, `ShapeVariant`, and an optional `ImageSoftPath` for an illustrated texture override. The renderer only draws data labels when the "enhanced" master switch is on (CVar `bh.Diagrams.Enhanced`; turning it off falls back to a generic schematic with no data — a runtime-reversible safety valve).

### 9.4 Render paths
1. **HUD** — drawn directly onto the HUD canvas inside the question panel.
2. **Train terminal** — optional (CVar `bh.Diagrams.TerminalRT`, off by default): draws onto a 512×256 render target bound to material `M_BH_DiagramRT`, so the diagram glows on the terminal screen. Default shipped behavior folds the diagram's "Given:" data into the prompt text instead.
3. **PNG export** — the `BHRenderDiagrams` commandlet renders every type/variant to PNGs for visual QA.

### 9.5 In-game preview & commandlets
- Console vars for QA: `bh.Diagrams.PreviewType N`, `bh.Diagrams.PreviewVariant N`, `bh.DiagramCoverage`, `bh.Diagrams.Enhanced 0/1`.
- **Editor commandlets** (`UCommandlet`, editor-only): `BHCreateDiagramMaterial` (authors the `M_BH_DiagramRT` material), `BHRenderDiagrams` (PNG export/QA), `BHExportLevel` (seeds authored `.umap`s — §20), `BHCreateHunterStateTree` (builds the hunter-AI StateTree asset — §18).

---

## 10. The atmosphere / horror director

The horror is orchestrated by two cooperating pieces: `ABHGameMode::TickDirector` (the scheduler — *when* and *who* to scare) and `UBHAtmosphereDirector` (the gatekeeper — a per-player **pressure budget + cooldown ledger** every scare must pass through). The whole system scales with a single **scare intensity** value (0–3; 3 in normal play, per-lesson in classroom mode; 0 disables all non-subtle cues).

### 10.1 Stimulus → presence
Anything notable reports an `EBHAtmosphereStimulusType` (Noise, Objective, Locker, Footstep, CCTV, Power, Monster, Manual) with a strength. This raises a **presence level** (0–100) by type — Monster/Manual 60, CCTV 44, Objective 38, Power 35, Locker 30, loud Footstep 18; plain Noise never raises presence. A spike also flickers nearby lights (amber→sickly-green) and may fire a reactive cue (a locker knock, footstep echo, blackout blink, or whisper) if the stimulus is strong enough and off cooldown.

### 10.2 The scheduler (`TickDirector`, Hunt only, every ~7 s / 4–6 s in revision)
Computes `DirectorPressure = presence×0.62 + timePressure×0.24 + objectiveProgress×0.28`. Rolls (a) a **scare** (base chance scaled by pressure, on a cooldown) and (b) a **cold-call** event. Scare targets are scored per survivor by dread, fear, near-miss proximity, detention mark, and remaining scare budget.

### 10.3 Scare types (`EBHScareEventType`)
Ambient, MonsterCharge, FaceFlash, AudioStinger, LightCut, CCTVGlitch, LockerKnock, Whisper, FootstepEcho. Classified as **heavy** (monster/face-flash/stinger/CCTV-glitch), **jumpscare** (monster/face-flash), or **subtle** (ambient/whisper/footstep-echo — the only cues allowed at intensity 0).

### 10.4 The per-player budget ledger
Each player has a regenerating pressure budget (max 0.58→1.72 and regen 0.010→0.038/s by intensity). Each cue costs budget (MonsterCharge is most expensive at ~1.12+; whispers/echoes are cheap). Cues are also gated by same-type cooldowns (MonsterCharge 74 s, FaceFlash 52 s, … Ambient 10 s, all scaled by intensity), a heavy-cue cooldown, a **same-location guard** (won't scare twice in the same spot too soon), and a jumpscare cooldown. If a desired cue is blocked, the director can **substitute** a cheaper one. Selection is deterministic per round seed (so it's reproducible but varied per player/location). Gameplay-critical cues bypass the budget.

### 10.5 Special events
- **Cold-call** — a classroom-flavored "the Teacher called on you": spikes fear/dread and presence, shows a status line ("The intercom says your name.", "You hear chalk write the answer you got wrong.").
- **Fake-out tension cue** — on its own 25 s cooldown: a phantom whisper/footstep *behind* you plus a brief light cut that resolves to **nothing** — building dread without a payoff.

---

## 11. Jumpscares

The real monster scare is a configurable variant system.

### 11.1 Variants (`FBHJumpscareVariant`)
Each has: id, display name, weight, **minimum scare intensity**, mesh/anim/material/sound (soft refs), world + close-up transforms, light color, and four "impact feel" fields:
- **ImpactFOVPunch** (0–30°, default 14) — FOV snaps inward on impact.
- **ImpactHitStopSeconds** (0–0.25, default 0.07) — brief client-local slow-mo.
- **ImpactRumbleIntensity** (0–1, default 0.85) — gamepad rumble.
- **ImpactStinger** — optional extra low-boom/transient audio layer.

The shipped pool seeds procedural proxies (ProxyRed/Cyan/Violet) plus real `FabMonster01–03` meshes, and **auto-discovers** "Whisper" variants by scanning content folders (pair-scoring meshes to materials/anims/sounds). A legacy SCP-096 proxy exists but is weight-0 (never auto-picked).

### 11.2 Approaches (`EBHJumpscareApproach`)
**HeadOn** (charges from ahead), **Behind** (spawns out of view, reveal on contact), **AlreadyThere** (spawns close and lunges), **CeilingDrop** (descends from above). The director picks an even mix but **never repeats the last approach**, and never reuses the last variant id back-to-back.

### 11.3 The monster actor (`ABHJumpscareMonster`)
A replicated actor that homes toward the target (or follows a scripted path), with a procedural cube-part body when no mesh loads. On contact (~190 units) it snaps to a close pose in front of the camera and sends a client horror cue with shake/jitter/flash plus the variant's FOV punch / hitstop / rumble / stinger. Lifespan and lock are randomized so repeats never feel identical.

### 11.4 Comfort
Three independent scalars apply on the client: **reduced jumpscares** (softens the close-up, skips hitstop and rumble, scales the scare to 0.45), **reduced flash** (quarters flash, skips the hard black-blink; lights and the monster mute their strobe to 0.25), and **reduced camera shake** (quarters shake and disables the FOV punch). Captions and high-contrast HUD round out the accessibility set. Defaults are configurable and player-overridable.

---

## 12. CCTV & security systems

A surveillance loop that pressures survivors and gives the Teacher (or bots) intel — with clear counterplay.

- **Security camera** (`ABHSecurityCamera`) — ticks every 0.35 s; detection range ~4200, cone ~44°, requires line of sight, Hunt phase, alive non-hidden survivor. **Stance matters:** prone roughly halves effective range and lowers the sight ray. On detection it: warns the survivor (with counterplay tips), sends every Teacher a **fuzzy "MOTION" ping** (offset 180–420 units — imprecise on purpose), alerts the zone/director, and may **glitch** (red lens, a CCTVGlitch scare cue).
- **Security monitor** (`ABHSecurityMonitor`) — the Teacher's terminal. A Teacher *actively holding* a monitor on a camera upgrades that camera's pings to a **precise named lock** (true location, refreshed). A Hall Monitor can use a monitor to **spoof a false motion ping** to Teachers (22 s cooldown). Survivors just see the live feed; it won't mark anyone for them.
- **Security shutter** (`ABHSecurityShutter`) — a circuit-gated barrier that lifts ~150 units; refuses to close on a living pawn (can't crush you).
- **Security terminal / power switch** (`ABHSecurityTerminal` / `ABHPowerSwitch`) — toggle a circuit's shutters or lights.
- **CCTV zone** (`ABHCCTVZone`) — a trigger volume that routes overlaps through a linked camera's full reveal/glitch pipeline even without line of sight (a "blind-spot extender").

**Circuit wiring (the counterplay):** opening a security circuit's shutters (via terminal or the shutter itself) **physically blinds the cameras and zones on that circuit**. So survivors counter cameras by: breaking line of sight, going prone, hiding in a locker, or opening the matching security circuit. The **DeadCCTV** round modifier disables most cameras for a round.

---

## 13. Lighting, environment & level construction

- **Flicker lights** (`ABHFlickerLight`) — replicated point-light actors that flicker with per-fixture frequency jitter (so each light has its own rhythm). They support **flicker bursts** (used by presence spikes and jumpscares) and bulk power-cuts. Reduced-flash players see a muted strobe.
- **How levels are built from blocks:** the procedural generator fills a `ABHStaticBlockField` with hundreds of `FBHStaticBlockSpec` entries (location/scale/rotation/tint/material/collision). It **batches them into instanced static-mesh components** keyed by render/collision signature — critical because a full level can be ~1800 blocks, and one component per block would register ~1800 components on every client. `ABHBlockActor` is the single-block variant reserved for geometry that needs independent replicated state.
- **Props:** `BHPropVisuals` is the shared toolkit (cube/cylinder/sphere meshes, PBR materials, text) that builds the cube-part visuals for cameras, monitors, terminals, etc. `ABHRuntimeMeshPropActor` places imported art by soft path without hard references.
- **Block materials** (`EBHBlockMaterial`): Tinted, Concrete, Plaster, RustedMetal, DiamondPlate, PaintedMetal, Tiles, WarningSign, FogSheet — loaded from `M_BH_*` materials, with FogSheet using a translucent material.
- **Footstep surfaces** (`EBHFootstepSurface`): Default, Concrete, Tile, Metal, Wet, Gravel, Glass, Soft — each with a noise multiplier, hearing radius, and atmosphere multiplier. **Glass is loudest/farthest-heard; Soft is quietest.** Surface is resolved from footstep components, physical materials, block materials, or actor tags, and feeds three consumers: AI hearing perception, the bot stimulus bus, and the atmosphere director.

---

## 14. Audio

- **`UBHSynthComponent`** — a procedural synth (no audio files needed). Ambient mode makes a two-sine drone modulated by a slow pulse plus white noise (whispers, footstep echoes, dread hums); a **scream mode** (triggered at high frequency + high noise) makes harsh inharmonic partials with vibrato for jumpscare launches.
- **`ABHAmbientEmitter`** — a thin replicated wrapper around the synth; the GameMode's `SpawnAmbient` helper is used by every atmosphere cue.
- **Gameplay audio identity cues** — positional sounds with captions for footsteps, flashlight on/off, CCTV static, breaker hum/complete, Teacher proximity, locker knock, and power-loss stinger (soft-referenced in settings).

---

## 15. Train intermission

Between hunt stages, players ride a train (`ABHTrainIntermissionManager`, server-authoritative, single phase timer).

**Phases & default durations** (`EBHTrainPhase`): Arrival (5 s) → Recap (35 s) → BonusQuestion (60 s) → Shop (45 s) → StationStop (30 s) → Departing (12 s). On the **final leg**, Recap goes straight to StationStop (a final leaderboard, no shop/bonus).

- **Doors & tunnel:** doors open during Arrival and StationStop; while "in transit" (Recap/Bonus/Shop/Departing) doors close, a moving-tunnel illusion plays (`ABHTrainTunnelMotionActor` streams light strips past the windows), and hidden barriers seal the doorways so nobody falls out. Players left outside are auto-boarded (teleported to safe interior spots) when doors close.
- **Recap** pulls class stats and the **3 most-recent missed questions** (with the correct answer and explanation) from the game instance and shows them on display screens.
- **Bonus question terminal** (`ABHTrainBonusQuestionTerminal`) — a single shared terminal for the whole class (per-player throttle + correction hold + one-answer-per-question so students don't disrupt each other). Correct answers pay a **1.25× point multiplier** (Easy 10 / Medium 15 / Hard 23), build mastery, and feed the spaced-repetition queue, but **do not** count toward the Hall Monitor contribution gate. The answer is never replicated. The diagram's givens are folded into the prompt text (or rendered to a screen if the terminal-RT CVar is on).
- **Activity stations** (`ABHTrainActivityStation`) — for everyone aboard:
  - **Snack cart** (+12 stamina, −3 fear), **Drink cooler** (+14 flashlight, +6 stamina, −1 fear).
  - **Reflex arcade** and **Memory table** minigames award 2 points on a win (routed to capture points for teachers, question points for everyone else).
- **Shop economy:** two currencies on the player state — **question points** (survivors, earned by correct answers) and **capture/hunter points** (teachers, earned by captures). Survivors buy chase tools; teachers buy passive upgrades. Each terminal shows cost, charges, effect, cooldown, and balance.

---

## 16. Final escape

On the final stage, the win is a scripted climax (`ABHEscapeStationManager`). State machine `EBHFinalEscapeState`: Inactive → Locked → Cutscene → EscapeActive → Departed / Failed.

- **Trigger:** the final-stage exit condition is met (converts Hall Monitors back to survivors first).
- **Cutscene (6.5 s):** everyone — including the Teacher — is input-frozen; the evacuation train arrives.
- **Escape window (120 s):** all escape doors open; players unlock but **Teachers stay frozen for the first 3.5 s**, then release. Survivors reach any green door (press E *or* just walk into the door volume) to board.
- **Anti-camp on Teachers:** a Teacher lingering within ~520 units of an open escape door for >4 s gets penalized (stamina drained, capture pressure disrupted, warning shown).
- **Win resolution:** each boarding survivor leaves play. During final escape, **survivors win once no alive survivors remain** (everyone still in play has boarded); the Teacher wins if the 120 s window expires with survivors still alive. (For the *standard* exit on non-final stages, the **first** escape wins immediately.)

---

## 17. Powerups & the shop economy

Powerups (`EBHPowerupType`, defined in `FBHPowerupLibrary`) are bought at train shop terminals and used in-match. Survivor charges live on the player state; the `UBHPowerupComponent` applies survivor effects with replicated end-times.

| Powerup | Cost | Charges | Effect |
|---------|------|---------|--------|
| **Stamina Boost** | 28 | 2 | +35 stamina now; recovery ×1.45, drain ×0.84 (95 s) |
| **Sprint Burst** | 36 | 2 | +10 stamina; sprint ×1.18 (4 s) |
| **Light Boost** | 24 | 2 | +82 battery; flashlight ×1.36 (70 s) |
| **Question Hint** | 18 | 3 | shows the active question's hint (20 s) |
| **Decoy Sound** | 32 | 2 | spawns a noise decoy |
| **Door Rush** | 38 | 1 | final-escape only: sprint/recovery boost (22 s) |
| **Anti-Scare Charm** | — | — | −28 fear, −32 dread |
| **Team Beacon** | — | — | (placeholder) |
| **Teacher Scan Focus** | 40 | 2 | passive: scan cools faster & reaches farther |
| **Teacher Blackout Surge** | 50 | 2 | passive: blackout hits more lights, shorter cooldown |
| **Teacher Patrol Intel** | 35 | 2 | passive: scans add recent-noise direction |

Survivor powerups map to **F1–F6** (StaminaBoost / SprintBurst / FlashlightBoost / QuestionHint / DecoySound / DoorRush). The three Teacher powerups are passives — bought as charge counts and read by the scan/blackout logic, not key-activated. Question-point value per correct answer: Easy 8 / Medium 12 / Hard 18 (×1.25 for bonus questions).

---

## 18. Bot AI

Bots (`ABHBotController`, an `AAIController`) can fill **any** role — Survivor, Teacher, or Hall Monitor — and they answer questions. All thinking is server-authoritative, on a ~0.25 s think interval, using UE perception (sight ~2800, hearing ~3220).

- **8 personalities** (`EBHBotPersonality`): Cautious, Objective, Bold, Trickster, Panicked, Aggressive, Suspicious, Ambusher — assigned deterministically from a name/id hash, pooled by role (e.g. Survivors lean Objective; Teachers lean Aggressive).
- **3 difficulties** (`EBHBotDifficulty`): Easy/Normal/Hard — affect decision noise, **answer correctness** (Easy ~56–72%, Hard ~92–99%), answer delay, and locker suspicion.
- **16 intents** (`EBHBotIntent`): Patrol, AnswerStation, WorkStation, RepairBreaker, Escape, Hide, Flee, Bait, Chase, InvestigateNoise, InvestigateLastSeen, SearchLocker, AmbushObjective, UseScan, UsePower, DropTrap. Each Think builds scored candidates and commits the best.
- **Stimulus memory** (`EBHBotStimulusType`: Sight, Noise, Locker, Objective, Trap, Capture, Escape, Unreachable) — last-seen/heard with decay (~12 s); memory is also **shared globally** so bots cooperate (e.g. avoiding a crowded objective via target claims).
- **Navigation:** runtime navmesh, approach-point finding (so a bot stands at a usable interaction spot, not inside the prop), target cooldowns for unreachable goals, and stuck handling.
- **Hunter behavior:** chase + capture (Teacher) or false-pressure/traps/misdirection (Hall Monitor — never actually captures); ambush points intercept between a survivor's last-seen position and an objective.
- **StateTree brain (optional):** Hunter bots can run a StateTree asset (`ST_BH_HunterAtmosphere`, built by a commandlet) with states like Patrol/Investigate/Chase/SearchLocker/Ambush; a watchdog falls back to the C++ policy if the tree stalls or ignores fresh stimuli. `bUseStateTreeAI` is **off by default**.
- **Policy subsystem** (`UBHBotPolicySubsystem`) — scores decision candidates; can load external linear weights from `Models/BlackoutHuntBotPolicy/bot_policy_weights.ini`, and self-disables (reverting to the C++ scorer) if it exceeds a per-call time budget.

---

## 19. Maps

Three runtime-generated levels:
- **Facility** — an abandoned facility (the reference map): concrete/plaster/tile, four quadrants, central hub with subway exits, color-coded route lighting, ~6 breakers, objective stations, lockers, batteries. Has a verified authored bake (~1790–2175 baked mesh actors).
- **Substation** — a larger 12-player utility/industrial map: transformer lanes, control rooms, perimeter routes, shutters, terminals, ~8 breakers, ~12 objectives, dense cover.
- **Foggrounds** — a large outdoor nighttime perimeter with heavy fog, service-road lanes, sheds, generator yards, sliding lift gates, two exits, ~10 breakers, ~14 objectives; this is the **final stage** with the final-escape climax.

**Round modifiers** (`EBHRoundModifier`) spice up a round: None, **LightsOut** (more circuits dark), **LoudFooting** (footsteps carry farther), **JammedDoors** (more doors shut), **DeadCCTV** (camera feeds offline), **PanicSurge** (scares/dread ramp faster). Fog has Light/Heavy/Extreme presets.

---

## 20. Authored-map pipeline

The runtime generator is the default, but a level can be switched to a hand-authored `.umap` with **zero behavior change by default**.

- **Detection:** drop a single `ABHLevelMarker` into a `.umap`; at BeginPlay the game mode discovers the placed gameplay actors (breakers, doors, exits, stations, escape managers, lights) instead of running the block generator. The marker carries level name, fog preset, stage index.
- **Gating:** `bUseAuthoredLevels` (default **False**) controls whether travel routes to `/Game/BlackoutHunt/Maps/<Level>` or stock `/Engine/Maps/Entry`. With it off, the resolver returns the identical Entry string — byte-for-byte unchanged.
- **Export commandlet** (`BHExportLevel`): builds the procedural level into a blank world, drops a marker, strips dynamic-material overrides (re-created at runtime), and saves the `.umap` — seeding an editable map from the generator. Run via `Export-AuthoredMaps.ps1`.
- **Code-driven bake:** an upgrade that produces a **fully editable, lit** `.umap` of real `StaticMeshActor`s (with PBR materials and player starts). Facility is done and verified; a "v2" cladding pass lays ContainersHouseCH wall panels. Critical correctness fixes are covered by tests (e.g. the hidden Teacher mirror-trap node is excluded from discovery, or the level would be unwinnable).
- **Twinmotion** path exists for art/lighting kitbashing via Datasmith import (meshes/materials/lighting only — gameplay stays in C++).

Enabling authored maps requires flipping `bUseAuthoredLevels=True` **and** adding the maps directory to the cook list together; rollback is just flipping the flag back (no code revert).

---

## 21. Multiplayer & networking

Implemented in `FBHNetworkSupport` and the game instance.

- **Direct-IP listen server** on UDP **7777**. Host with `HostGame`/`HostSubstationGame`/`HostFoggroundsGame`; join with `JoinGame host:7777`.
- **`BH1:…` join codes** — a URL-safe base64 encoding of a normalized `host:port` (no secret). Also accepts `blackouthunt://join/…` links. Join parsing is strict (rejects bad ports, non-`bh` schemes, unsafe host characters, paths/queries) before any travel.
- **Internet tunnel (Playit, Windows):** the game launches a **bundled `playit.exe` verified by pinned SHA-256** (never an arbitrary exe from PATH), scrapes the public allocation from its log, and the host copies a `BH1:` code for students. The classroom build ships an owned endpoint `blackouthunt.playit.plus:24761`.
- **Game hotspot (Windows):** creates a hosted-network Wi-Fi SSID via `netsh` for blocked classroom LANs (disabled by default).
- **Online subsystems:** default `Null` (local/dev). **EOS** and **Steam** are separate package profiles (own local-values INIs, temporarily injected config) reusing the same menu Host/Find/Join commands. EOS is the preferred no-Steam-fee path for public lobbies.
- **Reconnect grace:** 120 s mid-round rejoin with full state restore (see §3.7).
- **Engine failure surfacing:** network/travel failures are captured and shown to the player ("timeout / refused / version mismatch / class is full").
- Server tick is capped at 30 Hz to protect weak listen-server hosts; a per-controller RPC flood guard throttles spammable lobby actions (the engine's RPC-DoS detection block is present but commented out pending load-testing).

---

## 22. Accounts & progression

- **Profiles** (`UBHAccountSubsystem`): **Guest** (auto-created), **local username/password** (sanitized, AES-256-CBC + HMAC, machine-bound credential file, salted SHA-1 password hashing), or **Google/Microsoft OAuth**.
- **OAuth flow:** the game checks the backend `/health`, launches the system browser to `/auth/<provider>/start`, then **polls** `/auth/device/<id>` until authorized, receiving a session token + player object.
- **Progress:** rounds played, hunter/survivor wins, escapes, XP. Saved locally under `Saved\Account` (atomic writes with `.bak`/`.tmp` rotation, forward-compatibility lock). Synced to the backend with a bearer token when enabled.
- **Cosmetics** (`BHCosmeticUnlocks`) — **XP-gated, local, no gameplay effect**: 8 outfits (0–1250 XP), 8 shirt colors (free), 5 headwear (0–750 XP), gear (placeholder). XP loss can't keep a locked item equipped.
- **Classroom-safe defaults:** external login and backend sync are **off**; everything works locally with guest/local profiles. A "reset local classroom data" command wipes a shared school PC.
- **Backend** (`Tools/AccountBackend`): a dependency-free **Node.js** scaffold (`server.mjs`) — OAuth broker + device-poll bridge, JSON player persistence, **and feedback/telemetry ingest** with an owner-only dashboard. Endpoints: `/health`, `/auth/*`, `/auth/device/{id}`, `/player/me`, `/player/save`, `/feedback`, `/telemetry/session`, `/admin`. Ships with a Caddy TLS reverse proxy, Dockerfile, an `email-notify.mjs` helper, and start scripts (`Start-FeedbackStack.ps1`); provider secrets stay server-side. For the **feedback/telemetry path specifically**, the shipped game instead targets a zero-host **Google Apps Script web app** (`apps-script/Code.gs`, emails + Sheet) — the Node stack is the self-hosted fallback. See §24.

---

## 23. Classroom deployment

The project ships a **classroom-safe profile** by default:
- Classroom mode on, 32-player cap, external login/sync off, local profiles on, force-start off, host/admin controls restricted to the listen-server host machine.
- **Live Classroom** now binds **all interfaces by default** (`bClassroomLoopbackOnlyHost=False`), so students can join by **LAN IP** (same room/switch) **or** the **Playit tunnel / BH1 code** (off-LAN) — the tradeoff is a one-time Windows Firewall "Allow" on the host PC. Setting `bClassroomLoopbackOnlyHost=True` reverts to the old loopback-only binding (`127.0.0.1`, Playit-mandatory, **no LAN fallback** — if the school blocks Playit/UDP nobody can join), intended only for tunnel-only deployments.
- **Host-only tools:** lesson presets (save/apply/printable 12-question sets), a **Classroom Preflight** panel (version/endpoint/RHI/package/log/support-bundle paths), a "Run Class" runbook (11 numbered steps), question focus/complexity/mastery/duration/bot/scare controls, and anonymous playtest telemetry heatmap exports.
- **Classroom board** — a host-only projector view (no tactical info or answers): live session/roster/role/revision metric cards plus a per-student table (role, status, progress, per-topic mastery). Opens as a separate window.
- Students see only local display/audio/profile controls; they can join/ready/vote maps/change their own settings, but cannot assign roles, kick, force-start, run tunnels, or trigger admin scares.
- Reports go to `Saved\ClassReports` (CSV + a teacher recap markdown); presets to `Saved\ClassroomPresets`; telemetry to `Saved\PlaytestTelemetry` (host-explicit export only).
- A real first **live classroom test was scheduled for 2026-05-29**; the handoff docs note school PCs may block interactive consoles, motivating a non-console GUI launcher (`Tools/LowSpecLauncher`).

---

## 24. Feedback & telemetry

`UBHFeedbackSubsystem` collects, with consent, in-game feedback and lightweight diagnostics:
- **Feedback kinds:** Bug, Idea, Praise, Other, Survey. An end-of-round 20-second survey auto-prompts once per session.
- **Performance telemetry:** FPS min/max, hitch count, a histogram, device specs, and a recent-log tail (ring buffer attached to the log).
- **Transport:** POSTs feedback (`/feedback`) and telemetry (`/telemetry/session`) over **HTTPS** so player-identifying data isn't sent in cleartext; with no endpoint configured everything saves locally to `Saved/Feedback`. The **live shipped endpoint is a free Google Apps Script web app** (`FeedbackBackendBaseUrl` in `DefaultGame.ini` → `https://script.google.com/macros/s/…/exec?path=`): it runs always-on in the owner's Google account, **emails each submission to the owner and logs everything to a Google Sheet** (so it reaches the owner even when their PC is off), and now includes the reporter's PC specs + perf. The trailing `?path=` is required — the game appends `/feedback` or `/telemetry/session` as the **query value** because Apps Script rejects a real path suffix on `/exec`. The script lives at `Tools/AccountBackend/apps-script/Code.gs` (its `doGet` reports a `SCRIPT_VERSION`). The earlier **Playit tunnel → Caddy → Node backend (`server.mjs`, :8787)** stack remains as the self-hosted alternative (see `Tools/AccountBackend/DEPLOY.md`).
- Settings (`BHFeedbackSettings`): enable flags, diagnostics default, auto-survey, recent-log line count (200), min log verbosity (Warning), hitch threshold (100 ms).

---

## 25. UI — menu, classroom board, HUD

### 25.1 Main menu (`SBHMainMenu`, ~11k lines of Slate)
A start screen (START / HOW TO PLAY / CREDENTIALS) plus 10 tabs: **Play** (all host/join actions, organized into collapsible sections: Live Classroom maps, Test Rounds, Bots, Host LAN, Online Lobbies, Connection Tools, LAN Join Address + classroom join list + copy-join-code), **Guide**, **Classroom** (host-only suite — runbook, preflight, lesson presets, question controls), **Character** (avatar cosmetics with a live render-target preview), **Match** (round options, map vote, role assignment, kick, targeted scare), **Network** (suggested address + online session browser), **Account** (login + local credentials), **Controls**, **Settings** (audio / comfort / HUD / performance), **Feedback**. Nearly every button is a thin wrapper that calls a `…ForMenu` method on the player controller and shows the returned status string. Host-vs-student capability gates hide host tools from remote clients.

### 25.2 HUD (`ABHHUD`, immediate-mode canvas)
A hand-drawn "scanner/terminal" HUD (no UMG). Draws: timer + exit status, role panel (Teacher axe state / Hall Monitor tools / tester cheatsheet / spectator panel), vitals (battery, Teacher proximity, stamina/fear/dread, stress hints), a threat arrow + heat-sensor minimap when the Teacher is in sight, CCTV reveal markers, objective beats, a danger-tinted crosshair, nameplates, the interaction prompt, the **question panel + diagram** (via `FBHDiagramRenderer`), status toasts, and a phase banner. Theming (`BHHudTheme`) supports standard / **high-contrast** / **colorblind** palettes plus per-map accent tints, HUD scale (0.75–1.5), panel opacity, and six element toggles — all driven by player comfort preferences.

---

## 26. Performance modes & accessibility

- **Performance presets:** **Low 4GB** (45 FPS cap, dynamic resolution, small texture pool, no volumetric fog, integrated-GPU friendly), **High 16GB** (120 FPS cap, 1080p/100%), **Ultra** (uncapped). Startup auto-detects hardware; software/unknown/integrated/low-VRAM GPUs default to Low 4GB + 1280×720 windowed. Adaptive graphics (render scale + shadow/effects vs an FPS goal) is on by default. Classroom Windows builds default to **D3D11** and ship DX11 / DX11-Low launchers; a `-BHVirtualBoxSafe` path supports VM/lab validation.
- **Accessibility / comfort:** reduced jumpscares, reduced flash, reduced camera shake, captions (on by default), high-contrast HUD, colorblind palette, HUD scale/opacity, and a POV-animation intensity with a reduced-motion scale. Scare intensity itself is tunable per lesson (0 = no scares).

---

## 27. Code architecture

All gameplay is native C++ in `Source/BlackoutHunt/` (~120 source files). Highlights:

- **`BHTypes.h`** — the shared vocabulary: every enum (roles, phases, modifiers, stimulus types, question/diagram types, powerups, train/escape states, bot personalities/intents) and struct (question, diagram params, jumpscare variant, scare specs, powerup defs, revision stats, bot memory).
- **`ABHGameMode`** — the server-authoritative brain (~12k lines), split across `BHGameMode.cpp` plus partials: **BotServices** (bot roster/memory/claims/nav), **HostControls** (admin gating, role assignment, votes, kicks), **TrainFlow** (intermission + final-escape transitions). Owns level generation/discovery, round lifecycle, win/loss, atmosphere/scare scheduling, revision scoring, and reporting.
- **`ABHGameState`** — replicated match state (phase, timers, objective counts, presence, revision summary, train/escape state) plus Blueprint-pure text helpers for the HUD.
- **`ABHPlayerState`** — per-player role, life state, points (question/hunter + lifetime), revision stats, spaced-repetition queue, powerup charges, cosmetics, votes.
- **`ABHPlayerController`** — input bindings, the Server/Client RPC bridge to the game mode, local UI state (status toasts, CCTV reveal, horror-cue overlay), comfort/graphics/audio prefs, account commands.
- **`UBHGameInstance`** — survives travel: networking entry points, travel persistence + reconnect, question-attempt history (drives the train recap), telemetry export.
- **`UBHGameSettings`** — the config-backed tunables (all of §28).
- **Subsystems:** `UBHAccountSubsystem`, `UBHFeedbackSubsystem`, `UBHBotPolicySubsystem`.

Module dependencies (`BlackoutHunt.Build.cs`) include EnhancedInput, UMG/Slate, NetCore, AudioMixer, Sockets/HTTP/Json, Niagara, AIModule/NavigationSystem/StateTree, OnlineSubsystem (+EOS/Steam, EOS Win64-only, links `bcrypt.lib`), AssetRegistry; editor-only deps (UnrealEd, MaterialEditor, StateTreeEditor) support the commandlets.

A maintainability effort tracks splitting the two largest files (`BHGameMode.cpp`, `SBHMainMenu.cpp`); a `New-CodeHealthSnapshot.ps1` tool reports the largest files. (Note: numerous `.bak-*` and one `.tmp` file from a May 24 editing session sit beside the sources — they are backups, not part of the build.)

---

## 28. Configuration & tuning reference

The tunables live in `Config/DefaultGame.ini` under `[/Script/BlackoutHunt.BHGameSettings]`. Key values:

- **Round:** MinPlayers 2, **MaxPlayers 32**, PrepSeconds 45, HuntSeconds 900, RequiredBreakers 6, bAllowHostForceStart False, ReconnectGraceSeconds 120, bUseAuthoredLevels False.
- **Classroom:** bClassroomMode True, bAllowStudentTeacherAdminControls False, bAllowTunnelHelper True, bAllowHotspotHelper False, bClassroomLoopbackOnlyHost True, ClassroomJoinEndpoints = `blackouthunt.playit.plus:24761`.
- **Interaction/abilities:** InteractDistance 550, CaptureDistance 220, FlashlightDrainPerSecond 0.17, ScanCooldownSeconds 25, DecoyCooldownSeconds 10, BatteryRefillAmount 45, HunterSprintDrainMultiplierMax 0.85, HunterStaminaRecoveryMultiplier 1.75, TeacherAxeStaminaCost 5, TeacherAxeMinStamina 2.
- **Anti-camp:** Grace 60, Warning 50, RequiredMove 5 s / 650 units, SpeedThreshold 150, DreadPerSecond 2.6, FearPerSecond 0.55, AlertDelay 18, AlertCooldown 18.
- **Bots:** DefaultBotCount 5, DefaultBotDifficulty Normal, bUseStateTreeAI False, BotThinkInterval 0.25, BotSightRange 2800, BotHearingMemorySeconds 12, BotStuckSeconds 6.
- **Revision:** RevisionRoundSeconds 600, ClassThreshold 70, IndividualThreshold 50, ScareIntensity 3.
- **Train:** bUseTrainIntermissions True, Recap 35, BonusQuestion 60, Shop 45, StationStop 30, DepartureCountdown 12, StageOne/Two/Three 300/420/600.
- **Final escape:** 120 s window, 6.5 s cutscene, 3.5 s hunter release delay, anti-camp radius 520 / grace 4 / penalty 2.5.
- **Comfort defaults:** reduced jumpscares/flash/shake off, captions on, high-contrast off, HUD scale 1.0, opacity 1.0, all HUD elements on.

Other config: `DefaultEngine.ini` (GameMode/GameInstance classes, NetServerMaxTickRate 30, `DefaultPlatformService=Null`, DX11 default RHI, dynamic navmesh, UDP messaging disabled), `DefaultInput.ini` (the key bindings below), `DefaultScalability.ini` (low-end-friendly buckets). Account settings (`BackendBaseUrl=""`, external login off) and feedback settings (HTTPS endpoint, enable flags) have their own sections.

**Controls:** WASD move, mouse look, Enter ready, E interact/repair/exit-locker, F flashlight, 1-4 answer, Tab question cursor, N node marker, M/I map, V crosshair, F1–F6 powerups, Space jump/bunny-hop, Shift sprint, Ctrl crouch/roll, Alt prone/slide (+Space dive), Mouse1 Teacher capture, Q scan / real hint, R blackout / false hint, G decoy / trap, B classroom board, H/T/Y/U spectator support & role requests, Esc menu.

---

## 29. Build, packaging & tooling

The `Tools/` directory holds the whole pipeline (PowerShell + Python + shell). Highlights:

- **Build/iterate:** `Build-Editor.ps1` (fast editor compile), `Find-Unreal.ps1/.sh` (locate UE 5.7). A documented fast inner loop rebuilds the Shipping target and swaps the exe into `Builds\Windows` to avoid a ~30-minute cook.
- **Package:** `Package-Windows.ps1` (core; `-Classroom` adds distribution flags, app-local runtime DLLs, DX11 launchers, quickstart/notices, and runs the verifier), `Package-Windows-Classroom.ps1`, `-NoAccount.ps1`, `-EOS.ps1` (→ `Builds\WindowsEOS`, validates the EOS local INI), `-Steam.ps1` (→ `Builds\WindowsSteam`), `Package-Linux*.ps1/.sh` (incl. a Wine cross-cook), `Package-Mac.sh`. One-click `BUILD-WINDOWS-CLASSROOM.cmd` / `BUILD-WINDOWS-EOS.cmd` wrappers at the repo root.
- **Verify/stability:** `Verify-ClassroomPackage.ps1` (scans for forbidden files — pdbs, saved account/logs/reports, secrets, Steam/EOS local INIs — and verifies the bundled `playit.exe` hash), `Run-StabilityGate.ps1` (build → headless automation tests → packaged 2-client soak under normal + degraded network, asserting log markers), `Run-ClassroomStability.ps1` (full RC validation incl. a VirtualBox student-VM soak), `Run-PackagedClassroomSmoke.ps1`, `Test-RuntimeLogs.ps1` (log gate), `New-PlaytestEvidenceReport.ps1`, `New-ClassroomSupportBundle.ps1`, `New-CodeHealthSnapshot.ps1`.
- **Asset import (Python, via the editor Python plugin):** `Import*` scripts for ambientCG textures, Quaternius characters, KayKit weapons, SCP-096, Foggrounds NatureKit, audio; plus `CreateRouteMaterials.py`, `CreateMovementTuningAsset.py`, `CatalogMeshDimensions.py`, render/screenshot helpers, and `VerifyAuthoredMap.py`.
- **Authored maps:** `Export-AuthoredMaps.ps1`.
- **Low-spec/Wine:** `Tools/LowSpecLauncher` (no-console GUI launcher for locked-down PCs), `Run-Windows-Build-Wine.sh`, `WineArrowLook.py`.
- **Headless test command:** `UnrealEditor-Cmd "<project>" -ExecCmds="Automation RunTests BlackoutHunt; Quit" -NullRHI -unattended`.
- **Automation flags** (inert unless `-BHAutomation=1`): `-BHAutoHost=…`, `-BHAutoJoin`, `-BHAutoReady`, `-BHAutoMinPlayers`, `-BHAutoQuitSeconds`, `-BHAutomationTag`, `-BHVirtualBoxSafe` — these drive the soak/smoke scripts and write stable markers to `Saved\Logs\BlackoutHuntAutomation.log`.

---

## 30. Automated tests

~10 test files (guarded by `WITH_DEV_AUTOMATION_TESTS`, under the `BlackoutHunt.*` hierarchy), run headless. A `FBHScopedAutomationWorld` RAII harness builds a throwaway game world per test.

Coverage spans: the big `BHNativeProofTests` (28 tests — flicker restore, bot memory/policy, director budget throttling, host-authority gating, role-warmup safety, anti-camp movement, cosmetic thresholds, physics task identity, round modifiers, late-spectator/travel restoration, special-movement + crawlspace gating, capture windup/evasion/door-slam, button modules, CCTV detection/zones/monitor inspection, shutter circuits, footstep mapping, audio identity, breakable glass, objective beats, HUD clarity, exit-gate blocked reasons, reconnect-within-grace) plus focused suites for authored-level discovery, the automation command line, the feedback log sink, **revision** (mastery tuning, review queue, JSON round-trip, **diagram answer-safety**, diagram coverage/image paths/band heights), jumpscare spawn resolution & variants, network join-address normalization & BH1 codes, and the **train economy** (caught penalty, point caps, powerup gating, travel persistence). The 0.5.0 build reported **32 automation tests passing**.

---

## 31. Version history & roadmap

Beta history (all Windows classroom builds; OnlineSubsystemNull; Playit join endpoint):
- **0.2.0** — first classroom/LAN beta; classroom-safe defaults established.
- **0.2.1** — stability/automation pass (the `-BHAutomation` harness, VirtualBox-safe path, clean quit, network preflight).
- **0.2.2** — owned Playit endpoint as default join; lobby names; ready-up gate; late joins become survivor-spectators.
- **0.3.0** — app icon, doubled the question bank to 320, teacher map pick, restored SCP-096 prototype, switched Windows packages to D3D11.
- **0.4.0** — major gameplay layer: **train intermission + final escape**, StateTree hunter AI + bot nodes, **atmosphere director + CCTV**, powerups, expanded jumpscares; first Linux compatibility package.
- **0.5.0** — classroom-operations + content: Classroom Preflight, **lesson presets + printable sets**, telemetry heatmaps, anti-camp/prone/capture-windup, audio identity cues, expanded CCTV (Dead CCTV modifier), expanded train, Steam packaging, cosmetic persistence. 32 automation tests pass.
- **0.6.0** — **Authored Map Pipeline** (opt-in, behavior-identical by default), revision-quality anti-gaming + durable-mastery work, **editable JSON question bank**, and the **data-driven visual physics questions** (ray diagrams, graph reading). Windows freshly cooked 2026-05-29; Linux is a 0.5.0 parity build pending a re-cook.
- **0.7.0** — **interactive & visual questions** (click a diagram element, drag/order arrangements, on-screen keypad), the **three-role guided tutorial** (Survivor → Teacher → Hall Monitor, solo, no host), a **jumpscare overhaul** (in-your-face image, corner peek, behind-you payoff, real SCP-096), **authored maps on by default** (baked Facility/Substation/Foggrounds/Tutorial; fixes the block-field replication blowout), **2K structural textures**, and a packaged **EOS** Windows build (LAN/tunnel fallback in the same build). 62/62 automation tests pass.

The **`feature/visual-questions`** work — the diagram system at 21 types with ShapeVariant variants, full data-expected coverage (bank → 376), answer-safety + coverage tests, in-game preview tools, and opt-in terminal render-to-texture — landed in **0.7.0** and merged to `main`.

Roadmap docs (`ROADMAP.md`, `CODEX_GAME_IMPROVEMENT_TASKS.md`, `HIGH_VALUE_GAME_IMPROVEMENT_TASKS.md`, `REVISION_QUALITY_PLAN.md`) prioritize classroom reliability and clarity over new content, with stable task IDs: a polish track (P1–P10), a numbered backlog (1–19), high-value reliability tasks (HV1–HV10, e.g. packaged smoke test, low-spec readiness, rejoin hardening, teacher runbook, crash/log gate), and the five-pillar revision-quality plan (durable mastery, station/terminal anti-gaming, editable bank).

---

## 32. Assets & licensing

Tracked in `Docs/ASSETS.md` + `Docs/THIRD_PARTY_NOTICES.txt`:
- **CC0 / approved (shipped):** ambientCG PBR materials, Quaternius avatars, a KayKit axe prop, processed OpenGameArt scream audio, and native procedural jumpscare proxies.
- **Fab Standard License (shipped, conditional):** "Free Customizable Jumpscares" — only the migrated runtime subfolders are cooked (the source pack/demo maps are never cooked; `/Game/Free_Jumpscares` is redirected).
- **Excluded (risky/prototype):** SCP-096, Hider, Hunter/FNaTI, Whisper jumpscares, and all downloaded source archives — blocked until IP/license evidence is recorded.
- A **license-evidence gate is pending** for several packs cooked but not yet evidenced (SmartBasicInterfaces, surface-footstep, several horror-audio packs, ContainersHouseCH cladding) — must be verified before ship or dropped from the cook path.
- **Cook policy** is enforced in `DefaultGame.ini` (explicit always-cook allowlist + never-cook denylist) and the package verifier fails any build that stages source assets, secrets, or saved player data.

---

## 33. Known caveats & inconsistencies

These are accurate-as-of-this-writing notes a recipient should be aware of:

- **2026-06-02 live-play hardening pass (needs a Windows/Wine rebuild + playtest to verify):** (1) question-node answer teams are now **co-located** (present-at-the-node) instead of a class-wide committee, fixing a soft-lock where a lone player/Hall Monitor could never resolve a node (§8.5); (2) the contribution gate is now **displayed at its enforced 5–6** everywhere (was clamped to ≤4 in the UI/exit gate, so it looked satisfied while still locked); (3) Hall-Monitor station copy is role-aware (no more "hold E"/"CONTRIBUTE IN PHYSICS CLASSROOM" on a solved node), and the centered interaction prompt is suppressed under the question panel; (4) **`bClassroomLoopbackOnlyHost` now defaults `False`** so Live Classroom binds LAN + tunnel (§23, with a one-time firewall prompt); (5) travel/late-join state restore no longer name-matches on the OSS-Null path (a same-named joiner can't inherit another student's points/powerups/stats); (6) a multi-drop Wi-Fi blip no longer instantly ends the round while dropped survivors are inside the reconnect grace; (7) final-escape timeout credits a partial evacuation as a survivor win; (8) the online build-id is bumped to `BlackoutHunt-0.8.1` (with `ProjectVersion=0.8.1`), shipped as the **0.8.1** pre-release. The standing `[1,4]` contribution number cited just below is superseded by the 5–6 enforced value.
- **Version:** the live version is **`0.7.0`** — consistent across `ProjectVersion` (`Config/DefaultGame.ini`), the README/AGENTS "current beta target," the in-code online build-id (`BlackoutHunt-0.7.0` in `BHGameInstance.cpp`), the planning-task docs, the release notes (`Docs/BETA_RELEASE_NOTES_0.7.0.md`), and the GitHub pre-release `v0.7.0`, which targets a **Shipping EOS** Windows package as its primary artifact. `0.6.0` was retagged clean from `-beta.1` on 2026-05-31, matching the clean-semver convention of `v0.5.0`/`v0.4.0`; earlier releases keep their own cooked artifacts (the v0.6.0 Windows zip is still labelled `0.6.0-beta.1`). *(The build-id change needs a rebuild to take effect, and only same-build-id clients match in online sessions.)* Known remnants, all intentional: the **already-built Windows package** attached to the release was cooked as `0.6.0-beta.1`, so its zip filename and in-game version string still read `0.6.0-beta.1` until the next cook; the dated **live-classroom test logs** (`Docs/LIVE_CLASSROOM_*_2026-05-29.md`) correctly record the **`0.2.0-beta.6`** archive actually distributed for that test (its Windows zip shares the exact SHA-256 of the 0.5.0 zip — same artifact, different label); and historical references to `BETA_RELEASE_NOTES_0.5.0-beta.1.md` and the prior `0.5.0-beta.1` Linux artifact point at real, still-present files.
- **Question-bank count** is cited as 320 (0.3.0), 368, and 376 across docs as the bank grew; the current built-in distribution validates at **376** (94/topic), while one older test still references 368/92.
- **MaxPlayers** is 12 in the code default but **32** in the shipped `DefaultGame.ini` (the ini wins).
- **Linux** is behind (last parity cook 0.5.0); a 0.7.0 Linux re-cook is pending on a Linux host. EOS is verified for a Windows Dev cook; the 0.7.0 Windows release targets the first **Shipping EOS** cook.
- **Authored maps** are baked for Facility/Substation/Foggrounds/Tutorial and **on by default** in 0.7.0 (`bUseAuthoredLevels=True`, with the Maps directory cooked); the mesh-cladding art pass over the blockout is still in progress.
- **`.bak-*` / `.tmp` files** from a May 24 editing session sit beside many sources — they're backups, ignore them.
- A **concurrent automated writer** may edit this repo; check recent commit timestamps before any git write.
- A minor cosmetic artifact (`BHPlayerState.cpp` ~line 192) shows a comment starting with a stray backslash instead of `//` — likely a display quirk; worth a glance if it's literal.

---

*This document was assembled from a full read of the repository's documentation and source (core framework, character/survival, objectives/questions/diagrams, atmosphere/security/environment, train/escape/bots, networking/accounts/UI, and build/config/tests). For the authoritative numbers, the live source of truth is `Config/DefaultGame.ini`, `Source/BlackoutHunt/BHTypes.h`, and `Source/BlackoutHunt/BHGameSettings.cpp`.*
