# Classroom over a playit.gg tunnel — host & student guide

This is the verified path for running a full 10-student class when everyone is **not** on the same LAN
(e.g. school Wi‑Fi that blocks direct connections). The teacher's PC hosts; students connect over a
**playit.gg UDP tunnel**. EOS still provides accounts/identity, but the game traffic itself is plain UDP,
which is what the tunnel forwards.

## How it fits together

```
 student PC  ──UDP──▶  playit relay  ──UDP──▶  teacher PC: playit agent  ──▶  127.0.0.1:7777 (the game)
 (types blackouthunt.playit.plus:24761)                      (must forward to 127.0.0.1:7777)
```

The game hosts a listen server bound to `127.0.0.1:7777` (loopback‑only is the shipped default and is
**fine**, because the playit agent runs on the same PC and forwards to loopback). Students never need the
teacher's real IP — they only ever use the public tunnel address.

## Host (teacher) checklist — do this before class

1. **Start the playit agent** on the teacher PC and confirm its tunnel is:
   - **Protocol: UDP** (not TCP — UE game traffic is UDP).
   - **Local/forward target: `127.0.0.1:7777`** (NOT the LAN IP, NOT `0.0.0.0`). This is the single most
     common misconfiguration; loopback‑only hosting requires the agent to forward to `127.0.0.1`.
   - Public address is the one baked into the build: **`blackouthunt.playit.plus:24761`**
     (`Config/DefaultGame.ini` → `ClassroomJoinEndpoints`). If your playit account hands out a different
     host/port, update that endpoint (or just tell students the new `host:port`).
2. **Port 7777/UDP must be free** on the host (nothing else bound to it).
3. In the game, host with **Host Live Classroom** (or the Host* modes). The server binds `127.0.0.1:7777`.
4. Leave `bUseAuthoredLevels=True` (the shipped default) — it keeps the large level geometry off the
   per‑bunch replication path so every student finishes loading.

## Student steps — what to type

In the main menu → **JOIN**:
- Click the **saved classroom endpoint** if it's shown (one click, no typing), **or**
- Type the address exactly: **`blackouthunt.playit.plus:24761`**

Accepted variants (all normalize correctly): bare `host:port`, a copied `BH1:` invite code, a
`blackouthunt://join/…` link, and a pasted `udp://…` / `http(s)://…` prefix (the scheme is stripped).
**Do not** add a path or `?query` after the port. The high port (`:24761`) is required — without it the
client defaults to `:7777` and the tunnel join fails.

## If a student drops mid‑class

Just reopen **JOIN** and reconnect to the **same** address. Within the 120‑second grace window the server
restores their exact role, score, and inventory (keyed off an unguessable per‑client token, so two
students sharing a display name never get swapped). After the grace window they rejoin as a spectator
until the next level/lobby.

## Robustness notes (why this should "just work" for 10)

- **Connection timeouts** are lengthened (`InitialConnectTimeout=60`, `ConnectionTimeout=45`,
  `Config/DefaultEngine.ini`) so a slower student behind tunnel latency is not dropped during the heavy
  initial replication burst when up to ~10 students join at once.
- **Client rate caps** are raised to 50 KB/s (well above engine defaults) and the server tick is capped at
  30 Hz, so the host isn't saturated.
- **Session cap** travels with every level via `?MaxPlayers=`, so students aren't bounced after the first
  level transition.
- The engine session cap floor is 16, and the configured class size is 32 — a class of 10 has wide margin.

## Quick failure triage

| Symptom | Most likely cause |
|---|---|
| Every student fails to connect | playit agent down, or tunnel forwarding to the wrong local target (must be `127.0.0.1:7777`, UDP) |
| One student can't connect, others can | that student typed the wrong address / dropped the `:24761` port |
| Students connect then drop during load | (mitigated) tunnel latency + join burst — the new timeouts cover this; check host CPU isn't pegged |
| Students join but see empty rooms between levels | should not occur (block‑field is sharded + authored maps on); verify `bUseAuthoredLevels=True` |
