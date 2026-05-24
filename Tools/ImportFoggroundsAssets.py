import os
import shutil
import zipfile

import unreal


PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
THIRD_PARTY_ROOT = os.path.join(PROJECT_DIR, "ThirdParty", "Kenney")
SOURCE_ROOT = os.path.join(THIRD_PARTY_ROOT, "NatureKit")
ZIP_CANDIDATES = [
    os.path.join(THIRD_PARTY_ROOT, "NatureKit.zip"),
    os.path.join(THIRD_PARTY_ROOT, "nature-kit.zip"),
]
DESTINATION = "/Game/BlackoutHunt/Art/Foggrounds/Nature"
KENNEY_NATURE_URL = "https://kenney.nl/assets/nature-kit"


ASSETS = [
    ("SM_BH_FogTree_A", ("tree_pine", "pine", "tree_default", "tree")),
    ("SM_BH_FogTree_B", ("tree_oak", "tree_round", "tree_large", "tree")),
    ("SM_BH_FogTree_C", ("tree_dead", "dead_tree", "tree_bare", "tree")),
    ("SM_BH_FogRock_A", ("rock_large", "rock_1", "rock")),
    ("SM_BH_FogRock_B", ("rock_small", "rock_2", "stone")),
    ("SM_BH_FogFence", ("fence", "wood_fence", "barrier")),
]


def log(message):
    unreal.log("[BlackoutHunt Foggrounds Import] " + message)


def fail(message):
    unreal.log_error("[BlackoutHunt Foggrounds Import] " + message)
    raise RuntimeError(message)


def ensure_source_root():
    if os.path.isdir(SOURCE_ROOT):
        return

    for zip_path in ZIP_CANDIDATES:
        if os.path.exists(zip_path):
            os.makedirs(SOURCE_ROOT, exist_ok=True)
            with zipfile.ZipFile(zip_path, "r") as archive:
                archive.extractall(SOURCE_ROOT)
            log("Extracted {}".format(zip_path))
            return

    fail(
        "Missing Kenney Nature Kit source. Download the CC0 pack from {} and place it as "
        "ThirdParty/Kenney/NatureKit.zip, or extract it to ThirdParty/Kenney/NatureKit.".format(KENNEY_NATURE_URL)
    )


def verify_cc0_license():
    license_text = ""
    for root, _, files in os.walk(SOURCE_ROOT):
        for filename in files:
            lower = filename.lower()
            if "license" in lower or "readme" in lower:
                path = os.path.join(root, filename)
                try:
                    with open(path, "r", encoding="utf-8", errors="ignore") as handle:
                        license_text += "\n" + handle.read()
                except OSError:
                    pass

    normalized = license_text.lower()
    if "cc0" not in normalized and "creative commons zero" not in normalized:
        fail("Kenney Nature Kit license was not confirmed as CC0. Import stopped.")


def import_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    if hasattr(unreal.FBXImportType, "FBXIT_STATIC_MESH"):
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    if options.static_mesh_import_data:
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.generate_lightmap_u_vs = True
        options.static_mesh_import_data.auto_generate_collision = True
    return options


def ensure_material(name, color):
    path = "{}/{}".format(DESTINATION, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        fail("Could not create material {}".format(name))

    color_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        -420,
        0,
    )
    color_node.set_editor_property("parameter_name", "Color")
    color_node.set_editor_property("default_value", color)
    unreal.MaterialEditingLibrary.connect_material_property(
        color_node,
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
    roughness.set_editor_property("default_value", 0.82)
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness,
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def find_source(patterns):
    candidates = []
    for root, _, files in os.walk(SOURCE_ROOT):
        for filename in files:
            lower = filename.lower()
            if not lower.endswith((".fbx", ".obj")):
                continue
            score = 0
            for index, pattern in enumerate(patterns):
                if pattern.lower() in lower:
                    score = max(score, 100 - index)
            if score > 0:
                candidates.append((score, os.path.join(root, filename)))

    if not candidates:
        return None
    candidates.sort(key=lambda item: (-item[0], item[1]))
    return candidates[0][1]


def import_mesh(source_path, destination_name, material):
    task = unreal.AssetImportTask()
    task.filename = source_path
    task.destination_path = DESTINATION
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if source_path.lower().endswith(".fbx"):
        task.options = import_options()

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset("{}/{}".format(DESTINATION, destination_name))
    if not mesh:
        fail("Import did not produce {}".format(destination_name))

    try:
        mesh.set_material(0, material)
    except Exception as exc:
        unreal.log_warning("Could not assign material on {}: {}".format(destination_name, exc))
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    log("Imported {} -> {}".format(source_path, destination_name))


def main():
    ensure_source_root()
    verify_cc0_license()
    unreal.EditorAssetLibrary.make_directory(DESTINATION)

    tree_material = ensure_material("M_BH_FogTree", unreal.LinearColor(0.04, 0.18, 0.10, 1.0))
    rock_material = ensure_material("M_BH_FogRock", unreal.LinearColor(0.16, 0.17, 0.16, 1.0))
    fence_material = ensure_material("M_BH_FogFence", unreal.LinearColor(0.16, 0.13, 0.09, 1.0))

    for destination_name, patterns in ASSETS:
        source = find_source(patterns)
        if not source:
            fail("Could not find a source mesh for {} using patterns {}".format(destination_name, patterns))
        material = fence_material if "Fence" in destination_name else (rock_material if "Rock" in destination_name else tree_material)
        import_mesh(source, destination_name, material)

    unreal.EditorAssetLibrary.save_directory(DESTINATION)
    log("Foggrounds asset import complete.")
    unreal.SystemLibrary.quit_editor()


try:
    main()
except Exception as exc:
    unreal.log_error(str(exc))
    unreal.SystemLibrary.quit_editor()
