@echo off
setlocal EnableExtensions EnableDelayedExpansion
title Blackout Hunt - Build Windows EOS .exe (EOS main, LAN/tunnel fallback)

REM ============================================================================
REM  Blackout Hunt - ONE-CLICK Windows EOS build.
REM
REM  WHAT IT DOES:
REM    Compiles and packages the CURRENT source into a runnable Windows .exe at
REM    <this folder>\Builds\WindowsEOS  using the EOS online profile:
REM      - Epic Online Services is the MAIN online path (lobby browser + P2P
REM        relay / NAT traversal, no router port-forwarding needed).
REM      - Direct-IP / LAN and the Playit internet tunnel remain available in the
REM        SAME build as fallback. The EOS net driver is configured with an
REM        IpNetDriver fallback, and the LAN / tunnel / join-code menu actions
REM        are present, so players can still connect directly if EOS is not used.
REM
REM  REQUIRES (in addition to the classroom build requirements):
REM    - Config\EOS\EOSValues.local.ini filled with your Epic Dev Portal values.
REM      Run  Tools\New-EOSLocalValues.ps1  (or copy EOSValues.example.ini) and
REM      fill ProductId / SandboxId / DeploymentId / ClientId / ClientSecret.
REM      This file is gitignored and is never shipped inside the package.
REM
REM  HOW TO USE:
REM    Double-click this file on a Windows PC that has:
REM      - Visual Studio 2022 or 2026 with "Desktop development with C++"
REM      - Unreal Engine 5.7 on this same drive (<drive>\UE_5.7)
REM    If Windows SmartScreen warns, choose "More info" -> "Run anyway".
REM
REM  OUTPUT:
REM    <this folder>\Builds\WindowsEOS\BlackoutHunt.exe  (+ DLLs, paks, EOS SDK)
REM    Copy that whole Builds\WindowsEOS folder to the target PCs, or zip it.
REM
REM  NOTE:
REM    The account-free classroom build (OnlineSubsystemNull + direct-IP/tunnel,
REM    no Epic/Steam account) is still available via BUILD-WINDOWS-CLASSROOM.cmd.
REM ============================================================================

set "PROJDIR=%~dp0"
set "DRIVE=%~d0"
cd /d "%PROJDIR%"

echo(
echo ============================================================
echo   Blackout Hunt - Windows EOS build
echo   (EOS main + LAN/tunnel fallback)
echo   Project : %PROJDIR%
echo   Drive   : %DRIVE%
echo ============================================================
echo(

REM --- Pre-flight 1: EOS credentials present? ---
if not exist "%PROJDIR%Config\EOS\EOSValues.local.ini" (
    echo [!!] Missing  Config\EOS\EOSValues.local.ini
    echo      Create it with:
    echo        powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJDIR%Tools\New-EOSLocalValues.ps1"
    echo      then fill the Epic Dev Portal values and re-run this build.
    echo(
    pause
    endlocal
    exit /b 1
)
echo [ok] EOS values file found.
echo(

REM --- Pre-flight 2: Unreal Engine 5.7 reachable on this drive? ---
if exist "%DRIVE%\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" (
    echo [ok] Unreal Engine 5.7 found at %DRIVE%\UE_5.7
) else (
    echo [!!] Unreal Engine 5.7 NOT found at %DRIVE%\UE_5.7
    echo      Tools\Find-Unreal.ps1 will also try other drives and the standard
    echo      Epic install locations. If the engine is elsewhere this may fail.
)
echo(

REM --- Pre-flight 3: validate EOS credentials before the long cook ---
echo Validating EOS credentials...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJDIR%Tools\Package-Windows-EOS.ps1" -ValidateOnly
if errorlevel 1 (
    echo(
    echo [!!] EOS credential validation failed.
    echo      Fix Config\EOS\EOSValues.local.ini and re-run.
    echo(
    pause
    endlocal
    exit /b 1
)
echo(

echo Starting EOS packaging (Shipping). This can take 20-60 minutes.
echo Leave this window open; progress is printed below.
echo ------------------------------------------------------------
echo(

powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJDIR%Tools\Package-Windows-EOS.ps1" -Configuration Shipping
set "RC=%ERRORLEVEL%"

REM --- Playability gate: boot the cooked package and prove a mode actually reaches gameplay. ---
REM This catches the whole class of "every mode bounces back to the menu" failures (a map-load crash,
REM a net driver that cannot bind a listen host, a missing cooked map) that Verify-EOSPackage cannot see
REM because they only happen at runtime. Set BH_SKIP_SMOKE=1 to bypass for a quick local iteration build.
if "%RC%"=="0" if not "%BH_SKIP_SMOKE%"=="1" (
    echo(
    echo Running packaged playability smoke test ^(host + 2 clients must reach ROUND_STARTED^)...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJDIR%Tools\Run-PackagedClassroomSmoke.ps1" -PackageRoot "%PROJDIR%Builds\WindowsEOS" -HostMode LiveFacility -ClientCount 2 -AutoQuitSeconds 45
    set "RC=!ERRORLEVEL!"
    if not "!RC!"=="0" (
        echo(
        echo [!!] PLAYABILITY SMOKE TEST FAILED: the build cooked but a mode did not reach gameplay.
        echo      See the SMOKE_REPORT.md under Saved\PackagedClassroomSmoke. DO NOT SHIP this package.
    )
)

echo(
echo ============================================================
if "%RC%"=="0" (
    echo   BUILD SUCCEEDED.
    echo   Runnable package : %PROJDIR%Builds\WindowsEOS
    echo   Launch with      : %PROJDIR%Builds\WindowsEOS\BlackoutHunt.exe
    echo   EOS is the main online path; LAN / direct-IP and the Playit tunnel
    echo   remain available in this same build as fallback.
    echo   Copy the whole Builds\WindowsEOS folder to the target PCs, or zip it.
) else (
    echo   BUILD FAILED  ^(exit code %RC%^).
    echo   Most common causes: missing Visual Studio C++ workload / Windows SDK,
    echo   or an EOS value error. Scroll up to the FIRST red error line.
)
echo ============================================================
echo(
pause
endlocal
