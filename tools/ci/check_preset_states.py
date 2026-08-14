import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRESET_DIR = ROOT / "src" / "preset"
DOC = ROOT / "docs" / "preset_states.md"
GAPS_HEADING = "## Screens whose remaining states are not yet exposed"

ARRAY = re.compile(
    r"constexpr\s+std::array<(Phase|OptionChoice|Option),\s*\d+>\s+(\w+)\s*=\s*\{\{(.*?)\}\};",
    re.S,
)
LABEL = re.compile(r'\.label\s*=\s*"([^"]+)"')
CHOICES = re.compile(r"\.choices\s*=\s*(\w+)")
SCENE_ID = re.compile(r'\.id\s*=\s*"([^"]+)"')
PHASES = re.compile(r"\.phases\s*=\s*(\w+)")
OPTIONS = re.compile(r"\.options\s*=\s*(\w+)")


def preset_sources():
    return sorted(PRESET_DIR.glob("scene_presets_*.cpp")) + sorted(
        PRESET_DIR.glob("scene_presets_*.h")
    )


def read_source():
    arrays = {}
    for path in preset_sources():
        for kind, name, body in ARRAY.findall(path.read_text(encoding="utf-8")):
            arrays[name] = (kind, body)

    states = {}
    for path in preset_sources():
        text = path.read_text(encoding="utf-8")
        for block in text.split(".id = ")[1:]:
            ident = SCENE_ID.search(".id = " + block)
            if ident is None or not ident.group(1).startswith("iidx"):
                continue
            head = block[:600]
            labels = []
            phases = PHASES.search(head)
            if phases is not None and phases.group(1) in arrays:
                labels += LABEL.findall(arrays[phases.group(1)][1])
            options = OPTIONS.search(head)
            if options is not None and options.group(1) in arrays:
                for choice_ref in CHOICES.findall(arrays[options.group(1)][1]):
                    if choice_ref in arrays:
                        labels += LABEL.findall(arrays[choice_ref][1])
            if labels:
                states[ident.group(1)] = labels
    return states


def read_doc():
    documented = {}
    gap_rows = []
    text = DOC.read_text(encoding="utf-8")
    head, _, gaps = text.partition(GAPS_HEADING)
    for line in head.splitlines():
        if not line.startswith("|"):
            continue
        cols = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cols) < 3 or not cols[0].startswith("iidx"):
            continue
        documented.setdefault(cols[0], []).append(cols[1])
    for line in gaps.splitlines():
        if not line.startswith("|"):
            continue
        first = line.strip().strip("|").split("|")[0].strip()
        if first.startswith("iidx"):
            gap_rows.append(first)
    return documented, gap_rows


def main():
    source = read_source()
    documented, gaps = read_doc()
    problems = []

    for preset, labels in documented.items():
        have = source.get(preset, [])
        for label in labels:
            if label in have:
                continue
            problems.append(
                f"{preset}: the docs record a state '{label}' that the preset does not "
                f"expose. A preset must offer every state its screen can show."
            )

    for preset, labels in source.items():
        for label in labels:
            if label in documented.get(preset, []):
                continue
            problems.append(
                f"{preset}: state '{label}' has no row in docs/preset_states.md - record "
                f"where it came from in the game before shipping it."
            )

    if problems:
        print("preset-state gate FAILED:")
        for problem in problems:
            print(f"  {problem}")
        return 1

    total = sum(len(v) for v in source.values())
    print(f"preset-state gate OK: {total} state(s) across {len(source)} preset(s), all recorded")
    if gaps:
        print(f"  known gaps still to build: {len(gaps)} (see docs/preset_states.md)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
