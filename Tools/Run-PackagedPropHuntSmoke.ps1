# Copyright (c) 2026 Adam Rosta. All Rights Reserved.
# This source code is proprietary and confidential.
# Unauthorized copying or distribution is strictly prohibited.
#
# Run-PackagedPropHuntSmoke.ps1 -- the packaged 3-client PROP HUNT smoke (P9).
#
# A thin wrapper over Run-PackagedClassroomSmoke.ps1 with the prop-hunt auto-host mode: it boots a packaged
# listen host that boards the train lobby with ?BHPropHunt=1 armed (-BHAutoHost=PropHuntFacility /
# PropHuntContainersHouse via BHAutomationSupport), joins N clients, auto-readies everyone, departs to the
# arena/map, and gates on the same automation markers as the classroom smoke (HOST_LISTENING -> JOINED ->
# READY_SET -> ROUND_STARTED -> CLEAN_QUIT) plus the runtime-log error scan. ROUND_STARTED fires on the
# prop-hunt HIDE phase, so a pass means the full host->lobby->arena->hide pipeline works in the cooked build.
#
# Usage (after a cook, e.g. .\Tools\Package-Windows-EOS.ps1):
#   .\Tools\Run-PackagedPropHuntSmoke.ps1                          # prop hunt on Facility (no pack content needed)
#   .\Tools\Run-PackagedPropHuntSmoke.ps1 -Arena ContainersHouse   # prop hunt on the cooked warehouse arena
#   .\Tools\Run-PackagedPropHuntSmoke.ps1 -PackageRoot ..\Builds\WindowsEOS\Windows
#
# Exit code 0 = pass. The full report lands in Saved\PackagedClassroomSmoke\<timestamp>\SMOKE_REPORT.md.

param(
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\Windows",
    [ValidateSet("Facility", "ContainersHouse")]
    [string]$Arena = "Facility",
    [int]$ClientCount = 2,
    [int]$Port = 7777,
    [switch]$VirtualBoxSafe,
    [switch]$ShowWindows,
    [string[]]$RuntimeLogAllowedPattern = @()
)

$ErrorActionPreference = "Stop"

$hostMode = if ($Arena -eq "ContainersHouse") { "PropHuntContainersHouse" } else { "PropHuntFacility" }

$smokeArgs = @{
    PackageRoot = $PackageRoot
    HostMode = $hostMode
    ClientCount = $ClientCount
    Port = $Port
    # Prop hunt fronts a ~30s hide phase before the seek; give the shared auto-quit window room for
    # lobby + travel + hide + a slice of the seek so CLEAN_QUIT still fires from a healthy process.
    AutoQuitSeconds = 210
    RoundStartTimeoutSeconds = 150
}
if ($VirtualBoxSafe) { $smokeArgs.VirtualBoxSafe = $true }
if ($ShowWindows) { $smokeArgs.ShowWindows = $true }
if ($RuntimeLogAllowedPattern.Count -gt 0) { $smokeArgs.RuntimeLogAllowedPattern = $RuntimeLogAllowedPattern }

& (Join-Path $PSScriptRoot "Run-PackagedClassroomSmoke.ps1") @smokeArgs
exit $LASTEXITCODE
