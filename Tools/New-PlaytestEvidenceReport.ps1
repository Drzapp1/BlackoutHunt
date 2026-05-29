param(
    [string[]]$InputPath = @("$PSScriptRoot\..\Saved\PlaytestTelemetry"),
    [string]$OutputRoot = "$PSScriptRoot\..\Saved\PlaytestEvidence",
    [int]$BucketSize = 1000,
    [int]$Top = 8
)

$ErrorActionPreference = "Stop"

if ($BucketSize -le 0) {
    throw "BucketSize must be greater than zero."
}

if ($Top -le 0) {
    $Top = 8
}

$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))
$outputRootFull = [System.IO.Path]::GetFullPath($OutputRoot)
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$reportPath = Join-Path $outputRootFull "MapPlaytestEvidence-$timestamp.md"
$invariantCulture = [System.Globalization.CultureInfo]::InvariantCulture

New-Item -ItemType Directory -Force -Path $outputRootFull | Out-Null

function Get-DisplayPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    try {
        $full = [System.IO.Path]::GetFullPath($Path)
        $projectPrefix = $projectRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
        if ($full.StartsWith($projectPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $full.Substring($projectPrefix.Length)
        }
        return Split-Path -Leaf $full
    }
    catch {
        return Split-Path -Leaf $Path
    }
}

function Test-TelemetryEventCsv {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }

    $header = Get-Content -LiteralPath $Path -TotalCount 1 -ErrorAction Stop
    return $header -match '(^|,)EventType(,|$)' -and
        $header -match '(^|,)Map(,|$)' -and
        $header -match '(^|,)Phase(,|$)' -and
        $header -match '(^|,)X(,|$)' -and
        $header -match '(^|,)Y(,|$)'
}

function Get-CsvField {
    param(
        [Parameter(Mandatory = $true)]$Row,
        [Parameter(Mandatory = $true)][string]$Name,
        [AllowEmptyString()][string]$Default = ""
    )

    $property = $Row.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return $Default
    }

    $value = ([string]$property.Value).Trim()
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $Default
    }

    return $value
}

function Convert-ToDoubleOrZero {
    param([AllowEmptyString()][string]$Text)

    $number = 0.0
    if ([double]::TryParse($Text, [System.Globalization.NumberStyles]::Float, $invariantCulture, [ref]$number)) {
        return $number
    }
    return 0.0
}

function Convert-ToIntOrDefault {
    param(
        [AllowEmptyString()][string]$Text,
        [int]$Default = 1
    )

    $number = 0
    if ([int]::TryParse($Text, [System.Globalization.NumberStyles]::Integer, $invariantCulture, [ref]$number)) {
        return [Math]::Max(1, $number)
    }
    return $Default
}

function Get-BucketIndex {
    param(
        [double]$Value,
        [int]$Size
    )

    return [int][Math]::Floor($Value / [double]$Size)
}

function Get-BucketLabel {
    param(
        [int]$BucketX,
        [int]$BucketY,
        [int]$BucketZ,
        [int]$Size
    )

    return "x=$($BucketX * $Size) y=$($BucketY * $Size) z=$($BucketZ * $Size)"
}

function Get-AnswerResult {
    param([AllowEmptyString()][string]$Detail)

    if ($Detail -match '^\s*wrong\b') {
        return "wrong"
    }
    if ($Detail -match '^\s*correct\b') {
        return "correct"
    }
    return "answer_attempt"
}

function Get-SafeDetail {
    param(
        [AllowEmptyString()][string]$EventType,
        [AllowEmptyString()][string]$Detail
    )

    if ($EventType -eq "question_answer") {
        return Get-AnswerResult -Detail $Detail
    }

    $text = ([string]$Detail).Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        return ""
    }

    $text = $text -replace '(?i)\b(?:\d{1,3}\.){3}\d{1,3}\b', '[redacted-ip]'
    $text = $text -replace '(?i)(password|passwd|pwd|token|secret|credential|apikey|api_key)\s*=\s*[^;\s,|]+', '$1=[redacted]'
    $text = $text -replace '[\r\n|]+', ' '
    $text = $text.Trim()
    if ($text.Length -gt 80) {
        return $text.Substring(0, 77) + "..."
    }
    return $text
}

function Format-MarkdownCell {
    param(
        [AllowNull()]$Value,
        [int]$MaxLength = 120
    )

    if ($null -eq $Value) {
        return ""
    }

    $text = ([string]$Value) -replace '[\r\n]+', ' '
    $text = $text -replace '\|', '/'
    $text = $text.Trim()
    if ($text.Length -gt $MaxLength) {
        return $text.Substring(0, [Math]::Max(0, $MaxLength - 3)) + "..."
    }
    return $text
}

function Add-Line {
    param([AllowEmptyString()][string]$Line)
    $script:lines.Add($Line) | Out-Null
}

function Add-MarkdownTable {
    param(
        [Parameter(Mandatory = $true)][string[]]$Headers,
        [Parameter(Mandatory = $true)][object[]]$Rows
    )

    if ($Rows.Count -eq 0) {
        Add-Line "Insufficient data for this table."
        return
    }

    Add-Line ("| " + (($Headers | ForEach-Object { Format-MarkdownCell $_ }) -join " | ") + " |")
    Add-Line ("| " + (($Headers | ForEach-Object { "---" }) -join " | ") + " |")
    foreach ($row in $Rows) {
        Add-Line ("| " + (($row | ForEach-Object { Format-MarkdownCell $_ }) -join " | ") + " |")
    }
}

function Get-TopDetails {
    param([Parameter(Mandatory = $true)][object[]]$Rows)

    $detailGroups = @($Rows |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_.SafeDetail) } |
        Group-Object -Property SafeDetail)
    if ($detailGroups.Count -eq 0) {
        return ""
    }

    $parts = @()
    foreach ($group in ($detailGroups | Sort-Object -Property @{ Expression = { ($_.Group | Measure-Object -Property CountValue -Sum).Sum }; Descending = $true } | Select-Object -First 2)) {
        $total = [int](($group.Group | Measure-Object -Property CountValue -Sum).Sum)
        $parts += "$($group.Name) ($total)"
    }
    return $parts -join "; "
}

function Get-ZoneSummaries {
    param([Parameter(Mandatory = $true)][object[]]$Rows)

    $groups = @($Rows | Group-Object -Property Map, Phase, EventType, ZoneKey)
    $summaries = @()
    foreach ($group in $groups) {
        $sample = $group.Group[0]
        $summaries += [pscustomobject]@{
            Total = [int](($group.Group | Measure-Object -Property CountValue -Sum).Sum)
            Map = $sample.Map
            Phase = $sample.Phase
            EventType = $sample.EventType
            Zone = $sample.ZoneLabel
            BucketX = $sample.BucketX
            BucketY = $sample.BucketY
            BucketZ = $sample.BucketZ
            Detail = Get-TopDetails -Rows $group.Group
        }
    }

    return @($summaries | Sort-Object -Property @{ Expression = { $_.Total }; Descending = $true }, Map, Phase, EventType | Select-Object -First $Top)
}

function Get-NearbyActivityCount {
    param(
        [string]$Map,
        [int]$BucketX,
        [int]$BucketY,
        [int]$BucketZ
    )

    $nearby = @($script:activeRouteRows | Where-Object {
        $_.Map -eq $Map -and
        [Math]::Abs($_.BucketX - $BucketX) -le 1 -and
        [Math]::Abs($_.BucketY - $BucketY) -le 1 -and
        [Math]::Abs($_.BucketZ - $BucketZ) -le 1
    })
    if ($nearby.Count -eq 0) {
        return 0
    }
    return [int](($nearby | Measure-Object -Property CountValue -Sum).Sum)
}

function Get-SuggestedAction {
    param(
        [Parameter(Mandatory = $true)][string]$Kind,
        [Parameter(Mandatory = $true)]$Summary,
        [int]$NearbyActivity = 0
    )

    switch ($Kind) {
        "Capture" { return "Review sightlines, cover, locker spacing, and chase break options in this zone." }
        "Objective" {
            if ($Summary.EventType -in @("objective_stalled", "objective_unstarted")) {
                return "Add route signage or a stronger objective silhouette; check whether this station is too hidden or too risky."
            }
            return "Compare this active objective zone against stall zones before changing objective counts."
        }
        "Locker" {
            if ($Summary.EventType -eq "locker_unused" -and $NearbyActivity -gt 0) {
                return "Move, light, or signpost this locker so it supports nearby chase routes."
            }
            if ($Summary.EventType -eq "locker_unused") {
                return "Collect more route activity before moving this locker; it may be outside normal flow."
            }
            return "Check whether locker use creates fair escape loops instead of dead-end hiding."
        }
        "Route" { return "If this route dominates, improve alternate exit landmarks; if it is ignored, add route language or reduce friction." }
        "CCTV" { return "Review camera coverage, counterplay cover, and shutter access near this hotspot." }
        "Scare" { return "Check scare cooldowns, visibility, and comfort impact in this zone before adding more pressure." }
        "Battery" {
            if ($Summary.EventType -eq "battery_starved") {
                return "Consider a nearby pickup, shorter dark route, or flashlight drain tuning pass."
            }
            return "Compare pickup density here against starvation zones before moving batteries."
        }
        default { return "Review this cluster in a map walkthrough before changing layout or lesson tuning." }
    }
}

function Add-ZoneSection {
    param(
        [Parameter(Mandatory = $true)][string]$Title,
        [Parameter(Mandatory = $true)][string[]]$EventTypes,
        [Parameter(Mandatory = $true)][string]$InsufficientText,
        [Parameter(Mandatory = $true)][string]$Kind
    )

    Add-Line ""
    Add-Line "## $Title"
    $selected = @($eventRows | Where-Object { $EventTypes -contains $_.EventType })
    if ($selected.Count -eq 0) {
        Add-Line "Insufficient data: $InsufficientText"
        return
    }

    $summaries = @(Get-ZoneSummaries -Rows $selected)
    $tableRows = @()
    foreach ($summary in $summaries) {
        $nearby = 0
        $nearbyText = ""
        if ($Kind -eq "Locker") {
            $nearby = Get-NearbyActivityCount -Map $summary.Map -BucketX $summary.BucketX -BucketY $summary.BucketY -BucketZ $summary.BucketZ
            $nearbyText = if ($summary.EventType -eq "locker_unused") { "$nearby nearby active events" } else { "" }
        }
        $tableRows += ,@(
            $summary.Total,
            $summary.Map,
            $summary.Phase,
            $summary.EventType,
            $summary.Zone,
            $(if ($nearbyText) { $nearbyText } else { $summary.Detail }),
            (Get-SuggestedAction -Kind $Kind -Summary $summary -NearbyActivity $nearby)
        )
    }

    Add-MarkdownTable -Headers @("Count", "Map", "Phase", "Event", "Rough zone", "Evidence", "Suggested follow-up") -Rows $tableRows
}

function Add-WrongAnswerSection {
    Add-Line ""
    Add-Line "## Wrong-Answer Topic Clusters"
    $questionRows = @($eventRows | Where-Object { $_.EventType -eq "question_answer" })
    if ($questionRows.Count -eq 0) {
        Add-Line "Insufficient data: no question_answer rows were found."
        return
    }

    $wrongRows = @($questionRows | Where-Object { $_.AnswerResult -eq "wrong" })
    if ($wrongRows.Count -eq 0) {
        Add-Line "No wrong-answer rows were found in the available telemetry. Keep collecting data before changing lesson content."
        return
    }

    $groups = @($wrongRows | Group-Object -Property Map, Phase, Topic, Difficulty)
    $summaries = @()
    foreach ($group in $groups) {
        $sample = $group.Group[0]
        $wrongCount = [int](($group.Group | Measure-Object -Property CountValue -Sum).Sum)
        $attempts = @($questionRows | Where-Object {
            $_.Map -eq $sample.Map -and
            $_.Phase -eq $sample.Phase -and
            $_.Topic -eq $sample.Topic -and
            $_.Difficulty -eq $sample.Difficulty
        })
        $attemptCount = [int](($attempts | Measure-Object -Property CountValue -Sum).Sum)
        $rate = if ($attemptCount -gt 0) { "{0:P0}" -f ($wrongCount / [double]$attemptCount) } else { "n/a" }
        $topic = if ([string]::IsNullOrWhiteSpace($sample.Topic)) { "Unknown" } else { $sample.Topic }
        $difficulty = if ([string]::IsNullOrWhiteSpace($sample.Difficulty)) { "Unknown" } else { $sample.Difficulty }
        $summaries += [pscustomobject]@{
            Wrong = $wrongCount
            Attempts = $attemptCount
            WrongRate = $rate
            Map = $sample.Map
            Phase = $sample.Phase
            Topic = $topic
            Difficulty = $difficulty
        }
    }

    $tableRows = @()
    foreach ($summary in ($summaries | Sort-Object -Property @{ Expression = { $_.Wrong }; Descending = $true }, Topic, Difficulty | Select-Object -First $Top)) {
        $tableRows += ,@(
            $summary.Wrong,
            $summary.Attempts,
            $summary.WrongRate,
            $summary.Map,
            $summary.Phase,
            $summary.Topic,
            $summary.Difficulty,
            "Review $($summary.Topic) $($summary.Difficulty) hints, examples, or pre-brief wording. Do not publish answer keys."
        )
    }

    Add-MarkdownTable -Headers @("Wrong", "Attempts", "Wrong rate", "Map", "Phase", "Topic", "Difficulty", "Suggested follow-up") -Rows $tableRows
}

$inputWarnings = New-Object 'System.Collections.Generic.List[string]'
$eventFiles = New-Object 'System.Collections.Generic.List[System.IO.FileInfo]'
foreach ($pathText in $InputPath) {
    if (-not (Test-Path -LiteralPath $pathText)) {
        $inputWarnings.Add("Input path was not found: $(Get-DisplayPath -Path $pathText)") | Out-Null
        continue
    }

    $item = Get-Item -LiteralPath $pathText
    if ($item.PSIsContainer) {
        $candidateFiles = @(Get-ChildItem -LiteralPath $item.FullName -File -Filter "*.csv" -ErrorAction SilentlyContinue |
            Where-Object { Test-TelemetryEventCsv -Path $_.FullName })
        foreach ($file in $candidateFiles) {
            $eventFiles.Add($file) | Out-Null
        }
    }
    elseif ($item.Extension -ieq ".csv" -and (Test-TelemetryEventCsv -Path $item.FullName)) {
        $eventFiles.Add($item) | Out-Null
    }
    else {
        $inputWarnings.Add("Skipped non-telemetry CSV input: $(Get-DisplayPath -Path $item.FullName)") | Out-Null
    }
}

$eventRows = New-Object 'System.Collections.Generic.List[object]'
$fileSummaries = New-Object 'System.Collections.Generic.List[object]'
foreach ($file in ($eventFiles | Sort-Object -Property FullName -Unique)) {
    try {
        $csvRows = @(Import-Csv -LiteralPath $file.FullName)
    }
    catch {
        $inputWarnings.Add("Could not read CSV: $(Get-DisplayPath -Path $file.FullName)") | Out-Null
        continue
    }

    $imported = 0
    foreach ($row in $csvRows) {
        $eventType = (Get-CsvField -Row $row -Name "EventType" -Default "unknown").ToLowerInvariant()
        $map = Get-CsvField -Row $row -Name "Map" -Default "Unknown"
        $phase = Get-CsvField -Row $row -Name "Phase" -Default "Unknown"
        $detail = Get-CsvField -Row $row -Name "Detail" -Default ""
        $x = Convert-ToDoubleOrZero -Text (Get-CsvField -Row $row -Name "X" -Default "0")
        $y = Convert-ToDoubleOrZero -Text (Get-CsvField -Row $row -Name "Y" -Default "0")
        $z = Convert-ToDoubleOrZero -Text (Get-CsvField -Row $row -Name "Z" -Default "0")
        $bucketX = Get-BucketIndex -Value $x -Size $BucketSize
        $bucketY = Get-BucketIndex -Value $y -Size $BucketSize
        $bucketZ = Get-BucketIndex -Value $z -Size $BucketSize
        $countValue = Convert-ToIntOrDefault -Text (Get-CsvField -Row $row -Name "Count" -Default "1") -Default 1

        $eventRows.Add([pscustomobject]@{
            SourceFile = $file.Name
            Map = $map
            Phase = $phase
            EventType = $eventType
            SafeDetail = Get-SafeDetail -EventType $eventType -Detail $detail
            Topic = Get-CsvField -Row $row -Name "Topic" -Default ""
            Difficulty = Get-CsvField -Row $row -Name "Difficulty" -Default ""
            AnswerResult = Get-AnswerResult -Detail $detail
            BucketX = $bucketX
            BucketY = $bucketY
            BucketZ = $bucketZ
            ZoneKey = "$map|$bucketX|$bucketY|$bucketZ"
            ZoneLabel = Get-BucketLabel -BucketX $bucketX -BucketY $bucketY -BucketZ $bucketZ -Size $BucketSize
            CountValue = $countValue
        }) | Out-Null
        $imported++
    }

    $fileSummaries.Add([pscustomobject]@{
        Name = $file.Name
        Rows = $imported
    }) | Out-Null
}

$activeEventTypes = @(
    "capture",
    "objective_work_started",
    "objective_completed",
    "objective_stalled",
    "objective_unstarted",
    "breaker_repaired",
    "exit_route_choice",
    "escape",
    "escape_reached",
    "cctv_detection",
    "scare_cue",
    "jumpscare",
    "teacher_scan",
    "battery_pickup",
    "battery_starved"
)
$script:activeRouteRows = @($eventRows | Where-Object { $activeEventTypes -contains $_.EventType })

$script:lines = New-Object 'System.Collections.Generic.List[string]'
Add-Line "# Blackout Hunt Map Playtest Evidence Report"
Add-Line ""
Add-Line "- generated_local: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")"
Add-Line "- event_rows: $($eventRows.Count)"
Add-Line "- input_event_files: $($fileSummaries.Count)"
Add-Line "- rough_zone_bucket_size_uu: $BucketSize"
Add-Line "- output_scope: local Markdown under Saved; not intended for packaged classroom distribution"
Add-Line ""
Add-Line "Privacy guard: this report does not print SessionId, PlayerTag, TargetTag, player names, account data, IP addresses, credentials, answer choices, or answer keys. Question analysis is limited to result, topic, difficulty, map, phase, and rough coordinate buckets."

Add-Line ""
Add-Line "## Input Files"
if ($fileSummaries.Count -eq 0) {
    Add-Line "No telemetry event CSV files were found. Export heatmap telemetry from the host Classroom tab, then run this tool again."
}
else {
    $sourceRows = @()
    foreach ($summary in $fileSummaries) {
        $sourceRows += ,@($summary.Name, $summary.Rows)
    }
    Add-MarkdownTable -Headers @("File", "Rows read") -Rows $sourceRows
}

if ($inputWarnings.Count -gt 0) {
    Add-Line ""
    Add-Line "## Input Warnings"
    foreach ($warning in $inputWarnings) {
        Add-Line "- $warning"
    }
}

Add-Line ""
Add-Line "## Event Coverage"
if ($eventRows.Count -eq 0) {
    Add-Line "Insufficient data: no telemetry rows were available, so no map or lesson conclusions were generated."
}
else {
    $coverageRows = @()
    $coverageGroups = @($eventRows | Group-Object -Property Map, Phase, EventType)
    foreach ($group in ($coverageGroups | Sort-Object -Property @{ Expression = { ($_.Group | Measure-Object -Property CountValue -Sum).Sum }; Descending = $true }, Name | Select-Object -First 40)) {
        $sample = $group.Group[0]
        $coverageRows += ,@(
            [int](($group.Group | Measure-Object -Property CountValue -Sum).Sum),
            $sample.Map,
            $sample.Phase,
            $sample.EventType
        )
    }
    Add-MarkdownTable -Headers @("Count", "Map", "Phase", "Event") -Rows $coverageRows
}

Add-ZoneSection -Title "Top Capture Zones" -EventTypes @("capture") -InsufficientText "no capture rows were found." -Kind "Capture"
Add-ZoneSection -Title "Objective Activity And Stall Zones" -EventTypes @("objective_stalled", "objective_unstarted", "objective_work_started", "objective_completed", "breaker_repaired") -InsufficientText "no objective, breaker, stall, or unstarted rows were found." -Kind "Objective"
Add-ZoneSection -Title "Locker Use And Unused Locker Zones" -EventTypes @("locker_unused", "locker_entered", "locker_searched") -InsufficientText "no locker rows were found." -Kind "Locker"
Add-ZoneSection -Title "Route And Exit Choices" -EventTypes @("exit_route_choice", "escape", "escape_reached") -InsufficientText "no exit route or escape rows were found." -Kind "Route"
Add-ZoneSection -Title "CCTV Detection Hotspots" -EventTypes @("cctv_detection") -InsufficientText "no CCTV detection rows were found." -Kind "CCTV"
Add-ZoneSection -Title "Scare Frequency By Zone" -EventTypes @("scare_cue", "jumpscare") -InsufficientText "no scare cue or jumpscare rows were found." -Kind "Scare"
Add-ZoneSection -Title "Battery Starvation And Pickup Zones" -EventTypes @("battery_starved", "battery_pickup") -InsufficientText "no battery starvation or pickup rows were found." -Kind "Battery"
Add-WrongAnswerSection

Add-Line ""
Add-Line "## Reading Notes"
Add-Line "- Treat one export as directional evidence, not final balance truth. Look for repeated clusters across several classroom rounds."
Add-Line "- A rough zone is a bucketed world coordinate, not an exact player path."
Add-Line "- For map work, prioritize clusters that combine captures, stalls, starvation, or CCTV detections in the same map area."
Add-Line "- For lesson work, prioritize high wrong-rate topic/difficulty clusters and review prompts, hints, or pre-brief language without exposing answer keys."

Set-Content -LiteralPath $reportPath -Value $lines -Encoding ASCII
Write-Host "Playtest evidence report written to $reportPath"
