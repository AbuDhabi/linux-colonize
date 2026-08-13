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


def find_data_segment(data, segs):
    """Mirrors loadDataDetails(): V2's static/resident+data region is found
    by scanning for the "MS Run-Time" signature, 8 bytes before it."""
    marker = b"MS Run-Time"
    idx = data.find(marker)
    if idx == -1:
        raise RuntimeError('Could not locate "MS Run-Time" signature for the data segment')
    file_offset = idx - 8

    # Mirrors loadDataDetails(): for V2 (segmentList[0] is executable), the
    # resident/data region runs up to the first RTLink segment's header —
    # i.e. up to the *smallest* headerOffset among the parsed overlay
    # segments, not "rest of file".
    first_header_offset = min(s["headerOffset"] for s in segs)

    # loadSegment for this synthetic segment isn't printed by rtlink_decode's
    # listInfo, but IS used internally (see rtlink_decode_v2_gap.md — this is
    # the segment carrying the 10 patched data-segment pointers, at DOS
    # selector 0x1b5a for VICEROY.EXE specifically). Not needed for byte
    # extraction; pass a companion `rtlink_decode VICEROY.EXE` info-mode run
    # if you need it for a Ghidra base address on this one segment.
    return {
        "segmentIndex": 0,
        "loadSegment": None,  # see docstring above
        "codeOffset": file_offset,
        "codeSize": first_header_offset - file_offset,
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
    data_seg = find_data_segment(data, segs)

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
