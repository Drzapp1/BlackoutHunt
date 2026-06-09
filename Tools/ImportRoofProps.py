"""Imports a few CC0 Kenney "Space Kit" meshes for the train-roof stargazing area.

Source: Kenney Space Kit (CC0 / public domain) downloaded to
  Content/BlackoutHunt/Art/Downloaded/Kenney/SpaceKit/
Run (editor closed):
  & 'D:\\UE_5.7\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe' "D:\\BlackoutHunt\\BlackoutHunt.uproject" \
      -ExecutePythonScript="D:\\BlackoutHunt\\Tools\\ImportRoofProps.py" -unattended -nop4 -nosplash -stdout -nopause

The C++ (ABHGameMode::BuildTrainRoofExtras) references these /Game paths via SpawnRuntimeMeshProp with a
procedural-block fallback, so the roof works whether or not these assets are present.
"""

import os

import unreal


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SOURCE_ROOT = os.path.join(
    PROJECT_ROOT, "Content", "BlackoutHunt", "Art", "Downloaded", "Kenney", "SpaceKit"
)
DESTINATION = "/Game/BlackoutHunt/Art/Roof"

# (source FBX filename, destination StaticMesh asset name). The satellite dish on its mount reads as a roof
# observatory "telescope"; the rest dress the stargazing deck.
PROPS = [
    ("satelliteDish_detailed.fbx", "SM_BH_Telescope"),
    ("machine_wireless.fbx", "SM_BH_RoofAntenna"),
    ("rock_crystalsLargeA.fbx", "SM_BH_RoofRocks"),
    ("meteor.fbx", "SM_BH_RoofMeteor"),
    ("desk_chairStool.fbx", "SM_BH_RoofStool"),
]


def find_source(filename):
    for root, _dirs, files in os.walk(SOURCE_ROOT):
        if filename in files:
            return os.path.join(root, filename)
    return None


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
        set_if_exists(options.static_mesh_import_data, "auto_generate_collision", True)
        # Kenney FBX import small in UE; scale up so the props are roughly metre-sized on the roof.
        set_if_exists(options.static_mesh_import_data, "import_uniform_scale", 100.0)
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
    if isinstance(unreal.load_asset(desired_path), unreal.StaticMesh):
        return unreal.load_asset(desired_path)
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
    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    imported = 0
    for filename, asset_name in PROPS:
        source_path = find_source(filename)
        if not source_path:
            unreal.log_warning("Roof prop source not found, skipping: {}".format(filename))
            continue
        paths = import_asset(source_path, DESTINATION, asset_name, build_static_fbx_options())
        mesh = rename_first_static_mesh(DESTINATION, asset_name, paths)
        unreal.log("Roof prop final: {} -> {}".format(asset_name, mesh))
        imported += 1
    unreal.EditorAssetLibrary.save_directory(DESTINATION)
    unreal.log("Roof prop import complete: {} of {} imported.".format(imported, len(PROPS)))
    unreal.SystemLibrary.quit_editor()


main()
