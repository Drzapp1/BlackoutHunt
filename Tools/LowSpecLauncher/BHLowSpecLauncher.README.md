# Low-Spec Launcher (IT/developer fallback)

`BHLowSpecLauncher.cpp` is a tiny, no-console Windows launcher for locked-down school PCs that
block `.cmd` / console execution. Double-clicking it starts `BlackoutHunt.exe` from the **same
folder** with low-spec / DX11 / windowed arguments:

```
-d3d11 -BHVirtualBoxSafe -ResX=1280 -ResY=720 -WINDOWED
```

These are the exact arguments validated by the LT2 launcher prototype
(`Docs/LIVE_CLASSROOM_TEST_2026-05-29.md`, task LT2).

## Why this exists

A weak machine that *reaches the menu* already gets a safe profile automatically — the game
auto-applies 1280x720 windowed and a conservative graphics preset on first launch when it detects
integrated/software graphics (`ApplyStartupGraphicsSettings` in `BHPlayerController.cpp`). In-game
Settings then let students stay on `Low 4GB` / 720p.

The launcher only matters for machines that **fail before the menu** because the engine cannot
pick a usable renderer at default settings. Forcing `-d3d11` (and windowed/720p) at process start
is something in-engine Settings cannot do, and the packaged `.cmd` launchers do it but may be
blocked by a console-execution policy.

It does **not** help machines that expose only Microsoft Basic Display Adapter, Remote Desktop
software graphics, an unsupported VM graphics path, or a pre-DX11 GPU. Those cannot meet Unreal's
Direct3D feature level 11.0 / Shader Model 5.0 requirement and are out of scope for this package.

## Design constraints (do not regress these)

- **No console window:** build as a GUI-subsystem app (`WinMain`, `/SUBSYSTEM:WINDOWS`).
- **No absolute paths:** the launcher resolves `BlackoutHunt.exe` relative to its own location,
  so it survives zip/extract into any folder. (The earlier `.lnk` approach was rejected because
  it baked absolute paths.)
- **No admin rights:** it just `CreateProcess`es a sibling exe.

## Build (developer machine)

With MSVC tools available (Developer Command Prompt / `cl`):

```bat
cl /nologo /O2 /EHsc /DUNICODE /D_UNICODE BHLowSpecLauncher.cpp ^
   /Fe:BHLowSpecLauncher.exe /link /SUBSYSTEM:WINDOWS user32.lib
del BHLowSpecLauncher.obj
```

Place the resulting `BHLowSpecLauncher.exe` next to `BlackoutHunt.exe` in the extracted package.

## Required validation before shipping in a classroom package

Per the LT2/LT9 decisions, do **not** auto-stage an unbuilt or unvalidated binary into a release,
and do not add it to the frozen archive unless a real launch blocker is confirmed and a new
archive + sidecar hash + `Verify-ClassroomPackage.ps1` pass are produced. Before relying on it:

1. Build the exe as above.
2. Copy it beside `BlackoutHunt.exe` in a clean extracted package.
3. On the **actual school Windows image / locked-down student account**, double-click it and confirm:
   - no console window appears,
   - no administrator prompt appears,
   - the game launches windowed at 1280x720 on DX11 and reaches the menu.
4. Record the result (machine label, outcome) in the live-classroom feedback sheet.

Only once it passes step 3 on the real image should packaging/staging integration
(`Tools\Package-Windows-Classroom.ps1`) and a corresponding presence check in
`Tools\Verify-ClassroomPackage.ps1` be added.
