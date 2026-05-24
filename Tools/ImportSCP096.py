import os

import unreal


DESTINATION_PATH = "/Game/BlackoutHunt/Art/SCP096"
SOURCE_ROOT = r"D:\MainGame\Content\BlackoutHunt\Art\SCP096\SourceFiles"
FBX_PATH = os.path.join(SOURCE_ROOT, "source", "SCP-096.fbx")
TEXTURE_PATH = os.path.join(SOURCE_ROOT, "textures", "096-V_Dif_001.png")


def import_asset(filename, destination_name, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = DESTINATION_PATH
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if task.imported_object_paths:
        unreal.log("Imported: {}".format(", ".join(task.imported_object_paths)))
    else:
        unreal.log_warning("No imported object path returned for {}".format(filename))
    return task.imported_object_paths


def build_static_mesh_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = True
    options.import_textures = True
    options.automated_import_should_detect_type = False

    if options.static_mesh_import_data:
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.generate_lightmap_u_vs = True
        options.static_mesh_import_data.auto_generate_collision = False

    return options


def main():
    if not os.path.exists(FBX_PATH):
        raise RuntimeError("Missing FBX: {}".format(FBX_PATH))
    if not os.path.exists(TEXTURE_PATH):
        raise RuntimeError("Missing texture: {}".format(TEXTURE_PATH))

    texture_paths = import_asset(TEXTURE_PATH, "T_SCP096_Diffuse")
    mesh_paths = import_asset(FBX_PATH, "SM_SCP096", build_static_mesh_options())

    mesh = unreal.load_asset("{}/SM_SCP096".format(DESTINATION_PATH))
    if mesh:
        texture = unreal.load_asset("{}/T_SCP096_Diffuse".format(DESTINATION_PATH))
        material = None
        if texture:
            material_path = "{}/M_SCP096".format(DESTINATION_PATH)
            material = unreal.load_asset(material_path)
            if material:
                unreal.EditorAssetLibrary.delete_asset(material_path)

            material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "M_SCP096",
                DESTINATION_PATH,
                unreal.Material,
                unreal.MaterialFactoryNew(),
            )
            if material:
                texture_sample = unreal.MaterialEditingLibrary.create_material_expression(
                    material,
                    unreal.MaterialExpressionTextureSample,
                    -420,
                    0,
                )
                texture_sample.texture = texture
                unreal.MaterialEditingLibrary.connect_material_property(
                    texture_sample,
                    "RGB",
                    unreal.MaterialProperty.MP_BASE_COLOR,
                )
                unreal.MaterialEditingLibrary.recompile_material(material)
                unreal.EditorAssetLibrary.save_loaded_asset(material)

        if material:
            mesh.set_material(0, material)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    unreal.EditorAssetLibrary.save_directory(DESTINATION_PATH)
    unreal.log("SCP-096 import complete. Texture paths: {} Mesh paths: {}".format(texture_paths, mesh_paths))
    unreal.SystemLibrary.quit_editor()


main()
