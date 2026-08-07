#!/usr/bin/env python3
"""Generate / merge original_sources_annotated/FUNCTION_CATALOG.md and MODULE_MAP.md.

Parses FUN_* definitions from the Ghidra C exports, merges human-filled fields from
an existing catalog (and optional seed JSON), and rewrites the catalog + module map.

Usage:
  python3 scripts/gen_fun_catalog.py
  python3 scripts/gen_fun_catalog.py --dry-run
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VICEROY_C = ROOT / "original_sources_decompiled" / "viceroy_unpacked.c"
MAPEDIT_C = ROOT / "original_sources_decompiled" / "mapedit.c"
CATALOG_OUT = ROOT / "original_sources_annotated" / "FUNCTION_CATALOG.md"
MODULE_MAP_OUT = ROOT / "original_sources_annotated" / "MODULE_MAP.md"
SEED_JSON = ROOT / "scripts" / "fun_catalog_seed.json"

# Ghidra emits several def shapes: __cdecl16far/near, __stdcall16far,
# typed `void FUN_…(`, and bare `FUN_…(` (common for overlay stubs/thunks).
FUN_CDECL_RE = re.compile(
    r"__(?:cdecl16(?:far|near)|stdcall16far)\s+(FUN_([0-9a-f]{4})_[0-9a-f]+)\s*\("
)
FUN_TYPED_RE = re.compile(
    r"^(?:void|undefined[0-9]*|char\s*\*|byte|int|uint|long|bool|code)\s+"
    r"(FUN_([0-9a-f]{4})_[0-9a-f]+)\s*\("
)
FUN_BARE_RE = re.compile(r"^(FUN_([0-9a-f]{4})_[0-9a-f]+)\s*\(")
ROW_RE = re.compile(
    r"^\|\s*`(?P<sym>FUN_[0-9a-f]{4}_[0-9a-f]+)`\s*"
    r"\|\s*(?P<line>\d+)\s*"
    r"\|\s*(?P<size>\d+)\s*"
    r"\|\s*(?P<system>[^|]*)\s*"
    r"\|\s*(?P<purpose>[^|]*)\s*"
    r"\|\s*(?P<confidence>[^|]*)\s*"
    r"\|\s*(?P<links>[^|]*)\s*"
    r"\|$"
)

HUMAN_FIELDS = ("system", "purpose", "confidence", "links")
DEFAULTS = {
    "system": "unknown",
    "purpose": "unknown",
    "confidence": "unknown",
    "links": "",
}


def match_fun_def(line: str) -> re.Match[str] | None:
    """Match a FUN_* definition on an unindented source line."""
    if not line or line[0] in " \t":
        return None
    if "thunk_FUN_" in line:
        return None
    for pat in (FUN_CDECL_RE, FUN_TYPED_RE, FUN_BARE_RE):
        m = pat.search(line) if pat is FUN_CDECL_RE else pat.match(line)
        if m:
            return m
    return None


def extract_defs(path: Path) -> list[dict]:
    """Return ordered FUN_* defs with line numbers and coarse size hints."""
    lines = path.read_text(errors="replace").splitlines()
    found: list[tuple[int, str, str]] = []
    seen: set[str] = set()
    for i, line in enumerate(lines, 1):
        m = match_fun_def(line)
        if not m:
            continue
        sym, seg = m.group(1), m.group(2)
        if sym in seen:
            continue
        seen.add(sym)
        found.append((i, sym, seg))
    total_lines = len(lines)
    out: list[dict] = []
    for idx, (line_no, sym, seg) in enumerate(found):
        end = found[idx + 1][0] if idx + 1 < len(found) else total_lines + 1
        out.append(
            {
                "symbol": sym,
                "segment": seg,
                "line": line_no,
                "size": max(1, end - line_no),
                **DEFAULTS,
            }
        )
    return out


def parse_existing_catalog(path: Path) -> dict[str, dict[str, dict]]:
    """Parse human fields from an existing FUNCTION_CATALOG.md.

    Returns {exe: {symbol: {system, purpose, confidence, links}}}.
    """
    result: dict[str, dict[str, dict]] = {"viceroy": {}, "mapedit": {}}
    if not path.is_file():
        return result
    exe = None
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("## VICEROY"):
            exe = "viceroy"
            continue
        if line.startswith("## MAPEDIT"):
            exe = "mapedit"
            continue
        if exe is None:
            continue
        m = ROW_RE.match(line)
        if not m:
            continue
        fields = {
            k: m.group(k).strip()
            for k in ("system", "purpose", "confidence", "links")
        }
        # Ignore pure skeleton rows (all unknown / empty) — seed can refill.
        if (
            fields["system"] == "unknown"
            and fields["purpose"] == "unknown"
            and fields["confidence"] == "unknown"
            and not fields["links"]
        ):
            continue
        result[exe][m.group("sym")] = fields
    return result


def load_seed(path: Path) -> dict:
    if not path.is_file():
        return {"segment_systems": {}, "functions": {}}
    return json.loads(path.read_text())


def apply_merge(
    defs: list[dict],
    exe: str,
    existing: dict[str, dict],
    seed: dict,
) -> None:
    """Mutate defs: existing catalog wins, then seed fills unknowns, then segment defaults."""
    seg_seed = seed.get("segment_systems", {}).get(exe, {})
    fun_seed = seed.get("functions", {}).get(exe, {})

    for row in defs:
        sym = row["symbol"]
        seg = row["segment"]

        # 1) Preserve human edits from existing catalog.
        if sym in existing:
            for k in HUMAN_FIELDS:
                val = existing[sym].get(k, "")
                if val != "":
                    row[k] = val

        # 2) Seed function overlays fill only unknown / empty fields.
        if sym in fun_seed:
            for k in HUMAN_FIELDS:
                if k not in fun_seed[sym]:
                    continue
                if row[k] in ("", "unknown"):
                    row[k] = fun_seed[sym][k]

        # 3) Segment default system when still unknown.
        if row["system"] == "unknown" and seg in seg_seed:
            row["system"] = seg_seed[seg]["system"]
            if row["confidence"] == "unknown":
                # Segment-level tag is at most "inferred" unless seed says known
                # and we only set system (purpose stays unknown).
                conf = seg_seed[seg].get("confidence", "inferred")
                row["confidence"] = conf if conf != "known" else "inferred"


def esc_cell(s: str) -> str:
    return s.replace("|", "/").strip()


def render_catalog(
    viceroy: list[dict],
    mapedit: list[dict],
    seed: dict,
) -> str:
    lines: list[str] = []
    lines.append("# Function catalog")
    lines.append("")
    lines.append(
        "Light inventory of every `FUN_*` **definition** in the Ghidra C exports "
        "(including overlay stubs without `__cdecl16*` and `__stdcall16far` bodies). "
        "**Not** a deep annotation — one-line purpose (or `unknown`), system tag, "
        "confidence, and optional links. Regenerate with "
        "[`scripts/gen_fun_catalog.py`](../scripts/gen_fun_catalog.py); "
        "human-filled fields and "
        "[`scripts/fun_catalog_seed.json`](../scripts/fun_catalog_seed.json) "
        "are merged by symbol."
    )
    lines.append("")
    lines.append("Raw sources (never edit to rename):")
    lines.append("")
    lines.append(
        "- VICEROY: [`viceroy_unpacked.c`](../original_sources_decompiled/viceroy_unpacked.c)"
    )
    lines.append(
        "- MAPEDIT: [`mapedit.c`](../original_sources_decompiled/mapedit.c) "
        "(**separate address space**)"
    )
    lines.append("")
    lines.append(
        "Navigation: [`MODULE_MAP.md`](MODULE_MAP.md) (segment → system) · "
        "[`SYMBOL_MAP.md`](SYMBOL_MAP.md) (deep AI) · "
        "[`docs/original_index.md`](../docs/original_index.md)"
    )
    lines.append("")
    lines.append("| Field | Meaning |")
    lines.append("|-------|---------|")
    lines.append("| System | `ai` `mapgen` `mapdraw` `colony` `combat` `trade` `turn` `ui` `sound` `save` `platform` `thunk` `unknown` |")
    lines.append("| Confidence | `known` / `inferred` / `unknown` |")
    lines.append("| Size | Coarse lines until next `FUN_*` def (not exact body end) |")
    lines.append("")

    def emit_exe(title: str, exe_key: str, defs: list[dict], src_name: str) -> None:
        lines.append(f"## {title}")
        lines.append("")
        lines.append(
            f"{len(defs)} functions in `{src_name}` (cdecl/stdcall + unannotated stubs). "
            f"Address space is **{title}-only** — do not equate offsets with the other EXE."
        )
        lines.append("")
        by_seg: dict[str, list[dict]] = defaultdict(list)
        for row in defs:
            by_seg[row["segment"]].append(row)
        seg_meta = seed.get("segment_systems", {}).get(exe_key, {})
        for seg in sorted(by_seg.keys()):
            rows = by_seg[seg]
            meta = seg_meta.get(seg, {})
            label = meta.get("label", "")
            # Dominant system among rows for the heading.
            systems = {r["system"] for r in rows}
            if len(systems) == 1:
                sys_label = next(iter(systems))
            elif "unknown" in systems and len(systems) == 2:
                sys_label = next(s for s in systems if s != "unknown") + "/mixed"
            else:
                sys_label = "mixed" if len(systems) > 1 else "unknown"
            heading = f"### Segment `{seg}` ({len(rows)} defs) — {sys_label}"
            if label:
                heading += f" — {label}"
            lines.append(heading)
            lines.append("")
            lines.append(
                "| Symbol | Line | Size | System | Purpose | Confidence | Links |"
            )
            lines.append(
                "|--------|-----:|-----:|--------|---------|------------|-------|"
            )
            for r in rows:
                lines.append(
                    f"| `{r['symbol']}` | {r['line']} | {r['size']} | "
                    f"{esc_cell(r['system'])} | {esc_cell(r['purpose'])} | "
                    f"{esc_cell(r['confidence'])} | {esc_cell(r['links'])} |"
                )
            lines.append("")

    emit_exe("VICEROY", "viceroy", viceroy, "viceroy_unpacked.c")
    emit_exe("MAPEDIT", "mapedit", mapedit, "mapedit.c")
    return "\n".join(lines) + "\n"


def segment_rollups(
    defs: list[dict], exe: str, seed: dict
) -> tuple[dict[str, list[dict]], list[tuple[str, int]]]:
    """Return by_seg map and sorted list of (seg, defs) still system-unknown."""
    by_seg: dict[str, list[dict]] = defaultdict(list)
    for row in defs:
        by_seg[row["segment"]].append(row)
    seg_meta = seed.get("segment_systems", {}).get(exe, {})
    unknown: list[tuple[str, int]] = []
    for seg, rows in by_seg.items():
        dominant, _conf = dominant_system(rows, seg_meta.get(seg))
        if dominant == "unknown":
            unknown.append((seg, len(rows)))
    unknown.sort(key=lambda kv: (-kv[1], kv[0]))
    return by_seg, unknown


def conf_counts(defs: list[dict]) -> tuple[int, int, int, int]:
    known = sum(1 for r in defs if r["confidence"] == "known")
    inferred = sum(1 for r in defs if r["confidence"] == "inferred")
    unk = sum(1 for r in defs if r["confidence"] == "unknown")
    sys_unk = sum(1 for r in defs if r["system"] == "unknown")
    return known, inferred, unk, sys_unk


def render_module_map(
    viceroy: list[dict],
    mapedit: list[dict],
    seed: dict,
) -> str:
    lines: list[str] = []
    lines.append("# Module map — segment → system")
    lines.append("")
    lines.append(
        "Cheat sheet for Ghidra segment prefixes (`FUN_<seg>_<off>`). "
        "Per-function detail: [`FUNCTION_CATALOG.md`](FUNCTION_CATALOG.md). "
        "Deep AI labels: [`SYMBOL_MAP.md`](SYMBOL_MAP.md)."
    )
    lines.append("")
    lines.append(
        "Confidence here is for the **segment cluster**, not every function. "
        "Unlabeled segments stay `unknown` until a catalog peel (layer A) assigns a tag. "
        "Function seeds may override a segment tag (e.g. `281f` thunks, `1427`→ai) — "
        "that is intentional, not a bug."
    )
    lines.append("")

    v_by, v_unk = segment_rollups(viceroy, "viceroy", seed)
    m_by, m_unk = segment_rollups(mapedit, "mapedit", seed)
    vk, vi, vu, vs = conf_counts(viceroy)
    mk, mi, mu, ms = conf_counts(mapedit)
    v_labeled = len(v_by) - len(v_unk)
    m_labeled = len(m_by) - len(m_unk)

    lines.append("## Progress")
    lines.append("")
    lines.append(
        f"**VICEROY:** {len(viceroy)} funcs · confidence known={vk} "
        f"inferred={vi} unknown={vu} · system unknown={vs} · "
        f"segments {v_labeled} labeled / {len(v_unk)} unknown "
        f"(of {len(v_by)})."
    )
    lines.append("")
    if v_unk:
        ids = " ".join(f"`{seg}`" for seg, _n in v_unk)
        lines.append(f"Remaining unknown segments ({len(v_unk)}): {ids}")
        lines.append("")
    lines.append(
        f"**MAPEDIT (parked):** {len(mapedit)} funcs · confidence known={mk} "
        f"inferred={mi} unknown={mu} · system unknown={ms} · "
        f"segments {m_labeled} labeled / {len(m_unk)} unknown "
        f"(of {len(m_by)})."
    )
    lines.append("")

    def emit_table(exe: str, defs: list[dict], title: str) -> None:
        lines.append(f"## {title}")
        lines.append("")
        by_seg: dict[str, list[dict]] = defaultdict(list)
        for row in defs:
            by_seg[row["segment"]].append(row)
        seg_meta = seed.get("segment_systems", {}).get(exe, {})
        lines.append("| Segment | Defs | System | Confidence | Cluster label | Catalog |")
        lines.append("|---------|-----:|--------|------------|---------------|---------|")
        for seg in sorted(by_seg.keys(), key=lambda s: (-len(by_seg[s]), s)):
            rows = by_seg[seg]
            dominant, conf = dominant_system(rows, seg_meta.get(seg))
            label = seg_meta.get(seg, {}).get("label", "")
            if not label:
                label = "—" if dominant == "unknown" else dominant
            # Search-friendly link; heading anchors vary by viewer.
            link = f"FUNCTION_CATALOG.md"
            lines.append(
                f"| `{seg}` | {len(rows)} | {dominant} | {conf} | {esc_cell(label)} | "
                f"[catalog]({link}) |"
            )
        lines.append("")

    emit_table("viceroy", viceroy, "VICEROY")
    emit_table("mapedit", mapedit, "MAPEDIT (separate EXE)")
    lines.append("## Peel protocol")
    lines.append("")
    lines.append(
        "One thin layer per session (see annotated README). Do not mix layers:"
    )
    lines.append("")
    lines.append("| Layer | Work |")
    lines.append("|-------|------|")
    lines.append(
        "| **A** | Segment labeling — assign system to next unlabeled high-def segment |"
    )
    lines.append(
        "| **B** | String/XREF pass — label functions from `.asm` strings for one game area |"
    )
    lines.append(
        "| **C** | Call-tree from known entry — label one-hop callees |"
    )
    lines.append(
        "| **D** | Selective deepen — extract annotated stub under `original_sources_annotated/<system>/` |"
    )
    lines.append("")
    return "\n".join(lines) + "\n"


def Counter_systems(rows: list[dict]) -> dict[str, int]:
    counts: dict[str, int] = defaultdict(int)
    for r in rows:
        counts[r["system"]] += 1
    return counts


def dominant_system(
    rows: list[dict], meta: dict | None
) -> tuple[str, str]:
    counts = Counter_systems(rows)
    # Prefer non-unknown if present.
    ranked = sorted(
        counts.items(),
        key=lambda kv: (kv[0] == "unknown", -kv[1], kv[0]),
    )
    dominant = ranked[0][0]
    if meta and "system" in meta:
        # Seed segment label wins for MODULE_MAP when present.
        dominant = meta["system"]
        conf = meta.get("confidence", "inferred")
        return dominant, conf
    if dominant == "unknown":
        return "unknown", "unknown"
    # Mixed?
    non_unk = {s: n for s, n in counts.items() if s != "unknown"}
    if len(non_unk) > 1:
        return "mixed", "inferred"
    return dominant, "inferred"


def stats(defs: list[dict]) -> str:
    n = len(defs)
    known = sum(1 for r in defs if r["confidence"] == "known")
    inferred = sum(1 for r in defs if r["confidence"] == "inferred")
    unk = sum(1 for r in defs if r["confidence"] == "unknown")
    sys_unk = sum(1 for r in defs if r["system"] == "unknown")
    return (
        f"{n} funcs; confidence known={known} inferred={inferred} unknown={unk}; "
        f"system unknown={sys_unk}"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="Print stats only; do not write files",
    )
    ap.add_argument(
        "--catalog",
        type=Path,
        default=CATALOG_OUT,
        help="Output / merge path for FUNCTION_CATALOG.md",
    )
    ap.add_argument(
        "--module-map",
        type=Path,
        default=MODULE_MAP_OUT,
        help="Output path for MODULE_MAP.md",
    )
    ap.add_argument(
        "--seed",
        type=Path,
        default=SEED_JSON,
        help="Seed JSON path",
    )
    args = ap.parse_args()

    if not VICEROY_C.is_file() or not MAPEDIT_C.is_file():
        print("Missing decompiled sources", file=sys.stderr)
        return 1

    seed = load_seed(args.seed)
    existing = parse_existing_catalog(args.catalog)

    viceroy = extract_defs(VICEROY_C)
    mapedit = extract_defs(MAPEDIT_C)
    apply_merge(viceroy, "viceroy", existing.get("viceroy", {}), seed)
    apply_merge(mapedit, "mapedit", existing.get("mapedit", {}), seed)

    print("VICEROY:", stats(viceroy))
    print("MAPEDIT:", stats(mapedit))

    catalog_text = render_catalog(viceroy, mapedit, seed)
    module_text = render_module_map(viceroy, mapedit, seed)

    if args.dry_run:
        print(f"Would write {args.catalog} ({len(catalog_text)} bytes)")
        print(f"Would write {args.module_map} ({len(module_text)} bytes)")
        return 0

    args.catalog.parent.mkdir(parents=True, exist_ok=True)
    args.catalog.write_text(catalog_text)
    args.module_map.write_text(module_text)
    print(f"Wrote {args.catalog}")
    print(f"Wrote {args.module_map}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
