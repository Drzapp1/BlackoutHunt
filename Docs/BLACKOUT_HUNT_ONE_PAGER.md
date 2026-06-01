# Blackout Hunt — One-Page Overview

> **Survive a hunter in the dark by answering physics questions and restoring power to escape.**
> A multiplayer horror game that is also a real classroom revision tool.

## What it is
Blackout Hunt is an **Unreal Engine 5.7 asymmetric multiplayer horror game**. One player is the **Teacher** (the monster); everyone else is a **Survivor** trying to answer questions, repair the building's power, and escape a dark facility before being caught. Its twist: in **Physics Classroom mode**, every objective is an IGCSE-style physics question, and *learning gates the win* — the class has to hit mastery targets to unlock the exit.

It runs for **2–12 players** (32-player classroom cap) over LAN, a Wi-Fi hotspot, the internet (Playit tunnel), or EOS/Steam — and is built to work on locked-down school PCs.

## How a match plays
**Lobby** (assign roles, ready up) → **Prep** (45 s warmup) → **Hunt** (~15 min: answer stations, repair 6 breakers, avoid the Teacher) → **Train intermission** (review missed questions, bonus round, shop) → repeat across stages → **Final escape** (scripted climax: doors open, 120 s dash to the train) → **win/lose**.

- **Survivors win** if one escapes. **Teacher wins** by catching everyone or running out the clock.
- Caught survivors aren't benched — they return as **Hall Monitors** who lay traps and send (real or fake) hints.

## What makes it different
- **Real revision built in** — 376-question physics bank (4 topics, 7 question types), a "demonstrated & durable" mastery model, and a **spaced-repetition queue** that re-asks anything you got wrong until you get it right.
- **Visual questions** — questions require reading a generated **diagram** (21 types: circuits, free-body, graphs, ray diagrams, transformers…). Diagrams show the *givens*, never the answer.
- **Teacher-run, school-safe** — host-only classroom board (projector view), lesson presets, printable worksheets, mastery goals, and full comfort/accessibility options (reduced jumpscares/flash/shake, captions, high-contrast, colorblind, tunable scare intensity).

## Key features at a glance
- Asymmetric hunt with capture counterplay (flashlight stagger, timed dodges, door slams)
- Survival meters: flashlight battery, stamina, fear, dread, anti-camp pressure, detention marks
- Movement tech: sprint, roll, slide, dive, prone, crawl-spaces
- A horror **atmosphere director** with a per-player scare budget, jumpscare variants, whispers, and classroom "cold-calls"
- **CCTV** surveillance with clear counterplay (break line of sight, go prone, blind the circuit)
- Train **shop economy** + powerups; snack/drink/minigame stations
- **Bot AI** that fills any role and answers questions (8 personalities, 3 difficulties)
- Three maps (Facility, Substation, Foggrounds), runtime-generated with an opt-in hand-authored pipeline

## Tech snapshot
- **Engine:** Unreal Engine 5.7, native C++ gameplay + Slate UI
- **Multiplayer:** direct-IP listen server (UDP 7777), `BH1:` join codes, Playit tunnel, EOS/Steam profiles
- **Accounts:** local/guest by default (classroom-safe), optional Google/Microsoft OAuth + Node backend
- **Platforms:** Windows (primary, D3D11), Linux (parity), macOS (groundwork)
- **Version:** 0.6.0-beta.1 · current work on branch `feature/visual-questions`

## Want the full picture?
The complete, every-system reference is in **[Docs/BLACKOUT_HUNT_COMPLETE_OVERVIEW.md](BLACKOUT_HUNT_COMPLETE_OVERVIEW.md)** — 33 sections covering every mechanic, system, config knob, tool, and the build pipeline.
