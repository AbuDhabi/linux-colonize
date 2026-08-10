#!/usr/bin/env python3
"""Parse dosbox-x-dumps/dump_1816 (VR_1816 hang at FUN_4d56_1816)."""
from __future__ import annotations

import struct
import sys
import zipfile
from pathlib import Path

HDR = 8
DS = SS = 0x2385
STUB = bytes.fromhex("c80c00008b4602a300708b4604a302708b4606a30470ebfe")


def main(path: Path) -> None:
    z = zipfile.ZipFile(path)
    mem = z.read("Memory")
    cpu = z.read("CPU")
    print("Program", z.read("Program_Name"))
    print("Remark", z.read("Save_Remark"))

    # DOSBox-X: AX,CX,DX,BX,SP,BP,SI,DI,IP as uint32
    names = ["AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI", "IP"]
    regs = {n: struct.unpack_from("<I", cpu, i * 4)[0] & 0xFFFF for i, n in enumerate(names)}
    print("regs", {k: f"{v:04x}" for k, v in regs.items()})

    def ds(off: int, n: int = 2) -> bytes:
        return mem[HDR + DS * 16 + off : HDR + DS * 16 + off + n]

    sp, bp_reg = regs["SP"], regs["BP"]
    # ENTER 0xc ⇒ true BP = SP+0xc even if IRQ clobbered BP reg
    bp = (sp + 0xC) & 0xFFFF
    old, rip, rcs, param = struct.unpack_from("<HHHH", mem, HDR + SS * 16 + bp)
    print(f"frame SS:BP={bp:04x} (SP+0xc; BP_reg={bp_reg:04x}) "
          f"oldBP={old:04x} ret={rcs:04x}:{rip:04x} param={param}")

    scratch = struct.unpack_from("<HHH", ds(0x7000, 6))
    print(f"DS:7000 scratch ret={scratch[1]:04x}:{scratch[0]:04x} slot_word={scratch[2]:04x} "
          f"(slot log may be reloc-trashed)")

    # Live stub / overlay CS
    stub_at = mem.find(bytes.fromhex("c80c00008b4602a30070"))
    if stub_at >= 0:
        live = mem[stub_at : stub_at + 24]
        phys = stub_at - HDR
        cs = (phys - 0x1816) // 16
        print(f"live stub @{stub_at:#x} CS={cs:04x}:1816 bytes={live.hex()}")
        if live != STUB:
            print("  note: reloc flipped bytes vs file stub (param MOV often hit)")

    nation = struct.unpack_from("<H", ds(0x5394))[0]
    uc = struct.unpack_from("<H", ds(0x539c))[0]
    print(f"DS:5394 nation={nation} (pre-body; hang before param+4 write) units={uc}")

    # Static overlay vector CALLF *1816*
    i = 0
    while True:
        j = mem.find(bytes.fromhex("9a1618"), i)
        if j < 0:
            break
        seg = struct.unpack_from("<H", mem, j + 3)[0]
        print(f"CALLF *1816* @{j:#x} -> {seg:04x}:1816")
        i = j + 1

    rv = mem.find(b"Return Vector")
    if rv >= 0:
        print(f"\"Return Vector\" string @{rv:#x} (overlay manager; ret IP in same CS)")


if __name__ == "__main__":
    main(Path(sys.argv[1] if len(sys.argv) > 1 else "dosbox-x-dumps/dump_1816"))
