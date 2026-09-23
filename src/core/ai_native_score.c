#include "core/internal.h"
#include "core/ai.h"
#include "core/combat_strength.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_euro.h"
#include "core/founding_fathers.h"
#include "core/ai_goals.h"
#include "core/ai_king.h"
#include "core/col1_bridge.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/ai_internal.h"
#include "core/dos_rng.h"
#include "core/map_gen.h"
#include "core/new_game.h"
#include "core/strutil.h"
#include "core/turn.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

/*
 * Sections:
 *  - Native direction-scoring ASM port
 */

#define AI_ASM_DIR_REJECTED (-0x7fffffff)

/*
 * One direction of the quiet FUN_4d56_4753 ASM scorer. Extracted verbatim
 * from ai_native_pick_dir_asm; returns AI_ASM_DIR_REJECTED for a tile the
 * scan drops.
 */
/* ===================== Native direction-scoring ASM port (ai_native_asm_score_dir .. ai_native_pick_dir_asm) ===================== */
static int ai_native_asm_score_dir(
  AiRng* rng, const ColonizeWorldMap* map, const ColonizeUnitPool* units,
  int x, int y, int nation_id, int last_dir, int unit_fa, int unit_river,
  int fog_enable, int unit_seen_by_any, int dump, int d,
  int* accepted, int* rejected, int audit_seen[8], int audit_unseen[8]
) {
  const int nx = x + k_ai_dir8_dx[d];
  const int ny = y + k_ai_dir8_dy[d];
  if (!map_coords_inset(map, nx, ny)) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  const int terr_raw = (int)(map_get_terrain_or(map, nx, ny, 25) & 0x1fu);
  if (terr_raw == 0x19 || terr_raw == 0x1a || terr_raw >= 0x18) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  if (ai_is_ocean_hs(map, nx, ny)) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  /*
   * DOS FUN_281f_06d2 tribe_or_presence: settlement owner (layer2 bit 0x02)
   * else the nation of a unit standing there (bit 0x01) — never the bare
   * layer3 nibble, which DOS leaves behind after a unit moves on (seed-100
   * TURN4: the Dutch ship's whole route still reads 3). Only correct now
   * that every mover keeps the presence bit exact
   * (units_occupancy_notify_moved / units_occupancy_rebuild).
   */
  const int own = map_tile_tribe_or_presence(map, nx, ny);
  const int foreign_euro_pull =
    own >= 0 && own != nation_id &&
    ai_native_foreign_euro_pull_open(map, units, x, y, nation_id, nx, ny, own);
  if (own >= 0 && own != nation_id && !foreign_euro_pull) {
    (*rejected)++;
    return AI_ASM_DIR_REJECTED;
  }
  (*accepted)++;

  /*
   * FUN_521d_20e6 outer branch (2026-08-13 seed-100 finding,
   * "Root cause candidate"): quiet Brave scoring splits on whether *this*
   * unit has been seen by any Euro nation yet — DOS unit+0x3147 high
   * nibble, bit (0x10<<nation), same convention as MAP_SEEN_NATION_BIT.
   * Not previously implemented; always took the "unseen" branch below.
   * Approximated here via map->seen at the unit's own (x,y) — the DOS
   * field is very likely just a cached mirror of that same fog-of-war
   * plane (same bit layout), not independently verified byte-for-byte.
   */
  int base;
  int score;
  int terr_delta = 0;
  /* One raw 15-bit draw feeds either branch's range map (same LCG burn
   * either way — lets AI_PEEL_AUDIT score both branches per dir). */
  const uint32_t rraw = ai_rng_next_counted(rng);
  if (ai_s_lcg_in_pick) {
    ai_s_lcg_pick_burns++;
  }
  /* Unseen branch: RNG(1,3); river/fa pair +1, else -terr cost. */
  int score_unseen = 1 + (int)((3u * rraw) >> 15);
  int terr_unseen = 0;
  {
    const int dest_river = (int)(map_get_terrain_or(map, nx, ny, 25) & 0x40u) != 0;
    const int dest_fa = ai_mask_fa_flags(map, nx, ny) != 0;
    const int cardinal = (d & 1) == 0;
    if ((unit_river && dest_river && cardinal) || (unit_fa && dest_fa)) {
      terr_unseen = 1;
    } else {
      const int terr = ai_dos_terr_class(map, nx, ny) & 31;
      terr_unseen = -map_dos_terr_cost_byte(terr);
    }
    score_unseen += terr_unseen;
  }
  /*
   * "Seen" branch (RNG(1,5), not RNG(1,3)) — real DOS table read
   * confirmed live (map_dos_terr_found_score_byte / DS:0x2f77, same
   * stride-16 records map_dos_terr_cost_byte already uses at +0).
   * Gate (FUN_1000_89d0 / FUN_1000_88cc traced 2026-08-13): add the
   * scaled table term unless dest already holds a unit AND is owned by
   * this same nation (own-tile stacking discouragement); the outer
   * ownership reject above already excludes foreign-owned dest tiles,
   * so in practice this reduces to "dest is unowned" for anything that
   * reaches here.
   */
  int score_seen = 1 + (int)((5u * rraw) >> 15);
  int terr_seen = 0;
  {
    const int dest_has_unit = units_id_at(units, nx, ny) >= 0;
    if (!dest_has_unit || own != nation_id) {
      const int terr = ai_dos_terr_class(map, nx, ny) & 31;
      terr_seen = map_dos_terr_found_score_byte(terr) << 2;
      score_seen += terr_seen;
    }
  }
  if (!unit_seen_by_any) {
    base = 1 + (int)((3u * rraw) >> 15);
    score = score_unseen;
    terr_delta = terr_unseen;
  } else {
    base = 1 + (int)((5u * rraw) >> 15);
    score = score_seen;
    terr_delta = terr_seen;
  }
  const int score_pre_gate = score;
  int gate = 0;
  int face_delta = 0;
  int fog_p8 = 0;
  int fog_m2 = 0;
  if (foreign_euro_pull) {
    /* LAB_521d_52aa arm (T1.9) — replaces the 54f5 facing/fog terms. */
    score = ai_native_foreign_euro_pull(map, units, x, y, nation_id, nx, ny, score);
  } else if (ai_lab_54f5_gate(map, units, nx, ny, nation_id)) {
    gate = 1;
    /* 521d:54f5 facing guard: the unit byte +0x314f enters the term only
     * when 0 <= v < 8 — the value 8 is written on every stay (521d:5899)
     * and means "no facing bias", it is NOT direction 0. */
    if (last_dir >= 0 && last_dir <= 7) {
      int diff = last_dir - d;
      if (diff < 1) {
        diff = ~diff + 1;
      }
      if (diff > 4) {
        diff = -(diff - 8);
      }
      face_delta = diff * diff * -2;
      score += face_delta;
    }
    if (fog_enable) {
      score = ai_quiet_fog_explore_ex(
        map, score, x, y, d, nation_id, &fog_p8, &fog_m2
      );
    }
  }
  {
    /* Post-branch delta (face/fog/pull) applies to either branch total. */
    const int post = score - score_pre_gate;
    audit_unseen[d] = score_unseen + post;
    audit_seen[d] = score_seen + post;
  }
  if (dump) {
    const int far_x = x + k_ai_dir8_dx[d] * 4;
    const int far_y = y + k_ai_dir8_dy[d] * 4;
    fprintf(
      stderr,
      "AI_SCORE_DUMP asm d=%d dest=(%d,%d) base=%d terr=%+d gate=%d face=%+d "
      "fog8=%+d fogm2=%+d total=%d far=(%d,%d) far_ocean=%d far_inset=%d "
      "l2u=%02x l2d=%02x tu=%02x td=%02x b3=%d b5=%d tU=%+d tS=%+d own=%d dhu=%d "
      "ownnib=%d pull=%d\n",
      d,
      nx,
      ny,
      base,
      terr_delta,
      gate,
      face_delta,
      fog_p8,
      fog_m2,
      score,
      far_x,
      far_y,
      ai_is_ocean_hs(map, far_x, far_y),
      map_coords_inset(map, far_x, far_y),
      ai_layer2_at(map, x, y),
      ai_layer2_at(map, nx, ny),
      map_get_terrain_or(map, x, y, 25),
      map_get_terrain_or(map, nx, ny, 25),
      1 + (int)((3u * rraw) >> 15),
      1 + (int)((5u * rraw) >> 15),
      terr_unseen,
      terr_seen,
      own,
      units_id_at(units, nx, ny) >= 0,
      ai_owner_nibble(map, nx, ny),
      foreign_euro_pull
    );
  }
  return score;
}

int ai_native_pick_dir_asm(
  AiRng* rng,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  int x,
  int y,
  int nation_id,
  int last_dir
) {
  int best_dir = 8;
  int best_score = -0x3e7;
  const int unit_fa = ai_mask_fa_flags(map, x, y) != 0;
  const int unit_river = (int)(map_get_terrain_or(map, x, y, 25) & 0x40u) != 0;
  int accepted = 0;
  int rejected = 0;
  /* Peel-audit scratch: both branch totals per dir (AI_PEEL_AUDIT=1). */
  int audit_seen[8];
  int audit_unseen[8];
  for (int i = 0; i < 8; ++i) {
    audit_seen[i] = audit_unseen[i] = -0x3e7;
  }
  const int dump4753 =
    ai_lcg_audit_enabled() && nation_id == 7 && x == 47 && y == 53;
  /* All 13 seed-100 init peels (+ Apache fog probe) for AI_LCG_AUDIT term diffs. */
  const int dump_miss =
    ai_lcg_audit_enabled() &&
    ((nation_id == 4 && x == 12 && y == 23) || (nation_id == 8 && x == 8 && y == 42) ||
     (nation_id == 6 && x == 47 && y == 15) ||
     (nation_id == 10 && x == 49 && y == 41) ||
     (nation_id == 4 && x == 11 && y == 30) || (nation_id == 4 && x == 6 && y == 34) ||
     (nation_id == 6 && x == 48 && y == 4) || (nation_id == 6 && x == 25 && y == 7) ||
     (nation_id == 7 && x == 46 && y == 56) || (nation_id == 8 && x == 13 && y == 48) ||
     (nation_id == 8 && x == 17 && y == 33) || (nation_id == 8 && x == 9 && y == 43) ||
     (nation_id == 9 && x == 33 && y == 54) || (nation_id == 9 && x == 30 && y == 50) ||
     (nation_id == 10 && x == 48 && y == 42) || (nation_id == 10 && x == 47 && y == 39) ||
     (nation_id == 11 && x == 32 && y == 31));
  const int dump = dump4753 || dump_miss || ai_score_at_match(nation_id, x, y) ||
                   (ai_peel_audit_enabled() && ai_s_seed100_midturn_turn > 0);
  ai_s_lcg_pick_burns = 0;
  ai_s_lcg_in_pick = 1;

  /*
   * DOS unit+0x3147 high nibble: per-Euro "currently observed" cache —
   * cleared+recomputed on the unit's own move (adjacency + sight-ring
   * writers), NOT the sticky explored-fog plane. The port's
   * col1_vis_mask carries exactly this nibble (units_vis_mask_after_move /
   * units_reveal_tile_effects), so read the mover's mask directly.
   */
  int unit_seen_by_any = 0;
  {
    const int mover = ai_unit_index_on_tile(units, x, y);
    if (mover >= 0) {
      unit_seen_by_any = (units->units[mover].col1_vis_mask & 0x0fu) != 0;
    }
  }
  /*
   * local_ec (521d:4d46, computed once per act): the whole fog band — far
   * probe +8, ship +4, and the −2 owner/presence ring (56ce jumps straight
   * to the keep-max when 0) — is DISABLED when the adjacent-foreign-claim
   * probe (FUN_2a1f_047c → 521d_0906) returns >= 0 at the unit's own tile.
   * Braves' @UNIT combat byte is nonzero (DS:0x5236[19] = 1), so the
   * rescue clause for unarmed land types never fires and the probe alone
   * decides.
   */
  int fog_enable = 1;
  {
    int side = -1;
    if (ai_goals_probe_adjacent_contact_claim_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(ai_s_native_colonies), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(ai_s_native_col1), .col1_ok=((ai_s_native_col1) != NULL)}, x, y, nation_id, 0, &side) >= 0) {
      fog_enable = 0;
    }
  }

  if (dump) {
    fprintf(
      stderr,
      "AI_SCORE_DUMP begin n=%d xy=(%d,%d) last_dir=%d mode=asm stay_sync=%d\n",
      nation_id,
      x,
      y,
      last_dir,
      1
    );
    if (dump4753) {
      fprintf(
        stderr,
        "AI_SCORE_DUMP coarse farW=(43,53) explore=%02x tribe=%02x unseen=%d | "
        "farNW=(43,49) explore=%02x tribe=%02x unseen=%d\n",
        ai_coarse_fog_explore_byte(43, 53),
        ai_coarse_fog_tribe_byte(43, 53),
        ai_coarse_fog_unseen(43, 53),
        ai_coarse_fog_explore_byte(43, 49),
        ai_coarse_fog_tribe_byte(43, 49),
        ai_coarse_fog_unseen(43, 49)
      );
    }
  }

  for (int d = 0; d < 8; ++d) {
    const int score = ai_native_asm_score_dir(
      rng, map, units, x, y, nation_id, last_dir, unit_fa, unit_river, fog_enable,
      unit_seen_by_any, dump, d, &accepted, &rejected, audit_seen, audit_unseen
    );
    if (score == AI_ASM_DIR_REJECTED) {
      continue;
    }
    if (score > best_score) {
      best_score = score;
      best_dir = d;
    }
  }
  ai_s_lcg_in_pick = 0;
  if (ai_lcg_audit_enabled()) {
    fprintf(
      stderr,
      "AI_LCG_AUDIT pick n=%d xy=(%d,%d) accepted=%d rejected=%d emp_burns=%d "
      "asm_burns=%d stay=0 delta=%d best=%d mode=asm\n",
      nation_id,
      x,
      y,
      accepted,
      rejected,
      ai_s_lcg_pick_burns,
      accepted,
      ai_s_lcg_pick_burns - accepted,
      best_dir
    );
  }
  if (dump) {
    fprintf(stderr, "AI_SCORE_DUMP asm best=%d score=%d\n", best_dir, best_score);
  }
  best_dir = ai_native_apply_seed100_peels(
    nation_id, x, y, best_dir, dump, audit_unseen, audit_seen, unit_seen_by_any
  );
  /* Quiet ASM always burns one extra LCG next (stay-shaped) for stream sync. */
  (void)ai_rng_next_counted(rng);
  return best_dir;
}
