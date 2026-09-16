import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
EDITOR_PREFIX = "editor/"
SCANNED_PREFIXES = ("src/", "tests/")
SOURCE_SUFFIXES = (".cpp", ".h", ".hpp", ".inl")
PATTERNS = [
    re.compile(rb"#\s*include\s*[<\"]Q[A-Za-z]"),
    re.compile(rb"#\s*include\s*[<\"][^<\">]*(QtCore|QtGui|QtWidgets)/"),
    re.compile(rb"\bQ_OBJECT\b"),
    re.compile(rb"\bQt::"),
    re.compile(rb"\bQ(String|Widget|Object|Application|Image|Painter|Settings)\b"),
]


def tracked_sources():
    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard"],
        text=True)
    return [
        line for line in out.splitlines()
        if line.startswith(SCANNED_PREFIXES) and line.endswith(SOURCE_SUFFIXES)
    ]


def main():
    problems = []
    checked = 0
    for rel in tracked_sources():
        path = ROOT / rel
        if not path.is_file():
            continue
        checked += 1
        for lineno, line in enumerate(path.read_bytes().splitlines(), start=1):
            if any(pattern.search(line) for pattern in PATTERNS):
                problems.append(
                    f"{rel}:{lineno}: Qt outside {EDITOR_PREFIX} - the renderer and the "
                    f"editor's document model stay toolkit-free")
    for problem in problems:
        print(problem)
    if not problems:
        print(f"qt-isolation gate OK: {checked} shared sources checked")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
