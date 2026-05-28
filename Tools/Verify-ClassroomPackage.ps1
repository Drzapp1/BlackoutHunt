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
    @{ Name = "saved classroom reports"; Pattern = '[\\/]Saved[\\/]ClassReports[\\/]' },
    @{ Name = "saved playtest telemetry"; Pattern = '[\\/]Saved[\\/]PlaytestTelemetry[\\/]' },
    @{ Name = "account backend env"; Pattern = '(^|[\\/])\.env$' },
    @{ Name = "account backend data"; Pattern = '[\\/]AccountBackend[\\/]data[\\/]' },
    @{ Name = "backend state secret"; Pattern = '(^|[\\/])state-secret\.txt$' },
    @{ Name = "backend player data"; Pattern = '(^|[\\/])players\.json$' },
    @{ Name = "local credential data"; Pattern = '(^|[\\/])local_credentials\.enc\.json$' },
    @{ Name = "local profile data"; Pattern = '(^|[\\/])profile\.json$' },
    @{ Name = "local progress data"; Pattern = '(^|[\\/])progress\.json$' },
    @{ Name = "credential or secret file"; Pattern = '(^|[\\/])[^\\/]*(secret|credential|token|oauth|apikey|api_key)[^\\/]*\.(json|txt|ini|env|key|pem|pfx|ppk)$' },
    @{ Name = "source asset archive"; Pattern = '(^|[\\/])(fnati-classic-undying|low-poly-character-base-mesh|scp-096)\.zip$' },
    @{ Name = "raw content source file"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/].*\.(fbx|gltf|glb|blend|obj|psd|psb|ma|mb|max|zip)$' },
    @{ Name = "risky low-poly hider asset"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]BlackoutHunt[\\/]Art[\\/]Characters[\\/]Hider[\\/]' },
    @{ Name = "risky FNaTI hunter asset"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]BlackoutHunt[\\/]Art[\\/]Characters[\\/]Hunter[\\/]' },
    @{ Name = "raw imported art source directory"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]BlackoutHunt[\\/]Art[\\/].*[\\/]SourceFiles[\\/]' },
    @{ Name = "downloaded art source directory"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]BlackoutHunt[\\/]Art[\\/]Downloaded[\\/]' },
    @{ Name = "source Free_Jumpscares pack root"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]Free_Jumpscares[\\/]' },
    @{ Name = "source Free_Jumpscares external actor/object data"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]__(?:ExternalActors|ExternalObjects)__[\\/]Free_Jumpscares[\\/]' },
    @{ Name = "Free Customizable Jumpscares demo map"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]BlackoutHunt[\\/]Art[\\/]Jumpscares[\\/]FreeCustomizableJumpscares[\\/]Lvl_FirstPerson\.(uasset|umap|uexp|ubulk)$' },
    @{ Name = "Free Customizable Jumpscares demo external actor/object data"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]__(?:ExternalActors|ExternalObjects)__[\\/]BlackoutHunt[\\/]Art[\\/]Jumpscares[\\/]FreeCustomizableJumpscares[\\/]Lvl_FirstPerson[\\/]' },
    @{ Name = "Free Customizable Jumpscares demo input"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]BlackoutHunt[\\/]Art[\\/]Jumpscares[\\/]FreeCustomizableJumpscares[\\/]Input[\\/]' },
    @{ Name = "Free Customizable Jumpscares demo first-person content"; Pattern = '(^|[\\/])(?:BlackoutHunt[\\/])?Content[\\/]BlackoutHunt[\\/]Art[\\/]Jumpscares[\\/]FreeCustomizableJumpscares[\\/]Blueprints[\\/]FirstPerson[\\/]' }
)

$requiredAlwaysCookPaths = @(
    "/Game/BlackoutHunt/Art/Textures",
    "/Game/BlackoutHunt/Art/Materials",
    "/Game/BlackoutHunt/Art/UI",
    "/Game/BlackoutHunt/Audio",
    "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations",
    "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Materials",
    "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes",
    "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds",
    "/Game/BlackoutHunt/Art/Jumpscares/Whisper",
    "/Game/BlackoutHunt/Art/Characters/Quaternius",
    "/Game/BlackoutHunt/Art/Characters/FreeAnimationLibrary",
    "/Game/BlackoutHunt/Art/Weapons/KayKit",
    "/Game/BlackoutHunt/Data",
    "/Game/SmartBasicInterfaces",
    "/Game/A_Surface_Footstep/Niagara_FX",
    "/Game/ResidentHorrorV1/Audio",
    "/Game/FlashLight_System/Sound",
    "/Game/SecurityCameras/Sounds",
    "/Game/SoundsOfHorror/Impacts/CUE",
    "/Game/SoundsOfHorror/Rumbles/CUE"
)

$requiredNeverCookPaths = @(
    "/Game/BlackoutHunt/Art/Characters/Hider",
    "/Game/BlackoutHunt/Art/Characters/Hunter",
    "/Game/BlackoutHunt/Art/Downloaded",
    "/Game/Free_Jumpscares"
)

$forbiddenAlwaysCookPaths = @(
    "/Game/BlackoutHunt/Art/Jumpscares",
    "/Game/BlackoutHunt/Art/Characters",
    "/Game/BlackoutHunt/Art/Characters/Hider",
    "/Game/BlackoutHunt/Art/Characters/Hunter",
    "/Game/BlackoutHunt/Art/Downloaded",
    "/Game/Free_Jumpscares"
)

$requiredManifestCookedAssets = @(
    "BlackoutHunt/Content/BlackoutHunt/Art/SCP096/Skeletal/SK_SCP096.uasset",
    "BlackoutHunt/Content/BlackoutHunt/Art/SCP096/M_SCP096.uasset",
    "BlackoutHunt/Content/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.uasset"
)

$requiredAssetDocSnippets = @(
    "Risk decisions",
    "SCP096 prototype",
    "Low-poly Hider",
    "FNaTI Hunter",
    "__ExternalActors__",
    "PlaytestTelemetry",
    "Fab Standard License",
    "Attribution text for current package"
)

$requiredNoticeSnippets = @(
    "Blackout Hunt Third-Party Notices",
    "ambientCG",
    "Quaternius",
    "KayKit",
    "OpenGameArt",
    "Fab Free Customizable Jumpscares",
    "SCP096 prototype",
    "Excluded risky/prototype content"
)

$allFiles = Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Force
$failures = New-Object System.Collections.Generic.List[string]

function Add-ForbiddenPathFailures {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathText,
        [string]$Context = ""
    )

    foreach ($rule in $forbidden) {
        if ($PathText -match $rule.Pattern) {
            if ($Context) {
                $failures.Add("$($rule.Name) in ${Context}: $PathText")
            }
            else {
                $failures.Add("$($rule.Name): $PathText")
            }
        }
    }
}

function Test-ContainsPath {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Paths,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedPath
    )

    foreach ($path in $Paths) {
        if ($path -ieq $ExpectedPath) {
            return $true
        }
    }
    return $false
}

function Get-PackagingPathValues {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ConfigPath,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    if (-not (Test-Path -LiteralPath $ConfigPath)) {
        $failures.Add("packaging policy: missing config file $ConfigPath")
        return @()
    }

    $escapedKey = [regex]::Escape($Key)
    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($line in Get-Content -LiteralPath $ConfigPath) {
        if ($line -match "^\s*\+?$escapedKey=\(Path=""([^""]+)""\)") {
            $paths.Add($Matches[1])
        }
    }
    return @($paths)
}

function Test-PackagingPolicyConfig {
    $defaultGame = Join-Path $projectRoot "Config\DefaultGame.ini"
    $alwaysCook = @(Get-PackagingPathValues -ConfigPath $defaultGame -Key "DirectoriesToAlwaysCook")
    $neverCook = @(Get-PackagingPathValues -ConfigPath $defaultGame -Key "DirectoriesToNeverCook")
    $alwaysStageNonUfs = @(Get-PackagingPathValues -ConfigPath $defaultGame -Key "DirectoriesToAlwaysStageAsNonUFS")

    foreach ($requiredPath in $requiredAlwaysCookPaths) {
        if (-not (Test-ContainsPath -Paths $alwaysCook -ExpectedPath $requiredPath)) {
            $failures.Add("packaging policy: missing required DirectoriesToAlwaysCook path $requiredPath")
        }
    }

    foreach ($requiredPath in $requiredNeverCookPaths) {
        if (-not (Test-ContainsPath -Paths $neverCook -ExpectedPath $requiredPath)) {
            $failures.Add("packaging policy: missing required DirectoriesToNeverCook path $requiredPath")
        }
    }

    foreach ($cookPath in $alwaysCook) {
        foreach ($forbiddenCookPath in $forbiddenAlwaysCookPaths) {
            if ($cookPath -ieq $forbiddenCookPath) {
                $failures.Add("packaging policy: forbidden broad/risky DirectoriesToAlwaysCook path $cookPath")
            }
        }

        if ($cookPath -match '/(SourceFiles|Downloaded)($|/)' -or $cookPath -match '/Characters/(Hider|Hunter)($|/)' -or $cookPath -ieq "/Game/Free_Jumpscares") {
            $failures.Add("packaging policy: DirectoriesToAlwaysCook includes risky source path $cookPath")
        }
    }

    foreach ($stagePath in $alwaysStageNonUfs) {
        if ($stagePath -ine "BlackoutHunt/Art/UI") {
            $failures.Add("packaging policy: unexpected DirectoriesToAlwaysStageAsNonUFS path $stagePath")
        }
    }
}

function Test-AssetPolicyDocs {
    $assetDoc = Join-Path $projectRoot "Docs\ASSETS.md"
    if (-not (Test-Path -LiteralPath $assetDoc)) {
        $failures.Add("asset policy docs: missing Docs\ASSETS.md")
        return
    }

    $docText = Get-Content -LiteralPath $assetDoc -Raw
    foreach ($snippet in $requiredAssetDocSnippets) {
        if ($docText.IndexOf($snippet, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            $failures.Add("asset policy docs: missing required note '$snippet'")
        }
    }

    $defaultGame = Join-Path $projectRoot "Config\DefaultGame.ini"
    $documentedPolicyPaths = @()
    $documentedPolicyPaths += @(Get-PackagingPathValues -ConfigPath $defaultGame -Key "DirectoriesToAlwaysCook")
    $documentedPolicyPaths += @(Get-PackagingPathValues -ConfigPath $defaultGame -Key "DirectoriesToNeverCook")
    $documentedPolicyPaths += @(Get-PackagingPathValues -ConfigPath $defaultGame -Key "DirectoriesToAlwaysStageAsNonUFS")

    foreach ($policyPath in ($documentedPolicyPaths | Sort-Object -Unique)) {
        if ($docText.IndexOf($policyPath, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            $failures.Add("asset policy docs: Docs\ASSETS.md does not document package path $policyPath")
        }
    }
}

function Test-ThirdPartyNoticesSource {
    $noticePath = Join-Path $projectRoot "Docs\THIRD_PARTY_NOTICES.txt"
    if (-not (Test-Path -LiteralPath $noticePath)) {
        $failures.Add("third-party notices: missing Docs\THIRD_PARTY_NOTICES.txt")
        return
    }

    $noticeText = Get-Content -LiteralPath $noticePath -Raw
    foreach ($snippet in $requiredNoticeSnippets) {
        if ($noticeText.IndexOf($snippet, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            $failures.Add("third-party notices: missing required note '$snippet'")
        }
    }
}

function Test-PackagedThirdPartyNotices {
    $noticePath = Join-Path $resolvedPackageRoot "THIRD-PARTY-NOTICES.txt"
    if (-not (Test-Path -LiteralPath $noticePath)) {
        $failures.Add("third-party notices: missing packaged THIRD-PARTY-NOTICES.txt")
        return
    }

    $noticeText = Get-Content -LiteralPath $noticePath -Raw
    foreach ($snippet in $requiredNoticeSnippets) {
        if ($noticeText.IndexOf($snippet, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            $failures.Add("third-party notices: packaged THIRD-PARTY-NOTICES.txt missing required note '$snippet'")
        }
    }
}

Test-PackagingPolicyConfig
Test-AssetPolicyDocs
Test-ThirdPartyNoticesSource
Test-PackagedThirdPartyNotices

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
    Add-ForbiddenPathFailures -PathText $relative
}

$manifestFiles = @(
    $allFiles | Where-Object { $_.Name -imatch '^Manifest_.*_Win64\.txt$' }
)

if ($manifestFiles.Count -eq 0) {
    $failures.Add("package manifests: no Manifest_*_Win64.txt files found; cannot audit staged cooked content")
}

$manifestPathSet = New-Object -TypeName 'System.Collections.Generic.HashSet[string]' -ArgumentList ([System.StringComparer]::OrdinalIgnoreCase)
foreach ($manifestFile in $manifestFiles) {
    $manifestRelative = $manifestFile.FullName.Substring($resolvedPackageRoot.Length).TrimStart('\', '/')
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $manifestFile.FullName) {
        $lineNumber++
        $manifestPath = ($line -split "`t", 2)[0]
        if (-not [string]::IsNullOrWhiteSpace($manifestPath)) {
            $normalizedManifestPath = $manifestPath.Replace('\', '/')
            [void]$manifestPathSet.Add($normalizedManifestPath)
            Add-ForbiddenPathFailures -PathText $normalizedManifestPath -Context "${manifestRelative}:$lineNumber"
        }
    }
}

foreach ($requiredCookedAsset in $requiredManifestCookedAssets) {
    if (-not $manifestPathSet.Contains($requiredCookedAsset)) {
        $failures.Add("package manifests: missing required beta cooked asset $requiredCookedAsset")
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
