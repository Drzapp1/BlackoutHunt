param(
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\Windows",
    [string]$ExpectedPlayitSha256 = "88000d40af7a8e5a0548d27d71c0cad7d5f4b91fd85f6e9297237ac8b57fbdc9"
)

$ErrorActionPreference = "Stop"

$resolvedPackageRoot = [System.IO.Path]::GetFullPath((Resolve-Path $PackageRoot))
$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))

if (-not $resolvedPackageRoot.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to verify package outside project root: $resolvedPackageRoot"
}

$forbidden = @(
    @{ Name = "debug symbols"; Pattern = '\.pdb$' },
    @{ Name = "saved accounts"; Pattern = '[\\/]Saved[\\/]Account[\\/]' },
    @{ Name = "saved crashes"; Pattern = '[\\/]Saved[\\/]Crashes[\\/]' },
    @{ Name = "saved logs"; Pattern = '[\\/]Saved[\\/]Logs[\\/]' },
    @{ Name = "account backend env"; Pattern = '(^|[\\/])\.env$' },
    @{ Name = "account backend data"; Pattern = '[\\/]AccountBackend[\\/]data[\\/]' },
    @{ Name = "backend state secret"; Pattern = '(^|[\\/])state-secret\.txt$' },
    @{ Name = "backend player data"; Pattern = '(^|[\\/])players\.json$' },
    @{ Name = "local credential data"; Pattern = '(^|[\\/])local_credentials\.enc\.json$' },
    @{ Name = "local profile data"; Pattern = '(^|[\\/])profile\.json$' },
    @{ Name = "local progress data"; Pattern = '(^|[\\/])progress\.json$' }
)

$allFiles = Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Force
$failures = New-Object System.Collections.Generic.List[string]

foreach ($file in $allFiles) {
    $relative = $file.FullName.Substring($resolvedPackageRoot.Length).TrimStart('\', '/')
    foreach ($rule in $forbidden) {
        if ($relative -match $rule.Pattern) {
            $failures.Add("$($rule.Name): $relative")
        }
    }
}

$playitFiles = $allFiles | Where-Object { $_.Name -ieq "playit.exe" }
if (-not $playitFiles) {
    $failures.Add("playit.exe: missing from classroom package")
}
else {
    foreach ($playit in $playitFiles) {
        $hash = (Get-FileHash -LiteralPath $playit.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -ne $ExpectedPlayitSha256.ToLowerInvariant()) {
            $relative = $playit.FullName.Substring($resolvedPackageRoot.Length).TrimStart('\', '/')
            $failures.Add("playit.exe hash mismatch: $relative has $hash")
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Classroom package verification failed:`n" + ($failures -join "`n"))
    exit 1
}

Write-Host "Classroom package verification passed: $resolvedPackageRoot"
