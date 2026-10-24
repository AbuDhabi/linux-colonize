# VICEROY.EXE data tables

Static lookup tables extracted from `COLONIZE/VICEROY.EXE` for Colonizopedia tile
previews and residual DOS references.

**World-map feature art** (coasts, forests, hills, mountains, land transitions,
resources) is driven by **`MAPEDIT.EXE`**, not these tables — see
[assets.md](assets.md) "Map overlay compositing".

Regenerate embedded copies:

```bash
python3 scripts/extract_viceroy_tables.py
```

C API: `src/data/viceroy_tables.h`

## DS → file offset

`scripts/extract_viceroy_tables.py` anchors on a 28-byte sequence it takes to be the
river table at **DS:0x54de**, finds it at file `0x1dbc4`, and so uses
`file_offset = ds_offset + 0x186e6`. That contradicts the confirmed DS→EXE rule,
`file_offset = 121248 + ds_offset` (`0x1d9a0`; every GAME.TXT tag string in
[popup_tag_ids.md](popup_tag_ids.md) lands there): under it the anchor bytes sit at
DS:0x224. Treat anything extracted with `0x186e6` as unverified.

## Tables

| DS offset (extractor) | Symbol | Size | Purpose |
|-----------|--------|------|---------|
| `0x543f` | `viceroy_terrain_meta` | 29 × 52 | Read as per map terrain index (0–28, `PEDIA.TXT` `@TERRAINn`); unverified |

The bytes come from file `0x1db25` (DS:0x185 under the confirmed rule). The real
DS:0x543f is the **nation** table (stride 0x34, byte 0 = control, 0 = human; see
`src/core/colony_workers.c`, `original_sources_annotated/turn/europe_nation_eot.md`), so
the per-terrain reading has no decomp backing. Only `viceroy_terrain_meta` and its
`viceroy_terrain_class` accessor are generated and consumed (pedia.c,
`tests/smoke/test_viceroy_tables.c`); `viceroy_forest_phys0_sprite` is a hand-written
switch, not table data.

The other tables the extractor once emitted (`0x54de` river, `0x54fc` / `0x5502` feature
bases, `0x5599` connectivity) were retired 2026-09-14 (no consumer, duplication audit
TT-27..29); their purposes were guesses. Trap: **DS:0x5234** is the moves×3 column of
the stride-0xe `@UNIT` type table (`0x5235` defense, `0x5236` attack, `0x5237` holds;
`ai_euro_land.c`, `units_move.c`), not a tile-display table.

### `terrain_meta` (52 bytes / terrain)

| Offset | Field (as read, unverified) |
|--------|-------|
| 0 | `class_flag` — selects renderer path (land, water layer, river, hills, mountains) |
| 1–2 | Pedia preview width (u16 LE) for terrain types that use the sample grid |
| 3–4 | Pedia preview height (u16 LE) |
| 5–51 | Additional compositing / palette / layout bytes (not fully decoded yet) |

Byte 0 as extracted:

| Index | PEDIA name | `class_flag` |
|-------|------------|--------------|
| 2 / 3 / 4 | Plains / Prairie / Grassland | `0x54` / `0x72` / `0x01` |
| 6 / 7 | Marsh / Swamp | `0xff` / `0x0e` |
| 9 | Scrub forest | `0x47` |
| 27 / 28 | Mountains / Hills | `0xff` / `0x07` |
| others | | `0` |

pedia.c ORs `class_flag` with its own index tests (`== 27` → mountain PHYS0 36,
`== 28` → hill 48), so Marsh (`0xff`) also gets the mountain sprite in its
`PediaTerrainPreview.phys0_sprites` list (read only by `tests/unit/test_pedia.c`; no
renderer uses it).

## References in decomp exports

Prefer `original_sources_decompiled/viceroy_unpacked.c` for overlay-resident code.
See [decomp_inventory.md](decomp_inventory.md).

- `FUN_15eb_06d2` — Colonizopedia mini-map shares world-map drawing entry points
