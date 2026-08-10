#!/usr/bin/env python3
"""Rebuild VR_2A02.EXE — hang at 2A4D forge on overlay 0x0C, skip known noise.

Proven mid-turn edge: 2A4D → cave @294D (same page).

v14–v15 exact-IP peels only found more CC81 sites inside 1816 (1829, 183d,
1b87). v13 already showed: skip whole body CS → no hang through a turn.
v16 restores that gate (confirmation / closeout build):

  Hang when (BX&3FFF)==0x0C and far ret is not:
    - 1930:238B           (manager resume)
    - CS in {CC81, CC89}  (1816 overlay bank)

If this never hangs, the year-loop caller is not in the forge slot for 0x0C.
1452 left stock. No DS/SS peeks.

See tools/brave_dump/vr_1554.md.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
COLONIZE = ROOT / "COLONIZE"
VICEROY = COLONIZE / "VICEROY.EXE"

BASE = 0x13450
OFF_FORGE = BASE + 0x2A4D
OFF_CAVE = BASE + 0x294D
OFF_RETF = BASE + 0x1452
STOCK_FORGE = bytes.fromhex("b85415")
STOCK_CAVE_HEAD = b"Smart vectoring failed"


def _assemble_cave() -> bytes:
    parts: list[bytes] = []
    fixups: list[tuple[str, int, str | None]] = []
    labels: dict[str, int] = {}

    def emit(b: bytes) -> None:
        parts.append(b)

    def here() -> int:
        return sum(len(p) for p in parts)

    # (BX & 3FFF) == 0x0C
    emit(bytes.fromhex("8bc3"))
    emit(bytes.fromhex("25ff3f"))
    emit(bytes.fromhex("3d0c00"))
    fixups.append(("jnz_miss", here(), None))
    emit(bytes.fromhex("7500"))

    # skip 1930:238B
    emit(bytes.fromhex("368b04"))  # MOV AX,[SS:SI]
    emit(struct.pack("<BH", 0x3D, 0x238B))
    jnz_not_mgr = here()
    emit(bytes.fromhex("7500"))
    emit(bytes.fromhex("368b4402"))
    emit(struct.pack("<BH", 0x3D, 0x1930))
    fixups.append(("jz_miss", here(), None))
    emit(bytes.fromhex("7400"))
    labels["not_mgr"] = here()
    fixups.append(("jnz", jnz_not_mgr, "not_mgr"))

    # skip CS CC81 / CC89
    emit(bytes.fromhex("368b4402"))  # MOV AX,[SS:SI+2]
    for imm in (0xCC81, 0xCC89):
        emit(struct.pack("<BH", 0x3D, imm))
        fixups.append(("jz_miss", here(), None))
        emit(bytes.fromhex("7400"))

    labels["hang"] = here()
    emit(bytes.fromhex("ebfe"))
    labels["miss"] = here()
    emit(bytes.fromhex("b85415"))
    jmp_back_at = here()
    emit(bytes.fromhex("e90000"))

    blob = bytearray(b"".join(parts))
    for kind, at, target in fixups:
        if kind in ("jnz_miss", "jz_miss"):
            blob[at + 1] = (labels["miss"] - (at + 2)) & 0xFF
        elif kind == "jnz":
            assert target is not None
            blob[at + 1] = (labels[target] - (at + 2)) & 0xFF
    after = 0x294D + jmp_back_at + 3
    struct.pack_into("<H", blob, jmp_back_at + 1, (0x2A50 - after) & 0xFFFF)
    return bytes(blob)


def _assemble_hook() -> bytes:
    after = 0x2A50
    rel = (0x294D - after) & 0xFFFF
    return b"\xe9" + struct.pack("<H", rel)


def build() -> Path:
    raw = bytearray(VICEROY.read_bytes())
    stock = VICEROY.read_bytes()
    raw[OFF_RETF : OFF_RETF + 3] = stock[OFF_RETF : OFF_RETF + 3]
    raw[BASE + 0x15CF : BASE + 0x160C] = stock[BASE + 0x15CF : BASE + 0x160C]
    raw[BASE + 0x1782 : BASE + 0x1782 + 64] = stock[BASE + 0x1782 : BASE + 0x1782 + 64]

    got = bytes(raw[OFF_FORGE : OFF_FORGE + 3])
    if got != STOCK_FORGE:
        raise SystemExit(
            f"stock forge mismatch at {OFF_FORGE:#x}: got {got.hex()} "
            f"want {STOCK_FORGE.hex()}"
        )
    head = bytes(raw[OFF_CAVE : OFF_CAVE + len(STOCK_CAVE_HEAD)])
    if head != STOCK_CAVE_HEAD:
        raise SystemExit(
            f"stock cave head mismatch: got {head!r} want {STOCK_CAVE_HEAD!r}"
        )
    cave = _assemble_cave()
    if len(cave) > 0x80:
        raise SystemExit(f"cave too long: {len(cave)}")
    hook = _assemble_hook()
    raw[OFF_CAVE : OFF_CAVE + len(cave)] = cave
    raw[OFF_FORGE : OFF_FORGE + 3] = hook
    out = COLONIZE / "VR_2A02.EXE"
    out.write_bytes(raw)
    return out


def verify(path: Path) -> None:
    b = path.read_bytes()
    stock = VICEROY.read_bytes()
    cave = _assemble_cave()
    hook = _assemble_hook()
    assert b[OFF_FORGE : OFF_FORGE + 3] == hook
    assert b[OFF_CAVE : OFF_CAVE + len(cave)] == cave
    assert b[OFF_RETF : OFF_RETF + 3] == stock[OFF_RETF : OFF_RETF + 3]
    print(
        f"OK {path.name}: 2A4D→{hook.hex()} cave@294D len={len(cave)} "
        f"(v16: 0C, skip 238B + CS CC81/CC89; RETF stock)"
    )


def main() -> int:
    if not VICEROY.is_file():
        print(f"missing {VICEROY}", file=sys.stderr)
        return 1
    verify(build())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
