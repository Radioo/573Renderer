import argparse
import sys
from pathlib import Path

from preset_dump import DumpError, add_dump_argument, documents

ROOT = Path(__file__).resolve().parents[2]
DOC = ROOT / "docs" / "preset_states.md"
GAPS_HEADING = "## Screens whose remaining states are not yet exposed"


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


def table_rows(text, second_heading):
    rows = []
    inside = False
    for line in text.splitlines():
        if not line.startswith("|"):
            inside = False
            continue
        cols = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cols) < 3:
            inside = False
            continue
        if cols[0] == "preset" and cols[1] == second_heading:
            inside = True
            continue
        if inside and "".join(cols).strip("-: "):
            rows.append(cols)
    return rows


def read_doc():
    text = DOC.read_text(encoding="utf-8")
    head, _, gaps = text.partition(GAPS_HEADING)
    documented = {}
    for cols in table_rows(head, "state"):
        documented.setdefault(cols[0], []).append(cols[1])
    return documented, [cols[0] for cols in table_rows(gaps, "state still missing")]


def check(docs):
    if not docs:
        return [
            "the dump holds no preset documents at all. The gate can only check what the "
            "renderer dumps, so an empty dump means no state is checked against the docs."
        ], {}, []
    states = read_dump_states(docs)
    if not states:
        return [
            f"{len(docs)} document(s) dumped but not one marker or option choice was found. "
            f"Every screen with a sequence carries markers, so this means the gate is blind."
        ], {}, []

    known_ids = {document["id"] for document in docs}
    documented, gaps = read_doc()
    problems = []
    for preset, labels in documented.items():
        if preset not in known_ids:
            problems.append(
                f"{preset}: docs/preset_states.md names a preset no built-in document "
                f"provides. Fix the id in the docs, or ship the preset."
            )
            continue
        have = states.get(preset, [])
        for label in labels:
            if label in have:
                continue
            problems.append(
                f"{preset}: the docs record a state '{label}' that the preset does not "
                f"expose. A preset must offer every state its screen can show."
            )

    for preset, labels in states.items():
        for label in labels:
            if label in documented.get(preset, []):
                continue
            problems.append(
                f"{preset}: state '{label}' has no row in docs/preset_states.md - record "
                f"where it came from in the game before shipping it."
            )
    return problems, states, gaps


def main():
    parser = argparse.ArgumentParser(
        description="Check every marker and option choice of a built-in preset against "
        "docs/preset_states.md"
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
        print(f"  known gaps still to build: {len(gaps)} (see docs/preset_states.md)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
