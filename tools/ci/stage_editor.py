import argparse
import pathlib
import shutil
import sys

PLUGIN_DIRS = ["platforms", "imageformats", "iconengines"]
REQUIRED_PLUGINS = [
    "platforms/qwindows.dll",
    "imageformats/qjpeg.dll",
    "imageformats/qsvg.dll",
    "iconengines/qsvgicon.dll",
    "platforms/qminimal.dll",
]
EDITOR_FILES = ["ifs_editor.exe"]
EDITOR_SYMBOLS = ["ifs_editor.pdb"]
HOST_FILES = ["preview_host.exe"]
HOST_SYMBOLS = ["preview_host.pdb"]
NEEDED = ["ifs_editor.exe", "preview_host.exe"]


def copy_file(source, into, missing):
    if not source.is_file():
        missing.append(str(source))
        return
    shutil.copy2(source, into / source.name)


def copy_plugins(editor, into, missing):
    for name in PLUGIN_DIRS:
        source = editor / name
        if not source.is_dir():
            missing.append(str(source))
            continue
        shutil.copytree(source, into / name, dirs_exist_ok=True)


def stage(editor, host, into, symbols):
    if into.exists():
        shutil.rmtree(into)
    into.mkdir(parents=True)
    missing = []
    for name in EDITOR_FILES + (EDITOR_SYMBOLS if symbols else []):
        copy_file(editor / name, into, missing)
    for name in HOST_FILES + (HOST_SYMBOLS if symbols else []):
        copy_file(host / name, into, missing)
    for dll in sorted(editor.glob("*.dll")):
        shutil.copy2(dll, into / dll.name)
    copy_plugins(editor, into, missing)
    return missing


def unmet(into):
    wanted = NEEDED + REQUIRED_PLUGINS
    return [name for name in wanted if not (into / name).is_file()]


def main():
    parser = argparse.ArgumentParser(description="Stage a runnable IFS editor folder")
    parser.add_argument("--editor", default="build-editor", type=pathlib.Path)
    parser.add_argument("--host", default="build", type=pathlib.Path)
    parser.add_argument("--into", default="dist", type=pathlib.Path)
    parser.add_argument("--symbols", action="store_true")
    args = parser.parse_args()

    missing = stage(args.editor, args.host, args.into, args.symbols)
    short = unmet(args.into)
    if short:
        print("staged editor is missing: " + ", ".join(short), file=sys.stderr)
        if missing:
            print("nothing was copied from: " + ", ".join(missing), file=sys.stderr)
        return 1
    files = sorted(p for p in args.into.rglob("*") if p.is_file())
    size = sum(p.stat().st_size for p in files)
    print(f"staged {len(files)} files, {size / 1e6:.1f} MB, into {args.into}")
    for name in NEEDED:
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
