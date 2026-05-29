param(
    [string[]]$Levels = @("Facility", "Substation", "Foggrounds"),
    [switch]$BuildFirst
)

# Seeds authored level .umap assets from the runtime generator by running the BHExportLevel commandlet
# for each level. The result is /Game/BlackoutHunt/Maps/<Level> (Content/BlackoutHunt/Maps/<Level>.umap),
# preserving the tuned blockout so artists replace blocks with meshes in place. Editor must be compiled
# first (use -BuildFirst, or run Tools/Build-Editor.ps1). See Docs/FACILITY_VERTICAL_SLICE.md.

$ErrorActionPreference = "Stop"

$project = Resolve-Path "$PSScriptRoot\..\BlackoutHunt.uproject"
$unreal = & "$PSScriptRoot\Find-Unreal.ps1"

if ($BuildFirst) {
    Write-Host "Building editor target before export ..."
    & "$PSScriptRoot\Build-Editor.ps1"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$editorCmd = $unreal.Editor -replace "UnrealEditor\.exe$", "UnrealEditor-Cmd.exe"
if (-not (Test-Path $editorCmd)) {
    Write-Error "UnrealEditor-Cmd.exe not found at $editorCmd"
    exit 1
}

$valid = @("Facility", "Substation", "Foggrounds")
foreach ($level in $Levels) {
    if ($valid -notcontains $level) {
        Write-Error "Invalid level '$level'. Use Facility, Substation, or Foggrounds."
        exit 1
    }
}

foreach ($level in $Levels) {
    Write-Host "==> Exporting authored seed for $level ..."
    & $editorCmd "$project" -run=BHExportLevel -Level=$level -stdout -unattended -nopause -nosplash
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Export failed for $level (exit $LASTEXITCODE)."
        exit $LASTEXITCODE
    }
}

Write-Host ""
Write-Host "Done. Authored map seeds written under Content\BlackoutHunt\Maps\."
Write-Host "Next per map: open it, replace tinted blocks with the mesh kit in place, add baked lighting and a"
Write-Host "NavMeshBoundsVolume, set the ABHLevelMarker's bRebuildRuntimeNavigation = false, then save."
Write-Host "Before packaging: add +DirectoriesToAlwaysCook=(Path=`"/Game/BlackoutHunt/Maps`") to"
Write-Host "Config\DefaultGame.ini, update Docs\ASSETS.md, and run Tools\Verify-ClassroomPackage.ps1."
Write-Host "Enable bUseAuthoredLevels only after all target maps exist and travel is validated."
