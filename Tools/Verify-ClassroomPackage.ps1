param(
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\Windows",
    [string]$ExpectedPlayitSha256 = "88000d40af7a8e5a0548d27d71c0cad7d5f4b91fd85f6e9297237ac8b57fbdc9",
    [string]$ExpectedAppLocalDependencyRoot
)

$ErrorActionPreference = "Stop"

$resolvedPackageRoot = [System.IO.Path]::GetFullPath((Resolve-Path $PackageRoot))
$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))

if (-not $resolvedPackageRoot.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to verify package outside project root: $resolvedPackageRoot"
}

if (-not $ExpectedAppLocalDependencyRoot) {
    $unreal = & "$PSScriptRoot\Find-Unreal.ps1"
    $ExpectedAppLocalDependencyRoot = Join-Path $unreal.Root "Engine\Binaries\ThirdParty\AppLocalDependencies\Win64\x64"
}

$resolvedAppLocalDependencyRoot = [System.IO.Path]::GetFullPath((Resolve-Path $ExpectedAppLocalDependencyRoot))

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

$expectedAppLocalDlls = @(
    Get-ChildItem -LiteralPath $resolvedAppLocalDependencyRoot -Recurse -File -Filter "*.dll" -Force |
        ForEach-Object { $_.Name.ToLowerInvariant() } |
        Sort-Object -Unique
)

$requiredAppLocalDlls = @(
    "msvcp140.dll",
    "vcruntime140.dll",
    "vcruntime140_1.dll",
    "ucrtbase.dll"
)

if ($expectedAppLocalDlls.Count -eq 0) {
    $failures.Add("app-local runtime dependencies: no DLLs found under $resolvedAppLocalDependencyRoot")
}

foreach ($requiredDll in $requiredAppLocalDlls) {
    if ($expectedAppLocalDlls -notcontains $requiredDll) {
        $failures.Add("app-local runtime source is missing required DLL: $requiredDll")
    }
}

$rootLaunchers = @(
    Get-ChildItem -LiteralPath $resolvedPackageRoot -File -Filter "*.exe" -Force |
        Where-Object { $_.Name -ine "vc_redist.x64.exe" -and $_.Name -ine "vc_redist.arm64.exe" }
)

if ($rootLaunchers.Count -eq 0) {
    $failures.Add("app-local runtime dependencies: no root package launcher executable found")
}
else {
    $rootDlls = @(
        Get-ChildItem -LiteralPath $resolvedPackageRoot -File -Filter "*.dll" -Force |
            ForEach-Object { $_.Name.ToLowerInvariant() } |
            Sort-Object -Unique
    )

    foreach ($expectedDll in $expectedAppLocalDlls) {
        if ($rootDlls -notcontains $expectedDll) {
            $failures.Add("missing app-local runtime DLL beside root launcher: $expectedDll")
        }
    }
}

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

$shippingExecutables = @(
    $allFiles | Where-Object {
        $_.Name -imatch '-Win64-Shipping\.exe$' -and
        $_.FullName -match '[\\/]Binaries[\\/]Win64[\\/]'
    }
)

if ($shippingExecutables.Count -eq 0) {
    $failures.Add("app-local runtime dependencies: no Win64 Shipping executable found under Binaries\Win64")
}
else {
    foreach ($shippingExecutable in $shippingExecutables) {
        $binaryDir = Split-Path -Parent $shippingExecutable.FullName
        $relativeBinaryDir = $binaryDir.Substring($resolvedPackageRoot.Length).TrimStart('\', '/')
        $packagedDlls = @(
            Get-ChildItem -LiteralPath $binaryDir -File -Force |
                ForEach-Object { $_.Name.ToLowerInvariant() } |
                Sort-Object -Unique
        )

        foreach ($expectedDll in $expectedAppLocalDlls) {
            if ($packagedDlls -notcontains $expectedDll) {
                $failures.Add("missing app-local runtime DLL in ${relativeBinaryDir}: $expectedDll")
            }
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Error ("Classroom package verification failed:`n" + ($failures -join "`n"))
    exit 1
}

Write-Host "Classroom package verification passed: $resolvedPackageRoot"
