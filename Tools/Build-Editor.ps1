$ErrorActionPreference = "Stop"

$project = Resolve-Path "$PSScriptRoot\..\BlackoutHunt.uproject"
$unreal = & "$PSScriptRoot\Find-Unreal.ps1"

$buildDir = Split-Path $unreal.Build
Push-Location $buildDir
try {
    & $unreal.Build BlackoutHuntEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -notinstalledengine -NoXGE -MaxParallelActions=2
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}
