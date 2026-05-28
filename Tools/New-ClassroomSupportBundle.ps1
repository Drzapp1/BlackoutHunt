param(
    [string]$OutputRoot = "$PSScriptRoot\..\Builds\Support",
    [string]$PackageRoot = "$PSScriptRoot\..\Builds\Windows",
    [int]$MaxLogCount = 8,
    [switch]$SkipPackageVerify,
    [switch]$NoArchive
)

$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath((Resolve-Path "$PSScriptRoot\.."))
$outputRootFull = [System.IO.Path]::GetFullPath($OutputRoot)
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runRoot = Join-Path $outputRootFull "classroom-support-$timestamp"
$reportPath = Join-Path $runRoot "PREFLIGHT.md"
$configRoot = Join-Path $runRoot "config"
$logRoot = Join-Path $runRoot "logs"
$packageRootFull = $null

function New-CheckedDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
    return [System.IO.Path]::GetFullPath((Resolve-Path $Path))
}

function Write-ReportLine {
    param([AllowEmptyString()][string]$Line)
    Add-Content -LiteralPath $reportPath -Value $Line
}

function Get-RelativePathText {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $baseFull = [System.IO.Path]::GetFullPath($BasePath).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $pathFull = [System.IO.Path]::GetFullPath($Path)
    if ($pathFull.StartsWith($baseFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $pathFull.Substring($baseFull.Length)
    }
    return $pathFull
}

function Get-IniValues {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string[]]$Keys
    )

    $result = [ordered]@{}
    if (-not (Test-Path -LiteralPath $Path)) {
        foreach ($key in $Keys) {
            $result[$key] = "<missing file>"
        }
        return $result
    }

    $lines = Get-Content -LiteralPath $Path
    foreach ($key in $Keys) {
        $matches = @($lines | Where-Object { $_ -match "^\s*\+?$([regex]::Escape($key))=(.*)$" })
        if ($matches.Count -eq 0) {
            $result[$key] = "<unset>"
        }
        elseif ($matches.Count -eq 1) {
            $result[$key] = ($matches[0] -replace "^\s*\+?$([regex]::Escape($key))=", "").Trim()
        }
        else {
            $result[$key] = ($matches | ForEach-Object { ($_ -replace "^\s*\+?$([regex]::Escape($key))=", "").Trim() }) -join ", "
        }
    }
    return $result
}

function Copy-LatestLogs {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$TargetName
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        Write-ReportLine "- ${TargetName}: missing"
        return
    }

    $target = New-CheckedDirectory -Path (Join-Path $logRoot $TargetName)
    $logs = @(Get-ChildItem -LiteralPath $Source -File -Force |
        Where-Object { $_.Extension -iin @(".log", ".txt") } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First $MaxLogCount)

    if ($logs.Count -eq 0) {
        Write-ReportLine "- ${TargetName}: no log files"
        return
    }

    foreach ($log in $logs) {
        Copy-Item -LiteralPath $log.FullName -Destination (Join-Path $target $log.Name) -Force
    }
    Write-ReportLine "- ${TargetName}: copied $($logs.Count) latest file(s)"
}

New-CheckedDirectory -Path $runRoot | Out-Null
New-CheckedDirectory -Path $configRoot | Out-Null
New-CheckedDirectory -Path $logRoot | Out-Null

Set-Content -LiteralPath $reportPath -Value @(
    "# Blackout Hunt Classroom Preflight",
    "",
    "- timestamp: $timestamp",
    "- project_root: $projectRoot",
    "- machine: $env:COMPUTERNAME",
    "- user: $env:USERNAME",
    "- os: $([System.Environment]::OSVersion.VersionString)",
    "- powershell: $($PSVersionTable.PSVersion)"
)

$defaultGame = Join-Path $projectRoot "Config\DefaultGame.ini"
$defaultEngine = Join-Path $projectRoot "Config\DefaultEngine.ini"
$uproject = Join-Path $projectRoot "BlackoutHunt.uproject"

Write-ReportLine ""
Write-ReportLine "## Version"
$versionValues = Get-IniValues -Path $defaultGame -Keys @("ProjectVersion", "ProjectName", "Description")
foreach ($entry in $versionValues.GetEnumerator()) {
    Write-ReportLine "- $($entry.Key): $($entry.Value)"
}

Write-ReportLine ""
Write-ReportLine "## Classroom Settings"
$classroomValues = Get-IniValues -Path $defaultGame -Keys @(
    "bClassroomMode",
    "bAllowHostForceStart",
    "bAllowStudentTeacherAdminControls",
    "bAllowTunnelHelper",
    "bAllowHotspotHelper",
    "bClassroomLoopbackOnlyHost",
    "ClassroomJoinEndpoints",
    "RevisionRoundSeconds",
    "RevisionClassThreshold",
    "RevisionIndividualThreshold",
    "RevisionScareIntensity"
)
foreach ($entry in $classroomValues.GetEnumerator()) {
    Write-ReportLine "- $($entry.Key): $($entry.Value)"
}

$isAdmin = "unknown"
if ($IsWindows -or $env:OS -eq "Windows_NT") {
    try {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = [Security.Principal.WindowsPrincipal]::new($identity)
        $isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator).ToString()
    }
    catch {
        $isAdmin = "unknown"
    }
}
else {
    $isAdmin = "not-applicable"
}

$loopbackOnly = $classroomValues["bClassroomLoopbackOnlyHost"] -match "^(True|true|1)$"
$hotspotAllowed = $classroomValues["bAllowHotspotHelper"] -match "^(True|true|1)$"
$tunnelAllowed = $classroomValues["bAllowTunnelHelper"] -match "^(True|true|1)$"
$liveClassroomAdminRequired = "Review with IT"
if ($loopbackOnly -and $tunnelAllowed -and -not $hotspotAllowed) {
    $liveClassroomAdminRequired = "False"
}
$firewallPromptExpected = "Possible for direct LAN hosting"
if ($loopbackOnly) {
    $firewallPromptExpected = "No for Live Classroom loopback hosting"
}
$hotspotPolicy = "Disabled; use Playit/loopback classroom path"
if ($hotspotAllowed) {
    $hotspotPolicy = "Enabled; likely requires IT/admin networking permission"
}

Write-ReportLine ""
Write-ReportLine "## School Account Readiness"
Write-ReportLine "- process_admin: $isAdmin"
Write-ReportLine "- live_classroom_admin_required: $liveClassroomAdminRequired"
Write-ReportLine "- firewall_prompt_expected: $firewallPromptExpected"
Write-ReportLine "- hotspot_policy: $hotspotPolicy"

Write-ReportLine ""
Write-ReportLine "## Engine And Network Settings"
$engineValues = Get-IniValues -Path $defaultEngine -Keys @(
    "GameDefaultMap",
    "GlobalDefaultGameMode",
    "GameInstanceClass",
    "DefaultGraphicsRHI",
    "DefaultPlatformService",
    "EnabledByDefault",
    "EnableTransport",
    "EnableTunnel"
)
foreach ($entry in $engineValues.GetEnumerator()) {
    Write-ReportLine "- $($entry.Key): $($entry.Value)"
}

Write-ReportLine ""
Write-ReportLine "## Package Verification"
if (Test-Path -LiteralPath $PackageRoot) {
    $packageRootFull = [System.IO.Path]::GetFullPath((Resolve-Path $PackageRoot))
    Write-ReportLine "- package_root: $packageRootFull"
    if (-not $packageRootFull.StartsWith($projectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-ReportLine "- result: skipped, package root is outside project root"
    }
    elseif ($SkipPackageVerify) {
        Write-ReportLine "- result: skipped by -SkipPackageVerify"
    }
    else {
        $verifyOut = Join-Path $runRoot "verify-package.stdout.txt"
        $verifyErr = Join-Path $runRoot "verify-package.stderr.txt"
        $verifyScript = Join-Path $PSScriptRoot "Verify-ClassroomPackage.ps1"
        $verifyCommand = "& '$verifyScript' -PackageRoot '$packageRootFull'"
        $powershellExe = (Get-Command pwsh.exe -ErrorAction SilentlyContinue).Source
        if (-not $powershellExe) {
            $powershellExe = (Get-Command powershell.exe -ErrorAction SilentlyContinue).Source
        }

        if ($powershellExe) {
            $process = Start-Process -FilePath $powershellExe `
                -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", $verifyCommand) `
                -RedirectStandardOutput $verifyOut `
                -RedirectStandardError $verifyErr `
                -NoNewWindow `
                -Wait `
                -PassThru
            $verifyResult = if ($process.ExitCode -eq 0) { "pass" } else { "failed" }
            Write-ReportLine "- result: $verifyResult"
            Write-ReportLine "- exit_code: $($process.ExitCode)"
        }
        else {
            Write-ReportLine "- result: skipped, no PowerShell executable found for child verification"
        }
    }
}
else {
    Write-ReportLine "- package_root: missing"
}

Write-ReportLine ""
Write-ReportLine "## Git Status"
if (Get-Command git -ErrorAction SilentlyContinue) {
    $gitStatusPath = Join-Path $runRoot "git-status.txt"
    git -C $projectRoot status --short | Set-Content -LiteralPath $gitStatusPath
    $statusLines = @(Get-Content -LiteralPath $gitStatusPath)
    if ($statusLines.Count -eq 0) {
        Write-ReportLine "- clean: true"
    }
    else {
        Write-ReportLine "- clean: false"
        Write-ReportLine "- changed_paths: $($statusLines.Count)"
    }
}
else {
    Write-ReportLine "- git: unavailable"
}

Write-ReportLine ""
Write-ReportLine "## Safe Files Collected"
foreach ($file in @($defaultGame, $defaultEngine, (Join-Path $projectRoot "Config\DefaultInput.ini"), (Join-Path $projectRoot "Config\DefaultScalability.ini"), $uproject, (Join-Path $projectRoot "README.md"))) {
    if (Test-Path -LiteralPath $file) {
        Copy-Item -LiteralPath $file -Destination (Join-Path $configRoot (Split-Path -Leaf $file)) -Force
        Write-ReportLine "- config: $(Get-RelativePathText -BasePath $projectRoot -Path $file)"
    }
}

Write-ReportLine ""
Write-ReportLine "## Logs"
Copy-LatestLogs -Source (Join-Path $projectRoot "Saved\Logs") -TargetName "ProjectSaved"
Copy-LatestLogs -Source (Join-Path $projectRoot "Builds\Windows\BlackoutHunt\Saved\Logs") -TargetName "WindowsPackage"
Copy-LatestLogs -Source (Join-Path $projectRoot "Builds\Linux\BlackoutHunt\Saved\Logs") -TargetName "LinuxPackage"

if ($packageRootFull) {
    Write-ReportLine ""
    Write-ReportLine "## Package Inventory"
    $inventoryPath = Join-Path $runRoot "package-inventory.csv"
    Get-ChildItem -LiteralPath $packageRootFull -Recurse -File -Force |
        ForEach-Object {
            [pscustomobject]@{
                RelativePath = Get-RelativePathText -BasePath $packageRootFull -Path $_.FullName
                Length = $_.Length
                LastWriteTime = $_.LastWriteTime.ToString("s")
            }
        } |
        Export-Csv -NoTypeInformation -LiteralPath $inventoryPath
    Write-ReportLine "- inventory: package-inventory.csv"

    $playit = Get-ChildItem -LiteralPath $packageRootFull -Recurse -File -Filter "playit.exe" -Force | Select-Object -First 1
    if ($playit) {
        $hash = (Get-FileHash -LiteralPath $playit.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        Write-ReportLine "- playit_sha256: $hash"
    }
}

Write-ReportLine ""
Write-ReportLine "## Privacy Notes"
Write-ReportLine "- Saved account data is not collected."
Write-ReportLine "- Backend `.env` files and `Tools\AccountBackend\data` are not collected."
Write-ReportLine "- Review copied logs before sending outside the project team."

if (-not $NoArchive) {
    $archivePath = Join-Path $outputRootFull "BlackoutHunt-ClassroomSupport-$timestamp.zip"
    Compress-Archive -LiteralPath $runRoot -DestinationPath $archivePath -CompressionLevel Optimal -Force
    Write-Host "Classroom support bundle created: $archivePath"
}

Write-Host "Classroom preflight report: $reportPath"
