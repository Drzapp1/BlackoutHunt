param(
    [switch]$Classroom,
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path "$PSScriptRoot\.."
$project = Resolve-Path "$projectRoot\BlackoutHunt.uproject"
$archive = Join-Path $projectRoot "Builds\Linux"
$unreal = & "$PSScriptRoot\Find-Unreal.ps1"

if (-not $Configuration) {
    if ($Classroom) {
        $Configuration = "Shipping"
    }
    else {
        $Configuration = "Development"
    }
}

if ($Classroom -and (Test-Path -LiteralPath $archive)) {
    $resolvedRoot = [System.IO.Path]::GetFullPath($projectRoot)
    $resolvedArchive = [System.IO.Path]::GetFullPath($archive)
    if (-not $resolvedArchive.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean archive outside project root: $resolvedArchive"
    }
    Remove-Item -LiteralPath $archive -Recurse -Force
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
        "-platform=Linux",
        "-clientconfig=$Configuration",
        "-serverconfig=$Configuration",
        "-cook",
        "-map=/Engine/Maps/Entry",
        "-build",
        "-noxge",
        "-ubtargs=-NoXGE -MaxParallelActions=2",
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
