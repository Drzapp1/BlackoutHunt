# Blackout Hunt 0.7.2 Release Notes — Modes actually start now

## Headline

`0.7.2` fixes the showstopper that survived `0.7.1`: clicking **any** mode in a packaged
build bounced you straight back to the menu and never let you play. There were two separate
faults stacked on top of each other, both only happening in a cooked build (never in the
editor), which is why they slipped through. Both are fixed, and there's now an automated
playability gate so this class of bug can't ship again.

## What was actually wrong

Two cooked-build-only faults, one hiding behind the other:

1. **The authored maps crashed on load.** Props placed in a cooked map are built on the
   background loading thread. A few of them tinted their materials *while constructing*, and
   touching a dynamic material there trips a "must be on the game thread" check — so opening
   any authored map crashed the load instantly. (The editor never hit this because it loads
   on the game thread; `0.7.0` never hit it because the maps weren't in the package at all,
   so it quietly fell back to the runtime generator. Putting the maps in the package in
   `0.7.1` is what exposed the crash.)

2. **The host couldn't open a server.** Once the crash was fixed and the map loaded, the
   game still couldn't start listening: the packaged build used the EOS peer-to-peer net
   driver for hosting, which can't bind a plain listen/tunnel server, so it failed with
   "Could not bind local address" and dropped back to the menu. Hosting now uses standard
   IP networking, which is exactly what LAN and the Playit classroom tunnel
   (`blackouthunt.playit.plus:24761`) need. EOS still powers accounts and lobby discovery;
   peer-to-peer relay hosting can come back later as a per-mode option.

## What changed

- Material tinting on placed props is deferred to the game thread, so authored maps load
  without crashing.
- The packaged build hosts over IP (LAN / direct / Playit tunnel) instead of EOS P2P, so
  Host, Live Classroom, Test, Bot, and the Tutorial all reach gameplay.
- **New playability gate:** every EOS build now boots the cooked package and drives a real
  host + two clients all the way to a started round before it's allowed to pass. The build
  that produced this release went through that gate (host `HOST_LISTENING → ROUND_STARTED`,
  both clients `JOINED → ROUND_STARTED`).
- The `0.7.1` cooked-map presence check is still in place.

## Upgrading

Replace your `0.7.1` (or `0.7.0`) folder with this one. Everyone in a session should be on
`0.7.2`. No content or gameplay changes from `0.7.0` — this is purely the fix that makes the
modes start.
