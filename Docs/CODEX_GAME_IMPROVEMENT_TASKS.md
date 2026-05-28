# Blackout Hunt Codex Improvement Tasks

Use this file as handoff context for new Codex chats. A prompt like `work on task 2 in Docs/CODEX_GAME_IMPROVEMENT_TASKS.md` should be enough for Codex to understand the intended scope.

Recommended prompt format:

```text
Work on task N in Docs/CODEX_GAME_IMPROVEMENT_TASKS.md. Read the file, inspect the relevant code/assets first, follow the asset rule, implement the change, and run practical validation.
```

## Task Handoff Contract

Task numbers in this file are stable handoff IDs. Do not renumber existing tasks; append new tasks at the end and update the index below. A new Codex runner should be able to receive `work on task N` and know the intended player outcome, current implementation state, files to inspect first, safety constraints, acceptance criteria, and likely validation without asking for extra context.

Every task should keep this shape:

- Title: short player- or operator-facing outcome, not an implementation detail.
- High-value outcome: why the task matters to the beta or classroom experience.
- Current state: what already exists so the runner does not duplicate systems.
- Implementation guidance: constraints, non-goals, classroom safety, replication, assets, and comfort/accessibility notes.
- Search or inspection starting points: concrete code, config, docs, tools, and asset paths.
- Acceptance criteria: observable completion checks.
- Validation: build, package, support bundle, automation, or manual review appropriate to the touched surface.

When a task touches visuals, audio, packaging, or runtime asset references, the runner must check `Docs/ASSETS.md` and search `Content/` before adding placeholders. When a task touches host controls, classroom flow, accounts, reports, or networking, the runner must preserve host-only/admin boundaries and avoid exposing private student/account/network data.

## Recommended Polish Track

This section is the 10-task blueprint from the polish review: the 5 highest-value polish items plus the 5 new gameplay additions. Use IDs `P1` through `P10` to avoid conflicting with the existing numbered backlog below. A prompt like `work on polish task P3 in Docs/CODEX_GAME_IMPROVEMENT_TASKS.md` should be unambiguous.

The recommended order is:

| Polish task | Maps to backlog | Title | Why it is next |
| --- | --- | --- | --- |
| P1 | Task 2 | Facility Production Vertical Slice | One map needs to feel authored before adding more surface area. |
| P2 | Task 8 | HUD Objective Clarity | Live classroom players need to know the next action without a guide. |
| P3 | Task 1 | Classroom Preflight And Host Readiness | Teachers should verify network/package readiness before students wait. |
| P4 | Task 3 | Audio Identity Pass | Audio can improve horror feel and readability without new rules. |
| P5 | Task 4 | Security Camera Gameplay Loop | Existing camera actors/assets should become meaningful decisions. |
| P6 | Task 5 | Train Intermission Polish | The pacing layer should feel designed, not merely functional. |
| P7 | Task 11 | Role Warmup Room | Students need a short safe space to learn controls before a real round. |
| P8 | Task 14 | Hall Monitor Depth | Caught players should stay engaged without becoming full Teachers. |
| P9 | Task 16 | Map Event Modifiers | Replayability should improve without requiring a new map. |
| P10 | Task 15 | Physics Topic Objective Variety | Classroom content and physical gameplay should reinforce each other. |

### Polish Task P1 - Facility Production Vertical Slice

Turn Facility into the reference map direction before expanding the rest of the game. This is not a request to add a new map or increase objective count. The goal is to make the existing Facility read like an intentional horror space: landmarks, routes, exit language, lighting identity, lockers, objective silhouettes, and chase loops.

High-value outcome: when a player spawns or reaches the central hub, they can orient within seconds and understand where major routes, exits, and objective clusters are without relying on a HUD map.

Full task scope:

- Audit the current Facility route language from spawn, central hub, objective clusters, locker loops, and exits.
- Improve landmark identity for storage, lab, ward, utility/classroom, and exit routes using lighting, signs, silhouettes, materials, props, or decals.
- Make objective stations readable from doorway distance with silhouettes or nearby dressing, not only interaction prompts.
- Improve locker placement around chase loops where safe, with at least two break-line choices before capture commitment on major routes.
- Replace rough runtime placeholder language with more authored-looking signs/props where imported assets exist and package policy allows them.
- Capture or document validation viewpoints so future runners know what "readable" means in practice.

Current state:

- Facility is runtime-generated in C++ and already has route stripes, floor pads, blockers, lockers, batteries, objective stations, exits, and imported material use.
- `Docs/FACILITY_VERTICAL_SLICE.md` defines the acceptance goals and current runtime pass.
- A C++ vertical-slice pass already adds route chevrons, overhead signs, quadrant beacons, objective silhouettes, and route lights.
- Imported SmartBasicInterfaces props are already used through `ABHRuntimeMeshPropActor` with fallback blocks when meshes/materials are missing.

Implementation guidance:

- Start by reading the Facility generation code and `AddFacilityVerticalSlicePass()` in `BHGameMode.cpp`.
- Improve readability and visual identity before touching balance-sensitive placement.
- Keep the current objective count and core paths stable unless a placement clearly breaks the acceptance goals.
- Search imported assets before adding any new visual prop. Good candidates may exist in `Content/BlackoutHunt`, `Content/ResidentHorrorV1`, `Content/BFHorror`, `Content/HorrorTemplate`, `Content/RuinedCrypt`, and SmartBasicInterfaces.
- Use soft references, runtime lookup, or explicit fallbacks for optional content.
- Avoid broad map-builder refactors unless the current code makes the task impossible.

Inspect first:

- `Docs/FACILITY_VERTICAL_SLICE.md`
- `Docs/ASSETS.md`
- `Source/BlackoutHunt/BHGameMode.cpp`
- `Source/BlackoutHunt/BHRuntimeMeshPropActor.*`
- `Config/DefaultGame.ini`
- `Content/BlackoutHunt`, `Content/ResidentHorrorV1`, `Content/BFHorror`, `Content/HorrorTemplate`, `Content/RuinedCrypt`

Acceptance criteria:

- Every quadrant has a distinct landmark readable from the central hub.
- Main exit routes are findable without HUD map dependency.
- Objective silhouettes are readable from doorway distance.
- Lockers support escape loops rather than only dead-end hiding.
- Lighting/color communicates route identity.
- New visual assets have safe fallbacks or documented package paths.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Run `.\Tools\Verify-ClassroomPackage.ps1` if cook/stage paths or packaged assets change.
- Manually inspect Facility from spawn, central hub, each objective cluster, each exit, and at least two chase loops.
- Update `Docs/FACILITY_VERTICAL_SLICE.md` and `Docs/ASSETS.md` if asset use or acceptance state changes.

### Polish Task P2 - HUD Objective Clarity

Make live gameplay goals unmistakable for Survivors, Teachers, Hall Monitors, spectators, train passengers, and final escape players. Prefer short action text over explanations. The player should know what to do next and why an attempted action is blocked.

High-value outcome: reduce first-round confusion in live classroom sessions without adding tutorial text walls.

Full task scope:

- Audit all live HUD objective/status strings by role and phase: lobby, prep, hunt, Hall Monitor return, train, final escape, spectator, win/loss.
- Rewrite unclear prompts into short action text that names the next interaction, location, or blocked reason.
- Add missing blocked-action feedback for common failure cases such as wrong role, wrong phase, cooldown, contribution gate, distance, line of sight, resource, or host-only restriction.
- Keep classroom board, HUD, and menu labels consistent where they describe the same objective state.
- Preserve comfort/accessibility settings, including high-contrast HUD and captions for gameplay-relevant audio cues if touched.
- Hide test-only and host-only state from normal student clients.

Current state:

- HUD already shows prompts, role/status information, objective text, fear/dread concepts, train/final escape messaging, and blocked-action messages.
- Menu guide text explains the systems, but live gameplay still needs concise prompts.
- Classroom board already shows phase, timer, roster, objective progress, and revision mastery.

Implementation guidance:

- Keep prompts role-specific and do not expose hidden information.
- Do not expose host-only, admin, tunnel, classroom board, or test controls to students.
- Make Hall Monitor lock reasons mention Physics Classroom contribution gates when relevant.
- Keep exit/objective state language consistent between HUD, board, and menu.
- Preserve high-contrast HUD behavior.
- Avoid long strings in the center of the screen during chase pressure.

Inspect first:

- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/SBHClassroomBoard.*`
- `Source/BlackoutHunt/BHGameState.*`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHPlayerState.*`
- `Docs/TUNING.md`

Acceptance criteria:

- A new player can tell the next required action from HUD/status in Prep, Hunt, Train, and Final Escape states.
- Blocked actions explain why they are blocked.
- Teacher, Survivor, Hall Monitor, spectator, train, and final escape states remain distinct.
- Host-only and test-only messaging stays hidden during normal classroom play.
- High-contrast HUD still renders readable prompts.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Manually inspect HUD strings for each role and phase touched.
- If classroom board/status strings are changed, verify remote students do not see host-only details.

### Polish Task P3 - Classroom Preflight And Host Readiness

Build a host-facing classroom readiness panel that makes the deployment state obvious before students join. This should be useful on a teacher machine under real classroom constraints: locked-down Windows devices, Playit tunnel use, loopback defaults, D3D11 classroom builds, support bundle paths, and logs that a non-developer can find.

High-value outcome: the host can detect common setup problems before students are waiting in the room.

Full task scope:

- Add a host-only preflight panel or section in the menu flow where Live Classroom hosts already work.
- Summarize the active game version, beta target when available, map/classroom mode, configured Playit endpoint or join-code state, loopback-only host mode, direct LAN availability, tunnel/hotspot permission state, online subsystem, graphics preset, RHI/rendering mode, package path, runtime log path, and support bundle path.
- Provide explicit host actions where safe: refresh preflight, open log/support folder, create support bundle if a runtime-safe path exists, copy join code if already configured, and point the host toward the correct deployment doc.
- Make the readiness status readable as plain labels such as Ready, Needs setup, Missing endpoint, Package not found, Low graphics profile, or Unknown.
- Keep implementation small enough to ship safely, but design the API so support bundle/preflight code can grow without bloating Slate layout code.

Current state:

- Support bundle tooling already writes `PREFLIGHT.md` and a zip under `Builds\Support`.
- Live Classroom already has host flow, roster, saved Playit endpoint, classroom board, and host-only tabs.
- Classroom builds default to loopback + Playit for school-safe hosting.
- Some details live in docs/release notes, but the in-game host path does not yet surface them clearly.

Implementation guidance:

- Add this where classroom hosts already work, likely the Play or Classroom area of `SBHMainMenu.cpp`.
- Keep it host-only. Remote students must not see tunnel/admin/package/log details.
- Do not expose saved account data, backend tokens, IPs beyond the intended join endpoint, secrets, or unsafe logs.
- Reuse existing support/preflight logic where practical. Avoid copying full package-verification logic into UI.
- Prefer a small runtime summary object or helper if the UI needs several computed fields.
- Any external process or folder opening must be guarded for packaged and editor builds.
- The panel should help a teacher act, not dump raw developer logs.

Inspect first:

- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/SBHMainMenu.h`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHGameInstance.*`
- `Tools/New-ClassroomSupportBundle.ps1`
- `Tools/Verify-ClassroomPackage.ps1`
- `Docs/CLASSROOM_DEPLOYMENT.md`
- `Docs/ROADMAP.md`
- `Docs/BETA_RELEASE_NOTES_0.5.0-beta.1.md`

Acceptance criteria:

- Host can see whether classroom endpoint/tunnel setup looks ready before students join.
- Host can find package, log, and support paths without reading docs.
- Remote students cannot access host-only controls or diagnostics.
- The UI does not expose saved account data, backend secrets, private profile data, or unsafe logs.
- Preflight labels are understandable to a teacher, not only to a developer.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Run `.\Tools\New-ClassroomSupportBundle.ps1` if support-bundle behavior is touched.
- Manually inspect host and remote-student menu paths for visibility boundaries.
- Update `Docs/CLASSROOM_DEPLOYMENT.md` if the host workflow changes.

### Polish Task P4 - Audio Identity Pass

Improve moment-to-moment horror feel and gameplay readability through sound. Focus on cues players can act on: surface-specific footsteps, Teacher proximity tension, locker knocks, CCTV static, breaker hum, flashlight clicks, power-loss stingers, and ambient layers.

High-value outcome: the game feels more finished and readable without adding new mechanics.

Full task scope:

- Pick a coherent first pass across footsteps, Teacher proximity, lockers, CCTV, breakers, flashlight, and power-loss moments rather than adding one isolated sound.
- Wire imported audio through soft references or guarded loads with silent fallbacks.
- Add captions/status text for gameplay-relevant non-visual cues where captions are enabled.
- Review cooldowns, attenuation, team/role audience, and comfort settings so cues do not spam or mislead.
- Update cook/package policy only for the exact runtime folders required by the pass.
- Leave unaudited source audio out of staged builds.

Current state:

- Native footstep surface classes exist.
- Imported audio packages exist and are documented in `Docs/ASSETS.md`.
- Existing scare/audio cue work goes through `BHAtmosphereDirector` and client horror cue handling.
- Fear, dread, panic noise, decoys, scans, locker pressure, and atmosphere stimuli already exist.

Implementation guidance:

- Treat audio as gameplay feedback, not decoration.
- New cues must respect comfort and volume settings where relevant.
- Gameplay-relevant non-visual cues should have captions when captions are enabled.
- Use soft object paths or guarded loads so missing imported audio does not crash or block packaging.
- Do not use unaudited or risky source audio unless `Docs/ASSETS.md` marks it package-safe.
- Avoid spammy looped cues. Cooldowns and proximity gates matter.

Search assets first:

- `Content/A_Surface_Footstep`
- `Content/SoundsOfHorror`
- `Content/ResidentHorrorV1/Audio`
- `Content/FlashLight_System/Sound`
- `Content/SecurityCameras/Sounds`
- `Content/BlackoutHunt/Audio`

Inspect first:

- `Docs/ASSETS.md`
- `Source/BlackoutHunt/BHFootstepSurfaceComponent.*`
- `Source/BlackoutHunt/BHFootstepSurfaceVolume.*`
- `Source/BlackoutHunt/BHAtmosphereDirector.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/BHGameSettings.*`

Acceptance criteria:

- At least one important audio cue becomes more readable in normal gameplay.
- Imported assets are used where appropriate and documented if newly referenced.
- Missing assets do not crash, assert, or break classroom packaging.
- Comfort/audio settings still apply.
- Gameplay-relevant cues are not misleading or spammy.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Run `.\Tools\Verify-ClassroomPackage.ps1` if cook paths or asset staging change.
- Manually smoke test the changed cue in a normal gameplay path.
- Update `Docs/ASSETS.md` for new runtime audio paths.

### Polish Task P5 - Security Camera Gameplay Loop

Make security cameras and monitors mechanically useful. Cameras should create choices instead of only atmosphere: temporary Survivor presence pings, blind spots, CCTV power dependency, false Hall Monitor alerts, Teacher bait, or risky monitor use.

High-value outcome: existing camera assets and actors become a real gameplay loop with counterplay.

Full task scope:

- Define one complete camera loop end to end, such as temporary Survivor presence pings with blind spots, camera power dependency, and monitor feedback.
- Make camera state readable in-world or on the monitor: active, offline, jammed, false ping, cooldown, or blocked by blackout/power.
- Add a counterplay path so tracked players can adapt through blind spots, circuit switches, shutter state, cooldown timing, or movement choices.
- Decide whether Hall Monitors can create false CCTV alerts, and if so, keep the deception bounded and distinguishable after investigation.
- Preserve existing CCTV atmosphere/glitch behavior while adding the gameplay layer.
- Make replication/server authority explicit for any tracking or ping data.

Current state:

- Security camera, monitor, and CCTV zone actors already exist.
- CCTV stimuli can trigger atmosphere pressure and glitch cues.
- Imported camera meshes/materials/sounds exist under `Content/SecurityCameras`.
- Hall Monitors already have real/false hint tools and alarm traps.

Implementation guidance:

- Define who benefits from the loop before coding: Teacher, Survivors, Hall Monitors, or all roles.
- Keep location information fair. Do not give Teachers perfect tracking without cooldowns, range, blind spots, or power/circuit counterplay.
- Hall Monitor false pings should misdirect but remain bounded and eventually readable.
- Connect world/HUD/audio feedback to existing camera actors and assets.
- Consider tying cameras to circuit lighting, blackout, shutter, or terminal state.
- Preserve the distinction between Teacher capture and Hall Monitor pressure.

Search assets first:

- `Content/SecurityCameras`
- `Content/ResidentHorrorV1`
- `Content/SoundsOfHorror`

Inspect first:

- `Source/BlackoutHunt/BHSecurityCamera.*`
- `Source/BlackoutHunt/BHSecurityMonitor.*`
- `Source/BlackoutHunt/BHCCTVZone.*`
- `Source/BlackoutHunt/BHAtmosphereDirector.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHGameState.*`
- `Source/BlackoutHunt/BHHUD.*`

Acceptance criteria:

- Cameras produce at least one meaningful player decision.
- Camera feedback is readable in HUD, world, or audio.
- Any tracking has cooldown, range, power dependency, blind spot, or other counterplay.
- Hall Monitor interactions remain distinct from Teacher capture.
- Existing CCTV glitch/scare behavior still works.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Manually smoke test at least one camera/monitor decision from relevant roles.
- If replication is touched, verify server authority and client feedback paths.

### Polish Task P6 - Train Intermission Polish

Polish the train intermission into a clear pacing layer. Players should understand the current train phase, what they can do now, what happens next, and how bonus/shop/final escape systems affect the team.

High-value outcome: the train becomes a designed reward/recovery/escalation beat instead of only a transition.

Full task scope:

- Polish phase transitions for Arrival, Recap, Bonus Question, Shop, Station Stop, Departing, and Final Escape.
- Improve train display text so players know current phase, next action, remaining time, destination, and team recap status.
- Improve door feedback for opening, closing, locked, departing, and final escape handoff states.
- Improve shop feedback for cost, purchase success, insufficient currency, item charges, role restrictions, and cooldown/resource effects.
- Make bonus question presentation distinct from normal stations while preserving revision authority and scoring.
- Clarify final escape hunter release delay and anti-camp pressure without exposing hidden role/admin information.

Current state:

- Train phase enum includes Arrival, Recap, Bonus Question, Shop, Station Stop, and Departing.
- Train doors, displays, tunnel motion, bonus terminal, shop terminal, optional snack/drink/minigame activity stations, and final escape state already exist.
- Game instance builds recap overview, topics, missed questions, and tips.
- Train economy tests already exist.

Implementation guidance:

- Do not rebuild the train system from scratch.
- Improve visible/audio feedback around phase changes and remaining time.
- Shop purchases should clearly confirm cost, item, charges, role eligibility, and failures.
- Bonus questions should feel distinct from normal stations but use existing revision authority.
- Final escape messaging should explain hunter release delay and anti-camp pressure.
- Prefer existing train display actors before adding UI-only text.

Inspect first:

- `Source/BlackoutHunt/BHTrainIntermissionManager.*`
- `Source/BlackoutHunt/BHTrainDoor.*`
- `Source/BlackoutHunt/BHTrainDisplayActor.*`
- `Source/BlackoutHunt/BHTrainBonusQuestionTerminal.*`
- `Source/BlackoutHunt/BHPowerupShopTerminal.*`
- `Source/BlackoutHunt/BHGameInstance.*`
- `Source/BlackoutHunt/BHTrainEconomyTests.cpp`
- `Docs/ASSETS.md`

Acceptance criteria:

- Players can tell the current train phase and next action.
- Door/display/shop feedback is clearer than before.
- Classroom recap data still appears correctly.
- Final escape transition remains network-safe and replicated.
- Shop and bonus question feedback handles success and failure states.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Run targeted train economy/flow automation tests if touched.
- Manually review train display/shop/bonus/final escape text paths.

### Polish Task P7 - Role Warmup Room

Before a live round, give students a short, safe place to try core controls: flashlight, lockers, questions, decoys, scans, captures, and Hall Monitor tools. This should be optional and reset before the real match.

High-value outcome: reduce first-round confusion without making students read long instructions.

Full task scope:

- Add an optional host-controlled warmup phase or runtime practice area before the real round.
- Include practice affordances for flashlight, lockers, one safe question station, decoy/trap placement, Teacher scan/capture, and Hall Monitor tools where feasible.
- Add clear host controls to skip, start, or end warmup without exposing admin controls to students.
- Reset all warmup state before match start: cooldowns, resources, fear, detention marks, objective progress, traps, decoys, captures, reports, XP, and classroom statistics.
- Keep role information bounded. Survivors should not learn hidden Teacher-only information from the warmup implementation.
- Make late join, spectator, and ready-gate behavior predictable around the warmup phase.

Current state:

- Menu guide explains roles and controls.
- Live Classroom uses ready gate and host role assignment.
- Late joiners and spectators are handled separately from active players.
- Lockers, objective stations, decoys, traps, scans, captures, and Hall Monitor gates already exist.

Implementation guidance:

- Keep warmup short and host-controlled.
- A lobby practice phase or small runtime zone is preferable to a new full tutorial map unless required.
- Warmup actions must not affect classroom reports, XP, progression, round results, or real match stats.
- Do not leak Teacher-only information to Survivors.
- If live powers are usable in warmup, reset cooldowns, resources, fear, marks, captures, traps, decoys, and objective state before round start.
- Keep ForceStart/test affordances host-only.

Inspect first:

- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHGameState.*`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHPlayerState.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHLocker.*`
- `Source/BlackoutHunt/BHObjectiveStation.*`
- `Source/BlackoutHunt/BHAlarmTrap.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`

Acceptance criteria:

- Players can safely try their basic role controls before the real round.
- Warmup cannot decide the match or pollute reports/progression.
- Host can skip or end warmup.
- Resources and cooldowns reset for round start.
- Student clients do not gain host/admin controls.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Smoke test lobby to warmup to real round flow.
- Verify report/progression output does not include warmup-only activity.

### Polish Task P8 - Hall Monitor Depth

Give Hall Monitors more skill expression while preserving their identity as non-capturing pressure and misdirection players. Good candidates include fake objective pings, delayed traps, false CCTV alerts, short corridor denial, temporary fake footstep trails, or Teacher-visible route markers.

High-value outcome: caught students stay engaged without becoming full hunters.

Full task scope:

- Add one or two Hall Monitor tools with complete feedback, cooldowns, contribution-gate checks, and counterplay.
- Good first pair: false CCTV alert plus temporary fake footstep trail, or delayed trap plus Teacher-visible route marker.
- Keep all tools non-capturing and bounded in range, duration, and frequency.
- Add HUD/status reasons for locked, cooldown, invalid target, wrong phase, or contribution not met.
- Ensure Survivors and Teachers can read the effect enough to respond without making deception useless.
- Preserve classroom scoring and contribution gates for Physics Classroom Hall Monitors.

Current state:

- Hall Monitors are caught Survivors who return as fake hunters.
- They cannot capture.
- They can place alarm traps and send real/false hints.
- In Physics Classroom, their tools are gated by answer-team contribution.

Implementation guidance:

- Preserve the no-capture rule.
- New tools should create uncertainty, route pressure, or team communication choices.
- Tools must respect contribution gates and cooldowns.
- False information should misdirect, not make the game unreadable.
- Add clear HUD reasons when tools are locked, on cooldown, or unavailable by phase.
- Make the effect readable to Survivors and/or Teachers depending on intended audience.

Inspect first:

- `Source/BlackoutHunt/BHAlarmTrap.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHPlayerState.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/BHCCTVZone.*`
- `Source/BlackoutHunt/BHSecurityCamera.*`

Acceptance criteria:

- Hall Monitor has a new meaningful decision or a materially improved existing tool.
- Survivors and Teachers can read/respond to the effect.
- Tool lock and contribution rules remain intact.
- Hall Monitor still cannot capture.
- Tool failures produce clear HUD feedback.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Smoke test a Physics Classroom Hall Monitor before and after contribution unlock.
- Verify Teacher/Survivor clients only receive intended information.

### Polish Task P9 - Map Event Modifiers

Add one clear round modifier that changes how a familiar map plays without requiring a new map. The modifier must be visible, fair, replicated, and conservative enough for classroom defaults.

High-value outcome: the game gains replayability and surprise while reusing existing maps and systems.

Full task scope:

- Define a small modifier framework if the existing round options are not sufficient: enum/state, replicated active modifier, host/menu selection or random selection rules, HUD/classroom-board description, and safe defaults.
- Implement one production-quality modifier end to end before adding a list of half-finished modifiers.
- Good first modifier candidates are Unstable Lights, Noisy Floors, Dead CCTV, Low Battery Spawns, Jammed Doors, Roaming Blackout Zones, or Extra Locker Dread.
- Make the modifier affect both world behavior and player communication. Players should know the rule is active and be able to adapt.
- Make sure bots tolerate the modifier even if they do not fully optimize around it.

Current state:

- Round options and modifiers already exist in game state/menu concepts.
- Menu exposes some practice modifier controls.
- Maps are runtime-generated, which makes global modifiers practical.
- Lighting, CCTV, batteries, doors, lockers, fear/dread, and objective flow already have systems a modifier can hook into.

Implementation guidance:

- Add one modifier at a time with a complete UX and gameplay loop.
- Do not make a hidden random punishment. The modifier should be legible before or shortly after round start.
- Keep classroom default conservative and avoid modifiers that dramatically increase scare intensity unless the host opts in.
- Modifier state must replicate from the server. Clients should not independently roll the active modifier.
- Do not soft-lock objectives, exits, warmup, train flow, or late-join state.
- Avoid stacking many modifiers until each one is independently validated.

Inspect first:

- `Source/BlackoutHunt/BHTypes.h`
- `Source/BlackoutHunt/BHGameState.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/BHSecurityCamera.*`
- `Source/BlackoutHunt/BHFlickerLight.*`
- `Source/BlackoutHunt/BHDoor.*`
- `Source/BlackoutHunt/BHBatteryPickup.*`

Acceptance criteria:

- Host can see/select or identify the active modifier.
- Active modifier state replicates to clients.
- The modifier affects gameplay in a readable way.
- HUD or board text briefly explains the active rule.
- Modifier cannot soft-lock objectives, exits, or train/final escape flow.
- Bots can still complete or pressure a round without obvious crashes or stuck loops.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Smoke test a hosted round and a client view to confirm modifier replication and messaging.
- Run targeted automation tests if modifier state touches round flow, objectives, doors, cameras, or batteries.
- Update `Docs/TUNING.md` if the modifier exposes tunable values.

### Polish Task P10 - Physics Topic Objective Variety

Make physical objective interactions match the active Physics topic so classroom content and gameplay reinforce each other. Start with one topic as a vertical slice before generalizing.

High-value outcome: lesson questions stop feeling separate from the world task.

Full task scope:

- Build one topic-specific objective vertical slice, with Electricity as the likely first candidate because breakers/circuits already exist.
- Add visual identity, prompt language, station state, and completion feedback for the selected topic.
- Keep existing answer authority, mastery scoring, question selection, and team thresholds intact.
- Reuse the current hold/progress interaction underneath unless a small minigame clearly reinforces the lesson and remains quick.
- Create a pattern that can later support Waves, Forces and Motion, and Energy without duplicating station logic under unrelated names.
- Provide fallback visuals if imported meshes/materials are missing.

Current state:

- Objective stations already support station types and revision questions.
- Physics topics include Forces and Motion, Electricity, Waves, and Energy.
- Question bank includes topic, difficulty, question type, diagrams, hints, and explanations.
- Current physical station interaction can remain a hold/progress task underneath.

Implementation guidance:

- Keep answer authority and scoring in the existing revision system.
- Topic-themed physical tasks can reuse the same repair/progress interaction at first.
- Start with one topic, preferably Electricity because breakers/circuits already exist.
- Make silhouettes visually distinct from doorway distance.
- Avoid complex minigames unless they directly reinforce learning and do not slow the round.
- Use imported assets where available, with fallback visuals if assets are missing.

Examples:

- Electricity: restore circuits, route current, reset breakers, charge nodes.
- Waves: tune signal terminals, align receivers, stabilize CCTV/speaker interference.
- Forces and Motion: balance doors, release counterweights, align platform locks.
- Energy: reroute power, manage heat/efficiency, charge storage nodes.

Inspect first:

- `Source/BlackoutHunt/BHObjectiveStation.*`
- `Source/BlackoutHunt/BHRevisionQuestionBank.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHTypes.h`
- `Source/BlackoutHunt/BHHUD.*`
- `Docs/ASSETS.md`
- `Config/DefaultGame.ini`

Acceptance criteria:

- At least one Physics topic has a distinct physical interaction or visual identity.
- It still uses existing classroom scoring and mastery thresholds.
- HUD/objective text explains the action concisely.
- Fallback visuals exist if imported assets are unavailable.
- Bots and normal objective flow still tolerate the station.

Validation:

- Run `.\Tools\Build-Editor.ps1` for C++ changes when practical.
- Run targeted revision/objective automation tests if touched.
- Manually smoke test the topic-specific station from answer through physical completion.
- Run package verification if new cook paths are added.

## Existing Backlog Task Index

Use this as the quick disambiguation layer when assigning work by existing backlog number. These 19 tasks were already the broader improvement backlog; the `P1-P10` section above is the shorter recommended polish track.

| Task | Title | Primary outcome | First files to inspect | Key validation |
| --- | --- | --- | --- | --- |
| 1 | Classroom Preflight Screen | Host can verify classroom readiness before students wait. | `SBHMainMenu.cpp`, `BHPlayerController.cpp`, `Tools/New-ClassroomSupportBundle.ps1`, `Docs/CLASSROOM_DEPLOYMENT.md` | Build editor; create support bundle if touched. |
| 2 | Facility Authored Vertical Slice | Facility becomes the production reference map direction. | `BHGameMode.cpp`, `Docs/FACILITY_VERTICAL_SLICE.md`, `Docs/ASSETS.md`, `Content/` | Build editor; visual/manual map review. |
| 3 | Footstep And Audio Identity Pass | Horror/readability improves through gameplay-relevant audio. | `BHFootstepSurfaceComponent.*`, `BHAtmosphereDirector.*`, `BHCharacter.*`, audio asset paths | Build editor; package verification if cook paths change. |
| 4 | Security Camera Gameplay Loop | Cameras and monitors create real risk/reward decisions. | `BHSecurityCamera.*`, `BHSecurityMonitor.*`, `BHCCTVZone.*`, `BHAtmosphereDirector.*` | Build editor; manual role/counterplay review. |
| 5 | Train Intermission Polish | Train phase clearly communicates recap, shop, bonus, and departure beats. | `BHTrainIntermissionManager.*`, `BHTrainDoor.*`, `BHTrainDisplayActor.*`, `BHPowerupShopTerminal.*` | Build editor; train economy/flow tests if touched. |
| 6 | Bot Personality Tuning | Offline/test bots show distinct, useful archetypes. | `BHBotController.*`, `BHBotPolicySubsystem.*`, bot policy weights | Build editor; bot/debug round smoke test. |
| 7 | Asset, License, And Package Audit | Shipping risk is reduced with clear cook/license decisions. | `Docs/ASSETS.md`, `Config/DefaultGame.ini`, `Tools/Verify-ClassroomPackage.ps1`, `BlackoutHunt.uproject` | Package verification. |
| 8 | HUD Objective Clarity Pass | New players can tell the next required action in live play. | `BHHUD.*`, `SBHMainMenu.cpp`, `SBHClassroomBoard.*`, `BHGameState.*` | Build editor; manual HUD role review. |
| 9 | Split Large Source Files | Large files shrink one responsibility at a time with behavior preserved. | `BHGameMode.cpp`, `SBHMainMenu.cpp`, `Docs/MAINTAINABILITY.md` | Code health snapshot; build editor. |
| 10 | Teacher Lesson Presets | Hosts can save/apply repeat classroom settings safely. | `SBHMainMenu.cpp`, `BHPlayerController.*`, `BHGameMode.*`, `BHGameSettings.*` | Build editor; preset load/save smoke test. |
| 11 | Role Warmup Room | Students can safely practice role controls before a real round. | Lobby/ready flow, role assignment, lockers, objective stations, decoys/traps, capture/scan code | Build editor; classroom flow smoke test. |
| 12 | Dynamic Horror Director Budget | Scares vary and avoid repeatedly targeting the same player. | `BHAtmosphereDirector.*`, `BHGameMode.*`, `BHPlayerController.*`, `BHGameSettings.*` | Build editor; targeted scare/cue test or debug review. |
| 13 | Better Teacher Counterplay | Chases and captures feel readable, fair, and skill-timed. | `BHCharacter.*`, `BHPlayerController.*`, `BHHUD.*`, `BHDoor.*` | Build editor; chase/capture smoke test. |
| 14 | Hall Monitor Depth | Caught students gain non-capture pressure/misdirection choices. | `BHAlarmTrap.*`, `BHCharacter.*`, `BHPlayerState.*`, `BHGameMode.*`, `BHHUD.*` | Build editor; Hall Monitor gate/cooldown test. |
| 15 | Objective Variety By Physics Topic | Physical tasks reinforce the current Physics topic. | `BHObjectiveStation.*`, `BHRevisionQuestionBank.*`, `BHGameMode.*`, `BHTypes.h` | Build editor; revision/objective tests if touched. |
| 16 | Map Event Modifiers | One visible round modifier adds replayability without soft-locks. | `BHTypes.h`, `BHGameState.*`, `BHGameMode.*`, `SBHMainMenu.cpp` | Build editor; replicated modifier smoke test. |
| 17 | Spectator And Late-Join Usefulness | Late joiners have classroom-safe participation. | `BHGameMode.*`, `BHPlayerController.*`, `BHPlayerState.*`, `SBHClassroomBoard.*` | Build editor; late-join/spectator smoke test. |
| 18 | Progression Cosmetics Only | Local XP unlocks cosmetic choices without power creep. | `BHAccountSubsystem.*`, `SBHMainMenu.cpp`, Quaternius/flashlight assets | Build editor; local profile/reset smoke test. |
| 19 | Playtest Telemetry Heatmaps | Local anonymous data supports map/lesson tuning. | `BHGameMode.*`, `BHGameInstance.*`, `BHObjectiveStation.*`, report export code | Build editor; inspect anonymous output file. |

## Project Context

- Project root: `D:\BlackoutHunt`
- Engine: Unreal Engine 5
- Game: multiplayer classroom horror hunt prototype
- Current beta target: `0.5.0-beta.1`
- Core implementation style: native gameplay logic is mostly C++; keep new gameplay systems in C++ when extending existing systems.
- Important docs:
  - `README.md`
  - `Docs/ROADMAP.md`
  - `Docs/FACILITY_VERTICAL_SLICE.md`
  - `Docs/ASSETS.md`
  - `Docs/CLASSROOM_DEPLOYMENT.md`
  - `Docs/MAINTAINABILITY.md`
  - `Docs/TUNING.md`

## Current Running Tasks

These tasks are already in progress elsewhere. Avoid duplicating them unless explicitly asked.

- Add sprint roll and slide
- Import Fab and Epic packages
- Add teacher melee weapon
- Find unused Epic modules
- Fix Foggrounds lighting issue
- Fix jumpscare entity spawn
- Improve game stability

## Asset Rule

Before building new gameplay visuals or placeholder props, search `Content/` for relevant imported Epic/Fab/Marketplace packages and use those assets when they fit the feature. Prefer existing imported meshes, materials, effects, sounds, and blueprints over procedural cubes or new stand-ins.

Keep native gameplay logic in C++ when that is how the system is implemented, but wire C++ actors to the best available imported assets with safe fallbacks for missing package content.

Useful imported content paths include:

- `Content/BlackoutHunt`
- `Content/SecurityCameras`
- `Content/FlashLight_System`
- `Content/SoundsOfHorror`
- `Content/Free_Jumpscares`
- `Content/FirstPersonHorrorKit`
- `Content/BFHorror`
- `Content/HorrorTemplate`
- `Content/ResidentHorrorV1`
- `Content/RuinedCrypt`
- `Content/A_Surface_Footstep`
- `Content/BlackoutHunt/Art/Characters/Quaternius`

When implementing visual/audio work, search `Content/` first with `rg --files Content | rg "term"` or PowerShell `Get-ChildItem -Recurse`. Do not assume a procedural placeholder is needed until relevant imported assets have been checked.

## Existing Feature Notes

Post-round and classroom reporting already exist. Before proposing or adding reporting, inspect:

- `Source/BlackoutHunt/BHGameMode.cpp`, around classroom performance export
- `Source/BlackoutHunt/BHGameInstance.cpp`, train recap builders
- `Source/BlackoutHunt/BHTrainIntermissionManager.cpp`, recap display wiring

The project already has systems for:

- Live Classroom hosting
- Physics question focus, difficulty mix, and mastery thresholds
- Classroom board/projector view
- Classroom performance export
- Train intermission and final escape pacing
- Powerups and train shop
- Security cameras, CCTV zones, and monitor actors
- Atmosphere director and scare cues
- Jumpscare variants and fallback visuals
- Bot support, bot personalities, and StateTree-backed hunter atmosphere
- Local accounts, XP, and progress
- Comfort settings: reduced jumpscares, reduced flash, reduced camera shake, captions, high-contrast HUD

## Gameplay Vocabulary

- Teacher: hunter role. Can capture visible Survivors, search lockers, scan with `Q`, and trigger blackout pressure with `R`.
- Survivor: objective role. Answers Physics questions, repairs/uses objective stations and breakers, hides in lockers, uses flashlight, drops decoys, and escapes.
- Hall Monitor: caught Survivor return role. Looks threatening, cannot capture, can use traps/hints after classroom contribution gates are met.
- Physics Classroom: live classroom mode with revision questions, class mastery, individual mastery, weak topic tracking, train recap, and performance export.
- Facility: current central reference map, still runtime-generated/blockout-like. This should become the production vertical slice before adding more maps.
- Substation: larger industrial runtime map.
- Foggrounds: outdoor fog-heavy runtime map with perimeter routes.
- Train intermission: pacing layer between stages with recap, bonus question, shop, doors, stage timers, and final escape.

## General Implementation Guidance

- Prefer small, scoped changes that match existing C++ patterns.
- Read relevant classes before editing. This codebase has many gameplay systems already; avoid duplicating a system that exists under a different name.
- Keep classroom safety in mind: host/admin controls must stay host-only, students should not see or use admin/test functionality, and private profile/account data should not be exported.
- Preserve comfort settings. New scare, flash, camera shake, or audio intensity work should respect reduced jumpscares, reduced flash, reduced camera shake, captions, and high-contrast HUD where relevant.
- Keep package safety in mind. If a feature depends on imported assets, make sure those assets are in cook/stage paths or use soft references with fallbacks.
- Add or update focused tests when touching shared behavior, networking, classroom gates, reports, round flow, bots, or package tooling.
- For source moves/refactors, move one responsibility at a time and keep behavior identical unless the task explicitly asks for behavior changes.

## Practical Validation

Choose validation based on the task. Good options include:

- Build editor target: `.\Tools\Build-Editor.ps1`
- Run package verification when package/cook/assets are touched: `.\Tools\Verify-ClassroomPackage.ps1`
- Create support bundle when classroom preflight/support behavior is touched: `.\Tools\New-ClassroomSupportBundle.ps1`
- Run targeted Unreal automation tests when available. Existing test areas include native proof, network support, revision flow, jumpscare variants, train economy, and automation command-line parsing.
- For UI/HUD changes, manually inspect menu/HUD code paths and make sure normal students do not see host/test controls.
- For asset work, document asset paths used and confirm safe fallbacks for missing assets.

## Tasks

### Task 1 - Classroom Preflight Screen

Add a host-facing preflight screen or panel before students join. It should summarize version, configured Playit endpoint, loopback mode, tunnel/hotspot permissions, online subsystem, graphics preset/RHI, package/log paths, and whether the host is ready for classroom play.

High-value outcome: reduce teacher setup confusion and make network/package issues visible before students are waiting.

Current state:

- Support bundle tooling already writes `PREFLIGHT.md` and a zip under `Builds\Support`.
- Live Classroom already has host flow, roster, saved Playit endpoint, classroom board, and host-only tabs.
- Some preflight status exists in docs/release notes, but the in-game host path can still surface it more clearly.

Implementation guidance:

- Add this where classroom hosts already work, probably the Classroom or Play area in `SBHMainMenu.cpp`.
- Keep it host-only. Remote students should not see tunnel/admin/package details.
- Reuse existing support/preflight logic where practical. Avoid duplicating complex package-verification logic inside UI if a tool already owns it.
- The UI can start with a textual panel and explicit actions: refresh preflight, open support folder, create support bundle if callable.
- If adding a runtime summary API, keep it small and testable.

Check existing support bundle and preflight tooling first:

- `Tools/New-ClassroomSupportBundle.ps1`
- `Docs/CLASSROOM_DEPLOYMENT.md`
- `Docs/ROADMAP.md`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/BHPlayerController.cpp`

Acceptance criteria:

- Host can see whether the classroom endpoint/tunnel setup looks ready before students join.
- Host can find log/support paths without reading docs.
- Remote students cannot access host-only controls.
- No saved account data, backend secrets, or unsafe logs are exposed through UI/export.

### Task 2 - Facility Authored Vertical Slice

Turn Facility from runtime blockout into the production reference map direction. Prioritize landmarks, readable routes, embedded signage/decals, lighting identity, chase loops, locker placement, objective silhouettes, and validation screenshots.

High-value outcome: one map should feel intentionally authored before expanding map count.

Current state:

- Facility is runtime-generated and has route stripes, landmark floor pads, blockers, lockers, batteries, stations, exits, and imported material use.
- `Docs/FACILITY_VERTICAL_SLICE.md` defines the current acceptance goals.
- Current runtime route-readability work is a scaffold, not final authored presentation.

Implementation guidance:

- Start by reading the Facility generation code inside `BHGameMode.cpp`. Identify the Facility builder/placement functions before editing.
- Improve readability and route identity before adding more objective count.
- Use imported assets where available: signs, decals, doors, props, clutter, horror environment pieces, lights, and materials.
- Keep balance stable unless the task specifically asks for layout rebalance. If moving lockers/objectives, reason about chase loops and interaction prompt overlap.
- Prefer data-driven/reusable room or prop helpers if the same visual appears repeatedly.
- Avoid turning this into a broad map-system rewrite. One strong vertical slice is the goal.

Use `Docs/FACILITY_VERTICAL_SLICE.md` as the acceptance gate. Search imported assets before adding any new props. Good candidates may exist in `ResidentHorrorV1`, `RuinedCrypt`, `BFHorror`, `HorrorTemplate`, and project-local `BlackoutHunt` materials.

Acceptance criteria:

- Every quadrant has a distinct landmark readable from the central hub.
- Main exit routes are findable without HUD map dependency.
- Objective silhouettes are readable from doorway distance.
- Lockers support escape loops rather than only dead-end hiding.
- Lighting/color communicates route identity.
- New visual assets have safe fallbacks or documented package paths.

### Task 3 - Footstep And Audio Identity Pass

Improve moment-to-moment horror feel through sound. Add or wire surface-specific footsteps, Teacher proximity tension, locker knocks, CCTV static, breaker hum, flashlight clicks, power-loss stingers, and ambient layers.

High-value outcome: stronger readability and tension without adding new mechanics.

Current state:

- The project has native footstep surface classes and imported footstep/audio packages.
- Existing horror cue work goes through `BHAtmosphereDirector` and client horror cue handling.
- The game already uses fear/dread, panic noise, decoys, scans, locker pressure, and atmosphere stimuli.

Implementation guidance:

- Treat audio as gameplay feedback, not just decoration.
- Surface-specific footsteps should help Teachers and Survivors understand risk.
- New scare/audio cues must respect comfort and volume settings.
- Prefer soft object paths and graceful fallback if an imported sound is missing.
- Avoid using copyrighted or unaudited source audio unless `Docs/ASSETS.md` says it is package-safe.
- Consider captions for important non-visual cues if captions are enabled.

Search these assets first:

- `Content/A_Surface_Footstep`
- `Content/SoundsOfHorror`
- `Content/ResidentHorrorV1/Audio`
- `Content/FlashLight_System/Sound`
- `Content/SecurityCameras/Sounds`

Relevant code:

- `Source/BlackoutHunt/BHFootstepSurfaceComponent.*`
- `Source/BlackoutHunt/BHFootstepSurfaceVolume.*`
- `Source/BlackoutHunt/BHAtmosphereDirector.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHHUD.*`

Acceptance criteria:

- At least one important audio cue becomes more readable in normal gameplay.
- Imported assets are used where appropriate.
- Missing assets do not crash or break packaging.
- Comfort/audio settings still apply.
- Any new gameplay-relevant cue is not misleading or spammy.

### Task 4 - Security Camera Gameplay Loop

Make security cameras and monitors mechanically useful. Possibilities include temporary Survivor tracking, camera blind spots, false Hall Monitor pings, CCTV-triggered scares, Teacher bait, and monitor feeds that create risk/reward decisions.

High-value outcome: turn the existing camera assets and actors into a real gameplay loop.

Current state:

- Security camera, monitor, and CCTV zone actors already exist.
- CCTV stimuli can trigger atmosphere pressure and CCTV glitch cues.
- Imported camera meshes/materials/sounds are present under `Content/SecurityCameras`.

Implementation guidance:

- Define who benefits from cameras: Teacher, Survivors, Hall Monitors, or all roles.
- Keep location information fair. Avoid giving Teachers perfect tracking without counterplay.
- Add blind spots or cooldowns if a camera reveals player presence.
- Hall Monitor false pings should be useful deception but not impossible to verify.
- Connect visuals/audio to existing camera assets instead of placeholder screens.
- Consider map placement and power/light circuits: cameras can depend on active circuit state.

Search assets first:

- `Content/SecurityCameras`
- `Content/ResidentHorrorV1`
- `Content/SoundsOfHorror`

Relevant code:

- `Source/BlackoutHunt/BHSecurityCamera.*`
- `Source/BlackoutHunt/BHSecurityMonitor.*`
- `Source/BlackoutHunt/BHCCTVZone.*`
- `Source/BlackoutHunt/BHAtmosphereDirector.*`

Acceptance criteria:

- Cameras produce at least one meaningful player decision.
- Camera feedback is readable in HUD/world/audio.
- Any tracking has cooldown/range/counterplay.
- Hall Monitor interactions remain distinct from Teacher capture.
- Existing CCTV glitch/scare systems still work.

### Task 5 - Train Intermission Polish

Polish the train intermission into a strong pacing layer. Improve recap displays, animated doors, shop feedback, bonus question drama, destination signage, final escape callouts, and clarity around what players should do before departure.

High-value outcome: make the train phase feel like a designed reward/recovery/escalation beat rather than a functional transition.

Current state:

- Train phase enum includes Arrival, Recap, Bonus Question, Shop, Station Stop, Departing.
- Train doors, displays, tunnel motion, bonus terminal, shop terminal, and final escape state already exist.
- Game instance builds recap overview, topics, missed questions, and tips.

Implementation guidance:

- Do not rebuild the train system from scratch.
- Improve visible/audio feedback around phase changes and remaining time.
- Shop purchases should clearly confirm cost, item, charges, and role eligibility.
- Bonus questions should feel distinct from normal stations but use the same revision authority.
- Final escape messaging should make hunter release delay and anti-camp pressure understandable.
- Use existing train display actors before adding UI-only text.

Relevant code:

- `Source/BlackoutHunt/BHTrainIntermissionManager.*`
- `Source/BlackoutHunt/BHTrainDoor.*`
- `Source/BlackoutHunt/BHTrainDisplayActor.*`
- `Source/BlackoutHunt/BHTrainBonusQuestionTerminal.*`
- `Source/BlackoutHunt/BHPowerupShopTerminal.*`
- `Source/BlackoutHunt/BHGameInstance.*`

Acceptance criteria:

- Players can tell the current train phase and next action.
- Door/display/shop feedback is clearer than before.
- Classroom recap data still appears correctly.
- Final escape transition remains network-safe and replicated.

### Task 6 - Bot Personality Tuning

Make offline/test rounds more believable by adding or tuning visible bot archetypes: objective runner, cautious hider, decoy baiter, aggressive Teacher, roaming Hall Monitor.

High-value outcome: solo testing and classroom fill behave less randomly and better expose gameplay systems.

Keep behavior data-driven where possible.

Current state:

- Bot controller already builds decision candidates for survivor, Teacher, and Hall Monitor behaviors.
- There is a local bot policy subsystem and a StateTree hunter atmosphere asset.
- Bot difficulty and debug/status commands exist.

Implementation guidance:

- First inspect current `EBHBotPersonality`, candidate weights, and bot debug output.
- Tune one or two archetypes at a time.
- Make behaviors visible through debug/status labels so designers can tell which archetype is active.
- Avoid making bots perfect. Human-readable mistakes are useful for classrooms.
- If behavior touches objective claims, stuck handling, or pathing, add focused tests or debug markers.

Relevant code:

- `Source/BlackoutHunt/BHBotController.*`
- `Source/BlackoutHunt/BHBotPolicySubsystem.*`
- `Source/BlackoutHunt/BHBotStateTreeNodes.*`
- `Content/BlackoutHunt/AI/ST_BH_HunterAtmosphere.uasset`
- `Models/BlackoutHuntBotPolicy/bot_policy_weights.ini`

Acceptance criteria:

- At least one bot archetype behaves more distinctly.
- Debug/status output identifies intent/personality clearly.
- Offline bot rounds still start and complete.
- No regression to host/student role gates.

### Task 7 - Asset, License, And Package Audit

Resolve package-safe content decisions before broader distribution. Audit SCP096, FNaTI, low-poly source assets, Fab imports, cook paths, attribution text, and unused or risky assets.

High-value outcome: reduce shipping/legal/package risk.

Current state:

- `Docs/ASSETS.md` documents package-safe paths and known risky/prototype assets.
- The beta includes the SCP096 prototype only for classroom testing.
- Some imported source assets are explicitly excluded until license evidence is settled.
- Packaging settings cook selected `Content/BlackoutHunt` paths plus specific supporting paths.

Implementation guidance:

- Do not remove assets blindly. First document what is used, cooked, staged, or referenced by soft path.
- Search C++ config and assets for references before changing cook paths.
- Separate license decisions from technical unused-module cleanup.
- Update docs and verification scripts together if policy changes.
- Be conservative with Marketplace/Fab content until license and redistribution terms are clear.

Start with:

- `Docs/ASSETS.md`
- `Config/DefaultGame.ini`
- `Tools/Verify-ClassroomPackage.ps1`
- `BlackoutHunt.uproject`
- `Content/BlackoutHunt/Art`
- `Content/Free_Jumpscares`

Acceptance criteria:

- Risky assets have a clear include/exclude decision.
- Package cook/stage paths match documented policy.
- Attribution requirements are documented where needed.
- Verification tooling catches forbidden saved data/logs/secrets.

### Task 8 - HUD Objective Clarity Pass

Make moment-to-moment goals unmistakable. Improve text/status around answering stations, repairing breakers, exit open state, Teacher proximity, noise penalties, fear/dread, Hall Monitor tool locks, and train/final escape phases.

High-value outcome: reduce player confusion during live classroom play.

Current state:

- HUD already shows prompts, role/status information, objective text, fear/dread concepts, and train/final escape messaging.
- Menu guide text contains detailed explanations, but live gameplay still needs concise prompts.
- Classroom board shows phase, timer, roster, objective progress, and revision mastery.

Implementation guidance:

- Prefer short action-oriented text over long explanations during gameplay.
- Role-specific prompts should not expose hidden information or admin/test state.
- Hall Monitor lock reasons should mention contribution gates in Physics Classroom.
- Exit and objective states should be consistent between HUD, board, and menu.
- High-contrast HUD setting should still work.

Relevant code:

- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/SBHClassroomBoard.*`
- `Source/BlackoutHunt/BHGameState.*`
- `Source/BlackoutHunt/BHPlayerController.*`

Acceptance criteria:

- A new player can tell the next required action from HUD/status.
- Blocked actions explain why they are blocked.
- Teacher, Survivor, Hall Monitor, spectator, train, and final escape states remain distinct.
- Host-only/test noise is hidden during normal classroom play.

### Task 9 - Split Large Source Files

Reduce risk in the largest source files by moving one responsibility at a time while preserving behavior.

Primary targets:

- `Source/BlackoutHunt/BHGameMode.cpp`
- `Source/BlackoutHunt/SBHMainMenu.cpp`

Suggested extraction order:

- Runtime map builders
- Train/final escape flow
- Classroom revision flow
- Scare director bridge
- Bot services
- Menu panels/widgets

Current state:

- `BHGameMode.cpp` and `SBHMainMenu.cpp` are very large and contain multiple responsibilities.
- `Docs/MAINTAINABILITY.md` records extraction targets and rules.
- Broad file moves are intentionally deferred until behavior is stable, so each extraction should be narrow.

Implementation guidance:

- Do not combine refactor and feature behavior unless explicitly asked.
- Move one cohesive responsibility per change.
- Preserve public method names where possible to reduce call-site churn.
- Add helper classes/files with project vocabulary, not generic abstractions.
- Build after moves. Large Unreal C++ moves often fail on includes/module dependencies.
- Watch for generated code, reflection macros, and UObject ownership rules.

Use `Docs/MAINTAINABILITY.md` and `Tools/New-CodeHealthSnapshot.ps1`.

Acceptance criteria:

- Behavior is unchanged.
- The moved responsibility has a clearer owning file/class.
- Includes/build dependencies remain sane.
- Editor target builds.

### Task 10 - Teacher Lesson Presets

Let hosts save and load lesson/game presets such as `Electricity easy`, `Mixed exam prep`, `Low scare`, or `Hard mode`.

High-value outcome: make repeat classroom setup faster and less error-prone.

Current state:

- Host can adjust question focus, difficulty mix, mastery targets, scare intensity, role setup, maps, and bot settings.
- Settings/defaults live partly in `Config/DefaultGame.ini` and runtime menu state.
- Local accounts/profiles exist, but classroom mode should not require external login.

Implementation guidance:

- Store presets locally, probably under `Saved/` or a config-backed user settings path.
- Presets should be host-only and local-machine scoped.
- Include import/export only if safe and simple; avoid account sync unless requested.
- Validate loaded values against current enum/range constraints.
- The UI should make applying a preset explicit so a teacher does not accidentally change a live lobby.

Likely preset fields:

- Physics topic focus
- Difficulty mix
- Class mastery target
- Individual mastery target
- Round duration
- Scare intensity
- Map choice
- Bot count/difficulty
- Comfort defaults where host-controlled

Relevant code:

- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHGameSettings.*`
- `Config/DefaultGame.ini`

Acceptance criteria:

- Host can save a named preset and apply it later.
- Invalid/stale preset values fail gracefully.
- Students cannot change host presets.
- Applied presets update visible classroom controls/status.

### Task 11 - Role Warmup Room

Before a live round, give students a short safe space to try flashlight, lockers, questions, decoys, scans, captures, and Hall Monitor tools.

High-value outcome: reduce first-round confusion without adding tutorial text walls.

Current state:

- Menu guide explains roles and controls.
- Live Classroom uses ready gate and host role assignment.
- Late joiners and spectators are handled separately from active players.

Implementation guidance:

- Keep warmup short and optional for experienced hosts.
- Avoid creating a separate tutorial project/map unless needed; a lobby practice phase or small runtime zone may be enough.
- Warmup actions should not affect classroom reports, XP, or real round stats unless explicitly designed.
- Do not leak Teacher-only information to Survivors.
- If using live role powers in warmup, reset cooldowns/resources before the real round.

Can be implemented as a lobby practice phase, a small runtime training area, or a guided pre-round interaction sequence. Keep it classroom-safe and short.

Relevant systems:

- Lobby/ready flow
- Role assignment
- Lockers
- Objective stations
- Decoys/traps
- Teacher scan/capture
- Hall Monitor tool unlocks

Acceptance criteria:

- Players can safely try their basic role controls before the real round.
- Warmup cannot decide the match or pollute reports.
- Host can skip or end warmup.
- Resources/cooldowns reset for round start.

### Task 12 - Dynamic Horror Director Budget

Track recent scare exposure per player and spend a pressure budget on varied cues: lights, footsteps, CCTV, whispers, locker knocks, audio stingers, or monster beats. Avoid repeatedly hammering the same player with jumpscares.

High-value outcome: better pacing, fewer cheap scares, and stronger classroom comfort control.

Current state:

- `BHAtmosphereDirector` handles atmosphere stimuli and scare cue triggering.
- Game mode applies presence spikes and chooses jumpscare variants.
- Comfort/scare intensity settings already affect sensory scaling.

Implementation guidance:

- Add per-player recent scare memory and cooldowns.
- Separate pressure from sensory intensity. A low-scare classroom can still use subtle audio/lights.
- Keep cues varied by type and location.
- Prefer deterministic-ish behavior from round seed where possible so bugs can be reproduced.
- Do not starve gameplay-critical warnings because of scare budget.
- Respect reduced jumpscares, reduced flash, reduced camera shake, and captions.

Relevant code:

- `Source/BlackoutHunt/BHAtmosphereDirector.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHGameSettings.*`
- `Source/BlackoutHunt/BHTypes.h`

Acceptance criteria:

- Repeated scares against the same player are throttled.
- Multiple cue types can be selected.
- Scare intensity/comfort settings alter frequency and sensory output.
- Debug/status output can explain recent director choices.

### Task 13 - Better Teacher Counterplay

As Teacher melee/chase behavior is added, make counterplay readable and fair. Add elements like attack windup, miss recovery, survivor slide/duck timing, flashlight stagger, door-slam escape windows, or stamina/noise tradeoffs.

High-value outcome: captures feel earned instead of arbitrary.

Coordinate with the running task `Add teacher melee weapon`.

Current state:

- Teacher currently has capture, heartbeat scan, blackout, and locker search behavior.
- Sprint/roll/slide and Teacher melee are already running tasks elsewhere.
- Fear/dread/stamina/noise systems already affect chase pressure.

Implementation guidance:

- Do not implement a second melee system if the running melee task already owns it.
- If modifying counterplay, inspect current capture distance, visibility checks, movement, stamina, door, and locker code first.
- Counterplay should have readable animation/audio/HUD feedback.
- Survivors should pay a cost for escape tools: stamina, noise, cooldown, timing risk, or position.
- Teachers need miss recovery and feedback, not just failed input.

Relevant code:

- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHHUD.*`
- `Source/BlackoutHunt/BHDoor.*`
- movement tuning assets/settings

Acceptance criteria:

- Teacher attack/capture has clear tell and result feedback.
- Survivors have at least one skill-timed response.
- Failed attacks and successful escapes have costs/cooldowns.
- Bots do not break if melee/counterplay changes.

### Task 14 - Hall Monitor Depth

Give Hall Monitors more skill expression while preserving their identity as non-Teacher pressure/misdirection players.

Possible tools:

- Fake objective ping
- Delayed trap
- False CCTV alert
- Short corridor denial
- Temporary fake footstep trail
- Teacher-visible route marker

High-value outcome: caught students stay engaged without becoming full hunters.

Current state:

- Hall Monitors are caught Survivors who return as fake hunters.
- They cannot capture.
- They can place alarm traps and send real/false hints.
- In Physics Classroom, their tools are gated by answer-team contribution.

Implementation guidance:

- Preserve the no-capture rule.
- New monitor tools should create uncertainty, route pressure, or team communication choices.
- Tools must respect classroom contribution gates.
- Keep false information bounded. It should misdirect, not make the game unreadable.
- Add clear HUD reasons when tools are locked or on cooldown.

Relevant code:

- `Source/BlackoutHunt/BHAlarmTrap.*`
- `Source/BlackoutHunt/BHCharacter.*`
- `Source/BlackoutHunt/BHPlayerState.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHHUD.*`

Acceptance criteria:

- Hall Monitor has a new meaningful decision or improved existing tool.
- Survivors and Teachers can read/respond to the effect.
- Tool lock/contribution rules remain intact.
- Hall Monitor still cannot capture.

### Task 15 - Objective Variety By Physics Topic

Make physical objective interactions match the current Physics topic.

Examples:

- Electricity: restore circuits, route current, reset breakers
- Waves: tune signal terminals or align receivers
- Forces and motion: balance doors, counterweights, or platform locks
- Energy: reroute power, manage heat/efficiency, charge storage nodes

High-value outcome: classroom questions and gameplay actions reinforce each other.

Current state:

- Objective stations already support station types and revision questions.
- Physics topics include Forces and Motion, Electricity, Waves, and Energy.
- Question bank includes topic, difficulty, question type, diagrams, hints, and explanations.

Implementation guidance:

- Keep the answer authority and scoring in the existing revision system.
- Topic-themed physical tasks can reuse the same hold/progress interaction underneath.
- Start with one topic as a vertical slice, then generalize.
- Make world silhouettes visually distinct so students understand task type before interacting.
- Avoid slowing the round with complex minigames unless they reinforce learning.

Relevant code:

- `Source/BlackoutHunt/BHObjectiveStation.*`
- `Source/BlackoutHunt/BHRevisionQuestionBank.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHTypes.h`

Acceptance criteria:

- At least one Physics topic has a distinct physical interaction/visual identity.
- It still uses existing classroom scoring and thresholds.
- HUD/objective text explains the action concisely.
- Fallback visuals exist if imported assets are unavailable.

### Task 16 - Map Event Modifiers

Add one clear modifier per round for replayability.

Examples:

- Jammed doors
- Unstable lights
- Noisy floors
- Dead CCTV
- Low battery spawns
- Roaming blackout zones
- Extra locker dread

High-value outcome: variety without adding a new map.

Relevant existing concept: practice/round modifiers in menu and game state.

Current state:

- Round options and modifiers already exist in game state/menu.
- Menu exposes some practice modifier controls.
- Maps are runtime-generated, which makes global modifiers practical.

Implementation guidance:

- Add one modifier at a time with visible UI/HUD explanation.
- Modifiers should be legible and fair, not hidden random punishment.
- Keep classroom default conservative.
- Make sure bots understand or at least tolerate the modifier.
- Avoid combining many modifiers until each one is validated.

Relevant code:

- `Source/BlackoutHunt/BHTypes.h`
- `Source/BlackoutHunt/BHGameState.*`
- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`

Acceptance criteria:

- Host can see/select or identify the active modifier.
- Active modifier affects gameplay in a readable way.
- Modifier state replicates to clients.
- Modifier does not soft-lock objectives/exits.

### Task 17 - Spectator And Late-Join Usefulness

Late joiners already enter as survivor spectators during active rounds. Give them classroom-safe participation so they are not idle.

Possible options:

- Teacher-approved hint pool
- Audience vote between harmless prompts
- Projector-only prediction question
- Non-location-revealing team encouragement
- Queue role preference for next round

High-value outcome: late students remain included without compromising active play.

Current state:

- Late joiners during Hunt enter as survivor spectators until next lobby.
- Classroom board and roster already track players and phase.
- Host/admin restrictions are important in classroom mode.

Implementation guidance:

- Spectator actions must not reveal hidden player locations or answer keys.
- Teacher approval is safest for hints or audience participation.
- Consider projector-only interactions for classroom use.
- Keep participation low-frequency so it does not distract active players.
- Make next-round role preference useful but non-binding unless host approves.

Relevant code:

- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHPlayerController.*`
- `Source/BlackoutHunt/BHPlayerState.*`
- `Source/BlackoutHunt/SBHClassroomBoard.*`
- `Source/BlackoutHunt/BHHUD.*`

Acceptance criteria:

- Late joiner has at least one safe thing to do.
- Active players are not given unfair location/answer information.
- Host retains control over classroom-sensitive actions.
- Spectator state still resets correctly next lobby.

### Task 18 - Progression Cosmetics Only

Use existing local XP/progress for safe cosmetic unlocks. Avoid gameplay power creep.

Possible unlocks:

- Flashlight skins
- Survivor outfits
- Teacher silhouettes
- Title cards
- Lobby poses
- Menu badges

High-value outcome: progression motivation without classroom balance problems.

Current state:

- Local account/progress layer already tracks XP, rounds, wins, escapes, and similar stats.
- Avatar preview and Quaternius character mesh options exist in the menu.
- Classroom mode can disable external account sync while keeping local profiles.

Implementation guidance:

- Keep unlocks cosmetic only.
- Local shared school PCs need reset/clear behavior to remain safe.
- Avoid requiring online login.
- Use existing character/flashlight assets first.
- Make locked/unlocked state clear but not disruptive during classroom setup.

Relevant code/assets:

- `Source/BlackoutHunt/BHAccountSubsystem.*`
- `Source/BlackoutHunt/SBHMainMenu.cpp`
- `Content/BlackoutHunt/Art/Characters/Quaternius`
- `Content/FlashLight_System`

Acceptance criteria:

- XP can unlock at least one cosmetic category or item.
- Cosmetic choice persists locally.
- No gameplay advantage is granted.
- Reset local classroom data still clears relevant profile/progression state as intended.

### Task 19 - Playtest Telemetry Heatmaps

Log local, anonymous round data for tuning. Do not collect private student data.

Useful events:

- Capture locations
- Objective stall locations
- Unused lockers
- Common wrong answers
- Exit route choices
- Jumpscare/cue frequency
- Teacher scan use
- Battery starvation

High-value outcome: map and lesson tuning based on evidence instead of guesswork.

Keep output local and teacher/developer-controlled. Consider CSV or JSON under `Saved/` with a menu/export action.

Current state:

- Classroom performance export already exists.
- Game mode already knows captures, objectives, revision attempts, round results, and many stimuli.
- Support bundle tooling already has rules about safe logs and forbidden saved account data.

Implementation guidance:

- Do not collect names, credentials, account tokens, IPs, or private student data.
- Use local anonymous/session-scoped IDs if player separation is needed.
- Store under `Saved/` and make export explicit.
- Keep file format simple: CSV or JSON.
- Start with a few high-value events rather than logging everything.
- Consider a simple coordinate normalization or map name field so data is usable across maps.

Relevant code:

- `Source/BlackoutHunt/BHGameMode.*`
- `Source/BlackoutHunt/BHGameInstance.*`
- `Source/BlackoutHunt/BHObjectiveStation.*`
- `Source/BlackoutHunt/BHPlayerState.*`
- classroom report export path in `BHGameMode.cpp`

Acceptance criteria:

- Local telemetry file records useful anonymous events.
- Export does not include private student/account/network data.
- Data can be connected to map and round phase.
- Logging has negligible gameplay overhead.
