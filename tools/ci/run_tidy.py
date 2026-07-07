import importlib.util
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PINNED_VERSION = "21.1.6"


def clang_tidy_exe():
    spec = importlib.util.find_spec("clang_tidy")
    if spec is not None and spec.origin is not None:
        base = pathlib.Path(spec.origin).parent / "data" / "bin"
        for name in ("clang-tidy.exe", "clang-tidy"):
            candidate = base / name
            if candidate.exists():
                return str(candidate)
    return "clang-tidy"


TIDY = clang_tidy_exe()


def assert_pinned_version():
    out = subprocess.check_output([TIDY, "--version"], text=True)
    if PINNED_VERSION not in out:
        print(f"clang-tidy version mismatch: need {PINNED_VERSION}, using {TIDY}")
        print(f"found: {out.strip()}")
        print(f"fix: pip install clang-tidy=={PINNED_VERSION}")
        return False
    return True


def compiled_sources():
    db_path = ROOT / "build" / "compile_commands.json"
    if not db_path.is_file():
        print(f"missing {db_path} - configure the build first (build.bat)")
        return None
    db = json.loads(db_path.read_text(encoding="utf-8"))
    compiled = set()
    for entry in db:
        f = pathlib.Path(entry["file"])
        if not f.is_absolute():
            f = pathlib.Path(entry["directory"]) / f
        try:
            compiled.add(f.resolve().relative_to(ROOT).as_posix())
        except ValueError:
            continue
    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard",
         "--", "*.cpp"], text=True)
    tracked = [line for line in out.splitlines() if line and (ROOT / line).is_file()]
    return [f for f in tracked if pathlib.PurePosixPath(f).as_posix() in compiled]


def main():
    if not assert_pinned_version():
        return 2
    targets = compiled_sources()
    if targets is None:
        return 2
    if not targets:
        print("tidy gate OK: no compiled sources found")
        return 0
    cmd = [TIDY, "-p", str(ROOT / "build"), "--quiet", "--warnings-as-errors=*",
           "--extra-arg=/clang:-Qunused-arguments", *targets]
    result = subprocess.run(cmd, cwd=ROOT, check=False)
    if result.returncode == 0:
        print(f"tidy gate OK: {len(targets)} files (whole tree)")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
