import fnmatch
import json
import pathlib
import re
import subprocess
import sys
import tokenize

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXEMPT_PATH = pathlib.Path(__file__).resolve().parent / "no_comments_exempt.json"

CPP_TOKEN_RE = re.compile(r'R"(?P<d>[^()\\\s]{0,16})\((?s:.)*?\)(?P=d)"'
                          r'|"(?:\\.|[^"\\\n])*"'
                          r"|'(?:\\.|[^'\\\n])*'"
                          r'|//[^\n]*'
                          r'|(?s:/\*.*?\*/)')


def tracked_files():
    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard"],
        text=True)
    return [line for line in out.splitlines() if line and (ROOT / line).is_file()]


def is_exempt(rel, exempt):
    return any(
        fnmatch.fnmatch(rel, pattern) if "*" in pattern else rel == pattern for pattern in exempt)


def cpp_comments(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    last_pos = 0
    last_line = 1
    for m in CPP_TOKEN_RE.finditer(text):
        tok = m.group(0)
        if tok.startswith("//") or tok.startswith("/*"):
            last_line += text.count("\n", last_pos, m.start())
            last_pos = m.start()
            yield last_line, tok.splitlines()[0]


def python_comments(path):
    with path.open("rb") as f:
        for tok in tokenize.tokenize(f.readline):
            if tok.type == tokenize.COMMENT:
                yield tok.start[0], tok.string


def text_lines(path):
    return enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1)


def hash_comments(path):
    for lineno, line in text_lines(path):
        quote = None
        for i, ch in enumerate(line):
            if quote:
                if ch == quote and (i == 0 or line[i - 1] != "\\"):
                    quote = None
            elif ch in "'\"":
                quote = ch
            elif ch == "#":
                yield lineno, line[i:]
                break


def batch_comments(path):
    for lineno, line in text_lines(path):
        stripped = line.strip()
        low = stripped.lower()
        if low.startswith("rem ") or low == "rem" or stripped.startswith("::"):
            yield lineno, stripped


def slash_comments(path):
    for lineno, line in text_lines(path):
        stripped = line.strip()
        if stripped.startswith(("//", "/*")):
            yield lineno, stripped


CHECKERS = {
    ".cpp": cpp_comments,
    ".h": cpp_comments,
    ".hpp": cpp_comments,
    ".py": python_comments,
    ".yml": hash_comments,
    ".yaml": hash_comments,
    ".cmake": hash_comments,
    ".bat": batch_comments,
    ".json": slash_comments,
}
BASENAME_CHECKERS = {
    "CMakeLists.txt": hash_comments,
    ".clang-format": hash_comments,
    ".clang-tidy": hash_comments,
    ".gitignore": hash_comments,
    ".gitattributes": hash_comments,
}


def checker_for(rel):
    name = pathlib.PurePosixPath(rel).name
    if name in BASENAME_CHECKERS:
        return BASENAME_CHECKERS[name]
    return CHECKERS.get(pathlib.PurePosixPath(rel).suffix.lower())


def main():
    exempt = json.loads(EXEMPT_PATH.read_text(encoding="utf-8"))
    problems = []
    checked = 0
    for rel in tracked_files():
        if is_exempt(rel, exempt):
            continue
        checker = checker_for(rel)
        if checker is None:
            continue
        checked += 1
        for lineno, text in checker(ROOT / rel):
            problems.append(f"{rel}:{lineno}: comment forbidden: {text[:80]}")
    for problem in problems:
        print(problem)
    if not problems:
        print(f"no-comments gate OK: {checked} files checked")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
