param(
    [string]$SourceRoot = (Join-Path $PSScriptRoot '..\Source\BlackoutHunt'),
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\Builds\Support\code-health.md'),
    [int]$Top = 20
)

$ErrorActionPreference = 'Stop'

$resolvedSource = Resolve-Path $SourceRoot
$resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
$outputDir = Split-Path -Parent $resolvedOutput
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Count-TextLines {
    param([Parameter(Mandatory = $true)][string]$Path)

    $reader = [System.IO.File]::OpenText($Path)
    try {
        $count = 0
        while ($null -ne $reader.ReadLine()) {
            $count++
        }
        return $count
    }
    finally {
        $reader.Dispose()
    }
}

$files = Get-ChildItem -Path $resolvedSource -Recurse -File -Include *.h,*.cpp |
    ForEach-Object {
        $lineCount = Count-TextLines -Path $_.FullName
        [pscustomobject]@{
            File = Resolve-Path -Relative $_.FullName
            Lines = $lineCount
        }
    } |
    Sort-Object -Property Lines -Descending

$generatedAt = Get-Date -Format 'yyyy-MM-dd HH:mm:ss K'
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('# Blackout Hunt Code Health Snapshot')
$lines.Add('')
$lines.Add("- Generated: $generatedAt")
$lines.Add("- Source root: $resolvedSource")
$lines.Add('')
$lines.Add('## Largest Files')
$lines.Add('')
$lines.Add('| Lines | File |')
$lines.Add('| ---: | --- |')
foreach ($file in ($files | Select-Object -First $Top)) {
    $lines.Add("| $($file.Lines) | $($file.File) |")
}
$lines.Add('')
$lines.Add('## Suggested First Splits')
$lines.Add('')
$lines.Add('- `BHGameMode.cpp`: extract runtime map builders, revision/classroom flow, scare director bridge, bot services, train/final escape, and reporting.')
$lines.Add('- `SBHMainMenu.cpp`: extract tab panels after controls stabilize.')
$lines.Add('- Keep split PRs behavior-preserving and build after each step.')

$lines | Set-Content -LiteralPath $resolvedOutput -Encoding UTF8
Write-Host "Code health snapshot: $resolvedOutput"
