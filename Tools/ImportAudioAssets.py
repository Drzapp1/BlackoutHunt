import os

import unreal


PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SOURCE_ROOT = os.path.join(PROJECT_DIR, "Content", "BlackoutHunt", "Audio", "Sources")
DESTINATION = "/Game/BlackoutHunt/Audio"

ASSETS = [
    {
        "source": "BH_EerieLobbyLoop.wav",
        "name": "SW_EerieLobbyLoop",
        "looping": True,
        "group": "SOUNDGROUP_MUSIC",
    },
    {
        "source": "BH_MenuClick.wav",
        "name": "SW_MenuClick",
        "looping": False,
        "group": "SOUNDGROUP_UI",
    },
    {
        "source": "BH_TerrifiedScream_Faint.wav",
        "name": "SW_TerrifiedScreamFaint",
        "looping": False,
        "group": "SOUNDGROUP_Voice",
    },
]


def log(message):
    unreal.log("[BlackoutHunt Audio Import] " + message)


def fail(message):
    unreal.log_error("[BlackoutHunt Audio Import] " + message)
    raise RuntimeError(message)


def asset_path(package_path, name):
    return "{}/{}.{}".format(package_path, name, name)


def set_sound_group(sound_wave, group_name):
    if not hasattr(unreal, "SoundGroup"):
        return

    group = getattr(unreal.SoundGroup, group_name, None)
    if group is not None:
        sound_wave.set_editor_property("sound_group", group)


def import_sound(spec):
    source_path = os.path.join(SOURCE_ROOT, spec["source"])
    if not os.path.exists(source_path):
        fail("Missing audio source {}".format(source_path))

    task = unreal.AssetImportTask()
    task.filename = source_path
    task.destination_path = DESTINATION
    task.destination_name = spec["name"]
    task.automated = True
    task.save = True
    task.replace_existing = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    sound_wave = unreal.EditorAssetLibrary.load_asset(asset_path(DESTINATION, spec["name"]))
    if not sound_wave:
        fail("Could not import {}".format(source_path))

    sound_wave.set_editor_property("looping", spec["looping"])
    set_sound_group(sound_wave, spec["group"])

    if hasattr(sound_wave, "set_editor_property"):
        try:
            sound_wave.set_editor_property("compression_quality", 70)
        except Exception:
            pass

    unreal.EditorAssetLibrary.save_loaded_asset(sound_wave)
    log("Imported {} as {}".format(source_path, asset_path(DESTINATION, spec["name"])))


def main():
    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    for spec in ASSETS:
        import_sound(spec)

    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    log("Audio import complete.")


if __name__ == "__main__":
    main()
