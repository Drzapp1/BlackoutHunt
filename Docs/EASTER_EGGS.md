# Blackout Hunt — Easter Eggs

A small set of **hidden, cosmetic-only** treats for players to stumble on. This file is the owner/teacher
reference (the "secret menu") so nothing here ever surprises you mid-lesson.

## Ground rules (why these are safe in a classroom)

Every easter egg here is **strictly cosmetic and client-local or rare-atmospheric**. None of them:

- touch scoring, mastery, the spaced-repetition queue, classroom reports, or XP;
- change capture/escape/win-loss, role assignment, or any server-authoritative state;
- give any player a competitive advantage;
- affect stability (no new crash surface — they're guarded reads + draws).

They are **on by default**. To turn them all off for a strictly plain session, set the console variable:

```
bh.EasterEggs 0
```

(`1` = on, the default.) The single toggle is read by `BHAreEasterEggsEnabled()` (declared in
`Source/BlackoutHunt/BHTypes.h`, defined in `BHGameMode.cpp`). Each egg is also individually `git revert`-able —
they live on the `feature/easter-eggs-2026-06-06` branch, separate from the bug-fix work.

## The eggs

### 1. Scratched into the locker
**How to find it:** hide in a locker as a survivor.
While you're concealed, a faint line of "graffiti" a previous student left in the wood appears low on screen —
physics-flavoured, a little spooky, always wholesome (e.g. *"g = 9.81. it still pulls, even down here."*,
*"sound needs a medium. screams travel anyway."*, *"the answer is 42. find the question."*). The message is
chosen from the hiding spot, so **each locker keeps its own message** — like real graffiti that's always there.
Only you can see it, only while hidden. *(`BHHUD.cpp`, gated on `IsHiddenInLocker()`.)*

### 2. The faculty remembers
**How to find it:** set your player name to a famous physicist (case-insensitive) — `Newton`, `Einstein`,
`Curie`, `Tesla`, `Feynman`, `Bohr`, `Schrodinger`, `Hawking`, `Galileo`, `Faraday`, `Maxwell`, or `Planck`.
The first time you spawn, you get a one-time, **client-local** welcome whisper just for you — e.g. Newton →
*"Somewhere, an apple falls in your honour."*; Bohr → *"Classically, your exact position is uncertain."*;
Schrodinger → *"You are both hidden and found until someone opens the locker."* Purely a status line; no flair,
no advantage. *(`BHHUD.cpp`, one-time per session.)*

### 3. A whisper in the static
**How to find it:** play calmly. The shared "presence" readout (the building's mood line) **very rarely**
(~2.5% of idle updates) shows a hidden line instead of the usual idle text — *"the building hums at 50 Hz, like
the lights."*, *"something in the walls is solving for x."*, *"for a moment, the map shows a room that isn't
there."* It only ever fires at **calm presence**, so it can never replace a real danger warning — it's a
"wait… did I read that right?" flicker. *(`BHGameMode.cpp`, `UpdatePresenceDirector`.)*

### 4. The Konami code
**How to find it:** on the main menu, press **↑ ↑ ↓ ↓ ← → ← → B A**.
You get a hidden thank-you line in the menu status — a quiet nod to anyone who tried it. It is **not** a cheat:
it unlocks nothing and changes no settings. Normal menu navigation still works while you enter it. *(`SBHMainMenu.cpp`,
`OnKeyDown`.)*

### 5. The train roof
**How to find it:** during a **subway intermission** (the between-stages ride), go exploring in the **front cab
corner** — tucked against the wall is a small, dark **maintenance hatch**. Look at it and interact, and you're
lifted **out onto the roof of the train**: a low-railed walkway above the carriages with the tunnel rushing past.
It's a quiet vantage to enjoy while the timer runs (the rails are there so you don't wander off into the trench).
First time up earns the **Roof Rider** achievement (→ the **Top Hat** headwear). Cosmetic only: the intermission
advances on its **own timer**, so a roof visit never blocks boarding, changes scoring, or affects the round.
*(`BHTrainRoofHatch.cpp`, spawned in `BuildTrainIntermissionLevel`; gated by `bh.EasterEggs`.)*

## Achievements — the Awards tab, tints, hats & nameplate flair

Many rewards are gated behind **achievements** rather than raw XP. Earning one unlocks a cosmetic you can then
equip in the menu (a hidden avatar **tint**, a procedural **hat**, or nameplate **flair** — see below). All of
it is **cosmetic and persisted locally** (account progress, schema v2). Achievements also award a little XP, so
they feed the same unlock economy as play. None affect scoring, fairness, or stability.

There's a dedicated **Awards** tab in the main menu (next to *Character*) that lists every achievement as a
badge: a **5-pip difficulty meter** colour-coded by tier (Bronze → Silver → Gold → Platinum → Mythic), the
description, the cosmetic it unlocks, and an EARNED / locked state. Hidden ("secret") achievements show as
**???** until you find them. The header tracks **Earned N of M**.

| Achievement | Difficulty | How to earn it | Reward |
| --- | --- | --- | --- |
| **Spelunker** | ★ | Hide in a locker. | (XP only) |
| **Honorary Faculty** | ★ (secret) | Play under a famous physicist's name (egg #2). | **Chalk** tint |
| **First Blood** | ★★ | Capture your first survivor as the Teacher. | **Detention** tint |
| **Tourist** | ★★ | Try all four train-intermission activities. | **Commuter** tint |
| **Codebreaker** | ★★ (secret) | Enter the Konami code (egg #4). | **Arcade** tint |
| **Escape Artist** | ★★ | Reach the exit and get out of a round. | **Exit Sign** tint |
| **Veteran** | ★★ | Play 25 rounds. | **Veteran** tint |
| **Last One Standing** | ★★★ | Win a Hunt as a survivor. | **Suit** outfit |
| **Top of the Class** | ★★★ | Win a Hunt as the Teacher. | **Faculty** tint |
| **Roof Rider** | ★★★ (secret) | Find the hatch onto the train roof (egg #5). | **Top Hat** |
| **On a Roll** | ★★★ | Win three rounds in a row. | **Crown** |
| **Graduate** | ★★★ | Play 50 rounds. | **Graduation Cap** |
| **Flawless Hunt** | ★★★★ | Catch every survivor in one round (nobody escapes). | **Apex** tint |
| **Flow Master** | ★★★★★ | Land a full three-link flow chain. | **Slipstream** tint |
| **Perfect Chain** | ★★★★★ | Nail the momentum tech below (frame-perfect). | **Afterimage** tint + **Spacesuit** outfit |
| **Completionist** | ★★★★★ (secret) | Earn every hidden easter-egg award. | **Halo** |
| **Honor Roll** | ★★★ | Answer five questions correctly in a row. | **Honor Roll** title |
| **Polymath** | ★★★ | Answer correctly in all four physics topics. | **Polymath** title |

The ten hidden **tints** (Chalk / Arcade / Exit Sign / Afterimage / Veteran / Faculty / Slipstream / Detention /
Apex / Commuter) appear as locked swatches in the **Shirt** colour picker until earned, then become selectable
like any colour and persist. Their exact colour shows on **nameplates, the lobby roster, and map blips**; on the
8-material Quaternius body mesh they map to the nearest base material (a tint needs its own body material to render
exactly on the 3D model — easy to add later with the Quaternius art). The **Headwear** picker also has four
procedural achievement hats — **Top Hat** (Roof Rider), **Crown** (On a Roll), **Halo** (Completionist), and
**Graduation Cap** (Graduate) — each built from primitives and anchored to the head. Two **prestige outfits** are
achievement-locked rather than XP-gated: **Suit** (Last One Standing) and **Spacesuit** (Perfect Chain).
Achievements live in a small registry in `BHAccountSubsystem.cpp`; `UnlockAchievement()` is idempotent and toasts
on first earn.

### Nameplate flair: titles & emblems

Two reward types live on your **nameplate**, so *other* players see them (unlike the avatar-only tints):

- A **Title** shown under your name (e.g. *Honors Student*, *The Untouchable*, *Speedrunner*, *Graduate*,
  *Completionist*) — each gated on an achievement.
- An **Emblem** — a small coloured badge beside your name (Chalk Star, Exit Sign, Crown, Halo, Ember).

Equip the ones you've earned in the **Character** tab (the *Title* / *Emblem* rows under the cosmetics). They
replicate via the PlayerState and are drawn in `DrawNearbyNameTags` (survivors only — Teachers / hall monitors
stay anonymous). Cosmetic only; nothing about them touches play.

## The momentum tech (speedrun "flow chain")

**How to do it:** as a survivor, chain a special move (slide / dive / roll) into the next one
**frame-perfectly** — input the follow-up within ~0.12 s of the previous move ending. Nail it and you
**bypass the move cooldown once** and keep your momentum (a small speed scale), so a skilled player can *flow*
slide → dive → slide instead of stopping. It's meant to be **hard**: miss the tiny window and you just get the
normal cooldown message (no penalty). You can chain up to **3** links before a real cooldown resets it.

- **Fair by design:** available to **every** survivor — the only gate is skill (the window), not an unlock.
  **Survivor-side only** (a Hunter never gets it, so it can't speed up captures), the speed bonus is **modest**
  (~+15 % on the chained move), and the chain is **capped**.
- First clean chain unlocks the **Perfect Chain** achievement (→ the **Afterimage** tint) + a brief "Perfect
  chain!" cue.
- **Toggle:** `bh.MomentumTech 1` (default on) / `0` (off). *(`BHCharacter.cpp`, `TryStartSpecialMoveAuthority`.)*
- **Tuning (cvars):** `bh.MomentumChainWindow` (0.12 s input window — lower = harder), `bh.MomentumChainMaxLinks`
  (3 — chain cap), `bh.MomentumChainSpeedScale` (1.15 — chained-move speed). Defaults match the original tuning;
  adjust them live to balance the feature without a recompile.
- ⚠️ **This is the one fun feature that touches movement**, so it genuinely wants **balance playtesting** before
  you rely on it in a competitive class. The tight window + cap + modest bonus keep its impact small, and the
  cvar turns it off instantly if it ever feels off.

## Notes for the owner

- Toggles: `bh.EasterEggs` (the four cosmetic eggs) and `bh.MomentumTech` (the movement tech) are independent.
  Achievements aren't separately toggle-gated, but the three tied to easter eggs (Honorary Faculty, Codebreaker,
  Roof Rider) can only be earned while `bh.EasterEggs` is on, since their trigger *is* the egg.
- The physicist list and the locker/whisper messages are plain string tables in the source — easy to add to,
  reword, or trim to taste.
- If you ever want an egg that *does* affect play (a cosmetic unlock, a hidden avatar tint, a secret room in an
  authored map), keep it behind `bh.EasterEggs` and out of the scored path, and add it here so it's documented.
- Tone target: wholesome, physics-nerdy, gently spooky — fits the classroom-horror theme without breaking
  tension (the scares stay scary; these are the *quiet* secrets).
