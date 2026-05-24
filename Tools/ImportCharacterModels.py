import os
import shutil
import zipfile

import unreal


ART_ROOT = r"D:\MainGame\Content\BlackoutHunt\Art"
HIDER_ZIP = os.path.join(ART_ROOT, "low-poly-character-base-mesh.zip")
HUNTER_ZIP = os.path.join(ART_ROOT, "fnati-classic-undying.zip")

HIDER_SOURCE_ROOT = os.path.join(ART_ROOT, "Characters", "Hider", "SourceFiles")
HUNTER_SOURCE_ROOT = os.path.join(ART_ROOT, "Characters", "Hunter", "SourceFiles")

HIDER_DESTINATION = "/Game/BlackoutHunt/Art/Characters/Hider"
HUNTER_DESTINATION = "/Game/BlackoutHunt/Art/Characters/Hunter"


def reset_directory(path):
    if os.path.exists(path):
        shutil.rmtree(path)
    os.makedirs(path, exist_ok=True)


def extract_models():
    if not os.path.exists(HIDER_ZIP):
        raise RuntimeError("Missing hider model package: {}".format(HIDER_ZIP))
    if not os.path.exists(HUNTER_ZIP):
        raise RuntimeError("Missing hunter model package: {}".format(HUNTER_ZIP))

    reset_directory(HIDER_SOURCE_ROOT)
    with zipfile.ZipFile(HIDER_ZIP, "r") as archive:
        archive.extractall(HIDER_SOURCE_ROOT)

    reset_directory(HUNTER_SOURCE_ROOT)
    outer_root = os.path.join(HUNTER_SOURCE_ROOT, "_outer")
    with zipfile.ZipFile(HUNTER_ZIP, "r") as archive:
        archive.extractall(outer_root)

    nested_zip = os.path.join(outer_root, "source", "fnati_classic_undying.zip")
    if not os.path.exists(nested_zip):
        raise RuntimeError("Missing nested hunter zip: {}".format(nested_zip))
    with zipfile.ZipFile(nested_zip, "r") as archive:
        archive.extractall(HUNTER_SOURCE_ROOT)
    shutil.rmtree(outer_root, ignore_errors=True)


def import_asset(filename, destination_path, destination_name, options=None):
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


def set_if_exists(obj, name, value):
    if hasattr(obj, name):
        try:
            setattr(obj, name, value)
        except Exception as exc:
            unreal.log_warning("Could not set {}: {}".format(name, exc))


def build_static_fbx_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    set_if_exists(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)

    if options.static_mesh_import_data:
        set_if_exists(options.static_mesh_import_data, "combine_meshes", True)
        set_if_exists(options.static_mesh_import_data, "generate_lightmap_u_vs", True)
        set_if_exists(options.static_mesh_import_data, "auto_generate_collision", False)

    return options


def ensure_material(destination_path, name, base_color):
    material_path = "{}/{}".format(destination_path, name)
    existing = unreal.load_asset(material_path)
    if existing:
        unreal.EditorAssetLibrary.delete_asset(material_path)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        destination_path,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        return None

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -420,
        0,
    )
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", base_color)
    unreal.MaterialEditingLibrary.connect_material_property(
        color,
        "",
        unreal.MaterialProperty.MP_BASE_COLOR,
    )

    roughness = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionScalarParameter,
        -420,
        180,
    )
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.62)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def assign_material(mesh, material):
    if not mesh or not material:
        return
    slot_count = 1
    if hasattr(mesh, "get_num_sections"):
        try:
            slot_count = max(1, mesh.get_num_sections(0))
        except Exception:
            slot_count = 1
    if hasattr(mesh, "get_materials"):
        try:
            slot_count = max(slot_count, len(mesh.get_materials()))
        except Exception:
            pass
    for index in range(slot_count):
        try:
            mesh.set_material(index, material)
        except Exception as exc:
            unreal.log_warning("Could not assign material slot {} on {}: {}".format(index, mesh, exc))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def rename_first_asset_of_type(destination_path, desired_name, asset_type):
    desired_path = "{}/{}".format(destination_path, desired_name)
    asset = unreal.load_asset(desired_path)
    if isinstance(asset, asset_type):
        return asset

    for asset_path in unreal.EditorAssetLibrary.list_assets(destination_path, recursive=True, include_folder=False):
        candidate = unreal.load_asset(asset_path)
        if isinstance(candidate, asset_type):
            if asset_path != desired_path:
                if unreal.EditorAssetLibrary.does_asset_exist(desired_path):
                    unreal.EditorAssetLibrary.delete_asset(desired_path)
                unreal.EditorAssetLibrary.rename_asset(asset_path, desired_path)
                candidate = unreal.load_asset(desired_path)
            return candidate
    return None


def import_hider():
    fbx_path = os.path.join(HIDER_SOURCE_ROOT, "source", "Male_Character_Base_Mesh_Substance.fbx")
    if not os.path.exists(fbx_path):
        raise RuntimeError("Missing hider FBX: {}".format(fbx_path))

    import_asset(fbx_path, HIDER_DESTINATION, "SM_BH_Hider", build_static_fbx_options())
    mesh = unreal.load_asset("{}/SM_BH_Hider".format(HIDER_DESTINATION))
    if not mesh:
        mesh = rename_first_asset_of_type(HIDER_DESTINATION, "SM_BH_Hider", unreal.StaticMesh)

    material = ensure_material(HIDER_DESTINATION, "M_BH_Hider", unreal.LinearColor(0.80, 0.58, 0.44, 1.0))
    assign_material(mesh, material)
    unreal.log("Hider mesh final: {}".format(mesh))


def import_hunter():
    gltf_path = os.path.join(HUNTER_SOURCE_ROOT, "scene.gltf")
    if not os.path.exists(gltf_path):
        raise RuntimeError("Missing hunter glTF: {}".format(gltf_path))

    import_asset(gltf_path, HUNTER_DESTINATION, "SK_BH_Hunter")
    material = ensure_material(HUNTER_DESTINATION, "M_BH_Hunter", unreal.LinearColor(0.19, 0.025, 0.020, 1.0))

    skeletal_mesh = rename_first_asset_of_type(HUNTER_DESTINATION, "SK_BH_Hunter", unreal.SkeletalMesh)
    static_mesh = rename_first_asset_of_type(HUNTER_DESTINATION, "SM_BH_Hunter", unreal.StaticMesh)
    if skeletal_mesh:
        assign_material(skeletal_mesh, material)
    if static_mesh:
        assign_material(static_mesh, material)
    unreal.log("Hunter mesh final skeletal={} static={}".format(skeletal_mesh, static_mesh))


def main():
    extract_models()
    import_hider()
    import_hunter()
    unreal.EditorAssetLibrary.save_directory("/Game/BlackoutHunt/Art/Characters")
    unreal.log("Character model import complete.")
    unreal.SystemLibrary.quit_editor()


main()
