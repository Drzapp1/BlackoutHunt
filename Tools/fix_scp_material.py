"""Headless fix: enable 'Used with Skeletal Mesh' on the SCP-096 material and resave it.

In a packaged / standalone -game session UE will NOT auto-add a material usage flag at
runtime; it silently swaps in the default grey material instead (which is why SCP-096 looked
like it "didn't load"). Setting the flag on the asset and saving bakes it in so the real
material is used everywhere. Run via:
  UnrealEditor-Cmd.exe BlackoutHunt.uproject -run=pythonscript -script="<abs path>"
"""
import unreal

MAT_PATH = "/Game/BlackoutHunt/Art/SCP096/M_SCP096"


def main():
    mat = unreal.load_asset(MAT_PATH)
    if mat is None:
        unreal.log_error("SCP fix: could not load material %s" % MAT_PATH)
        return

    before = None
    try:
        before = mat.get_editor_property("used_with_skeletal_mesh")
    except Exception as exc:  # property may be named differently across versions
        unreal.log_warning("SCP fix: could not read used_with_skeletal_mesh (%s)" % exc)

    set_ok = False
    for prop in ("used_with_skeletal_mesh", "bUsedWithSkeletalMesh"):
        try:
            mat.set_editor_property(prop, True)
            set_ok = True
            break
        except Exception as exc:
            unreal.log_warning("SCP fix: set_editor_property('%s') failed: %s" % (prop, exc))

    # Also flag morph-target usage is unnecessary here; skeletal mesh is the one that matters.
    saved = unreal.EditorAssetLibrary.save_asset(MAT_PATH, only_if_is_dirty=False)

    after = None
    try:
        after = unreal.load_asset(MAT_PATH).get_editor_property("used_with_skeletal_mesh")
    except Exception:
        pass

    unreal.log("SCP fix: used_with_skeletal_mesh before=%s set_ok=%s after=%s saved=%s"
               % (before, set_ok, after, saved))


main()
