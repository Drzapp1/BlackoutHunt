import argparse
import sys

import unreal


SOURCE_ROOT = "/Game/Whisper"
DEST_ROOT = "/Game/BlackoutHunt/Art/Jumpscares/Whisper"


def normalize_name(raw_name):
    result = raw_name.lower()
    prefixes = (
        "skm_",
        "sk_",
        "sm_",
        "mi_",
        "m_",
        "mat_",
        "as_",
        "a_",
        "anim_",
        "sw_",
        "wav_",
        "cue_",
        "s_",
    )
    removed = True
    while removed:
        removed = False
        for prefix in prefixes:
            if result.startswith(prefix):
                result = result[len(prefix):]
                removed = True
                break
    return result.replace("_", "").replace("-", "").replace(" ", "").replace("whisper", "")


def digits_in_name(name):
    return "".join(character for character in name if character.isdigit())


def common_prefix_length(left, right):
    count = 0
    for left_char, right_char in zip(left, right):
        if left_char != right_char:
            break
        count += 1
    return count


def pair_score(mesh_path, candidate_path):
    mesh_folder = mesh_path.rsplit("/", 1)[0]
    candidate_folder = candidate_path.rsplit("/", 1)[0]
    mesh_name = mesh_path.rsplit("/", 1)[-1]
    candidate_name = candidate_path.rsplit("/", 1)[-1]
    mesh_normalized = normalize_name(mesh_name)
    candidate_normalized = normalize_name(candidate_name)
    mesh_digits = digits_in_name(mesh_name)
    candidate_digits = digits_in_name(candidate_name)

    score = 0
    if candidate_folder == mesh_folder:
        score += 10000
    elif candidate_folder.startswith(mesh_folder) or mesh_folder.startswith(candidate_folder):
        score += 3000
    if mesh_normalized and mesh_normalized == candidate_normalized:
        score += 8000
    elif mesh_normalized and candidate_normalized and (mesh_normalized in candidate_normalized or candidate_normalized in mesh_normalized):
        score += 4000
    if mesh_digits and mesh_digits == candidate_digits:
        score += 2500
    score += common_prefix_length(mesh_normalized, candidate_normalized) * 25
    return score


def best_match(mesh_path, candidates):
    if not candidates:
        return "none"
    return sorted(candidates, key=lambda candidate: (-pair_score(mesh_path, candidate), candidate))[0]


def list_loaded_assets(root):
    asset_paths = sorted(unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False))
    assets = []
    for asset_path in asset_paths:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset:
            assets.append((asset_path, asset))
    return assets


def classify_assets(root):
    report = {
        "meshes": [],
        "materials": [],
        "animations": [],
        "sounds": [],
    }

    for asset_path, asset in list_loaded_assets(root):
        if isinstance(asset, (unreal.SkeletalMesh, unreal.StaticMesh)):
            report["meshes"].append(asset_path)
        elif isinstance(asset, unreal.MaterialInterface):
            report["materials"].append(asset_path)
        elif isinstance(asset, unreal.AnimSequence):
            report["animations"].append(asset_path)
        elif isinstance(asset, unreal.SoundBase):
            report["sounds"].append(asset_path)

    return report


def log_report(root, report):
    unreal.log(f"Whisper jumpscare discovery report for {root}")
    for label in ("meshes", "materials", "animations", "sounds"):
        values = report[label]
        unreal.log(f"{label}: {len(values)}")
        for value in values:
            unreal.log(f"  {value}")
    if report["meshes"]:
        unreal.log("pairing preview:")
        for index, mesh_path in enumerate(report["meshes"], start=1):
            material_path = best_match(mesh_path, report["materials"])
            animation_path = best_match(mesh_path, report["animations"])
            sound_path = best_match(mesh_path, report["sounds"])
            unreal.log(f"  Whisper{index:02d}: mesh={mesh_path} material={material_path} animation={animation_path} sound={sound_path}")


def main():
    parser = argparse.ArgumentParser(description="Restore the Epic/Fab Whisper pack into BlackoutHunt's jumpscare content namespace.")
    parser.add_argument("--replace", action="store_true", help="Delete the destination directory before duplicating from the source.")
    args = parser.parse_args(sys.argv[1:])

    if not unreal.EditorAssetLibrary.does_directory_exist(SOURCE_ROOT):
        unreal.log_error(f"Missing source directory: {SOURCE_ROOT}. Import the Epic/Fab Whisper pack there first.")
        return 1

    source_report = classify_assets(SOURCE_ROOT)
    if not source_report["meshes"]:
        unreal.log_error(f"{SOURCE_ROOT} has no skeletal or static mesh assets for Whisper jumpscares.")
        log_report(SOURCE_ROOT, source_report)
        return 1

    if unreal.EditorAssetLibrary.does_directory_exist(DEST_ROOT):
        if args.replace:
            unreal.log(f"Deleting existing destination: {DEST_ROOT}")
            if not unreal.EditorAssetLibrary.delete_directory(DEST_ROOT):
                unreal.log_error(f"Could not delete existing destination: {DEST_ROOT}")
                return 1
        else:
            dest_report = classify_assets(DEST_ROOT)
            if dest_report["meshes"]:
                unreal.log(f"{DEST_ROOT} already contains usable Whisper assets. Use --replace to refresh from {SOURCE_ROOT}.")
                unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
                log_report(DEST_ROOT, dest_report)
                return 0
            unreal.log_error(f"{DEST_ROOT} exists but has no skeletal or static meshes. Re-run with --replace.")
            log_report(DEST_ROOT, dest_report)
            return 1

    unreal.log(f"Duplicating {SOURCE_ROOT} -> {DEST_ROOT}")
    if not unreal.EditorAssetLibrary.duplicate_directory(SOURCE_ROOT, DEST_ROOT):
        unreal.log_error("Unreal asset duplication failed.")
        return 1

    dest_report = classify_assets(DEST_ROOT)
    if not dest_report["meshes"]:
        unreal.log_error(f"{DEST_ROOT} has no skeletal or static mesh assets after restore.")
        log_report(DEST_ROOT, dest_report)
        return 1

    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
    log_report(DEST_ROOT, dest_report)
    unreal.log(f"Restored Whisper jumpscare assets to {DEST_ROOT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
