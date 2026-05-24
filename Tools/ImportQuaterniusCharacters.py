import os

import unreal


SOURCE_ROOT = r"D:\MainGame\Content\BlackoutHunt\Art\Downloaded\Quaternius\UltimateModularMenPack\Ultimate Modular Men- Feb 2022"
INDIVIDUAL_FBX_ROOT = os.path.join(SOURCE_ROOT, "Individual Characters", "FBX")
ANIMATIONS_FBX = os.path.join(SOURCE_ROOT, "Separate Skeletal Meshes and Animations", "Animations.fbx")

DEST_ROOT = "/Game/BlackoutHunt/Art/Characters/Quaternius"
MESH_DESTINATION = DEST_ROOT + "/Meshes"
ANIM_DESTINATION = DEST_ROOT + "/Animations"
SHARED_SKELETON_PATH = MESH_DESTINATION + "/SK_BH_Q_Skeleton"

CHARACTERS = [
    ("Casual_Hoodie.fbx", "SK_BH_Q_Casual_Hoodie"),
    ("Casual_2.fbx", "SK_BH_Q_Casual_2"),
    ("Worker.fbx", "SK_BH_Q_Worker"),
    ("Adventurer.fbx", "SK_BH_Q_Adventurer"),
    ("Farmer.fbx", "SK_BH_Q_Farmer"),
    ("Beach.fbx", "SK_BH_Q_Beach"),
    ("Punk.fbx", "SK_BH_Q_Punk"),
    ("Suit.fbx", "SK_BH_Q_Suit"),
    ("Swat.fbx", "SK_BH_Q_Swat"),
    ("Spacesuit.fbx", "SK_BH_Q_Spacesuit"),
    ("King.fbx", "SK_BH_Q_King"),
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


def ensure_source_files():
    missing = []
    for filename, _asset_name in CHARACTERS:
        path = os.path.join(INDIVIDUAL_FBX_ROOT, filename)
        if not os.path.exists(path):
            missing.append(path)
    if not os.path.exists(ANIMATIONS_FBX):
        missing.append(ANIMATIONS_FBX)
    if missing:
        raise RuntimeError("Missing Quaternius source files:\n{}".format("\n".join(missing)))


def clear_destination():
    if unreal.EditorAssetLibrary.does_directory_exist(DEST_ROOT):
        unreal.EditorAssetLibrary.delete_directory(DEST_ROOT)
    unreal.EditorAssetLibrary.make_directory(MESH_DESTINATION)
    unreal.EditorAssetLibrary.make_directory(ANIM_DESTINATION)


def skeletal_mesh_options(skeleton=None):
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = True
    options.import_textures = False
    options.automated_import_should_detect_type = False
    if hasattr(unreal.FBXImportType, "FBXIT_SKELETAL_MESH"):
        set_if_exists(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    if skeleton:
        set_if_exists(options, "skeleton", skeleton)

    if options.skeletal_mesh_import_data:
        set_if_exists(options.skeletal_mesh_import_data, "import_meshes_in_bone_hierarchy", True)
        set_if_exists(options.skeletal_mesh_import_data, "update_skeleton_reference_pose", False)
        set_if_exists(options.skeletal_mesh_import_data, "use_t0_as_ref_pose", False)

    return options


def animation_options(skeleton):
    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    if hasattr(unreal.FBXImportType, "FBXIT_ANIMATION"):
        set_if_exists(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    set_if_exists(options, "skeleton", skeleton)

    if options.anim_sequence_import_data:
        set_if_exists(options.anim_sequence_import_data, "import_custom_attribute", True)
        set_if_exists(options.anim_sequence_import_data, "delete_existing_custom_attribute_curves", False)
        set_if_exists(options.anim_sequence_import_data, "remove_redundant_keys", True)

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


def load_assets_of_type(paths, asset_type):
    assets = []
    for path in paths:
        asset = unreal.load_asset(path)
        if isinstance(asset, asset_type):
            assets.append((path, asset))
    return assets


def find_asset_of_type(destination_path, asset_type, name_hint=None):
    for asset_path in unreal.EditorAssetLibrary.list_assets(destination_path, recursive=False, include_folder=False):
        asset = unreal.load_asset(asset_path)
        if isinstance(asset, asset_type) and (not name_hint or name_hint.lower() in asset.get_name().lower()):
            return asset_path, asset
    return None, None


def rename_asset(asset_path, desired_path):
    asset_path = str(asset_path).split(".")[0]
    desired_path = str(desired_path).split(".")[0]
    if asset_path == desired_path:
        return unreal.load_asset(desired_path)
    if unreal.EditorAssetLibrary.does_asset_exist(desired_path):
        unreal.EditorAssetLibrary.delete_asset(desired_path)
    if not unreal.EditorAssetLibrary.rename_asset(asset_path, desired_path):
        raise RuntimeError("Could not rename {} to {}".format(asset_path, desired_path))
    return unreal.load_asset(desired_path)


def get_skeleton_from_mesh(skeletal_mesh):
    for property_name in ("skeleton", "Skeleton"):
        try:
            skeleton = skeletal_mesh.get_editor_property(property_name)
            if skeleton:
                return skeleton
        except Exception:
            pass
    return None


def import_character_mesh(fbx_name, asset_name, skeleton=None):
    source_path = os.path.join(INDIVIDUAL_FBX_ROOT, fbx_name)
    before = set(unreal.EditorAssetLibrary.list_assets(MESH_DESTINATION, recursive=False, include_folder=False))
    imported_paths = import_asset(source_path, MESH_DESTINATION, asset_name, skeletal_mesh_options(skeleton))
    after = set(unreal.EditorAssetLibrary.list_assets(MESH_DESTINATION, recursive=False, include_folder=False))
    new_paths = list(after - before) + list(imported_paths)

    skeletal_meshes = load_assets_of_type(new_paths, unreal.SkeletalMesh)
    if skeletal_meshes:
        mesh_path, mesh = skeletal_meshes[0]
    else:
        mesh_path, mesh = find_asset_of_type(MESH_DESTINATION, unreal.SkeletalMesh, asset_name.replace("SK_BH_Q_", ""))

    if not mesh:
        raise RuntimeError("No skeletal mesh was imported from {}".format(source_path))

    desired_path = "{}/{}".format(MESH_DESTINATION, asset_name)
    mesh = rename_asset(mesh_path, desired_path)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    return mesh


def rename_shared_skeleton(first_mesh):
    skeleton = get_skeleton_from_mesh(first_mesh)
    if not skeleton:
        raise RuntimeError("Could not resolve imported Quaternius skeleton")

    skeleton_path = skeleton.get_path_name().split(".")[0]
    if skeleton_path != SHARED_SKELETON_PATH:
        skeleton = rename_asset(skeleton_path, SHARED_SKELETON_PATH)
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton)
    return skeleton


def import_meshes():
    shared_skeleton = None
    imported_meshes = []
    for index, (fbx_name, asset_name) in enumerate(CHARACTERS):
        mesh = import_character_mesh(fbx_name, asset_name, shared_skeleton)
        imported_meshes.append(mesh)
        if index == 0:
            shared_skeleton = rename_shared_skeleton(mesh)
    return shared_skeleton, imported_meshes


def import_animations(shared_skeleton):
    import_asset(ANIMATIONS_FBX, ANIM_DESTINATION, "BH_Q_Animations", animation_options(shared_skeleton))


def choose_animation(assets, required, rejected=()):
    best = None
    best_score = -1
    for asset_path, asset in assets:
        name = asset.get_name().lower()
        if required not in name:
            continue
        if any(token in name for token in rejected):
            continue
        score = 1
        if name == required:
            score += 100
        if name.endswith("_" + required) or name.endswith("|" + required):
            score += 50
        if "neutral" in name:
            score += 10
        if name.startswith("a_bh_q_"):
            score -= 20
        if score > best_score:
            best = (asset_path, asset)
            best_score = score
    return best


def canonicalize_animations():
    animation_assets = []
    for asset_path in unreal.EditorAssetLibrary.list_assets(ANIM_DESTINATION, recursive=True, include_folder=False):
        asset = unreal.load_asset(asset_path)
        if isinstance(asset, unreal.AnimSequence):
            animation_assets.append((asset_path, asset))

    picks = {
        "A_BH_Q_Idle": choose_animation(animation_assets, "idle", ("gun", "sword", "shoot")),
        "A_BH_Q_Walk": choose_animation(animation_assets, "walk", ("back", "left", "right", "gun", "shoot")),
        "A_BH_Q_Run": choose_animation(animation_assets, "run", ("back", "left", "right", "gun", "shoot")),
        "A_BH_Q_Death": choose_animation(animation_assets, "death", ()),
    }

    for desired_name, picked in picks.items():
        if not picked:
            unreal.log_warning("Could not find animation for {}".format(desired_name))
            continue
        asset_path, _asset = picked
        desired_path = "{}/{}".format(ANIM_DESTINATION, desired_name)
        asset = rename_asset(asset_path, desired_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

    unreal.log("Imported {} Quaternius animation assets".format(len(animation_assets)))


def main():
    ensure_source_files()
    clear_destination()
    skeleton, meshes = import_meshes()
    import_animations(skeleton)
    canonicalize_animations()
    unreal.EditorAssetLibrary.save_directory(DEST_ROOT)
    unreal.log("Quaternius import complete. Meshes={} Skeleton={}".format(len(meshes), skeleton))
    unreal.SystemLibrary.quit_editor()


main()
