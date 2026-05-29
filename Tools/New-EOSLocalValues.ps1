param(
    [string]$ArtifactName = "BlackoutHunt",
    [ValidateSet("EpicAccount")]
    [string]$LoginMode = "EpicAccount",
    [string]$ProductId = $env:BH_EOS_PRODUCT_ID,
    [string]$SandboxId = $env:BH_EOS_SANDBOX_ID,
    [string]$DeploymentId = $env:BH_EOS_DEPLOYMENT_ID,
    [string]$ClientId = $env:BH_EOS_CLIENT_ID,
    [string]$ClientSecret = $env:BH_EOS_CLIENT_SECRET,
    [string]$ClientEncryptionKey = $env:BH_EOS_CLIENT_ENCRYPTION_KEY,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$targetPath = Join-Path $projectRoot "Config\EOS\EOSValues.local.ini"

function New-ClientEncryptionKey {
    $bytes = [byte[]]::new(32)
    $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    try {
        $rng.GetBytes($bytes)
    }
    finally {
        $rng.Dispose()
    }

    return ([System.BitConverter]::ToString($bytes) -replace "-", "").ToLowerInvariant()
}

if ((Test-Path -LiteralPath $targetPath) -and -not $Force) {
    throw "EOS local values already exist: $targetPath. Pass -Force to overwrite."
}

if ([string]::IsNullOrWhiteSpace($ClientEncryptionKey)) {
    $ClientEncryptionKey = New-ClientEncryptionKey
}

if ($ClientEncryptionKey -notmatch '^[0-9a-fA-F]{64}$') {
    throw "ClientEncryptionKey must contain exactly 64 hex characters."
}

New-Item -ItemType Directory -Force -Path (Split-Path $targetPath) | Out-Null

$content = @"
[EOS]
ArtifactName=$ArtifactName
LoginMode=$LoginMode
ProductId=$ProductId
SandboxId=$SandboxId
DeploymentId=$DeploymentId
ClientId=$ClientId
ClientSecret=$ClientSecret
ClientEncryptionKey=$ClientEncryptionKey
UseEAS=true
UseEOSConnect=true
UseEOSRTC=false
EnableOverlay=false
EnableSocialOverlay=false
PreferPersistentAuth=true
CacheDir=BlackoutHuntEOS
"@

[System.IO.File]::WriteAllText($targetPath, $content + "`r`n", [System.Text.Encoding]::ASCII)

$missing = @()
foreach ($entry in @(
    @{ Name = "ProductId"; Value = $ProductId },
    @{ Name = "SandboxId"; Value = $SandboxId },
    @{ Name = "DeploymentId"; Value = $DeploymentId },
    @{ Name = "ClientId"; Value = $ClientId },
    @{ Name = "ClientSecret"; Value = $ClientSecret },
    @{ Name = "ClientEncryptionKey"; Value = $ClientEncryptionKey }
)) {
    if ([string]::IsNullOrWhiteSpace($entry.Value)) {
        $missing += $entry.Name
    }
}

Write-Host "Wrote EOS local values: $targetPath"
if ($missing.Count -gt 0) {
    Write-Host "Still missing: $($missing -join ', ')"
    Write-Host "Fill those values from the Epic Developer Portal before packaging."
}
else {
    Write-Host "EOS local values look complete. Build with: .\Tools\Package-Windows-EOS.ps1 -Configuration Development"
}
