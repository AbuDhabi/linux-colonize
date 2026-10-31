# VICEROY.EXE data tables (retired)

`src/data/viceroy_tables.{c,h}` and `scripts/extract_viceroy_tables.py` were deleted
2026-10-31 (port_bugs #1336). No `VICEROY.EXE` table is baked into `src/` any more.

**World-map feature art** (coasts, forests, hills, mountains, land transitions,
resources) is driven by **`MAPEDIT.EXE`** — see [assets.md](assets.md) "Map overlay
compositing". The Colonizopedia terrain preview (`pedia_terrain_preview`, pedia.c)
uses its own index tests (27 → mountain PHYS0 36, 28 → hill 48) and a hand-written
forest canopy table.

## Why it went

The extractor anchored on a 28-byte sequence it took to be a river table at DS:0x54de
and used `file_offset = ds_offset + 0x186e6`. The confirmed DS→EXE rule is
`file_offset = 121248 + ds_offset` (`0x1d9a0`; every GAME.TXT tag string in
[popup_tag_ids.md](popup_tag_ids.md) lands there). Its `viceroy_terrain_meta`
("DS:0x543f", 29 × 52) really came from DS:0x185 and held DOS text bytes; the real
DS:0x543f is the **nation** table (stride 0x34, byte 0 = control). Byte 0 of row 6
(0xff) made the Marsh preview carry mountain sprite 36.

Trap: **DS:0x5234** is the moves×3 column of the stride-0xe `@UNIT` type table
(`0x5235` defense, `0x5236` attack, `0x5237` holds; `ai_euro_land.c`, `units_move.c`),
not a tile-display table.
