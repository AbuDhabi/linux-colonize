# Function catalog

Light inventory of every `FUN_*` **definition** in the Ghidra C exports (including overlay stubs without `__cdecl16*` and `__stdcall16far` bodies). **Not** a deep annotation — one-line purpose (or `unknown`), system tag, confidence, and optional links. Regenerate with [`scripts/gen_fun_catalog.py`](../scripts/gen_fun_catalog.py); human-filled fields and [`scripts/fun_catalog_seed.json`](../scripts/fun_catalog_seed.json) are merged by symbol.

Raw sources (never edit to rename):

- VICEROY: [`viceroy_unpacked.c`](../original_sources_decompiled/viceroy_unpacked.c)
- MAPEDIT: [`mapedit.c`](../original_sources_decompiled/mapedit.c) (**separate address space**)

Navigation: [`MODULE_MAP.md`](MODULE_MAP.md) (segment → system) · [`SYMBOL_MAP.md`](SYMBOL_MAP.md) (deep AI) · [`docs/original_index.md`](../docs/original_index.md)

| Field | Meaning |
|-------|---------|
| System | `ai` `mapgen` `mapdraw` `colony` `combat` `trade` `turn` `ui` `sound` `save` `platform` `thunk` `unknown` |
| Confidence | `known` / `inferred` / `unknown` |
| Size | Coarse lines until next `FUN_*` def (not exact body end) |

## VICEROY

2380 functions in `viceroy_unpacked.c` (cdecl/stdcall + unannotated stubs). Address space is **VICEROY-only** — do not equate offsets with the other EXE.

### Segment `1000` (2 defs) — ui — String-table index / Nth-string lookup

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1000_002c` | 399 | 17 | ui | Intern string into table arena (strlen→bump-alloc→strcpy); return index (bump DS:0x2d52) | inferred |  |
| `FUN_1000_0062` | 416 | 33 | ui | Return ptr to Nth NUL-terminated string in table (base DS:0x2d42) | inferred |  |

### Segment `1009` (15 defs) — ui — Timed turn-chrome / status text overlays

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1009_0004` | 449 | 20 | ui | Map overlay style→ink/attr (1/2→0x95, 3→0xc, else→0x44) | inferred |  |
| `FUN_1009_0036` | 469 | 39 | ui | Wait until overlay deadline or key; then flush key queue | inferred |  |
| `FUN_1009_00b4` | 508 | 42 | ui | Wait/restore then disarm overlay; clear buf + flags | inferred |  |
| `FUN_1009_017e` | 550 | 10 | ui | Append string onto status buffer at DS:0x2d54 | inferred |  |
| `FUN_1009_01a2` | 560 | 13 | ui | Resolve string-table idx then append to status buffer | inferred |  |
| `FUN_1009_01b8` | 573 | 12 | ui | itoa(int,base10) then append to status buffer | inferred |  |
| `FUN_1009_01d8` | 585 | 12 | ui | itoa(long,base10) then append to status buffer | inferred |  |
| `FUN_1009_01fc` | 597 | 13 | ui | Format long + suffix@0x6e then append to status buffer | inferred |  |
| `FUN_1009_0222` | 610 | 16 | ui | Truncate last char of status buffer if non-empty | inferred |  |
| `FUN_1009_0244` | 626 | 19 | ui | Arm timed overlay: flags + style + deadline=now+delta | inferred |  |
| `FUN_1009_0270` | 645 | 23 | ui | Poll overlay: if past deadline, clear via 00b4; return 1 | inferred |  |
| `FUN_1009_02ae` | 668 | 14 | ui | Set status-strip layout globals (x/y/w/h) | inferred |  |
| `FUN_1009_02cc` | 682 | 42 | ui | Draw/clear status text in strip (color, center blit) | inferred |  |
| `FUN_1009_0402` | 724 | 10 | ui | Arm timed overlay then immediately draw (0244+02cc; 1984_043a) | inferred |  |
| `FUN_1009_0420` | 734 | 9 | ui | strcat dest ← string at DS:0x50 (pad/append helper) | inferred |  |

### Segment `104b` (29 defs) — ui — Text / number blit helpers (1d1d_11b4)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_104b_0010` | 743 | 14 | ui | Append N spaces onto dest (loop 1009_0420 / DS:0x50) | inferred |  |
| `FUN_104b_0032` | 757 | 9 | ui | strcat dest ← ", " (DS:0x52) | inferred |  |
| `FUN_104b_0042` | 766 | 9 | ui | strcat dest ← ": " (DS:0x55) | inferred |  |
| `FUN_104b_0052` | 775 | 9 | ui | strcat dest ← ".  " (DS:0x58) | inferred |  |
| `FUN_104b_0062` | 784 | 9 | ui | strcat dest ← "%" (DS:0x5c) | inferred |  |
| `FUN_104b_0072` | 793 | 9 | ui | strcat dest ← "(" (DS:0x5e) | inferred |  |
| `FUN_104b_0082` | 802 | 9 | ui | strcat dest ← ")" (DS:0x60) | inferred |  |
| `FUN_104b_0092` | 811 | 9 | ui | strcat dest ← "{" (DS:0x62) | inferred |  |
| `FUN_104b_00a2` | 820 | 9 | ui | strcat dest ← "}" (DS:0x64) | inferred |  |
| `FUN_104b_00b2` | 829 | 9 | ui | strcat dest ← "+" (DS:0x66) | inferred |  |
| `FUN_104b_00c2` | 838 | 9 | ui | strcat dest ← "-" (DS:0x68) | inferred |  |
| `FUN_104b_00d2` | 847 | 9 | ui | strcat dest ← "x" (DS:0x6a) | inferred |  |
| `FUN_104b_00e2` | 856 | 10 | ui | Resolve string-table idx then strcat onto dest (1000_0062→1d1d_11b4) | inferred |  |
| `FUN_104b_00fc` | 866 | 12 | ui | Wrap string-table entry in {…} then strcat onto dest | inferred |  |
| `FUN_104b_012e` | 878 | 12 | ui | itoa(int,base10) then strcat onto dest | inferred |  |
| `FUN_104b_0156` | 890 | 27 | ui | itoa(base2) zero-pad to width 8 ("0"@0x6c) then strcat onto dest | inferred |  |
| `FUN_104b_01be` | 917 | 13 | ui | itoa(long,base10) then strcat onto dest | inferred |  |
| `FUN_104b_01e8` | 930 | 10 | ui | Format long then append "$" (01be + DS:0x6e) | inferred |  |
| `FUN_104b_0216` | 940 | 12 | ui | Measure string width−1 (primary font DS:0x89e/0x8a0) | inferred |  |
| `FUN_104b_0232` | 952 | 12 | ui | Measure string width−1 (alt font DS:0x268a/0x268c) | inferred |  |
| `FUN_104b_024e` | 964 | 13 | ui | Draw string at xy with color 0 (primary font) | inferred |  |
| `FUN_104b_0288` | 977 | 14 | ui | Set text colors then draw string (primary font) | inferred |  |
| `FUN_104b_02c2` | 991 | 16 | ui | Color+measure+draw string (primary); return remaining width | inferred |  |
| `FUN_104b_0318` | 1007 | 17 | ui | Center string in width then draw via 0288 | inferred |  |
| `FUN_104b_035c` | 1024 | 12 | ui | Draw string at xy with color 0 (alt font) | inferred |  |
| `FUN_104b_039a` | 1036 | 12 | ui | Draw string at xy with color 0 (alt font; 1c11 path) | inferred |  |
| `FUN_104b_03d2` | 1048 | 15 | ui | Measure+draw string (alt font, color 0); return remaining width | inferred |  |
| `FUN_104b_0430` | 1063 | 17 | ui | Center string in width then draw via 039a (alt font) | inferred |  |
| `FUN_104b_0478` | 1080 | 23 | ui | Append 2f74[idx×0x10] label (neg→2e0a); idx 8..23 also space+2db0 suffix | inferred |  |

### Segment `1097` (7 defs) — ui — Multi-item dialog spacing / number layout

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1097_0004` | 1103 | 59 | ui | Compute multi-item row pitch/spacing and leftover margin | inferred |  |
| `FUN_1097_00de` | 1162 | 25 | ui | Draw positive int (optional backdrop clear) with given text color | inferred |  |
| `FUN_1097_0174` | 1187 | 73 | ui | Layout/blit spaced icon row (1c36) with optional count labels | inferred |  |
| `FUN_1097_02da` | 1260 | 40 | ui | Hit-test which spaced-row item index is under cursor | inferred |  |
| `FUN_1097_0394` | 1300 | 139 | ui | Layout/blit multi-group item rows from DS:0x2ce0 list tables | inferred |  |
| `FUN_1097_067a` | 1439 | 11 | ui | Clear multi-item layout list (DS:0x2ce0=0) | inferred |  |
| `FUN_1097_0682` | 1450 | 21 | ui | Append item (id/count/alt @2cf4/2cce/2ce2) to layout list | inferred |  |

### Segment `1101` (6 defs) — ui — 16-row glyph/bitmap blit helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1101_000e` | 1471 | 9 | ui | Allocate glyph-blit scratch buffer (7ada_01a0←*0x2674) | inferred |  |
| `FUN_1101_0026` | 1480 | 15 | ui | Remap glyph sheet row (9/0x11→8; else if >7 subtract 0xf) | inferred |  |
| `FUN_1101_0050` | 1495 | 35 | ui | Opaque 16×16 glyph blit from sheet row into dest bitmap | inferred |  |
| `FUN_1101_00b4` | 1530 | 45 | ui | Transparent 16×16 glyph blit (write only where dest==0) | inferred |  |
| `FUN_1101_0126` | 1575 | 45 | ui | Scaled opaque glyph blit (subsample by 1<<zoom) | inferred |  |
| `FUN_1101_01dc` | 1620 | 55 | ui | Scaled transparent glyph blit (subsample; skip nonzero dest) | inferred |  |

### Segment `112b` (8 defs) — mapdraw — Unit orders/allegiance chrome (FUN_112b_01ba)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_112b_0002` | 1675 | 282 | mapdraw | Profession → ICONS.SS index (AX+0x52; specials for 0x13..0x1c) | inferred | docs/assets.md |
| `FUN_112b_0060` | 1957 | 40 | mapdraw | Unit display icon from type@5232 + profession overrides (via 0002) | inferred | docs/assets.md |
| `FUN_112b_010e` | 1997 | 33 | mapdraw | Pick stack unit for chrome (prefer ship 0xd–0x12) then icon via 0060 | inferred | docs/assets.md |
| `FUN_112b_015c` | 2030 | 19 | mapdraw | Orders-box rect (bit0) and/or color sprite blit (bit1) for unit chrome | inferred | docs/assets.md |
| `FUN_112b_01ba` | 2049 | 265 | mapdraw | Unit chrome: silhouette + nation orders box + letter (+ stack under-rect) | known | docs/assets.md |
| `FUN_112b_0790` | 2314 | 183 | mapdraw | Tribe/village map chrome: silhouette, relation bars, optional name | inferred |  |
| `FUN_112b_0c64` | 2497 | 102 | mapdraw | Colony map chrome: nation fill, fort tier, pop/name text | inferred |  |
| `FUN_112b_0eb6` | 2599 | 132 | mapdraw | Animate unit tile-to-tile move with per-frame 01ba chrome redraw | inferred | docs/assets.md |

### Segment `124c` (7 defs) — platform — Small helpers (e.g. DOS distance)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_124c_000c` | 2731 | 14 | platform | Clamp value to [lo,hi] | inferred |  |
| `FUN_124c_002a` | 2745 | 14 | platform | Swap two words through near pointers | inferred |  |
| `FUN_124c_0040` | 2759 | 17 | platform | DOS distance helper | known | ai/accessors.c |
| `FUN_124c_007c` | 2776 | 20 | platform | DOS-distance between points (abs dx/dy → 0040) | inferred |  |
| `FUN_124c_00c4` | 2796 | 17 | platform | Chebyshev component: max(/a/,/b/) | inferred |  |
| `FUN_124c_00f4` | 2813 | 20 | platform | Chebyshev distance between points (max /dx/,/dy/) | inferred |  |
| `FUN_124c_013c` | 2833 | 14 | platform | True if 8-way dir is ±1 neighbor of given dir | inferred |  |

### Segment `1262` (10 defs) — ui — Input wait / mouse hit-test / tip overlays

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1262_0002` | 2847 | 8 | ui | Read Shift-key state bits (BDA 40:17 & 3) | inferred | FUN_281f_03a2 |
| `FUN_1262_0012` | 2855 | 13 | ui | Clear tip-overlay rect (0x2da8…) + video flush | inferred | FUN_281f_03b6 |
| `FUN_1262_003c` | 2868 | 19 | ui | Busy-wait for next key/mouse event (BGM pump via 129f_00f6) | inferred | FUN_1262_0060 sibling |
| `FUN_1262_0060` | 2887 | 42 | ui | Wait for key/mouse with UI pump (timeout / abort flags) | inferred | FUN_281f_03c0 |
| `FUN_1262_00da` | 2929 | 15 | ui | Drain pending key/mouse event queue | inferred |  |
| `FUN_1262_00f6` | 2944 | 15 | ui | Mouse hit-test: cursor (0x7e8/0x7ea) inside x,y,w,h rect | inferred | FUN_281f_03ca |
| `FUN_1262_0128` | 2959 | 14 | ui | Wrap selection index into [0, n) | inferred | FUN_281f_038e |
| `FUN_1262_0142` | 2973 | 9 | ui | Bind tip-string far ptr (seg:off 0x7a:0x1a0a) | inferred | FUN_281f_0398; FUN_1a29_021b |
| `FUN_1262_0152` | 2982 | 70 | ui | Present tip overlay by kind (1=clear; 2–4 colony/nation text) | inferred | FUN_1262_02fe |
| `FUN_1262_02fe` | 3052 | 71 | ui | Mouse-move / tooltip debounce tick (dwell → tip kinds 1..10) | inferred | FUN_281f_03ac |

### Segment `129f` (7 defs) — sound — BGM helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_129f_0008` | 3123 | 1295 | sound | Pick next BGM/event id until ≠ current (DS:0x96) | inferred | FUN_129f_00f6 |
| `FUN_129f_00f6` | 4418 | 444 | sound | Idle BGM/sound pump: service pending track/event + gate play | inferred | FUN_129f_0318 sibling; FUN_12d8_000e |
| `FUN_129f_02cc` | 4862 | 19 | sound | Queue sound/event id if changed | inferred | FUN_281f_048e |
| `FUN_129f_0300` | 4881 | 11 | sound | Store BGM track id at DS:0x9a | inferred | FUN_281f_0498; FUN_129f_0318 |
| `FUN_129f_030c` | 4892 | 11 | sound | Store pending/next BGM id at DS:0x98 | inferred | FUN_281f_04a2 |
| `FUN_129f_0318` | 4903 | 17 | sound | Set BGM track id + gate play | inferred |  |
| `FUN_129f_034c` | 4920 | 15 | sound | Play SFX/BGM if sound enabled else only store id | inferred | FUN_281f_04b6 |

### Segment `12d6` (1 defs) — ui — Mouse-gated blit to VGA A000

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12d6_0000` | 4935 | 15 | ui | Mouse-gated blit offscreen (DS:0x2dac/0x2dae) → VGA A000 | inferred |  |

### Segment `12d8` (1 defs) — sound — BGM / event / SFX gating

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12d8_000e` | 4950 | 16 | sound | BGM / event / SFX gating | known | src/core/sound.c |

### Segment `12dd` (2 defs) — ui — Clipped blit dispatch (rect vs raw)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12dd_0002` | 4966 | 21 | ui | Clipped blit dispatch: tiled 1bf5 if clip-rect DS:0x82e else solid fill 1b9e | inferred |  |
| `FUN_12dd_0064` | 4987 | 20 | ui | Clipped blit dispatch: tiled 1bf5 if clip-rect DS:0x82c else solid fill 1b9e | inferred |  |

### Segment `12e9` (2 defs) — ui — Buffer fill via pitch helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12e9_0006` | 5007 | 8 | ui | Empty no-op stub (byte source for 12e9_008c pitched fill; body vacant in decomp) | inferred |  |
| `FUN_12e9_008c` | 5015 | 55 | ui | Fill pitched dest rect row-by-row with bytes from 12e9_0006 (1a4e pitch) | inferred |  |

### Segment `12fd` (3 defs) — ui — Once-only discovery/event dispatch (bitset DS:0x540a)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12fd_000e` | 5070 | 23 | ui | Set/clear discovery/event bit in DS:0x540a | inferred | FUN_12fd_0048 sibling |
| `FUN_12fd_0048` | 5093 | 19 | ui | Test discovery/event bit at DS:0x540a | inferred | FUN_281f_051a |
| `FUN_12fd_006c` | 5112 | 1034 | ui | Once-only discovery/event opcode dispatch | inferred |  |

### Segment `130d` (5 defs) — turn — Main game year/turn loop + intro splash

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_130d_000a` | 6146 | 74 | turn | Space-split splash text into ≤10 lines and present fullscreen | inferred |  |
| `FUN_130d_0172` | 6220 | 18 | turn | Autosave slot 8 on decade Spring, else slot 9 | known | docs/savegame.md |
| `FUN_130d_019e` | 6238 | 24 | turn | Compose demo/autoplay end splash strings → 000a | inferred |  |
| `FUN_130d_0222` | 6262 | 21 | turn | Compose independence-declared splash strings → 000a | inferred |  |
| `FUN_130d_0290` | 6283 | 236 | turn | Main game year/turn loop (nations, year/season, chrome) | known | turn/year_loop.c; docs/turn_between_players.md |

### Segment `137f` (26 defs) — mapgen — Map plane accessors (terrain/layer2/3)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_137f_000a` | 6519 | 16 | mapgen | map_tile_in_bounds — inset interior (1..pitch-2, 1..height-2) | known | ai/accessors.c |
| `FUN_137f_003c` | 6535 | 26 | mapgen | True if /dx/,/dy/ within radius mode (cardinal/diag bands) | inferred |  |
| `FUN_137f_00c0` | 6561 | 18 | mapgen | True if (x,y) inside camera/scroll viewport bounds | inferred |  |
| `FUN_137f_00f6` | 6579 | 10 | mapgen | terrain_ptr map accessor (DS:0x15c) | inferred | ai/accessors.c |
| `FUN_137f_010e` | 6589 | 11 | mapgen | terrain_byte map accessor | known | ai/accessors.c |
| `FUN_137f_012a` | 6600 | 10 | mapgen | layer2_ptr map accessor (DS:0x160) | inferred | ai/accessors.c |
| `FUN_137f_0142` | 6610 | 11 | mapgen | layer2_byte map accessor | known | ai/accessors.c |
| `FUN_137f_015e` | 6621 | 19 | mapgen | OR or AND-clear layer2 bits at tile | inferred | ai/accessors.c |
| `FUN_137f_0194` | 6640 | 10 | mapgen | layer3_ptr map accessor | known | ai/accessors.c |
| `FUN_137f_01ac` | 6650 | 11 | mapgen | layer3_byte map accessor | known | ai/accessors.c |
| `FUN_137f_01ca` | 6661 | 11 | mapgen | continent_id real body | known | ai/accessors.c |
| `FUN_137f_01dc` | 6672 | 15 | mapgen | set_continent_nibble — write layer3 low nibble | inferred | ai/accessors.c |
| `FUN_137f_0200` | 6687 | 16 | mapgen | owner_nibble — layer3 high nibble; 0xf → −1 | known | ai/accessors.c |
| `FUN_137f_0228` | 6703 | 26 | mapgen | set_owner_nibble | known | ai/accessors.c |
| `FUN_137f_02a0` | 6729 | 20 | mapgen | continent_id if in-bounds land (not ocean/HS); else −1 | inferred | ai/accessors.c |
| `FUN_137f_02e0` | 6749 | 10 | mapgen | explore_plane_ptr map accessor (DS:0x168) | inferred | ai/accessors.c |
| `FUN_137f_02f8` | 6759 | 11 | mapgen | tile_explore_mask — explore-plane byte at tile | known | ai/accessors.c |
| `FUN_137f_0314` | 6770 | 23 | mapgen | tile_owner_or_presence — owner if layer2 presence bit | known | ai/accessors.c |
| `FUN_137f_0358` | 6793 | 21 | mapgen | euro_settlement_owner real body | known | ai/accessors.c; ai/move_spent.c |
| `FUN_137f_0392` | 6814 | 26 | mapgen | Indian settlement owner (tribe bit, owner≥4); else −1 | inferred | ai/accessors.c |
| `FUN_137f_03e4` | 6840 | 23 | mapgen | tile_tribe_owner — owner if layer2 tribe bit | known | ai/accessors.c |
| `FUN_137f_0428` | 6863 | 14 | mapgen | tile_tribe_or_presence — tribe owner else presence owner | known | ai/accessors.c |
| `FUN_137f_044a` | 6877 | 28 | mapgen | Enemy Euro fort/colony owner vs nation when war bit set | inferred |  |
| `FUN_137f_04b0` | 6905 | 44 | mapgen | Procedural special-resource type from seed DS:0x190 | inferred |  |
| `FUN_137f_0598` | 6949 | 25 | mapgen | Procedural lost-city/rumour present on unowned land | inferred |  |
| `FUN_137f_0614` | 6974 | 28 | mapgen | Remap terrain type by climate setting DS:0x18e | inferred |  |

### Segment `13e4` (4 defs) — mapgen — Terrain class / ocean helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_13e4_000e` | 7002 | 13 | mapgen | decode_terrain_class | known | ai/accessors.c |
| `FUN_13e4_003a` | 7015 | 18 | mapgen | terrain_class_at — decode_terrain_class(terrain_byte) | known | ai/accessors.c |
| `FUN_13e4_0074` | 7033 | 14 | mapgen | ocean_or_high_seas real body | known | ai/accessors.c |
| `FUN_13e4_00a2` | 7047 | 15 | mapgen | True if terrain type is forest (8–23) | inferred | ai/accessors.c |

### Segment `13f1` (5 defs) — mapdraw — Exploration-bit / fog reveal around units & colonies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_13f1_000a` | 7062 | 34 | mapdraw | OR nation explore bit; claim/UI side effects on tile | inferred | ai/accessors.c |
| `FUN_13f1_00a6` | 7096 | 37 | mapdraw | Reveal ±5 around colony into nation explore plane | inferred | ai/accessors.c |
| `FUN_13f1_0158` | 7133 | 75 | mapdraw | Fog-reveal radius around point for Euro nation | inferred | ai/accessors.c |
| `FUN_13f1_02b4` | 7208 | 20 | mapdraw | Unit fog reveal (ship vs land radius into 0158) | inferred | ai/accessors.c |
| `FUN_13f1_02f8` | 7228 | 13 | mapdraw | Reveal exploration bits around unit (post-spawn/move) | inferred | ai/accessors.c |

### Segment `1427` (55 defs) — mixed — Tile / unit display and MP chrome

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1427_0002` | 7241 | 20 | mapdraw | Walk transport_next to stack head/top | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0026` | 7261 | 20 | mapdraw | Walk transport_prev to stack tail/bottom | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_004a` | 7281 | 14 | mapdraw | One step down stack (read transport_prev) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_005c` | 7295 | 45 | mapdraw | unit_index_on_tile: stack-head unit at (x,y) or −1 | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_012e` | 7340 | 26 | mapdraw | Nth unit in stack from head | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0164` | 7366 | 16 | mapdraw | Distance to stack head via transport_next | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0180` | 7382 | 27 | mapdraw | Nth non-transport unit in stack (5237==0) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_01dc` | 7409 | 17 | mapdraw | Count units in transport stack | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0204` | 7426 | 20 | mapdraw | Count units of given type in stack | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_023a` | 7446 | 32 | mapdraw | Unlink unit from stack; clear xy to 0xff | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_02ca` | 7478 | 35 | mapdraw | Place unit on tile; link into stack head | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0362` | 7513 | 10 | mapdraw | Move unit on map: unlink then place (023a+02ca) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_037e` | 7523 | 12 | mapdraw | Re-place unit at its current xy via 0362 | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_03a0` | 7535 | 30 | mapdraw | Demote unit to stack bottom | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_040c` | 7565 | 17 | ai | stack_set_xy post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_043e` | 7582 | 30 | mapdraw | Swap two units' transport link order | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_04d6` | 7612 | 69 | mapdraw | Reorder tile stack (transports/treasure/type priority) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0644` | 7681 | 10 | mapdraw | tile_stack_head: 04d6 reorder then 0002 | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_065a` | 7691 | 19 | mapdraw | Tile display (reads DS 0x5234); also unit_max_mp real body | known | ai/unit_mp.c; docs/viceroy_tables.md |
| `FUN_1427_06b4` | 7710 | 66 | mapdraw | Allocate/spawn unit into pool and place on tile | inferred |  |
| `FUN_1427_0824` | 7776 | 67 | mapdraw | Destroy unit; compact pool and fix stack indices | inferred |  |
| `FUN_1427_08ea` | 7843 | 24 | mapdraw | Select unit: tile refresh + reveal; set DS:0x5392 | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0954` | 7867 | 15 | mapdraw | Clear unit 3147 hi-nibble (keep nation); facing/vis reset | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0968` | 7882 | 15 | ai | stack_facing_refresh post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_0992` | 7897 | 15 | mapdraw | OR nation visibility bit (0x10<<n) into unit 3147 | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_09ac` | 7912 | 15 | mapdraw | stack_or_nation_flag: OR nation bit across stack | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_09dc` | 7927 | 45 | mapdraw | 8-adj foreign same-continent owner probe → DS:0x8cfa | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0ab0` | 7972 | 25 | mapdraw | 8-adj foreign presence-owner probe → DS:0x8cfa | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0b08` | 7997 | 37 | mapdraw | 8-adj foreign presence/tribe same-continent → 0x8cfa | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0bce` | 8034 | 16 | mapdraw | If no tribe on tile, run 0b08 neighbor probe | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0bfe` | 8050 | 31 | mapdraw | True if 8-adj has presence/colony of nation | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0c72` | 8081 | 16 | ai | unit_visibility_bits post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_0c9a` | 8097 | 26 | mapdraw | Build euro visibility bitmask at tile (owner+adj) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0ce6` | 8123 | 21 | ai | unit_post_move_chrome | known | ai/unit_mp.c |
| `FUN_1427_0d1e` | 8144 | 17 | mapdraw | Set unit nation low-nibble (3147 & 0xf) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_0d38` | 8161 | 297 | mapdraw | Unit/stack cargo+combat query dispatcher (opcode arg) | inferred |  |
| `FUN_1427_0f0e` | 8458 | 11 | mapdraw | Continent_id at unit's tile (137f_02a0) | inferred |  |
| `FUN_1427_0f30` | 8469 | 22 | mapdraw | Destroy entire transport stack via 0824 | inferred |  |
| `FUN_1427_0f64` | 8491 | 10 | mapdraw | Get profession low nibble (unit+0x17 / 315b) | inferred |  |
| `FUN_1427_0f74` | 8501 | 13 | mapdraw | Set profession low nibble (315b) | inferred |  |
| `FUN_1427_0f8e` | 8514 | 10 | mapdraw | Get profession high nibble (315b>>4) | inferred |  |
| `FUN_1427_0fa0` | 8524 | 11 | mapdraw | Set profession high nibble (315b) | inferred |  |
| `FUN_1427_0fc0` | 8535 | 15 | mapdraw | True if mounted type (dragoon/scout/mtd braves) | inferred |  |
| `FUN_1427_0fec` | 8550 | 16 | mapdraw | True if armed combat type (soldier/dragoon/arty/…) | inferred |  |
| `FUN_1427_101c` | 8566 | 40 | mapdraw | Ship cargo refresh walk on tile; return unit head | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_10be` | 8606 | 78 | mapdraw | Ship passenger embark/capacity bookkeeping (5237) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_1284` | 8684 | 22 | mapdraw | stack_has_ship: any type 0x0d..0x12 in stack | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_12c6` | 8706 | 16 | mapdraw | Set orders byte (314c) for whole stack | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_12f6` | 8722 | 19 | ai | unit_tile_list_refresh post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_1330` | 8741 | 25 | mapdraw | Has moves + on-map + not sentry/fortified | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_13b0` | 8766 | 20 | ai | unit_has_moves_remaining (behind 281f_097a) | known | ai/unit_mp.c |
| `FUN_1427_1410` | 8786 | 31 | mapdraw | Selectable-with-moves (1330 + Europe-dock special) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_14a0` | 8817 | 30 | mapdraw | Next unit index matching 1410 (round-robin) | inferred | original_sources_annotated/ai/unit_mp.c |
| `FUN_1427_14f4` | 8847 | 33 | mapdraw | Nearest unit of nation by dos_dist; dist→0x8cf8 | inferred |  |
| `FUN_1427_155e` | 8880 | 13 | ai | unit_exhaust_mp (behind 281f_0934) | known | ai/unit_mp.c |

### Segment `157e` (3 defs) — combat — Unit combat strength / engagement modifiers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_157e_0008` | 8893 | 21 | combat | Count village defense bonus probes (3×15eb_038e → 0..3) | inferred |  |
| `FUN_157e_004a` | 8914 | 54 | combat | Unit base combat×8 from type table 5235/5236 + vet/Drake/damage mods | known | docs/viceroy_tables.md |
| `FUN_157e_015e` | 8968 | 88 | combat | Full engagement strength: 004a × colony fort / village / terrain mods | inferred |  |

### Segment `15b3` (10 defs) — trade — Nation bilateral flags / name tables (Euro 0x13c, Indian 0x4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_15b3_0004` | 9056 | 13 | trade | Read bilateral diplomacy byte (Euro nation×0x13c / Indian×0x4e) | inferred | FUN_281f_0a38 |
| `FUN_15b3_0032` | 9069 | 15 | trade | Write bilateral diplomacy byte (Euro×0x13c / Indian×0x4e) | inferred |  |
| `FUN_15b3_0066` | 9084 | 18 | trade | OR diplomacy bit both directions; assert symmetry | inferred | FUN_281f_0a10 sibling; switch case→0066 |
| `FUN_15b3_00d0` | 9102 | 18 | trade | Clear diplomacy bit both directions; assert symmetry | inferred | FUN_281f_0a10 |
| `FUN_15b3_0144` | 9120 | 18 | trade | Format nation display name into buffer (table −0x72be; id3 special) | inferred |  |
| `FUN_15b3_0198` | 9138 | 21 | trade | Nation name string ptr (independence rebel/loyal remap) | inferred | FUN_281f_0a1a |
| `FUN_15b3_01e0` | 9159 | 21 | trade | Nation alt-name string ptr (independence remap; dialog subst) | inferred | FUN_281f_09a4 |
| `FUN_15b3_0228` | 9180 | 13 | trade | Nation name string ptr without independence remap | inferred | FUN_281f_0a24 |
| `FUN_15b3_024e` | 9193 | 13 | trade | Nation alt-name string ptr without independence remap | inferred |  |
| `FUN_15b3_0274` | 9206 | 13 | trade | Nation name-record base (0x5426+n×0x34; rebel→player remap) | inferred | FUN_281f_0a2e |

### Segment `15dc` (5 defs) — ai — Tribe / Indian current-context setters & lookups

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_15dc_0006` | 9219 | 16 | ai | Bind current Indian nation context (8d52/8d50/state@5ad6) | known | ai/indian_nation_turn.c; FUN_281f_0a42 |
| `FUN_15dc_0032` | 9235 | 18 | ai | Bind current tribe/Indian context from tribe index | known | include/viceroy_globals.h |
| `FUN_15dc_006a` | 9253 | 18 | ai | Indian nation class/tier from state+2 (returns 1/2/3) | inferred | FUN_281f_0a56 |
| `FUN_15dc_00a2` | 9271 | 17 | ai | Bucket integer into quartile 0..3 (<25/50/75) | inferred | FUN_281f_0a60 |
| `FUN_15dc_00e0` | 9288 | 10 | ai | Get Indian↔Euro relation word at DS:0x5b1c[i][e] | known | ai/indian_nation_turn.c; FUN_281f_030c |

### Segment `15eb` (107 defs) — mapdraw — High-density map / pedia draw paths

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_15eb_0002` | 9298 | 12 | mapdraw | Gate workplace/job id: 0 for {0x13,0x19-0x1c}, else 1 | inferred | thunk_FUN_281f_0c9a |
| `FUN_15eb_002c` | 9310 | 30 | mapdraw | Bind active colony (DS:0x8542) by index; set a897 visibility flag | inferred | thunk_FUN_281f_09e6 |
| `FUN_15eb_00a2` | 9340 | 40 | mapdraw | Any 8-neighbor is ocean/high-seas; stash best neighbor xy at 8dba/8dbc | inferred | thunk_FUN_281f_0d12; src/core/map_gen.c |
| `FUN_15eb_0142` | 9380 | 45 | mapdraw | Nearest colony by nation/continent; set active | inferred |  |
| `FUN_15eb_0218` | 9425 | 11 | mapdraw | Remap cargo/id 0x1c→0x13 before catalog table lookup | inferred |  |
| `FUN_15eb_022c` | 9436 | 12 | mapdraw | Cargo catalog title word at id×8−0x715e (after 0218 remap) | inferred | thunk_FUN_281f_0c18; FUN_6cb2_1820 |
| `FUN_15eb_0242` | 9448 | 12 | mapdraw | Cargo catalog secondary word at id×8−0x715c (after 0218 remap) | inferred | thunk_FUN_281f_0c40; FUN_6cb2_1820 |
| `FUN_15eb_0258` | 9460 | 11 | mapdraw | Continent id for colony idx (137f_02a0 on 5d46/5d47 xy) | inferred |  |
| `FUN_15eb_0274` | 9471 | 34 | mapdraw | Colony SoL % from bells/pop (0..100; +20 FF bonus) | inferred | thunk_FUN_281f_0c86 |
| `FUN_15eb_0302` | 9505 | 10 | mapdraw | Test building-present bit in active colony mask (+0x8a) | inferred |  |
| `FUN_15eb_0326` | 9515 | 22 | mapdraw | Set/clear building-present bit in active colony mask (+0x8a) | inferred |  |
| `FUN_15eb_035e` | 9537 | 13 | mapdraw | Test building bit in colony record feature mask (×0xca+0x5dca) | inferred | thunk_FUN_281f_0322 |
| `FUN_15eb_038e` | 9550 | 11 | mapdraw | Test building bit for active colony index (8dc6→035e) | inferred |  |
| `FUN_15eb_039e` | 9561 | 20 | mapdraw | Count owned buildings along parent chain (−707a) from start id | inferred | FUN_6cb2_1ba8 |
| `FUN_15eb_03d6` | 9581 | 20 | mapdraw | Count owned buildings along parent chain for explicit colony idx | inferred |  |
| `FUN_15eb_0410` | 9601 | 13 | mapdraw | Walk building parent chain (−707a) to root id | inferred |  |
| `FUN_15eb_0434` | 9614 | 22 | mapdraw | Highest owned building id still present along parent chain | inferred |  |
| `FUN_15eb_0470` | 9636 | 14 | mapdraw | Work-radius size class 2..4 from fortification chain (#10) | inferred | thunk_FUN_281f_0c5e |
| `FUN_15eb_0484` | 9650 | 20 | mapdraw | Map fortification size class to capacity constant (8/12/32) | inferred | thunk_FUN_281f_0c7c |
| `FUN_15eb_04c0` | 9670 | 30 | mapdraw | True if (x,y) is colony center or in fort-scaled work radius | inferred |  |
| `FUN_15eb_0544` | 9700 | 10 | mapdraw | Read nation treasury dword (id×0x13c−0x77ce) | inferred |  |
| `FUN_15eb_0556` | 9710 | 27 | mapdraw | Add to nation treasury with 0..0xF423F clamp | inferred |  |
| `FUN_15eb_0596` | 9737 | 9 | mapdraw | Subtract from nation treasury (negated 0556) | inferred |  |
| `FUN_15eb_05b2` | 9746 | 10 | mapdraw | Format nation gold into UI string (104b_01e8 + 'g') | inferred |  |
| `FUN_15eb_05cc` | 9756 | 13 | mapdraw | Present nation treasury value via 1009_01fc | inferred |  |
| `FUN_15eb_05e2` | 9769 | 20 | mapdraw | Map town-area relative (x,y) to 5×5 slot index 0..19 (−1 miss) | inferred |  |
| `FUN_15eb_0620` | 9789 | 19 | mapdraw | Test layer2 bit0x10 on colony town-area tile (plow/improve) | inferred |  |
| `FUN_15eb_0668` | 9808 | 16 | mapdraw | Set/clear layer2 bit0x10 on colony town-area tile | inferred | thunk_FUN_281f_0c90 |
| `FUN_15eb_06a6` | 9824 | 17 | mapdraw | Colonist-slot assigned to town-area tile (+0x70) or 0xFF | inferred |  |
| `FUN_15eb_06d2` | 9841 | 85 | mapdraw | Shared world-map / pedia draw entry | known | src/core/map.c |
| `FUN_15eb_08e6` | 9926 | 13 | mapdraw | True if unit type maps to a profession (table 0x30e≥0) | inferred |  |
| `FUN_15eb_0902` | 9939 | 10 | mapdraw | Default profession for unit type from table 0x30e | inferred |  |
| `FUN_15eb_0916` | 9949 | 10 | mapdraw | Map profession id → unit type byte (table 0x2f5) | inferred |  |
| `FUN_15eb_0924` | 9959 | 48 | mapdraw | Nth profession-capable unit in colony unit list (8d78) | inferred |  |
| `FUN_15eb_09c0` | 10007 | 36 | mapdraw | Recount colony unit tallies into 8d72/74/76 (cap capable@50) | inferred |  |
| `FUN_15eb_0a50` | 10043 | 15 | mapdraw | Warehouse capacity 100×(1+expansion at colony+0x95) | inferred |  |
| `FUN_15eb_0a76` | 10058 | 29 | mapdraw | Colony index at map (x,y), or −1 (assert if missing) | inferred | thunk_FUN_281f_07be |
| `FUN_15eb_0aec` | 10087 | 15 | mapdraw | Job→building-chain start id from table 0x2f4 (or −1) | inferred |  |
| `FUN_15eb_0b0c` | 10102 | 20 | mapdraw | Free warehouse headroom for cargo after stock/reserve | inferred |  |
| `FUN_15eb_0b52` | 10122 | 21 | mapdraw | Compute cargo shortfall/overflow scratch (−71ce/−71a6) | inferred |  |
| `FUN_15eb_0b96` | 10143 | 16 | mapdraw | Apply stock vs capacity for one cargo into shortfall scratch | inferred |  |
| `FUN_15eb_0bd4` | 10159 | 28 | mapdraw | Cargo headroom with building-chain 2/3 production cut | inferred |  |
| `FUN_15eb_0c52` | 10187 | 15 | mapdraw | True if cargo stock+turn exceeds warehouse capacity | inferred |  |
| `FUN_15eb_0c7a` | 10202 | 18 | mapdraw | Read packed colonist specialty nibble (+0x60) | inferred |  |
| `FUN_15eb_0cbc` | 10220 | 28 | mapdraw | Write packed colonist specialty nibble (+0x60, clamp 0..15) | inferred |  |
| `FUN_15eb_0d04` | 10248 | 108 | mapdraw | Remove colonist slot; compact jobs/specialty/area; −100 pop | inferred | thunk_FUN_281f_0a9c |
| `FUN_15eb_0d8e` | 10356 | 692 | mapdraw | Fill cargo-id list for leave-as gear (tools/muskets/horses); return count | inferred | asm switch on prof 0x13..0x18; Ghidra body overlapped |
| `FUN_15eb_0e18` | 11048 | 19 | mapdraw | Colonist profession: slot+0x20 or outside unit via 0924/0902 | inferred | thunk_FUN_281f_0c0e |
| `FUN_15eb_0e52` | 11067 | 16 | mapdraw | Colonist workplace/job: slot+0x40 or unit+0x315b | inferred | thunk_FUN_281f_0c54 |
| `FUN_15eb_0e8c` | 11083 | 21 | mapdraw | Set workplace/job (0x17→0x15); slot or outside unit | inferred | thunk_FUN_281f_0cae |
| `FUN_15eb_0ed4` | 11104 | 20 | mapdraw | Map specialty nibble to tier 0..3 (cuts at 4/8/15) | inferred |  |
| `FUN_15eb_0f1c` | 11124 | 39 | mapdraw | Resolve colonist people-band sprite/icon index | inferred | FUN_112b_0002 |
| `FUN_15eb_0fea` | 11163 | 19 | mapdraw | Classify profession change: stay/eject/admit/unit-role (0..3) | inferred |  |
| `FUN_15eb_1030` | 11182 | 22 | mapdraw | Set/clear colony flag bit in mask at +0x84 | inferred |  |
| `FUN_15eb_1068` | 11204 | 134 | mapdraw | Assign colony colonist to job (reassign/spawn unit/grow pop; warehouse cost adjust) | inferred |  |
| `FUN_15eb_1376` | 11338 | 20 | mapdraw | Count active-colony colonists whose current job equals param | inferred |  |
| `FUN_15eb_13ac` | 11358 | 20 | mapdraw | Count active-colony colonists whose specialty equals param | inferred |  |
| `FUN_15eb_13e2` | 11378 | 22 | mapdraw | Count colonists working a matching specialty job (<0x13) | inferred |  |
| `FUN_15eb_142a` | 11400 | 22 | mapdraw | Find nth colonist slot with given job byte (or −1) | inferred |  |
| `FUN_15eb_1476` | 11422 | 22 | mapdraw | Reseed DOS LCG from active colony tile XY (+0x8d80) | inferred |  |
| `FUN_15eb_14aa` | 11444 | 15 | mapdraw | Walk building parent chain (−0x707b) to root building id | inferred |  |
| `FUN_15eb_14d6` | 11459 | 10 | mapdraw | Map building/job id → table byte at DS:0x2ca | inferred |  |
| `FUN_15eb_14e4` | 11469 | 19 | mapdraw | If root building present, return its production-slot index (−0x716e) | inferred |  |
| `FUN_15eb_1526` | 11488 | 19 | mapdraw | Reverse of 14e4: building id owning production-slot, else −1 | inferred |  |
| `FUN_15eb_1568` | 11507 | 29 | mapdraw | Count workers in building (or −unit count when root is stockade) | inferred |  |
| `FUN_15eb_15c6` | 11536 | 19 | mapdraw | Building-upgrade depth for job (0/1/2 via parent −0x707a) | inferred |  |
| `FUN_15eb_1604` | 11555 | 21 | mapdraw | Colony production-panel layout sizes (w=200; h from difficulty) | inferred |  |
| `FUN_15eb_1646` | 11576 | 25 | mapdraw | Find 5×5 work-plot cell holding colonist id (via 06a6) | inferred |  |
| `FUN_15eb_169c` | 11601 | 16 | mapdraw | Redraw work-plot of colonist via shared 06d2 entry | inferred |  |
| `FUN_15eb_16c4` | 11617 | 23 | mapdraw | Job at 5×5 plot + out specialty (0e18/0e52); −1 if empty | inferred |  |
| `FUN_15eb_16fe` | 11640 | 20 | mapdraw | True if map tile terrain-class nibble is in [lo,hi] | inferred |  |
| `FUN_15eb_173e` | 11660 | 19 | mapdraw | Count of 8 neighbors whose terrain class is in [lo,hi] | inferred |  |
| `FUN_15eb_1782` | 11679 | 20 | mapdraw | True if map tile feature bits match mask (137f_0142) | inferred |  |
| `FUN_15eb_17ba` | 11699 | 19 | mapdraw | Count of 8 neighbors with matching feature-bit mask | inferred |  |
| `FUN_15eb_17fa` | 11718 | 53 | mapdraw | Resource-vs-job bonus table for field yield (special resources) | inferred |  |
| `FUN_15eb_18ec` | 11771 | 234 | mapdraw | Compose field yield for one 5×5 work plot (terrain/SoL/expert/mods) | inferred |  |
| `FUN_15eb_1d4c` | 12005 | 469 | mapdraw | Compose building/manufacturing yield for one colonist (out cargo idx) | inferred |  |
| `FUN_15eb_1f72` | 12474 | 221 | mapdraw | Recompute colony production totals (commons+plots+buildings→warehouse) | inferred |  |
| `FUN_15eb_23f2` | 12695 | 109 | mapdraw | Work-plot threat/feature flags (enemy/village/colony/center/invalid) | inferred |  |
| `FUN_15eb_268e` | 12804 | 22 | mapdraw | Fill 5×5 work-plot flag cache (−0x7210) once per colony view | inferred |  |
| `FUN_15eb_26e4` | 12826 | 63 | mapdraw | Fill 5×5 native-contact caches; optional map mark via 137f_0228 | inferred |  |
| `FUN_15eb_287e` | 12889 | 19 | mapdraw | Redraw all flagged 5×5 work plots via 06d2 | inferred |  |
| `FUN_15eb_28c8` | 12908 | 254 | mapdraw | Score/assign best work-plot job for colonist (trial 1068+18ec+06d2) | confirmed | [colonist_work_plot_28c8.md](turn/colonist_work_plot_28c8.md) |
| `FUN_15eb_2ea0` | 13162 | 39 | mapdraw | Auto-assign unplotted field workers via 28c8 (else job 0x0d) | inferred |  |
| `FUN_15eb_2f3c` | 13201 | 20 | mapdraw | Count map units with nonzero cargo/passenger capacity (0x5237) | inferred |  |
| `FUN_15eb_2f8e` | 13221 | 23 | mapdraw | Nth map unit with cargo/passenger capacity (or −1) | inferred |  |
| `FUN_15eb_2ff2` | 13244 | 18 | mapdraw | Read unit passenger/cargo type nibble at slot (0x3151) | inferred |  |
| `FUN_15eb_3040` | 13262 | 10 | mapdraw | Read unit cargo/passenger quantity byte at slot (0x3154) | inferred |  |
| `FUN_15eb_3054` | 13272 | 11 | mapdraw | Write unit cargo/passenger quantity byte at slot (0x3154) | inferred |  |
| `FUN_15eb_306a` | 13283 | 18 | mapdraw | Write unit passenger/cargo type nibble at slot (0x3151) | inferred |  |
| `FUN_15eb_30b8` | 13301 | 38 | mapdraw | Adjust unit cargo qty by type (merge/add slots up to 0x5237) | inferred |  |
| `FUN_15eb_317c` | 13339 | 28 | mapdraw | Remove unit cargo/passenger slot; compact; stash qty in 0x8dc4 | inferred |  |
| `FUN_15eb_3208` | 13367 | 30 | mapdraw | Free unit cargo capacity for type (empty slots×100 + partials) | inferred |  |
| `FUN_15eb_32a0` | 13397 | 26 | mapdraw | Find unit cargo slot index by type; stash qty in 0x8dc4 | inferred |  |
| `FUN_15eb_32f8` | 13423 | 31 | mapdraw | Decode build-menu index → kind (building/unit) + table idx | inferred |  |
| `FUN_15eb_334a` | 13454 | 24 | mapdraw | Play build-menu item sound (building −0x707e / unit 0x5230) | inferred |  |
| `FUN_15eb_33aa` | 13478 | 40 | mapdraw | Build-menu hammer cost (+ optional turns out) for item | inferred |  |
| `FUN_15eb_3454` | 13518 | 76 | mapdraw | Gate: can start job/construction (prereqs + warehouse mins) | inferred |  |
| `FUN_15eb_35d0` | 13594 | 29 | mapdraw | Move cargo from colony warehouse onto unit via 30b8 | inferred |  |
| `FUN_15eb_3620` | 13623 | 18 | mapdraw | Unload unit cargo slot back into colony warehouse (via 317c) | inferred |  |
| `FUN_15eb_3650` | 13641 | 111 | mapdraw | Build-menu eligibility for item (prereqs/pop/FF/coast/already-built) | inferred |  |
| `FUN_15eb_38ba` | 13752 | 21 | mapdraw | Count build-menu items currently eligible (3650 over 0..0x30) | inferred |  |
| `FUN_15eb_38e8` | 13773 | 21 | mapdraw | Nth eligible build-menu item id (via 3650) | inferred |  |
| `FUN_15eb_3930` | 13794 | 18 | mapdraw | Colony-area compose: flag cache + redraw + auto-assign + cargo-unit count | inferred |  |
| `FUN_15eb_394c` | 13812 | 10 | mapdraw | Refresh colony unit lists (09c0) then recompute production (1f72) | inferred |  |
| `FUN_15eb_3956` | 13822 | 10 | mapdraw | Full colony-view refresh compose (3930 + 394c) | inferred |  |
| `FUN_15eb_3960` | 13832 | 16 | mapdraw | Nation feature/FF bit test (table −0x77f1) | inferred |  |

### Segment `1984` (18 defs) — turn — Turn-owner chrome

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1984_0040` | 13848 | 12 | turn | Turn-owner fill blit via alt color table (DS:0x360) | inferred |  |
| `FUN_1984_006a` | 13860 | 13 | turn | Nation turn-owner 5x3 indicator (same chrome as 00aa) | inferred | src/core/turn.c |
| `FUN_1984_00aa` | 13873 | 13 | turn | Nation turn-owner 5x3 at (315,197) | known | src/core/turn.c |
| `FUN_1984_00e8` | 13886 | 16 | turn | If in-map, set focus tile DS:0x8540/0x853e | inferred |  |
| `FUN_1984_010a` | 13902 | 49 | turn | Toggle focus blink 0x929c; refresh focus-tile map chrome | inferred |  |
| `FUN_1984_029e` | 13951 | 27 | turn | Recenter viewport on tile; optional blink clear + chrome refresh | inferred |  |
| `FUN_1984_02fc` | 13978 | 43 | turn | If rect near viewport edge, recenter via 029e; return scrolled | inferred |  |
| `FUN_1984_03b2` | 14021 | 9 | turn | Ensure viewport contains single tile (x,y) | inferred |  |
| `FUN_1984_03ca` | 14030 | 25 | turn | Nudge focus by dir8; scroll + refresh chrome | inferred |  |
| `FUN_1984_043a` | 14055 | 10 | turn | Arm timed status overlay then draw (1009) | inferred |  |
| `FUN_1984_045a` | 14065 | 11 | turn | Append status-bar string from table 0x2dba[idx] | inferred |  |
| `FUN_1984_046e` | 14076 | 11 | turn | Status line: verb string + timed overlay at y=0x78 | inferred |  |
| `FUN_1984_0490` | 14087 | 21 | turn | Present/clear top status strip (7px); optional mouse region | inferred |  |
| `FUN_1984_04f6` | 14108 | 13 | turn | Tear down status chrome strip and overlays | inferred |  |
| `FUN_1984_053a` | 14121 | 17 | turn | Unit status chrome (nation+type+verb idx5 + nation name) | inferred |  |
| `FUN_1984_05b8` | 14138 | 17 | turn | Unit status chrome (same as 053a, verb idx7) | inferred |  |
| `FUN_1984_0636` | 14155 | 17 | turn | Unit status chrome (same as 053a, verb idx6) | inferred |  |
| `FUN_1984_06b4` | 14172 | 8 | turn | Stub: always return 0 | inferred |  |

### Segment `19ef` (4 defs) — platform — DOS LCG range helper

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_19ef_0008` | 14180 | 12 | platform | Seed DOS LCG from BIOS timer word (40:6c via 1c0c_0012), masked 0x7fff | known | FUN_1d1d_0df2; FUN_1c0c_0012; FUN_19ef_0032 |
| `FUN_19ef_001a` | 14192 | 9 | platform | Seed DOS LCG from caller word (mask 0x7fff → 1d1d_0df2) | known | FUN_1d1d_0df2 |
| `FUN_19ef_002c` | 14201 | 9 | platform | Near wrapper: reseed LCG from BIOS timer (calls 19ef_0008) | known | FUN_19ef_0008 |
| `FUN_19ef_0032` | 14210 | 18 | platform | DOS LCG range helper | known | src/core/dos_rng.c |

### Segment `19f6` (4 defs) — ui — Decimal number format / localized string blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_19f6_0002` | 14228 | 44 | ui | Overlay decimal digits of AX into zero-padded dest string (width DX; strcat "0"@0x368) | inferred |  |
| `FUN_19f6_00b0` | 14272 | 19 | ui | Copy string (*skip leading '*'); path-join via 1b4e if localize flag DS:0x36c | inferred |  |
| `FUN_19f6_00fa` | 14291 | 12 | ui | Format path/string via 00b0 into stack buf then open via 1d1d_04da | inferred |  |
| `FUN_19f6_0138` | 14303 | 9 | ui | Thin wrapper → 1b22_0022 (DOS file-exists probe) | inferred |  |

### Segment `1a0a` (3 defs) — ui — VGA page-flip / palette-cycle animation

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a0a_0004` | 14312 | 30 | ui | Init palette-cycle anim: seed 8 timers, sum lengths, set period/enable DS:0x372 | inferred |  |
| `FUN_1a0a_007a` | 14342 | 83 | ui | Tick palette-cycle: rotate RGB triples in anim list then DAC write (1c2e_0022) | inferred |  |
| `FUN_1a0a_01a6` | 14425 | 22 | ui | Page-flip memcpy between VGA A000 offs FD50↔FF00 (len 0x60; direction by param) | inferred |  |

### Segment `1a29` (4 defs) — platform — DOS timer INT vector install / restore

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a29_015b` | 14447 | 33 | platform | Install IRQ0 timer: save INT8, hook CS handler, PIT 0x7a8, point 267a→92e8 ticks | known | FUN_1c10_0008; FUN_1c0c_0006; FUN_1c0c_0022 |
| `FUN_1a29_01d1` | 14480 | 21 | platform | Restore prior INT8 + default PIT; retarget 267a to BIOS 40:6c | known | FUN_1a29_015b; FUN_1c10_0008 |
| `FUN_1a29_0209` | 14501 | 12 | platform | Arm custom timer alarm: set reload DS:0x92f6, clear elapsed 0x92f4 | inferred | FUN_1a29_015b |
| `FUN_1a29_021b` | 14513 | 18 | platform | Install far timer-tick callback at DS:0x92e4/0x92e6; clear tick latches | inferred | FUN_1262_0142 |

### Segment `1a4e` (2 defs) — ui — Blit pitch offset + viewport rect clip

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a4e_0008` | 14531 | 13 | ui | Pitched buffer offset: pitch*y + base + x (BX→pitch/base) | inferred |  |
| `FUN_1a4e_001c` | 14544 | 37 | ui | Clip rect against viewport; return 0 if any pixels remain visible | inferred |  |

### Segment `1a58` (18 defs) — platform — Mouse driver INT 33 show/hide / poll / mode setup

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a58_000d` | 14581 | 36 | platform | Show mouse cursor (INT33 AH=1 / soft nest→0456); nest DS:0xa899 | inferred |  |
| `FUN_1a58_0054` | 14617 | 28 | platform | Hide mouse cursor (INT33 AH=2 / soft nest→042d); nest DS:0xa899 | inferred |  |
| `FUN_1a58_008c` | 14645 | 81 | platform | Mouse INT 33 init / cursor mode setup (DS:0x83ac) | known |  |
| `FUN_1a58_01d9` | 14726 | 15 | platform | Set soft-cursor hotspot DS:0x590/0x592 (params & 0xf) | inferred |  |
| `FUN_1a58_026c` | 14741 | 58 | platform | Soft-cursor move: erase(042d), store scaled xy, redraw(0456); reentrancy DS:0x6d2 | inferred |  |
| `FUN_1a58_02ce` | 14799 | 14 | platform | Soft mode: mark cursor dirty (DS:0x58a=0xff, clear 0x58b) | inferred |  |
| `FUN_1a58_02e0` | 14813 | 22 | platform | Soft mode: force-refresh cursor via 026c if visible+pending (DS:0x58b) | inferred |  |
| `FUN_1a58_036b` | 14835 | 19 | platform | Scale INT33 raw CX/DX into DS:0x92fc/0x92fe via shift DS:0x598 | inferred |  |
| `FUN_1a58_038b` | 14854 | 36 | platform | Poll mouse xy/buttons (INT33); merge sticky flags | inferred |  |
| `FUN_1a58_03ce` | 14890 | 13 | platform | If soft cursor visible, call 02ce (dirty) before shape rebuild | inferred |  |
| `FUN_1a58_03e2` | 14903 | 16 | platform | If soft cursor visible, mode blit via DS:0x7e2 (post-shape update) | inferred |  |
| `FUN_1a58_042d` | 14919 | 13 | platform | Erase/restore under soft cursor; JMP DS:0x7da with save buf ES:DI | inferred |  |
| `FUN_1a58_0456` | 14932 | 47 | platform | Draw soft cursor: clip 16×16 at xy−hotspot, save-under, JMP DS:0x7dc | inferred |  |
| `FUN_1a58_054f` | 14979 | 14 | platform | Set cursor-shape far ptr + pitch (DS:0x5ac/0x5ae/0x5b0) | inferred |  |
| `FUN_1a58_0568` | 14993 | 18 | platform | Set shape source rect (L/T/R/B) + resolve ptr via FUN_1d1c_0000 | inferred |  |
| `FUN_1a58_0599` | 15011 | 15 | platform | Set shape cell origin (x,y); compute linear offsets DS:0x5be/0x5c0 | inferred |  |
| `FUN_1a58_05be` | 15026 | 21 | platform | If soft cursor overlaps dirty rect, restore under via DS:0x7de | inferred |  |
| `FUN_1a58_06fd` | 15047 | 13 | platform | Soft-cursor redraw after dirty blit; JMP DS:0x7e0 | inferred |  |

### Segment `1acb` (4 defs) — ui — Mouse hit-rect / button edge tracking

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1acb_0006` | 15060 | 17 | ui | Hit-test mouse xy (DS:0x7e8/0x7ea) inside [AX..BX]×[DX..param] rect | inferred |  |
| `FUN_1acb_0030` | 15077 | 18 | ui | Reset mouse/input latch (clear prev-xy/edges; poll 1a58_038b into DS:0x7e6) | inferred |  |
| `FUN_1acb_0056` | 15095 | 55 | ui | Sample mouse/buttons; set move/click/edge flags (DS:0x7ec..0x7f6) | inferred |  |
| `FUN_1acb_011a` | 15150 | 20 | ui | Busy-wait until custom timer word (DS:0x7fc/0x7fe) changes | inferred |  |

### Segment `1ade` (1 defs) — ui — VGA vsync + DAC palette write

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ade_0004` | 15170 | 42 | ui | Wait VGA retrace then program DAC RGB palette via 0x3c8/0x3c9 | known |  |

### Segment `1ae3` (2 defs) — platform — Stack clear + BIOS INT16 key-ready

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ae3_0006` | 15212 | 24 | platform | Zero-fill stack from DS:0x27e6 watermark up to SP; return bytes cleared | inferred |  |
| `FUN_1ae3_0042` | 15236 | 16 | platform | BIOS INT16 AH=01 key-ready: 1 if ZF clear, else 0 | known | FUN_281f_00f6; FUN_1ae7_0016 |

### Segment `1ae7` (2 defs) — platform — BIOS INT16 keyboard read / queue flush

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ae7_0016` | 15252 | 32 | platform | BIOS INT16 read next key (AH/AL); paired with 1ae3 key-ready / flush | known |  |
| `FUN_1ae7_0032` | 15284 | 21 | platform | Drain BIOS keyboard buffer (loop INT16 ready→read until empty) | known | FUN_291f_04a2; FUN_1ae3_0042; FUN_1ae7_0016 |

### Segment `1aea` (1 defs) — ui — Map keyboard / hotkey dispatch

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1aea_000c` | 15305 | 919 | ui | Map keyboard/hotkey dispatch (AX-0x110 switch into colony/mapdraw/sound) | inferred |  |

### Segment `1afb` (2 defs) — platform — String LF-terminate / NUL truncate

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1afb_000e` | 16224 | 15 | platform | NUL-truncate string at first LF (strchr 0x0A then write 0) | inferred | FUN_1d1d_10ea |
| `FUN_1afb_003c` | 16239 | 20 | platform | Append LF+NUL at end of string (strlen then write 0x0A,0) | inferred | FUN_1d1d_113c |

### Segment `1b01` (1 defs) — platform — Buffered far-buffer file/stream read

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b01_000e` | 16259 | 114 | platform | Buffered far-buffer file/stream read (seek/copy/refill; fail → 1d1d_0ec6) | inferred | FUN_2074_0008; FUN_1bdd_00e0 |

### Segment `1b22` (1 defs) — platform — DOS file-exists probe (INT21 3D/3E) + INT24 wrap

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b22_0022` | 16373 | 55 | platform | DOS file-exists probe (INT21 3D/3E) with INT24 critical-error handler wrap | known |  |

### Segment `1b2c` (2 defs) — platform — DOS char-stream string get / put

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b2c_0004` | 16428 | 24 | platform | Read NUL-terminated string from DOS char stream (max ~79; trailing getc) | inferred | FUN_1d1d_0786 |
| `FUN_1b2c_0040` | 16452 | 21 | platform | Write NUL-terminated string to DOS char stream then Ctrl-Z (0x1A) | inferred | FUN_1d1d_0758 |

### Segment `1b32` (2 defs) — platform — Filename extension / basename helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b32_000e` | 16473 | 18 | platform | Ensure filename has extension: if no '.', strcat default then suffix | inferred | FUN_1d1d_10ea; FUN_1d1d_11b4 |
| `FUN_1b32_005c` | 16491 | 23 | platform | Replace/append filename extension (truncate at '.', strcat new ext) | inferred | FUN_1b32_000e |

### Segment `1b4e` (1 defs) — platform — Path join: cwd + \ + name

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b4e_0004` | 16514 | 25 | platform | Path join: getcwd + optional '\' + name into out buffer | inferred | FUN_1d1d_117e; FUN_1d1d_11b4 |

### Segment `1b57` (1 defs) — platform — Ltrim spaces/tabs then strcpy

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b57_0002` | 16539 | 23 | platform | Ltrim leading spaces/tabs then strcpy back into dest | inferred | FUN_2094_000a; FUN_1d1d_117e |

### Segment `1b5e` (1 defs) — ui — Mouse cursor region update (17x17)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b5e_0000` | 16562 | 63 | ui | Update soft mouse cursor hotspot/shape from 17×17 mask (1c36 + 1b8f blit) | inferred |  |

### Segment `1b70` (2 defs) — ui — Mouse viewport / region setup (1a58)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b70_0002` | 16625 | 13 | ui | Bind mouse cursor shape to current viewport (1a58_054f/0568/0599 @2da8..2dae) | inferred |  |
| `FUN_1b70_003a` | 16638 | 17 | ui | Video restore/flush around blit | inferred |  |

### Segment `1b78` (1 defs) — ui — Mouse cursor region update (16x16)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b78_0000` | 16655 | 52 | ui | Update soft mouse cursor hotspot/shape from 16×16 sprite (1c36 + optional 1cd8) | inferred |  |

### Segment `1b8b` (1 defs) — platform — Set BIOS equipment video-mode bits (40:10)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b8b_0004` | 16707 | 16 | platform | Set BIOS equipment video bits at 40:10 (mono 0x30 if AX==7 else 0x20) | known |  |

### Segment `1b8d` (1 defs) — ui — Solid-rect fill thunk → 1b9e

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b8d_0004` | 16723 | 12 | ui | Filled-rect blit wrapper→1b9e | inferred |  |

### Segment `1b8f` (1 defs) — ui — Pitched buffer rect blit/copy

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b8f_0006` | 16735 | 105 | ui | Pitched buffer rect copy (word/byte rows; 1a4e+1c05 far ptrs) | inferred |  |

### Segment `1b9e` (1 defs) — ui — Solid-color pitched rect fill

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b9e_000a` | 16840 | 80 | ui | Solid-color pitched rect fill (clip via 1a4e_001c; color=param_1) | inferred |  |

### Segment `1baa` (1 defs) — ui — Pitched buffer rect blit/copy (variant)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1baa_0006` | 16920 | 101 | ui | Pitched buffer rect blit/copy variant (src/dst pitches; sibling of 1b8f) | inferred |  |

### Segment `1bb9` (1 defs) — ui — Put pixel via pitch helper (1a4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bb9_000a` | 17021 | 14 | ui | Put pixel via pitch helper (1a4e_0008) ← BL | inferred |  |

### Segment `1bbb` (1 defs) — ui — Get pixel via pitch helper (1a4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bbb_0006` | 17035 | 13 | ui | Get pixel via pitch helper (1a4e_0008) | inferred |  |

### Segment `1bbc` (1 defs) — ui — Horizontal span fill in pitched buffer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bbc_000c` | 17048 | 31 | ui | Horizontal span fill in pitched buffer (clipped to width) | inferred |  |

### Segment `1bc3` (1 defs) — ui — Vertical span fill in pitched buffer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bc3_0006` | 17079 | 31 | ui | Vertical span fill in pitched buffer (stride=pitch) | inferred |  |

### Segment `1bca` (1 defs) — ui — Rect outline via H/V span fills

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bca_0002` | 17110 | 18 | ui | Rectangle outline via two H fills (1bbc) + two V fills (1bc3) | inferred |  |

### Segment `1bd4` (1 defs) — ui — Color replace in pitched rect

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bd4_0006` | 17128 | 40 | ui | Color-replace param_2→param_1 across pitched rect | inferred |  |

### Segment `1bdd` (2 defs) — platform — Temp numbered file create / write slots

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bdd_0002` | 17168 | 59 | platform | Create numbered temp file into free slot (0..9); write payload; return slot | inferred | FUN_1bdd_00e0; FUN_19f6_0002 |
| `FUN_1bdd_00e0` | 17227 | 39 | platform | Read numbered temp file via 1b01 into dests; free slot and delete if ok | inferred | FUN_1bdd_0002; FUN_1b01_000e |

### Segment `1bf5` (1 defs) — ui — Tiled rect blit loop

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bf5_0000` | 17266 | 52 | ui | Tiled rect blit loop: stamp src tile across dest via repeated 1baa_0006 | inferred |  |

### Segment `1c05` (1 defs) — platform — Normalize far pointer (seg:off)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c05_0004` | 17318 | 8 | platform | Normalize far ptr → seg+=(off>>4), off&=0xf | inferred |  |

### Segment `1c06` (1 defs) — platform — Parse 0x/0b numeric literal prefix

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c06_000c` | 17326 | 27 | platform | Parse numeric literal: 0x→hex, 0b→bin, else default decimal atoi | inferred | FUN_209a_000a; FUN_20a0_000e |

### Segment `1c0c` (3 defs) — platform — Timer / tick word readers (custom + BIOS 046c)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c0c_0006` | 17353 | 12 | platform | Read custom timer tick word via DS:0x267a far ptr | inferred |  |
| `FUN_1c0c_0012` | 17365 | 8 | platform | Read BIOS timer dword at 40:6c/6e | known | FUN_1c0c_0006; FUN_19ef_0008 |
| `FUN_1c0c_0022` | 17373 | 10 | platform | Read custom IRQ0 tick dword at DS:0x8338/0x833a | inferred | FUN_1a29_015b; FUN_1c0c_0006 |

### Segment `1c10` (1 defs) — platform — Program PIT channel-0 divisor

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c10_0008` | 17383 | 14 | platform | Program PIT channel-0 divisor via ports 0x43/0x40 | known |  |

### Segment `1c11` (1 defs) — ui — 2-bit packed glyph decode to pitched buffer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c11_000c` | 17397 | 128 | ui | Decode 2-bit packed glyphs from font sheet into pitched buffer at xy | inferred |  |

### Segment `1c28` (1 defs) — ui — Store text-draw color words at DS:269e

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c28_000a` | 17525 | 17 | ui | Store text-draw color nibbles at DS:0x269e..0x26a1 (AL/DL/BL/param) | inferred |  |

### Segment `1c2a` (1 defs) — ui — String pixel-width via glyph table

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c2a_0006` | 17542 | 30 | ui | Sum string pixel-width from glyph-width table (param_2+ch) | inferred |  |

### Segment `1c2e` (2 defs) — ui — VGA vsync wait + DAC palette write

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c2e_000e` | 17572 | 16 | ui | Wait for VGA vertical retrace (port 0x3da bit3 edge) | inferred |  |
| `FUN_1c2e_0022` | 17588 | 46 | ui | Wait VGA retrace then program DAC RGB palette via 0x3c8/0x3c9 | known |  |

### Segment `1c36` (1 defs) — ui — Soft-sprite / cursor RLE blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c36_000a` | 17634 | 176 | ui | RLE soft-sprite blit from object table (+0x36); skip transparent -3 | inferred |  |

### Segment `1c56` (1 defs) — ui — Scaled/dithered sprite blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c56_0004` | 17810 | 231 | ui | Scaled/dithered RLE sprite blit (percent scale table → dest) | inferred |  |

### Segment `1c83` (1 defs) — ui — Sprite scale to destination size/pos

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c83_0002` | 18041 | 28 | ui | Fill dest rect size/pos from sprite dims × percent scale | inferred |  |

### Segment `1c89` (1 defs) — ui — Soft-sprite blit (sibling of 1c36)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c89_0006` | 18069 | 176 | ui | RLE soft-sprite underlay blit (write only where dest==0) | inferred |  |

### Segment `1caa` (1 defs) — ui — Scaled sprite blit (sibling of 1c56)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1caa_0004` | 18245 | 231 | ui | Scaled RLE underlay blit (dest==0 only; sibling of 1c56) | inferred |  |

### Segment `1cd8` (1 defs) — ui — Soft-sprite blit (3rd sibling)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1cd8_0004` | 18476 | 178 | ui | RLE silhouette fill: paint solid color param_1 via sprite mask | inferred |  |

### Segment `1cf8` (1 defs) — platform — Insertion-sort parallel word+byte arrays

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1cf8_000a` | 18654 | 88 | platform | Insertion-sort parallel word keys + byte payload arrays | inferred | FUN_291f_0ed0; FUN_1d05_0000 |

### Segment `1d05` (1 defs) — platform — Insertion-sort parallel byte+byte arrays

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d05_0000` | 18742 | 85 | platform | Insertion-sort parallel byte keys + byte payload arrays | inferred | FUN_1cf8_000a |

### Segment `1d11` (2 defs) — ui — INT10 mode set + Mode13h far-buffer blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d11_0000` | 18827 | 17 | ui | Stash INT10 mode word at DS:0x83aa; optionally invoke INT10 | inferred |  |
| `FUN_1d11_001c` | 18844 | 70 | ui | Mode13h far-buffer → A000 blit (pitch 0x140; bank wrap 0x7000) | inferred |  |

### Segment `1d1c` (1 defs) — platform — VGA Mode13h A000 address helper (y×320+x)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d1c_0000` | 18914 | 36 | platform | VGA Mode13h: ES=A000, DI=y×320+x from (CX,DX); used by soft-cursor shape setup | known | FUN_1a58_0456; FUN_1a58_0568 |

### Segment `1d1d` (127 defs) — platform — High-density + platform-adjacent (incl. LCG)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d1d_0016` | 18950 | 10 | platform | delay(ms) — sync BIOS 40:40 tick, lazy-calibrate via 00ca, then spin | known | FUN_1d1d_00ca; FUN_1d1d_0132 |
| `FUN_1d1d_00ca` | 18960 | 23 | platform | Calibrate delay constants (CS:0012/0014) via two 00ff VBLANK timings | known | FUN_1d1d_00ff; FUN_1d1d_0016 |
| `FUN_1d1d_00ff` | 18983 | 38 | platform | CRT delay calibrate inner: count loops across CGA/VGA status port 3DAh VBLANK | known | FUN_1d1d_00ca |
| `FUN_1d1d_0132` | 19021 | 11 | platform | Far wrapper → delay(ms) 0016 | inferred | FUN_1d1d_0016 |
| `FUN_1d1d_0150` | 19032 | 62 | platform | CRT/_start: INT21 AH=30/4A, BSS/init, 0248 setup, main 281f_0000, exit 030d | known | FUN_1d1d_0248; FUN_1d1d_030d; FUN_281f_0000 |
| `FUN_1d1d_0223` | 19094 | 107 | platform | abort — cleanup (1242/14c9) then exit vector [0x2774] with code 0xFF | inferred | FUN_1d1d_03d0; FUN_1d1d_1242; FUN_1d1d_14c9 |
| `FUN_1d1d_0248` | 19201 | 94 | platform | CRT pre-main: hook INT0 (AH=35/25), NO87 env parse, IOCTL AH=44 on std handles, run init tables | known | FUN_1d1d_03bd; FUN_1d1d_0150 |
| `FUN_1d1d_030d` | 19295 | 22 | platform | exit — run atexit/exit tables (03bd×4), 126a, restore vectors 0390, INT21 AH=4C | known | FUN_1d1d_03bd; FUN_1d1d_0390 |
| `FUN_1d1d_0390` | 19317 | 20 | platform | On exit: optional FPU callback, restore INT0 (+optional vector) via INT21 AH=25 | known | FUN_1d1d_030d |
| `FUN_1d1d_03bd` | 19337 | 25 | platform | Walk SI..DI function-pointer table; call each nonzero entry (startup/atexit) | inferred | FUN_1d1d_0248; FUN_1d1d_030d |
| `FUN_1d1d_03d0` | 19362 | 28 | platform | __chkstk — grow/probe SP by AX above DS:27e6 watermark; else abort/handler | known | FUN_1d1d_0223 |
| `FUN_1d1d_03f4` | 19390 | 41 | platform | fclose — flush FILE, DOS close handle (1e7a), clear flags | inferred |  |
| `FUN_1d1d_04ae` | 19431 | 18 | platform | fopen core — alloc FILE (1e46) then open via 16fc (path/mode/share) | inferred | FUN_1d1d_04da; FUN_1d1d_1e46; FUN_1d1d_16fc |
| `FUN_1d1d_04da` | 19449 | 9 | platform | fopen(path,mode) — wrapper → 04ae with share/flags=0 | inferred |  |
| `FUN_1d1d_04f0` | 19458 | 14 | platform | fprintf(FILE*,fmt,...) — lock 17e4, format 196e, unlock 1857 | inferred | FUN_1d1d_196e; FUN_1d1d_0712; FUN_1d1d_17e4; FUN_1d1d_1857 |
| `FUN_1d1d_0528` | 19472 | 72 | platform | fread(ptr,size,n,FILE*) — buffered/direct read; EOF/error flag bits | inferred | FUN_1d1d_0d82; FUN_1d1d_1556; FUN_1d1d_1f14 |
| `FUN_1d1d_060c` | 19544 | 70 | platform | fwrite(ptr,size,n,FILE*) — buffered/direct write; flush 1896 when needed | inferred | FUN_1d1d_0d82; FUN_1d1d_15ec; FUN_1d1d_1896; FUN_1d1d_1ffe |
| `FUN_1d1d_0712` | 19614 | 14 | platform | printf — format to stdout FILE DS:0x2916 via 196e | inferred |  |
| `FUN_1d1d_0750` | 19628 | 20 | platform | putchar — putc into stdout FILE at DS:0x2916 | inferred | FUN_1d1d_0758; FUN_1d1d_15ec |
| `FUN_1d1d_0758` | 19648 | 20 | platform | putc(c,FILE*) — buffer or flush via 15ec | inferred | FUN_1d1d_15ec; FUN_1b2c_0040 |
| `FUN_1d1d_0786` | 19668 | 25 | platform | getc(FILE*) — from buffer or refill via 1556 | inferred | FUN_1d1d_1556; FUN_1b2c_0004 |
| `FUN_1d1d_07a4` | 19693 | 56 | platform | strcat — append null-terminated string onto dest | inferred |  |
| `FUN_1d1d_07e4` | 19749 | 46 | platform | strcpy — null-terminated string copy (word-aligned) | inferred |  |
| `FUN_1d1d_0816` | 19795 | 44 | platform | strcmp — null-terminated string compare (CMPSB) | inferred |  |
| `FUN_1d1d_0842` | 19839 | 19 | platform | strlen — scan to NUL (SCASB) | inferred |  |
| `FUN_1d1d_085e` | 19858 | 46 | platform | strncat — append ≤n chars from src onto dest (NUL-term) | inferred |  |
| `FUN_1d1d_0894` | 19904 | 30 | platform | strncpy — copy ≤n chars; pad with NUL if src shorter | inferred |  |
| `FUN_1d1d_08bc` | 19934 | 77 | platform | strncmp — compare ≤n chars; return <0/0/>0 | inferred |  |
| `FUN_1d1d_08fa` | 20011 | 61 | platform | itoa — int→string (radix; signed if base 10) | inferred |  |
| `FUN_1d1d_0916` | 20072 | 9 | platform | ltoa — signed long→string (BL=1 entry into 2526 radix converter) | inferred | FUN_1d1d_2526; FUN_1d1d_08fa |
| `FUN_1d1d_092c` | 20081 | 13 | platform | Normalize key code (−0x20 if DS table bit2 at code+0x27ed) | inferred |  |
| `FUN_1d1d_0942` | 20094 | 24 | platform | getenv — scan environ at DS:27d3 for NAME=; return value ptr | inferred | FUN_1d1d_0842; FUN_1d1d_08bc |
| `FUN_1d1d_09a2` | 20118 | 22 | platform | ftell — FILE pos via 2290 → *out dword; fail → −1 | inferred |  |
| `FUN_1d1d_09ca` | 20140 | 65 | platform | fgets(buf,n,FILE*) — read line (stop at LF/EOF); NUL-terminate | inferred | FUN_1d1d_1556 |
| `FUN_1d1d_0a3e` | 20205 | 35 | platform | fseek — seek FILE (origin; flush 1896 + DOS lseek 1e9a) | inferred |  |
| `FUN_1d1d_0abe` | 20240 | 11 | platform | fsetpos(FILE*,fpos_t*) — fseek to saved dword offset (origin SEEK_SET) | inferred | FUN_1d1d_0a3e |
| `FUN_1d1d_0ad8` | 20251 | 21 | platform | rewind(FILE*) — flush 1896, clear EOF/error flags, DOS lseek 0 | inferred | FUN_1d1d_1896; FUN_1d1d_1e9a |
| `FUN_1d1d_0b1c` | 20272 | 21 | platform | setbuf(FILE*,buf) — NULL→_IONBF else 512-byte buffer via setvbuf 2406 | inferred | FUN_1d1d_2406 |
| `FUN_1d1d_0b48` | 20293 | 28 | platform | sprintf into buffer (FILE-like DS:0x2d30 → FUN_1d1d_196e) | inferred |  |
| `FUN_1d1d_0ba2` | 20321 | 30 | platform | filelength(fd) — lseek CUR/END/restore via 1e9a; return size or −1 | inferred | FUN_1d1d_1e9a |
| `FUN_1d1d_0c56` | 20351 | 32 | platform | strchr — find char in string (else null) | inferred |  |
| `FUN_1d1d_0c80` | 20383 | 31 | platform | stricmp/strcmpi — case-insensitive string compare | inferred |  |
| `FUN_1d1d_0cc2` | 20414 | 43 | platform | strnicmp — case-insensitive compare of ≤n chars | inferred |  |
| `FUN_1d1d_0d1a` | 20457 | 31 | platform | strrchr — find last occurrence of char in string (else null) | inferred | FUN_1d1d_10ea |
| `FUN_1d1d_0d46` | 20488 | 17 | platform | strlwr — in-place ASCII A–Z → a–z | inferred | FUN_1d1d_0d64 |
| `FUN_1d1d_0d64` | 20505 | 17 | platform | strupr — in-place ASCII a–z → A–Z | inferred |  |
| `FUN_1d1d_0d82` | 20522 | 37 | platform | memcpy (aligned word then tail) | inferred |  |
| `FUN_1d1d_0dae` | 20559 | 31 | platform | memset / byte-fill (aligned word then tail) | inferred |  |
| `FUN_1d1d_0ddc` | 20590 | 11 | platform | labs — 32-bit absolute value (NEG DX:AX if sign word < 0) | known |  |
| `FUN_1d1d_0df2` | 20601 | 12 | platform | srand — seed DOS LCG state at DS:0x28ee/0x28f0 (hi word 0) | known | FUN_1d1d_0e04; FUN_19ef_0008; FUN_19ef_001a |
| `FUN_1d1d_0e04` | 20613 | 16 | platform | DOS LCG core | known | src/core/dos_rng.c |
| `FUN_1d1d_0e2c` | 20629 | 12 | platform | searchpath — locate file on PATH (chkstk + 2586 getenv/concat probe) | inferred | FUN_1d1d_2586; FUN_1d1d_03d0; FUN_1d1d_0942 |
| `FUN_1d1d_0e4a` | 20641 | 13 | platform | unlink/remove — INT21 AH=41 delete file; status via 1500 | known | FUN_1d1d_1500 |
| `FUN_1d1d_0e58` | 20654 | 19 | platform | findnext — INT21 AH=4F (shared DTA save/restore path with 0e63) | known | FUN_1d1d_0e63; FUN_1d1d_1508 |
| `FUN_1d1d_0e63` | 20673 | 19 | platform | findfirst — INT21 AH=4E with AH=2F/1A DTA get/set around search | known | FUN_1d1d_0e58; FUN_1d1d_1508 |
| `FUN_1d1d_0e96` | 20692 | 25 | platform | DOS read — INT21 AH=3F (handle/buf/len); optional [0x2b18] hook | known | FUN_1d1d_0e9d; FUN_1d1d_1508 |
| `FUN_1d1d_0e9d` | 20717 | 25 | platform | DOS write — INT21 AH=40 (handle/buf/len); optional [0x2b18] hook | known | FUN_1d1d_0e96; FUN_1d1d_1508 |
| `FUN_1d1d_0ec6` | 20742 | 65 | platform | Signed 32-bit division helper | inferred |  |
| `FUN_1d1d_0f60` | 20807 | 12 | platform | 32-bit multiply helper | inferred |  |
| `FUN_1d1d_0f92` | 20819 | 14 | platform | In-place 32-bit /= — *dividend /= divisor via signed div 0ec6 | inferred | FUN_1d1d_0ec6 |
| `FUN_1d1d_0fb2` | 20833 | 51 | platform | memcpy — far/huge copy (seg wrap +0x1000 on offs overflow) | inferred |  |
| `FUN_1d1d_1010` | 20884 | 35 | platform | strchr (far) — find first char in string (else null) | inferred | FUN_1d1d_0c56 |
| `FUN_1d1d_103e` | 20919 | 34 | platform | stricmp (far) — case-insensitive string compare | inferred | FUN_1d1d_0c80 |
| `FUN_1d1d_1084` | 20953 | 49 | platform | strncmp (far) — compare ≤n chars | inferred | FUN_1d1d_08bc |
| `FUN_1d1d_10c0` | 21002 | 33 | platform | strncpy (far) — copy ≤n chars; NUL-pad if src shorter | inferred | FUN_1d1d_0894 |
| `FUN_1d1d_10ea` | 21035 | 34 | platform | strrchr (far) — find last char in string (else null) | inferred | FUN_1d1d_0d1a; FUN_1b32_000e |
| `FUN_1d1d_1118` | 21069 | 18 | platform | strupr (far) — in-place ASCII a–z → A–Z | inferred | FUN_1d1d_0d64 |
| `FUN_1d1d_113c` | 21087 | 20 | platform | strlen — count chars until NUL | inferred |  |
| `FUN_1d1d_117e` | 21107 | 50 | platform | strcpy — null-terminated string copy (word-aligned) | inferred |  |
| `FUN_1d1d_11b4` | 21157 | 61 | platform | strcat — append null-terminated string onto dest | inferred |  |
| `FUN_1d1d_11fa` | 21218 | 48 | platform | memset — far byte-fill (word then tail; split at seg end) | inferred |  |
| `FUN_1d1d_1242` | 21266 | 15 | platform | CRT abort emit: write msgs 0xFC then 0xFF via 14c9; optional far hook DS:0x28f2 | inferred | FUN_1d1d_14c9; FUN_1d1d_0223 |
| `FUN_1d1d_1264` | 21281 | 9 | platform | Abort wrapper — AX=2 then jump FUN_1d1d_0223 | inferred | FUN_1d1d_0223 |
| `FUN_1d1d_126a` | 21290 | 33 | platform | Validate PSP XOR checksum (0x42 bytes ^0x55); fail → abort + msg 1 | inferred | FUN_1d1d_1242; FUN_1d1d_14c9 |
| `FUN_1d1d_128e` | 21323 | 259 | platform | CRT startup: INT21 AH=30 DOS ver; build argv/env from PSP; jump app entry | inferred | FUN_1d1d_1420 |
| `FUN_1d1d_1420` | 21582 | 82 | platform | Build CRT environ[] pointer vector from DOS env block (via 26dc) | inferred | FUN_1d1d_26dc; FUN_1d1d_128e |
| `FUN_1d1d_149e` | 21664 | 29 | platform | Lookup CRT message string by id in table DS:0x2b42 | inferred | FUN_1d1d_14c9 |
| `FUN_1d1d_14c9` | 21693 | 30 | platform | Write CRT msg id to stderr (INT21 AH=40 BX=2) after 149e lookup | inferred | FUN_1d1d_149e |
| `FUN_1d1d_1500` | 21723 | 14 | platform | DOS CF check: success→0 else set errno (1528) and return −1 | inferred | FUN_1d1d_1528; FUN_1d1d_1515 |
| `FUN_1d1d_1508` | 21737 | 15 | platform | DOS CF check: success→0 else map errno (1528) and return AL | inferred | FUN_1d1d_1528 |
| `FUN_1d1d_1515` | 21752 | 13 | platform | On DOS CF set errno via 1528; return −1 (else passthrough) | inferred | FUN_1d1d_1528 |
| `FUN_1d1d_1528` | 21765 | 32 | platform | Map DOS error AL→errno at DS:0x27ac (table DS:0x28fa) | inferred |  |
| `FUN_1d1d_1556` | 21797 | 49 | platform | getc — buffered FILE get char; refill via DOS read 1f14 | inferred | FUN_1d1d_1f14; FUN_1d1d_2702 |
| `FUN_1d1d_15ec` | 21846 | 58 | platform | putc — buffered FILE put char; flush via DOS write 1ffe | inferred | FUN_1d1d_1ffe; FUN_1d1d_1e9a |
| `FUN_1d1d_16d0` | 21904 | 18 | platform | Free FILE heap buffer (flag&8) via 2c44; clear FILE ptrs | inferred | FUN_1d1d_2c44 |
| `FUN_1d1d_16fc` | 21922 | 76 | platform | fopen mode parse (r/w/a/+ /t/b) then _open 2746; init FILE | inferred | FUN_1d1d_2746; FUN_1d1d_04ae |
| `FUN_1d1d_17e4` | 21998 | 35 | platform | Ensure stdin/stdout/stderr FILE get 0x200 heap buffer | inferred | FUN_1d1d_2c65 |
| `FUN_1d1d_1857` | 22033 | 20 | platform | Flush console/device FILE if flag&0x10 + device bit; optional clear buf | inferred | FUN_1d1d_1896 |
| `FUN_1d1d_1896` | 22053 | 32 | platform | fflush — write FILE buffer via 1ffe; NULL FILE → flushall 1912 | inferred | FUN_1d1d_1ffe; FUN_1d1d_1912; FUN_1d1d_0a3e |
| `FUN_1d1d_1912` | 22085 | 30 | platform | flushall — fflush each open FILE in table DS:0x290e..0x2a4e | inferred | FUN_1d1d_1896 |
| `FUN_1d1d_196e` | 22115 | 28 | platform | printf/sprintf engine — dispatch format chars via table DS:0x2a56 | inferred | FUN_1d1d_0712; FUN_1d1d_0b48; FUN_1d1d_1daa |
| `FUN_1d1d_1d79` | 22143 | 15 | platform | printf va_arg — LODSW next int from [BP+0xa] list | inferred |  |
| `FUN_1d1d_1d81` | 22158 | 19 | platform | printf va_arg — LODSW×2 next long from [BP+0xa] list | inferred |  |
| `FUN_1d1d_1daa` | 22177 | 31 | platform | printf putc — store AL into FILE buffer (or flush via 15ec) | inferred | FUN_1d1d_15ec; FUN_1d1d_1dd4 |
| `FUN_1d1d_1dd4` | 22208 | 26 | platform | printf emit — write CX chars from ES:DI via putc (1daa) | inferred |  |
| `FUN_1d1d_1df2` | 22234 | 26 | platform | printf pad — write CX copies of DL via putc (1daa) | inferred |  |
| `FUN_1d1d_1e0e` | 22260 | 34 | platform | printf itoa — DX:AX÷radix→ASCII digits reverse (STOSB) | inferred |  |
| `FUN_1d1d_1e3f` | 22294 | 8 | platform | Shared far epilogue — POP DI/SI; leave; RETF | inferred |  |
| `FUN_1d1d_1e46` | 22302 | 24 | platform | Alloc free FILE slot from CRT table DS:0x290e..0x2a4e | inferred | FUN_1d1d_04ae; FUN_1d1d_16fc |
| `FUN_1d1d_1e7a` | 22326 | 21 | platform | DOS INT21 AH=3E close handle; clear fd table byte | inferred |  |
| `FUN_1d1d_1e9a` | 22347 | 54 | platform | DOS INT21 AH=42 lseek (origin + signed offset checks) | inferred |  |
| `FUN_1d1d_1f14` | 22401 | 99 | platform | DOS INT21 AH=3F read handle; text mode CR/LF→LF translate | inferred | FUN_1d1d_1556; FUN_1d1d_1515 |
| `FUN_1d1d_1ffe` | 22500 | 95 | platform | DOS write handle; text mode LF→CRLF then INT21 AH=40 | inferred | FUN_1d1d_20b2; FUN_1d1d_210a; FUN_1d1d_1896 |
| `FUN_1d1d_20b2` | 22595 | 30 | platform | Flush stacked write chunk via INT21 AH=40; fail → 1515 | inferred | FUN_1d1d_1ffe; FUN_1d1d_1515 |
| `FUN_1d1d_20fc` | 22625 | 9 | platform | Write-fail epilogue → set errno via 1515 | inferred | FUN_1d1d_1515 |
| `FUN_1d1d_210a` | 22634 | 19 | platform | DOS INT21 AH=40 write CX bytes (binary/raw path); then 1515 | inferred | FUN_1d1d_1ffe; FUN_1d1d_1515 |
| `FUN_1d1d_213e` | 22653 | 65 | platform | Near-heap grow: expand arena size then AH=4A via 21ca | inferred | FUN_1d1d_21ca; FUN_1d1d_221b; FUN_1d1d_2c65 |
| `FUN_1d1d_21ca` | 22718 | 32 | platform | DOS INT21 AH=4A resize mem block (paragraphs) | inferred | FUN_1d1d_213e |
| `FUN_1d1d_221b` | 22750 | 20 | platform | Walk near-heap free-list to 0xFFFE end marker | inferred | FUN_1d1d_213e |
| `FUN_1d1d_223c` | 22770 | 34 | platform | atoi — parse signed decimal integer from string | inferred |  |
| `FUN_1d1d_2290` | 22804 | 80 | platform | ftell core — FILE logical pos via lseek 1e9a ± buffer adjust | inferred | FUN_1d1d_09a2; FUN_1d1d_1e9a |
| `FUN_1d1d_2406` | 22884 | 48 | platform | setvbuf — attach/alloc FILE buffer (_IOFBF/_IONBF/_IOLBF-style) | inferred | FUN_1d1d_1896; FUN_1d1d_16d0; FUN_1d1d_2c65 |
| `FUN_1d1d_2526` | 22932 | 70 | platform | ultoa/ltoa — convert long to ASCII digits in buffer (radix) | inferred | FUN_1d1d_08fa; FUN_1d1d_196e |
| `FUN_1d1d_2586` | 23002 | 48 | platform | Search PATH dirs for file then spawn via 2b32; free temp path | inferred | FUN_1d1d_2b32; FUN_1d1d_0942; FUN_1d1d_2c65 |
| `FUN_1d1d_26dc` | 23050 | 22 | platform | Heap alloc with temp brk=0x400 via 2c65; fail → abort 0223 | inferred | FUN_1d1d_2c65; FUN_1d1d_0223; FUN_1d1d_1420 |
| `FUN_1d1d_2702` | 23072 | 26 | platform | Alloc/attach default 0x200 FILE buffer (or unbuffered fallback) | inferred | FUN_1d1d_2c65; FUN_1d1d_1556 |
| `FUN_1d1d_2746` | 23098 | 136 | platform | DOS open/creat (INT21 AH=3D/3C) with share/text flags; fill fd table | inferred | FUN_1d1d_16fc; FUN_1d1d_1515; FUN_1d1d_28f1 |
| `FUN_1d1d_28f1` | 23234 | 8 | platform | Map DOS open access bits (CX vs mask DS:0x27ae) into CL flags | inferred | FUN_1d1d_2746 |
| `FUN_1d1d_2902` | 23242 | 57 | platform | Stack free bytes remaining above watermark DS:0x27e6 | inferred | FUN_1d1d_03d0 |
| `FUN_1d1d_2922` | 23299 | 59 | platform | Near-heap malloc — carve block from free-list (arena DS:0x2778) | inferred | FUN_1d1d_2c65; FUN_1d1d_2c44 |
| `FUN_1d1d_299e` | 23358 | 156 | platform | spawn/exec setup: env block 2c8e, open COM/EXE, hand off 2f06 | inferred | FUN_1d1d_2c8e; FUN_1d1d_2f06; FUN_1d1d_2746 |
| `FUN_1d1d_2b32` | 23514 | 54 | platform | exec path: try .COM/.EXE/.BAT suffixes then spawn 299e | inferred | FUN_1d1d_299e; FUN_1d1d_328a |
| `FUN_1d1d_2c44` | 23568 | 18 | platform | free — mark near-heap block free (size/1; update rover DS:0x2780) | inferred | FUN_1d1d_2c65; FUN_1d1d_2922 |
| `FUN_1d1d_2c65` | 23586 | 25 | platform | malloc — near-heap alloc via 2922; grow arena 213e on fail | inferred | FUN_1d1d_2922; FUN_1d1d_213e |
| `FUN_1d1d_2c8e` | 23611 | 168 | platform | Build spawn env block: copy env strings + hex-encode open fds | inferred |  |
| `FUN_1d1d_2f06` | 23779 | 312 | platform | DOS spawn/exec: MCB walk + AH=48/4A carve; copy env/cmdline | inferred |  |
| `FUN_1d1d_328a` | 24091 | 30 | platform | DOS INT21 AH=43 get file attrs; −1 if mode&2 and read-only | inferred | FUN_1d1d_2b32 |

### Segment `2047` (5 defs) — platform — DOS Ctrl-C/Break / INT21 abort handlers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2047_000a` | 24121 | 64 | platform | Install Ctrl-C/Break abort context (INT21; save SS:SP at DS:0x26a3) | inferred |  |
| `FUN_2047_00b8` | 24185 | 32 | platform | Arm/save 5 abort INT vectors; set busy flag DS:0x26a2 | inferred |  |
| `FUN_2047_00e9` | 24217 | 15 | platform | If armed: INT21 then reinstall default abort handlers (0106) | inferred |  |
| `FUN_2047_0106` | 24232 | 20 | platform | Install 5 far INT abort handlers pointing to 2047:0103 | inferred |  |
| `FUN_2047_011f` | 24252 | 10 | platform | Read abort-busy flag DS:0x26a2 | inferred |  |

### Segment `2059` (3 defs) — sound — Sound driver jump table

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2059_0006` | 24262 | 13 | sound | Sound driver load/init poll entry (via DS:0xa654) | inferred | FUN_2a1f_0f56; FUN_2059_000a sibling |
| `FUN_2059_000a` | 24275 | 20 | sound | Sound driver jump table | known | src/core/sound.c |
| `FUN_2059_005f` | 24295 | 13 | sound | Sound driver unload/shutdown entry (via DS:0xa65c) | inferred | FUN_281f_05d8; FUN_2059_000a sibling |

### Segment `205f` (2 defs) — platform — DS:0x26f0 8-byte record table lookup helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_205f_000c` | 24308 | 31 | platform | Lookup key in DS:0x26f0[16]×8-byte table; return byte+6 (default 0x4e if miss) | inferred | FUN_205f_0046; FUN_2a1f_0c50 |
| `FUN_205f_0046` | 24339 | 30 | platform | Lookup key in DS:0x26f0[16]×8-byte table; return slot index or -1 | inferred | FUN_205f_000c |

### Segment `206d` (1 defs) — platform — Stream buffer fill + far-ptr normalize

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_206d_000a` | 24369 | 52 | platform | Copy into stream output buffer (size-budgeted); normalize far write ptr | inferred | FUN_1c05_0004 |

### Segment `2074` (1 defs) — platform — Size-budgeted stream read to 1b01

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2074_0008` | 24421 | 56 | platform | Size-budgeted stream read into far dest via 1b01; update remaining/total | inferred | FUN_1b01_000e |

### Segment `2088` (2 defs) — platform — XMS detect (INT2F) + init handle table

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2088_000c` | 24477 | 20 | platform | DOS write string to stdout (AH=40 handle 1) then CR/LF | inferred |  |
| `FUN_2088_0048` | 24497 | 46 | platform | Detect XMS via INT2F and install multiplex entry at DS:0x26e8 | known |  |

### Segment `2094` (1 defs) — platform — Rtrim trailing spaces/tabs

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2094_000a` | 24543 | 27 | platform | Rtrim trailing spaces/tabs from string in place | inferred |  |

### Segment `209a` (1 defs) — platform — Parse hex integer from string

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_209a_000a` | 24570 | 44 | platform | Parse hex integer from string (A–F + digits via ctype 0x27ed) | inferred |  |

### Segment `20a0` (1 defs) — platform — Parse binary integer from string

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_20a0_000e` | 24614 | 34 | platform | Parse binary integer from string (0/1 digits only) | inferred |  |

### Segment `2100` (1 defs) — platform — XMS size query via multiplex entry

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2100_000e` | 24648 | 19 | platform | XMS UMB size probe (AH=10 DX=FFFF; return DX<<4 on BL=B0) | inferred |  |

### Segment `2103` (2 defs) — platform — XMS handle alloc / free list

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2103_000a` | 24667 | 24 | platform | XMS Request UMB (AH=10); push handle into table DS:0xa66a (max 16) | inferred |  |
| `FUN_2103_004c` | 24691 | 26 | platform | Remove UMB handle from table then XMS Release UMB (AH=11) | inferred |  |

### Segment `210d` (116 defs) — platform — DOS/EMS runtime: INT 21/67, bank switch, overlay page helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_210d_051a` | 24717 | 44 | platform | Lookup AL in CS:0x4e9c[24]; hit → A000+(idx×4)<<8 | inferred |  |
| `FUN_210d_0727` | 24761 | 301 | platform | DOS/EMS bootstrap: INT21 AH=30, INT67 AH=42/43/45, INT21 AH=4A arenas + overlay setup | known | entry; FUN_210d_4c83; FUN_210d_4ce3 |
| `FUN_210d_0d91` | 25062 | 70 | platform | EMS overlay page-in helper (thunks call before far JMP) | inferred | ai/unit_mp.c; ai/accessors.c |
| `FUN_210d_0dab` | 25132 | 90 | platform | EMS overlay page-in helper (thunks call before far JMP) | inferred | ai/unit_mp.c; ai/accessors.c |
| `FUN_210d_1268` | 25222 | 51 | platform | Overlay RETF epilogue: 2607/265a stack EMS scan + illegal-mapping check | inferred | FUN_210d_2607; FUN_210d_265a; FUN_210d_2982 |
| `FUN_210d_12ad` | 25273 | 27 | platform | Snapshot EMS mapping into CS:3964..396a; set flag bit2 | inferred | FUN_210d_1341 |
| `FUN_210d_1341` | 25300 | 76 | platform | EMS overlay gate: set 290d.bit6; 2684/12ad scan or illegal-mapping→2982 | inferred | FUN_210d_1268; FUN_210d_2684; FUN_210d_12ad |
| `FUN_210d_1407` | 25376 | 33 | platform | EMS mapping validate on overlay return (2907 slot / 2684 / 2982) | inferred | FUN_210d_2684; FUN_210d_2982 |
| `FUN_210d_167b` | 25409 | 9 | platform | Thunk → 3791 resident dispatch (fixed AX/BX/CX/DX diagnostic args) | known | FUN_210d_3791 |
| `FUN_210d_1695` | 25418 | 22 | platform | Unvectored-call diagnostic: patch hex via 37d9/45d3 then JMP 37ad | inferred |  |
| `FUN_210d_1ac4` | 25440 | 40 | platform | Overlay unmap: capture INT21 vec (+optional IVT hook); maybe 3564; clear 2912.bit0 | inferred |  |
| `FUN_210d_1bc4` | 25480 | 46 | platform | DOS mem arena ±paras (INT21 AH=4A via 1c43) or measure free; EMS path via 1341 | known | FUN_210d_1c43; FUN_210d_1341; FUN_210d_54d0 |
| `FUN_210d_1c43` | 25526 | 21 | platform | INT21 AH=4A: resize arena ES:[28f3] to 28f9+AX paragraphs | known | FUN_210d_1bc4 |
| `FUN_210d_1c61` | 25547 | 11 | platform | Write overlay list-head word (CS:3952) into caller far ptr | inferred |  |
| `FUN_210d_1c75` | 25558 | 11 | platform | Far wrapper → 1d49 overlay-chain flush | known | FUN_210d_1d49 |
| `FUN_210d_1d49` | 25569 | 37 | platform | Flush overlay chain (1dbd/2b22) until head≥0x216; clear caseD_b | inferred | FUN_210d_1dbd; FUN_210d_2b22 |
| `FUN_210d_1dbd` | 25606 | 24 | platform | If AX bit15 overlay slot set: clear flag + optional 2d5a commit | inferred |  |
| `FUN_210d_2019` | 25630 | 29 | platform | Retain EMS overlay: set flags + INC [6]; overflow→33e3/58c2 then [6]=0x1000 | inferred | FUN_210d_33e3; FUN_210d_58c2; FUN_210d_5808 |
| `FUN_210d_238c` | 25659 | 8 | platform | Far no-op stub (NOP; RETF) | inferred |  |
| `FUN_210d_238e` | 25667 | 16 | platform | Add DX into word at [BX] if nonzero (reloc delta apply) | known | FUN_210d_3564; FUN_210d_2e59 |
| `FUN_210d_239d` | 25683 | 69 | platform | BP-stack walk: reloc far-call segs into EMS window by opcode probe | inferred |  |
| `FUN_210d_2492` | 25752 | 59 | platform | Normalize ES:DI→seg; classify vs EMS window/overlay slots (else 0xece) | inferred | FUN_210d_2590; FUN_210d_2607 |
| `FUN_210d_2590` | 25811 | 64 | platform | Scan SS stack via 2492; cache last EMS-window far ptr @CS:398d | inferred |  |
| `FUN_210d_2607` | 25875 | 49 | platform | Walk SS far-ptr chain via 2492 until EMS-window hit | inferred | FUN_210d_2492; FUN_210d_1268 |
| `FUN_210d_265a` | 25924 | 14 | platform | Arm stack EMS-scan mode (bit2 + save SP) then fall into 2684 | inferred | FUN_210d_2684 |
| `FUN_210d_2684` | 25938 | 154 | platform | BP/SS stack walk: probe CALLF/JMPF near EMS window; fail→2982/3791 | inferred | FUN_210d_2492; FUN_210d_239d; FUN_210d_2982 |
| `FUN_210d_2a48` | 26092 | 17 | platform | If overlay slot live with retain list: retain page(s) via 5808 | inferred | FUN_210d_5808 |
| `FUN_210d_2a6e` | 26109 | 24 | platform | Walk SI overlay back-chain (-1 sentinels); optional 1ebd unmap / 39d9 cb | inferred | FUN_210d_1ebd; FUN_210d_2aa5 |
| `FUN_210d_2aa5` | 26133 | 14 | platform | If overlay slot matches: 2a6e; if descriptor !bit5: 2d5a commit | inferred |  |
| `FUN_210d_2ae5` | 26147 | 28 | platform | Release EMS overlay retain (5856); maybe 2d5a commit if flags&0x21 | inferred | FUN_210d_5856; FUN_210d_2d5a; FUN_210d_2b22 |
| `FUN_210d_2b22` | 26175 | 42 | platform | Walk CS:0x3952 overlay chain; XOR bit15 + 2ae5 until sentinel | inferred |  |
| `FUN_210d_2b9b` | 26217 | 41 | platform | Ensure EMS space (2c57); if flags&6: copy page hdr + stream-read via 409c | inferred | FUN_210d_2c57; FUN_210d_402f; FUN_210d_409c |
| `FUN_210d_2c57` | 26258 | 54 | platform | Compute EMS page need; loop 40e6 bank/alloc until fit (fail→3791) | inferred | FUN_210d_40e6; FUN_210d_3791 |
| `FUN_210d_2d5a` | 26312 | 57 | platform | Commit EMS overlay slot: relink retain list / 5699 or 56e5 | inferred | FUN_210d_1dbd; FUN_210d_5699; FUN_210d_56e5 |
| `FUN_210d_2e59` | 26369 | 29 | platform | Add DX to CX far-ptr table entries at [SI×4] (reloc fixup) | inferred |  |
| `FUN_210d_2e78` | 26398 | 56 | platform | Map EMS physical pages into window (552c/5716); fail→3791 | inferred | FUN_210d_552c; FUN_210d_5716 |
| `FUN_210d_2fd2` | 26454 | 40 | platform | Classify [BP+0x18] far code via opcode probes (302e…3322) | inferred |  |
| `FUN_210d_3018` | 26494 | 19 | platform | Opcode probe: near CALL (E8) at DI-3 → 3254 | inferred | FUN_210d_3254; FUN_210d_2fd2 |
| `FUN_210d_302e` | 26513 | 19 | platform | Opcode probe: far CALL (9A) at DI-5 → 3254 | inferred | FUN_210d_3254; FUN_210d_2fd2 |
| `FUN_210d_3046` | 26532 | 35 | platform | Opcode probe: INT3/INTO/INT 0 at DI → 3080→3254 | inferred | FUN_210d_3080; FUN_210d_2fd2 |
| `FUN_210d_3080` | 26567 | 9 | platform | Forward to 3254 CALLF/JMPF opcode probe | known | FUN_210d_3254 |
| `FUN_210d_3094` | 26576 | 78 | platform | Opcode probe: FF/modrm forms → reloc helpers 3147/3179/31c4 + 3254 | inferred | FUN_210d_3147; FUN_210d_3179; FUN_210d_31c4; FUN_210d_3254 |
| `FUN_210d_3147` | 26654 | 19 | platform | Seed reloc slot 0x2fca from BP via XLAT 0x3171 + seg 0x397f | inferred |  |
| `FUN_210d_3179` | 26673 | 32 | platform | Accumulate reloc offsets into 0x2fc6 via XLAT 0x31b4; maybe SS→0x2fc8 | inferred |  |
| `FUN_210d_31c4` | 26705 | 48 | platform | ES:[BX+DI-1] ES: prefix → fill 0x2fc8 then 322c reloc write | inferred |  |
| `FUN_210d_3254` | 26753 | 65 | platform | Probe CALLF/CALL/JMPF opcode patterns at DX:AX (recurse on JMPF) | inferred |  |
| `FUN_210d_3322` | 26818 | 38 | platform | Opcode probe: walk back ≤4 for ES:MOV (0x8B26) + mask-0x1f checks | inferred | FUN_210d_2fd2 |
| `FUN_210d_3367` | 26856 | 25 | platform | Opcode probe: RETF+PUSH AX/DX with PUSH CS/imm → 3254 | inferred | FUN_210d_3254; FUN_210d_2fd2 |
| `FUN_210d_33e3` | 26881 | 17 | platform | Round EMS page [6] sizes bytes→paragraphs across ES:[18] slots | inferred |  |
| `FUN_210d_3564` | 26898 | 133 | platform | EMS/DOS arena compact: INT21 AH=48/4A/49 (+set INT21 vec); reload stream; 238e relocs | known | FUN_210d_1ac4; FUN_210d_1bc4; FUN_210d_4052; FUN_210d_238e |
| `FUN_210d_3791` | 27031 | 17 | platform | Pack BX:DX into 37cd/cf; JMPF via [37d5] (2-arg resident dispatch) | inferred |  |
| `FUN_210d_37ad` | 27048 | 21 | platform | Pack BX:DX:DI into 37cd..; JMPF via [37d5] (3-arg resident dispatch) | inferred |  |
| `FUN_210d_37d9` | 27069 | 40 | platform | Build map-address diagnostic (hex-patch via 45d3 into template) | inferred |  |
| `FUN_210d_391d` | 27109 | 13 | platform | Write hex word(s) into diagnostic buffer via 45d3 (+ optional seg:0xe) | inferred |  |
| `FUN_210d_3a0f` | 27122 | 97 | platform | Resolve/open .EXE path (suffix check + INT21 AH=3D) or copy via 275d helpers | inferred | FUN_275d_0a4f; FUN_275d_0909 |
| `FUN_210d_3d9b` | 27219 | 178 | platform | Alloc/init EMS page-map tables (AL selects path); fill [20..] indices + sizes | inferred | FUN_210d_4a4c; FUN_210d_4454; FUN_210d_4ce3 |
| `FUN_210d_3f46` | 27397 | 8 | platform | Validate EMS page budget vs CS:4e80/4e82; CF if short | inferred | FUN_210d_40e6 |
| `FUN_210d_3fb7` | 27405 | 33 | platform | Walk retain chain @[16]: refresh max EMS sizes; clear flags&0xe | inferred | FUN_210d_40e6 |
| `FUN_210d_402f` | 27438 | 16 | platform | Seed overlay I/O cursor [1a/1c/1e] from page header at 28cf+AX | inferred | FUN_210d_4052; FUN_210d_2b9b |
| `FUN_210d_4052` | 27454 | 37 | platform | Read CX overlay bytes in ≤0x400 chunks (4e28/466e/4e3b) | inferred | FUN_210d_466e; FUN_210d_409c |
| `FUN_210d_409c` | 27491 | 46 | platform | Read CX overlay-stream bytes (≤0x400 chunks via 45f1); CF if short | inferred |  |
| `FUN_210d_40e6` | 27537 | 158 | platform | Allocate/bank EMS pages into window; link page chain + set flags | inferred | FUN_210d_3f46; FUN_210d_3fb7; FUN_210d_2c57 |
| `FUN_210d_43f0` | 27695 | 43 | platform | Build EMS page LUT (49db) into 46fd×0x400 slots if flags&1 | inferred | FUN_210d_49db; FUN_210d_4e4e |
| `FUN_210d_4454` | 27738 | 34 | platform | EMS error diagnostic: hex-patch AH/AX via 45da then JMP 37ad ('Pa…') | inferred | FUN_210d_45da; FUN_210d_37ad |
| `FUN_210d_44db` | 27772 | 22 | platform | EMS status diagnostic: hex-patch via 45da then JMP 37ad ('Te…') | inferred | FUN_210d_45da; FUN_210d_37ad |
| `FUN_210d_45d3` | 27794 | 27 | platform | Store AX as 4 hex ASCII digits at ES:DI (via 45da/45e7) | inferred |  |
| `FUN_210d_45da` | 27821 | 26 | platform | Store AL as 2 hex ASCII digits at ES:DI (high nibble→45e7, then low) | known | FUN_210d_45e7; FUN_210d_45d3 |
| `FUN_210d_45e7` | 27847 | 26 | platform | Hex nibble→ASCII (DAA) STOSB at ES:DI | known | FUN_210d_45da |
| `FUN_210d_45f1` | 27873 | 80 | platform | Advance overlay-stream cursor (wrap 0x400); swap EMS map regs; maybe 4454/44db | inferred | FUN_210d_409c; FUN_210d_470f; FUN_210d_4751 |
| `FUN_210d_466e` | 27953 | 48 | platform | EMS bank transfer: map page (AH=44) + REP MOVSW, or 44db path; advance 1KB index via [BX+0x20] | known | FUN_210d_470f; FUN_210d_4751; FUN_210d_4454; FUN_210d_44db |
| `FUN_210d_470f` | 28001 | 26 | platform | Build EMS transfer descriptor (paras<<4, handle/ES:DI, copy length) for banked MOVSW path | inferred | FUN_210d_466e |
| `FUN_210d_4751` | 28027 | 29 | platform | Build alternate EMS/XMS move descriptor (DX high-bits packed into linear addr) for 44db path | inferred | FUN_210d_466e; FUN_210d_44db |
| `FUN_210d_479f` | 28056 | 124 | platform | Coalesce freed EMS overlay page block into neighbor freelists (1KB pages; flags at [0]) | inferred | FUN_210d_49c3 |
| `FUN_210d_49c3` | 28180 | 13 | platform | Walk EMS page-link words at [BX+0x20] for CX>>10 steps; return CX&0x3ff remainder | known | FUN_210d_479f |
| `FUN_210d_49db` | 28193 | 71 | platform | Find free EMS physical page in bitmap 4eb4/4eb6; compute A000-based frame offset | inferred | FUN_210d_4a4c |
| `FUN_210d_4a4c` | 28264 | 152 | platform | EMS init: detect (4c83), get/set INT67, parse frame/env, map pages via 4454, seed bitmaps | known | FUN_210d_4c83; FUN_210d_4454; FUN_210d_051a; FUN_275d_08ad |
| `FUN_210d_4c83` | 28416 | 53 | platform | EMS detect: INT21 AH=35 INT67, EMMXXXX0 CMPSW, INT67 AH=46 version≥3.2 + AH=41 page frame | known | FUN_275d_048f; FUN_210d_4a4c |
| `FUN_210d_4ce3` | 28469 | 47 | platform | XMS detect: DOS≥3, INT2F 4300/4310, driver AX=0 version≥2.00; stash entry at CS:4e84 | known | FUN_275d_048f; FUN_210d_5247 |
| `FUN_210d_4d4f` | 28516 | 46 | platform | Drain deferred EMS overlay lock queue (flag 0x10): unlink nodes from list at [0x18] | inferred |  |
| `FUN_210d_4de3` | 28562 | 20 | platform | Cache current EMS overlay page context (seg/handle/page counts) into CS:4e7a..4e82 | inferred |  |
| `FUN_210d_4e28` | 28582 | 14 | platform | EMS save page map (AH=47 via 4454) when overlay mapped flag [0] bit0 set | known | FUN_210d_4454 |
| `FUN_210d_4e3b` | 28596 | 15 | platform | EMS restore page map (AH=48 via 4454) when overlay mapped flag [0] bit0 set | known | FUN_210d_4454 |
| `FUN_210d_4e4e` | 28611 | 14 | platform | Map EMS pages from SI list (AH=50 if ver≥4.0 else AH=44 loop via 4454) | known | FUN_210d_4454 |
| `FUN_210d_4fe5` | 28625 | 46 | platform | Init overlay heap arena: free-list heads, size, alignment masks from BX bit count | inferred | FUN_210d_508b |
| `FUN_210d_508b` | 28671 | 117 | platform | Insert/grow overlay heap free block into doubly-linked arena; update free totals | inferred | FUN_210d_5a9d; FUN_210d_536e |
| `FUN_210d_5247` | 28788 | 19 | platform | Probe DOS ver + UMB link (AH=58); call XMS detect 4ce3; set strategy capability flags | known | FUN_210d_4ce3 |
| `FUN_210d_52a6` | 28807 | 13 | platform | Largest free memory: XMS UMB query or DOS AH=48 after save/restore strategy (538c/52d3/53b5) | known | FUN_210d_538c; FUN_210d_52d3; FUN_210d_53b5 |
| `FUN_210d_52d3` | 28820 | 14 | platform | DOS free-memory probe (INT21 AH=48 BX=FFFF) → available paragraphs in AX | known | FUN_210d_52a6 |
| `FUN_210d_535c` | 28834 | 9 | platform | Grow overlay arena via XMS/UMB alloc callback at 52df (loop through 536e) | inferred | FUN_210d_536e; FUN_210d_508b |
| `FUN_210d_5365` | 28843 | 9 | platform | Grow overlay arena via DOS AH=48 alloc callback at 5338 (loop through 536e) | inferred | FUN_210d_536e; FUN_210d_508b |
| `FUN_210d_536e` | 28852 | 24 | platform | Alloc-loop driver: CALL DI allocator, on success register block via 508b until size met | inferred | FUN_210d_508b; FUN_210d_535c; FUN_210d_5365 |
| `FUN_210d_538c` | 28876 | 22 | platform | Save DOS alloc strategy+UMB link; set first-fit-high + UMB linked (INT21 AH=58) | known | FUN_210d_53b5; FUN_210d_52a6 |
| `FUN_210d_53b5` | 28898 | 14 | platform | Restore prior DOS alloc strategy + UMB link state (INT21 AH=58 AL=1/3) | known | FUN_210d_538c; FUN_210d_53cb |
| `FUN_210d_53cb` | 28912 | 8 | platform | Shared POP BX/AX; RET epilogue for DOS AH=58 restore (Ghidra-split from 53b5) | known | FUN_210d_53b5 |
| `FUN_210d_545f` | 28920 | 38 | platform | Shrink overlay heap free region by AX paragraphs; update freelist via 597f/59ce/5b9d | inferred | FUN_210d_597f; FUN_210d_59ce; FUN_210d_5a9d; FUN_210d_5b9d |
| `FUN_210d_54d0` | 28958 | 26 | platform | Grow overlay heap free region by AX; splice block via 5a9d/5bcb/5b9d | inferred | FUN_210d_5a9d; FUN_210d_5bcb; FUN_210d_5b9d |
| `FUN_210d_552c` | 28984 | 115 | platform | Overlay heap compact/coalesce pass: walk freelist, merge neighbors, remap presence bits | inferred | FUN_210d_5bcb; FUN_210d_5c9e; FUN_210d_5d49; FUN_210d_5b26 |
| `FUN_210d_5699` | 29099 | 35 | platform | Unlink overlay block from freelist links; clear deferred-lock flag 0x10 | inferred | FUN_210d_597f |
| `FUN_210d_56e5` | 29134 | 23 | platform | Bump overlay block refcount at +6; on wrap call 58c2; set busy flag 0x08 | inferred | FUN_210d_58c2 |
| `FUN_210d_5716` | 29157 | 72 | platform | Allocate paragraphs from overlay freelist (fit walk, split via 5a9d/5b26, mark presence) | inferred | FUN_210d_59ce; FUN_210d_5a9d; FUN_210d_5b26; FUN_210d_5a76 |
| `FUN_210d_5808` | 29229 | 35 | platform | Retain EMS overlay page (set bit2 + ref=1, or INC [6]) | inferred |  |
| `FUN_210d_5856` | 29264 | 46 | platform | Release overlay block lock/refcount; unlink when count hits 0 (58f9/5afc) | inferred | FUN_210d_58f7; FUN_210d_58f9; FUN_210d_5afc |
| `FUN_210d_58c2` | 29310 | 16 | platform | Walk retained EMS chain (CS:5db9): bytes→paras [6] if !flags&5 | inferred |  |
| `FUN_210d_58f7` | 29326 | 10 | platform | Carry-clear stub (always success) gating overlay lock/unlock callers | known | FUN_210d_5856 |
| `FUN_210d_58f9` | 29336 | 47 | platform | Unlink overlay block from busy/free doubly-linked list; clear flag bit3 | inferred | FUN_210d_5856; FUN_210d_597f |
| `FUN_210d_597f` | 29383 | 31 | platform | Unlink overlay block from freelist (+0xc/+0xe); repair arena head pointers | inferred | FUN_210d_58f9; FUN_210d_5bcb |
| `FUN_210d_59ce` | 29414 | 40 | platform | Clear overlay page-presence bit in map; optional EMS notify (2684/265a/2590/2b9b) | inferred | FUN_210d_2684; FUN_210d_265a; FUN_210d_2590; FUN_210d_2b9b |
| `FUN_210d_5a76` | 29454 | 15 | platform | Set overlay page-presence bit in map for allocated block | inferred | FUN_210d_59ce |
| `FUN_210d_5a9d` | 29469 | 27 | platform | Insert block onto overlay freelist head; mark free (bit0); refresh max-free cache | inferred | FUN_210d_5afc; FUN_210d_508b |
| `FUN_210d_5afc` | 29496 | 8 | platform | Compute free size of overlay block (from +2, or distance to arena end) | inferred | FUN_210d_5a9d; FUN_210d_5b26; FUN_210d_5b9d |
| `FUN_210d_5b26` | 29504 | 40 | platform | Split overlay free block at BX; install new header and rewire +8/+0xa/+0xc links | inferred | FUN_210d_5afc |
| `FUN_210d_5b9d` | 29544 | 23 | platform | Walk overlay freelist to recompute max free size into 5dad | inferred | FUN_210d_5afc |
| `FUN_210d_5bcb` | 29567 | 52 | platform | Coalesce adjacent free overlay blocks; unlink absorbed nodes via 597f | inferred | FUN_210d_597f |
| `FUN_210d_5c9e` | 29619 | 54 | platform | Mark overlay block busy (clear free bit); split remainder back onto freelist | inferred | FUN_210d_5a9d |
| `FUN_210d_5d49` | 29673 | 8 | platform | Compare overlay block neighbor keys (+6) for coalesce/order eligibility | inferred | FUN_210d_552c |
| `FUN_210d_5dd0` | 29681 | 11 | platform | Far return CS:5dd8 overlay-runtime entry pointer with BX=FFFF | known |  |

### Segment `275d` (21 defs) — platform — DOS PATH/env parse / memory sizing / INT21 helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_275d_0000` | 29692 | 80 | platform | Size three DOS memory pools into out-word triple (env/EMS vs percent paths) | inferred |  |
| `FUN_275d_023a` | 29772 | 52 | platform | Query/clamp one memory size via 048f INT21 stub + 02b2 patch | inferred |  |
| `FUN_275d_02b2` | 29824 | 31 | platform | Patch INT21 probe stub bytes at DS:0x20235 (0x4e8b/'E') | inferred |  |
| `FUN_275d_033a` | 29855 | 40 | platform | Alternate memory-probe stub setup (CX flag merge + 048f) | inferred |  |
| `FUN_275d_0443` | 29895 | 28 | platform | Scale/clamp memory size by percent flags in CX | inferred |  |
| `FUN_275d_048f` | 29923 | 79 | platform | Parse env memory-size string (getenv 08ad; comma fields + 05ed) | inferred |  |
| `FUN_275d_05ed` | 30002 | 34 | platform | Parse unsigned decimal integer from SI (overflow→0xffff) | inferred |  |
| `FUN_275d_0624` | 30036 | 16 | platform | Skip leading spaces at SI | inferred |  |
| `FUN_275d_062d` | 30052 | 30 | platform | Copy ≤CX chars SI→DI and NUL-terminate | inferred |  |
| `FUN_275d_06b3` | 30082 | 19 | platform | Install INT24 critical-error vectors (8c3/b42) after 080f dispatch init | inferred |  |
| `FUN_275d_06db` | 30101 | 20 | platform | Save ES; set INT24 vector words to 8c3/b42 | inferred |  |
| `FUN_275d_0700` | 30121 | 68 | platform | Register far INT24/callback entry in table at DS:0x758 | inferred |  |
| `FUN_275d_07a4` | 30189 | 12 | platform | Fatal trampoline: load DAT_207c then far-jump 0x8bff | inferred |  |
| `FUN_275d_080f` | 30201 | 34 | platform | Dispatch DS:0x758 INT24 handlers whose mask matches AL | inferred |  |
| `FUN_275d_08ad` | 30235 | 56 | platform | getenv: find KEY= in env seg *[0x2c], copy value to DI | inferred |  |
| `FUN_275d_0909` | 30291 | 124 | platform | PATH/search-order dispatcher on letters C/E/P/A | inferred |  |
| `FUN_275d_0a11` | 30415 | 43 | platform | Build current-drive "D:\" + cwd path into DI (INT21) | inferred |  |
| `FUN_275d_0a4f` | 30458 | 40 | platform | Locate argv0 path after env block (PSP env trail) | inferred |  |
| `FUN_275d_0ab8` | 30498 | 14 | platform | Append path via 0acc then INT21 open/search | inferred |  |
| `FUN_275d_0acc` | 30512 | 35 | platform | strcat path component; set .exe detect flag DAT_20a5_00b8 | inferred |  |
| `FUN_275d_0b09` | 30547 | 84 | platform | INT21 canonicalize/build absolute search path (A-letter path) | inferred |  |

### Segment `281f` (371 defs) — mixed — Far thunks: RNG / map / UI fill helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_281f_0000` | 30631 | 10 | thunk | Far thunk → FUN_7421_025a (startup argv parse; set skip-XMS then init) | inferred |  |
| `FUN_281f_000e` | 30641 | 10 | thunk | Far thunk → FUN_1000_0000 (init string-table arena; reset bump DS:0x2d52) | inferred |  |
| `FUN_281f_0018` | 30651 | 10 | thunk | Far thunk → FUN_1000_002c (intern string into table arena; return index) | inferred |  |
| `FUN_281f_0022` | 30661 | 10 | thunk | Far thunk → FUN_1000_0062 (nth NUL-terminated string from table) | inferred |  |
| `FUN_281f_002c` | 30671 | 10 | thunk | Far thunk → FUN_7b08_0118 (bump-alloc from arena remain; else fatal) | inferred |  |
| `FUN_281f_0048` | 30681 | 10 | thunk | Far thunk → FUN_7b08_000e (alloc growable far-buffer arena) | inferred |  |
| `FUN_281f_0056` | 30691 | 10 | thunk | Far thunk → FUN_1009_00b4 (wait/restore then disarm overlay) | inferred |  |
| `FUN_281f_0060` | 30701 | 10 | thunk | Far thunk → FUN_1009_0402 (arm timed overlay then draw) | inferred |  |
| `FUN_281f_006a` | 30711 | 10 | thunk | Far thunk → FUN_1009_017e (append string onto status buffer) | inferred |  |
| `FUN_281f_0074` | 30721 | 10 | thunk | Far thunk → FUN_1009_01a2 (string-table idx → append status buffer) | inferred |  |
| `FUN_281f_007e` | 30731 | 10 | thunk | Far thunk → FUN_1009_01b8 (itoa base10 then append to status buffer) | inferred |  |
| `FUN_281f_0088` | 30741 | 10 | thunk | Far thunk → FUN_1009_0222 (truncate last char of status buffer) | inferred |  |
| `FUN_281f_0092` | 30751 | 10 | thunk | Far thunk → FUN_1009_0244 (arm timed overlay: flags+style+deadline) | inferred |  |
| `FUN_281f_009c` | 30761 | 10 | thunk | Far thunk → FUN_1009_0270 (poll overlay; clear if past deadline) | inferred |  |
| `FUN_281f_00a6` | 30771 | 10 | thunk | Far thunk → FUN_1009_02ae (set status-strip layout globals x/y/w/h) | inferred |  |
| `FUN_281f_00b0` | 30781 | 10 | thunk | Far thunk → FUN_1009_02cc (draw/clear status text strip) | inferred |  |
| `FUN_281f_00ba` | 30791 | 10 | thunk | Far thunk → FUN_1b9e_000a (blit/copy rect into video buffer) | inferred |  |
| `FUN_281f_00c4` | 30801 | 10 | thunk | Far thunk → FUN_1bf5_0000 (tiled rect blit loop via repeated 1baa_0006) | inferred |  |
| `FUN_281f_00ce` | 30811 | 10 | thunk | Far thunk → FUN_1bca_0002 (rectangle outline (H+V fills)) | inferred |  |
| `FUN_281f_00d8` | 30821 | 10 | thunk | Far thunk → FUN_104b_01e8 (format long then append "$") | inferred |  |
| `FUN_281f_00e2` | 30831 | 10 | thunk | Far thunk → FUN_1b70_003a (video restore/flush around blit) | inferred |  |
| `FUN_281f_00ec` | 30841 | 10 | thunk | Far thunk → FUN_1262_00da (drain pending key/mouse event queue) | inferred |  |
| `FUN_281f_00f6` | 30851 | 10 | platform | Far thunk → FUN_1ae3_0042 (BIOS INT16 key-ready check) | inferred |  |
| `FUN_281f_0100` | 30861 | 10 | thunk | Far thunk → FUN_104b_0318 (center string then draw via 0288) | inferred |  |
| `FUN_281f_010a` | 30871 | 10 | thunk | Far thunk → FUN_104b_0062 (strcat dest ← "%") | inferred |  |
| `FUN_281f_0114` | 30881 | 10 | thunk | Far thunk → FUN_104b_0216 (measure string width−1) | inferred |  |
| `FUN_281f_011e` | 30891 | 10 | thunk | Far thunk → FUN_104b_0072 (strcat dest ← "(") | inferred |  |
| `FUN_281f_0128` | 30901 | 10 | thunk | Far thunk → FUN_104b_0082 (strcat dest ← ")") | inferred |  |
| `FUN_281f_0132` | 30911 | 10 | thunk | Far thunk → FUN_104b_024e (draw string at xy color0) | inferred |  |
| `FUN_281f_013c` | 30921 | 10 | thunk | Far thunk → FUN_104b_0288 (set text colors then draw string) | inferred |  |
| `FUN_281f_0146` | 30931 | 10 | thunk | Far thunk → FUN_104b_00b2 (strcat dest ← "+") | inferred |  |
| `FUN_281f_0150` | 30941 | 10 | thunk | Far thunk → FUN_104b_02c2 (color+measure+draw string; rem width) | inferred |  |
| `FUN_281f_015a` | 30951 | 10 | thunk | Far thunk → FUN_104b_00c2 (strcat dest ← "-") | inferred |  |
| `FUN_281f_0164` | 30961 | 10 | thunk | Far thunk → FUN_104b_00d2 (strcat dest ← "x") | inferred |  |
| `FUN_281f_016e` | 30971 | 10 | thunk | Far thunk → FUN_104b_00e2 (resolve string ptr then UI text blit) | inferred |  |
| `FUN_281f_0178` | 30981 | 10 | thunk | Far thunk → FUN_1009_0420 (strcat dest ← string at DS:0x50) | inferred |  |
| `FUN_281f_0182` | 30991 | 10 | thunk | Far thunk → FUN_104b_012e (format int→string then UI text blit) | inferred |  |
| `FUN_281f_018c` | 31001 | 10 | thunk | Far thunk → FUN_104b_039a (draw string at xy with color 0 (alt font)) | inferred |  |
| `FUN_281f_0196` | 31011 | 10 | thunk | Far thunk → FUN_104b_0010 (append N spaces onto dest) | inferred |  |
| `FUN_281f_01a0` | 31021 | 10 | thunk | Far thunk → FUN_104b_0156 (itoa base2 zero-pad8 then strcat) | inferred |  |
| `FUN_281f_01aa` | 31031 | 10 | thunk | Far thunk → FUN_104b_03d2 (measure+draw string alt-font; rem width) | inferred |  |
| `FUN_281f_01b4` | 31041 | 10 | thunk | Far thunk → FUN_104b_0032 (strcat dest ← ", ") | inferred |  |
| `FUN_281f_01be` | 31051 | 10 | thunk | Far thunk → FUN_104b_0042 (strcat dest ← ": ") | inferred |  |
| `FUN_281f_01c8` | 31061 | 10 | thunk | Far thunk → FUN_104b_0430 (center string in width then draw via 039a) | inferred |  |
| `FUN_281f_01d2` | 31071 | 10 | thunk | Far thunk → FUN_104b_01be (itoa(long,base10) then strcat onto dest) | inferred |  |
| `FUN_281f_01dc` | 31081 | 10 | thunk | Far thunk → FUN_104b_0052 (fill/clear string buffer (0x58)) | inferred |  |
| `FUN_281f_01e6` | 31091 | 10 | thunk | Far thunk → FUN_104b_0478 (append 2f74[idx×0x10] label) | inferred |  |
| `FUN_281f_01f0` | 31101 | 10 | thunk | Far thunk → FUN_1c28_000a (set text draw color nibbles DS:0x269e) | inferred |  |
| `FUN_281f_01fa` | 31111 | 10 | thunk | Far thunk → FUN_1c11_000c (draw UI text string at xy) | inferred |  |
| `FUN_281f_0204` | 31121 | 10 | thunk | Far thunk → FUN_1c2a_0006 (string pixel-width via glyph table) | inferred |  |
| `FUN_281f_020e` | 31131 | 10 | thunk | Far thunk → FUN_1097_02da (hit-test spaced-row item index under cursor) | inferred |  |
| `FUN_281f_0218` | 31141 | 10 | thunk | Far thunk → FUN_1097_067a (clear multi-item layout list) | inferred |  |
| `FUN_281f_0222` | 31151 | 10 | thunk | Far thunk → FUN_1097_0682 (append item to layout list) | inferred |  |
| `FUN_281f_022c` | 31161 | 10 | thunk | Far thunk → FUN_1097_0394 (layout/blit multi-group item rows) | inferred |  |
| `FUN_281f_0236` | 31171 | 10 | thunk | Far thunk → FUN_1097_0174 (layout/blit spaced icon row) | inferred |  |
| `FUN_281f_0240` | 31181 | 10 | thunk | Far thunk → FUN_1097_0004 (compute multi-item row pitch/spacing+margin) | inferred |  |
| `FUN_281f_024a` | 31191 | 10 | thunk | Far thunk → FUN_112b_015c (orders-box rect and/or unit chrome blit) | inferred |  |
| `FUN_281f_0254` | 31201 | 10 | thunk | Far thunk → FUN_1c36_000a (sprite/frame blit from object table) | inferred |  |
| `FUN_281f_025e` | 31211 | 10 | thunk | Far thunk → FUN_1101_0050 (opaque 16×16 glyph blit from sheet row) | inferred |  |
| `FUN_281f_0268` | 31221 | 10 | thunk | Far thunk → FUN_1101_00b4 (transparent 16×16 glyph blit (dest==0)) | inferred |  |
| `FUN_281f_0272` | 31231 | 10 | thunk | Far thunk → FUN_1101_0126 (scaled opaque glyph blit (1<<zoom)) | inferred |  |
| `FUN_281f_027c` | 31241 | 10 | thunk | Far thunk → FUN_1101_000e (allocate glyph-blit scratch buffer) | inferred |  |
| `FUN_281f_0286` | 31251 | 10 | thunk | Far thunk → FUN_1101_01dc (scaled transparent glyph blit) | inferred |  |
| `FUN_281f_0290` | 31261 | 10 | thunk | Far thunk → FUN_1a4e_0008 (pitched buffer offset: pitch*y+base+x) | inferred |  |
| `FUN_281f_029a` | 31271 | 10 | thunk | Far thunk → FUN_7ada_01a0 (alloc via 2a1f_0e90 using size cell DS:0x2674) | inferred |  |
| `FUN_281f_02a8` | 31281 | 10 | thunk | Far thunk → FUN_112b_0c64 (colony map chrome: nation/fort/pop) | inferred |  |
| `FUN_281f_02b2` | 31291 | 10 | thunk | Far thunk → FUN_112b_0790 (tribe/village map chrome) | inferred |  |
| `FUN_281f_02bc` | 31301 | 10 | thunk | Far thunk → FUN_112b_01ba (unit chrome: silhouette + orders box + letter) | inferred |  |
| `FUN_281f_02c6` | 31311 | 10 | thunk | Far thunk → FUN_112b_0002 (profession→ICONS.SS index) | inferred |  |
| `FUN_281f_02d0` | 31321 | 10 | thunk | Far thunk → FUN_112b_0eb6 (animate unit tile-to-tile move) | inferred |  |
| `FUN_281f_02da` | 31331 | 10 | thunk | Far thunk → FUN_112b_0060 (unit display icon from type + profession) | inferred |  |
| `FUN_281f_02e4` | 31341 | 10 | thunk | Far thunk → FUN_1427_004a (one step down stack (read transport_prev)) | inferred |  |
| `FUN_281f_02ee` | 31351 | 10 | thunk | Far thunk → FUN_1427_0002 (walk transport_next to stack head) | inferred | ai/unit_mp.c |
| `FUN_281f_02f8` | 31361 | 10 | thunk | Far thunk → FUN_1c56_0004 (scaled/dithered sprite blit) | inferred |  |
| `FUN_281f_0302` | 31371 | 10 | mapgen | map_tile_in_bounds | known | ai/accessors.c |
| `FUN_281f_030c` | 31381 | 10 | ai | Indian↔Euro relation word get thunk→15dc_00e0 (DS:0x5b1c) | inferred | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_0316` | 31391 | 10 | thunk | Far thunk → FUN_4cc6_03f8 (best Euro threat nation+score near tribe) | inferred |  |
| `FUN_281f_0322` | 31401 | 10 | thunk | Far thunk → FUN_15eb_035e (test colony feature bit at DS:0x5dca) | inferred |  |
| `FUN_281f_032c` | 31411 | 10 | thunk | Far thunk → FUN_6afa_00c8 (blit map-buffer tile rect to screen) | inferred |  |
| `FUN_281f_033a` | 31421 | 10 | thunk | Far thunk → FUN_1baa_0006 (pitched buffer rect blit/copy) | inferred |  |
| `FUN_281f_0344` | 31431 | 10 | thunk | Far thunk → FUN_6b22_04bc (blit fog-visible on-map units in rect) | inferred |  |
| `FUN_281f_0352` | 31441 | 10 | thunk | Far thunk → FUN_1984_02fc (recenter viewport if rect near edge) | inferred |  |
| `FUN_281f_035c` | 31451 | 10 | thunk | Far thunk → FUN_124c_000c (clamp value to [lo,hi]) | inferred |  |
| `FUN_281f_0366` | 31461 | 10 | thunk | Far thunk → FUN_124c_002a (swap two words through near pointers) | inferred |  |
| `FUN_281f_0370` | 31471 | 10 | thunk | Far thunk → FUN_124c_0040 (DOS distance helper) | inferred |  |
| `FUN_281f_037a` | 31481 | 10 | thunk | Far thunk → FUN_124c_007c (Manhattan /dx/+/dy/ via 0040) | inferred |  |
| `FUN_281f_038e` | 31491 | 10 | thunk | Far thunk → FUN_1262_0128 (wrap selection index into [0, n)) | inferred |  |
| `FUN_281f_0398` | 31501 | 10 | thunk | Far thunk → FUN_1262_0142 (bind tip-string far ptr) | inferred |  |
| `FUN_281f_03a2` | 31511 | 10 | thunk | Far thunk → FUN_1262_0002 (read Shift-key state bits (BDA 40:17)) | inferred |  |
| `FUN_281f_03ac` | 31521 | 10 | thunk | Far thunk → FUN_1262_02fe (mouse-move / tooltip debounce tick) | inferred |  |
| `FUN_281f_03b6` | 31531 | 10 | thunk | Far thunk → FUN_1262_0012 (clear tip-overlay rect + video flush) | inferred |  |
| `FUN_281f_03c0` | 31541 | 10 | thunk | Far thunk → FUN_1262_0060 (wait for key/mouse with UI pump) | inferred |  |
| `FUN_281f_03ca` | 31551 | 10 | thunk | Far thunk → FUN_1262_00f6 (mouse hit-test cursor inside x,y,w,h) | inferred |  |
| `FUN_281f_03d4` | 31561 | 10 | thunk | Far thunk → FUN_7a05_014e (fatal video teardown then INT10 reset) | inferred |  |
| `FUN_281f_03e0` | 31571 | 10 | platform | Far thunk → FUN_1ae7_0016 (BIOS INT16 read next key) | inferred |  |
| `FUN_281f_03ea` | 31581 | 10 | thunk | Far thunk → FUN_12d6_0000 (blit VGA rect from offscreen buffer) | inferred |  |
| `FUN_281f_03f4` | 31591 | 10 | thunk | Far thunk → FUN_1ade_0004 (VGA retrace + program DAC palette) | inferred |  |
| `FUN_281f_03fe` | 31601 | 10 | ui | Dialog flush/run thunk→6f74_3744→0998 | inferred | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_040a` | 31611 | 10 | thunk | Far thunk → FUN_6f74_37f6 (OR dialog default-flags 0x18 into DS:0x1f56) | inferred |  |
| `FUN_281f_0416` | 31621 | 10 | thunk | Far thunk → FUN_6f74_03d0 (prefetch dialog string-table slot) | inferred |  |
| `FUN_281f_0422` | 31631 | 10 | thunk | Far thunk → FUN_7314_0208 (load nth string from dual-id resource) | inferred |  |
| `FUN_281f_042e` | 31641 | 10 | thunk | Far thunk → FUN_15b3_0144 (format nation display name into buffer) | inferred |  |
| `FUN_281f_0438` | 31651 | 10 | ui | Set dialog subst string slot thunk→6f74_03ec | inferred | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_0444` | 31661 | 10 | thunk | Far thunk → FUN_1b8f_0006 (pitched buffer rect copy) | inferred |  |
| `FUN_281f_044e` | 31671 | 10 | thunk | Far thunk → FUN_7944_000e (resource open/load by name) | inferred |  |
| `FUN_281f_045c` | 31681 | 10 | ui | Far thunk → FUN_1acb_011a (wait until custom timer word changes) | inferred |  |
| `FUN_281f_0466` | 31691 | 10 | ui | Far thunk → FUN_1acb_0056 (sample mouse/keyboard input state) | inferred |  |
| `FUN_281f_0470` | 31701 | 10 | ui | ui_pump thunk→129f_00f6 | known | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_047a` | 31711 | 10 | ui | Far thunk → FUN_1acb_0030 (reset mouse/input latch before act loop) | inferred |  |
| `FUN_281f_0484` | 31721 | 10 | thunk | Far thunk → FUN_1b8d_0004 (filled-rect blit wrapper→1b9e) | inferred |  |
| `FUN_281f_048e` | 31731 | 10 | thunk | Far thunk → FUN_129f_02cc (queue sound/event id if changed) | inferred |  |
| `FUN_281f_0498` | 31741 | 10 | thunk | Far thunk → FUN_129f_0300 (store BGM track id at DS:0x9a) | inferred |  |
| `FUN_281f_04a2` | 31751 | 10 | thunk | Far thunk → FUN_129f_030c (store pending/next BGM id at DS:0x98) | inferred |  |
| `FUN_281f_04ac` | 31761 | 10 | thunk | Far thunk → FUN_129f_0318 (set BGM track id + gate play) | inferred |  |
| `FUN_281f_04b6` | 31771 | 10 | thunk | Far thunk → FUN_129f_034c (play SFX if sound enabled else set id) | inferred |  |
| `FUN_281f_04c0` | 31781 | 10 | thunk | Far thunk → FUN_12d8_000e (BGM / event / SFX gating) | inferred |  |
| `FUN_281f_04ca` | 31791 | 10 | platform | Reseed LCG from timer word | known | ai/accessors.c; src/core/ai.c |
| `FUN_281f_04d4` | 31801 | 10 | platform | Wrapped RNG range (calls into libc / LCG) | known | ai/accessors.c; src/core/dos_rng.c |
| `FUN_281f_04de` | 31811 | 10 | thunk | Far thunk → FUN_2059_000a (sound driver jump table) | inferred |  |
| `FUN_281f_04e8` | 31821 | 10 | thunk | Far thunk → FUN_1a58_000d (show mouse cursor) | inferred |  |
| `FUN_281f_04f2` | 31831 | 10 | thunk | Far thunk → FUN_1a58_0054 (hide mouse cursor) | inferred |  |
| `FUN_281f_04fc` | 31841 | 10 | thunk | Far thunk → FUN_12dd_0002 (clipped blit: tiled 1bf5 or solid 1b9e) | inferred |  |
| `FUN_281f_0506` | 31851 | 10 | thunk | Far thunk → FUN_12dd_0064 (clipped blit: tiled 1bf5 or solid 1b9e) | inferred |  |
| `FUN_281f_0510` | 31861 | 10 | thunk | Far thunk → FUN_12e9_008c (fill pitched dest rect row-by-row) | inferred |  |
| `FUN_281f_051a` | 31871 | 10 | thunk | Far thunk → FUN_12fd_0048 (test discovery/event bit at DS:0x540a) | inferred |  |
| `FUN_281f_0524` | 31881 | 10 | thunk | Far thunk → FUN_12fd_006c (once-only discovery/event opcode dispatch) | inferred |  |
| `FUN_281f_052e` | 31891 | 10 | thunk | Far thunk → FUN_6f30_0062 (splash/image load+blit via resource stream) | inferred |  |
| `FUN_281f_053c` | 31901 | 9 | thunk | Far thunk → LAB_130d_019c (empty far RETF no-op) | inferred |  |
| `FUN_281f_0546` | 31910 | 10 | thunk | Far thunk → FUN_130d_0290 (main game year/turn loop) | inferred |  |
| `FUN_281f_0550` | 31920 | 10 | thunk | Far thunk → FUN_5bfb_00f8 (rank Euro nations by strength) | inferred | turn/mid_pass_indian_rank.md |
| `FUN_281f_055e` | 31930 | 10 | ui | Far thunk → FUN_49dd_0424 (camera-follow map chrome; human-visible AI) | inferred | ai/euro_dispatcher.c |
| `FUN_281f_056a` | 31940 | 10 | thunk | Far thunk → FUN_1984_04f6 (tear down status chrome strip/overlays) | inferred |  |
| `FUN_281f_0574` | 31950 | 10 | thunk | Far thunk → FUN_41f2_14a8 (end-game score → rebate + HoF) | inferred |  |
| `FUN_281f_0582` | 31960 | 10 | ai | euro_select_nation_context thunk→38fd_0000 (set nation index + Europe-market base) | inferred | ai/euro_dispatcher.c |
| `FUN_281f_0590` | 31970 | 10 | ui | Fill helper (turn box) | known | src/core/turn.c |
| `FUN_281f_059a` | 31980 | 10 | thunk | Far thunk → FUN_6a9f_00d8 (compute/clamp minimap scroll origin) | inferred |  |
| `FUN_281f_05a8` | 31990 | 10 | thunk | Far thunk → FUN_74a4_0000 (build game menu bar titles+items) | inferred |  |
| `FUN_281f_05b6` | 32000 | 10 | save | Far thunk → FUN_7562_0034 (direct save-slot write / autosave) | inferred |  |
| `FUN_281f_05c4` | 32010 | 10 | thunk | Far thunk → FUN_1a58_008c (mouse INT33 init / cursor mode) | inferred |  |
| `FUN_281f_05ce` | 32020 | 10 | thunk | Far thunk → FUN_1a29_01d1 (restore prior INT8 + default PIT) | inferred |  |
| `FUN_281f_05d8` | 32030 | 10 | thunk | Far thunk → FUN_2059_005f (sound driver unload/shutdown entry) | inferred |  |
| `FUN_281f_05e2` | 32040 | 10 | thunk | Far thunk → FUN_2047_011f (read abort-busy flag DS:0x26a2) | inferred |  |
| `FUN_281f_05ec` | 32050 | 10 | thunk | Far thunk → FUN_75c2_2d28 (768-byte palette snapshot save/restore) | inferred |  |
| `FUN_281f_05fa` | 32060 | 10 | thunk | Far thunk → FUN_38fd_55b6 (Europe screen entry + event loop) | inferred | turn/europe_finish_bridge.md |
| `FUN_281f_0608` | 32070 | 10 | thunk | Far thunk → FUN_2f2b_6cd4 (colony screen entry / main-loop teardown) | inferred |  |
| `FUN_281f_0614` | 32080 | 10 | thunk | Far thunk → FUN_15eb_0142 (nearest colony by nation/continent; set active) | inferred |  |
| `FUN_281f_061e` | 32090 | 10 | thunk | Far thunk → FUN_3844_0442 (year-end Euro chrome) | inferred |  |
| `FUN_281f_062c` | 32100 | 10 | thunk | Far thunk → FUN_2b5a_3b68 (map UI Move/View Pieces loop) | inferred |  |
| `FUN_281f_0638` | 32110 | 10 | thunk | Far thunk → FUN_521d_6d8e (Euro AI dispatcher per nation) | known | ai/euro_dispatcher.c; src/core/ai.c |
| `FUN_281f_0644` | 32120 | 10 | thunk | Far thunk → FUN_3844_00f2 (nation EOT: treasure/ships/Europe/colonies) | inferred |  |
| `FUN_281f_0652` | 32130 | 10 | thunk | Far thunk → FUN_6f74_37a2 (set secondary side-art then flush dialog) | inferred |  |
| `FUN_281f_065e` | 32140 | 10 | thunk | Far thunk → FUN_15b3_024e (nation alt-name ptr; no independence remap) | inferred |  |
| `FUN_281f_0668` | 32150 | 10 | thunk | Far thunk → FUN_43f7_2244 (human-turn mercenary hire offer) | inferred |  |
| `FUN_281f_0676` | 32160 | 10 | thunk | Far thunk → FUN_4d56_1b3a (mid-turn Indian action) | inferred |  |
| `FUN_281f_0682` | 32170 | 10 | mapgen | tile_owner_or_presence | known | ai/accessors.c |
| `FUN_281f_068c` | 32180 | 10 | thunk | Far thunk → FUN_137f_015e (OR or AND-clear layer2 bits at tile) | inferred |  |
| `FUN_281f_0696` | 32190 | 10 | mapgen | euro_settlement_owner thunk | known | ai/accessors.c |
| `FUN_281f_06a0` | 32200 | 10 | thunk | Far thunk → FUN_137f_0194 (layer3_ptr map accessor) | inferred |  |
| `FUN_281f_06aa` | 32210 | 10 | thunk | Far thunk → FUN_137f_0614 (remap terrain type by climate DS:0x18e) | inferred |  |
| `FUN_281f_06b4` | 32220 | 10 | mapgen | continent_id thunk | known | ai/accessors.c |
| `FUN_281f_06be` | 32230 | 10 | thunk | Far thunk → FUN_137f_03e4 (tile_tribe_owner — owner if layer2 tribe bit) | inferred |  |
| `FUN_281f_06c8` | 32240 | 10 | thunk | Far thunk → FUN_137f_003c (True if /dx/,/dy/ within radius mode) | inferred |  |
| `FUN_281f_06d2` | 32250 | 10 | mapgen | tile_tribe_or_presence | known | ai/accessors.c |
| `FUN_281f_06dc` | 32260 | 10 | thunk | Far thunk → FUN_137f_0200 (owner_nibble; 0xf → −1) | known | ai/accessors.c |
| `FUN_281f_06e6` | 32270 | 10 | thunk | Far thunk → FUN_137f_044a (enemy Euro fort/colony owner vs nation) | inferred |  |
| `FUN_281f_06f0` | 32280 | 10 | thunk | Far thunk → FUN_137f_0392 (Indian settlement owner or −1) | inferred | ai/accessors.c; ai/move_spent.c |
| `FUN_281f_06fa` | 32290 | 10 | thunk | Far thunk → FUN_137f_00c0 (true if (x,y) inside camera viewport) | inferred |  |
| `FUN_281f_0704` | 32300 | 10 | thunk | Far thunk → FUN_137f_0228 (set_owner_nibble) | inferred |  |
| `FUN_281f_070e` | 32310 | 10 | thunk | Far thunk → FUN_137f_00f6 (terrain_ptr map accessor (DS:0x15c)) | inferred |  |
| `FUN_281f_0718` | 32320 | 10 | thunk | Far thunk → FUN_137f_04b0 (procedural special-resource type from seed) | inferred |  |
| `FUN_281f_0722` | 32330 | 10 | thunk | Far thunk → FUN_137f_02a0 (continent_id if in-bounds land; else −1) | inferred |  |
| `FUN_281f_072c` | 32340 | 10 | mapgen | tile_has_minor_river (via terrain_byte) | known | ai/accessors.c |
| `FUN_281f_0736` | 32350 | 10 | thunk | Far thunk → FUN_137f_02e0 (explore_plane_ptr map accessor) | inferred |  |
| `FUN_281f_0740` | 32360 | 10 | thunk | Far thunk → FUN_137f_012a (layer2_ptr map accessor) | inferred |  |
| `FUN_281f_074a` | 32370 | 10 | mapgen | tile_explore_mask | known | ai/accessors.c |
| `FUN_281f_0754` | 32380 | 10 | mapgen | tile_fa_flags thunk | known | ai/accessors.c |
| `FUN_281f_075e` | 32390 | 10 | thunk | Far thunk → FUN_137f_0598 (procedural lost-city/rumour on unowned land) | inferred | ai/accessors.c |
| `FUN_281f_0768` | 32400 | 10 | mapgen | ocean_or_high_seas thunk | known | ai/accessors.c |
| `FUN_281f_0772` | 32410 | 10 | thunk | Far thunk → FUN_7a05_03ce (heap-size abort if over DS:0x2476) | inferred |  |
| `FUN_281f_077e` | 32420 | 10 | thunk | Far thunk → FUN_7a65_00e2 (dialog id + 3 numeric substs then run) | inferred |  |
| `FUN_281f_078c` | 32430 | 10 | mapgen | terrain_class_at | known | ai/accessors.c |
| `FUN_281f_07a0` | 32440 | 10 | thunk | Far thunk → FUN_13f1_02f8 (reveal exploration bits around unit) | inferred |  |
| `FUN_281f_07aa` | 32450 | 10 | thunk | Far thunk → FUN_13f1_00a6 (reveal ±5 around colony into explore plane) | inferred |  |
| `FUN_281f_07b4` | 32460 | 10 | thunk | Far thunk → FUN_15eb_3960 (nation feature/FF bit test (table −0x77f1)) | inferred |  |
| `FUN_281f_07be` | 32470 | 10 | thunk | Far thunk → FUN_15eb_0a76 (colony index at map xy) | inferred |  |
| `FUN_281f_07c8` | 32480 | 10 | thunk | Far thunk → FUN_364b_1b4c (refresh colony warehouse-capacity slots) | inferred |  |
| `FUN_281f_07d6` | 32490 | 10 | thunk | Far thunk → FUN_1427_09ac (stack_or_nation_flag) | known | ai/move_spent.c; ai/unit_mp.c |
| `FUN_281f_07e0` | 32500 | 10 | mapgen | unit_index_on_tile | known | ai/accessors.c |
| `FUN_281f_07ea` | 32510 | 10 | thunk | Far thunk → FUN_1427_04d6 (reorder tile stack by type priority) | inferred |  |
| `FUN_281f_07f4` | 32520 | 10 | thunk | Far thunk → FUN_1427_1410 (selectable-with-moves; Europe-dock) | inferred |  |
| `FUN_281f_07fe` | 32530 | 10 | ai | unit_visibility_bits thunk | known | ai/unit_mp.c |
| `FUN_281f_0808` | 32540 | 10 | thunk | Far thunk → FUN_1427_0824 (destroy unit; compact pool + fix stacks) | inferred |  |
| `FUN_281f_0812` | 32550 | 10 | thunk | Far thunk → FUN_1427_023a (unlink unit from stack; clear xy) | inferred |  |
| `FUN_281f_081c` | 32560 | 10 | thunk | Far thunk → FUN_1427_0f0e (continent_id at unit's tile) | inferred |  |
| `FUN_281f_0826` | 32570 | 10 | thunk | Far thunk → FUN_1427_0c9a (euro visibility bitmask at tile) | inferred | ai/unit_mp.c; ai/move_spent.c §6 |
| `FUN_281f_0830` | 32580 | 10 | thunk | Far thunk → FUN_1427_14a0 (next unit matching 1410; round-robin) | inferred |  |
| `FUN_281f_083a` | 32590 | 10 | thunk | Far thunk → FUN_1427_0f30 (destroy entire transport stack via 0824) | inferred |  |
| `FUN_281f_0844` | 32600 | 10 | thunk | Far thunk → FUN_1427_02ca (place unit on tile; link stack head) | inferred |  |
| `FUN_281f_084e` | 32610 | 10 | ai | unit_post_move_chrome thunk | known | ai/unit_mp.c |
| `FUN_281f_0858` | 32620 | 10 | thunk | Far thunk → FUN_1427_0f64 (get profession low nibble (315b)) | inferred |  |
| `FUN_281f_0862` | 32630 | 10 | thunk | Far thunk → FUN_1427_0f74 (set profession low nibble (315b)) | inferred |  |
| `FUN_281f_086c` | 32640 | 10 | thunk | Far thunk → FUN_1427_08ea (select unit: tile refresh + reveal) | inferred | ai/unit_mp.c |
| `FUN_281f_0876` | 32650 | 10 | thunk | Far thunk → FUN_1427_0f8e (get profession high nibble (315b)) | inferred |  |
| `FUN_281f_0880` | 32660 | 10 | thunk | Far thunk → FUN_1427_0362 (move unit on map: unlink then place) | inferred |  |
| `FUN_281f_088a` | 32670 | 10 | thunk | Far thunk → FUN_1427_1284 (stack_has_ship) | known | ai/move_spent.c; ai/unit_mp.c |
| `FUN_281f_0894` | 32680 | 10 | thunk | Far thunk → FUN_1427_0d1e (set unit nation low-nibble) | inferred |  |
| `FUN_281f_089e` | 32690 | 10 | thunk | **Correction 2026-08-16**: the "→ FUN_1427_037e" label is wrong for this call site — canonical `281f:089e` resolves (`address_mapping.csv`, `exact`) to resident `FUN_1000_8a8e`, a 2-call stub: `FUN_1000_1e61()` (an RTLink/interrupt-flavored trampoline, `LAB_1000_39e1`/`39f1` state bytes, jumptable warnings — not game logic) then `FUN_0000_45ee(unit)` → `FUN_0000_45d2(unit, unit.x[+0x3144], unit.y[+0x3145])`, i.e. "re-place/redraw unit at its own xy" — so the *old* label's verb was right, just attributed to the wrong symbol/segment. Verified directly via `GhidraDecompileAt.java 0000:18a8e` against `~/projects/ghidra_overlay_scratch` `OverlayTest`. See `turn/europe_nation_eot.md` "Transit turns — dead end" for why this matters (Harbor-menu Sail case). | known | turn/europe_nation_eot.md |
| `FUN_281f_08a8` | 32700 | 10 | thunk | Far thunk → FUN_1427_14f4 (nearest unit of nation by dos_dist) | inferred |  |
| `FUN_281f_08b2` | 32710 | 10 | thunk | Far thunk → FUN_1427_0fa0 (set profession high nibble (315b)) | inferred |  |
| `FUN_281f_08bc` | 32720 | 10 | thunk | Far thunk → FUN_1427_0d38 (unit/stack cargo+combat query dispatcher) | inferred |  |
| `FUN_281f_08c6` | 32730 | 10 | thunk | Far thunk → FUN_1427_03a0 (demote unit to stack bottom) | inferred |  |
| `FUN_281f_08d0` | 32740 | 10 | thunk | Far thunk → FUN_1427_0fc0 (true if mounted type) | inferred |  |
| `FUN_281f_08da` | 32750 | 10 | ai | stack_facing_refresh thunk | known | ai/unit_mp.c |
| `FUN_281f_08e4` | 32760 | 10 | thunk | Far thunk → FUN_1427_0644 (tile_stack_head) | known | ai/move_spent.c; ai/unit_mp.c |
| `FUN_281f_08ee` | 32770 | 10 | thunk | Far thunk → FUN_1427_0164 (distance to stack head via transport_next) | inferred |  |
| `FUN_281f_08f8` | 32780 | 10 | thunk | Far thunk → FUN_1427_12c6 (set orders byte for whole stack) | inferred | ai/unit_mp.c |
| `FUN_281f_0902` | 32790 | 10 | thunk | Far thunk → FUN_1427_0fec (true if armed combat type) | inferred |  |
| `FUN_281f_090c` | 32800 | 10 | ai | unit_max_mp thunk | known | ai/unit_mp.c |
| `FUN_281f_0916` | 32810 | 10 | ai | unit_tile_list_refresh thunk | known | ai/unit_mp.c |
| `FUN_281f_0920` | 32820 | 10 | thunk | Far thunk → FUN_1427_10be (ship passenger embark/capacity bookkeeping) | inferred |  |
| `FUN_281f_092a` | 32830 | 10 | thunk | Far thunk → FUN_1427_0180 (Nth non-transport unit in stack) | inferred |  |
| `FUN_281f_0934` | 32840 | 20 | ai | unit_exhaust_mp thunk | known | ai/unit_mp.c |
| `FUN_281f_0948` | 32860 | 10 | ai | stack_set_xy thunk | known | ai/unit_mp.c |
| `FUN_281f_0952` | 32870 | 10 | thunk | Far thunk → FUN_1427_0bce (if no tribe on tile, neighbor probe) | inferred |  |
| `FUN_281f_095c` | 32880 | 10 | thunk | Far thunk → FUN_1427_06b4 (allocate/spawn unit into pool and place) | inferred |  |
| `FUN_281f_0966` | 32890 | 10 | thunk | Far thunk → FUN_1427_1330 (has moves + on-map + not sentry/fortified) | inferred | ai/unit_mp.c |
| `FUN_281f_0970` | 32900 | 10 | thunk | Far thunk → FUN_1427_0bfe (true if 8-adj has presence/colony of nation) | inferred | ai/unit_mp.c |
| `FUN_281f_097a` | 32910 | 10 | ai | unit_has_moves_remaining thunk | known | ai/unit_mp.c |
| `FUN_281f_0984` | 32920 | 10 | thunk | Far thunk → FUN_1427_09dc (8-adj foreign same-continent owner probe) | inferred | ai/unit_mp.c |
| `FUN_281f_098e` | 32930 | 10 | thunk | Far thunk → FUN_1427_0026 (walk transport_prev to stack tail/bottom) | inferred |  |
| `FUN_281f_0998` | 32940 | 10 | thunk | Far thunk → FUN_6f74_36ca (parse+run+free dialog; return choice) | inferred |  |
| `FUN_281f_09a4` | 32950 | 10 | ui | Nation name string ptr thunk→15b3_01e0 (dialog subst) | inferred | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_09ae` | 32960 | 10 | thunk | Far thunk → FUN_6f74_042c (set dialog numeric subst far-ptr slot) | inferred |  |
| `FUN_281f_09ba` | 32970 | 10 | thunk | Far thunk → FUN_6b7e_0004 (map viewport blit/refresh frame) | inferred |  |
| `FUN_281f_09c8` | 32980 | 10 | thunk | Far thunk → FUN_157e_004a (unit base combat×8 + vet/Drake/damage) | inferred |  |
| `FUN_281f_09d2` | 32990 | 10 | thunk | Far thunk → FUN_157e_0008 (count village defense bonus 0..3) | inferred |  |
| `FUN_281f_09dc` | 33000 | 10 | thunk | Far thunk → FUN_157e_015e (full engagement strength w/ fort/terrain) | inferred |  |
| `FUN_281f_09e6` | 33010 | 10 | thunk | Far thunk → FUN_15eb_002c (set active colony pointer + visibility) | inferred |  |
| `FUN_281f_09f0` | 33020 | 10 | thunk | Far thunk → FUN_4cc6_0304 (find tribe index at (x,y) or −1) | inferred |  |
| `FUN_281f_09fc` | 33030 | 20 | thunk | Far thunk → FUN_15eb_038e (test building bit for active colony) | inferred |  |
| `FUN_281f_0a10` | 33050 | 10 | thunk | Far thunk → FUN_15b3_00d0 (clear diplomacy bit both directions) | inferred |  |
| `FUN_281f_0a1a` | 33060 | 10 | ui | Nation name string ptr thunk→15b3_0198 (dialog subst) | inferred | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_0a24` | 33070 | 10 | thunk | Far thunk → FUN_15b3_0228 (nation name string ptr (no indep remap)) | inferred |  |
| `FUN_281f_0a2e` | 33080 | 10 | thunk | Far thunk → FUN_15b3_0274 (nation name-record base (rebel→player)) | inferred |  |
| `FUN_281f_0a38` | 33090 | 10 | thunk | Far thunk → FUN_15b3_0004 (read nation diplomacy/treaty byte) | inferred |  |
| `FUN_281f_0a42` | 33100 | 10 | ai | indian_select_nation_context thunk→15dc_0006 (bind 8d52/8d50/8d4e) | known | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_0a4c` | 33110 | 10 | thunk | Far thunk → FUN_15dc_0032 (bind current tribe/Indian context) | known | include/viceroy_globals.h |
| `FUN_281f_0a56` | 33120 | 10 | thunk | Far thunk → FUN_15dc_006a (Indian nation class/tier 1/2/3) | inferred |  |
| `FUN_281f_0a60` | 33130 | 10 | thunk | Far thunk → FUN_15dc_00a2 (bucket int into quartile 0..3) | inferred |  |
| `FUN_281f_0a6a` | 33140 | 10 | thunk | Far thunk → FUN_15eb_17fa (resource-vs-job bonus table for yield) | inferred |  |
| `FUN_281f_0a74` | 33150 | 10 | thunk | Far thunk → FUN_15eb_0f1c (resolve colonist people-band sprite/icon) | inferred |  |
| `FUN_281f_0a7e` | 33160 | 10 | thunk | Far thunk → FUN_15eb_0cbc (write packed colonist specialty nibble) | inferred |  |
| `FUN_281f_0a88` | 33170 | 10 | thunk | Far thunk → FUN_15eb_14aa (walk building parent chain to root id) | inferred |  |
| `FUN_281f_0a92` | 33180 | 10 | thunk | Far thunk → FUN_15eb_0544 (read nation treasury dword) | inferred |  |
| `FUN_281f_0a9c` | 33190 | 10 | thunk | Far thunk → FUN_15eb_0d04 (remove colony colonist slot; compact) | inferred |  |
| `FUN_281f_0aa6` | 33200 | 10 | thunk | Far thunk → FUN_15eb_169c (redraw work-plot of colonist via 06d2) | inferred |  |
| `FUN_281f_0ab0` | 33210 | 10 | thunk | Far thunk → FUN_15eb_039e (count owned buildings along parent chain) | inferred |  |
| `FUN_281f_0aba` | 33220 | 10 | thunk | Far thunk → FUN_15eb_0556 (add to nation treasury w/ clamp) | inferred |  |
| `FUN_281f_0ac4` | 33230 | 10 | thunk | Far thunk → FUN_15eb_33aa (build-menu hammer cost for item) | inferred |  |
| `FUN_281f_0ace` | 33240 | 10 | thunk | Far thunk → FUN_15eb_14d6 (building/job id → table byte DS:0x2ca) | inferred |  |
| `FUN_281f_0ad8` | 33250 | 10 | thunk | Far thunk → FUN_15eb_35d0 (move warehouse cargo onto unit via 30b8) | inferred |  |
| `FUN_281f_0ae2` | 33260 | 10 | thunk | Far thunk → FUN_15eb_38ba (count eligible build-menu items) | inferred |  |
| `FUN_281f_0aec` | 33270 | 10 | thunk | Far thunk → FUN_15eb_317c (remove unit cargo/passenger slot; compact) | inferred |  |
| `FUN_281f_0af6` | 33280 | 10 | thunk | Far thunk → FUN_15eb_0596 (subtract from nation treasury) | inferred |  |
| `FUN_281f_0b00` | 33290 | 10 | thunk | Far thunk → FUN_15eb_0aec (job→building-chain start id (or −1)) | inferred |  |
| `FUN_281f_0b0a` | 33300 | 10 | thunk | Far thunk → FUN_15eb_0fea (classify profession change (0..3)) | inferred |  |
| `FUN_281f_0b14` | 33310 | 10 | thunk | Far thunk → FUN_15eb_03d6 (count owned buildings along parent chain) | inferred |  |
| `FUN_281f_0b1e` | 33320 | 10 | thunk | Far thunk → FUN_15eb_05b2 (format nation gold into UI string) | inferred |  |
| `FUN_281f_0b28` | 33330 | 10 | thunk | Far thunk → FUN_15eb_08e6 (true if unit type maps to a profession) | inferred |  |
| `FUN_281f_0b32` | 33340 | 10 | thunk | Far thunk → FUN_15eb_2f8e (Nth cargo-capable unit in stack) | inferred |  |
| `FUN_281f_0b3c` | 33350 | 10 | thunk | Far thunk → FUN_15eb_18ec (compose field yield for one 5×5 work plot) | inferred |  |
| `FUN_281f_0b46` | 33360 | 10 | thunk | Far thunk → FUN_15eb_0d8e (fill leave-as gear cargo-id list; count) | inferred |  |
| `FUN_281f_0b50` | 33370 | 10 | thunk | Far thunk → FUN_15eb_0b0c (free warehouse headroom for cargo) | inferred |  |
| `FUN_281f_0b5a` | 33380 | 10 | thunk | Far thunk → FUN_15eb_05cc (present nation treasury via 1009_01fc) | inferred |  |
| `FUN_281f_0b64` | 33390 | 10 | thunk | Far thunk → FUN_15eb_38e8 (Nth eligible build-menu item id) | inferred |  |
| `FUN_281f_0b6e` | 33400 | 10 | thunk | Far thunk → FUN_15eb_28c8 (score/assign best work-plot job) | inferred |  |
| `FUN_281f_0b78` | 33410 | 10 | thunk | Far thunk → FUN_15eb_0902 (default profession for unit type) | inferred |  |
| `FUN_281f_0b82` | 33420 | 10 | thunk | Far thunk → FUN_15eb_1376 (count colonists with given job) | inferred |  |
| `FUN_281f_0b8c` | 33430 | 10 | thunk | Far thunk → FUN_15eb_3650 (build-menu eligibility for item) | inferred |  |
| `FUN_281f_0b96` | 33440 | 10 | thunk | Far thunk → FUN_15eb_3208 (free unit cargo capacity for type) | inferred |  |
| `FUN_281f_0ba0` | 33450 | 10 | thunk | Far thunk → FUN_15eb_0410 (walk building parent chain to root) | inferred |  |
| `FUN_281f_0baa` | 33460 | 10 | thunk | Far thunk → FUN_15eb_1568 (count workers in building / −unit if stockade) | inferred |  |
| `FUN_281f_0bb4` | 33470 | 10 | thunk | Far thunk → FUN_15eb_3454 (gate: can start job/construction) | inferred |  |
| `FUN_281f_0bbe` | 33480 | 10 | thunk | Far thunk → FUN_15eb_1030 (set/clear colony flag bit at +0x84) | inferred |  |
| `FUN_281f_0bc8` | 33490 | 10 | thunk | Far thunk → FUN_15eb_0924 (Nth profession-capable unit in colony) | inferred |  |
| `FUN_281f_0bd2` | 33500 | 10 | thunk | Far thunk → FUN_15eb_268e (fill 5×5 work-plot flag cache) | inferred |  |
| `FUN_281f_0bdc` | 33510 | 10 | thunk | Far thunk → FUN_15eb_0434 (highest owned building along parent chain) | inferred |  |
| `FUN_281f_0be6` | 33520 | 10 | thunk | Far thunk → FUN_15eb_2ff2 (read unit passenger profession nibble) | inferred |  |
| `FUN_281f_0bf0` | 33530 | 10 | thunk | Far thunk → FUN_15eb_13ac (count colonists by specialty) | inferred |  |
| `FUN_281f_0bfa` | 33540 | 10 | thunk | Far thunk → FUN_15eb_394c (refresh colony unit lists + production) | inferred |  |
| `FUN_281f_0c04` | 33550 | 10 | thunk | Far thunk → FUN_15eb_1f72 (recompute colony production totals) | inferred |  |
| `FUN_281f_0c0e` | 33560 | 10 | thunk | Far thunk → FUN_15eb_0e18 (colonist profession: slot or outside unit) | inferred |  |
| `FUN_281f_0c18` | 33570 | 10 | thunk | Far thunk → FUN_15eb_022c (lookup word from remapped id table (−0x715e)) | inferred |  |
| `FUN_281f_0c22` | 33580 | 10 | thunk | Far thunk → FUN_15eb_3956 (full colony-view refresh compose) | inferred |  |
| `FUN_281f_0c2c` | 33590 | 10 | thunk | Far thunk → FUN_15eb_32a0 (find unit cargo slot index by type) | inferred |  |
| `FUN_281f_0c36` | 33600 | 10 | thunk | Far thunk → FUN_15eb_1068 (assign colony colonist to job) | inferred |  |
| `FUN_281f_0c40` | 33610 | 10 | thunk | Far thunk → FUN_15eb_0242 (cargo catalog secondary word at id×8) | inferred |  |
| `FUN_281f_0c4a` | 33620 | 10 | thunk | Far thunk → FUN_15eb_096e (unit id → profession-capable ordinal +colony+0x1f) | inferred |  |
| `FUN_281f_0c54` | 33630 | 10 | thunk | Far thunk → FUN_15eb_0e52 (colonist workplace/job read) | inferred |  |
| `FUN_281f_0c5e` | 33640 | 10 | thunk | Far thunk → FUN_15eb_0470 (work-radius size from fortification) | inferred |  |
| `FUN_281f_0c68` | 33650 | 10 | thunk | Far thunk → FUN_15eb_3040 (read unit cargo/passenger qty byte) | inferred |  |
| `FUN_281f_0c72` | 33660 | 10 | thunk | Far thunk → FUN_15eb_26e4 (fill 5×5 native-contact caches) | inferred |  |
| `FUN_281f_0c7c` | 33670 | 10 | thunk | Far thunk → FUN_15eb_0484 (fort size class → capacity 8/12/32) | inferred |  |
| `FUN_281f_0c86` | 33680 | 10 | thunk | Far thunk → FUN_15eb_0274 (colony SoL/happiness score 0..100) | inferred |  |
| `FUN_281f_0c90` | 33690 | 10 | thunk | Far thunk → FUN_15eb_0668 (set/clear layer2 bit0x10 on town tile) | inferred |  |
| `FUN_281f_0c9a` | 33700 | 10 | thunk | Far thunk → FUN_15eb_0002 (gate workplace/job id) | inferred |  |
| `FUN_281f_0ca4` | 33710 | 10 | thunk | Far thunk → FUN_15eb_3054 (write unit cargo/passenger qty byte) | inferred |  |
| `FUN_281f_0cae` | 33720 | 10 | thunk | Far thunk → FUN_15eb_0e8c (set workplace/job) | inferred |  |
| `FUN_281f_0cb8` | 33730 | 10 | thunk | Far thunk → FUN_15eb_1604 (colony production-panel layout sizes) | inferred |  |
| `FUN_281f_0cc2` | 33740 | 10 | thunk | Far thunk → FUN_15eb_32f8 (decode build-menu index → kind+idx) | inferred |  |
| `FUN_281f_0ccc` | 33750 | 10 | thunk | Far thunk → FUN_15eb_09c0 (recount colony unit tallies) | inferred |  |
| `FUN_281f_0cd6` | 33760 | 10 | thunk | Far thunk → FUN_15eb_1d4c (building/manufacturing yield for colonist) | inferred |  |
| `FUN_281f_0ce0` | 33770 | 10 | thunk | Far thunk → FUN_15eb_06a6 (colonist-slot on town-area tile) | inferred |  |
| `FUN_281f_0cea` | 33780 | 10 | thunk | Far thunk → FUN_15eb_306a (write unit passenger/cargo type nibble) | inferred |  |
| `FUN_281f_0cf4` | 33790 | 10 | thunk | Far thunk → FUN_15eb_142a (find nth colonist slot with given job) | inferred |  |
| `FUN_281f_0cfe` | 33800 | 10 | thunk | Far thunk → FUN_15eb_0302 (test building-present bit) | inferred |  |
| `FUN_281f_0d08` | 33810 | 10 | thunk | Far thunk → FUN_15eb_0c52 (cargo exceeds warehouse capacity?) | inferred |  |
| `FUN_281f_0d12` | 33820 | 10 | thunk | Far thunk → FUN_15eb_00a2 (any 8-neighbor ocean/high-seas) | inferred |  |
| `FUN_281f_0d1c` | 33830 | 10 | thunk | Far thunk → FUN_15eb_0c7a (read packed colonist specialty nibble) | inferred |  |
| `FUN_281f_0d26` | 33840 | 10 | thunk | Far thunk → FUN_15eb_0326 (set/clear building-present bit in colony) | inferred |  |
| `FUN_281f_0d30` | 33850 | 10 | thunk | Far thunk → FUN_15eb_1646 (find 5×5 work-plot cell for colonist) | inferred |  |
| `FUN_281f_0d3a` | 33860 | 10 | thunk | Far thunk → FUN_15eb_0a50 (warehouse capacity 100×(1+expansion)) | inferred |  |
| `FUN_281f_0d44` | 33870 | 10 | thunk | Far thunk → FUN_15eb_06d2 (shared world-map / pedia draw entry) | inferred |  |
| `FUN_281f_0d4e` | 33880 | 10 | thunk | Far thunk → FUN_15eb_334a (play build-menu item sound) | inferred |  |
| `FUN_281f_0d58` | 33890 | 10 | thunk | Far thunk → FUN_15eb_30b8 (adjust unit cargo qty by profession) | inferred |  |
| `FUN_281f_0d62` | 33900 | 10 | thunk | Far thunk → FUN_15eb_1476 (reseed DOS LCG from colony tile XY) | inferred |  |
| `FUN_281f_0d6c` | 33910 | 10 | ai | Apply Indian↔Euro relation delta (+dialogs) thunk→4cc6_00f2 | inferred | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_281f_0d78` | 33920 | 10 | thunk | Far thunk → FUN_4cc6_07c2 (Indian contact/alarm distance score) | inferred |  |
| `FUN_281f_0d84` | 33930 | 10 | thunk | Far thunk → FUN_4cc6_0356 (nearest village to xy) | inferred |  |
| `FUN_281f_0d9a` | 33940 | 10 | thunk | Far thunk → FUN_1984_03b2 (ensure viewport contains tile (x,y)) | inferred |  |
| `FUN_281f_0da4` | 33950 | 10 | thunk | Far thunk → FUN_1984_03ca (nudge focus by dir8; scroll+chrome) | inferred |  |
| `FUN_281f_0dae` | 33960 | 10 | ui | Progress-beat / turn-owner chrome thunk→1984_006a (6d8e section markers) | inferred | ai/euro_dispatcher.c |
| `FUN_281f_0db8` | 33970 | 10 | thunk | Far thunk → FUN_1984_00e8 (set focus tile if in-map) | inferred |  |
| `FUN_281f_0dc2` | 33980 | 10 | thunk | Far thunk → FUN_1984_043a (arm timed status overlay then draw) | inferred |  |
| `FUN_281f_0dcc` | 33990 | 10 | thunk | Far thunk → FUN_1984_010a (toggle focus blink; refresh focus chrome) | inferred |  |
| `FUN_281f_0dd6` | 34000 | 10 | thunk | Far thunk → FUN_1984_045a (append status-bar string from 0x2dba) | inferred |  |
| `FUN_281f_0de0` | 34010 | 10 | thunk | Far thunk → FUN_1984_046e (status line: verb string + timed overlay) | inferred |  |
| `FUN_281f_0dea` | 34020 | 10 | thunk | Far thunk → FUN_1984_0490 (present/clear top status strip (7px)) | inferred |  |
| `FUN_281f_0df4` | 34030 | 10 | thunk | Far thunk → FUN_1984_053a (unit status chrome nation+type+verb) | inferred |  |
| `FUN_281f_0dfe` | 34040 | 10 | thunk | Far thunk → FUN_1984_05b8 (unit status chrome (verb idx7)) | inferred |  |
| `FUN_281f_0e08` | 34050 | 10 | thunk | Far thunk → FUN_1984_029e (recenter viewport on tile + chrome) | inferred |  |
| `FUN_281f_0e12` | 34060 | 10 | thunk | Far thunk → FUN_1984_0636 (unit status chrome (verb idx6)) | inferred |  |
| `FUN_281f_0e1c` | 34070 | 10 | thunk | Far thunk → FUN_6b7e_00c0 (map overlay/chrome paint pass) | inferred |  |
| `FUN_281f_0e2a` | 34080 | 10 | thunk | Far thunk → FUN_6b22_03f6 (toggle/refresh selected-unit overlay blink) | inferred |  |
| `FUN_281f_0e38` | 34090 | 10 | thunk | Far thunk → FUN_6a9f_0360 (redraw clamped minimap sub-rect) | inferred |  |
| `FUN_281f_0e46` | 34100 | 10 | thunk | Far thunk → FUN_49dd_0156 (draw status-panel footer/help strip) | inferred |  |
| `FUN_281f_0e52` | 34110 | 10 | thunk | Far thunk → FUN_4b58_093c (draw menu-bar strip + enabled titles) | inferred |  |
| `FUN_281f_0e5e` | 34120 | 10 | thunk | Far thunk → FUN_1984_06b4 (stub: always return 0) | inferred |  |
| `FUN_281f_0e68` | 34130 | 10 | thunk | Far thunk → FUN_19ef_0008 (seed DOS LCG from BIOS timer) | inferred |  |
| `FUN_281f_0e72` | 34140 | 10 | thunk | Far thunk → FUN_1c0c_0012 (read BIOS timer dword at 40:6c/6e) | inferred |  |
| `FUN_281f_0e7c` | 34150 | 10 | thunk | Far thunk → FUN_19f6_00b0 (copy/localize string; path-join if flag) | inferred |  |
| `FUN_281f_0e86` | 34160 | 10 | thunk | Far thunk → FUN_19f6_00fa (format path via 00b0 then open file) | inferred |  |
| `FUN_281f_0e90` | 34170 | 10 | thunk | Far thunk → FUN_19f6_0138 (DOS file-exists probe via 1b22_0022) | inferred |  |
| `FUN_281f_0e9a` | 34180 | 10 | thunk | Far thunk → FUN_19f6_0002 (overlay zero-padded decimal digits into string) | inferred |  |
| `FUN_281f_0eae` | 34190 | 10 | thunk | Far thunk → FUN_1a0a_0004 (init palette-cycle anim timers) | inferred |  |
| `FUN_281f_0eb8` | 34200 | 10 | thunk | Far thunk → FUN_1a29_015b (install IRQ0 timer / PIT hook) | inferred |  |
| `FUN_281f_0ec2` | 34210 | 10 | thunk | Far thunk → FUN_1a29_0209 (arm custom timer alarm reload/elapsed) | inferred |  |
| `FUN_281f_0ecc` | 34220 | 10 | thunk | Far thunk → FUN_1a4e_001c (clip rect vs viewport; nonzero if visible) | inferred |  |
| `FUN_281f_0ed6` | 34230 | 10 | thunk | Far thunk → FUN_1d11_0000 (stash INT10 mode; optional set) | inferred |  |
| `FUN_281f_0ee0` | 34240 | 10 | thunk | Far thunk → FUN_1ae3_0006 (zero-fill stack from watermark up to SP) | inferred |  |
| `FUN_281f_0f24` | 34250 | 10 | thunk | Far thunk → FUN_2b5a_23ce (Sound Options dialog) | inferred |  |
| `FUN_281f_0f30` | 34260 | 10 | thunk | Far thunk → FUN_2b5a_32ee (Enter: open owned colony at focus) | inferred |  |
| `FUN_281f_0f3c` | 34270 | 10 | thunk | Far thunk → FUN_2b5a_0000 (prefetch options-dialog string slots) | inferred |  |
| `FUN_281f_0f54` | 34280 | 10 | thunk | Far thunk → FUN_2b5a_2464 (map menu-command mega-dispatch) | inferred |  |
| `FUN_281f_0f6c` | 34290 | 10 | thunk | Far thunk → FUN_2b5a_001e (European Status bring-up dialog) | inferred |  |
| `FUN_281f_0f78` | 34300 | 10 | thunk | Far thunk → FUN_2b5a_26f6 (GAME menu item dispatch) | inferred |  |
| `FUN_281f_0f90` | 34310 | 10 | thunk | Far thunk → FUN_2b5a_1b5a (activate-unit stack picker) | inferred |  |
| `FUN_281f_0f9c` | 34320 | 10 | thunk | Far thunk → FUN_2b5a_0e52 (select/activate unit UI; Move Pieces) | inferred |  |
| `FUN_281f_0fa8` | 34330 | 10 | thunk | Far thunk → FUN_2b5a_3344 (Enter/Space: colony or clear View Pieces) | inferred |  |
| `FUN_281f_0fcc` | 34340 | 10 | thunk | Far thunk → FUN_2b5a_36e6 (minimap drag: pan viewport) | inferred |  |
| `FUN_281f_0ff0` | 34350 | 10 | thunk | Far thunk → FUN_2b5a_303c (map keyboard/input dispatch) | inferred |  |

### Segment `291f` (271 defs) — thunk — Far thunks: EMS page-in then overlay JMPF (2f2b/38fd/6f74/...)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_291f_0018` | 34360 | 10 | thunk | Far thunk → FUN_2b5a_3ae6 (Move Pieces frame: poll then tick) | inferred |  |
| `FUN_291f_0030` | 34370 | 10 | thunk | Far thunk → FUN_2b5a_3752 (mouse-up: clear blink/View/end-turn) | inferred |  |
| `FUN_291f_0048` | 34380 | 10 | thunk | Far thunk → FUN_2b5a_33ce (mouse hit-test region 1..3) | inferred |  |
| `FUN_291f_006c` | 34390 | 10 | thunk | Far thunk → FUN_2b5a_0902 (baseline-enable menus; View Pieces) | inferred |  |
| `FUN_291f_0078` | 34400 | 10 | thunk | Far thunk → FUN_2b5a_37b2 (mouse-button dispatch by hit region) | inferred |  |
| `FUN_291f_0084` | 34410 | 10 | thunk | Far thunk → FUN_2b5a_3442 (set mouse cursor from table) | inferred |  |
| `FUN_291f_0090` | 34420 | 10 | thunk | Far thunk → FUN_2b5a_20f6 (Game Options checkbox dialog) | inferred |  |
| `FUN_291f_00a8` | 34430 | 10 | thunk | Far thunk → FUN_2b5a_3458 (set cursor wrapper arg 1) | inferred |  |
| `FUN_291f_00c0` | 34440 | 10 | thunk | Far thunk → FUN_2b5a_0b08 (order-menu gates: cargo/wagon) | inferred |  |
| `FUN_291f_00cc` | 34450 | 10 | thunk | Far thunk → FUN_2b5a_3462 (map click: select/goto/recenter) | inferred |  |
| `FUN_291f_00e4` | 34460 | 10 | thunk | Far thunk → FUN_2b5a_0b34 (contextual ORDERS menu enable) | inferred |  |
| `FUN_291f_00f0` | 34470 | 10 | thunk | Far thunk → FUN_2b5a_3802 (map input poll loop) | inferred |  |
| `FUN_291f_0108` | 34480 | 10 | thunk | Far thunk → FUN_2b5a_223a (Colony Report Options dialog) | inferred |  |
| `FUN_291f_0120` | 34490 | 10 | thunk | Far thunk → FUN_6f74_37fc (Tip/help dialog variant (DS:0x2008 mode) via 32a4+2580) | inferred |  |
| `FUN_291f_012c` | 34500 | 10 | thunk | Far thunk → FUN_7a65_0008 (label viewport tile (hidden-terrain chrome)) | inferred |  |
| `FUN_291f_013a` | 34510 | 10 | thunk | Far thunk → FUN_4b58_05c6 (set/clear menu-item hidden flag bit1) | inferred |  |
| `FUN_291f_0146` | 34520 | 10 | thunk | Far thunk → FUN_4b58_0552 (set/clear menu-item disabled flag bit0) | inferred |  |
| `FUN_291f_0152` | 34530 | 10 | thunk | Far thunk → FUN_4b58_05f6 (Clear hidden bit on every item under every menu) | inferred |  |
| `FUN_291f_015e` | 34540 | 10 | thunk | Far thunk → FUN_4b58_0582 (Clear disabled bit on every item under every menu) | inferred |  |
| `FUN_291f_016a` | 34550 | 10 | thunk | Far thunk → FUN_6f74_2580 (modal dialog input loop) | inferred |  |
| `FUN_291f_0176` | 34560 | 10 | thunk | Far thunk → FUN_6f74_0a00 (append labeled option/button node) | inferred |  |
| `FUN_291f_0182` | 34570 | 10 | thunk | Far thunk → FUN_6f74_32a4 (parse @-directive dialog script) | inferred |  |
| `FUN_291f_018e` | 34580 | 10 | thunk | Far thunk → FUN_6ba1_000c (Compute viewport origin/size/tile-px from zoom+camera; clamp rim) | inferred |  |
| `FUN_291f_019c` | 34590 | 10 | thunk | Far thunk → FUN_6f74_3760 (set side-art style then flush/run dialog) | inferred |  |
| `FUN_291f_01a8` | 34600 | 10 | thunk | Far thunk → FUN_7ada_01aa (DOS INT21 AH=49 free mem block) | inferred |  |
| `FUN_291f_01b6` | 34610 | 10 | thunk | Far thunk → FUN_6f74_08fa (set/clear option disabled bit0) | inferred |  |
| `FUN_291f_01c2` | 34620 | 10 | thunk | Far thunk → FUN_479b_01a6 (pioneer clear/plow work-tick) | inferred |  |
| `FUN_291f_01d0` | 34630 | 10 | thunk | Far thunk → FUN_479b_0f60 (unload best cargo unit→colony) | inferred |  |
| `FUN_291f_01de` | 34640 | 10 | thunk | Far thunk → FUN_479b_11a4 (load best cargo colony→unit) | inferred |  |
| `FUN_291f_01fa` | 34650 | 10 | thunk | Far thunk → FUN_479b_076e (found-colony order body) | inferred |  |
| `FUN_291f_0208` | 34660 | 10 | thunk | Far thunk → FUN_48d3_007a (landfall arrival stack goto/unload) | inferred |  |
| `FUN_291f_0216` | 34670 | 10 | thunk | Far thunk → FUN_479b_0526 (pioneer road work-tick) | inferred |  |
| `FUN_291f_0224` | 34680 | 10 | thunk | Far thunk → FUN_6f74_112a (alloc custom hit-target node at dialog+0x64) | inferred |  |
| `FUN_291f_0230` | 34690 | 10 | thunk | Far thunk → FUN_6f74_0f3c (append icon/image row to dialog) | inferred |  |
| `FUN_291f_023c` | 34700 | 10 | thunk | Far thunk → FUN_6f74_06d0 (alloc+init dialog box record) | inferred |  |
| `FUN_291f_0248` | 34710 | 10 | thunk | Far thunk → FUN_4d56_00e0 (Indian AI act chain →01e2/14fe) | inferred |  |
| `FUN_291f_0254` | 34720 | 10 | thunk | Far thunk → FUN_364b_1e64 (destroy colony; compact table + fix refs) | inferred |  |
| `FUN_291f_0262` | 34730 | 10 | thunk | Far thunk → FUN_6f74_3704 (Set/clear one bit in dialog checkbox mask (DS:0x1f54)) | inferred |  |
| `FUN_291f_026e` | 34740 | 10 | thunk | Far thunk → FUN_6f74_36fc (Clear dialog checkbox bitmask (DS:0x1f54)) | inferred |  |
| `FUN_291f_027a` | 34750 | 10 | thunk | Far thunk → FUN_7b04_001e (Max of DOS free probe (0002) vs XMS UMB size (2100_000e)) | inferred |  |
| `FUN_291f_0288` | 34760 | 10 | thunk | Far thunk → FUN_6afa_0168 (LFSR-order present each viewport tile (dissolve refresh)) | inferred |  |
| `FUN_291f_0296` | 34770 | 10 | thunk | Far thunk → FUN_6afa_0132 (Blit full map viewport buffer to screen (240×192)) | inferred |  |
| `FUN_291f_02a4` | 34780 | 10 | thunk | Far thunk → FUN_6ba1_1028 (Optional letterbox fill then viewport tile blit (0d6c)) | inferred |  |
| `FUN_291f_02b2` | 34790 | 10 | thunk | Far thunk → FUN_479b_0bd0 (Goto-colony order body (path, dock, unload/load)) | inferred |  |
| `FUN_291f_02c0` | 34800 | 10 | thunk | Far thunk → FUN_647e_090a (Colonist-slot picker dialog for current colony) | inferred |  |
| `FUN_291f_02ce` | 34810 | 10 | thunk | Far thunk → FUN_647e_0000 (bind colony record pointer) | inferred |  |
| `FUN_291f_02dc` | 34820 | 10 | thunk | Far thunk → FUN_647e_0796 (Colony list picker dialog; returns selected idx) | inferred |  |
| `FUN_291f_02ea` | 34830 | 10 | thunk | Far thunk → FUN_48d3_015e (spiral-find High Seas; set sail/goto) | inferred |  |
| `FUN_291f_02f8` | 34840 | 10 | thunk | Far thunk → FUN_647e_01c6 (Paginated colony-select dialog for unit) | inferred |  |
| `FUN_291f_0306` | 34850 | 10 | thunk | Far thunk → FUN_6f74_372e (Test one bit in dialog checkbox mask (DS:0x1f54)) | inferred |  |
| `FUN_291f_0320` | 34860 | 10 | thunk | Far thunk → FUN_7562_04e8 (manual Load slot picker then load) | inferred |  |
| `FUN_291f_032e` | 34870 | 10 | thunk | Far thunk → FUN_7562_030a (Manual Save slot picker (slots 0-7) then write) | inferred |  |
| `FUN_291f_033c` | 34880 | 10 | thunk | Far thunk → FUN_6f74_092a (set/clear option flag bit1) | inferred |  |
| `FUN_291f_0348` | 34890 | 10 | thunk | Far thunk → FUN_43f7_1528 (REF arrival announce; set 0x5382 bit1 (force present)) | inferred |  |
| `FUN_291f_0356` | 34900 | 10 | thunk | Far thunk → FUN_43f7_1a26 (Declare independence: crown setup, wipe other Euros, REF pools, war flag) | inferred |  |
| `FUN_291f_0364` | 34910 | 10 | thunk | Far thunk → FUN_43f7_0218 (Crown-nation bootstrap: fold status≠0 Euro into peer; set DS:0x53d2) | inferred |  |
| `FUN_291f_03aa` | 34920 | 10 | thunk | Far thunk → FUN_41f2_0092 (nation score + optional report UI) | inferred |  |
| `FUN_291f_03b8` | 34930 | 10 | thunk | Far thunk → FUN_3f41_2548 (Foreign Affairs Advisor F8) | inferred |  |
| `FUN_291f_03c6` | 34940 | 10 | thunk | Far thunk → FUN_3f41_220c (unit disposition list) | inferred |  |
| `FUN_291f_03d4` | 34950 | 10 | thunk | Far thunk → FUN_3f41_1ed8 (Naval Adviser body) | inferred |  |
| `FUN_291f_03e2` | 34960 | 10 | thunk | Far thunk → FUN_3f41_1710 (Economic Adviser F5) | inferred |  |
| `FUN_291f_03f0` | 34970 | 10 | thunk | Far thunk → FUN_3f41_10d8 (Labor Adviser F4) | inferred |  |
| `FUN_291f_03fe` | 34980 | 10 | thunk | Far thunk → FUN_3f41_06d0 (Religious Adviser F2) | inferred |  |
| `FUN_291f_040c` | 34990 | 10 | thunk | Far thunk → FUN_3f41_0618 (Continental Congress report F3) | inferred |  |
| `FUN_291f_041a` | 35000 | 10 | thunk | Far thunk → FUN_3f41_010a (Indian Adviser report F9) | inferred |  |
| `FUN_291f_0428` | 35010 | 10 | thunk | Far thunk → FUN_6cb2_0eac (Colonizopedia terrain article) | inferred |  |
| `FUN_291f_0436` | 35020 | 10 | thunk | Far thunk → FUN_6f74_3848 (number-entry dialog; stash at 9cc8) | inferred |  |
| `FUN_291f_044e` | 35030 | 10 | thunk | Far thunk → FUN_4720_049e (embark/naval order UI dispatch) | inferred |  |
| `FUN_291f_045c` | 35040 | 10 | thunk | Far thunk → FUN_4b58_051a (Set/clear menu disabled flag (bit0 at menu+0xc)) | inferred |  |
| `FUN_291f_0468` | 35050 | 10 | thunk | Far thunk → FUN_1b5e_0000 (Update soft mouse cursor hotspot/shape from 17×17 mask (1c36 + 1b8f blit)) | inferred |  |
| `FUN_291f_0472` | 35060 | 10 | thunk | Far thunk → FUN_4b58_0d94 (Modal menu loop: mouse/kbd navigate titles+items, select cmd into bar root) | inferred |  |
| `FUN_291f_047e` | 35070 | 10 | thunk | Far thunk → FUN_4b58_13ac (Hit-test mouse on menu titles; open matching pulldown via 0d94) | inferred |  |
| `FUN_291f_048a` | 35080 | 10 | thunk | Far thunk → FUN_4b58_14de (Find enabled item matching hotkey across menus; return command id) | inferred |  |
| `FUN_291f_0496` | 35090 | 10 | thunk | Far thunk → FUN_4b58_144a (Open pulldown by menu-title hotkey match, then run 0d94) | inferred |  |
| `FUN_291f_04a2` | 35100 | 10 | thunk | Far thunk → FUN_1ae7_0032 (drain BIOS keyboard buffer) | inferred |  |
| `FUN_291f_04ba` | 35110 | 10 | thunk | Far thunk → FUN_479b_0972 (Unit goto/move order tick; may enter colony) | inferred |  |
| `FUN_291f_04d4` | 35120 | 10 | thunk | Far thunk → FUN_2f2b_2b66 (Draw vertical multifunction tab buttons) | inferred |  |
| `FUN_291f_04e0` | 35130 | 10 | thunk | Far thunk → FUN_2f2b_171c (Draw settlement building strip (15 slots)) | inferred |  |
| `FUN_291f_04ec` | 35140 | 10 | thunk | Far thunk → FUN_2f2b_284c (Draw right multifunction pane by mode DS:0x337) | inferred |  |
| `FUN_291f_0504` | 35150 | 10 | thunk | Far thunk → FUN_2f2b_0722 (Draw one warehouse cargo-type amount label) | inferred |  |
| `FUN_291f_0510` | 35160 | 10 | thunk | Far thunk → FUN_2f2b_05b0 (Tiny far stub into colony chrome) | inferred |  |
| `FUN_291f_051c` | 35170 | 10 | thunk | Far thunk → FUN_2f2b_2f26 (Arm native-trade session pointers (0xbb8/9cce/9cd0)) | inferred |  |
| `FUN_291f_0534` | 35180 | 10 | thunk | Far thunk → FUN_2f2b_0ba8 (Draw colony area-view (5x5 map panel); trust ASM over corrupt C tail) | inferred |  |
| `FUN_291f_0540` | 35190 | 10 | thunk | Far thunk → FUN_2f2b_4284 (Select colonist; enter assign/profession mode (8d54=6)) | inferred |  |
| `FUN_291f_054c` | 35200 | 10 | thunk | Far thunk → FUN_2f2b_2f3e (Assign colonist to workplace/job (XREF-clear; decomp messy)) | inferred |  |
| `FUN_291f_0558` | 35210 | 10 | thunk | Far thunk → FUN_2f2b_1cce (Draw Production multifunction cargo/shortfall strip) | inferred |  |
| `FUN_291f_0564` | 35220 | 10 | thunk | Far thunk → FUN_2f2b_11b2 (Draw outside/fence unit strip) | inferred |  |
| `FUN_291f_057c` | 35230 | 10 | thunk | Far thunk → FUN_2f2b_2054 (Measure text extent for colony UI button/label) | inferred |  |
| `FUN_291f_0594` | 35240 | 10 | thunk | Far thunk → FUN_2f2b_4da6 (unload cargo hold→warehouse) | inferred |  |
| `FUN_291f_05a0` | 35250 | 10 | thunk | Far thunk → FUN_2f2b_42be (Enter colony drag/select mode (8d54=7)) | inferred |  |
| `FUN_291f_05ac` | 35260 | 10 | thunk | Far thunk → FUN_2f2b_289e (Warehouse cargo-strip X pos / selection marker) | inferred |  |
| `FUN_291f_05b8` | 35270 | 10 | thunk | Far thunk → FUN_2f2b_2484 (layout rect for transport hold slot N) | inferred |  |
| `FUN_291f_05c4` | 35280 | 10 | thunk | Far thunk → FUN_2f2b_17d0 (draw people band; colonists + SoL/Tory meters) | inferred |  |
| `FUN_291f_05d0` | 35290 | 10 | thunk | Far thunk → FUN_2f2b_6372 (colony keyboard dispatcher N/M/B/L/U/…) | inferred |  |
| `FUN_291f_05dc` | 35300 | 10 | thunk | Far thunk → FUN_2f2b_2c3c (draw multifunction mode-selector chrome) | inferred |  |
| `FUN_291f_05e8` | 35310 | 10 | thunk | Far thunk → FUN_2f2b_0434 (init settlement building-slot layout maps) | inferred |  |
| `FUN_291f_05f4` | 35320 | 10 | thunk | Far thunk → FUN_2f2b_42f2 (clear drag/select chrome @0x344) | inferred |  |
| `FUN_291f_0600` | 35330 | 10 | thunk | Far thunk → FUN_2f2b_3fa6 (area-tile click: select or assign colonist) | inferred |  |
| `FUN_291f_060c` | 35340 | 10 | thunk | Far thunk → FUN_2f2b_24b2 (draw transport / ship-hold pane) | inferred |  |
| `FUN_291f_0618` | 35350 | 10 | thunk | Far thunk → FUN_2f2b_011e (colony build/order gate; prereq/error codes) | inferred |  |
| `FUN_291f_0630` | 35360 | 10 | thunk | Far thunk → FUN_2f2b_0842 (draw area-view tile tooltip terrain/feature) | inferred |  |
| `FUN_291f_0648` | 35370 | 10 | thunk | Far thunk → FUN_2f2b_2c92 (full colony-screen redraw; all panel thunks) | inferred |  |
| `FUN_291f_0654` | 35380 | 10 | thunk | Far thunk → FUN_2f2b_28d6 (draw warehouse cargo strip; 16 types + Exit) | inferred |  |
| `FUN_291f_0660` | 35390 | 10 | thunk | Far thunk → FUN_2f2b_208c (draw framed invertible button/label) | inferred |  |
| `FUN_291f_066c` | 35400 | 10 | thunk | Far thunk → FUN_2f2b_14d4 (draw one settlement building; icon + badge) | inferred |  |
| `FUN_291f_0678` | 35410 | 10 | thunk | Far thunk → FUN_2f2b_5e44 (construction BUY remaining project; gold+tools) | inferred |  |
| `FUN_291f_0684` | 35420 | 10 | thunk | Far thunk → FUN_2f2b_2d0e (post-drag chrome refresh) | inferred |  |
| `FUN_291f_0690` | 35430 | 10 | thunk | Far thunk → FUN_2f2b_5a68 (construction CHANGE menu row; name/cost) | inferred |  |
| `FUN_291f_069c` | 35440 | 10 | thunk | Far thunk → FUN_2f2b_2d1c (toggle numbers DS:0x334; refresh active pane) | inferred |  |
| `FUN_291f_06b4` | 35450 | 10 | thunk | Far thunk → FUN_2f2b_5746 (docked-unit orders popup; sentry/fortify/…) | inferred |  |
| `FUN_291f_06c0` | 35460 | 10 | thunk | Far thunk → FUN_2f2b_13c2 (resolve building-type badge amount + sprite outs) | inferred |  |
| `FUN_291f_06cc` | 35470 | 10 | thunk | Far thunk → FUN_2f2b_40a0 (colony click region hit-test; panel codes) | inferred |  |
| `FUN_291f_06d8` | 35480 | 10 | thunk | Far thunk → FUN_2f2b_1e46 (draw Units multifunction pane; docked grid) | inferred |  |
| `FUN_291f_06e4` | 35490 | 10 | thunk | Far thunk → FUN_2f2b_12cc (draw colonists assigned to one workplace) | inferred |  |
| `FUN_291f_06f0` | 35500 | 10 | thunk | Far thunk → FUN_2f2b_0332 (reorder colonist slot tables after sort) | inferred |  |
| `FUN_291f_0708` | 35510 | 10 | thunk | Far thunk → FUN_2f2b_21da (draw Construction BUY / CHANGE buttons) | inferred |  |
| `FUN_291f_0714` | 35520 | 10 | thunk | Far thunk → FUN_2f2b_548e (people-band click; select, drag, or confirm) | inferred |  |
| `FUN_291f_0720` | 35530 | 10 | thunk | Far thunk → FUN_2f2b_4fec (transfer cargo between unit holds) | inferred |  |
| `FUN_291f_072c` | 35540 | 10 | thunk | Far thunk → FUN_2f2b_5bd2 (construction CHANGE project picker popup) | inferred |  |
| `FUN_291f_0738` | 35550 | 10 | thunk | Far thunk → FUN_2f2b_2d90 (force numbers-on + full pane refresh) | inferred |  |
| `FUN_291f_0744` | 35560 | 10 | thunk | Far thunk → FUN_2f2b_628a (colony mouse/click dispatcher by panel region) | inferred |  |
| `FUN_291f_0750` | 35570 | 10 | thunk | Far thunk → FUN_2f2b_348c (field-jobs / Leave-as profession popup) | inferred |  |
| `FUN_291f_075c` | 35580 | 10 | thunk | Far thunk → FUN_2f2b_2262 (refresh Construction CHANGE button highlight) | inferred |  |
| `FUN_291f_0768` | 35590 | 10 | thunk | Far thunk → FUN_2f2b_4424 (warehouse capacity / cargo-limits popup) | inferred |  |
| `FUN_291f_0774` | 35600 | 10 | thunk | Far thunk → FUN_2f2b_05b6 (colony wood-panel blit/setup) | inferred |  |
| `FUN_291f_078c` | 35610 | 10 | thunk | Far thunk → FUN_2f2b_2e2a (alternate numbers-on full refresh path) | inferred |  |
| `FUN_291f_0798` | 35620 | 10 | thunk | Far thunk → FUN_2f2b_22b6 (draw Construction hammer progress bars) | inferred |  |
| `FUN_291f_07b0` | 35630 | 10 | thunk | Far thunk → FUN_2f2b_2e92 (modal dialog frame helper) | inferred |  |
| `FUN_291f_07bc` | 35640 | 10 | thunk | Far thunk → FUN_2f2b_05ee (draw building/project name label at slot) | inferred |  |
| `FUN_291f_07c8` | 35650 | 10 | thunk | Far thunk → FUN_2f2b_51ec (people-band confirm / Leave-as entry) | inferred |  |
| `FUN_291f_07d4` | 35660 | 10 | thunk | Far thunk → FUN_2f2b_2eb2 (print dialog string-table line) | inferred |  |
| `FUN_291f_07ec` | 35670 | 10 | thunk | Far thunk → FUN_2f2b_0a3e (blit colony chrome rect via 281f_0444) | inferred |  |
| `FUN_291f_07f8` | 35680 | 10 | thunk | Far thunk → FUN_2f2b_4b62 (load cargo warehouse→hold; amount dialog) | inferred |  |
| `FUN_291f_0804` | 35690 | 10 | thunk | Far thunk → FUN_2f2b_0a74 (draw area-view surrounding map unit icons) | inferred |  |
| `FUN_291f_081c` | 35700 | 10 | thunk | Far thunk → FUN_2f2b_2ec6 (open 3-button message dialog wrapper) | inferred |  |
| `FUN_291f_0828` | 35710 | 10 | thunk | Far thunk → FUN_2f2b_2b2c (unit-chrome sprite rect helper) | inferred |  |
| `FUN_291f_0834` | 35720 | 10 | thunk | Far thunk → FUN_2f2b_16f2 (draw settlement empty/placeholder sprite if table flag) | inferred |  |
| `FUN_291f_0840` | 35730 | 10 | thunk | Far thunk → FUN_2f2b_0fce (draw colony top-bar name + date/nation string) | inferred |  |
| `FUN_291f_0858` | 35740 | 10 | thunk | Far thunk → FUN_2f2b_55da (transport hold-slot click / drag handler) | inferred |  |
| `FUN_291f_0864` | 35750 | 10 | thunk | Far thunk → FUN_2f2b_2eea (cargo-type confirm dialog wrapper) | inferred |  |
| `FUN_291f_0870` | 35760 | 10 | thunk | Far thunk → FUN_1d05_0000 (insertion-sort parallel byte keys + byte payload arrays) | inferred |  |
| `FUN_291f_087a` | 35770 | 10 | thunk | Far thunk → FUN_7939_000c (resource open helper) | inferred |  |
| `FUN_291f_0888` | 35780 | 10 | thunk | Far thunk → FUN_6b22_00ea (tribe overlay for map viewport) | inferred |  |
| `FUN_291f_0896` | 35790 | 10 | thunk | Far thunk → FUN_6b22_0248 (colony overlay for map viewport) | inferred |  |
| `FUN_291f_08a4` | 35800 | 10 | thunk | Far thunk → FUN_6ba1_10ae (temp DS:0x18a radius mode; refresh via 1028; clear) | inferred |  |
| `FUN_291f_08b2` | 35810 | 10 | thunk | Far thunk → FUN_1bc3_0006 (vertical byte-fill in buffer) | inferred |  |
| `FUN_291f_08bc` | 35820 | 10 | thunk | Far thunk → FUN_1bbc_000c (horizontal byte-fill in buffer) | inferred |  |
| `FUN_291f_08c6` | 35830 | 10 | thunk | Far thunk → FUN_6f74_0c32 (append body text line to flow list) | inferred |  |
| `FUN_291f_08d2` | 35840 | 10 | thunk | Far thunk → FUN_6f74_0c22 (set dialog body wrap-width) | inferred |  |
| `FUN_291f_08de` | 35850 | 10 | thunk | Far thunk → FUN_6cb2_1820 (Colonizopedia cargo/goods article) | inferred |  |
| `FUN_291f_08ec` | 35860 | 10 | thunk | Far thunk → FUN_6f74_09e2 (set current selected option ptr) | inferred |  |
| `FUN_291f_08f8` | 35870 | 10 | thunk | Far thunk → FUN_1b78_0000 (update soft mouse cursor hotspot/shape from 16×16 sprite) | inferred |  |
| `FUN_291f_0902` | 35880 | 10 | thunk | Far thunk → FUN_6cb2_1ba8 (build Colonizopedia colony-building article) | inferred |  |
| `FUN_291f_0910` | 35890 | 10 | thunk | Far thunk → FUN_6f74_309c (expand %STRING/%NUMBER/… placeholders) | inferred |  |
| `FUN_291f_091c` | 35900 | 10 | thunk | Far thunk → FUN_7314_0106 (next line from open string resource) | inferred |  |
| `FUN_291f_0928` | 35910 | 10 | thunk | Far thunk → FUN_7314_001a (open string resource; find @-entry) | inferred |  |
| `FUN_291f_0934` | 35920 | 10 | thunk | Far thunk → FUN_6cb2_05ce (Colonizopedia colonist-skill article) | inferred |  |
| `FUN_291f_0942` | 35930 | 10 | thunk | Far thunk → FUN_6cb2_07e6 (Colonizopedia unit-type article) | inferred |  |
| `FUN_291f_0950` | 35940 | 10 | thunk | Far thunk → FUN_364b_0688 (colony EOT production/SoL tick) | inferred |  |
| `FUN_291f_095e` | 35950 | 10 | thunk | Far thunk → FUN_6b7e_01f6 (save camera center/zoom for nation slot) | inferred |  |
| `FUN_291f_096c` | 35960 | 10 | thunk | Far thunk → FUN_6b7e_0218 (restore camera center/zoom for nation slot) | inferred |  |
| `FUN_291f_097a` | 35970 | 10 | thunk | Far thunk → FUN_364b_0114 (complete construction; apply upgrades/flags; reset hammers) | inferred |  |
| `FUN_291f_0988` | 35980 | 10 | thunk | Far thunk → FUN_364b_033a (area pass: set map feature 4 on ocean/hills tiles worked by jobs 6/7) | inferred |  |
| `FUN_291f_0996` | 35990 | 10 | thunk | Far thunk → FUN_364b_1b76 (gate: colony warehouse/trade slot usable for nation) | inferred |  |
| `FUN_291f_09a4` | 36000 | 10 | thunk | Far thunk → FUN_364b_1b1a (bind colony; place unit into colonist slot) | inferred |  |
| `FUN_291f_09b2` | 36010 | 10 | thunk | Far thunk → FUN_364b_1ba8 (found colony: bump 539e; init colony record via DS:0x8542) | inferred |  |
| `FUN_291f_09c0` | 36020 | 10 | thunk | Far thunk → FUN_364b_0636 (customs-house auto-sell gate for cargo type) | inferred |  |
| `FUN_291f_09ce` | 36030 | 10 | thunk | Far thunk → FUN_364b_03f6 (coastal fort fire: spawn attacks vs enemy ships on adjacent ocean) | known | turn/coastal_fort_fire.md |
| `FUN_291f_09dc` | 36040 | 10 | thunk | Far thunk → FUN_364b_0000 (colony message/confirm dialog; may set mode 337) | inferred |  |
| `FUN_291f_09ea` | 36050 | 10 | thunk | Far thunk → FUN_38fd_0040 (Europe ask: euro_price−1) | inferred |  |
| `FUN_291f_09f8` | 36060 | 10 | thunk | Far thunk → FUN_4345_0a22 (accrue liberty bells; FF election) | inferred | turn/nation_ticks_bells_ff.md |
| `FUN_291f_0a06` | 36070 | 10 | thunk | Far thunk → FUN_478c_00d0 (undo last unit spawn if colonist 0x17) | inferred |  |
| `FUN_291f_0a14` | 36080 | 10 | thunk | Far thunk → FUN_5fef_1b0e (main combat engagement for move-into) | inferred | ai/move_spent.c §3 |
| `FUN_291f_0a20` | 36090 | 10 | thunk | Far thunk → FUN_478c_002c (spawn colonist unit type 0x17) | inferred |  |
| `FUN_291f_0a2e` | 36100 | 10 | thunk | Far thunk → FUN_38fd_1dfa (sell volume: ledgers + tax gold) | inferred |  |
| `FUN_291f_0a3c` | 36110 | 10 | thunk | Far thunk → FUN_647e_060e (remove colonist slot; compact) | inferred |  |
| `FUN_291f_0a4a` | 36120 | 10 | thunk | Far thunk → FUN_647e_001a (bind colonist-slot pointer (slot×10→DS:0x9e18)) | inferred |  |
| `FUN_291f_0a58` | 36130 | 10 | thunk | Far thunk → FUN_3844_0004 (EOT treasure tick) | inferred |  |
| `FUN_291f_0a66` | 36140 | 10 | thunk | Far thunk → FUN_43f7_2424 (nation SoL refresh + threshold chrome) | inferred |  |
| `FUN_291f_0a74` | 36150 | 10 | thunk | Far thunk → FUN_4962_0018 (census units/colonies/cargo) | inferred | turn/census_tally.md |
| `FUN_291f_0a82` | 36160 | 10 | thunk | Far thunk → FUN_48d3_06ba (Europe-exit landfall; tax treasures) | inferred | turn/europe_exit_landfall.md |
| `FUN_291f_0a90` | 36170 | 10 | thunk | Far thunk → FUN_38fd_5e52 (Europe nation EOT market/tax/pool) | inferred | turn/europe_nation_eot.md |
| `FUN_291f_0a9e` | 36180 | 10 | thunk | Far thunk → FUN_4962_0606 (tally nation profession counts) | inferred | turn/census_tally.md |
| `FUN_291f_0aac` | 36190 | 10 | thunk | Far thunk → FUN_78d8_00c4 (reload resource far-ptrs from stream) | inferred |  |
| `FUN_291f_0aba` | 36200 | 10 | thunk | Far thunk → FUN_75c2_20e2 (endgame/victory announce dialog) | inferred |  |
| `FUN_291f_0ac8` | 36210 | 10 | thunk | Far thunk → FUN_6f74_0404 (format nation name into dialog subst) | inferred |  |
| `FUN_291f_0ad4` | 36220 | 10 | thunk | Far thunk → FUN_6f74_378a (set side-art style=8 then flush dialog) | inferred |  |
| `FUN_291f_0ae0` | 36230 | 10 | thunk | Far thunk → FUN_38fd_3dc8 (apply tax delta; may boycott) | inferred |  |
| `FUN_291f_0aee` | 36240 | 10 | thunk | Far thunk → FUN_48d3_0002 (landfall/goto duration roll) | inferred | turn/europe_finish_bridge.md |
| `FUN_291f_0afc` | 36250 | 10 | thunk | Far thunk → FUN_38fd_46d4 (roll next dock immigrant profession) | inferred |  |
| `FUN_291f_0b0a` | 36260 | 10 | thunk | Far thunk → FUN_38fd_1cf4 (exit Europe drag/sound mode) | inferred |  |
| `FUN_291f_0b18` | 36270 | 10 | thunk | Far thunk → FUN_38fd_199e (force-refresh Europe panels) | inferred |  |
| `FUN_291f_0b26` | 36280 | 10 | thunk | Far thunk → FUN_38fd_0718 (spawn purchased unit into Europe harbor) | inferred |  |
| `FUN_291f_0b34` | 36290 | 10 | thunk | Far thunk → FUN_38fd_584a (recruit-passage / immigration pressure score) | inferred |  |
| `FUN_291f_0b42` | 36300 | 10 | thunk | Far thunk → FUN_38fd_1fa2 (buy cargo dialog / execute buy flow) | inferred |  |
| `FUN_291f_0b50` | 36310 | 10 | thunk | Far thunk → FUN_38fd_19d8 (Europe text blit wrapper) | inferred |  |
| `FUN_291f_0b5e` | 36320 | 10 | thunk | Far thunk → FUN_38fd_0f5e (cargo-slot rect constants) | inferred |  |
| `FUN_291f_0b6c` | 36330 | 10 | thunk | Far thunk → FUN_38fd_6024 (new-game init Europe market state, all 4 nations) | inferred | docs/savegame.md |
| `FUN_291f_0b7a` | 36340 | 10 | thunk | Far thunk → FUN_38fd_5be8 (king audience tax event: favor-score picks cut or raise, applies same-call) | inferred |  |
| `FUN_291f_0b88` | 36350 | 10 | thunk | Far thunk → FUN_38fd_2a92 (sell/scrap harbor ship dialog) | inferred |  |
| `FUN_291f_0b96` | 36360 | 10 | thunk | Far thunk → FUN_38fd_19f8 (Europe string-table text blit by index) | inferred |  |
| `FUN_291f_0bb2` | 36370 | 10 | thunk | Far thunk → FUN_38fd_4b50 (purchase ship/artillery dialog) | inferred |  |
| `FUN_291f_0bc0` | 36380 | 10 | thunk | Far thunk → FUN_38fd_1456 (refresh Bound + Expected ship panels) | inferred |  |
| `FUN_291f_0bdc` | 36390 | 10 | thunk | Far thunk → FUN_38fd_1a0c (short Europe dialog open/close wrapper) | inferred |  |
| `FUN_291f_0bea` | 36400 | 10 | thunk | Far thunk → FUN_38fd_07c6 (recount harbor ships; clamp selection) | inferred |  |
| `FUN_291f_0c06` | 36410 | 10 | thunk | Far thunk → FUN_38fd_2dfe (pay to lift cargo boycott) | inferred |  |
| `FUN_291f_0c14` | 36420 | 10 | thunk | Far thunk → FUN_38fd_1d80 (buy volume: update tons/gold ledgers) | inferred |  |
| `FUN_291f_0c22` | 36430 | 10 | thunk | Far thunk → FUN_38fd_1a30 (buy/sell confirm dialog text: cargo × price) | inferred |  |
| `FUN_291f_0c30` | 36440 | 10 | thunk | Far thunk → FUN_38fd_146c (dock immigrant layout / scaling helper) | inferred |  |
| `FUN_291f_0c3e` | 36450 | 10 | thunk | Far thunk → FUN_38fd_0016 (Europe effective price = base+adj) | inferred |  |
| `FUN_291f_0c5a` | 36460 | 10 | thunk | Far thunk → FUN_38fd_1aba (hit-test Europe regions to panel/mode code) | inferred |  |
| `FUN_291f_0c68` | 36470 | 10 | thunk | Far thunk → FUN_38fd_0d48 (market cargo cell layout / scaling helper) | inferred |  |
| `FUN_291f_0c76` | 36480 | 10 | thunk | Far thunk → FUN_38fd_081c (init harbor unit count + Europe selection state) | inferred |  |
| `FUN_291f_0c84` | 36490 | 10 | thunk | Far thunk → FUN_38fd_5930 (Europe EOT FF cargo gift / grant) | inferred |  |
| `FUN_291f_0ca0` | 36500 | 10 | thunk | Far thunk → FUN_38fd_4f6e (Europe keyboard/hotkey input dispatcher) | inferred |  |
| `FUN_291f_0cae` | 36510 | 10 | thunk | Far thunk → FUN_38fd_2edc (loading-panel ship click / selection handler) | inferred |  |
| `FUN_291f_0cbc` | 36520 | 10 | thunk | Far thunk → FUN_38fd_0058 (market dynamics: adjust euro_price[] / pressure from colony ledgers) | inferred |  |
| `FUN_291f_0cca` | 36530 | 10 | thunk | Far thunk → FUN_38fd_41ce (Train expert dialog; job list + gold) | inferred |  |
| `FUN_291f_0cd8` | 36540 | 10 | thunk | Far thunk → FUN_38fd_05e8 (test cargo boycott bit) | inferred |  |
| `FUN_291f_0ce6` | 36550 | 10 | thunk | Far thunk → FUN_38fd_0836 (Europe window frame setup) | inferred |  |
| `FUN_291f_0d02` | 36560 | 10 | thunk | Far thunk → FUN_38fd_23c4 (sell cargo dialog / execute sell+tax) | inferred |  |
| `FUN_291f_0d10` | 36570 | 10 | thunk | Far thunk → FUN_38fd_086c (Load Europe screen art / PIK bring-up) | inferred |  |
| `FUN_291f_0d1e` | 36580 | 10 | thunk | Far thunk → FUN_38fd_2bfe (Harbor ship context menu; sail/sell/unload) | inferred |  |
| `FUN_291f_0d2c` | 36590 | 10 | thunk | Far thunk → FUN_38fd_4884 (Recruit dialog; 3 pool slots + passage) | inferred |  |
| `FUN_291f_0d3a` | 36600 | 10 | thunk | Far thunk → FUN_38fd_08a4 (Draw market cargo name/price caption) | inferred |  |
| `FUN_291f_0d56` | 36610 | 10 | thunk | Far thunk → FUN_38fd_14e2 (Blit one dock immigrant unit) | inferred |  |
| `FUN_291f_0d72` | 36620 | 10 | thunk | Far thunk → FUN_38fd_1b9e (Enter Europe UI mode 10) | inferred |  |
| `FUN_291f_0d80` | 36630 | 10 | thunk | Far thunk → FUN_38fd_0666 (resolve Nth harbor unit index) | inferred |  |
| `FUN_291f_0d8e` | 36640 | 10 | thunk | Far thunk → FUN_38fd_1ebc (apply buy: debit gold, add harbor tons) | inferred |  |
| `FUN_291f_0d9c` | 36650 | 10 | thunk | Far thunk → FUN_38fd_127c (Draw Bound For New World ship panel) | inferred |  |
| `FUN_291f_0daa` | 36660 | 10 | thunk | Far thunk → FUN_38fd_3694 (Dock immigrant info / embark bark dialog) | inferred |  |
| `FUN_291f_0db8` | 36670 | 10 | thunk | Far thunk → FUN_38fd_1bd2 (Drag-start bark dialog) | inferred |  |
| `FUN_291f_0dc6` | 36680 | 10 | thunk | Far thunk → FUN_38fd_1f0c (apply sell: pull hold tons toward quote) | inferred |  |
| `FUN_291f_0dd4` | 36690 | 10 | thunk | Far thunk → FUN_38fd_18fc (Full Europe-screen redraw) | inferred |  |
| `FUN_291f_0de2` | 36700 | 10 | thunk | Far thunk → FUN_38fd_15aa (Draw docks immigrant panel) | inferred |  |
| `FUN_291f_0df0` | 36710 | 10 | thunk | Far thunk → FUN_38fd_0e16 (Blit one market cargo cell + highlight) | inferred |  |
| `FUN_291f_0dfe` | 36720 | 10 | thunk | Far thunk → FUN_38fd_1956 (Europe overlay teardown) | inferred |  |
| `FUN_291f_0e0c` | 36730 | 10 | thunk | Far thunk → FUN_38fd_06c4 (Resolve Nth harbor ship index) | inferred |  |
| `FUN_291f_0e28` | 36740 | 10 | thunk | Far thunk → FUN_38fd_3746 (Dock immigrant action mega-dialog; board/orders) | inferred |  |
| `FUN_291f_0e36` | 36750 | 10 | thunk | Far thunk → FUN_38fd_285c (Transfer cargo between ship holds) | inferred |  |
| `FUN_291f_0e44` | 36760 | 10 | thunk | Far thunk → FUN_38fd_1960 (Toggle/refresh highlighted panel by mode) | inferred |  |
| `FUN_291f_0e52` | 36770 | 10 | thunk | Far thunk → FUN_38fd_1c64 (enter Europe drag mode 8) | inferred |  |
| `FUN_291f_0e60` | 36780 | 10 | thunk | Far thunk → FUN_38fd_4e8e (Europe mouse/drag input dispatcher) | inferred |  |
| `FUN_291f_0e6e` | 36790 | 10 | thunk | Far thunk → FUN_38fd_1f66 (Clear top market strip; init row Y) | inferred |  |
| `FUN_291f_0e7c` | 36800 | 10 | thunk | Far thunk → FUN_38fd_1382 (Draw Expected Soon ship panel) | inferred |  |
| `FUN_291f_0e8a` | 36810 | 10 | thunk | Far thunk → FUN_38fd_1f7e (Advance market row Y cursor) | inferred |  |
| `FUN_291f_0e98` | 36820 | 10 | thunk | Far thunk → FUN_38fd_30aa (Hold-slot click: load/unload cargo) | inferred |  |
| `FUN_291f_0eb4` | 36830 | 10 | thunk | Far thunk → FUN_38fd_1f8e (Blit market strip panel) | inferred |  |
| `FUN_291f_0ec2` | 36840 | 10 | thunk | Far thunk → FUN_48d3_0346 (retarget stack after landfall/colony-goto) | inferred |  |
| `FUN_291f_0ed0` | 36850 | 10 | thunk | Far thunk → FUN_1cf8_000a (insertion-sort parallel word+byte arrays) | inferred |  |
| `FUN_291f_0eda` | 36860 | 10 | thunk | Far thunk → FUN_3f3f_0006 (CRC/LFSR step; shift+XOR poly on LSB) | inferred |  |
| `FUN_291f_0ee8` | 36870 | 10 | thunk | Far thunk → FUN_3f41_008a (Report footer/title strip blit) | inferred |  |
| `FUN_291f_0ef6` | 36880 | 10 | thunk | Far thunk → FUN_3f41_1438 (Economic Adviser header chrome; REPORT5) | inferred |  |
| `FUN_291f_0f04` | 36890 | 10 | thunk | Far thunk → FUN_3f41_1b94 (Colony Adviser header chrome; REPORT6) | inferred |  |
| `FUN_291f_0f12` | 36900 | 10 | thunk | Far thunk → FUN_3f41_20b4 (Unit disposition report header; column labels) | inferred |  |
| `FUN_291f_0f20` | 36910 | 10 | thunk | Far thunk → FUN_3f41_1bec (Colony Adviser F6; pop/build/garrison rows) | inferred |  |
| `FUN_291f_0f2e` | 36920 | 10 | thunk | Far thunk → FUN_3f41_1550 (Economic Adviser colony cargo-stock rows) | inferred |  |
| `FUN_291f_0f3c` | 36930 | 10 | thunk | Far thunk → FUN_3f41_0d3e (Labor Adviser detail; profession colony placements) | inferred |  |
| `FUN_291f_0f4a` | 36940 | 10 | thunk | Far thunk → FUN_3f41_0000 (Report plate bring-up; art+palette into 2da8) | inferred |  |
| `FUN_291f_0f58` | 36950 | 10 | thunk | Far thunk → FUN_3f41_1e80 (Naval/military-in-colony report header chrome) | inferred |  |
| `FUN_291f_0f66` | 36960 | 10 | thunk | Far thunk → FUN_4345_0982 (Compute next liberty-bell threshold) | inferred |  |
| `FUN_291f_0f74` | 36970 | 10 | thunk | Far thunk → FUN_4345_024a (FF election / announcement UI screen) | inferred |  |
| `FUN_291f_0f82` | 36980 | 10 | thunk | Far thunk → FUN_49dd_02d0 (Resolve tile tip string; orders/colony/terrain) | inferred |  |
| `FUN_291f_0f8e` | 36990 | 10 | thunk | Far thunk → FUN_41f2_0f56 (high-score table load/insert/save + UI) | inferred |  |
| `FUN_291f_0f9c` | 37000 | 10 | thunk | Far thunk → FUN_41f2_0b70 (Score→difficulty gold rebate + treasure dialog) | inferred |  |
| `FUN_291f_0faa` | 37010 | 10 | thunk | Far thunk → FUN_41f2_000e (Present nation-score report title dialog) | inferred |  |
| `FUN_291f_0fb8` | 37020 | 10 | thunk | Far thunk → FUN_7314_0000 (close config/name file handle) | inferred |  |
| `FUN_291f_0fc4` | 37030 | 10 | thunk | Far thunk → FUN_7314_015e (Extract next comma field; advance cursor) | inferred |  |
| `FUN_291f_0fd0` | 37040 | 10 | thunk | Far thunk → FUN_78d8_0054 (advance resource stream: load via 78ef, bump cursor) | inferred |  |
| `FUN_291f_0fde` | 37050 | 10 | thunk | Far thunk → FUN_78d8_0000 (reset resource-stream cursor/remain from DS:0x23c6) | inferred |  |
| `FUN_291f_0fec` | 37060 | 10 | thunk | Far thunk → FUN_4345_0342 (Apply elected founding-father effects) | inferred |  |

### Segment `2a1f` (294 defs) — mixed — Map-gen dispatch / helpers (also Euro act thunks)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2a1f_0000` | 37070 | 10 | mapgen | Far thunk → FUN_4345_06d2 (FF congress debate / nominate UI) | inferred |  |
| `FUN_2a1f_000e` | 37080 | 10 | mapgen | Far thunk → FUN_4345_0126 (Build FF name string into buffer) | inferred |  |
| `FUN_2a1f_001c` | 37090 | 10 | mapgen | Far thunk → FUN_4345_015a (Pick strongest FF category slot for nation) | inferred |  |
| `FUN_2a1f_002a` | 37100 | 10 | mapgen | Far thunk → FUN_4345_01a6 (Enumerate/draw FF entries for nation) | inferred |  |
| `FUN_2a1f_0038` | 37110 | 10 | mapgen | Far thunk → FUN_4345_0000 (Set/clear nation bitflag in Europe-record) | inferred |  |
| `FUN_2a1f_0046` | 37120 | 10 | mapgen | Far thunk → FUN_4345_005a (Calendar era tier from year thresholds) | inferred |  |
| `FUN_2a1f_0054` | 37130 | 10 | mapgen | Far thunk → FUN_4345_0080 (Count unelected FF of category with weight) | inferred |  |
| `FUN_2a1f_0062` | 37140 | 10 | mapgen | Far thunk → FUN_6cb2_1f28 (Build Colonizopedia founding-father article) | inferred |  |
| `FUN_2a1f_0070` | 37150 | 10 | mapgen | Far thunk → FUN_43f7_0082 (REF/war unit-type id for class+nation) | inferred |  |
| `FUN_2a1f_007e` | 37160 | 10 | mapgen | Far thunk → FUN_43f7_1d42 (Tax→REF funding: grow expeditionary pools + notify) | inferred |  |
| `FUN_2a1f_008c` | 37170 | 10 | mapgen | Far thunk → FUN_43f7_0108 (Eliminate nation: move treasury/relations, scrub units) | inferred |  |
| `FUN_2a1f_009a` | 37180 | 10 | mapgen | Far thunk → FUN_43f7_160a (Independence rename cinematic: animate new nation name) | inferred |  |
| `FUN_2a1f_00a8` | 37190 | 10 | mapgen | Far thunk → FUN_43f7_0512 (Purge non-player units at (x,y); capture/surrender msgs) | inferred |  |
| `FUN_2a1f_00b6` | 37200 | 10 | mapgen | Far thunk → FUN_43f7_0188 (Sink nation ships not docked in a colony) | inferred |  |
| `FUN_2a1f_00c4` | 37210 | 10 | mapgen | Far thunk → FUN_43f7_1eca (Promote veterans to Continental Army/Cavalry when SoL>50%) | inferred |  |
| `FUN_2a1f_00d2` | 37220 | 10 | mapgen | Far thunk → FUN_43f7_05ea (Set DS:0x848[crown nation] @COUNTRY color to 0x0f) | inferred | src/core/turn.c |
| `FUN_2a1f_00e0` | 37230 | 10 | mapgen | Far thunk → FUN_43f7_05f4 (@COUNTRY to DS color table) | inferred | src/core/turn.c |
| `FUN_2a1f_00ee` | 37240 | 10 | mapgen | Far thunk → FUN_43f7_060a (Colony garrison/defense score for REF landing target pick) | inferred |  |
| `FUN_2a1f_00fc` | 37250 | 10 | mapgen | Far thunk → FUN_43f7_0982 (REF invasion wave: Man-O-War + pool units at target colony) | inferred |  |
| `FUN_2a1f_010a` | 37260 | 10 | mapgen | Far thunk → FUN_43f7_10f0 (Foreign-intervention landing near colony) | inferred |  |
| `FUN_2a1f_0118` | 37270 | 10 | mapgen | Far thunk → FUN_43f7_06a6 (Crown turn: spawn irregulars near player colonies when REF empty) | inferred |  |
| `FUN_2a1f_0126` | 37280 | 10 | mapgen | Far thunk → FUN_43f7_2022 (Independence-war nation turn: REF grow/land or intervene hire) | inferred |  |
| `FUN_2a1f_0134` | 37290 | 10 | mapgen | Far thunk → FUN_43f7_0004 (Pop-weighted nation SoL aggregate over owned colonies) | inferred |  |
| `FUN_2a1f_0142` | 37300 | 10 | mapgen | Far thunk → FUN_465b_0000 (move spent cost/ADD/post-ADD chrome) | inferred |  |
| `FUN_2a1f_0150` | 37310 | 10 | mapgen | Far thunk → FUN_465b_0c1e (step unit in dir8 via move_spent_add) | known | ai/indian_nation_turn.c; ai/move_spent.c |
| `FUN_2a1f_015e` | 37320 | 10 | mapgen | Far thunk → FUN_5f7a_0662 (dispatch colony native-trade session) | inferred |  |
| `FUN_2a1f_016c` | 37330 | 10 | mapgen | Far thunk → FUN_4d56_4528 (Indian settlement enter/raid contact) | inferred | ai/indian_settlement_4528.md; ai/move_spent.c §3 |
| `FUN_2a1f_0178` | 37340 | 10 | mapgen | Far thunk → FUN_65dd_0004 (lost-city/rumour RNG outcome resolve) | inferred |  |
| `FUN_2a1f_0186` | 37350 | 10 | mapgen | Far thunk → FUN_5fef_1908 (treasure capture: ransom/gold/remove) | inferred |  |
| `FUN_2a1f_0192` | 37360 | 10 | mapgen | Far thunk → FUN_5bfb_3180 (adj ship/unit combat loot around (x,y)) | inferred |  |
| `FUN_2a1f_01a0` | 37370 | 10 | ui | EMS thunk → FUN_4720_0006 (ship cargo free-space / embark probe) | inferred |  |
| `FUN_2a1f_01ae` | 37380 | 10 | mapgen | Far thunk → FUN_4720_015c (Naval-move validity check; set DS:0x9e4e reason) | inferred |  |
| `FUN_2a1f_01bc` | 37390 | 10 | mapgen | Far thunk → FUN_478c_0002 (Init unit-spawn scratch block at DS:0x5372) | inferred |  |
| `FUN_2a1f_01ca` | 37400 | 10 | mapgen | Far thunk → FUN_478c_007e (Spawn ship unit type 0 or 0x0d) | inferred |  |
| `FUN_2a1f_01d8` | 37410 | 10 | mapgen | Far thunk → FUN_479b_00ca (Gold-spend gate for pioneer follow-up) | inferred |  |
| `FUN_2a1f_01e6` | 37420 | 10 | mapgen | Far thunk → FUN_479b_0158 (Pioneer tools wear-tick; clear order when depleted) | inferred |  |
| `FUN_2a1f_01f4` | 37430 | 10 | mapgen | Far thunk → FUN_479b_0000 (Generate/assign next colony name from nation tables) | inferred |  |
| `FUN_2a1f_0202` | 37440 | 10 | mapgen | Far thunk → FUN_479b_0b84 (Unit-at-colony predicate) | inferred |  |
| `FUN_2a1f_0210` | 37450 | 10 | mapgen | Far thunk → FUN_6662_0f74 (unit goto next-step director) | inferred |  |
| `FUN_2a1f_021c` | 37460 | 10 | colony | EMS thunk → FUN_647e_057a (read packed building/cargo nibble) | inferred |  |
| `FUN_2a1f_022a` | 37470 | 10 | colony | EMS thunk → FUN_647e_0522 (read packed colonist-slot nibble) | inferred |  |
| `FUN_2a1f_0238` | 37480 | 10 | mapgen | Far thunk → FUN_48d3_064e (For each ship on tile, spiral-place near landfall goto) | inferred | turn/europe_finish_bridge.md |
| `FUN_2a1f_0246` | 37490 | 10 | mapgen | Far thunk → FUN_48d3_03d0 (Tick landfall delay; move/act unit when expired) | inferred | turn/europe_finish_bridge.md |
| `FUN_2a1f_0254` | 37500 | 10 | mapgen | Far thunk → FUN_48d3_0434 (Tile OK for HS landfall ship place?) | inferred | docs/ai_transcription.md |
| `FUN_2a1f_0262` | 37510 | 10 | mapgen | Far thunk → FUN_48d3_048e (Spiral-place ship on HS near landfall goto) | inferred | src/core/ai.c |
| `FUN_2a1f_0270` | 37520 | 10 | ai | indian_relation_tick thunk→4962_06b6 (recount tribes/units/goods) | known | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_2a1f_027e` | 37530 | 10 | mapgen | Far thunk → FUN_6662_0906 (Short-range goto cost via 00f2 when both axes <8) | inferred | src/core/units.c |
| `FUN_2a1f_028a` | 37540 | 10 | mapgen | Far thunk → FUN_49dd_01aa (Sort and blit unit cargo commodity icons into status panel) | inferred |  |
| `FUN_2a1f_0296` | 37550 | 10 | mapgen | Far thunk → FUN_49dd_0000 (Open unit/tile status panel window) | inferred |  |
| `FUN_2a1f_02a2` | 37560 | 10 | mapgen | Far thunk → FUN_49dd_0386 (Resolve unit profession/job name string) | inferred |  |
| `FUN_2a1f_02ae` | 37570 | 10 | mapgen | Far thunk → FUN_49dd_0086 (Close/dismiss status panel) | inferred |  |
| `FUN_2a1f_02ba` | 37580 | 10 | mapgen | Far thunk → FUN_49dd_009c (append freeform tip/text line into status panel) | inferred |  |
| `FUN_2a1f_02c6` | 37590 | 10 | mapgen | Far thunk → FUN_49dd_00f6 (append indexed tip string from table 0x2db0 into status panel) | inferred |  |
| `FUN_2a1f_02d2` | 37600 | 10 | mapgen | Far thunk → FUN_4b58_02f6 (alloc/init menu-bar root) | inferred |  |
| `FUN_2a1f_02de` | 37610 | 10 | mapgen | Far thunk → FUN_4b58_0016 (pack 6-word menu style/color record into buffer) | inferred |  |
| `FUN_2a1f_02ea` | 37620 | 10 | mapgen | Far thunk → FUN_4b58_0a64 (compute pulldown popup layout rects; clamp to 320×200) | inferred |  |
| `FUN_2a1f_02f6` | 37630 | 10 | mapgen | Far thunk → FUN_4b58_043e (find menu node by id in bar's linked menu list) | inferred |  |
| `FUN_2a1f_0302` | 37640 | 10 | mapgen | Far thunk → FUN_4b58_0484 (find menu-item node by command id across all menus) | inferred |  |
| `FUN_2a1f_030e` | 37650 | 10 | mapgen | Far thunk → FUN_4b58_00ae (measure menu label width after stripping '~' hotkey markup) | inferred |  |
| `FUN_2a1f_031a` | 37660 | 10 | mapgen | Far thunk → FUN_4b58_063a (append titled menu node to bar) | inferred |  |
| `FUN_2a1f_0326` | 37670 | 10 | mapgen | Far thunk → FUN_4b58_0b7a (draw open pulldown: outline/fill/item labels) | inferred |  |
| `FUN_2a1f_0332` | 37680 | 10 | mapgen | Far thunk → FUN_4b58_0104 (draw menu label text with ~hotkey color highlight) | inferred |  |
| `FUN_2a1f_033e` | 37690 | 10 | mapgen | Far thunk → FUN_4b58_07d6 (append menu-item under a menu; empty label → separator) | inferred |  |
| `FUN_2a1f_034a` | 37700 | 10 | mapgen | Far thunk → FUN_4b58_023e (resolve ~hotkey scan/char code from label) | inferred |  |
| `FUN_2a1f_0356` | 37710 | 10 | mapgen | Far thunk → FUN_7b08_009e (wrap existing far block as arena; no alloc) | inferred |  |
| `FUN_2a1f_0364` | 37720 | 10 | mapgen | Far thunk → FUN_7ab9_0000 (image/resource load attempt) | inferred |  |
| `FUN_2a1f_0372` | 37730 | 10 | mapgen | Far thunk → FUN_78ef_0002 (open RM* resource archive + stream) | inferred |  |
| `FUN_2a1f_0380` | 37740 | 10 | mapgen | Far thunk → FUN_1aea_000c (map keyboard/hotkey dispatch into colony/mapdraw/sound) | inferred |  |
| `FUN_2a1f_038a` | 37750 | 10 | mapgen | Far thunk → FUN_7ab9_00be (handle image-load status) | inferred |  |
| `FUN_2a1f_0398` | 37760 | 10 | ai | Clear euro missions on Indian tribes (alarm) thunk→4cc6_0000 | inferred | original_sources_annotated/ai/indian_nation_turn.c |
| `FUN_2a1f_0434` | 37770 | 10 | mapgen | Far thunk → FUN_4d56_2154 (meet economics / 0x9e* tables) | inferred | ai/indian_meet_scoring_2154.md |
| `FUN_2a1f_0440` | 37780 | 10 | mapgen | Far thunk → FUN_4d56_0038 (settlement-record CREATE); sole caller is FUN_6a09_0006 (tribe placement, 3 sites) | confirmed | docs/ai_transcription.md, original_sources_annotated/ai/settlement_record_8d4a.md |
| `FUN_2a1f_044c` | 37790 | 10 | mapgen | Far thunk → FUN_4d56_2820 (heavy Indian decision / raid-scale logic) | inferred | ai/indian_trade_2820.md |
| `FUN_2a1f_016c` | 37330 | 10 | mapgen | Far thunk → FUN_4d56_4528 (Indian settlement enter/raid contact) | inferred | ai/indian_settlement_4528.md; ai/move_spent.c §3 |
| `FUN_2a1f_0458` | 37800 | 10 | mapgen | Far thunk → FUN_5fef_0000 (pick best defender unit at tile by combat score walk) | inferred |  |
| `FUN_2a1f_0464` | 37810 | 10 | mapgen | Far thunk → FUN_521d_0656 (walk unit stack/chain) | inferred |  |
| `FUN_2a1f_0470` | 37820 | 10 | mapgen | Far thunk → FUN_521d_016a (upsert primary goal) | inferred | ai/euro_goals.c |
| `FUN_2a1f_047c` | 37830 | 10 | mapgen | Far thunk → FUN_521d_0906 (probe adjacent contact/claim) | inferred |  |
| `FUN_2a1f_0488` | 37840 | 10 | mapgen | Far thunk → FUN_521d_5b66 (Euro per-unit act; 6d8e unit loop) | inferred | ai/euro_dispatcher.c; ai/euro_unit_act.md |
| `FUN_2a1f_0494` | 37850 | 10 | mapgen | Far thunk → FUN_521d_03d0 (founding/expansion urgency) | inferred |  |
| `FUN_2a1f_04a0` | 37860 | 10 | mapgen | Far thunk → FUN_521d_0000 (clear primary goal slot) | inferred |  |
| `FUN_2a1f_04ac` | 37870 | 10 | mapgen | Far thunk → FUN_521d_06ae (best adjacent founding tile) | inferred | ai/euro_goals.c; ai/move_scoring.md |
| `FUN_2a1f_04b8` | 37880 | 10 | mapgen | Far thunk → FUN_521d_001c (invalidate nearby secondary goals) | inferred |  |
| `FUN_2a1f_04c4` | 37890 | 10 | mapgen | Far thunk → FUN_521d_0214 (upsert secondary goal) | inferred |  |
| `FUN_2a1f_04d0` | 37900 | 10 | mapgen | Far thunk → FUN_521d_0492 (colony-count balance flags) | inferred |  |
| `FUN_2a1f_04dc` | 37910 | 10 | mapgen | Far thunk → FUN_521d_5c38 (Europe hire gate stub) | inferred |  |
| `FUN_2a1f_04e8` | 37920 | 10 | mapgen | Far thunk → FUN_521d_0072 (shift primary goal table) | inferred |  |
| `FUN_2a1f_04f4` | 37930 | 10 | mapgen | Far thunk → FUN_521d_20e6 (move scoring) | inferred | ai/euro_unit_act.md; ai/move_scoring.md |
| `FUN_2a1f_0500` | 37940 | 10 | mapgen | Far thunk → FUN_521d_5c3c (Europe buy/hire unit) | inferred |  |
| `FUN_2a1f_050c` | 37950 | 10 | mapgen | Far thunk → FUN_521d_0a60 (euro unit/colony goals; 6d8e plan pass) | inferred | ai/euro_dispatcher.c |
| `FUN_2a1f_0518` | 37960 | 10 | mapgen | Far thunk → FUN_521d_00a8 (shift secondary goal table) | inferred |  |
| `FUN_2a1f_0524` | 37970 | 10 | mapgen | Far thunk → FUN_521d_02be (upsert AI work-queue entry) | inferred |  |
| `FUN_2a1f_0530` | 37980 | 10 | mapgen | Far thunk → FUN_521d_5cf6 (refresh colony context; 6d8e inventory) | inferred | ai/euro_dispatcher.c |
| `FUN_2a1f_053c` | 37990 | 10 | mapgen | Far thunk → FUN_521d_052c (unit desirability score) | inferred |  |
| `FUN_2a1f_0548` | 38000 | 10 | mapgen | Far thunk → FUN_521d_00de (shift work-queue table) | inferred |  |
| `FUN_2a1f_0554` | 38010 | 10 | mapgen | Far thunk → FUN_521d_5d04 (euro unit planning; 6d8e colony/plan pass) | inferred | ai/euro_dispatcher.c |
| `FUN_2a1f_0560` | 38020 | 10 | mapgen | Far thunk → FUN_521d_031c (clear AI work queue) | inferred |  |
| `FUN_2a1f_056c` | 38030 | 10 | mapgen | Far thunk → FUN_521d_0896 (filter profession by distance/wealth) | inferred |  |
| `FUN_2a1f_0578` | 38040 | 10 | mapgen | Far thunk → FUN_521d_0342 (promote secondary→primary goals; 6d8e plan pass) | inferred | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_2a1f_0584` | 38050 | 10 | mapgen | Far thunk → FUN_521d_0116 (max priority among primary goals) | inferred |  |
| `FUN_2a1f_0590` | 38060 | 10 | mapgen | Far thunk → FUN_521d_0600 (composite unit priority) | inferred |  |
| `FUN_2a1f_059c` | 38070 | 10 | mapgen | Far thunk → FUN_6662_0086 (sign(dx,dy) → 8-way direction index) | inferred | src/core/units.c |
| `FUN_2a1f_05a8` | 38080 | 10 | mapgen | Far thunk → FUN_5952_035e (colony production/buildings/stock tick) | inferred |  |
| `FUN_2a1f_05b4` | 38090 | 10 | mapgen | Far thunk → FUN_5952_0214 (gate/set construction project; clear build-busy on fail) | inferred |  |
| `FUN_2a1f_05c0` | 38100 | 10 | mapgen | Far thunk → FUN_5952_0280 (need building upgrade? cargo stock vs building tier) | inferred |  |
| `FUN_2a1f_05cc` | 38110 | 10 | mapgen | Far thunk → FUN_5952_02f4 (clear build-busy flag; map index → building id) | inferred |  |
| `FUN_2a1f_05d8` | 38120 | 10 | mapgen | Far thunk → FUN_5952_0000 (maybe spawn wagon-train link between nearby same-nation colonies) | inferred |  |
| `FUN_2a1f_05e4` | 38130 | 10 | mapgen | Far thunk → FUN_5952_0306 (set/clear colony specialty cargo from stock/market) | inferred |  |
| `FUN_2a1f_05f0` | 38140 | 10 | mapgen | Far thunk → FUN_6662_00f2 (goto BFS over terr_cost with path-cost overlay and Z/Esc confirm) | inferred |  |
| `FUN_2a1f_05fc` | 38150 | 10 | mapgen | Far thunk → FUN_5bfb_153e (large diplomacy/war-declaration body) | inferred |  |
| `FUN_2a1f_060a` | 38160 | 10 | mapgen | Far thunk → FUN_5bfb_12d0 (clear armed-unit goto/orders adjacent to a colony) | inferred |  |
| `FUN_2a1f_0618` | 38170 | 10 | mapgen | Far thunk → FUN_5bfb_102a (diplomacy multi-line dialog present) | inferred |  |
| `FUN_2a1f_0626` | 38180 | 10 | ai | EMS thunk → FUN_5bfb_312e (unit combat-power factor) | inferred |  |
| `FUN_2a1f_0634` | 38190 | 10 | mapgen | Far thunk → FUN_5bfb_0000 (cargo/treasury census outs for diplomacy) | inferred |  |
| `FUN_2a1f_0642` | 38200 | 10 | mapgen | Far thunk → FUN_5bfb_1092 (diplomacy short 1–2 option dialog present) | inferred |  |
| `FUN_2a1f_0650` | 38210 | 10 | mapgen | Far thunk → FUN_5bfb_0182 (set diplomacy bit0x40 + human peace/teach dialogs) | inferred |  |
| `FUN_2a1f_065e` | 38220 | 10 | mapgen | Far thunk → FUN_5bfb_13b0 (form or break alliance between two nations) | inferred |  |
| `FUN_2a1f_066c` | 38230 | 10 | mapgen | Far thunk → FUN_5bfb_022e (Indian unit contact/meet body) | inferred |  |
| `FUN_2a1f_067a` | 38240 | 10 | mapgen | Far thunk → FUN_5bfb_10ec (Euro A↔B war/ally eligibility by military balance) | inferred |  |
| `FUN_2a1f_0688` | 38250 | 10 | mapgen | Far thunk → FUN_6f74_37cc (set tertiary side-art DS:0x1f60 then flush/run) | inferred |  |
| `FUN_2a1f_0694` | 38260 | 10 | mapgen | Far thunk → FUN_5f7a_020e (native cargo trade: price, pick stock, confirm sell/barter) | inferred |  |
| `FUN_2a1f_06a2` | 38270 | 10 | mapgen | Far thunk → FUN_5f7a_000e (colony native-meet gold dialog; ±100 treasury) | inferred |  |
| `FUN_2a1f_06b0` | 38280 | 10 | combat | EMS thunk → FUN_5fef_016c (pick cargo slot to plunder) | inferred | ai/indian_raid_loot.md |
| `FUN_2a1f_06bc` | 38290 | 10 | mapgen | Far thunk → FUN_5fef_16ea (remap specialty id after combat demotion) | inferred |  |
| `FUN_2a1f_06c8` | 38300 | 10 | combat | EMS thunk → FUN_5fef_0f14 (Indian raid colony loot + tension) | inferred | ai/indian_raid_loot.md |
| `FUN_2a1f_06d4` | 38310 | 10 | combat | EMS thunk → FUN_5fef_172c (post-combat specialty/type change) | inferred |  |
| `FUN_2a1f_06e0` | 38320 | 10 | combat | EMS thunk → FUN_5fef_0352 (apply combat outcome) | inferred |  |
| `FUN_2a1f_06ec` | 38330 | 19 | mapgen | EMS page-in stub only (no further far call) | inferred |  |
| `FUN_2a1f_0704` | 38349 | 10 | ui | EMS thunk → FUN_636c_0000 (dual-column compare/report dialog) | inferred |  |
| `FUN_2a1f_0710` | 38359 | 10 | mapgen | Far thunk → FUN_6f74_2278 (dialog chrome fill + border bevels) | inferred |  |
| `FUN_2a1f_071c` | 38369 | 10 | mapgen | Far thunk → FUN_647e_0e80 (cargo/commodity type picker dialog) | inferred |  |
| `FUN_2a1f_072a` | 38379 | 10 | mapgen | Far thunk → FUN_647e_05aa (write packed building/cargo nibble) | inferred |  |
| `FUN_2a1f_0738` | 38389 | 10 | mapgen | Far thunk → FUN_647e_05ec (set colonist-slot unit-index; clear dual UI counters) | inferred |  |
| `FUN_2a1f_0746` | 38399 | 10 | mapgen | Far thunk → FUN_647e_0f2c (add/remove cargo item in colonist warehouse row) | inferred |  |
| `FUN_2a1f_0754` | 38409 | 10 | colony | EMS thunk → FUN_647e_0040 (resolve slot label ptr) | inferred |  |
| `FUN_2a1f_0762` | 38419 | 10 | mapgen | Far thunk → FUN_647e_0094 (gate: unit may join/enter colony) | inferred |  |
| `FUN_2a1f_0770` | 38429 | 10 | mapgen | Far thunk → FUN_647e_09da (draw colony report panel) | inferred |  |
| `FUN_2a1f_077e` | 38439 | 10 | mapgen | Far thunk → FUN_647e_1064 (colony-list mouse hit to row/action) | inferred |  |
| `FUN_2a1f_078c` | 38449 | 10 | mapgen | Far thunk → FUN_647e_04f0 (map building/warehouse index to packed-nibble offset) | inferred |  |
| `FUN_2a1f_079a` | 38459 | 10 | mapgen | Far thunk → FUN_647e_10d2 (colony-list mouse: rename colony or dismiss) | inferred |  |
| `FUN_2a1f_07a8` | 38469 | 10 | mapgen | Far thunk → FUN_647e_0dd4 (assign/reassign colonist into selected slot) | inferred |  |
| `FUN_2a1f_07b6` | 38479 | 10 | mapgen | Far thunk → FUN_647e_0548 (write packed nibble to colonist-slot byte+2) | inferred |  |
| `FUN_2a1f_07c4` | 38489 | 10 | mapgen | Far thunk → FUN_6f74_3084 (set dialog script resource triple) | inferred |  |
| `FUN_2a1f_07d0` | 38499 | 10 | mapgen | Far thunk → FUN_6662_0b4e (coarse sector BFS toward goto with path-cost overlay) | inferred |  |
| `FUN_2a1f_07dc` | 38509 | 10 | mapgen | EMS thunk → FUN_67bf_0000 (continent flood-fill IDs) | known | src/core/map_gen.c |
| `FUN_2a1f_07ea` | 38519 | 10 | mapgen | Far thunk → FUN_67f4_0088 (post-flood-fill coast/neighbor bitmasks + continent tallies) | inferred |  |
| `FUN_2a1f_07f8` | 38529 | 10 | mapgen | Far thunk → FUN_682a_000c (write per-tile fertility/bonus nibble across map) | inferred |  |
| `FUN_2a1f_0806` | 38539 | 10 | mapgen | EMS thunk → FUN_684c_0116 (continents land-blob wander) | known | src/core/map_gen.c |
| `FUN_2a1f_0814` | 38549 | 10 | mapgen | EMS thunk → FUN_684c_021c (short cardinal extra-mass wander) | known | src/core/map_gen.c |
| `FUN_2a1f_0822` | 38559 | 10 | mapgen | EMS thunk → FUN_684c_02a8 (one land blob / form dispatch) | known | src/core/map_gen.c |
| `FUN_2a1f_0830` | 38569 | 10 | mapgen | EMS thunk → FUN_684c_03e4 (mountain landlocked / four-diagonal check) | known | src/core/map_gen.c |
| `FUN_2a1f_083e` | 38579 | 10 | mapgen | Dispatches into map-gen pipeline | known | src/core/map_gen.c; docs/assets.md |
| `FUN_2a1f_084c` | 38589 | 10 | mapgen | EMS thunk → FUN_684c_04a6 (rivers pass) | known | src/core/map_gen.c |
| `FUN_2a1f_085a` | 38599 | 10 | mapgen | EMS thunk → FUN_684c_009c (archipelago/normal land-blob wander) | known | src/core/map_gen.c |
| `FUN_2a1f_0868` | 38609 | 10 | mapgen | EMS thunk → FUN_1bbb_0006 pitched-byte get (terrain read in mapgen) | known | src/core/map_gen.c |
| `FUN_2a1f_0872` | 38619 | 10 | mapgen | EMS thunk → FUN_1bb9_000a pitched-byte put (terrain write in mapgen) | known | src/core/map_gen.c |
| `FUN_2a1f_087c` | 38629 | 10 | mapgen | Far thunk → FUN_6a09_0006 (tribe capitals, satellites, Brave spawn loop) | inferred | src/core/ai.c |
| `FUN_2a1f_088a` | 38639 | 10 | mapgen | Far thunk → FUN_7314_0198 (end current config field parse) | inferred |  |
| `FUN_2a1f_0896` | 38649 | 10 | mapgen | Far thunk → FUN_6a9f_0118 (map viewport tile loop) | inferred | src/core/map.c |
| `FUN_2a1f_08a4` | 38659 | 10 | mapgen | Far thunk → FUN_6a9f_0486 (full minimap chrome + tile fill + present) | inferred |  |
| `FUN_2a1f_08b2` | 38669 | 10 | mapgen | Far thunk → FUN_6a9f_0000 (return fixed 16×16 map-color sample byte) | inferred |  |
| `FUN_2a1f_08c0` | 38679 | 10 | mapgen | Far thunk → FUN_6a9f_0034 (return map-color sample byte for palette index) | inferred |  |
| `FUN_2a1f_08ce` | 38689 | 10 | mapgen | Far thunk → FUN_6a9f_0346 (fill minimap tile-index buffer) | inferred |  |
| `FUN_2a1f_08dc` | 38699 | 10 | mapgen | Far thunk → FUN_6a9f_0068 (load terrain→display color LUTs) | inferred |  |
| `FUN_2a1f_08ea` | 38709 | 10 | mapgen | Far thunk → FUN_6afa_0224 (present/flip full map viewport) | inferred |  |
| `FUN_2a1f_08f8` | 38719 | 10 | mapgen | Far thunk → FUN_6afa_023c (present/flip tile-count map region) | inferred |  |
| `FUN_2a1f_0906` | 38729 | 10 | mapgen | Far thunk → FUN_6afa_000c (clamp rect into map viewport tile bounds) | inferred |  |
| `FUN_2a1f_0914` | 38739 | 10 | mapgen | Far thunk → FUN_6afa_0052 (clip rect into viewport; rewrite w/h) | inferred |  |
| `FUN_2a1f_0922` | 38749 | 10 | mapgen | Far thunk → FUN_6b22_0102 (blit fog-visible colonies in map rect) | inferred |  |
| `FUN_2a1f_0930` | 38759 | 10 | mapgen | Far thunk → FUN_6b22_0428 (blit unit overlay or clear same-tile blink) | inferred |  |
| `FUN_2a1f_093e` | 38769 | 10 | mapgen | Far thunk → FUN_6b22_058e (unit overlay for full map viewport) | inferred |  |
| `FUN_2a1f_094c` | 38779 | 10 | mapgen | Far thunk → FUN_6b22_0002 (blit fog-visible tribes in map rect) | inferred |  |
| `FUN_2a1f_095a` | 38789 | 10 | mapgen | Far thunk → FUN_6b22_034c (blit one unit icon unless colony occupies tile) | inferred |  |
| `FUN_2a1f_0968` | 38799 | 10 | mapgen | Far thunk → FUN_6ba1_0d6c (viewport tile-compose loop) | inferred |  |
| `FUN_2a1f_0976` | 38809 | 10 | mapgen | Far thunk → FUN_6ba1_10be (set_owner_nibble 0xf on every tile; unowned init) | inferred | src/core/ai.c; ai/accessors.c |
| `FUN_2a1f_0984` | 38819 | 10 | mapgen | Far thunk → FUN_1caa_0004 (scaled RLE underlay blit) | inferred |  |
| `FUN_2a1f_098e` | 38829 | 10 | mapgen | Far thunk → FUN_1c89_0006 (RLE soft-sprite underlay blit) | inferred |  |
| `FUN_2a1f_0998` | 38839 | 10 | mapgen | Far thunk → FUN_6cb2_039c (init article title paint) | inferred |  |
| `FUN_2a1f_09c2` | 38849 | 10 | ui | EMS thunk → FUN_6cb2_03bc (dialog compositor setup/flush) | inferred |  |
| `FUN_2a1f_09d0` | 38859 | 10 | mapgen | Far thunk → FUN_6cb2_0424 (begin article compositor) | inferred |  |
| `FUN_2a1f_09de` | 38869 | 10 | mapgen | Far thunk → FUN_6cb2_0178 (bubble-sort encyclopedia list by title) | inferred |  |
| `FUN_2a1f_09ec` | 38879 | 10 | mapgen | Far thunk → FUN_6cb2_214a (paint encyclopedia list) | inferred |  |
| `FUN_2a1f_09fa` | 38889 | 10 | ui | EMS thunk → FUN_6cb2_048c (info/dialog panel text + sprite strip) | inferred |  |
| `FUN_2a1f_0a08` | 38899 | 10 | mapgen | Far thunk → FUN_6cb2_2322 (fill encyclopedia index for one category) | inferred |  |
| `FUN_2a1f_0a16` | 38909 | 10 | mapgen | Far thunk → FUN_6cb2_0276 (map list index → 3-col grid xy) | inferred |  |
| `FUN_2a1f_0a24` | 38919 | 10 | mapgen | Far thunk → FUN_6cb2_02c4 (hit-test cursor vs list grid) | inferred |  |
| `FUN_2a1f_0a32` | 38929 | 10 | mapgen | Far thunk → FUN_6cb2_0000 (free Colonizopedia index triple-buffers) | inferred |  |
| `FUN_2a1f_0a40` | 38939 | 10 | mapgen | Far thunk → FUN_6cb2_0058 (alloc index triple-buffers) | inferred |  |
| `FUN_2a1f_0a4e` | 38949 | 10 | mapgen | Far thunk → FUN_6cb2_033a (format list-row title; terrain forest suffix) | inferred |  |
| `FUN_2a1f_0a5c` | 38959 | 10 | mapgen | Far thunk → FUN_6f30_0004 (fill splash rect DS:0x2da8 then video flush 00e2) | inferred |  |
| `FUN_2a1f_0a6a` | 38969 | 10 | mapgen | Far thunk → FUN_7a83_002a (fade palette toward target RGB; timer-gated) | inferred |  |
| `FUN_2a1f_0a78` | 38979 | 10 | mapgen | Far thunk → FUN_7ab3_0008 (wait VGA retrace; read DAC RGB via 0x3c7/0x3c9) | inferred |  |
| `FUN_2a1f_0a86` | 38989 | 10 | mapgen | Far thunk → FUN_7952_0000 (open/alloc named resource into far buf) | inferred |  |
| `FUN_2a1f_0a94` | 38999 | 10 | mapgen | Far thunk → FUN_1b32_000e (ensure filename has extension; strcat default suffix) | inferred |  |
| `FUN_2a1f_0a9e` | 39009 | 10 | mapgen | Far thunk → FUN_6f74_089a (find option/button node by id in dialog option list) | inferred |  |
| `FUN_2a1f_0aaa` | 39019 | 10 | mapgen | Far thunk → FUN_6f74_255e (select icon row and redraw icon column) | inferred |  |
| `FUN_2a1f_0ab6` | 39029 | 10 | mapgen | Far thunk → FUN_6f74_1ae8 (blit dialog side-panel art) | inferred |  |
| `FUN_2a1f_0ac2` | 39039 | 10 | mapgen | Far thunk → FUN_6f74_098a (read option checkbox/toggle state) | inferred |  |
| `FUN_2a1f_0ace` | 39049 | 10 | mapgen | Far thunk → FUN_6f74_0d44 (append edit-field node) | inferred |  |
| `FUN_2a1f_0ada` | 39059 | 10 | mapgen | Far thunk → FUN_6f74_09ba (write option checkbox/toggle state) | inferred |  |
| `FUN_2a1f_0af2` | 39069 | 10 | mapgen | Far thunk → FUN_6f74_248e (full dialog paint pass) | inferred |  |
| `FUN_2a1f_0afe` | 39079 | 10 | mapgen | Far thunk → FUN_6f74_0be8 (OR option-flags/5 + append hotkey option) | inferred |  |
| `FUN_2a1f_0b0a` | 39089 | 10 | mapgen | Far thunk → FUN_6f74_1a78 (draw corner help-‘?’ tip) | inferred |  |
| `FUN_2a1f_0b16` | 39099 | 10 | mapgen | Far thunk → FUN_7314_01b6 (blit/print full config line buffer) | inferred |  |
| `FUN_2a1f_0b22` | 39109 | 10 | mapgen | Far thunk → FUN_7314_01c8 (blit/print current comma field) | inferred |  |
| `FUN_2a1f_0b2e` | 39119 | 10 | mapgen | Far thunk → FUN_7314_01da (parse '0'/'1' bit string to byte) | inferred |  |
| `FUN_2a1f_0b3a` | 39129 | 10 | mapgen | Far thunk → FUN_1c06_000c (parse numeric literal hex/bin/decimal) | inferred |  |
| `FUN_2a1f_0b44` | 39139 | 10 | mapgen | Far thunk → FUN_1b57_0002 (ltrim spaces/tabs then strcpy) | inferred |  |
| `FUN_2a1f_0b4e` | 39149 | 10 | mapgen | Far thunk → FUN_1afb_000e (NUL-truncate string at first LF) | inferred |  |
| `FUN_2a1f_0b58` | 39159 | 10 | mapgen | Far thunk → FUN_733a_0b3e (paint nation-pick chrome) | inferred |  |
| `FUN_2a1f_0b66` | 39169 | 10 | mapgen | Far thunk → FUN_733a_0790 (difficulty-select modal event loop) | inferred | src/core/new_game.c |
| `FUN_2a1f_0b74` | 39179 | 10 | mapgen | Far thunk → FUN_733a_0c2a (nation-select modal event loop) | inferred | src/core/new_game.c |
| `FUN_2a1f_0b82` | 39189 | 10 | mapgen | Far thunk → FUN_733a_0000 (CUSTOMIZE / difficulty-style UI entry) | inferred | src/core/new_game.c |
| `FUN_2a1f_0b90` | 39199 | 10 | mapgen | Far thunk → FUN_733a_04d0 (map difficulty index → DIFFICUL.PIK cell) | inferred |  |
| `FUN_2a1f_0b9e` | 39209 | 10 | mapgen | Far thunk → FUN_733a_002c (draw one CUSTOMIZE cell) | inferred |  |
| `FUN_2a1f_0bac` | 39219 | 10 | mapgen | Far thunk → FUN_733a_0512 (CUSTOMIZE / new-game UI helper) | inferred | src/core/new_game.c |
| `FUN_2a1f_0bba` | 39229 | 10 | mapgen | Far thunk → FUN_733a_01a4 (paint CUSTOMIZE chrome) | inferred |  |
| `FUN_2a1f_0bc8` | 39239 | 10 | mapgen | Far thunk → FUN_733a_0992 (map nation index → NATIONS.PIK cell) | inferred |  |
| `FUN_2a1f_0bd6` | 39249 | 10 | mapgen | Far thunk → FUN_733a_09c6 (draw one nation-pick cell) | inferred |  |
| `FUN_2a1f_0be4` | 39259 | 10 | mapgen | Far thunk → FUN_733a_0270 (CUSTOMIZE / new-game UI helper) | inferred |  |
| `FUN_2a1f_0bf2` | 39269 | 10 | mapgen | Far thunk → FUN_733a_06a4 (paint difficulty chrome) | inferred |  |
| `FUN_2a1f_0c00` | 39279 | 9 | mapgen | Far thunk → LAB_733a_0e7a (empty far RETF no-op) | inferred |  |
| `FUN_2a1f_0c1c` | 39288 | 10 | mapgen | Far thunk → FUN_7421_0054 (argv -opt char dispatch; case-fold via 0x27ed) | inferred |  |
| `FUN_2a1f_0c2a` | 39298 | 10 | mapgen | Far thunk → FUN_7421_0188 (startup config fread into DS:0x260a..) | inferred |  |
| `FUN_2a1f_0c38` | 39308 | 10 | mapgen | Far thunk → FUN_75c2_2d46 (game boot: video/memory/asset init) | inferred |  |
| `FUN_2a1f_0c46` | 39318 | 10 | mapgen | Far thunk → FUN_2085_0004 (init DS:0x26c9 handler block; far stubs → 2b1f) | inferred |  |
| `FUN_2a1f_0c50` | 39328 | 10 | mapgen | Far thunk → FUN_205f_000c (lookup byte+6 in 8-byte table DS:0x26f0 by key) | inferred |  |
| `FUN_2a1f_0c64` | 39338 | 10 | mapgen | Far thunk → FUN_7455_0122 (set large-map compact flag DS:0x15a) | inferred |  |
| `FUN_2a1f_0c72` | 39348 | 10 | mapgen | Far thunk → FUN_7455_0058 (allocate terrain/L2/L3 map planes) | inferred |  |
| `FUN_2a1f_0c80` | 39358 | 10 | mapgen | Far thunk → FUN_7455_0434 (bootstrap map dims/buffers; default 120×75 or from file) | inferred |  |
| `FUN_2a1f_0c8e` | 39368 | 10 | mapgen | Far thunk → FUN_7455_0166 (load map planes from file: dims + terrain/L2/L3) | inferred |  |
| `FUN_2a1f_0c9c` | 39378 | 10 | mapgen | Far thunk → FUN_79db_000c (chunked DOS file read into far buffer) | inferred |  |
| `FUN_2a1f_0caa` | 39388 | 10 | mapgen | Far thunk → FUN_1b32_005c (replace/append filename extension) | inferred |  |
| `FUN_2a1f_0cb4` | 39398 | 10 | mapgen | Far thunk → FUN_1b01_000e (buffered far-buffer file/stream read) | inferred |  |
| `FUN_2a1f_0cbe` | 39408 | 10 | mapgen | Far thunk → FUN_74a4_0bbe (load menu-cursor resource handle) | inferred |  |
| `FUN_2a1f_0ccc` | 39418 | 10 | mapgen | Far thunk → FUN_74a4_0b0a (load 12 menu/cursor sprites) | inferred |  |
| `FUN_2a1f_0cda` | 39428 | 10 | mapgen | Far thunk → FUN_7562_0008 (build COLONY## path strings for slot) | inferred |  |
| `FUN_2a1f_0ce8` | 39438 | 10 | mapgen | Far thunk → FUN_7562_0052 (Save/Load slot-list builder) | inferred | docs/savegame.md |
| `FUN_2a1f_0cf6` | 39448 | 10 | mapgen | Far thunk → FUN_75c2_0288 (savegame write: flags/units/colonies/tribes) | inferred | docs/savegame.md |
| `FUN_2a1f_0d04` | 39458 | 10 | mapgen | Far thunk → FUN_75c2_0840 (savegame header probe; slot error codes) | inferred | docs/savegame.md |
| `FUN_2a1f_0d12` | 39468 | 10 | mapgen | Far thunk → FUN_75c2_0940 (savegame load counterpart to 0288) | inferred | docs/savegame.md |
| `FUN_2a1f_0d20` | 39478 | 10 | mapgen | Far thunk → FUN_75c2_1380 (fill unit-type/stat table row) | inferred |  |
| `FUN_2a1f_0d2e` | 39488 | 10 | mapgen | Far thunk → FUN_75c2_13dc (write building-link table record) | inferred |  |
| `FUN_2a1f_0d3c` | 39498 | 10 | mapgen | Far thunk → FUN_75c2_10ae (new-game nation/difficulty setup) | inferred |  |
| `FUN_2a1f_0d4a` | 39508 | 10 | mapgen | Far thunk → FUN_75c2_1418 (write companion table record) | inferred |  |
| `FUN_2a1f_0d58` | 39518 | 10 | mapgen | Far thunk → FUN_75c2_0204 (copy options into live UI words) | inferred |  |
| `FUN_2a1f_0d66` | 39528 | 10 | mapgen | Far thunk → FUN_75c2_144c (init building/prereq link tables) | inferred |  |
| `FUN_2a1f_0d74` | 39538 | 10 | mapgen | Far thunk → FUN_75c2_024c (hardcode default option/UI words) | inferred |  |
| `FUN_2a1f_0d82` | 39548 | 10 | mapgen | Far thunk → FUN_75c2_1770 (bootstrap static catalogs) | inferred |  |
| `FUN_2a1f_0d90` | 39558 | 10 | mapgen | Far thunk → FUN_75c2_2758 (reapply options after video restore) | inferred |  |
| `FUN_2a1f_0d9e` | 39568 | 10 | mapgen | Far thunk → FUN_75c2_276e (reset options to defaults) | inferred |  |
| `FUN_2a1f_0dac` | 39578 | 10 | mapgen | Far thunk → FUN_75c2_2778 (title/main menu loop) | inferred | docs/savegame.md |
| `FUN_2a1f_0dba` | 39588 | 10 | mapgen | Far thunk → FUN_75c2_0000 (paginated directory name picker) | inferred |  |
| `FUN_2a1f_0dc8` | 39598 | 10 | mapgen | Far thunk → FUN_75c2_2324 (new-game flourish wrapper) | inferred |  |
| `FUN_2a1f_0dd6` | 39608 | 10 | mapgen | Far thunk → FUN_75c2_235c (new-game/world bootstrap) | inferred | docs/savegame.md |
| `FUN_2a1f_0de4` | 39618 | 10 | mapgen | Far thunk → FUN_1b2c_0040 (write string to DOS char stream) | inferred | FUN_1d1d_0758 |
| `FUN_2a1f_0dee` | 39628 | 10 | mapgen | Far thunk → FUN_1b2c_0004 (read string from DOS char stream) | inferred | FUN_1d1d_0786 |
| `FUN_2a1f_0df8` | 39638 | 10 | mapgen | Far thunk → FUN_1bd4_0006 (color-replace across pitched rect) | inferred |  |
| `FUN_2a1f_0e02` | 39648 | 10 | mapgen | Far thunk → FUN_7acf_0002 (alloc far buffer into BX handle struct) | inferred |  |
| `FUN_2a1f_0e10` | 39658 | 10 | mapgen | Far thunk → FUN_78d8_0022 (alloc resource-stream buffer @23c6) | inferred |  |
| `FUN_2a1f_0e1e` | 39668 | 10 | mapgen | Far thunk → FUN_1b70_0002 (bind mouse cursor shape to viewport) | inferred |  |
| `FUN_2a1f_0e28` | 39678 | 10 | mapgen | Far thunk → FUN_7a7c_000e (load 768-byte VGA palette from resource) | inferred |  |
| `FUN_2a1f_0e36` | 39688 | 10 | mapgen | Far thunk → FUN_1b8b_0004 (set BIOS equipment video bits @40:10) | inferred |  |
| `FUN_2a1f_0e40` | 39698 | 10 | mapgen | Far thunk → FUN_7a4c_006a (calibrate fade speed via vsync+PIT) | inferred |  |
| `FUN_2a1f_0e4e` | 39708 | 10 | mapgen | Far thunk → FUN_7aa1_0002 (dismiss message-box handle 0x2604) | inferred |  |
| `FUN_2a1f_0e5c` | 39718 | 10 | mapgen | Far thunk → FUN_7a9d_0004 (load dialog string prep buf →0x929e) | inferred |  |
| `FUN_2a1f_0e6a` | 39728 | 10 | mapgen | Far thunk → FUN_7aa1_003a (message box: create+run modal) | inferred |  |
| `FUN_2a1f_0e78` | 39738 | 10 | mapgen | Far thunk → FUN_1c05_0004 (normalize far ptr → seg+=(off>>4), off&=0xf) | inferred |  |
| `FUN_2a1f_0e82` | 39748 | 10 | mapgen | Far thunk → FUN_798d_0000 (compressed resource chunk read) | inferred |  |
| `FUN_2a1f_0e90` | 39758 | 10 | mapgen | Far thunk → FUN_7ada_0022 (DOS heap alloc + high-water tracking) | inferred |  |
| `FUN_2a1f_0e9e` | 39768 | 10 | mapgen | Far thunk → FUN_7962_0000 (open resource file into handle struct) | inferred |  |
| `FUN_2a1f_0eac` | 39778 | 10 | mapgen | Far thunk → FUN_7962_021c (close resource handle) | inferred |  |
| `FUN_2a1f_0eba` | 39788 | 10 | mapgen | Far thunk → FUN_79a8_004a (wire compressed stream I/O) | inferred |  |
| `FUN_2a1f_0ec8` | 39798 | 10 | mapgen | Far thunk → FUN_79a8_0014 (invoke progress callback) | inferred |  |
| `FUN_2a1f_0ed6` | 39808 | 10 | mapgen | Far thunk → FUN_79a8_000a (return far-buffer near offset) | inferred |  |
| `FUN_2a1f_0ee4` | 39818 | 10 | mapgen | Far thunk → FUN_79ec_0082 (dispatch compress/decompress codec) | inferred |  |
| `FUN_2a1f_0f1a` | 39828 | 10 | mapgen | Far thunk → FUN_7a05_00a4 (dump resource lines until $ match) | inferred |  |
| `FUN_2a1f_0f26` | 39838 | 10 | mapgen | Far thunk → FUN_2088_000c (DOS write string to stdout) | inferred |  |
| `FUN_2a1f_0f30` | 39848 | 10 | mapgen | Far thunk → FUN_7a4c_0000 (write VGA DAC RGB range) | inferred |  |
| `FUN_2a1f_0f3e` | 39858 | 10 | mapgen | Far thunk → FUN_7a83_0002 (dim palette RGB channels) | inferred |  |
| `FUN_2a1f_0f4c` | 39868 | 10 | mapgen | Far thunk → FUN_2047_0106 (install INT abort handlers) | inferred |  |
| `FUN_2a1f_0f56` | 39878 | 10 | mapgen | Far thunk → FUN_2059_0006 (sound driver load/init poll) | inferred | FUN_2059_000a sibling |
| `FUN_2a1f_0f60` | 39888 | 10 | mapgen | Far thunk → FUN_2047_00b8 (arm abort INT vectors) | inferred |  |
| `FUN_2a1f_0f6a` | 39898 | 10 | mapgen | Far thunk → FUN_2047_000a (install Ctrl-C/Break abort context) | inferred |  |
| `FUN_2a1f_0f74` | 39908 | 10 | mapgen | Far thunk → FUN_2047_00e9 (reinstall default abort handlers) | inferred |  |
| `FUN_2a1f_0f7e` | 39918 | 10 | mapgen | Far thunk → FUN_7ad6_000e (free handle far buffer) | inferred |  |
| `FUN_2a1f_0f8c` | 39928 | 10 | mapgen | Far thunk → FUN_1bdd_00e0 (read numbered temp file) | inferred | FUN_1bdd_0002; FUN_1b01_000e |
| `FUN_2a1f_0f96` | 39938 | 10 | mapgen | Far thunk → FUN_1bdd_0002 (create numbered temp file) | inferred | FUN_1bdd_00e0; FUN_19f6_0002 |
| `FUN_2a1f_0fa0` | 39948 | 10 | mapgen | Far thunk → FUN_7acf_003c (alloc far buffer into handle) | inferred |  |
| `FUN_2a1f_0fae` | 39958 | 10 | mapgen | Far thunk → FUN_7ada_0006 (run heap callback with reentrancy flag) | inferred |  |
| `FUN_2a1f_0fbc` | 39968 | 10 | mapgen | Far thunk → FUN_2100_000e (XMS UMB size probe) | inferred |  |
| `FUN_2a1f_0fc6` | 39978 | 10 | mapgen | Far thunk → FUN_7b04_0002 (DOS free-memory probe) | inferred |  |
| `FUN_2a1f_0fd4` | 39988 | 10 | mapgen | Far thunk → FUN_2103_004c (XMS release UMB) | inferred |  |
| `FUN_2a1f_0fde` | 39998 | 10 | mapgen | Far thunk → FUN_2103_000a (XMS request UMB) | inferred |  |
| `FUN_2a1f_0fe8` | 40008 | 179 | mapgen | Far thunk → FUN_79ec_0004 (resource-stream progress pump) | inferred |  |

### Segment `2b5a` (53 defs) — ui — Map selected-unit order / input UI (DS:0x5392)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2b5a_0000` | 40187 | 10 | ui | Prefetch two options-dialog string slots (0x80c/0x810 via 6f74_03d0) | inferred |  |
| `FUN_2b5a_001e` | 40197 | 40 | ui | European Status bring-up: nation blob copy + dialog (12fd/6f74) | inferred |  |
| `FUN_2b5a_0070` | 40237 | 1844 | ui | Selected-unit ORDERS mega-dispatch/dialog hub (DS:0x5392; decomp noisy) | inferred |  |
| `FUN_2b5a_0722` | 42081 | 24 | ui | Show Hidden Terrain: label every viewport tile via 7a65_0008 | inferred |  |
| `FUN_2b5a_0902` | 42105 | 41 | ui | Baseline-enable map menu widgets; set View Pieces (5390=1) | inferred |  |
| `FUN_2b5a_0b08` | 42146 | 18 | ui | Order-menu gates: cargo-capable? / wagon type==2? | inferred |  |
| `FUN_2b5a_0b34` | 42164 | 103 | ui | Contextual ORDERS menu enable/disable for selected unit (Move Pieces) | inferred |  |
| `FUN_2b5a_0e52` | 42267 | 73 | ui | Select/activate unit UI: chrome, Move Pieces, optional ORDERS hub 0070 | inferred |  |
| `FUN_2b5a_0f92` | 42340 | 24 | ui | Set map zoom level 0..3 (DS:0x184); recompute viewport + refresh | inferred |  |
| `FUN_2b5a_1112` | 42364 | 63 | ui | Fortify order (314c=5); optional adjacent-native contact; exhaust MP | inferred |  |
| `FUN_2b5a_123e` | 42427 | 93 | ui | Clear/Plow order (314c=8) → pioneer work body 479b_01a6 | inferred |  |
| `FUN_2b5a_1454` | 42520 | 89 | ui | Build Road order (314c=9) → pioneer road body 479b_0526 | inferred |  |
| `FUN_2b5a_199e` | 42609 | 76 | ui | Focus-tile click: enter colony/village or confirm-disband unit | inferred |  |
| `FUN_2b5a_1b5a` | 42685 | 109 | ui | Activate-unit stack picker dialog; select unit and clear orders | inferred |  |
| `FUN_2b5a_1dfc` | 42794 | 31 | ui | Go-To order (314c=3): colony-select dialog or sail/Europe fallback | inferred |  |
| `FUN_2b5a_1e66` | 42825 | 47 | ui | Begin Trade Route order (314c=2) via colony/slot pickers | inferred |  |
| `FUN_2b5a_1f36` | 42872 | 38 | ui | Redraw map at each zoom tier 0..3 (VIEW zoom apply) | inferred |  |
| `FUN_2b5a_1fc0` | 42910 | 24 | ui | Redraw map walking zoom index downward to 0 | inferred |  |
| `FUN_2b5a_20f6` | 42934 | 60 | ui | Game Options checkbox dialog → pack bits into DS:0x5382/0x5383 | inferred |  |
| `FUN_2b5a_223a` | 42994 | 64 | ui | Colony Report Options dialog → pack bits into DS:0x5384/0x5385 | inferred |  |
| `FUN_2b5a_23ce` | 43058 | 62 | ui | Sound Options dialog → DS:0xa0/a2/a4 + 0x5386 sound flags | inferred |  |
| `FUN_2b5a_2464` | 43120 | 1672 | ui | Map menu-command mega-dispatch (GAME/VIEW/ORDERS/…; decomp noisy) | inferred |  |
| `FUN_2b5a_268c` | 44792 | 27 | ui | Seven-checkbox options dialog → pack bits into DS:0x894 | inferred |  |
| `FUN_2b5a_26f6` | 44819 | 68 | ui | GAME menu item dispatch (options/save/…) then ORDERS menu refresh | inferred |  |
| `FUN_2b5a_2866` | 44887 | 18 | ui | Activate unit at focus tile (1b5a) then refresh ORDERS menu | inferred |  |
| `FUN_2b5a_3036` | 44905 | 9 | ui | Thin wrapper → contextual ORDERS menu refresh (0b34) | inferred |  |
| `FUN_2b5a_303c` | 44914 | 93 | ui | Map keyboard/input dispatch on DS:0x981e | inferred |  |
| `FUN_2b5a_3094` | 45007 | 44 | ui | Move/View Pieces chord: toggle 5383.0x20 + status chrome; advance b92 | inferred |  |
| `FUN_2b5a_30ce` | 45051 | 31 | ui | Move/View Pieces chord tail (b92 stage / key 0x117/0x131) | inferred |  |
| `FUN_2b5a_3104` | 45082 | 18 | ui | Esc/cancel map modal: dialog flush then clear DS:0x53c2 | inferred |  |
| `FUN_2b5a_311c` | 45100 | 19 | ui | Nudge viewport center Y −1 (scroll up) + chrome refresh | inferred |  |
| `FUN_2b5a_313e` | 45119 | 22 | ui | Nudge viewport center Y +1 (scroll down) + chrome refresh | inferred |  |
| `FUN_2b5a_3145` | 45141 | 31 | ui | Clamp/set viewport center Y from prior compare; refresh chrome | inferred |  |
| `FUN_2b5a_3154` | 45172 | 21 | ui | Nudge viewport center X −1 (scroll left) + chrome refresh | inferred |  |
| `FUN_2b5a_316e` | 45193 | 21 | ui | Nudge viewport center X +1 (scroll right) + chrome refresh | inferred |  |
| `FUN_2b5a_3188` | 45214 | 19 | ui | Page viewport up by DS:0x188 + chrome refresh | inferred |  |
| `FUN_2b5a_3194` | 45233 | 19 | ui | Page viewport down by DS:0x188 + chrome refresh | inferred |  |
| `FUN_2b5a_31be` | 45252 | 21 | ui | Page viewport right by DS:0x188 + chrome refresh | inferred |  |
| `FUN_2b5a_321c` | 45273 | 78 | ui | Numpad/arrow scroll dispatch (0x34..0x39/0x110) → pan helpers | inferred |  |
| `FUN_2b5a_3252` | 45351 | 489 | ui | Extended map-key dispatch (0x149+): wait/activate, build colony, … | inferred |  |
| `FUN_2b5a_32a2` | 45840 | 30 | ui | Post-key commit: dir8 move or unit-act, then ORDERS menu refresh | inferred |  |
| `FUN_2b5a_32ee` | 45870 | 24 | ui | Enter: open owned colony at focus tile (else ignore) | inferred |  |
| `FUN_2b5a_3344` | 45894 | 33 | ui | Enter/Space: open colony or clear View Pieces / end-turn latch | inferred |  |
| `FUN_2b5a_33ce` | 45927 | 25 | ui | Mouse hit-test: map / right panel / minimap region code 1..3 | inferred |  |
| `FUN_2b5a_3442` | 45952 | 11 | ui | Set mouse cursor from table entry DS:0x83a | inferred |  |
| `FUN_2b5a_3458` | 45963 | 9 | ui | Thin wrapper → set cursor (3442) with arg 1 | inferred |  |
| `FUN_2b5a_3462` | 45972 | 150 | ui | Map click: select unit/colony, set Go-To, or recenter viewport | inferred |  |
| `FUN_2b5a_36e6` | 46122 | 22 | ui | Minimap drag: pan viewport center to mapped tile | inferred |  |
| `FUN_2b5a_3752` | 46144 | 35 | ui | Mouse-up edge: clear focus blink / View Pieces / end-turn latch | inferred |  |
| `FUN_2b5a_37b2` | 46179 | 34 | ui | Mouse-button dispatch by hit region (33ce) → click/drag handlers | inferred |  |
| `FUN_2b5a_3802` | 46213 | 200 | ui | Map input poll loop: sample key/mouse → 303c / menu / click paths | inferred |  |
| `FUN_2b5a_3ae6` | 46413 | 403 | ui | Move Pieces frame: poll input (3802) then tick active unit order | inferred |  |
| `FUN_2b5a_3b68` | 46816 | 291 | ui | Map UI main loop: clear selection, then Move/View Pieces frame | inferred |  |

### Segment `2f2b` (75 defs) — colony — Colony screen / build / colonist logic (DS:0x8542)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2f2b_011e` | 47107 | 93 | colony | Colony build / order gate (prereq / error codes) | inferred |  |
| `FUN_2f2b_0332` | 47200 | 59 | colony | Reorder colonist slot tables after sort | inferred |  |
| `FUN_2f2b_0434` | 47259 | 73 | colony | Init settlement building-slot layout maps | inferred | docs/assets.md |
| `FUN_2f2b_05b0` | 47332 | 9 | colony | Tiny far stub into colony chrome | inferred |  |
| `FUN_2f2b_05b6` | 47341 | 16 | colony | Colony wood-panel blit/setup | inferred | docs/assets.md |
| `FUN_2f2b_05ee` | 47357 | 43 | colony | Draw building/project name label at settlement slot | inferred |  |
| `FUN_2f2b_0722` | 47400 | 30 | colony | Draw one warehouse cargo-type amount label | inferred |  |
| `FUN_2f2b_0842` | 47430 | 63 | colony | Draw area-view tile tooltip (terrain/feature text) | inferred | docs/assets.md |
| `FUN_2f2b_0a3e` | 47493 | 14 | colony | Blit colony chrome rect (281f_0444) | inferred |  |
| `FUN_2f2b_0a74` | 47507 | 53 | colony | Draw area-view surrounding map unit icons | inferred | docs/assets.md |
| `FUN_2f2b_0ba8` | 47560 | 161 | colony | Draw colony area-view (5x5 map panel); trust ASM over corrupt C tail | inferred | docs/assets.md |
| `FUN_2f2b_0d89` | 47721 | 100 | colony | Draw one area-view terrain tile (loop body) | inferred | docs/assets.md |
| `FUN_2f2b_0fce` | 47821 | 62 | colony | Draw colony top-bar name + date/nation string | inferred | docs/assets.md |
| `FUN_2f2b_11b2` | 47883 | 64 | colony | Draw outside/fence unit strip | inferred | docs/assets.md |
| `FUN_2f2b_12cc` | 47947 | 49 | colony | Draw colonists assigned to one workplace building | inferred | docs/assets.md |
| `FUN_2f2b_13c2` | 47996 | 259 | colony | Resolve building-type badge amount + sprite outs (ASM to RETF) | inferred |  |
| `FUN_2f2b_14d4` | 48255 | 101 | colony | Draw one settlement building (icon + production badge) | inferred | docs/assets.md |
| `FUN_2f2b_16f2` | 48356 | 13 | colony | Draw settlement empty/placeholder sprite if table flag | inferred |  |
| `FUN_2f2b_171c` | 48369 | 31 | colony | Draw settlement building strip (15 slots) | inferred | docs/assets.md |
| `FUN_2f2b_17d0` | 48400 | 132 | colony | Draw people band (colonists + SoL/Tory meters) | inferred | docs/assets.md |
| `FUN_2f2b_1cce` | 48532 | 61 | colony | Draw Production multifunction cargo/shortfall strip | inferred | docs/assets.md |
| `FUN_2f2b_1e46` | 48593 | 115 | colony | Draw Units multifunction pane (docked unit grid) | inferred | docs/assets.md |
| `FUN_2f2b_2054` | 48708 | 18 | colony | Measure text extent for colony UI button/label | inferred |  |
| `FUN_2f2b_208c` | 48726 | 59 | colony | Draw framed invertible button/label | inferred |  |
| `FUN_2f2b_21da` | 48785 | 21 | colony | Draw Construction BUY / CHANGE buttons | inferred | docs/assets.md |
| `FUN_2f2b_2262` | 48806 | 15 | colony | Refresh Construction CHANGE button highlight | inferred |  |
| `FUN_2f2b_22b6` | 48821 | 78 | colony | Draw Construction hammer progress bars | inferred | docs/assets.md |
| `FUN_2f2b_2484` | 48899 | 16 | colony | Layout rect for transport hold slot N | inferred |  |
| `FUN_2f2b_24b2` | 48915 | 121 | colony | Draw transport / ship-hold pane | inferred | docs/assets.md |
| `FUN_2f2b_284c` | 49036 | 25 | colony | Draw right multifunction pane by mode DS:0x337 | inferred | docs/assets.md |
| `FUN_2f2b_289e` | 49061 | 16 | colony | Warehouse cargo-strip X pos / selection marker | inferred |  |
| `FUN_2f2b_28d6` | 49077 | 85 | colony | Draw warehouse cargo strip (16 types + Exit) | inferred | docs/assets.md |
| `FUN_2f2b_2b2c` | 49162 | 18 | colony | Unit-chrome sprite rect helper | inferred |  |
| `FUN_2f2b_2b66` | 49180 | 39 | colony | Draw vertical multifunction tab buttons | inferred |  |
| `FUN_2f2b_2c3c` | 49219 | 56 | colony | Draw multifunction mode-selector chrome | inferred |  |
| `FUN_2f2b_2c92` | 49275 | 23 | colony | Full colony-screen redraw (all panel thunks) | inferred | docs/assets.md |
| `FUN_2f2b_2d0e` | 49298 | 10 | colony | Post-drag chrome refresh | inferred |  |
| `FUN_2f2b_2d1c` | 49308 | 33 | colony | Toggle numbers display (DS:0x334) and refresh active pane | inferred |  |
| `FUN_2f2b_2d90` | 49341 | 37 | colony | Force numbers-on + full pane refresh | inferred |  |
| `FUN_2f2b_2e2a` | 49378 | 27 | colony | Alternate numbers-on full refresh path | inferred |  |
| `FUN_2f2b_2e92` | 49405 | 10 | colony | Modal dialog frame helper | inferred |  |
| `FUN_2f2b_2eb2` | 49415 | 11 | colony | Print dialog string-table line | inferred |  |
| `FUN_2f2b_2ec6` | 49426 | 11 | colony | Open 3-button message dialog wrapper | inferred |  |
| `FUN_2f2b_2eea` | 49437 | 21 | colony | Cargo-type confirm dialog wrapper | inferred |  |
| `FUN_2f2b_2f26` | 49458 | 23 | colony | Arm native-trade session pointers (0xbb8/9cce/9cd0) | inferred |  |
| `FUN_2f2b_2f3e` | 49481 | 985 | colony | Assign colonist to workplace/job (XREF-clear; decomp messy) | inferred | docs/assets.md |
| `FUN_2f2b_348c` | 50466 | 441 | colony | Field-jobs / Leave-as profession popup | inferred | docs/assets.md |
| `FUN_2f2b_3fa6` | 50907 | 47 | colony | Area-tile click: select or assign colonist | inferred | docs/assets.md |
| `FUN_2f2b_40a0` | 50954 | 54 | colony | Colony click region hit-test (panel codes) | inferred | docs/assets.md |
| `FUN_2f2b_41c0` | 51008 | 45 | colony | People-band hit-test → colonist index | inferred |  |
| `FUN_2f2b_4284` | 51053 | 19 | colony | Select colonist; enter assign/profession mode (8d54=6) | inferred | docs/assets.md |
| `FUN_2f2b_42be` | 51072 | 13 | colony | Enter colony drag/select mode (8d54=7) | inferred | docs/assets.md |
| `FUN_2f2b_42f2` | 51085 | 14 | colony | Clear drag/select chrome (0x344) | inferred |  |
| `FUN_2f2b_4424` | 51099 | 40 | colony | Warehouse capacity / cargo-limits popup | inferred |  |
| `FUN_2f2b_44d4` | 51139 | 174 | colony | Settlement building-slot click / assign handler | inferred | docs/assets.md |
| `FUN_2f2b_47bc` | 51313 | 100 | colony | Area-view click / assign handler | inferred | docs/assets.md |
| `FUN_2f2b_4a1c` | 51413 | 73 | colony | Multifunction tab click (Production/Units/Construction) | inferred | docs/assets.md |
| `FUN_2f2b_4b62` | 51486 | 79 | colony | Load cargo warehouse→hold (amount dialog) | inferred | docs/assets.md |
| `FUN_2f2b_4da6` | 51565 | 79 | colony | Unload cargo hold→warehouse | inferred | docs/assets.md |
| `FUN_2f2b_4fec` | 51644 | 78 | colony | Transfer cargo between unit holds | inferred |  |
| `FUN_2f2b_51ec` | 51722 | 458 | colony | People-band confirm / Leave-as entry (XREF-clear; decomp messy) | inferred | docs/assets.md |
| `FUN_2f2b_548e` | 52180 | 67 | colony | People-band click (select, drag, or confirm) | inferred | docs/assets.md |
| `FUN_2f2b_55da` | 52247 | 47 | colony | Transport hold-slot click / drag handler | inferred | docs/assets.md |
| `FUN_2f2b_56ce` | 52294 | 27 | colony | Drag-mode router (people band vs transport hold) | inferred | docs/assets.md |
| `FUN_2f2b_5746` | 52321 | 120 | colony | Docked-unit orders popup (sentry / fortify / …) | inferred | docs/assets.md |
| `FUN_2f2b_59a0` | 52441 | 49 | colony | Units-pane click (select docked unit / open orders) | inferred | docs/assets.md |
| `FUN_2f2b_5a68` | 52490 | 59 | colony | Construction CHANGE menu row (name / cost) | inferred |  |
| `FUN_2f2b_5bd2` | 52549 | 134 | colony | Construction CHANGE project picker popup | inferred | docs/assets.md |
| `FUN_2f2b_5e44` | 52683 | 77 | colony | Construction BUY remaining project (gold + tools) | inferred | docs/assets.md |
| `FUN_2f2b_5fc6` | 52760 | 53 | colony | Construction BUY/CHANGE button hit + dispatch | inferred | docs/assets.md |
| `FUN_2f2b_60dc` | 52813 | 71 | colony | Warehouse cargo-strip click / drag handler | inferred | docs/assets.md |
| `FUN_2f2b_628a` | 52884 | 1478 | colony | Colony mouse/click dispatcher by panel region | inferred | docs/assets.md |
| `FUN_2f2b_6372` | 54362 | 1575 | colony | Colony keyboard dispatcher (N/M/B/L/U/…) | inferred | docs/assets.md |
| `FUN_2f2b_6c46` | 55937 | 38 | colony | Clear hover dirty-flags; refresh stale panels | inferred |  |
| `FUN_2f2b_6cd4` | 55975 | 847 | colony | Colony screen entry / bring-up + main loop teardown | inferred | docs/assets.md |

### Segment `364b` (11 defs) — colony — Colony found / build screen (DS:0x8542)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_364b_0000` | 56822 | 60 | colony | Colony message/confirm dialog (title from DS:0x8542+2; may set mode 337) | inferred |  |
| `FUN_364b_0114` | 56882 | 95 | colony | Complete construction project; apply upgrades/flags and reset hammers | inferred |  |
| `FUN_364b_033a` | 56977 | 39 | colony | Area pass: set map feature 4 on ocean/hills tiles worked by jobs 6/7 | inferred |  |
| `FUN_364b_03f6` | 57016 | 107 | colony | Coastal fort fire: spawn attacks vs enemy ships on adjacent ocean | **Done** thin (`units_coastal_fort_fire_pulse`); ship-slow PARK | turn/coastal_fort_fire.md |
| `FUN_364b_0636` | 57123 | 29 | colony | Customs-house auto-sell gate for cargo type | inferred |  |
| `FUN_364b_0688` | 57152 | 803 | colony | Colony EOT production/stock/SoL/construction tick (DS:0x8542) | inferred | turn/colony_eot_production.md |
| `FUN_364b_1aec` | 57955 | 15 | colony | Bind colony; assign unit into workplace via 2f3e | inferred |  |
| `FUN_364b_1b1a` | 57970 | 16 | colony | Bind colony; place unit into colonist slot (0c36) | inferred |  |
| `FUN_364b_1b4c` | 57986 | 15 | colony | Refresh colony warehouse-capacity slots from pop/buildings | inferred |  |
| `FUN_364b_1b76` | 58001 | 14 | colony | Gate: colony warehouse/trade slot usable for nation | inferred |  |
| `FUN_364b_1ba8` | 58015 | 253 | colony | Found colony: bump 539e, init colony record via DS:0x8542 | inferred |  |

### Segment `3844` (3 defs) — turn — Euro EOT treasure / ship-ready unit chrome

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_3844_0004` | 58268 | 37 | turn | EOT treasure tick (type0/prof 0x1b): after 8 turns outside colony, remove + msg | inferred | turn/nation_eot.c |
| `FUN_3844_00f2` | 58305 | 125 | turn | Nation EOT: treasure ticks, ship-build ready chrome, Europe EOT, colony pass | inferred | turn/nation_eot.c; turn/nation_eot_ship_spawn.md; turn/between_turns.md |
| `FUN_3844_0442` | 58430 | 265 | turn | Year-end Euro chrome: independence, diplomacy, calendar events | inferred | turn/year_end_chrome.c; turn/year_end_chrome.md |

### Segment `38fd` (81 defs) — trade — Nation Europe market / cargo trade (nation*0x13c via 0x84fc)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_38fd_0000` | 58695 | 12 | trade | Set current nation index and Europe-market base (nation*0x13c) | inferred | docs/savegame.md |
| `FUN_38fd_0016` | 58707 | 15 | trade | Europe effective price = euro_price[cargo] + adj (floor 0) | inferred | docs/savegame.md |
| `FUN_38fd_0040` | 58722 | 19 | trade | Europe ask helper: euro_price[cargo] - 1 (floor 0) | inferred |  |
| `FUN_38fd_0058` | 58741 | 269 | trade | Market dynamics: adjust euro_price[] / pressure from colony ledgers | inferred | turn/europe_nation_eot.md; docs/savegame.md |
| `FUN_38fd_05e8` | 59010 | 10 | trade | Test cargo boycott bit in nation boycott_bitmap | inferred | docs/savegame.md |
| `FUN_38fd_05fc` | 59020 | 28 | trade | Count Europe-harbor cargo units | inferred |  |
| `FUN_38fd_0666` | 59048 | 25 | trade | Resolve Nth harbor unit index | inferred |  |
| `FUN_38fd_06c4` | 59073 | 25 | trade | Resolve Nth harbor ship index | inferred |  |
| `FUN_38fd_0718` | 59098 | 50 | trade | Spawn purchased/recruited unit into Europe harbor | inferred |  |
| `FUN_38fd_07c6` | 59148 | 22 | trade | Recount harbor ships; clamp selection | inferred |  |
| `FUN_38fd_081c` | 59170 | 16 | trade | Init harbor unit count + Europe selection state | inferred |  |
| `FUN_38fd_0836` | 59186 | 14 | trade | Europe window frame setup | inferred |  |
| `FUN_38fd_086c` | 59200 | 16 | trade | Load Europe screen art / PIK bring-up | inferred | docs/assets.md |
| `FUN_38fd_08a4` | 59216 | 42 | trade | Draw market cargo name/price caption | inferred |  |
| `FUN_38fd_0a26` | 59258 | 35 | trade | Draw Europe top status strip (nation/season/gold/tax) | inferred | docs/assets.md |
| `FUN_38fd_0b64` | 59293 | 63 | trade | Draw Europe market cargo strip (16 cells) | inferred | docs/assets.md |
| `FUN_38fd_0d48` | 59356 | 60 | trade | Market cargo cell layout / scaling helper | inferred |  |
| `FUN_38fd_0e16` | 59416 | 48 | trade | Blit one market cargo cell (+ highlight) | inferred |  |
| `FUN_38fd_0f5e` | 59464 | 14 | trade | Cargo-slot rect constants | inferred |  |
| `FUN_38fd_0f8c` | 59478 | 106 | trade | Draw Loading / harbor-ships panel | inferred | docs/assets.md |
| `FUN_38fd_127c` | 59584 | 37 | trade | Draw Bound For New World ship panel | inferred | docs/assets.md |
| `FUN_38fd_1382` | 59621 | 33 | trade | Draw Expected Soon ship panel | inferred | docs/assets.md |
| `FUN_38fd_1456` | 59654 | 11 | trade | Refresh Bound + Expected ship panels | inferred |  |
| `FUN_38fd_146c` | 59665 | 36 | trade | Dock immigrant layout / scaling helper | inferred |  |
| `FUN_38fd_14e2` | 59701 | 41 | trade | Blit one dock immigrant unit | inferred |  |
| `FUN_38fd_15aa` | 59742 | 37 | trade | Draw docks immigrant panel | inferred | docs/assets.md |
| `FUN_38fd_1660` | 59779 | 19 | trade | Recruit-button label metrics | inferred |  |
| `FUN_38fd_1696` | 59798 | 82 | trade | Draw one bevelled Europe text button | inferred |  |
| `FUN_38fd_1878` | 59880 | 29 | trade | Draw Recruit / Train / Purchase button column | inferred | docs/assets.md |
| `FUN_38fd_18fc` | 59909 | 17 | trade | Full Europe-screen redraw | inferred | docs/assets.md |
| `FUN_38fd_1956` | 59926 | 10 | trade | Europe overlay teardown | inferred |  |
| `FUN_38fd_1960` | 59936 | 28 | trade | Toggle/refresh highlighted panel by mode | inferred |  |
| `FUN_38fd_199e` | 59964 | 22 | trade | Force-refresh Europe panels after mutation | inferred |  |
| `FUN_38fd_19d8` | 59986 | 10 | trade | Europe text blit wrapper | inferred |  |
| `FUN_38fd_19f8` | 59996 | 11 | trade | Europe string-table text blit by index | inferred |  |
| `FUN_38fd_1a0c` | 60007 | 11 | trade | Short Europe dialog open/close wrapper | inferred |  |
| `FUN_38fd_1a30` | 60018 | 28 | trade | Buy/sell confirm dialog text (cargo x price) | inferred |  |
| `FUN_38fd_1aba` | 60046 | 42 | trade | Hit-test Europe regions to panel/mode code | inferred |  |
| `FUN_38fd_1b9e` | 60088 | 13 | trade | Enter Europe UI mode 10 | inferred |  |
| `FUN_38fd_1bd2` | 60101 | 23 | trade | Drag-start bark dialog | inferred |  |
| `FUN_38fd_1c64` | 60124 | 16 | trade | Enter Europe drag mode 8 | inferred |  |
| `FUN_38fd_1cac` | 60140 | 16 | trade | Enter Europe drag mode 9 | inferred |  |
| `FUN_38fd_1cf4` | 60156 | 14 | trade | Exit Europe drag/sound mode | inferred |  |
| `FUN_38fd_1d28` | 60170 | 16 | trade | Decrement euro_price[cargo] (floor 0) | inferred |  |
| `FUN_38fd_1d44` | 60186 | 18 | trade | Difficulty/tax-scaled percent helper | inferred |  |
| `FUN_38fd_1d80` | 60204 | 43 | trade | Buy volume: update tons/gold ledgers | inferred | docs/savegame.md |
| `FUN_38fd_1dfa` | 60247 | 54 | trade | Sell volume: update ledgers + tax-adjusted gold | inferred | docs/savegame.md |
| `FUN_38fd_1ebc` | 60301 | 19 | trade | Apply buy: debit gold, add harbor tons | inferred | docs/savegame.md |
| `FUN_38fd_1f0c` | 60320 | 23 | trade | Apply sell: pull hold tons toward quote | inferred | docs/savegame.md |
| `FUN_38fd_1f66` | 60343 | 12 | trade | Clear top market strip; init row Y | inferred |  |
| `FUN_38fd_1f7e` | 60355 | 11 | trade | Advance market row Y cursor | inferred |  |
| `FUN_38fd_1f8e` | 60366 | 11 | trade | Blit market strip panel | inferred |  |
| `FUN_38fd_1fa2` | 60377 | 130 | trade | Buy cargo dialog / execute buy flow | inferred | docs/savegame.md |
| `FUN_38fd_23c4` | 60507 | 143 | trade | Sell cargo dialog / execute sell + tax | inferred | docs/savegame.md |
| `FUN_38fd_285c` | 60650 | 79 | trade | Transfer cargo between ship holds | inferred |  |
| `FUN_38fd_2a92` | 60729 | 58 | trade | Sell/scrap harbor ship dialog | inferred |  |
| `FUN_38fd_2bfe` | 60787 | 117 | trade | Harbor ship context menu (sail / sell / unload) | inferred |  |
| `FUN_38fd_2dfe` | 60904 | 45 | trade | Pay to lift cargo boycott: cost = ask_price×500, credits nation.royal_money, clears boycott bit | confirmed | turn/europe_nation_eot.md |
| `FUN_38fd_2edc` | 60949 | 88 | trade | Loading-panel ship click / selection handler | inferred |  |
| `FUN_38fd_30aa` | 61037 | 47 | trade | Hold-slot click: load/unload cargo | inferred |  |
| `FUN_38fd_31c6` | 61084 | 23 | trade | Hold-strip hit-test / drag-target arm | inferred |  |
| `FUN_38fd_3502` | 61107 | 73 | trade | Market cargo strip click / buy-sell arm | inferred |  |
| `FUN_38fd_3694` | 61180 | 32 | trade | Dock immigrant info / embark bark dialog | inferred |  |
| `FUN_38fd_3746` | 61212 | 2854 | trade | Dock immigrant action mega-dialog (board / orders); decomp noisy | inferred |  |
| `FUN_38fd_3c86` | 64066 | 66 | trade | Docks panel click / immigrant select handler | inferred |  |
| `FUN_38fd_3dc8` | 64132 | 184 | trade | Apply tax delta; may boycott a cargo | inferred | docs/savegame.md |
| `FUN_38fd_41ce` | 64316 | 139 | trade | Train expert dialog (job list + gold) | inferred | docs/assets.md |
| `FUN_38fd_44a4` | 64455 | 61 | trade | King tax-raise event (pay gold + bump tax) | inferred | docs/savegame.md |
| `FUN_38fd_4590` | 64516 | 38 | trade | Tax-pressure / boycott RNG check | inferred |  |
| `FUN_38fd_46d4` | 64554 | 106 | trade | Roll next dock immigrant profession | inferred |  |
| `FUN_38fd_4884` | 64660 | 140 | trade | Recruit dialog (3 pool slots + passage cost) | inferred | docs/assets.md; turn/europe_nation_eot.md |
| `FUN_38fd_4b50` | 64800 | 115 | trade | Purchase ship/artillery dialog | inferred | docs/assets.md |
| `FUN_38fd_4e8e` | 64915 | 974 | trade | Europe mouse/drag input dispatcher; decomp noisy | inferred |  |
| `FUN_38fd_4f6e` | 65889 | 2205 | trade | Europe keyboard/hotkey input dispatcher; decomp noisy | inferred |  |
| `FUN_38fd_5580` | 68094 | 20 | trade | Clear drag-pending flag; refresh market strip | inferred |  |
| `FUN_38fd_55b6` | 68114 | 134 | trade | Europe screen entry + main event loop | inferred | turn/europe_finish_bridge.md; docs/assets.md |
| `FUN_38fd_584a` | 68248 | 57 | trade | Recruit-passage / immigration pressure score | inferred |  |
| `FUN_38fd_5930` | 68305 | 115 | trade | Europe EOT FF cargo gift / grant | inferred |  |
| `FUN_38fd_5be8` | 68420 | 119 | trade | King audience tax event (cut or raise by favor score; applies via FUN_38fd_3dc8) | inferred | docs/savegame.md |
| `FUN_38fd_5e52` | 68539 | 88 | trade | Europe nation end-of-turn (market + tax + pool) | inferred | turn/europe_nation_eot.md; docs/savegame.md |
| `FUN_38fd_6024` | 68627 | 755 | trade | New-game init Europe market state (all 4 nations) | inferred | docs/savegame.md |

### Segment `3f3f` (1 defs) — platform — CRC/LFSR step

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_3f3f_0006` | 69382 | 15 | platform | CRC/LFSR step: shift right, XOR poly when LSB set | known |  |

### Segment `3f41` (18 defs) — ui — Report / diplomacy / market UI screens

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_3f41_0000` | 69397 | 27 | ui | Report plate bring-up (load art + palette into 2da8 box) | inferred | docs/assets.md |
| `FUN_3f41_008a` | 69424 | 27 | ui | Report footer/title strip blit (default y=0xb8) | inferred |  |
| `FUN_3f41_010a` | 69451 | 160 | ui | Indian Adviser report (F9): tribe rows, villages, converts | inferred | docs/assets.md |
| `FUN_3f41_0618` | 69611 | 39 | ui | Continental Congress report (F3): title + tax line | inferred | docs/assets.md |
| `FUN_3f41_06d0` | 69650 | 174 | ui | Religious Adviser (F2): crosses, SoL bars, FF list | inferred | docs/assets.md |
| `FUN_3f41_0ae6` | 69824 | 90 | ui | Mid-entry of 06d0 FF-list tail (Ghidra split) | inferred |  |
| `FUN_3f41_0d3e` | 69914 | 144 | ui | Labor Adviser detail: one profession’s colony placements | inferred |  |
| `FUN_3f41_10d8` | 70058 | 123 | ui | Labor Adviser (F4): profession grid; click→0d3e | inferred | docs/assets.md |
| `FUN_3f41_1438` | 70181 | 31 | ui | Economic Adviser header chrome (REPORT5 plate) | inferred | docs/assets.md |
| `FUN_3f41_1550` | 70212 | 69 | ui | Economic Adviser colony cargo-stock rows | inferred |  |
| `FUN_3f41_1710` | 70281 | 145 | ui | Economic Adviser (F5): cargo buy/sell ledger table | inferred | docs/assets.md |
| `FUN_3f41_1b94` | 70426 | 17 | ui | Colony Adviser header chrome (REPORT6 plate) | inferred | docs/assets.md |
| `FUN_3f41_1bec` | 70443 | 95 | ui | Colony Adviser (F6): per-colony pop/build/garrison rows | inferred | docs/assets.md |
| `FUN_3f41_1e80` | 70538 | 17 | ui | Naval/military-in-colony report header chrome | inferred |  |
| `FUN_3f41_1ed8` | 70555 | 75 | ui | Naval Adviser body: combat units docked per colony | inferred | docs/assets.md |
| `FUN_3f41_20b4` | 70630 | 45 | ui | Unit disposition report header (column labels) | inferred |  |
| `FUN_3f41_220c` | 70675 | 112 | ui | Unit disposition list (land/naval; orders/dest chrome) | inferred |  |
| `FUN_3f41_2548` | 70787 | 247 | ui | Foreign Affairs Advisor (F8): euro rivals, war, strength | inferred | docs/assets.md |

### Segment `41f2` (9 defs) — ai — Tribe growth (Indian-turn growth tick + message UI)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_41f2_000e` | 71034 | 16 | ai | Present nation-score report title dialog (2da8 box) | inferred |  |
| `FUN_41f2_0048` | 71050 | 18 | ai | Advance score-report text cursor (2d0e/2d10) + draw line | inferred |  |
| `FUN_41f2_0092` | 71068 | 346 | ai | Nation score compute + optional full report UI | inferred |  |
| `FUN_41f2_0266` | 71414 | 329 | ai | Mid-entry of 0092 colonist-score loop (Ghidra split) | inferred |  |
| `FUN_41f2_0280` | 71743 | 342 | ai | Tribe growth tick from Indian nation turn (4d56_1816) | known | docs/ai_transcription.md |
| `FUN_41f2_0294` | 72085 | 330 | ai | Mid-label of 0092 nation-score/report (NOT the 152e/0038 callee — Ghidra misresolve 2026-08-24; those call `2a1f:0410`→overlay:0, candidate `4d56:0000`) | inferred | docs/ai_port_plan.md T1.15 |
| `FUN_41f2_0b70` | 72415 | 137 | ai | Score→difficulty-scaled gold rebate + treasure dialog | inferred |  |
| `FUN_41f2_0f56` | 72552 | 175 | ai | High-score table load/insert/save + present UI | inferred |  |
| `FUN_41f2_14a8` | 72727 | 72 | ai | End-game score snapshot → rebate(0b70) + HoF(0f56) | inferred |  |

### Segment `4345` (13 defs) — trade — Nation trade flags / FF / Europe-market helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4345_0000` | 72799 | 22 | trade | Set/clear nation bitflag in Europe-record (nation*0x13c) | inferred |  |
| `FUN_4345_003c` | 72821 | 14 | trade | Invert FF-has check | inferred |  |
| `FUN_4345_005a` | 72835 | 15 | trade | Calendar era tier from year thresholds | inferred |  |
| `FUN_4345_0080` | 72850 | 26 | trade | Count unelected FF of category with weight | inferred |  |
| `FUN_4345_00e0` | 72876 | 23 | trade | Count elected FF of category | inferred |  |
| `FUN_4345_0126` | 72899 | 14 | trade | Build FF name string into buffer | inferred |  |
| `FUN_4345_015a` | 72913 | 28 | trade | Pick strongest FF category slot for nation | inferred |  |
| `FUN_4345_01a6` | 72941 | 41 | trade | Enumerate/draw FF entries for nation | inferred |  |
| `FUN_4345_024a` | 72982 | 62 | trade | FF election / announcement UI screen | inferred |  |
| `FUN_4345_0342` | 73044 | 133 | trade | Apply elected founding-father effects | inferred |  |
| `FUN_4345_06d2` | 73177 | 116 | trade | FF congress debate / nominate UI | inferred |  |
| `FUN_4345_0982` | 73293 | 40 | trade | Compute next liberty-bell threshold | inferred |  |
| `FUN_4345_0a22` | 73333 | 144 | trade | Accrue liberty bells; trigger FF election | inferred | turn/nation_ticks_bells_ff.md |

### Segment `43f7` (21 defs) — ai/ui — King/REF/tax/independence + @COUNTRY colors

Thin map: [ai/king_ref.md](ai/king_ref.md). Linux: `src/core/ai_king.c`.

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_43f7_0004` | 73477 | 42 | ai | Pop-weighted nation SoL aggregate over owned colonies | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_0082` | 73519 | 29 | ai | REF/war unit-type id for class+nation (peace vs independence) | inferred | ai/king_ref.md |
| `FUN_43f7_0108` | 73548 | 22 | ai | Eliminate nation: move treasury/relations, scrub units, status=2 | inferred | ai/king_ref.md |
| `FUN_43f7_0188` | 73570 | 31 | ai | Sink nation ships not docked in a colony (types 0x0d–0x12) | inferred | ai/king_ref.md |
| `FUN_43f7_0218` | 73601 | 117 | ai | Crown-nation bootstrap: fold status≠0 Euro into peer; set DS:0x53d2 | inferred | ai/king_ref.md |
| `FUN_43f7_0512` | 73718 | 49 | ai | Purge non-player units at (x,y); capture/surrender msgs | inferred | ai/king_ref.md |
| `FUN_43f7_05ea` | 73767 | 11 | ui | Set DS:0x848[crown nation] @COUNTRY color to 0x0f | known | src/core/turn.c |
| `FUN_43f7_05f4` | 73778 | 14 | ui | @COUNTRY to DS color table | known | src/core/turn.c |
| `FUN_43f7_060a` | 73792 | 37 | ai | Colony garrison/defense score for REF landing target pick | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_06a6` | 73829 | 106 | ai | Crown turn: spawn irregulars near player colonies when REF empty | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_0982` | 73935 | 335 | ai | REF invasion wave: Man-O-War + pool units at target colony | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_10f0` | 74270 | 192 | ai | Foreign-intervention landing (ally expedition near colony) | inferred | ai/king_ref.md (PARKED) |
| `FUN_43f7_1528` | 74462 | 37 | ai | REF arrival announce; set 0x5382 bit1 (force present) | inferred | ai/king_ref.md (PARKED) |
| `FUN_43f7_160a` | 74499 | 207 | ui | Independence rename cinematic: animate new nation name letters | inferred | ai/king_ref.md (PARKED) |
| `FUN_43f7_1a26` | 74706 | 140 | ai | Declare independence: crown setup, wipe other Euros, REF pools, war flag | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_1d42` | 74846 | 64 | ai | Tax→REF funding: grow expeditionary pools + notify | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_1eca` | 74910 | 66 | ai | Promote veterans to Continental Army/Cavalry when colony SoL>50% | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_2022` | 74976 | 98 | ai | Independence-war nation turn: REF grow/land or intervene hire | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_2244` | 75074 | 82 | ui | Human-turn mercenary hire offer | inferred | ai/king_ref.md (UI OPEN) |
| `FUN_43f7_2424` | 75156 | 61 | ai | Nation SoL refresh + threshold chrome / war dispatch | inferred | ai/king_ref.md; src/core/ai_king.c |
| `FUN_43f7_2564` | 75217 | 200 | ui | Declare-independence prompt (SoL≥50% gate + confirm) | inferred | ai/king_ref.md (UI OPEN) |

### Segment `465b` (2 defs) — combat — Move spent / ADD / combat-adjacent

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_465b_0000` | 75417 | 430 | combat | Move spent cost / ADD / post-ADD chrome (combat tails parked) | known | ai/move_spent.c; src/core/ai.c |
| `FUN_465b_0c1e` | 75847 | 23 | combat | Step unit in dir8 via thunk into move_spent_add (465b_0000) | inferred | ai/move_spent.c; ai/indian_nation_turn.c |

### Segment `4720` (3 defs) — ui — Ship embark / naval-move validity + order UI (DS:0x9e4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4720_0006` | 75870 | 70 | ui | Ship cargo free-space / embark probe (type 0xd..0x12 hold left) | inferred |  |
| `FUN_4720_015c` | 75940 | 127 | ui | Naval-move validity check; set DS:0x9e4e reason; return ok | inferred |  |
| `FUN_4720_049e` | 76067 | 463 | ui | Dispatch embark/naval order UI by DS:0x9e4e case (0..8) | inferred |  |

### Segment `478c` (4 defs) — colony — Colonist (type 0x17) / ship unit spawn helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_478c_0002` | 76530 | 19 | colony | Init unit-spawn scratch block at DS:0x5372 | inferred |  |
| `FUN_478c_002c` | 76549 | 23 | colony | Spawn colonist unit (type 0x17, prof 0x2d) | inferred |  |
| `FUN_478c_007e` | 76572 | 22 | colony | Spawn ship unit (type 0 or 0x0d) | inferred |  |
| `FUN_478c_00d0` | 76594 | 23 | colony | Undo last unit spawn if it was colonist 0x17 | inferred |  |

### Segment `479b` (13 defs) — colony — Pioneer clear/plow / goto-colony order bodies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_479b_0000` | 76617 | 56 | colony | Generate/assign next colony name from nation tables | inferred |  |
| `FUN_479b_00ca` | 76673 | 24 | colony | Gold-spend gate for pioneer follow-up (tile bit 0x10) | inferred |  |
| `FUN_479b_0158` | 76697 | 25 | colony | Pioneer tools wear-tick (-20); clear order when depleted | inferred |  |
| `FUN_479b_01a6` | 76722 | 140 | colony | Pioneer clear/plow work-tick; may grant colony food | inferred |  |
| `FUN_479b_0526` | 76862 | 99 | colony | Pioneer road work-tick (layer2 bit 8); may grant lumber | inferred |  |
| `FUN_479b_076e` | 76961 | 91 | colony | Found-colony order body (name, create at unit tile) | inferred |  |
| `FUN_479b_0972` | 77052 | 74 | colony | Unit goto/move order tick; may enter colony | inferred |  |
| `FUN_479b_0b26` | 77126 | 20 | colony | Classify ocean/high-seas tile (codes 0/1/2/5) | inferred |  |
| `FUN_479b_0b6c` | 77146 | 12 | colony | Set unit order=fortified (6); refresh chrome | inferred |  |
| `FUN_479b_0b84` | 77158 | 21 | colony | Unit-at-colony predicate | inferred |  |
| `FUN_479b_0bd0` | 77179 | 164 | colony | Goto-colony order body (path, dock, unload/load) | inferred |  |
| `FUN_479b_0f60` | 77343 | 92 | colony | Unload best cargo from unit into colony stockpile | inferred |  |
| `FUN_479b_11a4` | 77435 | 128 | colony | Load best cargo from colony onto unit | inferred |  |

### Segment `48d3` (9 defs) — ai — Euro landfall goto / unit-order helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_48d3_0002` | 77563 | 29 | ai | Landfall/goto duration roll (1 or 2 turns; docks/colony count) | inferred | turn/europe_finish_bridge.md |
| `FUN_48d3_007a` | 77592 | 44 | ai | Landfall arrival: stack goto/unload setup from active unit tile | inferred | docs/ai_transcription.md |
| `FUN_48d3_015e` | 77636 | 96 | ai | Spiral-find nearest High Seas tile; set sail/goto order | inferred | docs/ai_transcription.md |
| `FUN_48d3_0346` | 77732 | 29 | ai | Retarget stacked units after landfall/colony-goto arrival | inferred | docs/ai_transcription.md |
| `FUN_48d3_03d0` | 77761 | 25 | ai | Tick landfall delay (315a); move/act unit when expired | inferred | turn/europe_finish_bridge.md |
| `FUN_48d3_0434` | 77786 | 24 | ai | Tile OK for HS landfall ship place? (bounds, HS, owner) | inferred | docs/ai_transcription.md |
| `FUN_48d3_048e` | 77810 | 95 | ai | Spiral-place ship on HS near landfall goto (Euro AI) | inferred | src/core/ai.c |
| `FUN_48d3_064e` | 77905 | 38 | ai | For each ship on tile, spiral-place near landfall goto (048e) | inferred | turn/europe_finish_bridge.md |
| `FUN_48d3_06ba` | 77943 | 150 | ai | Europe-exit landfall: tax treasures; focus arriving ship | inferred | turn/europe_exit_landfall.md; docs/ai_transcription.md |

### Segment `4962` (4 defs) — ai — Per-nation unit/colony/cargo census tallies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4962_0006` | 78093 | 18 | ai | Saturating byte add (cap 0xff) for census counters | inferred |  |
| `FUN_4962_0018` | 78111 | 221 | ai | Census units/colonies/cargo tallies for one nation | inferred | turn/census_tally.md |
| `FUN_4962_0606` | 78332 | 46 | ai | Tally nation profession counts from units + colony jobs | inferred | turn/census_tally.md |
| `FUN_4962_06b6` | 78378 | 48 | ai | Indian relation-tick census (tribes/units/goods recount) | known | ai/indian_nation_turn.c; FUN_2a1f_0270 |

### Segment `49dd` (9 defs) — ui — Unit cargo / profession status panels

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_49dd_0000` | 78426 | 22 | ui | Open unit/tile status panel window (281f frame at 0x49dd) | inferred |  |
| `FUN_49dd_0086` | 78448 | 9 | ui | Close/dismiss status panel (281f_00e2) | inferred |  |
| `FUN_49dd_009c` | 78457 | 17 | ui | Append one freeform tip/text line into status panel | inferred |  |
| `FUN_49dd_00f6` | 78474 | 17 | ui | Append indexed tip string from table 0x2db0 into status panel | inferred |  |
| `FUN_49dd_0156` | 78491 | 17 | ui | Draw status-panel footer/help strip (0x2dbe + optional advance) | inferred |  |
| `FUN_49dd_01aa` | 78508 | 59 | ui | Sort and blit unit cargo commodity icons into status panel | inferred |  |
| `FUN_49dd_02d0` | 78567 | 39 | ui | Resolve tile tip string (orders table / colony name / terrain) | inferred |  |
| `FUN_49dd_0386` | 78606 | 41 | ui | Resolve unit profession/job name string (with type specials) | inferred |  |
| `FUN_49dd_0424` | 78647 | 750 | ui | Compose map unit/tile status panel (MP, orders, cargo, colony, tribe) | inferred | ai/euro_dispatcher.c |

### Segment `4b58` (24 defs) — ui — Window / frame widget draw (281f_00ba family)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4b58_0000` | 79397 | 15 | ui | Normalize menu font-height byte (remap 6→5) | inferred |  |
| `FUN_4b58_0016` | 79412 | 15 | ui | Pack 6-word menu style/color record into buffer | inferred | thunk_FUN_2a1f_02de |
| `FUN_4b58_004a` | 79427 | 25 | ui | Menu chrome fill: tiled blit (281f_00c4) if mode=7+tile src, else solid rect (281f_00ba) | inferred | FUN_281f_00ba; FUN_281f_00c4 |
| `FUN_4b58_00ae` | 79452 | 19 | ui | Measure menu label width after stripping '~' hotkey markup | inferred | FUN_281f_0204; thunk_FUN_2a1f_030e |
| `FUN_4b58_0104` | 79471 | 49 | ui | Draw menu label text with ~hotkey color highlight (01f0/01fa) | inferred | FUN_281f_01f0; FUN_281f_01fa; thunk_FUN_2a1f_0332 |
| `FUN_4b58_023e` | 79520 | 45 | ui | Resolve ~hotkey scan/char code from label (~F specials, case-fold via 0x27ed) | inferred | thunk_FUN_2a1f_034a |
| `FUN_4b58_02f6` | 79565 | 43 | ui | Alloc/init menu-bar root (styles from DS:0x149c palette, empty menu list) | inferred | FUN_281f_029a; FUN_2a1f_02d2 |
| `FUN_4b58_043e` | 79608 | 34 | ui | Find menu node by id in bar's linked menu list | inferred | thunk_FUN_2a1f_02f6 |
| `FUN_4b58_0484` | 79642 | 44 | ui | Find menu-item node by command id across all menus | inferred | thunk_FUN_2a1f_0302 |
| `FUN_4b58_051a` | 79686 | 24 | ui | Set/clear menu disabled flag (bit0 at menu+0xc) | inferred | FUN_291f_045c |
| `FUN_4b58_0552` | 79710 | 16 | ui | Set/clear menu-item disabled flag (bit0); used by map ORDERS enable | inferred | FUN_291f_0146; FUN_2b5a |
| `FUN_4b58_0582` | 79726 | 37 | ui | Clear disabled bit on every item under every menu | inferred | FUN_291f_015e |
| `FUN_4b58_05c6` | 79763 | 16 | ui | Set/clear menu-item hidden flag (bit1); used by map ORDERS enable | inferred | FUN_291f_013a; FUN_2b5a |
| `FUN_4b58_05f6` | 79779 | 37 | ui | Clear hidden bit on every item under every menu | inferred | FUN_291f_0152 |
| `FUN_4b58_063a` | 79816 | 77 | ui | Append titled menu node to bar (layout x, hotkey, link into +0x38 list) | inferred | FUN_2a1f_031a; FUN_74a4_0000 |
| `FUN_4b58_07d6` | 79893 | 79 | ui | Append menu-item (label+cmd id) under a menu; empty label → disabled separator | inferred | FUN_2a1f_033e; FUN_74a4_0000 |
| `FUN_4b58_093c` | 79972 | 52 | ui | Draw menu-bar strip + enabled titles (004a fill, text blit); optional 00e2 flush | inferred | FUN_4b58_004a; FUN_281f_00e2; thunk_FUN_281f_0e52 |
| `FUN_4b58_0a64` | 80024 | 56 | ui | Compute pulldown popup layout rects from item count; clamp to 320×200 | inferred | thunk_FUN_2a1f_02ea |
| `FUN_4b58_0b7a` | 80080 | 92 | ui | Draw open pulldown: outline (00ce), fill (004a), item labels/checks, 00e2 flush | inferred | FUN_281f_00ba; FUN_281f_00ce; thunk_FUN_2a1f_0326 |
| `FUN_4b58_0d94` | 80172 | 312 | ui | Modal menu loop: mouse/kbd navigate titles+items, select cmd into bar root | inferred | thunk_FUN_291f_0472; FUN_4b58_0b7a |
| `FUN_4b58_13ac` | 80484 | 43 | ui | Hit-test mouse on menu titles; open matching pulldown via 0d94 | inferred | FUN_291f_047e; FUN_4b58_0d94 |
| `FUN_4b58_144a` | 80527 | 42 | ui | Open pulldown by menu-title hotkey match, then run 0d94 | inferred | FUN_291f_0496; FUN_4b58_0d94 |
| `FUN_4b58_14de` | 80569 | 58 | ui | Find enabled item matching hotkey across menus; return command id | inferred | FUN_291f_048a |
| `FUN_4b58_15a4` | 80627 | 147 | ui | Load menu color/style nibbles from sprite table into DS:0x149c..0x14b8 palette | inferred | FUN_281f_0254; FUN_2a1f_0372 |

### Segment `4cc6` (7 defs) — ai — Indian tribe relations / nearest-village / contact score

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4cc6_0000` | 80774 | 32 | ai | Clear Euro missions on tribes of an Indian nation (alarm) | known | ai/indian_nation_turn.c; FUN_2a1f_0398 |
| `FUN_4cc6_0092` | 80806 | 20 | ai | Set war/alarm bit0x40 vs Indian + clear missions | inferred |  |
| `FUN_4cc6_00f2` | 80826 | 95 | ai | Apply Indian↔Euro relation delta (+dialogs/war roll) | known | ai/indian_nation_turn.c; FUN_281f_0d6c |
| `FUN_4cc6_0304` | 80921 | 23 | ai | Find tribe index at map (x,y) or -1 | inferred |  |
| `FUN_4cc6_0356` | 80944 | 47 | ai | Nearest village to (x,y); optional nation/terrain filter; sets 8db8 | inferred |  |
| `FUN_4cc6_03f8` | 80991 | 200 | ai | Best Euro threat nation+score near a tribe (units/colonies) | inferred |  |
| `FUN_4cc6_07c2` | 81191 | 62 | ai | Indian contact/alarm distance score (cur_tribe + indian_state + difficulty) | inferred |  |

### Segment `4d56` (21 defs) — ai — Indian AI / village growth

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4d56_0000` | — | 56 | ai | Tribe worth/init helper: `(flags&4)?tech+1:2*tech+3` (tech=indian[n-4].tech). Candidate 152e/0038 callee via `2a1f:0410`; not live-confirmed (capital growth contradiction) | known | docs/ai_port_plan.md T1.15; src/core/ai.c `ai_tribe_initial_pop` |
| `FUN_4d56_0038` | 81253 | 39 | ai | Settlement-record CREATE (full-field init); caller is FUN_6a09_0006 only, not a contact-chain helper | confirmed | docs/ai_transcription.md, original_sources_annotated/ai/settlement_record_8d4a.md |
| `FUN_4d56_00e0` | 81292 | 60 | ai | Chains to 01e2 / 14fe | inferred | docs/ai_transcription.md |
| `FUN_4d56_01e2` | 81352 | 19 | ai | Thin wrapper to 14fe | inferred | docs/ai_transcription.md |
| `FUN_4d56_14fe` | 81371 | 16 | ai | Indian unit act / dispatches growth 152e | inferred | ai/indian_nation_turn.c; src/core/ai.c |
| `FUN_4d56_152e` | 81387 | 156 | ai | Village growth accumulator to pop++ | known | ai/indian_nation_turn.c; src/core/ai.c |
| `FUN_4d56_1816` | 81543 | 141 | ai | Indian nation turn (live via thunk 0x1C9A0 → JMPF; forged ret 1930:1554 via 2A02 overlay 0x0C; year-loop FUN_* open) | known | ai/indian_nation_turn.c; turn/mid_pass_indian_rank.md; tools/brave_dump/vr_1554.md; src/core/ai.c |
| `FUN_4d56_1b3a` | 81684 | 59 | ai | Mid-turn: clear 0x5b04 tables, tribe probes, colony ownership — does **not** call 2154 | known | turn/mid_pass_indian_rank.md; ai/indian_contact.md |
| `FUN_4d56_2154` | 81743 | 321 | ai | Meet economics: tribe neighborhood → DS:0x9e* gift/demand tables (from 5bfb_022e) | known | ai/indian_meet_scoring_2154.md; ai/indian_contact.md |
| `FUN_4d56_2820` | 82064 | 222 | ai | Heavy Indian decision / raid-scale logic | inferred | ai/indian_trade_2820.md; indian_trade_helpers.c |
| `FUN_4d56_2aac` | 82286 | 39 | ai | Indian trade dispatch: route selected good → 2e92/2bbc/2b92 or refuse | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_2af6` | 82325 | 29 | ai | Abort trade close: clear tribe last-goods flags, reshuffle demand → 3582 | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_2b92` | 82354 | 222 | ai | Player Indian buy-offer loop: price good, accept/haggle/reject dialog | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_2bbc` | 82576 | 210 | ai | AI/non-human Indian buy-offer loop (same pricing, auto choices) | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_2e92` | 82786 | 53 | ai | Trade no-deal exit (invalid good/capacity) → 311e demand or close | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_2f96` | 82839 | 189 | ai | Trade haggle (choice 2): bump offer/tension, resume buy dialog loop | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_306c` | 83028 | 193 | ai | Trade hard-bargain (choice 3): worse terms + tension, resume loop | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_311e` | 83221 | 239 | ai | Indian counter-demand: pick tribute goods + priced buy-back dialog | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_3582` | 83460 | 21 | ai | Small helper after 2820 | inferred | ai/indian_trade_2820.md |
| `FUN_4d56_359c` | 83481 | 30 | ai | Relation-gated Indian attack on unit: kill / warn / displace by RNG | inferred | docs/ai_transcription.md |
| `FUN_4d56_417e` | 83511 | 188 | ai | Mid-size Indian AI helper | inferred | docs/ai_transcription.md |
| `FUN_4d56_4528` | 83699 | 3073 | ai | Largest Indian cluster (combat/raid-adjacent) | inferred | ai/indian_settlement_4528.md; indian_raid_outcomes.md |

### Segment `521d` (29 defs) — ai — European AI planner

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_521d_0000` | 86772 | 15 | ai | Clear one primary goal-table slot (nation×64) | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_001c` | 86787 | 32 | ai | Invalidate nearby secondary goals matching a code | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0072` | 86819 | 19 | ai | Shift primary goal table down to open a slot | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_00a8` | 86838 | 19 | ai | Shift secondary goal table down to open a slot | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_00de` | 86857 | 18 | ai | Shift work-queue table down to open a slot | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0116` | 86875 | 21 | ai | Max priority among matching primary goals | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_016a` | 86896 | 37 | ai | Upsert primary goal (x,y,code,prio) for nation | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0214` | 86933 | 38 | ai | Upsert secondary goal (x,y,code,prio) for nation | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_02be` | 86971 | 25 | ai | Upsert entry into global AI work queue | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_031c` | 86996 | 16 | ai | Clear global AI work-queue table | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0342` | 87012 | 27 | ai | Clear primary goals; promote secondary→primary (6d8e plan pass) | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_03a6` | 87039 | 19 | ai | Clear all 16 secondary goal slots for nation (0xff) | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_03d0` | 87058 | 40 | ai | Nation founding / expansion urgency score | inferred | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0492` | 87098 | 41 | ai | Colony-count vs target balance flags | inferred | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_052c` | 87139 | 57 | ai | Unit desirability score (type + diplo + founding) | inferred | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0600` | 87196 | 23 | ai | Composite unit priority (052c+0492+03d0) | inferred | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0656` | 87219 | 18 | ai | Walk unit stack/chain to end (cargo scan) | inferred | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_06ae` | 87237 | 82 | ai | Pick best adjacent founding / site tile | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0896` | 87319 | 26 | ai | Filter profession/role by distance and colony wealth | inferred | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0906` | 87345 | 63 | ai | Probe adjacent tiles for contact / claim profession | known | ai/euro_goals.c; ai/euro_dispatcher.c |
| `FUN_521d_0a60` | 87408 | ~5510 (was wrongly `839` — canonical export silently truncated; see `ai/euro_goal_orders_0a60_full.md`) | ai | Euro unit/colony goal writer + goal-consumption/orders engine (sectioned; mid-game OPEN) | known | ai/euro_dispatcher.c; ai/euro_goals.c; ai/euro_g_table_0a60.md; ai/euro_goal_orders_0a60_full.md |
| `FUN_521d_20c6` | 88247 | 19 | ai | Stamp unit orders=0x0B goto with dest (DL,BL)+param | inferred | ai/move_scoring.md; near FUN_521d_20e6 |
| `FUN_521d_20e6` | 88266 | 2180 | ai | Direction / move scoring (all unit kinds); quiet Brave slice annotated | known | ai/quiet_brave_scoring.c; move_scoring.md; move_scoring_land.md; move_scoring_ship.md |
| `FUN_521d_5b66` | 90446 | 44 (was wrongly `1815` — corrupted-blob desync at that citation; real bodies live in `FUN_479b_076e`/`01a6`/`0526`/`0972` at 76722-77122; see `ai/euro_unit_act.md`) | ai | Euro per-unit act dispatcher (cases → `479b_*` handlers, often → move_scoring 20e6); thin map | known | ai/euro_dispatcher.c; ai/euro_unit_act.md |
| `FUN_521d_5c38` | 92261 | 8 | ai | Always-true predicate stub (Europe hire gate) | inferred | ai/euro_dispatcher.c; docs/ai_transcription.md |
| `FUN_521d_5c3c` | 92269 | 47 | ai | Try buy/hire Europe unit if treasury allows | inferred | ai/euro_dispatcher.c; docs/ai_transcription.md |
| `FUN_521d_5cf6` | 92316 | 9 | ai | Refresh colony context pointer (6d8e inventory) | inferred | ai/euro_dispatcher.c; docs/ai_transcription.md |
| `FUN_521d_5d04` | 92325 | 748 | ai | Euro nation planning / hire / treasury (OPEN mid; 6d8e via 0554) | inferred | ai/euro_dispatcher.c; docs/ai_transcription.md |
| `FUN_521d_6d8e` | 93073 | 516 | ai | Euro AI dispatcher per nation (thunk wiring corrected) | known | ai/euro_dispatcher.c; src/core/ai.c |

### Segment `5952` (6 defs) — colony — Colony production / buildings / stock tick (DS:0x8542)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5952_0000` | 93589 | 97 | colony | Maybe spawn wagon-train link between nearby same-nation colonies | inferred |  |
| `FUN_5952_0214` | 93686 | 35 | colony | Gate/set construction project (0x94); clear build-busy on fail | inferred |  |
| `FUN_5952_0280` | 93721 | 28 | colony | Need building upgrade? (cargo stock vs building tier) | inferred |  |
| `FUN_5952_02f4` | 93749 | 11 | colony | Clear build-busy flag; map index → building id (+0x1f) | inferred |  |
| `FUN_5952_0306` | 93760 | 30 | colony | Set/clear colony specialty cargo (0x8d) from stock/market | inferred |  |
| `FUN_5952_035e` | 93790 | 2658 | colony | Colony production/buildings/stock tick on current colony (DS:0x8542) | inferred |  |

### Segment `5bfb` (12 defs) — ai — Indian contact / diplomacy / alarm

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5bfb_0000` | 96448 | 58 | ai | Cargo/treasury census outs for diplomacy (nation filter) | inferred | FUN_2a1f_0634 |
| `FUN_5bfb_00f8` | 96506 | 30 | ai | Rank four Euro nations by strength into order table | inferred | turn/mid_pass_indian_rank.md |
| `FUN_5bfb_0182` | 96536 | 29 | ai | Set diplomacy bit0x40 + human peace/teach dialogs | inferred | FUN_2a1f_0650 |
| `FUN_5bfb_022e` | 96565 | 540 | ai | Indian unit contact/meet body (first contact, gifts, demand) | inferred | FUN_2a1f_066c |
| `FUN_5bfb_102a` | 97105 | 24 | ai | Diplomacy multi-line dialog present (N option pumps) | inferred | FUN_2a1f_0618 |
| `FUN_5bfb_1092` | 97129 | 22 | ai | Diplomacy short 1–2 option dialog present | inferred | FUN_2a1f_0642 |
| `FUN_5bfb_10ec` | 97151 | 63 | ai | Euro A↔B war/ally eligibility by military balance | inferred | FUN_2a1f_067a |
| `FUN_5bfb_12d0` | 97214 | 46 | ai | Clear armed-unit goto/orders adjacent to a colony | inferred | FUN_2a1f_060a |
| `FUN_5bfb_13b0` | 97260 | 61 | ai | Form or break alliance (bit0x40) between two nations | inferred | FUN_2a1f_065e |
| `FUN_5bfb_153e` | 97321 | 1112 | ai | Large diplomacy/war-declaration body (trade/military score) | inferred | FUN_2a1f_05fc |
| `FUN_5bfb_312e` | 98433 | 24 | ai | Unit combat-power factor (type/HP modifiers) | inferred | FUN_2a1f_0626 |
| `FUN_5bfb_3180` | 98457 | 352 | ai | Adjacent ship/unit combat loot resolution around (x,y) | inferred | FUN_2a1f_0192 |

### Segment `5f7a` (3 defs) — trade — Colony native-trade / cargo sell & buy

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5f7a_000e` | 98809 | 76 | trade | Colony native-meet gold dialog (type-5 path; ±100 treasury) | inferred |  |
| `FUN_5f7a_020e` | 98885 | 168 | trade | Native cargo trade: price, pick stock, confirm sell/barter | inferred |  |
| `FUN_5f7a_0662` | 99053 | 58 | trade | Dispatch colony native-trade session (000e vs 020e) | inferred |  |

### Segment `5fef` (11 defs) — combat — Unit/colony combat and Indian raid resolution

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5fef_0000` | 99111 | 98 | combat | Pick best defender unit at tile by combat score walk | inferred |  |
| `FUN_5fef_016c` | 99209 | 83 | combat | Pick cargo slot to plunder (human menu or AI goods-value sort) | inferred | ai/indian_raid_loot.md |
| `FUN_5fef_0352` | 99292 | 416 | combat | Apply combat outcome: capture/convert, type strip, naval loot, destroy | inferred |  |
| `FUN_5fef_0ec0` | 99708 | 30 | combat | Sweep units applying 0352 vs target; compact indices after kills | inferred |  |
| `FUN_5fef_0f14` | 99738 | 302 | combat | Indian raid loot from colony (goods/building/unit/gold) + tension | inferred | ai/indian_raid_loot.md |
| `FUN_5fef_16ea` | 100040 | 24 | combat | Remap specialty id after combat demotion (vet→colonist path) | inferred |  |
| `FUN_5fef_172c` | 100064 | 94 | combat | Post-combat soldier/dragoon specialty or type change (promote/strip) | inferred |  |
| `FUN_5fef_1908` | 100158 | 93 | combat | Treasure capture: ransom dialog, credit gold, remove treasure unit | inferred |  |
| `FUN_5fef_1b0e` | 100251 | 1063 | combat | Main combat engagement: odds, RNG, win/lose tails for move-into | inferred |  |
| `FUN_5fef_31ea` | 101314 | 224 | combat | Nested post-win Indian fallout: treasure spawn, war, tension | inferred |  |
| `FUN_5fef_36fe` | 101538 | 122 | combat | Nested empty-tile combat chrome: clear colony/village UI at xy | inferred |  |

### Segment `636c` (1 defs) — ui — Dual-column compare / report dialog

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_636c_0000` | 101660 | 562 | ui | Dual-column compare/report dialog (measure pass then draw) | inferred |  |

### Segment `647e` (23 defs) — colony — Colony list / select UI (rec*0x4a via DS:0x9e14)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_647e_0000` | 102222 | 13 | colony | Bind colony record pointer (idx*0x4a to DS:0x9e14) | inferred |  |
| `FUN_647e_001a` | 102235 | 15 | colony | Bind colonist-slot pointer (slot*10 to DS:0x9e18) | inferred |  |
| `FUN_647e_0040` | 102250 | 18 | colony | Resolve slot label ptr (nation name or colony name) | inferred |  |
| `FUN_647e_0094` | 102268 | 66 | colony | Gate: unit may join/enter colony | inferred |  |
| `FUN_647e_01c6` | 102334 | 131 | colony | Paginated colony-select dialog for unit | inferred |  |
| `FUN_647e_04f0` | 102465 | 13 | colony | Map building/warehouse index to packed-nibble offset | inferred |  |
| `FUN_647e_0522` | 102478 | 13 | colony | Read packed nibble from colonist-slot byte+2 | inferred |  |
| `FUN_647e_0548` | 102491 | 28 | colony | Write packed nibble to colonist-slot byte+2 | inferred |  |
| `FUN_647e_057a` | 102519 | 18 | colony | Read packed building/cargo nibble | inferred |  |
| `FUN_647e_05aa` | 102537 | 19 | colony | Write packed building/cargo nibble | inferred |  |
| `FUN_647e_05ec` | 102556 | 13 | colony | Set colonist-slot unit-index; clear dual UI counters | inferred |  |
| `FUN_647e_060e` | 102569 | 59 | colony | Remove colonist slot; compact 10-byte slots | inferred |  |
| `FUN_647e_06c2` | 102628 | 55 | colony | Delete colony by idx; unlink units; compact records | inferred |  |
| `FUN_647e_0796` | 102683 | 70 | colony | Colony list picker dialog; returns selected idx | inferred |  |
| `FUN_647e_090a` | 102753 | 40 | colony | Colonist-slot picker dialog for current colony | inferred |  |
| `FUN_647e_09da` | 102793 | 108 | colony | Draw colony report panel (name, status, cargo rows) | inferred |  |
| `FUN_647e_0dd4` | 102901 | 39 | colony | Assign/reassign colonist into selected slot | inferred |  |
| `FUN_647e_0e80` | 102940 | 36 | colony | Cargo/commodity type picker dialog (16 goods) | inferred |  |
| `FUN_647e_0f2c` | 102976 | 63 | colony | Add/remove cargo item in colonist warehouse row | inferred |  |
| `FUN_647e_1064` | 103039 | 29 | colony | Colony-list mouse hit to row/action | inferred |  |
| `FUN_647e_10d2` | 103068 | 31 | colony | Colony-list mouse: rename colony or dismiss | inferred |  |
| `FUN_647e_115c` | 103099 | 58 | colony | Colony view entry: pick/bind colony, input loop | inferred |  |
| `FUN_647e_1486` | 103157 | 242 | colony | Confirm-and-delete colony | inferred |  |

### Segment `65dd` (1 defs) — combat — Unit combat outcome resolution (RNG cases)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_65dd_0004` | 103399 | 362 | map | Lost City Rumour resolve: RNG(1,9) case loop + terrain/session-counter gates → @LOSTCITY{case} / @BURIAL{1-3} / @SCREWED (strings DS:0x1dae/0x1db7/0x1dbe; counters DS:0x1dc6/0x1dc7) | known | src/core/units.c units_lcr_roll_outcome |

### Segment `6662` (7 defs) — ui — Goto pathfinding BFS + path-cost overlay

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6662_0000` | 103761 | 34 | ui | Probe 2×2 for land/sea tile matching mode (281f_0768 / coast bit) | inferred |  |
| `FUN_6662_0086` | 103795 | 46 | ui | Sign(dx,dy) → 8-way direction index (tables 0xb4/0xbe) | inferred | src/core/units.c |
| `FUN_6662_00f2` | 103841 | 309 | ui | Goto BFS over terr_cost with path-cost overlay and Z/Esc confirm | inferred |  |
| `FUN_6662_0906` | 104150 | 52 | ui | Short-range goto cost via 00f2 when both axes <8; else −1 | inferred | src/core/units.c |
| `FUN_6662_09ae` | 104202 | 70 | ui | Pick neighboring ÷4 sector toward land/sea connectivity bitmask | inferred |  |
| `FUN_6662_0b4e` | 104272 | 216 | ui | Coarse sector BFS (15×18) toward goto with path-cost overlay/Z/Esc | inferred |  |
| `FUN_6662_0f74` | 104488 | 316 | ui | Unit goto next-step director: adjacent / near flood / far sector BFS | inferred | src/core/units.c |

### Segment `67bf` (1 defs) — mapgen — Continent flood-fill IDs

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_67bf_0000` | 104804 | 163 | mapgen | Continent flood-fill IDs | known | src/core/map_gen.c |

### Segment `67f4` (2 defs) — mapgen — Coast/neighbor bitmasks + continent tallies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_67f4_0008` | 104967 | 43 | mapgen | Scan 2×2 for ocean-match tile; return continent_id + coords | inferred |  |
| `FUN_67f4_0088` | 105010 | 137 | mapgen | Build 15x18 coast/neighbor bitmasks and per-continent terrain tallies post-flood-fill | inferred |  |

### Segment `682a` (1 defs) — mapgen — Map fertility / bonus value writer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_682a_000c` | 105147 | 87 | mapgen | Write per-tile fertility/bonus nibble across the map | inferred |  |

### Segment `684c` (8 defs) — mapgen — Procedural NEW WORLD map gen

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_684c_0004` | 105234 | 25 | mapgen | Stamp land mask at (x,y) and optional +E/+S neighbours | inferred | src/core/map_gen.c |
| `FUN_684c_009c` | 105259 | 23 | mapgen | Archipelago/normal land-blob wander (diagonal steps) | inferred | src/core/map_gen.c |
| `FUN_684c_0116` | 105282 | 39 | mapgen | Land blobs / form thunk | known | src/core/map_gen.c |
| `FUN_684c_021c` | 105321 | 26 | mapgen | Land blobs / form thunk | known | src/core/map_gen.c |
| `FUN_684c_02a8` | 105347 | 66 | mapgen | Land blobs / form thunk | known | src/core/map_gen.c |
| `FUN_684c_03e4` | 105413 | 25 | mapgen | True if mountain is landlocked (four diagonals non-ocean) | inferred | src/core/map_gen.c |
| `FUN_684c_04a6` | 105438 | 164 | mapgen | Rivers pass — random walks painting river bits | inferred | src/core/map_gen.c |
| `FUN_684c_08c0` | 105602 | 2089 | mapgen | NEW WORLD procedural map entry | known | src/core/map_gen.c |

### Segment `6a09` (1 defs) — ai — Tribe placement

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6a09_0006` | 107691 | 335 | ai | Tribe capitals, satellites, Brave spawn loop | known | src/core/ai.c |

### Segment `6a9f` (8 defs) — mapdraw — Map viewport tile loop

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6a9f_0000` | 108026 | 19 | mapdraw | Return fixed 16×16 map-color sample byte (LUT helper) | inferred |  |
| `FUN_6a9f_0034` | 108045 | 19 | mapdraw | Return map-color sample byte for palette index | inferred |  |
| `FUN_6a9f_0068` | 108064 | 32 | mapdraw | Load terrain→display color LUTs for tile loop / minimap | inferred |  |
| `FUN_6a9f_00d8` | 108096 | 22 | mapdraw | Compute/clamp minimap scroll origin (DS:9ccc/9cca) | inferred |  |
| `FUN_6a9f_0118` | 108118 | 120 | mapdraw | Map viewport tile loop | known | src/core/map.c |
| `FUN_6a9f_0346` | 108238 | 12 | mapdraw | Fill minimap tile-index buffer (56×39 via 0118) | inferred |  |
| `FUN_6a9f_0360` | 108250 | 51 | mapdraw | Redraw clamped minimap sub-rectangle + optional present | inferred |  |
| `FUN_6a9f_0486` | 108301 | 87 | mapdraw | Full minimap chrome + tile fill + present | inferred |  |

### Segment `6afa` (7 defs) — mapdraw — Map viewport clamp / tile blit helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6afa_000c` | 108388 | 31 | mapdraw | Clamp x0,y0,x1,y1 into map viewport tile bounds | inferred |  |
| `FUN_6afa_0052` | 108419 | 45 | mapdraw | Clip (x,y,w,h) into viewport; rewrite w/h (≥0) | inferred |  |
| `FUN_6afa_00c8` | 108464 | 15 | mapdraw | Blit map-buffer tile rect to screen (tile→px) | inferred |  |
| `FUN_6afa_0132` | 108479 | 13 | mapdraw | Blit full map viewport buffer to screen (240×192) | inferred |  |
| `FUN_6afa_0168` | 108492 | 60 | mapdraw | LFSR-order present each viewport tile (dissolve refresh) | inferred |  |
| `FUN_6afa_0224` | 108552 | 11 | mapdraw | Present/flip full map viewport | inferred |  |
| `FUN_6afa_023c` | 108563 | 11 | mapdraw | Present/flip tile-count-sized map region | inferred |  |

### Segment `6b22` (9 defs) — mapdraw — Map viewport tribe/colony/unit overlay blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6b22_0002` | 108574 | 47 | mapdraw | Blit fog-visible tribes in map rect (ICONS overlay) | inferred |  |
| `FUN_6b22_00ea` | 108621 | 12 | mapdraw | Tribe overlay for full map viewport | inferred |  |
| `FUN_6b22_0102` | 108633 | 66 | mapdraw | Blit fog-visible colonies in map rect | inferred |  |
| `FUN_6b22_0248` | 108699 | 12 | mapdraw | Colony overlay for full map viewport | inferred |  |
| `FUN_6b22_034c` | 108711 | 24 | mapdraw | Blit one unit icon unless colony occupies tile | inferred |  |
| `FUN_6b22_03f6` | 108735 | 25 | mapdraw | Toggle/refresh selected-unit overlay blink | inferred |  |
| `FUN_6b22_0428` | 108760 | 28 | mapdraw | Blit unit overlay or clear same-tile blink | inferred |  |
| `FUN_6b22_04bc` | 108788 | 56 | mapdraw | Blit fog-visible on-map units in rect | inferred |  |
| `FUN_6b22_058e` | 108844 | 73 | mapdraw | Unit overlay for full map viewport | inferred |  |

### Segment `6b7e` (5 defs) — mapdraw — Map viewport refresh / camera save-restore

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6b7e_0004` | 108917 | 36 | mapdraw | Refresh map rect: compose, overlays, minimap; optional present | inferred |  |
| `FUN_6b7e_00c0` | 108953 | 38 | mapdraw | Full map viewport refresh + overlays + minimap/chrome | inferred |  |
| `FUN_6b7e_018a` | 108991 | 22 | mapdraw | Refresh tile under selected unit + blink | inferred |  |
| `FUN_6b7e_01f6` | 109013 | 15 | mapdraw | Save camera center/zoom for nation slot (5394) | inferred |  |
| `FUN_6b7e_0218` | 109028 | 15 | mapdraw | Restore camera center/zoom for nation slot (5394) | inferred |  |

### Segment `6ba1` (18 defs) — mapdraw — Map tile neighbor masks / viewport blit helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6ba1_000c` | 109043 | 101 | mapdraw | Compute viewport origin/size/tile-px from zoom+camera; clamp rim | known | src/core/map_panel.c |
| `FUN_6ba1_01b4` | 109144 | 67 | mapdraw | Build ocean/coast 8-neighbor edge mask for tile compose | inferred |  |
| `FUN_6ba1_0314` | 109211 | 32 | mapdraw | Build NSEW neighbor presence mask from map plane | inferred |  |
| `FUN_6ba1_0374` | 109243 | 32 | mapdraw | NSEW hill/mountain neighbor mask (byte&0xa0 match) | inferred |  |
| `FUN_6ba1_03e4` | 109275 | 19 | mapdraw | True if neighbor is connective forest (non-scrub) | inferred |  |
| `FUN_6ba1_041e` | 109294 | 33 | mapdraw | NSEW connective-forest neighbor mask via 03e4 | inferred |  |
| `FUN_6ba1_0484` | 109327 | 32 | mapdraw | NSEW neighbor bit-presence mask from layer2 plane | inferred |  |
| `FUN_6ba1_04e4` | 109359 | 35 | mapdraw | 8-neighbor bit-presence mask from layer2 plane | inferred |  |
| `FUN_6ba1_0558` | 109394 | 15 | mapdraw | Blit current map-tile terrain sprite (soft RLE vs scaled by DS:0x186) | inferred |  |
| `FUN_6ba1_05b8` | 109409 | 15 | mapdraw | Blit base terrain cell (zoom0 sheet vs scaled) | inferred |  |
| `FUN_6ba1_061c` | 109424 | 17 | mapdraw | Blit terrain/PHYS overlay sprite at tile pixel | inferred |  |
| `FUN_6ba1_067c` | 109441 | 15 | mapdraw | Blit current map-tile glyph/bitmap overlay (mode by DS:0x184) | inferred |  |
| `FUN_6ba1_06e0` | 109456 | 107 | mapdraw | Blit diagonal corner/edge terrain transitions | inferred |  |
| `FUN_6ba1_0938` | 109563 | 189 | mapdraw | Compose/draw one viewport map tile (terrain, features, overlays) | inferred |  |
| `FUN_6ba1_0d6c` | 109752 | 145 | mapdraw | Viewport tile-compose loop (fog gate; per-tile 0938) | inferred |  |
| `FUN_6ba1_1028` | 109897 | 29 | mapdraw | Optional letterbox fill then viewport tile blit (0d6c) | inferred |  |
| `FUN_6ba1_10ae` | 109926 | 14 | mapdraw | Temp DS:0x18a radius mode; refresh via 1028; clear | inferred |  |
| `FUN_6ba1_10be` | 109940 | 50 | mapdraw | set_owner_nibble 0xf on every tile (unowned init) | known | src/core/ai.c; ai/accessors.c |

### Segment `6cb2` (21 defs) — ui — Info / dialog text panels (281f compositor)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6cb2_0000` | 109990 | 31 | ui | Free Colonizopedia index triple-buffers (DS:1ea6/1eaa/1eae) via 291f_01a8 | inferred | thunk_FUN_2a1f_0a32 |
| `FUN_6cb2_0058` | 110021 | 37 | ui | Alloc index triple-buffers (word titles + type/idx bytes); clear a5aa | inferred | thunk_FUN_2a1f_0a40; FUN_281f_029a |
| `FUN_6cb2_00c0` | 110058 | 18 | ui | Append one encyclopedia list entry (title ptr, cat type, index) | inferred |  |
| `FUN_6cb2_00fc` | 110076 | 30 | ui | Swap two encyclopedia list entries across the triple arrays | inferred |  |
| `FUN_6cb2_0178` | 110106 | 63 | ui | Bubble-sort encyclopedia list by title string (1d1d_103e) | inferred | thunk_FUN_2a1f_09de |
| `FUN_6cb2_0276` | 110169 | 21 | ui | Map list index → 3-col grid xy (24/page, scroll via a5ac) | inferred | thunk_FUN_2a1f_0a16 |
| `FUN_6cb2_02c4` | 110190 | 40 | ui | Hit-test cursor vs list grid; −2/−3 = Exit / page-chrome | inferred | thunk_FUN_2a1f_0a24 |
| `FUN_6cb2_033a` | 110230 | 22 | ui | Format list-row title; terrain 8–15 append forest suffix (2db0) | inferred | thunk_FUN_2a1f_0a4e |
| `FUN_6cb2_039c` | 110252 | 10 | ui | Init article title paint (colors + format into 833c) | inferred | thunk_FUN_2a1f_0998; FUN_281f_0422 |
| `FUN_6cb2_03bc` | 110262 | 24 | ui | Flush/finish article compositor (flag 1f56/0x20, restore clip) | inferred | thunk_FUN_2a1f_09c2; FUN_281f_0444 |
| `FUN_6cb2_0424` | 110286 | 22 | ui | Begin article compositor (optional side-art 1ec4 + 200px box) | inferred | thunk_FUN_2a1f_09d0; FUN_291f_087a |
| `FUN_6cb2_048c` | 110308 | 44 | ui | Draw related sprite strip + name (job/cargo icon row) | inferred | thunk_FUN_2a1f_09fa |
| `FUN_6cb2_05ce` | 110352 | 99 | ui | Build Colonizopedia colonist-skill article (JOB + related icons) | inferred | FUN_291f_0934; feeds 6f74 subst via 281f_0438 |
| `FUN_6cb2_07e6` | 110451 | 186 | ui | Build Colonizopedia unit-type article (5230 table + cargo sprites) | inferred | FUN_291f_0942 |
| `FUN_6cb2_0eac` | 110637 | 273 | ui | Build Colonizopedia terrain article (3×3 tile preview + yields) | inferred | FUN_291f_0428; docs/assets.md Colonizopedia |
| `FUN_6cb2_1820` | 110910 | 113 | ui | Build Colonizopedia cargo/goods article (−715e + building chain) | inferred | FUN_291f_08de |
| `FUN_6cb2_1ba8` | 111023 | 107 | ui | Build Colonizopedia colony-building article (−707e + parent/good) | inferred | FUN_291f_0902 |
| `FUN_6cb2_1f28` | 111130 | 39 | ui | Build Colonizopedia founding-father article (−69ae title+body) | inferred | FUN_2a1f_0062 |
| `FUN_6cb2_214a` | 111169 | 109 | ui | Paint encyclopedia list (3-col titles, highlight, Exit chrome) | inferred | thunk_FUN_2a1f_09ec; WOODPANL list UI |
| `FUN_6cb2_2322` | 111278 | 1252 | ui | Fill encyclopedia index for one category 0–6 (or via 00c0) | inferred | thunk_FUN_2a1f_0a08; Ghidra switch overlapped |
| `FUN_6cb2_24b8` | 112530 | 2067 | ui | Colonizopedia browser: fill/sort/list loop → article by cat type | inferred | docs/assets.md Colonizopedia; cat7=all |

### Segment `6f30` (3 defs) — ui — Splash / image load+blit via resource stream

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6f30_0004` | 114597 | 12 | ui | Fill splash rect (DS:0x2da8 box) then video flush 00e2 | inferred |  |
| `FUN_6f30_002e` | 114609 | 14 | ui | Blit one splash sprite frame (width from obj+0x48/−0x4c) | inferred |  |
| `FUN_6f30_0062` | 114623 | 147 | ui | Splash/image load+blit via resource stream (frames, caption, palette) | inferred |  |

### Segment `6f74` (58 defs) — ui — Text layout / flow-wrap dialog compositor (incl. FUN_6f74_1198)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6f74_0000` | 114770 | 20 | ui | Load side-art image into dialog+0x6c/6e and strcpy-init buffer | inferred |  |
| `FUN_6f74_0042` | 114790 | 35 | ui | Bind primary side-art (DS:0x1f5c style) then load via 0000 | inferred |  |
| `FUN_6f74_00c2` | 114825 | 15 | ui | Bind secondary side-art (DS:0x1f5e offset) then load via 0000 | inferred |  |
| `FUN_6f74_00ec` | 114840 | 15 | ui | Bind tertiary side-art (DS:0x1f60 offset) then load via 0000 | inferred |  |
| `FUN_6f74_033c` | 114855 | 31 | ui | Compositor glyph/blit dispatch (mode-7 side-art vs text paint) | inferred |  |
| `FUN_6f74_03d0` | 114886 | 9 | ui | Prefetch dialog string-table slot into subst buffer (idx×0x40−0x632e) | inferred | FUN_2b5a_0000; FUN_281f_0416 |
| `FUN_6f74_03ec` | 114895 | 12 | ui | Set dialog subst string slot (strcpy from string-table ptr) | inferred | FUN_281f_0438 |
| `FUN_6f74_0404` | 114907 | 13 | ui | Format nation name into dialog subst string slot | inferred | FUN_281f_042e→15b3_0144 |
| `FUN_6f74_042c` | 114920 | 13 | ui | Set dialog numeric subst far-ptr slot (idx×4−0x6350) | inferred | FUN_281f_09ae |
| `FUN_6f74_0446` | 114933 | 23 | ui | Pack 8-word dialog paint/state record (xy/size/colors) | inferred |  |
| `FUN_6f74_04f6` | 114956 | 27 | ui | Apply dialog text color/style from ctx field+10 | inferred |  |
| `FUN_6f74_0538` | 114983 | 66 | ui | Measure markup string width ({bold}//end/~glyph) | inferred |  |
| `FUN_6f74_0642` | 115049 | 33 | ui | Resolve ~hotkey glyph code from option label (~F / case) | inferred |  |
| `FUN_6f74_06d0` | 115082 | 100 | ui | Alloc+init dialog box record from flags/script attrs | inferred | FUN_291f_023c |
| `FUN_6f74_089a` | 115182 | 36 | ui | Find option/button node by id in dialog option list | inferred | thunk_FUN_2a1f_0a9e |
| `FUN_6f74_08fa` | 115218 | 17 | ui | Set/clear option disabled bit0 | inferred | FUN_291f_01b6 |
| `FUN_6f74_092a` | 115235 | 16 | ui | Set/clear option flag bit1 | inferred |  |
| `FUN_6f74_095a` | 115251 | 25 | ui | Clear disabled bit0 on all options in list | inferred |  |
| `FUN_6f74_098a` | 115276 | 17 | ui | Read option checkbox/toggle state (field+6) | inferred | thunk_FUN_2a1f_0ac2 |
| `FUN_6f74_09ba` | 115293 | 14 | ui | Write option checkbox/toggle state (field+6) | inferred | thunk_FUN_2a1f_0ada |
| `FUN_6f74_09e2` | 115307 | 13 | ui | Set current selected option ptr (dialog+0x4c/4e) | inferred |  |
| `FUN_6f74_0a00` | 115320 | 99 | ui | Append labeled option/button node to dialog option list | inferred | FUN_291f_0176 |
| `FUN_6f74_0be8` | 115419 | 20 | ui | OR option-flags/5 and append hotkey option via 0a00 | inferred |  |
| `FUN_6f74_0c22` | 115439 | 9 | ui | Set dialog body wrap-width (field+0x28) | inferred | FUN_291f_08d2 |
| `FUN_6f74_0c32` | 115448 | 74 | ui | Append body text line (^center / ^^ flags) to flow list | inferred | FUN_291f_08c6; FUN_6f74_1198 |
| `FUN_6f74_0d44` | 115522 | 86 | ui | Append edit-field node (label + buffered input) | inferred | thunk_FUN_2a1f_0ace |
| `FUN_6f74_0f16` | 115608 | 16 | ui | Font line-height (remap type6→5 when no side-art) | inferred |  |
| `FUN_6f74_0f3c` | 115624 | 103 | ui | Append icon/image row node to dialog icon list | inferred |  |
| `FUN_6f74_112a` | 115727 | 20 | ui | Alloc custom hit-target node at dialog+0x64 | inferred |  |
| `FUN_6f74_116c` | 115747 | 9 | ui | Blit string with dialog color pair (7b29_49e8 @+0x74) | inferred | FUN_6f74_1198 |
| `FUN_6f74_1198` | 115756 | 126 | ui | Flow-wrap text layout for dialog / wood-panel body | known | src/core/new_game.c; docs/assets.md |
| `FUN_6f74_14c6` | 115882 | 267 | ui | Compute dialog geometry: sizes, centering, side-art place | inferred | FUN_6f74_1198 |
| `FUN_6f74_1a3c` | 116149 | 17 | ui | Blit dialog outer frame/border (unless suppressed) | inferred |  |
| `FUN_6f74_1a78` | 116166 | 29 | ui | Draw corner help-‘?’ tip when DS:0x1f66 set | inferred | thunk_FUN_2a1f_0b0a |
| `FUN_6f74_1ae8` | 116195 | 26 | ui | Blit dialog side-panel art (dialog+0x68) | inferred | thunk_FUN_2a1f_0ab6 |
| `FUN_6f74_1b7c` | 116221 | 86 | ui | Paint option/button column (highlight + labels) | inferred |  |
| `FUN_6f74_1e14` | 116307 | 84 | ui | Paint edit-field column (boxes + buffered text) | inferred |  |
| `FUN_6f74_201e` | 116391 | 78 | ui | Paint icon/image rows (optional selection chrome) | inferred |  |
| `FUN_6f74_2278` | 116469 | 50 | ui | Draw dialog chrome fill + border bevels (2da8 box) | inferred | thunk_FUN_2a1f_0710 |
| `FUN_6f74_248e` | 116519 | 40 | ui | Full paint pass: layout→frame→icons→flow→options→edits | inferred | thunk_FUN_2a1f_0af2 |
| `FUN_6f74_255e` | 116559 | 16 | ui | Select icon row (+0x50) and redraw icon column | inferred | thunk_FUN_2a1f_0aaa |
| `FUN_6f74_2580` | 116575 | 567 | ui | Modal dialog input loop (mouse/keys); return choice id | inferred | FUN_291f_016a |
| `FUN_6f74_3084` | 117142 | 13 | ui | Set dialog script resource triple (DS:0x1f9e/a0/a2) | inferred |  |
| `FUN_6f74_309c` | 117155 | 92 | ui | Expand %STRING/%NUMBER/%HEX/%GOLD/%YEAR placeholders | inferred | FUN_291f_0910 |
| `FUN_6f74_32a4` | 117247 | 191 | ui | Parse @-directive dialog script into box (title/opts/attrs) | inferred | FUN_291f_0182 |
| `FUN_6f74_36ca` | 117438 | 19 | ui | Parse+run+free dialog (32a4→2580→free); return choice | inferred | FUN_281f_0998 |
| `FUN_6f74_36fc` | 117457 | 11 | ui | Clear dialog checkbox bitmask (DS:0x1f54) | inferred |  |
| `FUN_6f74_3704` | 117468 | 17 | ui | Set/clear one bit in dialog checkbox mask (DS:0x1f54) | inferred |  |
| `FUN_6f74_372e` | 117485 | 11 | ui | Test one bit in dialog checkbox mask (DS:0x1f54) | inferred |  |
| `FUN_6f74_3744` | 117496 | 9 | ui | Dialog flush/run wrapper →36ca via 281f_0998 | inferred | FUN_281f_03fe |
| `FUN_6f74_3760` | 117505 | 12 | ui | Set side-art style DS:0x1f5c then flush/run dialog | inferred | FUN_291f_019c |
| `FUN_6f74_378a` | 117517 | 12 | ui | Set side-art style=8 then flush/run dialog | inferred |  |
| `FUN_6f74_37a2` | 117529 | 12 | ui | Set secondary side-art DS:0x1f5e then flush/run | inferred |  |
| `FUN_6f74_37cc` | 117541 | 12 | ui | Set tertiary side-art DS:0x1f60 then flush/run | inferred |  |
| `FUN_6f74_37f6` | 117553 | 11 | ui | OR dialog default-flags 0x18 into DS:0x1f56 | inferred | FUN_281f_040a |
| `FUN_6f74_37fc` | 117564 | 25 | ui | Tip/help dialog variant (DS:0x2008 mode) via 32a4+2580 | inferred | FUN_291f_0120 |
| `FUN_6f74_3848` | 117589 | 16 | ui | Number-entry dialog helper; stash parsed value at DS:0x9cc8 | inferred |  |
| `FUN_6f74_388a` | 117605 | 219 | ui | Load dialog chrome/palette colors into DS:0x1f3c..0x1f4e | inferred |  |

### Segment `7314` (9 defs) — platform — Config/name file line parse (comma fields)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7314_0000` | 117824 | 14 | platform | Close config/name file handle at DS:0x2014 if open | inferred |  |
| `FUN_7314_001a` | 117838 | 66 | platform | Open config/name file; seek @section; scan lines for tag match | inferred |  |
| `FUN_7314_0106` | 117904 | 27 | platform | Read next config line to DS:0x833c; '_'→space; set field cursor 0xa608 | inferred |  |
| `FUN_7314_015e` | 117931 | 28 | platform | Extract next comma field into DS:0xa5b8; advance cursor 0xa608 | inferred |  |
| `FUN_7314_0198` | 117959 | 10 | platform | End current config field parse (291f_0fc4 + 2a1f_0b3a) | inferred |  |
| `FUN_7314_01b6` | 117969 | 10 | platform | Blit/print full config line buffer DS:0x833c | inferred |  |
| `FUN_7314_01c8` | 117979 | 10 | platform | Blit/print current comma field DS:0xa5b8 | inferred |  |
| `FUN_7314_01da` | 117989 | 20 | platform | Parse '0'/'1' bit string in field 0xa5b8 to byte | inferred |  |
| `FUN_7314_0208` | 118009 | 64 | platform | Load nth string from dual-id resource (skip n lines) | inferred |  |

### Segment `733a` (12 defs) — ui — New-game / CUSTOMIZE UI

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_733a_0000` | 118073 | 19 | ui | CUSTOMIZE / difficulty-style UI entry | known | src/core/new_game.c |
| `FUN_733a_002c` | 118092 | 47 | ui | Draw one CUSTOMIZE cell; highlight + category/value labels if selected | inferred | thunk_FUN_2a1f_0b9e; FUN_733a_0512 sibling |
| `FUN_733a_01a4` | 118139 | 34 | ui | Paint CUSTOMIZE chrome (title + 4×3 cell redraw) | inferred | thunk_FUN_2a1f_0bba; FUN_733a_0270 |
| `FUN_733a_0270` | 118173 | 138 | ui | CUSTOMIZE / new-game UI helper | known | src/core/new_game.c |
| `FUN_733a_04d0` | 118311 | 21 | ui | Map difficulty index → DIFFICUL.PIK cell origin | inferred | thunk_FUN_2a1f_0b90; FUN_733a_0000 sibling |
| `FUN_733a_0512` | 118332 | 58 | ui | CUSTOMIZE / new-game UI helper | known | src/core/new_game.c |
| `FUN_733a_06a4` | 118390 | 41 | ui | Paint difficulty chrome (title + five DIFFICUL cells) | inferred | thunk_FUN_2a1f_0bf2; FUN_733a_0790 |
| `FUN_733a_0790` | 118431 | 111 | ui | Difficulty-select modal event loop (keys/mouse → DS:0x53a6) | inferred | FUN_733a_0270 sibling; src/core/new_game.c |
| `FUN_733a_0992` | 118542 | 12 | ui | Map nation index → NATIONS.PIK 2×2 cell origin | inferred | thunk_FUN_2a1f_0bc8; FUN_733a_0000 sibling |
| `FUN_733a_09c6` | 118554 | 47 | ui | Draw one nation-pick cell; highlight + name/bonus if selected | inferred | thunk_FUN_2a1f_0bd6; FUN_733a_0512 sibling |
| `FUN_733a_0b3e` | 118601 | 41 | ui | Paint nation-pick chrome (title + four NATIONS cells) | inferred | thunk_FUN_2a1f_0b58; FUN_733a_0c2a |
| `FUN_733a_0c2a` | 118642 | 215 | ui | Nation-select modal event loop (keys/mouse → DS:0x5398) | inferred | FUN_733a_0270 sibling; src/core/new_game.c |

### Segment `7421` (2 defs) — platform — Startup config fread + argv parse

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7421_0188` | 118857 | 27 | platform | Startup config fread: open + seven word reads into DS:0x260a..0x2616 | inferred |  |
| `FUN_7421_025a` | 118884 | 66 | platform | Startup argv parse (-opts); set DS:0x26e6 skip-XMS then run init | inferred |  |

### Segment `7455` (7 defs) — mapgen — Map plane buffer alloc (pitch 0x853a → terrain/L2/L3)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7455_0000` | 118950 | 33 | mapgen | Mirror pitch/height/plane far-ptrs into mapgen scratch | inferred |  |
| `FUN_7455_0058` | 118983 | 58 | mapgen | Allocate terrain/layer2/layer3 map planes from pitch×height | inferred |  |
| `FUN_7455_0122` | 119041 | 20 | mapgen | Set large-map compact flag DS:0x15a from plane budget | inferred |  |
| `FUN_7455_0166` | 119061 | 65 | mapgen | Load map planes from file (dims + terrain/L2/L3) | inferred |  |
| `FUN_7455_02a6` | 119126 | 58 | mapgen | Save map planes to file | inferred |  |
| `FUN_7455_03b0` | 119184 | 28 | mapgen | Set dims; memset terrain=ocean, layer2/3=0 | inferred |  |
| `FUN_7455_0434` | 119212 | 70 | mapgen | Bootstrap map dims/buffers (default 120×75 or from file) | inferred |  |

### Segment `74a4` (3 defs) — ui — Map menu bar load from MENU.TXT (4b58 widgets)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_74a4_0000` | 119282 | 236 | ui | Build map menu bar titles+items from MENU.TXT (4b58 widgets) | inferred |  |
| `FUN_74a4_0b0a` | 119518 | 39 | ui | Load 12 menu/cursor sprites from resource into glyph scratch | inferred |  |
| `FUN_74a4_0bbe` | 119557 | 26 | ui | Load menu-cursor resource handle into DS:0x16c/0x16e (via 0b0a) | inferred |  |

### Segment `7562` (5 defs) — save — COLONY## slot path / list / Save(0-7) / Load(0-9) / autosave

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7562_0008` | 119583 | 11 | save | Build COLONY## path strings for a slot index into DS basename buffers | known | docs/savegame.md |
| `FUN_7562_0034` | 119594 | 12 | save | Direct slot write (path + 75c2_0288); autosave 8/9 and fixed-slot saves | known | docs/savegame.md |
| `FUN_7562_0052` | 119606 | 111 | save | Slot-list builder: probe slots, format Empty/leader+year rows for Save/Load UI | known | docs/savegame.md |
| `FUN_7562_030a` | 119717 | 65 | save | Manual Save slot picker (slots 0-7) then write | known | docs/savegame.md |
| `FUN_7562_04e8` | 119782 | 69 | save | Manual Load slot picker (slots 0-9, skip empty) then load | known | docs/savegame.md |

### Segment `75c2` (20 defs) — save — Savegame R/W of units / colonies / tribes / flags

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_75c2_0000` | 119851 | 130 | save | Paginated directory name picker (10/page, prev/next) | inferred |  |
| `FUN_75c2_0204` | 119981 | 24 | save | Copy options bytes DS:0x830-0x839 into live option/UI words | known |  |
| `FUN_75c2_024c` | 120005 | 22 | save | Hardcode default option/UI word values (counterpart to 0204) | known |  |
| `FUN_75c2_0288` | 120027 | 94 | save | Savegame write: flags/units/colonies/tribes blobs | known | docs/savegame.md |
| `FUN_75c2_0840` | 120121 | 67 | save | Prefix/header probe (COLONIZE sig, version, map WxH); slot error codes | known | docs/savegame.md |
| `FUN_75c2_0940` | 120188 | 356 | save | Savegame load counterpart to FUN_75c2_0288 | known | docs/savegame.md |
| `FUN_75c2_10ae` | 120544 | 160 | save | New-game nation/difficulty setup; mark human nation(s) in player records | inferred |  |
| `FUN_75c2_1380` | 120704 | 31 | save | Fill one 16-byte unit-type/stat table row from data stream | inferred |  |
| `FUN_75c2_13dc` | 120735 | 20 | save | Write one 12-byte building-link table record (used by 144c) | inferred |  |
| `FUN_75c2_1418` | 120755 | 19 | save | Write one 6-byte companion table record (used by 144c) | inferred |  |
| `FUN_75c2_144c` | 120774 | 56 | save | Init building/prereq link tables via 13dc/1418 | inferred |  |
| `FUN_75c2_1770` | 120830 | 519 | save | Bootstrap static catalogs (names, cargos, buildings, default options) | inferred |  |
| `FUN_75c2_20e2` | 121349 | 123 | save | Endgame/victory announce dialog; independence continue flag | inferred |  |
| `FUN_75c2_2324` | 121472 | 22 | save | Thin new-game flourish wrapper that calls 20e2 | inferred |  |
| `FUN_75c2_235c` | 121494 | 191 | save | New-game/world bootstrap: clear header, default map 58x72, spawn units | known | docs/savegame.md |
| `FUN_75c2_2758` | 121685 | 14 | save | After video restore, reapply options via 0204 | inferred |  |
| `FUN_75c2_276e` | 121699 | 10 | save | Reset options to defaults via 024c | inferred |  |
| `FUN_75c2_2778` | 121709 | 245 | save | Title/main menu loop (New/Load/Options); Load → 7562_04e8 | inferred | docs/savegame.md |
| `FUN_75c2_2d28` | 121954 | 14 | save | 768-byte palette snapshot save/restore pair | inferred |  |
| `FUN_75c2_2d46` | 121968 | 369 | save | Game boot: video/memory/asset init before main loop | inferred |  |

### Segment `78d8` (4 defs) — platform — Resource stream buffer alloc / cursor / far-ptr load

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_78d8_0000` | 122337 | 17 | platform | Reset resource-stream cursor/remain from base buffer DS:0x23c6 | inferred |  |
| `FUN_78d8_0022` | 122354 | 21 | platform | Alloc resource-stream far buffer (281f_029a) then reset via 0000 | inferred |  |
| `FUN_78d8_0054` | 122375 | 38 | platform | Advance stream: load via 78ef then bump cursor / shrink remain | inferred |  |
| `FUN_78d8_00c4` | 122413 | 63 | platform | Load/reload resource far-ptrs from stream; fatal after 3 reloads | inferred |  |

### Segment `78ef` (1 defs) — platform — Resource archive open (RM* / ext) + stream setup

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_78ef_0002` | 122476 | 245 | platform | Open RM* resource archive (strip ext), alloc stream, set read budgets | inferred |  |

### Segment `7939` (1 defs) — platform — Resource open helper (path + close)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7939_000c` | 122721 | 41 | platform | Resource open helper: path+prefix 0x23fa, open/read chunks, close | inferred |  |

### Segment `7944` (1 defs) — platform — Resource open helper (sibling)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7944_000e` | 122762 | 41 | platform | Resource open helper sibling: path+prefix 0x2402, extra chunk read | inferred |  |

### Segment `7952` (1 defs) — platform — Resource open / alloc path

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7952_0000` | 122803 | 70 | platform | Resource open/alloc path: fopen, heap alloc, one-chunk load; return buf | inferred |  |

### Segment `7962` (3 defs) — platform — Resource file open / close handle helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7962_0000` | 122873 | 97 | platform | Open resource file into handle struct (read-header vs write-create paths) | inferred |  |
| `FUN_7962_020a` | 122970 | 11 | platform | Store codec/type byte at resource-handle +0x2b | inferred |  |
| `FUN_7962_021c` | 122981 | 39 | platform | Close resource handle (optional rewind seek + fclose); clear open flag | inferred |  |

### Segment `798d` (1 defs) — platform — Compressed resource chunk read

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_798d_0000` | 123020 | 112 | platform | Compressed resource chunk read (seek/size; wire 79a8_004a consume) | inferred |  |

### Segment `79a8` (4 defs) — platform — Compressed resource stream I/O + progress callback

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_79a8_000a` | 123132 | 8 | platform | Return near offset of far buffer ptr (stream-callback wire helper) | inferred |  |
| `FUN_79a8_0014` | 123140 | 14 | platform | Invoke progress callback at DS:0x245e if installed | inferred |  |
| `FUN_79a8_002a` | 123154 | 15 | platform | Install stream heap buffer + progress callback (DS:0x245a..0x2460) | inferred |  |
| `FUN_79a8_004a` | 123169 | 190 | platform | Wire compressed stream I/O (callbacks + heap buffer + consume loop) | inferred |  |

### Segment `79db` (1 defs) — platform — Chunked DOS file read into far buffer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_79db_000c` | 123359 | 70 | platform | Chunked DOS file write from far buffer (AH=40, ≤0xF000 slices) | inferred |  |

### Segment `79ec` (2 defs) — platform — Resource stream progress pump / dispatch

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_79ec_0004` | 123429 | 38 | platform | Resource-stream progress pump: src→dst callbacks until remain drained | inferred |  |
| `FUN_79ec_0082` | 123467 | 58 | platform | Dispatch compress/decompress codec by mode (DS:0x26cc..0x26e0 table) | inferred |  |

### Segment `7a05` (5 defs) — platform — Fatal/abort error text + INT10 video reset

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a05_0000` | 123525 | 38 | platform | Load error-text line N from resource into dest (strip ctrls) | inferred |  |
| `FUN_7a05_00a4` | 123563 | 45 | platform | Dump resource lines until "$…" match at DS:0x24cb (print each) | inferred |  |
| `FUN_7a05_014e` | 123608 | 20 | platform | Fatal video teardown then INT10 mode reset | inferred |  |
| `FUN_7a05_0180` | 123628 | 71 | platform | Format/print fatal abort text + hooks; exit via 1d1d_030d(3) | inferred |  |
| `FUN_7a05_03ce` | 123699 | 49 | platform | If size≥DS:0x2476: format addrs, load msgs (0000), abort (0180) | inferred |  |

### Segment `7a4c` (2 defs) — ui — VGA DAC write + PIT fade-speed calibrate

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a4c_0000` | 123748 | 45 | ui | Write VGA DAC RGB range (0x3c8/0x3c9); return PIT ticks elapsed | inferred |  |
| `FUN_7a4c_006a` | 123793 | 88 | ui | Calibrate fade speed (vsync+PIT → DS:0x802/804/806; arm 0x800) | inferred |  |

### Segment `7a65` (3 defs) — ui — Map tip blit + parameterized dialog text (6f74)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a65_0008` | 123881 | 38 | ui | Label viewport tile tip if in-camera (hidden-terrain chrome) | inferred |  |
| `FUN_7a65_00e2` | 123919 | 19 | ui | Prefetch dialog id + store 3 numeric substs (DS:0x9cb0) then run | inferred |  |
| `FUN_7a65_0124` | 123938 | 29 | ui | fopen path-pair (by DS:0x25f0) + fprintf 4 args (fmt 0x25df) + fclose | inferred |  |

### Segment `7a7c` (1 defs) — ui — Load 768-byte VGA palette from resource

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a7c_000e` | 123967 | 26 | ui | Load 768-byte VGA palette from resource (name DS:0x25f2) into dest | inferred |  |

### Segment `7a83` (2 defs) — ui — Palette RGB fade / channel shift

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a83_0002` | 123993 | 35 | ui | Right-shift each of 768 palette RGB bytes by N (dim channels) | inferred |  |
| `FUN_7a83_002a` | 124028 | 84 | ui | Fade palette toward target RGB (step until match; timer gate) | inferred |  |

### Segment `7a9d` (1 defs) — ui — Dialog string buffer prep (0x929e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a9d_0004` | 124112 | 17 | ui | Load 0x22-byte dialog string prep buffer into DS:0x929e | inferred |  |

### Segment `7aa1` (2 defs) — ui — Parameterized dialog / message box (# subst)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7aa1_0002` | 124129 | 19 | ui | Dismiss message-box handle DS:0x2604 (sound/abort teardown) | inferred |  |
| `FUN_7aa1_003a` | 124148 | 59 | ui | Message box: #→char subst, create+run modal; store handle 0x2604 | inferred |  |

### Segment `7ab3` (1 defs) — ui — VGA vsync + DAC palette read

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ab3_0008` | 124207 | 44 | ui | Wait VGA retrace then read DAC RGB palette via 0x3c7/0x3c9 | known |  |

### Segment `7ab9` (2 defs) — ui — Image/resource load + blit error codes

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ab9_0000` | 124251 | 38 | ui | Image/resource load attempt; return status (-1 blit / -3 / -10-n) | inferred |  |
| `FUN_7ab9_00be` | 124289 | 32 | ui | Handle image-load status: temp-file write, blit, or free buffer | inferred |  |

### Segment `7acf` (2 defs) — platform — Far-buffer alloc into handle struct

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7acf_0002` | 124321 | 24 | platform | Alloc far buffer into handle via 281f_029a; store size+ptr or fail 0 | inferred |  |
| `FUN_7acf_003c` | 124345 | 24 | platform | Alloc far buffer into handle via 7ada_0022 (DX:AX size); sibling of 0002 | inferred |  |

### Segment `7ad6` (1 defs) — platform — Far-buffer free / clear handle struct

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ad6_000e` | 124369 | 21 | platform | Free handle far buffer (7ada_01aa) and zero size/ptr fields | inferred |  |

### Segment `7ada` (5 defs) — platform — DOS heap alloc / resize / high-water tracking

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ada_0006` | 124390 | 18 | platform | Run heap callback with reentrancy flag DS:0x2672 set | inferred |  |
| `FUN_7ada_0022` | 124408 | 84 | platform | DOS heap alloc (INT21 AH=48 / 2a1f) with high-water tracking | inferred |  |
| `FUN_7ada_01a0` | 124492 | 9 | platform | Alloc via 2a1f_0e90 using size cell DS:0x2674 | inferred |  |
| `FUN_7ada_01aa` | 124501 | 27 | platform | DOS INT21 AH=49 free mem block (large→2a1f_0fd4) | inferred |  |
| `FUN_7ada_01fa` | 124528 | 36 | platform | DOS INT21 AH=4A resize mem block; return CF as ±1 | inferred |  |

### Segment `7b04` (2 defs) — platform — DOS free-memory probe (INT21) + size max

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7b04_0002` | 124564 | 16 | platform | DOS free-memory probe (INT21 AH=48 BX=FFFF) → available bytes | inferred |  |
| `FUN_7b04_001e` | 124580 | 29 | platform | Max of DOS free probe (0002) vs XMS UMB size (2100_000e) | inferred |  |

### Segment `7b08` (5 defs) — platform — Growable far-buffer / arena alloc helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7b08_000e` | 124609 | 40 | platform | Alloc growable far-buffer arena (2a1f_0e90); init cursor/remain | inferred |  |
| `FUN_7b08_009e` | 124649 | 25 | platform | Wrap existing far block as arena (no alloc; owned=0) | inferred |  |
| `FUN_7b08_00dc` | 124674 | 22 | platform | Free owned arena (7ada_01aa) and clear far-buffer fields | inferred |  |
| `FUN_7b08_0118` | 124696 | 34 | platform | Bump-alloc from arena remain; else fatal 281f_0772 | inferred |  |
| `FUN_7b08_0182` | 124730 | 398 | platform | INT21 shrink arena to used size; return final far size | inferred |  |

## MAPEDIT

557 functions in `mapedit.c` (cdecl/stdcall + unannotated stubs). Address space is **MAPEDIT-only** — do not equate offsets with the other EXE.

### Segment `1000` (61 defs) — ui — MAPEDIT main UI hub: menus, viewport, terrain chrome

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1000_0002` | 81 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0004` | 89 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0006` | 97 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0008` | 105 | 8 | ui | unknown | inferred |  |
| `FUN_1000_000a` | 113 | 8 | ui | unknown | inferred |  |
| `FUN_1000_000c` | 121 | 8 | ui | unknown | inferred |  |
| `FUN_1000_000e` | 129 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0010` | 137 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0012` | 145 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0014` | 153 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0016` | 161 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0018` | 169 | 24 | ui | unknown | inferred |  |
| `FUN_1000_0060` | 193 | 29 | ui | unknown | inferred |  |
| `FUN_1000_00b6` | 222 | 48 | ui | unknown | inferred |  |
| `FUN_1000_0186` | 270 | 8 | ui | unknown | inferred |  |
| `FUN_1000_0196` | 278 | 88 | ui | unknown | inferred |  |
| `FUN_1000_056a` | 366 | 100 | ui | unknown | inferred |  |
| `FUN_1000_0750` | 466 | 17 | ui | unknown | inferred |  |
| `FUN_1000_077e` | 483 | 16 | ui | unknown | inferred |  |
| `FUN_1000_07a6` | 499 | 23 | ui | unknown | inferred |  |
| `FUN_1000_082c` | 522 | 9 | ui | unknown | inferred |  |
| `FUN_1000_0842` | 531 | 22 | ui | unknown | inferred |  |
| `FUN_1000_08e2` | 553 | 19 | ui | unknown | inferred |  |
| `FUN_1000_094e` | 572 | 128 | ui | unknown | inferred |  |
| `FUN_1000_0ce0` | 700 | 17 | ui | unknown | inferred |  |
| `FUN_1000_0d2a` | 717 | 18 | ui | unknown | inferred |  |
| `FUN_1000_0d9e` | 735 | 17 | ui | unknown | inferred |  |
| `FUN_1000_0e14` | 752 | 93 | ui | unknown | inferred |  |
| `FUN_1000_10cc` | 845 | 82 | ui | unknown | inferred |  |
| `FUN_1000_11ee` | 927 | 15 | ui | unknown | inferred |  |
| `FUN_1000_1226` | 942 | 64 | ui | unknown | inferred |  |
| `FUN_1000_1310` | 1006 | 25 | ui | unknown | inferred |  |
| `FUN_1000_1404` | 1031 | 28 | ui | unknown | inferred |  |
| `FUN_1000_145e` | 1059 | 43 | ui | unknown | inferred |  |
| `FUN_1000_1514` | 1102 | 30 | ui | unknown | inferred |  |
| `FUN_1000_157a` | 1132 | 11 | ui | unknown | inferred |  |
| `FUN_1000_1582` | 1143 | 24 | ui | unknown | inferred |  |
| `FUN_1000_15cc` | 1167 | 23 | ui | unknown | inferred |  |
| `FUN_1000_15fc` | 1190 | 30 | ui | unknown | inferred |  |
| `FUN_1000_1670` | 1220 | 75 | ui | unknown | inferred |  |
| `FUN_1000_17e0` | 1295 | 170 | ui | unknown | inferred |  |
| `FUN_1000_1b84` | 1465 | 25 | ui | unknown | inferred |  |
| `FUN_1000_1be0` | 1490 | 57 | ui | unknown | inferred |  |
| `FUN_1000_1d28` | 1547 | 35 | ui | unknown | inferred |  |
| `FUN_1000_1db6` | 1582 | 127 | ui | unknown | inferred |  |
| `FUN_1000_1f7e` | 1709 | 10 | ui | unknown | inferred |  |
| `FUN_1000_1f8e` | 1719 | 53 | ui | unknown | inferred |  |
| `FUN_1000_2082` | 1772 | 22 | ui | unknown | inferred |  |
| `FUN_1000_20e0` | 1794 | 32 | ui | unknown | inferred |  |
| `FUN_1000_2124` | 1826 | 100 | ui | unknown | inferred |  |
| `FUN_1000_229c` | 1926 | 14 | ui | unknown | inferred |  |
| `FUN_1000_22b0` | 1940 | 18 | ui | unknown | inferred |  |
| `FUN_1000_22e4` | 1958 | 29 | ui | unknown | inferred |  |
| `FUN_1000_2336` | 1987 | 104 | ui | unknown | inferred |  |
| `FUN_1000_247a` | 2091 | 49 | ui | unknown | inferred |  |
| `FUN_1000_2516` | 2140 | 142 | ui | unknown | inferred |  |
| `FUN_1000_27de` | 2282 | 10 | ui | unknown | inferred |  |
| `FUN_1000_27f6` | 2292 | 13 | ui | unknown | inferred |  |
| `FUN_1000_2828` | 2305 | 27 | ui | unknown | inferred |  |
| `FUN_1000_2872` | 2332 | 28 | ui | unknown | inferred |  |
| `FUN_1000_28d8` | 2360 | 60 | ui | unknown | inferred |  |

### Segment `1297` (4 defs) — platform — Path/name helpers: digit overlay, *-strip copy, fopen

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1297_000c` | 2420 | 44 | platform | Overlay decimal digits of AX into a path template string | inferred |  |
| `FUN_1297_00ba` | 2464 | 19 | platform | unknown | inferred |  |
| `FUN_1297_0104` | 2483 | 12 | platform | unknown | inferred |  |
| `FUN_1297_0142` | 2495 | 9 | platform | unknown | inferred |  |

### Segment `12ab` (26 defs) — mapdraw — Resources / rumours

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12ab_000e` | 2504 | 16 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0040` | 2520 | 26 | mapdraw | unknown | inferred |  |
| `FUN_12ab_00c4` | 2546 | 18 | mapdraw | unknown | inferred |  |
| `FUN_12ab_00fa` | 2564 | 10 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0112` | 2574 | 11 | mapdraw | unknown | inferred |  |
| `FUN_12ab_012e` | 2585 | 10 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0146` | 2595 | 11 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0162` | 2606 | 19 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0198` | 2625 | 10 | mapdraw | unknown | inferred |  |
| `FUN_12ab_01b0` | 2635 | 11 | mapdraw | unknown | inferred |  |
| `FUN_12ab_01ce` | 2646 | 11 | mapdraw | unknown | inferred |  |
| `FUN_12ab_01e0` | 2657 | 15 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0204` | 2672 | 16 | mapdraw | Resources / rumours helper | known | src/core/map.c |
| `FUN_12ab_022c` | 2688 | 26 | mapdraw | unknown | inferred |  |
| `FUN_12ab_02a4` | 2714 | 20 | mapdraw | unknown | inferred |  |
| `FUN_12ab_02e4` | 2734 | 10 | mapdraw | unknown | inferred |  |
| `FUN_12ab_02fc` | 2744 | 11 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0318` | 2755 | 18 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0346` | 2773 | 21 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0380` | 2794 | 21 | mapdraw | Resources / rumours helper | known | src/core/map.c |
| `FUN_12ab_03ba` | 2815 | 18 | mapdraw | unknown | inferred |  |
| `FUN_12ab_03e8` | 2833 | 14 | mapdraw | unknown | inferred |  |
| `FUN_12ab_040a` | 2847 | 24 | mapdraw | unknown | inferred |  |
| `FUN_12ab_0458` | 2871 | 44 | mapdraw | Resources / rumours helper | known | src/core/map.c; docs/assets.md |
| `FUN_12ab_0540` | 2915 | 25 | mapdraw | Resources / rumours helper | known | src/core/map.c |
| `FUN_12ab_05bc` | 2940 | 28 | mapdraw | unknown | inferred |  |

### Segment `130b` (6 defs) — mapdraw — 16x16 sprite blit + color-0 mask blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_130b_0006` | 2968 | 9 | mapdraw | unknown | inferred |  |
| `FUN_130b_001e` | 2977 | 15 | mapdraw | unknown | inferred |  |
| `FUN_130b_0048` | 2992 | 35 | mapdraw | 16x16 opaque sprite blit (coast underlayer path) | known | src/core/map.c |
| `FUN_130b_00ac` | 3027 | 45 | mapdraw | unknown | inferred |  |
| `FUN_130b_011e` | 3072 | 45 | mapdraw | unknown | inferred |  |
| `FUN_130b_01d4` | 3117 | 51 | mapdraw | unknown | inferred |  |

### Segment `1334` (3 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1334_000a` | 3168 | 12 | unknown | unknown | unknown |  |
| `FUN_1334_0036` | 3180 | 20 | unknown | unknown | unknown |  |
| `FUN_1334_006c` | 3200 | 33 | unknown | unknown | unknown |  |

### Segment `133d` (56 defs) — ui — Dialog / window layout and compositor

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_133d_000e` | 3233 | 20 | ui | unknown | inferred |  |
| `FUN_133d_0050` | 3253 | 32 | ui | unknown | inferred |  |
| `FUN_133d_00d0` | 3285 | 15 | ui | unknown | inferred |  |
| `FUN_133d_00fa` | 3300 | 15 | ui | unknown | inferred |  |
| `FUN_133d_0124` | 3315 | 100 | ui | unknown | inferred |  |
| `FUN_133d_034a` | 3415 | 31 | ui | unknown | inferred |  |
| `FUN_133d_03de` | 3446 | 9 | ui | unknown | inferred |  |
| `FUN_133d_03fa` | 3455 | 13 | ui | unknown | inferred |  |
| `FUN_133d_043a` | 3468 | 13 | ui | unknown | inferred |  |
| `FUN_133d_0454` | 3481 | 23 | ui | unknown | inferred |  |
| `FUN_133d_0494` | 3504 | 29 | ui | unknown | inferred |  |
| `FUN_133d_0504` | 3533 | 27 | ui | unknown | inferred |  |
| `FUN_133d_0546` | 3560 | 54 | ui | unknown | inferred |  |
| `FUN_133d_0650` | 3614 | 33 | ui | unknown | inferred |  |
| `FUN_133d_06de` | 3647 | 99 | ui | unknown | inferred |  |
| `FUN_133d_08a8` | 3746 | 36 | ui | unknown | inferred |  |
| `FUN_133d_0908` | 3782 | 17 | ui | unknown | inferred |  |
| `FUN_133d_0938` | 3799 | 16 | ui | unknown | inferred |  |
| `FUN_133d_0968` | 3815 | 25 | ui | unknown | inferred |  |
| `FUN_133d_0998` | 3840 | 17 | ui | unknown | inferred |  |
| `FUN_133d_09c8` | 3857 | 14 | ui | unknown | inferred |  |
| `FUN_133d_09f0` | 3871 | 13 | ui | unknown | inferred |  |
| `FUN_133d_0a0e` | 3884 | 101 | ui | unknown | inferred |  |
| `FUN_133d_0bf6` | 3985 | 21 | ui | unknown | inferred |  |
| `FUN_133d_0c30` | 4006 | 9 | ui | unknown | inferred |  |
| `FUN_133d_0c40` | 4015 | 76 | ui | unknown | inferred |  |
| `FUN_133d_0d52` | 4091 | 90 | ui | unknown | inferred |  |
| `FUN_133d_0f24` | 4181 | 16 | ui | unknown | inferred |  |
| `FUN_133d_0f4a` | 4197 | 106 | ui | unknown | inferred |  |
| `FUN_133d_1138` | 4303 | 20 | ui | unknown | inferred |  |
| `FUN_133d_117a` | 4323 | 9 | ui | unknown | inferred |  |
| `FUN_133d_11a6` | 4332 | 126 | ui | unknown | inferred |  |
| `FUN_133d_14d4` | 4458 | 266 | ui | unknown | inferred |  |
| `FUN_133d_1a4a` | 4724 | 18 | ui | unknown | inferred |  |
| `FUN_133d_1a86` | 4742 | 29 | ui | unknown | inferred |  |
| `FUN_133d_1af6` | 4771 | 29 | ui | unknown | inferred |  |
| `FUN_133d_1b8a` | 4800 | 92 | ui | unknown | inferred |  |
| `FUN_133d_1e22` | 4892 | 82 | ui | unknown | inferred |  |
| `FUN_133d_202c` | 4974 | 69 | ui | unknown | inferred |  |
| `FUN_133d_2286` | 5043 | 50 | ui | unknown | inferred |  |
| `FUN_133d_249c` | 5093 | 40 | ui | unknown | inferred |  |
| `FUN_133d_256c` | 5133 | 16 | ui | unknown | inferred |  |
| `FUN_133d_258e` | 5149 | 522 | ui | unknown | inferred |  |
| `FUN_133d_3092` | 5671 | 13 | ui | unknown | inferred |  |
| `FUN_133d_30aa` | 5684 | 92 | ui | unknown | inferred |  |
| `FUN_133d_32b2` | 5776 | 186 | ui | unknown | inferred |  |
| `FUN_133d_36d8` | 5962 | 18 | ui | unknown | inferred |  |
| `FUN_133d_3712` | 5980 | 17 | ui | unknown | inferred |  |
| `FUN_133d_373c` | 5997 | 11 | ui | unknown | inferred |  |
| `FUN_133d_376e` | 6008 | 12 | ui | unknown | inferred |  |
| `FUN_133d_3798` | 6020 | 12 | ui | unknown | inferred |  |
| `FUN_133d_37b0` | 6032 | 12 | ui | unknown | inferred |  |
| `FUN_133d_37da` | 6044 | 12 | ui | unknown | inferred |  |
| `FUN_133d_380a` | 6056 | 25 | ui | unknown | inferred |  |
| `FUN_133d_3856` | 6081 | 16 | ui | unknown | inferred |  |
| `FUN_133d_3898` | 6097 | 34 | ui | unknown | inferred |  |

### Segment `16d7` (24 defs) — ui — Text layout / flow-wrap compositor (~ color switches)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_16d7_0008` | 6131 | 15 | ui | unknown | inferred |  |
| `FUN_16d7_001e` | 6146 | 15 | ui | unknown | inferred |  |
| `FUN_16d7_0052` | 6161 | 25 | ui | unknown | inferred |  |
| `FUN_16d7_00b6` | 6186 | 22 | ui | unknown | inferred |  |
| `FUN_16d7_010c` | 6208 | 47 | ui | unknown | inferred |  |
| `FUN_16d7_0246` | 6255 | 45 | ui | unknown | inferred |  |
| `FUN_16d7_02fe` | 6300 | 41 | ui | unknown | inferred |  |
| `FUN_16d7_0446` | 6341 | 34 | ui | unknown | inferred |  |
| `FUN_16d7_048c` | 6375 | 44 | ui | unknown | inferred |  |
| `FUN_16d7_0522` | 6419 | 20 | ui | unknown | inferred |  |
| `FUN_16d7_055a` | 6439 | 17 | ui | unknown | inferred |  |
| `FUN_16d7_058a` | 6456 | 37 | ui | unknown | inferred |  |
| `FUN_16d7_05ce` | 6493 | 17 | ui | unknown | inferred |  |
| `FUN_16d7_05fe` | 6510 | 37 | ui | unknown | inferred |  |
| `FUN_16d7_0642` | 6547 | 79 | ui | unknown | inferred |  |
| `FUN_16d7_07de` | 6626 | 79 | ui | unknown | inferred |  |
| `FUN_16d7_0944` | 6705 | 51 | ui | unknown | inferred |  |
| `FUN_16d7_0a6c` | 6756 | 56 | ui | unknown | inferred |  |
| `FUN_16d7_0b82` | 6812 | 91 | ui | unknown | inferred |  |
| `FUN_16d7_0d9c` | 6903 | 294 | ui | unknown | inferred |  |
| `FUN_16d7_13b4` | 7197 | 43 | ui | unknown | inferred |  |
| `FUN_16d7_1452` | 7240 | 42 | ui | unknown | inferred |  |
| `FUN_16d7_14e6` | 7282 | 58 | ui | unknown | inferred |  |
| `FUN_16d7_15ac` | 7340 | 38 | ui | unknown | inferred |  |

### Segment `1842` (8 defs) — platform — Config/text file open + line/comma-field parse

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1842_0000` | 7378 | 14 | platform | unknown | inferred |  |
| `FUN_1842_001a` | 7392 | 62 | platform | unknown | inferred |  |
| `FUN_1842_0106` | 7454 | 27 | platform | unknown | inferred |  |
| `FUN_1842_015e` | 7481 | 28 | platform | unknown | inferred |  |
| `FUN_1842_0198` | 7509 | 10 | platform | unknown | inferred |  |
| `FUN_1842_01c8` | 7519 | 10 | platform | unknown | inferred |  |
| `FUN_1842_01da` | 7529 | 20 | platform | unknown | inferred |  |
| `FUN_1842_0208` | 7549 | 24 | platform | unknown | inferred |  |

### Segment `1865` (7 defs) — platform — Clamp / swap / approx distance / facing helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1865_000e` | 7573 | 14 | platform | unknown | inferred |  |
| `FUN_1865_002c` | 7587 | 14 | platform | unknown | inferred |  |
| `FUN_1865_0042` | 7601 | 17 | platform | unknown | inferred |  |
| `FUN_1865_007e` | 7618 | 20 | platform | unknown | inferred |  |
| `FUN_1865_00c6` | 7638 | 17 | platform | unknown | inferred |  |
| `FUN_1865_00f6` | 7655 | 20 | platform | unknown | inferred |  |
| `FUN_1865_013e` | 7675 | 14 | platform | unknown | inferred |  |

### Segment `187b` (6 defs) — ui — Viewport rect clip and blit helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_187b_004a` | 7689 | 45 | ui | unknown | inferred |  |
| `FUN_187b_00c0` | 7734 | 15 | ui | unknown | inferred |  |
| `FUN_187b_012a` | 7749 | 14 | ui | unknown | inferred |  |
| `FUN_187b_0160` | 7763 | 51 | ui | unknown | inferred |  |
| `FUN_187b_021c` | 7814 | 11 | ui | unknown | inferred |  |
| `FUN_187b_0234` | 7825 | 13 | ui | unknown | inferred |  |

### Segment `18a2` (3 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_18a2_000a` | 7838 | 13 | unknown | unknown | unknown |  |
| `FUN_18a2_0068` | 7851 | 16 | unknown | unknown | unknown |  |
| `FUN_18a2_00b0` | 7867 | 9 | unknown | unknown | unknown |  |

### Segment `18ad` (28 defs) — ui — Text / number blit and string-table helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_18ad_0032` | 7876 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_0042` | 7885 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_0052` | 7894 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_0062` | 7903 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_0072` | 7912 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_0082` | 7921 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_0092` | 7930 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_00a2` | 7939 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_00b2` | 7948 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_00c2` | 7957 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_00d2` | 7966 | 9 | ui | unknown | inferred |  |
| `FUN_18ad_00e2` | 7975 | 10 | ui | unknown | inferred |  |
| `FUN_18ad_00fc` | 7985 | 12 | ui | unknown | inferred |  |
| `FUN_18ad_012e` | 7997 | 12 | ui | unknown | inferred |  |
| `FUN_18ad_0156` | 8009 | 27 | ui | unknown | inferred |  |
| `FUN_18ad_01be` | 8036 | 13 | ui | unknown | inferred |  |
| `FUN_18ad_01e8` | 8049 | 10 | ui | unknown | inferred |  |
| `FUN_18ad_0216` | 8059 | 12 | ui | unknown | inferred |  |
| `FUN_18ad_0232` | 8071 | 12 | ui | unknown | inferred |  |
| `FUN_18ad_024e` | 8083 | 13 | ui | unknown | inferred |  |
| `FUN_18ad_0288` | 8096 | 14 | ui | unknown | inferred |  |
| `FUN_18ad_02c2` | 8110 | 16 | ui | unknown | inferred |  |
| `FUN_18ad_0318` | 8126 | 17 | ui | unknown | inferred |  |
| `FUN_18ad_035c` | 8143 | 12 | ui | unknown | inferred |  |
| `FUN_18ad_039a` | 8155 | 12 | ui | unknown | inferred |  |
| `FUN_18ad_03d2` | 8167 | 15 | ui | unknown | inferred |  |
| `FUN_18ad_0430` | 8182 | 17 | ui | unknown | inferred |  |
| `FUN_18ad_0478` | 8199 | 22 | ui | unknown | inferred |  |

### Segment `18f9` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_18f9_0b0e` | 8221 | 36 | unknown | unknown | unknown |  |
| `FUN_18f9_0bc2` | 8257 | 15 | unknown | unknown | unknown |  |

### Segment `19b7` (4 defs) — mapdraw — Terrain class index

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_19b7_0006` | 8272 | 13 | mapdraw | Terrain class index | known | src/core/map.c |
| `FUN_19b7_0032` | 8285 | 18 | mapdraw | unknown | inferred |  |
| `FUN_19b7_006c` | 8303 | 14 | mapdraw | unknown | inferred |  |
| `FUN_19b7_009a` | 8317 | 15 | mapdraw | unknown | inferred |  |

### Segment `19c4` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_19c4_0002` | 8332 | 154 | unknown | unknown | unknown |  |

### Segment `19f9` (8 defs) — mapdraw — Map plane buffer alloc / .MP load-save / clear

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_19f9_000a` | 8486 | 33 | mapdraw | unknown | inferred |  |
| `FUN_19f9_0062` | 8519 | 58 | mapdraw | unknown | inferred |  |
| `FUN_19f9_0128` | 8577 | 8 | mapdraw | unknown | inferred |  |
| `FUN_19f9_012c` | 8585 | 20 | mapdraw | unknown | inferred |  |
| `FUN_19f9_0170` | 8605 | 65 | mapdraw | unknown | inferred |  |
| `FUN_19f9_02b0` | 8670 | 58 | mapdraw | unknown | inferred |  |
| `FUN_19f9_03ba` | 8728 | 28 | mapdraw | unknown | inferred |  |
| `FUN_19f9_043e` | 8756 | 46 | mapdraw | unknown | inferred |  |

### Segment `1a47` (17 defs) — mapdraw — Tile compositor entry and land/coast/transition/river/hill/forest masks

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a47_0006` | 8802 | 101 | mapdraw | unknown | inferred |  |
| `FUN_1a47_01ae` | 8903 | 64 | mapdraw | Land mask / coast setup | known | src/core/map.c |
| `FUN_1a47_030e` | 8967 | 32 | mapdraw | River connectivity mask | known | src/core/map.c |
| `FUN_1a47_036e` | 8999 | 32 | mapdraw | Hill connectivity mask | known | src/core/map.c |
| `FUN_1a47_03de` | 9031 | 19 | mapdraw | unknown | inferred |  |
| `FUN_1a47_0418` | 9050 | 33 | mapdraw | Forest connectivity mask | known | src/core/map.c |
| `FUN_1a47_047e` | 9083 | 32 | mapdraw | unknown | inferred |  |
| `FUN_1a47_04de` | 9115 | 35 | mapdraw | unknown | inferred |  |
| `FUN_1a47_0552` | 9150 | 17 | mapdraw | unknown | inferred |  |
| `FUN_1a47_05b2` | 9167 | 15 | mapdraw | Coastal underlayer | known | src/core/map.c |
| `FUN_1a47_0616` | 9182 | 17 | mapdraw | unknown | inferred |  |
| `FUN_1a47_0676` | 9199 | 15 | mapdraw | Masked ocean fill | known | src/core/map.c |
| `FUN_1a47_06da` | 9214 | 104 | mapdraw | Land-land transitions | known | src/core/map.c |
| `FUN_1a47_0932` | 9318 | 191 | mapdraw | Tile draw / compositor entry | known | src/core/map.c; docs/assets.md |
| `FUN_1a47_0d66` | 9509 | 135 | mapdraw | unknown | inferred |  |
| `FUN_1a47_1022` | 9644 | 26 | mapdraw | unknown | inferred |  |
| `FUN_1a47_10b8` | 9670 | 17 | mapdraw | unknown | inferred |  |

### Segment `1b56` (9 defs) — mapdraw — Minimap terrain palette cache + viewport blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b56_0004` | 9687 | 19 | mapdraw | unknown | inferred |  |
| `FUN_1b56_0038` | 9706 | 19 | mapdraw | unknown | inferred |  |
| `FUN_1b56_006c` | 9725 | 32 | mapdraw | unknown | inferred |  |
| `FUN_1b56_00dc` | 9757 | 22 | mapdraw | unknown | inferred |  |
| `FUN_1b56_011c` | 9779 | 84 | mapdraw | unknown | inferred |  |
| `FUN_1b56_0274` | 9863 | 12 | mapdraw | unknown | inferred |  |
| `FUN_1b56_028e` | 9875 | 52 | mapdraw | unknown | inferred |  |
| `FUN_1b56_03b4` | 9927 | 34 | mapdraw | unknown | inferred |  |
| `FUN_1b56_04ca` | 9961 | 10 | mapdraw | unknown | inferred |  |

### Segment `1ba3` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ba3_0000` | 9971 | 30 | unknown | unknown | unknown |  |

### Segment `1baa` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1baa_0002` | 10001 | 26 | unknown | unknown | unknown |  |

### Segment `1baf` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1baf_0004` | 10027 | 16 | unknown | unknown | unknown |  |
| `FUN_1baf_0018` | 10043 | 21 | unknown | unknown | unknown |  |

### Segment `1bb2` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bb2_000e` | 10064 | 1162 | unknown | unknown | unknown |  |

### Segment `1bc4` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bc4_0000` | 11226 | 15 | unknown | unknown | unknown |  |
| `FUN_1bc4_002e` | 11241 | 20 | unknown | unknown | unknown |  |

### Segment `1bca` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bca_0000` | 11261 | 115 | unknown | unknown | unknown |  |

### Segment `1bea` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bea_0008` | 11376 | 72 | unknown | unknown | unknown |  |

### Segment `1bfb` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bfb_001c` | 11448 | 55 | unknown | unknown | unknown |  |

### Segment `1c04` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c04_000e` | 11503 | 18 | unknown | unknown | unknown |  |
| `FUN_1c04_005c` | 11521 | 23 | unknown | unknown | unknown |  |

### Segment `1c11` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c11_0004` | 11544 | 25 | unknown | unknown | unknown |  |

### Segment `1c1a` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c1a_0002` | 11569 | 23 | unknown | unknown | unknown |  |

### Segment `1c21` (3 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c21_002a` | 11592 | 15 | unknown | unknown | unknown |  |
| `FUN_1c21_0042` | 11607 | 59 | unknown | unknown | unknown |  |
| `FUN_1c21_0110` | 11666 | 26 | unknown | unknown | unknown |  |

### Segment `1c34` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c34_000c` | 11692 | 13 | unknown | unknown | unknown |  |
| `FUN_1c34_0044` | 11705 | 17 | unknown | unknown | unknown |  |

### Segment `1c3c` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c3c_000a` | 11722 | 15 | unknown | unknown | unknown |  |

### Segment `1c3e` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c3e_000a` | 11737 | 24 | unknown | unknown | unknown |  |
| `FUN_1c3e_0044` | 11761 | 24 | unknown | unknown | unknown |  |

### Segment `1c46` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c46_0006` | 11785 | 22 | unknown | unknown | unknown |  |

### Segment `1c49` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c49_000e` | 11807 | 12 | unknown | unknown | unknown |  |

### Segment `1c4c` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c4c_0000` | 11819 | 105 | unknown | unknown | unknown |  |

### Segment `1c5b` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c5b_0004` | 11924 | 80 | unknown | unknown | unknown |  |

### Segment `1c67` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c67_0000` | 12004 | 101 | unknown | unknown | unknown |  |

### Segment `1c76` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c76_0004` | 12105 | 14 | unknown | unknown | unknown |  |

### Segment `1c78` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c78_0000` | 12119 | 13 | unknown | unknown | unknown |  |

### Segment `1c79` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c79_0006` | 12132 | 31 | unknown | unknown | unknown |  |

### Segment `1c80` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c80_0000` | 12163 | 31 | unknown | unknown | unknown |  |

### Segment `1c86` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c86_000c` | 12194 | 17 | unknown | unknown | unknown |  |

### Segment `1c91` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c91_0000` | 12211 | 13 | unknown | unknown | unknown |  |
| `FUN_1c91_0044` | 12224 | 38 | unknown | unknown | unknown |  |

### Segment `1c9d` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c9d_000e` | 12262 | 35 | unknown | unknown | unknown |  |
| `FUN_1c9d_00ec` | 12297 | 34 | unknown | unknown | unknown |  |

### Segment `1cb9` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1cb9_0000` | 12331 | 52 | unknown | unknown | unknown |  |

### Segment `1cc9` (9 defs) — platform — DOS conventional heap (MCB INT21)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1cc9_0004` | 12383 | 55 | platform | DOS MCB heap alloc/split (INT 21) | known |  |
| `FUN_1cc9_009c` | 12438 | 27 | platform | unknown | inferred |  |
| `FUN_1cc9_00f0` | 12465 | 15 | platform | unknown | inferred |  |
| `FUN_1cc9_010c` | 12480 | 34 | platform | unknown | inferred |  |
| `FUN_1cc9_0136` | 12514 | 85 | platform | unknown | inferred |  |
| `FUN_1cc9_02e2` | 12599 | 11 | platform | unknown | inferred |  |
| `FUN_1cc9_02ec` | 12610 | 29 | platform | unknown | inferred |  |
| `FUN_1cc9_0310` | 12639 | 24 | platform | unknown | inferred |  |
| `FUN_1cc9_0360` | 12663 | 16 | platform | unknown | inferred |  |

### Segment `1d08` (3 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d08_000e` | 12679 | 43 | unknown | unknown | unknown |  |
| `FUN_1d08_0068` | 12722 | 11 | unknown | unknown | unknown |  |
| `FUN_1d08_0082` | 12733 | 19 | unknown | unknown | unknown |  |

### Segment `1d12` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d12_000c` | 12752 | 27 | unknown | unknown | unknown |  |

### Segment `1d18` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d18_0006` | 12779 | 10 | unknown | unknown | unknown |  |
| `FUN_1d18_0022` | 12789 | 12 | unknown | unknown | unknown |  |

### Segment `1d1c` (3 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d1c_017d` | 12801 | 33 | unknown | unknown | unknown |  |
| `FUN_1d1c_01f3` | 12834 | 21 | unknown | unknown | unknown |  |
| `FUN_1d1c_023d` | 12855 | 18 | unknown | unknown | unknown |  |

### Segment `1d43` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d43_000a` | 12873 | 67 | unknown | unknown | unknown |  |

### Segment `1d53` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d53_0008` | 12940 | 128 | unknown | unknown | unknown |  |

### Segment `1d6a` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d6a_0006` | 13068 | 17 | unknown | unknown | unknown |  |

### Segment `1d6c` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d6c_0002` | 13085 | 30 | unknown | unknown | unknown |  |

### Segment `1d70` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d70_000a` | 13115 | 44 | unknown | unknown | unknown |  |

### Segment `1d75` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d75_000e` | 13159 | 45 | unknown | unknown | unknown |  |
| `FUN_1d75_0074` | 13204 | 79 | unknown | unknown | unknown |  |

### Segment `1d8f` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d8f_0000` | 13283 | 176 | unknown | unknown | unknown |  |

### Segment `1dae` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1dae_000a` | 13459 | 231 | unknown | unknown | unknown |  |

### Segment `1ddb` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ddb_0008` | 13690 | 437 | unknown | unknown | unknown |  |

### Segment `1e71` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1e71_000c` | 14127 | 176 | unknown | unknown | unknown |  |

### Segment `1e92` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1e92_000a` | 14303 | 231 | unknown | unknown | unknown |  |

### Segment `1ec0` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ec0_000a` | 14534 | 25 | unknown | unknown | unknown |  |

### Segment `1ec5` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ec5_0008` | 14559 | 17 | unknown | unknown | unknown |  |
| `FUN_1ec5_0024` | 14576 | 70 | unknown | unknown | unknown |  |

### Segment `1ed0` (4 defs) — platform — Fatal/abort error text + INT10 video reset

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ed0_0008` | 14646 | 38 | platform | unknown | inferred |  |
| `FUN_1ed0_00ac` | 14684 | 45 | platform | unknown | inferred |  |
| `FUN_1ed0_0156` | 14729 | 84 | platform | Fatal abort: INT10 video reset, print error text, exit(3) | inferred |  |
| `FUN_1ed0_03d6` | 14813 | 32 | platform | unknown | inferred |  |

### Segment `1f16` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1f16_000e` | 14845 | 20 | unknown | unknown | unknown |  |
| `FUN_1f16_00e6` | 14865 | 79 | unknown | unknown | unknown |  |

### Segment `1f36` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1f36_000a` | 14944 | 46 | unknown | unknown | unknown |  |

### Segment `1f3e` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1f3e_000c` | 14990 | 28 | unknown | unknown | unknown |  |
| `FUN_1f3e_005a` | 15018 | 13 | unknown | unknown | unknown |  |

### Segment `1f45` (5 defs) — platform — Growable far-buffer / arena on 1cc9

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1f45_0000` | 15031 | 42 | platform | unknown | inferred |  |
| `FUN_1f45_0090` | 15073 | 25 | platform | unknown | inferred |  |
| `FUN_1f45_00ce` | 15098 | 22 | platform | unknown | inferred |  |
| `FUN_1f45_010a` | 15120 | 34 | platform | unknown | inferred |  |
| `FUN_1f45_0174` | 15154 | 33 | platform | unknown | inferred |  |

### Segment `1f65` (23 defs) — platform — Mouse driver INT 33 show/hide / init / poll

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1f65_0007` | 15187 | 36 | platform | unknown | inferred |  |
| `FUN_1f65_004e` | 15223 | 28 | platform | unknown | inferred |  |
| `FUN_1f65_0086` | 15251 | 91 | platform | Mouse INT 33 probe/init (presence flag) | known |  |
| `FUN_1f65_01f4` | 15342 | 12 | platform | unknown | inferred |  |
| `FUN_1f65_0222` | 15354 | 28 | platform | unknown | inferred |  |
| `FUN_1f65_02c3` | 15382 | 11 | platform | unknown | inferred |  |
| `FUN_1f65_038a` | 15393 | 35 | platform | unknown | inferred |  |
| `FUN_1f65_03f8` | 15428 | 82 | platform | unknown | inferred |  |
| `FUN_1f65_045d` | 15510 | 58 | platform | unknown | inferred |  |
| `FUN_1f65_04bf` | 15568 | 14 | platform | unknown | inferred |  |
| `FUN_1f65_04d1` | 15582 | 22 | platform | unknown | inferred |  |
| `FUN_1f65_0500` | 15604 | 31 | platform | unknown | inferred |  |
| `FUN_1f65_055c` | 15635 | 19 | platform | unknown | inferred |  |
| `FUN_1f65_057c` | 15654 | 36 | platform | unknown | inferred |  |
| `FUN_1f65_05bf` | 15690 | 15 | platform | unknown | inferred |  |
| `FUN_1f65_05d7` | 15705 | 15 | platform | unknown | inferred |  |
| `FUN_1f65_064a` | 15720 | 13 | platform | unknown | inferred |  |
| `FUN_1f65_066f` | 15733 | 47 | platform | unknown | inferred |  |
| `FUN_1f65_075e` | 15780 | 14 | platform | unknown | inferred |  |
| `FUN_1f65_0777` | 15794 | 18 | platform | unknown | inferred |  |
| `FUN_1f65_07a8` | 15812 | 15 | platform | unknown | inferred |  |
| `FUN_1f65_07cd` | 15827 | 22 | platform | unknown | inferred |  |
| `FUN_1f65_0903` | 15849 | 13 | platform | unknown | inferred |  |

### Segment `1ffe` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ffe_01c7` | 15862 | 20 | unknown | unknown | unknown |  |

### Segment `201f` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_201f_000c` | 15882 | 27 | unknown | unknown | unknown |  |

### Segment `2025` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2025_000c` | 15909 | 59 | unknown | unknown | unknown |  |
| `FUN_2025_00ea` | 15968 | 38 | unknown | unknown | unknown |  |

### Segment `203d` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_203d_000a` | 16006 | 33 | unknown | unknown | unknown |  |
| `FUN_203d_009e` | 16039 | 27 | unknown | unknown | unknown |  |

### Segment `204f` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_204f_0002` | 16066 | 8 | unknown | unknown | unknown |  |

### Segment `2050` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2050_000a` | 16074 | 44 | unknown | unknown | unknown |  |

### Segment `2056` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2056_000e` | 16118 | 34 | unknown | unknown | unknown |  |

### Segment `205b` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_205b_0006` | 16152 | 14 | unknown | unknown | unknown |  |

### Segment `205c` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_205c_000a` | 16166 | 44 | unknown | unknown | unknown |  |

### Segment `2061` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2061_000a` | 16210 | 16 | unknown | unknown | unknown |  |
| `FUN_2061_001e` | 16226 | 46 | unknown | unknown | unknown |  |

### Segment `2069` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2069_0006` | 16272 | 56 | unknown | unknown | unknown |  |

### Segment `2074` (10 defs) — ui — VGA palette RGB match / slot allocator

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2074_000e` | 16328 | 15 | ui | unknown | inferred |  |
| `FUN_2074_002c` | 16343 | 68 | ui | unknown | inferred |  |
| `FUN_2074_00ec` | 16411 | 23 | ui | unknown | inferred |  |
| `FUN_2074_0288` | 16434 | 27 | ui | RGB squared-distance for palette match | inferred |  |
| `FUN_2074_02ae` | 16461 | 34 | ui | unknown | inferred |  |
| `FUN_2074_0332` | 16495 | 24 | ui | unknown | inferred |  |
| `FUN_2074_0392` | 16519 | 12 | ui | unknown | inferred |  |
| `FUN_2074_03a6` | 16531 | 22 | ui | unknown | inferred |  |
| `FUN_2074_03e0` | 16553 | 24 | ui | unknown | inferred |  |
| `FUN_2074_0416` | 16577 | 257 | ui | unknown | inferred |  |

### Segment `2114` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2114_0002` | 16834 | 8 | unknown | unknown | unknown |  |

### Segment `2115` (4 defs) — platform — DOS Ctrl-C/Break / INT21 abort handlers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2115_00b2` | 16842 | 32 | platform | unknown | inferred |  |
| `FUN_2115_00e3` | 16874 | 15 | platform | unknown | inferred |  |
| `FUN_2115_0100` | 16889 | 20 | platform | Install 5 far INT abort handlers pointing into this segment | known |  |
| `FUN_2115_0119` | 16909 | 10 | platform | unknown | inferred |  |

### Segment `2127` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2127_0004` | 16919 | 20 | unknown | unknown | unknown |  |
| `FUN_2127_0059` | 16939 | 13 | unknown | unknown | unknown |  |

### Segment `212d` (6 defs) — platform — EMS INT67 page map + INT21 env helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_212d_0006` | 16952 | 92 | platform | unknown | inferred |  |
| `FUN_212d_00ba` | 17044 | 16 | platform | unknown | inferred |  |
| `FUN_212d_00d4` | 17060 | 22 | platform | EMS page-map via INT 67 | known |  |
| `FUN_212d_0114` | 17082 | 16 | platform | unknown | inferred |  |
| `FUN_212d_0136` | 17098 | 16 | platform | unknown | inferred |  |
| `FUN_212d_015e` | 17114 | 18 | platform | unknown | inferred |  |

### Segment `2145` (5 defs) — platform — EMS page-frame heap / handle allocator

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2145_000c` | 17132 | 15 | platform | unknown | inferred |  |
| `FUN_2145_0036` | 17147 | 38 | platform | unknown | inferred |  |
| `FUN_2145_00a0` | 17185 | 37 | platform | unknown | inferred |  |
| `FUN_2145_0106` | 17222 | 42 | platform | unknown | inferred |  |
| `FUN_2145_01c4` | 17264 | 24 | platform | unknown | inferred |  |

### Segment `2202` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2202_0008` | 17288 | 38 | unknown | unknown | unknown |  |
| `FUN_2202_0086` | 17326 | 48 | unknown | unknown | unknown |  |

### Segment `221a` (3 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_221a_0000` | 17374 | 164 | unknown | unknown | unknown |  |
| `FUN_221a_033c` | 17538 | 11 | unknown | unknown | unknown |  |
| `FUN_221a_034e` | 17549 | 39 | unknown | unknown | unknown |  |

### Segment `2258` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2258_0002` | 17588 | 115 | unknown | unknown | unknown |  |

### Segment `227b` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_227b_0000` | 17703 | 20 | unknown | unknown | unknown |  |

### Segment `227e` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_227e_000c` | 17723 | 19 | unknown | unknown | unknown |  |

### Segment `2281` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2281_0008` | 17742 | 24 | unknown | unknown | unknown |  |
| `FUN_2281_004a` | 17766 | 26 | unknown | unknown | unknown |  |

### Segment `2289` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2289_0000` | 17792 | 18 | unknown | unknown | unknown |  |

### Segment `228b` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_228b_0004` | 17810 | 8 | unknown | unknown | unknown |  |
| `FUN_228b_0040` | 17818 | 13 | unknown | unknown | unknown |  |

### Segment `2290` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2290_0006` | 17831 | 18 | unknown | unknown | unknown |  |

### Segment `2292` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2292_000a` | 17849 | 28 | unknown | unknown | unknown |  |

### Segment `2297` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2297_000e` | 17877 | 66 | unknown | unknown | unknown |  |

### Segment `22aa` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_22aa_001b` | 17943 | 27 | unknown | unknown | unknown |  |

### Segment `22b0` (3 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_22b0_0000` | 17970 | 63 | unknown | unknown | unknown |  |
| `FUN_22b0_00ca` | 18033 | 28 | unknown | unknown | unknown |  |
| `FUN_22b0_0100` | 18061 | 38 | unknown | unknown | unknown |  |

### Segment `22c7` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_22c7_000e` | 18099 | 87 | unknown | unknown | unknown |  |

### Segment `22e0` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_22e0_0008` | 18186 | 86 | unknown | unknown | unknown |  |

### Segment `22ec` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_22ec_0008` | 18272 | 51 | unknown | unknown | unknown |  |

### Segment `22fb` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_22fb_0006` | 18323 | 33 | unknown | unknown | unknown |  |

### Segment `2302` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2302_000a` | 18356 | 23 | unknown | unknown | unknown |  |

### Segment `2309` (4 defs) — platform — Compressed resource stream I/O + progress callback

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2309_000a` | 18379 | 8 | platform | unknown | inferred |  |
| `FUN_2309_0014` | 18387 | 14 | platform | unknown | inferred |  |
| `FUN_2309_002a` | 18401 | 15 | platform | unknown | inferred |  |
| `FUN_2309_004a` | 18416 | 164 | platform | Wire compressed stream I/O (callbacks + heap buffer + consume loop) | inferred |  |

### Segment `233e` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_233e_000a` | 18580 | 12 | unknown | unknown | unknown |  |

### Segment `2343` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2343_0004` | 18592 | 17 | unknown | unknown | unknown |  |

### Segment `2345` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2345_000a` | 18609 | 78 | unknown | unknown | unknown |  |

### Segment `2357` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2357_000e` | 18687 | 17 | unknown | unknown | unknown |  |

### Segment `2359` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2359_0006` | 18704 | 53 | unknown | unknown | unknown |  |

### Segment `2360` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2360_000a` | 18757 | 52 | unknown | unknown | unknown |  |

### Segment `2367` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2367_0008` | 18809 | 36 | unknown | unknown | unknown |  |

### Segment `2378` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2378_0004` | 18845 | 175 | unknown | unknown | unknown |  |

### Segment `2388` (98 defs) — platform — DOS CRT / runtime: INT21 I/O, env parse, strlen/strcpy family

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2388_00f1` | 19020 | 109 | platform | unknown | inferred |  |
| `FUN_2388_0116` | 19129 | 94 | platform | unknown | inferred |  |
| `FUN_2388_01db` | 19223 | 22 | platform | unknown | inferred |  |
| `FUN_2388_025e` | 19245 | 20 | platform | unknown | inferred |  |
| `FUN_2388_028b` | 19265 | 25 | platform | unknown | inferred |  |
| `FUN_2388_029e` | 19290 | 28 | platform | unknown | inferred |  |
| `FUN_2388_02c2` | 19318 | 41 | platform | unknown | inferred |  |
| `FUN_2388_037c` | 19359 | 18 | platform | unknown | inferred |  |
| `FUN_2388_03a8` | 19377 | 9 | platform | unknown | inferred |  |
| `FUN_2388_03be` | 19386 | 72 | platform | unknown | inferred |  |
| `FUN_2388_04a2` | 19458 | 70 | platform | unknown | inferred |  |
| `FUN_2388_05a8` | 19528 | 14 | platform | unknown | inferred |  |
| `FUN_2388_05e6` | 19542 | 56 | platform | unknown | inferred |  |
| `FUN_2388_0626` | 19598 | 46 | platform | unknown | inferred |  |
| `FUN_2388_0658` | 19644 | 44 | platform | unknown | inferred |  |
| `FUN_2388_0684` | 19688 | 19 | platform | strlen (scan to NUL) | known |  |
| `FUN_2388_06a0` | 19707 | 46 | platform | unknown | inferred |  |
| `FUN_2388_06d6` | 19753 | 30 | platform | unknown | inferred |  |
| `FUN_2388_06fe` | 19783 | 77 | platform | unknown | inferred |  |
| `FUN_2388_073c` | 19860 | 61 | platform | unknown | inferred |  |
| `FUN_2388_0758` | 19921 | 9 | platform | unknown | inferred |  |
| `FUN_2388_0762` | 19930 | 22 | platform | unknown | inferred |  |
| `FUN_2388_078a` | 19952 | 65 | platform | unknown | inferred |  |
| `FUN_2388_07fe` | 20017 | 35 | platform | unknown | inferred |  |
| `FUN_2388_087e` | 20052 | 11 | platform | unknown | inferred |  |
| `FUN_2388_0898` | 20063 | 21 | platform | unknown | inferred |  |
| `FUN_2388_08dc` | 20084 | 21 | platform | unknown | inferred |  |
| `FUN_2388_0908` | 20105 | 28 | platform | unknown | inferred |  |
| `FUN_2388_0962` | 20133 | 30 | platform | unknown | inferred |  |
| `FUN_2388_09e8` | 20163 | 32 | platform | unknown | inferred |  |
| `FUN_2388_0a12` | 20195 | 43 | platform | unknown | inferred |  |
| `FUN_2388_0a6a` | 20238 | 17 | platform | unknown | inferred |  |
| `FUN_2388_0a88` | 20255 | 17 | platform | unknown | inferred |  |
| `FUN_2388_0aa6` | 20272 | 13 | platform | unknown | inferred |  |
| `FUN_2388_0ab4` | 20285 | 19 | platform | unknown | inferred |  |
| `FUN_2388_0abf` | 20304 | 19 | platform | unknown | inferred |  |
| `FUN_2388_0af2` | 20323 | 25 | platform | unknown | inferred |  |
| `FUN_2388_0af9` | 20348 | 25 | platform | unknown | inferred |  |
| `FUN_2388_0b22` | 20373 | 65 | platform | unknown | inferred |  |
| `FUN_2388_0bbc` | 20438 | 12 | platform | unknown | inferred |  |
| `FUN_2388_0bee` | 20450 | 56 | platform | unknown | inferred |  |
| `FUN_2388_0c4a` | 20506 | 51 | platform | unknown | inferred |  |
| `FUN_2388_0ca8` | 20557 | 35 | platform | unknown | inferred |  |
| `FUN_2388_0cd6` | 20592 | 34 | platform | unknown | inferred |  |
| `FUN_2388_0d1c` | 20626 | 49 | platform | unknown | inferred |  |
| `FUN_2388_0d58` | 20675 | 33 | platform | unknown | inferred |  |
| `FUN_2388_0d82` | 20708 | 34 | platform | unknown | inferred |  |
| `FUN_2388_0db0` | 20742 | 18 | platform | unknown | inferred |  |
| `FUN_2388_0dd4` | 20760 | 20 | platform | unknown | inferred |  |
| `FUN_2388_0dec` | 20780 | 50 | platform | unknown | inferred |  |
| `FUN_2388_0e22` | 20830 | 61 | platform | unknown | inferred |  |
| `FUN_2388_0e68` | 20891 | 48 | platform | unknown | inferred |  |
| `FUN_2388_0eb0` | 20939 | 96 | platform | unknown | inferred |  |
| `FUN_2388_0f7a` | 21035 | 15 | platform | unknown | inferred |  |
| `FUN_2388_0f9c` | 21050 | 9 | platform | unknown | inferred |  |
| `FUN_2388_0fa2` | 21059 | 33 | platform | unknown | inferred |  |
| `FUN_2388_0fc6` | 21092 | 259 | platform | unknown | inferred |  |
| `FUN_2388_1158` | 21351 | 82 | platform | unknown | inferred |  |
| `FUN_2388_11d6` | 21433 | 29 | platform | unknown | inferred |  |
| `FUN_2388_1201` | 21462 | 30 | platform | unknown | inferred |  |
| `FUN_2388_1238` | 21492 | 14 | platform | unknown | inferred |  |
| `FUN_2388_1240` | 21506 | 15 | platform | unknown | inferred |  |
| `FUN_2388_124d` | 21521 | 13 | platform | unknown | inferred |  |
| `FUN_2388_1260` | 21534 | 32 | platform | unknown | inferred |  |
| `FUN_2388_128e` | 21566 | 49 | platform | unknown | inferred |  |
| `FUN_2388_1324` | 21615 | 58 | platform | unknown | inferred |  |
| `FUN_2388_1408` | 21673 | 18 | platform | unknown | inferred |  |
| `FUN_2388_1434` | 21691 | 76 | platform | unknown | inferred |  |
| `FUN_2388_151c` | 21767 | 35 | platform | unknown | inferred |  |
| `FUN_2388_158f` | 21802 | 20 | platform | unknown | inferred |  |
| `FUN_2388_15ce` | 21822 | 32 | platform | unknown | inferred |  |
| `FUN_2388_164a` | 21854 | 30 | platform | unknown | inferred |  |
| `FUN_2388_16a6` | 21884 | 29 | platform | unknown | inferred |  |
| `FUN_2388_1b7e` | 21913 | 24 | platform | unknown | inferred |  |
| `FUN_2388_1bb2` | 21937 | 21 | platform | unknown | inferred |  |
| `FUN_2388_1bd2` | 21958 | 54 | platform | unknown | inferred |  |
| `FUN_2388_1c4c` | 22012 | 99 | platform | unknown | inferred |  |
| `FUN_2388_1d36` | 22111 | 95 | platform | unknown | inferred |  |
| `FUN_2388_1dea` | 22206 | 30 | platform | unknown | inferred |  |
| `FUN_2388_1e34` | 22236 | 9 | platform | unknown | inferred |  |
| `FUN_2388_1e42` | 22245 | 19 | platform | unknown | inferred |  |
| `FUN_2388_1e76` | 22264 | 34 | platform | unknown | inferred |  |
| `FUN_2388_1eca` | 22298 | 16 | platform | unknown | inferred |  |
| `FUN_2388_1ef6` | 22314 | 80 | platform | unknown | inferred |  |
| `FUN_2388_206c` | 22394 | 48 | platform | unknown | inferred |  |
| `FUN_2388_212c` | 22442 | 37 | platform | unknown | inferred |  |
| `FUN_2388_2158` | 22479 | 70 | platform | unknown | inferred |  |
| `FUN_2388_21b8` | 22549 | 22 | platform | unknown | inferred |  |
| `FUN_2388_21de` | 22571 | 26 | platform | unknown | inferred |  |
| `FUN_2388_2222` | 22597 | 136 | platform | unknown | inferred |  |
| `FUN_2388_23cd` | 22733 | 8 | platform | unknown | inferred |  |
| `FUN_2388_23de` | 22741 | 57 | platform | unknown | inferred |  |
| `FUN_2388_23fe` | 22798 | 18 | platform | unknown | inferred |  |
| `FUN_2388_241f` | 22816 | 22 | platform | unknown | inferred |  |
| `FUN_2388_2448` | 22838 | 65 | platform | unknown | inferred |  |
| `FUN_2388_24d4` | 22903 | 32 | platform | unknown | inferred |  |
| `FUN_2388_2525` | 22935 | 20 | platform | unknown | inferred |  |
| `FUN_2388_2546` | 22955 | 58 | platform | unknown | inferred |  |

