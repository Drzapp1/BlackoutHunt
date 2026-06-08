# Easter-egg HUB — wiring handoff

The hidden easter-egg HUB (press **Escape at the main menu**) is implemented except for two small edits to
files that were being **continuously edited by a parallel session** while I worked, so I could not land them
without clobbering that work. Everything else is in place and builds. This note has the exact remaining edits.

## What is already done (committed / in the tree)

- `BHTypes.h` — `EBHTutorialPhase::EasterEgg` enumerator.
- `BHGameModeEasterEgg.cpp` (new file) — `ABHGameMode::BuildEasterEggRoom()`: a clear central gallery (kept open)
  ringed by themed sector zones, with N/S doorways to **Movement range** and **Teacher evasion** skill rooms, an
  east doorway to a **secret room** that lists the hidden eggs, and glowing `UTextRenderComponent` plaques —
  including a **Trophy Hall** plaque that prints the player's lifetime record (rounds, wins, escapes, best streak,
  mastery %, achievements, XP) read from `UBHAccountSubsystem::GetProgress()`/`GetAchievementCounts()`. Player
  spawns dead centre; the Teacher spawn is parked in the evasion room.
- `BHGameMode.h` — `BuildEasterEggRoom()` declaration.
- `BHGameMode.cpp` — `?BHTutorialPhase=EasterEgg` URL parse case, and `BuildTutorialLevel()` early-outs to
  `BuildEasterEggRoom()` when `RuntimeTutorialPhase == EBHTutorialPhase::EasterEgg`.
- `BHPlayerController.cpp` — `EasterEgg` cases added to both `HostTutorialPhase` (`PhaseName="EasterEgg"`) and
  `HostTutorialPhaseForMenu` (`Label="Easter Egg"`).

## Remaining edit 1 — menu Escape hook (`SBHMainMenu.cpp`, `OnKeyDown`)

In the front-end Escape branch (currently sets the status text "Host or join a direct-IP game to start."), launch
the HUB instead:

```cpp
if (InKeyEvent.GetKey() == EKeys::Escape)
{
    if (IsInNetworkedGame())
    {
        return OnResumeClicked();
    }
    // Easter egg: Escape at the front-end menu drops you into the hidden HUB.
    if (ABHPlayerController* EggPC = PlayerController.Get())
    {
        FString EggMessage;
        EggPC->HostTutorialPhaseForMenu(EBHTutorialPhase::EasterEgg, /*bChain*/ false, EggMessage);
        return FReply::Handled();
    }
    StatusText = FText::FromString(TEXT("Host or join a direct-IP game to start."));
    return FReply::Handled();
}
```

## Remaining edit 2 — director EasterEgg case (`BHTutorialDirector.cpp`, `Activate`)

`Activate`'s phase switch has no `EasterEgg` case, so it currently falls through to the Survivor `default` and would
run the Survivor *lesson* inside the HUB. Add a case that just makes the player playable and shows a welcome (no
steps), e.g.:

```cpp
case EBHTutorialPhase::EasterEgg:
    EnsurePlayablyConfigured();          // make the local player a free-roaming survivor (no lesson)
    ShowTutorialCard(TEXT("THE BACK ROOMS"),
        TEXT("You found the hidden wing. Wander the sectors, check your Trophy Hall, train, or find the secret egg map."), 6.0f);
    return;                              // do NOT EnterStep(...) — there is no scripted lesson here
```

(If `EnsurePlayablyConfigured()` is private/named differently, mirror whatever the Movement phase uses to spawn and
unlock the lone player; the key point is: **do not start the Survivor step machine** for this phase.)

## Optional polish (nice-to-have)

- Spawn a slow practice Teacher bot in the evasion room (reuse `SpawnScriptedBot` / `DriveBotChase`).
- Drop a few `ABHNoiseDecoy`/cover blocks in the Movement range so the roll/slide/dive have targets.
- A "press a key to return to the menu" beat (or reuse the standalone return path).
