param(
    [switch]$Classroom,
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration
)

$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path "$PSScriptRoot\.."
$project = Resolve-Path "$projectRoot\BlackoutHunt.uproject"
$archive = Join-Path $projectRoot "Builds\Windows"
$packageDdc = Join-Path $projectRoot "Builds\DerivedDataCache"
$unreal = & "$PSScriptRoot\Find-Unreal.ps1"
$appLocalDependencies = Join-Path $unreal.Root "Engine\Binaries\ThirdParty\AppLocalDependencies"
$appLocalDependenciesX64 = Join-Path $appLocalDependencies "Win64\x64"

function Copy-AppLocalDependenciesToPackageRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceRoot,
        [Parameter(Mandatory = $true)]
        [string]$PackageRoot
    )

    $rootLauncher = Join-Path $PackageRoot "BlackoutHunt.exe"
    if (-not (Test-Path -LiteralPath $rootLauncher)) {
        throw "Missing packaged root launcher: $rootLauncher"
    }

    $runtimeDlls = Get-ChildItem -LiteralPath $SourceRoot -Recurse -File -Filter "*.dll" -Force
    if (-not $runtimeDlls) {
        throw "No app-local runtime DLLs found under: $SourceRoot"
    }

    foreach ($runtimeDll in $runtimeDlls) {
        Copy-Item -LiteralPath $runtimeDll.FullName -Destination (Join-Path $PackageRoot $runtimeDll.Name) -Force
    }
}

if (-not $Configuration) {
    if ($Classroom) {
        $Configuration = "Shipping"
    }
    else {
        $Configuration = "Development"
    }
}

if ($Classroom) {
    if (-not (Test-Path -LiteralPath $appLocalDependenciesX64)) {
        throw "Missing Unreal app-local dependency set: $appLocalDependenciesX64"
    }

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
New-Item -ItemType Directory -Force -Path $packageDdc | Out-Null

$uatDir = Split-Path $unreal.RunUAT
$previousLocalDataCachePath = (Get-Item -Path "Env:\UE-LocalDataCachePath" -ErrorAction SilentlyContinue).Value
Push-Location $uatDir
try {
    Set-Item -Path "Env:\UE-LocalDataCachePath" -Value $packageDdc
    $uatArgs = @(
        "BuildCookRun",
        "-project=$project",
        "-notinstalledengine",
        "-noP4",
        "-platform=Win64",
        "-clientconfig=$Configuration",
        "-serverconfig=$Configuration",
        "-cook",
        "-ddc=InstalledNoZenLocalFallback",
        "-map=/Engine/Maps/Entry",
        "-build",
        "-noxge",
        "-ubtargs=-WaitMutex -NoXGE -NoUBA -MaxParallelActions=1",
        "-stage",
        "-pak",
        "-archive",
        "-archivedirectory=$archive"
    )
    if ($Classroom) {
        $uatArgs += "-distribution"
        $uatArgs += "-nodebuginfo"
        $uatArgs += "-clean"
        $uatArgs += "-applocaldirectory=$appLocalDependencies"
    }

    & $unreal.RunUAT @uatArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
    if ($null -ne $previousLocalDataCachePath) {
        Set-Item -Path "Env:\UE-LocalDataCachePath" -Value $previousLocalDataCachePath
    }
    else {
        Remove-Item -Path "Env:\UE-LocalDataCachePath" -ErrorAction SilentlyContinue
    }
}

if ($Classroom) {
    Copy-AppLocalDependenciesToPackageRoot -SourceRoot $appLocalDependenciesX64 -PackageRoot $archive
    & "$PSScriptRoot\Verify-ClassroomPackage.ps1" -PackageRoot $archive -ExpectedAppLocalDependencyRoot $appLocalDependenciesX64
}
