import argparse
import pathlib
import sys
import zipfile

import stage_editor

RENDERER_ZIP = "573Renderer.zip"
EDITOR_ZIP = "573Editor.zip"
RENDERER_FROM_WIN64 = ["573Renderer.exe", "573Encoder.exe"]
RENDERER_FROM_WIN32 = ["573Renderer32.exe"]


def renderer_files(win64, win32):
    return [win64 / name for name in RENDERER_FROM_WIN64] + [
        win32 / name for name in RENDERER_FROM_WIN32
    ]


def editor_files(editor):
    return [editor / name for name in stage_editor.NEEDED]


def write_zip(target, entries):
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        for source, name in entries:
            archive.write(source, name)


def package(win64, win32, editor, into):
    short = [str(path) for path in renderer_files(win64, win32) if not path.is_file()]
    short += [str(editor / name) for name in stage_editor.unmet(editor)]
    if short:
        return short
    into.mkdir(parents=True, exist_ok=True)
    write_zip(into / RENDERER_ZIP, [(path, path.name) for path in renderer_files(win64, win32)])
    write_zip(into / EDITOR_ZIP, [(path, path.name) for path in editor_files(editor)])
    return []


def main():
    parser = argparse.ArgumentParser(description="Pack the renderer and editor release zips")
    parser.add_argument("--win64", default="bin", type=pathlib.Path)
    parser.add_argument("--win32", default="bin", type=pathlib.Path)
    parser.add_argument("--editor", default="dist", type=pathlib.Path)
    parser.add_argument("--into", default="release", type=pathlib.Path)
    args = parser.parse_args()

    short = package(args.win64, args.win32, args.editor, args.into)
    if short:
        print("release is missing: " + ", ".join(short), file=sys.stderr)
        return 1
    for name in (RENDERER_ZIP, EDITOR_ZIP):
        target = args.into / name
        with zipfile.ZipFile(target) as archive:
            count = len(archive.namelist())
        print(f"{target}: {count} files, {target.stat().st_size / 1e6:.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
