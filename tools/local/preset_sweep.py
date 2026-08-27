import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

RENDERER = ROOT / "bin" / "573Renderer.exe"
SHOTS = ROOT / "screenshots"


def dump_module():
    sys.path.insert(0, str(ROOT / "tools" / "ci"))
    import preset_dump

    return preset_dump


def slug(label):
    return label.split(",")[0].strip().replace(" ", "_").lower()


def shots_of(document, frames):
    markers = document.get("markers", [])
    if not markers:
        return [("", frames)]
    return [(f"-{slug(m['label'])}", int(m["frame"]) + 60) for m in markers]


def states_of(document):
    options = document.get("options", [])
    if not options:
        return [[]]
    first = options[0]
    return [[f"{first['id']}={choice['label']}"] for choice in first.get("choices", [])]


def run_one(game_dir, preset, out, frames, option):
    command = [
        str(RENDERER),
        "--preset-test",
        game_dir,
        preset,
        str(out),
        str(frames),
    ]
    for spec in option:
        command += ["--preset-option", spec]
    result = subprocess.run(command, check=False, capture_output=True)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(
        description="Render every built-in preset of a build in every state and fail on a "
        "frozen model"
    )
    parser.add_argument("build", help="build id, e.g. iidx11")
    parser.add_argument("game_dir", help="the game install directory")
    parser.add_argument("--frames", type=int, default=120)
    parser.add_argument(
        "--dump",
        default=None,
        help="read documents from an existing --preset-dump-defaults directory",
    )
    args = parser.parse_args()

    preset_dump = dump_module()
    try:
        docs = [d for d in preset_dump.documents(args.dump) if d["build"] == args.build]
    except preset_dump.DumpError as bad:
        print(f"preset sweep FAILED: {bad}")
        return 1
    if not docs:
        print(f"preset sweep FAILED: no built-in preset document has build '{args.build}'")
        return 1

    SHOTS.mkdir(exist_ok=True)
    failures = []
    total = 0
    for document in docs:
        preset = document["id"]
        states = states_of(document)
        for index, option in enumerate(states):
            for marker_suffix, frames in shots_of(document, args.frames):
                total += 1
                suffix = marker_suffix if len(states) == 1 else f"-state{index}{marker_suffix}"
                out = SHOTS / f"{preset}{suffix}.png"
                print(f"[{total}] {preset}{suffix} -> {out.name}", flush=True)
                code = run_one(args.game_dir, preset, out, frames, option)
                verdict = "ok" if code == 0 else f"FAILED ({code})"
                print(f"    {verdict}")
                if code != 0:
                    failures.append(f"{preset}{suffix}")

    print(f"\n{total - len(failures)}/{total} preset state(s) render with a moving model")
    if failures:
        print("frozen or broken: " + ", ".join(failures))
        print(
            "exit 8 means the preset was built from the screen's INIT and misses the "
            "per-frame update that drives the model"
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
