param(
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\Windows",
    [int]$ClientCount = 2,
    [ValidateSet("LiveClassroom", "LiveFacility", "LiveSubstation", "LiveFoggrounds", "PropHuntFacility", "PropHuntContainersHouse")]
    [string]$HostMode = "LiveFacility",
    [int]$Port = 7777,
    [int]$AutoQuitSeconds = 150,
    [int]$HostStartupTimeoutSeconds = 90,
    [int]$ClientJoinTimeoutSeconds = 90,
    [int]$ReadyTimeoutSeconds = 90,
    [int]$RoundStartTimeoutSeconds = 120,
    [int]$ExitTimeoutSeconds = 75,
    [int]$HostQuitDelaySeconds = 12,
    [switch]$SkipPortCheck,
    [switch]$VirtualBoxSafe,
    [switch]$ShowWindows,
    [string[]]$RuntimeLogAllowedPattern = @()
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runRoot = Join-Path $projectRoot "Saved\PackagedClassroomSmoke\$timestamp"
$reportPath = Join-Path $runRoot "SMOKE_REPORT.md"
$runtimeLogGateScript = Join-Path $PSScriptRoot "Test-RuntimeLogs.ps1"

New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

$script:SmokeProcesses = New-Object System.Collections.Generic.List[object]
$script:RunFailed = $false
$script:FailureMessage = ""

function Get-FullPathAllowMissing {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Write-ReportLine {
    param([AllowEmptyString()][string]$Line)
    Add-Content -LiteralPath $reportPath -Value $Line -Encoding ASCII
}

function Quote-ArgumentForReport {
    param([AllowEmptyString()][string]$Value)

    if ($Value -match '\s') {
        return '"' + ($Value -replace '"', '\"') + '"'
    }

    return $Value
}

function Resolve-PackageRoot {
    param([Parameter(Mandatory = $true)][string]$Root)

    $fullRoot = Get-FullPathAllowMissing -Path $Root
    if (-not (Test-Path -LiteralPath $fullRoot -PathType Container)) {
        throw "Classroom package root was not found: $fullRoot. Build it first with .\Tools\Package-Windows-Classroom.ps1."
    }

    return [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $fullRoot))
}

function Get-PackagedGameExecutable {
    param([Parameter(Mandatory = $true)][string]$Root)

    $shippingExe = Join-Path $Root "BlackoutHunt\Binaries\Win64\BlackoutHunt-Win64-Shipping.exe"
    if (Test-Path -LiteralPath $shippingExe -PathType Leaf) {
        return $shippingExe
    }

    $developmentExe = Get-ChildItem -LiteralPath $Root -Recurse -File -Filter "BlackoutHunt-Win64-*.exe" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '[\\/]Binaries[\\/]Win64[\\/]' } |
        Sort-Object FullName |
        Select-Object -First 1
    if ($developmentExe) {
        return $developmentExe.FullName
    }

    $rootLauncher = Join-Path $Root "BlackoutHunt.exe"
    if (Test-Path -LiteralPath $rootLauncher -PathType Leaf) {
        return $rootLauncher
    }

    throw "No packaged Blackout Hunt executable was found under $Root. Build it first with .\Tools\Package-Windows-Classroom.ps1."
}

function Get-UdpPortOwners {
    param([Parameter(Mandatory = $true)][int]$LocalPort)

    $owners = New-Object System.Collections.Generic.List[object]

    try {
        $endpoints = @(Get-NetUDPEndpoint -LocalPort $LocalPort -ErrorAction Stop)
        foreach ($endpoint in $endpoints) {
            $owners.Add([pscustomobject]@{
                ProcessId = [int]$endpoint.OwningProcess
                Source = "Get-NetUDPEndpoint"
            })
        }
    }
    catch {
        try {
            $netstatLines = @(netstat -ano -p udp | Select-String -Pattern "^\s*UDP\s+\S+:$LocalPort\s+\S+\s+(\d+)\s*$")
            foreach ($line in $netstatLines) {
                if ($line.Line -match "^\s*UDP\s+\S+:$LocalPort\s+\S+\s+(\d+)\s*$") {
                    $owners.Add([pscustomobject]@{
                        ProcessId = [int]$Matches[1]
                        Source = "netstat"
                    })
                }
            }
        }
        catch {
        }
    }

    return @($owners | Sort-Object ProcessId -Unique)
}

function Assert-PortAvailable {
    param([Parameter(Mandatory = $true)][int]$LocalPort)

    if ($SkipPortCheck) {
        Write-ReportLine "- port_check: skipped"
        return
    }

    $owners = @(Get-UdpPortOwners -LocalPort $LocalPort)
    if ($owners.Count -eq 0) {
        Write-ReportLine "- port_check: UDP $LocalPort appears available"
        return
    }

    $details = New-Object System.Collections.Generic.List[string]
    foreach ($owner in $owners) {
        $processText = "pid $($owner.ProcessId)"
        try {
            $process = Get-Process -Id $owner.ProcessId -ErrorAction Stop
            if ($process.ProcessName) {
                $processText += " ($($process.ProcessName))"
            }
            if ($process.Path) {
                $processText += " $($process.Path)"
            }
        }
        catch {
        }
        $details.Add($processText)
    }

    throw "UDP port $LocalPort is already in use by $($details -join ', '). Stop that process or rerun with -Port before starting the smoke test."
}

function New-LaunchRecord {
    param(
        [Parameter(Mandatory = $true)][string]$Role,
        [Parameter(Mandatory = $true)][string]$Tag,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $roleRoot = Join-Path $runRoot $Role
    $userDir = Join-Path $roleRoot "UserDir"
    $logPath = Join-Path $roleRoot "$Role.log"
    New-Item -ItemType Directory -Force -Path $userDir | Out-Null

    return [pscustomobject]@{
        Role = $Role
        Tag = $Tag
        RoleRoot = $roleRoot
        UserDir = $userDir
        LogPath = $logPath
        MarkerLogPath = Join-Path $userDir "Saved\Logs\BlackoutHuntAutomation.log"
        Arguments = $Arguments
        Process = $null
        ProcessId = $null
        StartedAt = $null
    }
}

function Start-SmokeProcess {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath,
        [Parameter(Mandatory = $true)]$Record
    )

    $windowStyle = if ($ShowWindows) { "Minimized" } else { "Hidden" }
    $workingDirectory = Split-Path -Parent $ExePath
    $process = Start-Process -FilePath $ExePath -ArgumentList $Record.Arguments -WorkingDirectory $workingDirectory -WindowStyle $windowStyle -PassThru
    $Record.Process = $process
    $Record.ProcessId = $process.Id
    $Record.StartedAt = Get-Date
    $script:SmokeProcesses.Add($Record)

    $argText = ($Record.Arguments | ForEach-Object { Quote-ArgumentForReport $_ }) -join " "
    Write-Host "$($Record.Role) started: pid $($Record.ProcessId)"
    Write-ReportLine "- $($Record.Role): pid $($Record.ProcessId)"
    Write-ReportLine "  - tag: $($Record.Tag)"
    Write-ReportLine "  - log: $($Record.LogPath)"
    Write-ReportLine "  - marker_log: $($Record.MarkerLogPath)"
    Write-ReportLine "  - args: $argText"
}

function Get-MarkerLines {
    param([Parameter(Mandatory = $true)]$Record)

    if (-not (Test-Path -LiteralPath $Record.MarkerLogPath -PathType Leaf)) {
        return @()
    }

    try {
        return @(Get-Content -LiteralPath $Record.MarkerLogPath -ErrorAction Stop)
    }
    catch {
        return @()
    }
}

function Test-SmokeMarker {
    param(
        [Parameter(Mandatory = $true)]$Record,
        [Parameter(Mandatory = $true)][string]$Marker
    )

    foreach ($line in (Get-MarkerLines -Record $Record)) {
        if ($line.Trim().StartsWith($Marker, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    return $false
}

function Assert-NoUnexpectedExit {
    param(
        [Parameter(Mandatory = $true)][object[]]$Records,
        [Parameter(Mandatory = $true)][string]$StageName
    )

    foreach ($record in $Records) {
        if (-not $record.Process) {
            continue
        }

        $record.Process.Refresh()
        if ($record.Process.HasExited) {
            $exitCode = $record.Process.ExitCode
            throw "$($record.Role) exited with code $exitCode while waiting for $StageName. Log: $($record.LogPath). Marker log: $($record.MarkerLogPath)."
        }
    }
}

function Wait-ForSmokeMarker {
    param(
        [Parameter(Mandatory = $true)][object[]]$Records,
        [Parameter(Mandatory = $true)][string]$Marker,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$StageName,
        [object[]]$GuardRecords = @()
    )

    Write-Host "Waiting for $StageName marker: $Marker"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        Assert-NoUnexpectedExit -Records (@($Records) + @($GuardRecords)) -StageName $StageName

        $missing = @($Records | Where-Object { -not (Test-SmokeMarker -Record $_ -Marker $Marker) })
        if ($missing.Count -eq 0) {
            Write-ReportLine "- ${StageName}: found $Marker"
            return
        }

        Start-Sleep -Seconds 2
    }

    $missingRoles = @($Records | Where-Object { -not (Test-SmokeMarker -Record $_ -Marker $Marker) } | ForEach-Object { $_.Role })
    throw "$StageName timed out after $TimeoutSeconds seconds. Missing marker '$Marker' for: $($missingRoles -join ', ')."
}

function Wait-ForCleanExit {
    param(
        [Parameter(Mandatory = $true)][object[]]$Records,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    Write-Host "Waiting for clean process exit"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $running = @()
        foreach ($record in $Records) {
            $record.Process.Refresh()
            if (-not $record.Process.HasExited) {
                $running += $record
            }
        }

        if ($running.Count -eq 0) {
            foreach ($record in $Records) {
                if (-not (Test-SmokeMarker -Record $record -Marker "CLEAN_QUIT")) {
                    throw "$($record.Role) exited without CLEAN_QUIT marker. Marker log: $($record.MarkerLogPath)."
                }
                if ($record.Process.ExitCode -ne 0) {
                    Write-ReportLine "- $($record.Role)_exit_code_warning: $($record.Process.ExitCode) after CLEAN_QUIT"
                }
            }

            Write-ReportLine "- clean_exit: all processes exited with CLEAN_QUIT"
            return
        }

        Start-Sleep -Seconds 2
    }

    $stillRunning = @($Records | Where-Object {
        $_.Process.Refresh()
        -not $_.Process.HasExited
    } | ForEach-Object { "$($_.Role) pid $($_.ProcessId)" })
    throw "Timed out after $TimeoutSeconds seconds waiting for clean exit: $($stillRunning -join ', ')."
}

function Stop-SmokeProcesses {
    $trackedPids = @()
    foreach ($record in $script:SmokeProcesses) {
        if (-not $record.Process) {
            continue
        }

        $trackedPids += [int]$record.ProcessId
        try {
            $record.Process.Refresh()
            if (-not $record.Process.HasExited) {
                Write-Host "Stopping $($record.Role): pid $($record.ProcessId)"
                Stop-Process -Id $record.ProcessId -Force -ErrorAction SilentlyContinue
            }
        }
        catch {
        }
    }

    try {
        $tagNeedle = "packaged-smoke-$timestamp"
        $leftovers = @(Get-CimInstance Win32_Process -Filter "Name = 'BlackoutHunt-Win64-Shipping.exe'" -ErrorAction Stop |
            Where-Object { $_.CommandLine -like "*$tagNeedle*" -and ($trackedPids -notcontains [int]$_.ProcessId) })
        foreach ($leftover in $leftovers) {
            Write-Host "Stopping smoke-tagged process: pid $($leftover.ProcessId)"
            Stop-Process -Id $leftover.ProcessId -Force -ErrorAction SilentlyContinue
        }
    }
    catch {
    }

    try {
        $runRootForward = $runRoot.Replace('\', '/')
        $playitLeftovers = @(Get-CimInstance Win32_Process -Filter "Name = 'playit.exe'" -ErrorAction Stop |
            Where-Object { $_.CommandLine -like "*$runRoot*" -or $_.CommandLine -like "*$runRootForward*" })
        foreach ($playit in $playitLeftovers) {
            Write-Host "Stopping smoke-started Playit helper: pid $($playit.ProcessId)"
            Stop-Process -Id $playit.ProcessId -Force -ErrorAction SilentlyContinue
        }
    }
    catch {
    }
}

function Assert-SmokeLogsClean {
    if (-not (Test-Path -LiteralPath $runtimeLogGateScript -PathType Leaf)) {
        throw "Runtime log gate script was not found: $runtimeLogGateScript"
    }

    if ($RuntimeLogAllowedPattern.Count -gt 0) {
        & $runtimeLogGateScript -Path $runRoot -AllowedFindingPattern $RuntimeLogAllowedPattern
    }
    else {
        & $runtimeLogGateScript -Path $runRoot
    }
    Write-ReportLine "- runtime_log_scan: clean"
}

function Assert-SmokeAutomationMarkers {
    param([Parameter(Mandatory = $true)][object[]]$Records)

    foreach ($record in $Records) {
        $requiredMarkers = if ($record.Role -eq "host") {
            [string[]]@("BH_AUTOMATION_BOOT", "HOST_LISTENING", "READY_SET", "ROUND_STARTED", "CLEAN_QUIT")
        }
        else {
            [string[]]@("BH_AUTOMATION_BOOT", "JOIN_ATTEMPT", "JOINED", "READY_SET", "ROUND_STARTED", "CLEAN_QUIT")
        }

        $logPaths = @()
        if (Test-Path -LiteralPath $record.LogPath -PathType Leaf) {
            $logPaths += $record.LogPath
        }
        $logPaths += $record.MarkerLogPath

        if ($RuntimeLogAllowedPattern.Count -gt 0) {
            & $runtimeLogGateScript -Path $logPaths -ExpectAutomationMarkers -AutomationTag $record.Tag -RequiredAutomationMarker $requiredMarkers -AllowedFindingPattern $RuntimeLogAllowedPattern
        }
        else {
            & $runtimeLogGateScript -Path $logPaths -ExpectAutomationMarkers -AutomationTag $record.Tag -RequiredAutomationMarker $requiredMarkers
        }
        Write-ReportLine "- marker_gate_$($record.Role): clean"
    }
}

function Write-MarkerSummary {
    param([Parameter(Mandatory = $true)][object[]]$Records)

    Write-ReportLine ""
    Write-ReportLine "## Marker Summary"
    foreach ($record in $Records) {
        Write-ReportLine ""
        Write-ReportLine "### $($record.Role)"
        Write-ReportLine "- pid: $($record.ProcessId)"
        if ($record.Process) {
            try {
                $record.Process.Refresh()
                if ($record.Process.HasExited) {
                    Write-ReportLine "- exit_code: $($record.Process.ExitCode)"
                }
                else {
                    Write-ReportLine "- exit_code: still running at report time"
                }
            }
            catch {
                Write-ReportLine "- exit_code: unavailable"
            }
        }
        Write-ReportLine "- log: $($record.LogPath)"
        Write-ReportLine "- marker_log: $($record.MarkerLogPath)"

        $markerLines = @(Get-MarkerLines -Record $record)
        if ($markerLines.Count -eq 0) {
            Write-ReportLine "- markers_found: none"
            continue
        }

        Write-ReportLine "- markers_found:"
        foreach ($line in $markerLines) {
            Write-ReportLine "  - $line"
        }
    }
}

Set-Content -LiteralPath $reportPath -Encoding ASCII -Value @(
    "# Blackout Hunt Packaged Classroom Smoke Test",
    "",
    "- timestamp: $timestamp",
    "- project_root: $projectRoot",
    "- package_root_requested: $PackageRoot",
    "- host_mode: $HostMode",
    "- client_count: $ClientCount",
    "- port: $Port",
    "- client_auto_quit_seconds: $AutoQuitSeconds",
    "- host_auto_quit_seconds: $($AutoQuitSeconds + $HostQuitDelaySeconds)",
    "- run_root: $runRoot"
)

try {
    if ($ClientCount -lt 2) {
        throw "ClientCount must be at least 2 for the classroom smoke test."
    }

    $resolvedPackageRoot = Resolve-PackageRoot -Root $PackageRoot
    $gameExe = Get-PackagedGameExecutable -Root $resolvedPackageRoot

    Write-Host "Package root: $resolvedPackageRoot"
    Write-Host "Game executable: $gameExe"
    Write-Host "Smoke artifacts: $runRoot"
    Write-ReportLine "- package_root: $resolvedPackageRoot"
    Write-ReportLine "- game_executable: $gameExe"

    Assert-PortAvailable -LocalPort $Port

    $commonArgs = @(
        "-BHAutomation=1",
        "-BHAutoReady=1",
        "-unattended",
        "-nosplash",
        "-NoSound",
        "-log",
        "-d3d11",
        "-windowed",
        "-ResX=1280",
        "-ResY=720"
    )
    if ($VirtualBoxSafe) {
        $commonArgs += "-BHVirtualBoxSafe"
    }

    Write-ReportLine ""
    Write-ReportLine "## Launch Records"

    $hostTag = "packaged-smoke-$timestamp-host"
    $hostUserDir = Join-Path $runRoot "host\UserDir"
    $hostLogPath = Join-Path $runRoot "host\host.log"
    $hostArgs = @(
        "-BHAutoHost=$HostMode",
        "-BHAutoMinPlayers=$($ClientCount + 1)",
        "-BHAutoQuitSeconds=$($AutoQuitSeconds + $HostQuitDelaySeconds)",
        "-BHAutomationTag=$hostTag",
        "-UserDir=$hostUserDir",
        "-abslog=$hostLogPath",
        "-port=$Port"
    ) + $commonArgs
    $hostRecord = New-LaunchRecord -Role "host" -Tag $hostTag -Arguments $hostArgs
    Start-SmokeProcess -ExePath $gameExe -Record $hostRecord
    Wait-ForSmokeMarker -Records @($hostRecord) -Marker "HOST_LISTENING" -TimeoutSeconds $HostStartupTimeoutSeconds -StageName "host_startup"

    $clients = @()
    for ($index = 1; $index -le $ClientCount; ++$index) {
        $role = "client-$index"
        $tag = "packaged-smoke-$timestamp-$role"
        $clientUserDir = Join-Path $runRoot "$role\UserDir"
        $clientLogPath = Join-Path $runRoot "$role\$role.log"
        $clientArgs = @(
            "-BHAutoJoin=127.0.0.1:$Port",
            "-BHAutoQuitSeconds=$AutoQuitSeconds",
            "-BHAutomationTag=$tag",
            "-UserDir=$clientUserDir",
            "-abslog=$clientLogPath"
        ) + $commonArgs
        $client = New-LaunchRecord -Role $role -Tag $tag -Arguments $clientArgs
        Start-SmokeProcess -ExePath $gameExe -Record $client
        $clients += $client
        Start-Sleep -Seconds 3
    }

    $allRecords = @($hostRecord) + $clients
    Wait-ForSmokeMarker -Records @($clients) -Marker "JOINED" -TimeoutSeconds $ClientJoinTimeoutSeconds -StageName "client_join" -GuardRecords @($hostRecord)
    Wait-ForSmokeMarker -Records $allRecords -Marker "READY_SET" -TimeoutSeconds $ReadyTimeoutSeconds -StageName "ready_gate"
    Wait-ForSmokeMarker -Records $allRecords -Marker "ROUND_STARTED" -TimeoutSeconds $RoundStartTimeoutSeconds -StageName "round_start"

    $cleanExitTimeout = [Math]::Max($ExitTimeoutSeconds, $AutoQuitSeconds + $HostQuitDelaySeconds + 60)
    Wait-ForCleanExit -Records $allRecords -TimeoutSeconds $cleanExitTimeout
    Assert-SmokeLogsClean
    Assert-SmokeAutomationMarkers -Records $allRecords

    Write-ReportLine ""
    Write-ReportLine "## Result"
    Write-ReportLine "- status: pass"
    Write-Host "Packaged classroom smoke test passed."
}
catch {
    $script:RunFailed = $true
    $script:FailureMessage = $_.Exception.Message
    Write-ReportLine ""
    Write-ReportLine "## Result"
    Write-ReportLine "- status: fail"
    Write-ReportLine "- error: $script:FailureMessage"
    [Console]::Error.WriteLine($script:FailureMessage)
}
finally {
    Stop-SmokeProcesses
    if ($script:SmokeProcesses.Count -gt 0) {
        Write-MarkerSummary -Records @($script:SmokeProcesses.ToArray())
    }
    Write-Host "Smoke report: $reportPath"
}

if ($script:RunFailed) {
    exit 1
}
