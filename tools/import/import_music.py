"""Import only the music loops (tools/build/wav/Music*.wav) into the helper project at /Game/Audio/Music.

Run inside Unreal (content-only helper project):
  UnrealEditor-Cmd.exe C:/TaG/tools/assetproj/TaGAssets.uproject -run=pythonscript -script="C:/TaG/tools/import/import_music.py" -unattended -nop4 -nosplash -nullrhi
Then copy tools/assetproj/Content/Audio/Music -> game/Content/Audio/Music (copy_to_game.py copies all of Audio).
The meshes are not touched, so their packages stay byte-identical in git.
"""
import os

import unreal

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WAV_DIR = os.path.join(TOOLS, "build", "wav")
DEST = "/Game/Audio/Music"


def main():
    wavs = sorted(f for f in os.listdir(WAV_DIR) if f.startswith("Music") and f.lower().endswith(".wav"))
    tasks = []
    for f in wavs:
        t = unreal.AssetImportTask()
        t.filename = os.path.join(WAV_DIR, f)
        t.destination_path = DEST
        t.destination_name = os.path.splitext(f)[0]
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for f in wavs:
        name = os.path.splitext(f)[0]
        path = "%s/%s" % (DEST, name)
        sound = unreal.EditorAssetLibrary.load_asset(path)
        if sound is None:
            unreal.log_error("TAGMUSIC missing %s" % path)
            continue
        sound.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False)
        unreal.log("TAGMUSIC imported %s duration=%.1f" % (path, sound.get_editor_property("duration")))


main()
