# Movement & Advanced Movement Links

Blackout Hunt's player movement is a small server-authoritative state machine on `ABHCharacter`
(`EBHMovementSpecialState { None, Rolling, Sliding, Prone, Diving }`, `BHTypes.h`). On top of plain
walk/sprint/crouch/jump there is a set of **advanced movement links** — combos and chains decoded from held
input flags inside the `Start*` handlers. This doc is the single reference for what they are, how to do them, and
the cvars that tune them.

## Default bindings (`Config/DefaultInput.ini`)

| Action | Key |
| --- | --- |
| Move | `W A S D` |
| Jump | `Space` |
| Sprint | `Left Shift` (hold) |
| Crouch | `Left Ctrl` |
| Prone | `Left Alt` |

The advanced inputs are **combos** — the order matters (Sprint must already be held when you tap Ctrl/Alt).

## The links

| Link | Input | What it does |
| --- | --- | --- |
| **Bunny-hop** | tap `Shift` to reach sprint speed, then chain `Space` taps (buffer each just before landing) | Holds top speed across jumps. **Airborne the per-second sprint stamina drain stops** (and so does regen), so a clean chain is cheaper than holding Shift — you only pay the flat per-jump cost. There is a 0.16 s landing jump-buffer; air steering is weak (AirControl 0.16). |
| **Roll** | hold `Shift`, then tap `Ctrl` | Forward dash, stays standing height, **capture/hit-immune for the whole move**, auto-stands. A *clean* roll (no wall bump) is quieter than a bonked one (see `bh.QuietRollCleanScale`). |
| **Slide** | hold `Shift`, then tap `Alt` | Longer/lower than a roll, lowers the capsule, capture-immune. **Hold `Alt` → settle prone; release `Alt` late → stand; release `Alt` early → brake into a quiet crouch** (see `bh.SlideStopWindow`). Loud. |
| **Dive** | hold `Alt`, then tap `Space` while moving | Flat forward escape lunge, capture-immune, **always ends prone** (tap `Space`/`Ctrl` to stand). Loudest move. Started **from** prone it is the quieter "crawl-gap dash" (see `bh.CrawlDashNoiseScale`). |
| **Prone** | tap `Alt` (Sprint not held) | Slowest, quietest, hardest-to-see stance. |
| **Drop-roll** | hold `Shift` + tap `Ctrl` just before landing | Buffers the roll and fires it on touchdown, **suppressing the loud hard-landing noise** — a skill-timed silent landing (see `bh.DropRollWindow`). |
| **Roll-out-of-locker** | hold `Shift` while exiting a locker | Converts the exposed standing pop into a capture-immune forward roll (see `bh.LockerRollExit`). |
| **Momentum flow-chain** | re-input a transient move within ~0.12 s of the previous one ending | Survivor-only. Bypasses the cooldown once and grants a speed bonus, capped at 3 links. Early links are quieter; the link that hits the cap fires a loud **overextension** tell (see `bh.MomentumChainNoiseScale`). |
| **Vault** *(ships dark)* | roll/slide into low, vaultable cover | Hops up-and-over the obstacle instead of dead-stopping, consuming `FBHMovementCurveTuning.VerticalImpulse`. Off by default; needs playtest + collision review and level-geometry conventions before going live (see `bh.VaultTech`). |

Transient moves (roll/slide/dive) **cannot be cancelled early** — they run their full duration; the only "chain"
is the post-end flow-chain. Prone gives only a 0.64 s capture-grace tail, not ongoing immunity.

## Tuning cvars

All default to the shipped behaviour; set to the "off" value to disable a feature.

| cvar | Default | Meaning |
| --- | --- | --- |
| `bh.QuietRollCleanScale` | 0.65 | Roll-impact noise multiplier for a clean roll; bonked rolls stay 1.0. 1.0 = off. |
| `bh.MomentumChainNoiseScale` | 0.18 | Per-link noise reduction for a chained move (`noise *= 1 - k*depth`, floored 0.35). 0 = off. |
| `bh.SlideStopWindow` | 0.45 | Release Prone before this normalized slide time to brake into a quiet crouch. 0 = off. |
| `bh.DropRollWindow` | 0.18 s | Buffer a roll requested within this of landing and fire it on touchdown. 0 = off. |
| `bh.LockerRollExit` | 1 | Hold Sprint on a locker exit to roll out. 0 = plain stand-up exit. |
| `bh.CrawlDashNoiseScale` | 0.55 | Noise multiplier for a dive started from prone. 1.0 = same as a standing dive. |
| `bh.VaultTech` | 0 | Roll/slide into a low ledge vaults over it (consumes `VerticalImpulse`). **Off by default.** |
| `bh.VaultMaxLedgeHeight` | 95 | Max obstacle top height (cm) that `bh.VaultTech` will vault rather than dead-stop. |
| `bh.MomentumTech` / `bh.MomentumChainWindow` / `bh.MomentumChainMaxLinks` / `bh.MomentumChainSpeedScale` | 1 / 0.12 / 3 / 1.15 | The frame-perfect flow-chain knobs. |

Per-role speeds/distances/cooldowns live in `FBHMovementSpecialTuning` / `FBHMovementRoleTuning`
(`BHTypes.h`), defaulted in `BHMakeDefaultMovementSpecialTuning` (`BHCharacter.cpp`) and mirrored in
`BHMovementTuningAsset.cpp`; a `UBHMovementTuningAsset` / `UBHGameSettings` can override every value.

## Movement tutorial

A standalone **Movement** tutorial teaches every link hands-on: `EBHTutorialPhase::Movement`, driven by the
`EMovementStep` machine in `ABHTutorialDirector`. Launch it from **Play → Tutorial: one role → MOVEMENT TUTORIAL**
(or URL option `?BHTutorialPhase=Movement`). Each step prompts one move and auto-advances the instant the player
performs it — detected from live state (sprint speed / crouch / prone / airborne) and the server-side
`TutorialActionMask` latches for the transient moves (roll/slide/dive) and the bunny-hop chain — with a generous
timeout fallback so it can never hard-lock. It runs solo on the baked Tutorial map with no chase.

## Known follow-ups

- **Accessibility (toggle Sprint/Crouch/Prone, input-buffer leniency, auto-hop assist)** — designed, not yet
  built. Hooks: `IsComfortOptionEnabledForMenu` (`BHPlayerController`), the `BHGameSettings` Comfort block, and the
  `Start*`/`StopProne`/`TryBHopJump` handlers. Input-ergonomics only; must not change stamina/noise/cooldown.
- **Vault** is code-complete but `bh.VaultTech`-gated off pending collision/playtest review and a level-geometry
  "vaultable cover" convention (precedent: `ABHTrainRoofParkourGate`).
