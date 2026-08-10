#!/usr/bin/env python3
"""Rebuild VR_2A02.EXE — hang on overlay-0C forge with *external* far ret.

Hang edge: `1930:2A4D` → same-page cave `294D`.

History:
  v1  {0B,0C,0D} → load hang on BX=800B
  v2  0C && nation==3 → no hang
  v3  0C only → AI-turn hang; [SS:SI]=1930:238b (overlay-manager resume,
      not year-loop). Game DS:7000 log was zero (EMS); use CS scratch.
  v4  0C && [SS:SI+2] != 1930 — skip manager-internal resumes.

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
STOCK_FORGE = bytes.fromhex("b85415")
# CS-relative scratch past stub (still in diagnostic string)
LOG_IP = 0x29C0  # CS:29C0 ret IP, +2 CS, +4 BX


def _assemble_cave_v4() -> bytes:
    """Log; hang only if BX&3FFF==0C and [SS:SI+2]!=1930."""
    parts: list[bytes] = []
    parts.append(bytes.fromhex("368b04"))
    parts.append(b"\x2e\xa3" + struct.pack("<H", LOG_IP))
    parts.append(bytes.fromhex("368b4402"))
    parts.append(b"\x2e\xa3" + struct.pack("<H", LOG_IP + 2))
    parts.append(bytes.fromhex("8bc3"))
    parts.append(b"\x2e\xa3" + struct.pack("<H", LOG_IP + 4))
    parts.append(bytes.fromhex("25ff3f"))
    parts.append(bytes.fromhex("3d0c00"))
    jnz_miss_at = sum(len(p) for p in parts)
    parts.append(bytes.fromhex("7500"))  # JNZ miss
    parts.append(bytes.fromhex("36817c023019"))  # CMP [SS:SI+2],1930
    jz_miss_at = sum(len(p) for p in parts)
    parts.append(bytes.fromhex("7400"))  # JZ miss (internal)
    hang_at = sum(len(p) for p in parts)
    parts.append(bytes.fromhex("ebfe"))
    miss_at = sum(len(p) for p in parts)
    parts.append(bytes.fromhex("b85415"))
    jmp_back_at = sum(len(p) for p in parts)
    parts.append(bytes.fromhex("e90000"))

    blob = bytearray(b"".join(parts))
    blob[jnz_miss_at + 1] = (miss_at - (jnz_miss_at + 2)) & 0xFF
    blob[jz_miss_at + 1] = (miss_at - (jz_miss_at + 2)) & 0xFF
    after = 0x294D + jmp_back_at + 3
    rel = (0x2A50 - after) & 0xFFFF
    struct.pack_into("<H", blob, jmp_back_at + 1, rel)
    assert blob[hang_at : hang_at + 2] == bytes.fromhex("ebfe")
    assert blob[miss_at : miss_at + 3] == bytes.fromhex("b85415")
    return bytes(blob)

def _assemble_hook() -> bytes:
    after = 0x2A50
    rel = (0x294D - after) & 0xFFFF
    return b"\xe9" + struct.pack("<H", rel)


def build() -> Path:
    raw = bytearray(VICEROY.read_bytes())
    got = bytes(raw[OFF_FORGE : OFF_FORGE + 3])
    if got != STOCK_FORGE:
        raise SystemExit(
            f"stock forge mismatch at {OFF_FORGE:#x}: got {got.hex()} "
            f"want {STOCK_FORGE.hex()}"
        )
    cave = _assemble_cave_v4()
    hook = _assemble_hook()
    raw[OFF_CAVE : OFF_CAVE + len(cave)] = cave
    raw[OFF_FORGE : OFF_FORGE + 3] = hook
    out = COLONIZE / "VR_2A02.EXE"
    out.write_bytes(raw)
    return out


def verify(path: Path) -> None:
    b = path.read_bytes()
    cave = _assemble_cave_v4()
    hook = _assemble_hook()
    assert b[OFF_FORGE : OFF_FORGE + 3] == hook
    assert b[OFF_CAVE : OFF_CAVE + len(cave)] == cave
    print(
        f"OK {path.name}: 2A4D→{hook.hex()} cave@294D len={len(cave)} "
        f"(v4: 0C && ret.CS!=1930; log CS:{LOG_IP:04X})"
    )


def main() -> int:
    if not VICEROY.is_file():
        print(f"missing {VICEROY}", file=sys.stderr)
        return 1
    verify(build())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
