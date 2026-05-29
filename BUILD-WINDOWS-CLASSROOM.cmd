@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Blackout Hunt - Build Windows Classroom .exe

REM ============================================================================
REM  Blackout Hunt - ONE-CLICK Windows build.
REM
REM  WHAT IT DOES:
REM    Compiles and packages the CURRENT source on this drive into a runnable
REM    Windows .exe at  <this folder>\Builds\Windows  (Classroom Shipping).
REM    Because it compiles the source that is on the drive right now, the
REM    resulting .exe contains ALL the latest changes.
REM
REM  HOW TO USE:
REM    Double-click this file on a Windows PC that has:
REM      - Visual Studio 2022 or 2026 with "Desktop development with C++"
REM      - The Unreal Engine 5.7 on this same drive (<drive>\UE_5.7)
REM    (Unreal bundles its own .NET, so no separate .NET install is needed.)
REM    If Windows SmartScreen warns, choose "More info" -> "Run anyway".
REM
REM  OUTPUT:
REM    <this folder>\Builds\Windows\BlackoutHunt.exe  (+ DLLs, paks, launchers)
REM    Copy that whole Builds\Windows folder to the target PCs, or zip it.
REM ============================================================================

set "PROJDIR=%~dp0"
set "DRIVE=%~d0"
cd /d "%PROJDIR%"

echo(
echo ============================================================
echo   Blackout Hunt - Windows Classroom build
echo   Project : %PROJDIR%
echo   Drive   : %DRIVE%
echo ============================================================
echo(

REM --- Pre-flight 1: Unreal Engine 5.7 reachable on this drive? ---
if exist "%DRIVE%\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" (
    echo [ok] Unreal Engine 5.7 found at %DRIVE%\UE_5.7
) else (
    echo [!!] Unreal Engine 5.7 NOT found at %DRIVE%\UE_5.7
    echo      Tools\Find-Unreal.ps1 will also try D: and the standard Epic install
    echo      locations. If the engine is somewhere else this build will fail.
)
echo(

REM --- Pre-flight 2: Visual Studio C++ toolchain present? ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "HAVE_VC="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`""%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath" 2^>nul`) do set "HAVE_VC=%%I"
)
if defined HAVE_VC (
    echo [ok] Visual Studio C++ toolchain: !HAVE_VC!
) else (
    echo [!!] WARNING: no Visual Studio C++ toolchain detected.
    echo      Install Visual Studio 2022/2026 with the "Desktop development with
    echo      C++" workload, then re-run. Continuing in case vswhere is just absent...
)
echo(

echo Starting packaging (Classroom Shipping). This can take 20-60 minutes.
echo Leave this window open; progress is printed below.
echo ------------------------------------------------------------
echo(

powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJDIR%Tools\Package-Windows-Classroom.ps1"
set "RC=%ERRORLEVEL%"

echo(
echo ============================================================
if "%RC%"=="0" (
    echo   BUILD SUCCEEDED.
    echo   Runnable package : %PROJDIR%Builds\Windows
    echo   Launch with      : %PROJDIR%Builds\Windows\BlackoutHunt.exe
    echo   Copy the whole Builds\Windows folder to the target PCs, or zip it.
) else (
    echo   BUILD FAILED  ^(exit code %RC%^).
    echo   Most common cause: the Visual Studio C++ workload or Windows SDK is
    echo   missing. Scroll up to the FIRST red error line for the real cause.
)
echo ============================================================
echo(
pause
endlocal
