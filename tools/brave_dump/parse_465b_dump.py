#!/usr/bin/env python3
"""Parse a mid-turn 465b hang save (dump_b465 / dump_b465n / dump_b465s)."""
from __future__ import annotations

import struct
import sys
import zipfile
from pathlib import Path

HDR = 8
DS = 0x2385
# CPU_Regs: 8 regs + ip as uint32, then Bitu flags (uintptr = 8 on linux x64)
REG_NAMES = ["AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI", "IP"]

INTEREST = {(49, 40), (45, 52), (49, 39), (46, 53), (47, 15), (47, 16), (54, 38)}


def parse_cpu_regs(cpu: bytes) -> dict[str, int]:
    regs = {}
    for i, n in enumerate(REG_NAMES):
        regs[n] = struct.unpack_from("<I", cpu, i * 4)[0] & 0xFFFF
    return regs


def main(path: Path) -> None:
    z = zipfile.ZipFile(path) if path.is_file() else None
    if z is None:
        # Unzipped dump dir
        mem = (path / "Memory").read_bytes()
        cpu = (path / "CPU").read_bytes()
        print("Program", (path / "Program_Name").read_bytes())
        print("Remark", (path / "Save_Remark").read_bytes())
    else:
        print("Program", z.read("Program_Name"))
        print("Remark", z.read("Save_Remark"))
        mem = z.read("Memory")
        cpu = z.read("CPU")

    regs = parse_cpu_regs(cpu)
    ax = regs["AX"]
    print("regs", {k: f"{v:04x}" for k, v in regs.items()})
    print(f"AH(cost if B465S hang)={ax >> 8:02x} AL={ax & 0xff:02x}")

    def ds(off: int, n: int = 1) -> bytes:
        return mem[HDR + DS * 16 + off : HDR + DS * 16 + off + n]

    bx = regs["BX"]
    bp = regs["BP"]
    if bx < 0x8000 and (bx % 0x1C) == 0:
        x = ds(bx + 0x3144, 1)[0]
        y = ds(bx + 0x3145, 1)[0]
        typ = ds(bx + 0x3146, 1)[0]
        nat = ds(bx + 0x3147, 1)[0] & 0x0F
        spent = ds(bx + 0x3149, 1)[0]
        print(f"unit BX={bx:04x} idx={bx // 0x1C} n={nat} t={typ} ({x},{y}) spent={spent}")
    else:
        print(f"BX={bx:04x} not a unit frame (post-turn / idle)")

    for label, seg_base in [("DS", DS << 4)]:
        for delta, name in [(0x3E, "local_40/BP-3E"), (0x2A, "cost/BP-2A")]:
            addr = HDR + seg_base + ((bp - delta) & 0xFFFF)
            if 0 <= addr < len(mem):
                print(f"try {name} via {label} @ {addr:x} = {mem[addr]}")

    scratch = ds(0x7000, 4)
    print(f"DS:7000 scratch {scratch.hex()} BX_word={int.from_bytes(scratch[0:2],'little'):04x} "
          f"AL_byte={scratch[2] if len(scratch)>2 else 0:02x}")

    for pat, name in [
        (bytes.fromhex("ebfe4931"), "ADD-replaced hang"),
        (bytes.fromhex("c3ebfe"), "stub hang (RET;EB FE)"),
        (bytes.fromhex("74fec3"), "JZ-self hang + RET"),
        (bytes.fromhex("81fbf801"), "CMP BX,1F8 Sioux"),
        (bytes.fromhex("81fba401"), "CMP BX,1A4 Apache"),
        (bytes.fromhex("eb0f00874931eb02"), "force stub prologue"),
        (bytes.fromhex("00874931eb02909081fbf80174fec3"), "force stub R"),
        (bytes.fromhex("00874931eb02909081fba40174fec3"), "force stub A"),
        (bytes.fromhex("891e0070a20270"), "logger L DS:7000"),
        (bytes.fromhex("7d0f81fbf80174fe"), "F force-max probe hang"),
        (bytes.fromhex("6b5e061c81fbf80174fe5e5fc9cb"), "X exit stub at 0c1e"),
        (bytes.fromhex("e9020090"), "X exit JMP +2"),
        (bytes.fromhex("6bde1c81fbf80174fe5ec9cb"), "E 0934/155e hang Sioux"),
        (bytes.fromhex("e8490090"), "ADD1 CALL force stub"),
        (bytes.fromhex("e8faf990"), "ADD1 CALL cave (bad)"),
        (bytes.fromhex("00874931"), "live ADD1 stock"),
        (bytes.fromhex("88c400874931"), "B465S stub ADD"),
        (bytes.fromhex("8a876a39"), "reloc-corrupted nation load"),
        (bytes.fromhex("00874931"), "live ADD"),
        (bytes.fromhex("e87c1190"), "B465S CALL at ADD2"),
    ]:
        hits = []
        start = 0
        while True:
            i = mem.find(pat, start)
            if i < 0:
                break
            hits.append(i)
            start = i + 1
        print(name, len(hits), [hex(h) for h in hits[:4]])

    uc = int.from_bytes(ds(0x539c, 2), "little")
    print("unit_count", uc)
    for i in range(uc):
        base = 0x3144 + i * 0x1C
        ux, uy = ds(base, 1)[0], ds(base + 1, 1)[0]
        if (ux, uy) in INTEREST or (ds(base + 3, 1)[0] & 0x0F) >= 4 and ds(base + 2, 1)[0] == 19:
            if (ux, uy) in INTEREST:
                print(
                    f"  [{i}] n={ds(base+3,1)[0]&0xf} t={ds(base+2,1)[0]} ({ux},{uy}) "
                    f"spent={ds(base+5,1)[0]}"
                )


if __name__ == "__main__":
    main(Path(sys.argv[1] if len(sys.argv) > 1 else "dosbox-x-dumps/dump_b465"))
