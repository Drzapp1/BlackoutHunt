param(
    [int]$NormalSoakMinutes = 25,
    [int]$DegradedSoakMinutes = 10,
    [int]$ClientCount = 2,
    [ValidateSet("Development", "Shipping")]
    [string]$Configuration = "Development",
    [string]$TestFilter = "BlackoutHunt.*",
    [string]$PreferredUnrealRoot = "D:\UE_5.7",
    [string]$ExistingPackageRoot = "",
    [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))
$project = Join-Path $projectRoot "BlackoutHunt.uproject"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runRoot = Join-Path $projectRoot "Saved\Stability\$timestamp"
$logRoot = Join-Path $runRoot "Logs"
$packageRoot = if ($SkipPackage) {
    if ([string]::IsNullOrWhiteSpace($ExistingPackageRoot)) {
        Join-Path $projectRoot "Builds\Windows"
    }
    else {
        [System.IO.Path]::GetFullPath((Resolve-Path $ExistingPackageRoot))
    }
}
else {
    Join-Path $runRoot "Package"
}
$reportPath = Join-Path $runRoot "STABILITY_GATE_REPORT.md"
$failurePattern = "(?i)(Fatal error:|Unhandled Exception|EXCEPTION_ACCESS_VIOLATION|Assertion failed:|Ensure condition failed|Automation Test Failed|LogAutomationController: Error|BH_AUTOMATION.*FAIL:|FAIL:join timeout|FAIL:auto|FAIL:unknown)"

New-Item -ItemType Directory -Force -Path $runRoot, $logRoot | Out-Null
if (-not $SkipPackage) {
    New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
}

function Write-ReportLine {
    param([AllowEmptyString()][string]$Line)
    Add-Content -LiteralPath $reportPath -Value $Line
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Script
    )

    Write-Host "== $Name =="
    Write-ReportLine ""
    Write-ReportLine "## $Name"
    $started = Get-Date
    try {
        & $Script
        $elapsed = (Get-Date) - $started
        Write-ReportLine "- result: pass"
        Write-ReportLine "- elapsed_seconds: $([int]$elapsed.TotalSeconds)"
    }
    catch {
        $elapsed = (Get-Date) - $started
        Write-ReportLine "- result: failed"
        Write-ReportLine "- elapsed_seconds: $([int]$elapsed.TotalSeconds)"
        Write-ReportLine "- error: $($_.Exception.Message)"
        throw
    }
}

function Invoke-LoggedProcess {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$ArgumentList,
        [Parameter(Mandatory = $true)][string]$LogPath
    )

    Write-Host "Running $Name"
    Write-ReportLine "- command: $FilePath $($ArgumentList -join ' ')"
    & $FilePath @ArgumentList > $LogPath 2>&1
    $exitCode = $LASTEXITCODE
    if (Test-Path -LiteralPath $LogPath) {
        Get-Content -LiteralPath $LogPath | ForEach-Object { Write-Host $_ }
    }
    if ($exitCode -ne 0) {
        throw "$Name exited with code $exitCode. See $LogPath"
    }
}

function Assert-LogsClean {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $matches = Get-ChildItem -LiteralPath $Path -Recurse -File -Force |
        Where-Object { $_.Extension -in ".log", ".txt", ".err", ".out" } |
        Select-String -Pattern $failurePattern -List

    if ($matches) {
        $failureLog = Join-Path $runRoot "failure-matches.txt"
        $matches | ForEach-Object { "$($_.Path):$($_.LineNumber): $($_.Line)" } |
            Set-Content -LiteralPath $failureLog -Encoding ASCII
        throw "Failure signatures were found. See $failureLog"
    }
}

function Copy-SavedArtifacts {
    $snapshotRoot = Join-Path $runRoot "SavedSnapshot"
    New-Item -ItemType Directory -Force -Path $snapshotRoot | Out-Null
    foreach ($relative in @("Saved\Logs", "Saved\Crashes")) {
        $source = Join-Path $projectRoot $relative
        if (Test-Path -LiteralPath $source) {
            $target = Join-Path $snapshotRoot $relative
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            Copy-Item -LiteralPath $source -Destination $target -Recurse -Force
        }
    }
}

function Get-PackagedGameExe {
    param([Parameter(Mandatory = $true)][string]$Root)

    $rootLauncher = Get-ChildItem -LiteralPath $Root -Recurse -File -Filter "BlackoutHunt.exe" -ErrorAction SilentlyContinue |
        Sort-Object @{ Expression = { $_.FullName.Length } } |
        Select-Object -First 1
    if ($rootLauncher) {
        return $rootLauncher.FullName
    }

    $developmentExe = Get-ChildItem -LiteralPath $Root -Recurse -File -Filter "BlackoutHunt-Win64-$Configuration.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($developmentExe) {
        return $developmentExe.FullName
    }

    throw "Packaged BlackoutHunt executable was not found under $Root"
}

function Start-GameProcess {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Name
    )

    Write-ReportLine "- ${Name}_args: $($Arguments -join ' ')"
    return Start-Process -FilePath $ExePath -ArgumentList $Arguments -WorkingDirectory (Split-Path -Parent $ExePath) -WindowStyle Minimized -PassThru
}

function Wait-GameProcesses {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process[]]$Processes,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $running = @($Processes | Where-Object { $_ -and -not $_.HasExited })
        if ($running.Count -eq 0) {
            break
        }
        Start-Sleep -Seconds 5
    }

    $stillRunning = @($Processes | Where-Object { $_ -and -not $_.HasExited })
    if ($stillRunning.Count -gt 0) {
        foreach ($process in $stillRunning) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
        throw "Timed out waiting for packaged game processes to exit."
    }

    foreach ($process in $Processes) {
        if ($process.ExitCode -ne 0) {
            throw "Packaged game process $($process.Id) exited with code $($process.ExitCode)."
        }
    }
}

function Invoke-SoakPass {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$ExePath,
        [Parameter(Mandatory = $true)][int]$Minutes,
        [switch]$DegradedNetwork
    )

    $seconds = [Math]::Max(60, $Minutes * 60)
    $passRoot = Join-Path $logRoot $Name
    New-Item -ItemType Directory -Force -Path $passRoot | Out-Null

    $networkArgs = @()
    if ($DegradedNetwork) {
        $networkArgs = @("-PktLag=120", "-PktLagVariance=35", "-PktLoss=3")
    }

    $processes = New-Object System.Collections.Generic.List[System.Diagnostics.Process]
    try {
        $hostLog = Join-Path $passRoot "host.log"
        $hostArgs = @(
            "-BHAutomation=1",
            "-BHAutoHost=LiveFacility",
            "-BHAutoReady=1",
            "-BHAutoMinPlayers=$($ClientCount + 1)",
            "-BHAutoQuitSeconds=$seconds",
            "-BHAutomationTag=$Name-host-$timestamp",
            "-d3d11",
            "-windowed",
            "-ResX=1280",
            "-ResY=720",
            "-log",
            "-abslog=$hostLog"
        ) + $networkArgs
        $processes.Add((Start-GameProcess -ExePath $ExePath -Arguments $hostArgs -Name "$Name host"))
        Start-Sleep -Seconds 10

        for ($index = 1; $index -le $ClientCount; ++$index) {
            $clientLog = Join-Path $passRoot "client-$index.log"
            $clientArgs = @(
                "-BHAutomation=1",
                "-BHAutoJoin=127.0.0.1:7777",
                "-BHAutoReady=1",
                "-BHAutoQuitSeconds=$seconds",
                "-BHAutomationTag=$Name-client-$index-$timestamp",
                "-d3d11",
                "-windowed",
                "-ResX=1280",
                "-ResY=720",
                "-log",
                "-abslog=$clientLog"
            ) + $networkArgs
            $processes.Add((Start-GameProcess -ExePath $ExePath -Arguments $clientArgs -Name "$Name client $index"))
            Start-Sleep -Seconds 4
        }

        Wait-GameProcesses -Processes $processes.ToArray() -TimeoutSeconds ($seconds + 180)
        Assert-LogsClean -Path $passRoot
    }
    finally {
        foreach ($process in $processes) {
            if ($process -and -not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

Set-Content -LiteralPath $reportPath -Encoding ASCII -Value @(
    "# Blackout Hunt Stability Gate",
    "",
    "- timestamp: $timestamp",
    "- project_root: $projectRoot",
    "- configuration: $Configuration",
    "- test_filter: $TestFilter",
    "- package_root: $packageRoot",
    "- normal_soak_minutes: $NormalSoakMinutes",
    "- degraded_soak_minutes: $DegradedSoakMinutes",
    "- client_count: $ClientCount"
)

Start-Transcript -LiteralPath (Join-Path $logRoot "transcript.txt") | Out-Null
try {
    $unreal = & "$PSScriptRoot\Find-Unreal.ps1" -PreferredRoot $PreferredUnrealRoot
    $editorCmd = Join-Path (Split-Path $unreal.Editor -Parent) "UnrealEditor-Cmd.exe"
    if (-not (Test-Path -LiteralPath $editorCmd)) {
        $editorCmd = $unreal.Editor
    }

    Invoke-Step "Development Build" {
        Invoke-LoggedProcess -Name "Development build" -FilePath $unreal.Build -LogPath (Join-Path $logRoot "build-development.log") -ArgumentList @(
            "BlackoutHuntEditor",
            "Win64",
            "Development",
            "-Project=$project",
            "-WaitMutex",
            "-NoXGE",
            "-NoUBA",
            "-MaxParallelActions=1"
        )
    }

    Invoke-Step "Automation Tests" {
        $automationReport = Join-Path $runRoot "AutomationReport"
        New-Item -ItemType Directory -Force -Path $automationReport | Out-Null
        Invoke-LoggedProcess -Name "Automation tests" -FilePath $editorCmd -LogPath (Join-Path $logRoot "automation-tests.log") -ArgumentList @(
            $project,
            "-unattended",
            "-nop4",
            "-nosplash",
            "-NullRHI",
            "-NoSound",
            "-ExecCmds=Automation RunTests $TestFilter; Quit",
            "-TestExit=Automation Test Queue Empty",
            "-ReportOutputPath=$automationReport",
            "-abslog=$(Join-Path $logRoot 'automation-editor.log')"
        )
        Assert-LogsClean -Path $automationReport
        Assert-LogsClean -Path $logRoot
    }

    if (-not $SkipPackage) {
        Invoke-Step "Cook And Package Win64" {
            Invoke-LoggedProcess -Name "Win64 package" -FilePath $unreal.RunUAT -LogPath (Join-Path $logRoot "package-win64.log") -ArgumentList @(
                "BuildCookRun",
                "-project=$project",
                "-notinstalledengine",
                "-noP4",
                "-platform=Win64",
                "-clientconfig=$Configuration",
                "-serverconfig=$Configuration",
                "-cook",
                "-ddc=InstalledNoZenLocalFallback",
                "-map=/Engine/Maps/Entry",
                "-build",
                "-noxge",
                "-ubtargs=-WaitMutex -NoXGE -NoUBA -MaxParallelActions=1",
                "-stage",
                "-pak",
                "-archive",
                "-archivedirectory=$packageRoot",
                "-unattended",
                "-CrashForUAT"
            )
            Assert-LogsClean -Path $logRoot
        }
    }

    $gameExe = Get-PackagedGameExe -Root $packageRoot
    Write-ReportLine "- packaged_exe: $gameExe"

    Invoke-Step "Normal Network Soak" {
        Invoke-SoakPass -Name "normal-soak" -ExePath $gameExe -Minutes $NormalSoakMinutes
    }

    Invoke-Step "Degraded Network Soak" {
        Invoke-SoakPass -Name "degraded-soak" -ExePath $gameExe -Minutes $DegradedSoakMinutes -DegradedNetwork
    }

    Invoke-Step "Artifact Sweep" {
        Copy-SavedArtifacts
        Assert-LogsClean -Path $runRoot
        Write-ReportLine "- artifacts: $runRoot"
    }
}
finally {
    Copy-SavedArtifacts
    Stop-Transcript | Out-Null
}

Write-Host "Stability gate artifacts: $runRoot"
