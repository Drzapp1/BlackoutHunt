param(
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration = "Development",
    [string]$EOSValuesPath,
    [switch]$InitLocalValues,
    [switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$project = Join-Path $projectRoot "BlackoutHunt.uproject"
$archive = Join-Path $projectRoot "Builds\WindowsEOS"
$packageDdc = Join-Path $projectRoot "Builds\DerivedDataCache"
$engineConfigPath = Join-Path $projectRoot "Config\DefaultEngine.ini"
$defaultValuesPath = Join-Path $projectRoot "Config\EOS\EOSValues.example.ini"
$localValuesPath = Join-Path $projectRoot "Config\EOS\EOSValues.local.ini"

function Resolve-InputPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Test-IsSameOrChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$Candidate,
        [Parameter(Mandatory = $true)][string]$Root
    )

    $trimChars = [char[]]@([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $candidateFull = [System.IO.Path]::GetFullPath($Candidate).TrimEnd($trimChars)
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd($trimChars)

    if ($candidateFull.Equals($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }

    return $candidateFull.StartsWith($rootFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
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

function Assert-RequiredEOSValue {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Value) -or $Value.Trim().StartsWith("<") -or $Value.Trim().EndsWith(">")) {
        throw "$Name must be filled in $Path before building an EOS package."
    }
}

function Assert-HexEOSValue {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][int]$Length,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if ($Value -notmatch "^[0-9a-fA-F]{$Length}$") {
        throw "$Name must contain exactly $Length hex characters in $Path."
    }
}

function Assert-NoWhitespaceEOSValue {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if ($Value -match '\s') {
        throw "$Name must not contain whitespace in $Path."
    }
}

function New-EOSEngineConfig {
    param(
        [Parameter(Mandatory = $true)][string]$ArtifactName,
        [Parameter(Mandatory = $true)][string]$ProductId,
        [Parameter(Mandatory = $true)][string]$SandboxId,
        [Parameter(Mandatory = $true)][string]$DeploymentId,
        [Parameter(Mandatory = $true)][string]$ClientId,
        [Parameter(Mandatory = $true)][string]$ClientSecret,
        [Parameter(Mandatory = $true)][string]$ClientEncryptionKey,
        [Parameter(Mandatory = $true)][bool]$UseEAS,
        [Parameter(Mandatory = $true)][bool]$UseEOSConnect,
        [Parameter(Mandatory = $true)][bool]$UseEOSRTC,
        [Parameter(Mandatory = $true)][bool]$EnableOverlay,
        [Parameter(Mandatory = $true)][bool]$EnableSocialOverlay,
        [Parameter(Mandatory = $true)][bool]$PreferPersistentAuth,
        [Parameter(Mandatory = $true)][string]$CacheDir
    )

    $useEASValue = $UseEAS.ToString().ToLowerInvariant()
    $useEOSConnectValue = $UseEOSConnect.ToString().ToLowerInvariant()
    $useEOSRTCValue = $UseEOSRTC.ToString().ToLowerInvariant()
    $enableOverlayValue = $EnableOverlay.ToString().ToLowerInvariant()
    $enableSocialOverlayValue = $EnableSocialOverlay.ToString().ToLowerInvariant()
    $preferPersistentAuthValue = $PreferPersistentAuth.ToString().ToLowerInvariant()

    return @"

; EOS online package profile. Generated temporarily by Tools\Package-Windows-EOS.ps1.
[OnlineSubsystem]
DefaultPlatformService=EOS

[/Script/Engine.Engine]
; Transport is plain IP so the game binds a real UDP socket: this is what LAN, direct-IP, and the Playit
; tunnel (the documented classroom join, blackouthunt.playit.plus:24761) require. EOS still provides
; accounts/identity + lobby discovery (DefaultPlatformService=EOS below), but NetDriverEOS as the game
; net driver could not bind a plain ?listen host without a live EOS P2P session and bounced every mode back
; to the menu ("Could not bind local address" -> "returning to Entry"). P2P-relay hosting can return later
; via per-mode net-driver selection; for now school-network tunnel/LAN connectivity is the priority.
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="BeaconNetDriver",DriverClassName="/Script/OnlineSubsystemUtils.IpNetDriver",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

[/Script/OnlineSubsystemEOS.EOSSettings]
DefaultArtifactName=$ArtifactName
CacheDir=$CacheDir
bUseEAS=$useEASValue
bUseEOSConnect=$useEOSConnectValue
bUseEOSRTC=$useEOSRTCValue
bEnableOverlay=$enableOverlayValue
bEnableSocialOverlay=$enableSocialOverlayValue
bPreferPersistentAuth=$preferPersistentAuthValue
!AuthScopeFlags=ClearArray
+AuthScopeFlags=BasicProfile
+AuthScopeFlags=FriendsList
+AuthScopeFlags=Presence
!Artifacts=ClearArray
+Artifacts=(ArtifactName="$ArtifactName",ClientId="$ClientId",ClientSecret="$ClientSecret",ProductId="$ProductId",SandboxId="$SandboxId",DeploymentId="$DeploymentId",ClientEncryptionKey="$ClientEncryptionKey")

[SocketSubsystemEOS]
RelayControl=AllowRelays
"@
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

if ($InitLocalValues) {
    if (Test-Path -LiteralPath $localValuesPath) {
        Write-Host "EOS local values already exist and were left unchanged: $localValuesPath"
        Write-Host "Build with: .\Tools\Package-Windows-EOS.ps1 -Configuration Development"
        return
    }

    & "$PSScriptRoot\New-EOSLocalValues.ps1"
    return
}

if (-not $EOSValuesPath) {
    $EOSValuesPath = $localValuesPath
}
else {
    $EOSValuesPath = Resolve-InputPath $EOSValuesPath
}

if (-not (Test-Path -LiteralPath $EOSValuesPath)) {
    if ($EOSValuesPath -ne $localValuesPath) {
        throw "EOS values file not found: $EOSValuesPath"
    }

    throw "Missing EOS values file: $EOSValuesPath. Copy Config\EOS\EOSValues.example.ini to Config\EOS\EOSValues.local.ini, then fill the EOS Developer Portal values."
}

if (([System.IO.Path]::GetFullPath($EOSValuesPath)) -ieq ([System.IO.Path]::GetFullPath($defaultValuesPath))) {
    throw "Do not package with the example EOS values file. Copy it to Config\EOS\EOSValues.local.ini and fill real EOS values first."
}

$artifactName = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "ArtifactName" -DefaultValue "BlackoutHunt"
$loginMode = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "LoginMode" -DefaultValue "EpicAccount"
$productId = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "ProductId"
$sandboxId = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "SandboxId"
$deploymentId = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "DeploymentId"
$clientId = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "ClientId"
$clientSecret = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "ClientSecret"
$clientEncryptionKey = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "ClientEncryptionKey"
$useEAS = ConvertTo-BoolValue (Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "UseEAS" -DefaultValue "true")
$useEOSConnect = ConvertTo-BoolValue (Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "UseEOSConnect" -DefaultValue "true")
$useEOSRTC = ConvertTo-BoolValue (Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "UseEOSRTC" -DefaultValue "false")
$enableOverlay = ConvertTo-BoolValue (Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "EnableOverlay" -DefaultValue "false")
$enableSocialOverlay = ConvertTo-BoolValue (Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "EnableSocialOverlay" -DefaultValue "false")
$preferPersistentAuth = ConvertTo-BoolValue (Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "PreferPersistentAuth" -DefaultValue "true")
$cacheDir = Read-IniValue -Path $EOSValuesPath -Section "EOS" -Key "CacheDir" -DefaultValue "BlackoutHuntEOS"

Assert-RequiredEOSValue -Name "ArtifactName" -Value $artifactName -Path $EOSValuesPath
if (-not $loginMode.Equals("EpicAccount", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsupported EOS LoginMode '$loginMode' in $EOSValuesPath. This package profile currently supports LoginMode=EpicAccount. Use Tools\Package-Windows-NoAccount.ps1 for account-free guest/direct-IP/Playit sessions."
}
Assert-RequiredEOSValue -Name "ProductId" -Value $productId -Path $EOSValuesPath
Assert-RequiredEOSValue -Name "SandboxId" -Value $sandboxId -Path $EOSValuesPath
Assert-RequiredEOSValue -Name "DeploymentId" -Value $deploymentId -Path $EOSValuesPath
Assert-RequiredEOSValue -Name "ClientId" -Value $clientId -Path $EOSValuesPath
Assert-RequiredEOSValue -Name "ClientSecret" -Value $clientSecret -Path $EOSValuesPath
Assert-RequiredEOSValue -Name "ClientEncryptionKey" -Value $clientEncryptionKey -Path $EOSValuesPath
Assert-RequiredEOSValue -Name "CacheDir" -Value $cacheDir -Path $EOSValuesPath
Assert-HexEOSValue -Name "ProductId" -Value $productId -Length 32 -Path $EOSValuesPath
Assert-HexEOSValue -Name "SandboxId" -Value $sandboxId -Length 32 -Path $EOSValuesPath
Assert-HexEOSValue -Name "DeploymentId" -Value $deploymentId -Length 32 -Path $EOSValuesPath
Assert-HexEOSValue -Name "ClientEncryptionKey" -Value $clientEncryptionKey -Length 64 -Path $EOSValuesPath
Assert-NoWhitespaceEOSValue -Name "ClientId" -Value $clientId -Path $EOSValuesPath
Assert-NoWhitespaceEOSValue -Name "ClientSecret" -Value $clientSecret -Path $EOSValuesPath
Assert-NoWhitespaceEOSValue -Name "ArtifactName" -Value $artifactName -Path $EOSValuesPath
Assert-NoWhitespaceEOSValue -Name "CacheDir" -Value $cacheDir -Path $EOSValuesPath

if (-not $useEAS -and -not $useEOSConnect) {
    throw "At least one of UseEAS or UseEOSConnect must be true in $EOSValuesPath."
}

if ($ValidateOnly) {
    Write-Host "EOS local values validation passed."
    Write-Host "ArtifactName: $artifactName"
    Write-Host "LoginMode: $loginMode"
    Write-Host "Product/Sandbox/Deployment IDs: configured"
    Write-Host "Client credentials: configured"
    Write-Host "ClientEncryptionKey: valid 64 hex characters"
    Write-Host "UseEAS: $useEAS"
    Write-Host "UseEOSConnect: $useEOSConnect"
    Write-Host "UseEOSRTC: $useEOSRTC"
    Write-Host "Build with: .\Tools\Package-Windows-EOS.ps1 -Configuration Development"
    return
}

$resolvedArchive = [System.IO.Path]::GetFullPath($archive)
if (-not (Test-IsSameOrChildPath -Candidate $resolvedArchive -Root $projectRoot)) {
    throw "Refusing to clean archive outside project root: $resolvedArchive"
}

$originalEngineConfigBytes = [System.IO.File]::ReadAllBytes($engineConfigPath)
$originalEngineConfigText = [System.IO.File]::ReadAllText($engineConfigPath)
$previousLocalDataCachePath = [System.Environment]::GetEnvironmentVariable("UE-LocalDataCachePath", "Process")
$unreal = & "$PSScriptRoot\Find-Unreal.ps1"

try {
    [System.IO.File]::WriteAllText($engineConfigPath, ($originalEngineConfigText.TrimEnd() + (New-EOSEngineConfig `
        -ArtifactName $artifactName `
        -ProductId $productId `
        -SandboxId $sandboxId `
        -DeploymentId $deploymentId `
        -ClientId $clientId `
        -ClientSecret $clientSecret `
        -ClientEncryptionKey $clientEncryptionKey `
        -UseEAS $useEAS `
        -UseEOSConnect $useEOSConnect `
        -UseEOSRTC $useEOSRTC `
        -EnableOverlay $enableOverlay `
        -EnableSocialOverlay $enableSocialOverlay `
        -PreferPersistentAuth $preferPersistentAuth `
        -CacheDir $cacheDir) + "`r`n"), [System.Text.UTF8Encoding]::new($false))

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
            # Prop-hunt arena: Map_ContainersHouse_Demo is the runtime fallback arena. Once Tools/Setup-PropHuntArena.py
            # has baked /Game/BlackoutHunt/Maps/Arena_ContainersHouse, append it here too (the runtime prefers the bake).
            "-map=/Engine/Maps/Entry+/Game/BlackoutHunt/Maps/Facility+/Game/BlackoutHunt/Maps/Substation+/Game/BlackoutHunt/Maps/Foggrounds+/Game/BlackoutHunt/Maps/Tutorial+/Game/ContainersHouseCH/Maps/Map_ContainersHouse_Demo",
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

    & "$PSScriptRoot\Verify-EOSPackage.ps1" -PackageRoot $archive
}
finally {
    [System.IO.File]::WriteAllBytes($engineConfigPath, $originalEngineConfigBytes)
    if ($null -ne $previousLocalDataCachePath) {
        [System.Environment]::SetEnvironmentVariable("UE-LocalDataCachePath", $previousLocalDataCachePath, "Process")
    }
    else {
        [System.Environment]::SetEnvironmentVariable("UE-LocalDataCachePath", $null, "Process")
    }
}
