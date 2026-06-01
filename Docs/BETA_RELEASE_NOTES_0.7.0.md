# Blackout Hunt 0.7.0 Release Notes — Visual Questions, Tutorial, Jumpscares

## Headline

`0.7.0` is a **major** beta. It turns the data-driven physics questions into a
fully **interactive** experience, adds a **three-role guided tutorial** so a brand-new
class can learn the game with no host, overhauls the **jumpscares**, and flips the
**authored maps on by default**. It also ships an **EOS** Windows package with the
Epic lobby browser + P2P relay as the main online path (LAN / Playit tunnel still work
in the same build).

## Scope

This remains a classroom build first. One teacher machine hosts Live Classroom and
students join over LAN or the owned Playit endpoint:

```text
blackouthunt.playit.plus:24761
```

New in `0.7.0`, the **EOS profile** is a real online option: clients sign in with an
Epic account, browse lobbies, and connect through Epic's P2P relay with no router
port-forwarding, while the same build keeps direct-IP / LAN / tunnel as a fallback.

## Visual & interactive questions

- **Click the picture.** For the diagram types whose parts *are* the answer (the EM
  spectrum's seven bands), the band is now a clickable region that maps to the matching
  choice. Answer-safe by construction — the hit zones come from the diagram geometry,
  never from the correct answer.
- **Drag and order.** DragDropMatching and Ordering questions are answered by dragging
  pieces onto slots, parsed and graded server-side. Proper (non-multiple-choice) drag
  questions now mix into both revision and standard Hunt nodes (`bh.`-tunable frequency).
- **Mouse or keys.** A question cursor (free with the cursor toggle) lets you click
  choices, drag pieces, and use an on-screen numeric keypad for Calculation questions;
  the keyboard path is unchanged.
- **Cleaner diagrams.** Labels are bounded so they stay on-panel at any HUD scale, and
  the renderer gained filled-triangle / arrow primitives for the new diagram art.

## Guided tutorial

- A **solo, server-driven tutorial** baked into a dedicated Tutorial map, launched
  straight from the menu — **no second player needed**.
- Three role lessons that **chain** Survivor → Teacher → Hall Monitor (full course), or
  play any one on its own: move/flashlight/hide/crawl/breaker/questions/escape for the
  Survivor; scan/chase/capture/blackout for the Teacher; answer-to-unlock/hint/marker/trap
  for the Hall Monitor.
- Built so a 15–16 year old can't get stuck: every step has a generous timeout, the
  student is auto-revived if the scripted Teacher catches them, and a single marker
  always points at the next thing to do.

## Jumpscare overhaul

- New scare types alongside the monster charge: a **full-screen face image**, a passive
  **corner peek** that ducks back when you look at it, and a **"behind you"** payoff that
  holds a directive until you turn around. All honour the reduced-jumpscares comfort
  setting.
- A dedicated **real SCP-096** scare wired to the actual mesh/animations, plus a fix so
  its material no longer falls back to default grey in packaged builds.

## Authored maps on by default

- `bUseAuthoredLevels` now defaults to **True**: Facility, Substation, Foggrounds, and the
  Tutorial load from baked `.umap`s under `Content/BlackoutHunt/Maps` instead of the
  runtime generator. Loading identical placed actors on server and client also sidesteps
  the `ABHStaticBlockField` spec-array replication that exceeded the 64 KB net-bunch cap
  and failed to replicate on late joiners.
- The block field now batches by render/collision signature (one instanced component per
  signature, not per block), so a ~1800-block level no longer registers ~1800 components
  per client.

## Other changes

- **2K structural textures.** The concrete / plaster / metal / tile blockout textures were
  reimported from their 2K ambientCG sources (was 1K); the big world-aligned surfaces read
  far sharper. World-aligned (triplanar) concrete/plaster materials keep tiling consistent
  on large scaled walls and slabs.
- **Feedback.** Reports now include the player's graphics settings, and classroom builds no
  longer attach a student's display name (an anonymous session tag is used instead).
- **Bots** were re-tuned so a pursuer is never slower than a walking human, and hosts can
  fill the table to capacity from the menu.
- **Anti-grief.** Gates, power switches, and the security terminal only respond to alive
  players during a live round, and gate toggles are throttled.

## Build provenance & parity (read before testing)

This release is cut on the **Windows** dual-boot (UE 5.7 + MSVC):

- **Windows — freshly cooked at release time.** The primary artifact is the **EOS-profile
  Windows package** (Shipping), cooked from the exact `v0.7.0` commit via
  `Tools/Package-Windows-EOS.ps1` and verified with `Tools/Verify-EOSPackage.ps1`. The EOS
  net driver carries an `IpNetDriver` fallback, so the same build still does LAN / direct-IP
  / Playit tunnel.
- **Linux** is not re-cooked for `0.7.0` in this pass; the Linux cross-toolchain lives only
  on the Linux boot and a parity re-cook is pending there.

The exact attached filename and its SHA-256 are recorded on the GitHub release page.

## Known limits

- This is a **beta**: the new tutorial, jumpscares, and authored-maps-by-default switch are
  built and pass the automation suite, but have not yet had a full in-engine classroom
  playtest. Report anything that feels off.
- The authored maps are the baked **blockout** with a mesh-cladding art pass still in
  progress; they play identically to the generator but are not final-art.
- EOS online play needs the host/client to have the EOS profile build and an Epic account;
  the account-free classroom path (Playit / LAN) remains for school PCs.
- Unreal still requires a GPU/driver exposing Direct3D feature level 11.0 / Shader Model 5.0.

## Feedback

Record tester reports with:

- build version: `0.7.0`
- platform and exact package (EOS vs classroom)
- map and mode
- host/client count and the join path used (EOS lobby, saved Playit endpoint, direct LAN/IP)
- machine specs and graphics preset
- lesson preset or manual/JSON question set used
- steps to reproduce, and the packaged runtime log when safe to share
