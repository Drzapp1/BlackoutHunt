param(
    [string]$PreferredRoot = "D:\UE_5.7"
)

$candidates = @(
    $PreferredRoot,
    "D:\Epic Games\UE_5.7",
    "D:\Epic Games\UE_5.6",
    "C:\Program Files\Epic Games\UE_5.7",
    "C:\Program Files\Epic Games\UE_5.6"
)

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
