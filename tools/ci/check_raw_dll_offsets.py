import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "src"

BASES = "afpcore|afp_core|afpu_base|afpu_mod|m"
MODULE_BASE = re.compile(
    r"\(uint8_t\*\)\s*(" + BASES + r")\s*\+\s*0x[0-9A-Fa-f]+")
CAST_BASE = re.compile(r"reinterpret_cast<uint8_t\*>\([^)]*\)\s*\+\s*0x[0-9A-Fa-f]+")


def main() -> int:
    bad = []
    for path in sorted(SRC.rglob("*.cpp")) + sorted(SRC.rglob("*.h")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for number, line in enumerate(text.splitlines(), 1):
            if MODULE_BASE.search(line) or CAST_BASE.search(line):
                bad.append(f"{path.relative_to(ROOT)}:{number}: {line.strip()}")
    if bad:
        print("raw-dll-offset gate FAILED: a literal offset is added to a DLL base.")
        print("Put it in AfpProfiles::DllOffsetSet and read it from ActiveOffsets(),")
        print("so every game build carries its own measured address:")
        for entry in bad:
            print("  " + entry)
        return 1
    print(f"raw-dll-offset gate OK: {len(list(SRC.rglob('*.cpp')))} sources checked")
    return 0


if __name__ == "__main__":
    sys.exit(main())
