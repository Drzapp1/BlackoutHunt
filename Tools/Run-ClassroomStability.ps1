param(
    [string]$Version = "0.2.0-beta.6",
    [string]$ValidationRoot = "$PSScriptRoot\..\Builds\Validation",
    [string]$ArchiveRoot = "$PSScriptRoot\..\Builds\Archives",
    [string]$BaseVmName = "BlackoutHunt-Win11Eval-01",
    [string[]]$StudentVmNames = @("BlackoutHunt-Win11Student-01", "BlackoutHunt-Win11Student-02"),
    [string]$HostJoinAddress = "192.168.56.1:7777",
    [string]$GuestUser = "student",
    [string]$GuestPassword = "",
    [int]$SoakSeconds = 3600,
    [switch]$SkipBuild,
    [switch]$SkipPackage,
    [switch]$SkipAutomationTests,
    [switch]$SkipVmLaunch,
    [switch]$SkipDefenderScan,
    [switch]$LaunchTeacher
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))
$packageRoot = Join-Path $projectRoot "Builds\Windows"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runRoot = Join-Path ([System.IO.Path]::GetFullPath($ValidationRoot)) "classroom-stability-$Version-$timestamp"
$logRoot = Join-Path $runRoot "logs"
$extractRoot = Join-Path $runRoot "extract"
$reportPath = Join-Path $runRoot "CLASSROOM_STABILITY_REPORT.md"
$archiveRootFull = [System.IO.Path]::GetFullPath($ArchiveRoot)
$archivePath = Join-Path $archiveRootFull "BlackoutHunt-$Version-Windows-Classroom-$timestamp.zip"

function New-CheckedDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
    return [System.IO.Path]::GetFullPath((Resolve-Path $Path))
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

function Get-VBoxManage {
    $candidates = @(
        (Join-Path ${env:ProgramFiles} "Oracle\VirtualBox\VBoxManage.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Oracle\VirtualBox\VBoxManage.exe"),
        (Join-Path "D:\VirtualBox" "VBoxManage.exe")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    $fromPath = Get-Command VBoxManage.exe -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    throw "VBoxManage.exe was not found."
}

function Get-RegisteredVmNames {
    param([Parameter(Mandatory = $true)][string]$VBoxManage)
    $output = & $VBoxManage list vms
    if ($LASTEXITCODE -ne 0) {
        throw "VBoxManage list vms failed."
    }
    return @($output | ForEach-Object {
        if ($_ -match '^"(.+)"\s+\{') { $Matches[1] }
    })
}

function Ensure-StudentVm {
    param(
        [Parameter(Mandatory = $true)][string]$VBoxManage,
        [Parameter(Mandatory = $true)][string]$VmName,
        [Parameter(Mandatory = $true)][string]$BaseName
    )

    $names = Get-RegisteredVmNames -VBoxManage $VBoxManage
    if ($names -contains $VmName) {
        return
    }
    if ($names -notcontains $BaseName) {
        throw "Base VM '$BaseName' was not found; cannot clone '$VmName'."
    }

    & $VBoxManage clonevm $BaseName --name $VmName --basefolder "D:\VMs" --register
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to clone $VmName from $BaseName."
    }
}

function Restore-PreRcSnapshotIfPresent {
    param(
        [Parameter(Mandatory = $true)][string]$VBoxManage,
        [Parameter(Mandatory = $true)][string]$VmName
    )

    $snapshots = & $VBoxManage snapshot $VmName list --machinereadable 2>$null
    if ($LASTEXITCODE -ne 0 -or ($snapshots -notmatch 'SnapshotName-.+="pre-rc-test"')) {
        Write-ReportLine "- ${VmName}: no pre-rc-test snapshot found; leaving current state"
        return
    }

    & $VBoxManage controlvm $VmName poweroff 2>$null | Out-Null
    & $VBoxManage snapshot $VmName restore "pre-rc-test"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to restore pre-rc-test snapshot for $VmName."
    }
    Write-ReportLine "- ${VmName}: restored pre-rc-test snapshot"
}

function Invoke-GuestPowerShell {
    param(
        [Parameter(Mandatory = $true)][string]$VBoxManage,
        [Parameter(Mandatory = $true)][string]$VmName,
        [Parameter(Mandatory = $true)][string]$Command
    )

    if ([string]::IsNullOrWhiteSpace($GuestPassword)) {
        Write-ReportLine "- ${VmName}: skipped guest command because GuestPassword was not provided"
        return
    }

    & $VBoxManage guestcontrol $VmName run --username $GuestUser --password $GuestPassword --exe "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -- powershell.exe -NoProfile -ExecutionPolicy Bypass -Command $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Guest command failed on $VmName."
    }
}

function Copy-ToGuest {
    param(
        [Parameter(Mandatory = $true)][string]$VBoxManage,
        [Parameter(Mandatory = $true)][string]$VmName,
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$TargetDirectory
    )

    if ([string]::IsNullOrWhiteSpace($GuestPassword)) {
        Write-ReportLine "- ${VmName}: skipped guest copy because GuestPassword was not provided"
        return
    }

    & $VBoxManage guestcontrol $VmName copyto --username $GuestUser --password $GuestPassword --target-directory $TargetDirectory $Source
    if ($LASTEXITCODE -ne 0) {
        throw "Guest copyto failed on $VmName."
    }
}

function Copy-FromGuest {
    param(
        [Parameter(Mandatory = $true)][string]$VBoxManage,
        [Parameter(Mandatory = $true)][string]$VmName,
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$TargetDirectory
    )

    if ([string]::IsNullOrWhiteSpace($GuestPassword)) {
        return
    }

    New-Item -ItemType Directory -Force -Path $TargetDirectory | Out-Null
    & $VBoxManage guestcontrol $VmName copyfrom --username $GuestUser --password $GuestPassword --recursive $Source $TargetDirectory 2>$null
}

New-CheckedDirectory -Path $runRoot | Out-Null
New-CheckedDirectory -Path $logRoot | Out-Null
New-CheckedDirectory -Path $extractRoot | Out-Null
New-CheckedDirectory -Path $archiveRootFull | Out-Null

Set-Content -LiteralPath $reportPath -Value @(
    "# Blackout Hunt Classroom Stability Validation",
    "",
    "- version: $Version",
    "- timestamp: $timestamp",
    "- project_root: $projectRoot",
    "- package_root: $packageRoot"
)

Start-Transcript -LiteralPath (Join-Path $logRoot "transcript.txt") | Out-Null
try {
    Invoke-Step "Host Preflight" {
        Write-ReportLine "- host_os: $([System.Environment]::OSVersion.VersionString)"
        Write-ReportLine "- powershell: $($PSVersionTable.PSVersion)"
        Write-ReportLine "- git_status:"
        (git -C $projectRoot status --short) | ForEach-Object { Write-ReportLine "  - $_" }
        $drive = Get-PSDrive -Name "D"
        Write-ReportLine "- d_free_gb: $([math]::Round($drive.Free / 1GB, 2))"
    }

    if (-not $SkipBuild) {
        Invoke-Step "Build Editor" {
            & "$PSScriptRoot\Build-Editor.ps1" *>&1 | Tee-Object -FilePath (Join-Path $logRoot "build-editor.log")
            if ($LASTEXITCODE -ne 0) { throw "Build-Editor.ps1 failed with exit code $LASTEXITCODE." }
        }
    }

    if (-not $SkipAutomationTests) {
        Invoke-Step "Automation Tests" {
            $unreal = & "$PSScriptRoot\Find-Unreal.ps1"
            $editorCmd = Join-Path (Split-Path $unreal.Editor -Parent) "UnrealEditor-Cmd.exe"
            $project = Join-Path $projectRoot "BlackoutHunt.uproject"
            $testLog = Join-Path $logRoot "automation-tests.log"
            $tests = @(
                "BlackoutHunt.Network.NormalizeJoinAddress",
                "BlackoutHunt.Automation.CommandLine"
            )
            foreach ($test in $tests) {
                & $editorCmd $project -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests $test" "-TestExit=Automation Test Queue Empty" *>&1 |
                    Tee-Object -FilePath $testLog -Append
                if ($LASTEXITCODE -ne 0) { throw "Automation test $test failed with exit code $LASTEXITCODE." }
            }
        }
    }

    if (-not $SkipPackage) {
        Invoke-Step "Package Classroom" {
            & "$PSScriptRoot\Package-Windows-Classroom.ps1" *>&1 | Tee-Object -FilePath (Join-Path $logRoot "package-windows-classroom.log")
            if ($LASTEXITCODE -ne 0) { throw "Package-Windows-Classroom.ps1 failed with exit code $LASTEXITCODE." }
        }
    }

    Invoke-Step "Verify Package" {
        & "$PSScriptRoot\Verify-ClassroomPackage.ps1" -PackageRoot $packageRoot *>&1 | Tee-Object -FilePath (Join-Path $logRoot "verify-classroom-package.log")
        if ($LASTEXITCODE -ne 0) { throw "Verify-ClassroomPackage.ps1 failed with exit code $LASTEXITCODE." }
    }

    Invoke-Step "Archive And Hash" {
        if (Test-Path -LiteralPath $archivePath) {
            throw "Archive already exists: $archivePath"
        }
        Compress-Archive -LiteralPath $packageRoot -DestinationPath $archivePath -CompressionLevel Optimal
        $hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        Set-Content -LiteralPath "$archivePath.sha256" -Value "$hash  $(Split-Path -Leaf $archivePath)"
        Write-ReportLine "- archive: $archivePath"
        Write-ReportLine "- sha256: $hash"
    }

    Invoke-Step "Clean Extraction" {
        Expand-Archive -LiteralPath $archivePath -DestinationPath $extractRoot -Force
        Get-ChildItem -LiteralPath $extractRoot -Recurse -File -Force |
            Select-Object FullName, Length |
            Export-Csv -NoTypeInformation -LiteralPath (Join-Path $logRoot "extracted-files.csv")
    }

    Invoke-Step "Package Hygiene Audit" {
        $forbidden = @(
            @{ Name = "debug symbols"; Pattern = '\.pdb$' },
            @{ Name = "saved accounts"; Pattern = '[\\/]Saved[\\/]Account[\\/]' },
            @{ Name = "saved crashes"; Pattern = '[\\/]Saved[\\/]Crashes[\\/]' },
            @{ Name = "saved logs"; Pattern = '[\\/]Saved[\\/]Logs[\\/]' },
            @{ Name = "env files"; Pattern = '(^|[\\/])\.env$' },
            @{ Name = "backend data"; Pattern = '[\\/]AccountBackend[\\/]data[\\/]' },
            @{ Name = "secrets"; Pattern = '(secret|token|client_secret|password)' }
        )

        $failures = New-Object System.Collections.Generic.List[string]
        Get-ChildItem -LiteralPath $extractRoot -Recurse -File -Force | ForEach-Object {
            $relative = $_.FullName.Substring($extractRoot.Length).TrimStart('\', '/')
            foreach ($rule in $forbidden) {
                if ($relative -match $rule.Pattern) {
                    $failures.Add("$($rule.Name): $relative")
                }
            }
        }

        if ($failures.Count -gt 0) {
            $failures | Set-Content -LiteralPath (Join-Path $logRoot "hygiene-failures.txt")
            throw "Package hygiene audit failed. See hygiene-failures.txt."
        }
        Write-ReportLine "- result_detail: no forbidden file patterns found"
    }

    if (-not $SkipDefenderScan -and (Get-Command Start-MpScan -ErrorAction SilentlyContinue)) {
        Invoke-Step "Defender Scan" {
            Start-MpScan -ScanType CustomScan -ScanPath $archivePath
            Start-MpScan -ScanType CustomScan -ScanPath $extractRoot
            Write-ReportLine "- result_detail: Start-MpScan completed for archive and extraction"
        }
    }

    if ($LaunchTeacher) {
        Invoke-Step "Launch Teacher Automation" {
            $rootExe = Get-ChildItem -LiteralPath $extractRoot -Recurse -File -Filter "BlackoutHunt.exe" |
                Sort-Object FullName |
                Select-Object -First 1
            if (-not $rootExe) { throw "Could not find BlackoutHunt.exe in extracted archive." }
            $args = "-BHAutomation=1 -BHAutoHost=LiveClassroom -BHAutoReady=1 -BHAutoQuitSeconds=$SoakSeconds -BHAutomationTag=teacher-$timestamp -log"
            Start-Process -FilePath $rootExe.FullName -ArgumentList $args -WorkingDirectory (Split-Path -Parent $rootExe.FullName) -WindowStyle Minimized
            Write-ReportLine "- teacher_exe: $($rootExe.FullName)"
            Write-ReportLine "- teacher_args: $args"
        }
    }

    if (-not $SkipVmLaunch) {
        Invoke-Step "Windows Student VM Automation" {
            $vbox = Get-VBoxManage
            Write-ReportLine "- vboxmanage: $vbox"
            (& $vbox --version) | ForEach-Object { Write-ReportLine "- virtualbox_version: $_" }

            foreach ($vm in $StudentVmNames) {
                Ensure-StudentVm -VBoxManage $vbox -VmName $vm -BaseName $BaseVmName
                Restore-PreRcSnapshotIfPresent -VBoxManage $vbox -VmName $vm
                & $vbox startvm $vm --type headless | Out-Null
                Start-Sleep -Seconds 20
                Invoke-GuestPowerShell -VBoxManage $vbox -VmName $vm -Command "New-Item -ItemType Directory -Force -Path C:\BHValidation | Out-Null"
                Copy-ToGuest -VBoxManage $vbox -VmName $vm -Source $archivePath -TargetDirectory "C:\BHValidation"
                $archiveName = Split-Path -Leaf $archivePath
                $tag = $vm.ToLowerInvariant()
                $guestCommand = @"
`$ErrorActionPreference='Stop';
`$root='C:\BHValidation\$timestamp';
New-Item -ItemType Directory -Force -Path `$root | Out-Null;
Expand-Archive -LiteralPath 'C:\BHValidation\$archiveName' -DestinationPath `$root -Force;
`$exe=Get-ChildItem -LiteralPath `$root -Recurse -Filter BlackoutHunt.exe | Select-Object -First 1;
if (-not `$exe) { throw 'BlackoutHunt.exe not found after extraction' }
Start-Process -FilePath `$exe.FullName -WorkingDirectory (Split-Path -Parent `$exe.FullName) -ArgumentList '-BHAutomation=1 -BHAutoJoin=$HostJoinAddress -BHAutoReady=1 -BHAutoQuitSeconds=$SoakSeconds -BHAutomationTag=$tag -BHVirtualBoxSafe -log';
Start-Sleep -Seconds $([Math]::Min($SoakSeconds + 30, 3900));
wevtutil qe Application /q:"*[System[(Level=1 or Level=2)]]" /f:text /c:80 > "C:\BHValidation\$timestamp-events-$tag.txt";
"@
                Invoke-GuestPowerShell -VBoxManage $vbox -VmName $vm -Command $guestCommand
                Copy-FromGuest -VBoxManage $vbox -VmName $vm -Source "C:\Users\$GuestUser\AppData\Local\BlackoutHunt\Saved\Logs" -TargetDirectory (Join-Path $logRoot $vm)
                Copy-FromGuest -VBoxManage $vbox -VmName $vm -Source "C:\BHValidation\$timestamp-events-$tag.txt" -TargetDirectory (Join-Path $logRoot $vm)
                Write-ReportLine "- ${vm}: automation launch attempted"
            }
        }
    }

    Invoke-Step "Classification" {
        Write-ReportLine "- classification: pending-human-review"
        Write-ReportLine "- note: promote to beta-ready or beta-ready-with-known-limitations only after reviewing logs, VM events, crash folders, and classroom join markers."
    }
}
finally {
    Stop-Transcript | Out-Null
}

Write-Host "Classroom stability run created: $runRoot"
Write-Host "Archive: $archivePath"
