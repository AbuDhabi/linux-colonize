#!/usr/bin/env python3
"""Rebuild VR_1930.EXE — hang at overlay thunk entry → FUN_4d56_1816.

dump_1930 (v2 stub): reloc at thunk+3 (CALLF segment slot) overwrote EB FE
with 0E 07 (PUSH CS; POP ES). Stub became fall-through junk — no hang.

v3: place EB FE at bytes 0..1 (opcode / offset area, not reloc seg slots).
Reloc may still patch +3..+4 and +8..+9; those are never executed.

Stock thunk @0x1C9A0: 9A AB 0D | 0D 11 | EA 16 18 | 00 00
                         ^op ^off   ^SEG     ^JMPF      ^SEG

On hang (no PUSH BP): far caller at [SS:SP]=IP, [SS:SP+2]=CS.

See tools/brave_dump/vr_1930.md.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLONIZE = ROOT / "COLONIZE"
VICEROY = COLONIZE / "VICEROY.EXE"

OFF_THUNK = 0x1C9A0
STOCK = bytes.fromhex("9aab0d0d11ea16180000")

# Hang at entry; leave remaining 8 bytes as stock so reloc slots stay "segment-shaped"
# (still patched by reloc, but never fetched as code).
STUB = bytes.fromhex("ebfe") + STOCK[2:]


def build() -> Path:
    raw = bytearray(VICEROY.read_bytes())
    got = bytes(raw[OFF_THUNK : OFF_THUNK + len(STOCK)])
    if got != STOCK:
        raise SystemExit(
            f"stock thunk mismatch at {OFF_THUNK:#x}: got {got.hex()} want {STOCK.hex()}"
        )
    raw[OFF_THUNK : OFF_THUNK + len(STUB)] = STUB
    out = COLONIZE / "VR_1930.EXE"
    out.write_bytes(raw)
    return out


def verify(path: Path) -> None:
    b = path.read_bytes()
    assert b[OFF_THUNK : OFF_THUNK + 2] == bytes.fromhex("ebfe")
    assert b[OFF_THUNK : OFF_THUNK + 10] == STUB
    print(
        f"OK {path.name}: thunk@{OFF_THUNK:#x}={STUB.hex()} "
        f"(EB FE at +0; reloc slots +3/+8 untouched by hang opcode)"
    )


def main() -> int:
    if not VICEROY.is_file():
        print(f"missing {VICEROY}", file=sys.stderr)
        return 1
    verify(build())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
