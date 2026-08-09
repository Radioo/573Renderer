import fnmatch
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXEMPT_PATH = pathlib.Path(__file__).resolve().parent / "no_comments_exempt.json"

MACHINE_PATH_RE = re.compile(r"(?<![A-Za-z0-9_])[A-Za-z]:[\\/][A-Za-z0-9_]")

FIXTURE_EXEMPT = ["tests/*"]


def tracked_files():
    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard"],
        text=True)
    return [line for line in out.splitlines() if line and (ROOT / line).is_file()]


def is_exempt(rel, exempt):
    return any(
        fnmatch.fnmatch(rel, pattern) if "*" in pattern else rel == pattern for pattern in exempt)


def file_problems(rel):
    data = (ROOT / rel).read_bytes()
    if b"\0" in data:
        return []
    text = data.decode("utf-8", errors="replace")
    problems = []
    for lineno, line in enumerate(text.splitlines(), start=1):
        if MACHINE_PATH_RE.search(line):
            problems.append(f"{rel}:{lineno}: absolute drive-letter path: {line.strip()[:120]}")
    return problems


def main():
    exempt = json.loads(EXEMPT_PATH.read_text(encoding="utf-8")) + FIXTURE_EXEMPT
    files = [rel for rel in tracked_files() if not is_exempt(rel, exempt)]
    problems = []
    for rel in files:
        problems.extend(file_problems(rel))
    for problem in problems:
        print(problem)
    if not problems:
        print(f"machine-path gate OK: {len(files)} files, no absolute drive-letter paths")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
