import pathlib
import subprocess
import sys

LIMIT = 1000
WARN_AT = 900
ROOT = pathlib.Path(__file__).resolve().parents[2]


def tracked_files():
    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard",
         "--", "*.cpp", "*.h", "*.hpp"], text=True)
    return [line for line in out.splitlines() if line and (ROOT / line).is_file()]


def count_lines(rel):
    with (ROOT / rel).open(encoding="utf-8", errors="replace") as f:
        return sum(1 for _ in f)


def main():
    counts = {rel: count_lines(rel) for rel in tracked_files()}
    problems = [f"{rel}: {n} lines exceeds the {LIMIT}-line limit"
                for rel, n in sorted(counts.items()) if n > LIMIT]
    warnings = [f"WARN {rel}: {n} lines, within {LIMIT - n} of the {LIMIT}-line limit"
                for rel, n in sorted(counts.items()) if WARN_AT <= n <= LIMIT]
    for warning in warnings:
        print(warning)
    for problem in problems:
        print(problem)
    if not problems:
        print(f"file-length gate OK: {len(counts)} files, all within the {LIMIT}-line limit"
              f"{f' ({len(warnings)} in the warning band)' if warnings else ''}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
