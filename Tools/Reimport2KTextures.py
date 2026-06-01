"""Reimport the structural blockout textures from ambientCG 2K sources (was 1K).

The T_BH_<Set>_<key> Texture2D assets were originally imported from the 1K-JPG ambientCG packs. The big
world-aligned floor/wall/ceiling surfaces tile these across huge areas, so 1K reads soft. This reimports each
asset from the matching 2K-JPG file with replace_existing_settings=False, which keeps every asset's existing
import settings (normal-map flag, linear vs sRGB, compression, LOD group) and only swaps in the higher-res
pixels. Materials reference the texture assets and baked maps reference the materials, so NO material rebuild
and NO map re-bake are needed -- the upgrade is transparent.

Run headless:
  & 'D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe' "<proj>\\BlackoutHunt.uproject" \
      -ExecutePythonScript="<proj>\\Tools\\Reimport2KTextures.py" -unattended -nop4 -nosplash -stdout -nopause
"""
import os
import unreal

TEX_DIR = "/Game/BlackoutHunt/Art/Textures"
SRC_BASE = "D:/BlackoutHunt/ThirdParty/ambientCG"

# UE asset map-key -> ambientCG 2K file suffix. UE wants DirectX-style normals -> NormalDX.
KEY_SUFFIX = {
    "color": "Color",
    "normal": "NormalDX",
    "roughness": "Roughness",
    "metallic": "Metalness",
    "ao": "AmbientOcclusion",
}
SETS = ["Concrete048", "Plaster001", "PaintedMetal004", "DiamondPlate009", "Metal063"]


def log(m):
    unreal.log("[BH 2K] " + str(m))


def src_jpg(set_name, key):
    return "{}/{}/{}_2K-JPG_{}.jpg".format(SRC_BASE, set_name, set_name, KEY_SUFFIX[key])


def main():
    tasks = []
    planned = []
    for set_name in SETS:
        for key in KEY_SUFFIX:
            asset_name = "T_BH_{}_{}".format(set_name, key)
            asset_path = "{}/{}".format(TEX_DIR, asset_name)
            if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                continue  # not every set has metallic/ao
            src = src_jpg(set_name, key)
            if not os.path.exists(src):
                unreal.log_warning("[BH 2K] missing 2K source {}".format(src))
                continue
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", src)
            task.set_editor_property("destination_path", TEX_DIR)
            task.set_editor_property("destination_name", asset_name)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("replace_existing_settings", False)  # keep normalmap/linear/sRGB/compression
            task.set_editor_property("automated", True)
            task.set_editor_property("save", True)
            task.set_editor_property("factory", unreal.TextureFactory())
            tasks.append(task)
            planned.append(asset_name)

    if not tasks:
        unreal.log_error("[BH 2K] nothing to reimport")
        return

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.EditorAssetLibrary.save_directory(TEX_DIR, only_if_is_dirty=False, recursive=False)
    for name in planned:
        log("reimported {}".format(name))
    log("done: {} textures upgraded to 2K".format(len(planned)))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error("[BH 2K] FAILED: " + str(exc))
        raise
