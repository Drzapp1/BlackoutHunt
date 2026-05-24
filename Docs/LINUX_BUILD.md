# Linux Build

Blackout Hunt can be packaged for Linux from the existing Windows UE 5.7 install, but that UE install must include Epic's `platform_Linux` optional component.

## Current Machine

The UE install at `D:\UE_5.7` has the Windows editor and the Linux target platform metadata, but the `platform_Linux` optional payload is missing. The manifest says that component is about 16.5 GiB installed, with about 30,828 files.

Without Epic access, the project cannot download that payload. The scripts in `Tools` now stop before download/auth work and explain the missing piece.

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

## Native Linux UE Alternative

If a native Linux Unreal Engine 5.7 install is available, set `UE_ROOT` to it and use:

```sh
UE_ROOT=/path/to/UE_5.7 ./Tools/Package-Linux.sh
```

The current `D:\UE_5.7` tree is a Windows UE install, so it is not expected to work with the native `RunUAT.sh` flow.
