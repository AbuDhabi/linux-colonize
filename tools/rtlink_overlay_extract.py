#!/usr/bin/env python3
"""Extract each RTLink v2 overlay segment from VICEROY.EXE as its own raw
byte blob, at its own true DOS load address — no flattening, no relocation
math, so nothing here can introduce the kind of corruption tracked in
docs/rtlink_decode_v2_gap.md.

Self-contained reimplementation of the parsing rtlink_decode's
variation2.cpp does (loadSegmentListV2 / findSegmentListV2), so this has no
dependency on that external tool. Only reads VICEROY.EXE; writes nothing
back to it.

Usage:
    python3 tools/rtlink_overlay_extract.py COLONIZE/VICEROY.EXE OUTDIR

Writes OUTDIR/segments.json (manifest: index, loadSegment, codeOffset,
codeSize, isDataSegment) and OUTDIR/seg_<NN>_<loadSegment_hex>.bin per
segment (raw code bytes, exactly as they sit in the file — same bytes
rtlink_decode itself copies per segment).

See docs/rtlink_decode_v2_gap.md for why per-overlay Ghidra import (see
tools/ghidra_import_overlays.py) is preferred over trusting the flattened
VICEROY_OUT*.EXE for any function that still carries a Ghidra WARNING.
"""
import json
import struct
import sys
from pathlib import Path


def read_u16(data, off):
    return struct.unpack_from("<H", data, off)[0]


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def parse_relocations(data, reloc_offset, num_relocations):
    """Each MZ relocation entry is two LE words (offset, segment) read as one
    LE32 by rtlink_decode; getOffset()=value&0xffff, getSegment()=value>>16."""
    entries = []
    for i in range(num_relocations):
        off = reloc_offset + i * 4
        value = read_u32(data, off)
        entries.append((value & 0xFFFF, value >> 16))  # (offset, segment)
    return entries


def find_segment_list_v2(data, code_offset, relocations):
    """Mirrors variation2.cpp::findSegmentListV2: scan relocation entries with
    offset==0 for the RTLink segment list (records of 32 bytes, segment
    numbers 2, 3, ... at +14 within each record)."""
    for (roff, rseg) in relocations:
        if roff != 0:
            continue
        rtlink_start = code_offset + (rseg << 4) + roff
        if rtlink_start + 256 > len(data):
            continue
        window = data[rtlink_start:rtlink_start + 256]
        for delta in range(0, 256 - 32 - 14, 16):
            if len(window) < delta + 32 + 16:
                break
            num1 = read_u16(window, delta + 14)
            num2 = read_u16(window, delta + 32 + 14)
            if num1 == 2 and num2 == 3:
                return rtlink_start + delta
    return None


def load_segment_list_v2(data, code_offset, relocations):
    segments_offset = find_segment_list_v2(data, code_offset, relocations)
    if segments_offset is None:
        raise RuntimeError("Could not find V2 segment list — is this really an RTLink v2 EXE?")

    segs = []
    offset = 0
    segment_num = 2
    while True:
        rec = segments_offset + offset
        if rec + 32 > len(data):
            break
        if read_u16(data, rec + 14) != segment_num:
            break
        seg = {
            "segmentIndex": segment_num,
            "loadSegment": read_u16(data, rec + 0),
            "headerOffset": read_u32(data, rec + 8),
        }
        segs.append(seg)
        segment_num += 1
        offset += 32

    # Per-segment header: segmentParagraphs, headerParagraphs, (unused),
    # relocationStart, numRelocations — then the segment's own relocation
    # list (relocationStart must be 0, per rtlink_decode's assert).
    for seg in segs:
        ho = seg["headerOffset"]
        segment_paragraphs = read_u16(data, ho + 0)
        header_paragraphs = read_u16(data, ho + 2)
        # ho+4: unused word
        relocation_start = read_u16(data, ho + 6)
        num_relocations = read_u16(data, ho + 8)
        assert relocation_start == 0, f"unexpected relocationStart={relocation_start} at seg {seg['segmentIndex']}"

        seg["codeOffset"] = ho + header_paragraphs * 16
        seg["codeSize"] = (segment_paragraphs - header_paragraphs) * 16
        assert seg["codeSize"] % 16 == 0

        rel_base = ho + 10
        seg_relocs = []
        for i in range(num_relocations):
            off_val = read_u16(data, rel_base + i * 4)
            seg_val = read_u16(data, rel_base + i * 4 + 2)
            seg_relocs.append((off_val, seg_val))
        seg["relocations"] = seg_relocs

    return segs


def find_data_segment(data, code_offset, segs):
    """The static/resident region — code AND data both, no separate split in
    the file — is simply everything between the MZ header's own codeOffset
    and the first RTLink segment's header. Standard DOS relocatable-EXE
    convention: this whole span is compiled assuming it's loaded at CS=0000
    in its own frame (DOS relocates it as a unit at load time), so its
    correct Ghidra base is runtime segment 0 — *not* derived from any
    signature scan inside it.

    (Earlier version of this function found only the narrow "MS Run-Time"
    tail of this region — by scanning for that libc-startup-copyright
    string — and used its position as the block's base (0x1b5a for
    VICEROY.EXE). That value is real (it's where rtlink_decode's own
    internal dataSeg.loadSegment lands, and matches the 10 far-pointer
    fixups in docs/rtlink_decode_v2_gap.md), but it's an offset *within*
    this larger region, not the region's own base — using it as the base
    left ~112KB of the actual resident code unextracted, including the
    RTLink thunk-stub table itself, which is why cross-overlay call targets
    were landing on undefined memory. See the "overlay2.c mostly broken"
    finding this was diagnosed from.)"""
    first_header_offset = min(s["headerOffset"] for s in segs)
    return {
        "segmentIndex": 0,
        "loadSegment": 0,
        "codeOffset": code_offset,
        "codeSize": first_header_offset - code_offset,
        "isDataSegment": True,
    }


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} VICEROY.EXE OUTDIR", file=sys.stderr)
        sys.exit(1)

    exe_path = Path(sys.argv[1])
    outdir = Path(sys.argv[2])
    outdir.mkdir(parents=True, exist_ok=True)

    data = exe_path.read_bytes()
    if data[:2] != b"MZ":
        raise RuntimeError("not an MZ executable")

    num_relocations = read_u16(data, 6)
    code_offset = read_u16(data, 8) << 4
    reloc_offset = read_u16(data, 24)
    assert reloc_offset < 0x100 and reloc_offset % 2 == 0

    relocations = parse_relocations(data, reloc_offset, num_relocations)
    segs = load_segment_list_v2(data, code_offset, relocations)
    data_seg = find_data_segment(data, code_offset, segs)

    manifest = []
    for seg in segs:
        blob = data[seg["codeOffset"]: seg["codeOffset"] + seg["codeSize"]]
        fname = f"seg_{seg['segmentIndex']:02d}_load{seg['loadSegment']:04x}.bin"
        (outdir / fname).write_bytes(blob)
        manifest.append({
            "segmentIndex": seg["segmentIndex"],
            "loadSegment": seg["loadSegment"],
            "codeOffset": seg["codeOffset"],
            "codeSize": seg["codeSize"],
            "isDataSegment": False,
            "file": fname,
            "numRelocations": len(seg["relocations"]),
        })

    blob = data[data_seg["codeOffset"]: data_seg["codeOffset"] + data_seg["codeSize"]]
    fname = "seg_data_resident.bin"
    (outdir / fname).write_bytes(blob)
    manifest.append({
        "segmentIndex": data_seg["segmentIndex"],
        "loadSegment": data_seg["loadSegment"],
        "codeOffset": data_seg["codeOffset"],
        "codeSize": data_seg["codeSize"],
        "isDataSegment": True,
        "file": fname,
        "numRelocations": 0,
    })

    (outdir / "segments.json").write_text(json.dumps(manifest, indent=2))

    # Also a dead-simple tab-separated manifest — tools/GhidraImportOverlays.java
    # reads this instead of pulling in a JSON library on Ghidra's script classpath.
    # Columns: segmentIndex  isDataSegment(0/1)  loadSegmentHex  codeSize  file
    with open(outdir / "segments.tsv", "w") as f:
        for m in manifest:
            load_seg = "" if m["loadSegment"] is None else f"{m['loadSegment']:x}"
            f.write(f"{m['segmentIndex']}\t{1 if m['isDataSegment'] else 0}\t"
                     f"{load_seg}\t{m['codeSize']}\t{m['file']}\n")

    print(f"Wrote {len(manifest)} segments to {outdir}")
    for m in manifest:
        tag = "DATA" if m["isDataSegment"] else "OVL "
        ls = "?" if m["loadSegment"] is None else f"{m['loadSegment']:04x}h"
        print(f"  [{tag}] idx={m['segmentIndex']:2d} loadSegment={ls:>6} "
              f"codeOffset={m['codeOffset']:#x} codeSize={m['codeSize']:#x} "
              f"relocs={m['numRelocations']}")


if __name__ == "__main__":
    main()
