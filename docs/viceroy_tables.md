# VICEROY.EXE data tables

Static lookup tables used by the DOS map renderer and Colonizopedia tile previews, extracted from `COLONIZE/VICEROY.EXE`.

Regenerate embedded copies:

```bash
python3 scripts/extract_viceroy_tables.py
```

C API: `src/data/viceroy_tables.h`

## DS → file offset

All tables use the same linear mapping anchored on the river transition table at **DS:0x54de** (file `0x1dbc4` in the shipped executable):

```
file_offset = ds_offset + viceroy_ds_to_file_offset   // 0x186e6
```

## Tables

| DS offset | Symbol | Size | Purpose |
|-----------|--------|------|---------|
| `0x543f` | `viceroy_terrain_meta` | 29 × 52 | Per map terrain index (0–28, matches `PEDIA.TXT` `@TERRAINn`) |
| `0x5234` | `viceroy_tile_display` | 29 × 14 | Per tile display type (`0x3146` in the runtime tile object) |
| `0x54de` | `viceroy_river_transition` | 28 | Overlay nibble → PHYS0 river sprite offset |
| `0x54fc` | `viceroy_feature_sprite_bases_a` | 4 | Feature sprite base indices (mountain/hill path) |
| `0x5502` | `viceroy_feature_sprite_bases_b` | 4 | Feature sprite base indices (forest/resource path) |
| `0x5599` | `viceroy_connectivity_transition` | 21 | Cardinal connectivity → transition variant |

### `terrain_meta` (52 bytes / terrain)

| Offset | Field |
|--------|-------|
| 0 | `class_flag` — selects renderer path (land, water layer, river, hills, mountains) |
| 1–2 | Pedia preview width (u16 LE) for terrain types that use the sample grid |
| 3–4 | Pedia preview height (u16 LE) |
| 5–51 | Additional compositing / palette / layout bytes (not fully decoded yet) |

Known `class_flag` values:

| Index | PEDIA name | `class_flag` |
|-------|------------|--------------|
| 0–5, 8–26 | Land / ocean / sea | `0` |
| 27 | Mountains | `255` |
| 28 | Hills | `7` |

### `tile_display` (14 bytes / display type)

Indexed by tile display type (`0x3146`), not the raw `.MP` terrain index.

| Offset | DS alias | Meaning |
|--------|----------|---------|
| 0 | `0x5234` | Base layer hint |
| 1 | `0x5235` | PHYS0 sprite group A (`× 8` → atlas index) |
| 2 | `0x5236` | PHYS0 sprite group B (`× 8`) |
| 3 | `0x5237` | Max overlay layers for this display type |

**Note:** In the on-disk EXE, only display types **0–9** are zero-initialized; types **10+** overlap unrelated read-only data (MS C runtime strings). The DOS game likely extends or overwrites this table at runtime. Types 0–9 are stored as extracted; do not treat file bytes at indices ≥ 10 as authoritative without further reverse engineering.

### `river_transition` (28 bytes)

Used in `viceroy.c` case `0x10` via `table[overlay_nibble + 0x54de]` to pick river PHYS0 sprites (range 1–39).

### `feature_sprite_bases_a` / `_b`

Small base-index tables immediately after the river transition table:

- A: `{12, 8, 22, 5}`
- B: `{17, 21, 25, 65}`

### `connectivity_transition` (21 bytes)

Candidate for coastline / connectivity variant selection (sprites in the 112–139 band).
**Not wired** in the Linux port; coast work is **parked** — see [decomp_inventory.md](decomp_inventory.md) (**Parked: coastlines**). Live RAM analysis showed per-tile `PHYS0` buffer bytes do not map cleanly to a simple neighbour mask; validate this table against DOS output before indexing it from `map.c`.

### Linux compositor usage

| Table | Wired? | Notes |
|-------|--------|-------|
| `connectivity_transition` | no | Coast parked; candidate table — needs DOS validation |
| `feature_sprite_bases_b[3]` | yes | Tundra row forest canopy (65) |
| `viceroy_forest_phys0_sprite()` | yes | All non-scrub forest PHYS0 overlays (map + pedia) |
| `river_transition` | no | Indexed by runtime `0x314c` in UI/unit paths; not map connectivity |
| `feature_sprite_bases_a` | no | Deferred with map compositor |
| `terrain_meta` class | pedia | Mountains/hills class flags |

## References in decomp exports

Prefer `viceroy_unpacked.c` for overlay-resident code; packed `viceroy.c` lacks
`FUN_6a9f_*` / `FUN_6b22_*`. See [decomp_inventory.md](decomp_inventory.md).

- `FUN_157e_*` — reads `0x5235` / `0x5236`, multiplies by 8 for **unit** PHYS0
  sprite index (not world-map coast composition)
- `FUN_112b_010e` (and similar) — `0x54de` river table with unit records
- case `0x10` — `0x54de` river table, `0x543f` terrain class checks
- `FUN_1427_065a` — reads `0x5234` / `0x5237` for layer counts
- `FUN_6a9f_0118` — map viewport tile loop uses `0xa576` / `0x848`, not these
  PHYS0 coast tables
- `FUN_15eb_06d2` — Colonizopedia mini-map shares world-map drawing entry points
