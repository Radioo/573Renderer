import argparse
import pathlib
import sys

import pefile

FIRST_ID = 256
BLOCK_IDS = 16
BLOCK_BYTES = 6
NAME_STRIDE = 4
MAX_BLOCKS = 4096
LINE_ROOM = 84

KNOWN = {
    0x390: "aeplib",
    0x440: "stop",
    0x442: "gotoAndPlay",
    0x443: "gotoAndStop",
    0x814: "deepStop",
    0x815: "deepGotoAndPlay",
    0x832: "aep_set_frame_control",
    0x833: "aep_set_rect_mask",
    0x836: "aep_set_set_frame",
}


class Image:
    def __init__(self, path):
        binary = pefile.PE(str(path))
        self.base = binary.OPTIONAL_HEADER.ImageBase
        self.bytes = binary.get_memory_mapped_image()

    def word(self, address):
        at = address - self.base
        if at < 0 or at + 2 > len(self.bytes):
            raise ValueError(f"{address:#x} is outside the image")
        return self.bytes[at] | (self.bytes[at + 1] << 8)

    def text(self, address):
        at = address - self.base
        if at < 0 or at >= len(self.bytes):
            raise ValueError(f"{address:#x} is outside the image")
        return self.bytes[at : self.bytes.index(b"\0", at)].decode("cp932", "replace")


def read_table(image, blocks, index, names):
    found = {}
    for block in range(MAX_BLOCKS):
        triple = blocks + BLOCK_BYTES * block
        low = image.word(triple)
        high = image.word(triple + 2)
        shift = image.word(triple + 4)
        if low == 0 and high == 0:
            continue
        if low != FIRST_ID + BLOCK_IDS * block or high <= low or high > low + BLOCK_IDS:
            return found
        for ident in range(low, high):
            slot = image.word(index + 2 * (ident + shift - low))
            found[ident] = image.text(names + NAME_STRIDE * slot)
    return found


def check(found):
    trouble = []
    for ident, name in sorted(KNOWN.items()):
        if found.get(ident) != name:
            trouble.append(f"  {ident:#x} reads as {found.get(ident)!r}, not {name!r}")
    if trouble:
        print("The addresses do not describe the builtin name table:")
        print("\n".join(trouble))
        return False
    clashes = [n for n in set(found.values()) if list(found.values()).count(n) > 1]
    if clashes:
        print(f"{len(clashes)} names are shared by more than one id, such as {clashes[:5]}")
        return False
    return True


def runs_of(found):
    runs = []
    for ident in sorted(found):
        if runs and runs[-1][0] + len(runs[-1][1]) == ident:
            runs[-1][1].append(found[ident])
        else:
            runs.append((ident, [found[ident]]))
    return runs


def wrapped(names):
    lines = [""]
    for name in names:
        piece = name if not lines[-1] else " " + name
        if len(lines[-1]) + len(piece) > LINE_ROOM and lines[-1]:
            lines.append(name)
            continue
        lines[-1] += piece
    return lines


def written(runs):
    out = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace AfpScript::Detail {",
        "",
        "struct NameRun {",
        "    uint16_t first;",
        "    std::string_view names;",
        "};",
        "",
        f"constexpr std::array<NameRun, {len(runs)}> kNameRuns{{{{",
    ]
    for first, names in runs:
        lines = wrapped(names)
        out.append(f"    {{.first = {first:#x},")
        out.append(f'     .names = "{lines[0]}"')
        for more in lines[1:]:
            out.append(f'              " {more}"')
        out[-1] += "},"
    out += ["}};", "", "}", ""]
    return "\n".join(out)


def main():
    here = pathlib.Path(__file__).resolve()
    parser = argparse.ArgumentParser(
        description="Write the AFP builtin name table out of afp-core.dll")
    parser.add_argument("--dll", required=True, type=pathlib.Path)
    parser.add_argument("--names", required=True, type=lambda t: int(t, 0))
    parser.add_argument("--index", required=True, type=lambda t: int(t, 0))
    parser.add_argument("--blocks", required=True, type=lambda t: int(t, 0))
    parser.add_argument(
        "--out", type=pathlib.Path,
        default=here.parents[2] / "src" / "formats" / "afp_script_names_data.h")
    args = parser.parse_args()

    found = read_table(Image(args.dll), args.blocks, args.index, args.names)
    found = {i: n for i, n in found.items() if n}
    if not found:
        print("No names read: check --blocks against the block table in the lookup function.")
        return 1
    if not check(found):
        return 1
    runs = runs_of(found)
    args.out.write_text(written(runs), encoding="utf-8", newline="\n")
    print(f"{len(found)} names in {len(runs)} runs -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
