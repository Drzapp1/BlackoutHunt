import os

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCE_ROOT = os.path.join(
    PROJECT_ROOT,
    "Content",
    "BlackoutHunt",
    "Art",
    "Downloaded",
    "KayKit",
    "KayKit-Character-Pack-Adventures-1.0",
    "addons",
    "kaykit_character_pack_adventures",
    "Assets",
    "fbx",
)
DESTINATION = "/Game/BlackoutHunt/Art/Weapons/KayKit"

WEAPONS = [
    ("axe_1handed.fbx", "SM_BH_Axe_1H"),
]


def set_if_exists(obj, name, value):
    if not hasattr(obj, name):
        return
    try:
        setattr(obj, name, value)
    except Exception:
        try:
            obj.set_editor_property(name, value)
        except Exception as exc:
            unreal.log_warning("Could not set {} on {}: {}".format(name, obj, exc))


def build_static_fbx_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = True
    options.automated_import_should_detect_type = False
    if hasattr(unreal.FBXImportType, "FBXIT_STATIC_MESH"):
        set_if_exists(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)

    if options.static_mesh_import_data:
        set_if_exists(options.static_mesh_import_data, "combine_meshes", True)
        set_if_exists(options.static_mesh_import_data, "generate_lightmap_u_vs", True)
        set_if_exists(options.static_mesh_import_data, "auto_generate_collision", False)

    return options


def import_asset(filename, destination_path, destination_name, options):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination_path
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log("Imported {} -> {}".format(filename, task.imported_object_paths))
    return task.imported_object_paths


def rename_first_static_mesh(destination_path, desired_name, imported_paths):
    desired_path = "{}/{}".format(destination_path, desired_name)
    desired_asset = unreal.load_asset(desired_path)
    if isinstance(desired_asset, unreal.StaticMesh):
        return desired_asset

    candidates = list(imported_paths)
    candidates.extend(unreal.EditorAssetLibrary.list_assets(destination_path, recursive=False, include_folder=False))
    for asset_path in candidates:
        asset = unreal.load_asset(asset_path)
        if not isinstance(asset, unreal.StaticMesh):
            continue

        source_path = str(asset_path).split(".")[0]
        if source_path != desired_path:
            if unreal.EditorAssetLibrary.does_asset_exist(desired_path):
                unreal.EditorAssetLibrary.delete_asset(desired_path)
            if not unreal.EditorAssetLibrary.rename_asset(source_path, desired_path):
                raise RuntimeError("Could not rename {} to {}".format(source_path, desired_path))
            asset = unreal.load_asset(desired_path)

        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        return asset

    raise RuntimeError("No static mesh imported for {}".format(desired_name))


def main():
    missing = [os.path.join(SOURCE_ROOT, filename) for filename, _asset_name in WEAPONS if not os.path.exists(os.path.join(SOURCE_ROOT, filename))]
    if missing:
        raise RuntimeError("Missing KayKit weapon source files:\n{}".format("\n".join(missing)))

    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    for filename, asset_name in WEAPONS:
        source_path = os.path.join(SOURCE_ROOT, filename)
        imported_paths = import_asset(source_path, DESTINATION, asset_name, build_static_fbx_options())
        mesh = rename_first_static_mesh(DESTINATION, asset_name, imported_paths)
        unreal.log("KayKit weapon mesh final: {}".format(mesh))

    unreal.EditorAssetLibrary.save_directory(DESTINATION)
    unreal.log("KayKit weapon import complete.")
    unreal.SystemLibrary.quit_editor()


main()
