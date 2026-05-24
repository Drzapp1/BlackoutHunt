$ErrorActionPreference = "Stop"

$backendDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$envPath = Join-Path $backendDir ".env"
$examplePath = Join-Path $backendDir ".env.example"

function ConvertFrom-SecureStringPlainText {
    param([Parameter(Mandatory = $true)][System.Security.SecureString]$Value)

    $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($Value)
    try {
        [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
    }
}

function Read-EnvFile {
    param([Parameter(Mandatory = $true)][string]$Path)

    $values = [ordered]@{}
    if (Test-Path -LiteralPath $Path) {
        foreach ($line in Get-Content -LiteralPath $Path) {
            if ($line -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)\s*$') {
                $values[$matches[1]] = $matches[2]
            }
        }
    }
    return $values
}

function Set-EnvValue {
    param(
        [Parameter(Mandatory = $true)][System.Collections.IDictionary]$Values,
        [Parameter(Mandatory = $true)][string]$Key,
        [Parameter(Mandatory = $true)][string]$Value
    )

    if ($Values.Contains($Key)) {
        $Values[$Key] = $Value
    }
    else {
        $Values.Add($Key, $Value)
    }
}

if (-not (Test-Path -LiteralPath $envPath)) {
    if (Test-Path -LiteralPath $examplePath) {
        Copy-Item -LiteralPath $examplePath -Destination $envPath
    }
    else {
        New-Item -ItemType File -Path $envPath | Out-Null
    }
}

$values = Read-EnvFile -Path $envPath

Write-Host "Google OAuth redirect URI for this backend:"
Write-Host "  http://127.0.0.1:8787/auth/google/callback"
Write-Host ""

$clientId = Read-Host "Google OAuth Client ID"
$clientSecretSecure = Read-Host "Google OAuth Client Secret" -AsSecureString
$clientSecret = ConvertFrom-SecureStringPlainText -Value $clientSecretSecure

if ([string]::IsNullOrWhiteSpace($clientId)) {
    throw "GOOGLE_CLIENT_ID cannot be empty."
}

if ($clientId.Trim() -match '^(web\s*application|desktop\s*app|android|ios)$') {
    throw "That is the OAuth client type, not the Client ID. In Google Cloud Console, open your OAuth client and copy the value labeled Client ID. It usually ends with .apps.googleusercontent.com."
}

if ($clientId.Trim() -notmatch '\.apps\.googleusercontent\.com$') {
    throw "This does not look like a Google OAuth Client ID. Copy the value labeled Client ID from Google Cloud Console; it usually ends with .apps.googleusercontent.com."
}

if ([string]::IsNullOrWhiteSpace($clientSecret)) {
    throw "GOOGLE_CLIENT_SECRET cannot be empty."
}

Set-EnvValue -Values $values -Key "PORT" -Value "8787"
Set-EnvValue -Values $values -Key "PUBLIC_BASE_URL" -Value "http://127.0.0.1:8787"
Set-EnvValue -Values $values -Key "GOOGLE_CLIENT_ID" -Value $clientId.Trim()
Set-EnvValue -Values $values -Key "GOOGLE_CLIENT_SECRET" -Value $clientSecret.Trim()

$orderedKeys = @(
    "PORT",
    "PUBLIC_BASE_URL",
    "STATE_SECRET",
    "GOOGLE_CLIENT_ID",
    "GOOGLE_CLIENT_SECRET",
    "MICROSOFT_CLIENT_ID",
    "MICROSOFT_CLIENT_SECRET"
)

$lines = [System.Collections.Generic.List[string]]::new()
foreach ($key in $orderedKeys) {
    if ($values.Contains($key)) {
        $lines.Add("$key=$($values[$key])")
    }
}

foreach ($key in $values.Keys) {
    if ($orderedKeys -notcontains $key) {
        $lines.Add("$key=$($values[$key])")
    }
}

Set-Content -LiteralPath $envPath -Value $lines -Encoding UTF8

Write-Host ""
Write-Host "Updated $envPath"
Write-Host "Restart the backend so it reads the new credentials:"
Write-Host "  cd $backendDir"
Write-Host "  node .\server.mjs"
