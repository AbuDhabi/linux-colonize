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
| `FUN_1000_002c` | 399 | 17 | ui | unknown | inferred |  |
| `FUN_1000_0062` | 416 | 33 | ui | unknown | inferred |  |

### Segment `1009` (15 defs) — ui — Timed turn-chrome / status text overlays

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1009_0004` | 449 | 20 | ui | unknown | inferred |  |
| `FUN_1009_0036` | 469 | 39 | ui | unknown | inferred |  |
| `FUN_1009_00b4` | 508 | 42 | ui | unknown | inferred |  |
| `FUN_1009_017e` | 550 | 10 | ui | unknown | inferred |  |
| `FUN_1009_01a2` | 560 | 13 | ui | unknown | inferred |  |
| `FUN_1009_01b8` | 573 | 12 | ui | unknown | inferred |  |
| `FUN_1009_01d8` | 585 | 12 | ui | unknown | inferred |  |
| `FUN_1009_01fc` | 597 | 13 | ui | unknown | inferred |  |
| `FUN_1009_0222` | 610 | 16 | ui | unknown | inferred |  |
| `FUN_1009_0244` | 626 | 19 | ui | unknown | inferred |  |
| `FUN_1009_0270` | 645 | 23 | ui | unknown | inferred |  |
| `FUN_1009_02ae` | 668 | 14 | ui | unknown | inferred |  |
| `FUN_1009_02cc` | 682 | 42 | ui | unknown | inferred |  |
| `FUN_1009_0402` | 724 | 10 | ui | unknown | inferred |  |
| `FUN_1009_0420` | 734 | 9 | ui | unknown | inferred |  |

### Segment `104b` (29 defs) — ui — Text / number blit helpers (1d1d_11b4)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_104b_0010` | 743 | 14 | ui | unknown | inferred |  |
| `FUN_104b_0032` | 757 | 9 | ui | unknown | inferred |  |
| `FUN_104b_0042` | 766 | 9 | ui | unknown | inferred |  |
| `FUN_104b_0052` | 775 | 9 | ui | unknown | inferred |  |
| `FUN_104b_0062` | 784 | 9 | ui | unknown | inferred |  |
| `FUN_104b_0072` | 793 | 9 | ui | unknown | inferred |  |
| `FUN_104b_0082` | 802 | 9 | ui | unknown | inferred |  |
| `FUN_104b_0092` | 811 | 9 | ui | unknown | inferred |  |
| `FUN_104b_00a2` | 820 | 9 | ui | unknown | inferred |  |
| `FUN_104b_00b2` | 829 | 9 | ui | unknown | inferred |  |
| `FUN_104b_00c2` | 838 | 9 | ui | unknown | inferred |  |
| `FUN_104b_00d2` | 847 | 9 | ui | unknown | inferred |  |
| `FUN_104b_00e2` | 856 | 10 | ui | unknown | inferred |  |
| `FUN_104b_00fc` | 866 | 12 | ui | unknown | inferred |  |
| `FUN_104b_012e` | 878 | 12 | ui | unknown | inferred |  |
| `FUN_104b_0156` | 890 | 27 | ui | unknown | inferred |  |
| `FUN_104b_01be` | 917 | 13 | ui | unknown | inferred |  |
| `FUN_104b_01e8` | 930 | 10 | ui | unknown | inferred |  |
| `FUN_104b_0216` | 940 | 12 | ui | unknown | inferred |  |
| `FUN_104b_0232` | 952 | 12 | ui | unknown | inferred |  |
| `FUN_104b_024e` | 964 | 13 | ui | unknown | inferred |  |
| `FUN_104b_0288` | 977 | 14 | ui | unknown | inferred |  |
| `FUN_104b_02c2` | 991 | 16 | ui | unknown | inferred |  |
| `FUN_104b_0318` | 1007 | 17 | ui | unknown | inferred |  |
| `FUN_104b_035c` | 1024 | 12 | ui | unknown | inferred |  |
| `FUN_104b_039a` | 1036 | 12 | ui | unknown | inferred |  |
| `FUN_104b_03d2` | 1048 | 15 | ui | unknown | inferred |  |
| `FUN_104b_0430` | 1063 | 17 | ui | unknown | inferred |  |
| `FUN_104b_0478` | 1080 | 23 | ui | unknown | inferred |  |

### Segment `1097` (7 defs) — ui — Multi-item dialog spacing / number layout

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1097_0004` | 1103 | 59 | ui | unknown | inferred |  |
| `FUN_1097_00de` | 1162 | 25 | ui | unknown | inferred |  |
| `FUN_1097_0174` | 1187 | 73 | ui | unknown | inferred |  |
| `FUN_1097_02da` | 1260 | 40 | ui | unknown | inferred |  |
| `FUN_1097_0394` | 1300 | 139 | ui | unknown | inferred |  |
| `FUN_1097_067a` | 1439 | 11 | ui | unknown | inferred |  |
| `FUN_1097_0682` | 1450 | 21 | ui | unknown | inferred |  |

### Segment `1101` (6 defs) — ui — 16-row glyph/bitmap blit helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1101_000e` | 1471 | 9 | ui | unknown | inferred |  |
| `FUN_1101_0026` | 1480 | 15 | ui | unknown | inferred |  |
| `FUN_1101_0050` | 1495 | 35 | ui | unknown | inferred |  |
| `FUN_1101_00b4` | 1530 | 45 | ui | unknown | inferred |  |
| `FUN_1101_0126` | 1575 | 45 | ui | unknown | inferred |  |
| `FUN_1101_01dc` | 1620 | 55 | ui | unknown | inferred |  |

### Segment `112b` (8 defs) — mapdraw — Unit orders/allegiance chrome (FUN_112b_01ba)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_112b_0002` | 1675 | 282 | mapdraw | unknown | inferred |  |
| `FUN_112b_0060` | 1957 | 40 | mapdraw | unknown | inferred |  |
| `FUN_112b_010e` | 1997 | 33 | mapdraw | unknown | inferred |  |
| `FUN_112b_015c` | 2030 | 19 | mapdraw | unknown | inferred |  |
| `FUN_112b_01ba` | 2049 | 265 | mapdraw | Unit chrome: silhouette + nation orders box + letter (+ stack under-rect) | known | docs/assets.md |
| `FUN_112b_0790` | 2314 | 183 | mapdraw | unknown | inferred |  |
| `FUN_112b_0c64` | 2497 | 102 | mapdraw | unknown | inferred |  |
| `FUN_112b_0eb6` | 2599 | 132 | mapdraw | unknown | inferred |  |

### Segment `124c` (7 defs) — platform — Small helpers (e.g. DOS distance)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_124c_000c` | 2731 | 14 | platform | unknown | inferred |  |
| `FUN_124c_002a` | 2745 | 14 | platform | unknown | inferred |  |
| `FUN_124c_0040` | 2759 | 17 | platform | DOS distance helper | known | ai/accessors.c |
| `FUN_124c_007c` | 2776 | 20 | platform | unknown | inferred |  |
| `FUN_124c_00c4` | 2796 | 17 | platform | unknown | inferred |  |
| `FUN_124c_00f4` | 2813 | 20 | platform | unknown | inferred |  |
| `FUN_124c_013c` | 2833 | 14 | platform | unknown | inferred |  |

### Segment `1262` (10 defs) — ui — Input wait / mouse hit-test / tip overlays

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1262_0002` | 2847 | 8 | ui | unknown | inferred |  |
| `FUN_1262_0012` | 2855 | 13 | ui | unknown | inferred |  |
| `FUN_1262_003c` | 2868 | 19 | ui | unknown | inferred |  |
| `FUN_1262_0060` | 2887 | 42 | ui | unknown | inferred |  |
| `FUN_1262_00da` | 2929 | 15 | ui | unknown | inferred |  |
| `FUN_1262_00f6` | 2944 | 15 | ui | unknown | inferred |  |
| `FUN_1262_0128` | 2959 | 14 | ui | unknown | inferred |  |
| `FUN_1262_0142` | 2973 | 9 | ui | unknown | inferred |  |
| `FUN_1262_0152` | 2982 | 70 | ui | unknown | inferred |  |
| `FUN_1262_02fe` | 3052 | 71 | ui | unknown | inferred |  |

### Segment `129f` (7 defs) — sound — BGM helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_129f_0008` | 3123 | 1295 | sound | unknown | inferred |  |
| `FUN_129f_00f6` | 4418 | 444 | sound | unknown | inferred |  |
| `FUN_129f_02cc` | 4862 | 19 | sound | unknown | inferred |  |
| `FUN_129f_0300` | 4881 | 11 | sound | unknown | inferred |  |
| `FUN_129f_030c` | 4892 | 11 | sound | unknown | inferred |  |
| `FUN_129f_0318` | 4903 | 17 | sound | unknown | inferred |  |
| `FUN_129f_034c` | 4920 | 15 | sound | unknown | inferred |  |

### Segment `12d6` (1 defs) — ui — Mouse-gated blit to VGA A000

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12d6_0000` | 4935 | 15 | ui | unknown | inferred |  |

### Segment `12d8` (1 defs) — sound — BGM / event / SFX gating

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12d8_000e` | 4950 | 16 | sound | BGM / event / SFX gating | known | src/core/sound.c |

### Segment `12dd` (2 defs) — ui — Clipped blit dispatch (rect vs raw)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12dd_0002` | 4966 | 21 | ui | unknown | inferred |  |
| `FUN_12dd_0064` | 4987 | 20 | ui | unknown | inferred |  |

### Segment `12e9` (2 defs) — ui — Buffer fill via pitch helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12e9_0006` | 5007 | 8 | ui | unknown | inferred |  |
| `FUN_12e9_008c` | 5015 | 55 | ui | unknown | inferred |  |

### Segment `12fd` (3 defs) — ui — Once-only discovery/event dispatch (bitset DS:0x540a)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_12fd_000e` | 5070 | 23 | ui | unknown | inferred |  |
| `FUN_12fd_0048` | 5093 | 19 | ui | unknown | inferred |  |
| `FUN_12fd_006c` | 5112 | 1034 | ui | unknown | inferred |  |

### Segment `130d` (5 defs) — turn — Main game year/turn loop + intro splash

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_130d_000a` | 6146 | 74 | turn | unknown | inferred |  |
| `FUN_130d_0172` | 6220 | 18 | turn | unknown | inferred |  |
| `FUN_130d_019e` | 6238 | 24 | turn | unknown | inferred |  |
| `FUN_130d_0222` | 6262 | 21 | turn | unknown | inferred |  |
| `FUN_130d_0290` | 6283 | 236 | turn | Main game year/turn loop (nations, year/season, chrome) | known |  |

### Segment `137f` (26 defs) — mapgen — Map plane accessors (terrain/layer2/3)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_137f_000a` | 6519 | 16 | mapgen | unknown | inferred |  |
| `FUN_137f_003c` | 6535 | 26 | mapgen | unknown | inferred |  |
| `FUN_137f_00c0` | 6561 | 18 | mapgen | unknown | inferred |  |
| `FUN_137f_00f6` | 6579 | 10 | mapgen | unknown | inferred |  |
| `FUN_137f_010e` | 6589 | 11 | mapgen | terrain_byte map accessor | known | ai/accessors.c |
| `FUN_137f_012a` | 6600 | 10 | mapgen | unknown | inferred |  |
| `FUN_137f_0142` | 6610 | 11 | mapgen | layer2_byte map accessor | known | ai/accessors.c |
| `FUN_137f_015e` | 6621 | 19 | mapgen | unknown | inferred |  |
| `FUN_137f_0194` | 6640 | 10 | mapgen | layer3_ptr map accessor | known | ai/accessors.c |
| `FUN_137f_01ac` | 6650 | 11 | mapgen | layer3_byte map accessor | known | ai/accessors.c |
| `FUN_137f_01ca` | 6661 | 11 | mapgen | continent_id real body | known | ai/accessors.c |
| `FUN_137f_01dc` | 6672 | 15 | mapgen | unknown | inferred |  |
| `FUN_137f_0200` | 6687 | 16 | mapgen | unknown | inferred |  |
| `FUN_137f_0228` | 6703 | 26 | mapgen | set_owner_nibble | known | ai/accessors.c |
| `FUN_137f_02a0` | 6729 | 20 | mapgen | unknown | inferred |  |
| `FUN_137f_02e0` | 6749 | 10 | mapgen | unknown | inferred |  |
| `FUN_137f_02f8` | 6759 | 11 | mapgen | unknown | inferred |  |
| `FUN_137f_0314` | 6770 | 23 | mapgen | unknown | inferred |  |
| `FUN_137f_0358` | 6793 | 21 | mapgen | euro_settlement_owner real body | known | ai/accessors.c; ai/move_spent.c |
| `FUN_137f_0392` | 6814 | 26 | mapgen | unknown | inferred |  |
| `FUN_137f_03e4` | 6840 | 23 | mapgen | unknown | inferred |  |
| `FUN_137f_0428` | 6863 | 14 | mapgen | unknown | inferred |  |
| `FUN_137f_044a` | 6877 | 28 | mapgen | unknown | inferred |  |
| `FUN_137f_04b0` | 6905 | 44 | mapgen | unknown | inferred |  |
| `FUN_137f_0598` | 6949 | 25 | mapgen | unknown | inferred |  |
| `FUN_137f_0614` | 6974 | 28 | mapgen | unknown | inferred |  |

### Segment `13e4` (4 defs) — mapgen — Terrain class / ocean helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_13e4_000e` | 7002 | 13 | mapgen | decode_terrain_class | known | ai/accessors.c |
| `FUN_13e4_003a` | 7015 | 18 | mapgen | unknown | inferred |  |
| `FUN_13e4_0074` | 7033 | 14 | mapgen | ocean_or_high_seas real body | known | ai/accessors.c |
| `FUN_13e4_00a2` | 7047 | 15 | mapgen | unknown | inferred |  |

### Segment `13f1` (5 defs) — mapdraw — Exploration-bit / fog reveal around units & colonies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_13f1_000a` | 7062 | 34 | mapdraw | unknown | inferred |  |
| `FUN_13f1_00a6` | 7096 | 37 | mapdraw | unknown | inferred |  |
| `FUN_13f1_0158` | 7133 | 75 | mapdraw | unknown | inferred |  |
| `FUN_13f1_02b4` | 7208 | 20 | mapdraw | unknown | inferred |  |
| `FUN_13f1_02f8` | 7228 | 13 | mapdraw | Reveal exploration bits around unit (post-spawn/move) | inferred | ai/accessors.c |

### Segment `1427` (55 defs) — mixed — Tile / unit display and MP chrome

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1427_0002` | 7241 | 20 | mapdraw | unknown | inferred |  |
| `FUN_1427_0026` | 7261 | 20 | mapdraw | unknown | inferred |  |
| `FUN_1427_004a` | 7281 | 14 | mapdraw | unknown | inferred |  |
| `FUN_1427_005c` | 7295 | 45 | mapdraw | unknown | inferred |  |
| `FUN_1427_012e` | 7340 | 26 | mapdraw | unknown | inferred |  |
| `FUN_1427_0164` | 7366 | 16 | mapdraw | unknown | inferred |  |
| `FUN_1427_0180` | 7382 | 27 | mapdraw | unknown | inferred |  |
| `FUN_1427_01dc` | 7409 | 17 | mapdraw | unknown | inferred |  |
| `FUN_1427_0204` | 7426 | 20 | mapdraw | unknown | inferred |  |
| `FUN_1427_023a` | 7446 | 32 | mapdraw | unknown | inferred |  |
| `FUN_1427_02ca` | 7478 | 35 | mapdraw | unknown | inferred |  |
| `FUN_1427_0362` | 7513 | 10 | mapdraw | unknown | inferred |  |
| `FUN_1427_037e` | 7523 | 12 | mapdraw | unknown | inferred |  |
| `FUN_1427_03a0` | 7535 | 30 | mapdraw | unknown | inferred |  |
| `FUN_1427_040c` | 7565 | 17 | ai | stack_set_xy post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_043e` | 7582 | 30 | mapdraw | unknown | inferred |  |
| `FUN_1427_04d6` | 7612 | 69 | mapdraw | unknown | inferred |  |
| `FUN_1427_0644` | 7681 | 10 | mapdraw | unknown | inferred |  |
| `FUN_1427_065a` | 7691 | 19 | mapdraw | Tile display (reads DS 0x5234); also unit_max_mp real body | known | ai/unit_mp.c; docs/viceroy_tables.md |
| `FUN_1427_06b4` | 7710 | 66 | mapdraw | unknown | inferred |  |
| `FUN_1427_0824` | 7776 | 67 | mapdraw | unknown | inferred |  |
| `FUN_1427_08ea` | 7843 | 24 | mapdraw | unknown | inferred |  |
| `FUN_1427_0954` | 7867 | 15 | mapdraw | unknown | inferred |  |
| `FUN_1427_0968` | 7882 | 15 | ai | stack_facing_refresh post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_0992` | 7897 | 15 | mapdraw | unknown | inferred |  |
| `FUN_1427_09ac` | 7912 | 15 | mapdraw | unknown | inferred |  |
| `FUN_1427_09dc` | 7927 | 45 | mapdraw | unknown | inferred |  |
| `FUN_1427_0ab0` | 7972 | 25 | mapdraw | unknown | inferred |  |
| `FUN_1427_0b08` | 7997 | 37 | mapdraw | unknown | inferred |  |
| `FUN_1427_0bce` | 8034 | 16 | mapdraw | unknown | inferred |  |
| `FUN_1427_0bfe` | 8050 | 31 | mapdraw | unknown | inferred |  |
| `FUN_1427_0c72` | 8081 | 16 | ai | unit_visibility_bits post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_0c9a` | 8097 | 26 | mapdraw | unknown | inferred |  |
| `FUN_1427_0ce6` | 8123 | 21 | ai | unit_post_move_chrome | known | ai/unit_mp.c |
| `FUN_1427_0d1e` | 8144 | 17 | mapdraw | unknown | inferred |  |
| `FUN_1427_0d38` | 8161 | 297 | mapdraw | unknown | inferred |  |
| `FUN_1427_0f0e` | 8458 | 11 | mapdraw | unknown | inferred |  |
| `FUN_1427_0f30` | 8469 | 22 | mapdraw | unknown | inferred |  |
| `FUN_1427_0f64` | 8491 | 10 | mapdraw | unknown | inferred |  |
| `FUN_1427_0f74` | 8501 | 13 | mapdraw | unknown | inferred |  |
| `FUN_1427_0f8e` | 8514 | 10 | mapdraw | unknown | inferred |  |
| `FUN_1427_0fa0` | 8524 | 11 | mapdraw | unknown | inferred |  |
| `FUN_1427_0fc0` | 8535 | 15 | mapdraw | unknown | inferred |  |
| `FUN_1427_0fec` | 8550 | 16 | mapdraw | unknown | inferred |  |
| `FUN_1427_101c` | 8566 | 40 | mapdraw | unknown | inferred |  |
| `FUN_1427_10be` | 8606 | 78 | mapdraw | unknown | inferred |  |
| `FUN_1427_1284` | 8684 | 22 | mapdraw | unknown | inferred |  |
| `FUN_1427_12c6` | 8706 | 16 | mapdraw | unknown | inferred |  |
| `FUN_1427_12f6` | 8722 | 19 | ai | unit_tile_list_refresh post-ADD chrome | known | ai/unit_mp.c |
| `FUN_1427_1330` | 8741 | 25 | mapdraw | unknown | inferred |  |
| `FUN_1427_13b0` | 8766 | 20 | ai | unit_has_moves_remaining (behind 281f_097a) | known | ai/unit_mp.c |
| `FUN_1427_1410` | 8786 | 31 | mapdraw | unknown | inferred |  |
| `FUN_1427_14a0` | 8817 | 30 | mapdraw | unknown | inferred |  |
| `FUN_1427_14f4` | 8847 | 33 | mapdraw | unknown | inferred |  |
| `FUN_1427_155e` | 8880 | 13 | ai | unit_exhaust_mp (behind 281f_0934) | known | ai/unit_mp.c |

### Segment `157e` (3 defs) — combat — Unit combat strength / engagement modifiers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_157e_0008` | 8893 | 21 | combat | unknown | inferred |  |
| `FUN_157e_004a` | 8914 | 54 | combat | unknown | inferred |  |
| `FUN_157e_015e` | 8968 | 88 | combat | unknown | inferred |  |

### Segment `15b3` (10 defs) — trade — Nation bilateral flags / name tables (Euro 0x13c, Indian 0x4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_15b3_0004` | 9056 | 13 | trade | unknown | inferred |  |
| `FUN_15b3_0032` | 9069 | 15 | trade | unknown | inferred |  |
| `FUN_15b3_0066` | 9084 | 18 | trade | unknown | inferred |  |
| `FUN_15b3_00d0` | 9102 | 18 | trade | unknown | inferred |  |
| `FUN_15b3_0144` | 9120 | 18 | trade | unknown | inferred |  |
| `FUN_15b3_0198` | 9138 | 21 | trade | unknown | inferred |  |
| `FUN_15b3_01e0` | 9159 | 21 | trade | unknown | inferred |  |
| `FUN_15b3_0228` | 9180 | 13 | trade | unknown | inferred |  |
| `FUN_15b3_024e` | 9193 | 13 | trade | unknown | inferred |  |
| `FUN_15b3_0274` | 9206 | 13 | trade | unknown | inferred |  |

### Segment `15dc` (5 defs) — ai — Tribe / Indian current-context setters & lookups

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_15dc_0006` | 9219 | 16 | ai | unknown | inferred |  |
| `FUN_15dc_0032` | 9235 | 18 | ai | Bind current tribe/Indian context from tribe index | known | include/viceroy_globals.h |
| `FUN_15dc_006a` | 9253 | 18 | ai | unknown | inferred |  |
| `FUN_15dc_00a2` | 9271 | 17 | ai | unknown | inferred |  |
| `FUN_15dc_00e0` | 9288 | 10 | ai | unknown | inferred |  |

### Segment `15eb` (107 defs) — mapdraw — High-density map / pedia draw paths

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_15eb_0002` | 9298 | 12 | mapdraw | unknown | inferred |  |
| `FUN_15eb_002c` | 9310 | 30 | mapdraw | unknown | inferred |  |
| `FUN_15eb_00a2` | 9340 | 40 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0142` | 9380 | 45 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0218` | 9425 | 11 | mapdraw | unknown | inferred |  |
| `FUN_15eb_022c` | 9436 | 12 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0242` | 9448 | 12 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0258` | 9460 | 11 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0274` | 9471 | 34 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0302` | 9505 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0326` | 9515 | 22 | mapdraw | unknown | inferred |  |
| `FUN_15eb_035e` | 9537 | 13 | mapdraw | unknown | inferred |  |
| `FUN_15eb_038e` | 9550 | 11 | mapdraw | unknown | inferred |  |
| `FUN_15eb_039e` | 9561 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_03d6` | 9581 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0410` | 9601 | 13 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0434` | 9614 | 22 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0470` | 9636 | 14 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0484` | 9650 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_04c0` | 9670 | 30 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0544` | 9700 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0556` | 9710 | 27 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0596` | 9737 | 9 | mapdraw | unknown | inferred |  |
| `FUN_15eb_05b2` | 9746 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_05cc` | 9756 | 13 | mapdraw | unknown | inferred |  |
| `FUN_15eb_05e2` | 9769 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0620` | 9789 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0668` | 9808 | 16 | mapdraw | unknown | inferred |  |
| `FUN_15eb_06a6` | 9824 | 17 | mapdraw | unknown | inferred |  |
| `FUN_15eb_06d2` | 9841 | 85 | mapdraw | Shared world-map / pedia draw entry | known | src/core/map.c |
| `FUN_15eb_08e6` | 9926 | 13 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0902` | 9939 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0916` | 9949 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0924` | 9959 | 48 | mapdraw | unknown | inferred |  |
| `FUN_15eb_09c0` | 10007 | 36 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0a50` | 10043 | 15 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0a76` | 10058 | 29 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0aec` | 10087 | 15 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0b0c` | 10102 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0b52` | 10122 | 21 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0b96` | 10143 | 16 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0bd4` | 10159 | 28 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0c52` | 10187 | 15 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0c7a` | 10202 | 18 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0cbc` | 10220 | 28 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0d04` | 10248 | 108 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0d8e` | 10356 | 692 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0e18` | 11048 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0e52` | 11067 | 16 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0e8c` | 11083 | 21 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0ed4` | 11104 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0f1c` | 11124 | 39 | mapdraw | unknown | inferred |  |
| `FUN_15eb_0fea` | 11163 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1030` | 11182 | 22 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1068` | 11204 | 134 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1376` | 11338 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_13ac` | 11358 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_13e2` | 11378 | 22 | mapdraw | unknown | inferred |  |
| `FUN_15eb_142a` | 11400 | 22 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1476` | 11422 | 22 | mapdraw | unknown | inferred |  |
| `FUN_15eb_14aa` | 11444 | 15 | mapdraw | unknown | inferred |  |
| `FUN_15eb_14d6` | 11459 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_14e4` | 11469 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1526` | 11488 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1568` | 11507 | 29 | mapdraw | unknown | inferred |  |
| `FUN_15eb_15c6` | 11536 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1604` | 11555 | 21 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1646` | 11576 | 25 | mapdraw | unknown | inferred |  |
| `FUN_15eb_169c` | 11601 | 16 | mapdraw | unknown | inferred |  |
| `FUN_15eb_16c4` | 11617 | 23 | mapdraw | unknown | inferred |  |
| `FUN_15eb_16fe` | 11640 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_173e` | 11660 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1782` | 11679 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_17ba` | 11699 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_17fa` | 11718 | 53 | mapdraw | unknown | inferred |  |
| `FUN_15eb_18ec` | 11771 | 234 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1d4c` | 12005 | 469 | mapdraw | unknown | inferred |  |
| `FUN_15eb_1f72` | 12474 | 221 | mapdraw | unknown | inferred |  |
| `FUN_15eb_23f2` | 12695 | 109 | mapdraw | unknown | inferred |  |
| `FUN_15eb_268e` | 12804 | 22 | mapdraw | unknown | inferred |  |
| `FUN_15eb_26e4` | 12826 | 63 | mapdraw | unknown | inferred |  |
| `FUN_15eb_287e` | 12889 | 19 | mapdraw | unknown | inferred |  |
| `FUN_15eb_28c8` | 12908 | 254 | mapdraw | unknown | inferred |  |
| `FUN_15eb_2ea0` | 13162 | 39 | mapdraw | unknown | inferred |  |
| `FUN_15eb_2f3c` | 13201 | 20 | mapdraw | unknown | inferred |  |
| `FUN_15eb_2f8e` | 13221 | 23 | mapdraw | unknown | inferred |  |
| `FUN_15eb_2ff2` | 13244 | 18 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3040` | 13262 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3054` | 13272 | 11 | mapdraw | unknown | inferred |  |
| `FUN_15eb_306a` | 13283 | 18 | mapdraw | unknown | inferred |  |
| `FUN_15eb_30b8` | 13301 | 38 | mapdraw | unknown | inferred |  |
| `FUN_15eb_317c` | 13339 | 28 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3208` | 13367 | 30 | mapdraw | unknown | inferred |  |
| `FUN_15eb_32a0` | 13397 | 26 | mapdraw | unknown | inferred |  |
| `FUN_15eb_32f8` | 13423 | 31 | mapdraw | unknown | inferred |  |
| `FUN_15eb_334a` | 13454 | 24 | mapdraw | unknown | inferred |  |
| `FUN_15eb_33aa` | 13478 | 40 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3454` | 13518 | 76 | mapdraw | unknown | inferred |  |
| `FUN_15eb_35d0` | 13594 | 29 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3620` | 13623 | 18 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3650` | 13641 | 111 | mapdraw | unknown | inferred |  |
| `FUN_15eb_38ba` | 13752 | 21 | mapdraw | unknown | inferred |  |
| `FUN_15eb_38e8` | 13773 | 21 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3930` | 13794 | 18 | mapdraw | unknown | inferred |  |
| `FUN_15eb_394c` | 13812 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3956` | 13822 | 10 | mapdraw | unknown | inferred |  |
| `FUN_15eb_3960` | 13832 | 16 | mapdraw | unknown | inferred |  |

### Segment `1984` (18 defs) — turn — Turn-owner chrome

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1984_0040` | 13848 | 12 | turn | unknown | inferred |  |
| `FUN_1984_006a` | 13860 | 13 | turn | unknown | inferred |  |
| `FUN_1984_00aa` | 13873 | 13 | turn | Nation turn-owner 5x3 at (315,197) | known | src/core/turn.c |
| `FUN_1984_00e8` | 13886 | 16 | turn | unknown | inferred |  |
| `FUN_1984_010a` | 13902 | 49 | turn | unknown | inferred |  |
| `FUN_1984_029e` | 13951 | 27 | turn | unknown | inferred |  |
| `FUN_1984_02fc` | 13978 | 43 | turn | unknown | inferred |  |
| `FUN_1984_03b2` | 14021 | 9 | turn | unknown | inferred |  |
| `FUN_1984_03ca` | 14030 | 25 | turn | unknown | inferred |  |
| `FUN_1984_043a` | 14055 | 10 | turn | unknown | inferred |  |
| `FUN_1984_045a` | 14065 | 11 | turn | unknown | inferred |  |
| `FUN_1984_046e` | 14076 | 11 | turn | unknown | inferred |  |
| `FUN_1984_0490` | 14087 | 21 | turn | unknown | inferred |  |
| `FUN_1984_04f6` | 14108 | 13 | turn | unknown | inferred |  |
| `FUN_1984_053a` | 14121 | 17 | turn | unknown | inferred |  |
| `FUN_1984_05b8` | 14138 | 17 | turn | unknown | inferred |  |
| `FUN_1984_0636` | 14155 | 17 | turn | unknown | inferred |  |
| `FUN_1984_06b4` | 14172 | 8 | turn | unknown | inferred |  |

### Segment `19ef` (4 defs) — platform — DOS LCG range helper

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_19ef_0008` | 14180 | 12 | platform | unknown | inferred |  |
| `FUN_19ef_001a` | 14192 | 9 | platform | unknown | inferred |  |
| `FUN_19ef_002c` | 14201 | 9 | platform | unknown | inferred |  |
| `FUN_19ef_0032` | 14210 | 18 | platform | DOS LCG range helper | known | src/core/dos_rng.c |

### Segment `19f6` (4 defs) — ui — Decimal number format / localized string blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_19f6_0002` | 14228 | 44 | ui | unknown | inferred |  |
| `FUN_19f6_00b0` | 14272 | 19 | ui | unknown | inferred |  |
| `FUN_19f6_00fa` | 14291 | 12 | ui | unknown | inferred |  |
| `FUN_19f6_0138` | 14303 | 9 | ui | unknown | inferred |  |

### Segment `1a0a` (3 defs) — ui — VGA page-flip / palette-cycle animation

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a0a_0004` | 14312 | 30 | ui | unknown | inferred |  |
| `FUN_1a0a_007a` | 14342 | 83 | ui | unknown | inferred |  |
| `FUN_1a0a_01a6` | 14425 | 22 | ui | unknown | inferred |  |

### Segment `1a29` (4 defs) — platform — DOS timer INT vector install / restore

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a29_015b` | 14447 | 33 | platform | unknown | inferred |  |
| `FUN_1a29_01d1` | 14480 | 21 | platform | unknown | inferred |  |
| `FUN_1a29_0209` | 14501 | 12 | platform | unknown | inferred |  |
| `FUN_1a29_021b` | 14513 | 18 | platform | unknown | inferred |  |

### Segment `1a4e` (2 defs) — ui — Blit pitch offset + viewport rect clip

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a4e_0008` | 14531 | 13 | ui | unknown | inferred |  |
| `FUN_1a4e_001c` | 14544 | 37 | ui | unknown | inferred |  |

### Segment `1a58` (18 defs) — platform — Mouse driver INT 33 show/hide / poll / mode setup

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1a58_000d` | 14581 | 36 | platform | unknown | inferred |  |
| `FUN_1a58_0054` | 14617 | 28 | platform | unknown | inferred |  |
| `FUN_1a58_008c` | 14645 | 81 | platform | Mouse INT 33 init / cursor mode setup (DS:0x83ac) | known |  |
| `FUN_1a58_01d9` | 14726 | 15 | platform | unknown | inferred |  |
| `FUN_1a58_026c` | 14741 | 58 | platform | unknown | inferred |  |
| `FUN_1a58_02ce` | 14799 | 14 | platform | unknown | inferred |  |
| `FUN_1a58_02e0` | 14813 | 22 | platform | unknown | inferred |  |
| `FUN_1a58_036b` | 14835 | 19 | platform | unknown | inferred |  |
| `FUN_1a58_038b` | 14854 | 36 | platform | unknown | inferred |  |
| `FUN_1a58_03ce` | 14890 | 13 | platform | unknown | inferred |  |
| `FUN_1a58_03e2` | 14903 | 16 | platform | unknown | inferred |  |
| `FUN_1a58_042d` | 14919 | 13 | platform | unknown | inferred |  |
| `FUN_1a58_0456` | 14932 | 47 | platform | unknown | inferred |  |
| `FUN_1a58_054f` | 14979 | 14 | platform | unknown | inferred |  |
| `FUN_1a58_0568` | 14993 | 18 | platform | unknown | inferred |  |
| `FUN_1a58_0599` | 15011 | 15 | platform | unknown | inferred |  |
| `FUN_1a58_05be` | 15026 | 21 | platform | unknown | inferred |  |
| `FUN_1a58_06fd` | 15047 | 13 | platform | unknown | inferred |  |

### Segment `1acb` (4 defs) — ui — Mouse hit-rect / button edge tracking

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1acb_0006` | 15060 | 17 | ui | unknown | inferred |  |
| `FUN_1acb_0030` | 15077 | 18 | ui | unknown | inferred |  |
| `FUN_1acb_0056` | 15095 | 55 | ui | unknown | inferred |  |
| `FUN_1acb_011a` | 15150 | 20 | ui | unknown | inferred |  |

### Segment `1ade` (1 defs) — ui — VGA vsync + DAC palette write

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ade_0004` | 15170 | 42 | ui | Wait VGA retrace then program DAC RGB palette via 0x3c8/0x3c9 | known |  |

### Segment `1ae3` (2 defs) — platform — Stack clear + BIOS INT16 key-ready

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ae3_0006` | 15212 | 24 | platform | unknown | inferred |  |
| `FUN_1ae3_0042` | 15236 | 16 | platform | unknown | inferred |  |

### Segment `1ae7` (2 defs) — platform — BIOS INT16 keyboard read / queue flush

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1ae7_0016` | 15252 | 32 | platform | BIOS INT16 read next key (AH/AL); paired with 1ae3 key-ready / flush | known |  |
| `FUN_1ae7_0032` | 15284 | 21 | platform | unknown | inferred |  |

### Segment `1aea` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1aea_000c` | 15305 | 919 | unknown | unknown | unknown |  |

### Segment `1afb` (2 defs) — platform — String LF-terminate / NUL truncate

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1afb_000e` | 16224 | 15 | platform | unknown | inferred |  |
| `FUN_1afb_003c` | 16239 | 20 | platform | unknown | inferred |  |

### Segment `1b01` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b01_000e` | 16259 | 114 | unknown | unknown | unknown |  |

### Segment `1b22` (1 defs) — platform — DOS file-exists probe (INT21 3D/3E) + INT24 wrap

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b22_0022` | 16373 | 55 | platform | DOS file-exists probe (INT21 3D/3E) with INT24 critical-error handler wrap | known |  |

### Segment `1b2c` (2 defs) — platform — DOS char-stream string get / put

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b2c_0004` | 16428 | 24 | platform | unknown | inferred |  |
| `FUN_1b2c_0040` | 16452 | 21 | platform | unknown | inferred |  |

### Segment `1b32` (2 defs) — platform — Filename extension / basename helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b32_000e` | 16473 | 18 | platform | unknown | inferred |  |
| `FUN_1b32_005c` | 16491 | 23 | platform | unknown | inferred |  |

### Segment `1b4e` (1 defs) — platform — Path join: cwd + \ + name

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b4e_0004` | 16514 | 25 | platform | unknown | inferred |  |

### Segment `1b57` (1 defs) — platform — Ltrim spaces/tabs then strcpy

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b57_0002` | 16539 | 23 | platform | unknown | inferred |  |

### Segment `1b5e` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b5e_0000` | 16562 | 63 | unknown | unknown | unknown |  |

### Segment `1b70` (2 defs) — ui — Mouse viewport / region setup (1a58)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b70_0002` | 16625 | 13 | ui | unknown | inferred |  |
| `FUN_1b70_003a` | 16638 | 17 | ui | unknown | inferred |  |

### Segment `1b78` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b78_0000` | 16655 | 52 | unknown | unknown | unknown |  |

### Segment `1b8b` (1 defs) — platform — Set BIOS equipment video-mode bits (40:10)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b8b_0004` | 16707 | 16 | platform | unknown | inferred |  |

### Segment `1b8d` (1 defs) — ui — Solid-rect fill thunk → 1b9e

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b8d_0004` | 16723 | 12 | ui | unknown | inferred |  |

### Segment `1b8f` (1 defs) — ui — Pitched buffer rect blit/copy

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b8f_0006` | 16735 | 105 | ui | unknown | inferred |  |

### Segment `1b9e` (1 defs) — ui — Solid-color pitched rect fill

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1b9e_000a` | 16840 | 80 | ui | unknown | inferred |  |

### Segment `1baa` (1 defs) — ui — Pitched buffer rect blit/copy (variant)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1baa_0006` | 16920 | 101 | ui | unknown | inferred |  |

### Segment `1bb9` (1 defs) — ui — Put pixel via pitch helper (1a4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bb9_000a` | 17021 | 14 | ui | unknown | inferred |  |

### Segment `1bbb` (1 defs) — ui — Get pixel via pitch helper (1a4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bbb_0006` | 17035 | 13 | ui | unknown | inferred |  |

### Segment `1bbc` (1 defs) — ui — Horizontal span fill in pitched buffer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bbc_000c` | 17048 | 31 | ui | unknown | inferred |  |

### Segment `1bc3` (1 defs) — ui — Vertical span fill in pitched buffer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bc3_0006` | 17079 | 31 | ui | unknown | inferred |  |

### Segment `1bca` (1 defs) — ui — Rect outline via H/V span fills

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bca_0002` | 17110 | 18 | ui | unknown | inferred |  |

### Segment `1bd4` (1 defs) — ui — Color replace in pitched rect

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bd4_0006` | 17128 | 40 | ui | unknown | inferred |  |

### Segment `1bdd` (2 defs) — platform — Temp numbered file create / write slots

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bdd_0002` | 17168 | 59 | platform | unknown | inferred |  |
| `FUN_1bdd_00e0` | 17227 | 39 | platform | unknown | inferred |  |

### Segment `1bf5` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1bf5_0000` | 17266 | 52 | unknown | unknown | unknown |  |

### Segment `1c05` (1 defs) — platform — Normalize far pointer (seg:off)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c05_0004` | 17318 | 8 | platform | unknown | inferred |  |

### Segment `1c06` (1 defs) — platform — Parse 0x/0b numeric literal prefix

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c06_000c` | 17326 | 27 | platform | unknown | inferred |  |

### Segment `1c0c` (3 defs) — platform — Timer / tick word readers (custom + BIOS 046c)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c0c_0006` | 17353 | 12 | platform | unknown | inferred |  |
| `FUN_1c0c_0012` | 17365 | 8 | platform | unknown | inferred |  |
| `FUN_1c0c_0022` | 17373 | 10 | platform | unknown | inferred |  |

### Segment `1c10` (1 defs) — platform — Program PIT channel-0 divisor

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c10_0008` | 17383 | 14 | platform | Program PIT channel-0 divisor via ports 0x43/0x40 | known |  |

### Segment `1c11` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c11_000c` | 17397 | 128 | unknown | unknown | unknown |  |

### Segment `1c28` (1 defs) — ui — Store text-draw color words at DS:269e

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c28_000a` | 17525 | 17 | ui | unknown | inferred |  |

### Segment `1c2a` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c2a_0006` | 17542 | 30 | unknown | unknown | unknown |  |

### Segment `1c2e` (2 defs) — ui — VGA vsync wait + DAC palette write

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c2e_000e` | 17572 | 16 | ui | unknown | inferred |  |
| `FUN_1c2e_0022` | 17588 | 46 | ui | Wait VGA retrace then program DAC RGB palette via 0x3c8/0x3c9 | known |  |

### Segment `1c36` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c36_000a` | 17634 | 176 | unknown | unknown | unknown |  |

### Segment `1c56` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c56_0004` | 17810 | 231 | unknown | unknown | unknown |  |

### Segment `1c83` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c83_0002` | 18041 | 28 | unknown | unknown | unknown |  |

### Segment `1c89` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1c89_0006` | 18069 | 176 | unknown | unknown | unknown |  |

### Segment `1caa` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1caa_0004` | 18245 | 231 | unknown | unknown | unknown |  |

### Segment `1cd8` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1cd8_0004` | 18476 | 178 | unknown | unknown | unknown |  |

### Segment `1cf8` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1cf8_000a` | 18654 | 88 | unknown | unknown | unknown |  |

### Segment `1d05` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d05_0000` | 18742 | 85 | unknown | unknown | unknown |  |

### Segment `1d11` (2 defs) — ui — INT10 mode set + Mode13h far-buffer blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d11_0000` | 18827 | 17 | ui | unknown | inferred |  |
| `FUN_1d11_001c` | 18844 | 70 | ui | unknown | inferred |  |

### Segment `1d1c` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d1c_0000` | 18914 | 36 | unknown | unknown | unknown |  |

### Segment `1d1d` (127 defs) — platform — High-density + platform-adjacent (incl. LCG)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_1d1d_0016` | 18950 | 10 | platform | unknown | inferred |  |
| `FUN_1d1d_00ca` | 18960 | 23 | platform | unknown | inferred |  |
| `FUN_1d1d_00ff` | 18983 | 38 | platform | unknown | inferred |  |
| `FUN_1d1d_0132` | 19021 | 11 | platform | unknown | inferred |  |
| `FUN_1d1d_0150` | 19032 | 62 | platform | unknown | inferred |  |
| `FUN_1d1d_0223` | 19094 | 107 | platform | unknown | inferred |  |
| `FUN_1d1d_0248` | 19201 | 94 | platform | unknown | inferred |  |
| `FUN_1d1d_030d` | 19295 | 22 | platform | unknown | inferred |  |
| `FUN_1d1d_0390` | 19317 | 20 | platform | unknown | inferred |  |
| `FUN_1d1d_03bd` | 19337 | 25 | platform | unknown | inferred |  |
| `FUN_1d1d_03d0` | 19362 | 28 | platform | unknown | inferred |  |
| `FUN_1d1d_03f4` | 19390 | 41 | platform | unknown | inferred |  |
| `FUN_1d1d_04ae` | 19431 | 18 | platform | unknown | inferred |  |
| `FUN_1d1d_04da` | 19449 | 9 | platform | unknown | inferred |  |
| `FUN_1d1d_04f0` | 19458 | 14 | platform | unknown | inferred |  |
| `FUN_1d1d_0528` | 19472 | 72 | platform | unknown | inferred |  |
| `FUN_1d1d_060c` | 19544 | 70 | platform | unknown | inferred |  |
| `FUN_1d1d_0712` | 19614 | 14 | platform | unknown | inferred |  |
| `FUN_1d1d_0750` | 19628 | 20 | platform | unknown | inferred |  |
| `FUN_1d1d_0758` | 19648 | 20 | platform | unknown | inferred |  |
| `FUN_1d1d_0786` | 19668 | 25 | platform | unknown | inferred |  |
| `FUN_1d1d_07a4` | 19693 | 56 | platform | unknown | inferred |  |
| `FUN_1d1d_07e4` | 19749 | 46 | platform | unknown | inferred |  |
| `FUN_1d1d_0816` | 19795 | 44 | platform | unknown | inferred |  |
| `FUN_1d1d_0842` | 19839 | 19 | platform | unknown | inferred |  |
| `FUN_1d1d_085e` | 19858 | 46 | platform | unknown | inferred |  |
| `FUN_1d1d_0894` | 19904 | 30 | platform | unknown | inferred |  |
| `FUN_1d1d_08bc` | 19934 | 77 | platform | unknown | inferred |  |
| `FUN_1d1d_08fa` | 20011 | 61 | platform | unknown | inferred |  |
| `FUN_1d1d_0916` | 20072 | 9 | platform | unknown | inferred |  |
| `FUN_1d1d_092c` | 20081 | 13 | platform | unknown | inferred |  |
| `FUN_1d1d_0942` | 20094 | 24 | platform | unknown | inferred |  |
| `FUN_1d1d_09a2` | 20118 | 22 | platform | unknown | inferred |  |
| `FUN_1d1d_09ca` | 20140 | 65 | platform | unknown | inferred |  |
| `FUN_1d1d_0a3e` | 20205 | 35 | platform | unknown | inferred |  |
| `FUN_1d1d_0abe` | 20240 | 11 | platform | unknown | inferred |  |
| `FUN_1d1d_0ad8` | 20251 | 21 | platform | unknown | inferred |  |
| `FUN_1d1d_0b1c` | 20272 | 21 | platform | unknown | inferred |  |
| `FUN_1d1d_0b48` | 20293 | 28 | platform | unknown | inferred |  |
| `FUN_1d1d_0ba2` | 20321 | 30 | platform | unknown | inferred |  |
| `FUN_1d1d_0c56` | 20351 | 32 | platform | unknown | inferred |  |
| `FUN_1d1d_0c80` | 20383 | 31 | platform | unknown | inferred |  |
| `FUN_1d1d_0cc2` | 20414 | 43 | platform | unknown | inferred |  |
| `FUN_1d1d_0d1a` | 20457 | 31 | platform | unknown | inferred |  |
| `FUN_1d1d_0d46` | 20488 | 17 | platform | unknown | inferred |  |
| `FUN_1d1d_0d64` | 20505 | 17 | platform | unknown | inferred |  |
| `FUN_1d1d_0d82` | 20522 | 37 | platform | unknown | inferred |  |
| `FUN_1d1d_0dae` | 20559 | 31 | platform | unknown | inferred |  |
| `FUN_1d1d_0ddc` | 20590 | 11 | platform | unknown | inferred |  |
| `FUN_1d1d_0df2` | 20601 | 12 | platform | unknown | inferred |  |
| `FUN_1d1d_0e04` | 20613 | 16 | platform | DOS LCG core | known | src/core/dos_rng.c |
| `FUN_1d1d_0e2c` | 20629 | 12 | platform | unknown | inferred |  |
| `FUN_1d1d_0e4a` | 20641 | 13 | platform | unknown | inferred |  |
| `FUN_1d1d_0e58` | 20654 | 19 | platform | unknown | inferred |  |
| `FUN_1d1d_0e63` | 20673 | 19 | platform | unknown | inferred |  |
| `FUN_1d1d_0e96` | 20692 | 25 | platform | unknown | inferred |  |
| `FUN_1d1d_0e9d` | 20717 | 25 | platform | unknown | inferred |  |
| `FUN_1d1d_0ec6` | 20742 | 65 | platform | unknown | inferred |  |
| `FUN_1d1d_0f60` | 20807 | 12 | platform | unknown | inferred |  |
| `FUN_1d1d_0f92` | 20819 | 14 | platform | unknown | inferred |  |
| `FUN_1d1d_0fb2` | 20833 | 51 | platform | unknown | inferred |  |
| `FUN_1d1d_1010` | 20884 | 35 | platform | unknown | inferred |  |
| `FUN_1d1d_103e` | 20919 | 34 | platform | unknown | inferred |  |
| `FUN_1d1d_1084` | 20953 | 49 | platform | unknown | inferred |  |
| `FUN_1d1d_10c0` | 21002 | 33 | platform | unknown | inferred |  |
| `FUN_1d1d_10ea` | 21035 | 34 | platform | unknown | inferred |  |
| `FUN_1d1d_1118` | 21069 | 18 | platform | unknown | inferred |  |
| `FUN_1d1d_113c` | 21087 | 20 | platform | unknown | inferred |  |
| `FUN_1d1d_117e` | 21107 | 50 | platform | unknown | inferred |  |
| `FUN_1d1d_11b4` | 21157 | 61 | platform | unknown | inferred |  |
| `FUN_1d1d_11fa` | 21218 | 48 | platform | unknown | inferred |  |
| `FUN_1d1d_1242` | 21266 | 15 | platform | unknown | inferred |  |
| `FUN_1d1d_1264` | 21281 | 9 | platform | unknown | inferred |  |
| `FUN_1d1d_126a` | 21290 | 33 | platform | unknown | inferred |  |
| `FUN_1d1d_128e` | 21323 | 259 | platform | unknown | inferred |  |
| `FUN_1d1d_1420` | 21582 | 82 | platform | unknown | inferred |  |
| `FUN_1d1d_149e` | 21664 | 29 | platform | unknown | inferred |  |
| `FUN_1d1d_14c9` | 21693 | 30 | platform | unknown | inferred |  |
| `FUN_1d1d_1500` | 21723 | 14 | platform | unknown | inferred |  |
| `FUN_1d1d_1508` | 21737 | 15 | platform | unknown | inferred |  |
| `FUN_1d1d_1515` | 21752 | 13 | platform | unknown | inferred |  |
| `FUN_1d1d_1528` | 21765 | 32 | platform | unknown | inferred |  |
| `FUN_1d1d_1556` | 21797 | 49 | platform | unknown | inferred |  |
| `FUN_1d1d_15ec` | 21846 | 58 | platform | unknown | inferred |  |
| `FUN_1d1d_16d0` | 21904 | 18 | platform | unknown | inferred |  |
| `FUN_1d1d_16fc` | 21922 | 76 | platform | unknown | inferred |  |
| `FUN_1d1d_17e4` | 21998 | 35 | platform | unknown | inferred |  |
| `FUN_1d1d_1857` | 22033 | 20 | platform | unknown | inferred |  |
| `FUN_1d1d_1896` | 22053 | 32 | platform | unknown | inferred |  |
| `FUN_1d1d_1912` | 22085 | 30 | platform | unknown | inferred |  |
| `FUN_1d1d_196e` | 22115 | 28 | platform | unknown | inferred |  |
| `FUN_1d1d_1d79` | 22143 | 15 | platform | unknown | inferred |  |
| `FUN_1d1d_1d81` | 22158 | 19 | platform | unknown | inferred |  |
| `FUN_1d1d_1daa` | 22177 | 31 | platform | unknown | inferred |  |
| `FUN_1d1d_1dd4` | 22208 | 26 | platform | unknown | inferred |  |
| `FUN_1d1d_1df2` | 22234 | 26 | platform | unknown | inferred |  |
| `FUN_1d1d_1e0e` | 22260 | 34 | platform | unknown | inferred |  |
| `FUN_1d1d_1e3f` | 22294 | 8 | platform | unknown | inferred |  |
| `FUN_1d1d_1e46` | 22302 | 24 | platform | unknown | inferred |  |
| `FUN_1d1d_1e7a` | 22326 | 21 | platform | unknown | inferred |  |
| `FUN_1d1d_1e9a` | 22347 | 54 | platform | unknown | inferred |  |
| `FUN_1d1d_1f14` | 22401 | 99 | platform | unknown | inferred |  |
| `FUN_1d1d_1ffe` | 22500 | 95 | platform | unknown | inferred |  |
| `FUN_1d1d_20b2` | 22595 | 30 | platform | unknown | inferred |  |
| `FUN_1d1d_20fc` | 22625 | 9 | platform | unknown | inferred |  |
| `FUN_1d1d_210a` | 22634 | 19 | platform | unknown | inferred |  |
| `FUN_1d1d_213e` | 22653 | 65 | platform | unknown | inferred |  |
| `FUN_1d1d_21ca` | 22718 | 32 | platform | unknown | inferred |  |
| `FUN_1d1d_221b` | 22750 | 20 | platform | unknown | inferred |  |
| `FUN_1d1d_223c` | 22770 | 34 | platform | unknown | inferred |  |
| `FUN_1d1d_2290` | 22804 | 80 | platform | unknown | inferred |  |
| `FUN_1d1d_2406` | 22884 | 48 | platform | unknown | inferred |  |
| `FUN_1d1d_2526` | 22932 | 70 | platform | unknown | inferred |  |
| `FUN_1d1d_2586` | 23002 | 48 | platform | unknown | inferred |  |
| `FUN_1d1d_26dc` | 23050 | 22 | platform | unknown | inferred |  |
| `FUN_1d1d_2702` | 23072 | 26 | platform | unknown | inferred |  |
| `FUN_1d1d_2746` | 23098 | 136 | platform | unknown | inferred |  |
| `FUN_1d1d_28f1` | 23234 | 8 | platform | unknown | inferred |  |
| `FUN_1d1d_2902` | 23242 | 57 | platform | unknown | inferred |  |
| `FUN_1d1d_2922` | 23299 | 59 | platform | unknown | inferred |  |
| `FUN_1d1d_299e` | 23358 | 156 | platform | unknown | inferred |  |
| `FUN_1d1d_2b32` | 23514 | 54 | platform | unknown | inferred |  |
| `FUN_1d1d_2c44` | 23568 | 18 | platform | unknown | inferred |  |
| `FUN_1d1d_2c65` | 23586 | 25 | platform | unknown | inferred |  |
| `FUN_1d1d_2c8e` | 23611 | 168 | platform | unknown | inferred |  |
| `FUN_1d1d_2f06` | 23779 | 312 | platform | unknown | inferred |  |
| `FUN_1d1d_328a` | 24091 | 30 | platform | unknown | inferred |  |

### Segment `2047` (5 defs) — platform — DOS Ctrl-C/Break / INT21 abort handlers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2047_000a` | 24121 | 64 | platform | unknown | inferred |  |
| `FUN_2047_00b8` | 24185 | 32 | platform | unknown | inferred |  |
| `FUN_2047_00e9` | 24217 | 15 | platform | unknown | inferred |  |
| `FUN_2047_0106` | 24232 | 20 | platform | unknown | inferred |  |
| `FUN_2047_011f` | 24252 | 10 | platform | unknown | inferred |  |

### Segment `2059` (3 defs) — sound — Sound driver jump table

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2059_0006` | 24262 | 13 | sound | unknown | inferred |  |
| `FUN_2059_000a` | 24275 | 20 | sound | Sound driver jump table | known | src/core/sound.c |
| `FUN_2059_005f` | 24295 | 13 | sound | unknown | inferred |  |

### Segment `205f` (2 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_205f_000c` | 24308 | 31 | unknown | unknown | unknown |  |
| `FUN_205f_0046` | 24339 | 30 | unknown | unknown | unknown |  |

### Segment `206d` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_206d_000a` | 24369 | 52 | unknown | unknown | unknown |  |

### Segment `2074` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2074_0008` | 24421 | 56 | unknown | unknown | unknown |  |

### Segment `2088` (2 defs) — platform — XMS detect (INT2F) + init handle table

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2088_000c` | 24477 | 20 | platform | unknown | inferred |  |
| `FUN_2088_0048` | 24497 | 46 | platform | Detect XMS via INT2F and install multiplex entry at DS:0x26e8 | known |  |

### Segment `2094` (1 defs) — platform — Rtrim trailing spaces/tabs

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2094_000a` | 24543 | 27 | platform | unknown | inferred |  |

### Segment `209a` (1 defs) — platform — Parse hex integer from string

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_209a_000a` | 24570 | 44 | platform | unknown | inferred |  |

### Segment `20a0` (1 defs) — platform — Parse binary integer from string

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_20a0_000e` | 24614 | 34 | platform | unknown | inferred |  |

### Segment `2100` (1 defs) — platform — XMS size query via multiplex entry

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2100_000e` | 24648 | 19 | platform | unknown | inferred |  |

### Segment `2103` (2 defs) — platform — XMS handle alloc / free list

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2103_000a` | 24667 | 24 | platform | unknown | inferred |  |
| `FUN_2103_004c` | 24691 | 26 | platform | unknown | inferred |  |

### Segment `210d` (116 defs) — platform — DOS/EMS runtime: INT 21/67, bank switch, overlay page helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_210d_051a` | 24717 | 44 | platform | unknown | inferred |  |
| `FUN_210d_0727` | 24761 | 301 | platform | unknown | inferred |  |
| `FUN_210d_0d91` | 25062 | 70 | platform | EMS overlay page-in helper (thunks call before far JMP) | inferred | ai/unit_mp.c; ai/accessors.c |
| `FUN_210d_0dab` | 25132 | 90 | platform | EMS overlay page-in helper (thunks call before far JMP) | inferred | ai/unit_mp.c; ai/accessors.c |
| `FUN_210d_1268` | 25222 | 51 | platform | unknown | inferred |  |
| `FUN_210d_12ad` | 25273 | 27 | platform | unknown | inferred |  |
| `FUN_210d_1341` | 25300 | 76 | platform | unknown | inferred |  |
| `FUN_210d_1407` | 25376 | 33 | platform | unknown | inferred |  |
| `FUN_210d_167b` | 25409 | 9 | platform | unknown | inferred |  |
| `FUN_210d_1695` | 25418 | 22 | platform | unknown | inferred |  |
| `FUN_210d_1ac4` | 25440 | 40 | platform | unknown | inferred |  |
| `FUN_210d_1bc4` | 25480 | 46 | platform | unknown | inferred |  |
| `FUN_210d_1c43` | 25526 | 21 | platform | unknown | inferred |  |
| `FUN_210d_1c61` | 25547 | 11 | platform | unknown | inferred |  |
| `FUN_210d_1c75` | 25558 | 11 | platform | unknown | inferred |  |
| `FUN_210d_1d49` | 25569 | 37 | platform | unknown | inferred |  |
| `FUN_210d_1dbd` | 25606 | 24 | platform | unknown | inferred |  |
| `FUN_210d_2019` | 25630 | 29 | platform | unknown | inferred |  |
| `FUN_210d_238c` | 25659 | 8 | platform | unknown | inferred |  |
| `FUN_210d_238e` | 25667 | 16 | platform | unknown | inferred |  |
| `FUN_210d_239d` | 25683 | 69 | platform | unknown | inferred |  |
| `FUN_210d_2492` | 25752 | 59 | platform | unknown | inferred |  |
| `FUN_210d_2590` | 25811 | 64 | platform | unknown | inferred |  |
| `FUN_210d_2607` | 25875 | 49 | platform | unknown | inferred |  |
| `FUN_210d_265a` | 25924 | 14 | platform | unknown | inferred |  |
| `FUN_210d_2684` | 25938 | 154 | platform | unknown | inferred |  |
| `FUN_210d_2a48` | 26092 | 17 | platform | unknown | inferred |  |
| `FUN_210d_2a6e` | 26109 | 24 | platform | unknown | inferred |  |
| `FUN_210d_2aa5` | 26133 | 14 | platform | unknown | inferred |  |
| `FUN_210d_2ae5` | 26147 | 28 | platform | unknown | inferred |  |
| `FUN_210d_2b22` | 26175 | 42 | platform | unknown | inferred |  |
| `FUN_210d_2b9b` | 26217 | 41 | platform | unknown | inferred |  |
| `FUN_210d_2c57` | 26258 | 54 | platform | unknown | inferred |  |
| `FUN_210d_2d5a` | 26312 | 57 | platform | unknown | inferred |  |
| `FUN_210d_2e59` | 26369 | 29 | platform | unknown | inferred |  |
| `FUN_210d_2e78` | 26398 | 56 | platform | unknown | inferred |  |
| `FUN_210d_2fd2` | 26454 | 40 | platform | unknown | inferred |  |
| `FUN_210d_3018` | 26494 | 19 | platform | unknown | inferred |  |
| `FUN_210d_302e` | 26513 | 19 | platform | unknown | inferred |  |
| `FUN_210d_3046` | 26532 | 35 | platform | unknown | inferred |  |
| `FUN_210d_3080` | 26567 | 9 | platform | unknown | inferred |  |
| `FUN_210d_3094` | 26576 | 78 | platform | unknown | inferred |  |
| `FUN_210d_3147` | 26654 | 19 | platform | unknown | inferred |  |
| `FUN_210d_3179` | 26673 | 32 | platform | unknown | inferred |  |
| `FUN_210d_31c4` | 26705 | 48 | platform | unknown | inferred |  |
| `FUN_210d_3254` | 26753 | 65 | platform | unknown | inferred |  |
| `FUN_210d_3322` | 26818 | 38 | platform | unknown | inferred |  |
| `FUN_210d_3367` | 26856 | 25 | platform | unknown | inferred |  |
| `FUN_210d_33e3` | 26881 | 17 | platform | unknown | inferred |  |
| `FUN_210d_3564` | 26898 | 133 | platform | unknown | inferred |  |
| `FUN_210d_3791` | 27031 | 17 | platform | unknown | inferred |  |
| `FUN_210d_37ad` | 27048 | 21 | platform | unknown | inferred |  |
| `FUN_210d_37d9` | 27069 | 40 | platform | unknown | inferred |  |
| `FUN_210d_391d` | 27109 | 13 | platform | unknown | inferred |  |
| `FUN_210d_3a0f` | 27122 | 97 | platform | unknown | inferred |  |
| `FUN_210d_3d9b` | 27219 | 178 | platform | unknown | inferred |  |
| `FUN_210d_3f46` | 27397 | 8 | platform | unknown | inferred |  |
| `FUN_210d_3fb7` | 27405 | 33 | platform | unknown | inferred |  |
| `FUN_210d_402f` | 27438 | 16 | platform | unknown | inferred |  |
| `FUN_210d_4052` | 27454 | 37 | platform | unknown | inferred |  |
| `FUN_210d_409c` | 27491 | 46 | platform | unknown | inferred |  |
| `FUN_210d_40e6` | 27537 | 158 | platform | unknown | inferred |  |
| `FUN_210d_43f0` | 27695 | 43 | platform | unknown | inferred |  |
| `FUN_210d_4454` | 27738 | 34 | platform | unknown | inferred |  |
| `FUN_210d_44db` | 27772 | 22 | platform | unknown | inferred |  |
| `FUN_210d_45d3` | 27794 | 27 | platform | unknown | inferred |  |
| `FUN_210d_45da` | 27821 | 26 | platform | unknown | inferred |  |
| `FUN_210d_45e7` | 27847 | 26 | platform | unknown | inferred |  |
| `FUN_210d_45f1` | 27873 | 80 | platform | unknown | inferred |  |
| `FUN_210d_466e` | 27953 | 48 | platform | unknown | inferred |  |
| `FUN_210d_470f` | 28001 | 26 | platform | unknown | inferred |  |
| `FUN_210d_4751` | 28027 | 29 | platform | unknown | inferred |  |
| `FUN_210d_479f` | 28056 | 124 | platform | unknown | inferred |  |
| `FUN_210d_49c3` | 28180 | 13 | platform | unknown | inferred |  |
| `FUN_210d_49db` | 28193 | 71 | platform | unknown | inferred |  |
| `FUN_210d_4a4c` | 28264 | 152 | platform | unknown | inferred |  |
| `FUN_210d_4c83` | 28416 | 53 | platform | unknown | inferred |  |
| `FUN_210d_4ce3` | 28469 | 47 | platform | unknown | inferred |  |
| `FUN_210d_4d4f` | 28516 | 46 | platform | unknown | inferred |  |
| `FUN_210d_4de3` | 28562 | 20 | platform | unknown | inferred |  |
| `FUN_210d_4e28` | 28582 | 14 | platform | unknown | inferred |  |
| `FUN_210d_4e3b` | 28596 | 15 | platform | unknown | inferred |  |
| `FUN_210d_4e4e` | 28611 | 14 | platform | unknown | inferred |  |
| `FUN_210d_4fe5` | 28625 | 46 | platform | unknown | inferred |  |
| `FUN_210d_508b` | 28671 | 117 | platform | unknown | inferred |  |
| `FUN_210d_5247` | 28788 | 19 | platform | unknown | inferred |  |
| `FUN_210d_52a6` | 28807 | 13 | platform | unknown | inferred |  |
| `FUN_210d_52d3` | 28820 | 14 | platform | unknown | inferred |  |
| `FUN_210d_535c` | 28834 | 9 | platform | unknown | inferred |  |
| `FUN_210d_5365` | 28843 | 9 | platform | unknown | inferred |  |
| `FUN_210d_536e` | 28852 | 24 | platform | unknown | inferred |  |
| `FUN_210d_538c` | 28876 | 22 | platform | unknown | inferred |  |
| `FUN_210d_53b5` | 28898 | 14 | platform | unknown | inferred |  |
| `FUN_210d_53cb` | 28912 | 8 | platform | unknown | inferred |  |
| `FUN_210d_545f` | 28920 | 38 | platform | unknown | inferred |  |
| `FUN_210d_54d0` | 28958 | 26 | platform | unknown | inferred |  |
| `FUN_210d_552c` | 28984 | 115 | platform | unknown | inferred |  |
| `FUN_210d_5699` | 29099 | 35 | platform | unknown | inferred |  |
| `FUN_210d_56e5` | 29134 | 23 | platform | unknown | inferred |  |
| `FUN_210d_5716` | 29157 | 72 | platform | unknown | inferred |  |
| `FUN_210d_5808` | 29229 | 35 | platform | unknown | inferred |  |
| `FUN_210d_5856` | 29264 | 46 | platform | unknown | inferred |  |
| `FUN_210d_58c2` | 29310 | 16 | platform | unknown | inferred |  |
| `FUN_210d_58f7` | 29326 | 10 | platform | unknown | inferred |  |
| `FUN_210d_58f9` | 29336 | 47 | platform | unknown | inferred |  |
| `FUN_210d_597f` | 29383 | 31 | platform | unknown | inferred |  |
| `FUN_210d_59ce` | 29414 | 40 | platform | unknown | inferred |  |
| `FUN_210d_5a76` | 29454 | 15 | platform | unknown | inferred |  |
| `FUN_210d_5a9d` | 29469 | 27 | platform | unknown | inferred |  |
| `FUN_210d_5afc` | 29496 | 8 | platform | unknown | inferred |  |
| `FUN_210d_5b26` | 29504 | 40 | platform | unknown | inferred |  |
| `FUN_210d_5b9d` | 29544 | 23 | platform | unknown | inferred |  |
| `FUN_210d_5bcb` | 29567 | 52 | platform | unknown | inferred |  |
| `FUN_210d_5c9e` | 29619 | 54 | platform | unknown | inferred |  |
| `FUN_210d_5d49` | 29673 | 8 | platform | unknown | inferred |  |
| `FUN_210d_5dd0` | 29681 | 11 | platform | unknown | inferred |  |

### Segment `275d` (21 defs) — platform — DOS PATH/env parse / memory sizing / INT21 helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_275d_0000` | 29692 | 80 | platform | unknown | inferred |  |
| `FUN_275d_023a` | 29772 | 52 | platform | unknown | inferred |  |
| `FUN_275d_02b2` | 29824 | 31 | platform | unknown | inferred |  |
| `FUN_275d_033a` | 29855 | 40 | platform | unknown | inferred |  |
| `FUN_275d_0443` | 29895 | 28 | platform | unknown | inferred |  |
| `FUN_275d_048f` | 29923 | 79 | platform | unknown | inferred |  |
| `FUN_275d_05ed` | 30002 | 34 | platform | unknown | inferred |  |
| `FUN_275d_0624` | 30036 | 16 | platform | unknown | inferred |  |
| `FUN_275d_062d` | 30052 | 30 | platform | unknown | inferred |  |
| `FUN_275d_06b3` | 30082 | 19 | platform | unknown | inferred |  |
| `FUN_275d_06db` | 30101 | 20 | platform | unknown | inferred |  |
| `FUN_275d_0700` | 30121 | 68 | platform | unknown | inferred |  |
| `FUN_275d_07a4` | 30189 | 12 | platform | unknown | inferred |  |
| `FUN_275d_080f` | 30201 | 34 | platform | unknown | inferred |  |
| `FUN_275d_08ad` | 30235 | 56 | platform | unknown | inferred |  |
| `FUN_275d_0909` | 30291 | 124 | platform | unknown | inferred |  |
| `FUN_275d_0a11` | 30415 | 43 | platform | unknown | inferred |  |
| `FUN_275d_0a4f` | 30458 | 40 | platform | unknown | inferred |  |
| `FUN_275d_0ab8` | 30498 | 14 | platform | unknown | inferred |  |
| `FUN_275d_0acc` | 30512 | 35 | platform | unknown | inferred |  |
| `FUN_275d_0b09` | 30547 | 84 | platform | unknown | inferred |  |

### Segment `281f` (371 defs) — mixed — Far thunks: RNG / map / UI fill helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_281f_0000` | 30631 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_000e` | 30641 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0018` | 30651 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0022` | 30661 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_002c` | 30671 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0048` | 30681 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0056` | 30691 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0060` | 30701 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_006a` | 30711 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0074` | 30721 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_007e` | 30731 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0088` | 30741 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0092` | 30751 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_009c` | 30761 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00a6` | 30771 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00b0` | 30781 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00ba` | 30791 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00c4` | 30801 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00ce` | 30811 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00d8` | 30821 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00e2` | 30831 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00ec` | 30841 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_00f6` | 30851 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0100` | 30861 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_010a` | 30871 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0114` | 30881 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_011e` | 30891 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0128` | 30901 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0132` | 30911 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_013c` | 30921 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0146` | 30931 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0150` | 30941 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_015a` | 30951 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0164` | 30961 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_016e` | 30971 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0178` | 30981 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0182` | 30991 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_018c` | 31001 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0196` | 31011 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01a0` | 31021 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01aa` | 31031 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01b4` | 31041 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01be` | 31051 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01c8` | 31061 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01d2` | 31071 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01dc` | 31081 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01e6` | 31091 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01f0` | 31101 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_01fa` | 31111 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0204` | 31121 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_020e` | 31131 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0218` | 31141 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0222` | 31151 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_022c` | 31161 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0236` | 31171 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0240` | 31181 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_024a` | 31191 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0254` | 31201 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_025e` | 31211 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0268` | 31221 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0272` | 31231 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_027c` | 31241 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0286` | 31251 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0290` | 31261 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_029a` | 31271 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02a8` | 31281 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02b2` | 31291 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02bc` | 31301 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02c6` | 31311 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02d0` | 31321 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02da` | 31331 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02e4` | 31341 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02ee` | 31351 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_02f8` | 31361 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0302` | 31371 | 10 | mapgen | map_tile_in_bounds | known | ai/accessors.c |
| `FUN_281f_030c` | 31381 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0316` | 31391 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0322` | 31401 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_032c` | 31411 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_033a` | 31421 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0344` | 31431 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0352` | 31441 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_035c` | 31451 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0366` | 31461 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0370` | 31471 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_037a` | 31481 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_038e` | 31491 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0398` | 31501 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03a2` | 31511 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03ac` | 31521 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03b6` | 31531 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03c0` | 31541 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03ca` | 31551 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03d4` | 31561 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03e0` | 31571 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03ea` | 31581 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03f4` | 31591 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_03fe` | 31601 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_040a` | 31611 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0416` | 31621 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0422` | 31631 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_042e` | 31641 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0438` | 31651 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0444` | 31661 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_044e` | 31671 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_045c` | 31681 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0466` | 31691 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0470` | 31701 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_047a` | 31711 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0484` | 31721 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_048e` | 31731 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0498` | 31741 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04a2` | 31751 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04ac` | 31761 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04b6` | 31771 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04c0` | 31781 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04ca` | 31791 | 10 | platform | Reseed LCG from timer word | known | ai/accessors.c; src/core/ai.c |
| `FUN_281f_04d4` | 31801 | 10 | platform | Wrapped RNG range (calls into libc / LCG) | known | ai/accessors.c; src/core/dos_rng.c |
| `FUN_281f_04de` | 31811 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04e8` | 31821 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04f2` | 31831 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_04fc` | 31841 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0506` | 31851 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0510` | 31861 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_051a` | 31871 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0524` | 31881 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_052e` | 31891 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_053c` | 31901 | 9 | thunk | unknown | inferred |  |
| `FUN_281f_0546` | 31910 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0550` | 31920 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_055e` | 31930 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_056a` | 31940 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0574` | 31950 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0582` | 31960 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0590` | 31970 | 10 | ui | Fill helper (turn box) | known | src/core/turn.c |
| `FUN_281f_059a` | 31980 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05a8` | 31990 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05b6` | 32000 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05c4` | 32010 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05ce` | 32020 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05d8` | 32030 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05e2` | 32040 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05ec` | 32050 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_05fa` | 32060 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0608` | 32070 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0614` | 32080 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_061e` | 32090 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_062c` | 32100 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0638` | 32110 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0644` | 32120 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0652` | 32130 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_065e` | 32140 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0668` | 32150 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0676` | 32160 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0682` | 32170 | 10 | mapgen | tile_owner_or_presence | known | ai/accessors.c |
| `FUN_281f_068c` | 32180 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0696` | 32190 | 10 | mapgen | euro_settlement_owner thunk | known | ai/accessors.c |
| `FUN_281f_06a0` | 32200 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_06aa` | 32210 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_06b4` | 32220 | 10 | mapgen | continent_id thunk | known | ai/accessors.c |
| `FUN_281f_06be` | 32230 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_06c8` | 32240 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_06d2` | 32250 | 10 | mapgen | tile_tribe_or_presence | known | ai/accessors.c |
| `FUN_281f_06dc` | 32260 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_06e6` | 32270 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_06f0` | 32280 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_06fa` | 32290 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0704` | 32300 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_070e` | 32310 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0718` | 32320 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0722` | 32330 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_072c` | 32340 | 10 | mapgen | tile_has_minor_river (via terrain_byte) | known | ai/accessors.c |
| `FUN_281f_0736` | 32350 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0740` | 32360 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_074a` | 32370 | 10 | mapgen | tile_explore_mask | known | ai/accessors.c |
| `FUN_281f_0754` | 32380 | 10 | mapgen | tile_fa_flags thunk | known | ai/accessors.c |
| `FUN_281f_075e` | 32390 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0768` | 32400 | 10 | mapgen | ocean_or_high_seas thunk | known | ai/accessors.c |
| `FUN_281f_0772` | 32410 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_077e` | 32420 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_078c` | 32430 | 10 | mapgen | terrain_class_at | known | ai/accessors.c |
| `FUN_281f_07a0` | 32440 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07aa` | 32450 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07b4` | 32460 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07be` | 32470 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07c8` | 32480 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07d6` | 32490 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07e0` | 32500 | 10 | mapgen | unit_index_on_tile | known | ai/accessors.c |
| `FUN_281f_07ea` | 32510 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07f4` | 32520 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_07fe` | 32530 | 10 | ai | unit_visibility_bits thunk | known | ai/unit_mp.c |
| `FUN_281f_0808` | 32540 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0812` | 32550 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_081c` | 32560 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0826` | 32570 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0830` | 32580 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_083a` | 32590 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0844` | 32600 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_084e` | 32610 | 10 | ai | unit_post_move_chrome thunk | known | ai/unit_mp.c |
| `FUN_281f_0858` | 32620 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0862` | 32630 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_086c` | 32640 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0876` | 32650 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0880` | 32660 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_088a` | 32670 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0894` | 32680 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_089e` | 32690 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08a8` | 32700 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08b2` | 32710 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08bc` | 32720 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08c6` | 32730 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08d0` | 32740 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08da` | 32750 | 10 | ai | stack_facing_refresh thunk | known | ai/unit_mp.c |
| `FUN_281f_08e4` | 32760 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08ee` | 32770 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_08f8` | 32780 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0902` | 32790 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_090c` | 32800 | 10 | ai | unit_max_mp thunk | known | ai/unit_mp.c |
| `FUN_281f_0916` | 32810 | 10 | ai | unit_tile_list_refresh thunk | known | ai/unit_mp.c |
| `FUN_281f_0920` | 32820 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_092a` | 32830 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0934` | 32840 | 20 | ai | unit_exhaust_mp thunk | known | ai/unit_mp.c |
| `FUN_281f_0948` | 32860 | 10 | ai | stack_set_xy thunk | known | ai/unit_mp.c |
| `FUN_281f_0952` | 32870 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_095c` | 32880 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0966` | 32890 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0970` | 32900 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_097a` | 32910 | 10 | ai | unit_has_moves_remaining thunk | known | ai/unit_mp.c |
| `FUN_281f_0984` | 32920 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_098e` | 32930 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0998` | 32940 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09a4` | 32950 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09ae` | 32960 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09ba` | 32970 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09c8` | 32980 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09d2` | 32990 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09dc` | 33000 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09e6` | 33010 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09f0` | 33020 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_09fc` | 33030 | 20 | thunk | unknown | inferred |  |
| `FUN_281f_0a10` | 33050 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a1a` | 33060 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a24` | 33070 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a2e` | 33080 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a38` | 33090 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a42` | 33100 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a4c` | 33110 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a56` | 33120 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a60` | 33130 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a6a` | 33140 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a74` | 33150 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a7e` | 33160 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a88` | 33170 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a92` | 33180 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0a9c` | 33190 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0aa6` | 33200 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ab0` | 33210 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0aba` | 33220 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ac4` | 33230 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ace` | 33240 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ad8` | 33250 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ae2` | 33260 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0aec` | 33270 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0af6` | 33280 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b00` | 33290 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b0a` | 33300 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b14` | 33310 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b1e` | 33320 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b28` | 33330 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b32` | 33340 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b3c` | 33350 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b46` | 33360 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b50` | 33370 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b5a` | 33380 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b64` | 33390 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b6e` | 33400 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b78` | 33410 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b82` | 33420 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b8c` | 33430 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0b96` | 33440 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ba0` | 33450 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0baa` | 33460 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0bb4` | 33470 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0bbe` | 33480 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0bc8` | 33490 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0bd2` | 33500 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0bdc` | 33510 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0be6` | 33520 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0bf0` | 33530 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0bfa` | 33540 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c04` | 33550 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c0e` | 33560 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c18` | 33570 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c22` | 33580 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c2c` | 33590 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c36` | 33600 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c40` | 33610 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c4a` | 33620 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c54` | 33630 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c5e` | 33640 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c68` | 33650 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c72` | 33660 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c7c` | 33670 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c86` | 33680 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c90` | 33690 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0c9a` | 33700 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ca4` | 33710 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0cae` | 33720 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0cb8` | 33730 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0cc2` | 33740 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ccc` | 33750 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0cd6` | 33760 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ce0` | 33770 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0cea` | 33780 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0cf4` | 33790 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0cfe` | 33800 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d08` | 33810 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d12` | 33820 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d1c` | 33830 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d26` | 33840 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d30` | 33850 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d3a` | 33860 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d44` | 33870 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d4e` | 33880 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d58` | 33890 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d62` | 33900 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d6c` | 33910 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d78` | 33920 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d84` | 33930 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0d9a` | 33940 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0da4` | 33950 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0dae` | 33960 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0db8` | 33970 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0dc2` | 33980 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0dcc` | 33990 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0dd6` | 34000 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0de0` | 34010 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0dea` | 34020 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0df4` | 34030 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0dfe` | 34040 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e08` | 34050 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e12` | 34060 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e1c` | 34070 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e2a` | 34080 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e38` | 34090 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e46` | 34100 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e52` | 34110 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e5e` | 34120 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e68` | 34130 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e72` | 34140 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e7c` | 34150 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e86` | 34160 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e90` | 34170 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0e9a` | 34180 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0eae` | 34190 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0eb8` | 34200 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ec2` | 34210 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ecc` | 34220 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ed6` | 34230 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ee0` | 34240 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f24` | 34250 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f30` | 34260 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f3c` | 34270 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f54` | 34280 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f6c` | 34290 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f78` | 34300 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f90` | 34310 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0f9c` | 34320 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0fa8` | 34330 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0fcc` | 34340 | 10 | thunk | unknown | inferred |  |
| `FUN_281f_0ff0` | 34350 | 10 | thunk | unknown | inferred |  |

### Segment `291f` (271 defs) — thunk — Far thunks: EMS page-in then overlay JMPF (2f2b/38fd/6f74/...)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_291f_0018` | 34360 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0030` | 34370 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0048` | 34380 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_006c` | 34390 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0078` | 34400 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0084` | 34410 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0090` | 34420 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_00a8` | 34430 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_00c0` | 34440 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_00cc` | 34450 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_00e4` | 34460 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_00f0` | 34470 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0108` | 34480 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0120` | 34490 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_012c` | 34500 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_013a` | 34510 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0146` | 34520 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0152` | 34530 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_015e` | 34540 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_016a` | 34550 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0176` | 34560 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0182` | 34570 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_018e` | 34580 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_019c` | 34590 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_01a8` | 34600 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_01b6` | 34610 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_01c2` | 34620 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_01d0` | 34630 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_01de` | 34640 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_01fa` | 34650 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0208` | 34660 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0216` | 34670 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0224` | 34680 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0230` | 34690 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_023c` | 34700 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0248` | 34710 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0254` | 34720 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0262` | 34730 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_026e` | 34740 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_027a` | 34750 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0288` | 34760 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0296` | 34770 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_02a4` | 34780 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_02b2` | 34790 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_02c0` | 34800 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_02ce` | 34810 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_02dc` | 34820 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_02ea` | 34830 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_02f8` | 34840 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0306` | 34850 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0320` | 34860 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_032e` | 34870 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_033c` | 34880 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0348` | 34890 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0356` | 34900 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0364` | 34910 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_03aa` | 34920 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_03b8` | 34930 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_03c6` | 34940 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_03d4` | 34950 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_03e2` | 34960 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_03f0` | 34970 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_03fe` | 34980 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_040c` | 34990 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_041a` | 35000 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0428` | 35010 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0436` | 35020 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_044e` | 35030 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_045c` | 35040 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0468` | 35050 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0472` | 35060 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_047e` | 35070 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_048a` | 35080 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0496` | 35090 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_04a2` | 35100 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_04ba` | 35110 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_04d4` | 35120 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_04e0` | 35130 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_04ec` | 35140 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0504` | 35150 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0510` | 35160 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_051c` | 35170 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0534` | 35180 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0540` | 35190 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_054c` | 35200 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0558` | 35210 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0564` | 35220 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_057c` | 35230 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0594` | 35240 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05a0` | 35250 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05ac` | 35260 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05b8` | 35270 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05c4` | 35280 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05d0` | 35290 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05dc` | 35300 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05e8` | 35310 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_05f4` | 35320 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0600` | 35330 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_060c` | 35340 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0618` | 35350 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0630` | 35360 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0648` | 35370 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0654` | 35380 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0660` | 35390 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_066c` | 35400 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0678` | 35410 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0684` | 35420 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0690` | 35430 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_069c` | 35440 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_06b4` | 35450 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_06c0` | 35460 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_06cc` | 35470 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_06d8` | 35480 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_06e4` | 35490 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_06f0` | 35500 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0708` | 35510 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0714` | 35520 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0720` | 35530 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_072c` | 35540 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0738` | 35550 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0744` | 35560 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0750` | 35570 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_075c` | 35580 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0768` | 35590 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0774` | 35600 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_078c` | 35610 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0798` | 35620 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_07b0` | 35630 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_07bc` | 35640 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_07c8` | 35650 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_07d4` | 35660 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_07ec` | 35670 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_07f8` | 35680 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0804` | 35690 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_081c` | 35700 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0828` | 35710 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0834` | 35720 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0840` | 35730 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0858` | 35740 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0864` | 35750 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0870` | 35760 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_087a` | 35770 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0888` | 35780 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0896` | 35790 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08a4` | 35800 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08b2` | 35810 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08bc` | 35820 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08c6` | 35830 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08d2` | 35840 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08de` | 35850 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08ec` | 35860 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_08f8` | 35870 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0902` | 35880 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0910` | 35890 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_091c` | 35900 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0928` | 35910 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0934` | 35920 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0942` | 35930 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0950` | 35940 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_095e` | 35950 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_096c` | 35960 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_097a` | 35970 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0988` | 35980 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0996` | 35990 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_09a4` | 36000 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_09b2` | 36010 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_09c0` | 36020 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_09ce` | 36030 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_09dc` | 36040 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_09ea` | 36050 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_09f8` | 36060 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a06` | 36070 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a14` | 36080 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a20` | 36090 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a2e` | 36100 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a3c` | 36110 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a4a` | 36120 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a58` | 36130 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a66` | 36140 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a74` | 36150 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a82` | 36160 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a90` | 36170 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0a9e` | 36180 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0aac` | 36190 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0aba` | 36200 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ac8` | 36210 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ad4` | 36220 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ae0` | 36230 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0aee` | 36240 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0afc` | 36250 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b0a` | 36260 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b18` | 36270 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b26` | 36280 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b34` | 36290 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b42` | 36300 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b50` | 36310 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b5e` | 36320 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b6c` | 36330 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b7a` | 36340 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b88` | 36350 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0b96` | 36360 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0bb2` | 36370 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0bc0` | 36380 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0bdc` | 36390 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0bea` | 36400 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c06` | 36410 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c14` | 36420 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c22` | 36430 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c30` | 36440 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c3e` | 36450 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c5a` | 36460 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c68` | 36470 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c76` | 36480 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0c84` | 36490 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ca0` | 36500 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0cae` | 36510 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0cbc` | 36520 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0cca` | 36530 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0cd8` | 36540 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ce6` | 36550 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d02` | 36560 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d10` | 36570 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d1e` | 36580 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d2c` | 36590 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d3a` | 36600 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d56` | 36610 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d72` | 36620 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d80` | 36630 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d8e` | 36640 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0d9c` | 36650 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0daa` | 36660 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0db8` | 36670 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0dc6` | 36680 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0dd4` | 36690 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0de2` | 36700 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0df0` | 36710 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0dfe` | 36720 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e0c` | 36730 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e28` | 36740 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e36` | 36750 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e44` | 36760 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e52` | 36770 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e60` | 36780 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e6e` | 36790 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e7c` | 36800 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e8a` | 36810 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0e98` | 36820 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0eb4` | 36830 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ec2` | 36840 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ed0` | 36850 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0eda` | 36860 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ee8` | 36870 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0ef6` | 36880 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f04` | 36890 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f12` | 36900 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f20` | 36910 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f2e` | 36920 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f3c` | 36930 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f4a` | 36940 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f58` | 36950 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f66` | 36960 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f74` | 36970 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f82` | 36980 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f8e` | 36990 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0f9c` | 37000 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0faa` | 37010 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0fb8` | 37020 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0fc4` | 37030 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0fd0` | 37040 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0fde` | 37050 | 10 | thunk | unknown | inferred |  |
| `FUN_291f_0fec` | 37060 | 10 | thunk | unknown | inferred |  |

### Segment `2a1f` (294 defs) — mapgen — Map-gen dispatch / helpers (also Euro act thunks)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2a1f_0000` | 37070 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_000e` | 37080 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_001c` | 37090 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_002a` | 37100 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0038` | 37110 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0046` | 37120 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0054` | 37130 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0062` | 37140 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0070` | 37150 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_007e` | 37160 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_008c` | 37170 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_009a` | 37180 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_00a8` | 37190 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_00b6` | 37200 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_00c4` | 37210 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_00d2` | 37220 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_00e0` | 37230 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_00ee` | 37240 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_00fc` | 37250 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_010a` | 37260 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0118` | 37270 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0126` | 37280 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0134` | 37290 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0142` | 37300 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0150` | 37310 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_015e` | 37320 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_016c` | 37330 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0178` | 37340 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0186` | 37350 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0192` | 37360 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_01a0` | 37370 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_01ae` | 37380 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_01bc` | 37390 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_01ca` | 37400 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_01d8` | 37410 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_01e6` | 37420 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_01f4` | 37430 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0202` | 37440 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0210` | 37450 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_021c` | 37460 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_022a` | 37470 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0238` | 37480 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0246` | 37490 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0254` | 37500 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0262` | 37510 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0270` | 37520 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_027e` | 37530 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_028a` | 37540 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0296` | 37550 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02a2` | 37560 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02ae` | 37570 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02ba` | 37580 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02c6` | 37590 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02d2` | 37600 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02de` | 37610 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02ea` | 37620 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_02f6` | 37630 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0302` | 37640 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_030e` | 37650 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_031a` | 37660 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0326` | 37670 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0332` | 37680 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_033e` | 37690 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_034a` | 37700 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0356` | 37710 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0364` | 37720 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0372` | 37730 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0380` | 37740 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_038a` | 37750 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0398` | 37760 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0434` | 37770 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0440` | 37780 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_044c` | 37790 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0458` | 37800 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0464` | 37810 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0470` | 37820 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_047c` | 37830 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0488` | 37840 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0494` | 37850 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04a0` | 37860 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04ac` | 37870 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04b8` | 37880 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04c4` | 37890 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04d0` | 37900 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04dc` | 37910 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04e8` | 37920 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_04f4` | 37930 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0500` | 37940 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_050c` | 37950 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0518` | 37960 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0524` | 37970 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0530` | 37980 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_053c` | 37990 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0548` | 38000 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0554` | 38010 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0560` | 38020 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_056c` | 38030 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0578` | 38040 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0584` | 38050 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0590` | 38060 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_059c` | 38070 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05a8` | 38080 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05b4` | 38090 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05c0` | 38100 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05cc` | 38110 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05d8` | 38120 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05e4` | 38130 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05f0` | 38140 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_05fc` | 38150 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_060a` | 38160 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0618` | 38170 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0626` | 38180 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0634` | 38190 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0642` | 38200 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0650` | 38210 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_065e` | 38220 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_066c` | 38230 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_067a` | 38240 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0688` | 38250 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0694` | 38260 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_06a2` | 38270 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_06b0` | 38280 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_06bc` | 38290 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_06c8` | 38300 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_06d4` | 38310 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_06e0` | 38320 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_06ec` | 38330 | 19 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0704` | 38349 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0710` | 38359 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_071c` | 38369 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_072a` | 38379 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0738` | 38389 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0746` | 38399 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0754` | 38409 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0762` | 38419 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0770` | 38429 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_077e` | 38439 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_078c` | 38449 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_079a` | 38459 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_07a8` | 38469 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_07b6` | 38479 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_07c4` | 38489 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_07d0` | 38499 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_07dc` | 38509 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_07ea` | 38519 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_07f8` | 38529 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0806` | 38539 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0814` | 38549 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0822` | 38559 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0830` | 38569 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_083e` | 38579 | 10 | mapgen | Dispatches into map-gen pipeline | known | src/core/map_gen.c; docs/assets.md |
| `FUN_2a1f_084c` | 38589 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_085a` | 38599 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0868` | 38609 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0872` | 38619 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_087c` | 38629 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_088a` | 38639 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0896` | 38649 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_08a4` | 38659 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_08b2` | 38669 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_08c0` | 38679 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_08ce` | 38689 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_08dc` | 38699 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_08ea` | 38709 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_08f8` | 38719 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0906` | 38729 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0914` | 38739 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0922` | 38749 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0930` | 38759 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_093e` | 38769 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_094c` | 38779 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_095a` | 38789 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0968` | 38799 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0976` | 38809 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0984` | 38819 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_098e` | 38829 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0998` | 38839 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_09c2` | 38849 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_09d0` | 38859 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_09de` | 38869 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_09ec` | 38879 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_09fa` | 38889 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a08` | 38899 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a16` | 38909 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a24` | 38919 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a32` | 38929 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a40` | 38939 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a4e` | 38949 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a5c` | 38959 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a6a` | 38969 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a78` | 38979 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a86` | 38989 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a94` | 38999 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0a9e` | 39009 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0aaa` | 39019 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ab6` | 39029 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ac2` | 39039 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ace` | 39049 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ada` | 39059 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0af2` | 39069 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0afe` | 39079 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b0a` | 39089 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b16` | 39099 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b22` | 39109 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b2e` | 39119 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b3a` | 39129 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b44` | 39139 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b4e` | 39149 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b58` | 39159 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b66` | 39169 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b74` | 39179 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b82` | 39189 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b90` | 39199 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0b9e` | 39209 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0bac` | 39219 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0bba` | 39229 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0bc8` | 39239 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0bd6` | 39249 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0be4` | 39259 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0bf2` | 39269 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c00` | 39279 | 9 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c1c` | 39288 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c2a` | 39298 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c38` | 39308 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c46` | 39318 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c50` | 39328 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c64` | 39338 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c72` | 39348 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c80` | 39358 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c8e` | 39368 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0c9c` | 39378 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0caa` | 39388 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0cb4` | 39398 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0cbe` | 39408 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ccc` | 39418 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0cda` | 39428 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ce8` | 39438 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0cf6` | 39448 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d04` | 39458 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d12` | 39468 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d20` | 39478 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d2e` | 39488 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d3c` | 39498 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d4a` | 39508 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d58` | 39518 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d66` | 39528 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d74` | 39538 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d82` | 39548 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d90` | 39558 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0d9e` | 39568 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0dac` | 39578 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0dba` | 39588 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0dc8` | 39598 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0dd6` | 39608 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0de4` | 39618 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0dee` | 39628 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0df8` | 39638 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e02` | 39648 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e10` | 39658 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e1e` | 39668 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e28` | 39678 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e36` | 39688 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e40` | 39698 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e4e` | 39708 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e5c` | 39718 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e6a` | 39728 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e78` | 39738 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e82` | 39748 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e90` | 39758 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0e9e` | 39768 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0eac` | 39778 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0eba` | 39788 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ec8` | 39798 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ed6` | 39808 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0ee4` | 39818 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f1a` | 39828 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f26` | 39838 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f30` | 39848 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f3e` | 39858 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f4c` | 39868 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f56` | 39878 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f60` | 39888 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f6a` | 39898 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f74` | 39908 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f7e` | 39918 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f8c` | 39928 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0f96` | 39938 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0fa0` | 39948 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0fae` | 39958 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0fbc` | 39968 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0fc6` | 39978 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0fd4` | 39988 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0fde` | 39998 | 10 | mapgen | unknown | inferred |  |
| `FUN_2a1f_0fe8` | 40008 | 179 | mapgen | unknown | inferred |  |

### Segment `2b5a` (53 defs) — ui — Map selected-unit order / input UI (DS:0x5392)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2b5a_0000` | 40187 | 10 | ui | unknown | inferred |  |
| `FUN_2b5a_001e` | 40197 | 40 | ui | unknown | inferred |  |
| `FUN_2b5a_0070` | 40237 | 1844 | ui | unknown | inferred |  |
| `FUN_2b5a_0722` | 42081 | 24 | ui | unknown | inferred |  |
| `FUN_2b5a_0902` | 42105 | 41 | ui | unknown | inferred |  |
| `FUN_2b5a_0b08` | 42146 | 18 | ui | unknown | inferred |  |
| `FUN_2b5a_0b34` | 42164 | 103 | ui | unknown | inferred |  |
| `FUN_2b5a_0e52` | 42267 | 73 | ui | unknown | inferred |  |
| `FUN_2b5a_0f92` | 42340 | 24 | ui | unknown | inferred |  |
| `FUN_2b5a_1112` | 42364 | 63 | ui | unknown | inferred |  |
| `FUN_2b5a_123e` | 42427 | 93 | ui | unknown | inferred |  |
| `FUN_2b5a_1454` | 42520 | 89 | ui | unknown | inferred |  |
| `FUN_2b5a_199e` | 42609 | 76 | ui | unknown | inferred |  |
| `FUN_2b5a_1b5a` | 42685 | 109 | ui | unknown | inferred |  |
| `FUN_2b5a_1dfc` | 42794 | 31 | ui | unknown | inferred |  |
| `FUN_2b5a_1e66` | 42825 | 47 | ui | unknown | inferred |  |
| `FUN_2b5a_1f36` | 42872 | 38 | ui | unknown | inferred |  |
| `FUN_2b5a_1fc0` | 42910 | 24 | ui | unknown | inferred |  |
| `FUN_2b5a_20f6` | 42934 | 60 | ui | unknown | inferred |  |
| `FUN_2b5a_223a` | 42994 | 64 | ui | unknown | inferred |  |
| `FUN_2b5a_23ce` | 43058 | 62 | ui | unknown | inferred |  |
| `FUN_2b5a_2464` | 43120 | 1672 | ui | unknown | inferred |  |
| `FUN_2b5a_268c` | 44792 | 27 | ui | unknown | inferred |  |
| `FUN_2b5a_26f6` | 44819 | 68 | ui | unknown | inferred |  |
| `FUN_2b5a_2866` | 44887 | 18 | ui | unknown | inferred |  |
| `FUN_2b5a_3036` | 44905 | 9 | ui | unknown | inferred |  |
| `FUN_2b5a_303c` | 44914 | 93 | ui | Map keyboard/input dispatch on DS:0x981e | inferred |  |
| `FUN_2b5a_3094` | 45007 | 44 | ui | unknown | inferred |  |
| `FUN_2b5a_30ce` | 45051 | 31 | ui | unknown | inferred |  |
| `FUN_2b5a_3104` | 45082 | 18 | ui | unknown | inferred |  |
| `FUN_2b5a_311c` | 45100 | 19 | ui | unknown | inferred |  |
| `FUN_2b5a_313e` | 45119 | 22 | ui | unknown | inferred |  |
| `FUN_2b5a_3145` | 45141 | 31 | ui | unknown | inferred |  |
| `FUN_2b5a_3154` | 45172 | 21 | ui | unknown | inferred |  |
| `FUN_2b5a_316e` | 45193 | 21 | ui | unknown | inferred |  |
| `FUN_2b5a_3188` | 45214 | 19 | ui | unknown | inferred |  |
| `FUN_2b5a_3194` | 45233 | 19 | ui | unknown | inferred |  |
| `FUN_2b5a_31be` | 45252 | 21 | ui | unknown | inferred |  |
| `FUN_2b5a_321c` | 45273 | 78 | ui | unknown | inferred |  |
| `FUN_2b5a_3252` | 45351 | 489 | ui | unknown | inferred |  |
| `FUN_2b5a_32a2` | 45840 | 30 | ui | unknown | inferred |  |
| `FUN_2b5a_32ee` | 45870 | 24 | ui | unknown | inferred |  |
| `FUN_2b5a_3344` | 45894 | 33 | ui | unknown | inferred |  |
| `FUN_2b5a_33ce` | 45927 | 25 | ui | unknown | inferred |  |
| `FUN_2b5a_3442` | 45952 | 11 | ui | unknown | inferred |  |
| `FUN_2b5a_3458` | 45963 | 9 | ui | unknown | inferred |  |
| `FUN_2b5a_3462` | 45972 | 150 | ui | unknown | inferred |  |
| `FUN_2b5a_36e6` | 46122 | 22 | ui | unknown | inferred |  |
| `FUN_2b5a_3752` | 46144 | 35 | ui | unknown | inferred |  |
| `FUN_2b5a_37b2` | 46179 | 34 | ui | unknown | inferred |  |
| `FUN_2b5a_3802` | 46213 | 200 | ui | unknown | inferred |  |
| `FUN_2b5a_3ae6` | 46413 | 403 | ui | unknown | inferred |  |
| `FUN_2b5a_3b68` | 46816 | 291 | ui | unknown | inferred |  |

### Segment `2f2b` (75 defs) — colony — Colony screen / build / colonist logic (DS:0x8542)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_2f2b_011e` | 47107 | 93 | colony | Colony build / order gate (prereq / error codes) | inferred |  |
| `FUN_2f2b_0332` | 47200 | 59 | colony | unknown | inferred |  |
| `FUN_2f2b_0434` | 47259 | 73 | colony | unknown | inferred |  |
| `FUN_2f2b_05b0` | 47332 | 9 | colony | unknown | inferred |  |
| `FUN_2f2b_05b6` | 47341 | 16 | colony | unknown | inferred |  |
| `FUN_2f2b_05ee` | 47357 | 43 | colony | unknown | inferred |  |
| `FUN_2f2b_0722` | 47400 | 30 | colony | unknown | inferred |  |
| `FUN_2f2b_0842` | 47430 | 63 | colony | unknown | inferred |  |
| `FUN_2f2b_0a3e` | 47493 | 14 | colony | unknown | inferred |  |
| `FUN_2f2b_0a74` | 47507 | 53 | colony | unknown | inferred |  |
| `FUN_2f2b_0ba8` | 47560 | 161 | colony | unknown | inferred |  |
| `FUN_2f2b_0d89` | 47721 | 100 | colony | unknown | inferred |  |
| `FUN_2f2b_0fce` | 47821 | 62 | colony | unknown | inferred |  |
| `FUN_2f2b_11b2` | 47883 | 64 | colony | unknown | inferred |  |
| `FUN_2f2b_12cc` | 47947 | 49 | colony | unknown | inferred |  |
| `FUN_2f2b_13c2` | 47996 | 259 | colony | unknown | inferred |  |
| `FUN_2f2b_14d4` | 48255 | 101 | colony | unknown | inferred |  |
| `FUN_2f2b_16f2` | 48356 | 13 | colony | unknown | inferred |  |
| `FUN_2f2b_171c` | 48369 | 31 | colony | unknown | inferred |  |
| `FUN_2f2b_17d0` | 48400 | 132 | colony | unknown | inferred |  |
| `FUN_2f2b_1cce` | 48532 | 61 | colony | unknown | inferred |  |
| `FUN_2f2b_1e46` | 48593 | 115 | colony | unknown | inferred |  |
| `FUN_2f2b_2054` | 48708 | 18 | colony | unknown | inferred |  |
| `FUN_2f2b_208c` | 48726 | 59 | colony | unknown | inferred |  |
| `FUN_2f2b_21da` | 48785 | 21 | colony | unknown | inferred |  |
| `FUN_2f2b_2262` | 48806 | 15 | colony | unknown | inferred |  |
| `FUN_2f2b_22b6` | 48821 | 78 | colony | unknown | inferred |  |
| `FUN_2f2b_2484` | 48899 | 16 | colony | unknown | inferred |  |
| `FUN_2f2b_24b2` | 48915 | 121 | colony | unknown | inferred |  |
| `FUN_2f2b_284c` | 49036 | 25 | colony | unknown | inferred |  |
| `FUN_2f2b_289e` | 49061 | 16 | colony | unknown | inferred |  |
| `FUN_2f2b_28d6` | 49077 | 85 | colony | unknown | inferred |  |
| `FUN_2f2b_2b2c` | 49162 | 18 | colony | unknown | inferred |  |
| `FUN_2f2b_2b66` | 49180 | 39 | colony | unknown | inferred |  |
| `FUN_2f2b_2c3c` | 49219 | 56 | colony | unknown | inferred |  |
| `FUN_2f2b_2c92` | 49275 | 23 | colony | unknown | inferred |  |
| `FUN_2f2b_2d0e` | 49298 | 10 | colony | unknown | inferred |  |
| `FUN_2f2b_2d1c` | 49308 | 33 | colony | unknown | inferred |  |
| `FUN_2f2b_2d90` | 49341 | 37 | colony | unknown | inferred |  |
| `FUN_2f2b_2e2a` | 49378 | 27 | colony | unknown | inferred |  |
| `FUN_2f2b_2e92` | 49405 | 10 | colony | unknown | inferred |  |
| `FUN_2f2b_2eb2` | 49415 | 11 | colony | unknown | inferred |  |
| `FUN_2f2b_2ec6` | 49426 | 11 | colony | unknown | inferred |  |
| `FUN_2f2b_2eea` | 49437 | 21 | colony | unknown | inferred |  |
| `FUN_2f2b_2f26` | 49458 | 23 | colony | unknown | inferred |  |
| `FUN_2f2b_2f3e` | 49481 | 985 | colony | unknown | inferred |  |
| `FUN_2f2b_348c` | 50466 | 441 | colony | unknown | inferred |  |
| `FUN_2f2b_3fa6` | 50907 | 47 | colony | unknown | inferred |  |
| `FUN_2f2b_40a0` | 50954 | 54 | colony | unknown | inferred |  |
| `FUN_2f2b_41c0` | 51008 | 45 | colony | unknown | inferred |  |
| `FUN_2f2b_4284` | 51053 | 19 | colony | unknown | inferred |  |
| `FUN_2f2b_42be` | 51072 | 13 | colony | unknown | inferred |  |
| `FUN_2f2b_42f2` | 51085 | 14 | colony | unknown | inferred |  |
| `FUN_2f2b_4424` | 51099 | 40 | colony | unknown | inferred |  |
| `FUN_2f2b_44d4` | 51139 | 174 | colony | unknown | inferred |  |
| `FUN_2f2b_47bc` | 51313 | 100 | colony | unknown | inferred |  |
| `FUN_2f2b_4a1c` | 51413 | 73 | colony | unknown | inferred |  |
| `FUN_2f2b_4b62` | 51486 | 79 | colony | unknown | inferred |  |
| `FUN_2f2b_4da6` | 51565 | 79 | colony | unknown | inferred |  |
| `FUN_2f2b_4fec` | 51644 | 78 | colony | unknown | inferred |  |
| `FUN_2f2b_51ec` | 51722 | 458 | colony | unknown | inferred |  |
| `FUN_2f2b_548e` | 52180 | 67 | colony | unknown | inferred |  |
| `FUN_2f2b_55da` | 52247 | 47 | colony | unknown | inferred |  |
| `FUN_2f2b_56ce` | 52294 | 27 | colony | unknown | inferred |  |
| `FUN_2f2b_5746` | 52321 | 120 | colony | unknown | inferred |  |
| `FUN_2f2b_59a0` | 52441 | 49 | colony | unknown | inferred |  |
| `FUN_2f2b_5a68` | 52490 | 59 | colony | unknown | inferred |  |
| `FUN_2f2b_5bd2` | 52549 | 134 | colony | unknown | inferred |  |
| `FUN_2f2b_5e44` | 52683 | 77 | colony | unknown | inferred |  |
| `FUN_2f2b_5fc6` | 52760 | 53 | colony | unknown | inferred |  |
| `FUN_2f2b_60dc` | 52813 | 71 | colony | unknown | inferred |  |
| `FUN_2f2b_628a` | 52884 | 1478 | colony | unknown | inferred |  |
| `FUN_2f2b_6372` | 54362 | 1575 | colony | unknown | inferred |  |
| `FUN_2f2b_6c46` | 55937 | 38 | colony | unknown | inferred |  |
| `FUN_2f2b_6cd4` | 55975 | 847 | colony | unknown | inferred |  |

### Segment `364b` (11 defs) — colony — Colony found / build screen (DS:0x8542)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_364b_0000` | 56822 | 60 | colony | unknown | inferred |  |
| `FUN_364b_0114` | 56882 | 95 | colony | unknown | inferred |  |
| `FUN_364b_033a` | 56977 | 39 | colony | unknown | inferred |  |
| `FUN_364b_03f6` | 57016 | 107 | colony | unknown | inferred |  |
| `FUN_364b_0636` | 57123 | 29 | colony | unknown | inferred |  |
| `FUN_364b_0688` | 57152 | 803 | colony | unknown | inferred |  |
| `FUN_364b_1aec` | 57955 | 15 | colony | unknown | inferred |  |
| `FUN_364b_1b1a` | 57970 | 16 | colony | unknown | inferred |  |
| `FUN_364b_1b4c` | 57986 | 15 | colony | unknown | inferred |  |
| `FUN_364b_1b76` | 58001 | 14 | colony | unknown | inferred |  |
| `FUN_364b_1ba8` | 58015 | 253 | colony | Found colony: bump 539e, init colony record via DS:0x8542 | inferred |  |

### Segment `3844` (3 defs) — turn — Euro EOT treasure / ship-ready unit chrome

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_3844_0004` | 58268 | 37 | turn | unknown | inferred |  |
| `FUN_3844_00f2` | 58305 | 125 | turn | unknown | inferred |  |
| `FUN_3844_0442` | 58430 | 265 | turn | unknown | inferred |  |

### Segment `38fd` (81 defs) — trade — Nation Europe market / cargo trade (nation*0x13c via 0x84fc)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_38fd_0000` | 58695 | 12 | trade | Set current nation index and Europe-market base (nation*0x13c) | inferred | docs/savegame.md |
| `FUN_38fd_0016` | 58707 | 15 | trade | unknown | inferred |  |
| `FUN_38fd_0040` | 58722 | 19 | trade | unknown | inferred |  |
| `FUN_38fd_0058` | 58741 | 269 | trade | unknown | inferred |  |
| `FUN_38fd_05e8` | 59010 | 10 | trade | unknown | inferred |  |
| `FUN_38fd_05fc` | 59020 | 28 | trade | unknown | inferred |  |
| `FUN_38fd_0666` | 59048 | 25 | trade | unknown | inferred |  |
| `FUN_38fd_06c4` | 59073 | 25 | trade | unknown | inferred |  |
| `FUN_38fd_0718` | 59098 | 50 | trade | unknown | inferred |  |
| `FUN_38fd_07c6` | 59148 | 22 | trade | unknown | inferred |  |
| `FUN_38fd_081c` | 59170 | 16 | trade | unknown | inferred |  |
| `FUN_38fd_0836` | 59186 | 14 | trade | unknown | inferred |  |
| `FUN_38fd_086c` | 59200 | 16 | trade | unknown | inferred |  |
| `FUN_38fd_08a4` | 59216 | 42 | trade | unknown | inferred |  |
| `FUN_38fd_0a26` | 59258 | 35 | trade | unknown | inferred |  |
| `FUN_38fd_0b64` | 59293 | 63 | trade | unknown | inferred |  |
| `FUN_38fd_0d48` | 59356 | 60 | trade | unknown | inferred |  |
| `FUN_38fd_0e16` | 59416 | 48 | trade | unknown | inferred |  |
| `FUN_38fd_0f5e` | 59464 | 14 | trade | unknown | inferred |  |
| `FUN_38fd_0f8c` | 59478 | 106 | trade | unknown | inferred |  |
| `FUN_38fd_127c` | 59584 | 37 | trade | unknown | inferred |  |
| `FUN_38fd_1382` | 59621 | 33 | trade | unknown | inferred |  |
| `FUN_38fd_1456` | 59654 | 11 | trade | unknown | inferred |  |
| `FUN_38fd_146c` | 59665 | 36 | trade | unknown | inferred |  |
| `FUN_38fd_14e2` | 59701 | 41 | trade | unknown | inferred |  |
| `FUN_38fd_15aa` | 59742 | 37 | trade | unknown | inferred |  |
| `FUN_38fd_1660` | 59779 | 19 | trade | unknown | inferred |  |
| `FUN_38fd_1696` | 59798 | 82 | trade | unknown | inferred |  |
| `FUN_38fd_1878` | 59880 | 29 | trade | unknown | inferred |  |
| `FUN_38fd_18fc` | 59909 | 17 | trade | unknown | inferred |  |
| `FUN_38fd_1956` | 59926 | 10 | trade | unknown | inferred |  |
| `FUN_38fd_1960` | 59936 | 28 | trade | unknown | inferred |  |
| `FUN_38fd_199e` | 59964 | 22 | trade | unknown | inferred |  |
| `FUN_38fd_19d8` | 59986 | 10 | trade | unknown | inferred |  |
| `FUN_38fd_19f8` | 59996 | 11 | trade | unknown | inferred |  |
| `FUN_38fd_1a0c` | 60007 | 11 | trade | unknown | inferred |  |
| `FUN_38fd_1a30` | 60018 | 28 | trade | unknown | inferred |  |
| `FUN_38fd_1aba` | 60046 | 42 | trade | unknown | inferred |  |
| `FUN_38fd_1b9e` | 60088 | 13 | trade | unknown | inferred |  |
| `FUN_38fd_1bd2` | 60101 | 23 | trade | unknown | inferred |  |
| `FUN_38fd_1c64` | 60124 | 16 | trade | unknown | inferred |  |
| `FUN_38fd_1cac` | 60140 | 16 | trade | unknown | inferred |  |
| `FUN_38fd_1cf4` | 60156 | 14 | trade | unknown | inferred |  |
| `FUN_38fd_1d28` | 60170 | 16 | trade | unknown | inferred |  |
| `FUN_38fd_1d44` | 60186 | 18 | trade | unknown | inferred |  |
| `FUN_38fd_1d80` | 60204 | 43 | trade | unknown | inferred |  |
| `FUN_38fd_1dfa` | 60247 | 54 | trade | unknown | inferred |  |
| `FUN_38fd_1ebc` | 60301 | 19 | trade | unknown | inferred |  |
| `FUN_38fd_1f0c` | 60320 | 23 | trade | unknown | inferred |  |
| `FUN_38fd_1f66` | 60343 | 12 | trade | unknown | inferred |  |
| `FUN_38fd_1f7e` | 60355 | 11 | trade | unknown | inferred |  |
| `FUN_38fd_1f8e` | 60366 | 11 | trade | unknown | inferred |  |
| `FUN_38fd_1fa2` | 60377 | 130 | trade | unknown | inferred |  |
| `FUN_38fd_23c4` | 60507 | 143 | trade | unknown | inferred |  |
| `FUN_38fd_285c` | 60650 | 79 | trade | unknown | inferred |  |
| `FUN_38fd_2a92` | 60729 | 58 | trade | unknown | inferred |  |
| `FUN_38fd_2bfe` | 60787 | 117 | trade | unknown | inferred |  |
| `FUN_38fd_2dfe` | 60904 | 45 | trade | unknown | inferred |  |
| `FUN_38fd_2edc` | 60949 | 88 | trade | unknown | inferred |  |
| `FUN_38fd_30aa` | 61037 | 47 | trade | unknown | inferred |  |
| `FUN_38fd_31c6` | 61084 | 23 | trade | unknown | inferred |  |
| `FUN_38fd_3502` | 61107 | 73 | trade | unknown | inferred |  |
| `FUN_38fd_3694` | 61180 | 32 | trade | unknown | inferred |  |
| `FUN_38fd_3746` | 61212 | 2854 | trade | unknown | inferred |  |
| `FUN_38fd_3c86` | 64066 | 66 | trade | unknown | inferred |  |
| `FUN_38fd_3dc8` | 64132 | 184 | trade | unknown | inferred |  |
| `FUN_38fd_41ce` | 64316 | 139 | trade | unknown | inferred |  |
| `FUN_38fd_44a4` | 64455 | 61 | trade | unknown | inferred |  |
| `FUN_38fd_4590` | 64516 | 38 | trade | unknown | inferred |  |
| `FUN_38fd_46d4` | 64554 | 106 | trade | unknown | inferred |  |
| `FUN_38fd_4884` | 64660 | 140 | trade | unknown | inferred |  |
| `FUN_38fd_4b50` | 64800 | 115 | trade | unknown | inferred |  |
| `FUN_38fd_4e8e` | 64915 | 974 | trade | unknown | inferred |  |
| `FUN_38fd_4f6e` | 65889 | 2205 | trade | unknown | inferred |  |
| `FUN_38fd_5580` | 68094 | 20 | trade | unknown | inferred |  |
| `FUN_38fd_55b6` | 68114 | 134 | trade | unknown | inferred |  |
| `FUN_38fd_584a` | 68248 | 57 | trade | unknown | inferred |  |
| `FUN_38fd_5930` | 68305 | 115 | trade | unknown | inferred |  |
| `FUN_38fd_5be8` | 68420 | 119 | trade | unknown | inferred |  |
| `FUN_38fd_5e52` | 68539 | 88 | trade | unknown | inferred |  |
| `FUN_38fd_6024` | 68627 | 755 | trade | unknown | inferred |  |

### Segment `3f3f` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_3f3f_0006` | 69382 | 15 | unknown | unknown | unknown |  |

### Segment `3f41` (18 defs) — ui — Report / diplomacy / market UI screens

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_3f41_0000` | 69397 | 27 | ui | unknown | inferred |  |
| `FUN_3f41_008a` | 69424 | 27 | ui | unknown | inferred |  |
| `FUN_3f41_010a` | 69451 | 160 | ui | unknown | inferred |  |
| `FUN_3f41_0618` | 69611 | 39 | ui | unknown | inferred |  |
| `FUN_3f41_06d0` | 69650 | 174 | ui | unknown | inferred |  |
| `FUN_3f41_0ae6` | 69824 | 90 | ui | unknown | inferred |  |
| `FUN_3f41_0d3e` | 69914 | 144 | ui | unknown | inferred |  |
| `FUN_3f41_10d8` | 70058 | 123 | ui | unknown | inferred |  |
| `FUN_3f41_1438` | 70181 | 31 | ui | unknown | inferred |  |
| `FUN_3f41_1550` | 70212 | 69 | ui | unknown | inferred |  |
| `FUN_3f41_1710` | 70281 | 145 | ui | unknown | inferred |  |
| `FUN_3f41_1b94` | 70426 | 17 | ui | unknown | inferred |  |
| `FUN_3f41_1bec` | 70443 | 95 | ui | unknown | inferred |  |
| `FUN_3f41_1e80` | 70538 | 17 | ui | unknown | inferred |  |
| `FUN_3f41_1ed8` | 70555 | 75 | ui | unknown | inferred |  |
| `FUN_3f41_20b4` | 70630 | 45 | ui | unknown | inferred |  |
| `FUN_3f41_220c` | 70675 | 112 | ui | unknown | inferred |  |
| `FUN_3f41_2548` | 70787 | 247 | ui | unknown | inferred |  |

### Segment `41f2` (9 defs) — ai — Tribe growth (Indian-turn growth tick + message UI)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_41f2_000e` | 71034 | 16 | ai | unknown | inferred |  |
| `FUN_41f2_0048` | 71050 | 18 | ai | unknown | inferred |  |
| `FUN_41f2_0092` | 71068 | 346 | ai | unknown | inferred |  |
| `FUN_41f2_0266` | 71414 | 329 | ai | unknown | inferred |  |
| `FUN_41f2_0280` | 71743 | 342 | ai | Tribe growth tick from Indian nation turn (4d56_1816) | known | docs/ai_transcription.md |
| `FUN_41f2_0294` | 72085 | 330 | ai | unknown | inferred |  |
| `FUN_41f2_0b70` | 72415 | 137 | ai | unknown | inferred |  |
| `FUN_41f2_0f56` | 72552 | 175 | ai | unknown | inferred |  |
| `FUN_41f2_14a8` | 72727 | 72 | ai | unknown | inferred |  |

### Segment `4345` (13 defs) — trade — Nation trade flags / FF / Europe-market helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4345_0000` | 72799 | 22 | trade | Set/clear nation bitflag in Europe-record (nation*0x13c) | inferred |  |
| `FUN_4345_003c` | 72821 | 14 | trade | unknown | inferred |  |
| `FUN_4345_005a` | 72835 | 15 | trade | unknown | inferred |  |
| `FUN_4345_0080` | 72850 | 26 | trade | unknown | inferred |  |
| `FUN_4345_00e0` | 72876 | 23 | trade | unknown | inferred |  |
| `FUN_4345_0126` | 72899 | 14 | trade | unknown | inferred |  |
| `FUN_4345_015a` | 72913 | 28 | trade | unknown | inferred |  |
| `FUN_4345_01a6` | 72941 | 41 | trade | unknown | inferred |  |
| `FUN_4345_024a` | 72982 | 62 | trade | unknown | inferred |  |
| `FUN_4345_0342` | 73044 | 133 | trade | unknown | inferred |  |
| `FUN_4345_06d2` | 73177 | 116 | trade | unknown | inferred |  |
| `FUN_4345_0982` | 73293 | 40 | trade | unknown | inferred |  |
| `FUN_4345_0a22` | 73333 | 144 | trade | unknown | inferred |  |

### Segment `43f7` (21 defs) — ui — Nation / @COUNTRY colors

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_43f7_0004` | 73477 | 42 | ui | unknown | inferred |  |
| `FUN_43f7_0082` | 73519 | 29 | ui | unknown | inferred |  |
| `FUN_43f7_0108` | 73548 | 22 | ui | unknown | inferred |  |
| `FUN_43f7_0188` | 73570 | 31 | ui | unknown | inferred |  |
| `FUN_43f7_0218` | 73601 | 117 | ui | unknown | inferred |  |
| `FUN_43f7_0512` | 73718 | 49 | ui | unknown | inferred |  |
| `FUN_43f7_05ea` | 73767 | 11 | ui | unknown | inferred |  |
| `FUN_43f7_05f4` | 73778 | 14 | ui | @COUNTRY to DS color table | known | src/core/turn.c |
| `FUN_43f7_060a` | 73792 | 37 | ui | unknown | inferred |  |
| `FUN_43f7_06a6` | 73829 | 106 | ui | unknown | inferred |  |
| `FUN_43f7_0982` | 73935 | 335 | ui | unknown | inferred |  |
| `FUN_43f7_10f0` | 74270 | 192 | ui | unknown | inferred |  |
| `FUN_43f7_1528` | 74462 | 37 | ui | unknown | inferred |  |
| `FUN_43f7_160a` | 74499 | 207 | ui | unknown | inferred |  |
| `FUN_43f7_1a26` | 74706 | 140 | ui | unknown | inferred |  |
| `FUN_43f7_1d42` | 74846 | 64 | ui | unknown | inferred |  |
| `FUN_43f7_1eca` | 74910 | 66 | ui | unknown | inferred |  |
| `FUN_43f7_2022` | 74976 | 98 | ui | unknown | inferred |  |
| `FUN_43f7_2244` | 75074 | 82 | ui | unknown | inferred |  |
| `FUN_43f7_2424` | 75156 | 61 | ui | unknown | inferred |  |
| `FUN_43f7_2564` | 75217 | 200 | ui | unknown | inferred |  |

### Segment `465b` (2 defs) — combat — Move spent / ADD / combat-adjacent

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_465b_0000` | 75417 | 430 | combat | Move spent cost / ADD / post-ADD chrome (combat tails parked) | known | ai/move_spent.c; src/core/ai.c |
| `FUN_465b_0c1e` | 75847 | 23 | combat | unknown | inferred |  |

### Segment `4720` (3 defs) — ui — Ship embark / naval-move validity + order UI (DS:0x9e4e)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4720_0006` | 75870 | 70 | ui | unknown | inferred |  |
| `FUN_4720_015c` | 75940 | 127 | ui | unknown | inferred |  |
| `FUN_4720_049e` | 76067 | 463 | ui | unknown | inferred |  |

### Segment `478c` (4 defs) — colony — Colonist (type 0x17) / ship unit spawn helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_478c_0002` | 76530 | 19 | colony | unknown | inferred |  |
| `FUN_478c_002c` | 76549 | 23 | colony | unknown | inferred |  |
| `FUN_478c_007e` | 76572 | 22 | colony | unknown | inferred |  |
| `FUN_478c_00d0` | 76594 | 23 | colony | unknown | inferred |  |

### Segment `479b` (13 defs) — colony — Pioneer clear/plow / goto-colony order bodies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_479b_0000` | 76617 | 56 | colony | unknown | inferred |  |
| `FUN_479b_00ca` | 76673 | 24 | colony | unknown | inferred |  |
| `FUN_479b_0158` | 76697 | 25 | colony | unknown | inferred |  |
| `FUN_479b_01a6` | 76722 | 140 | colony | Pioneer clear/plow work-tick; may grant colony food | inferred |  |
| `FUN_479b_0526` | 76862 | 99 | colony | unknown | inferred |  |
| `FUN_479b_076e` | 76961 | 91 | colony | unknown | inferred |  |
| `FUN_479b_0972` | 77052 | 74 | colony | unknown | inferred |  |
| `FUN_479b_0b26` | 77126 | 20 | colony | unknown | inferred |  |
| `FUN_479b_0b6c` | 77146 | 12 | colony | unknown | inferred |  |
| `FUN_479b_0b84` | 77158 | 21 | colony | unknown | inferred |  |
| `FUN_479b_0bd0` | 77179 | 164 | colony | unknown | inferred |  |
| `FUN_479b_0f60` | 77343 | 92 | colony | unknown | inferred |  |
| `FUN_479b_11a4` | 77435 | 128 | colony | unknown | inferred |  |

### Segment `48d3` (9 defs) — ai — Euro landfall goto / unit-order helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_48d3_0002` | 77563 | 29 | ai | unknown | inferred |  |
| `FUN_48d3_007a` | 77592 | 44 | ai | unknown | inferred |  |
| `FUN_48d3_015e` | 77636 | 96 | ai | unknown | inferred |  |
| `FUN_48d3_0346` | 77732 | 29 | ai | unknown | inferred |  |
| `FUN_48d3_03d0` | 77761 | 25 | ai | unknown | inferred |  |
| `FUN_48d3_0434` | 77786 | 24 | ai | unknown | inferred |  |
| `FUN_48d3_048e` | 77810 | 95 | ai | Spiral-place ship on HS near landfall goto (Euro AI) | inferred | src/core/ai.c |
| `FUN_48d3_064e` | 77905 | 38 | ai | unknown | inferred |  |
| `FUN_48d3_06ba` | 77943 | 150 | ai | unknown | inferred |  |

### Segment `4962` (4 defs) — ai — Per-nation unit/colony/cargo census tallies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4962_0006` | 78093 | 18 | ai | unknown | inferred |  |
| `FUN_4962_0018` | 78111 | 221 | ai | Census units/colonies/cargo tallies for one nation | inferred |  |
| `FUN_4962_0606` | 78332 | 46 | ai | unknown | inferred |  |
| `FUN_4962_06b6` | 78378 | 48 | ai | unknown | inferred |  |

### Segment `49dd` (9 defs) — ui — Unit cargo / profession status panels

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_49dd_0000` | 78426 | 22 | ui | unknown | inferred |  |
| `FUN_49dd_0086` | 78448 | 9 | ui | unknown | inferred |  |
| `FUN_49dd_009c` | 78457 | 17 | ui | unknown | inferred |  |
| `FUN_49dd_00f6` | 78474 | 17 | ui | unknown | inferred |  |
| `FUN_49dd_0156` | 78491 | 17 | ui | unknown | inferred |  |
| `FUN_49dd_01aa` | 78508 | 59 | ui | unknown | inferred |  |
| `FUN_49dd_02d0` | 78567 | 39 | ui | unknown | inferred |  |
| `FUN_49dd_0386` | 78606 | 41 | ui | unknown | inferred |  |
| `FUN_49dd_0424` | 78647 | 750 | ui | unknown | inferred |  |

### Segment `4b58` (24 defs) — ui — Window / frame widget draw (281f_00ba family)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4b58_0000` | 79397 | 15 | ui | unknown | inferred |  |
| `FUN_4b58_0016` | 79412 | 15 | ui | unknown | inferred |  |
| `FUN_4b58_004a` | 79427 | 25 | ui | unknown | inferred |  |
| `FUN_4b58_00ae` | 79452 | 19 | ui | unknown | inferred |  |
| `FUN_4b58_0104` | 79471 | 49 | ui | unknown | inferred |  |
| `FUN_4b58_023e` | 79520 | 45 | ui | unknown | inferred |  |
| `FUN_4b58_02f6` | 79565 | 43 | ui | unknown | inferred |  |
| `FUN_4b58_043e` | 79608 | 34 | ui | unknown | inferred |  |
| `FUN_4b58_0484` | 79642 | 44 | ui | unknown | inferred |  |
| `FUN_4b58_051a` | 79686 | 24 | ui | unknown | inferred |  |
| `FUN_4b58_0552` | 79710 | 16 | ui | unknown | inferred |  |
| `FUN_4b58_0582` | 79726 | 37 | ui | unknown | inferred |  |
| `FUN_4b58_05c6` | 79763 | 16 | ui | unknown | inferred |  |
| `FUN_4b58_05f6` | 79779 | 37 | ui | unknown | inferred |  |
| `FUN_4b58_063a` | 79816 | 77 | ui | unknown | inferred |  |
| `FUN_4b58_07d6` | 79893 | 79 | ui | unknown | inferred |  |
| `FUN_4b58_093c` | 79972 | 52 | ui | unknown | inferred |  |
| `FUN_4b58_0a64` | 80024 | 56 | ui | unknown | inferred |  |
| `FUN_4b58_0b7a` | 80080 | 92 | ui | unknown | inferred |  |
| `FUN_4b58_0d94` | 80172 | 312 | ui | unknown | inferred |  |
| `FUN_4b58_13ac` | 80484 | 43 | ui | unknown | inferred |  |
| `FUN_4b58_144a` | 80527 | 42 | ui | unknown | inferred |  |
| `FUN_4b58_14de` | 80569 | 58 | ui | unknown | inferred |  |
| `FUN_4b58_15a4` | 80627 | 147 | ui | unknown | inferred |  |

### Segment `4cc6` (7 defs) — ai — Indian tribe relations / nearest-village / contact score

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4cc6_0000` | 80774 | 32 | ai | unknown | inferred |  |
| `FUN_4cc6_0092` | 80806 | 20 | ai | unknown | inferred |  |
| `FUN_4cc6_00f2` | 80826 | 95 | ai | unknown | inferred |  |
| `FUN_4cc6_0304` | 80921 | 23 | ai | unknown | inferred |  |
| `FUN_4cc6_0356` | 80944 | 47 | ai | unknown | inferred |  |
| `FUN_4cc6_03f8` | 80991 | 200 | ai | unknown | inferred |  |
| `FUN_4cc6_07c2` | 81191 | 62 | ai | Indian contact/alarm distance score (cur_tribe + indian_state + difficulty) | inferred |  |

### Segment `4d56` (21 defs) — ai — Indian AI / village growth

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_4d56_0038` | 81253 | 39 | ai | Small Indian AI helper; calls into 00e0 / map probes | unknown | docs/ai_transcription.md |
| `FUN_4d56_00e0` | 81292 | 60 | ai | Chains to 01e2 / 14fe | unknown | docs/ai_transcription.md |
| `FUN_4d56_01e2` | 81352 | 19 | ai | Thin wrapper to 14fe | unknown | docs/ai_transcription.md |
| `FUN_4d56_14fe` | 81371 | 16 | ai | Indian unit act / dispatches growth 152e | inferred | ai/indian_nation_turn.c; src/core/ai.c |
| `FUN_4d56_152e` | 81387 | 156 | ai | Village growth accumulator to pop++ | known | ai/indian_nation_turn.c; src/core/ai.c |
| `FUN_4d56_1816` | 81543 | 141 | ai | Indian nation turn entry (alarm, unit loop, relation ticks) | known | ai/indian_nation_turn.c; src/core/ai.c |
| `FUN_4d56_1b3a` | 81684 | 59 | ai | Calls 2154; mid-turn Indian action | inferred | docs/ai_transcription.md |
| `FUN_4d56_2154` | 81743 | 321 | ai | Larger Indian action body (raid-adjacent) | inferred | docs/ai_transcription.md |
| `FUN_4d56_2820` | 82064 | 222 | ai | Heavy Indian decision / raid-scale logic | inferred | docs/ai_transcription.md |
| `FUN_4d56_2aac` | 82286 | 39 | ai | unknown | inferred |  |
| `FUN_4d56_2af6` | 82325 | 29 | ai | unknown | inferred |  |
| `FUN_4d56_2b92` | 82354 | 222 | ai | unknown | inferred |  |
| `FUN_4d56_2bbc` | 82576 | 210 | ai | unknown | inferred |  |
| `FUN_4d56_2e92` | 82786 | 53 | ai | unknown | inferred |  |
| `FUN_4d56_2f96` | 82839 | 189 | ai | unknown | inferred |  |
| `FUN_4d56_306c` | 83028 | 193 | ai | unknown | inferred |  |
| `FUN_4d56_311e` | 83221 | 239 | ai | unknown | inferred |  |
| `FUN_4d56_3582` | 83460 | 21 | ai | Small helper after 2820 | unknown | docs/ai_transcription.md |
| `FUN_4d56_359c` | 83481 | 30 | ai | unknown | inferred |  |
| `FUN_4d56_417e` | 83511 | 188 | ai | Mid-size Indian AI helper | unknown | docs/ai_transcription.md |
| `FUN_4d56_4528` | 83699 | 3073 | ai | Largest Indian cluster (combat/raid-adjacent) | inferred | docs/ai_transcription.md |

### Segment `521d` (29 defs) — ai — European AI planner

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_521d_0000` | 86772 | 15 | ai | unknown | inferred |  |
| `FUN_521d_001c` | 86787 | 32 | ai | unknown | inferred |  |
| `FUN_521d_0072` | 86819 | 19 | ai | unknown | inferred |  |
| `FUN_521d_00a8` | 86838 | 19 | ai | unknown | inferred |  |
| `FUN_521d_00de` | 86857 | 18 | ai | unknown | inferred |  |
| `FUN_521d_0116` | 86875 | 21 | ai | unknown | inferred |  |
| `FUN_521d_016a` | 86896 | 37 | ai | unknown | inferred |  |
| `FUN_521d_0214` | 86933 | 38 | ai | unknown | inferred |  |
| `FUN_521d_02be` | 86971 | 25 | ai | unknown | inferred |  |
| `FUN_521d_031c` | 86996 | 16 | ai | unknown | inferred |  |
| `FUN_521d_0342` | 87012 | 27 | ai | unknown | inferred |  |
| `FUN_521d_03a6` | 87039 | 19 | ai | unknown | inferred |  |
| `FUN_521d_03d0` | 87058 | 40 | ai | unknown | inferred |  |
| `FUN_521d_0492` | 87098 | 41 | ai | unknown | inferred |  |
| `FUN_521d_052c` | 87139 | 57 | ai | unknown | inferred |  |
| `FUN_521d_0600` | 87196 | 23 | ai | unknown | inferred |  |
| `FUN_521d_0656` | 87219 | 18 | ai | unknown | inferred |  |
| `FUN_521d_06ae` | 87237 | 82 | ai | unknown | inferred |  |
| `FUN_521d_0896` | 87319 | 26 | ai | unknown | inferred |  |
| `FUN_521d_0906` | 87345 | 63 | ai | unknown | inferred |  |
| `FUN_521d_0a60` | 87408 | 839 | ai | Euro unit / colony goal logic | inferred | ai/euro_dispatcher.c; docs/ai_transcription.md |
| `FUN_521d_20c6` | 88247 | 19 | ai | unknown | inferred |  |
| `FUN_521d_20e6` | 88266 | 2180 | ai | Direction / move scoring (all unit kinds); quiet Brave slice annotated | known | ai/quiet_brave_scoring.c; ai/move_scoring.md |
| `FUN_521d_5b66` | 90446 | 1815 | ai | unknown | inferred |  |
| `FUN_521d_5c38` | 92261 | 8 | ai | unknown | inferred |  |
| `FUN_521d_5c3c` | 92269 | 47 | ai | unknown | inferred |  |
| `FUN_521d_5cf6` | 92316 | 9 | ai | unknown | inferred |  |
| `FUN_521d_5d04` | 92325 | 748 | ai | Euro unit goals / planning (alongside 0a60) | inferred | ai/euro_dispatcher.c; docs/ai_transcription.md |
| `FUN_521d_6d8e` | 93073 | 516 | ai | Euro AI dispatcher per nation | known | ai/euro_dispatcher.c; src/core/ai.c |

### Segment `5952` (6 defs) — colony — Colony production / buildings / stock tick (DS:0x8542)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5952_0000` | 93589 | 97 | colony | unknown | inferred |  |
| `FUN_5952_0214` | 93686 | 35 | colony | unknown | inferred |  |
| `FUN_5952_0280` | 93721 | 28 | colony | unknown | inferred |  |
| `FUN_5952_02f4` | 93749 | 11 | colony | unknown | inferred |  |
| `FUN_5952_0306` | 93760 | 30 | colony | unknown | inferred |  |
| `FUN_5952_035e` | 93790 | 2658 | colony | Colony production/buildings/stock tick on current colony (DS:0x8542) | inferred |  |

### Segment `5bfb` (12 defs) — ai — Indian contact / diplomacy / alarm

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5bfb_0000` | 96448 | 58 | ai | unknown | inferred |  |
| `FUN_5bfb_00f8` | 96506 | 30 | ai | unknown | inferred |  |
| `FUN_5bfb_0182` | 96536 | 29 | ai | unknown | inferred |  |
| `FUN_5bfb_022e` | 96565 | 540 | ai | unknown | inferred |  |
| `FUN_5bfb_102a` | 97105 | 24 | ai | unknown | inferred |  |
| `FUN_5bfb_1092` | 97129 | 22 | ai | unknown | inferred |  |
| `FUN_5bfb_10ec` | 97151 | 63 | ai | unknown | inferred |  |
| `FUN_5bfb_12d0` | 97214 | 46 | ai | unknown | inferred |  |
| `FUN_5bfb_13b0` | 97260 | 61 | ai | unknown | inferred |  |
| `FUN_5bfb_153e` | 97321 | 1112 | ai | unknown | inferred |  |
| `FUN_5bfb_312e` | 98433 | 24 | ai | unknown | inferred |  |
| `FUN_5bfb_3180` | 98457 | 352 | ai | unknown | inferred |  |

### Segment `5f7a` (3 defs) — trade — Colony native-trade / cargo sell & buy

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5f7a_000e` | 98809 | 76 | trade | unknown | inferred |  |
| `FUN_5f7a_020e` | 98885 | 168 | trade | unknown | inferred |  |
| `FUN_5f7a_0662` | 99053 | 58 | trade | unknown | inferred |  |

### Segment `5fef` (11 defs) — combat — Unit/colony combat and Indian raid resolution

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_5fef_0000` | 99111 | 98 | combat | unknown | inferred |  |
| `FUN_5fef_016c` | 99209 | 83 | combat | unknown | inferred |  |
| `FUN_5fef_0352` | 99292 | 416 | combat | unknown | inferred |  |
| `FUN_5fef_0ec0` | 99708 | 30 | combat | unknown | inferred |  |
| `FUN_5fef_0f14` | 99738 | 302 | combat | unknown | inferred |  |
| `FUN_5fef_16ea` | 100040 | 24 | combat | unknown | inferred |  |
| `FUN_5fef_172c` | 100064 | 94 | combat | unknown | inferred |  |
| `FUN_5fef_1908` | 100158 | 93 | combat | unknown | inferred |  |
| `FUN_5fef_1b0e` | 100251 | 1063 | combat | unknown | inferred |  |
| `FUN_5fef_31ea` | 101314 | 224 | combat | unknown | inferred |  |
| `FUN_5fef_36fe` | 101538 | 122 | combat | unknown | inferred |  |

### Segment `636c` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_636c_0000` | 101660 | 562 | unknown | unknown | unknown |  |

### Segment `647e` (23 defs) — colony — Colony list / select UI (rec*0x4a via DS:0x9e14)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_647e_0000` | 102222 | 13 | colony | Bind colony record pointer (idx*0x4a to DS:0x9e14) | inferred |  |
| `FUN_647e_001a` | 102235 | 15 | colony | unknown | inferred |  |
| `FUN_647e_0040` | 102250 | 18 | colony | unknown | inferred |  |
| `FUN_647e_0094` | 102268 | 66 | colony | unknown | inferred |  |
| `FUN_647e_01c6` | 102334 | 131 | colony | unknown | inferred |  |
| `FUN_647e_04f0` | 102465 | 13 | colony | unknown | inferred |  |
| `FUN_647e_0522` | 102478 | 13 | colony | unknown | inferred |  |
| `FUN_647e_0548` | 102491 | 28 | colony | unknown | inferred |  |
| `FUN_647e_057a` | 102519 | 18 | colony | unknown | inferred |  |
| `FUN_647e_05aa` | 102537 | 19 | colony | unknown | inferred |  |
| `FUN_647e_05ec` | 102556 | 13 | colony | unknown | inferred |  |
| `FUN_647e_060e` | 102569 | 59 | colony | unknown | inferred |  |
| `FUN_647e_06c2` | 102628 | 55 | colony | unknown | inferred |  |
| `FUN_647e_0796` | 102683 | 70 | colony | unknown | inferred |  |
| `FUN_647e_090a` | 102753 | 40 | colony | unknown | inferred |  |
| `FUN_647e_09da` | 102793 | 108 | colony | unknown | inferred |  |
| `FUN_647e_0dd4` | 102901 | 39 | colony | unknown | inferred |  |
| `FUN_647e_0e80` | 102940 | 36 | colony | unknown | inferred |  |
| `FUN_647e_0f2c` | 102976 | 63 | colony | unknown | inferred |  |
| `FUN_647e_1064` | 103039 | 29 | colony | unknown | inferred |  |
| `FUN_647e_10d2` | 103068 | 31 | colony | unknown | inferred |  |
| `FUN_647e_115c` | 103099 | 58 | colony | unknown | inferred |  |
| `FUN_647e_1486` | 103157 | 242 | colony | unknown | inferred |  |

### Segment `65dd` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_65dd_0004` | 103399 | 362 | unknown | unknown | unknown |  |

### Segment `6662` (7 defs) — ui — Goto pathfinding BFS + path-cost overlay

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6662_0000` | 103761 | 34 | ui | unknown | inferred |  |
| `FUN_6662_0086` | 103795 | 46 | ui | unknown | inferred |  |
| `FUN_6662_00f2` | 103841 | 309 | ui | Goto BFS over terr_cost with path-cost overlay and Z/Esc confirm | inferred |  |
| `FUN_6662_0906` | 104150 | 52 | ui | unknown | inferred |  |
| `FUN_6662_09ae` | 104202 | 70 | ui | unknown | inferred |  |
| `FUN_6662_0b4e` | 104272 | 216 | ui | unknown | inferred |  |
| `FUN_6662_0f74` | 104488 | 316 | ui | unknown | inferred |  |

### Segment `67bf` (1 defs) — mapgen — Continent flood-fill IDs

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_67bf_0000` | 104804 | 163 | mapgen | Continent flood-fill IDs | known | src/core/map_gen.c |

### Segment `67f4` (2 defs) — mapgen — Coast/neighbor bitmasks + continent tallies

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_67f4_0008` | 104967 | 43 | mapgen | unknown | inferred |  |
| `FUN_67f4_0088` | 105010 | 137 | mapgen | Build 15x18 coast/neighbor bitmasks and per-continent terrain tallies post-flood-fill | inferred |  |

### Segment `682a` (1 defs) — mapgen — Map fertility / bonus value writer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_682a_000c` | 105147 | 87 | mapgen | unknown | inferred |  |

### Segment `684c` (8 defs) — mapgen — Procedural NEW WORLD map gen

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_684c_0004` | 105234 | 25 | mapgen | unknown | inferred |  |
| `FUN_684c_009c` | 105259 | 23 | mapgen | unknown | inferred |  |
| `FUN_684c_0116` | 105282 | 39 | mapgen | Land blobs / form thunk | known | src/core/map_gen.c |
| `FUN_684c_021c` | 105321 | 26 | mapgen | Land blobs / form thunk | known | src/core/map_gen.c |
| `FUN_684c_02a8` | 105347 | 66 | mapgen | Land blobs / form thunk | known | src/core/map_gen.c |
| `FUN_684c_03e4` | 105413 | 25 | mapgen | unknown | inferred |  |
| `FUN_684c_04a6` | 105438 | 164 | mapgen | unknown | inferred |  |
| `FUN_684c_08c0` | 105602 | 2089 | mapgen | NEW WORLD procedural map entry | known | src/core/map_gen.c |

### Segment `6a09` (1 defs) — ai — Tribe placement

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6a09_0006` | 107691 | 335 | ai | Tribe capitals, satellites, Brave spawn loop | known | src/core/ai.c |

### Segment `6a9f` (8 defs) — mapdraw — Map viewport tile loop

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6a9f_0000` | 108026 | 19 | mapdraw | unknown | inferred |  |
| `FUN_6a9f_0034` | 108045 | 19 | mapdraw | unknown | inferred |  |
| `FUN_6a9f_0068` | 108064 | 32 | mapdraw | unknown | inferred |  |
| `FUN_6a9f_00d8` | 108096 | 22 | mapdraw | unknown | inferred |  |
| `FUN_6a9f_0118` | 108118 | 120 | mapdraw | Map viewport tile loop | known | src/core/map.c |
| `FUN_6a9f_0346` | 108238 | 12 | mapdraw | unknown | inferred |  |
| `FUN_6a9f_0360` | 108250 | 51 | mapdraw | unknown | inferred |  |
| `FUN_6a9f_0486` | 108301 | 87 | mapdraw | unknown | inferred |  |

### Segment `6afa` (7 defs) — mapdraw — Map viewport clamp / tile blit helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6afa_000c` | 108388 | 31 | mapdraw | unknown | inferred |  |
| `FUN_6afa_0052` | 108419 | 45 | mapdraw | unknown | inferred |  |
| `FUN_6afa_00c8` | 108464 | 15 | mapdraw | unknown | inferred |  |
| `FUN_6afa_0132` | 108479 | 13 | mapdraw | unknown | inferred |  |
| `FUN_6afa_0168` | 108492 | 60 | mapdraw | unknown | inferred |  |
| `FUN_6afa_0224` | 108552 | 11 | mapdraw | unknown | inferred |  |
| `FUN_6afa_023c` | 108563 | 11 | mapdraw | unknown | inferred |  |

### Segment `6b22` (9 defs) — mapdraw — Map viewport tribe/colony/unit overlay blit

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6b22_0002` | 108574 | 47 | mapdraw | unknown | inferred |  |
| `FUN_6b22_00ea` | 108621 | 12 | mapdraw | unknown | inferred |  |
| `FUN_6b22_0102` | 108633 | 66 | mapdraw | unknown | inferred |  |
| `FUN_6b22_0248` | 108699 | 12 | mapdraw | unknown | inferred |  |
| `FUN_6b22_034c` | 108711 | 24 | mapdraw | unknown | inferred |  |
| `FUN_6b22_03f6` | 108735 | 25 | mapdraw | unknown | inferred |  |
| `FUN_6b22_0428` | 108760 | 28 | mapdraw | unknown | inferred |  |
| `FUN_6b22_04bc` | 108788 | 56 | mapdraw | unknown | inferred |  |
| `FUN_6b22_058e` | 108844 | 73 | mapdraw | unknown | inferred |  |

### Segment `6b7e` (5 defs) — mapdraw — Map viewport refresh / camera save-restore

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6b7e_0004` | 108917 | 36 | mapdraw | unknown | inferred |  |
| `FUN_6b7e_00c0` | 108953 | 38 | mapdraw | unknown | inferred |  |
| `FUN_6b7e_018a` | 108991 | 22 | mapdraw | unknown | inferred |  |
| `FUN_6b7e_01f6` | 109013 | 15 | mapdraw | unknown | inferred |  |
| `FUN_6b7e_0218` | 109028 | 15 | mapdraw | unknown | inferred |  |

### Segment `6ba1` (18 defs) — mapdraw — Map tile neighbor masks / viewport blit helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6ba1_000c` | 109043 | 101 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_01b4` | 109144 | 67 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_0314` | 109211 | 32 | mapdraw | Build NSEW neighbor presence mask from map plane | inferred |  |
| `FUN_6ba1_0374` | 109243 | 32 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_03e4` | 109275 | 19 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_041e` | 109294 | 33 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_0484` | 109327 | 32 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_04e4` | 109359 | 35 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_0558` | 109394 | 15 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_05b8` | 109409 | 15 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_061c` | 109424 | 17 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_067c` | 109441 | 15 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_06e0` | 109456 | 107 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_0938` | 109563 | 189 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_0d6c` | 109752 | 145 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_1028` | 109897 | 29 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_10ae` | 109926 | 14 | mapdraw | unknown | inferred |  |
| `FUN_6ba1_10be` | 109940 | 50 | mapdraw | unknown | inferred |  |

### Segment `6cb2` (21 defs) — ui — Info / dialog text panels (281f compositor)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6cb2_0000` | 109990 | 31 | ui | unknown | inferred |  |
| `FUN_6cb2_0058` | 110021 | 37 | ui | unknown | inferred |  |
| `FUN_6cb2_00c0` | 110058 | 18 | ui | unknown | inferred |  |
| `FUN_6cb2_00fc` | 110076 | 30 | ui | unknown | inferred |  |
| `FUN_6cb2_0178` | 110106 | 63 | ui | unknown | inferred |  |
| `FUN_6cb2_0276` | 110169 | 21 | ui | unknown | inferred |  |
| `FUN_6cb2_02c4` | 110190 | 40 | ui | unknown | inferred |  |
| `FUN_6cb2_033a` | 110230 | 22 | ui | unknown | inferred |  |
| `FUN_6cb2_039c` | 110252 | 10 | ui | unknown | inferred |  |
| `FUN_6cb2_03bc` | 110262 | 24 | ui | unknown | inferred |  |
| `FUN_6cb2_0424` | 110286 | 22 | ui | unknown | inferred |  |
| `FUN_6cb2_048c` | 110308 | 44 | ui | unknown | inferred |  |
| `FUN_6cb2_05ce` | 110352 | 99 | ui | unknown | inferred |  |
| `FUN_6cb2_07e6` | 110451 | 186 | ui | unknown | inferred |  |
| `FUN_6cb2_0eac` | 110637 | 273 | ui | unknown | inferred |  |
| `FUN_6cb2_1820` | 110910 | 113 | ui | unknown | inferred |  |
| `FUN_6cb2_1ba8` | 111023 | 107 | ui | unknown | inferred |  |
| `FUN_6cb2_1f28` | 111130 | 39 | ui | unknown | inferred |  |
| `FUN_6cb2_214a` | 111169 | 109 | ui | unknown | inferred |  |
| `FUN_6cb2_2322` | 111278 | 1252 | ui | unknown | inferred |  |
| `FUN_6cb2_24b8` | 112530 | 2067 | ui | unknown | inferred |  |

### Segment `6f30` (3 defs) — ui — Splash / image load+blit via resource stream

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6f30_0004` | 114597 | 12 | ui | unknown | inferred |  |
| `FUN_6f30_002e` | 114609 | 14 | ui | unknown | inferred |  |
| `FUN_6f30_0062` | 114623 | 147 | ui | unknown | inferred |  |

### Segment `6f74` (58 defs) — ui — Text layout / flow-wrap dialog compositor (incl. FUN_6f74_1198)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_6f74_0000` | 114770 | 20 | ui | unknown | inferred |  |
| `FUN_6f74_0042` | 114790 | 35 | ui | unknown | inferred |  |
| `FUN_6f74_00c2` | 114825 | 15 | ui | unknown | inferred |  |
| `FUN_6f74_00ec` | 114840 | 15 | ui | unknown | inferred |  |
| `FUN_6f74_033c` | 114855 | 31 | ui | unknown | inferred |  |
| `FUN_6f74_03d0` | 114886 | 9 | ui | unknown | inferred |  |
| `FUN_6f74_03ec` | 114895 | 12 | ui | unknown | inferred |  |
| `FUN_6f74_0404` | 114907 | 13 | ui | unknown | inferred |  |
| `FUN_6f74_042c` | 114920 | 13 | ui | unknown | inferred |  |
| `FUN_6f74_0446` | 114933 | 23 | ui | unknown | inferred |  |
| `FUN_6f74_04f6` | 114956 | 27 | ui | unknown | inferred |  |
| `FUN_6f74_0538` | 114983 | 66 | ui | unknown | inferred |  |
| `FUN_6f74_0642` | 115049 | 33 | ui | unknown | inferred |  |
| `FUN_6f74_06d0` | 115082 | 100 | ui | unknown | inferred |  |
| `FUN_6f74_089a` | 115182 | 36 | ui | unknown | inferred |  |
| `FUN_6f74_08fa` | 115218 | 17 | ui | unknown | inferred |  |
| `FUN_6f74_092a` | 115235 | 16 | ui | unknown | inferred |  |
| `FUN_6f74_095a` | 115251 | 25 | ui | unknown | inferred |  |
| `FUN_6f74_098a` | 115276 | 17 | ui | unknown | inferred |  |
| `FUN_6f74_09ba` | 115293 | 14 | ui | unknown | inferred |  |
| `FUN_6f74_09e2` | 115307 | 13 | ui | unknown | inferred |  |
| `FUN_6f74_0a00` | 115320 | 99 | ui | unknown | inferred |  |
| `FUN_6f74_0be8` | 115419 | 20 | ui | unknown | inferred |  |
| `FUN_6f74_0c22` | 115439 | 9 | ui | unknown | inferred |  |
| `FUN_6f74_0c32` | 115448 | 74 | ui | unknown | inferred |  |
| `FUN_6f74_0d44` | 115522 | 86 | ui | unknown | inferred |  |
| `FUN_6f74_0f16` | 115608 | 16 | ui | unknown | inferred |  |
| `FUN_6f74_0f3c` | 115624 | 103 | ui | unknown | inferred |  |
| `FUN_6f74_112a` | 115727 | 20 | ui | unknown | inferred |  |
| `FUN_6f74_116c` | 115747 | 9 | ui | unknown | inferred |  |
| `FUN_6f74_1198` | 115756 | 126 | ui | Flow-wrap text layout for dialog / wood-panel body | known | src/core/new_game.c; docs/assets.md |
| `FUN_6f74_14c6` | 115882 | 267 | ui | unknown | inferred |  |
| `FUN_6f74_1a3c` | 116149 | 17 | ui | unknown | inferred |  |
| `FUN_6f74_1a78` | 116166 | 29 | ui | unknown | inferred |  |
| `FUN_6f74_1ae8` | 116195 | 26 | ui | unknown | inferred |  |
| `FUN_6f74_1b7c` | 116221 | 86 | ui | unknown | inferred |  |
| `FUN_6f74_1e14` | 116307 | 84 | ui | unknown | inferred |  |
| `FUN_6f74_201e` | 116391 | 78 | ui | unknown | inferred |  |
| `FUN_6f74_2278` | 116469 | 50 | ui | unknown | inferred |  |
| `FUN_6f74_248e` | 116519 | 40 | ui | unknown | inferred |  |
| `FUN_6f74_255e` | 116559 | 16 | ui | unknown | inferred |  |
| `FUN_6f74_2580` | 116575 | 567 | ui | unknown | inferred |  |
| `FUN_6f74_3084` | 117142 | 13 | ui | unknown | inferred |  |
| `FUN_6f74_309c` | 117155 | 92 | ui | unknown | inferred |  |
| `FUN_6f74_32a4` | 117247 | 191 | ui | unknown | inferred |  |
| `FUN_6f74_36ca` | 117438 | 19 | ui | unknown | inferred |  |
| `FUN_6f74_36fc` | 117457 | 11 | ui | unknown | inferred |  |
| `FUN_6f74_3704` | 117468 | 17 | ui | unknown | inferred |  |
| `FUN_6f74_372e` | 117485 | 11 | ui | unknown | inferred |  |
| `FUN_6f74_3744` | 117496 | 9 | ui | unknown | inferred |  |
| `FUN_6f74_3760` | 117505 | 12 | ui | unknown | inferred |  |
| `FUN_6f74_378a` | 117517 | 12 | ui | unknown | inferred |  |
| `FUN_6f74_37a2` | 117529 | 12 | ui | unknown | inferred |  |
| `FUN_6f74_37cc` | 117541 | 12 | ui | unknown | inferred |  |
| `FUN_6f74_37f6` | 117553 | 11 | ui | unknown | inferred |  |
| `FUN_6f74_37fc` | 117564 | 25 | ui | unknown | inferred |  |
| `FUN_6f74_3848` | 117589 | 16 | ui | unknown | inferred |  |
| `FUN_6f74_388a` | 117605 | 219 | ui | unknown | inferred |  |

### Segment `7314` (9 defs) — platform — Config/name file line parse (comma fields)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7314_0000` | 117824 | 14 | platform | unknown | inferred |  |
| `FUN_7314_001a` | 117838 | 66 | platform | unknown | inferred |  |
| `FUN_7314_0106` | 117904 | 27 | platform | unknown | inferred |  |
| `FUN_7314_015e` | 117931 | 28 | platform | unknown | inferred |  |
| `FUN_7314_0198` | 117959 | 10 | platform | unknown | inferred |  |
| `FUN_7314_01b6` | 117969 | 10 | platform | unknown | inferred |  |
| `FUN_7314_01c8` | 117979 | 10 | platform | unknown | inferred |  |
| `FUN_7314_01da` | 117989 | 20 | platform | unknown | inferred |  |
| `FUN_7314_0208` | 118009 | 64 | platform | unknown | inferred |  |

### Segment `733a` (12 defs) — ui — New-game / CUSTOMIZE UI

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_733a_0000` | 118073 | 19 | ui | CUSTOMIZE / difficulty-style UI entry | known | src/core/new_game.c |
| `FUN_733a_002c` | 118092 | 47 | ui | unknown | inferred |  |
| `FUN_733a_01a4` | 118139 | 34 | ui | unknown | inferred |  |
| `FUN_733a_0270` | 118173 | 138 | ui | CUSTOMIZE / new-game UI helper | known | src/core/new_game.c |
| `FUN_733a_04d0` | 118311 | 21 | ui | unknown | inferred |  |
| `FUN_733a_0512` | 118332 | 58 | ui | CUSTOMIZE / new-game UI helper | known | src/core/new_game.c |
| `FUN_733a_06a4` | 118390 | 41 | ui | unknown | inferred |  |
| `FUN_733a_0790` | 118431 | 111 | ui | unknown | inferred |  |
| `FUN_733a_0992` | 118542 | 12 | ui | unknown | inferred |  |
| `FUN_733a_09c6` | 118554 | 47 | ui | unknown | inferred |  |
| `FUN_733a_0b3e` | 118601 | 41 | ui | unknown | inferred |  |
| `FUN_733a_0c2a` | 118642 | 215 | ui | unknown | inferred |  |

### Segment `7421` (2 defs) — platform — Startup config fread + argv parse

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7421_0188` | 118857 | 27 | platform | unknown | inferred |  |
| `FUN_7421_025a` | 118884 | 66 | platform | unknown | inferred |  |

### Segment `7455` (7 defs) — mapgen — Map plane buffer alloc (pitch 0x853a → terrain/L2/L3)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7455_0000` | 118950 | 33 | mapgen | unknown | inferred |  |
| `FUN_7455_0058` | 118983 | 58 | mapgen | Allocate terrain/layer2/layer3 map planes from pitch×height | inferred |  |
| `FUN_7455_0122` | 119041 | 20 | mapgen | unknown | inferred |  |
| `FUN_7455_0166` | 119061 | 65 | mapgen | unknown | inferred |  |
| `FUN_7455_02a6` | 119126 | 58 | mapgen | unknown | inferred |  |
| `FUN_7455_03b0` | 119184 | 28 | mapgen | unknown | inferred |  |
| `FUN_7455_0434` | 119212 | 70 | mapgen | unknown | inferred |  |

### Segment `74a4` (3 defs) — ui — Map menu bar load from MENU.TXT (4b58 widgets)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_74a4_0000` | 119282 | 236 | ui | unknown | inferred |  |
| `FUN_74a4_0b0a` | 119518 | 39 | ui | unknown | inferred |  |
| `FUN_74a4_0bbe` | 119557 | 26 | ui | unknown | inferred |  |

### Segment `7562` (5 defs) — save — Hall of Fame / score-file format & list UI

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7562_0008` | 119583 | 11 | save | unknown | inferred |  |
| `FUN_7562_0034` | 119594 | 12 | save | unknown | inferred |  |
| `FUN_7562_0052` | 119606 | 111 | save | unknown | inferred |  |
| `FUN_7562_030a` | 119717 | 65 | save | unknown | inferred |  |
| `FUN_7562_04e8` | 119782 | 69 | save | unknown | inferred |  |

### Segment `75c2` (20 defs) — save — Savegame R/W of units / colonies / tribes / flags

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_75c2_0000` | 119851 | 130 | save | unknown | inferred |  |
| `FUN_75c2_0204` | 119981 | 24 | save | unknown | inferred |  |
| `FUN_75c2_024c` | 120005 | 22 | save | unknown | inferred |  |
| `FUN_75c2_0288` | 120027 | 94 | save | Savegame write: flags/units/colonies/tribes blobs | known | docs/savegame.md |
| `FUN_75c2_0840` | 120121 | 67 | save | unknown | inferred |  |
| `FUN_75c2_0940` | 120188 | 356 | save | Savegame load counterpart to FUN_75c2_0288 | known | docs/savegame.md |
| `FUN_75c2_10ae` | 120544 | 160 | save | unknown | inferred |  |
| `FUN_75c2_1380` | 120704 | 31 | save | unknown | inferred |  |
| `FUN_75c2_13dc` | 120735 | 20 | save | unknown | inferred |  |
| `FUN_75c2_1418` | 120755 | 19 | save | unknown | inferred |  |
| `FUN_75c2_144c` | 120774 | 56 | save | unknown | inferred |  |
| `FUN_75c2_1770` | 120830 | 519 | save | unknown | inferred |  |
| `FUN_75c2_20e2` | 121349 | 123 | save | unknown | inferred |  |
| `FUN_75c2_2324` | 121472 | 22 | save | unknown | inferred |  |
| `FUN_75c2_235c` | 121494 | 191 | save | unknown | inferred |  |
| `FUN_75c2_2758` | 121685 | 14 | save | unknown | inferred |  |
| `FUN_75c2_276e` | 121699 | 10 | save | unknown | inferred |  |
| `FUN_75c2_2778` | 121709 | 245 | save | unknown | inferred |  |
| `FUN_75c2_2d28` | 121954 | 14 | save | unknown | inferred |  |
| `FUN_75c2_2d46` | 121968 | 369 | save | unknown | inferred |  |

### Segment `78d8` (4 defs) — platform — Resource stream buffer alloc / cursor / far-ptr load

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_78d8_0000` | 122337 | 17 | platform | unknown | inferred |  |
| `FUN_78d8_0022` | 122354 | 21 | platform | unknown | inferred |  |
| `FUN_78d8_0054` | 122375 | 38 | platform | unknown | inferred |  |
| `FUN_78d8_00c4` | 122413 | 63 | platform | Load/reload resource far-ptrs from stream; fatal after 3 reloads | inferred |  |

### Segment `78ef` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_78ef_0002` | 122476 | 245 | unknown | unknown | unknown |  |

### Segment `7939` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7939_000c` | 122721 | 41 | unknown | unknown | unknown |  |

### Segment `7944` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7944_000e` | 122762 | 41 | unknown | unknown | unknown |  |

### Segment `7952` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7952_0000` | 122803 | 70 | unknown | unknown | unknown |  |

### Segment `7962` (3 defs) — platform — Resource file open / close handle helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7962_0000` | 122873 | 97 | platform | unknown | inferred |  |
| `FUN_7962_020a` | 122970 | 11 | platform | unknown | inferred |  |
| `FUN_7962_021c` | 122981 | 39 | platform | unknown | inferred |  |

### Segment `798d` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_798d_0000` | 123020 | 112 | unknown | unknown | unknown |  |

### Segment `79a8` (4 defs) — platform — Compressed resource stream I/O + progress callback

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_79a8_000a` | 123132 | 8 | platform | unknown | inferred |  |
| `FUN_79a8_0014` | 123140 | 14 | platform | unknown | inferred |  |
| `FUN_79a8_002a` | 123154 | 15 | platform | unknown | inferred |  |
| `FUN_79a8_004a` | 123169 | 190 | platform | Wire compressed stream I/O (callbacks + heap buffer + consume loop) | inferred |  |

### Segment `79db` (1 defs) — platform — Chunked DOS file read into far buffer

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_79db_000c` | 123359 | 70 | platform | unknown | inferred |  |

### Segment `79ec` (2 defs) — platform — Resource stream progress pump / dispatch

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_79ec_0004` | 123429 | 38 | platform | unknown | inferred |  |
| `FUN_79ec_0082` | 123467 | 58 | platform | unknown | inferred |  |

### Segment `7a05` (5 defs) — platform — Fatal/abort error text + INT10 video reset

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a05_0000` | 123525 | 38 | platform | unknown | inferred |  |
| `FUN_7a05_00a4` | 123563 | 45 | platform | unknown | inferred |  |
| `FUN_7a05_014e` | 123608 | 20 | platform | unknown | inferred |  |
| `FUN_7a05_0180` | 123628 | 71 | platform | unknown | inferred |  |
| `FUN_7a05_03ce` | 123699 | 49 | platform | unknown | inferred |  |

### Segment `7a4c` (2 defs) — ui — VGA DAC write + PIT fade-speed calibrate

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a4c_0000` | 123748 | 45 | ui | unknown | inferred |  |
| `FUN_7a4c_006a` | 123793 | 88 | ui | unknown | inferred |  |

### Segment `7a65` (3 defs) — ui — Map tip blit + parameterized dialog text (6f74)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a65_0008` | 123881 | 38 | ui | unknown | inferred |  |
| `FUN_7a65_00e2` | 123919 | 19 | ui | unknown | inferred |  |
| `FUN_7a65_0124` | 123938 | 29 | ui | unknown | inferred |  |

### Segment `7a7c` (1 defs) — ui — Load 768-byte VGA palette from resource

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a7c_000e` | 123967 | 26 | ui | unknown | inferred |  |

### Segment `7a83` (2 defs) — ui — Palette RGB fade / channel shift

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a83_0002` | 123993 | 35 | ui | unknown | inferred |  |
| `FUN_7a83_002a` | 124028 | 84 | ui | unknown | inferred |  |

### Segment `7a9d` (1 defs) — unknown

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7a9d_0004` | 124112 | 17 | unknown | unknown | unknown |  |

### Segment `7aa1` (2 defs) — ui — Parameterized dialog / message box (# subst)

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7aa1_0002` | 124129 | 19 | ui | unknown | inferred |  |
| `FUN_7aa1_003a` | 124148 | 59 | ui | unknown | inferred |  |

### Segment `7ab3` (1 defs) — ui — VGA vsync + DAC palette read

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ab3_0008` | 124207 | 44 | ui | Wait VGA retrace then read DAC RGB palette via 0x3c7/0x3c9 | known |  |

### Segment `7ab9` (2 defs) — ui — Image/resource load + blit error codes

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ab9_0000` | 124251 | 38 | ui | unknown | inferred |  |
| `FUN_7ab9_00be` | 124289 | 32 | ui | unknown | inferred |  |

### Segment `7acf` (2 defs) — platform — Far-buffer alloc into handle struct

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7acf_0002` | 124321 | 24 | platform | unknown | inferred |  |
| `FUN_7acf_003c` | 124345 | 24 | platform | unknown | inferred |  |

### Segment `7ad6` (1 defs) — platform — Far-buffer free / clear handle struct

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ad6_000e` | 124369 | 21 | platform | unknown | inferred |  |

### Segment `7ada` (5 defs) — platform — DOS heap alloc / resize / high-water tracking

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7ada_0006` | 124390 | 18 | platform | unknown | inferred |  |
| `FUN_7ada_0022` | 124408 | 84 | platform | unknown | inferred |  |
| `FUN_7ada_01a0` | 124492 | 9 | platform | unknown | inferred |  |
| `FUN_7ada_01aa` | 124501 | 27 | platform | unknown | inferred |  |
| `FUN_7ada_01fa` | 124528 | 36 | platform | unknown | inferred |  |

### Segment `7b04` (2 defs) — platform — DOS free-memory probe (INT21) + size max

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7b04_0002` | 124564 | 16 | platform | unknown | inferred |  |
| `FUN_7b04_001e` | 124580 | 29 | platform | unknown | inferred |  |

### Segment `7b08` (5 defs) — platform — Growable far-buffer / arena alloc helpers

| Symbol | Line | Size | System | Purpose | Confidence | Links |
|--------|-----:|-----:|--------|---------|------------|-------|
| `FUN_7b08_000e` | 124609 | 40 | platform | unknown | inferred |  |
| `FUN_7b08_009e` | 124649 | 25 | platform | unknown | inferred |  |
| `FUN_7b08_00dc` | 124674 | 22 | platform | unknown | inferred |  |
| `FUN_7b08_0118` | 124696 | 34 | platform | unknown | inferred |  |
| `FUN_7b08_0182` | 124730 | 398 | platform | unknown | inferred |  |

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

