param(
    [ValidateSet("Shipping", "Development")]
    [string]$Configuration = "Shipping"
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\Package-Linux.ps1" -Classroom -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
