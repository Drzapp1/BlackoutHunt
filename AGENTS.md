# Blackout Hunt Agent Playbook

These instructions apply to the whole repository. Use them as the first stop for Codex or any other agent working in this project.

## Core Rules

- Before building new gameplay visuals or placeholder props, search `Content/` for relevant imported Epic/Marketplace packages and use those assets when they fit the feature. Prefer existing imported meshes, materials, effects, sounds, and blueprints over procedural cubes or new stand-ins.
- Keep native gameplay logic in C++ when that is how the system is implemented, but wire the C++ actors to the best available imported assets with safe fallbacks for missing package content.
- The worktree may already contain unrelated user changes. Do not revert, overwrite, or clean up changes you did not make unless the user explicitly asks.

## Start Here

- Confirm the current task against `README.md` and, for larger work, `Docs/CODEX_GAME_IMPROVEMENT_TASKS.md`.
- Run `git status --short` before editing so you know what was already dirty.
- Inspect the relevant C++ classes, config entries, docs, and assets before designing new behavior.
- Prefer the smallest change that fits existing systems and keeps packaged classroom builds safe.
- If a task touches visuals, audio, packages, or runtime references, check `Docs/ASSETS.md` before editing.
- Work from the repository root unless a tool script says otherwise. Most Windows helpers are PowerShell scripts under `Tools`.

## Project Context

- Blackout Hunt is an Unreal Engine 5.7 direct-IP multiplayer classroom horror hunt prototype.
- Current beta target is `0.2.0-beta.6`; classroom Windows packaging is the primary release path.
- Main project paths:
  - `Source/BlackoutHunt`: native gameplay, UI, networking, automation tests, and module rules.
  - `Content`: imported assets, project art/audio, runtime maps, external actors, and package-sensitive content.
  - `Config`: gameplay tuning, input, scalability, cook/stage rules, and classroom defaults.
  - `Docs`: roadmap, deployment, tuning, assets, maintainability, release notes, and task handoff context.
  - `Tools`: build, package, import, validation, support-bundle, and maintenance scripts.
- Important reference docs include `README.md`, `Docs/ASSETS.md`, `Docs/CLASSROOM_DEPLOYMENT.md`, `Docs/TUNING.md`, `Docs/FACILITY_VERTICAL_SLICE.md`, `Docs/MAINTAINABILITY.md`, and `Docs/CODEX_GAME_IMPROVEMENT_TASKS.md`.

## Change Routing

- Gameplay, roles, objectives, bots, match flow, and replication usually belong in `Source/BlackoutHunt`.
- Menu, HUD, classroom board, and host/student UI work usually routes through Slate code in `Source/BlackoutHunt`, especially `SBHMainMenu` and related widgets.
- Tuning and defaults usually belong in `Config/DefaultGame.ini`; document operator-facing changes in `Docs/TUNING.md`.
- Packaging, classroom deployment, Linux/Wine, and support-bundle work usually belongs in `Tools` plus the matching `Docs` page.
- Asset import or cook/stage changes must be reflected in `Docs/ASSETS.md` and the relevant config or import script.
- Avoid editing `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, generated build output, or package output unless the user explicitly asks for generated artifacts.

## Implementation Guidance

- Read the relevant existing C++ classes before adding new systems. This repo already has many gameplay systems under `Source/BlackoutHunt`, so avoid duplicating behavior under a new name.
- Prefer small, scoped C++ changes that match the local Unreal patterns for actors, components, replicated state, Slate UI, console commands, settings, and automation tests.
- Keep gameplay authority and replication explicit. Host/admin controls must remain host-only, and student clients should not receive teacher/admin/test affordances through UI or gameplay shortcuts.
- Preserve accessibility and comfort settings. New scares, flashes, camera shakes, captions, HUD contrast, or audio pressure should respect the reduced jumpscare, reduced flash, reduced camera shake, captions, and high-contrast settings where relevant.
- When changing tuning values, prefer `Config/DefaultGame.ini` under `[/Script/BlackoutHunt.BHGameSettings]` when the setting already belongs there. Update `Docs/TUNING.md` for player- or operator-facing tuning changes.
- For UI work, keep normal student views separate from host-only classroom, role assignment, tunnel, network admin, and test controls.
- For refactors, move one responsibility at a time and preserve behavior unless the task explicitly asks for behavior changes.
- Prefer soft references, guarded asset loading, and explicit fallbacks for optional content. Avoid hard failures when a Marketplace or Fab package is absent locally.
- Keep logs and automation markers stable when possible; docs and packaged validation workflows may depend on them.

## Assets, Licensing, and Packaging

- Search existing assets first. Useful commands:
  - `rg --files Content | rg -i "term"`
  - `Get-ChildItem -Recurse Content`
- Prefer package-safe assets already documented in `Docs/ASSETS.md`, including project-local BlackoutHunt assets, ambientCG materials, Quaternius avatars, KayKit props, generated/in-house audio, and approved runtime jumpscare folders.
- Keep source packs, downloaded archives, raw DCC/source files, demo maps, and unverified assets out of staged builds unless the docs and cook rules explicitly allow them.
- Consult `Docs/ASSETS.md` before enabling new cook paths or runtime asset references. Respect the documented "always cook" and "never cook" policy in `Config/DefaultGame.ini`.
- Be careful with risky character or IP-derived assets. Do not include Hider, Hunter, source jumpscare pack roots, downloaded source folders, or assets without license evidence in classroom packages.
- When a feature depends on imported content, use soft references or runtime lookup with safe C++ fallbacks so the game still functions when optional assets are absent.
- If asset, cook, stage, package, or distribution behavior changes, update the relevant docs and run package verification.

## Classroom and Network Safety

- The classroom build is designed for school-safe hosting. Keep listen-server host controls, preflight details, tunnel controls, force-start/test shortcuts, role assignment, and kick/admin actions restricted to the host machine.
- Do not expose private account/profile data in classroom exports, logs, support bundles, or student-facing screens.
- Preserve Live Classroom defaults unless a task explicitly changes them: local classroom profile, host roster, ready checks, Playit/loopback support, classroom board, mastery goals, and student-safe controls.
- Network changes should consider direct LAN, loopback classroom hosting, Playit tunnel join codes, `OnlineSubsystemNull` local testing, and future EOS/Steam subsystem configuration.

## Validation

Choose validation based on the files and behavior touched:

- C++ or module changes: `.\Tools\Build-Editor.ps1`
- Package/cook/stage/assets changes: `.\Tools\Verify-ClassroomPackage.ps1`
- Release candidate or classroom packaging changes: `.\Tools\Package-Windows-Classroom.ps1`
- Classroom support or preflight changes: `.\Tools\New-ClassroomSupportBundle.ps1`
- Maintainability/refactor checkpoints: `.\Tools\New-CodeHealthSnapshot.ps1`
- Broad stability-sensitive changes: `.\Tools\Run-StabilityGate.ps1`
- Shared gameplay, networking, classroom flow, reports, bots, jumpscare variants, train economy, command-line automation, or package tooling: run targeted Unreal automation tests when available.

For documentation-only changes, a focused file review and `git status --short -- AGENTS.md` are enough unless the docs describe changed behavior that should be verified separately.

## Before Handoff

- Summarize what changed and call out any validation that was not run.
- List asset paths, cook/config changes, and fallback behavior when relevant.
- Mention any unrelated dirty files you noticed but did not touch.
- Keep final notes concise and include exact commands or paths a follow-up agent needs.
