/*
 * Quiet NEW WORLD Brave move scoring — ASM LAB_521d_4ea9 / flags 0x10|0x20.
 *
 * Recovered from original_sources_decompiled/viceroy_unpacked.c FUN_521d_20e6
 * and viceroy_unpacked.asm around CODE_125:521d:4ea9.
 *
 * Applies only when unit type table flags at DS:0x523d include 0x10 or 0x20
 * (Brave type 19 → flags 0x38). Other unit kinds use different bases in the
 * same scorer and are out of scope here.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern int rng_range(int lo, int hi_inclusive);
extern int tile_has_minor_river(int x, int y);
extern int tile_fa_flags(int x, int y);
extern int ocean_or_high_seas(int x, int y);
extern int owner_nibble(int x, int y);
extern uint8_t terrain_byte(int x, int y);
extern int decode_terrain_class(uint8_t terrain);
extern int map_tile_in_bounds(int x, int y);
extern int coarse_fog_unseen(int x, int y);
extern int tile_explore_mask(int x, int y);
extern int tile_owner_or_presence(int x, int y);
extern int tile_tribe_or_presence(int x, int y);
extern int unit_index_on_tile(int x, int y);
extern int diplomacy_flags(int self_nation, int other_nation);
extern int unit_type_combat_byte(int unit_type);

/* Direction deltas at DS:0xbe / 0xb4 — same order as Linux k_ai_dir8_*. */
static const int k_dir8_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int k_dir8_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

/* ====================================================================== */
/* Score terms                                                            */
/* ====================================================================== */

/* Ghidra: LAB_521d_4fb4 | quiet_score_base — FUN_281f_04d4(1,3). */
int quiet_score_base(void) {
  return rng_range(1, 3); /* burns LCG */
}

/*
 * Ghidra: LAB_521d_4fc8..500a | quiet_score_terrain
 *
 * If (unit has minor-river and dest has river and dir is cardinal)
 *    OR (unit has fa-mask and dest has fa-mask):
 *      score += 1
 * Else:
 *      score -= terr_cost_table[terr_class]   // NOT ×3
 */
int quiet_score_terrain(int score, int unit_x, int unit_y, int nx, int ny, int dir) {
  int unit_river = tile_has_minor_river(unit_x, unit_y);
  int unit_fa = tile_fa_flags(unit_x, unit_y) != 0;
  int dest_river = tile_has_minor_river(nx, ny);
  int dest_fa = tile_fa_flags(nx, ny) != 0;
  int cardinal = (dir & 1) == 0;

  if ((unit_river && dest_river && cardinal) || (unit_fa && dest_fa)) {
    return score + 1; /* LAB_521d_4ffa */
  }
  int terr = decode_terrain_class(terrain_byte(nx, ny)) & 31;
  static const uint8_t k_terr_cost[32] = {
      1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 2, 2, 2, 2, 3, 3,
      2, 1, 2, 2, 2, 2, 3, 3, 2, 1, 1, 3, 2, 13, 255, 255};
  return score - (int)k_terr_cost[terr];
}

/*
 * Ghidra: LAB_521d_54f5 entry | quiet_lab_54f5_gate
 *
 * Enter facing / military −10 / bVar20 fog only when:
 *   (unit_index_on_tile(dest) < 0 && tile_tribe_or_presence(dest) < 0)
 *   || owner_nibble(dest) == self_nation
 *
 * Callees: FUN_281f_07e0 (empty), FUN_281f_06d2 (tribe/presence),
 * FUN_281f_06dc (owner).
 */
int quiet_lab_54f5_gate(int dest_x, int dest_y, int nation_id) {
  int own = owner_nibble(dest_x, dest_y);
  if (own == nation_id) {
    return 1;
  }
  if (unit_index_on_tile(dest_x, dest_y) < 0 &&
      tile_tribe_or_presence(dest_x, dest_y) < 0) {
    return 1;
  }
  return 0;
}

/*
 * Ghidra: LAB_521d_54f5 facing | quiet_score_facing
 *
 * diff = circular |last_dir - dir| clamped to 0..4
 * score += -diff * diff * 2
 */
int quiet_score_facing(int score, int dir, int last_dir) {
  if (last_dir < 0 || last_dir > 7) {
    return score;
  }
  int diff = last_dir - dir;
  if (diff < 1) {
    diff = ~diff + 1; /* decomp two's-complement abs when diff < 1 */
  }
  if (diff > 4) {
    diff = -(diff - 8); /* decomp: -(local_6e + -8) */
  }
  return score + diff * diff * -2;
}

/*
 * Ghidra: LAB_521d_54f5 military neighbor loop | quiet_score_military_minus10
 *
 * For each of 8 neighbors of dest:
 *   presence = 0682; if foreign && (0a38 & 0x60)==0x20 && mover combat==0:
 *     for each unit on nbr with type-table combat != 0: score -= 10
 * Brave type 19 has combat byte 0 → enters the path; early NEW WORLD usually
 * finds no war-flagged foreign combat units → often a no-op.
 * Annotated: diplomacy_flags stub returns 0 → −10 never fires here.
 * Linux cutover walks the live unit pool when diplomacy is wired.
 */
int quiet_score_military_minus10(
  int score,
  int dest_x,
  int dest_y,
  int nation_id,
  int mover_type
) {
  if (unit_type_combat_byte(mover_type) != 0) {
    return score;
  }
  for (int n = 0; n < 8; ++n) {
    int nx = dest_x + k_dir8_dx[n];
    int ny = dest_y + k_dir8_dy[n];
    int presence = tile_owner_or_presence(nx, ny);
    if (presence < 0 || presence == nation_id) {
      continue;
    }
    if ((diplomacy_flags(nation_id, presence) & 0x60) != 0x20) {
      continue;
    }
    /* DOS: FUN_281f_07e0 / 02e4 unit walk; subtract 10 per combat≠0 unit. */
    (void)nx;
    (void)ny;
  }
  return score;
}

/*
 * Ghidra: bVar20 block after facing/−10 | quiet_score_fog_explore
 *
 * bVar20 init (Brave type 19): starts true (type != wagon 0x12); may clear
 * later for some Euro paths. NEW WORLD Indian quiet: treat as enabled.
 *
 * Far probe = unit + 4×dir.
 *   +8 if explore-index byte==0 (y>>2)+(x>>2)*18 && !ocean(far) && inset(far)
 *       — not the tribe /5 index; see accessors.c coarse_fog_* 
 *   +4 ship west-bias — skipped (local_34 ship flag; Braves N/A)
 * Neighbor loop around far (8 dirs):
 *   +2 explore-mask clear — ONLY if nation_id < 4 (Europeans); Indians skip
 *   −2 if tile_owner_or_presence(nbr) >= 0
 *   +2f79[terr] if local_6a — NEW WORLD forces local_6a=0; skip
 */
int quiet_score_fog_explore(
  int score,
  int unit_x,
  int unit_y,
  int dir,
  int nation_id,
  int enable_fog /* bVar20 */
) {
  if (!enable_fog || dir < 0 || dir > 7) {
    return score;
  }

  int far_x = unit_x + k_dir8_dx[dir] * 4;
  int far_y = unit_y + k_dir8_dy[dir] * 4;

  if (coarse_fog_unseen(far_x, far_y) && !ocean_or_high_seas(far_x, far_y) &&
      map_tile_in_bounds(far_x, far_y)) {
    score += 8;
  }

  for (int n = 0; n < 8; ++n) {
    int nx = far_x + k_dir8_dx[n];
    int ny = far_y + k_dir8_dy[n];
    if (!map_tile_in_bounds(nx, ny)) {
      continue;
    }
    if (nation_id >= 0 && nation_id < 4) {
      int mask = tile_explore_mask(nx, ny);
      int euro_bit = 0x10 << (nation_id & 3);
      if ((mask & euro_bit) == 0 && !ocean_or_high_seas(nx, ny)) {
        score += 2;
      }
    }
    if (tile_owner_or_presence(nx, ny) >= 0) {
      score -= 2;
    }
  }
  return score;
}

/*
 * Ghidra: LAB_521d_52aa colony / capital pull | quiet_score_colony_pull
 *
 * Combat-capable units near foreign colonies. Brave type 19 has combat
 * strength byte DS:0x5236 == 0 → path rejects early (JMP 5183).
 * When colony_count==0 (NEW WORLD early), this is a documented no-op.
 */
int quiet_score_colony_pull(int score, int colony_count) {
  if (colony_count == 0) {
    return score; /* no-op — DOS condition preserved */
  }
  /* parked mid-game: FUN_291f_0a14 / FUN_281f_08bc weighting */
  return score;
}

/* ====================================================================== */
/* Dir pick                                                               */
/* ====================================================================== */

/*
 * Ghidra: quiet Brave path through FUN_521d_20e6 | quiet_brave_pick_dir_asm
 *
 * Dir loop 0..7 (stay handled by caller for LCG). Reject ocean/HS / foreign.
 * Score = base + terrain; then if LAB_521d_54f5 gate: facing + −10 + fog;
 * else colony_pull (no-op early). Keep max.
 */
int quiet_brave_pick_dir_asm(
  int x,
  int y,
  int nation_id,
  int last_dir,
  int colony_count,
  int enable_fog,
  int mover_type /* Brave 19 */
) {
  int best_dir = VICEROY_DIR_STAY;
  int best_score = -0x3e7; /* ASM init local_e2 = 0xfc19 (−999) */

  for (int d = 0; d < 8; ++d) {
    int nx = x + k_dir8_dx[d];
    int ny = y + k_dir8_dy[d];

    uint8_t terr = (uint8_t)(terrain_byte(nx, ny) & VICEROY_TERRAIN_TYPE_MASK);
    if (terr == VICEROY_TERRAIN_OCEAN || terr == VICEROY_TERRAIN_HIGH_SEAS ||
        terr >= 0x18) {
      continue;
    }
    if (ocean_or_high_seas(nx, ny)) {
      continue;
    }
    int own = owner_nibble(nx, ny);
    if (own >= 0 && own != nation_id) {
      continue; /* foreign → combat path, not quiet */
    }

    int score = quiet_score_base();
    score = quiet_score_terrain(score, x, y, nx, ny, d);
    if (terr == VICEROY_TERRAIN_HIGH_SEAS) {
      score -= 0x10; /* LAB_521d_5070 — usually unreachable after reject */
    }

    if (quiet_lab_54f5_gate(nx, ny, nation_id)) {
      score = quiet_score_facing(score, d, last_dir);
      score = quiet_score_military_minus10(score, nx, ny, nation_id, mover_type);
      score = quiet_score_fog_explore(score, x, y, d, nation_id, enable_fog);
    } else {
      score = quiet_score_colony_pull(score, colony_count);
    }

    if (score > best_score) {
      best_score = score;
      best_dir = d;
    }
  }
  return best_dir;
}
