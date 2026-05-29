# Linux Build

Blackout Hunt can be packaged for Linux from the existing Windows UE 5.7 install, but that UE install must include Epic's `platform_Linux` optional component.

## Current Machine

The UE install at `D:\UE_5.7` has the Windows editor and the `platform_Linux` optional payload installed. The manifest says that component is about 16.5 GiB installed, with about 30,828 files, and `Tools/Check-Unreal-Linux-Platform.sh` currently reports zero missing Linux platform files.

This is still a Windows UE install, so native Linux host build files such as `Engine/Build/BatchFiles/Linux/SetupEnvironment.sh` are not expected to be present. From this Linux machine, package with the Wine flow below instead of the native `RunUAT.sh` flow.

## Run Current Windows Build On Linux

The existing Windows package can run on this Linux machine through Wine when D3D11 is routed through DXVK:

```sh
./Tools/Run-Windows-Build-Wine.sh
```

This uses a separate Wine prefix at `$HOME/.local/share/wineprefixes/blackouthunt`, installs DXVK from Heroic's local DXVK cache when available, and launches the real packaged executable at `Builds/Windows/BlackoutHunt/Binaries/Win64/BlackoutHunt.exe`. Set `BLACKOUTHUNT_WINEPREFIX=/path/to/prefix` if you want a different prefix.

Do not use `-vulkan` with the current Windows package. The package was not cooked with Windows Vulkan shader formats, so Unreal will stop with:

```text
Unable to launch with RHI 'Vulkan' since the project is not configured to support it.
```

That is a packaging/configuration limitation of the Windows build, not a Linux Vulkan driver problem.

## Check Status

From Linux:

```sh
./Tools/Check-Unreal-Linux-Platform.sh
```

This reads the local Epic manifest only. It does not log in, download, or modify the engine.

## Install Missing UE Files

When Epic access is available again, authenticate Heroic/Legendary, then install the missing tag:

```sh
./Tools/Install-Unreal-Linux-Platform.sh
```

The script uses Heroic's bundled Legendary CLI when a standalone `legendary` command is not installed. It installs only the `platform_Linux` tag from the existing UE 5.7 manifest into `D:\UE_5.7`.

## Package From This Linux Machine

After `platform_Linux` is installed:

```sh
./Tools/Package-Linux-Wine.sh
```

This runs the Windows `RunUAT.bat` through Wine and archives the package to `Builds/Linux`.

### Faster Cooks

The package phase is dominated by C++ compilation and, on a cold run, shader
compilation. `Package-Linux-Wine.sh` exposes three knobs to cut that time:

| Env var | Default | Effect |
| --- | --- | --- |
| (built in) | on | Cooked shaders/DDC persist in `Builds/DerivedDataCache` between runs via `-ddc=InstalledNoZenLocalFallback` + `UE-LocalDataCachePath`, so a warm cook skips the ~250s cold shader compile. Set `BLACKOUTHUNT_DDC=/path` to relocate. |
| `BUILD_PARALLEL` | `nproc` | UnrealBuildTool compile parallelism. The previous value was a serial `2`; the default now matches the core count. Lower it (`BUILD_PARALLEL=2`) if parallel clang actions exhaust RAM on a low-memory host. |
| `ITERATE` | `0` | `ITERATE=1` re-cooks only changed assets (`-iterate`). Big win for repeat dev cooks; leave off for release/distribution packages. |

Example fast iteration cook:

```sh
BUILD_PARALLEL=4 ITERATE=1 ./Tools/Package-Linux-Wine.sh
```

The first run after this change still pays the cold shader compile (Linux uses
different shader formats than the Windows cook, so the warmed Windows DDC does
not carry over); subsequent Linux cooks reuse the warmed Linux entries.

The Windows packaging scripts mirror these knobs: `Tools/Package-Windows.ps1`
accepts `-MaxParallel <n>` (default = processor count, was a serial `1`) and
`-Incremental` (skip the `-clean` full rebuild and cook iteratively), and also
honors `BUILD_PARALLEL`.

Linux classroom packages use the default `OnlineSubsystemNull` direct-IP path. The
project's EOS plugins are kept Win64-only because the current UE 5.7 Windows
install does not provide Linux-ready `OnlineSubsystemEOS`/`SocketSubsystemEOS`
runtime binaries.

## Native Linux UE Alternative

If a native Linux Unreal Engine 5.7 install is available, set `UE_ROOT` to it and use:

```sh
UE_ROOT=/path/to/UE_5.7 ./Tools/Package-Linux.sh
```

The current `D:\UE_5.7` tree is a Windows UE install, so it is not expected to work with the native `RunUAT.sh` flow.
