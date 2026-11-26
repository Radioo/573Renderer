import argparse
import sys
from pathlib import Path

from preset_dump import DumpError, add_dump_argument, documents

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "tools" / "ci" / "data" / "preset_states.tsv"


def states_of(document):
    labels = [marker["label"] for marker in document.get("markers", [])]
    for option in document.get("options", []):
        labels += [choice["label"] for choice in option.get("choices", [])]
    return labels


def read_dump_states(docs):
    states = {}
    for document in docs:
        labels = states_of(document)
        if labels:
            states[document["id"]] = labels
    return states


def read_recorded():
    documented = {}
    gaps = []
    for line in DATA.read_text(encoding="utf-8").splitlines()[1:]:
        cols = line.split("\t")
        if len(cols) != 3:
            continue
        if cols[0] == "state":
            documented.setdefault(cols[1], []).append(cols[2])
        elif cols[0] == "gap":
            gaps.append(cols[1])
    return documented, gaps


def check(docs):
    if not docs:
        return [
            "the dump holds no preset documents at all. The gate can only check what the "
            "renderer dumps, so an empty dump means no state is checked at all."
        ], {}, []
    states = read_dump_states(docs)
    if not states:
        return [
            f"{len(docs)} document(s) dumped but not one marker or option choice was found. "
            f"Every screen with a sequence carries markers, so this means the gate is blind."
        ], {}, []

    known_ids = {document["id"] for document in docs}
    documented, gaps = read_recorded()
    problems = []
    for preset, labels in documented.items():
        if preset not in known_ids:
            problems.append(
                f"{preset}: tools/ci/data/preset_states.tsv names a preset no built-in "
                f"document provides. Fix the id there, or ship the preset."
            )
            continue
        have = states.get(preset, [])
        for label in labels:
            if label in have:
                continue
            problems.append(
                f"{preset}: a state '{label}' is recorded that the preset does not "
                f"expose. A preset must offer every state its screen can show."
            )

    for preset, labels in states.items():
        for label in labels:
            if label in documented.get(preset, []):
                continue
            problems.append(
                f"{preset}: state '{label}' has no row in tools/ci/data/preset_states.tsv. "
                f"Find where it comes from in the game, then record it."
            )
    return problems, states, gaps


def main():
    parser = argparse.ArgumentParser(
        description="Check every marker and option choice of a built-in preset against "
        "tools/ci/data/preset_states.tsv"
    )
    add_dump_argument(parser)
    args = parser.parse_args()

    try:
        docs = documents(args.dump)
    except DumpError as bad:
        print("preset-state gate FAILED:")
        print(f"  {bad}")
        return 1

    problems, states, gaps = check(docs)
    if problems:
        print("preset-state gate FAILED:")
        for problem in problems:
            print(f"  {problem}")
        return 1

    total = sum(len(v) for v in states.values())
    print(f"preset-state gate OK: {total} state(s) across {len(states)} preset(s), all recorded")
    if gaps:
        print(f"  known gaps still to build: {len(gaps)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
