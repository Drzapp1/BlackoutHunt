param(
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\Windows",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Resolve-PackageRoot {
    param([string]$Root)

    $resolved = [System.IO.Path]::GetFullPath((Resolve-Path $Root))
    if ((Test-Path -LiteralPath (Join-Path $resolved "BlackoutHunt.exe")) -and
        (Test-Path -LiteralPath (Join-Path $resolved "BlackoutHunt"))) {
        return $resolved
    }

    $parent = Split-Path -Parent $resolved
    if ((Split-Path -Leaf $resolved) -ieq "BlackoutHunt" -and
        (Test-Path -LiteralPath (Join-Path $parent "BlackoutHunt.exe"))) {
        return $parent
    }

    throw "PackageRoot must point at a packaged Windows root containing BlackoutHunt.exe: $Root"
}

function Test-ManifestAsset {
    param(
        [string]$ManifestPath,
        [string]$AssetPath
    )

    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        return $false
    }

    $escaped = [regex]::Escape($AssetPath).Replace("/", "[\\/]")
    return [bool](Select-String -LiteralPath $ManifestPath -Pattern $escaped -Quiet)
}

function New-JumpscareVariantLine {
    param(
        [string]$VariantId,
        [string]$DisplayName,
        [double]$Weight,
        [int]$MinimumScareIntensity,
        [string]$SkeletalMesh,
        [string]$RunAnimation,
        [string]$StaticMesh,
        [string]$Material,
        [string]$LaunchSound,
        [double]$VisualScale,
        [double]$CloseScale,
        [string]$LightColor,
        [double]$FocusHeight,
        [double]$CameraShakeIntensity,
        [double]$FlashIntensity,
        [double]$CameraJitterDuration
    )

    $visualOffset = '(X=0.000000,Y=0.000000,Z=-56.000000)'
    $visualRotation = '(Pitch=0.000000,Yaw=-90.000000,Roll=0.000000)'
    $visualScaleText = "(X=$($VisualScale.ToString('0.000000')),Y=$($VisualScale.ToString('0.000000')),Z=$($VisualScale.ToString('0.000000')))"
    $closeOffset = '(X=92.000000,Y=0.000000,Z=-104.000000)'
    $closeRotation = '(Pitch=0.000000,Yaw=0.000000,Roll=0.000000)'
    $closeScaleText = "(X=$($CloseScale.ToString('0.000000')),Y=$($CloseScale.ToString('0.000000')),Z=$($CloseScale.ToString('0.000000')))"

    return '+JumpscareVariants=(' +
        "VariantId=`"$VariantId`"," +
        "DisplayName=`"$DisplayName`"," +
        "Weight=$($Weight.ToString('0.00'))," +
        "MinimumScareIntensity=$MinimumScareIntensity," +
        "SkeletalMesh=`"$SkeletalMesh`"," +
        "RunAnimation=`"$RunAnimation`"," +
        "StaticMesh=`"$StaticMesh`"," +
        "Material=`"$Material`"," +
        'VisualActorClass=None,' +
        "LaunchSound=`"$LaunchSound`"," +
        "VisualOffset=$visualOffset," +
        "VisualRotation=$visualRotation," +
        "VisualScale=$visualScaleText," +
        "CloseVisualOffset=$closeOffset," +
        "CloseVisualRotation=$closeRotation," +
        "CloseVisualScale=$closeScaleText," +
        "LightColor=$LightColor," +
        "FocusHeight=$($FocusHeight.ToString('0.0'))," +
        "CameraShakeIntensity=$($CameraShakeIntensity.ToString('0.00'))," +
        "FlashIntensity=$($FlashIntensity.ToString('0.00'))," +
        "CameraJitterDuration=$($CameraJitterDuration.ToString('0.00'))" +
        ')'
}

$resolvedPackageRoot = Resolve-PackageRoot -Root $PackageRoot
$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))
if (-not $resolvedPackageRoot.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to patch a package outside the project root: $resolvedPackageRoot"
}

$manifestPath = Join-Path $resolvedPackageRoot "Manifest_UFSFiles_Win64.txt"
$gameConfigDir = Join-Path $resolvedPackageRoot "BlackoutHunt\Saved\Config\Windows"
$gameConfigPath = Join-Path $gameConfigDir "Game.ini"

$fab01Mesh = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes/Skeletal_Meshes/SKM_Monster_01.SKM_Monster_01"
$fab01Anim = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations/AS_Monster_01_Idle.AS_Monster_01_Idle"
$fab01Material = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Materials/M_Monster_01.M_Monster_01"
$fab01Sound = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds/SW_Jumpscare_01.SW_Jumpscare_01"
$fab02Mesh = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes/Skeletal_Meshes/SKM_Monster_02.SKM_Monster_02"
$fab02Anim = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations/AS_Monster_02_Walk.AS_Monster_02_Walk"
$fab02Material = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Materials/M_Monster_02.M_Monster_02"
$fab02Sound = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds/SW_Jumpscare_02.SW_Jumpscare_02"
$fab03Mesh = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes/Skeletal_Meshes/SKM_Monster_03.SKM_Monster_03"
$fab03Anim = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Animations/AS_Monster_03_Jumpscare.AS_Monster_03_Jumpscare"
$fab03Material = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Materials/M_Monster_03.M_Monster_03"
$fab03Sound = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds/SW_Jumpscare_03.SW_Jumpscare_03"
$fallbackSound = "/Game/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.SW_TerrifiedScreamFaint"

$hasFab01Mesh = Test-ManifestAsset -ManifestPath $manifestPath -AssetPath "BlackoutHunt/Content/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Meshes/Skeletal_Meshes/SKM_Monster_01.uasset"

$beginMarker = "; BEGIN BLACKOUTHUNT JUMPSCARE NO-COOK HOTFIX"
$endMarker = "; END BLACKOUTHUNT JUMPSCARE NO-COOK HOTFIX"
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add($beginMarker)
$lines.Add("; Generated by Tools/Apply-JumpscareNoCookHotfix.ps1. No cook or package step is required.")
$lines.Add("[/Script/BlackoutHunt.BHGameSettings]")
$lines.Add("!JumpscareVariants=ClearArray")
$lines.Add((New-JumpscareVariantLine -VariantId "ProxyRed" -DisplayName "Procedural Red Proxy No-Cook" -Weight 1.00 -MinimumScareIntensity 0 -SkeletalMesh "" -RunAnimation "" -StaticMesh "" -Material "" -LaunchSound $fallbackSound -VisualScale 1.00 -CloseScale 1.34 -LightColor "(R=1.000000,G=0.020000,B=0.000000,A=1.000000)" -FocusHeight 145.0 -CameraShakeIntensity 0.96 -FlashIntensity 0.70 -CameraJitterDuration 1.10))
$lines.Add((New-JumpscareVariantLine -VariantId "ProxyCyan" -DisplayName "Procedural Cyan Proxy No-Cook" -Weight 0.72 -MinimumScareIntensity 1 -SkeletalMesh "" -RunAnimation "" -StaticMesh "" -Material "" -LaunchSound $fallbackSound -VisualScale 1.00 -CloseScale 1.40 -LightColor "(R=0.000000,G=0.780000,B=1.000000,A=1.000000)" -FocusHeight 154.0 -CameraShakeIntensity 0.86 -FlashIntensity 0.58 -CameraJitterDuration 1.00))
$lines.Add((New-JumpscareVariantLine -VariantId "ProxyViolet" -DisplayName "Procedural Violet Proxy No-Cook" -Weight 0.64 -MinimumScareIntensity 2 -SkeletalMesh "" -RunAnimation "" -StaticMesh "" -Material "" -LaunchSound $fallbackSound -VisualScale 1.00 -CloseScale 1.46 -LightColor "(R=0.780000,G=0.030000,B=1.000000,A=1.000000)" -FocusHeight 162.0 -CameraShakeIntensity 0.92 -FlashIntensity 0.64 -CameraJitterDuration 1.10))
$lines.Add((New-JumpscareVariantLine -VariantId "FabMonster01" -DisplayName "Free Jumpscare 01 No-Cook" -Weight 1.20 -MinimumScareIntensity 1 -SkeletalMesh $fab01Mesh -RunAnimation $fab01Anim -StaticMesh "" -Material $fab01Material -LaunchSound $fab01Sound -VisualScale 0.40 -CloseScale 0.56 -LightColor "(R=1.000000,G=0.000000,B=0.020000,A=1.000000)" -FocusHeight 138.0 -CameraShakeIntensity 0.92 -FlashIntensity 0.76 -CameraJitterDuration 1.05))
$lines.Add((New-JumpscareVariantLine -VariantId "FabMonster02" -DisplayName "Free Jumpscare 02 No-Cook" -Weight 1.15 -MinimumScareIntensity 1 -SkeletalMesh $fab02Mesh -RunAnimation $fab02Anim -StaticMesh "" -Material $fab02Material -LaunchSound $fab02Sound -VisualScale 0.38 -CloseScale 0.56 -LightColor "(R=0.780000,G=0.030000,B=1.000000,A=1.000000)" -FocusHeight 142.0 -CameraShakeIntensity 0.92 -FlashIntensity 0.72 -CameraJitterDuration 1.00))
$lines.Add((New-JumpscareVariantLine -VariantId "FabMonster03" -DisplayName "Free Jumpscare 03 No-Cook" -Weight 1.10 -MinimumScareIntensity 2 -SkeletalMesh $fab03Mesh -RunAnimation $fab03Anim -StaticMesh "" -Material $fab03Material -LaunchSound $fab03Sound -VisualScale 0.36 -CloseScale 0.54 -LightColor "(R=0.000000,G=0.780000,B=1.000000,A=1.000000)" -FocusHeight 148.0 -CameraShakeIntensity 0.94 -FlashIntensity 0.78 -CameraJitterDuration 1.10))
$lines.Add($endMarker)
$hotfixBlock = ($lines -join [Environment]::NewLine) + [Environment]::NewLine

$existing = ""
if (Test-Path -LiteralPath $gameConfigPath) {
    $existing = Get-Content -LiteralPath $gameConfigPath -Raw
    $pattern = "(?s)\r?\n?$([regex]::Escape($beginMarker)).*?$([regex]::Escape($endMarker))\r?\n?"
    $existing = [regex]::Replace($existing, $pattern, "")
}

$newContent = $existing.TrimEnd() + [Environment]::NewLine + [Environment]::NewLine + $hotfixBlock

if ($DryRun) {
    Write-Host "Would write no-cook jumpscare hotfix to $gameConfigPath"
    Write-Host $hotfixBlock
    exit 0
}

New-Item -ItemType Directory -Path $gameConfigDir -Force | Out-Null
if (Test-Path -LiteralPath $gameConfigPath) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    Copy-Item -LiteralPath $gameConfigPath -Destination "$gameConfigPath.bak-$timestamp" -Force
}

Set-Content -LiteralPath $gameConfigPath -Value $newContent -Encoding ASCII
Write-Host "Applied no-cook jumpscare hotfix:"
Write-Host "  package: $resolvedPackageRoot"
Write-Host "  config:  $gameConfigPath"
Write-Host "  fallback visuals: procedural proxy variants"
Write-Host "  optional Fab visuals: $(if ($hasFab01Mesh) { 'package manifest contains FabMonster01' } else { 'not found; runtime will keep proxy visuals' })"
