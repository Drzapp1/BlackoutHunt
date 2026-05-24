import os
import sys

import unreal


PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_ROOT = os.path.join(PROJECT_DIR, "ThirdParty", "ambientCG")
TEXTURE_DEST = "/Game/BlackoutHunt/Art/Textures"
MATERIAL_DEST = "/Game/BlackoutHunt/Art/Materials"


ASSETS = {
    "Concrete048": {
        "material": "M_BH_Concrete",
        "asset_prefix": "Concrete048_1K-JPG",
        "tiling": 24.0,
        "roughness": 0.88,
        "metallic": 0.0,
    },
    "Plaster001": {
        "material": "M_BH_Plaster",
        "asset_prefix": "Plaster001_1K-JPG",
        "tiling": 10.0,
        "roughness": 0.82,
        "metallic": 0.0,
    },
    "Metal063": {
        "material": "M_BH_RustedMetal",
        "asset_prefix": "Metal063_1K-JPG",
        "tiling": 7.0,
        "roughness": 0.76,
        "metallic": 0.65,
    },
    "DiamondPlate009": {
        "material": "M_BH_DiamondPlate",
        "asset_prefix": "DiamondPlate009_1K-JPG",
        "tiling": 18.0,
        "roughness": 0.58,
        "metallic": 0.8,
    },
    "PaintedMetal004": {
        "material": "M_BH_PaintedMetal",
        "asset_prefix": "PaintedMetal004_1K-JPG",
        "tiling": 8.0,
        "roughness": 0.72,
        "metallic": 0.55,
    },
    "Tiles078": {
        "material": "M_BH_Tiles",
        "asset_prefix": "Tiles078_1K-JPG",
        "tiling": 16.0,
        "roughness": 0.78,
        "metallic": 0.0,
    },
    "Sign009": {
        "material": "M_BH_WarningSign",
        "asset_prefix": "Sign009_1K-JPG",
        "tiling": 1.0,
        "roughness": 0.65,
        "metallic": 0.0,
    },
}


MAP_SUFFIXES = {
    "color": "Color",
    "normal": "NormalGL",
    "roughness": "Roughness",
    "metallic": "Metalness",
    "ao": "AmbientOcclusion",
}


def log(message):
    unreal.log("[BlackoutHunt Art Import] " + message)


def fail(message):
    unreal.log_error("[BlackoutHunt Art Import] " + message)
    raise RuntimeError(message)


def asset_path(package_path, name):
    return f"{package_path}/{name}.{name}"


def ensure_directories():
    unreal.EditorAssetLibrary.make_directory(TEXTURE_DEST)
    unreal.EditorAssetLibrary.make_directory(MATERIAL_DEST)


def source_file(asset_id, asset_prefix, suffix):
    path = os.path.join(SOURCE_ROOT, asset_id, f"{asset_prefix}_{suffix}.jpg")
    return path if os.path.exists(path) else None


def import_texture(asset_id, asset_prefix, map_key):
    suffix = MAP_SUFFIXES[map_key]
    path = source_file(asset_id, asset_prefix, suffix)
    if not path:
        return None

    name = f"T_BH_{asset_id}_{map_key}"
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = TEXTURE_DEST
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(asset_path(TEXTURE_DEST, name))
    if not texture:
        fail(f"Could not import texture {path}")

    if map_key == "normal":
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif map_key in ("roughness", "metallic", "ao"):
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)

    unreal.EditorAssetLibrary.save_asset(asset_path(TEXTURE_DEST, name), only_if_is_dirty=False)
    return texture


def delete_existing_material(name):
    path = asset_path(MATERIAL_DEST, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not unreal.EditorAssetLibrary.delete_asset(path):
            fail(f"Could not delete existing material {path}")


def create_texture_sample(material, texture, x, y, texcoord):
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSample, x, y
    )
    sample.set_editor_property("texture", texture)
    if texcoord:
        unreal.MaterialEditingLibrary.connect_material_expressions(texcoord, "", sample, "UVs")
    return sample


def create_constant(material, value, x, y):
    const = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    const.set_editor_property("r", value)
    return const


def create_material(name, textures, tiling, roughness_default, metallic_default):
    delete_existing_material(name)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        MATERIAL_DEST,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not material:
        fail(f"Could not create material {name}")

    material.set_editor_property("use_material_attributes", False)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -640, 40
    )
    texcoord.set_editor_property("coordinate_index", 0)
    texcoord.set_editor_property("u_tiling", tiling)
    texcoord.set_editor_property("v_tiling", tiling)

    if textures.get("color"):
        color = create_texture_sample(material, textures["color"], -420, -220, texcoord)
        unreal.MaterialEditingLibrary.connect_material_property(
            color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
        )

    if textures.get("normal"):
        normal = create_texture_sample(material, textures["normal"], -420, 70, texcoord)
        unreal.MaterialEditingLibrary.connect_material_property(
            normal, "RGB", unreal.MaterialProperty.MP_NORMAL
        )

    if textures.get("roughness"):
        roughness = create_texture_sample(material, textures["roughness"], -420, 330, texcoord)
        unreal.MaterialEditingLibrary.connect_material_property(
            roughness, "R", unreal.MaterialProperty.MP_ROUGHNESS
        )
    else:
        roughness = create_constant(material, roughness_default, -420, 330)
        unreal.MaterialEditingLibrary.connect_material_property(
            roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
        )

    if textures.get("metallic"):
        metallic = create_texture_sample(material, textures["metallic"], -420, 520, texcoord)
        unreal.MaterialEditingLibrary.connect_material_property(
            metallic, "R", unreal.MaterialProperty.MP_METALLIC
        )
    else:
        metallic = create_constant(material, metallic_default, -420, 520)
        unreal.MaterialEditingLibrary.connect_material_property(
            metallic, "", unreal.MaterialProperty.MP_METALLIC
        )

    if textures.get("ao"):
        ao = create_texture_sample(material, textures["ao"], -420, 710, texcoord)
        unreal.MaterialEditingLibrary.connect_material_property(
            ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION
        )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(asset_path(MATERIAL_DEST, name), only_if_is_dirty=False)
    return material


def main():
    if not os.path.isdir(SOURCE_ROOT):
        fail(f"Missing source asset directory: {SOURCE_ROOT}")

    ensure_directories()
    created = []

    for asset_id, spec in ASSETS.items():
        log(f"Importing {asset_id}")
        textures = {}
        for map_key in MAP_SUFFIXES:
            textures[map_key] = import_texture(asset_id, spec["asset_prefix"], map_key)

        if not textures.get("color"):
            fail(f"{asset_id} has no Color map")

        create_material(
            spec["material"],
            textures,
            spec["tiling"],
            spec["roughness"],
            spec["metallic"],
        )
        created.append(spec["material"])

    unreal.EditorAssetLibrary.save_directory("/Game/BlackoutHunt", only_if_is_dirty=False, recursive=True)
    log("Created materials: " + ", ".join(created))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        unreal.log_error(str(exc))
        sys.exit(1)
