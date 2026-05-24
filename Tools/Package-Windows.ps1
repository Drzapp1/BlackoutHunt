param(
    [switch]$Classroom,
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path "$PSScriptRoot\.."
$project = Resolve-Path "$projectRoot\BlackoutHunt.uproject"
$archive = Join-Path $projectRoot "Builds\Windows"
$unreal = & "$PSScriptRoot\Find-Unreal.ps1"

if (-not $Configuration) {
    if ($Classroom) {
        $Configuration = "Shipping"
    }
    else {
        $Configuration = "Development"
    }
}

if ($Classroom) {
    $resolvedRoot = [System.IO.Path]::GetFullPath($projectRoot)
    $resolvedArchive = [System.IO.Path]::GetFullPath($archive)
    if (-not $resolvedArchive.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean archive outside project root: $resolvedArchive"
    }
    if (Test-Path -LiteralPath $archive) {
        Remove-Item -LiteralPath $archive -Recurse -Force
    }
}

New-Item -ItemType Directory -Force -Path $archive | Out-Null

$uatDir = Split-Path $unreal.RunUAT
Push-Location $uatDir
try {
    $uatArgs = @(
        "BuildCookRun",
        "-project=$project",
        "-notinstalledengine",
        "-noP4",
        "-platform=Win64",
        "-clientconfig=$Configuration",
        "-serverconfig=$Configuration",
        "-cook",
        "-map=/Engine/Maps/Entry",
        "-build",
        "-noxge",
        "-ubtargs=-WaitMutex -NoXGE -MaxParallelActions=2",
        "-stage",
        "-pak",
        "-archive",
        "-archivedirectory=$archive"
    )
    if ($Classroom) {
        $uatArgs += "-distribution"
        $uatArgs += "-nodebuginfo"
    }

    & $unreal.RunUAT @uatArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}

if ($Classroom) {
    & "$PSScriptRoot\Verify-ClassroomPackage.ps1" -PackageRoot $archive
}
