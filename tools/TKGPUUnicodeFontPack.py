"""Convert user-supplied Unicode HEX bitmap glyphs into TKGPU's indexed pack.

Accepts 8x16 and 16x16 records (e.g. a compatible Unifont HEX source).
No font data is distributed by this tool. Respect the source font's license.
"""
import argparse
import struct
from pathlib import Path


def build(source: Path, destination: Path) -> int:
    glyphs = {}
    with source.open(encoding="ascii") as stream:
        for line in stream:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            code, data = line.split(":", 1)
            cp = int(code, 16)
            if not 0 <= cp <= 0x10FFFF or 0xD800 <= cp <= 0xDFFF:
                continue
            raw = bytes.fromhex(data)
            if len(raw) == 16:
                width = 8
                raw = b"".join(bytes((row, 0)) for row in raw)
            elif len(raw) == 32:
                width = 16
            else:
                continue
            glyphs[cp] = (width, raw)
    if not glyphs:
        raise ValueError("No supported glyphs found")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("wb") as output:
        output.write(struct.pack("<8sI", b"TGFONT1\x00", len(glyphs)))
        for cp, (width, raw) in sorted(glyphs.items()):
            output.write(struct.pack("<IB32s", cp, width, raw))
    return len(glyphs)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    print(f"Packed {build(args.source, args.destination)} glyphs")
