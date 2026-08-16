import argparse
import sys
from pathlib import Path

VERDICTS = {"background": "Background", "chrome": "Chrome"}

HEADER = """#include "preset/preset_layer_verdicts.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

namespace Preset {

namespace {

struct Row {
    std::string_view package;
    std::string_view layer;
    LayerVerdict verdict;
};

constexpr std::array<Row, %d> kRows = {
"""

FOOTER = """};

}

LayerVerdict VerdictFor(std::string_view package_dir, std::string_view layer) {
    for (const Row& row : kRows) {
        if (row.package == package_dir && row.layer == layer) return row.verdict;
    }
    return LayerVerdict::Unclassified;
}

std::string_view VerdictName(LayerVerdict verdict) {
    switch (verdict) {
    case LayerVerdict::Background:
        return "background";
    case LayerVerdict::Chrome:
        return "chrome";
    case LayerVerdict::Unclassified:
    default:
        return "unclassified";
    }
}

}
"""


def read_rows(path):
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            continue
        cols = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cols) < 4 or cols[3] not in VERDICTS:
            continue
        rows.append((cols[0], cols[1], VERDICTS[cols[3]]))
    return sorted(set(rows))


def render(rows):
    body = "".join(
        f'    Row{{"{package}", "{layer}", LayerVerdict::{verdict}}},\n'
        for package, layer, verdict in rows
    )
    return (HEADER % len(rows)) + body + FOOTER


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="docs/preset_layers.md")
    parser.add_argument("output", type=Path, help="generated .cpp to write")
    args = parser.parse_args()

    rows = read_rows(args.source)
    if not rows:
        print(f"{args.source}: no classification rows found", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    text = render(rows)
    if args.output.exists() and args.output.read_text(encoding="utf-8") == text:
        return 0
    args.output.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
