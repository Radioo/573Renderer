import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUI_PREFIXES = ("src/gui/",)
SOURCE_SUFFIXES = (".cpp", ".h")
PATTERNS = [
    re.compile(rb"\bImGui::"),
    re.compile(rb"\bImGuiIO\b"),
    re.compile(rb"#\s*include\s*[<\"]imgui"),
]


def tracked_sources():
    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard"],
        text=True)
    return [
        line for line in out.splitlines()
        if line.startswith("src/") and line.endswith(SOURCE_SUFFIXES)
    ]


def main():
    problems = []
    checked = 0
    for rel in tracked_sources():
        if rel.startswith(GUI_PREFIXES):
            continue
        path = ROOT / rel
        if not path.is_file():
            continue
        checked += 1
        for lineno, line in enumerate(path.read_bytes().splitlines(), start=1):
            for pattern in PATTERNS:
                if pattern.search(line):
                    problems.append(
                        f"{rel}:{lineno}: ImGui usage outside src/gui/ "
                        f"(matches {pattern.pattern.decode()})")
    for problem in problems:
        print(problem)
    if not problems:
        print(f"gui-isolation gate OK: {checked} non-gui sources checked")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
