import fnmatch
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXEMPT_PATH = pathlib.Path(__file__).resolve().parent / "no_comments_exempt.json"

BANNED_CHARS = {
    chr(0x2014): "em dash (U+2014)",
    chr(0x2013): "en dash (U+2013)",
    chr(0x2018): "left smart quote (U+2018)",
    chr(0x2019): "right smart quote (U+2019)",
    chr(0x201C): "left smart double quote (U+201C)",
    chr(0x201D): "right smart double quote (U+201D)",
    chr(0x00A0): "non-breaking space (U+00A0)",
}

BANNED_SUBSTRINGS = {
    "&" + name: label
    for name, label in {
        "mdash;": "em dash entity",
        "ndash;": "en dash entity",
        "#8212;": "em dash numeric entity",
        "#8211;": "en dash numeric entity",
    }.items()
}


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
        for ch, label in BANNED_CHARS.items():
            if ch in line:
                problems.append(f"{rel}:{lineno}: {label}")
        for sub, label in BANNED_SUBSTRINGS.items():
            if sub in line:
                problems.append(f"{rel}:{lineno}: {label}")
    return problems


def main():
    exempt = json.loads(EXEMPT_PATH.read_text(encoding="utf-8"))
    files = [rel for rel in tracked_files() if not is_exempt(rel, exempt)]
    problems = []
    for rel in files:
        problems.extend(file_problems(rel))
    for problem in problems:
        print(problem)
    if not problems:
        print(f"banned-chars gate OK: {len(files)} files, no banned characters")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
