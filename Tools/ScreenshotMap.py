"""Capture editor-viewport screenshots of an authored BlackoutHunt map.

Runs in the GUI editor (UnrealEditor.exe, NOT -Cmd, so there is a rendering viewport). Loads the map,
points the perspective viewport at a few framings, fires `HighResShot` for each, then quits. Output lands
in Saved/Screenshots/ (HighResScreenshot*.png).

Run:
  & "D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "D:\\BlackoutHunt\\BlackoutHunt.uproject" `
      -ExecutePythonScript="D:\\BlackoutHunt\\Tools\\ScreenshotMap.py Facility" -nosplash -nopause
"""

import sys

import unreal


def log(msg):
    unreal.log("[BH Screenshot] " + str(msg))


level = sys.argv[1] if len(sys.argv) > 1 else "Facility"
asset_path = "/Game/BlackoutHunt/Maps/{}".format(level)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les.load_level(asset_path)
world = ues.get_editor_world()

# (location, rotation) framings tuned to the Facility envelope (X +/-5500, Y +/-4400, ceiling 335).
VIEWS = [
    (unreal.Vector(0.0, 0.0, 5200.0),   unreal.Rotator(-88.0, 0.0, 0.0)),    # top-down floorplan
    (unreal.Vector(4600.0, 0.0, 240.0), unreal.Rotator(-5.0, 180.0, 0.0)),   # spawn lane -> hub
    (unreal.Vector(0.0, 0.0, 240.0),    unreal.Rotator(-3.0, 0.0, 0.0)),     # hub eye level
    (unreal.Vector(-2150.0, -2600.0, 240.0), unreal.Rotator(-4.0, 90.0, 0.0)),  # storage room
]

S = {"n": 0, "i": 0}
PERIOD = 30  # ticks per view (boot/render headroom)


def tick(delta):
    S["n"] += 1
    n = S["n"]
    if S["i"] < len(VIEWS):
        phase = n % PERIOD
        if phase == 4:
            loc, rot = VIEWS[S["i"]]
            ues.set_level_viewport_camera_info(loc, rot)
        elif phase == 22:
            unreal.SystemLibrary.execute_console_command(world, "HighResShot 1920x1080")
            log("shot {} of {}".format(S["i"] + 1, len(VIEWS)))
        elif phase == 29:
            S["i"] += 1
    else:
        if n % PERIOD == 10:
            unreal.unregister_slate_post_tick_callback(HANDLE)
            log("done; quitting")
            unreal.SystemLibrary.quit_editor()


HANDLE = unreal.register_slate_post_tick_callback(tick)
log("registered tick; loading {}".format(asset_path))
