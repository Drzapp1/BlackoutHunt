param(
    [ValidateSet("Shipping", "Development")]
    [string]$Configuration = "Shipping"
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\Package-Windows-Classroom.ps1" -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "No-account package ready: Builds\Windows\BlackoutHunt.exe"
Write-Host "Host: use LIVE CLASSROOM or START INTERNET TUNNEL, then copy/share the BH1 join code."
Write-Host "Clients: launch the same package, choose Guest if prompted, paste the BH1 code or host:port, then Join Game."
