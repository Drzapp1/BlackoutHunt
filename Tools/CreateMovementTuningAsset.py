import unreal


ASSET_NAME = "DA_BH_MovementTuning"
ASSET_DIR = "/Game/BlackoutHunt/Data"
ASSET_PATH = f"{ASSET_DIR}/{ASSET_NAME}"


def main():
    unreal.EditorAssetLibrary.make_directory(ASSET_DIR)

    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        unreal.log(f"Movement tuning asset already exists: {ASSET_PATH}")
        return

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.BHMovementTuningAsset)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = asset_tools.create_asset(ASSET_NAME, ASSET_DIR, unreal.BHMovementTuningAsset, factory)
    if not asset:
        raise RuntimeError(f"Failed to create {ASSET_PATH}")

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_directory(ASSET_DIR, only_if_is_dirty=False, recursive=True)
    unreal.log(f"Created movement tuning asset: {ASSET_PATH}")


main()
