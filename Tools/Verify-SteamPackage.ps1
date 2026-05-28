param(
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\WindowsSteam",
    [switch]$ForSteamDepot
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedPackageRoot = [System.IO.Path]::GetFullPath((Resolve-Path $PackageRoot))

if (-not $resolvedPackageRoot.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
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
$gameExecutable = Join-Path $resolvedPackageRoot "BlackoutHunt\Binaries\Win64\BlackoutHunt.exe"
if (-not (Test-Path -LiteralPath $rootLauncher)) {
    Add-Failure "missing root launcher: BlackoutHunt.exe"
}
if (-not (Test-Path -LiteralPath $gameExecutable)) {
    Add-Failure "missing Win64 executable: BlackoutHunt\Binaries\Win64\BlackoutHunt.exe"
}

$steamDlls = @($allFiles | Where-Object { $_.Name -ieq "steam_api64.dll" })
if ($steamDlls.Count -lt 1) {
    Add-Failure "missing Steam runtime DLL: steam_api64.dll"
}

$steamAppIdFiles = @($allFiles | Where-Object { $_.Name -ieq "steam_appid.txt" })
if ($ForSteamDepot -and $steamAppIdFiles.Count -gt 0) {
    foreach ($file in $steamAppIdFiles) {
        Add-Failure "steam_appid.txt must not be included in Steam depot packages: $(Get-RelativePackagePath $file.FullName)"
    }
}
elseif ($steamAppIdFiles.Count -gt 0) {
    $allowedSteamAppIdFiles = @(
        (Join-Path $resolvedPackageRoot "steam_appid.txt"),
        (Join-Path $resolvedPackageRoot "BlackoutHunt\Binaries\Win64\steam_appid.txt")
    )

    foreach ($expectedPath in $allowedSteamAppIdFiles) {
        if (-not (Test-Path -LiteralPath $expectedPath)) {
            Add-Failure "local Steam testing package should include $((Get-RelativePackagePath $expectedPath)) when any steam_appid.txt is present"
        }
    }

    foreach ($file in $steamAppIdFiles) {
        $fullPath = [System.IO.Path]::GetFullPath($file.FullName)
        $allowed = $false
        foreach ($allowedPath in $allowedSteamAppIdFiles) {
            if ($fullPath -ieq [System.IO.Path]::GetFullPath($allowedPath)) {
                $allowed = $true
                break
            }
        }

        if (-not $allowed) {
            Add-Failure "unexpected steam_appid.txt location: $(Get-RelativePackagePath $file.FullName)"
        }

        $content = (Get-Content -LiteralPath $file.FullName -Raw).Trim()
        if ($content -notmatch '^\d+$') {
            Add-Failure "steam_appid.txt must contain only a numeric App ID: $(Get-RelativePackagePath $file.FullName)"
        }
    }
}

$forbidden = @(
    @{ Name = "saved accounts"; Pattern = '[\\/]Saved[\\/]Account[\\/]' },
    @{ Name = "saved crashes"; Pattern = '[\\/]Saved[\\/]Crashes[\\/]' },
    @{ Name = "saved logs"; Pattern = '[\\/]Saved[\\/]Logs[\\/]' },
    @{ Name = "saved classroom reports"; Pattern = '[\\/]Saved[\\/]ClassReports[\\/]' },
    @{ Name = "saved playtest telemetry"; Pattern = '[\\/]Saved[\\/]PlaytestTelemetry[\\/]' },
    @{ Name = "account backend env"; Pattern = '(^|[\\/])\.env$' },
    @{ Name = "Steam local values"; Pattern = '(^|[\\/])SteamValues\.local\.ini$' },
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

if ($failures.Count -gt 0) {
    $message = "Steam package verification failed:`n - " + (($failures | Sort-Object -Unique) -join "`n - ")
    throw $message
}

Write-Host "Steam package verification passed: $resolvedPackageRoot"
