import importlib.util
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PINNED_VERSION = "19.1.7"


def clang_format_exe():
    spec = importlib.util.find_spec("clang_format")
    if spec is not None and spec.origin is not None:
        base = pathlib.Path(spec.origin).parent / "data" / "bin"
        for name in ("clang-format.exe", "clang-format"):
            candidate = base / name
            if candidate.exists():
                return str(candidate)
    return "clang-format"


FORMAT = clang_format_exe()


def tracked_sources():
    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard",
         "--", "*.cpp", "*.h", "*.hpp"], text=True)
    return [line for line in out.splitlines() if line and (ROOT / line).is_file()]


def assert_pinned_version():
    out = subprocess.check_output([FORMAT, "--version"], text=True)
    if PINNED_VERSION not in out:
        print(f"clang-format version mismatch: need {PINNED_VERSION}, using {FORMAT}")
        print(f"found: {out.strip()}")
        print(f"fix: pip install clang-format=={PINNED_VERSION}")
        return False
    return True


def main():
    if not assert_pinned_version():
        return 2
    fix = "--fix" in sys.argv
    files = tracked_sources()
    mode = ["-i"] if fix else ["--dry-run", "--Werror"]
    result = subprocess.run([FORMAT, *mode, *files], cwd=ROOT, check=False)
    if result.returncode == 0:
        print(f"format gate OK: {len(files)} files ({'fixed' if fix else 'checked'})")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
