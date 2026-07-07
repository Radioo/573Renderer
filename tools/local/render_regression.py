import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXE = ROOT / "bin" / "573Renderer.exe"
BASELINE_DIR = ROOT / "local_baselines"
BASELINE_FILE = BASELINE_DIR / "render_regression.json"

ENV_SDVX = "R573_SDVX_DIR"
ENV_IIDX = "R573_IIDX_DIR"
ENV_DDR = "R573_DDR_DIR"


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run_renderer(args, timeout_s, env=None):
    full_env = os.environ.copy()
    if env:
        full_env.update(env)
    proc = subprocess.run([str(EXE), *args], cwd=ROOT, timeout=timeout_s,
                          capture_output=True, env=full_env, check=False)
    return proc.returncode


def collect_hashes(out_dir, patterns):
    hashes = {}
    for pattern in patterns:
        for f in sorted(out_dir.glob(pattern)):
            hashes[f.name] = sha256_file(f)
    return hashes


def scenario_sdvx(game_dir, work):
    out = work / "sdvx"
    out.mkdir(parents=True)
    rc = run_renderer([
        "--no-gui", "--game-dir", game_dir, "--profile", "sdvx7",
        "--render-size", "1080x1920",
        "--ifs", str(pathlib.PureWindowsPath(game_dir) / "data/graphics/select_bg/select_bg_vi.ifs"),
        "--export", str(out / "out.webp"), "--export-max-frames", "12",
        "--export-bg", "32,64,96", "--export-crop", "100,200,400,300",
        "--export-dump-frames", str(out),
    ], timeout_s=360)
    if rc != 0:
        return None, f"renderer exit {rc}"
    return collect_hashes(out, ["*.bgra", "out.webp"]), None


def scenario_iidx(game_dir, work):
    out = work / "iidx"
    out.mkdir(parents=True)
    rc = run_renderer([
        "--no-gui", "--game-dir", game_dir, "--profile", "iidx33",
        "--ifs", str(pathlib.PureWindowsPath(game_dir) / "data/graphic/02005.ifs"),
        "--export", str(out / "out.webp"), "--export-max-frames", "12",
        "--export-dump-frames", str(out),
    ], timeout_s=600)
    if rc != 0:
        return None, f"renderer exit {rc}"
    return collect_hashes(out, ["*.bgra", "out.webp"]), None


def scenario_ddr(game_dir, work):
    out = work / "ddr"
    out.mkdir(parents=True)
    gd = pathlib.PureWindowsPath(game_dir)
    rc = run_renderer([
        "--ddr-test", str(gd / "modules"),
        str(gd / "data/arc/custom/background/background_0009.arc"),
        str(out / "final.png"), "12",
    ], timeout_s=300, env={"DDR_CAPTURE_FROM": "0", "DDR_CAPTURE_TO": "11"})
    if rc != 0:
        return None, f"renderer exit {rc}"
    return collect_hashes(out, ["seq_*.png", "final.png"]), None


SCENARIOS = [
    ("sdvx_select_bg_vi", ENV_SDVX, scenario_sdvx),
    ("iidx_02005", ENV_IIDX, scenario_iidx),
    ("ddr_bg_0009", ENV_DDR, scenario_ddr),
]


def main():
    ap = argparse.ArgumentParser(
        description="Local render-regression net: runs the standing byte-compare "
                    "scenarios and SHA-compares every dumped frame + encoded output "
                    "against the locally blessed baseline. Game dirs come from the "
                    "R573_SDVX_DIR / R573_IIDX_DIR / R573_DDR_DIR environment "
                    "variables; scenarios without a dir are skipped loudly.")
    ap.add_argument("--bless", action="store_true",
                    help="overwrite the baseline with this run's hashes")
    opts = ap.parse_args()

    if not EXE.is_file():
        print(f"FATAL: {EXE} not found - build first (build.bat)")
        return 2

    baseline = {}
    if BASELINE_FILE.is_file():
        baseline = json.loads(BASELINE_FILE.read_text(encoding="utf-8"))

    results = {}
    failures = []
    skipped = []
    work = pathlib.Path(tempfile.mkdtemp(prefix="r573_regress_"))
    try:
        for name, env_var, fn in SCENARIOS:
            game_dir = os.environ.get(env_var, "")
            if not game_dir:
                skipped.append(f"{name} ({env_var} not set)")
                continue
            print(f"[{name}] running...")
            hashes, err = fn(game_dir, work)
            if err:
                failures.append(f"{name}: {err}")
                continue
            if not hashes:
                failures.append(f"{name}: produced no outputs")
                continue
            results[name] = hashes
            if opts.bless or name not in baseline:
                continue
            ref = baseline[name]
            for fname in sorted(set(ref) | set(hashes)):
                if fname not in hashes:
                    failures.append(f"{name}/{fname}: missing from this run")
                elif fname not in ref:
                    failures.append(f"{name}/{fname}: not in baseline (re-bless?)")
                elif ref[fname] != hashes[fname]:
                    failures.append(f"{name}/{fname}: hash mismatch")
            print(f"[{name}] {len(hashes)} outputs compared")
    finally:
        shutil.rmtree(work, ignore_errors=True)

    for s in skipped:
        print(f"SKIPPED: {s}")

    if opts.bless:
        if failures:
            for f in failures:
                print(f"FAIL: {f}")
            print("bless aborted: fix the scenario failures first")
            return 1
        BASELINE_DIR.mkdir(exist_ok=True)
        baseline.update(results)
        BASELINE_FILE.write_text(json.dumps(baseline, indent=1, sort_keys=True),
                                 encoding="utf-8")
        total = sum(len(v) for v in results.values())
        print(f"BLESSED: {len(results)} scenario(s), {total} output hashes -> {BASELINE_FILE}")
        return 0

    fresh = [n for n in results if n not in baseline]
    for n in fresh:
        print(f"NO BASELINE for {n}: run with --bless to record one")

    if failures:
        for f in failures:
            print(f"FAIL: {f}")
        return 1
    if not results:
        print("nothing ran - set the R573_*_DIR environment variables")
        return 1
    if fresh and len(fresh) == len(results):
        return 1
    print("render regression OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
