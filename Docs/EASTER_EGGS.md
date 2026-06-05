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

## Notes for the owner

- The physicist list and the locker/whisper messages are plain string tables in the source — easy to add to,
  reword, or trim to taste.
- If you ever want an egg that *does* affect play (a cosmetic unlock, a hidden avatar tint, a secret room in an
  authored map), keep it behind `bh.EasterEggs` and out of the scored path, and add it here so it's documented.
- Tone target: wholesome, physics-nerdy, gently spooky — fits the classroom-horror theme without breaking
  tension (the scares stay scary; these are the *quiet* secrets).
