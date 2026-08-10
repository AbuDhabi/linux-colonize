#!/usr/bin/env python3
"""Rebuild VR_1816.EXE from stock VICEROY.EXE.

Hang at FUN_4d56_1816 entry (Indian nation turn) to prove whether DOS ever
enters it and, if so, capture the far return CS:IP + indian_slot.

File offset 0x485f6 = 4d56:1816 prologue in this VICEROY.EXE build
(ENTER 0xc; …; ADD AX,4; MOV [5394],AX). Overlay caution: verify stub bytes
in live Memory after load before treating a no-hang as “unreached”.

Scratch (game DS) after hang:
  DS:7000  return IP
  DS:7002  return CS
  DS:7004  param_1 (indian slot 0..7)

Also readable on stack: [BP+2]=IP, [BP+4]=CS, [BP+6]=param (ENTER ran).

See tools/brave_dump/vr_1816.md.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLONIZE = ROOT / "COLONIZE"
VICEROY = COLONIZE / "VICEROY.EXE"

# FUN_4d56_1816 — matched to Ghidra 4d56:1816 via ENTER/PUSH SI / ADD AX,4; MOV [5394]
OFF_1816 = 0x485F6

STOCK_PROLOGUE = bytes.fromhex(
    "c80c000056c746f80000ff36a6839aca041f1883c4028b46"
)  # 24 bytes (file CALLF seg 181f before reloc; Ghidra shows 281f)

# ENTER 0xc,0 ; log [BP+2]/[BP+4]/[BP+6] → DS:7000..7004 ; hang
STUB = bytes.fromhex(
    "c80c0000"  # ENTER 0xc,0
    "8b4602"  # MOV AX,[BP+2]  ret IP
    "a30070"  # MOV [0x7000],AX
    "8b4604"  # MOV AX,[BP+4]  ret CS
    "a30270"  # MOV [0x7002],AX
    "8b4606"  # MOV AX,[BP+6]  indian slot
    "a30470"  # MOV [0x7004],AX
    "ebfe"  # JMP $
)


def build() -> Path:
    raw = bytearray(VICEROY.read_bytes())
    got = bytes(raw[OFF_1816 : OFF_1816 + len(STOCK_PROLOGUE)])
    if got != STOCK_PROLOGUE:
        raise SystemExit(
            f"stock 1816 mismatch at {OFF_1816:#x}: got {got.hex()} "
            f"want {STOCK_PROLOGUE.hex()}"
        )
    if len(STUB) != len(STOCK_PROLOGUE):
        raise SystemExit(f"stub len {len(STUB)} != stock {len(STOCK_PROLOGUE)}")
    raw[OFF_1816 : OFF_1816 + len(STUB)] = STUB
    out = COLONIZE / "VR_1816.EXE"
    out.write_bytes(raw)
    return out


def verify(path: Path) -> None:
    b = path.read_bytes()
    assert b[OFF_1816 : OFF_1816 + len(STUB)] == STUB, path.name
    # Neighbor markers still intact (14fe / body after stub)
    assert b[OFF_1816 - 0x318 : OFF_1816 - 0x318 + 4] == bytes.fromhex(
        "c8020000"
    ), "14fe ENTER vanished"
    assert b[OFF_1816 + 0x324 : OFF_1816 + 0x328] == bytes.fromhex(
        "c8120000"
    ), "1b3a ENTER vanished"
    print(
        f"OK {path.name}: 1816@{OFF_1816:#x} stub={STUB.hex()} "
        f"({len(STUB)} B hang+log DS:7000)"
    )


def main() -> int:
    if not VICEROY.is_file():
        print(f"missing {VICEROY}", file=sys.stderr)
        return 1
    if len(STUB) != 24:
        print(f"stub must be 24 bytes, got {len(STUB)}", file=sys.stderr)
        return 1
    verify(build())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
