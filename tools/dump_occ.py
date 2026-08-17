#!/usr/bin/env python3
"""
Dump colony details, occupations, professions, and stocks directly from raw DOS saves.
Usage: python3 tools/dump_occ.py <path/to/save.SAV> [filter_name]
"""
import sys
import struct

CARGO_NAMES = [
    "Food", "Sugar", "Tobacco", "Cotton", "Furs", "Lumber", "Ore", "Silver",
    "Horses", "Rum", "Cigars", "Cloth", "Coats", "TradeGoods", "Tools", "Muskets"
]

def dump_save(path, filter_name=None):
    with open(path, "rb") as f:
        data = f.read()

    # Find colonies (colony structure starts with 2 bytes x,y before 24-byte name)
    colony_count = struct.unpack_from("<H", data, 186)[0]
    print(f"File: {path}, Colony count header: {colony_count}")

    pos = 0  # scan entire save
    found = 0
    while pos < len(data) - 200:
        # Colony candidate: check valid name ASCII chars
        name_raw = data[pos+2:pos+26]
        if name_raw[0] >= ord('A') and name_raw[0] <= ord('Z'):
            null_pos = name_raw.find(b'\x00')
            if null_pos > 0:
                try:
                    name = name_raw[:null_pos].decode('ascii')
                    if all(32 <= ord(ch) <= 126 for ch in name):
                        if filter_name is None or filter_name.lower() in name.lower():
                            x, y = data[pos], data[pos+1]
                            nation = data[pos+26]
                            pop = data[pos+31]
                            print(f"\nColony: '{name}' at ({x}, {y}) - Nation {nation}, Pop {pop}")
                            occs = list(data[pos+32:pos+32+pop])
                            profs = list(data[pos+48:pos+48+pop])
                            for p in range(pop):
                                print(f"  Worker {p:2d}: occ={occs[p]:3d} (0x{occs[p]:02x})  prof={profs[p]:3d} (0x{profs[p]:02x})")
                            stocks = struct.unpack_from("<16H", data, pos+124)
                            stock_items = [f"{CARGO_NAMES[i]}:{stocks[i]}" for i in range(16) if stocks[i] > 0]
                            if stock_items:
                                print(f"  Stock: {', '.join(stock_items)}")
                            found += 1
                        pos += 200
                        continue
                except Exception:
                    pass
        pos += 1

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 tools/dump_occ.py <path/to/save.SAV> [filter_name]")
        sys.exit(1)
    dump_save(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
