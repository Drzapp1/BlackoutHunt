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
`Curie`, `Tesla`, `Feynman`, `Bohr`, `Schrodinger`, `Hawking`, `Galileo`, `Faraday`, `Maxwell`, `Planck`,
`Lovelace`, `Noether`, `Hertz`, `Joule`, `Ohm`, `Volta`, `Ampere`, `Kelvin`, `Rutherford`, `Heisenberg`,
`Pascal`, `Doppler` — or even a cheeky `42` / `Adams` / `Pi`.
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
First time up earns the **Roof Rider** achievement (→ the **Roof Rider** nameplate title). Cosmetic only: the intermission
advances on its **own timer**, so a roof visit never blocks boarding, changes scoring, or affects the round.
*(`BHTrainRoofHatch.cpp`, spawned in `BuildTrainIntermissionLevel`; gated by `bh.EasterEggs`.)*

**The roof-light breaker (per-player).** It's dark up on the roof. Tucked into the far back corner of the roof
walkway is a small, grimy **electrical breaker** that's easy to miss — but pressing the **N** "node marker" key
points you straight to it (it registers as a trackable node, the same indicator the question stations use).
Throw it and a row of cool "service lights" fades on over the roof. The lighting is **per player and purely
local**: each passenger lights the roof for *themselves* (client-local lights, no shared world state), so one
person flipping the breaker never changes anyone else's view, and throwing it again turns their lights back off.
Cosmetic only. *(`BHTrainRoofBreaker.cpp` → `ABHCharacter::ClientToggleRoofServiceLights` spawns client-local
`ABHTrainServiceLight` actors; spawned in `BuildTrainIntermissionLevel`, gated by `bh.EasterEggs`.)*

**Stargazing, decorations & rooftop parkour.** The roof is now a proper hang-out. Overhead is a **starfield**
(plus a warm **festoon-light** string along the edge) for stargazing — built deterministically so every player
sees the same sky (`ABHRoofStarfield`). Around it sit calm **decorations** — a little telescope, deck seating,
antenna masts, a utility box, and roof vents (throw the breaker to light them up). And a short **parkour course**
of five ascending platforms ends in a glowing finish gate (`ABHTrainRoofParkourGate`): clear the climb and
interact with it to earn the **Roof Runner** achievement (and its XP). Reaching the top platform requires the
climb, so the finish can't be cheesed. If you ever get stuck outside, a roof **down-hatch** (press E) drops you
back inside, and **O** ("reset to train") always returns you to the carriage. All cosmetic; gated by
`bh.EasterEggs`. *(`BuildTrainRoofExtras` in `BHGameMode.cpp`, shared by the intermission and lobby roofs.)*

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
| **Roof Rider** | ★★★ (secret) | Find the hatch onto the train roof (egg #5). | **Roof Rider** title |
| **Roof Runner** | ★★★ (secret) | Clear the rooftop parkour course (egg #5). | (XP only) |
| **On a Roll** | ★★★ | Win three rounds in a row. | **Crown** emblem |
| **Graduate** | ★★★ | Play 50 rounds. | **Graduate** title |
| **Flawless Hunt** | ★★★★ | Catch every survivor in one round (nobody escapes). | **Apex** tint |
| **Flow Master** | ★★★★★ | Land a full three-link flow chain. | **Slipstream** tint |
| **Perfect Chain** | ★★★★★ | Nail the momentum tech below (frame-perfect). | **Afterimage** tint + **Spacesuit** outfit |
| **Completionist** | ★★★★★ (secret) | Earn every hidden easter-egg award. | **Halo** emblem + **Completionist** title |
| **Honor Roll** | ★★★ | Answer five questions correctly in a row. | **Honor Roll** title |
| **Polymath** | ★★★ | Answer correctly in all four physics topics. | **Polymath** title |

### 2026 expansion — the long tail

A second wave of achievements so regulars always have a next goal, with a heavy share of **secret** ones.
Rewards are nameplate **titles**, nameplate **emblems**, or **re-gated outfits** — no new hats (the avatars already
wear hats; new procedural headwear is intentionally out of scope). Most of the wardrobe now sits behind play goals:
the **Farmer / Beach / Punk** outfits moved off raw XP and onto the *Centurion / Houdini / Unstoppable* achievements
(only **Adventurer** is kept as an early 100-XP carrot).

| Achievement | Difficulty | How to earn it | Reward |
| --- | --- | --- | --- |
| **Centurion** | ★★★★ | Play 100 rounds. | **Farmer** outfit |
| **Houdini** | ★★★ | Escape 10 times. | **Beach** outfit |
| **Unstoppable** | ★★★★ | Win five rounds in a row. | **Punk** outfit |
| **Survivalist** | ★★★★ | Win 10 Hunts as a survivor. | *Survivalist* title |
| **Headmaster** | ★★★★ | Win 10 Hunts as the Teacher. | *Headmaster* title |
| **Honor Society** | ★★ | Reach 1,000 XP. | *Honor Society* title |
| **Dean's List** | ★★★★ | Reach 5,000 XP. | *Dean's List* title |
| **Truant Officer** | ★★★★ | Capture 50 survivors (lifetime). | **Hall Pass** emblem |
| **Valedictorian** | ★★★★ | Answer ten questions correctly in a row. | *Valedictorian* title |
| **Bookworm** | ★★★ | Answer 100 questions correctly (lifetime). | *Bookworm* title |
| **Subject Expert** | ★★★ | Answer 25 correctly in a single topic. | **Atom** emblem |
| **Momentum Maestro** | ★★★★★ (secret) | Land ten perfect momentum chains. | *Maestro* title |
| **Pop Quiz** | ★★★★ (secret) | Capture a survivor in the first 20 seconds. | **Pop Quiz** emblem |
| **Ghost in the Walls** | ★★★ (secret) | Hide in a locker 25 times. | *Ghost* title |
| **Saved by the Bell** | ★★★★ (secret) | Board the evacuation train in the final escape. | *Saved by the Bell* title |
| **Comeback Kid** | ★★★★ (secret) | Win as the last survivor while others were caught. | **Phoenix** emblem |
| **Lights On** | ★★ (secret) | Throw the breaker and light the train roof. | (XP only) |
| **Did You See That?** | ★★★ (secret) | Witness a "whisper in the static" (egg #3). | **Static** emblem |
| **Burning the Midnight Oil** | ★★ (secret) | Finish a round after midnight (local clock). | *Night Owl* title |
| **Don't Panic** | ★★ (secret) | Play named `42` / `Adams`. | *Hitchhiker* title |
| **It Still Pulls** | ★★★ (secret) | On the menu, enter the code **G 9 8 1** (`g = 9.81`). | **Falling Apple** emblem |
| **Wall Reader** | ★★ (secret) | Read a piece of locker graffiti (egg #1). | (XP only) |
| **Honor Graduate** | ★★★★★ | Earn every *standard* (non-secret) achievement. | *Honor Graduate* title |
| **Perfectionist** | ★★★★★ (secret) | Earn every achievement, secrets and all. | *Perfectionist* title |

These ride the same plumbing as the originals: countable milestones live in `RecordRoundResult` /
`RecordQuestionResult` / the new `RecordCapture` / `RecordLockerHide` / `RecordPerfectChain` (account schema **v3**
added the lifetime-capture, locker-hide, and perfect-chain counters); event/secret ones grant through the existing
`ClientGrantAchievement` RPC or a client-local `UnlockAchievement`; and the two capstones roll up automatically in
`UnlockAchievement` by scanning the registry, so they stay correct as more achievements are added.

The ten hidden **tints** (Chalk / Arcade / Exit Sign / Afterimage / Veteran / Faculty / Slipstream / Detention /
Apex / Commuter) appear as locked swatches in the **Shirt** colour picker until earned, then become selectable
like any colour and persist. Their exact colour shows on **nameplates, the lobby roster, and map blips**; on the
8-material Quaternius body mesh they map to the nearest base material (a tint needs its own body material to render
exactly on the 3D model — easy to add later with the Quaternius art). **Headwear has been removed** (2026): the
procedural achievement hats and XP hats stacked on the Quaternius skins' baked-in headwear and read wrong, so the
**Headwear** category is now empty (the picker is gone). The four achievements that used to grant hats now reward
nameplate flair instead — **Roof Rider** → *Roof Rider* title, **On a Roll** → **Crown** emblem, **Completionist** →
**Halo** emblem + *Completionist* title, **Graduate** → *Graduate* title. Two **prestige outfits** are
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
