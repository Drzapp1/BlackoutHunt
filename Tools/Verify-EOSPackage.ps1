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
$gameExecutable = Join-Path $resolvedPackageRoot "BlackoutHunt\Binaries\Win64\BlackoutHunt.exe"
if (-not (Test-Path -LiteralPath $rootLauncher)) {
    Add-Failure "missing root launcher: BlackoutHunt.exe"
}
if (-not (Test-Path -LiteralPath $gameExecutable)) {
    Add-Failure "missing Win64 executable: BlackoutHunt\Binaries\Win64\BlackoutHunt.exe"
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

if ($failures.Count -gt 0) {
    $message = "EOS package verification failed:`n - " + (($failures | Sort-Object -Unique) -join "`n - ")
    throw $message
}

Write-Host "EOS package verification passed: $resolvedPackageRoot"
