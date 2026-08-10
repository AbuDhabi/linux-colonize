#!/usr/bin/env python3
"""Parse dosbox-x-dumps/vr_0e52 (VR_0E52 hang after loader @ JMPF)."""
from __future__ import annotations

import struct
import sys
import zipfile
from pathlib import Path

HDR = 8
GAME_DS = 0x2385
THUNK_FILE = 0x22858  # Memory blob offset of CALLF (no HDR add)


def _seg(cpu: bytes, off: int) -> int:
    """DOSBox-X CPU save: segment word at record+3 within 8-byte slot."""
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

    live = mem[THUNK_FILE : THUNK_FILE + 10]
    print(f"thunk @{THUNK_FILE:#x}: {live.hex()}")
    if live[5:7] == bytes.fromhex("ebfe"):
        print("  hang at JMPF site OK (after loader)")
    else:
        print("  unexpected thunk")

    # Game DS (nation) — CPU DS may be A000 during IRQ
    gbase = HDR + GAME_DS * 16
    nation, units = struct.unpack_from("<HH", mem, gbase + 0x5394)
    print(f"game DS={GAME_DS:04x} nation@5394={nation} units@539c={units}")

    hang_phys = THUNK_FILE - HDR + 5  # DOS linear of EB FE if THUNK_FILE includes HDR? 
    # Memory[0x22858] is CALLF; dumps index without adding HDR for this site.
    # DOS linear of EB FE = 0x22855 when HDR=8 and file off 0x2285d — keep aliases:
    hang_aliases = []
    dos_ebfe = 0x22855
    for c in (0x2042, 0x224A, 0x1451):
        ip = dos_ebfe - c * 16
        if 0 <= ip < 0x10000:
            hang_aliases.append((c, ip))
            print(f"  hang alias {c:04x}:{ip:04x} bytes={mem[HDR+c*16+ip:HDR+c*16+ip+2].hex()}")

    # Prefer game stack SS=2385 high: look for Return Vector / outer under IRQ
    for stack_ss in (GAME_DS, ss):
        sbase = HDR + stack_ss * 16
        print(f"--- stack SS={stack_ss:04x} ---")
        # Scan e8a0..e900 for 1930:1554
        for sp in range(0xE8A0, 0xE900, 2):
            ip, cseg = struct.unpack_from("<HH", mem, sbase + sp)
            if cseg == 0x1930 and ip == 0x1554:
                print(f"  {stack_ss:04x}:{sp:04x} -> 1930:1554 (Return Vector)")
                above = [f"{struct.unpack_from('<H', mem, sbase + sp - i)[0]:04x}" for i in (6, 4, 2)]
                print(f"    words above (IRQ?): {above}")
            if (cseg, ip) in hang_aliases:
                print(f"  {stack_ss:04x}:{sp:04x} -> hang alias {cseg:04x}:{ip:04x}")

    # Saved CS:IP often IRQ — note if not on hang
    ip = regs["IP"]
    site = HDR + cs * 16 + ip
    if 0 <= site < len(mem) - 2:
        print(f"saved CS:IP bytes {mem[site:site+6].hex()} (hang? {mem[site:site+2]==bytes.fromhex('ebfe')})")

    b1930 = HDR + 0x1930 * 16
    print(f"1930:0E54 (stock MOV?) {mem[b1930+0x0E54:b1930+0x0E59].hex()}")
    print(f"1930:1554 {mem[b1930+0x1554:b1930+0x1560].hex()}")
    rv = mem.find(b"Return Vector")
    if rv >= 0:
        print(f"\"Return Vector\" @{rv:#x} (~1930:{rv - b1930:04x})")


if __name__ == "__main__":
    main(Path(sys.argv[1] if len(sys.argv) > 1 else "dosbox-x-dumps/vr_0e52"))
