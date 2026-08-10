#!/usr/bin/env python3
"""Parse dosbox-x-dumps/vr_2a02 (VR_2A02 hang at Return Vector forge)."""
from __future__ import annotations

import struct
import sys
import zipfile
from pathlib import Path

HDR = 8
GAME_DS = 0x2385
CS1930 = 0x1930


def _seg(cpu: bytes, off: int) -> int:
    return struct.unpack_from("<H", cpu, off + 3)[0]


def main(path: Path) -> None:
    z = zipfile.ZipFile(path)
    mem = z.read("Memory")
    cpu = z.read("CPU")
    print("Program", z.read("Program_Name"))
    print("Remark", z.read("Save_Remark"))

    names = ["AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI", "IP"]
    regs = {n: struct.unpack_from("<I", cpu, i * 4)[0] & 0xFFFF for i, n in enumerate(names)}
    es, cs, ss, ds = _seg(cpu, 0xE8), _seg(cpu, 0xF0), _seg(cpu, 0xF8), _seg(cpu, 0x100)
    print("regs", {k: f"{v:04x}" for k, v in regs.items()})
    print(f"segs ES={es:04x} CS={cs:04x} SS={ss:04x} DS={ds:04x}")

    b1930 = HDR + CS1930 * 16
    print(f"1930:2A4D (hook) {mem[b1930 + 0x2A4D : b1930 + 0x2A50].hex()}")
    cave = mem[b1930 + 0x294D : b1930 + 0x294D + 0x30]
    print(f"1930:294D cave head {cave[:16].hex()} … hang? {cave[-2:]==bytes.fromhex('ebfe') or bytes.fromhex('ebfe') in cave}")
    if bytes.fromhex("ebfe") in cave:
        print(f"  EB FE at cave+{cave.find(bytes.fromhex('ebfe')):#x}")

    # Scratch log — v4+ uses CS:29C0 (v3 wrote 2385:7000, often zero/EMS)
    b1930 = HDR + CS1930 * 16
    ip, cseg, bx = struct.unpack_from("<HHH", mem, b1930 + 0x29C0)
    print(
        f"CS:29C0 log ret={cseg:04x}:{ip:04x} BX={bx:04x} "
        f"(BX&3FFF={(bx & 0x3FFF):04x})"
    )
    for label, dseg in (("game", GAME_DS), ("cpu", ds)):
        base = HDR + dseg * 16
        ip, cseg, bx = struct.unpack_from("<HHH", mem, base + 0x7000)
        print(
            f"DS:{dseg:04x} @7000 ret={cseg:04x}:{ip:04x} BX={bx:04x} "
            f"(BX&3FFF={(bx & 0x3FFF):04x})"
        )

    gbase = HDR + GAME_DS * 16
    nation = struct.unpack_from("<H", mem, gbase + 0x5394)[0]
    units = struct.unpack_from("<H", mem, gbase + 0x539c)[0]
    print(f"game DS={GAME_DS:04x} nation@5394={nation} units@539c={units}")

    # If hang CS:IP is 1930:xxxx on EB FE
    site = HDR + cs * 16 + regs["IP"]
    if 0 <= site < len(mem) - 2:
        print(
            f"saved CS:IP {cs:04x}:{regs['IP']:04x} bytes={mem[site:site+4].hex()} "
            f"hang={mem[site:site+2]==bytes.fromhex('ebfe')}"
        )

    # Optional: SI still points at forged slot — print [SS:SI] if SS looks game-like
    for stack_ss in (GAME_DS, ss):
        if regs["SI"] == 0:
            continue
        sbase = HDR + stack_ss * 16 + regs["SI"]
        if 0 <= sbase < len(mem) - 4:
            ip, cseg = struct.unpack_from("<HH", mem, sbase)
            print(f"[SS={stack_ss:04x}:SI={regs['SI']:04x}] -> {cseg:04x}:{ip:04x}")


if __name__ == "__main__":
    main(Path(sys.argv[1] if len(sys.argv) > 1 else "dosbox-x-dumps/vr_2a02"))
