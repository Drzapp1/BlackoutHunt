param(
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\WindowsEOS"
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

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

if (-not (Test-Path -LiteralPath $PackageRoot)) {
    throw "EOS package root not found: $PackageRoot"
}

$resolvedPackageRoot = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $PackageRoot).Path)

if (-not (Test-IsSameOrChildPath -Candidate $resolvedPackageRoot -Root $projectRoot)) {
    throw "Refusing to verify package outside project root: $resolvedPackageRoot"
}

$failures = New-Object System.Collections.Generic.List[string]
$allFiles = @(Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Force)

function Add-Failure {
    param([Parameter(Mandatory = $true)][string]$Message)
    $failures.Add($Message)
}

function Get-RelativePackagePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return $Path.Substring($resolvedPackageRoot.Length).TrimStart('\', '/')
}

$rootLauncher = Join-Path $resolvedPackageRoot "BlackoutHunt.exe"
# The staged game binary is BlackoutHunt.exe for a Development cook but
# BlackoutHunt-Win64-Shipping.exe for a Shipping cook; accept either name.
$win64Dir = Join-Path $resolvedPackageRoot "BlackoutHunt\Binaries\Win64"
$gameExecutables = @()
if (Test-Path -LiteralPath $win64Dir) {
    $gameExecutables = @(Get-ChildItem -LiteralPath $win64Dir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq "BlackoutHunt.exe" -or $_.Name -ieq "BlackoutHunt-Win64-Shipping.exe" })
}
if (-not (Test-Path -LiteralPath $rootLauncher)) {
    Add-Failure "missing root launcher: BlackoutHunt.exe"
}
if ($gameExecutables.Count -lt 1) {
    Add-Failure "missing Win64 game executable: BlackoutHunt\Binaries\Win64\BlackoutHunt.exe or BlackoutHunt-Win64-Shipping.exe"
}

$eosDlls = @($allFiles | Where-Object { $_.Name -ieq "EOSSDK-Win64-Shipping.dll" })
if ($eosDlls.Count -lt 1) {
    Add-Failure "missing EOS runtime DLL: EOSSDK-Win64-Shipping.dll"
}

$forbidden = @(
    @{ Name = "saved accounts"; Pattern = '[\\/]Saved[\\/]Account[\\/]' },
    @{ Name = "saved crashes"; Pattern = '[\\/]Saved[\\/]Crashes[\\/]' },
    @{ Name = "saved logs"; Pattern = '[\\/]Saved[\\/]Logs[\\/]' },
    @{ Name = "saved classroom reports"; Pattern = '[\\/]Saved[\\/]ClassReports[\\/]' },
    @{ Name = "saved playtest telemetry"; Pattern = '[\\/]Saved[\\/]PlaytestTelemetry[\\/]' },
    @{ Name = "account backend env"; Pattern = '(^|[\\/])\.env$' },
    @{ Name = "Steam local values"; Pattern = '(^|[\\/])SteamValues\.local\.ini$' },
    @{ Name = "EOS local values"; Pattern = '(^|[\\/])EOSValues\.local\.ini$' },
    @{ Name = "local credential data"; Pattern = '(^|[\\/])local_credentials\.enc\.json$' },
    @{ Name = "local profile data"; Pattern = '(^|[\\/])profile\.json$' },
    @{ Name = "local progress data"; Pattern = '(^|[\\/])progress\.json$' },
    @{ Name = "credential or secret file"; Pattern = '(^|[\\/])[^\\/]*(secret|credential|token|oauth|apikey|api_key)[^\\/]*\.(json|txt|ini|env|key|pem|pfx|ppk)$' }
)

foreach ($file in $allFiles) {
    $relativePath = Get-RelativePackagePath $file.FullName
    foreach ($rule in $forbidden) {
        if ($relativePath -match $rule.Pattern) {
            Add-Failure "$($rule.Name): $relativePath"
        }
    }
}

# Assert the authored maps actually made it into the cooked container. bUseAuthoredLevels=True (and the
# always-authored Tutorial) route every Host/Bot/Test/Live-Classroom/Tutorial mode to /Game/BlackoutHunt/Maps/*;
# if those packages are missing from the pak the travel fails and the engine bounces back to the Standalone
# /Engine/Maps/Entry menu, so "every mode just returns to the menu". A bad -map= cook list silently dropped
# them in 0.7.0 -- catch that here instead of in a player's hands.
$requiredMaps = @("Facility", "Substation", "Foggrounds", "Tutorial")
# Prop-hunt arena maps (cooked from their import-pack paths, e.g. /Game/ContainersHouseCH/Maps/...). The same
# pak-listing regex covers them since it only anchors on /Maps/<name>.umap. Add Arena_ContainersHouse once
# Tools/Setup-PropHuntArena.py has baked it and it joins the cook -map= lists.
$requiredArenaMaps = @("Map_ContainersHouse_Demo")
$utoc = Join-Path $resolvedPackageRoot "BlackoutHunt\Content\Paks\BlackoutHunt-Windows.utoc"
if (-not (Test-Path -LiteralPath $utoc)) {
    Add-Failure "missing cooked pak container: BlackoutHunt\Content\Paks\BlackoutHunt-Windows.utoc"
}
else {
    $unrealPak = $null
    try {
        $unreal = & (Join-Path $PSScriptRoot "Find-Unreal.ps1")
        if ($unreal -and $unreal.Root) {
            $candidate = Join-Path $unreal.Root "Engine\Binaries\Win64\UnrealPak.exe"
            if (Test-Path -LiteralPath $candidate) { $unrealPak = $candidate }
        }
    }
    catch { $unrealPak = $null }

    if (-not $unrealPak) {
        Write-Warning "UnrealPak.exe not found; skipping the cooked-map presence check. Install UE 5.7 to enable it."
    }
    else {
        $listing = & $unrealPak $utoc -List 2>&1 | Out-String
        foreach ($map in $requiredMaps) {
            if ($listing -notmatch "(?i)/Maps/$map\.umap") {
                Add-Failure "authored map missing from cook: /Game/BlackoutHunt/Maps/$map.umap (fix the -map= list in the Package-* script and re-cook)"
            }
        }
        foreach ($map in $requiredArenaMaps) {
            if ($listing -notmatch "(?i)/Maps/$map\.umap") {
                Add-Failure "prop-hunt arena map missing from cook: $map.umap (fix the -map= list in the Package-* script and re-cook)"
            }
        }
    }
}

if ($failures.Count -gt 0) {
    $message = "EOS package verification failed:`n - " + (($failures | Sort-Object -Unique) -join "`n - ")
    throw $message
}

Write-Host "EOS package verification passed: $resolvedPackageRoot"
