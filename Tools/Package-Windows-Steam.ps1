param(
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration = "Development",
    [string]$SteamValuesPath,
    [switch]$AllowSpacewarShipping
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$project = Join-Path $projectRoot "BlackoutHunt.uproject"
$archive = Join-Path $projectRoot "Builds\WindowsSteam"
$packageDdc = Join-Path $projectRoot "Builds\DerivedDataCache"
$engineConfigPath = Join-Path $projectRoot "Config\DefaultEngine.ini"
$gameConfigPath = Join-Path $projectRoot "Config\DefaultGame.ini"
$defaultValuesPath = Join-Path $projectRoot "Config\Steam\SteamValues.example.ini"
$localValuesPath = Join-Path $projectRoot "Config\Steam\SteamValues.local.ini"
$unreal = & "$PSScriptRoot\Find-Unreal.ps1"

function Resolve-InputPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Read-IniValue {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Section,
        [Parameter(Mandatory = $true)][string]$Key,
        [string]$DefaultValue = ""
    )

    $currentSection = ""
    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith(";") -or $trimmed.StartsWith("#")) {
            continue
        }

        if ($trimmed -match '^\[(.+)\]$') {
            $currentSection = $Matches[1].Trim()
            continue
        }

        if ($currentSection -ieq $Section -and $trimmed -match '^([^=]+)=(.*)$') {
            $name = $Matches[1].Trim()
            if ($name -ieq $Key) {
                return $Matches[2].Trim().Trim('"')
            }
        }
    }

    return $DefaultValue
}

function ConvertTo-BoolValue {
    param([Parameter(Mandatory = $true)][string]$Value)

    switch -Regex ($Value.Trim()) {
        '^(1|true|yes|on)$' { return $true }
        '^(0|false|no|off)$' { return $false }
        default { throw "Invalid boolean value '$Value'." }
    }
}

function New-SteamEngineConfig {
    param(
        [Parameter(Mandatory = $true)][string]$AppId,
        [Parameter(Mandatory = $true)][string]$GameVersion
    )

    return @"

; Steam online package profile. Generated temporarily by Tools\Package-Windows-Steam.ps1.
[OnlineSubsystem]
DefaultPlatformService=Steam

[/Script/Engine.Engine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemSteam.SteamNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="BeaconNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=$AppId
bInitServerOnClient=true
bRelaunchInSteam=false
GameServerQueryPort=27015
bAllowP2PPacketRelay=true
P2PConnectionTimeout=90
GameVersion=$GameVersion

[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="/Script/OnlineSubsystemSteam.SteamNetConnection"
"@
}

function Write-SteamAppIdFiles {
    param(
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [Parameter(Mandatory = $true)][string]$AppId
    )

    $targets = @(
        $PackageRoot,
        (Join-Path $PackageRoot "BlackoutHunt\Binaries\Win64")
    )

    foreach ($target in $targets) {
        if (-not (Test-Path -LiteralPath $target)) {
            throw "Cannot write steam_appid.txt; missing package directory: $target"
        }

        [System.IO.File]::WriteAllText((Join-Path $target "steam_appid.txt"), "$AppId`r`n", [System.Text.Encoding]::ASCII)
    }
}

function Invoke-UnrealBuildTarget {
    param(
        [Parameter(Mandatory = $true)][string]$TargetName,
        [Parameter(Mandatory = $true)][string]$TargetConfiguration
    )

    $buildDir = Split-Path $unreal.Build
    $buildArgs = @(
        $TargetName,
        "Win64",
        $TargetConfiguration,
        "-Project=$project",
        "-WaitMutex",
        "-notinstalledengine",
        "-NoXGE",
        "-NoUBA",
        "-MaxParallelActions=1"
    )

    if ($TargetName -eq "BlackoutHuntEditor") {
        $buildArgs += "-NoHotReloadFromIDE"
    }

    Push-Location $buildDir
    try {
        & $unreal.Build @buildArgs
        if ($LASTEXITCODE -ne 0) { throw "UnrealBuildTool failed for $TargetName $TargetConfiguration with exit code $LASTEXITCODE." }
    }
    finally {
        Pop-Location
    }
}

if (-not $SteamValuesPath) {
    $SteamValuesPath = $localValuesPath
}
else {
    $SteamValuesPath = Resolve-InputPath $SteamValuesPath
}

if (-not (Test-Path -LiteralPath $SteamValuesPath)) {
    if ($SteamValuesPath -ne $localValuesPath) {
        throw "Steam values file not found: $SteamValuesPath"
    }

    $SteamValuesPath = $defaultValuesPath
    Write-Host "Using default Steam values from $SteamValuesPath. Copy to Config\Steam\SteamValues.local.ini for a real Steam App ID."
}

if (-not (Test-Path -LiteralPath $SteamValuesPath)) {
    throw "Missing Steam values file: $SteamValuesPath"
}

$steamAppId = Read-IniValue -Path $SteamValuesPath -Section "Steam" -Key "SteamAppId"
if ($steamAppId -notmatch '^\d+$' -or [int64]$steamAppId -le 0) {
    throw "SteamAppId must be a positive numeric Steam App ID in $SteamValuesPath."
}

# Read the live project version so the Steam GameVersion never drifts from Config\DefaultGame.ini.
$gameVersion = Read-IniValue -Path $gameConfigPath -Section "/Script/EngineSettings.GeneralProjectSettings" -Key "ProjectVersion"
if (-not $gameVersion) {
    throw "Could not read ProjectVersion from $gameConfigPath."
}

$includeSteamAppIdTxt = ConvertTo-BoolValue (Read-IniValue -Path $SteamValuesPath -Section "Steam" -Key "IncludeSteamAppIdTxtForLocalTesting" -DefaultValue "false")
if ($Configuration -eq "Shipping" -and $steamAppId -eq "480" -and -not $AllowSpacewarShipping) {
    throw "Shipping Steam packages cannot use Spacewar App ID 480. Set a real SteamAppId or pass -AllowSpacewarShipping for an explicit smoke-test package."
}

if ($Configuration -eq "Shipping" -and $includeSteamAppIdTxt) {
    throw "Shipping Steam packages must not include steam_appid.txt. Set IncludeSteamAppIdTxtForLocalTesting=false in $SteamValuesPath."
}

$resolvedRoot = [System.IO.Path]::GetFullPath($projectRoot)
$resolvedArchive = [System.IO.Path]::GetFullPath($archive)
if (-not $resolvedArchive.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean archive outside project root: $resolvedArchive"
}

$originalEngineConfigBytes = [System.IO.File]::ReadAllBytes($engineConfigPath)
$originalEngineConfigText = [System.IO.File]::ReadAllText($engineConfigPath)
$previousSteamAppId = [System.Environment]::GetEnvironmentVariable("BH_STEAM_APP_ID", "Process")
$previousLocalDataCachePath = [System.Environment]::GetEnvironmentVariable("UE-LocalDataCachePath", "Process")

try {
    [System.Environment]::SetEnvironmentVariable("BH_STEAM_APP_ID", $steamAppId, "Process")
    [System.IO.File]::WriteAllText($engineConfigPath, ($originalEngineConfigText.TrimEnd() + (New-SteamEngineConfig -AppId $steamAppId -GameVersion $gameVersion) + "`r`n"), [System.Text.UTF8Encoding]::new($false))

    if (Test-Path -LiteralPath $archive) {
        Remove-Item -LiteralPath $archive -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $archive | Out-Null
    New-Item -ItemType Directory -Force -Path $packageDdc | Out-Null

    Invoke-UnrealBuildTarget -TargetName "BlackoutHuntEditor" -TargetConfiguration "Development"
    Invoke-UnrealBuildTarget -TargetName "BlackoutHunt" -TargetConfiguration $Configuration

    $uatDir = Split-Path $unreal.RunUAT
    Push-Location $uatDir
    try {
        [System.Environment]::SetEnvironmentVariable("UE-LocalDataCachePath", $packageDdc, "Process")
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
            "-skipbuild",
            "-noxge",
            "-ubtargs=-WaitMutex -NoXGE -NoUBA -MaxParallelActions=1",
            "-stage",
            "-pak",
            "-archive",
            "-archivedirectory=$archive"
        )

        if ($Configuration -eq "Shipping") {
            $uatArgs += "-distribution"
            $uatArgs += "-nodebuginfo"
            $uatArgs += "-clean"
        }

        & $unreal.RunUAT @uatArgs
        if ($LASTEXITCODE -ne 0) { throw "RunUAT failed with exit code $LASTEXITCODE." }
    }
    finally {
        Pop-Location
        if ($null -ne $previousLocalDataCachePath) {
            [System.Environment]::SetEnvironmentVariable("UE-LocalDataCachePath", $previousLocalDataCachePath, "Process")
        }
        else {
            [System.Environment]::SetEnvironmentVariable("UE-LocalDataCachePath", $null, "Process")
        }
    }

    if ($includeSteamAppIdTxt) {
        Write-SteamAppIdFiles -PackageRoot $archive -AppId $steamAppId
    }

    & "$PSScriptRoot\Verify-SteamPackage.ps1" -PackageRoot $archive
}
finally {
    [System.IO.File]::WriteAllBytes($engineConfigPath, $originalEngineConfigBytes)
    if ($null -ne $previousSteamAppId) {
        [System.Environment]::SetEnvironmentVariable("BH_STEAM_APP_ID", $previousSteamAppId, "Process")
    }
    else {
        [System.Environment]::SetEnvironmentVariable("BH_STEAM_APP_ID", $null, "Process")
    }
}
