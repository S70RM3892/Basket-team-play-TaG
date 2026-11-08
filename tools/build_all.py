"""
build_all.py - regenerate every art/audio asset of Basket Team-play TaG in one go.

  C:/Users/monns/AppData/Local/Programs/Python/Python312/python.exe C:/TaG/tools/build_all.py
      [--skip-previews] [--skip-copy] [--no-audio]

Steps (each can also be run by hand, see tools/README.md):
  1. audio/synth_sfx.py                 -> build/wav/*.wav + sfx.json
  2. blender/build_assets.py (Blender)  -> build/fbx/**.fbx + assets.json
  3. blender/render_previews.py -- portraits -> build/portraits/T_Portrait_<Key>.png (imported in step 4)
  4. import/import_all.py (UE, helper project tools/assetproj) -> /Game/Art, /Game/Audio packages
  5. import/verify.py (UE)              -> art/verify_report.txt + art/manifest.json
  6. import/copy_to_game.py             -> game/Content/Art, game/Content/Audio
  7. blender/render_previews.py         -> art/previews/*.png
--no-audio leaves the sounds alone: no synthesis, no sound import, and only game/Content/Art is copied.
Override tool locations with the environment variables TAG_BLENDER and TAG_UE_CMD.
Never opens or builds game/BasketTaG.uproject.
"""
import os
import subprocess
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
BLENDER = os.environ.get("TAG_BLENDER", r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe")
UE_CMD = os.environ.get("TAG_UE_CMD", r"C:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")
UPROJECT = os.path.join(TOOLS, "assetproj", "TaGAssets.uproject")
PY = sys.executable


def run(title, cmd, log=None):
    print("==> %s" % title, flush=True)
    t0 = time.time()
    if log:
        with open(log, "w", encoding="utf-8", errors="replace") as f:
            r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT)
    else:
        r = subprocess.run(cmd)
    print("    %s in %.1fs%s" % ("ok" if r.returncode == 0 else "FAILED (%d)" % r.returncode, time.time() - t0,
                                 ("  (log: %s)" % log) if log else ""), flush=True)
    if r.returncode != 0:
        sys.exit(r.returncode)


def ue_script(path, *args):
    # the commandlet passes everything after the script path to Python's sys.argv
    return [UE_CMD, UPROJECT, "-run=pythonscript", "-script=%s" % " ".join((path,) + args), "-unattended", "-nop4",
            "-nosplash", "-nullrhi", "-stdout", "-FullStdOutLogOutput"]


def main():
    build = os.path.join(TOOLS, "build")
    os.makedirs(build, exist_ok=True)
    audio = "--no-audio" not in sys.argv
    if audio:
        run("synthesize SFX", [PY, os.path.join(TOOLS, "audio", "synth_sfx.py")])
    run("model + export FBX (Blender)", [BLENDER, "-b", "--factory-startup", "-P",
                                          os.path.join(TOOLS, "blender", "build_assets.py")],
        os.path.join(build, "blender_export_log.txt"))
    run("hero portraits (Blender)", [BLENDER, "-b", "--factory-startup", "-P",
                                     os.path.join(TOOLS, "blender", "render_previews.py"), "--", "portraits"],
        os.path.join(build, "blender_portraits_log.txt"))
    run("import into the helper project (UE)",
        ue_script(os.path.join(TOOLS, "import", "import_all.py"), *(() if audio else ("-nosounds",))),
        os.path.join(build, "import_stdout.txt"))
    run("verify inside UE", ue_script(os.path.join(TOOLS, "import", "verify.py")),
        os.path.join(build, "verify_stdout.txt"))
    with open(os.path.join(TOOLS, "art", "verify_report.txt"), encoding="utf-8") as f:
        head = [next(f) for _ in range(4)]
    print("    " + head[3].strip())
    if "--skip-copy" not in sys.argv:
        run("copy packages into game/Content", [PY, os.path.join(TOOLS, "import", "copy_to_game.py")] +
            ([] if audio else ["--only", "Art"]))
    if "--skip-previews" not in sys.argv:
        run("preview renders (Blender)", [BLENDER, "-b", "--factory-startup", "-P",
                                         os.path.join(TOOLS, "blender", "render_previews.py"), "--",
                                         "heroes,turnarounds,ball,court"],
            os.path.join(build, "blender_render_log.txt"))


if __name__ == "__main__":
    main()
