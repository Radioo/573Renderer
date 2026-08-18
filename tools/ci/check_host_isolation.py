import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCOPES = ("src/gui/timeline/", "src/gui/gui_preset_library")
SOURCE_SUFFIXES = (".cpp", ".h", ".hpp", ".inl")
HOSTS = ("PresetHost", "Scene3dHost", "Gc2dHost")
PATTERNS = [(host, re.compile((r"\b" + host + r"::").encode())) for host in HOSTS]


def in_scope(rel):
    return rel.startswith(SCOPES) and rel.endswith(SOURCE_SUFFIXES)


def tracked_sources(root):
    out = subprocess.check_output(
        ["git", "-C", str(root), "ls-files", "--cached", "--others", "--exclude-standard"],
        text=True)
    return [line for line in out.splitlines() if in_scope(line)]


def check(root):
    problems = []
    checked = 0
    for rel in tracked_sources(root):
        path = root / rel
        if not path.is_file():
            continue
        checked += 1
        for lineno, line in enumerate(path.read_bytes().splitlines(), start=1):
            for host, pattern in PATTERNS:
                if pattern.search(line):
                    problems.append(
                        f"{rel}:{lineno}: {host}:: called from the preset editor - the editor "
                        f"talks to the render thread through PresetCmd only")
    return problems, checked


def main():
    problems, checked = check(ROOT)
    for problem in problems:
        print(problem)
    if not problems:
        print(f"host-isolation gate OK: {checked} editor sources checked")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
