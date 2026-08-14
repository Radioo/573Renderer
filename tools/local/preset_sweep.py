import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RENDERER = ROOT / "bin" / "573Renderer.exe"
PRESET_DIR = ROOT / "src" / "preset"
SHOTS = ROOT / "screenshots"

SCENE_ID = re.compile(r'\.id\s*=\s*"([^"]+)"')
OPTION_CHOICES = re.compile(r"constexpr\s+std::array<OptionChoice,\s*(\d+)>\s+(\w+)")
SCENE_OPTIONS = re.compile(r"\.options\s*=\s*(\w+)")
OPTION_LIST = re.compile(
    r"constexpr\s+std::array<Option,\s*\d+>\s+(\w+)\s*=\s*\{\{(.*?)\}\};", re.S
)
CHOICES_FIELD = re.compile(r"\.choices\s*=\s*(\w+)")


def scenes_for(build):
    path = PRESET_DIR / f"scene_presets_{build}.cpp"
    text = path.read_text(encoding="utf-8")
    choice_counts = {name: int(n) for n, name in OPTION_CHOICES.findall(text)}
    option_choices = {}
    for name, body in OPTION_LIST.findall(text):
        field = CHOICES_FIELD.search(body)
        if field is not None:
            option_choices[name] = choice_counts.get(field.group(1), 1)

    out = []
    for block in text.split(".id = ")[1:]:
        ident = SCENE_ID.search(".id = " + block)
        if ident is None:
            continue
        options = SCENE_OPTIONS.search(block.split("},")[0] + block[:400])
        states = option_choices.get(options.group(1), 1) if options else 1
        scene_id = ident.group(1)
        if not scene_id.startswith(f"{build}-"):
            continue
        out.append((scene_id, states))
    return out


def main():
    parser = argparse.ArgumentParser(
        description="Render every preset of a build in every state and fail on a frozen model"
    )
    parser.add_argument("build", help="build id, e.g. iidx11")
    parser.add_argument("game_dir", help="the game install directory")
    parser.add_argument("--frames", type=int, default=120)
    args = parser.parse_args()

    SHOTS.mkdir(exist_ok=True)
    failures = []
    total = 0
    for scene, states in scenes_for(args.build):
        for state in range(states):
            total += 1
            suffix = "" if states == 1 else f"-state{state}"
            out = SHOTS / f"{scene}{suffix}.png"
            result = subprocess.run(
                [
                    str(RENDERER),
                    "--preset-test",
                    args.game_dir,
                    scene,
                    str(out),
                    str(args.frames),
                    str(state),
                ],
                check=False,
                capture_output=True,
            )
            verdict = "ok" if result.returncode == 0 else f"FAILED ({result.returncode})"
            print(f"{scene}{suffix}: {verdict} -> {out.name}")
            if result.returncode != 0:
                failures.append(f"{scene}{suffix}")

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
