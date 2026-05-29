param(
    [string]$PreferredRoot = "D:\UE_5.7"
)

# Resolve the drive this repo lives on so the bundled UE 5.7 engine is found no
# matter which Windows drive letter the project drive happens to mount as
# (D:, E:, F:, ...). The engine is expected at <drive>\UE_5.7 next to the project.
$selfDrive = $null
if ($PSScriptRoot) {
    try { $selfDrive = Split-Path -Qualifier $PSScriptRoot } catch { $selfDrive = $null }
}

$candidates = @($PreferredRoot)
if ($selfDrive) {
    $candidates += @(
        "$selfDrive\UE_5.7",
        "$selfDrive\Epic Games\UE_5.7",
        "$selfDrive\Epic Games\UE_5.6"
    )
}
$candidates += @(
    "D:\UE_5.7",
    "D:\Epic Games\UE_5.7",
    "D:\Epic Games\UE_5.6",
    "C:\Program Files\Epic Games\UE_5.7",
    "C:\Program Files\Epic Games\UE_5.6"
)
$candidates = $candidates | Select-Object -Unique

foreach ($root in $candidates) {
    $editor = Join-Path $root "Engine\Binaries\Win64\UnrealEditor.exe"
    if (Test-Path $editor) {
        [pscustomobject]@{
            Root = $root
            Editor = $editor
            Build = Join-Path $root "Engine\Build\BatchFiles\Build.bat"
            RunUAT = Join-Path $root "Engine\Build\BatchFiles\RunUAT.bat"
        }
        exit 0
    }
}

Write-Error "UnrealEditor.exe was not found. Install Unreal Engine 5.7, then rerun."
exit 1
