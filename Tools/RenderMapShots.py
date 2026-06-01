"""Render PNG shots of an authored BlackoutHunt map, headless.

SceneCapture2D -> TextureRenderTarget2D(RTF_RGBA8) -> export_render_target (GPU render to a texture, no live
viewport needed). For a clear "what does it look like" pass the capture: disables fog, hides the ceiling
slab, disables the mood PostProcessVolume, and floods the interior with a single moderate directional light
(like sun through an open roof) so geometry + wall cladding read evenly without point-blank blowout. None of
this is saved; the shipped map keeps its dynamic flicker-light horror mood + fog.

Run:
  & "D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "D:\\BlackoutHunt\\BlackoutHunt.uproject" `
      -ExecutePythonScript="D:\\BlackoutHunt\\Tools\\RenderMapShots.py Facility" `
      -AllowCommandletRendering -unattended -nop4 -nosplash -nopause -stdout
"""

import os
import sys

import unreal


def log(msg):
    unreal.log("[BH Render] " + str(msg))


LEVEL = sys.argv[1] if len(sys.argv) > 1 else "Facility"
# mode "flood" (default): white flood + ceiling hidden, for clear geometry. "scene": no flood, keep ceiling,
# let the map's own colored flicker lights light it (shows the real in-game route-colour atmosphere).
MODE = sys.argv[2] if len(sys.argv) > 2 else "flood"
ASSET = "/Game/BlackoutHunt/Maps/{}".format(LEVEL)
OUTDIR = os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "Renders"
)

# (name, location, rotation(pitch,yaw,roll)) -- all perspective, placed in open lanes along the spine.
SHOTS = {
    "Facility": [
        ("02_hub_spine",      unreal.Vector(-700, 0, 175), unreal.Rotator(-4, 0, 0)),
        ("03_east_rooms",     unreal.Vector(2300, 0, 175), unreal.Rotator(-4, 0, 0)),
        ("04_west_rooms",     unreal.Vector(-2300, 0, 175), unreal.Rotator(-4, 180, 0)),
        ("05_spawn_lane",     unreal.Vector(4700, 0, 185), unreal.Rotator(-6, 180, 0)),
        ("06_cross_spine",    unreal.Vector(0, 0, 175), unreal.Rotator(-4, 90, 0)),
        ("07_perimeter_wall", unreal.Vector(-5000, 0, 175), unreal.Rotator(-2, 180, 0)),
        ("08_internal_wall",  unreal.Vector(0, -700, 165), unreal.Rotator(-2, -90, 0)),
    ],
    "Substation": [("02_interior", unreal.Vector(0, 0, 180), unreal.Rotator(-4, 0, 0))],
    "Foggrounds": [("02_interior", unreal.Vector(0, 0, 220), unreal.Rotator(-4, 0, 0))],
}


def main():
    if not os.path.isdir(OUTDIR):
        os.makedirs(OUTDIR)

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les.load_level(ASSET)
    world = ues.get_editor_world()

    hidden_ceiling = 0
    disabled_pp = 0
    for actor in eas.get_all_level_actors():
        if isinstance(actor, unreal.ExponentialHeightFog):
            comp = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
            if comp:
                try:
                    comp.set_editor_property("fog_density", 0.0)
                except Exception:
                    pass
        elif isinstance(actor, unreal.PostProcessVolume):
            try:
                actor.set_editor_property("enabled", False)
                disabled_pp += 1
            except Exception:
                pass
        elif isinstance(actor, unreal.StaticMeshActor) and MODE != "scene":
            s = actor.get_actor_scale3d()
            z = actor.get_actor_location().z
            if s.z <= 0.5 and s.x >= 50.0 and s.y >= 50.0 and z > 300.0:
                actor.set_actor_hidden_in_game(True)
                smc = actor.get_component_by_class(unreal.StaticMeshComponent)
                if smc:
                    smc.set_visibility(False, True)
                hidden_ceiling += 1
    log("mode={}; fog off; hid {} ceiling slab(s); disabled {} post-process volume(s)".format(MODE, hidden_ceiling, disabled_pp))

    if MODE != "scene":
        # Flood key (open-roof daylight) so geometry/cladding read. "flood" washes out the coloured lights;
        # "balanced" uses a dim key so the map's coloured route lights still tint the scene.
        key = 3.2 if MODE == "flood" else 0.6
        back = 2.0 if MODE == "flood" else 0.35
        dl = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(-58, -35, 0))
        try:
            dlc = dl.get_component_by_class(unreal.DirectionalLightComponent)
            dlc.set_mobility(unreal.ComponentMobility.MOVABLE)
            dlc.set_intensity(key)
        except Exception as exc:
            log("fill light warn: {}".format(exc))
        dl2 = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(-45, 150, 0))
        try:
            dl2c = dl2.get_component_by_class(unreal.DirectionalLightComponent)
            dl2c.set_mobility(unreal.ComponentMobility.MOVABLE)
            dl2c.set_intensity(back)
            dl2c.set_cast_shadows(False)
        except Exception as exc:
            log("backfill warn: {}".format(exc))
        sky = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500), unreal.Rotator(0, 0, 0))
        try:
            skc = sky.get_component_by_class(unreal.SkyLightComponent)
            skc.set_mobility(unreal.ComponentMobility.MOVABLE)
            skc.set_intensity(0.3 if MODE == "balanced" else 0.5)
        except Exception as exc:
            log("sky warn: {}".format(exc))
    # MODE == "scene": add no lights -- the map's own coloured flicker lights light it (real route-colour mood).

    rt = unreal.RenderingLibrary.create_render_target2d(world, 1920, 1080, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    if not rt:
        log("FAILED to create render target")
        return

    for name, loc, rot in SHOTS.get(LEVEL, SHOTS["Facility"]):
        try:
            cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, loc, rot)
            scc = cap.get_component_by_class(unreal.SceneCaptureComponent2D)
            scc.set_editor_property("texture_target", rt)
            scc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            scc.set_editor_property("fov_angle", 95.0)
            scc.capture_scene()
            scc.capture_scene()
            unreal.RenderingLibrary.export_render_target(world, rt, OUTDIR, name + ".png")
            eas.destroy_actor(cap)
            log("rendered {}".format(name))
        except Exception as exc:
            log("shot {} FAILED: {}".format(name, exc))

    log("done -> {}".format(OUTDIR))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("[BH Render] FAILED: " + str(exc))
finally:
    unreal.SystemLibrary.quit_editor()
