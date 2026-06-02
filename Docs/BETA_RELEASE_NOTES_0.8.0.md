# Blackout Hunt 0.8.0 — holding up with a full class

The big thing 0.7.2 fixed was getting the modes to actually start. This release is about what happens *after* that, once there's a real class in the session: ten students, every level, everyone joining over the playit tunnel. I did a stability pass on exactly that scenario and fixed the things that fell over.

## Playing with ten

The multiplayer correctness fixes — the kind you only trip over with a full room:

- The normal exit no longer ends the round for everyone the moment the first survivor gets out. It resolves once every survivor who's still alive has either escaped or been caught, so getting out is something the whole group does instead of a race that strands your friends.
- There's always at least one hunter now. A round that somehow had none used to just sit there burning down the clock; now it resolves as a survivor win.
- Your role and score follow *you* across level changes, even when two students picked the same name. That's keyed on a reconnect token the server hands out rather than the display name, so duplicate names can't swap roles or scores anymore.
- Answering is tracked per player. Before, two people answering in the same instant could collide and one vote would quietly disappear.
- Spectators and reconnecting caught players don't drop through the floor anymore — the broken spectator camera is fixed.

## Scale and the network

The things that only show up under load with ten clients:

- The train intermission's procedural blocks are split up so they can't blow past the engine's per-message size limit and drop people mid-transition.
- I trimmed what gets sent to every client. Fear/stamina-style meters only go to their own owner now, and the CCTV monitor only does its expensive per-frame capture when someone is actually stood near it.
- Connect timeouts are longer so a student on a slow tunnel link isn't kicked during the join rush at the start of a round, and the player cap you set actually sticks across a level change instead of silently snapping back to 16.

## The blackout actually bites now

When the power's out your flashlight is meant to feel unreliable, so I gave the blackout some teeth. During one the light drains a lot faster, dims down, and keeps weakening over a few seconds, all on a tunable radius. If it's too harsh or too soft for your group it's all in config.

## Hosting over the tunnel

The most important host setup step is now front and centre in the guide: your playit.gg account needs a Custom UDP tunnel forwarding `blackouthunt.playit.plus:24761` to your local `127.0.0.1:7777`. The game already warns you when that mapping is missing, but it's an easy thing to miss the first time, so it's the headline of the host instructions now. And don't run two playit agents at once — that bites too.

## A note on the licence

I tightened the LICENSE while I was in here. It's still source-available — look at it, learn from it, just don't ship it as your own — but "commercial use" is actually defined now, and there's a path for a separate commercial licence if anyone ever wants one. Every source file picked up a copyright header and there's a COPYRIGHT file at the root.

## Getting in (Windows)

Unzip it, run `BlackoutHunt.exe`, and make sure everyone in a session is on 0.8.0. For a hosted class, set up the Custom UDP tunnel above first. This build went through the automated playability gate before I tagged it — a real host plus two clients booted the cooked package and played through to a started round.

## Known limits

- The live tunnel still needs that Custom UDP mapping set in your playit dashboard by hand. The game detects it and walks you through it, but I can't do that part for you.
- This is a beta. It's been through the automation suite (64 tests) and the cooked host-plus-clients smoke run, but not yet a proper in-engine playtest with real people — so treat it as one, and tell me what breaks :)
