import argparse
import sys
from pathlib import Path

from preset_dump import DumpError, add_dump_argument, asset_dirs, clips_of, documents

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "tools" / "ci" / "data" / "preset_layers.tsv"

SPRITE_TYPES = ("sprite.draw", "sprite.animate")


def read_verdicts():
    verdicts = {}
    for line in DATA.read_text(encoding="utf-8").splitlines()[1:]:
        cols = line.split("\t")
        if len(cols) != 3 or cols[2] not in ("background", "chrome"):
            continue
        verdicts[(cols[0], cols[1])] = cols[2]
    return verdicts


def read_layers(docs):
    used = []
    hidden = []
    for document in docs:
        dirs = asset_dirs(document)
        origin = f"{document['build']}/{document['id']}"
        for clip in clips_of(document, SPRITE_TYPES):
            params = clip.get("params", {})
            package = dirs.get(params.get("asset"))
            if package is None:
                continue
            layer = params.get("cell") or params.get("animation")
            if layer is not None:
                used.append((origin, package, layer))
            for part in params.get("hidden_parts", []):
                hidden.append((origin, package, part))
    return used, hidden


def check(docs):
    problems = []
    if not docs:
        return [
            "the dump holds no preset documents at all. The gate can only check what the "
            "renderer dumps, so an empty dump means it is silently passing every layer."
        ]
    used, hidden = read_layers(docs)
    if not used:
        return [
            f"{len(docs)} document(s) dumped but not one sprite.draw or sprite.animate clip "
            f"was found. The gate would pass without looking at a single layer."
        ]

    verdicts = read_verdicts()
    for origin, package, layer in used:
        verdict = verdicts.get((package, layer))
        if verdict is None:
            problems.append(
                f"{origin}: {package}/{layer} is in a preset but has no row in "
                f"tools/ci/data/preset_layers.tsv. Render it with --gc2d-sheet, look at it, "
                f"then add its verdict"
            )
        elif verdict != "background":
            problems.append(
                f"{origin}: {package}/{layer} is classified '{verdict}' in "
                f"tools/ci/data/preset_layers.tsv, and a preset may only draw background layers"
            )

    for origin, package, part in hidden:
        verdict = verdicts.get((package, part))
        if verdict is None:
            problems.append(
                f"{origin}: hidden part {package}/{part} has no row in "
                f"tools/ci/data/preset_layers.tsv"
            )
        elif verdict != "chrome":
            problems.append(
                f"{origin}: hidden part {package}/{part} is classified '{verdict}', "
                f"and only chrome is worth hiding"
            )
    return problems


def main():
    parser = argparse.ArgumentParser(
        description="Check every 2D layer a built-in preset draws against "
        "tools/ci/data/preset_layers.tsv"
    )
    add_dump_argument(parser)
    args = parser.parse_args()

    try:
        docs = documents(args.dump)
    except DumpError as bad:
        print("preset-layer gate FAILED:")
        print(f"  {bad}")
        return 1

    problems = check(docs)
    if problems:
        print("preset-layer gate FAILED:")
        for problem in problems:
            print(f"  {problem}")
        return 1

    used, hidden = read_layers(docs)
    distinct = {(package, layer) for _, package, layer in used}
    print(
        f"preset-layer gate OK: {len(distinct)} layer(s) drawn by {len(used)} clip(s) and "
        f"{len(hidden)} hidden part(s) across {len(docs)} document(s), all classified as "
        f"background art"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
