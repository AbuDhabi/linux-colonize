/*
 * AI-hot map / RNG / move-cost accessors.
 *
 * Bodies rewritten from original_sources_decompiled/viceroy_unpacked.c for
 * readability. Far-call thunks (FUN_281f_*) are inlined into the real
 * FUN_137f_* / FUN_13e4_* / FUN_19ef_* implementations where the thunk was
 * only FUN_210d_0d91() + a near call.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

/* Forward: real LCG implementations live in FUN_19ef / FUN_1d1d (ported as
 * src/core/dos_rng.c). Declared here so call sites stay explicit. */
extern void dos_rng_reseed_from_timer(uint16_t timer_word); /* FUN_19ef_002c path */
extern int dos_rng_range(int lo, int hi_inclusive);         /* FUN_19ef_0032 / FUN_1d1d_0e04 */

/* ====================================================================== */
/* RNG                                                                    */
/* ====================================================================== */

/* Ghidra: FUN_281f_04ca | ai_reseed_from_timer
 * Thunk → FUN_19ef_002c. Euro 6d8e and Indian 1816 both reseed from
 * VICEROY_DS_TIMER_WORD (VR_SEED locks this to 100). */
void ai_reseed_from_timer(uint16_t timer_word) {
  dos_rng_reseed_from_timer(timer_word);
}

/* Ghidra: FUN_281f_04d4 | rng_range
 * Thunk → FUN_19ef_0032. Inclusive range; every call advances the LCG. */
int rng_range(int lo, int hi_inclusive) {
  return dos_rng_range(lo, hi_inclusive);
}

/* ====================================================================== */
/* Map plane readers                                                      */
/* ====================================================================== */

/*
 * Pitch-addressed byte load:
 *   map_ptr[y * pitch + x]
 * Ghidra returns CONCAT11(hi, lo) for 16-bit far pointers; we only need the
 * byte value for AI scoring / costing.
 */

/* Ghidra: FUN_137f_010e | terrain_byte  (FUN_281f_072c was a thunk here)
 *
 *   pitch = *(int *)0x853a
 *   base  = *(int *)0x015c
 *   return *(uint8_t *)(base + y * pitch + x)
 */
uint8_t terrain_byte(int x, int y) {
  /* int pitch = g_map_pitch; uint8_t *base = g_terrain_plane; */
  /* return base[y * pitch + x]; */
  (void)x;
  (void)y;
  return 0; /* not linked to a live DS image */
}

/* Ghidra: FUN_137f_0142 | layer2_byte  (FUN_281f_0754 thunk)
 * Same formula; base at DS:0x0160. */
uint8_t layer2_byte(int x, int y) {
  (void)x;
  (void)y;
  return 0;
}

/* Ghidra: FUN_137f_01ac | layer3_byte — base at DS:0x0164. */
uint8_t layer3_byte(int x, int y) {
  (void)x;
  (void)y;
  return 0;
}

/* Ghidra: FUN_137f_0194 | layer3_ptr — address of layer3 cell for RMW. */
uint8_t *layer3_ptr(int x, int y) {
  /* return &g_layer3[y * g_map_pitch + x]; */
  (void)x;
  (void)y;
  return 0;
}

/* Ghidra: FUN_281f_072c inlined → terrain_byte.
 * AI uses (terrain_byte(x,y) & VICEROY_TERRAIN_RIVER_BIT) for minor-river cost. */
int tile_has_minor_river(int x, int y) {
  return (terrain_byte(x, y) & VICEROY_TERRAIN_RIVER_BIT) != 0;
}

/* Ghidra: FUN_281f_0754 inlined → layer2_byte & FA mask.
 * Linux: ai_mask_fa_flags. */
int tile_fa_flags(int x, int y) {
  return (int)(layer2_byte(x, y) & VICEROY_LAYER2_FA_MASK);
}

/* Ghidra: FUN_137f_01ca | continent_id  (FUN_281f_06b4 thunk)
 * Low nibble of layer3. Linux: ai_continent_id. */
int continent_id(int x, int y) {
  return (int)(layer3_byte(x, y) & 0x0fu);
}

/* Ghidra: FUN_137f_0228 | set_owner_nibble
 * Writes high nibble of layer3; preserves low (continent) nibble.
 * Linux: ai_set_owner_nibble. param nation_or_ff: 0..14 owner, 0xf unowned.
 * Side path when nation < 4 also probes colonies / UI — not needed for Brave claim. */
void set_owner_nibble(int x, int y, int nation_or_ff) {
  uint8_t *p = layer3_ptr(x, y);
  if (!p) {
    return;
  }
  *p = (uint8_t)((*p & 0x0fu) | ((nation_or_ff & 0x0f) << 4));
}

/* Owner high nibble; 0xf → unowned (−1 in Linux). */
int owner_nibble(int x, int y) {
  int hi = (layer3_byte(x, y) >> 4) & 0x0f;
  return (hi == VICEROY_OWNER_UNOWNED) ? -1 : hi;
}

/* Ghidra: FUN_13e4_0074 | ocean_or_high_seas  (FUN_281f_0768 thunk)
 * True when terrain type is 0x19 (ocean) or 0x1a (high seas).
 * Linux: ai_is_ocean_hs. */
int ocean_or_high_seas(int x, int y) {
  uint8_t t = (uint8_t)(terrain_byte(x, y) & VICEROY_TERRAIN_TYPE_MASK);
  return (t == VICEROY_TERRAIN_OCEAN || t == VICEROY_TERRAIN_HIGH_SEAS) ? 1 : 0;
}

/* Ghidra: FUN_13e4_000e | decode_terrain_class
 * Hill bit 0x20 → class 0x1b/0x1c from major 0x80; else low 5 bits.
 * Linux: ai_dos_terr_class. */
int decode_terrain_class(uint8_t terrain) {
  if ((terrain & VICEROY_TERRAIN_HILL_BIT) != 0) {
    /* unaff major-bit path in decomp; port uses fixed 0x1b/0x1c mapping */
    return 0x1b; /* see ai.c for major-bit nuance */
  }
  return (int)(terrain & VICEROY_TERRAIN_TYPE_MASK);
}

/* Ghidra: FUN_281f_074a → FUN_137f_02f8 | tile_explore_mask
 * Fourth map plane at DS:0x168 (not layer3 @0x164). Used as
 * (0x10 << euro_nation) bit test for +2 explore bonus — Europeans only. */
int tile_explore_mask(int x, int y) {
  (void)x;
  (void)y;
  /* return g_explore_plane[y * pitch + x]; */
  return 0;
}

/* Ghidra: coarse fog cell at DS:-0x6056 pitch 0x12 | coarse_fog_unseen
 * Index = (x>>2) + (y>>2)*18; byte 0 = unseen. Early NEW WORLD ≈ all zero. */
int coarse_fog_unseen(int x, int y) {
  (void)x;
  (void)y;
  return 1; /* annotated: early-game all-unseen */
}

/* Ghidra: FUN_281f_0682 → FUN_137f_0314 | tile_owner_or_presence
 * −1 if not inset or layer2 bit0 clear; else owner from FUN_137f_0200. */
int tile_owner_or_presence(int x, int y) {
  return owner_nibble(x, y); /* simplified: owner hi-nibble or −1 */
}

/* Ghidra: FUN_281f_078c | terrain_class_at — decode_terrain_class(terrain_byte). */
int terrain_class_at(int x, int y) {
  return decode_terrain_class(terrain_byte(x, y));
}

/* Ghidra: FUN_281f_0302 → FUN_137f_000a | map_tile_in_bounds — inset interior. */
int map_tile_in_bounds(int x, int y) {
  (void)x;
  (void)y;
  return 1;
}


/* Ghidra: FUN_124c_0040 | dos_dist
 * Diagonal-ish distance on absolute deltas: min/2 + max.
 * Linux: ai_dos_dist. */
int dos_dist(int dx, int dy) {
  if (dx < 0) {
    dx = -dx;
  }
  if (dy < 0) {
    dy = -dy;
  }
  if (dy < dx) {
    return (dy >> 1) + dx;
  }
  return (dx >> 1) + dy;
}

/*
 * Ghidra: FUN_465b_0000 | move_spent_add  (cost portion used by quiet Braves)
 *
 * Full 465b is ~426 lines (combat / colony / foreign-tile branches). The quiet
 * NEW WORLD Brave path only needs the opening cost calculation:
 *
 *   spent = terr_cost_table[class(dest)] * 3
 *   if both tiles have fa-mask & 0x0a → spent = 1
 *   if both have minor-river & move is cardinal → spent = 1
 *   if dest has tribe (layer2&2) and owned and spent > 3 → spent = 3
 *
 * Linux: ai_dos_move_spent. Remaining 465b (ocean-transition force-to-max,
 * foreign combat) still RE-open for Sioux/Apache spent mismatches.
 */
int move_spent_cost_only(
  int unit_index,
  int from_x,
  int from_y,
  int to_x,
  int to_y,
  int dir
) {
  (void)unit_index;
  int terr_class = decode_terrain_class(terrain_byte(to_x, to_y));
  /* g_terr_cost[terr_class] at DS:0x2f76 — Linux k_ai_dos_terr_cost */
  int spent = /* g_terr_cost[terr_class & 31] */ 0 * 3;
  (void)terr_class;

  int fa_from = tile_fa_flags(from_x, from_y);
  int fa_to = tile_fa_flags(to_x, to_y);
  if (fa_from != 0 && fa_to != 0) {
    spent = 1;
  }

  /* Cardinal = even dir (N/E/S/W). Decomp: from_x==to_x || from_y==to_y. */
  if (tile_has_minor_river(from_x, from_y) && tile_has_minor_river(to_x, to_y) &&
      (dir & 1) == 0) {
    spent = 1;
  }

  /* FUN_281f_06be / tribe-tile cap: only when dest layer2&2. */
  if ((layer2_byte(to_x, to_y) & VICEROY_LAYER2_TRIBE) != 0) {
    if (owner_nibble(to_x, to_y) >= 0 && spent > 3) {
      spent = 3;
    }
  }

  if (spent > 100) {
    spent = 1;
  }
  return spent;
}

/* ====================================================================== */
/* Unit readiness                                                         */
/* ====================================================================== */

/*
 * Ghidra: FUN_1427_13b0 | unit_has_moves_remaining  (FUN_281f_097a thunk)
 *
 * True when unit index is live, belongs to g_active_nation_id, is not
 * suppressed (flag 0x80 unless type==wagon 0x0b), and moves_spent < max_mp
 * (FUN_1427_065a allotment — Brave thirds path → 3).
 *
 * Linux mid-turn pulse approximates with spent < max_mp (allows 465b to push
 * spent past max, matching 097a loop behavior).
 */
int unit_has_moves_remaining(int unit_index) {
  if (unit_index < 0 /* || unit_index >= g_unit_count */) {
    return 0;
  }
  ViceroyUnit *u = VICEROY_UNIT_AT(unit_index);
  if ((int)u->x < 0) {
    return 0; /* decomp: x signed check — inactive */
  }
  /* nation must match g_active_nation_id at DS:0x5394 */
  /* if ((u->nation_id) != g_active_nation_id) return 0; */
  if ((u->unk_04 & 0x80) != 0 && u->type != 0x0b) {
    return 0;
  }
  /* max_mp = FUN_1427_065a(); return u->moves_spent < max_mp; */
  (void)u;
  return 0;
}
