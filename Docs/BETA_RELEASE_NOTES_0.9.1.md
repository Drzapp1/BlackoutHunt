# Blackout Hunt 0.9.1 (beta) — prerelease

A big "between-rounds" update: the subway intermission is now a fully interactive 15-car social lobby, the boot/menu got a horror-flavored overhaul, and movement, maps, tutorial, and anti-cheat all received fixes and new skill tech.

## Train Lobby — the headline feature
- The subway intermission is now a walkable 15-car lobby; new controls: C sit, X emote, E interact, O return to cabin.
- Play full minigames vs AI or other players: 6x6 minichess, Connect Four, Tic-Tac-Toe, Othello/Reversi, Blackjack, Slots, and Darts — all server-authoritative with per-player scoring.
- Sit on benches/booths/reading chairs (your avatar visibly sits for everyone), and use look-at + tap/hold to interact with anything.
- Comfort props: pettable wandering cats, aquariums with startle-able fish, a lightable fireplace, a disco dance floor with cycling patterns, and free-snack vending machines.
- A jukebox in the karaoke car cycles spatialized music loops (new lofi/chill tracks added), plus hand-straps, a stop-cord, and themed mood lighting per car.
- Hidden lobby treats: a rare ghost passenger near the observation deck and a secret greenhouse switch that throws a party for the whole car.

## Menu & Boot
- New boot sequence: intro sound effects (hum, POST beeps, impact/static) and a "broken screen" BSOD corruption effect replacing the old screen wobble.
- The Controls tab now lets you rebind the Attack/Capture button to any key or mouse button (with a Reset to Left Mouse), saved across sessions.
- Expanded in-menu Guide and Controls: new key reference for N (node marker), Tab (station cursor), M/I (map), flashlight-stagger and locker roll-out callouts, and rewritten role/expert strategy tips.
- Version strings updated to 0.9.1 throughout the boot console and start screen.

## Movement
- New CS2/Source-style air-strafing: build speed above sprint by syncing mouse + strafe in the air, with a hard ceiling so it stays rewarding, not broken (on by default; fully predicted and server-authoritative). All roles get it.
- Drop-roll now reliably fires on landing (hold sprint+crouch, or buffer it), including for networked clients.
- Locker roll-out: hold Sprint and tap E to exit a locker as a capture-immune roll instead of standing exposed.
- New O "reset to spawn" unstick if you get wedged in the map — cooldown-gated and blocked while a teacher is chasing you, so it can't be abused as an escape.
- Reworked emote wheel: hold X, the cursor snaps to center, aim a wedge with the mouse, release to send.

## Maps
- Substation reflow: an open east-west spine with looping arches, a fixed "Power" objective marker that now points at a fair nearby breaker, fewer/clearer doors, and crawl-spaces relocated into real flanking routes (verified 34/34 reachable).
- Facility (Backrooms): crawl-tunnel network expanded 16 to 26, with longer tunnels that give prone survivors a genuine shortcut/escape advantage; crawl-immunity volume edge gap fixed.

## Tutorial & Classroom
- Tutorial fixes: flow-chain steps now register (including for remote clients), hints reach bot Teachers, the decoy demo plays, students stay safe until scripted, and Monitor tools visibly react.
- Captions are bigger and centered with shorter prompts; added a start chime, locker-exit variety, and a roll-camera comfort tip.
- Cleared a bug where tutorial capture-immunity could leak into live play.

## Balance, Achievements & Anti-Cheat
- Teacher stamina recovery nerfed (1.75x to 0.8x) for fairer chase pacing.
- Monitors must now finish revision: when all survivors are caught, a grace window keeps the round going (with a point dock for monitors who don't hit their contribution target) instead of ending instantly.
- New achievements and easter eggs: rooftop stargazing/parkour hangout (Roof Runner), "Stuck in a Tree" (press O to climb down), "Roll Call" for the real teacher signing in, plus a long tail of round/topic/secret achievements; procedural headwear removed in favor of nameplate titles & emblems.
- New opt-in, server-side anti-cheat backstop (`bh.AntiCheat`, off by default, log-only when on) watching for speed/teleport/fly anomalies; never policies the host and won't flag legit movement tech.
- Dev "unlock everything" shortcut is now compiled out of production builds and locked to the developer account behind a one-time password.
