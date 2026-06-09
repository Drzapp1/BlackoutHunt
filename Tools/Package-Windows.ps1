param(
    [switch]$Classroom,
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration,
    # UnrealBuildTool compile parallelism. The previous hardcoded value of 1
    # compiled serially and was the dominant cost of packaging. Defaults to the
    # processor count; lower it on memory-constrained hosts where parallel
    # cl.exe actions risk OOM. Honors the BUILD_PARALLEL env var when set.
    [int]$MaxParallel = 0,
    # Skip the full -clean rebuild and cook iteratively. Much faster for repeat
    # builds when code/content is unchanged. Leave off for hermetic releases.
    [switch]$Incremental
)

$ErrorActionPreference = "Stop"

if ($MaxParallel -le 0) {
    if ($env:BUILD_PARALLEL) {
        $MaxParallel = [int]$env:BUILD_PARALLEL
    }
    else {
        $MaxParallel = [Environment]::ProcessorCount
    }
}
if ($MaxParallel -lt 1) { $MaxParallel = 1 }

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

function Write-ClassroomLaunchHelpers {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageRoot
    )

    $dx11Launcher = @'
@echo off
setlocal
cd /d "%~dp0"
start "" "%~dp0BlackoutHunt.exe" -d3d11
'@

    $safeLauncher = @'
@echo off
setlocal
cd /d "%~dp0"
start "" "%~dp0BlackoutHunt.exe" -d3d11 -BHVirtualBoxSafe -ResX=1280 -ResY=720 -WINDOWED
'@

    $troubleshooting = @'
Blackout Hunt classroom graphics launch notes

Use Launch-BlackoutHunt-DX11.cmd on older Windows machines.
Use Launch-BlackoutHunt-DX11-Low.cmd for VM or low-spec validation. It forces D3D11, 1280x720 windowed mode, and the VirtualBox-safe renderer path.

On a new install, Auto graphics also routes software, unknown, integrated, or low-VRAM GPUs to Low 4GB plus 1280x720 windowed mode unless the local user already saved a resolution. The Settings panel and Host Preflight graphics line show the active preset, cap, resolution, and render scale.

If Windows still shows "A D3D11-compatible GPU is required", the machine is not exposing Direct3D feature level 11.0 / Shader Model 5.0 to Unreal. Check:

- Install the real Intel/NVIDIA/AMD graphics driver; Microsoft Basic Display Adapter is not enough.
- Avoid launching through Remote Desktop if it hides hardware acceleration.
- On VirtualBox, enable 3D Acceleration, use the VBoxSVGA controller, install Guest Additions, and allocate the maximum video memory. Some VirtualBox hosts still cannot expose a UE-compatible D3D11 path.
- Very old GPUs such as Intel HD 3000-era hardware cannot run this UE5 package.
'@

    Set-Content -LiteralPath (Join-Path $PackageRoot "Launch-BlackoutHunt-DX11.cmd") -Value $dx11Launcher -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $PackageRoot "Launch-BlackoutHunt-DX11-Low.cmd") -Value $safeLauncher -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $PackageRoot "GPU-TROUBLESHOOTING.txt") -Value $troubleshooting -Encoding ASCII
}

function Copy-ClassroomPackageNotices {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageRoot
    )

    $noticeSource = Join-Path $projectRoot "Docs\THIRD_PARTY_NOTICES.txt"
    if (-not (Test-Path -LiteralPath $noticeSource)) {
        throw "Missing third-party notices source: $noticeSource"
    }

    Copy-Item -LiteralPath $noticeSource -Destination (Join-Path $PackageRoot "THIRD-PARTY-NOTICES.txt") -Force
}

function Write-NoAccountQuickstart {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PackageRoot
    )

    $quickstart = @'
Blackout Hunt no-account online quickstart

Use this package when players should join without Steam, Epic, Google, Microsoft, or any other account.

Host:
1. Launch BlackoutHunt.exe.
2. Choose Guest if the account panel appears.
3. Use LIVE CLASSROOM, or host a normal map and click START INTERNET TUNNEL if players are not on the same LAN.
4. If Playit setup opens, create or select a Custom UDP tunnel to 127.0.0.1:7777.
5. Use COPY JOIN CODE and send the BH1:... code to players.

Client:
1. Launch BlackoutHunt.exe.
2. Choose Guest if the account panel appears.
3. Paste the BH1:... code into HOST / IP / CODE.
4. Click JOIN GAME.

LAN/direct IP also works when UDP 7777 reaches the host.
'@

    Set-Content -LiteralPath (Join-Path $PackageRoot "NO-ACCOUNT-ONLINE.txt") -Value $quickstart -Encoding ASCII
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
        "-map=/Engine/Maps/Entry+/Game/BlackoutHunt/Maps/Facility+/Game/BlackoutHunt/Maps/Substation+/Game/BlackoutHunt/Maps/Foggrounds+/Game/BlackoutHunt/Maps/Tutorial",
        "-build",
        "-noxge",
        # -NoHotReloadFromIDE: skip UBT's "Live Coding is active" guard so a headless package cook
        # is not blocked by a concurrent editor session on the same engine (correct for packaging,
        # which never wants IDE hot-reload). [live-play-hardening]
        "-ubtargs=-WaitMutex -NoXGE -NoUBA -NoHotReloadFromIDE -MaxParallelActions=$MaxParallel",
        "-stage",
        "-pak",
        "-archive",
        "-archivedirectory=$archive"
    )
    if ($Incremental) {
        $uatArgs += "-iterativecooking"
    }
    if ($Classroom) {
        $uatArgs += "-distribution"
        $uatArgs += "-nodebuginfo"
        $uatArgs += "-applocaldirectory=$appLocalDependencies"
        if (-not $Incremental) {
            $uatArgs += "-clean"
        }
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
    Write-ClassroomLaunchHelpers -PackageRoot $archive
    Write-NoAccountQuickstart -PackageRoot $archive
    Copy-ClassroomPackageNotices -PackageRoot $archive
    & "$PSScriptRoot\Verify-ClassroomPackage.ps1" -PackageRoot $archive -ExpectedAppLocalDependencyRoot $appLocalDependenciesX64
    # Fail the package build if verification fails. Mirrors the explicit exit-code propagation used
    # for the UAT call above (line 207) so a future refactor of the verifier cannot let a failed
    # classroom package slip through to distribution.
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
