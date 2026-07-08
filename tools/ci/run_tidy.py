import concurrent.futures
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
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


def compiled_targets():
    db_path = BUILD / "compile_commands.json"
    if not db_path.is_file():
        print(f"missing {db_path} - configure the build first (build.bat)")
        return None
    db = json.loads(db_path.read_text(encoding="utf-8"))
    by_abs = {}
    for entry in db:
        f = pathlib.Path(entry["file"])
        if not f.is_absolute():
            f = pathlib.Path(entry["directory"]) / f
        abs_src = f.resolve()
        obj = None
        m = re.search(r'/Fo("[^"]+"|\S+)', entry["command"])
        if m is not None:
            obj = m.group(1).strip('"').replace("\\", "/")
        by_abs[abs_src] = {"command": entry["command"], "obj": obj}

    out = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--cached", "--others", "--exclude-standard",
         "--", "*.cpp"], text=True)
    targets = []
    for line in out.splitlines():
        if not line:
            continue
        abs_src = (ROOT / line).resolve()
        info = by_abs.get(abs_src)
        if info is None or not abs_src.is_file():
            continue
        targets.append({"rel": line, "abs": abs_src, "command": info["command"],
                        "obj": info["obj"]})
    return targets


def ninja_exe():
    env = os.environ.get("TIDY_NINJA")
    if env:
        return env
    cache = BUILD / "CMakeCache.txt"
    if cache.is_file():
        for ln in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if ln.startswith("CMAKE_MAKE_PROGRAM"):
                val = ln.split("=", 1)[-1].strip()
                if val and "ninja" in val.lower():
                    return val
    return "ninja"


def load_dep_map():
    try:
        out = subprocess.check_output([ninja_exe(), "-C", str(BUILD), "-t", "deps"],
                                      text=True, stderr=subprocess.DEVNULL)
    except (OSError, subprocess.CalledProcessError):
        return None
    deps = {}
    cur = None
    for line in out.splitlines():
        if not line.strip():
            cur = None
            continue
        if not line[0].isspace():
            if ":" in line and line.rstrip().endswith("(VALID)"):
                obj = line.split(":", 1)[0].strip().replace("\\", "/")
                cur = []
                deps[obj] = cur
            else:
                cur = None
        elif cur is not None:
            cur.append((BUILD / line.strip()).resolve())
    return deps


class ContentHashes:
    def __init__(self):
        self._cache = {}

    def of(self, path):
        key = str(path)
        if key not in self._cache:
            try:
                self._cache[key] = hashlib.sha256(path.read_bytes()).hexdigest()
            except OSError:
                self._cache[key] = None
        return self._cache[key]


def file_key(target, deps, config_hash, hashes):
    if deps is None or target["obj"] is None:
        return None
    dep_paths = deps.get(target["obj"])
    if dep_paths is None:
        return None
    src_hash = hashes.of(target["abs"])
    if src_hash is None:
        return None
    dep_hashes = []
    for d in dep_paths:
        dh = hashes.of(d)
        if dh is None:
            return None
        dep_hashes.append(dh)
    h = hashlib.sha256()
    h.update(PINNED_VERSION.encode())
    h.update(config_hash.encode())
    h.update(target["command"].encode())
    h.update(src_hash.encode())
    for dh in sorted(dep_hashes):
        h.update(dh.encode())
    return h.hexdigest()


def run_one(target, cache_dir, key):
    if key is not None and cache_dir is not None:
        marker = cache_dir / (key + ".ok")
        if marker.is_file():
            return ("hit", target["rel"], "")
    cmd = [TIDY, "-p", str(BUILD), "--quiet", "--warnings-as-errors=*",
           "--extra-arg=/clang:-Qunused-arguments", str(target["abs"])]
    proc = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True, check=False)
    if proc.returncode == 0:
        if key is not None and cache_dir is not None:
            marker = cache_dir / (key + ".ok")
            tmp = cache_dir / (key + f".tmp{os.getpid()}")
            tmp.write_text("", encoding="utf-8")
            os.replace(tmp, marker)
        return ("ran", target["rel"], "")
    return ("fail", target["rel"], (proc.stdout or "") + (proc.stderr or ""))


def worker_count():
    env = os.environ.get("TIDY_JOBS")
    if env and env.isdigit() and int(env) > 0:
        return int(env)
    return min(os.cpu_count() or 4, 8)


def main():
    if not assert_pinned_version():
        return 2
    targets = compiled_targets()
    if targets is None:
        return 2
    if not targets:
        print("tidy gate OK: no compiled sources found")
        return 0

    cache_dir = None
    deps = None
    if not os.environ.get("TIDY_NO_CACHE"):
        cache_dir = pathlib.Path(os.environ.get("TIDY_CACHE_DIR") or (ROOT / ".tidycache"))
        cache_dir.mkdir(parents=True, exist_ok=True)
        deps = load_dep_map()
        if deps is None:
            print("tidy cache: ninja dep graph unavailable - running without cache")
            cache_dir = None

    config_hash = hashlib.sha256((ROOT / ".clang-tidy").read_bytes()).hexdigest()
    hashes = ContentHashes()
    keys = {t["rel"]: file_key(t, deps, config_hash, hashes) for t in targets}

    hits = ran = 0
    failures = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=worker_count()) as pool:
        futs = {pool.submit(run_one, t, cache_dir, keys[t["rel"]]): t for t in targets}
        for fut in concurrent.futures.as_completed(futs):
            status, rel, output = fut.result()
            if status == "hit":
                hits += 1
            elif status == "ran":
                ran += 1
            else:
                failures.append((rel, output))

    for rel, output in failures:
        print(f"::group::clang-tidy failure: {rel}")
        print(output.rstrip())
        print("::endgroup::")

    cached = "cache off" if cache_dir is None else f"{hits} cached / {ran} analysed"
    if failures:
        print(f"tidy gate FAILED: {len(failures)} file(s) with findings "
              f"({len(targets)} total, {cached})")
        return 1
    print(f"tidy gate OK: {len(targets)} files (whole tree, {cached})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
