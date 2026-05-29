param(
    [string[]]$Path = @("$PSScriptRoot\..\Builds\Windows\BlackoutHunt\Saved\Logs"),
    [switch]$ExpectAutomationMarkers,
    [string[]]$RequiredAutomationMarker = @("BH_AUTOMATION_BOOT", "CLEAN_QUIT"),
    [string]$AutomationTag = "",
    [int]$SoftLoadWarningLimit = 12,
    [string[]]$AllowedFindingPattern = @()
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))
$logExtensions = @(".log", ".txt", ".err", ".out")
$findings = New-Object System.Collections.Generic.List[object]

$denyRules = @(
    @{
        Name = "crash or fatal error"
        Pattern = "(?i)(Fatal error:|LowLevelFatalError|Unhandled Exception|EXCEPTION_(ACCESS_VIOLATION|STACK_OVERFLOW|ILLEGAL_INSTRUCTION)|CrashReportClient.*(?:Error|Fatal|crashed)|appError called)"
    },
    @{
        Name = "assert or ensure"
        Pattern = "(?i)(Assertion failed:|Ensure condition failed|=== Handled ensure|LogOutputDevice:\s*Error:\s*Ensure)"
    },
    @{
        Name = "missing runtime DLL"
        Pattern = "(?i)(Unable to load required.*\.dll|Failed to load required.*\.dll|delay-load module.*could not be loaded|The code execution cannot proceed because .*\.dll|VCRUNTIME\d*.*not found|MSVCP\d*.*not found|ucrtbase\.dll.*not found|Missing import.*\.dll)"
    },
    @{
        Name = "net driver or travel failure"
        Pattern = "(?i)(NetworkFailure:|TravelFailure:|PendingNetGameCreateFailure|NetDriver.*CreateFailure|Failed to create net driver|CreateNamedNetDriver failed|Failed to init net driver|Failed to listen|binding to port .* failed|ConnectionTimeout|PendingConnectionFailure|ServerTravelFailure|ClientTravelFailure|LoadMapFailure)"
    },
    @{
        Name = "required asset or map load error"
        Pattern = "(?i)(Log(?:Linker|Streaming|UObjectGlobals|PackageName|AssetRegistry):\s*Error:.*(Failed to load|Failed to find|Can.?t find file|Couldn.?t find file|Missing Package|Unable to find package|does not exist)|Failed to load map|Failed to enter .*map|Failed to load package .*\.(umap|uasset))"
    },
    @{
        Name = "severe runtime log error"
        Pattern = "(?i)(LogWindows:\s*Error:|LogOutputDevice:\s*Error:|LogPakFile:\s*Error:|LogSerialization:\s*Error:|LogStreaming:\s*Error:|LogLinker:\s*Error:)"
    },
    @{
        Name = "automation failure marker"
        Pattern = "(?i)(BH_AUTOMATION.*FAIL:|(?:^|\s)FAIL:(?:join timeout|invalid join address|unknown host mode|auto host requires standalone menu|auto|unknown)|Automation Test Failed|LogAutomation(?:Controller|CommandLine):\s*Error:|Automation.*(?:FAILED|Failed))"
    }
)

$softLoadPattern = "(?i)(Log(?:Linker|UObjectGlobals|Streaming|PackageName|AssetRegistry|StreamingManager):\s*Warning:.*(Failed to load|Failed to find|Can.?t find file|Couldn.?t find file|Missing Package|Unable to find package|Failed to resolve|does not exist)|Failed to load /Game/|Can.?t find file for package|Unable to find package for cooking|Soft Object Path.*Fail)"

function Get-DisplayPath {
    param([Parameter(Mandatory = $true)][string]$InputPath)

    $root = $projectRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $full = [System.IO.Path]::GetFullPath($InputPath)
    if ($full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $full.Substring($root.Length)
    }

    if (-not [string]::IsNullOrWhiteSpace($env:USERPROFILE)) {
        $userRoot = [System.IO.Path]::GetFullPath($env:USERPROFILE).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
        if ($full.StartsWith($userRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            return "%USERPROFILE%\" + $full.Substring($userRoot.Length)
        }
    }

    return $full
}

function ConvertTo-SafeLogLine {
    param([AllowEmptyString()][string]$Line)

    $safe = [string]$Line
    $safe = [regex]::Replace($safe, "BH1:[A-Za-z0-9+/_=:.%~#?&-]+", "BH1:<redacted>")
    $safe = [regex]::Replace($safe, "(?i)\b(?:[a-z0-9-]+\.)*playit\.(?:gg|cloud|dev)\S*", "<playit-endpoint-redacted>")
    $safe = [regex]::Replace($safe, "(?<!\d)(10\.\d{1,3}\.\d{1,3}\.\d{1,3}|192\.168\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[0-1])\.\d{1,3}\.\d{1,3})(?::\d+)?", "<private-ip>")
    $safe = [regex]::Replace($safe, "(?i)\b(token|secret|password|credential|api[_-]?key|oauth)[=:]\S+", '${1}=<redacted>')
    $safe = [regex]::Replace($safe, "(?i)(Authorization:\s*Bearer\s+)\S+", '${1}<redacted>')

    if ($safe.Length -gt 360) {
        return $safe.Substring(0, 357) + "..."
    }

    return $safe
}

function Test-FindingAllowed {
    param([Parameter(Mandatory = $true)]$Finding)

    foreach ($pattern in $AllowedFindingPattern) {
        if ([string]::IsNullOrWhiteSpace($pattern)) {
            continue
        }

        if ($Finding.Rule -match $pattern -or $Finding.Path -match $pattern -or $Finding.Line -match $pattern) {
            return $true
        }
    }

    return $false
}

function Add-Finding {
    param(
        [Parameter(Mandatory = $true)][string]$Rule,
        [Parameter(Mandatory = $true)][string]$File,
        [int]$LineNumber = 0,
        [AllowEmptyString()][string]$Line = ""
    )

    $finding = [pscustomobject]@{
        Rule = $Rule
        Path = Get-DisplayPath -InputPath $File
        LineNumber = $LineNumber
        Line = ConvertTo-SafeLogLine -Line $Line
    }

    if (-not (Test-FindingAllowed -Finding $finding)) {
        $findings.Add($finding)
    }
}

function Resolve-LogFiles {
    param([Parameter(Mandatory = $true)][string[]]$InputPaths)

    $files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    foreach ($inputPath in $InputPaths) {
        if (-not (Test-Path -LiteralPath $inputPath)) {
            throw "Runtime log path does not exist: $inputPath"
        }

        $item = Get-Item -LiteralPath $inputPath -Force
        if ($item.PSIsContainer) {
            Get-ChildItem -LiteralPath $item.FullName -Recurse -File -Force |
                Where-Object { $logExtensions -contains $_.Extension.ToLowerInvariant() } |
                ForEach-Object { $files.Add($_) }
        }
        elseif ($logExtensions -contains $item.Extension.ToLowerInvariant()) {
            $files.Add($item)
        }
    }

    return @($files | Sort-Object FullName -Unique)
}

function Test-AutomationMarkers {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo[]]$LogFiles)

    if (-not $ExpectAutomationMarkers) {
        return
    }

    $allMarkerLines = New-Object System.Collections.Generic.List[object]
    $markerSearchTerms = @("FAIL:", "BH_AUTOMATION_BOOT", "HOST_LISTENING", "JOIN_ATTEMPT", "JOINED", "READY_SET", "ROUND_STARTED", "CLEAN_QUIT")
    $markerSearchTerms += @($RequiredAutomationMarker | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    $markerSearchPattern = (($markerSearchTerms | Sort-Object -Unique) | ForEach-Object { [regex]::Escape($_) }) -join "|"
    foreach ($logFile in $LogFiles) {
        $matches = @(Select-String -LiteralPath $logFile.FullName -Pattern $markerSearchPattern -ErrorAction SilentlyContinue)
        foreach ($match in $matches) {
            if (-not [string]::IsNullOrWhiteSpace($AutomationTag) -and $match.Line -notmatch [regex]::Escape("Tag=$AutomationTag")) {
                continue
            }

            $allMarkerLines.Add([pscustomobject]@{
                File = $logFile.FullName
                LineNumber = $match.LineNumber
                Line = $match.Line
            })
        }
    }

    foreach ($marker in $RequiredAutomationMarker) {
        if ([string]::IsNullOrWhiteSpace($marker)) {
            continue
        }

        $markerPattern = [regex]::Escape($marker)
        $found = $false
        foreach ($markerLine in $allMarkerLines) {
            if ($markerLine.Line -match $markerPattern) {
                $found = $true
                break
            }
        }

        if (-not $found) {
            $scope = if ([string]::IsNullOrWhiteSpace($AutomationTag)) { "any automation tag" } else { "Tag=$AutomationTag" }
            Add-Finding -Rule "missing automation marker" -File ($LogFiles[0].FullName) -Line "Required marker '$marker' was not found for $scope."
        }
    }
}

$logFiles = @(Resolve-LogFiles -InputPaths $Path)
if ($logFiles.Count -eq 0) {
    throw "Runtime log gate found no log files under: $($Path -join ', ')"
}

foreach ($logFile in $logFiles) {
    foreach ($rule in $denyRules) {
        $match = Select-String -LiteralPath $logFile.FullName -Pattern $rule.Pattern -List -ErrorAction SilentlyContinue
        if ($match) {
            Add-Finding -Rule $rule.Name -File $logFile.FullName -LineNumber $match.LineNumber -Line $match.Line
        }
    }

    if ($SoftLoadWarningLimit -gt 0) {
        $softLoadMatches = @(Select-String -LiteralPath $logFile.FullName -Pattern $softLoadPattern -ErrorAction SilentlyContinue)
        if ($softLoadMatches.Count -ge $SoftLoadWarningLimit) {
            $first = $softLoadMatches[0]
            Add-Finding -Rule "repeated soft-load warning spam" -File $logFile.FullName -LineNumber $first.LineNumber -Line "Matched $($softLoadMatches.Count) soft-load warnings; first: $($first.Line)"
        }
    }
}

Test-AutomationMarkers -LogFiles $logFiles

if ($findings.Count -gt 0) {
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("Runtime log gate failed:")
    foreach ($finding in ($findings | Select-Object -First 40)) {
        $location = if ($finding.LineNumber -gt 0) { "$($finding.Path):$($finding.LineNumber)" } else { $finding.Path }
        $lines.Add("- [$($finding.Rule)] ${location}: $($finding.Line)")
    }

    if ($findings.Count -gt 40) {
        $lines.Add("- ... $($findings.Count - 40) additional finding(s) omitted")
    }

    throw ($lines -join "`n")
}

Write-Host "Runtime log gate passed: scanned $($logFiles.Count) file(s)."
if ($ExpectAutomationMarkers) {
    $tagText = if ([string]::IsNullOrWhiteSpace($AutomationTag)) { "any tag" } else { "Tag=$AutomationTag" }
    Write-Host "Automation markers present for ${tagText}: $($RequiredAutomationMarker -join ', ')"
}
