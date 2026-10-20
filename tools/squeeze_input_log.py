#!/usr/bin/env python3
"""Shorten idle stretches in a debug.input_logger log (regression_replay tests).

Every gap of more than MAX_GAP frames between two consecutive records is cut
down to MAX_GAP frames; t_ms / unix_ms shift by the same share of the gap, so
the remaining frames keep their recorded frame time. Too small a MAX_GAP lands
inputs before the game is ready for them (3 broke carpentry_two_end_turns, 8
held); 20 is the safe default.

usage: squeeze_input_log.py IN.jsonl OUT.jsonl [MAX_GAP]
"""
import json
import sys

src, dst = sys.argv[1], sys.argv[2]
max_gap = int(sys.argv[3]) if len(sys.argv) > 3 else 20
lines = [json.loads(l) for l in open(src) if l.strip()]
out, cut_frames, cut_ms, prev = [], 0, 0, None
for rec in lines:
    if rec.get("type") != "session":
        if prev:
            gap = rec["frame"] - prev["frame"]
            if gap > max_gap:
                cut_frames += gap - max_gap
                cut_ms += (rec["t_ms"] - prev["t_ms"]) * (gap - max_gap) // gap
        prev = dict(rec)
        rec["frame"] -= cut_frames
        rec["t_ms"] -= cut_ms
        rec["unix_ms"] -= cut_ms
    out.append(rec)
with open(dst, "w") as f:
    f.writelines(json.dumps(r, separators=(",", ":")) + "\n" for r in out)
