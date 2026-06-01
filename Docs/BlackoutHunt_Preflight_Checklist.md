# Blackout Hunt — Live Classroom Pre-Flight Checklist

Target: **0.6.0-beta.1**, 32-player live session, school PCs, Playit endpoint `blackouthunt.playit.plus:24761`.
Use this as a hunt list. Anything that can fail **silently** is marked 🔇.

---

## 1. Network & Connectivity

- [ ] 🔇 Playit tunnel/agent confirmed online **the morning of** — not just last week. Tunnels die quietly.
- [ ] What does the join screen show if the tunnel is down? Confirm it's not a dead-end "connection lost".
- [ ] School firewall: does outbound to the Playit endpoint actually pass on the student VLAN? Test from a *student* machine, not yours.
- [ ] 🔇 Client-side packet loss / high latency on school Wi-Fi — does movement rubber-band, do answers register late or double-submit?
- [ ] Hotspot fallback path tested end-to-end (not just that the button exists). Does it require admin rights it won't have?
- [ ] Direct-LAN fallback works if Playit fails entirely. Know the host IP + port in advance.
- [ ] `BH1:` invite-code path parses correctly (you have join-address normalization tests — confirm they cover the format students will actually type).
- [ ] 🔇 Reconnect grace (120s): is it actually invisible to the student, and does state restore correctly after a real disconnect (not a simulated one)?
- [ ] DNS resolution of the Playit domain works on locked-down DNS (some school networks force their own resolver).

## 2. Player Count & Scaling (the 32 trap)

- [ ] 🔇 `NumPublicConnections` — the bug already flagged. Confirm the EOS/online session advertised cap matches the 32 gameplay cap, not 12.
- [ ] 🔇 Did netcode actually run at 20+ real clients, or only config-unlocked + 2-client soak? Find the largest count you've genuinely tested.
- [ ] Server tick (`NetServerMaxTickRate 30`) under 32 clients — does the host machine hold 30Hz or melt? Watch host CPU/frametime with a full lobby.
- [ ] Relevancy / replication: with 32 pawns, are you replicating everything to everyone, or culling? 32×32 = a lot of actor updates.
- [ ] Lobby UI at 32 names — does the roster render, scroll, and stay readable? Long/duplicate/emoji lobby names.
- [ ] Ready-up gate at 32 — does one AFK student block the whole class start? Is there a host override path that *isn't* force-start?
- [ ] 🔇 Bandwidth on the host uplink — 31 clients pulling state. School upload is often tiny.

## 3. State Authority / Desync (your recurring bug class)

- [ ] 🔇 Late-joiner restore gate (flagged bug) — confirm late joins during Hunt come in as **spectators only**, never resurrect as live/capturable.
- [ ] 🔇 Hall Monitor tool gate (flagged bug) — freeze the contribution denominator per round; confirm displayed eligibility == enforced eligibility.
- [ ] 🔇 Win-condition counter — does "alive survivors" stay correct across captures, disconnects, reconnects, and late joins simultaneously?
- [ ] 🔇 Capture during a reconnect window — what happens if the Teacher captures someone mid-reconnect?
- [ ] 🔇 Two survivors interacting with the same breaker/station at the same instant — race condition on the server-authoritative check?
- [ ] Travel-progress entry cleared after reconnect (root cause of the late-joiner bug — make sure the fix clears it everywhere, not just the one path).
- [ ] 🔇 Role reassignment on host migration — does anything survive if the host itself drops? (Probably catastrophic; know the answer.)

## 4. Performance on Low-Spec School PCs

- [ ] DX11 default confirmed; `Launch-BlackoutHunt-DX11-Low.cmd` works on the actual oldest lab machine.
- [ ] 🔇 GPU feature level: any machine on Microsoft Basic Display Adapter / no DX11 / RDP will fail **before the menu**. Audit the lab GPUs.
- [ ] Frame rate during a jumpscare + fog + multiple players visible — the worst-case frame, not the menu.
- [ ] Foggrounds moonlight/volumetric pass — heaviest visual scene. Does it tank low-spec?
- [ ] VRAM at lowest preset — older integrated GPUs.
- [ ] Cold-boot launch on a machine that's never run it (first-shader-compile hitch).
- [ ] 🔇 Thermal/throttle on a 60-min session — does FPS degrade over time on weak hardware?

## 5. Round Lifecycle & Game Flow

- [ ] Full match start → hunt → train intermission → escape → results, end to end, with bots filling to 32.
- [ ] 🔇 Train intermission with a player who got captured *during* the prior stage — do they board correctly?
- [ ] Final escape anti-camp logic (radius 520 / grace 4 / penalty 2.5) — fires correctly, doesn't punish legit play.
- [ ] Escape train boarding at the wire — does a survivor boarding on the last second count as a win?
- [ ] 🔇 Stage timer expiry mid-question — does an in-flight answer get counted or dropped?
- [ ] Teacher captures the *last* survivor exactly as another escapes — who wins? Confirm deterministic.
- [ ] Boss/checkpoint round (if active) quorum math with the real player count.

## 6. Education Layer Correctness

- [ ] 🔇 Mastery counts: server-authoritative confirmed clean — but spot-check that class threshold (70) + individual (50) actually gate the exit as intended at 32 players.
- [ ] 🔇 Spaced-repetition re-queue: a wrong answer re-surfaces; a *right* answer on retry clears it. Verify both directions.
- [ ] 🔇 Anti-gaming locks: confirm a student can't farm mastery by spamming/guessing the same terminal.
- [ ] Visual/diagram questions (ray diagrams, graphs) render correctly on **low** graphics + small resolution. Are labels legible at 720p?
- [ ] 🔇 Diagram answer-safety — confirm no diagram question has an ambiguous/duplicate correct answer (you have a test; trust but verify the live bank == 376).
- [ ] Question bank actually loads the intended lesson preset / JSON set, not the default.
- [ ] Caught Hall Monitor still contributes to class mastery (per design) — verify it counts.
- [ ] No offensive/wrong physics content slipped into the bank (a teacher will read these).

## 7. Host / Teacher UX Footguns

- [ ] 🔇 "Leave to Main Menu" / leave-session — flagged: one unconfirmed click ends the class. Add a confirm.
- [ ] Accidental host-only control trigger mid-session — can the teacher recover, or is the round dead?
- [ ] Kick flow — does kicking a disruptive student cleanly free a slot and not corrupt counts?
- [ ] Pause / discussion control (boss checkpoint pause) works and resumes cleanly.
- [ ] Teacher can see who's connected / answered / stuck at a glance — or are they blind?
- [ ] If the teacher's machine alt-tabs / screen-locks / sleeps — does the server survive? (School power settings.)

## 8. Audio

- [ ] 🔇 Missing optional jumpscare/audio assets (SCP-096, Fab packs) — confirm guarded fallbacks fire, no silent crash or muted scare.
- [ ] Audio identity cues (footsteps, breaker hum, CCTV static) play for clients, not just host.
- [ ] Tension-track switch in final 60s works and returns to normal pool after.
- [ ] Captions on by default (accessibility) — actually showing.
- [ ] No copyrighted music accidentally in the cooked build (you have a cook allowlist — confirm it held).

## 9. Recovery & Resilience

- [ ] One student's PC hard-crashes mid-hunt — does it free the slot, update counts, allow rejoin?
- [ ] Student closes the game and reopens — clean rejoin within grace?
- [ ] 🔇 Mass disconnect (Wi-Fi blip hits 10 students at once) — does the server survive the storm?
- [ ] Host can restart a round without restarting the whole server / re-handing-out the join link.

## 10. Packaging & Deployment

- [ ] Fresh extract on a clean standard-user account launches with **no** admin rights / no VC++ redist prompt.
- [ ] SHA-256 of the distributed zip matches the release (`97c39e05…` for Windows 0.6.0).
- [ ] 🔇 Package verifier confirmed no saved accounts/logs/secrets/telemetry/pdbs shipped (privacy + size).
- [ ] The exact build students get == the build you tested. No "works on my dev cook" gap.
- [ ] Distribution method to 30 machines decided & tested (USB? Network share? Pre-staged?). Copy time for 800MB ×30.
- [ ] Antivirus / SmartScreen doesn't quarantine an unsigned exe on school machines — test on a locked-down one.

## 11. Data & Privacy

- [ ] 🔇 Telemetry/heatmap export is host-controlled and anonymous — confirm no student PII written anywhere.
- [ ] Local profiles resettable; no leftover data from your testing in the shipped build.
- [ ] Lobby names are the only identifier — students won't be prompted for real names / emails (external login disabled).

---

## Top 10 "if only one thing breaks live, it's probably this"

1. 🔇 NumPublicConnections caps you at 12 → half the class can't join.
2. 🔇 Playit tunnel down the morning of → no one can join, dead-end error.
3. 🔇 A lab GPU has no DX11 → those machines crash before the menu.
4. 🔇 Late-joiner resurrects live → win condition silently breaks.
5. 🔇 Host hasn't truly tested >2 real clients → tick rate / replication falls over at 32.
6. 🔇 SmartScreen / AV quarantines the exe on locked-down PCs.
7. Teacher misclicks "Leave" → whole class ends.
8. 🔇 Hall Monitor gate desync → tools granted/revoked wrongly on join/leave.
9. 🔇 Mastery threshold never unlocks the exit at 32 → no one can escape, match stalls.
10. 🔇 Host machine sleeps/locks under school power policy → server dies mid-session.

---

*Generated for the 0.6.0-beta.1 classroom run. 🔇 = can fail silently; prioritize these.*
