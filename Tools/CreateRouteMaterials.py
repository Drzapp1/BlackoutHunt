"""Create the BlackoutHunt route-colour material palette (serializable MaterialInstanceConstants).

The authoring bake assigns these to the saturated decorative cue blocks (route stripes, signs, pads,
beacons) so the colour survives into the .umap -- the runtime per-instance tint was a dynamic material
instance that could not serialize. Parented to /Engine/BasicShapes/BasicShapeMaterial (which exposes a
"Color" vector param, as the runtime block field uses).

Run:
  & "D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "D:\\BlackoutHunt\\BlackoutHunt.uproject" `
      -ExecutePythonScript="D:\\BlackoutHunt\\Tools\\CreateRouteMaterials.py" -unattended -nop4 -nosplash -nopause -stdout
"""
import unreal


def log(m):
    unreal.log("[BH RouteMats] " + str(m))


PKG = "/Game/BlackoutHunt/Art/Materials"
BASE = unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")

# name -> (r, g, b). Matches the Facility/Substation route identity.
COLORS = {
    "Green":  (0.10, 0.85, 0.42),   # exits
    "Amber":  (0.95, 0.55, 0.14),   # storage
    "Cyan":   (0.14, 0.68, 0.92),   # lab
    "Violet": (0.66, 0.34, 0.92),   # ward
    "Yellow": (0.95, 0.80, 0.16),   # warning / revision
    "Red":    (0.90, 0.13, 0.10),   # hazard
    "Blue":   (0.16, 0.55, 0.92),   # revision
}


def main():
    if not BASE:
        unreal.log_error("[BH RouteMats] BasicShapeMaterial not found")
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialInstanceConstantFactoryNew()
    for name, (r, g, b) in COLORS.items():
        asset = "MI_BH_Route_{}".format(name)
        full = "{}/{}.{}".format(PKG, asset, asset)
        if unreal.EditorAssetLibrary.does_asset_exist("{}/{}".format(PKG, asset)):
            mic = unreal.load_asset(full)
        else:
            mic = tools.create_asset(asset, PKG, unreal.MaterialInstanceConstant, factory)
        if not mic:
            log("FAILED to create {}".format(asset))
            continue
        unreal.MaterialEditingLibrary.set_material_instance_parent(mic, BASE)
        col = unreal.LinearColor(r, g, b, 1.0)
        for param in ("Color", "BaseColor"):
            try:
                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mic, param, col)
            except Exception:
                pass
        unreal.EditorAssetLibrary.save_asset("{}/{}".format(PKG, asset), only_if_is_dirty=False)
        log("wrote {} = ({:.2f},{:.2f},{:.2f})".format(asset, r, g, b))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("[BH RouteMats] FAILED: " + str(exc))
finally:
    unreal.SystemLibrary.quit_editor()
