import os

import unreal


DESTINATION_PATH = "/Game/BlackoutHunt/Art/SCP096/Skeletal"
FBX_PATH = r"D:\MainGame\Content\BlackoutHunt\Art\SCP096\SourceFiles\source\SCP-096.fbx"
MATERIAL_PATH = "/Game/BlackoutHunt/Art/SCP096/M_SCP096"


def set_if_exists(obj, name, value):
    if hasattr(obj, name):
        try:
            setattr(obj, name, value)
        except Exception as exc:
            unreal.log_warning("Could not set {}: {}".format(name, exc))


def import_skeletal():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    set_if_exists(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)

    if options.skeletal_mesh_import_data:
        set_if_exists(options.skeletal_mesh_import_data, "import_meshes_in_bone_hierarchy", True)
        set_if_exists(options.skeletal_mesh_import_data, "update_skeleton_reference_pose", True)
        set_if_exists(options.skeletal_mesh_import_data, "use_t0_as_ref_pose", False)

    if options.anim_sequence_import_data:
        set_if_exists(options.anim_sequence_import_data, "import_custom_attribute", True)
        set_if_exists(options.anim_sequence_import_data, "delete_existing_custom_attribute_curves", False)

    task = unreal.AssetImportTask()
    task.filename = FBX_PATH
    task.destination_path = DESTINATION_PATH
    task.destination_name = "SK_SCP096"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log("SCP096 skeletal imported paths: {}".format(task.imported_object_paths))

    material = unreal.load_asset(MATERIAL_PATH)
    run_anim = None
    skeletal_mesh = None
    assets = unreal.EditorAssetLibrary.list_assets(DESTINATION_PATH, recursive=False, include_folder=False)
    for asset_path in assets:
        asset = unreal.load_asset(asset_path)
        if isinstance(asset, unreal.SkeletalMesh):
            skeletal_mesh = asset
            if asset_path != DESTINATION_PATH + "/SK_SCP096":
                unreal.EditorAssetLibrary.rename_asset(asset_path, DESTINATION_PATH + "/SK_SCP096")
                skeletal_mesh = unreal.load_asset(DESTINATION_PATH + "/SK_SCP096")
        elif isinstance(asset, unreal.AnimSequence):
            run_anim = asset
            if asset_path != DESTINATION_PATH + "/A_SCP096_Run":
                unreal.EditorAssetLibrary.rename_asset(asset_path, DESTINATION_PATH + "/A_SCP096_Run")
                run_anim = unreal.load_asset(DESTINATION_PATH + "/A_SCP096_Run")

    if skeletal_mesh and material:
        skeletal_mesh.set_material(0, material)
        unreal.EditorAssetLibrary.save_loaded_asset(skeletal_mesh)
    if run_anim:
        unreal.EditorAssetLibrary.save_loaded_asset(run_anim)

    unreal.EditorAssetLibrary.save_directory(DESTINATION_PATH)
    unreal.log("SCP096 skeletal final mesh={} anim={}".format(skeletal_mesh, run_anim))
    unreal.SystemLibrary.quit_editor()


if not os.path.exists(FBX_PATH):
    raise RuntimeError("Missing FBX: {}".format(FBX_PATH))

import_skeletal()
