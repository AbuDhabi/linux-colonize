#!/usr/bin/env python3
"""Rebuild VR_0E52.EXE — hang after overlay loader returns to 1816 thunk.

Loader hooks failed:
  v1  0E52 clobbered POP displacement → freeze
  v2  near-JMP cave 3B76 → exit 19 (evm0015 Virtual Page)
  v3  string-hole stub + ES:[ret] peek → exit 20 (evm0020 EMS mapping)

v4: do not patch CS1930. Hang at the resident thunk's JMPF site (file
0x1C9A0+5) so the loader has already RETF'd; [SS:SP] is the outer far
caller. No EMS far-peek, no distant cave.

  CALLF  loader     ; stock (live offset 0E52)
  EB FE             ; was JMPF 4d56:1816 — hang here after loader

See tools/brave_dump/vr_0e52.md.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLONIZE = ROOT / "COLONIZE"
VICEROY = COLONIZE / "VICEROY.EXE"

OFF_THUNK = 0x1C9A0
STOCK = bytes.fromhex("9aab0d0d11ea16180000")
# Keep CALLF; hang at JMPF opcode (reloc still patches +8..+9, never fetched)
STUB = STOCK[:5] + bytes.fromhex("ebfe") + STOCK[7:]


def build() -> Path:
    raw = bytearray(VICEROY.read_bytes())
    got = bytes(raw[OFF_THUNK : OFF_THUNK + len(STOCK)])
    if got != STOCK:
        raise SystemExit(
            f"stock thunk mismatch at {OFF_THUNK:#x}: got {got.hex()} "
            f"want {STOCK.hex()}"
        )
    raw[OFF_THUNK : OFF_THUNK + len(STUB)] = STUB
    out = COLONIZE / "VR_0E52.EXE"
    out.write_bytes(raw)
    return out


def verify(path: Path) -> None:
    b = path.read_bytes()
    assert b[OFF_THUNK : OFF_THUNK + len(STUB)] == STUB
    # Loader fall-through untouched
    base = 0x13450
    assert b[base + 0x0E54 : base + 0x0E59] == bytes.fromhex("2e89268339")
    assert b[base + 0x0E4F : base + 0x0E54] == bytes.fromhex("2e8f067f39")
    print(
        f"OK {path.name}: thunk@{OFF_THUNK:#x}={STUB.hex()} "
        f"(CALLF loader + hang at JMPF; read outer at [SS:SP])"
    )


def main() -> int:
    if not VICEROY.is_file():
        print(f"missing {VICEROY}", file=sys.stderr)
        return 1
    verify(build())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
