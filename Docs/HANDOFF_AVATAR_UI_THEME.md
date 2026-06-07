# Handoff — Avatar customization & UI theme system

**Date:** 2026-06-07  ·  **Branch:** `feature/easter-eggs-2026-06-06`  ·  **Project:** `D:\BlackoutHunt` (UE 5.7, Windows)

This brief lets a fresh agent continue the avatar-customization + menu-theming work without re-deriving context. Read it top to bottom, then start at **§4 Open work**.

---

## 1. What this is

Blackout Hunt is a classroom IGCSE-physics teaching tool + asymmetric horror game. Recent work has been on the **main-menu Character tab**: avatar customization (per-part colours, headwear) and a new **UI theme** system (menu colour schemes). Everything below is **committed** on the branch.

## 2. Build / run / test loop

All commands run from `D:\BlackoutHunt` (the Bash tool resets cwd to `c:\psim-v2`, so prefix with `cd /d/BlackoutHunt &&`).

- **Build (editor target):** `pwsh -NoProfile -File Tools/Build-Editor.ps1`
  - Background-safe. Last line prints `Result: Succeeded` or `Result: Failed`. To capture only errors: pipe to `grep -iE "error C[0-9]|error LNK|: error|Result:"`.
  - New `.cpp`/`.h` in `Source/BlackoutHunt/` are auto-globbed by UBT — no `.uproject` edit needed.
- **Run (standalone game):** launch via `Tools/Find-Unreal.ps1` →
  `Start-Process <Editor> -ArgumentList '"D:\BlackoutHunt\BlackoutHunt.uproject"','-game','-windowed','-resx=1600','-resy=900','-log'`
  - Boots to the main menu in ~5–120 s (off the editor binary, uncooked assets). Poll the process `MainWindowTitle -like 'BlackoutHunt*'` + `Responding` for menu-ready.
- **MUST close the running instance before rebuilding** (it locks `UnrealEditor-BlackoutHunt.dll`): `Get-Process -Name UnrealEditor | Stop-Process -Force`.
- **Dev unlock:** press **F9 at the menu** → `BHUnlockAllCosmetics` (unlocks every cosmetic for testing; compiled out of Shipping). The engine **console does not open at the menu** (UI-only input mode); it works in-round.
- **Commits:** plain `git commit` works on this repo (no `--no-verify` needed). Footer every message with `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`.

## 3. Done this session (committed, newest first)

- `ba7a237` — theme buttons restyled (colour swatch + readable text; they read as grayed before but were always enabled); larger avatar preview (camera 212→150, model 0.56→0.66).
- `373a2ad` — **UI theme system BEGUN**: `BHMenuTheme.h/.cpp` (5 presets), picker in Character tab, 2 menu backgrounds routed; feedback web-app URL updated; `Code.gs` now emails `survey` kinds.
- `4bfafa7` — recolour parts rebuild on outfit switch; raised glasses/visor.
- `19d21b7` — **per-part avatar colours**: de-flatten authored colours + per-slot recolour (replicated), stretch-preview fix, removed broken procedural hats from the picker, "Shirt"→"Nameplate" relabel, F9 dev unlock, head-bone hats.

> ⚠️ A **second agent is editing the same files in parallel** (comfort options like `bHideVeryClosePlayers`/`SetProximityHiddenLocal`, menu text-resizing, train-roof actors, etc.). Expect `Edit` "modified since read" — just re-Read and re-apply with exact-match. Some commits bundle that agent's interleaved work (the tree only commits cleanly as a whole because of cross-file deps).

## 4. Open work (priority order)

### A. Full UI-theme colour routing  ← the main remaining theme task
The theme **foundation is done**; only 2 colours are routed (proof-of-pipeline). Route the rest of `SBHMainMenu.cpp`'s hundreds of hardcoded colours through `BHActiveMenuTheme()` **by role**.
- Roles available on `FBHMenuTheme` (see `BHMenuTheme.h`): `Background, Panel, PanelBorder, Header, TextPrimary, TextDim, Accent, AccentBright, ButtonIdle, ButtonHover, Danger`.
- Find sites: `.BorderBackgroundColor(FLinearColor(...))`, `.ColorAndOpacity(FLinearColor(...))`, `.BorderImage`/tints, etc. Classify each (panel bg → `Panel`, header text → `Header`, accents → `Accent`, …).
- **Binding form matters:**
  - Inside a **member function** of `SBHMainMenu` → `.BorderBackgroundColor(this, &SBHMainMenu::GetThemePanelColor)` (member getters `GetThemeBackgroundColor()`/`GetThemePanelColor()` already exist; add more per role).
  - Inside a **file-local builder function** (anonymous namespace) → **no `this`**; use a lambda:
    `.BorderBackgroundColor(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda([]() { return FSlateColor(BHActiveMenuTheme().Panel); })))`
    (This was the C2355/"`this` can't be referenced" trap — two routed lines were in file-local functions.)
- Dynamic bindings re-read each repaint, so switching themes is **instant** (no menu rebuild needed). Prefer them over static reads.
- This is **single-file & sequential** — do NOT fan out parallel agents over `SBHMainMenu.cpp` (they collide). One careful pass.

### B. Theme persistence
The active theme is a file-static (`GActiveMenuThemeIndex` in `BHMenuTheme.cpp`) — **resets on restart**. Persist it:
- Add `int32 SelectedThemeIndex = 0;` to `FBHAccountProgress` (`BHAccountSubsystem.h`), serialize in `ProgressToJson`/`ApplyProgressJson` (`BHAccountSubsystem.cpp`, key e.g. `selected_theme_index`) — mirror `SelectedTitleIndex`.
- On menu construct (and on account load), call `BHSetActiveMenuThemeIndex(Progress.SelectedThemeIndex)`. In `OnThemeClicked`, also write it to the account + save.
- Themes are **local UI only — do NOT replicate** (unlike avatar cosmetics).
- Optional: unlock-gate themes like cosmetics (XP/achievement). Not required.

### C. Character-panel layout rebalance (+ headwear alignment)
**Symptom:** preview is small/low, content huddled in the left ~⅓, big empty space on the right.
- The Character tab is a horizontal split: a narrow LEFT preview column + the customization content. Restructure so the **preview is large** (use the right half) and content reflows. This is real layout surgery in `BuildCharacterCustomizationPanel` / the tab's container — not a one-value tweak.
- Preview internals: render target **512×768 (2:3)** captured by a `USceneCaptureComponent2D` in `EnsureAvatarPreviewScene()`; displayed via `SImage` wrapped in `SScaleBox(ScaleToFit)` (the earlier stretch fix). Tunables near top of `SBHMainMenu.cpp`: `MenuAvatarPreviewModelScale=0.66`, `MenuAvatarPreviewCameraDistance=150`, `…CameraHeight=46`, `…CameraFov=34`.
- **Headwear still looks misplaced** and is almost certainly **downstream of this**: glasses/visor sit at fixed offsets tuned for a normally-framed avatar, but the preview renders it small/low. Fix the framing first, then re-check headwear. Positions: in-game `BHCharacter.cpp` `HeadwearIndex == 2` (glasses, Z=72) / `== 4` (visor, Z 67–77); menu preview `MenuApplyPreviewPiece` `case 2`/`case 4` in `SBHMainMenu.cpp`.

### D. Black colour option (recolour + nameplate swatches)
Swatch rows currently show 8 colours from `MenuAvatarPaletteColor(0..7)` — **no black**. Adding black is non-trivial: the recolour pipeline maps a colour index → a Quaternius **palette material** via `BHQuaterniusPaletteMaterial` (8 `MaterialPaths`: LightBlue/Brown2/Green/Red/Purple/Gold/LightGreen/White — **no Black**). The skins DO have a `Black` material. Options: add a Black entry to the palette + material list (mind that palette indices are persisted/replicated as `colourIndex+1`, 1..18, so **append, don't reorder**), or special-case a "black" path in the apply.

## 5. Key systems & files

**UI theme**
- `Source/BlackoutHunt/BHMenuTheme.h/.cpp` — `FBHMenuTheme`, 5 presets (Classic/Midnight/Crimson/Emerald/Arcade), `BHActiveMenuTheme()/BHSetActiveMenuThemeIndex()/BHActiveMenuThemeIndex()/BHMenuThemeCount()/BHMenuThemeAt()/BHMenuThemeName()`.
- `SBHMainMenu.cpp` — `BuildThemeSection()` (picker, in Character tab after the recolour section), `OnThemeClicked(int32)`, `GetThemeBackgroundColor()/GetThemePanelColor()`. Decls in `SBHMainMenu.h`.

**Per-part avatar colour (done — reference)**
- Registry: `BHColorableMaterialNames()/BHColorableMaterialIndex()/BHColorableMaterialCount()` in `BHCosmeticUnlocks.h/.cpp` (append-only; index is persisted/replicated key).
- Replicated `TArray<uint8> AvatarSlotColors` on `ABHPlayerState` (0 = authored default, else palette index+1). Account `FBHAccountProgress::AvatarSlotColors` + JSON + `SetAvatarSlotColor`.
- Apply (match by **slot name** via `GetMaterialSlotNames()`, fallback to material name): `BHApplyQuaterniusPalette` (`BHCharacter.cpp`, in-game) and `MenuApplyQuaterniusPalette` (`SBHMainMenu.cpp`, preview). Default leaves authored colours (the "de-flatten").
- Picker: `BuildRecolorSection()` (`SBHMainMenu.cpp`) — rebuildable via `RecolorSectionBox` (SBox), refreshed in `OnAvatarPresetClicked`. Friendly labels via `MenuColorablePartLabel`, swatches via `MenuColorablePartColor`.
- The avatar palette colours: `BHAvatarPaletteColor(int32)` (`BHCharacter.cpp`, 18 entries: 8 base + 10 prestige tints) and menu mirror `MenuAvatarPaletteColor`.

**Skins:** `BHSelectQuaterniusMeshPath(BHPS)` (`BHCharacter.cpp`) + menu mirror `MenuAvatarMeshPath`. The Quaternius skins bake hats into shared-material geometry (why procedural cosmetic hats were removed from the picker). `BHShouldPreserveQuaterniusMaterial` / `MenuShouldPreserveQuaterniusMaterial` shields Skin/Hair/Eye/Eyebrow/Moustache from recolour.

**Feedback (done — config):** game POSTs to a Google Apps Script (`FeedbackBackendBaseUrl` in `Config/DefaultGame.ini`, now the user's new `/exec?path=` deployment). `Tools/AccountBackend/apps-script/Code.gs` logs all feedback to a Sheet and emails kinds in `EMAIL_KINDS` — now includes `'survey'` (end-of-round polls). **The user must redeploy the Apps Script** for that to take effect (the live version is Google-hosted, not the local file).

## 6. Gotchas / constraints

- **Concurrent agent** (see §3) — re-Read on "modified since read"; exact-match edits.
- **Do NOT touch** the stray `D:\BlackoutHunt\BlackoutHunt\` duplicate tree or `Tools/cook-progress-bar.sh` (owner's call). Stage specific paths (`git add Source/ Config/ Docs/ …`), never `-A`.
- Real EOS/.env/secrets are gitignored — never echo/commit them.
- `SBHMenuButton` = the custom menu button. Click+payload: `.OnClicked(this, &SBHMainMenu::Method, intPayload)`. No `IsEnabled` needed (defaults enabled).
- Slate exact-match edits in deeply nested Slate are tab-sensitive — reveal tabs with PowerShell `($line -replace "\`t","<T>")` when an `Edit` fails.

## 7. Current todo (carry over)

1. Full UI-theme colour routing (§4A).
2. Theme persistence + optional unlock-gating (§4B).
3. Character-panel layout rebalance + headwear alignment (§4C).
4. Black colour option (§4D).
5. (User action) Redeploy the Apps Script so survey emails start.
