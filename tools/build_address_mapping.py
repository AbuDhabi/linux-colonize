#!/usr/bin/env python3
"""Build the FUN_<seg>_<off> (canonical, viceroy_unpacked*.c) <-> overlay
addressing (viceroy_overlays.c / OverlayTest project) lookup table.

Ties together three things captured earlier in this investigation
(see docs/rtlink_decode_v2_gap.md):

1. tools/viceroy_v2_output_layout.json — per-segment file-offset layout of
   the flattened VICEROY_OUT_2.EXE (original codeOffset/codeSize/loadSegment
   from rtlink_decode's own bookkeeping, plus where each segment landed in
   the flattened output file). Static, specific to VICEROY.EXE; captured via
   a one-off debug build of rtlink_decode, not re-derived here.
2. A CSV dump of every function in the canonical Ghidra project
   (decompiled-colonize / VICEROY_OUT_2.EXE) with its file-byte-offset —
   produced by tools/DumpCanonicalFuncs.java (uses Ghidra's own
   MemoryBlockSourceInfo, so no guessing about Ghidra's addressing).
3. A CSV dump of every function in the OverlayTest project — produced by
   tools/DumpOverlayFuncs.java.

For each canonical function: its file-byte-offset in VICEROY_OUT_2.EXE
locates it in exactly one of the 31 original overlay segments, or the
resident/data region (same file, since rtlink_decode copies segment bytes
1:1, just relocated to a new file position — see docs/rtlink_decode_v2_gap.md
"processExecutable"). That gives a target address in the OverlayTest
project's addressing (resident space is based at runtime 0; each overlay
space is based at its own loadSegment<<4). Look that address up against the
OverlayTest function list to get the corresponding overlay-side name.

Usage:
    python3 tools/build_address_mapping.py \
        tools/viceroy_v2_output_layout.json \
        <canonical_funcs.csv> <overlay_funcs.csv> \
        <output.csv>
"""
import csv
import json
import sys
from bisect import bisect_right


def load_layout(path):
    with open(path) as f:
        data = json.load(f)
    g = data["global"]
    segs = sorted(data["segments"], key=lambda s: s["outputCodeOffset"])
    return g, segs


def locate_in_original(file_offset, global_layout, segs):
    """file_offset is a byte offset within the flattened VICEROY_OUT_2.EXE.
    Returns (region, local_offset) where region is 'resident' or an overlay
    segment dict, local_offset is the offset from that region's own start."""
    resident_start = global_layout["outputCodeOffset"]
    # Resident/data region runs from outputCodeOffset up to the first
    # segment's outputCodeOffset (segs is sorted by outputCodeOffset).
    resident_end = segs[0]["outputCodeOffset"] if segs else None
    if resident_end is not None and resident_start <= file_offset < resident_end:
        return ("resident", file_offset - resident_start)

    for seg in segs:
        start = seg["outputCodeOffset"]
        end = start + seg["codeSize"]
        if start <= file_offset < end:
            return (seg, file_offset - start)

    return (None, None)


def load_overlay_index(path):
    """Returns {space_name: sorted list of (start_offset, end_offset, name)}"""
    index = {}
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            space = row["space"]
            off = int(row["offset_hex"], 16)
            length = int(row["body_length"])
            index.setdefault(space, []).append((off, off + length, row["name"]))
    for space in index:
        index[space].sort()
    return index


def find_overlay_function(space, offset, index):
    entries = index.get(space)
    if not entries:
        return None, "no-such-space"
    starts = [e[0] for e in entries]
    i = bisect_right(starts, offset) - 1
    if i < 0:
        return None, "before-first-function"
    start, end, name = entries[i]
    if start == offset:
        return name, "exact"
    if start < offset < end:
        return name, "contained"
    return None, "gap"


def main():
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(1)

    layout_path, canonical_csv, overlay_csv, out_csv = sys.argv[1:5]
    global_layout, segs = load_layout(layout_path)
    overlay_index = load_overlay_index(overlay_csv)

    rows_out = []
    stats = {"exact": 0, "contained": 0, "gap": 0, "before-first-function": 0,
             "no-such-space": 0, "unmapped-region": 0}

    with open(canonical_csv) as f:
        reader = csv.DictReader(f)
        for row in reader:
            canon_name = row["name"]
            canon_addr = row["address"]
            file_offset = int(row["file_offset_hex"], 16)

            region, local_offset = locate_in_original(file_offset, global_layout, segs)
            if region is None:
                stats["unmapped-region"] += 1
                rows_out.append([canon_name, canon_addr, "", "", "", "unmapped-region"])
                continue

            if region == "resident":
                space = "ram"
                target_offset = local_offset
            else:
                space = f"OVL{region['idx'] + 2:02d}_L{region['loadSegment']:04x}"
                target_offset = region["loadSegment"] * 16 + local_offset

            ovl_name, match_kind = find_overlay_function(space, target_offset, overlay_index)
            stats[match_kind] = stats.get(match_kind, 0) + 1
            rows_out.append([canon_name, canon_addr, space,
                              format(target_offset, "x"), ovl_name or "", match_kind])

    with open(out_csv, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["canonical_name", "canonical_address", "overlay_space",
                     "overlay_offset_hex", "overlay_name", "match_kind"])
        w.writerows(rows_out)

    print(f"Wrote {len(rows_out)} rows to {out_csv}")
    print("Match breakdown:", stats)


if __name__ == "__main__":
    main()
