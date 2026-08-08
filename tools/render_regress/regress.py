import argparse
import concurrent.futures
import hashlib
import json
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFEST = pathlib.Path(__file__).resolve().parent / "cases.json"
GAME_DIRS = pathlib.Path(__file__).resolve().parent / "game_dirs.json"
GOLDEN_DIR = ROOT / "tools" / "render_regress" / "goldens"
OUT_DIR = ROOT / "screenshots" / "regress"


def load_cases(only):
    cases = json.loads(MANIFEST.read_text(encoding="utf-8"))["cases"]
    if only:
        wanted = set(only)
        cases = [c for c in cases if c["name"] in wanted]
        missing = wanted - {c["name"] for c in cases}
        if missing:
            sys.exit(f"unknown case(s): {', '.join(sorted(missing))}")
    return cases


def load_game_dirs():
    if not GAME_DIRS.exists():
        return None
    return json.loads(GAME_DIRS.read_text(encoding="utf-8"))


def resolve_ifs(case, game_dir):
    candidates = [game_dir / p for p in case["ifs_candidates"]]
    existing = [p for p in candidates if p.exists()]
    if not existing:
        return None
    return max(existing, key=lambda p: p.stat().st_mtime)


def exe_for(case):
    name = "573Renderer32.exe" if case["arch"] == "x86" else "573Renderer.exe"
    return ROOT / "bin" / name


def run_case(args):
    case, game_dirs = args
    started = time.monotonic()
    exe = exe_for(case)
    if not exe.exists():
        return case, None, f"missing {exe.name} - build it first"

    configured = game_dirs.get(case["name"]) if game_dirs else None
    if not configured:
        return case, None, "no game dir configured in game_dirs.json"

    game_dir = pathlib.Path(configured)
    if not game_dir.exists():
        return case, None, "game dir not present on this machine"

    ifs = resolve_ifs(case, game_dir)
    if ifs is None:
        return case, None, "no candidate ifs found"

    frames = case["frames"]
    prefix = OUT_DIR / f"{case['name']}_"
    prefix.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(exe),
        "--no-gui",
        "--game-dir", str(game_dir),
        "--profile", case["profile"],
        "--ifs", str(ifs),
        "--screenshot-frames", ",".join(str(f) for f in frames),
        "--screenshot-prefix", str(prefix),
        "--exit-after-frames", str(max(frames) + 5),
    ]
    if case.get("render_size"):
        cmd += ["--render-size", case["render_size"]]
    if case.get("animation"):
        cmd += ["--animation", case["animation"]]
    if case.get("swap_to"):
        swap = game_dir / case["swap_to"]
        if not swap.exists():
            return case, None, f"swap target {case['swap_to']} not present"
        cmd += ["--swap-after-frames", str(case.get("swap_after", 5)),
                "--ifs2", str(swap)]

    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=case.get("timeout", 120))
    elapsed = time.monotonic() - started

    shots = {}
    for f in frames:
        p = pathlib.Path(f"{prefix}{f}.png")
        if not p.exists():
            return case, None, f"frame {f} not produced (exit {proc.returncode}, {elapsed:.1f}s)"
        shots[str(f)] = hashlib.sha256(p.read_bytes()).hexdigest()
    return case, {"frames": shots, "seconds": round(elapsed, 1), "ifs": ifs.name}, None


def main():
    ap = argparse.ArgumentParser(description="573Renderer render regression harness")
    ap.add_argument("--update", action="store_true", help="record current output as the golden set")
    ap.add_argument("--case", action="append", help="run only this case (repeatable)")
    ap.add_argument("--jobs", type=int, default=1, help="parallel cases (default 1)")
    args = ap.parse_args()

    cases = load_cases(args.case)
    game_dirs = load_game_dirs()
    if game_dirs is None:
        print(f"no {GAME_DIRS.name} - copy game_dirs.example.json to it and fill in your paths")
        print("(that file is gitignored; game locations stay on your machine)")
        return 1
    GOLDEN_DIR.mkdir(parents=True, exist_ok=True)
    golden_path = GOLDEN_DIR / "hashes.json"
    golden = json.loads(golden_path.read_text(encoding="utf-8")) if golden_path.exists() else {}

    results = {}
    skipped, failed, passed = [], [], []

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        for case, got, err in pool.map(run_case, [(c, game_dirs) for c in cases]):
            name = case["name"]
            if err and ("not present" in err or "no game dir configured" in err):
                skipped.append((name, err))
                print(f"SKIP  {name:16s} {err}")
                continue
            if err:
                failed.append((name, err))
                print(f"FAIL  {name:16s} {err}")
                continue

            results[name] = got
            want = golden.get(name, {}).get("frames")
            if args.update or want is None:
                state = "RECORD" if args.update else "NEW"
                print(f"{state:5s} {name:16s} {got['ifs']} {got['seconds']}s")
                passed.append(name)
                continue

            diffs = [f for f, h in got["frames"].items() if want.get(f) != h]
            if diffs:
                failed.append((name, f"frame(s) {','.join(diffs)} differ from golden"))
                print(f"FAIL  {name:16s} frame(s) {','.join(diffs)} differ ({got['seconds']}s)")
            else:
                passed.append(name)
                print(f"ok    {name:16s} {got['ifs']} {got['seconds']}s")

    if args.update:
        merged = dict(golden)
        merged.update(results)
        golden_path.write_text(json.dumps(merged, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"\nrecorded {len(results)} case(s) -> {golden_path.relative_to(ROOT)}")
        return 0

    print(f"\n{len(passed)} ok, {len(failed)} failed, {len(skipped)} skipped")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
