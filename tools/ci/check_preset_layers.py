import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRESET_DIR = ROOT / "src" / "preset"
DOC = ROOT / "docs" / "preset_layers.md"

PACKAGE_CONST = re.compile(r'constexpr\s+std::string_view\s+(\w+)\s*=\s*"([^"]+)"\s*;')
PART_LIST = re.compile(
    r"constexpr\s+std::array<std::string_view,\s*\d+>\s+(\w+)\s*=\s*\{([^}]*)\}", re.S
)
QUOTED = re.compile(r'"([^"]+)"')
LAYER = re.compile(
    r"\.package_dir\s*=\s*(\w+)\s*,(?P<body>.*?)(?=\.package_dir\s*=|\}\}\s*;)", re.S
)
SPRITE = re.compile(r'\.sprite\s*=\s*"([^"]+)"')
HIDDEN = re.compile(r"\.hidden_parts\s*=\s*(\w+)")


def read_doc():
    verdicts = {}
    for line in DOC.read_text(encoding="utf-8").splitlines():
        if not line.startswith("|"):
            continue
        cols = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cols) < 4 or cols[3] not in ("background", "chrome"):
            continue
        verdicts[(cols[0], cols[1])] = cols[3]
    return verdicts


def read_presets():
    used = []
    hidden = []
    for path in sorted(PRESET_DIR.glob("scene_presets_*.cpp")):
        text = path.read_text(encoding="utf-8")
        packages = dict(PACKAGE_CONST.findall(text))
        parts = {
            name: QUOTED.findall(body) for name, body in PART_LIST.findall(text)
        }
        for match in LAYER.finditer(text):
            package = packages.get(match.group(1))
            if package is None:
                continue
            body = match.group("body")
            sprite = SPRITE.search(body)
            if sprite is not None:
                used.append((path.name, package, sprite.group(1)))
            skip = HIDDEN.search(body)
            if skip is not None:
                for part in parts.get(skip.group(1), []):
                    hidden.append((path.name, package, part))
    return used, hidden


def main():
    verdicts = read_doc()
    used, hidden = read_presets()
    problems = []

    for origin, package, layer in used:
        verdict = verdicts.get((package, layer))
        if verdict is None:
            problems.append(
                f"{origin}: {package}/{layer} is in a preset but has no row in "
                f"docs/preset_layers.md - render it with --gc2d-sheet, look at it, "
                f"then write down what it is"
            )
        elif verdict != "background":
            problems.append(
                f"{origin}: {package}/{layer} is classified '{verdict}' in "
                f"docs/preset_layers.md - a preset may only draw background layers"
            )

    for origin, package, part in hidden:
        verdict = verdicts.get((package, part))
        if verdict is None:
            problems.append(
                f"{origin}: hidden part {package}/{part} has no row in "
                f"docs/preset_layers.md"
            )
        elif verdict != "chrome":
            problems.append(
                f"{origin}: hidden part {package}/{part} is classified '{verdict}' - "
                f"only chrome is worth hiding"
            )

    if problems:
        print("preset-layer gate FAILED:")
        for problem in problems:
            print(f"  {problem}")
        return 1

    print(
        f"preset-layer gate OK: {len(used)} layer(s) and {len(hidden)} hidden part(s), "
        f"all classified as background art"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
