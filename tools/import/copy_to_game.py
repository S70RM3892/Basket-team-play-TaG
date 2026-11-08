"""
copy_to_game.py - copy the imported packages from the helper project into the game project.

  python C:/TaG/tools/import/copy_to_game.py

tools/assetproj/Content/Art   -> game/Content/Art     (same relative paths, package paths stay /Game/Art/...)
tools/assetproj/Content/Audio -> game/Content/Audio

Only these two folders are written.  Files are overwritten when their content differs and verified by SHA-256
afterwards.  Files that exist in the game folders but not in the helper project are reported, never deleted.
Close the Unreal Editor on the game project first (it may hold the packages open).
"""
import hashlib
import os
import shutil
import sys

TOOLS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(TOOLS, "assetproj", "Content")
DST = os.path.normpath(os.path.join(TOOLS, "..", "game", "Content"))
FOLDERS = ("Art", "Audio")


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    if not os.path.isdir(DST):
        sys.exit("game Content folder not found: %s" % DST)
    copied = same = 0
    src_files = set()
    for folder in FOLDERS:
        root = os.path.join(SRC, folder)
        if not os.path.isdir(root):
            sys.exit("missing %s - run tools/import/import_all.py first" % root)
        for dp, _, files in os.walk(root):
            for f in files:
                s = os.path.join(dp, f)
                rel = os.path.relpath(s, SRC)
                src_files.add(rel.lower())
                d = os.path.join(DST, rel)
                os.makedirs(os.path.dirname(d), exist_ok=True)
                if os.path.exists(d) and os.path.getsize(d) == os.path.getsize(s) and sha(d) == sha(s):
                    same += 1
                    continue
                shutil.copy2(s, d)
                if sha(d) != sha(s):
                    sys.exit("copy verification failed for %s" % rel)
                copied += 1
    extra = []
    for folder in FOLDERS:
        for dp, _, files in os.walk(os.path.join(DST, folder)):
            for f in files:
                rel = os.path.relpath(os.path.join(dp, f), DST)
                if rel.lower() not in src_files:
                    extra.append(rel)
    print("copied %d files, %d already identical -> %s" % (copied, same, DST))
    if extra:
        print("files in the game folders that the pipeline did not produce (left untouched):")
        for e in sorted(extra):
            print("  " + e)


if __name__ == "__main__":
    main()
