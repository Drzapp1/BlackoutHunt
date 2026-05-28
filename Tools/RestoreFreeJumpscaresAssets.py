import argparse
import sys

import unreal


SOURCE_ROOT = "/Game/Free_Jumpscares"
DEST_ROOT = "/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares"

REQUIRED_ASSETS = (
    "Meshes/Skeletal_Meshes/SKM_Monster_01",
    "Meshes/Skeletal_Meshes/SKM_Monster_02",
    "Meshes/Skeletal_Meshes/SKM_Monster_03",
    "Materials/M_Monster_01",
    "Materials/M_Monster_02",
    "Materials/M_Monster_03",
    "Sounds/SW_Jumpscare_01",
    "Sounds/SW_Jumpscare_02",
    "Sounds/SW_Jumpscare_03",
    "Animations/AS_Monster_01_Idle",
    "Animations/AS_Monster_02_Walk",
    "Animations/AS_Monster_03_Jumpscare",
)


def asset_path(root, relative_name):
    name = relative_name.rsplit("/", 1)[-1]
    return f"{root}/{relative_name}.{name}"


def validate_root(root):
    missing = [relative for relative in REQUIRED_ASSETS if not unreal.EditorAssetLibrary.does_asset_exist(asset_path(root, relative))]
    return missing


def main():
    parser = argparse.ArgumentParser(description="Restore the local Free_Jumpscares pack into BlackoutHunt's packaged content namespace.")
    parser.add_argument("--replace", action="store_true", help="Delete the destination directory before duplicating from the source.")
    args = parser.parse_args(sys.argv[1:])

    if not unreal.EditorAssetLibrary.does_directory_exist(SOURCE_ROOT):
        unreal.log_error(f"Missing source directory: {SOURCE_ROOT}")
        return 1

    source_missing = validate_root(SOURCE_ROOT)
    if source_missing:
        unreal.log_error(f"{SOURCE_ROOT} is incomplete. Missing: {', '.join(source_missing)}")
        return 1

    if unreal.EditorAssetLibrary.does_directory_exist(DEST_ROOT):
        if args.replace:
            unreal.log(f"Deleting existing destination: {DEST_ROOT}")
            if not unreal.EditorAssetLibrary.delete_directory(DEST_ROOT):
                unreal.log_error(f"Could not delete existing destination: {DEST_ROOT}")
                return 1
        else:
            dest_missing = validate_root(DEST_ROOT)
            if not dest_missing:
                unreal.log(f"Destination already contains required jumpscare assets: {DEST_ROOT}")
                unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
                return 0
            unreal.log_error(f"Destination exists but is incomplete. Re-run with --replace. Missing: {', '.join(dest_missing)}")
            return 1

    unreal.log(f"Duplicating {SOURCE_ROOT} -> {DEST_ROOT}")
    if not unreal.EditorAssetLibrary.duplicate_directory(SOURCE_ROOT, DEST_ROOT):
        unreal.log_error("Unreal asset duplication failed.")
        return 1

    dest_missing = validate_root(DEST_ROOT)
    if dest_missing:
        unreal.log_error(f"Restored destination is incomplete. Missing: {', '.join(dest_missing)}")
        return 1

    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
    unreal.log(f"Restored Free Customizable Jumpscares assets to {DEST_ROOT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
