param(
    [string]$OutputRoot = "$PSScriptRoot\..\Builds\MacTransfer",
    [switch]$ManifestOnly
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Test-IsPathInsideOrSame {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd('\')
    return $fullPath.Equals($fullRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($fullRoot + '\', [System.StringComparison]::OrdinalIgnoreCase)
}

$projectRoot = Resolve-Path "$PSScriptRoot\.."
$projectRootFull = [System.IO.Path]::GetFullPath($projectRoot)
$projectRootDrive = [System.IO.Path]::GetPathRoot($projectRootFull).TrimEnd('\')
if ($projectRootDrive -ine "D:") {
    throw "Refusing to create a Mac build input from outside D: drive: $projectRootFull"
}

if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    $outputRootFull = [System.IO.Path]::GetFullPath($OutputRoot)
}
else {
    $outputRootFull = [System.IO.Path]::GetFullPath((Join-Path $projectRootFull $OutputRoot))
}
if (-not (Test-IsPathInsideOrSame -Path $outputRootFull -Root $projectRootFull)) {
    throw "OutputRoot must stay inside the project root: $outputRootFull"
}
if ([System.IO.Path]::GetPathRoot($outputRootFull).TrimEnd('\') -ine "D:") {
    throw "OutputRoot must stay on D: drive: $outputRootFull"
}

$defaultOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRootFull "Builds\MacTransfer"))
if (-not (Test-IsPathInsideOrSame -Path $outputRootFull -Root $defaultOutputRoot)) {
    throw "OutputRoot must be under Builds\MacTransfer: $outputRootFull"
}

New-Item -ItemType Directory -Force -Path $outputRootFull | Out-Null

function Get-ProjectVersion {
    param([string]$ProjectRoot)

    $gameIni = Join-Path $ProjectRoot "Config\DefaultGame.ini"
    if (-not (Test-Path -LiteralPath $gameIni)) {
        return "unknown"
    }

    $match = Select-String -LiteralPath $gameIni -Pattern '^ProjectVersion=(.+)$' | Select-Object -First 1
    if (-not $match) {
        return "unknown"
    }

    return ($match.Matches[0].Groups[1].Value.Trim() -replace '[^\w\.\-]+', '_')
}

function Test-ExcludedRelativePath {
    param([string]$RelativePath)

    $parts = $RelativePath -split '[\\/]'
    foreach ($part in $parts) {
        if ($script:ExcludedDirectoryNames.Contains($part)) {
            return $true
        }
    }

    return $false
}

$script:ExcludedDirectoryNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    ".git",
    ".vs",
    "Binaries",
    "Builds",
    "DerivedDataCache",
    "Intermediate",
    "Saved"
) | ForEach-Object { [void]$script:ExcludedDirectoryNames.Add($_) }

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$version = Get-ProjectVersion -ProjectRoot $projectRootFull
$baseName = "BlackoutHunt-$version-MacBuildInput-$timestamp"
$archivePath = Join-Path $outputRootFull "$baseName.zip"
$manifestPath = Join-Path $outputRootFull "$baseName.manifest.tsv"
$metadataPath = Join-Path $outputRootFull "$baseName.metadata.json"
$rootPrefix = $projectRootFull.TrimEnd('\') + '\'

$includedFiles = New-Object System.Collections.Generic.List[object]
$script:totalBytes = [Int64]0

Get-ChildItem -LiteralPath $projectRootFull -Recurse -Force -File -ErrorAction SilentlyContinue |
    ForEach-Object {
        $relative = $_.FullName.Substring($rootPrefix.Length)
        if (Test-ExcludedRelativePath -RelativePath $relative) {
            return
        }

        $includedFiles.Add([pscustomobject]@{
            FullName = $_.FullName
            RelativePath = $relative
            Bytes = [Int64]$_.Length
        })
        $script:totalBytes += [Int64]$_.Length
    }

$includedFiles = @($includedFiles | Sort-Object RelativePath)

$manifestLines = New-Object System.Collections.Generic.List[string]
$manifestLines.Add("# Blackout Hunt macOS build input manifest")
$manifestLines.Add("# CreatedUtc`t$((Get-Date).ToUniversalTime().ToString('s'))Z")
$manifestLines.Add("# ProjectRoot`t$projectRootFull")
$manifestLines.Add("# Archive`t$archivePath")
$manifestLines.Add("# ExcludedDirectoryNames`t$(([string[]]$script:ExcludedDirectoryNames | Sort-Object) -join ',')")
$manifestLines.Add("# FileCount`t$($includedFiles.Count)")
$manifestLines.Add("# TotalBytes`t$($script:totalBytes)")
$manifestLines.Add("Bytes`tRelativePath")
foreach ($file in $includedFiles) {
    $manifestLines.Add("$($file.Bytes)`t$($file.RelativePath)")
}
Set-Content -LiteralPath $manifestPath -Value $manifestLines -Encoding UTF8

$metadata = [pscustomobject]@{
    createdUtc = (Get-Date).ToUniversalTime().ToString("o")
    projectRoot = $projectRootFull
    archivePath = if ($ManifestOnly) { $null } else { $archivePath }
    manifestPath = $manifestPath
    fileCount = $includedFiles.Count
    totalBytes = $script:totalBytes
    excludedDirectoryNames = @([string[]]$script:ExcludedDirectoryNames | Sort-Object)
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

if (-not $ManifestOnly) {
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }

    $zip = [System.IO.Compression.ZipFile]::Open($archivePath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $includedFiles) {
            $entryName = $file.RelativePath.Replace('\', '/')
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $zip,
                $file.FullName,
                $entryName,
                [System.IO.Compression.CompressionLevel]::Optimal
            ) | Out-Null
        }
    }
    finally {
        $zip.Dispose()
    }

    $hash = Get-FileHash -LiteralPath $archivePath -Algorithm SHA256
    Set-Content -LiteralPath "$archivePath.sha256" -Value "$($hash.Hash.ToLowerInvariant())  $(Split-Path -Leaf $archivePath)" -Encoding ASCII
}

[pscustomobject]@{
    Archive = if ($ManifestOnly) { "<manifest only>" } else { $archivePath }
    Manifest = $manifestPath
    Metadata = $metadataPath
    FileCount = $includedFiles.Count
    TotalGB = [math]::Round($script:totalBytes / 1GB, 2)
}
