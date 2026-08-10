#!/usr/bin/env python3
"""Parse dosbox-x-dumps/dump_1930 (and later) for VR_1930 thunk hang."""
from __future__ import annotations

import struct
import sys
import zipfile
from pathlib import Path

HDR = 8
DS = SS = 0x2385


def main(path: Path) -> None:
    z = zipfile.ZipFile(path)
    mem = z.read("Memory")
    cpu = z.read("CPU")
    print("Program", z.read("Program_Name"))
    print("Remark", z.read("Save_Remark"))
    names = ["AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI", "IP"]
    regs = {n: struct.unpack_from("<I", cpu, i * 4)[0] & 0xFFFF for i, n in enumerate(names)}
    print("regs", {k: f"{v:04x}" for k, v in regs.items()})

    site = 0x22858  # dump_1816 live thunk; same slot expected
    live = mem[site : site + 10]
    print(f"thunk slot @{site:#x}: {live.hex()}")
    if live[:2] == bytes.fromhex("ebfe"):
        print("  hang opcode OK at +0")
    elif live[:4] == bytes.fromhex("558bec0e07"):
        print("  FAIL: v2 stub — reloc ate EB FE at +3 (got PUSH CS; POP ES)")
    elif live[:3] == bytes.fromhex("558bec"):
        print("  WARN: PUSH BP stub variant", live.hex())
    else:
        print("  unexpected thunk bytes")

    sp = regs["SP"]
    base = HDR + SS * 16
    rip, rcs = struct.unpack_from("<HH", mem, base + sp)
    print(f"SS:SP={sp:04x} far ret (if hung at entry EB FE) = {rcs:04x}:{rip:04x}")

    bp = regs["BP"]
    if 0x100 <= bp <= 0xF000:
        old, ip2, cs2 = struct.unpack_from("<HHH", mem, base + bp)
        print(f"SS:BP={bp:04x} old={old:04x} ret={cs2:04x}:{ip2:04x}")

    # EB FE near thunk bank
    for j in range(site - 0x40, site + 0x80):
        if mem[j : j + 2] == bytes.fromhex("ebfe"):
            print(f"EB FE @{j:#x} (delta {j - site:+d} from slot)")


if __name__ == "__main__":
    main(Path(sys.argv[1] if len(sys.argv) > 1 else "dosbox-x-dumps/dump_1930"))
