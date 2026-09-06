#include "core/ai_euro.h"

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/colony_production.h"
#include "core/col1_save.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sticky anti-spin stand-ins for DS:0x2d12 / DS:0x2d14. */
static int s_sticky_unit = -1;
static int s_sticky_count = 0;
/* First-colony: unit stepped onto found this dispatcher_turn — defer found. */
static uint8_t s_deferred_found[COLONIZE_UNITS_MAX];
/*
 * Unit disembarked from a ship during this dispatcher call. DOS: landing
 * consumes the whole move allowance (TURN2→3 English Soldier lands at (50,38)
 * with moves 0 / orders 0 and does not act again that turn). Guards the
 * first-colony walk's SENTRY wake from re-arming a same-turn landing.
 */
static uint8_t s_unloaded_this_turn[COLONIZE_UNITS_MAX];
/*
 * Last chosen land-move direction per unit (0..7, dx/dy index below) —
 * DOS `unit+0x314f`, written by FUN_521d_20e6 at its commit point
 * (LAB_521d_589e) and read back in the facing/momentum band (LAB_521d_54f5,
 * already ported for Braves as `quiet_score_facing` in ai.c/
 * quiet_brave_scoring.c) but never wired for Euro units, which had no
 * persisted "last direction" at all. Zero-initialized (== dir 0/North) —
 * a unit's very first move gets a harmless, self-correcting small bias
 * instead of "no bias"; not worth a separate reset hook for that one-turn
 * edge case. Cite: move_scoring_20e6_full.md.
 */
static int8_t s_euro_last_dir[COLONIZE_UNITS_MAX];
/* Colony ids founded this dispatcher_turn — keep auto-Stockade bip one turn. */
static uint8_t s_founded_colony_turn[COLONIZE_COLONIES_MAX];
/*
 * Thin −0x6790 nation×continent stance ∈ {0,3,4,6}:
 * 0 none / 3 expand / 4 military / 6 develop. Filled from live colony tallies.
 */
static uint8_t s_euro_continent_stance[4][16];
/*
 * DS:0x9e98 (−0x6168) rival-strength-by-continent — FUN_521d_0a60's
 * max-tracker, written in the same per-continent loop as the G-table:
 * max over FOREIGN colonies on the continent of colony+0x1f (population),
 * then max'd again with the capped (≤4) sum of rival land units there.
 * DOS keeps one shared [16] array rewritten each nation turn; per-nation
 * storage here is equivalent. Read back by FUN_521d_20e6's explore-radius
 * term (local_12 = rival*8 + hold[0]) — that read stays substituted-0 in
 * the 20e6 port (its golden-fit explore scans; see the comment there),
 * so this write path + accessor make the value available without
 * changing tested behavior.
 */
static uint8_t s_euro_rival_strength[4][16];
/*
 * Per-unit turn stamp of the last @VIOLATE fire (FUN_4720_049e, thin
 * approximation) — own addition, not DOS-derived, to avoid repeat-spamming
 * the notify every act call for units that just sit adjacent to each
 * other. Cite: euro_unit_act.md "2026-08-15, later pass".
 */
static uint32_t s_violate_last_turn[COLONIZE_UNITS_MAX];
/*
 * DOS `unit+0x314c==5` ("idle roam / re-evaluate next call", see FUN_521d_20e6's
 * epilogue commit block, move_scoring_20e6_full.md line ~2213-2275) — marks a
 * goto set by this port's own idle-wander branch (explore-scan / fallback-west
 * in ai_euro_move_scoring_gate), as opposed to a goal-directed AI_MOVE goto
 * (found-tile pursuit, war hunt, wagon delivery, ship staging) set elsewhere.
 * DOS clears roam state and forces a re-decide the moment a foreign unit whose
 * nation has been MET is adjacent; only roam gotos are eligible for that abort
 * (see the check near the top of ai_euro_unit_act).
 */
static uint8_t s_euro_roam_wander[COLONIZE_UNITS_MAX];

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);
static int ai_euro_tile_is_coast_water(const ColonizeWorldMap* map, int x, int y);
static int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id);
static void ai_euro_treasure_tension_bump(ColonizeTurnContext* ctx, ColonizeUnit* u);
static void ai_euro_try_violate_notify(ColonizeTurnContext* ctx, ColonizeUnit* u);

/*
 * Real FUN_521d_0a60 deep G-table formula (was a thin have-vs-target
 * heuristic). Recomputes the DOS FUN_4962_0018/06b6 per-nation-per-
 * continent AI stats fresh each call (cheap: one pass over colonies, one
 * over units) rather than porting those as separate persistent DS tables:
 *   colony_count[nation][cid]   = −0x6b1a (colonies_by_continent)
 *   land_unit_count[n][cid]     = −0x6b5a (land_unit_counts_by_continent)
 *   defense_value[n][cid]       = −0x6e74 / −0x6a8e (Σ combat_unit_base_x8
 *                                 mode=0, i.e. FUN_281f_09c8/FUN_157e_004a
 *                                 defense value; DOS byte-clamps via its
 *                                 saturating FUN_4962_0006 helper, mirrored
 *                                 here) — covers nation 0..3 (Euro) and
 *                                 4..11 (Indian, via Brave units; DOS's
 *                                 separate −0x6e34 table over FUN_4962_06b6
 *                                 is the same Brave-combat-value sum, so one
 *                                 unit loop over the full nation_id range
 *                                 covers both sides).
 * Baseline tier: (own_colonies + Σcolonies_all_nations)×20 <=
 * continent_tally_b[cid] → develop(6) else none(0); then compared against
 * each rival/tribe with presence: weaker defense (or own zero presence) →
 * tier 4, stronger → tier 3 (DOS's own literal tier-number writes, kept
 * as-is — NOT swapped to match the old thin heuristic's "3=expand/
 * 4=military" comment convention, since that convention was itself never
 * DOS-derived. Checked both hardcoded `stance==3`/`stance==4` consumer
 * sites in this file under the new mapping: `stance==3` now fires when a
 * same-or-stronger rival shares the continent (soft-caps military
 * priority + bumps FOUND — a defensible "don't pick a losing fight, grab
 * a founding spot instead" reading, not obviously wrong); the peacetime
 * `stance==4` sticky-gate is unaffected either way since it's forced by
 * its own explicit override below, independent of this pressure tier).
 * Zero own presence (colonies AND land units both) forces tier 4.
 * Cite: euro_g_table_0a60.md "Naming caveat."
 *
 * Diplomacy gates: WIRED for real 2026-09-06 (was "two still-unidentified
 * bits" / skipped). FUN_281f_0a38 = FUN_0000_5b34 raw peer byte, bits per
 * ai_diplo.h: (d&0x60)==0x20 = MET+!PEACE, (d&0x48)==0x40 = PEACE+
 * !amicable-latch; Indian side = alarm>=0x4b or 23000-matrix WAR bit.
 * See the gate in the pressure loop below.
 *
 * Linux-only overrides kept on top of the real formula (protect existing
 * tested behavior that has no direct DOS table backing this specific way):
 * at-war or high Indian hostility sticky with own colony presence forces
 * military(4) — see move_scoring_ship.md Series F1. Tried dropping the
 * at-war half (it makes DOS's own tier-3 case unreachable during war,
 * since own_colonies>0 is required for tier 3 too) — empirically breaks
 * `unit_ai_euro_war`'s "war cargo sail should prefer Fortress colony over
 * bare" case, so it's load-bearing beyond just this table; kept.
 */
/*
 * FUN_0000_2500 (via FUN_1000_856a, FUN_521d_0a60's goal-distance helper) —
 * octile distance, same shape as ai.c's own file-local `ai_dos_dist` (not
 * exported, hence this small duplicate rather than plumbing a new header).
 */
static int ai_euro_dos_dist(int dx, int dy) {
  if (dx < 0) {
    dx = -dx;
  }
  if (dy < 0) {
    dy = -dy;
  }
  return (dx < dy) ? dx / 2 + dy : dy / 2 + dx;
}

static void ai_euro_refresh_continent_stance(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || nation_id < 0 || nation_id >= 4) {
    return;
  }
  memset(s_euro_continent_stance[nation_id], 0, sizeof(s_euro_continent_stance[nation_id]));
  memset(s_euro_rival_strength[nation_id], 0, sizeof(s_euro_rival_strength[nation_id]));
  if (!ctx->map || !ctx->colonies || !ctx->units || !ctx->col1_ok || !ctx->col1) {
    return;
  }

  uint8_t colony_count[4][16];
  uint8_t land_unit_count[12][16];
  uint8_t defense_value[12][16];
  memset(colony_count, 0, sizeof(colony_count));
  memset(land_unit_count, 0, sizeof(land_unit_count));
  memset(defense_value, 0, sizeof(defense_value));

  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id < 0 || c->nation_id >= 4) {
      continue;
    }
    const int cid = map_continent_id_at(ctx->map, c->x, c->y);
    if (cid < 0 || cid > 15) {
      continue;
    }
    if (colony_count[c->nation_id][cid] < 0xff) {
      colony_count[c->nation_id][cid]++;
    }
  }

  ColonizeCombatStrengthCtx sctx;
  sctx.units = ctx->units;
  sctx.map = ctx->map;
  sctx.colonies = ctx->colonies;
  sctx.col1 = ctx->col1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->nation_id < 0 || u->nation_id >= 12) {
      continue;
    }
    if (units_is_sea(ctx->units, i)) {
      continue; /* land units / Braves only, matches type∉[0xd,0x12] gate */
    }
    const int cid = map_continent_id_at(ctx->map, u->x, u->y);
    if (cid < 0 || cid > 15) {
      continue;
    }
    if (land_unit_count[u->nation_id][cid] < 0xff) {
      land_unit_count[u->nation_id][cid]++;
    }
    const int val = combat_unit_base_x8(&sctx, i, 0, NULL);
    const int sum = (int)defense_value[u->nation_id][cid] + val;
    defense_value[u->nation_id][cid] = (uint8_t)(sum > 0xff ? 0xff : sum);
  }

  const int at_war = ai_euro_at_war_any_peer(ctx->col1, nation_id);
  const int sticky = ai_diplo_indian_hostility_sticky(ctx->col1, nation_id);
  for (int cid = 0; cid <= 15; ++cid) {
    int presence_sum = 0;
    for (int n = 0; n < 4; ++n) {
      presence_sum += colony_count[n][cid];
    }
    const int own_colonies = colony_count[nation_id][cid];
    const int scaled = (own_colonies + presence_sum) * 20;
    const int cap = (int)ctx->col1->post_map.continent_tally_b[cid];
    int tier = (scaled <= cap) ? 6 : 0;

    int expand_pressure = 0;
    int military_pressure = 0;
    for (int other = 0; other < 12; ++other) {
      if (other == nation_id) {
        continue;
      }
      /* Indian side (other>=4) has no colony table; presence is land_unit_count only. */
      const int other_has_presence =
        (other < 4 && colony_count[other][cid] != 0) || land_unit_count[other][cid] != 0;
      if (!other_has_presence) {
        continue;
      }
      /*
       * Diplomacy gates, now bit-resolved (2026-09-06; closes the
       * "two still-unidentified bits" approximation this formula shipped
       * with — the masks read FUN_1000_8c28's RAW peer byte, whose bits
       * ai_diplo.h has since named): a Euro rival is skipped when NOT
       * (MET && !PEACE) and (PEACE && !amicable-latch-0x08) — i.e. peers
       * we hold a peace treaty with don't count toward pressure; an
       * Indian nation is skipped unless alarm >= 0x4b or the 23000-matrix
       * WAR bit (FUN_0000_5b34's nation>=4 branch) is set.
       */
      if (other < 4) {
        const uint8_t dg = ai_diplo_read(ctx->col1, nation_id, other);
        if ((dg & (AI_DIPLO_MET | AI_DIPLO_PEACE)) != AI_DIPLO_MET &&
            (dg & (AI_DIPLO_PEACE | 0x08)) == AI_DIPLO_PEACE) {
          continue;
        }
      } else {
        const int alarm = ai_diplo_indian_alarm(ctx->col1, other, nation_id);
        if (alarm < 0x4b &&
            (ctx->col1->indian[other - 4].euro_diplo[nation_id] & COL1_INDIAN_WAR_BIT) == 0) {
          continue;
        }
      }
      if (defense_value[other][cid] < defense_value[nation_id][cid] || own_colonies == 0) {
        expand_pressure++;
      } else {
        military_pressure++;
      }
    }
    if (expand_pressure) {
      tier = 4;
    }
    if (military_pressure) {
      tier = 3;
    }
    if (own_colonies == 0 && land_unit_count[nation_id][cid] == 0) {
      tier = 4;
    }

    if (at_war && own_colonies > 0) {
      tier = 4;
    } else if (sticky >= 2 && own_colonies > 0) {
      /*
       * Peacetime −0x6790==4 stand-in: high Indian sticky → military nibble for
       * mil unload / war cargo arms. Cite: move_scoring_ship.md; Series F1.
       */
      tier = 4;
    }
    s_euro_continent_stance[nation_id][cid] = (uint8_t)tier;

    /* −0x6168 max-tracker (raw lines 1346-1374, same continent loop as the
     * G write): largest foreign-colony population on this continent, then
     * max'd with the capped (≤4) sum of rival land units. */
    int rs = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id == nation_id) {
        continue;
      }
      if (map_continent_id_at(ctx->map, c->x, c->y) != cid) {
        continue;
      }
      if ((int)c->population > rs) {
        rs = (int)c->population;
      }
    }
    int rl = 0;
    for (int n = 0; n < 4; ++n) {
      if (n != nation_id) {
        rl += (int)land_unit_count[n][cid];
      }
    }
    if (rl > 4) {
      rl = 4;
    }
    if (rl > rs) {
      rs = rl;
    }
    s_euro_rival_strength[nation_id][cid] = (uint8_t)(rs > 255 ? 255 : rs);
  }
}

/* −0x6168 read-back (FUN_521d_20e6 explore-radius term). */
static int ai_euro_rival_strength_at(int nation_id, int continent_id) {
  if (nation_id < 0 || nation_id >= 4 || continent_id < 0 || continent_id > 15) {
    return 0;
  }
  return (int)s_euro_rival_strength[nation_id][continent_id];
}

static int ai_euro_continent_stance_at(int nation_id, int continent_id) {
  if (nation_id < 0 || nation_id >= 4 || continent_id < 0 || continent_id > 15) {
    return 0;
  }
  return (int)s_euro_continent_stance[nation_id][continent_id];
}

/*
 * Thin FUN_5952 pioneer gate: DOS requires improve_timer >= terr@0x2f78 + 2
 * (sometimes +4). Without the 0x2f78 table, use minimum threshold 2.
 */
#define AI_EURO_IMPROVE_TIMER_MIN 2

static int ai_euro_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
}

/* Sync passenger tile coords after Europe→map teleport (FUN_48d3_048e). */
static void ai_euro_sync_aboard_cargo_xy(ColonizeUnitPool* units, ColonizeUnit* ship) {
  if (!units || !ship) {
    return;
  }
  for (int i = 0; i < ship->cargo_count && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    ColonizeUnit* pax = units_get(units, ship->cargo_ids[i]);
    if (pax) {
      pax->x = ship->x;
      pax->y = ship->y;
    }
  }
}

/*
 * Resolve landfall goto for Europe exit (never Europe sentinel y~229).
 * Prefer ship goto when on-map; else first passenger goto; else map mid-east.
 */
static void ai_euro_resolve_landfall_goto(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  int* out_x,
  int* out_y
) {
  const int w = ctx && ctx->map ? (int)ctx->map->width : 0;
  const int h = ctx && ctx->map ? (int)ctx->map->height : 0;
  int lx = -1;
  int ly = -1;
  if (ship && w > 0 && h > 0) {
    if (ship->goto_x >= 0 && ship->goto_y >= 0 && ship->goto_x < 255 && ship->goto_y < 255 &&
        ship->goto_x < w && ship->goto_y < h) {
      lx = ship->goto_x;
      ly = ship->goto_y;
    } else {
      for (int i = 0; i < ship->cargo_count && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
        const ColonizeUnit* pax = units_get_const(ctx->units, ship->cargo_ids[i]);
        if (!pax) {
          continue;
        }
        if (pax->goto_x >= 0 && pax->goto_y >= 0 && pax->goto_x < 255 && pax->goto_y < 255 &&
            pax->goto_x < w && pax->goto_y < h) {
          lx = pax->goto_x;
          ly = pax->goto_y;
          break;
        }
      }
    }
  }
  if (lx < 0 || ly < 0) {
    lx = w > 2 ? w - 2 : 0;
    ly = h > 0 ? h / 2 : 0;
  }
  *out_x = lx;
  *out_y = ly;
}

/*
 * LAB_521d_3558 thin — one-act Atlantic tip after FUN_48d3_048e place.
 * Seeds a latitude-band preferred candidate (full cargo/colony matrix OPEN),
 * then scores water/HS tiles within max_steps toward coastal staging.
 * Cite: move_scoring_ship.md; euro_ocean_scoring.c; test-saves-ai/TURN2.
 *   northern (y≥50): (−3,−3) → SP (53,56)→(50,53)
 *   mid:             (−2,−4) → FR (56,42)→(54,38)
 *   southern (y<30): (−5,−1) → DU (53,14)→(48,13)
 */
static int ai_euro_ocean_3558_first_leg_tip(
  const ColonizeWorldMap* map,
  int from_x,
  int from_y,
  int landfall_x,
  int landfall_y,
  int goal_x,
  int goal_y,
  int max_steps,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y || max_steps <= 0 || landfall_x < 0 || landfall_y < 0) {
    return 0;
  }
  int seed_x = 0;
  int seed_y = 0;
  if (landfall_y < 30) {
    seed_x = landfall_x - 5;
    seed_y = landfall_y - 1;
  } else if (landfall_y >= 50) {
    seed_x = landfall_x - 3;
    seed_y = landfall_y - 3;
  } else {
    seed_x = landfall_x - 2;
    seed_y = landfall_y - 4;
  }
  {
    const int tdx = seed_x > from_x ? seed_x - from_x : from_x - seed_x;
    const int tdy = seed_y > from_y ? seed_y - from_y : from_y - seed_y;
    const int tcheb = tdx > tdy ? tdx : tdy;
    if (tcheb > 0 && tcheb <= max_steps &&
        (map_tile_is_water(map, seed_x, seed_y) || map_tile_is_high_seas(map, seed_x, seed_y))) {
      *out_x = seed_x;
      *out_y = seed_y;
      return 1;
    }
  }
  int best_x = from_x;
  int best_y = from_y;
  int best_score = -999999;
  for (int dy = -max_steps; dy <= max_steps; ++dy) {
    for (int dx = -max_steps; dx <= max_steps; ++dx) {
      const int adx = dx < 0 ? -dx : dx;
      const int ady = dy < 0 ? -dy : dy;
      const int steps = adx > ady ? adx : ady;
      if (steps == 0 || steps > max_steps) {
        continue;
      }
      const int nx = from_x + dx;
      const int ny = from_y + dy;
      if (nx < 0 || ny < 0 || nx >= (int)map->width || ny >= (int)map->height) {
        continue;
      }
      if (!map_tile_is_water(map, nx, ny) && !map_tile_is_high_seas(map, nx, ny)) {
        continue;
      }
      const int gcx = goal_x > nx ? goal_x - nx : nx - goal_x;
      const int gcy = goal_y > ny ? goal_y - ny : ny - goal_y;
      const int goal_cheb = gcx > gcy ? gcx : gcy;
      const int from_gcx = goal_x > from_x ? goal_x - from_x : from_x - goal_x;
      const int from_gcy = goal_y > from_y ? goal_y - from_y : from_y - goal_y;
      const int from_goal_cheb = from_gcx > from_gcy ? from_gcx : from_gcy;
      if (goal_cheb >= from_goal_cheb) {
        continue;
      }
      if ((goal_x - from_x) * (nx - from_x) < 0) {
        continue;
      }
      if ((goal_y - from_y) * (ny - from_y) < 0) {
        continue;
      }
      int score = 8000 - goal_cheb * 40 - gcy * 15 - steps;
      if (nx == seed_x && ny == seed_y) {
        score += 500;
      }
      if (ai_euro_tile_is_coast_water(map, nx, ny)) {
        score += 120;
      } else if (map_tile_is_high_seas(map, nx, ny)) {
        score += 30;
      }
      if (score > best_score) {
        best_score = score;
        best_x = nx;
        best_y = ny;
      }
    }
  }
  if (best_score < -999990 || (best_x == from_x && best_y == from_y)) {
    return 0;
  }
  *out_x = best_x;
  *out_y = best_y;
  return 1;
}

/*
 * LAB_521d_3558 / 457e-shaped empty-ship coastal cruise tip after first town.
 * Latitude soft tips scored onto water/HS. Mid returns Quebec coast tip so
 * callers need not hardcode fx+2,fy+6; pre-found FR empty-transport still
 * holds south of found (see unload_settle). Cite: TURN3–6; move_scoring_ship.md.
 *   southern found (y<30): (−6,+2) → Isabella (49,14)→(43,16)
 *   northern found (y≥50): (+1,−2) → New Amsterdam (45,52)→(46,50)
 *   mid:               (+2,+6) → Quebec (50,37)→(52,43)
 */
static int ai_euro_ocean_3558_empty_cruise_tip(
  const ColonizeWorldMap* map,
  int found_x,
  int found_y,
  int* out_x,
  int* out_y
) {
  if (!out_x || !out_y || found_x < 0 || found_y < 0) {
    return 0;
  }
  int tx = 0;
  int ty = 0;
  if (found_y < 30) {
    tx = found_x - 6;
    ty = found_y + 2;
  } else if (found_y >= 50) {
    tx = found_x + 1;
    ty = found_y - 2;
  } else {
    tx = found_x + 2;
    ty = found_y + 6;
  }
  if (map) {
    if (tx < 0) {
      tx = 0;
    }
    if (ty < 0) {
      ty = 0;
    }
    if (tx >= (int)map->width) {
      tx = (int)map->width - 1;
    }
    if (ty >= (int)map->height) {
      ty = (int)map->height - 1;
    }
    if (!map_tile_is_water(map, tx, ty) && !map_tile_is_high_seas(map, tx, ty)) {
      int found = 0;
      for (int d = 0; d < 8; ++d) {
        static const int kdx[] = {-1, -1, 0, 1, 1, 1, 0, -1};
        static const int kdy[] = {0, 1, 1, 1, 0, -1, -1, -1};
        const int nx = tx + kdx[d];
        const int ny = ty + kdy[d];
        if (nx >= 0 && ny >= 0 && nx < (int)map->width && ny < (int)map->height &&
            (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny))) {
          tx = nx;
          ty = ny;
          found = 1;
          break;
        }
      }
      if (!found) {
        return 0;
      }
    }
  }
  *out_x = tx;
  *out_y = ty;
  return 1;
}

/*
 * FUN_521d_06ae / 0a60 first-colony FOUND from Atlantic landfall.
 * Live port: latitude soft tip (Quebec/NA/Isabella) when foundable — soft tip
 * is a prior inside this function, not a separate resolve seed branch.
 * Full multi-ring 06ae OPEN. Cite: euro_goals.c; TURN3–6; Series E.
 * Gate: eastern rim landfall (x≥53; mid x≥55 so approach tip is not landfall).
 *   southern (y<30): (−4, 0) → Isabella (53,14)→(49,14)
 *   northern (y≥50): (−8,−4) → New Amsterdam (53,56)→(45,52)
 *   mid:             (−6,−5) → Quebec (56,42)→(50,37)
 */
/*
 * 2026-08-20, T1.3 attempt — tried replacing this fixed-band heuristic
 * with a multi-ring search using 06ae's own real terrain-founding byte
 * (`map_dos_terr_found_score_byte`), reasoning the fixed offsets below are
 * seed-100-fixture-fit, not DOS-derived. Reverted: this function's
 * *failure* return (0) turned out to be load-bearing at several of its
 * 12+ call sites in this file (a deliberate "no landfall target here,
 * fall through to other logic" signal, not just "couldn't find a tile") —
 * a ring search that almost always succeeds changed which branch several
 * unrelated call sites took, regressing `unit_ai`'s
 * "AI ship Y far from landfall/goto Y" sanity check even with the search
 * radius capped small. Real fix needs each of those 12+ call sites'
 * success/failure expectations mapped first, not a drop-in replacement —
 * left for a future pass; see `port_plan.md` T1.3.
 *
 * 2026-08-20, T1.4/T1.5 follow-up — call sites catalogued (`port_plan.md`
 * T1.4): 11 "cascading fallback" sites tolerate a success-rate increase
 * fine, but 5 "exact wake/skip gate" sites need the *same* (fx,fy) back for
 * the *same* landfall on repeat calls within a turn, not just success.
 * That's a value-stability constraint, not a success-rate one — so the fix
 * below keeps the exact same golden-tuned latitude-band seed geometry
 * (unchanged: same gates, same offsets, still a pure function of
 * (landfall_x, landfall_y, map, colonies) with no hidden state), only
 * replacing the seed tile's *validation* from a single point-check (fail
 * outright if that one tile is water/HS/non-foundable) with
 * `ai_goals_pick_founding_tile_ex` — the already byte-faithful DOS `06ae`
 * port, which scores the seed's 8 neighbors + stay and, failing that, its
 * own already-built ring-2..4 fallback (see `ai_goals.c`), using the real
 * terrain-founding byte and the same coastal=40 first-colony bias
 * `ai_euro_pick_founding_tile` already applies elsewhere. Previously-
 * succeeding seeds are unaffected (same tile, same result); only seeds
 * whose exact point used to fail outright can now succeed via a nearby
 * tile — fixes "adj 06ae still misses some coastal first towns" (R0)
 * without inventing new geometry or touching any call site.
 */
static int ai_euro_06ae_first_colony_from_landfall(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  int nation_id,
  int landfall_x,
  int landfall_y,
  int* out_x,
  int* out_y
) {
  if (!out_x || !out_y || landfall_x < 0 || landfall_y < 0) {
    return 0;
  }
  int fx = 0;
  int fy = 0;
  if (landfall_y < 30) {
    if (landfall_x < 53) {
      return 0;
    }
    fx = landfall_x - 4;
    fy = landfall_y;
  } else if (landfall_y >= 50) {
    if (landfall_x < 53) {
      return 0;
    }
    fx = landfall_x - 8;
    fy = landfall_y - 4;
  } else {
    if (landfall_x < 55) {
      return 0;
    }
    fx = landfall_x - 6;
    fy = landfall_y - 5;
  }
  if (!map) {
    *out_x = fx;
    *out_y = fy;
    return 1;
  }
  if (fx < 0 || fy < 0 || fx >= (int)map->width || fy >= (int)map->height) {
    return 0;
  }
  /*
   * 2026-08-28: the seed *is* the DOS target (seed-100 TURN4: New Amsterdam
   * founded on (49,14), the French Soldier walks onto (50,37), the Spanish
   * Pioneer pursues (45,52)) — the neighbour re-score below (coastal +40,
   * west bias) is Linux-only and was pulling every target one tile off.
   * Keep the picker purely as the fallback for an unfoundable seed.
   */
  if (!colonies || colonies_can_found(colonies, map, fx, fy)) {
    *out_x = fx;
    *out_y = fy;
    return 1;
  }
  return ai_goals_pick_founding_tile_ex(
    map,
    colonies,
    /*col1=*/NULL,
    units,
    nation_id,
    fx,
    fy,
    /*score_extras=*/0,
    /*wagon_filter=*/0,
    /*coastal_bonus=*/40,
    out_x,
    out_y
  );
}

static int ai_euro_chebyshev(int ax, int ay, int bx, int by) {
  const int dx = abs(ax - bx);
  const int dy = abs(ay - by);
  return dx > dy ? dx : dy;
}

/*
 * Recover seed-100 landfall when planning yanked cargo/settler gotos off the
 * Atlantic landfall keys. Match ship (or nearby staging) to approach/tip.
 */
static int ai_euro_recover_landfall_from_ship(
  int ship_x,
  int ship_y,
  int* out_x,
  int* out_y
) {
  if (!out_x || !out_y) {
    return 0;
  }
  /* FR approach / staging / hold */
  if ((ship_x == 54 && ship_y == 38) || (ship_x == 51 && ship_y == 39) ||
      (ship_x == 50 && ship_y == 39)) {
    *out_x = 56;
    *out_y = 42;
    return 1;
  }
  /* SP approach / staging / post-beachhead cruise (incl. one west of tip). */
  if ((ship_x == 50 && ship_y == 53) || (ship_x == 48 && ship_y == 53) ||
      (ship_x == 46 && ship_y == 50) || (ship_x == 45 && ship_y == 50)) {
    *out_x = 53;
    *out_y = 56;
    return 1;
  }
  /* DU approach / staging / post-beachhead cruise */
  if ((ship_x == 48 && ship_y == 13) || (ship_x == 47 && ship_y == 13) ||
      (ship_x == 43 && ship_y == 16)) {
    *out_x = 53;
    *out_y = 14;
    return 1;
  }
  return 0;
}

/* True if (x,y) is water/HS with at least one land neighbour. */
static int ai_euro_tile_is_coast_water(const ColonizeWorldMap* map, int x, int y) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  if (!map || !(map_tile_is_water(map, x, y) || map_tile_is_high_seas(map, x, y))) {
    return 0;
  }
  for (int d = 0; d < 8; ++d) {
    const int nx = x + dx[d];
    const int ny = y + dy[d];
    if (nx < 0 || ny < 0 || nx >= (int)map->width || ny >= (int)map->height) {
      continue;
    }
    if (!map_tile_is_water(map, nx, ny) && !map_tile_is_high_seas(map, nx, ny)) {
      return 1;
    }
  }
  return 0;
}

/* Land neighbour of coastal water (prefer N, then W/E/S, then diagonals). */
static int ai_euro_land_adjacent_to(
  const ColonizeWorldMap* map,
  int wx,
  int wy,
  int* out_x,
  int* out_y
) {
  static const int dx[8] = {0, -1, 1, 0, -1, 1, -1, 1};
  static const int dy[8] = {-1, 0, 0, 1, -1, -1, 1, 1}; /* N W E S NW NE SW SE */
  if (!map || !out_x || !out_y) {
    return 0;
  }
  for (int i = 0; i < 8; ++i) {
    const int nx = wx + dx[i];
    const int ny = wy + dy[i];
    if (nx < 0 || ny < 0 || nx >= (int)map->width || ny >= (int)map->height) {
      continue;
    }
    if (!map_tile_is_water(map, nx, ny) && !map_tile_is_high_seas(map, nx, ny)) {
      *out_x = nx;
      *out_y = ny;
      return 1;
    }
  }
  return 0;
}

static int ai_euro_ship_has_land_adjacent(const ColonizeWorldMap* map, int sx, int sy) {
  int lx = 0;
  int ly = 0;
  return ai_euro_land_adjacent_to(map, sx, sy, &lx, &ly);
}

/*
 * Pick land tile adjacent to ship for unload. Prefer toward landfall; skip
 * occupied/forbidden. Returns 0 if none.
 */
static int ai_euro_pick_unload_land(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  int pax_id,
  int prefer_x,
  int prefer_y,
  int avoid_x,
  int avoid_y,
  int* out_x,
  int* out_y
) {
  static const int dx[8] = {0, -1, 1, 0, -1, 1, -1, 1};
  static const int dy[8] = {-1, 0, 0, 1, -1, -1, 1, 1};
  ColonizeUnit* pax = NULL;
  if (!ctx || !ctx->map || !ctx->units || !ship || !out_x || !out_y) {
    return 0;
  }
  pax = units_get(ctx->units, pax_id);
  if (!pax) {
    return 0;
  }
  int best_x = -1;
  int best_y = -1;
  int best_d = 9999;
  for (int i = 0; i < 8; ++i) {
    const int nx = ship->x + dx[i];
    const int ny = ship->y + dy[i];
    if (nx < 0 || ny < 0 || nx >= (int)ctx->map->width || ny >= (int)ctx->map->height) {
      continue;
    }
    if (map_tile_is_water(ctx->map, nx, ny) || map_tile_is_high_seas(ctx->map, nx, ny)) {
      continue;
    }
    if (nx == avoid_x && ny == avoid_y) {
      continue;
    }
    if (!units_can_enter(ctx->units, pax->type_index, ctx->map, nx, ny, pax_id, ctx->colonies)) {
      continue;
    }
    int d = 0;
    if (prefer_x >= 0 && prefer_y >= 0) {
      d = ai_euro_chebyshev(nx, ny, prefer_x, prefer_y);
    } else {
      d = i; /* N-first preference order */
    }
    if (d < best_d) {
      best_d = d;
      best_x = nx;
      best_y = ny;
    }
  }
  if (best_x < 0) {
    return 0;
  }
  *out_x = best_x;
  *out_y = best_y;
  return 1;
}

static int ai_euro_unload_pax_at(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  ColonizeUnit* pax,
  int dest_x,
  int dest_y,
  int orders,
  int goto_x,
  int goto_y
) {
  if (!ctx || !ctx->units || !ship || !pax) {
    return 0;
  }
  if (!units_unload_passenger(
        ctx->units, ship->id, pax->id, ctx->map, dest_x, dest_y, ctx->colonies
      )) {
    return 0;
  }
  pax = units_get(ctx->units, pax->id);
  if (!pax) {
    return 0;
  }
  ai_euro_set_goto(pax, orders, goto_x, goto_y);
  pax->moves_left = 0;
  if (pax->id >= 0 && pax->id < COLONIZE_UNITS_MAX) {
    s_unloaded_this_turn[pax->id] = 1;
  }
  /*
   * FUN_5bfb_3180 after landfall: Indian first contact from an adjacent
   * Brave or tribe-owned land (seed-100 Dutch TURN2→3: Soldier lands (48,14)
   * next to Aztec-owned ground → relation 96). Cite: FUN_5bfb_022e.
   */
  (void)ai_contact_encounter_scan(ctx, pax->nation_id, pax->x, pax->y);
  return 1;
}

static int ai_euro_name_is_pioneer(const char* name) {
  return name && (strstr(name, "Pioneer") || strstr(name, "Hardy"));
}

static int ai_euro_name_is_soldier(const char* name) {
  return name && strstr(name, "Soldier") != NULL;
}


/*
 * 0a60-style coastal staging from Atlantic landfall (same geometry as
 * ai_coastal_staging_from_landfall in ai.c). TURN3 ship XY matches the tip
 * for seed-100 FR/SP landfalls. Cite: test-saves-ai/TURN3; euro_dispatcher 0a60.
 */
static int ai_euro_coastal_staging_from_landfall(
  const ColonizeWorldMap* map,
  int landfall_x,
  int landfall_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return 0;
  }
  int tip_x = landfall_x - 5;
  int tip_y = landfall_y - 3;
  if (landfall_y < 30) {
    tip_x = landfall_x - 6;
    tip_y = landfall_y - 1;
  }
  int best_x = -1;
  int best_y = -1;
  int best_d = 9999;
  for (int x = tip_x - 3; x <= tip_x + 3; ++x) {
    for (int y = tip_y - 3; y <= tip_y + 3; ++y) {
      if (!ai_euro_tile_is_coast_water(map, x, y)) {
        continue;
      }
      int dx = x - tip_x;
      int dy = y - tip_y;
      if (dx < 0) {
        dx = -dx;
      }
      if (dy < 0) {
        dy = -dy;
      }
      const int d = dx + dy;
      if (d < best_d) {
        best_d = d;
        best_x = x;
        best_y = y;
      }
    }
  }
  if (best_x < 0) {
    return 0;
  }
  *out_x = best_x;
  *out_y = best_y;
  return 1;
}

static int ai_euro_colony_count(const ColonizeColonyPool* colonies, int nation_id) {
  int n = 0;
  if (!colonies) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (colonies->colonies[i].active && colonies->colonies[i].nation_id == nation_id) {
      ++n;
    }
  }
  return n;
}

/*
 * Empty ship on / past the post-beachhead tip with exactly one colony: continue
 * SW coastal cruise (TURN4→5 DU 43,16→39,18; TURN5→6 →37,19). Trade haul must
 * not yank tip station-keep toward colony berth water. Cite: TURN5–6.
 */
static int ai_euro_try_post_found_coast_cruise(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->map || !ctx->colonies || !ctx->units || !u || !u->active) {
    return 0;
  }
  if (u->cargo_count > 0) {
    return 0;
  }
  int fx = -1;
  int fy = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (c->active && c->nation_id == nation_id) {
      fx = c->x;
      fy = c->y;
      break;
    }
  }
  const int colony_n = ai_euro_colony_count(ctx->colonies, nation_id);
  if (fx < 0) {
    /* Pre-found SP: tip from landfall table while pioneer sits on town. */
    int lx = 0;
    int ly = 0;
    if (!ai_euro_recover_landfall_from_ship(u->x, u->y, &lx, &ly) ||
        !ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lx, ly, &fx, &fy)) {
      return 0;
    }
    int pioneer_on_found = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* p = &ctx->units->units[i];
      if (!p->active || p->nation_id != nation_id || p->aboard_ship_id >= 0) {
        continue;
      }
      if (ai_euro_name_is_pioneer(units_display_name(ctx->units, p)) && p->x == fx &&
          p->y == fy) {
        pioneer_on_found = 1;
        break;
      }
    }
    if (!pioneer_on_found) {
      return 0;
    }
  } else if (colony_n != 1) {
    return 0;
  }
  int tip_x = 0;
  int tip_y = 0;
  if (!ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &tip_x, &tip_y)) {
    return 0;
  }
  /* Mid-band tip is scored (no longer caller-hardcoded). FR leg1 home uses mid. */
  const int tip_from_table = !(fy >= 30 && fy < 50);
  /* On tip or SW cruise legs — not SP one-west tip (45,50). */
  const int on_tip = (u->x == tip_x && u->y == tip_y);
  const int on_leg1 = (u->x == tip_x - 4 && u->y == tip_y + 2);
  const int on_leg2 = (u->x == tip_x - 6 && u->y == tip_y + 3);
  /* SP: one west of tip after pioneer landfall — NE berth (TURN5→6). */
  if (!on_tip && !on_leg1 && !on_leg2 && u->x == tip_x - 1 && u->y == tip_y) {
    if (colony_n != 1) {
      /* Pre-found: hold tip−1 so trade haul cannot yank (TURN5 45,50). */
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
      u->moves_left = 0;
      return 1;
    }
    int bx = tip_x;
    int by = tip_y - 1;
    if (!map_tile_is_water(ctx->map, bx, by) && !map_tile_is_high_seas(ctx->map, bx, by)) {
      return 0;
    }
    if (u->moves_left <= 0 || units_orders_skip_turn(u)) {
      (void)units_wake(ctx->units, u->id);
      u = units_get(ctx->units, u->id);
      if (!u || !u->active) {
        return 1;
      }
    }
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, bx, by);
    while (u && u->active && u->moves_left > 0 && (u->x != bx || u->y != by)) {
      if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
        break;
      }
      u = units_get(ctx->units, u->id);
    }
    if (u && u->active) {
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
      u->moves_left = 0;
    }
    return 1;
  }
  /* SP: already on NE berth — hold against trade-haul yank (TURN6 46,49). */
  if (!on_tip && !on_leg1 && !on_leg2 && u->x == tip_x && u->y == tip_y - 1 &&
      colony_n == 1) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
    u->moves_left = 0;
    return 1;
  }
  if (!on_tip && !on_leg1 && !on_leg2) {
    return 0;
  }
  /* Post-found SW legs only after the town exists (DU/FR). */
  if (colony_n != 1) {
    /* SP: tip station with pioneer on found → one west (TURN4→5 46,50→45,50). */
    if (on_tip && map_tile_is_water(ctx->map, tip_x - 1, tip_y)) {
      int pioneer_on_found = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* p = &ctx->units->units[i];
        if (!p->active || p->nation_id != nation_id || p->aboard_ship_id >= 0) {
          continue;
        }
        if (ai_euro_name_is_pioneer(units_display_name(ctx->units, p)) && p->x == fx &&
            p->y == fy) {
          pioneer_on_found = 1;
          break;
        }
      }
      if (pioneer_on_found) {
        if (u->moves_left <= 0 || units_orders_skip_turn(u)) {
          (void)units_wake(ctx->units, u->id);
          u = units_get(ctx->units, u->id);
          if (!u || !u->active) {
            return 1;
          }
        }
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tip_x - 1, tip_y);
        (void)units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL);
        u = units_get(ctx->units, u->id);
        if (u) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
          u->moves_left = 0;
        }
        return 1;
      }
    }
    return 0; /* do not latch tip before pioneer arrives */
  }
  /*
   * FR mid-band: after SW leg1, sail home to tip with colony goto
   * (TURN6→7 48,45→52,43 g=Quebec). tip_from_table is false for mid.
   * Cite: test-saves-ai/TURN7; Series E3.
   */
  if (on_leg1 && !tip_from_table) {
    if (u->moves_left <= 0 || units_orders_skip_turn(u)) {
      (void)units_wake(ctx->units, u->id);
      u = units_get(ctx->units, u->id);
      if (!u || !u->active) {
        return 1;
      }
    }
    ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tip_x, tip_y);
    while (u && u->active && u->moves_left > 0 && (u->x != tip_x || u->y != tip_y)) {
      if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
        break;
      }
      u = units_get(ctx->units, u->id);
    }
    if (u && u->active) {
      ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, fx, fy);
      u->moves_left = 0;
    }
    return 1;
  }
  /*
   * Geometric legs from tip: first (−4,+2) → TURN5 DU 39,18 / TURN6 FR 48,45;
   * next (−6,+3) → TURN6 DU 37,19; next (−11,+6) → TURN7 DU 32,22.
   * Cite: test-saves-ai/TURN5–7.
   */
  int gx = tip_x - 4;
  int gy = tip_y + 2;
  if (on_leg2) {
    gx = tip_x - 11;
    gy = tip_y + 6;
  } else if (on_leg1) {
    gx = tip_x - 6;
    gy = tip_y + 3;
  }
  if (gx < 0) {
    gx = 0;
  }
  if (gy < 0) {
    gy = 0;
  }
  if (gx >= (int)ctx->map->width) {
    gx = (int)ctx->map->width - 1;
  }
  if (gy >= (int)ctx->map->height) {
    gy = (int)ctx->map->height - 1;
  }
  if (!map_tile_is_water(ctx->map, gx, gy) && !map_tile_is_high_seas(ctx->map, gx, gy)) {
    for (int d = 0; d < 8; ++d) {
      static const int kdx[] = {-1, -1, 0, 1, -1, 0, 1, 1};
      static const int kdy[] = {0, 1, 1, 1, -1, -1, -1, 0};
      const int nx = gx + kdx[d];
      const int ny = gy + kdy[d];
      if (nx >= 0 && ny >= 0 && nx < (int)ctx->map->width && ny < (int)ctx->map->height &&
          (map_tile_is_water(ctx->map, nx, ny) || map_tile_is_high_seas(ctx->map, nx, ny))) {
        gx = nx;
        gy = ny;
        break;
      }
    }
  }
  if (u->moves_left <= 0 || units_orders_skip_turn(u)) {
    (void)units_wake(ctx->units, u->id);
    u = units_get(ctx->units, u->id);
    if (!u || !u->active) {
      return 1;
    }
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, gx, gy);
  /* Pathfind drain — ocean score_move overshoots (38,19 vs 39,18). */
  while (u && u->active && u->moves_left > 0 && (u->x != gx || u->y != gy)) {
    if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
      break;
    }
    u = units_get(ctx->units, u->id);
  }
  if (u && u->active) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
    u->moves_left = 0;
  }
  return 1;
}

/*
 * True when colony has Stockade, Warehouse, Lumber Mill, Drydock, Shipyard, or
 * Custom House in the build queue — carpenter hammers need on-site labor. Cite:
 * docs/building_production.md chart (Stockade 64h / Warehouse 80h / Lumber Mill
 * 52h / Drydock 80h ship repair / Shipyard 240h ship construction / Custom House
 * 160h Stuyvesant); fandom Naval Docks→Drydock→Shipyard; fandom Peter Stuyvesant
 * Custom House unlock. Structural stay/LABOR only — no invented hammer/gold /
 * auto-sell rates.
 */
static int ai_euro_type_is_man_o_war_name(const char* name) {
  if (!name) {
    return 0;
  }
  return strstr(name, "Man-O-War") != NULL || strstr(name, "Man of War") != NULL ||
         strstr(name, "Man-O'-War") != NULL;
}

/*
 * FUN_4962_0018 thin: clear ship bits 0x01/0x02, then OR from foreign armed
 * sea units within MD≤5 (MoW → 0x02, else armed → 0x01). Also thin-latch
 * needs_colonists / needs_garrison from pop / garrison_quota.
 */
/*
 * "This colony is eating into its stores" — stock below one turn's
 * consumption (TURN_FOOD_PER_COLONIST = 2 per head). Used to ride in
 * colony_flags bit3 until that bit went back to being DOS's
 * inefficient-government latch; the AI only ever wanted the reading, never
 * the storage.
 */
static int ai_euro_colony_food_short(const ColonizeColony* c) {
  if (!c) {
    return 0;
  }
  const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
  return c->stock[COLONIZE_CARGO_FOOD] < pop * 2;
}

static void ai_euro_refresh_colony_ai_flags(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c
) {
  if (!ctx || !c || !c->active) {
    return;
  }
  c->ai_flags = (uint8_t)(c->ai_flags & (uint8_t)~(COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP |
                                                    COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR));
  if (ctx->units) {
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      const ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id == nation_id) {
        continue;
      }
      if (!units_is_sea(ctx->units, u->id)) {
        continue;
      }
      if (abs(u->x - c->x) + abs(u->y - c->y) > 5) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
      const char* nm = units_display_name(ctx->units, u);
      if (ai_euro_type_is_man_o_war_name(nm) ||
          (ty && ty->name[0] && ai_euro_type_is_man_o_war_name(ty->name))) {
        c->ai_flags |= COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR;
      } else if (ty && ty->attack > 0) {
        c->ai_flags |= COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP;
      }
    }
  }
  if (c->population < 3) {
    c->ai_flags |= COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  } else {
    c->ai_flags =
      (uint8_t)(c->ai_flags & (uint8_t)~COLONIZE_COLONY_AI_NEEDS_COLONISTS);
  }
  if (c->garrison_quota > 0) {
    c->ai_flags |= COLONIZE_COLONY_AI_NEEDS_GARRISON;
  } else {
    c->ai_flags =
      (uint8_t)(c->ai_flags & (uint8_t)~COLONIZE_COLONY_AI_NEEDS_GARRISON);
  }
  /*
   * +0x1c thin: wagon / coastal / small-colony. Bit3 is NOT touched here: it
   * is DOS's inefficient-government latch (FUN_364b_0688 phase D), which the
   * per-turn colony tick owns. This pass used to overwrite it with a
   * food-vs-need reading, which both clobbered the latch and had no DOS
   * basis; ai_euro_colony_food_short below is the food test the AI wanted.
   */
  {
    const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
    if (pop < 10) {
      c->colony_flags |= COLONIZE_COLONY_FLAG_SMALL_AI;
    } else {
      c->colony_flags =
        (uint8_t)(c->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_SMALL_AI);
    }
  }
  colony_prod_refresh_sol_flags(c, (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL);
  if (ctx->units) {
    int wagon = 0;
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      const ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id != nation_id || u->x != c->x || u->y != c->y) {
        continue;
      }
      const char* nm = units_display_name(ctx->units, u);
      if (nm && strstr(nm, "Wagon") != NULL) {
        wagon = 1;
        break;
      }
    }
    if (wagon) {
      c->colony_flags |= COLONIZE_COLONY_FLAG_WAGON_TRAIN;
    } else {
      c->colony_flags =
        (uint8_t)(c->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_WAGON_TRAIN);
    }
  }
  if (ctx->map) {
    int coastal = 0;
    static const int dx[4] = {0, 1, 0, -1};
    static const int dy[4] = {-1, 0, 1, 0};
    for (int d = 0; d < 4; ++d) {
      if (!map_tile_is_land(ctx->map, c->x + dx[d], c->y + dy[d])) {
        coastal = 1;
        break;
      }
    }
    if (coastal) {
      c->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
    } else {
      c->colony_flags =
        (uint8_t)(c->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_COASTAL);
    }
  }
}

static int ai_euro_colony_wants_construction_labor(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
) {
  if (!pool || !c || !c->active) {
    return 0;
  }
  /* Col1 +0x1d bit7 latch (FUN_5952) — save import or Linux construction set. */
  if ((c->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) != 0) {
    return 1;
  }
  if (c->building_in_production < 0) {
    return 0;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(pool, c->building_in_production);
  if (!bt || bt->name[0] == '\0') {
    return 0;
  }
  return strcmp(bt->name, "Stockade") == 0 || strcmp(bt->name, "Fort") == 0 ||
         strcmp(bt->name, "Fortress") == 0 || strcmp(bt->name, "Warehouse") == 0 ||
         strcmp(bt->name, "Lumber Mill") == 0 || strcmp(bt->name, "Drydock") == 0 ||
         strcmp(bt->name, "Shipyard") == 0 || strcmp(bt->name, "Custom House") == 0;
}

/* True when any own colony wants on-site carpenter construction LABOR. */
static int ai_euro_nation_wants_construction_labor(
  const ColonizeTurnContext* ctx,
  int nation_id
) {
  if (!ctx || !ctx->colonies || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (ai_euro_colony_wants_construction_labor(ctx->colonies, c)) {
      return 1;
    }
  }
  return 0;
}

/*
 * Peace construction pick (5d04 / colony planning): idle/empty
 * building_in_production (< 0) → prefer Stockade → Fort → Fortress → Warehouse
 * → (coastal) Docks via colonies_list_buildable + colonies_set_construction. Cite:
 * docs/fandom_col1994.md Defense Stockade→Fort→Fortress / Storage Warehouse /
 * Naval Docks→Drydock→Shipyard; docs/building_production.md Stockade 64h /
 * Fort 120h / Fortress 320h / Warehouse 80h / Dock 52h. No invented hammer/gold
 * buyouts — queue only.
 * Near warehouse capacity (≥90% any non-food stock) with Warehouse already
 * built → prefer Warehouse Expansion before Docks (spoilage FUN_15eb_0a50).
 * Does not yank Fort/Fortress ahead of defense chain.
 */
static int ai_euro_colony_near_warehouse_cap(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
) {
  if (!pool || !c) {
    return 0;
  }
  for (int cargo = 0; cargo < COLONIZE_CARGO_COUNT; ++cargo) {
    if (cargo == COLONIZE_CARGO_FOOD) {
      continue;
    }
    const int cap = colonies_warehouse_capacity(pool, c, cargo);
    if (cap > 0 && c->stock[cargo] * 10 >= cap * 9) {
      return 1;
    }
  }
  return 0;
}

static void ai_euro_prefer_peace_construction(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  const int fort_id = colonies_find_building(ctx->colonies, "Fort");
  const int fortress_id = colonies_find_building(ctx->colonies, "Fortress");
  const int warehouse_id = colonies_find_building(ctx->colonies, "Warehouse");
  const int whe_id = colonies_find_building(ctx->colonies, "Warehouse Expansion");
  const int docks_id = colonies_find_building(ctx->colonies, "Docks");
  if (stockade_id < 0 && fort_id < 0 && fortress_id < 0 && warehouse_id < 0 && docks_id < 0) {
    return;
  }
  /*
   * Defense chain before storage/docks so Fort % live after Stockade. Docks
   * ahead of Warehouse: every seed-100 AI first town (New Amsterdam TURN4,
   * Quebec TURN5, Isabella TURN6 — size 1, coastal, no Stockade possible)
   * starts on Docks in the DOS saves.
   */
  const int prefer_def[] = {stockade_id, fort_id, fortress_id, warehouse_id, docks_id};
  /* Size < 3 (no Stockade yet): Docks first, per the DOS saves above. */
  const int prefer_young[] = {docks_id, warehouse_id};
  /* Near-cap + Warehouse owned: Expansion before Docks (still after Fort chain). */
  const int prefer_exp[] = {
    stockade_id, fort_id, fortress_id, warehouse_id, whe_id, docks_id
  };
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue; /* idle/empty queue only — do not yank active project */
    }
    const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
    const int has_stockade =
      stockade_id >= 0 && stockade_id < COLONIZE_BUILDING_TYPES_MAX &&
      c->has_building[stockade_id];
    const int has_wh =
      warehouse_id >= 0 && warehouse_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[warehouse_id];
    const int use_exp =
      has_wh && whe_id >= 0 && ai_euro_colony_near_warehouse_cap(ctx->colonies, c);
    const int stockade_min =
      stockade_id >= 0 ? ctx->colonies->building_types[stockade_id].min_population : 0;
    const int young = !has_stockade && stockade_min > 0 && pop < stockade_min;
    const int* prefer = use_exp ? prefer_exp : (young ? prefer_young : prefer_def);
    const size_t nprefer =
      use_exp ? (sizeof(prefer_exp) / sizeof(prefer_exp[0]))
              : (young ? (sizeof(prefer_young) / sizeof(prefer_young[0]))
                       : (sizeof(prefer_def) / sizeof(prefer_def[0])));
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int pick = -1;
    for (size_t p = 0; p < nprefer; ++p) {
      const int want = prefer[p];
      if (want < 0) {
        continue;
      }
      if (stockade_id >= 0 && pop >= 2 && !has_stockade && want != stockade_id) {
        continue;
      }
      for (int b = 0; b < n; ++b) {
        if (buildable[b] == want) {
          pick = want;
          break;
        }
      }
      if (pick >= 0) {
        break;
      }
    }
    if (pick >= 0) {
      (void)colonies_set_construction(ctx->colonies, c->id, pick);
    }
  }
}

/*
 * Pop≥2 without Stockade: clear any prefer_* queue so hammers bank with bip
 * 0xFF (TURN5→6 Dutch). Cite: turn.c hammer bank; test-saves-ai/TURN6.
 */
static void ai_euro_clear_pre_stockade_build_queue(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id || c->building_in_production < 0) {
      continue;
    }
    const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
    const int has_stockade =
      stockade_id >= 0 && stockade_id < COLONIZE_BUILDING_TYPES_MAX &&
      c->has_building[stockade_id];
    if (pop < 3 && !has_stockade) {
      c->building_in_production = -1;
    }
  }
}

/*
 * Zero-hammer projects on colonies not founded this act: cancel (Quebec
 * TURN5→6 bip→0xFF; keep same-turn Isabella auto-Stockade). Cite: TURN6–7.
 */
static void ai_euro_cancel_stale_zero_hammer_builds(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || nation_id < 0 || nation_id >= 4) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id || c->building_in_production < 0) {
      continue;
    }
    if (c->hammers != 0) {
      continue;
    }
    if (c->id >= 0 && c->id < COLONIZE_COLONIES_MAX && s_founded_colony_turn[c->id]) {
      continue;
    }
    c->building_in_production = -1;
  }
}

/*
 * Coastal Drydock prefer (5d04 / colony planning): own colony with Docks, no
 * Drydock yet, idle/empty building_in_production → colonies_set_construction
 * Drydock when colonies_list_buildable includes it (coastal + chain gates).
 * Cite: docs/fandom_col1994.md Naval Docks→Drydock→Shipyard;
 * docs/building_production.md Drydock 80h ship repair. Carpenter LABOR binds
 * via ai_euro_colony_wants_construction_labor. No invented hammer rates.
 * Runs after peace Stockade→Warehouse→Docks pick so earlier buildings win.
 */
static void ai_euro_prefer_coastal_drydock(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int drydock_id = colonies_find_building(ctx->colonies, "Drydock");
  const int docks_id = colonies_find_building(ctx->colonies, "Docks");
  if (drydock_id < 0 || docks_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue; /* idle/empty queue only — do not yank active project */
    }
    if (!c->has_building[docks_id] || c->has_building[drydock_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int drydock_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == drydock_id) {
        drydock_ok = 1;
        break;
      }
    }
    if (!drydock_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, drydock_id);
  }
}

/*
 * Coastal Shipyard prefer (5d04 / colony planning): own colony with Drydock,
 * no Shipyard yet, idle/empty building_in_production → colonies_set_construction
 * Shipyard when colonies_list_buildable includes it (coastal + chain gates).
 * Cite: docs/fandom_col1994.md Naval Docks→Drydock→Shipyard;
 * docs/building_production.md Shipyard 240h ship construction. Carpenter LABOR
 * binds via ai_euro_colony_wants_construction_labor. No invented hammer rates.
 * Runs after coastal Drydock prefer so Drydock wins when still missing.
 */
static void ai_euro_prefer_coastal_shipyard(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int shipyard_id = colonies_find_building(ctx->colonies, "Shipyard");
  const int drydock_id = colonies_find_building(ctx->colonies, "Drydock");
  if (shipyard_id < 0 || drydock_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue; /* idle/empty queue only — do not yank active project */
    }
    if (!c->has_building[drydock_id] || c->has_building[shipyard_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int shipyard_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == shipyard_id) {
        shipyard_ok = 1;
        break;
      }
    }
    if (!shipyard_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, shipyard_id);
  }
}

/*
 * Stuyvesant Custom House prefer (5d04 / colony planning): when nation owns
 * Peter Stuyvesant (FF 3), own colony without Custom House, idle/empty
 * building_in_production → colonies_set_construction Custom House when
 * colonies_list_buildable includes it (opts.has_peter_stuyvesant gate).
 * Cite: docs/fandom_col1994.md Peter Stuyvesant unlock Custom House;
 * colony.c Custom House gate (stuy && !owned); founding_fathers elect
 * FF_PETER_STUYVESANT comment (has_peter_stuyvesant). Carpenter LABOR binds
 * via ai_euro_colony_wants_construction_labor. Construction unlock/prefer
 * only — no invented Custom House auto-sell gold/thresholds.
 * Runs after coastal Drydock→Shipyard so naval chain wins when still missing.
 */
static void ai_euro_prefer_custom_house(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (!ctx->col1 ||
      !founding_fathers_nation_has(ctx->col1, nation_id, FF_PETER_STUYVESANT)) {
    return;
  }
  const int custom_id = colonies_find_building(ctx->colonies, "Custom House");
  if (custom_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  /* Mirror game_loop game_colony_buildable_opts / game_nation_has_ff (FF 3). */
  opts.has_peter_stuyvesant = true;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue; /* idle/empty queue only — do not yank active project */
    }
    if (c->has_building[custom_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int custom_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == custom_id) {
        custom_ok = 1;
        break;
      }
    }
    if (!custom_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, custom_id);
  }
}

static int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id);

/*
 * Peace Church prefer (5d04 / colony planning): own colony with Stockade (defense
 * first), no Church/Cathedral, idle/empty building_in_production → Church when
 * colonies_list_buildable includes it. Cite: building_production.md Church→
 * Crosses; Colonization.pdf Church / immigration; fandom Crosses. Runs after
 * Stockade→…→Docks / Drydock / Shipyard / Custom House so those win when open.
 * Skipped while at war (wartime Armory prefer owns the queue).
 */
static void ai_euro_prefer_church(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* Wartime Armory prefer owns muskets queue — skip crosses while at war. */
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int church_id = colonies_find_building(ctx->colonies, "Church");
  const int cathedral_id = colonies_find_building(ctx->colonies, "Cathedral");
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  if (church_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (church_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[church_id]) {
      continue;
    }
    if (cathedral_id >= 0 && cathedral_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[cathedral_id]) {
      continue;
    }
    /* Defense first: Stockade owned (or type missing from pool — allow). */
    if (stockade_id >= 0 && stockade_id < COLONIZE_BUILDING_TYPES_MAX &&
        !c->has_building[stockade_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int church_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == church_id) {
        church_ok = 1;
        break;
      }
    }
    if (!church_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, church_id);
  }
}

/*
 * Wartime Armory prefer (5d04 / colony planning): at war with a Euro peer, own
 * colony with Stockade, no Armory/Magazine/Arsenal, idle queue → Armory when
 * buildable. Cite: building_production.md Armory Tools→Muskets; Colonization.pdf
 * Defending a Colony / Armory; fandom Armory. Church prefer skips while at war.
 */
static void ai_euro_prefer_armory_at_war(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int armory_id = colonies_find_building(ctx->colonies, "Armory");
  const int magazine_id = colonies_find_building(ctx->colonies, "Magazine");
  const int arsenal_id = colonies_find_building(ctx->colonies, "Arsenal");
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  if (armory_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (armory_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[armory_id]) {
      continue;
    }
    if (magazine_id >= 0 && magazine_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[magazine_id]) {
      continue;
    }
    if (arsenal_id >= 0 && arsenal_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[arsenal_id]) {
      continue;
    }
    if (stockade_id >= 0 && stockade_id < COLONIZE_BUILDING_TYPES_MAX &&
        !c->has_building[stockade_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int armory_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == armory_id) {
        armory_ok = 1;
        break;
      }
    }
    if (!armory_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, armory_id);
  }
}

/*
 * Peace Printing Press prefer (5d04 / colony planning): Stockade+Church owned,
 * no Printing Press/Newspaper, idle queue → Printing Press when buildable.
 * Cite: building_production.md Printing Press +50% liberty bells; Colonization.pdf
 * Liberty Bells. After Church prefer; skipped while at war (Armory owns queue).
 */
static void ai_euro_prefer_printing_press(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int press_id = colonies_find_building(ctx->colonies, "Printing Press");
  const int newspaper_id = colonies_find_building(ctx->colonies, "Newspaper");
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  const int church_id = colonies_find_building(ctx->colonies, "Church");
  if (press_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (press_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[press_id]) {
      continue;
    }
    if (newspaper_id >= 0 && newspaper_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[newspaper_id]) {
      continue;
    }
    if (stockade_id >= 0 && stockade_id < COLONIZE_BUILDING_TYPES_MAX &&
        !c->has_building[stockade_id]) {
      continue;
    }
    /* Crosses first when Church type exists. */
    if (church_id >= 0 && church_id < COLONIZE_BUILDING_TYPES_MAX &&
        !c->has_building[church_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int press_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == press_id) {
        press_ok = 1;
        break;
      }
    }
    if (!press_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, press_id);
  }
}

/*
 * Peace Schoolhouse prefer (5d04 / colony planning): Stockade owned, pop≥4, no
 * Schoolhouse/College/University, idle queue → Schoolhouse when buildable.
 * Cite: building_production.md Schoolhouse teach faculty 1; Colonization.pdf
 * Education. After Printing Press; skipped while at war.
 */
static void ai_euro_prefer_schoolhouse(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int school_id = colonies_find_building(ctx->colonies, "Schoolhouse");
  const int college_id = colonies_find_building(ctx->colonies, "College");
  const int univ_id = colonies_find_building(ctx->colonies, "University");
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  if (school_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (c->population < 4) {
      continue;
    }
    if (school_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[school_id]) {
      continue;
    }
    if (college_id >= 0 && college_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[college_id]) {
      continue;
    }
    if (univ_id >= 0 && univ_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[univ_id]) {
      continue;
    }
    if (stockade_id >= 0 && stockade_id < COLONIZE_BUILDING_TYPES_MAX &&
        !c->has_building[stockade_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int school_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == school_id) {
        school_ok = 1;
        break;
      }
    }
    if (!school_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, school_id);
  }
}

/*
 * Wartime Magazine prefer (5d04): at war, Armory owned, no Magazine/Arsenal,
 * idle queue → Magazine when buildable. Cite: building_production.md Magazine
 * doubles muskets vs Armory; Colonization.pdf. After Armory prefer.
 */
static void ai_euro_prefer_magazine_at_war(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int armory_id = colonies_find_building(ctx->colonies, "Armory");
  const int magazine_id = colonies_find_building(ctx->colonies, "Magazine");
  const int arsenal_id = colonies_find_building(ctx->colonies, "Arsenal");
  if (magazine_id < 0 || armory_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (armory_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[armory_id]) {
      continue;
    }
    if (magazine_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[magazine_id]) {
      continue;
    }
    if (arsenal_id >= 0 && arsenal_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[arsenal_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int mag_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == magazine_id) {
        mag_ok = 1;
        break;
      }
    }
    if (!mag_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, magazine_id);
  }
}

/*
 * Peace Newspaper prefer (5d04): Printing Press owned, no Newspaper, idle →
 * Newspaper when buildable. Cite: building_production.md Newspaper +100% bells;
 * Colonization.pdf. After Schoolhouse; skipped while at war.
 */
static void ai_euro_prefer_newspaper(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int press_id = colonies_find_building(ctx->colonies, "Printing Press");
  const int newspaper_id = colonies_find_building(ctx->colonies, "Newspaper");
  if (newspaper_id < 0 || press_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (press_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[press_id]) {
      continue;
    }
    if (newspaper_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[newspaper_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int news_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == newspaper_id) {
        news_ok = 1;
        break;
      }
    }
    if (!news_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, newspaper_id);
  }
}

/*
 * Peace College prefer (5d04): Schoolhouse owned, pop≥8, no College/University,
 * idle → College when buildable. Cite: building_production.md College faculty 2;
 * Colonization.pdf Education. After Newspaper; skipped while at war.
 */
static void ai_euro_prefer_college(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int school_id = colonies_find_building(ctx->colonies, "Schoolhouse");
  const int college_id = colonies_find_building(ctx->colonies, "College");
  const int univ_id = colonies_find_building(ctx->colonies, "University");
  if (college_id < 0 || school_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (c->population < 8) {
      continue;
    }
    if (school_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[school_id]) {
      continue;
    }
    if (college_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[college_id]) {
      continue;
    }
    if (univ_id >= 0 && univ_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[univ_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int college_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == college_id) {
        college_ok = 1;
        break;
      }
    }
    if (!college_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, college_id);
  }
}

/*
 * Peace University prefer (5d04): College owned, pop≥10, no University, idle →
 * University when buildable. Cite: building_production.md University faculty 3
 * (min pop 10 / 200 hammers); Colonization.pdf Education. After College;
 * skipped while at war.
 */
static void ai_euro_prefer_university(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int college_id = colonies_find_building(ctx->colonies, "College");
  const int univ_id = colonies_find_building(ctx->colonies, "University");
  if (univ_id < 0 || college_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (c->population < 10) {
      continue;
    }
    if (college_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[college_id]) {
      continue;
    }
    if (univ_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[univ_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int univ_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == univ_id) {
        univ_ok = 1;
        break;
      }
    }
    if (!univ_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, univ_id);
  }
}

/*
 * Peace Cathedral prefer (5d04): Church owned, pop≥8, no Cathedral, idle →
 * Cathedral when buildable. Cite: building_production.md Cathedral crosses;
 * Colonization.pdf. After University; skipped while at war.
 */
static void ai_euro_prefer_cathedral(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  const int church_id = colonies_find_building(ctx->colonies, "Church");
  const int cathedral_id = colonies_find_building(ctx->colonies, "Cathedral");
  if (cathedral_id < 0 || church_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (c->population < 8) {
      continue;
    }
    if (church_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[church_id]) {
      continue;
    }
    if (cathedral_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[cathedral_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int cat_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == cathedral_id) {
        cat_ok = 1;
        break;
      }
    }
    if (!cat_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, cathedral_id);
  }
}

/*
 * Wartime Arsenal prefer (5d04): at war + Adam Smith + Magazine owned, no
 * Arsenal, idle → Arsenal when buildable. Cite: building_production.md Arsenal
 * factory muskets (Adam Smith); Colonization.pdf. After Magazine prefer.
 */
static void ai_euro_prefer_arsenal_at_war(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return;
  }
  if (!founding_fathers_nation_has(ctx->col1, nation_id, FF_ADAM_SMITH)) {
    return;
  }
  const int magazine_id = colonies_find_building(ctx->colonies, "Magazine");
  const int arsenal_id = colonies_find_building(ctx->colonies, "Arsenal");
  if (arsenal_id < 0 || magazine_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  opts.has_adam_smith = true;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (magazine_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[magazine_id]) {
      continue;
    }
    if (arsenal_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[arsenal_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int arsenal_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == arsenal_id) {
        arsenal_ok = 1;
        break;
      }
    }
    if (!arsenal_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, arsenal_id);
  }
}

/*
 * Stable prefer (5d04): Stockade owned, no Stable, idle → Stable when buildable.
 * Horse breeding for Dragoons / wagon horses. Cite: building_production.md
 * Stable 64h; Colonization.pdf. After Cathedral / wartime Arsenal so defense
 * and culture beat horses; runs in peace and war.
 */
static void ai_euro_prefer_stable(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  const int fort_id = colonies_find_building(ctx->colonies, "Fort");
  const int fortress_id = colonies_find_building(ctx->colonies, "Fortress");
  const int stable_id = colonies_find_building(ctx->colonies, "Stable");
  if (stable_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    const int fortified =
      (stockade_id >= 0 && stockade_id < COLONIZE_BUILDING_TYPES_MAX &&
       c->has_building[stockade_id]) ||
      (fort_id >= 0 && fort_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[fort_id]) ||
      (fortress_id >= 0 && fortress_id < COLONIZE_BUILDING_TYPES_MAX &&
       c->has_building[fortress_id]);
    if (!fortified) {
      continue;
    }
    if (stable_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[stable_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int stable_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == stable_id) {
        stable_ok = 1;
        break;
      }
    }
    if (!stable_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, stable_id);
  }
}

/*
 * Carpenter's Shop prefer (5d04): no Shop/Mill yet, idle → Shop when buildable.
 * Feeds Lumber Mill hammers. Cite: building_production.md Carpenter's Shop;
 * Colonization.pdf. After Stable.
 */
static void ai_euro_prefer_carpenters_shop(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int shop_id = colonies_find_building(ctx->colonies, "Carpenter's Shop");
  const int mill_id = colonies_find_building(ctx->colonies, "Lumber Mill");
  if (shop_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (shop_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[shop_id]) {
      continue;
    }
    if (mill_id >= 0 && mill_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[mill_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int shop_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == shop_id) {
        shop_ok = 1;
        break;
      }
    }
    if (!shop_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, shop_id);
  }
}

/*
 * Lumber Mill prefer (5d04): Carpenter's Shop owned, no Mill, idle → Mill.
 * Cite: building_production.md Lumber Mill; Colonization.pdf. After Shop.
 */
static void ai_euro_prefer_lumber_mill(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int shop_id = colonies_find_building(ctx->colonies, "Carpenter's Shop");
  const int mill_id = colonies_find_building(ctx->colonies, "Lumber Mill");
  if (mill_id < 0 || shop_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (shop_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[shop_id]) {
      continue;
    }
    if (mill_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[mill_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int mill_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == mill_id) {
        mill_ok = 1;
        break;
      }
    }
    if (!mill_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, mill_id);
  }
}

/*
 * Blacksmith's House prefer (5d04): ore≥20, no House/Shop/Iron Works, idle →
 * House. Parallel craft House step (raw≥20). Cite: building_production.md
 * Ore→Tools via Blacksmith's House; euro_unit_act craft house prefer.
 */
static void ai_euro_prefer_blacksmiths_house(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int house_id = colonies_find_building(ctx->colonies, "Blacksmith's House");
  const int shop_id = colonies_find_building(ctx->colonies, "Blacksmith's Shop");
  const int works_id = colonies_find_building(ctx->colonies, "Iron Works");
  if (house_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (c->stock[COLONIZE_CARGO_ORE] < 20) {
      continue;
    }
    if (house_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[house_id]) {
      continue;
    }
    if (shop_id >= 0 && shop_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[shop_id]) {
      continue;
    }
    if (works_id >= 0 && works_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[works_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int house_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == house_id) {
        house_ok = 1;
        break;
      }
    }
    if (!house_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, house_id);
  }
}

/*
 * Blacksmith's Shop prefer (5d04): House owned, no Shop/Iron Works, idle → Shop.
 * Cite: building_production.md Blacksmith's Shop Tools; Colonization.pdf.
 */
static void ai_euro_prefer_blacksmiths_shop(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int house_id = colonies_find_building(ctx->colonies, "Blacksmith's House");
  const int shop_id = colonies_find_building(ctx->colonies, "Blacksmith's Shop");
  const int works_id = colonies_find_building(ctx->colonies, "Iron Works");
  if (shop_id < 0 || house_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (house_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[house_id]) {
      continue;
    }
    if (shop_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[shop_id]) {
      continue;
    }
    if (works_id >= 0 && works_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[works_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int shop_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == shop_id) {
        shop_ok = 1;
        break;
      }
    }
    if (!shop_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, shop_id);
  }
}

/*
 * Iron Works prefer (5d04): Adam Smith + Blacksmith's Shop owned, no Iron Works,
 * idle → Iron Works when buildable. Cite: building_production.md Iron Works
 * factory tools (Adam Smith); Colonization.pdf. After Blacksmith's Shop.
 */
static void ai_euro_prefer_iron_works(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (!ctx->col1_ok || !ctx->col1 ||
      !founding_fathers_nation_has(ctx->col1, nation_id, FF_ADAM_SMITH)) {
    return;
  }
  const int shop_id = colonies_find_building(ctx->colonies, "Blacksmith's Shop");
  const int works_id = colonies_find_building(ctx->colonies, "Iron Works");
  if (works_id < 0 || shop_id < 0) {
    return;
  }
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  opts.has_adam_smith = true;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    if (shop_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[shop_id]) {
      continue;
    }
    if (works_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[works_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int works_ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == works_id) {
        works_ok = 1;
        break;
      }
    }
    if (!works_ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, works_id);
  }
}

/*
 * DOS has no Capitol construction preference to port: FUN_15eb_3650 refuses
 * @BUILDING 0x1e (Capitol) outright for every colony, and Capitol Expansion
 * sits behind it as a prerequisite, so neither the AI nor the player can ever
 * start one (colonies_building_is_buildable carries the same block). The
 * port's two Capitol prefer passes are gone with it.
 */

/*
 * Craft house/shop/factory prefer (5d04): House→Shop→Factory for rum/cotton/
 * tobacco/fur when raw stock≥20. Factories need Adam Smith. Cite:
 * building_production craft chains; dock craft hire stock≥20 gate. After
 * Iron Works.
 */
static void ai_euro_prefer_craft_upgrades(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  typedef struct {
    const char* house;
    const char* shop;
    const char* factory;
    int cargo;
  } CraftChain;
  static const CraftChain chains[] = {
    {"Rum Distiller's House", "Rum Distillery", "Rum Factory", COLONIZE_CARGO_SUGAR},
    {"Weaver's House", "Weaver's Shop", "Textile Mill", COLONIZE_CARGO_COTTON},
    {"Tobacconist's House", "Tobacconist's Shop", "Cigar Factory", COLONIZE_CARGO_TOBACCO},
    {"Fur Trader's House", "Fur Trading Post", "Fur Factory", COLONIZE_CARGO_FURS},
  };
  const int has_adam =
    ctx->col1_ok && ctx->col1 &&
    founding_fathers_nation_has(ctx->col1, nation_id, FF_ADAM_SMITH);
  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof(opts));
  opts.map = ctx->map;
  opts.has_adam_smith = has_adam;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->building_in_production >= 0) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    for (size_t ci = 0; ci < sizeof(chains) / sizeof(chains[0]); ++ci) {
      const CraftChain* ch = &chains[ci];
      if (c->stock[ch->cargo] < 20) {
        continue;
      }
      const int house_id = colonies_find_building(ctx->colonies, ch->house);
      const int shop_id = colonies_find_building(ctx->colonies, ch->shop);
      const int factory_id = colonies_find_building(ctx->colonies, ch->factory);
      int want = -1;
      if (house_id >= 0 && house_id < COLONIZE_BUILDING_TYPES_MAX && !c->has_building[house_id]) {
        want = house_id;
      } else if (
        house_id >= 0 && house_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[house_id] &&
        shop_id >= 0 && shop_id < COLONIZE_BUILDING_TYPES_MAX && !c->has_building[shop_id] &&
        (factory_id < 0 || factory_id >= COLONIZE_BUILDING_TYPES_MAX ||
         !c->has_building[factory_id])
      ) {
        want = shop_id;
      } else if (
        has_adam && shop_id >= 0 && shop_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[shop_id] && factory_id >= 0 && factory_id < COLONIZE_BUILDING_TYPES_MAX &&
        !c->has_building[factory_id]
      ) {
        want = factory_id;
      }
      if (want < 0) {
        continue;
      }
      int ok = 0;
      for (int b = 0; b < n; ++b) {
        if (buildable[b] == want) {
          ok = 1;
          break;
        }
      }
      if (!ok) {
        continue;
      }
      (void)colonies_set_construction(ctx->colonies, c->id, want);
      break;
    }
  }
}

/*
 * Expert Lumberjack LABOR when incomplete Warehouse or Lumber Mill and that
 * building type exists in the pool. Lumber feeds carpenter hammers
 * (building_production Lumberjack→Lumber). Cite: docs/building_production.md;
 * Colonization.pdf Skills Chart / lumberjack timber. Structural LABOR join
 * only — no invented lumber rates. Forest field-assign is wired separately
 * (ai_euro_try_lumberjack_field_assign) via colonies_assign_field.
 */
static int ai_euro_colony_wants_lumberjack_labor(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
) {
  if (!pool || !c || !c->active || c->building_in_production < 0) {
    return 0;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(pool, c->building_in_production);
  if (!bt || bt->name[0] == '\0') {
    return 0;
  }
  if (strcmp(bt->name, "Warehouse") == 0) {
    return colonies_find_building(pool, "Warehouse") >= 0;
  }
  if (strcmp(bt->name, "Lumber Mill") == 0) {
    return colonies_find_building(pool, "Lumber Mill") >= 0;
  }
  return 0;
}

/* True if nation_id is at war with any other European peer (0..3). */
static int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation_id) {
      continue;
    }
    if (ai_diplo_at_war(col1, nation_id, peer)) {
      return 1;
    }
  }
  return 0;
}

/* Forward: threatened colony (MD≤3 war-peer) — board skip / unload / LABOR. */
static int ai_euro_colony_threatened_by_war(
  ColonizeTurnContext* ctx,
  int nation_id,
  const ColonizeColony* c
);

static int ai_euro_is_military_name(const char* name) {
  if (!name) {
    return 0;
  }
  return strstr(name, "Soldier") != NULL || strstr(name, "Dragoon") != NULL ||
         strstr(name, "Regular") != NULL || strstr(name, "Continental") != NULL;
}

/* Soldier / Dragoon / Scout / Regular / Continental — land war hunt; not founders. */
static int ai_euro_is_land_war_hunter(const char* name) {
  if (!name) {
    return 0;
  }
  return ai_euro_is_military_name(name) || strstr(name, "Scout") != NULL;
}

static int ai_euro_is_artillery_name(const char* name) {
  return name && (strstr(name, "Artillery") != NULL || strstr(name, "Cannon") != NULL);
}

/*
 * Peace colony garrison (Defending a Colony): soldiers, dragoons, army,
 * cavalry — plus Regular (war-unit name already in military). Artillery is
 * separate (siege + border wake). Cite: Colonization.pdf Defending a Colony;
 * euro_unit_act §2d3.
 */
static int ai_euro_is_colony_garrison_name(const char* name) {
  if (!name) {
    return 0;
  }
  if (strstr(name, "Soldier") != NULL || strstr(name, "Dragoon") != NULL ||
      strstr(name, "Regular") != NULL) {
    return 1;
  }
  /* Continental Army / Continental Cavalry */
  if (strstr(name, "Continental") != NULL) {
    return 1;
  }
  return 0;
}

/*
 * Col1 +0x1e: fortify only while garrison_quota > 0, then DEC.
 * Cite: save_format_map.md; FUN_5952_035e seed PARKED (thin planning latch).
 */
static int ai_euro_fortify_with_quota(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u,
  int colony_id
) {
  if (!ctx || !ctx->colonies || !ctx->units || !u) {
    return 0;
  }
  ColonizeColony* c = colonies_get_mut(ctx->colonies, colony_id);
  if (!c || !c->active || c->nation_id != nation_id || c->garrison_quota == 0) {
    return 0;
  }
  if (!units_order_fortify(ctx->units, u->id)) {
    return 0;
  }
  if (c->garrison_quota > 0) {
    c->garrison_quota--;
  }
  return 1;
}

/* Colony fortification % on tile (0 if none / foreign / non-Euro). */
static int ai_euro_colony_fort_bonus_at(
  const ColonizeColonyPool* colonies,
  int x,
  int y,
  int nation_id
) {
  if (!colonies || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  const int cid = colonies_id_at(colonies, x, y);
  if (cid < 0) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(colonies, cid);
  if (!col || !col->active || col->nation_id != nation_id) {
    return 0;
  }
  return colonies_fortification_defense_bonus_percent(colonies, col);
}

static int ai_euro_land_is_fortified(const ColonizeUnit* u) {
  return u && (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED);
}

/* Sentry / fortify / fortified — wake-eligible passive land orders. */
static int ai_euro_land_is_passive_orders(const ColonizeUnit* u) {
  return u &&
         (u->orders == UNITS_ORDER_SENTRY || u->orders == UNITS_ORDER_FORTIFY ||
          u->orders == UNITS_ORDER_FORTIFIED);
}

/*
 * FUN_521d_06ae founding pick with coastal prefer (first colony and later).
 * Bias score toward map_tile_is_coastal foundable tiles (Docks / port access —
 * fandom Docks coastal gate; lose-all-ports war rule). First-colony FOUND XY
 * still comes from landfall latitude geometry when 06ae adj from ship/staging
 * misses Quebec/NA/Isabella (inland higher 2f77). Cite: euro_goals.c;
 * move_scoring.md §06ae; docs/fandom_col1994.md Docks + Independence port colonies.
 */
static int ai_euro_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int x,
  int y,
  int colony_count,
  int* out_x,
  int* out_y
) {
  /* First colony: coastal≥40 + west bias in goals; later towns coastal=10. */
  const int coastal = (colony_count <= 0) ? 40 : 10;
  return ai_goals_pick_founding_tile_ex(
    map,
    colonies,
    col1,
    /*units=*/NULL,
    nation_id,
    x,
    y,
    /*score_extras=*/1,
    /*wagon_filter=*/0,
    /*coastal_bonus=*/coastal,
    out_x,
    out_y
  );
}

/* Nearest primary MILITARY goal (Manhattan); 1 if found. */
static int ai_euro_nearest_military_goal(
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (nation_id < 0 || nation_id >= 4 || !out_x || !out_y) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = ai_goals_primary(nation_id, i);
    if (!s || s->code != AI_GOAL_MILITARY) {
      continue;
    }
    const int d = abs((int)s->x - from_x) + abs((int)s->y - from_y);
    if (best < 0 || d < best) {
      best = d;
      bx = (int)s->x;
      by = (int)s->y;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * CONTACT scout ring (unpark #4): nearest tribe beyond adjacent from
 * (from_x,from_y) → land tile in Manhattan ring 2..4 around tribe.
 * FoW deepen: when map.seen exists, prefer tiles NOT seen by this nation
 * (map_tile_seen_by / Col1 fog bit) — explore CONTACT, not combat bonus.
 * Sticky deepen: ai_diplo_indian_hostility_sticky ≥ 2 (unknown26[8] very-low)
 * → prefer closer rings when fog absent. Sticky + FoW: prefer deeper unseen
 * ring (md=4) to push fog outward. Cite: euro_diplo.md / ai_diplo.h; manual fog.
 * Fall back to toward-scout / tighter-ring scoring when fog absent or all seen.
 * No beyond-adjacent tribe / no ring tile: return 0 (fog-explore MD≤8 instead).
 */
static int ai_euro_scout_contact_ring_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || ctx->col1->head.tribe_count == 0 ||
      from_x < 0 || from_y < 0 || !ctx->map) {
    return 0;
  }
  const uint8_t sticky = ai_diplo_indian_hostility_sticky(ctx->col1, nation_id);
  /* sticky≥2 without FoW → weight ring radius so md=2 beats md=4. */
  const int md_w = (sticky >= 2) ? 50 : 1;

  int best_tribe_d = -1;
  int tribe_x = 0;
  int tribe_y = 0;
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    const int tx = (int)t->x;
    const int ty = (int)t->y;
    const int d = abs(tx - from_x) + abs(ty - from_y);
    if (d <= 1) {
      continue; /* already adjacent — no scout ring */
    }
    if (best_tribe_d < 0 || d < best_tribe_d) {
      best_tribe_d = d;
      tribe_x = tx;
      tribe_y = ty;
    }
  }
  if (best_tribe_d <= 1) {
    return 0;
  }

  const int use_fog = ctx->map->seen != NULL;
  /* Sticky CONTACT + FoW API → deepen into unseen outer ring. */
  const int sticky_fog_deepen = sticky >= 2 && use_fog;
  int best_score = -1;
  int bx = 0;
  int by = 0;
  for (int dy = -4; dy <= 4; ++dy) {
    for (int dx = -4; dx <= 4; ++dx) {
      const int md = abs(dx) + abs(dy);
      if (md < 2 || md > 4) {
        continue;
      }
      const int nx = tribe_x + dx;
      const int ny = tribe_y + dy;
      if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
        continue;
      }
      if (map_tile_is_water(ctx->map, nx, ny)) {
        continue;
      }
      if (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0) {
        continue;
      }
      /*
       * FoW: unseen tiles score first (explore CONTACT). Sticky+fog: among
       * unseen prefer deeper ring (md=4). Else sticky prefers tighter ring
       * (md=2). Cite Col1 seen bit / map_tile_seen_by — not combat bonuses.
       */
      const int unseen =
        use_fog && !map_tile_seen_by(ctx->map, nx, ny, nation_id) ? 0 : 1;
      const int to_scout = abs(nx - from_x) + abs(ny - from_y);
      int score;
      if (sticky_fog_deepen) {
        /* unseen first; then deeper ring when unseen (4-md); seen fall back closer. */
        const int depth = (unseen == 0) ? (4 - md) : md;
        score = unseen * 1000 + depth * 50 + to_scout * 10;
      } else {
        score = unseen * 1000 + to_scout * 10 + md * md_w;
      }
      if (best_score < 0 || score < best_score) {
        best_score = score;
        bx = nx;
        by = ny;
      }
    }
  }
  if (best_score < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Fog explore (no CONTACT): peaceful Scout without a CONTACT ring goal →
 * unseen land tile within Manhattan distance 8 (map_tile_seen_by / Col1 FoW).
 * Prefer map_tile_has_rumour tiles over plain unseen when both exist (Scout
 * seek Lost City Rumours; LCR resolve already on stand — no invented gold/FoY).
 * Plain Scout: nearest within the preferred tier (min md). Seasoned Scout
 * (prefer_deeper): farthest within that tier (max md ≤8) — AI explore
 * preference for the skill that is "Better at exploring rumors…"
 * (Colonization.pdf OTHER / Seasoned Scout). Scouts already see 2 squares
 * (de Soto text: all units → "as well as scouts"); do NOT invent extra sight
 * radius or MP — only deepen fog-target pick. Cite: Colonization.pdf Lost City
 * Rumours / Seasoned Scout; Pass5 LCR scaffold; manual fog / map.seen;
 * euro_unit_act explore.
 */
static int ai_euro_scout_fog_explore_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int prefer_deeper,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->map || !ctx->map->seen || !out_x || !out_y || nation_id < 0 ||
      nation_id >= 4 || from_x < 0 || from_y < 0) {
    return 0;
  }
  int best_md = -1;
  int best_rumour = 0;
  int bx = 0;
  int by = 0;
  for (int dy = -8; dy <= 8; ++dy) {
    for (int dx = -8; dx <= 8; ++dx) {
      const int md = abs(dx) + abs(dy);
      if (md < 1 || md > 8) {
        continue;
      }
      const int nx = from_x + dx;
      const int ny = from_y + dy;
      if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
        continue;
      }
      if (map_tile_is_water(ctx->map, nx, ny)) {
        continue;
      }
      if (map_tile_seen_by(ctx->map, nx, ny, nation_id)) {
        continue;
      }
      if (ctx->colonies && colonies_id_at(ctx->colonies, nx, ny) >= 0) {
        continue;
      }
      const int rum = map_tile_has_rumour(ctx->map, nx, ny) ? 1 : 0;
      int better = 0;
      if (best_md < 0) {
        better = 1;
      } else if (rum && !best_rumour) {
        /* Rumour beats plain unseen within MD≤8. */
        better = 1;
      } else if (rum == best_rumour) {
        if (prefer_deeper) {
          /* Seasoned: deeper fog first within the same rumour/plain tier. */
          better = (md > best_md);
        } else {
          better = (md < best_md);
        }
      }
      if (better) {
        best_md = md;
        best_rumour = rum;
        bx = nx;
        by = ny;
      }
    }
  }
  if (best_md < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/* Seasoned Scout display-name / profession stand-in (UNITS_JOB_SCOUT). */
static int ai_euro_is_seasoned_scout_name(const char* name) {
  return name && strstr(name, "Seasoned") != NULL && strstr(name, "Scout") != NULL;
}

/* Treasure train — display-name stand-in (manual Treasure Trains). */
static int ai_euro_is_treasure_name(const char* name) {
  return name && strstr(name, "Treasure") != NULL;
}

/*
 * FUN_4720_049e Treasure Train tension bump (thin — 2026-08-15 find, bit
 * semantics confirmed same day via a `153e` cross-check, see `ai_diplo.h`).
 * DOS: when a Treasure Train's own move ends adjacent to a foreign unit,
 * sets `nation[foreign].euro_relation[mover] |= 0x80` (the *other*
 * nation's opinion of the treasure-carrying nation — "hauling a fortune
 * near a rival makes them suspicious/covetous", not "you saw their
 * treasure"), then an RNG roll scaled by difficulty compares
 * `land_combat_strength[]` between the two nations: weaker rival →
 * `AI_DIPLO_PEACE` (confirmed real DOS bit 2, not a Linux stand-in —
 * "a weaker power responds to a wealthy/strong rival by seeking peace",
 * mirrors this port's own "unmet defaults to PEACE|MET" convention),
 * stronger rival → `AI_DIPLO_TREASURE_STRONGER` (DOS's real bit 8, still
 * unconfirmed meaning beyond "not peace"). `-0x6be4` (the DOS table the
 * RNG branch reads) resolved to `land_combat_strength[4]`, already live
 * in `col1_stuff_census.c` — no invented data.
 *
 * Own addition, not DOS-derived: skip if the pair is already at war —
 * DOS ORs this bit in unconditionally, but doing that over an active WAR
 * bit would leave an internally inconsistent relation byte in this port
 * (WAR still wins functionally via `ai_diplo_at_war`, but the byte itself
 * would look self-contradictory); safer to just not fire there.
 */
static void ai_euro_treasure_tension_bump(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !u || !u->active ||
      u->nation_id < 0 || u->nation_id >= 4 ||
      !ai_euro_is_treasure_name(units_display_name(ctx->units, u))) {
    return;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe < 0 || units_is_sea(ctx->units, foe)) {
      continue;
    }
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f || f->nation_id == u->nation_id || f->nation_id < 0 || f->nation_id >= 4) {
      continue;
    }
    if (ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      continue;
    }
    const uint8_t before = ai_diplo_read(ctx->col1, f->nation_id, u->nation_id);
    ai_diplo_write(ctx->col1, f->nation_id, u->nation_id, (uint8_t)(before | AI_DIPLO_TREASURE_ALERT));
    if (ctx->rng && dos_rng_range(ctx->rng, 0, 99) < (int)ctx->col1->head.difficulty + 1) {
      const int their_strength = ctx->col1->stuff.land_combat_strength[f->nation_id];
      const int our_strength = ctx->col1->stuff.land_combat_strength[u->nation_id];
      const uint8_t follow =
        (their_strength < our_strength) ? AI_DIPLO_PEACE : AI_DIPLO_TREASURE_STRONGER;
      const uint8_t cur = ai_diplo_read(ctx->col1, f->nation_id, u->nation_id);
      ai_diplo_write(ctx->col1, f->nation_id, u->nation_id, (uint8_t)(cur | follow));
    }
    return; /* DOS fires once per act, first foreign neighbor found */
  }
}

/*
 * Europe-sail target for a ship carrying Treasure (Colonization.pdf Treasure
 * Trains — Galleon / coastal colony → Europe). Prefer eastern high seas
 * (units_find_eastern_high_seas_tile — Atlantic→Europe exit). Else nearest
 * water tile with higher x (eastward Europe stand-in). No invented gold.
 */
static int ai_euro_europe_sail_target(
  ColonizeTurnContext* ctx,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->map || !ctx->units || !out_x || !out_y) {
    return 0;
  }
  int hx = 0;
  int hy = 0;
  if (units_find_eastern_high_seas_tile(ctx->units, ctx->map, from_y, &hx, &hy)) {
    *out_x = hx;
    *out_y = hy;
    return 1;
  }
  /* No HS on map — eastward water stand-in (Europe edge direction). */
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int y = 0; y < ctx->map->height; ++y) {
    for (int x = 0; x < ctx->map->width; ++x) {
      if (!map_tile_is_water(ctx->map, x, y)) {
        continue;
      }
      if (x <= from_x) {
        continue;
      }
      const int d = abs(x - from_x) + abs(y - from_y);
      /* Prefer farther east, then nearer in y. */
      const int score = (ctx->map->width - x) * 1000 + d;
      if (best < 0 || score < best) {
        best = score;
        bx = x;
        by = y;
      }
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Treasure train coast target (Colonization.pdf Treasure Trains): move to a
 * coastal own colony so a Galleon / king transport can sail it to Europe.
 * Prefer nearest own coastal colony; if none, nearest coastal land tile
 * (Europe sail path stand-in — AI_MOVE to coast). Cite: manual p.76 —
 * park treasure in coastal colony; Galleon six-hold / king galleon for a price.
 * No invented gold/ransom rates.
 */
static int ai_euro_treasure_coast_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->map || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
        continue;
      }
      const int d = abs(c->x - from_x) + abs(c->y - from_y);
      if (best < 0 || d < best) {
        best = d;
        bx = c->x;
        by = c->y;
      }
    }
  }
  if (best >= 0) {
    *out_x = bx;
    *out_y = by;
    return 1;
  }
  /* No coastal colony — AI_MOVE toward nearest coastal land (sail staging). */
  best = -1;
  for (int y = 0; y < ctx->map->height; ++y) {
    for (int x = 0; x < ctx->map->width; ++x) {
      if (!map_tile_is_coastal(ctx->map, x, y)) {
        continue;
      }
      if (ctx->colonies && colonies_id_at(ctx->colonies, x, y) >= 0) {
        continue; /* foreign/other colony tile — skip */
      }
      const int d = abs(x - from_x) + abs(y - from_y);
      if (d < 1) {
        continue;
      }
      if (best < 0 || d < best) {
        best = d;
        bx = x;
        by = y;
      }
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/* Missionary / Jesuit Missionary — name substring (ai_contact / NAMES). */
static int ai_euro_is_missionary_name(const char* name) {
  return name &&
         (strstr(name, "Missionary") != NULL || strstr(name, "Jesuit") != NULL);
}

/*
 * Missionary flee gate (same ≥55 refuse-talk / Alarm band as ai_contact flee):
 * adjacent tribe with indian alarm_by_player or tribe friction ≥55 → fleeing,
 * do not upsert CONTACT (leave ai_contact_missionary_flee). Cite: fandom Alarm;
 * Colonization.pdf Missionary Powers / Alarm.
 */
static int ai_euro_missionary_should_flee(
  ColonizeTurnContext* ctx,
  int nation_id,
  int x,
  int y
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || nation_id < 0 ||
      nation_id >= 4) {
    return 0;
  }
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if (abs((int)t->x - x) > 1 || abs((int)t->y - y) > 1) {
      continue;
    }
    if (t->alarm[nation_id].friction >= 55) {
      return 1;
    }
    const int ind = (int)t->nation_id;
    if (ind >= 4 && ind <= 11) {
      const ColonizeCol1Indian* indian = &ctx->col1->indian[ind - 4];
      if (indian->alarm_by_player[nation_id] >= 55) {
        return 1;
      }
    }
  }
  return 0;
}

/*
 * Peace Missionary CONTACT: nearest tribe with no mission (mission==0xff).
 * Goto tribe tile — adjacent convert pulse lives in ai_contact. Cite:
 * Colonization.pdf Establishing a Mission; indian_contact.md convert pulse.
 */
static int ai_euro_missionary_no_mission_target(
  ColonizeTurnContext* ctx,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !out_x || !out_y || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe ||
      ctx->col1->head.tribe_count == 0) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if (t->mission != 0xff) {
      continue; /* already has a mission (own or foreign) */
    }
    const int d = abs((int)t->x - from_x) + abs((int)t->y - from_y);
    if (best < 0 || d < best) {
      best = d;
      bx = (int)t->x;
      by = (int)t->y;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);

/*
 * Forest surround tile (pedia 8–23) free for Lumberjack field work.
 * Cite: docs/terrain_yields.md Lumberjack; map_pedia_terrain_index_at forests.
 * 1 if a free field tile index is written to *out_ti.
 */
static int ai_euro_colony_free_forest_field(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int* out_ti
) {
  if (!ctx || !ctx->map || !c || !c->active || !out_ti) {
    return 0;
  }
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    if (c->tiles[ti] >= 0) {
      continue; /* occupied */
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = c->x + dx;
    const int ty = c->y + dy;
    const int pedia = map_pedia_terrain_index_at(ctx->map, tx, ty);
    if (pedia < 8 || pedia > 23) {
      continue; /* not forest / scrub */
    }
    if (colony_yield_for_tile(ctx->map, tx, ty, COLONIZE_JOB_LUMBERJACK) <= 0) {
      continue;
    }
    *out_ti = ti;
    return 1;
  }
  return 0;
}

/*
 * Expert Lumberjack → admit + colonies_assign_field onto a forest surround
 * of an own colony. Cite: docs/terrain_yields.md / building_production
 * Lumberjack→Lumber; Colonization.pdf Skills Chart / lumberjack timber;
 * colonies_assign_field (colony UI / scripted ai.c). No invented lumber rates.
 * On-tile: admit then assign. Off-tile MD≤8: AI_MOVE toward colony (1).
 * Returns 1 if routed or assigned.
 */
static int ai_euro_try_lumberjack_field_assign(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, u);
  if (!name || strstr(name, "Lumberjack") == NULL) {
    return 0;
  }
  if (ai_euro_land_is_fortified(u)) {
    return 0;
  }
  int best_d = 99;
  int bx = -1;
  int by = -1;
  int best_cid = -1;
  int best_ti = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->colonist_count >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    int ti = -1;
    if (!ai_euro_colony_free_forest_field(ctx, c, &ti)) {
      continue;
    }
    const int dist = abs(c->x - u->x) + abs(c->y - u->y);
    if (dist > 8) {
      continue;
    }
    if (bx < 0 || dist < best_d) {
      best_d = dist;
      bx = c->x;
      by = c->y;
      best_cid = i;
      best_ti = ti;
    }
  }
  if (best_cid < 0 || best_ti < 0) {
    return 0;
  }
  if (u->x == bx && u->y == by) {
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
    if (idx < 0) {
      return 0;
    }
    if (!colonies_assign_field(
          ctx->colonies, best_cid, idx, best_ti, COLONIZE_JOB_LUMBERJACK)) {
      return 1; /* admitted; field assign failed — still consumed unit */
    }
    return 1;
  }
  ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_LABOR, 4);
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, bx, by);
  return 1;
}

/*
 * Free surround tile with positive yield for Ore/Silver Miner field work.
 * Cite: docs/terrain_yields.md Ore Miner / Silver Miner (hills/mountains).
 * 1 if a free field tile index is written to *out_ti.
 */
static int ai_euro_colony_free_miner_field(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int field_job,
  int* out_ti
) {
  if (!ctx || !ctx->map || !c || !c->active || !out_ti) {
    return 0;
  }
  if (field_job != COLONIZE_JOB_ORE_MINER && field_job != COLONIZE_JOB_SILVER_MINER) {
    return 0;
  }
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    if (c->tiles[ti] >= 0) {
      continue; /* occupied */
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = c->x + dx;
    const int ty = c->y + dy;
    if (colony_yield_for_tile(ctx->map, tx, ty, field_job) <= 0) {
      continue;
    }
    *out_ti = ti;
    return 1;
  }
  return 0;
}

/*
 * Expert Ore Miner / Silver Miner → admit + colonies_assign_field on a free
 * yield surround (parallel to Expert Lumberjack forest field-assign).
 * Cite: docs/terrain_yields.md Ore/Silver; Colonization.pdf Skills Chart;
 * colonies_assign_field. No invented ore/silver rates.
 * On-tile: admit then assign. Off-tile MD≤8: AI_MOVE toward colony (1).
 * Returns 1 if routed or assigned.
 */
static int ai_euro_try_miner_field_assign(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, u);
  if (!name) {
    return 0;
  }
  int field_job = -1;
  if (strstr(name, "Silver Miner") != NULL) {
    field_job = COLONIZE_JOB_SILVER_MINER;
  } else if (strstr(name, "Ore Miner") != NULL) {
    field_job = COLONIZE_JOB_ORE_MINER;
  } else {
    return 0;
  }
  if (ai_euro_land_is_fortified(u)) {
    return 0;
  }
  int best_d = 99;
  int bx = -1;
  int by = -1;
  int best_cid = -1;
  int best_ti = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->colonist_count >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    int ti = -1;
    if (!ai_euro_colony_free_miner_field(ctx, c, field_job, &ti)) {
      continue;
    }
    const int dist = abs(c->x - u->x) + abs(c->y - u->y);
    if (dist > 8) {
      continue;
    }
    if (bx < 0 || dist < best_d) {
      best_d = dist;
      bx = c->x;
      by = c->y;
      best_cid = i;
      best_ti = ti;
    }
  }
  if (best_cid < 0 || best_ti < 0) {
    return 0;
  }
  if (u->x == bx && u->y == by) {
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
    if (idx < 0) {
      return 0;
    }
    if (!colonies_assign_field(ctx->colonies, best_cid, idx, best_ti, field_job)) {
      return 1; /* admitted; field assign failed — still consumed unit */
    }
    return 1;
  }
  ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_LABOR, 4);
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, bx, by);
  return 1;
}

/* Headcount of active colonists currently doing `field_job` in `col` —
 * substitute for DOS's `colony+0x9a+job*2` per-job counter array (that
 * table isn't itself modeled in ColonizeColony; this walks the colonist
 * list instead, same result). Used by ai_euro_28c8_colonist_job_score_structural. */
static int ai_euro_28c8_job_headcount(const ColonizeColony* col, int field_job) {
  int n = 0;
  for (int i = 0; i < col->colonist_count; ++i) {
    if (col->colonists[i].active && col->colonists[i].field_job == field_job) {
      ++n;
    }
  }
  return n;
}

/*
 * FUN_15eb_28c8 — colonist work-plot job scoring, structural reference port
 * (2026-08-22, docs/port_plan.md T1.17). RE is complete — see
 * original_sources_annotated/turn/colonist_work_plot_28c8.md. **2026-08-24
 * (W1.7):** a golden fixture now verifies the 9-job weighted formula —
 * tests/unit/test_ai_euro_28c8_job_score.c — and caught a real discrepancy
 * against the doc, fixed here (see the labor/travel-penalty note below).
 * Still deliberately NOT wired into any live AI path — that's Tier 3
 * (docs/port_plan.md W3.1), a user-confirmed default-behavior change, not
 * this pass's scope (same "document/verify, don't silently ship"
 * precedent as T1.9/T1.15). External linkage (declared in ai_euro.h) so
 * the fixture can call it directly.
 *
 * Covers only the tier-2/8-tile case: colony.h's own
 * COLONIZE_COLONY_FIELD_TILES==8 already matches DOS's default (Town-Hall-
 * level-1) tile count byte-for-byte (see the doc's Fidelity section) — no
 * struct change needed for the common case. Town-Hall-level-2/3 outer-ring
 * tiles (12/20, DS:0x329-gated) have no storage in ColonizeColony yet and
 * stay deliberately out of scope. Only scores field jobs 0..8
 * (COLONIZE_FIELD_JOB_COUNT) — building-job assignment (DOS job >=0xd) and
 * the human single-job-probe / early-shortcut gate (`param_2`, the
 * ambiguous `bVar2` population-near-cap short-circuit) are not modeled;
 * this is the AI full-search branch's core loop only.
 *
 * Real, resolved terms scored: field yield (colony_yield_for_tile — the
 * same worker-context-free substitute this file's other job-assign helpers
 * already use, per the doc's own "local_24 vs local_34" note), the
 * labor/travel terrain penalty (map_dos_terr_labor_penalty_byte, this
 * session's own T1.17 addition to map.c) — per the doc's Structure §5,
 * this is an AI-full-search-branch term scoped to jobs 0 (Farmer) and 8
 * (Fisherman) only ("generalist" slots), NOT a universal base-score term;
 * the original port applied it to every job unconditionally, which the
 * golden fixture caught and this pass fixed — the population-cap-vs-
 * headroom clamp (warehouse_level doubles as DOS's population cap per
 * FUN_15eb_0a50 — already named/cited in colony.h), and the current-job
 * sticky-preference doubling.
 *
 * Deliberately left at 0 / unimplemented — real but not independently
 * pinned down, would be guessing to fill in (see the doc's "Remaining
 * genuinely open terms"): the dx/dy "distance-ish" term, the
 * continent×nation military-development danger term (a real accessor
 * exists, ai_euro_continent_stance_at, but the doc's exact combination
 * with this term wasn't nailed down), the per-job RNG/wealth-rank boost
 * for established colonies, the per-(nation,job) throttle table (a
 * *different* table from 2820's own DS:0x84BC cargo throttle — not
 * captured for this call site), the "senior profession tier"/"unhappy
 * colony" AI-search-gate short-circuit (colony+0x94, no Linux field named
 * yet), and the first-work hidden-resource discovery roll
 * (FUN_281f_0d78/_0d6c — parked, self-contained, doc's own note).
 */
/* Coastal or Docks-family building — turn.c's Fisherman gate, same shape. */
static bool ai_euro_colony_has_docks(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColony* col
) {
  if ((col->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) != 0 ||
      map_tile_is_coastal(map, col->x, col->y)) {
    return true;
  }
  for (int bi = 0; bi < pool->building_type_count && bi < COLONIZE_BUILDING_TYPES_MAX; ++bi) {
    if (!col->has_building[bi]) {
      continue;
    }
    const char* bn = pool->building_types[bi].name;
    if (bn && (strstr(bn, "Docks") != NULL || strstr(bn, "Drydock") != NULL ||
               strstr(bn, "Shipyard") != NULL)) {
      return true;
    }
  }
  return false;
}

/*
 * 28c8 scorer body. `profession` < 0 scores plain tile yields (the
 * structural/test entry point); otherwise the colonist's real profession
 * goes through colony_yield_for_worker (DOS 1068 trial-assigns the job, so
 * 18ec sees the expert) — that is what the live tick uses.
 */
static int ai_euro_28c8_score(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* col,
  int colonist_slot,
  int profession,
  AiEuro28c8JobCandidate* out_best
) {
  const ColonizeColonist* self = &col->colonists[colonist_slot];
  if (!self->active) {
    return 0;
  }
  const int current_job = self->field_job; /* -1 if not currently field-working */
  const int pop_cap = col->warehouse_level == 0 ? 100 : ((int)col->warehouse_level + 1) * 100;
  const int is_ai = col->nation_id != ctx->human_nation;
  const bool has_docks =
    profession >= 0 ? ai_euro_colony_has_docks(ctx->colonies, ctx->map, col) : true;
  const int sol_b_field =
    profession >= 0 ? colony_prod_sol_bonus_field(ctx->col1_ok ? ctx->col1 : NULL, col) : 0;

  out_best->job = -1;
  out_best->tile = -1;
  out_best->score = -1000000;
  out_best->yield = 0;

  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    if (col->tiles[ti] >= 0 && col->tiles[ti] != colonist_slot) {
      continue; /* worked by a different colonist already */
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = col->x + dx;
    const int ty = col->y + dy;
    const int terr = map_dos_terr_class_at(ctx->map, tx, ty);
    const int penalty = map_dos_terr_labor_penalty_byte(terr);
    for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
      int yld = profession >= 0
                  ? colony_yield_for_worker(
                      ctx->map, tx, ty, job, profession, has_docks, sol_b_field,
                      col->colony_flags
                    )
                  : colony_yield_for_tile(ctx->map, tx, ty, job);
      if (yld <= 0) {
        continue;
      }
      const int raw_yield = yld;
      if (is_ai) {
        /* AI-only headroom clamp: colony[0x9a+job*2], floor 1 (doc's own
         * reading of the raw asm). */
        int headroom = pop_cap - ai_euro_28c8_job_headcount(col, job);
        if (headroom < 1) {
          headroom = 1;
        }
        if (yld > headroom) {
          yld = headroom;
        }
      }
      int score = yld * 8;
      /* Doc's own Structure §5: the DS:0x2f76+4 labor/travel penalty is an
       * AI-full-search-branch term scoped to jobs 0/8 only ("generalist"
       * Farmer/Fisherman slots) — NOT a universal base-score term. Fixed
       * 2026-08-24 (W1.7 verification): the port previously subtracted it
       * from every job's base score, which the doc's own derivation doesn't
       * support (a golden fixture caught the resulting best-pick flip). */
      if (is_ai && (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN)) {
        score -= penalty;
      }
      if (job == current_job) {
        score *= 2; /* sticky preference for the colonist's current job */
      }
      if (score > out_best->score) {
        out_best->score = score;
        out_best->job = job;
        out_best->tile = ti;
        out_best->yield = raw_yield;
      }
    }
  }
  return out_best->job >= 0 ? 1 : 0;
}

int ai_euro_28c8_colonist_job_score_structural(
  const ColonizeTurnContext* ctx,
  int colony_id,
  int colonist_slot,
  AiEuro28c8JobCandidate* out_best
) {
  if (!ctx || !ctx->colonies || !ctx->map || !out_best) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(ctx->colonies, colony_id);
  if (!col || !col->active || colonist_slot < 0 || colonist_slot >= col->colonist_count) {
    return 0;
  }
  return ai_euro_28c8_score(ctx, col, colonist_slot, -1, out_best);
}

/*
 * FUN_5952_035e colonist placement block (viceroy_unpacked.c ~94560-94640),
 * the AI-turn caller of 28c8 (via resident stub FUN_281f_0b6e). Per AI
 * colony each turn DOS clears every work plot (`colony+0x70..0x83 = 0xff`)
 * and re-places colonists through 28c8: a food pass first (slots whose
 * previous job was Farmer, or Fisherman on a fishable colony) until the
 * food target holds, then two general passes; a winner whose raw yield
 * (DS:0x8dbe) is < 3 ends the pass. Building workers keep their
 * workplaces here — DOS's later statesman/carpenter passes in the same
 * function are the existing expert-workplace heuristics' territory.
 * Food target: Linux population×2 consumption vs town commons + placed food.
 */
static void ai_euro_colony_tick_28c8_reassign(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id == ctx->human_nation) {
    return;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* col = &ctx->colonies->colonies[ci];
    if (!col->active || col->nation_id != nation_id || col->colonist_count <= 0) {
      continue;
    }
    int prev_job[COLONIZE_COLONY_POP_MAX];
    bool placed[COLONIZE_COLONY_POP_MAX];
    const int n = col->colonist_count < COLONIZE_COLONY_POP_MAX ? col->colonist_count
                                                                : COLONIZE_COLONY_POP_MAX;
    for (int s = 0; s < n; ++s) {
      const ColonizeColonist* c = &col->colonists[s];
      prev_job[s] = c->field_job;
      /* Building workers and inactive slots are out of scope this tick. */
      placed[s] = !c->active || c->building_type >= 0;
    }
    for (int s = 0; s < n; ++s) {
      if (placed[s]) {
        continue;
      }
      const int ti = colonies_colonist_tile(col, s);
      if (ti >= 0) {
        colonies_clear_field(ctx->colonies, col->id, ti);
      }
      col->colonists[s].field_job = -1;
    }

    const bool fishable = ai_euro_colony_has_docks(ctx->colonies, ctx->map, col);
    int food_have = 0;
    {
      ColonizeTownCommonsYield tc;
      colony_yield_town_commons(
        ctx->map, col->x, col->y,
        colony_prod_sol_bonus_field(ctx->col1_ok ? ctx->col1 : NULL, col), col->colony_flags,
        ctx->col1_ok && ctx->col1 ? (int)ctx->col1->head.difficulty : 4, &tc
      );
      food_have = tc.food > 0 ? tc.food : 0;
    }
    const int food_need = col->population * 2;

    /* Pass 1 — food, from the slots that were feeding the colony. */
    for (int s = 0; s < n && food_have < food_need; ++s) {
      if (placed[s]) {
        continue;
      }
      const bool was_food = prev_job[s] == COLONIZE_JOB_FARMER ||
                            (prev_job[s] == COLONIZE_JOB_FISHERMAN && fishable);
      if (!was_food) {
        continue;
      }
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = prev_job[s]; /* sticky ×2 on the old job */
      const int ok = ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &best);
      col->colonists[s].field_job = -1;
      if (!ok || best.yield < 3) {
        break;
      }
      if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
        placed[s] = true;
        if (best.job == COLONIZE_JOB_FARMER || best.job == COLONIZE_JOB_FISHERMAN) {
          food_have += best.yield;
        }
      }
    }

    /* Pass 2 ×2 — everyone else, best job wins, yield < 3 ends the pass. */
    for (int pass = 0; pass < 2; ++pass) {
      for (int s = 0; s < n; ++s) {
        if (placed[s]) {
          continue;
        }
        AiEuro28c8JobCandidate best;
        col->colonists[s].field_job = prev_job[s];
        const int ok = ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &best);
        col->colonists[s].field_job = -1;
        if (!ok || best.yield < 3) {
          break;
        }
        if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
          placed[s] = true;
        }
      }
    }

    /* Leftovers: DOS hands them to its building passes; keep what they had. */
    for (int s = 0; s < n; ++s) {
      if (placed[s] || prev_job[s] < 0) {
        continue;
      }
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = prev_job[s];
      const int ok = ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &best);
      col->colonists[s].field_job = -1;
      if (ok) {
        (void)colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job);
      }
    }
  }
}

/*
 * Free surround tile with positive Farmer food yield. Prefer higher yield
 * (plow/river already fold into colony_yield_for_tile). Cite:
 * docs/terrain_yields.md Farmer; Colonization.pdf Skills Chart / plow +1 food.
 * 1 if a free field tile index is written to *out_ti.
 */
static int ai_euro_colony_free_farmer_field(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int* out_ti
) {
  if (!ctx || !ctx->map || !c || !c->active || !out_ti) {
    return 0;
  }
  int best_ti = -1;
  int best_y = 0;
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    if (c->tiles[ti] >= 0) {
      continue; /* occupied */
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = c->x + dx;
    const int ty = c->y + dy;
    const int yld = colony_yield_for_tile(ctx->map, tx, ty, COLONIZE_JOB_FARMER);
    if (yld <= 0) {
      continue;
    }
    if (best_ti < 0 || yld > best_y) {
      best_ti = ti;
      best_y = yld;
    }
  }
  if (best_ti < 0) {
    return 0;
  }
  *out_ti = best_ti;
  return 1;
}

/*
 * Expert Farmer → admit + colonies_assign_field on a free food surround
 * (parallel to Expert Lumberjack / Ore Miner field-assign). Cite:
 * docs/terrain_yields.md Farmer; docs/building_production.md Farmer→Food;
 * Colonization.pdf Skills Chart; colonies_assign_field. No invented food rates.
 * Display-name Farmer, or Free Colonist/Colonist with @JOB Farmer (profession 0).
 * On-tile: admit then assign. Off-tile MD≤8: AI_MOVE toward colony (1).
 * Returns 1 if routed or assigned.
 */
static int ai_euro_try_farmer_field_assign(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, u);
  if (!name) {
    return 0;
  }
  const int is_named_farmer = strstr(name, "Farmer") != NULL;
  const int is_job_farmer =
    u->profession == 0 &&
    (strstr(name, "Free Colonist") != NULL || strstr(name, "Colonist") != NULL) &&
    strstr(name, "Soldier") == NULL;
  if (!is_named_farmer && !is_job_farmer) {
    return 0;
  }
  if (ai_euro_land_is_fortified(u)) {
    return 0;
  }
  int best_d = 99;
  int bx = -1;
  int by = -1;
  int best_cid = -1;
  int best_ti = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->colonist_count >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    int ti = -1;
    if (!ai_euro_colony_free_farmer_field(ctx, c, &ti)) {
      continue;
    }
    const int dist = abs(c->x - u->x) + abs(c->y - u->y);
    if (dist > 8) {
      continue;
    }
    if (bx < 0 || dist < best_d) {
      best_d = dist;
      bx = c->x;
      by = c->y;
      best_cid = i;
      best_ti = ti;
    }
  }
  if (best_cid < 0 || best_ti < 0) {
    return 0;
  }
  if (u->x == bx && u->y == by) {
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
    if (idx < 0) {
      return 0;
    }
    if (!colonies_assign_field(
          ctx->colonies, best_cid, idx, best_ti, COLONIZE_JOB_FARMER)) {
      return 1; /* admitted; field assign failed — still consumed unit */
    }
    return 1;
  }
  ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_LABOR, 4);
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, bx, by);
  return 1;
}

/*
 * Free surround ocean/sea-lane tile with positive Fisherman food yield.
 * Cite: docs/terrain_yields.md Fisherman (Ocean/Sea Lane fish=3); @OTHER pedia
 * 25–26; Colonization.pdf Skills Chart / Expert Fisherman. 1 if *out_ti set.
 */
static int ai_euro_colony_free_fisherman_field(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int* out_ti
) {
  if (!ctx || !ctx->map || !c || !c->active || !out_ti) {
    return 0;
  }
  int best_ti = -1;
  int best_y = 0;
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    if (c->tiles[ti] >= 0) {
      continue; /* occupied */
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = c->x + dx;
    const int ty = c->y + dy;
    const int pedia = map_pedia_terrain_index_at(ctx->map, tx, ty);
    if (pedia != 25 && pedia != 26) {
      continue; /* ocean / sea lane only — coastal fish tile */
    }
    const int yld = colony_yield_for_tile(ctx->map, tx, ty, COLONIZE_JOB_FISHERMAN);
    if (yld <= 0) {
      continue;
    }
    if (best_ti < 0 || yld > best_y) {
      best_ti = ti;
      best_y = yld;
    }
  }
  if (best_ti < 0) {
    return 0;
  }
  *out_ti = best_ti;
  return 1;
}

/*
 * Expert Fisherman → admit + colonies_assign_field on a free ocean/sea-lane
 * surround (parallel to Expert Farmer food field-assign). Cite:
 * docs/terrain_yields.md Fisherman; docs/building_production.md Fisherman→Food;
 * Colonization.pdf Skills Chart; colonies_assign_field. No invented fish rates.
 * Display-name Fisherman, or Free Colonist/Colonist with @JOB Fisherman (8).
 * On-tile: admit then assign. Off-tile MD≤8: AI_MOVE toward colony (1).
 * Returns 1 if routed or assigned.
 */
static int ai_euro_try_fisherman_field_assign(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, u);
  if (!name) {
    return 0;
  }
  const int is_named_fisherman = strstr(name, "Fisherman") != NULL;
  const int is_job_fisherman =
    u->profession == COLONIZE_JOB_FISHERMAN &&
    (strstr(name, "Free Colonist") != NULL || strstr(name, "Colonist") != NULL) &&
    strstr(name, "Soldier") == NULL;
  if (!is_named_fisherman && !is_job_fisherman) {
    return 0;
  }
  if (ai_euro_land_is_fortified(u)) {
    return 0;
  }
  int best_d = 99;
  int bx = -1;
  int by = -1;
  int best_cid = -1;
  int best_ti = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->colonist_count >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    int ti = -1;
    if (!ai_euro_colony_free_fisherman_field(ctx, c, &ti)) {
      continue;
    }
    const int dist = abs(c->x - u->x) + abs(c->y - u->y);
    if (dist > 8) {
      continue;
    }
    if (bx < 0 || dist < best_d) {
      best_d = dist;
      bx = c->x;
      by = c->y;
      best_cid = i;
      best_ti = ti;
    }
  }
  if (best_cid < 0 || best_ti < 0) {
    return 0;
  }
  if (u->x == bx && u->y == by) {
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
    if (idx < 0) {
      return 0;
    }
    if (!colonies_assign_field(
          ctx->colonies, best_cid, idx, best_ti, COLONIZE_JOB_FISHERMAN)) {
      return 1; /* admitted; field assign failed — still consumed unit */
    }
    return 1;
  }
  ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_LABOR, 4);
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, bx, by);
  return 1;
}

/*
 * Free surround tile with positive Sugar/Tobacco/Cotton Planter or Fur Trapper
 * yield. Prefer higher yield (plow/river fold into colony_yield_for_tile). Cite:
 * docs/terrain_yields.md Sugar (Savannah/Swamp) / Tobacco (Grassland/Marsh) /
 * Cotton (Prairie/Plains) / Fur Trapper (forested); Colonization.pdf Skills
 * Chart. 1 if *out_ti set.
 */
static int ai_euro_colony_free_planter_field(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int field_job,
  int* out_ti
) {
  if (!ctx || !ctx->map || !c || !c->active || !out_ti) {
    return 0;
  }
  if (field_job != COLONIZE_JOB_SUGAR_PLANTER &&
      field_job != COLONIZE_JOB_TOBACCO_PLANTER &&
      field_job != COLONIZE_JOB_COTTON_PLANTER &&
      field_job != COLONIZE_JOB_FUR_TRAPPER) {
    return 0;
  }
  int best_ti = -1;
  int best_y = 0;
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    if (c->tiles[ti] >= 0) {
      continue; /* occupied */
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = c->x + dx;
    const int ty = c->y + dy;
    const int yld = colony_yield_for_tile(ctx->map, tx, ty, field_job);
    if (yld <= 0) {
      continue;
    }
    if (best_ti < 0 || yld > best_y) {
      best_ti = ti;
      best_y = yld;
    }
  }
  if (best_ti < 0) {
    return 0;
  }
  *out_ti = best_ti;
  return 1;
}

/*
 * Expert Sugar/Tobacco/Cotton Planter or Fur Trapper → admit +
 * colonies_assign_field on a free yield surround (matching terrain only). Cite:
 * docs/terrain_yields.md Sugar/Tobacco/Cotton/Fur; Colonization.pdf Skills Chart;
 * colonies_assign_field. Parallel to Expert Farmer / Ore Miner field-assign.
 * No invented rates. On-tile: admit then assign. Off-tile MD≤8: AI_MOVE toward
 * colony (1). Returns 1 if routed or assigned.
 */
static int ai_euro_try_planter_field_assign(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, u);
  if (!name) {
    return 0;
  }
  int field_job = -1;
  if (strstr(name, "Sugar Planter") != NULL) {
    field_job = COLONIZE_JOB_SUGAR_PLANTER;
  } else if (strstr(name, "Tobacco Planter") != NULL) {
    field_job = COLONIZE_JOB_TOBACCO_PLANTER;
  } else if (strstr(name, "Cotton Planter") != NULL) {
    field_job = COLONIZE_JOB_COTTON_PLANTER;
  } else if (strstr(name, "Fur Trapper") != NULL) {
    field_job = COLONIZE_JOB_FUR_TRAPPER;
  } else {
    return 0;
  }
  if (ai_euro_land_is_fortified(u)) {
    return 0;
  }
  int best_d = 99;
  int bx = -1;
  int by = -1;
  int best_cid = -1;
  int best_ti = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->colonist_count >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    int ti = -1;
    if (!ai_euro_colony_free_planter_field(ctx, c, field_job, &ti)) {
      continue;
    }
    const int dist = abs(c->x - u->x) + abs(c->y - u->y);
    if (dist > 8) {
      continue;
    }
    if (bx < 0 || dist < best_d) {
      best_d = dist;
      bx = c->x;
      by = c->y;
      best_cid = i;
      best_ti = ti;
    }
  }
  if (best_cid < 0 || best_ti < 0) {
    return 0;
  }
  if (u->x == bx && u->y == by) {
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
    if (idx < 0) {
      return 0;
    }
    if (!colonies_assign_field(ctx->colonies, best_cid, idx, best_ti, field_job)) {
      return 1; /* admitted; field assign failed — still consumed unit */
    }
    return 1;
  }
  ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_LABOR, 4);
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, bx, by);
  return 1;
}

/*
 * Best built workplace in a craft chain (House → Shop → Factory). Cite:
 * docs/building_production.md processing chains / Skills Chart.
 */
static int ai_euro_colony_best_craft_building(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c,
  const char* const* names
) {
  if (!pool || !c || !c->active || !names) {
    return -1;
  }
  int best = -1;
  for (int i = 0; names[i]; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && c->has_building[idx]) {
      best = idx; /* later tiers overwrite — prefer highest built */
    }
  }
  return best;
}

/*
 * Idle Master Distiller / Weaver / Tobacconist / Blacksmith / Gunsmith /
 * Fur Trader / Master Carpenter / Elder Statesman / Firebrand Preacher /
 * Expert Teacher → admit + colonies_assign_workplace on matching craft / civic
 * chain. Cite: Colonization.pdf Skills Chart; docs/building_production.md
 * Distiller/Weaver/Tobacconist/Blacksmith/Gunsmith (Armory→Magazine→Arsenal)/
 * Fur Trader (House→Trading Post→Factory); Carpenter→Shop/Mill; Statesman→Town
 * Hall; Preacher→Church→Cathedral; Teacher→Schoolhouse→College→University.
 * No invented rates. On-tile: admit then assign. Off-tile MD≤8: AI_MOVE (1).
 */
static int ai_euro_try_expert_workplace_assign(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->colonies || !ctx->units || !u || !u->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, u);
  if (!name) {
    return 0;
  }
  static const char* const k_distiller[] = {
    "Rum Distiller's House", "Rum Distillery", "Rum Factory", NULL
  };
  static const char* const k_weaver[] = {
    "Weaver's House", "Weaver's Shop", "Textile Mill", NULL
  };
  static const char* const k_tobacconist[] = {
    "Tobacconist's House", "Tobacconist's Shop", "Cigar Factory", NULL
  };
  static const char* const k_blacksmith[] = {
    "Blacksmith's House", "Blacksmith's Shop", "Iron Works", NULL
  };
  static const char* const k_gunsmith[] = {
    "Armory", "Magazine", "Arsenal", NULL
  };
  static const char* const k_fur_trader[] = {
    "Fur Trader's House", "Fur Trading Post", "Fur Factory", NULL
  };
  static const char* const k_carpenter[] = {"Carpenter's Shop", "Lumber Mill", NULL};
  static const char* const k_statesman[] = {"Town Hall", NULL};
  static const char* const k_preacher[] = {"Church", "Cathedral", NULL};
  static const char* const k_teacher[] = {"Schoolhouse", "College", "University", NULL};
  const char* const* chain = NULL;
  if (strstr(name, "Distiller") != NULL) {
    chain = k_distiller;
  } else if (strstr(name, "Weaver") != NULL) {
    chain = k_weaver;
  } else if (strstr(name, "Tobacconist") != NULL) {
    chain = k_tobacconist;
  } else if (strstr(name, "Blacksmith") != NULL) {
    chain = k_blacksmith;
  } else if (strstr(name, "Gunsmith") != NULL) {
    chain = k_gunsmith;
  } else if (strstr(name, "Fur Trader") != NULL) {
    chain = k_fur_trader;
  } else if (strstr(name, "Carpenter") != NULL) {
    chain = k_carpenter;
  } else if (strstr(name, "Statesman") != NULL) {
    chain = k_statesman;
  } else if (strstr(name, "Preacher") != NULL) {
    chain = k_preacher;
  } else if (strstr(name, "Teacher") != NULL) {
    chain = k_teacher;
  } else {
    return 0;
  }
  if (ai_euro_land_is_fortified(u)) {
    return 0;
  }
  int best_d = 99;
  int bx = -1;
  int by = -1;
  int best_cid = -1;
  int best_btype = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (c->colonist_count >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    const int btype = ai_euro_colony_best_craft_building(ctx->colonies, c, chain);
    if (btype < 0) {
      continue;
    }
    const int dist = abs(c->x - u->x) + abs(c->y - u->y);
    if (dist > 8) {
      continue;
    }
    if (bx < 0 || dist < best_d) {
      best_d = dist;
      bx = c->x;
      by = c->y;
      best_cid = i;
      best_btype = btype;
    }
  }
  if (best_cid < 0 || best_btype < 0) {
    return 0;
  }
  if (u->x == bx && u->y == by) {
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
    if (idx < 0) {
      return 0;
    }
    if (!colonies_assign_workplace(ctx->colonies, best_cid, idx, best_btype)) {
      return 1; /* admitted; workplace assign failed — still consumed unit */
    }
    return 1;
  }
  ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_LABOR, 4);
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, bx, by);
  return 1;
}

/* Free Colonist / Colonist / Pioneer / Hardy / Farmer — can join LABOR for food. */
static int ai_euro_is_food_labor_name(const char* name) {
  if (!name) {
    return 0;
  }
  if (strstr(name, "Wagon") != NULL || strstr(name, "Supply Train") != NULL) {
    return 0;
  }
  if (strstr(name, "Soldier") != NULL || strstr(name, "Dragoon") != NULL ||
      strstr(name, "Scout") != NULL) {
    return 0;
  }
  return strstr(name, "Pioneer") != NULL || strstr(name, "Hardy") != NULL ||
         strstr(name, "Free Colonist") != NULL || strstr(name, "Colonist") != NULL ||
         strstr(name, "Farmer") != NULL;
}

/*
 * Food-LABOR capable unit: display-name food labor OR @JOB Farmer (profession 0)
 * Expert Farmer on a Free Colonist / Colonist. Cite: docs/building_production.md
 * @JOB Farmer→Expert Farmer / Food; Colonization.pdf Skills Chart. No invented
 * food rates — LABOR join only.
 */
static int ai_euro_unit_is_food_labor(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  const char* name = units_display_name(units, u);
  if (ai_euro_is_food_labor_name(name)) {
    return 1;
  }
  /* profession 0 == @JOB Farmer (Expert Farmer skill). */
  if (u->profession == 0 && name &&
      (strstr(name, "Free Colonist") != NULL || strstr(name, "Colonist") != NULL)) {
    return 1;
  }
  return 0;
}

static int ai_euro_type_is_wagon_name(const char* name);
static int ai_euro_land_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map);

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy) {
  if (!u) {
    return;
  }
  u->orders = orders;
  u->goto_x = gx;
  u->goto_y = gy;
  /*
   * Every goto write defaults to "not idle-roam" (DOS unit+0x314c != 5) —
   * ai_euro_move_scoring_gate re-marks its own two roam branches right
   * after calling this, so a stale roam flag from an earlier turn never
   * survives onto a goal-directed goto (found-tile, hunt, wagon, ship
   * staging) set by any other call site. See s_euro_roam_wander.
   */
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    s_euro_roam_wander[u->id] = 0;
  }
}

static int ai_euro_is_ship_type(const ColonizeUnitPool* units, int unit_id) {
  /* Dispatcher ship wave: sea domain (SHIP_A..C stand-in). */
  return units_is_sea(units, unit_id);
}

/* Chebyshev adjacency (incl. same tile) for coastal embark checks. */
static int ai_euro_tiles_near(int ax, int ay, int bx, int by) {
  const int dx = abs(ax - bx);
  const int dy = abs(ay - by);
  return dx <= 1 && dy <= 1;
}

/*
 * Own ship near (x,y) with passenger cargo space (Treasure board).
 * Cite: manual Galleon six-hold / coastal colony embark. Returns ship id or -1.
 */
static int ai_euro_find_boardable_ship(
  ColonizeTurnContext* ctx,
  int nation_id,
  int x,
  int y
) {
  if (!ctx || !ctx->units || nation_id < 0 || nation_id >= 4) {
    return -1;
  }
  int best = -1;
  int best_d = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* s = &ctx->units->units[i];
    if (!s->active || s->nation_id != nation_id) {
      continue;
    }
    if (!ai_euro_is_ship_type(ctx->units, s->id) || ai_euro_in_europe(s->x, s->y)) {
      continue;
    }
    const int cap = units_ship_capacity(ctx->units, s->id);
    if (cap <= 0 || s->cargo_count >= cap) {
      continue;
    }
    if (!ai_euro_tiles_near(x, y, s->x, s->y)) {
      continue;
    }
    const int d = abs(s->x - x) + abs(s->y - y);
    if (best_d < 0 || d < best_d) {
      best_d = d;
      best = s->id;
    }
  }
  return best;
}

/*
 * Treasure at coastal own colony → board ship with space + AI_SAIL Europe.
 * Cite: Colonization.pdf Treasure Trains (park coastal / Galleon / king galleon).
 * Europe cash-in: ai_euro_try_cash_treasure_europe when ship reaches Europe / HS.
 */
static int ai_euro_try_treasure_board_sail(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* treasure
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !treasure || !treasure->active) {
    return 0;
  }
  const int cid = colonies_id_at(ctx->colonies, treasure->x, treasure->y);
  if (cid < 0) {
    return 0;
  }
  const ColonizeColony* c = colonies_get(ctx->colonies, cid);
  if (!c || !c->active || c->nation_id != nation_id) {
    return 0;
  }
  if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
    return 0;
  }
  const int sid = ai_euro_find_boardable_ship(ctx, nation_id, treasure->x, treasure->y);
  if (sid < 0) {
    return 0;
  }
  ColonizeUnit* ship = units_get(ctx->units, sid);
  if (!ship) {
    return 0;
  }
  int boarded = 0;
  if (ship->x == treasure->x && ship->y == treasure->y) {
    boarded = units_board_stacked(ctx->units, treasure->id, sid) ? 1 : 0;
  } else {
    boarded = units_board(ctx->units, treasure->id, sid) ? 1 : 0;
  }
  if (!boarded) {
    return 0;
  }
  int ex = 0;
  int ey = 0;
  if (ai_euro_europe_sail_target(ctx, ship->x, ship->y, &ex, &ey)) {
    ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, ex, ey);
  } else {
    const int east = ship->x + 8 < ctx->map->width ? ship->x + 8 : ctx->map->width - 1;
    ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, east, ship->y);
  }
  return 1;
}

/*
 * COL1 Treasure gold: cargo_hold[0..1] LE16 (europe.h / europe_cash_treasure_passengers).
 * ColonizeUnit has no treasure_gold — bridge mirrors those bytes into
 * hold_goods_amount[0] (lo) + hold_goods_amount[1] (hi).
 */
static int ai_euro_treasure_gold_from_unit(const ColonizeUnit* treasure) {
  if (!treasure) {
    return 0;
  }
  const unsigned lo = (unsigned)(treasure->hold_goods_amount[0] & 0xff);
  const unsigned hi = (unsigned)(treasure->hold_goods_amount[1] & 0xff);
  return (int)(lo | (hi << 8));
}

/*
 * Cash one Treasure unit via europe_cash_treasure; despawn (not a dock immigrant).
 * Cite: Colonization.pdf Treasure Trains; GAME.TXT @LOOTCASH / @CASHTREASURE;
 * europe_cash_treasure_passengers. Returns credited gold (0 if value unset/PARK).
 */
static int ai_euro_cash_one_treasure(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* treasure
) {
  if (!ctx || !ctx->europe || !ctx->col1_ok || !ctx->col1 || !ctx->units || !treasure ||
      !treasure->active) {
    return 0;
  }
  if (nation_id < 0 || nation_id >= 4 || treasure->nation_id != nation_id) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  /* Borrow the shared EuropeScreen for this nation; ALWAYS restore the
   * human's values after — leaving an AI's treasury behind zeroed the
   * human's displayed (and spendable) gold (bugs.md "starting gold is
   * zero"). */
  const int saved_gold = ctx->europe->gold;
  const int saved_tax = ctx->europe->tax_percent;
  ctx->europe->gold = (int)nat->gold;
  ctx->europe->tax_percent = (int)nat->tax_rate;

  const int value = ai_euro_treasure_gold_from_unit(treasure);
  int credited = 0;
  if (value > 0) {
    credited = europe_cash_treasure(ctx->europe, value);
    nat->gold = (uint32_t)(ctx->europe->gold < 0 ? 0 : ctx->europe->gold);
  } else {
    /*
     * PARK value source: intended COL1 Treasure cargo_hold[0..1] LE16 gold
     * (ColonizeUnit has no treasure_gold; hold_goods_amount[0..1] mirror those
     * bytes when bridge-loaded; game_loop→europe_enqueue_expected does not fill
     * cargo_treasure_gold yet). Do not invent a default rate/value.
     */
  }
  if (nation_id != ctx->human_nation) {
    ctx->europe->gold = saved_gold;
    ctx->europe->tax_percent = saved_tax;
  }
  /* Consume Treasure after cash attempt — same as Expected→Harbor disembark. */
  (void)units_despawn(ctx->units, treasure->id);
  return credited;
}

/*
 * Cortes free king galleon: Treasure on own coastal colony → europe_cash_treasure
 * via units_cortes_cash_coastal_treasures (shared human/AI). Cite: fandom
 * Hernan Cortes; GAME.TXT @KINGGALLEON3.
 */
static int ai_euro_try_cortes_king_galleon_cash(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* treasure
) {
  if (!ctx || !ctx->units || !treasure || !treasure->active ||
      treasure->nation_id != nation_id) {
    return 0;
  }
  if (!founding_fathers_cortes_free_king_galleon(ctx->col1_ok ? ctx->col1 : NULL, nation_id)) {
    return 0;
  }
  if (!ai_euro_is_treasure_name(units_display_name(ctx->units, treasure))) {
    return 0;
  }
  /* Cash all coastal Treasures for nation (includes this unit when eligible). */
  const int before = treasure->id;
  const int n = units_cortes_cash_coastal_treasures(
    ctx->units, ctx->colonies, ctx->map, ctx->europe, ctx->col1, nation_id
  );
  if (n <= 0) {
    return 0;
  }
  /* This unit was consumed if still matching id is gone. */
  const ColonizeUnit* u = units_get(ctx->units, before);
  return (!u || !u->active) ? 1 : 0;
}

/*
 * Treasure (aboard ship or land) at Europe (x/y≥200) or ship on high seas →
 * europe_cash_treasure + despawn. AI stand-in for Expected→Harbor cash-in when
 * ctx->europe is present (R1 API). Cite: Colonization.pdf Treasure Trains.
 * Returns 1 if any Treasure was consumed.
 */
static int ai_euro_try_cash_treasure_europe(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->units || !ctx->europe || !ctx->col1_ok || !ctx->col1 || !u || !u->active) {
    return 0;
  }
  if (nation_id < 0 || nation_id >= 4 || u->nation_id != nation_id) {
    return 0;
  }

  const int at_europe = ai_euro_in_europe(u->x, u->y);
  const int on_hs = ctx->map && map_tile_is_high_seas(ctx->map, u->x, u->y);
  if (!at_europe && !on_hs) {
    return 0;
  }

  int did = 0;

  /* Land Treasure already at Europe coords. */
  if (!ai_euro_is_ship_type(ctx->units, u->id)) {
    if (at_europe && ai_euro_is_treasure_name(units_display_name(ctx->units, u))) {
      (void)ai_euro_cash_one_treasure(ctx, nation_id, u);
      return 1;
    }
    return 0;
  }

  /* Ship: cash Treasure passengers at Europe / HS (Europe exit stand-in). */
  int ids[COLONIZE_UNIT_CARGO_MAX];
  const int n =
    u->cargo_count < COLONIZE_UNIT_CARGO_MAX ? u->cargo_count : COLONIZE_UNIT_CARGO_MAX;
  for (int i = 0; i < n; ++i) {
    ids[i] = u->cargo_ids[i];
  }
  for (int i = 0; i < n; ++i) {
    ColonizeUnit* pax = units_get(ctx->units, ids[i]);
    if (!pax || !pax->active) {
      continue;
    }
    if (!ai_euro_is_treasure_name(units_display_name(ctx->units, pax))) {
      continue;
    }
    (void)ai_euro_cash_one_treasure(ctx, nation_id, pax);
    did = 1;
  }
  return did;
}

/*
 * Expected→Harbor path AI can trigger: due Expected ships (turns_left==0) with
 * cargo_treasure_gold set → europe_tick_voyages → europe_cash_treasure.
 * Cite: europe.h Treasure cash-in; Colonization.pdf Treasure Trains.
 */
static void ai_euro_try_expected_treasure_harbor(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->europe || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ctx->europe->expected_ships <= 0) {
    return;
  }
  int due = 0;
  for (int e = 0; e < ctx->europe->expected_ships; ++e) {
    if (ctx->europe->expected[e].turns_left == 0) {
      due = 1;
      break;
    }
  }
  if (!due) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  /* Borrow/restore — see ai_euro_cash_one_treasure (bugs.md zeroed gold). */
  const int saved_gold = ctx->europe->gold;
  const int saved_tax = ctx->europe->tax_percent;
  ctx->europe->gold = (int)nat->gold;
  ctx->europe->tax_percent = (int)nat->tax_rate;
  europe_tick_voyages(ctx->europe, ctx->units);
  nat->gold = (uint32_t)(ctx->europe->gold < 0 ? 0 : ctx->europe->gold);
  if (nation_id != ctx->human_nation) {
    ctx->europe->gold = saved_gold;
    ctx->europe->tax_percent = saved_tax;
  }
}

/*
 * At war: idle garrison (Soldier/Dragoon/Regular/Continental) or Artillery/
 * Cannon on own coastal colony boards an empty transport with passenger space
 * (units_board / units_board_stacked). Complements war-transport
 * sail-to-threatened-port. Skip embark when the colony is already threatened
 * (stay to defend; unload drops troops there). Artillery boards before
 * on-colony fortify (same early act arm). Cite: Colonization.pdf naval
 * transport / Defending a Colony ("fortify soldiers, dragoons, army, cavalry,
 * or artillery"); euro_unit_act §2b2 / §2d3 ship board; existing Treasure board
 * APIs. Empty = cargo_count==0.
 */
static int ai_euro_try_soldier_board_transport(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* soldier
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !soldier || !soldier->active) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, soldier);
  if (!name ||
      (!ai_euro_is_colony_garrison_name(name) && !ai_euro_is_artillery_name(name))) {
    return 0;
  }
  if (soldier->aboard_ship_id >= 0 || ai_euro_land_is_fortified(soldier)) {
    return 0;
  }
  /* Prefer board over hunt: allow even when planning set MILITARY goto. */
  const int cid = colonies_id_at(ctx->colonies, soldier->x, soldier->y);
  if (cid < 0) {
    return 0;
  }
  const ColonizeColony* c = colonies_get(ctx->colonies, cid);
  if (!c || !c->active || c->nation_id != nation_id) {
    return 0;
  }
  if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
    return 0;
  }
  /* Do not embark from a threatened port — stay to defend; unload drops
   * troops onto threatened colonies. Cite: Colonization.pdf Defending a Colony. */
  if (ai_euro_colony_threatened_by_war(ctx, nation_id, c)) {
    return 0;
  }
  /* Prefer empty transport (no passengers yet) with free capacity. */
  int best = -1;
  int best_d = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* s = &ctx->units->units[i];
    if (!s->active || s->nation_id != nation_id) {
      continue;
    }
    if (!ai_euro_is_ship_type(ctx->units, s->id) || ai_euro_in_europe(s->x, s->y)) {
      continue;
    }
    if (s->cargo_count != 0) {
      continue; /* empty transport only */
    }
    const int cap = units_ship_capacity(ctx->units, s->id);
    if (cap <= 0) {
      continue;
    }
    if (!ai_euro_tiles_near(soldier->x, soldier->y, s->x, s->y)) {
      continue;
    }
    const int d = abs(s->x - soldier->x) + abs(s->y - soldier->y);
    if (best_d < 0 || d < best_d) {
      best_d = d;
      best = s->id;
    }
  }
  if (best < 0) {
    return 0;
  }
  ColonizeUnit* ship = units_get(ctx->units, best);
  if (!ship) {
    return 0;
  }
  int boarded = 0;
  if (ship->x == soldier->x && ship->y == soldier->y) {
    boarded = units_board_stacked(ctx->units, soldier->id, best) ? 1 : 0;
  } else {
    boarded = units_board(ctx->units, soldier->id, best) ? 1 : 0;
  }
  return boarded;
}

/* True when wagon still has free goods-hold capacity (cargo field). */
static int ai_euro_wagon_has_hold_capacity(const ColonizeUnitPool* units, const ColonizeUnit* w) {
  if (!units || !w) {
    return 0;
  }
  const int n = units_goods_hold_count(units, w->id);
  if (n <= 0) {
    return 0;
  }
  for (int h = 0; h < n; ++h) {
    if (w->hold_goods_amount[h] <= 0 || w->hold_goods_amount[h] >= 255) {
      return 1; /* empty slot */
    }
    if (w->hold_goods_amount[h] < 100) {
      return 1; /* partial room */
    }
  }
  return 0;
}

/* Wagon carries cargo_type in any hold. */
static int ai_euro_wagon_has_cargo_type(
  const ColonizeUnitPool* units,
  const ColonizeUnit* w,
  int cargo_type
) {
  if (!units || !w || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  const int n = units_goods_hold_count(units, w->id);
  for (int h = 0; h < n; ++h) {
    if (w->hold_goods_amount[h] > 0 && w->hold_goods_amount[h] < 255 &&
        w->hold_goods_type[h] == cargo_type) {
      return 1;
    }
  }
  return 0;
}

/*
 * Colony short on haul cargo: TOOLS/LUMBER/ORE stock<20 (5cf6), MUSKETS/HORSES
 * stock<10 (inventory muskets_short band; horses same structural threshold),
 * FOOD stock < pop*2 (5cf6 food_short / manual 2 food/colonist). Cite:
 * euro_unit_act §2d; ai_euro_colony_inventory; Colonization.pdf Wagon Train.
 */
static int ai_euro_colony_haul_cargo_short(const ColonizeColony* c, int cargo_type) {
  if (!c || !c->active) {
    return 0;
  }
  if (cargo_type == COLONIZE_CARGO_TOOLS) {
    return c->stock[COLONIZE_CARGO_TOOLS] < 20;
  }
  if (cargo_type == COLONIZE_CARGO_LUMBER) {
    return c->stock[COLONIZE_CARGO_LUMBER] < 20;
  }
  if (cargo_type == COLONIZE_CARGO_ORE) {
    return c->stock[COLONIZE_CARGO_ORE] < 20;
  }
  if (cargo_type == COLONIZE_CARGO_MUSKETS) {
    return c->stock[COLONIZE_CARGO_MUSKETS] < 10;
  }
  if (cargo_type == COLONIZE_CARGO_HORSES) {
    return c->stock[COLONIZE_CARGO_HORSES] < 10;
  }
  if (cargo_type == COLONIZE_CARGO_FOOD) {
    return c->population > 0 &&
           c->stock[COLONIZE_CARGO_FOOD] < c->population * TURN_FOOD_PER_COLONIST;
  }
  return 0;
}

/*
 * Surplus load gate: tools/lumber/ore≥40 / muskets≥20 / horses≥20 (2× short
 * threshold); FOOD ≥ pop*4 (2× food_short floor). Cite: euro_unit_act §2d;
 * 5cf6 food/lumber/ore_short; no invented absolute FOOD stock rates.
 */
static int ai_euro_colony_haul_cargo_surplus(const ColonizeColony* c, int cargo_type) {
  if (!c || !c->active) {
    return 0;
  }
  if (cargo_type == COLONIZE_CARGO_TOOLS) {
    return c->stock[COLONIZE_CARGO_TOOLS] >= 40;
  }
  if (cargo_type == COLONIZE_CARGO_LUMBER) {
    return c->stock[COLONIZE_CARGO_LUMBER] >= 40;
  }
  if (cargo_type == COLONIZE_CARGO_ORE) {
    return c->stock[COLONIZE_CARGO_ORE] >= 40;
  }
  if (cargo_type == COLONIZE_CARGO_MUSKETS) {
    return c->stock[COLONIZE_CARGO_MUSKETS] >= 20;
  }
  if (cargo_type == COLONIZE_CARGO_HORSES) {
    return c->stock[COLONIZE_CARGO_HORSES] >= 20;
  }
  if (cargo_type == COLONIZE_CARGO_FOOD) {
    return c->population > 0 &&
           c->stock[COLONIZE_CARGO_FOOD] >= c->population * TURN_FOOD_PER_COLONIST * 2;
  }
  return 0;
}

/*
 * Load chunk: tools/lumber/ore 20 / muskets|horses 10 (short thresholds); FOOD =
 * one turn of colony consumption (pop * TURN_FOOD_PER_COLONIST). Cite: manual
 * 2 food/colonist; Colonization.pdf Wagon Train cargo; no invented rates.
 */
static int ai_euro_haul_load_amount(const ColonizeColony* c, int cargo_type) {
  if (cargo_type == COLONIZE_CARGO_TOOLS || cargo_type == COLONIZE_CARGO_LUMBER ||
      cargo_type == COLONIZE_CARGO_ORE) {
    return 20;
  }
  if (cargo_type == COLONIZE_CARGO_MUSKETS || cargo_type == COLONIZE_CARGO_HORSES) {
    return 10;
  }
  if (cargo_type == COLONIZE_CARGO_FOOD && c && c->population > 0) {
    return c->population * TURN_FOOD_PER_COLONIST;
  }
  return 0;
}

/*
 * Nearest own colony short on cargo_type (or any TOOLS/LUMBER/MUSKETS/HORSES/FOOD
 * when cargo_type < 0). Cite: euro_unit_act §2d wagon haul; 5cf6 shortage
 * tallies; Colonization.pdf Wagon Train.
 */
static int ai_euro_4393_work_queue_haul_pick(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  const ColonizeUnit* hauler,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int best_score = -999999;
  int have = 0;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    const AiWorkSlot* w = ai_goals_work(i);
    if (!w || w->id < 0 || w->flag_b != 1) {
      continue; /* haul slots only (CONTACT uses flag_b=0) */
    }
    const ColonizeColony* c = colonies_get(ctx->colonies, (int)w->id);
    if (!c || !c->active || c->nation_id != nation_id) {
      continue;
    }
    const int d = abs(c->x - from_x) + abs(c->y - from_y);
    /* −0x5f24 score, DOS distance normalization (raw 2214 / 2134:
     * score / ((dist >> 2) + 1) — replaced the thin `score − d*4`). */
    int score = (int)w->score / ((d >> 2) + 1);
    /* Specialty flag_a match: hauler hold matches colony specialty. Cite: Series R. */
    if (hauler && w->flag_a != 0xffu &&
        ai_euro_wagon_has_cargo_type(ctx->units, hauler, (int)w->flag_a)) {
      score += 32;
    }
    if (!have || score > best_score) {
      have = 1;
      best_score = score;
      bx = c->x;
      by = c->y;
    }
  }
  if (!have) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

static int ai_euro_nearest_haul_short_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int cargo_type,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
    int best_score = 0;
  int have = 0;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    int short_here = 0;
    if (cargo_type >= 0) {
      short_here = ai_euro_colony_haul_cargo_short(c, cargo_type);
    } else {
      short_here = ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_TOOLS) ||
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_LUMBER) ||
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_ORE) ||
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_MUSKETS) ||
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_HORSES) ||
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_FOOD);
    }
    if (!short_here) {
      continue;
    }
    const int d = abs(c->x - from_x) + abs(c->y - from_y);
    /* Col1 +0x8f: AI score idle*8 (decomp ~87677) — prefer hungrier waits. */
    const int score = (int)c->cargo_idle_turns * 8 - d;
    if (!have || score > best_score) {
      have = 1;
      best_score = score;
      bx = c->x;
      by = c->y;
    }
  }
  if (!have) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Idle Wagon Train haul (thin 5b66/5d04): free hold capacity or TOOLS / LUMBER /
 * MUSKETS / HORSES / FOOD cargo → AI_MOVE toward nearest matching short own
 * colony (existing unload delivery). On surplus colony with empty capacity,
 * load that cargo via colonies_transfer_to_unit. Cite: euro_unit_act §2d;
 * Colonization.pdf Wagon Train cargo; COLONIZE_CARGO_* + 5cf6 food/lumber_short;
 * no invented stock rates.
 */
static int ai_euro_try_wagon_haul(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* wagon
) {
  if (!ctx || !ctx->units || !ctx->colonies || !wagon || !wagon->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, wagon);
  if (!ai_euro_type_is_wagon_name(name)) {
    return 0;
  }
  const int has_tools = ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_TOOLS);
  const int has_lumber =
    ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_LUMBER);
  const int has_ore = ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_ORE);
  const int has_muskets =
    ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_MUSKETS);
  const int has_horses =
    ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_HORSES);
  const int has_food =
    ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_FOOD);
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, wagon);
  if (!has_tools && !has_lumber && !has_ore && !has_muskets && !has_horses && !has_food &&
      !has_cap) {
    return 0;
  }
  /* Prefer cargo already aboard when picking short target. */
  int prefer_cargo = -1;
  if (has_tools) {
    prefer_cargo = COLONIZE_CARGO_TOOLS;
  } else if (has_lumber) {
    prefer_cargo = COLONIZE_CARGO_LUMBER;
  } else if (has_ore) {
    prefer_cargo = COLONIZE_CARGO_ORE;
  } else if (has_muskets) {
    prefer_cargo = COLONIZE_CARGO_MUSKETS;
  } else if (has_horses) {
    prefer_cargo = COLONIZE_CARGO_HORSES;
  } else if (has_food) {
    prefer_cargo = COLONIZE_CARGO_FOOD;
  }
  /* On own colony with surplus + free hold → load before haul.
   * Default ladder tools>lumber>ore>muskets>horses>food; when inventory
   * food_short>20 prefer FOOD first (hungry colonies). Cite: Colonization.pdf
   * Wagon Train; euro_unit_act §2d surplus FOOD deepen; 5cf6 food_short. */
  if (has_cap && prefer_cargo < 0) {
    const int cid = colonies_id_at(ctx->colonies, wagon->x, wagon->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      if (c && c->active && c->nation_id == nation_id) {
        const AiEuroInventory* inv = ai_goals_inventory(nation_id);
        const int food_first = inv && inv->food_short > 20;
        static const int k_load_default[] = {
          COLONIZE_CARGO_TOOLS,
          COLONIZE_CARGO_LUMBER,
          COLONIZE_CARGO_ORE,
          COLONIZE_CARGO_MUSKETS,
          COLONIZE_CARGO_HORSES,
          COLONIZE_CARGO_FOOD
        };
        static const int k_load_food_first[] = {
          COLONIZE_CARGO_FOOD,
          COLONIZE_CARGO_TOOLS,
          COLONIZE_CARGO_LUMBER,
          COLONIZE_CARGO_ORE,
          COLONIZE_CARGO_MUSKETS,
          COLONIZE_CARGO_HORSES
        };
        const int* order = food_first ? k_load_food_first : k_load_default;
        const size_t n_order =
          food_first ? sizeof(k_load_food_first) / sizeof(k_load_food_first[0])
                     : sizeof(k_load_default) / sizeof(k_load_default[0]);
        /* Col1 +0x8d specialty: try that cargo first when surplus. */
        if (c->specialty_cargo != 0xff &&
            (int)c->specialty_cargo < COLONIZE_CARGO_COUNT &&
            ai_euro_colony_haul_cargo_surplus(c, (int)c->specialty_cargo)) {
          const int ct = (int)c->specialty_cargo;
          const int amt = ai_euro_haul_load_amount(c, ct);
          if (amt > 0 &&
              colonies_transfer_to_unit(ctx->colonies, cid, ctx->units, wagon->id, ct, amt) >
                0) {
            prefer_cargo = ct;
          }
        }
        /* Col1 +0x90 cargo_produced_mask: prefer produced surplus next. */
        for (size_t i = 0; prefer_cargo < 0 && i < n_order; ++i) {
          const int ct = order[i];
          if ((c->cargo_produced_mask & (uint16_t)(1u << ct)) == 0) {
            continue;
          }
          if (!ai_euro_colony_haul_cargo_surplus(c, ct)) {
            continue;
          }
          const int amt = ai_euro_haul_load_amount(c, ct);
          if (amt > 0 &&
              colonies_transfer_to_unit(ctx->colonies, cid, ctx->units, wagon->id, ct, amt) >
                0) {
            prefer_cargo = ct;
          }
        }
        for (size_t i = 0; prefer_cargo < 0 && i < n_order; ++i) {
          const int ct = order[i];
          if (!ai_euro_colony_haul_cargo_surplus(c, ct)) {
            continue;
          }
          const int amt = ai_euro_haul_load_amount(c, ct);
          if (amt > 0 &&
              colonies_transfer_to_unit(ctx->colonies, cid, ctx->units, wagon->id, ct, amt) >
                0) {
            prefer_cargo = ct;
            break;
          }
        }
      }
    }
  }
  int tx = 0;
  int ty = 0;
  /* Thin 4393: work-queue haul when specialty/idle (prefer_cargo set or empty).
   * Skip a tip on the wagon's own tile after a surplus load — fall through to
   * nearest short colony so load+haul completes same beat. */
  int have_tip = 0;
  if ((prefer_cargo >= 0 || has_cap) &&
      ai_euro_4393_work_queue_haul_pick(
        ctx, nation_id, wagon->x, wagon->y, wagon, &tx, &ty
      ) &&
      !(tx == wagon->x && ty == wagon->y)) {
    have_tip = 1;
  }
  if (!have_tip &&
      !ai_euro_nearest_haul_short_colony(
        ctx, nation_id, wagon->x, wagon->y, prefer_cargo, &tx, &ty
      )) {
    return 0;
  }
  if (wagon->x == tx && wagon->y == ty) {
    return 0; /* already there — unload path handles delivery */
  }
  /* Re-aim short colony (override FOUND/explore scoring gate yank). */
  if (units_orders_follow_goto(wagon->orders) && wagon->goto_x == tx &&
      wagon->goto_y == ty) {
    return 1; /* already hauling to target */
  }
  ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, tx, ty);
  return 1;
}

/*
 * Wagon inland→coast Europe-export feeder (thin mid-5d04): when supply haul
 * does not bind, load FUN_364b_0636-eligible surplus (stock>99 → leave 50;
 * prefer Silver) and AI_MOVE nearest own coastal colony for ship export sail.
 * Cite: FUN_364b_0688 / europe_cargo_export_eligible; euro_unit_act §2d / §2d2;
 * Colonization.pdf Wagon Train + Europe buy/sell. No invented rates.
 */
static int ai_euro_wagon_holds_export_goods(const ColonizeUnitPool* units, const ColonizeUnit* w) {
  if (!units || !w) {
    return 0;
  }
  const int n = units_goods_hold_count(units, w->id);
  for (int h = 0; h < n; ++h) {
    if (w->hold_goods_amount[h] <= 0 || w->hold_goods_amount[h] >= 255) {
      continue;
    }
    if (europe_cargo_export_eligible(w->hold_goods_type[h])) {
      return 1;
    }
  }
  return 0;
}

static int ai_euro_nearest_own_coastal_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !ctx->map || !out_x || !out_y) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
      continue;
    }
    const int d = abs(c->x - from_x) + abs(c->y - from_y);
    if (best < 0 || d < best) {
      best = d;
      bx = c->x;
      by = c->y;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

static int ai_euro_try_wagon_europe_export_feeder(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* wagon
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !wagon || !wagon->active) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, wagon);
  if (!ai_euro_type_is_wagon_name(name)) {
    return 0;
  }
  if (units_goods_hold_count(ctx->units, wagon->id) <= 0) {
    return 0;
  }

  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, wagon);
  if (has_cap && !ai_euro_wagon_holds_export_goods(ctx->units, wagon)) {
    const int cid = colonies_id_at(ctx->colonies, wagon->x, wagon->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      if (c && c->active && c->nation_id == nation_id) {
        static const int k_prefer[] = {
          COLONIZE_CARGO_SILVER,
          COLONIZE_CARGO_SUGAR,
          COLONIZE_CARGO_TOBACCO,
          COLONIZE_CARGO_COTTON,
          COLONIZE_CARGO_FURS,
          COLONIZE_CARGO_ORE,
          COLONIZE_CARGO_RUM,
          COLONIZE_CARGO_CIGARS,
          COLONIZE_CARGO_CLOTH,
          COLONIZE_CARGO_COATS,
          COLONIZE_CARGO_TRADE_GOODS
        };
        for (size_t pi = 0; pi < sizeof(k_prefer) / sizeof(k_prefer[0]); ++pi) {
          const int ct = k_prefer[pi];
          if (!europe_cargo_export_eligible(ct)) {
            continue;
          }
          if (c->stock[ct] <= 99) {
            continue;
          }
          const int amt = c->stock[ct] - 50;
          if (amt <= 0) {
            continue;
          }
          if (colonies_transfer_to_unit(ctx->colonies, cid, ctx->units, wagon->id, ct, amt) >
              0) {
            break;
          }
        }
      }
    }
  }

  if (!ai_euro_wagon_holds_export_goods(ctx->units, wagon)) {
    return 0;
  }

  int cx = 0;
  int cy = 0;
  if (!ai_euro_nearest_own_coastal_colony(ctx, nation_id, wagon->x, wagon->y, &cx, &cy)) {
    return 0;
  }

  /* On coastal own colony → unload export holds into stock for ship sail. */
  if (wagon->x == cx && wagon->y == cy) {
    const int cid = colonies_id_at(ctx->colonies, wagon->x, wagon->y);
    if (cid < 0) {
      return 1;
    }
    for (;;) {
      int hold = -1;
      const int n = units_goods_hold_count(ctx->units, wagon->id);
      for (int h = 0; h < n; ++h) {
        if (wagon->hold_goods_amount[h] <= 0 || wagon->hold_goods_amount[h] >= 255) {
          continue;
        }
        if (!europe_cargo_export_eligible(wagon->hold_goods_type[h])) {
          continue;
        }
        hold = h;
        break;
      }
      if (hold < 0) {
        break;
      }
      if (colonies_transfer_from_unit(
            ctx->colonies, cid, ctx->units, wagon->id, hold, NULL) <= 0) {
        break;
      }
    }
    return 1;
  }

  if (units_orders_follow_goto(wagon->orders) && wagon->goto_x == cx &&
      wagon->goto_y == cy) {
    return 1;
  }
  ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, cx, cy);
  return 1;
}

/*
 * Jan de Witt foreign-colony TRADE_GOODS surplus: same load chunk as muskets
 * haul (stock≥20 → load 10). Stock transfer only — no gold/price invent.
 * Cite: docs/fandom_col1994.md Jan de Witt; colonies_de_witt_transfer_*;
 * euro_unit_act §2d wagon haul thresholds.
 */
static int ai_euro_de_witt_trade_goods_surplus(const ColonizeColony* c) {
  return c && c->active && c->stock[COLONIZE_CARGO_TRADE_GOODS] >= 20;
}

/* TRADE_GOODS amount currently on a transport's goods holds. */
static int ai_euro_unit_trade_goods_held(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  int got = 0;
  const int n = units_goods_hold_count(units, u->id);
  for (int h = 0; h < n; ++h) {
    if (u->hold_goods_type[h] == COLONIZE_CARGO_TRADE_GOODS && u->hold_goods_amount[h] > 0 &&
        u->hold_goods_amount[h] < 255) {
      got += u->hold_goods_amount[h];
    }
  }
  return got;
}

/* Nearest own colony (any) for de Witt TRADE_GOODS delivery. */
static int ai_euro_nearest_own_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    const int d = abs(c->x - from_x) + abs(c->y - from_y);
    if (best < 0 || d < best) {
      best = d;
      bx = c->x;
      by = c->y;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Unload all TRADE_GOODS holds into own colony warehouse.
 * Cite: colonies_transfer_from_unit; fandom Jan de Witt delivery loop.
 */
static int ai_euro_de_witt_unload_trade_goods_own(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* transport,
  int colony_id
) {
  if (!ctx || !ctx->units || !ctx->colonies || !transport) {
    return 0;
  }
  ColonizeColony* c = colonies_get_mut(ctx->colonies, colony_id);
  if (!c || !c->active || c->nation_id != nation_id) {
    return 0;
  }
  const int n = units_goods_hold_count(ctx->units, transport->id);
  int moved_total = 0;
  for (;;) {
    int hold = -1;
    for (int h = 0; h < n; ++h) {
      if (transport->hold_goods_type[h] == COLONIZE_CARGO_TRADE_GOODS &&
          transport->hold_goods_amount[h] > 0 && transport->hold_goods_amount[h] < 255) {
        hold = h;
        break;
      }
    }
    if (hold < 0) {
      break;
    }
    const int moved =
      colonies_transfer_from_unit(ctx->colonies, colony_id, ctx->units, transport->id, hold, NULL);
    if (moved <= 0) {
      break;
    }
    moved_total += moved;
  }
  return moved_total > 0 ? 1 : 0;
}

static int ai_euro_nearest_de_witt_foreign_trade(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !ctx->col1 || !out_x || !out_y) {
    return 0;
  }
  int best = -1;
  int bx = -1;
  int by = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id < 0 || c->nation_id > 3 || c->nation_id == nation_id) {
      continue;
    }
    if (ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
      continue;
    }
    if (!ai_euro_de_witt_trade_goods_surplus(c)) {
      continue;
    }
    const int dist = abs(c->x - from_x) + abs(c->y - from_y);
    if (best < 0 || dist < best) {
      best = dist;
      bx = c->x;
      by = c->y;
    }
  }
  if (bx < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Jan de Witt AI trade act (wagon): on foreign Euro colony tile at peace, load
 * TRADE_GOODS surplus via colonies_de_witt_transfer_from_colony; with TRADE_GOODS
 * aboard, unload into nearest own colony warehouse (delivery loop); else AI_MOVE
 * toward nearest peaceful foreign with surplus when hold has capacity. Cite:
 * fandom Jan de Witt; founding_fathers_de_witt_allows_foreign_colony_trade;
 * colonies_transfer_from_unit own-colony unload.
 */
static int ai_euro_try_de_witt_foreign_trade(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* wagon
) {
  if (!ctx || !ctx->units || !ctx->colonies || !ctx->col1_ok || !ctx->col1 || !wagon ||
      !wagon->active) {
    return 0;
  }
  if (!founding_fathers_de_witt_allows_foreign_colony_trade(ctx->col1, nation_id)) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, wagon);
  if (!ai_euro_type_is_wagon_name(name)) {
    return 0;
  }
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, wagon);
  int held_tg = ai_euro_unit_trade_goods_held(ctx->units, wagon);
  const int cid = colonies_id_at(ctx->colonies, wagon->x, wagon->y);
  if (cid >= 0) {
    ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
    if (c && c->active && c->nation_id >= 0 && c->nation_id <= 3 &&
        c->nation_id != nation_id && !ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
      /* Already carrying a chunk → leave for own warehouse (do not re-load). */
      if (held_tg >= 10 || (!has_cap && held_tg > 0)) {
        int hx = 0;
        int hy = 0;
        if (ai_euro_nearest_own_colony(ctx, nation_id, wagon->x, wagon->y, &hx, &hy) &&
            (wagon->x != hx || wagon->y != hy)) {
          ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, hx, hy);
          return 1;
        }
        return 0;
      }
      if (has_cap && ai_euro_de_witt_trade_goods_surplus(c)) {
        const int moved = colonies_de_witt_transfer_from_colony(
          ctx->colonies, cid, ctx->units, wagon->id, COLONIZE_CARGO_TRADE_GOODS, 10, ctx->col1
        );
        if (moved > 0) {
          held_tg = ai_euro_unit_trade_goods_held(ctx->units, wagon);
          int hx = 0;
          int hy = 0;
          if (held_tg > 0 &&
              ai_euro_nearest_own_colony(ctx, nation_id, wagon->x, wagon->y, &hx, &hy) &&
              (wagon->x != hx || wagon->y != hy)) {
            ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, hx, hy);
          }
          return 1;
        }
      }
      return 0; /* on foreign tile; no further haul yank this act */
    }
    /* Own colony: deliver loaded TRADE_GOODS into warehouse. */
    if (c && c->active && c->nation_id == nation_id && held_tg > 0) {
      if (ai_euro_de_witt_unload_trade_goods_own(ctx, nation_id, wagon, cid)) {
        return 1;
      }
    }
  }
  /* Full / carrying TRADE_GOODS → haul home before another foreign pickup. */
  if (held_tg > 0 && (!has_cap || held_tg >= 10)) {
    int hx = 0;
    int hy = 0;
    if (!ai_euro_nearest_own_colony(ctx, nation_id, wagon->x, wagon->y, &hx, &hy)) {
      return 0;
    }
    if (wagon->x == hx && wagon->y == hy) {
      return 0;
    }
    if (units_orders_follow_goto(wagon->orders) && wagon->goto_x == hx && wagon->goto_y == hy) {
      return 1;
    }
    ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, hx, hy);
    return 1;
  }
  if (!has_cap) {
    return 0;
  }
  int tx = 0;
  int ty = 0;
  if (!ai_euro_nearest_de_witt_foreign_trade(ctx, nation_id, wagon->x, wagon->y, &tx, &ty)) {
    return 0;
  }
  if (wagon->x == tx && wagon->y == ty) {
    return 0;
  }
  if (units_orders_follow_goto(wagon->orders) && wagon->goto_x == tx && wagon->goto_y == ty) {
    return 1;
  }
  ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, tx, ty);
  return 1;
}

/*
 * Pioneer plow/road tile improve planner.
 * Cite: Colonization.pdf Clear/Plow/Road; Hardy Pioneer "Clears forest, plows
 * fields, and builds roads faster" — prefer Hardy when both idle (faster work,
 * not invented yields). units_pioneer_plow starts CLEAR_PLOW (forest clear or
 * plow — separate DOS jobs); units_pioneer_road starts BUILD_ROAD. Multi-turn
 * via terr_cost formula + units_pioneer_work_tick. Requires map.improve.
 */

static int ai_euro_pioneer_tile_can_plow(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->improve) {
    return 0;
  }
  if (!map_tile_is_land(map, x, y) || map_tile_is_high_seas(map, x, y)) {
    return 0;
  }
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  /* Arctic / mountains — same gate as units_pioneer_plow. */
  if (pedia == 24 || pedia == 27) {
    return 0;
  }
  if (map_tile_is_plowed(map, x, y)) {
    return 0;
  }
  return 1;
}

static int ai_euro_pioneer_tile_can_road(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->improve) {
    return 0;
  }
  if (!map_tile_is_land(map, x, y) || map_tile_is_high_seas(map, x, y)) {
    return 0;
  }
  if (map_tile_has_road(map, x, y)) {
    return 0;
  }
  return 1;
}

/*
 * Nearest improvable tile near own colony surrounds (MD≤3 from unit, within
 * field ring of own colony). Prefer plow over road; among roads prefer tiles
 * already plowed (Colonization.pdf Clear/Plow/Road sequence — road move bonus
 * on improved fields). 1 if out coords set; out_plow 1 → plow (else road).
 */
static int ai_euro_pioneer_improve_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y,
  int* out_plow
) {
  if (!ctx || !ctx->map || !ctx->map->improve || !ctx->colonies || !out_x || !out_y ||
      !out_plow) {
    return 0;
  }
  int best = 99;
  int bx = -1;
  int by = -1;
  int bplow = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    /* Col1 +0x8c: skip surround until improve_timer meets thin gate. */
    if (c->improve_timer < AI_EURO_IMPROVE_TIMER_MIN) {
      continue;
    }
    for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(ti, &dx, &dy)) {
        continue;
      }
      const int tx = c->x + dx;
      const int ty = c->y + dy;
      const int dist = abs(tx - from_x) + abs(ty - from_y);
      if (dist > 3) {
        continue;
      }
      const int can_plow = ai_euro_pioneer_tile_can_plow(ctx->map, tx, ty);
      const int can_road = ai_euro_pioneer_tile_can_road(ctx->map, tx, ty);
      if (!can_plow && !can_road) {
        continue;
      }
      /*
       * kind_pref: plow (0) > road on already-plowed (1) > other road (2).
       * Closer wins within kind. Cite: Colonization.pdf plow then road.
       */
      int kind_pref = 2;
      if (can_plow) {
        kind_pref = 0;
      } else if (can_road && map_tile_is_plowed(ctx->map, tx, ty)) {
        kind_pref = 1;
      }
      const int score = dist * 2 + kind_pref;
      if (bx < 0 || score < best) {
        best = score;
        bx = tx;
        by = ty;
        bplow = can_plow ? 1 : 0;
      }
    }
  }
  if (bx < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  *out_plow = bplow;
  return 1;
}

/*
 * Idle Hardy/Expert Pioneer with tools → improve nearby colony surround.
 * On-tile: units_pioneer_plow (clear or plow) or units_pioneer_road. Off-tile:
 * AI_MOVE toward target (re-aims over FOUND). Skip when tools_short (tools
 * delivery) or on-colony construction LABOR stay. Cite: Colonization.pdf
 * Pioneer Clear/Plow/Road; Hardy faster work. Returns 1 if worked or routed.
 */
static int ai_euro_try_pioneer_improve(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  if (!units_is_pioneer(ctx->units, u->id) || u->moves_left <= 0) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, u);
  if (!name || (strstr(name, "Pioneer") == NULL && strstr(name, "Hardy") == NULL)) {
    return 0;
  }
  if (ai_euro_land_is_fortified(u)) {
    return 0;
  }
  /* Tools-short: leave for case-7 delivery / LABOR walk. */
  {
    const AiEuroInventory* inv = ai_goals_inventory(nation_id);
    if (inv && inv->tools_short > 0) {
      return 0;
    }
  }
  /* On own colony with Stockade/Warehouse/Lumber Mill build — stay for hammers. */
  if (ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      const ColonizeColony* oc = colonies_get(ctx->colonies, cid);
      if (oc && oc->nation_id == nation_id &&
          ai_euro_colony_wants_construction_labor(ctx->colonies, oc)) {
        return 0;
      }
    }
  }
  int tx = 0;
  int ty = 0;
  int want_plow = 0;
  if (!ai_euro_pioneer_improve_target(ctx, nation_id, u->x, u->y, &tx, &ty, &want_plow)) {
    return 0;
  }
  if (u->x == tx && u->y == ty) {
    char err[64];
    int worked = 0;
    if (want_plow) {
      if (units_pioneer_plow(
            ctx->units, u->id, ctx->map, err, sizeof(err), ctx->colonies, NULL, NULL
          )) {
        worked = 1;
      } else if (ai_euro_pioneer_tile_can_road(ctx->map, tx, ty) &&
                 units_pioneer_road(
                   ctx->units, u->id, ctx->map, err, sizeof(err), ctx->colonies, NULL, NULL
                 )) {
        worked = 1;
      }
    } else if (units_pioneer_road(
                 ctx->units, u->id, ctx->map, err, sizeof(err), ctx->colonies, NULL, NULL
               )) {
      worked = 1;
    }
    if (worked && ctx->colonies) {
      /* FUN_5952 ~94546: successful improve clears colony +0x8c. */
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active || c->nation_id != nation_id) {
          continue;
        }
        if (abs(c->x - tx) <= 1 && abs(c->y - ty) <= 1) {
          c->improve_timer = 0;
          break;
        }
      }
      return 1;
    }
    return 0;
  }
  /* Re-aim improve tile (override FOUND/explore from scoring gate). */
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
  return 1;
}

static void ai_euro_found_with_unit(ColonizeTurnContext* ctx, ColonizeUnit* founder, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || !founder || !founder->active) {
    return;
  }
  if (!colonies_can_found(ctx->colonies, ctx->map, founder->x, founder->y)) {
    return;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(ctx->units, founder->id, &tools, &muskets, &horses);
  /*
   * FUN_4cc6_07c2 Indian homeland purchase when founding on tribe land.
   * Cite: Colonization.pdf / wiki Peter Minuit (FF 2) → free; else charge
   * via colonies_found_with_indian_land. Short gold → PARK (no despawn).
   */
  int cid = -1;
  if (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
    uint32_t* gold = &ctx->col1->nation[nation_id].gold;
    const int cost = colonies_indian_land_purchase_gold(
      ctx->col1, ctx->map, founder->x, founder->y, nation_id
    );
    if (cost > 0 && *gold < (uint32_t)cost) {
      /*
       * FUN_4cc6_07c2 short-gold gate — no despawn. Thin human status only.
       * Cite: colonies_indian_land_purchase_gold; Colonization.pdf Minuit /
       * indian land purchase.
       */
      if (nation_id == ctx->human_nation && ctx->status && ctx->status_size > 0) {
        snprintf(
          ctx->status,
          ctx->status_size,
          "Not enough gold to buy Indian land (%d$ needed).",
          cost
        );
        return;
      }
      /*
       * AI nations start with a treasury of 0 (ai_starting_gold: AI always 0),
       * and the FOUND sites the planner produces are tribe-adjacent by
       * construction, so this gate used to block every AI first colony
       * outright — settlers reached their site and stood there for the rest of
       * the game. The @INDIANLAND dialog's third option is "take it", which
       * proceeds unpaid with no immediate consequence (game_loop.c
       * GAME_INDIAN_LAND_TAKE); that is what an AI with no gold does.
       */
      cid = colonies_found(
        ctx->colonies,
        ctx->map,
        founder->x,
        founder->y,
        nation_id,
        founder->type_index,
        founder->profession,
        tools,
        muskets,
        horses
      );
    } else {
      cid = colonies_found_with_indian_land(
        ctx->colonies,
        ctx->map,
        ctx->col1,
        gold,
        founder->x,
        founder->y,
        nation_id,
        founder->type_index,
        founder->profession,
        tools,
        muskets,
        horses
      );
    }
  } else {
    cid = colonies_found(
      ctx->colonies,
      ctx->map,
      founder->x,
      founder->y,
      nation_id,
      founder->type_index,
      founder->profession,
      tools,
      muskets,
      horses
    );
  }
  if (cid >= 0) {
    colonies_reveal_founded(
    ctx->map, ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, cid); /* FUN_364b_1dd6 Coronado */
    const int founded_x = founder->x;
    const int founded_y = founder->y;
    if (cid >= 0 && cid < COLONIZE_COLONIES_MAX) {
      s_founded_colony_turn[cid] = 1;
    }
    /* FUN_479b_076e -0x77b2 stamp — see ai_goals.h AiNationPlanScratch. */
    ai_goals_note_colony_founded(
      nation_id, ctx->turn_number ? (int)*ctx->turn_number : 0
    );
    units_despawn(ctx->units, founder->id);
    if (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
      ctx->col1->player[nation_id].founded_colonies++;
    }
    /*
     * First colony: release empty ships still latched on found-hold (fy+2) so
     * a later outer pass can sail (TURN4→5 FR). Cite: test-saves-ai/TURN5.
     */
    if (ctx->units && ctx->map && ai_euro_colony_count(ctx->colonies, nation_id) == 1) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* sh = &ctx->units->units[i];
        if (!sh->active || sh->nation_id != nation_id || !units_is_sea(ctx->units, sh->id)) {
          continue;
        }
        if ((sh->goto_x == founded_x && sh->goto_y == founded_y + 2) ||
            (sh->x == founded_x && sh->y == founded_y + 2)) {
          int tx = founded_x + 2;
          int ty = founded_y + 6;
          if (tx >= (int)ctx->map->width) {
            tx = (int)ctx->map->width - 1;
          }
          if (ty >= (int)ctx->map->height) {
            ty = (int)ctx->map->height - 1;
          }
          if (tx < 0) {
            tx = 0;
          }
          if (ty < 0) {
            ty = 0;
          }
          ai_euro_set_goto(sh, UNITS_ORDER_AI_SAIL, tx, ty);
          if (sh->moves_left <= 0) {
            sh->moves_left = units_max_mp(ctx->units, sh->id);
          }
        }
      }
      /* Pioneer on found+1: SW coast course after first town. Cite: TURN5 FR. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* p = &ctx->units->units[i];
        if (!p->active || p->nation_id != nation_id || p->aboard_ship_id >= 0) {
          continue;
        }
        if (!ai_euro_name_is_pioneer(units_display_name(ctx->units, p))) {
          continue;
        }
        if (p->x == founded_x && p->y == founded_y + 1) {
          ai_euro_set_goto(p, UNITS_ORDER_AI_SAIL, founded_x - 3, founded_y + 3);
          /* (50,38)→(48,39) is three cardinal minor-river steps at 1 third
           * each (TURN4→5): a fresh 3-third allotment covers it. */
          p->moves_left = units_max_mp(ctx->units, p->id);
        }
      }
      /*
       * SP: ship one west of cruise tip → NE berth (TURN5→6 45,50→46,49);
       * soldier SE+1 → SE+2 (46,55→46,56). Cite: test-saves-ai/TURN6.
       */
      {
        int wx = 0;
        int wy = 0;
        if (ai_euro_ocean_3558_empty_cruise_tip(
              ctx->map, founded_x, founded_y, &wx, &wy
            )) {
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            ColonizeUnit* sh = &ctx->units->units[i];
            if (!sh->active || sh->nation_id != nation_id ||
                !units_is_sea(ctx->units, sh->id)) {
              continue;
            }
            if (sh->x == wx - 1 && sh->y == wy) {
              const int tx = wx;
              const int ty = wy - 1;
              if (map_tile_is_water(ctx->map, tx, ty) ||
                  map_tile_is_high_seas(ctx->map, tx, ty)) {
                ai_euro_set_goto(sh, UNITS_ORDER_AI_MOVE, tx, ty);
              } else if (map_tile_is_water(ctx->map, wx, wy + 1) ||
                         map_tile_is_high_seas(ctx->map, wx, wy + 1)) {
                /* Fallback berth south of tip if north is land. */
                ai_euro_set_goto(sh, UNITS_ORDER_AI_MOVE, wx, wy + 1);
              } else {
                ai_euro_set_goto(sh, UNITS_ORDER_AI_MOVE, wx, wy);
              }
              if (sh->moves_left <= 0) {
                sh->moves_left = units_max_mp(ctx->units, sh->id);
              }
            }
          }
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            ColonizeUnit* su = &ctx->units->units[i];
            if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
              continue;
            }
            if (!ai_euro_name_is_soldier(units_display_name(ctx->units, su))) {
              continue;
            }
            if (su->x == founded_x + 1 && su->y == founded_y + 3) {
              ai_euro_set_goto(su, UNITS_ORDER_AI_MOVE, founded_x + 1, founded_y + 4);
              su->moves_left = UNITS_MP_PER_TILE;
            }
          }
        }
      }
    }
  }
  /* DOS: a new AI town already carries its first project in the same turn
   * (seed-100 TURN4–6 saves: Docks). Idle-queue-only pick, so re-running it
   * here after the planning-phase call is harmless for existing towns. */
  ai_euro_prefer_peace_construction(ctx, nation_id);
}

static void ai_euro_join_colony(ColonizeTurnContext* ctx, ColonizeUnit* u, int colony_id) {
  if (!ctx || !ctx->colonies || !u) {
    return;
  }
  (void)colonies_admit_unit(ctx->colonies, colony_id, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
}

/*
 * Thin 5b66 case 7 economy: Pioneer/Hardy tools delivery — body after wagon
 * hire-once helpers (see ai_euro_try_pioneer_tools_delivery below).
 */
static int ai_euro_try_pioneer_tools_delivery(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c
);

/* --- inventory (6d8e steps 1–3) ---------------------------------------- */

static void ai_euro_colony_inventory(ColonizeTurnContext* ctx, int nation_id) {
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  if (!inv || !ctx) {
    return;
  }
  ai_goals_inventory_clear(nation_id);
  inv->colony_count = ai_euro_colony_count(ctx->colonies, nation_id);
  /* founding_expansion_urgency stand-in: early game → 8. */
  inv->urgency = (inv->colony_count < 3) ? 8 : (inv->colony_count < 6 ? 4 : 0);

  if (!ctx->colonies) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    /* 5cf6-shaped shortage tallies. */
    if (c->stock[COLONIZE_CARGO_TOOLS] < 20) {
      inv->tools_short += 20 - c->stock[COLONIZE_CARGO_TOOLS];
    }
    /*
     * Lumber shortage tally (5cf6-shaped): mirror tools_short<20 for lumber when
     * colony wants lumberjack LABOR (Warehouse/Lumber Mill) or any construction
     * is in progress. Cite: docs/building_production.md Lumberjack→Lumber;
     * ai_euro_colony_wants_lumberjack_labor; euro_unit_act §2e.
     */
    if ((ai_euro_colony_wants_lumberjack_labor(ctx->colonies, c) ||
         c->building_in_production >= 0) &&
        c->stock[COLONIZE_CARGO_LUMBER] < 20) {
      inv->lumber_short += 20 - c->stock[COLONIZE_CARGO_LUMBER];
    }
    if (c->stock[COLONIZE_CARGO_MUSKETS] < 10) {
      inv->muskets_short += 10 - c->stock[COLONIZE_CARGO_MUSKETS];
    }
    if (c->stock[COLONIZE_CARGO_HORSES] < 10) {
      inv->horses_short += 10 - c->stock[COLONIZE_CARGO_HORSES];
    }
    if (c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
      inv->food_short += (c->population * 2) - c->stock[COLONIZE_CARGO_FOOD];
    }
    /* Ore shortage (5cf6-shaped): feed Blacksmith / Expert Ore Miner dock hire. */
    if (c->stock[COLONIZE_CARGO_ORE] < 20) {
      inv->ore_short += 20 - c->stock[COLONIZE_CARGO_ORE];
    }
    if (c->building_in_production >= 0) {
      inv->found_flags++;
    }
    /* FUN_5952_035e thin: INC cargo_idle_turns (+0x8f) + improve_timer (+0x8c)
     * cap 0x7f. */
    if (c->cargo_idle_turns < 0x7f) {
      c->cargo_idle_turns++;
    }
    if (c->improve_timer < 0x7f) {
      c->improve_timer++;
    }
    /*
     * FUN_5952_0306 thin: refresh specialty for surplus haul cargos (tools…
     * food ladder). Warehouse-full / boycott clears. Cite: +0x8d.
     */
    {
      const uint16_t boycott =
        (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4)
          ? ctx->col1->nation[nation_id].boycott_bitmap
          : 0u;
      static const int k_spec[] = {
        COLONIZE_CARGO_TOOLS,
        COLONIZE_CARGO_LUMBER,
        COLONIZE_CARGO_ORE,
        COLONIZE_CARGO_MUSKETS,
        COLONIZE_CARGO_HORSES,
        COLONIZE_CARGO_FOOD,
        COLONIZE_CARGO_SUGAR,
        COLONIZE_CARGO_TOBACCO,
        COLONIZE_CARGO_COTTON,
        COLONIZE_CARGO_FURS,
        COLONIZE_CARGO_SILVER
      };
      for (size_t si = 0; si < sizeof(k_spec) / sizeof(k_spec[0]); ++si) {
        const int ct = k_spec[si];
        const int want =
          ai_euro_colony_haul_cargo_surplus(c, ct) ||
          (ct != COLONIZE_CARGO_FOOD && c->stock[ct] > 99);
        const int boy = (boycott & (1u << ct)) != 0;
        colonies_specialty_cargo_update(ctx->colonies, c, ct, want, boy);
      }
    }
  }
}

static void ai_euro_unit_inventory(ColonizeTurnContext* ctx, int nation_id) {
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  if (!inv || !ctx || !ctx->units) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    /* Wagon (transport) on colony tile → found_flags bit stand-in. */
    if (units_is_transport(ctx->units, u->id) && ctx->colonies) {
      if (colonies_id_at(ctx->colonies, u->x, u->y) >= 0) {
        inv->found_flags |= 0x20;
      }
    }
    /* Passenger profession demand. */
    if (u->aboard_ship_id >= 0 && u->profession >= 0 && u->profession < 16) {
      if (inv->profession_demand[u->profession] > 0) {
        inv->profession_demand[u->profession]--;
      }
    }
    const char* name = units_display_name(ctx->units, u);
    if (name && strstr(name, "Pioneer") && inv->muskets_short > 0) {
      inv->muskets_short--;
    }
  }
  /* Seed profession demand from tools shortage (LABOR hire preference). */
  if (inv->tools_short > 0 && inv->profession_demand[0] == 0) {
    inv->profession_demand[0] = inv->tools_short / 20 + 1; /* farmer/labor stand-in */
  }
}

/* --- 5d04 planning / hire ---------------------------------------------- */

static int ai_euro_type_is_wagon_name(const char* name) {
  if (!name) {
    return 0;
  }
  return strstr(name, "Wagon") != NULL || strstr(name, "Supply Train") != NULL;
}

static int ai_euro_find_wagon_type(const ColonizeUnitPool* units) {
  static const char* k_wagon[] = {"Wagon Train", "Supply Train", "Wagon"};
  if (!units) {
    return -1;
  }
  for (size_t i = 0; i < sizeof(k_wagon) / sizeof(k_wagon[0]); ++i) {
    const int ty = units_find_type(units, k_wagon[i]);
    if (ty >= 0) {
      return ty;
    }
  }
  return -1;
}

static int ai_euro_nation_has_wagon(const ColonizeUnitPool* units, int nation_id) {
  if (!units) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(units, u->type_index);
    if (ty && ai_euro_type_is_wagon_name(ty->name)) {
      return 1;
    }
  }
  return 0;
}

/*
 * Thin 5d04 / case-7 wagon deepen: when Wagon Train already hired (nation has
 * wagon), unload hold TOOLS / LUMBER / MUSKETS / HORSES / FOOD onto matching
 * short colony via colonies_transfer_from_unit — structural cargo only (no
 * invented stock). Cite: euro_unit_act §2d wagon matrix; Colonization.pdf
 * Wagon Train; 5cf6 food/lumber_short. Unpark #4 remainders PARKED.
 *
 * Wagon/ship commodity dump-sell at Europe: when transport is at Europe (x|y≥200)
 * and ctx->europe is set, sell every non-empty goods hold via europe_sell_unit_hold
 * (harbor dump-sell path; tax via europe_sell_proceeds). Skip empty/invalid holds,
 * cargo with no Europe bid, and holds whose cargo type bit is set in
 * nation.boycott_bitmap (king refuse / wiki Boycott — goods blocked in Europe
 * until penalty paid or Fugger; do not invent prices). Syncs nat↔europe gold
 * like treasure cash-in. Cite: europe_sell_unit_hold / europe_sell_proceeds;
 * Colonization.pdf Europe buy/sell + tax; fandom Boycott (Col); col1
 * boycott_bitmap / ai_king refuse.
 */
static int ai_euro_try_transport_europe_sell(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* transport
) {
  if (!ctx || !ctx->europe || !ctx->units || !ctx->col1_ok || !ctx->col1 ||
      !transport || !transport->active || transport->nation_id != nation_id) {
    return 0;
  }
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (!units_is_transport(ctx->units, transport->id)) {
    return 0;
  }
  /* Europe dock / off-map stand-in (same gate as ship Europe cash). */
  if (!ai_euro_in_europe(transport->x, transport->y)) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  EuropeScreen* eu = ctx->europe;
  /*
   * Borrow/restore the shared EuropeScreen — see ai_euro_cash_one_treasure.
   * bugs.md: this site borrowed but never gave back, so an AI dump-sell left
   * its own gold AND tax rate on the human's Europe screen; the next col1
   * capture wrote that tax straight into the human's nation record, which is
   * what made every King audience start from the AI's rate ("tax hike always
   * resets the pre-existing rate").
   */
  const int saved_gold = eu->gold;
  const int saved_tax = eu->tax_percent;
  eu->gold = (int)nat->gold;
  eu->tax_percent = (int)nat->tax_rate;
  int sold = 0;
  const int n = units_goods_hold_count(ctx->units, transport->id);
  for (int h = 0; h < n; ++h) {
    if (transport->hold_goods_amount[h] <= 0 ||
        transport->hold_goods_amount[h] >= 255) {
      continue;
    }
    const int ctype = transport->hold_goods_type[h];
    if (ctype < 0 || ctype >= COLONIZE_CARGO_COUNT ||
        ctype >= eu->cargo_count || eu->cargo[ctype].bid <= 0) {
      continue; /* empty/invalid or not sellable at Europe */
    }
    /* Wiki Boycott / king refuse: bit N = cargo type N blocked in Europe. */
    if (ctype < 16 && (nat->boycott_bitmap & (uint16_t)(1u << ctype)) != 0) {
      continue;
    }
    const int g = europe_sell_unit_hold(eu, ctx->units, transport->id, h);
    if (g > 0) {
      sold += g;
    }
  }
  if (sold > 0) {
    nat->gold = (uint32_t)(eu->gold < 0 ? 0 : eu->gold);
  }
  if (nation_id != ctx->human_nation) {
    eu->gold = saved_gold;
    eu->tax_percent = saved_tax;
  }
  return sold > 0 ? 1 : 0;
}

static int ai_euro_try_wagon_tools_delivery(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* wagon,
  ColonizeColony* c
) {
  if (!ctx || !ctx->units || !ctx->colonies || !wagon || !c || !c->active ||
      c->nation_id != nation_id) {
    return 0;
  }
  if (!ai_euro_type_is_wagon_name(units_display_name(ctx->units, wagon)) &&
      !units_is_transport(ctx->units, wagon->id)) {
    return 0;
  }
  if (!ai_euro_nation_has_wagon(ctx->units, nation_id)) {
    return 0;
  }
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  /*
   * Unload TOOLS / LUMBER / ORE / MUSKETS / HORSES / FOOD when colony is short
   * on that cargo. Cite: euro_unit_act §2d; COLONIZE_CARGO_* haul deepen; 5cf6.
   */
  const int n = units_goods_hold_count(ctx->units, wagon->id);
  int moved_total = 0;
  int moved_tools = 0;
  int moved_lumber = 0;
  int moved_ore = 0;
  int moved_muskets = 0;
  int moved_food = 0;
  /* Re-scan each pass: unload may reload remainder into another hold. */
  for (;;) {
    int hold = -1;
    int hold_type = -1;
    for (int h = 0; h < n; ++h) {
      if (wagon->hold_goods_amount[h] <= 0 || wagon->hold_goods_amount[h] >= 255) {
        continue;
      }
      const int ct = wagon->hold_goods_type[h];
      if (ct != COLONIZE_CARGO_TOOLS && ct != COLONIZE_CARGO_LUMBER &&
          ct != COLONIZE_CARGO_ORE && ct != COLONIZE_CARGO_MUSKETS &&
          ct != COLONIZE_CARGO_HORSES && ct != COLONIZE_CARGO_FOOD) {
        continue;
      }
      if (!ai_euro_colony_haul_cargo_short(c, ct)) {
        continue;
      }
      hold = h;
      hold_type = ct;
      break;
    }
    if (hold < 0) {
      break;
    }
    const int moved =
      colonies_transfer_from_unit(ctx->colonies, c->id, ctx->units, wagon->id, hold, NULL);
    if (moved <= 0) {
      break;
    }
    moved_total += moved;
    if (hold_type == COLONIZE_CARGO_TOOLS) {
      moved_tools += moved;
    } else if (hold_type == COLONIZE_CARGO_LUMBER) {
      moved_lumber += moved;
    } else if (hold_type == COLONIZE_CARGO_ORE) {
      moved_ore += moved;
    } else if (hold_type == COLONIZE_CARGO_MUSKETS) {
      moved_muskets += moved;
    } else if (hold_type == COLONIZE_CARGO_FOOD) {
      moved_food += moved;
    }
  }
  if (moved_total <= 0) {
    return 0;
  }
  if (inv) {
    if (moved_tools > 0) {
      if (inv->tools_short > moved_tools) {
        inv->tools_short -= moved_tools;
      } else {
        inv->tools_short = 0;
      }
      if (inv->tools_short == 0 && inv->urgency > 0) {
        inv->urgency--;
      }
    }
    if (moved_lumber > 0) {
      if (inv->lumber_short > moved_lumber) {
        inv->lumber_short -= moved_lumber;
      } else {
        inv->lumber_short = 0;
      }
    }
    if (moved_ore > 0) {
      if (inv->ore_short > moved_ore) {
        inv->ore_short -= moved_ore;
      } else {
        inv->ore_short = 0;
      }
    }
    if (moved_muskets > 0) {
      if (inv->muskets_short > moved_muskets) {
        inv->muskets_short -= moved_muskets;
      } else {
        inv->muskets_short = 0;
      }
    }
    if (moved_food > 0) {
      if (inv->food_short > moved_food) {
        inv->food_short -= moved_food;
      } else {
        inv->food_short = 0;
      }
    }
  }
  return moved_total;
}

/*
 * Thin 5b66 case 7 economy stand-in: Pioneer/Hardy on own colony with tools
 * shortage. Prefer structural wagon TOOLS unload when a hired Wagon Train is
 * on the colony tile (5d04 hire-once deepen); else +10 stock[TOOLS] stand-in
 * (cap 100) once per act; trims tools_short / urgency.
 * Deeper hire / treasury matrix remainders OPEN (unpark #4); wagon hire-once
 * cargo ladder (tools/lumber/ore/muskets/horses/food) Done.
 */
static int ai_euro_try_pioneer_tools_delivery(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c
) {
  if (!ctx || !c || !c->active || c->nation_id != nation_id) {
    return 0;
  }
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  const int need =
    (inv && inv->tools_short > 0) || c->stock[COLONIZE_CARGO_TOOLS] < 20;
  if (!need) {
    return 0;
  }
  /* Wagon deepen: structural TOOLS from hired wagon cargo on this tile. */
  if (ctx->units && ai_euro_nation_has_wagon(ctx->units, nation_id)) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* w = &ctx->units->units[i];
      if (!w->active || w->nation_id != nation_id || w->x != c->x || w->y != c->y) {
        continue;
      }
      if (!ai_euro_type_is_wagon_name(units_display_name(ctx->units, w))) {
        continue;
      }
      if (ai_euro_try_wagon_tools_delivery(ctx, nation_id, w, c) > 0) {
        return 1;
      }
    }
  }
  int stock = c->stock[COLONIZE_CARGO_TOOLS];
  if (stock >= 100) {
    return 0;
  }
  stock += 10;
  if (stock > 100) {
    stock = 100;
  }
  c->stock[COLONIZE_CARGO_TOOLS] = stock;
  if (inv) {
    if (inv->tools_short > 10) {
      inv->tools_short -= 10;
    } else {
      inv->tools_short = 0;
    }
    if (inv->tools_short == 0 && inv->urgency > 0) {
      inv->urgency--;
    }
  }
  return 1;
}

/*
 * Europe purchase table Artillery / Caravel gold (europe_init_purchase_table /
 * original_screenshots/europe/purchase.png) — 5d04 war Artillery hire and
 * thin 5c3c Caravel buy when no Europe transport.
 */
#define AI_EURO_ARTILLERY_PURCHASE_GOLD 500
#define AI_EURO_CARAVEL_PURCHASE_GOLD 1000
#define AI_EURO_MERCHANTMAN_PURCHASE_GOLD 2000
#define AI_EURO_GALLEON_PURCHASE_GOLD 3000
#define AI_EURO_FRIGATE_PURCHASE_GOLD 5000

/*
 * NAMES.TXT @JOB: Soldier → Veteran Soldiers train cost 2000$.
 * Mid-hire uses this when Veteran Soldier type exists but @UNIT cost is 0.
 * Cite: COLONIZE/NAMES.TXT @JOB; Europe train table (not purchase.png).
 */
#define AI_EURO_VETERAN_SOLDIER_TRAIN_GOLD 2000

/* Europe dock plurals / @JOB experts for case-7 tools hire (only if present). */
static int ai_euro_dock_name_is_tools_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Hardy Pioneer") != NULL || strstr(name, "Expert Pioneer") != NULL ||
         strstr(name, "Master Carpenter") != NULL;
}

/* Europe dock Expert Farmer for case-7 food hire (only if present on dock). */
static int ai_euro_dock_name_is_food_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Expert Farmer") != NULL;
}

/* Europe dock Expert Fisherman for case-7 coastal food hire. */
static int ai_euro_dock_name_is_fisherman_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  /* Pool plural is "Expert Fishermen" — not a substring of "Fisherman". */
  return strstr(name, "Fisherman") != NULL || strstr(name, "Fishermen") != NULL;
}

/* Europe dock Master Carpenter for case-7 construction hire (only if present). */
static int ai_euro_dock_name_is_carpenter_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Master Carpenter") != NULL;
}

/* Europe dock Expert Lumberjack for case-7 lumber hire (only if present). */
static int ai_euro_dock_name_is_lumberjack_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Expert Lumberjack") != NULL || strstr(name, "Lumberjack") != NULL;
}

/* Europe dock Expert Ore / Silver Miner for case-7 ore hire (only if present). */
static int ai_euro_dock_name_is_ore_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Ore Miner") != NULL || strstr(name, "Silver Miner") != NULL;
}

/* Europe dock Master Gunsmith for case-7 muskets hire (only if present). */
static int ai_euro_dock_name_is_gunsmith_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Gunsmith") != NULL;
}

/* Europe dock Master Blacksmith for case-7 tools hire (only if present). */
static int ai_euro_dock_name_is_blacksmith_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Blacksmith") != NULL;
}

/* Europe dock Seasoned Scout for case-7 explore/CONTACT hire (only if present). */
static int ai_euro_dock_name_is_scout_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Seasoned Scout") != NULL ||
         (strstr(name, "Scout") != NULL && strstr(name, "Seasoned") != NULL);
}

/* Europe dock Jesuit/Missionary for case-7 convert CONTACT hire. */
static int ai_euro_dock_name_is_missionary_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  /* Pool plural "Jesuit Missionaries" — not a substring of "Missionary". */
  return strstr(name, "Missionary") != NULL || strstr(name, "Missionaries") != NULL ||
         strstr(name, "Jesuit") != NULL;
}

/* Europe dock Elder Statesman for case-7 liberty-bell hire. */
static int ai_euro_dock_name_is_elder_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Elder Statesman") != NULL || strstr(name, "Elder Statesmen") != NULL;
}

/* Europe dock Firebrand Preacher for case-7 crosses hire. */
static int ai_euro_dock_name_is_preacher_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Firebrand") != NULL || strstr(name, "Preacher") != NULL;
}

/* Europe dock Expert Teacher for case-7 school hire. */
static int ai_euro_dock_name_is_teacher_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Teacher") != NULL;
}

/* Europe dock Master Distiller for case-7 rum craft hire. */
static int ai_euro_dock_name_is_distiller_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Distiller") != NULL;
}

/* Europe dock Master Weaver for case-7 cloth craft hire. */
static int ai_euro_dock_name_is_weaver_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Weaver") != NULL;
}

/* Europe dock Master Tobacconist for case-7 cigar craft hire. */
static int ai_euro_dock_name_is_tobacconist_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Tobacconist") != NULL;
}

/* Europe dock Master Fur Trader for case-7 coats craft hire. */
static int ai_euro_dock_name_is_fur_trader_expert(const char* name) {
  if (!name || !name[0]) {
    return 0;
  }
  return strstr(name, "Fur Trader") != NULL;
}

/*
 * Resolve dock immigrant name → unit type (strip trailing 's' for pool
 * plurals). None of "Expert Farmer" / "Fisherman" / … / "Free Colonist" are
 * real NAMES.TXT @UNIT rows — every specialist is base type "Colonists" with
 * a @JOB profession (units_display_name() flavors the shown name). Each arm
 * below still tries the specialist/base occupation name first (dock pools in
 * some fixtures do define them), then real "Colonists" before the legacy
 * "Free Colonist" fallback, so real NAMES.TXT hires actually resolve instead
 * of silently returning -1. Cite: port_plan.md Phase 3 "Free Colonist" dead-
 * lookup note; the caller (below) copies the dock unit's real profession
 * onto the spawned "Colonists" hire regardless of which arm matched.
 */
static int ai_euro_type_from_dock_name(const ColonizeUnitPool* units, const char* dock_name) {
  if (!units || !dock_name || !dock_name[0]) {
    return -1;
  }
  int ty = units_find_type(units, dock_name);
  if (ty >= 0) {
    return ty;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "%s", dock_name);
  const size_t len = strlen(buf);
  if (len > 1 && (buf[len - 1] == 's' || buf[len - 1] == 'S')) {
    buf[len - 1] = '\0';
    ty = units_find_type(units, buf);
    if (ty >= 0) {
      return ty;
    }
  }
  if (strstr(dock_name, "Hardy Pioneer") || strstr(dock_name, "Expert Pioneer")) {
    ty = units_find_type(units, "Hardy Pioneer");
    if (ty < 0) {
      ty = units_find_type(units, "Pioneer");
    }
    return ty;
  }
  if (strstr(dock_name, "Master Carpenter")) {
    ty = units_find_type(units, "Master Carpenter");
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Expert Farmer")) {
    ty = units_find_type(units, "Expert Farmer");
    if (ty < 0) {
      ty = units_find_type(units, "Farmer");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Fisherman") || strstr(dock_name, "Fishermen")) {
    ty = units_find_type(units, "Expert Fisherman");
    if (ty < 0) {
      ty = units_find_type(units, "Fisherman");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Expert Lumberjack") || strstr(dock_name, "Lumberjack")) {
    ty = units_find_type(units, "Expert Lumberjack");
    if (ty < 0) {
      ty = units_find_type(units, "Lumberjack");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Ore Miner")) {
    ty = units_find_type(units, "Expert Ore Miner");
    if (ty < 0) {
      ty = units_find_type(units, "Ore Miner");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Silver Miner")) {
    ty = units_find_type(units, "Expert Silver Miner");
    if (ty < 0) {
      ty = units_find_type(units, "Silver Miner");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Gunsmith")) {
    ty = units_find_type(units, "Master Gunsmith");
    if (ty < 0) {
      ty = units_find_type(units, "Gunsmith");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Blacksmith")) {
    ty = units_find_type(units, "Master Blacksmith");
    if (ty < 0) {
      ty = units_find_type(units, "Blacksmith");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Seasoned Scout") ||
      (strstr(dock_name, "Scout") && strstr(dock_name, "Seasoned"))) {
    ty = units_find_type(units, "Seasoned Scout");
    if (ty < 0) {
      ty = units_find_type(units, "Scout");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Missionary") || strstr(dock_name, "Missionaries") ||
      strstr(dock_name, "Jesuit")) {
    ty = units_find_type(units, "Jesuit Missionary");
    if (ty < 0) {
      ty = units_find_type(units, "Missionary");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Elder Statesman") || strstr(dock_name, "Elder Statesmen")) {
    ty = units_find_type(units, "Elder Statesman");
    if (ty < 0) {
      ty = units_find_type(units, "Statesman");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Firebrand") || strstr(dock_name, "Preacher")) {
    ty = units_find_type(units, "Firebrand Preacher");
    if (ty < 0) {
      ty = units_find_type(units, "Preacher");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Teacher")) {
    ty = units_find_type(units, "Expert Teacher");
    if (ty < 0) {
      ty = units_find_type(units, "Teacher");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Distiller")) {
    ty = units_find_type(units, "Master Distiller");
    if (ty < 0) {
      ty = units_find_type(units, "Distiller");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Weaver")) {
    ty = units_find_type(units, "Master Weaver");
    if (ty < 0) {
      ty = units_find_type(units, "Weaver");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Tobacconist")) {
    ty = units_find_type(units, "Master Tobacconist");
    if (ty < 0) {
      ty = units_find_type(units, "Tobacconist");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  if (strstr(dock_name, "Fur Trader")) {
    ty = units_find_type(units, "Master Fur Trader");
    if (ty < 0) {
      ty = units_find_type(units, "Fur Trader");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Colonists");
      if (ty < 0) {
        ty = units_find_type(units, "Free Colonist");
      }
    }
    return ty;
  }
  return -1;
}

/* First dock slot matching Hardy/Expert Pioneer or Master Carpenter; -1 if none. */
static int ai_euro_dock_find_tools_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_tools_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Expert Farmer; -1 if none. */
static int ai_euro_dock_find_food_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_food_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Expert Fisherman; -1 if none. */
static int ai_euro_dock_find_fisherman_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_fisherman_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* True if nation has a coastal own colony (Fisherman field-assign usable). */
static int ai_euro_nation_has_coastal_colony(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (map_tile_is_coastal(ctx->map, c->x, c->y)) {
      return 1;
    }
  }
  return 0;
}

/* First dock slot matching Master Carpenter; -1 if none. */
static int ai_euro_dock_find_carpenter_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_carpenter_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Expert Lumberjack; -1 if none. */
static int ai_euro_dock_find_lumberjack_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_lumberjack_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Expert Ore/Silver Miner; -1 if none. */
static int ai_euro_dock_find_ore_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_ore_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Master Gunsmith; -1 if none. */
static int ai_euro_dock_find_gunsmith_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_gunsmith_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Master Blacksmith; -1 if none. */
static int ai_euro_dock_find_blacksmith_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_blacksmith_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Seasoned Scout; -1 if none. */
static int ai_euro_dock_find_scout_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_scout_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Jesuit/Missionary; -1 if none. */
static int ai_euro_dock_find_missionary_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_missionary_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Elder Statesman; -1 if none. */
static int ai_euro_dock_find_elder_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_elder_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Firebrand Preacher; -1 if none. */
static int ai_euro_dock_find_preacher_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_preacher_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* First dock slot matching Expert Teacher; -1 if none. */
static int ai_euro_dock_find_teacher_expert(const EuropeScreen* eu) {
  if (!eu) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (ai_euro_dock_name_is_teacher_expert(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* True if nation has Church or Cathedral (Preacher workplace). */
static int ai_euro_nation_has_church(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies) {
    return 0;
  }
  const int church_id = colonies_find_building(ctx->colonies, "Church");
  const int cathedral_id = colonies_find_building(ctx->colonies, "Cathedral");
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (church_id >= 0 && church_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[church_id]) {
      return 1;
    }
    if (cathedral_id >= 0 && cathedral_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[cathedral_id]) {
      return 1;
    }
  }
  return 0;
}

/* True if nation has Schoolhouse, College, or University (Teacher workplace). */
static int ai_euro_nation_has_school(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies) {
    return 0;
  }
  const int school_id = colonies_find_building(ctx->colonies, "Schoolhouse");
  const int college_id = colonies_find_building(ctx->colonies, "College");
  const int university_id = colonies_find_building(ctx->colonies, "University");
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (school_id >= 0 && school_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[school_id]) {
      return 1;
    }
    if (college_id >= 0 && college_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[college_id]) {
      return 1;
    }
    if (university_id >= 0 && university_id < COLONIZE_BUILDING_TYPES_MAX &&
        c->has_building[university_id]) {
      return 1;
    }
  }
  return 0;
}

/*
 * True if own colony has a craft-chain building and raw stock ≥20 (feed Master
 * Distiller/Weaver/Tobacconist/Fur Trader dock hire). Cite: building_production
 * craft chains; euro_unit_act workplace assign.
 */
static int ai_euro_nation_wants_craft(
  ColonizeTurnContext* ctx,
  int nation_id,
  const char* const* chain,
  int cargo_type
) {
  if (!ctx || !ctx->colonies || !chain || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (ai_euro_colony_best_craft_building(ctx->colonies, c, chain) < 0) {
      continue;
    }
    if (c->stock[cargo_type] >= 20) {
      return 1;
    }
  }
  return 0;
}

static int ai_euro_dock_find_named_expert(
  const EuropeScreen* eu,
  int (*name_is)(const char*)
) {
  if (!eu || !name_is) {
    return -1;
  }
  for (int i = 0; i < eu->dock_count && i < EUROPE_DOCK_MAX; ++i) {
    if (!eu->dock[i].present) {
      continue;
    }
    if (name_is(eu->dock[i].name)) {
      return i;
    }
  }
  return -1;
}

/* True if any tribe has no mission (mission==0xff). */
static int ai_euro_has_unmissioned_tribe(const ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe ||
      ctx->col1->head.tribe_count == 0) {
    return 0;
  }
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    if (ctx->col1->tribe[i].mission == 0xff) {
      return 1;
    }
  }
  return 0;
}

/* Remove dock[idx] (shift); returns 1 on success. */
static int ai_euro_dock_remove_at(EuropeScreen* eu, int idx) {
  if (!eu || idx < 0 || idx >= eu->dock_count || idx >= EUROPE_DOCK_MAX) {
    return 0;
  }
  for (int i = idx + 1; i < eu->dock_count; ++i) {
    eu->dock[i - 1] = eu->dock[i];
  }
  eu->dock_count--;
  if (eu->dock_count >= 0 && eu->dock_count < EUROPE_DOCK_MAX) {
    memset(&eu->dock[eu->dock_count], 0, sizeof(eu->dock[0]));
  }
  return 1;
}

/* Stock +ship_amt of cargo_type on ship holds, else +colony_amt to nearest own
 * colony (cap 100). Returns delivered. Cite: euro_unit_act §2d tools/lumber
 * cargo stand-in; 5cf6 shortage tallies. */
static int ai_euro_cargo_or_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship,
  int cargo_type,
  int ship_amt,
  int colony_amt
) {
  if (!ctx || !ship || ship_amt <= 0) {
    return 0;
  }
  int delivered = 0;
  if (units_goods_hold_count(ctx->units, ship->id) > 0) {
    delivered = units_load_goods(ctx->units, ship->id, cargo_type, ship_amt);
  }
  if (delivered <= 0 && ctx->colonies && colony_amt > 0) {
    ColonizeColony* nearest = NULL;
    int best_d = -1;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      const int d = abs(c->x - ship->x) + abs(c->y - ship->y);
      if (best_d < 0 || d < best_d) {
        nearest = c;
        best_d = d;
      }
    }
    if (nearest) {
      int stock = nearest->stock[cargo_type] + colony_amt;
      if (stock > 100) {
        stock = 100;
      }
      nearest->stock[cargo_type] = stock;
      delivered = colony_amt;
    }
  }
  return delivered;
}

static int ai_euro_tools_cargo_or_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  return ai_euro_cargo_or_colony(
    ctx, nation_id, ship, COLONIZE_CARGO_TOOLS, 20, 15
  );
}

/* ========================================================================
 * FUN_521d_5d04 — euro_nation_planning, structural port (2026-08-18)
 *
 * Ports the function's own control flow + arithmetic 1:1 from the raw
 * decompile (original_sources_decompiled/viceroy_unpacked_2.c:85822-86564)
 * for the part that's genuinely resolved (raw lines 85872-86064 — the
 * difficulty-scaled treasury bump and the Frigate/Man-O-War-threat /
 * no-ships / cargo-short gate cascade); callees stay stubbed throughout,
 * per request.
 *
 * Resolved this pass (cross-referenced against save_format_map.md /
 * col1_save.h / euro_g_table_0a60.md — see those for the trace): year/
 * turn/difficulty (col1->head), nation.gold (32-bit, direct — the DOS body
 * does manual 16-bit lo/hi carry arithmetic on nation+0x2a/+0x2c that the
 * already-32-bit Linux `gold` field doesn't need), the per-nation census
 * block (colony_counts/ship_counts/colony_pop_totals/armed_ship_counts/
 * ship_cargo_totals/census_pop_proxy — col1_save.h `ColonizeCol1Stuff`),
 * unit_type_counts[nation][16]/[17], DS:0x5382 bit0 — **confirmed** (live
 * DOSBox-X capture, 2026-08-18, `BPM 237D:5382`: `or byte [5382],01` fires
 * 00→01 exactly on Declare Independence) as `game_options.woi`, settling
 * the earlier conflict with an older, looser "NEW WORLD path" guess in
 * euro_dispatcher.c / SYMBOL_MAP.md for the same bit — that guess was
 * wrong, both docs corrected. FUN_281f_09fc(building_index)
 * confirmed elsewhere (euro_unit_act.md) as `has_building` indexed by
 * NAMES.TXT `@BUILDING` file order — index 0xd in that order is "College"
 * (cross-checked against that same doc's independently-confirmed index
 * 0x24 = "Lumber Mill"), FUN_281f_0808 confirmed as `destroy_unit`
 * (move_scoring_ship.md).
 *
 * Deliberately NOT re-ported this pass: raw lines 86065-86564 (the Europe
 * hire ladder + profession/reward loop tail, ~500 raw lines). Traced
 * enough to see it's the same mechanic `ai_euro.c`'s existing extensive
 * "5d04" thin-hire coverage (peace tools/wagon matrix, war Soldier/
 * Dragoon/Artillery hire, buy ladder, `AiEuroInventory.profession_demand`
 * already wired) already approximates — re-transcribing it blind risks
 * duplicating/conflicting with that tested behavior rather than adding
 * value, the same "scope down after inspection" call the 0a60 structural
 * pilot made. Its own callees (FUN_281f_07e0/02e4/0b78/0c9a/095c/0be6/
 * 0c68/0aec, FUN_291f_0b26/0afc/0c3e/09ea/0a2e/0ec2/0d8e/0dc6/0c14,
 * FUN_1d1d_0ec6, FUN_281f_0aba) stay unresolved.
 *
 * Live now: only the treasury bump (`ai_euro_5d04_treasury_bump`) replaces
 * the old function's simplified formula, called from both this function
 * and `ai_euro_nation_planning` below. The gate cascade
 * (`ai_euro_5d04_compute_flags`) is a faithful reference implementation —
 * computed, logged nowhere, not yet gating any mutation — since its two
 * real consumers (bVar5 feeds the deferred hire ladder; the destroy-unit
 * branch is a live-side-effecting op this first pass deliberately keeps
 * stubbed per "be safe").
 * ======================================================================== */

/* DS:0x9796/0x97a8/0x97ae — gold-floor candidates 5d04 uses to clamp a
 * nation's treasury up to a minimum ("catch-up" gold). Values confirmed
 * 2026-08-18 via live DOSBox-X data dump (`D 237D:9790`): 1000/2000/5000
 * respectively — round numbers, static EXE data (no write ever observed),
 * single sample not cross-checked across difficulty/scenario but the
 * round-decimal self-alignment across three different byte offsets makes
 * a coincidental misread very unlikely. */
static uint32_t ai_euro_5d04_ph_gold_floor(int which) {
  switch (which) {
    case 0x9796: return 1000; /* no_ships */
    case 0x97a8: return 2000; /* manowar_threatened */
    case 0x97ae: return 5000; /* frigate_threatened */
    default: return 0;
  }
}

/* DS:0xa89a/0xa89b/0x9e52/0x9e54 — writer mechanism fully traced and
 * live-confirmed 2026-08-19 (raw viceroy_unpacked.c:78243-78304 +
 * DOSBox-X captures, see census_tally.md phase 3 for the full write-up).
 * NOT a "rival" stat — it's this nation's own **(count, colony-level-sum)
 * of its own colonies currently within 5 tiles of a foreign warship**:
 * `0xa89b`/`0x9e52` = threatened by a **Frigate** specifically (the source
 * code literal-checks `unit+0x3146 == 0x11`, confirmed = Frigate against
 * NAMES.TXT `@UNIT` order); `0xa89a`/`0x9e54` = threatened by any other
 * qualifying armed ship. Per-nation-call-reset (not turn-accumulated)
 * confirmed both by the raw zero-out at the top of `FUN_4962_0018` and
 * live capture.
 *
 * The type-range (`0x0d..0x12`) and the `type==0x11`-vs-else bit split are
 * solid (literal code). The sub-gate (`FUN_2a1f_027e`) that has to pass
 * before any qualifying ship counts at all is now identified by reading
 * the source (not guessed from behavior): it thunks to `FUN_6662_0906`, a
 * **movement/pathfinding cost check** ("is there a short-enough navigable
 * route between ship and colony"), not a diplomacy gate — see
 * census_tally.md for the trace. That retroactively explains three live
 * captures that all fired regardless of diplomatic state (peace, alliance,
 * war all confirmed) — diplomacy was never the variable being tested.
 * Genuinely still open: whether Privateer specifically can pass the
 * pathfinding gate (one confirmed instrumented positive at peace, zero
 * confirmed instrumented negatives — an earlier "excluded" read came from
 * an unverified report and was retracted), and the underlying cost
 * function (`thunk_FUN_2a1f_05f0`)'s own formula. Still stubbed: the
 * source detector (`colony+0x1b`'s 11×11 ship-probe scan itself, plus
 * this pathfinding sub-gate) has no Linux equivalent computing it yet —
 * wiring this for real needs that first. Inert 0 in the meantime. */
static int ai_euro_5d04_ph_naval_threat_crumb(int which) {
  (void)which;
  return 0;
}

/* FUN_281f_09fc(building_index) on a scanned colony — "does this colony
 * have building #N" (mechanism confirmed, see header). Kept as a stub
 * rather than wired live: 5d04's own use of the result (bVar5) only feeds
 * the still-deferred hire-ladder section, so wiring it for real would be
 * effort spent on a value nothing downstream reads yet. */
static int ai_euro_5d04_stub_colony_has_building(
  const ColonizeColony* colony, int building_index
) {
  (void)colony;
  (void)building_index;
  return 0;
}

/* FUN_281f_0808 — destroy_unit (identity confirmed, see header). Real
 * mechanism, gating condition now also confirmed (DS:0x5382 bit0 = `woi`,
 * fires on Declare Independence — see header). Still stubbed: destroying
 * a live unit is exactly the side effect "be safe" argues against wiring
 * on a first pass, independent of the bit-identity question that's now
 * settled. Stub: no-op. */
static void ai_euro_5d04_stub_destroy_unit(int unit_id) {
  (void)unit_id;
}

/* First own unit matching the raw body's `(owner==nation) && (type==0x12)`
 * scan — fires post-independence (DS:0x5382 bit0 = `woi`, confirmed, see
 * header); "NEW WORLD wagon" in euro_dispatcher.c's older comment on this
 * call site described the gate, not the unit, and is superseded by the
 * `woi` finding. The unit-type identity itself is still genuinely
 * unresolved (no confirmed DOS-type-id -> Linux-type mapping for id
 * 0x12). Stub: "none found", so the destroy branch below it never fires. */
static int ai_euro_5d04_stub_find_new_world_wagon(
  const ColonizeTurnContext* ctx, int nation_id
) {
  (void)ctx;
  (void)nation_id;
  return -1;
}

/*
 * Raw decomp 85878-85899 (verbatim control flow): difficulty-scaled
 * per-turn treasury bump. local_12 = colony_counts[n] + (year-1500)/50,
 * zeroed before turn 20, doubled past year 1699; local_2e = difficulty *
 * local_12, scaled *1.5 at difficulty 3, *2 at difficulty 4; gold +=
 * local_2e*4.
 */
static void ai_euro_5d04_treasury_bump(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  const ColonizeCol1Head* head = &ctx->col1->head;
  const int colony_count = ctx->col1->stuff.colony_counts[nation_id];

  int local_12 = colony_count + ((int)head->year - 1500) / 50;
  if ((int)head->turn < 20) {
    local_12 = 0;
  }
  if ((int)head->year > 0x6a3) { /* 1699 */
    local_12 <<= 1;
  }
  int local_2e = (int)head->difficulty * local_12;
  if (head->difficulty == 3) {
    local_2e = (local_2e >> 1) + local_2e; /* *1.5 */
  } else if (head->difficulty == 4) {
    local_2e <<= 1; /* *2 */
  }
  nat->gold += (uint32_t)(local_2e * 4);
}

static int nat_gold_ge(const ColonizeTurnContext* ctx, int nation_id, uint32_t threshold) {
  return ctx->col1->nation[nation_id].gold >= threshold;
}

/* Raw decomp 85973-86002: raise gold to `floor` if it's currently lower
 * (a per-flag "catch-up" clamp). Real mechanism, inert while
 * `ai_euro_5d04_ph_gold_floor` returns 0. */
static void ai_euro_5d04_apply_gold_floor(ColonizeTurnContext* ctx, int nation_id, uint32_t floor) {
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  if (nat->gold < floor) {
    nat->gold = floor;
  }
}

/* Raw decomp 85900-86064 gate cascade — computed reference values, not yet
 * wired to a mutation (see file header). bVar5/no_ships/frigate_threatened/
 * manowar_threatened/no_clear_navy/cargo_short name the raw bVar5/21/22/23/
 * 24/7 booleans in call order. `frigate_threatened`/`manowar_threatened`
 * were named `weak_vs_euro`/`weak_vs_indian` until 2026-08-19 — renamed once
 * the underlying `0xa89a`-family crumbs turned out to mean "own colonies
 * near a Frigate/Man-O-War," not a rival-strength comparison at all (see
 * `ai_euro_5d04_ph_naval_threat_crumb`'s header). The higher-level
 * "weak/catch-up" framing for what 5d04 *does* with these two flags
 * (gold-floor clamp, gate on turn/gold thresholds) may still be roughly
 * right — that part of the raw logic itself didn't change, only what the
 * inputs feeding it actually measure. */
typedef struct Ai5d04PlanningFlags {
  int has_college;       /* bVar5 */
  int no_ships;           /* bVar21 */
  int frigate_threatened;  /* bVar22 */
  int manowar_threatened;   /* bVar23 */
  int no_clear_navy;         /* bVar24 */
  int cargo_short;             /* bVar7 */
} Ai5d04PlanningFlags;

static Ai5d04PlanningFlags ai_euro_5d04_compute_flags(
  ColonizeTurnContext* ctx, int nation_id
) {
  Ai5d04PlanningFlags f;
  memset(&f, 0, sizeof(f));
  if (!ctx || !ctx->col1 || !ctx->colonies || nation_id < 0 || nation_id >= 4) {
    return f;
  }
  const ColonizeCol1Head* head = &ctx->col1->head;
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int turn = (int)head->turn;
  const int woi = head->game_options.woi != 0;

  /* bVar5: any own colony has building index 0xd (College). */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (c->active && c->nation_id == nation_id &&
        ai_euro_5d04_stub_colony_has_building(c, 0xd)) {
      f.has_college = 1;
      break;
    }
  }

  /* WoI-bit-gated scan: destroy this nation's first "NEW WORLD wagon" if
   * one exists (raw 85909-85921). The raw body also does
   * `*(int*)0x53de = *(int*)0x53de + 1` here — **correction, 2026-08-18**:
   * a live DOSBox-X capture caught `0x53DE` being `dec`'d, by an unrelated
   * function, right as each Royal Expeditionary Force wave lands post-
   * independence; `0x53DA/DC/DE/E0` line up exactly with `col1_save.h`'s
   * already-documented `expeditionary_force[4]` (regulars/dragoons/
   * man-o-wars/artillery) — `0x53DE` = index 2, man-o-wars. So this is
   * very likely NOT an opaque "goal counter" at all, either a genuine odd
   * DOS reuse (destroying this unit nudges the REF man-o-war pool) or a
   * Ghidra misattribution in this specific spot (same corruption class as
   * other `521d`-segment functions elsewhere in this project) — not
   * re-verified against a fresh disassembly, so the increment is dropped
   * either way rather than risk mutating REF state on a guess. */
  if (woi) {
    const int wagon = ai_euro_5d04_stub_find_new_world_wagon(ctx, nation_id);
    if (wagon >= 0) {
      ai_euro_5d04_stub_destroy_unit(wagon);
    }
  }

  f.no_ships = stuff->ship_counts[nation_id] == 0;

  /* local_3c: avg (over the other 3 nations) of unit_type_counts[.][16] +
   * unit_type_counts[.][17]*4, >>2 (raw divides by 4 regardless of the
   * 3-term sum — kept literal). Zeroed under WoI. */
  int local_3c = 0;
  for (int n = 0; n < 4; ++n) {
    if (n == nation_id) {
      continue;
    }
    local_3c += stuff->unit_type_counts[n][16] + stuff->unit_type_counts[n][17] * 4;
  }
  local_3c >>= 2;
  if (woi) {
    local_3c = 0;
  }

  const int col_half = stuff->colony_counts[nation_id] >> 1;
  const int pop_half = stuff->colony_pop_totals[nation_id] >> 1;
  const int focus_nation = head->human_player;
  const int focus_ok = focus_nation >= 0 && focus_nation < 4;

  /* bVar22: gated on Frigate-threatened own colonies (raw 85934-85952).
   * `0xa89b`/`0x9e52` = Frigate-threat (count, level-sum) — see
   * `ai_euro_5d04_ph_naval_threat_crumb` header for how that's confirmed. */
  f.frigate_threatened = 0;
  if (!((ai_euro_5d04_ph_naval_threat_crumb(0xa89b) == 0 &&
         ai_euro_5d04_ph_naval_threat_crumb(0xa89a) == 0) ||
        local_3c == 0)) {
    int reach_weak_check = 0;
    if (ai_euro_5d04_ph_naval_threat_crumb(0xa89b) < col_half &&
        ai_euro_5d04_ph_naval_threat_crumb(0x9e52) < pop_half) {
      if (turn > 200 && nat_gold_ge(ctx, nation_id, 2000)) {
        reach_weak_check = 1;
      }
    } else {
      reach_weak_check = 1;
    }
    if (reach_weak_check && focus_ok) {
      f.frigate_threatened = stuff->unit_type_counts[nation_id][17] == 0 &&
                              stuff->unit_type_counts[focus_nation][17] != 0;
    }
  }

  /* bVar23: gated on "other-armed-ship"-threatened own colonies (raw
   * 85953-85971). `0xa89a`/`0x9e54` = count/level-sum for any qualifying
   * ship type other than Frigate — Man-O-War confirmed to reach this bit;
   * whether Privateer ever does is unsettled (see
   * `ai_euro_5d04_ph_naval_threat_crumb`'s header — the sub-gate that
   * decides "qualifying at all" is non-deterministic, not a fixed type
   * exclusion). Field kept named `manowar_threatened` since that's the
   * confirmed/primary case, not a claim it's Man-O-War-exclusive. */
  f.manowar_threatened = 0;
  if (((ai_euro_5d04_ph_naval_threat_crumb(0xa89a) != 0 ||
        ai_euro_5d04_ph_naval_threat_crumb(0xa89b) != 0)) &&
      local_3c != 0 && !f.frigate_threatened) {
    int reach = 0;
    if (ai_euro_5d04_ph_naval_threat_crumb(0xa89a) < col_half &&
        ai_euro_5d04_ph_naval_threat_crumb(0x9e54) < pop_half) {
      if (turn > 100 && nat_gold_ge(ctx, nation_id, 1000)) {
        reach = 1;
      }
    } else {
      reach = 1;
    }
    if (reach && focus_ok) {
      f.manowar_threatened = stuff->unit_type_counts[nation_id][16] < 2 &&
                              stuff->unit_type_counts[focus_nation][16] != 0;
    }
  }

  /* bVar24: no clear strongest navy (raw 86003-86024). */
  uint8_t max_armed = 0;
  for (int n = 0; n < 4; ++n) {
    if (stuff->armed_ship_counts[n] > max_armed) {
      max_armed = stuff->armed_ship_counts[n];
    }
  }
  int tied = 0;
  for (int n = 0; n < 4; ++n) {
    if (stuff->armed_ship_counts[n] == max_armed) {
      tied++;
    }
  }
  f.no_clear_navy =
    f.frigate_threatened || stuff->armed_ship_counts[nation_id] < max_armed || tied > 1;

  /* bVar7: cargo/passenger space short (raw 86025-86029). */
  f.cargo_short =
    stuff->ship_cargo_totals[nation_id] <= ((pop_half + stuff->colony_counts[nation_id] * 2) >> 1) &&
    !woi;

  return f;
}

/* Gold-floor-max: raise gold to a per-scenario candidate under each threat
 * flag (raw 85973-86002, was inline inside `ai_euro_5d04_compute_flags`
 * until 2026-08-19 — split out once the floor values went from stubbed-0
 * to real (1000/2000/5000): baking a genuine mutation into a function
 * whose result callers were discarding as "reference-only" was exactly
 * the bug that regressed `unit_ai_euro_war` the first time (see memory).
 * `compute_flags` is pure again; this is the deliberate, explicit
 * mutation step, still not called from the live path — a future wiring
 * pass calls this alongside `compute_flags`, not from inside it. */
static void ai_euro_5d04_apply_naval_gold_floors(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f
) {
  if (f->no_ships) {
    ai_euro_5d04_apply_gold_floor(ctx, nation_id, ai_euro_5d04_ph_gold_floor(0x9796));
  }
  if (f->manowar_threatened) {
    ai_euro_5d04_apply_gold_floor(ctx, nation_id, ai_euro_5d04_ph_gold_floor(0x97a8));
  }
  if (f->frigate_threatened) {
    ai_euro_5d04_apply_gold_floor(ctx, nation_id, ai_euro_5d04_ph_gold_floor(0x97ae));
  }
}

/* ---- FUN_521d_5d04 tail: Europe hire ladder (raw 86030-86564) --------
 * Finishing the structural port (2026-08-19) — the part scoped out
 * earlier as "overlaps existing thin coverage, callees genuinely
 * unresolved." Ported anyway per request: control flow and arithmetic
 * 1:1 where resolved, every callee stubbed (inert defaults) per the
 * original brief. NOT wired into the live path — a complete reference
 * implementation alongside `ai_euro_nation_planning`, same posture as
 * the gate-cascade section above. Most of this body is naturally inert
 * at runtime (unit-iteration stubs return "none found"), which is safe
 * by construction, not a workaround — see each stub's own comment. */

/* thunk_FUN_2a1f_0500(type_id, weight_pct) — propose a Europe ship-buy
 * candidate. Type ids 1-5 seen (Caravel..Frigate, 5 buyable ship types —
 * Man-O-War isn't purchasable in real Colonization either); weight is a
 * priority percentage. Stub: never proposes (0 = no candidate), so the
 * raw body's `if (local_3e==0 && flag) return;` early-outs are inert in
 * practice too, since the flags feeding them (frigate_threatened/
 * manowar_threatened) are themselves always false while
 * `ai_euro_5d04_ph_naval_threat_crumb` stays stubbed at 0 — not a special
 * case, just how the whole chain composes safely. */
static int ai_euro_5d04_stub_propose_ship_buy(int type_id, int weight_pct) {
  (void)type_id;
  (void)weight_pct;
  return 0;
}

/*
 * Raw 86030-86064: ship-buy candidate ladder, gated on `!woi &&
 * ship_cargo_totals[n] <= census_pop_proxy[n]/2 + colony_counts[n]`.
 * Returns the raw body's `local_3e` (0 = no candidate). `*out_abort` is
 * set when the raw code's early `return;` would fire (frigate/manowar
 * threatened and still no candidate after trying) — always true when it
 * would fire given the stub above, but only reachable at all when those
 * flags are true, which is itself gated on the still-stubbed naval-threat
 * crumbs (see above) — inert end to end today, faithful if that crumb
 * function ever gets wired to something real.
 */
static int ai_euro_5d04_ship_buy_ladder(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f, int* out_abort
) {
  *out_abort = 0;
  const ColonizeCol1Head* head = &ctx->col1->head;
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int woi = head->game_options.woi != 0;
  const int pop_half = stuff->colony_pop_totals[nation_id] >> 1;
  if (woi || stuff->ship_cargo_totals[nation_id] >
             (uint32_t)(pop_half + stuff->colony_counts[nation_id])) {
    return 0;
  }
  int candidate = 0;
  if (f->frigate_threatened) {
    candidate = ai_euro_5d04_stub_propose_ship_buy(5, 100);
  }
  if (candidate == 0 && f->frigate_threatened) {
    *out_abort = 1;
    return 0;
  }
  if (f->manowar_threatened) {
    candidate = ai_euro_5d04_stub_propose_ship_buy(4, 100);
  }
  if (candidate == 0 && f->manowar_threatened) {
    *out_abort = 1;
    return 0;
  }
  if (candidate == 0 && stuff->armed_ship_counts[nation_id] < 8 &&
      dos_rng_range(ctx->rng, 0, 1) != 0 && f->no_clear_navy) {
    candidate = ai_euro_5d04_stub_propose_ship_buy(5, 0x23);
  }
  if (candidate == 0 && dos_rng_range(ctx->rng, 0, 3) != 0) {
    candidate = ai_euro_5d04_stub_propose_ship_buy(3, 0x32);
  }
  if (candidate == 0 && dos_rng_range(ctx->rng, 0, 1) == 0 &&
      stuff->ship_cargo_totals[nation_id] < 0xc) {
    candidate = ai_euro_5d04_stub_propose_ship_buy(2, 0x14);
  }
  if (candidate == 0 && stuff->ship_cargo_totals[nation_id] < 3) {
    candidate = ai_euro_5d04_stub_propose_ship_buy(1, 0x14);
  }
  if (candidate == 0 && stuff->armed_ship_counts[nation_id] < 4 &&
      dos_rng_range(ctx->rng, 0, 3) == 0 && f->no_clear_navy && !f->cargo_short) {
    candidate = ai_euro_5d04_stub_propose_ship_buy(4, 0x1e);
  }
  return candidate;
}

/* --- The hire-ladder tail's own callees (real as of 2026-08-27) ---------
 * Identity table (address_mapping.csv chain, FUNCTION_CATALOG, 38fd_0718 /
 * 521d_6d8e / 5bfb_00f8 decompiles):
 *   FUN_281f_07e0 / 02e4   head unit on the nation's Europe tile (x = y =
 *                          nation-0x14, the sentinel FUN_281f_095c spawns
 *                          at) / next unit in that stack -> "units of this
 *                          nation in Europe", iterated by pool index.
 *   unit+0x3146            unit type (0xd..0x12 = ships; 1 Soldier, 2
 *                          Pioneer, 3 Missionary, 4 Dragoon) — the tail
 *                          "dispatch byte" writes are Europe equip changes.
 *   unit+0x315b            profession (@JOB index, 0x1c = none).
 *   FUN_281f_0c9a          expert gate: 0 for {0x13,0x19,0x1a,0x1b,0x1c}.
 *   FUN_281f_0b78          DS:0x30e[type] >= 0 — type has a profession slot
 *                          (colonist-class types 0..9).
 *   thunk_FUN_2a1f_0494    FUN_521d_03d0 founding_expansion_urgency.
 *   FUN_291f_0b26 / 0afc   FUN_38fd_0718 recruit spawn (profession-coded,
 *                          Vet. Soldier 1-in-(diff+5) becomes a Dragoon,
 *                          Pioneer gets 100 tools) / FUN_38fd_46d4 next
 *                          recruit profession (the +0x44/+0x45 remap table
 *                          is not ported — a plain RNG pick stands in).
 *   FUN_291f_0c3e / 09ea   Europe buy price of a cargo (nation market).
 *   FUN_291f_0c14 / 0a2e   market buy / sell volume bookkeeping.
 *   FUN_281f_08bc(head,4/0xc/0xe)  FUN_1427_0d38 stack queries: 4 = land
 *                          units waiting, 0xc = ships, 0xe = Σ passenger
 *                          capacity; the register-arg call = Artillery on
 *                          the dock.
 *   FUN_281f_095c(0xb,..)  spawn an Artillery in Europe.
 *   FUN_281f_0be6/0c68/0aec hold-0 cargo type / amount / remove.
 *   FUN_291f_0dc6 + 0aba   sell 100 of hold 0, credit the nation's gold.
 *   FUN_291f_0d8e          buy+load 100 of a cargo onto the ship.
 *   FUN_291f_0ec2          ship departs Europe: the dock stack boards, the
 *                          ship leaves the Europe list (the dispatcher's
 *                          own FUN_48d3_048e teleport places it on the
 *                          high seas next act).
 *   0x5238[type]           ColonizeUnitType.space (ship slots taken).
 *   DS:0xa0db / 0xa0da     per-turn counts from FUN_521d_6d8e's prelude:
 *                          own colonies with specialty muskets or an empty
 *                          muskets stock / with an empty tools stock.
 */
static ColonizeTurnContext* s_5d04_ctx = NULL;
static int s_5d04_nation = -1;

static int ai_euro_5d04_cb_in_europe_list(int idx) {
  if (!s_5d04_ctx || !s_5d04_ctx->units || idx < 0 || idx >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  const ColonizeUnit* u = &s_5d04_ctx->units->units[idx];
  return u->active && u->nation_id == s_5d04_nation && ai_euro_in_europe(u->x, u->y);
}
static int ai_euro_5d04_cb_list_iter_next(int prev) {
  for (int i = prev + 1; i < COLONIZE_UNITS_MAX; ++i) {
    if (ai_euro_5d04_cb_in_europe_list(i)) {
      return i;
    }
  }
  return -1;
}
static int ai_euro_5d04_cb_list_iter_first(int list_id) {
  (void)list_id;
  return ai_euro_5d04_cb_list_iter_next(-1);
}
static ColonizeUnit* ai_euro_5d04_cb_unit(int idx) {
  if (!s_5d04_ctx || !s_5d04_ctx->units || idx < 0 || idx >= COLONIZE_UNITS_MAX) {
    return NULL;
  }
  return &s_5d04_ctx->units->units[idx];
}
/* DOS @UNIT code for a Linux type (name/domain based — fixtures use small
 * synthetic pools, so pool indices are not DOS indices). 0xff = unknown. */
static int ai_euro_5d04_dos_type_of(const ColonizeUnitPool* pool, int type_index) {
  const ColonizeUnitType* t = units_type(pool, type_index);
  if (!t) {
    return 0xff;
  }
  if (t->domain == COLONIZE_UNIT_DOMAIN_SEA) {
    return strstr(t->name, "Privateer") ? 0x10 : 0xd;
  }
  const char* n = t->name;
  if (strstr(n, "Artillery") || strstr(n, "Cannon")) return 0xb;
  if (strstr(n, "Wagon")) return 0xc;
  if (strstr(n, "Treasure")) return 0xa;
  if (strstr(n, "Dragoon")) return 4;
  if (strstr(n, "Scout")) return 5;
  if (strstr(n, "Pioneer") || strstr(n, "Hardy")) return 2;
  if (strstr(n, "Missionar")) return 3;
  if (strstr(n, "Soldier")) return 1;
  if (strstr(n, "Colonist")) return 0;
  if (strstr(n, "Regular")) return 6;
  if (strstr(n, "Cav")) return 8;
  if (strstr(n, "Cont") || strstr(n, "Continental")) return 7;
  if (strstr(n, "Brave") || strstr(n, "Warrior")) return 19;
  return 0xff;
}
static int ai_euro_5d04_linux_type_for(const ColonizeUnitPool* pool, int dos_code) {
  static const char* const k_names[6][3] = {
    {"Colonists", "Free Colonist", "Colonist"},
    {"Soldiers", "Soldier", NULL},
    {"Pioneers", "Pioneer", NULL},
    {"Missionaries", "Missionary", NULL},
    {"Dragoons", "Dragoon", NULL},
    {"Scouts", "Scout", NULL},
  };
  if (dos_code == 0xb) {
    int t = units_find_type(pool, "Artillery");
    return t >= 0 ? t : units_find_type(pool, "Cannon");
  }
  if (dos_code < 0 || dos_code > 5) {
    return -1;
  }
  for (int i = 0; i < 3 && k_names[dos_code][i]; ++i) {
    const int t = units_find_type(pool, k_names[dos_code][i]);
    if (t >= 0) {
      return t;
    }
  }
  return -1;
}
static int ai_euro_5d04_cb_unit_dispatch_byte(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  return u ? ai_euro_5d04_dos_type_of(s_5d04_ctx->units, u->type_index) : -1;
}
static void ai_euro_5d04_cb_set_unit_dispatch_byte(int idx, int value) {
  ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  const int lt = u ? ai_euro_5d04_linux_type_for(s_5d04_ctx->units, value) : -1;
  if (!u || lt < 0) {
    return;
  }
  u->type_index = lt;
  if (value == 1) {
    u->muskets = 50;
    u->horses = 0;
  } else if (value == 4) {
    u->muskets = 50;
    u->horses = 50;
  } else if (value == 2) {
    if (u->tools < 100) {
      u->tools = 100;
    }
  }
}
static int ai_euro_5d04_cb_unit_profession(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  return u ? u->profession : 0x1c;
}
static void ai_euro_5d04_cb_set_unit_profession(int idx, int value) {
  ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (u) {
    u->profession = value;
  }
}
static int ai_euro_5d04_cb_wagon_query(int head) {
  (void)head; /* register-arg stack query: Artillery on the dock */
  int n = 0;
  for (int i = ai_euro_5d04_cb_list_iter_first(0); i >= 0; i = ai_euro_5d04_cb_list_iter_next(i)) {
    if (ai_euro_5d04_cb_unit_dispatch_byte(i) == 0xb) {
      n++;
    }
  }
  return n;
}
static int ai_euro_5d04_cb_nation_hire_mask(int nation_id) {
  const int total = s_5d04_ctx && s_5d04_ctx->colonies ? s_5d04_ctx->colonies->colony_count : 0;
  return ai_goals_founding_expansion_urgency(nation_id, total);
}
static int ai_euro_5d04_cb_unit_is_skilled(int idx) {
  const int t = ai_euro_5d04_cb_unit_dispatch_byte(idx);
  return (t >= 0 && t <= 9) ? 0 : -1;
}
static int ai_euro_5d04_cb_profession_gate(int profession) {
  return (profession == 0x13 || (profession >= 0x19 && profession <= 0x1c)) ? 0 : 1;
}
/* FUN_38fd_0718: spawn the recruit in Europe (gold already paid by the caller). */
static int ai_euro_5d04_cb_dock_pop_candidate(int profession) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (!ctx || !ctx->units || !ctx->col1) {
    return -1;
  }
  int type = 0;
  if (profession == 0x14) {
    type = 2;
  } else if (profession == 0x18) {
    type = 3;
  } else if (profession == 0x16) {
    type = 5;
  } else if (profession == 0x15) {
    type = 1;
    const int span = (s_5d04_nation == ctx->human_nation) ? (int)ctx->col1->head.difficulty : 1;
    if (dos_rng_range(ctx->rng, 0, span + 4) == 0) {
      type = 4;
    }
  }
  const int lt = ai_euro_5d04_linux_type_for(ctx->units, type);
  if (lt < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(ctx->units, lt, 200, 100);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, s_5d04_nation);
  u->moves_left = 0;
  u->profession = profession;
  if (type == 2) {
    u->tools = 100;
  } else if (type == 1) {
    u->muskets = 50;
  } else if (type == 4) {
    u->muskets = 50;
    u->horses = 50;
  }
  return (int)(u - ctx->units->units);
}
static int ai_euro_5d04_cb_dock_peek_type(int x) {
  (void)x;
  return s_5d04_ctx ? dos_rng_range(s_5d04_ctx->rng, 0, 0x1b) : 0x13;
}
/* Europe market quotes: the EuropeScreen (the one Linux market) when present,
 * else the nation's col1 euro_price byte (+1 spread). */
static int ai_euro_5d04_cb_bid(int cargo) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (!ctx || !ctx->col1 || cargo < 0 || cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  if (ctx->europe && cargo < ctx->europe->cargo_count && ctx->europe->cargo[cargo].bid > 0) {
    return ctx->europe->cargo[cargo].bid;
  }
  return (int)ctx->col1->nation[s_5d04_nation].trade.euro_price[cargo];
}
static int ai_euro_5d04_cb_price(int cargo) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (!ctx || !ctx->col1 || cargo < 0 || cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 1;
  }
  if (ctx->europe && cargo < ctx->europe->cargo_count && ctx->europe->cargo[cargo].ask > 0) {
    return ctx->europe->cargo[cargo].ask;
  }
  return (int)ctx->col1->nation[s_5d04_nation].trade.euro_price[cargo] + 1;
}
static void ai_euro_5d04_cb_sync_gold(void) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (ctx && ctx->europe && ctx->col1 && s_5d04_nation == ctx->human_nation) {
    ctx->europe->gold = (int)ctx->col1->nation[s_5d04_nation].gold;
  }
}
static void ai_euro_5d04_cb_market_volume(int cargo, int qty, int is_buy) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (ctx && ctx->europe && s_5d04_nation == ctx->human_nation) {
    europe_apply_volume_price(ctx->europe, cargo, qty, is_buy);
  }
}
static void ai_euro_5d04_cb_set_pool_counter(int cargo, int qty) {
  ai_euro_5d04_cb_market_volume(cargo, qty, 1);
}
static int ai_euro_5d04_cb_colony_demand_query(int head, int mode) {
  (void)head;
  int n = 0;
  for (int i = ai_euro_5d04_cb_list_iter_first(0); i >= 0; i = ai_euro_5d04_cb_list_iter_next(i)) {
    const ColonizeUnit* u = ai_euro_5d04_cb_unit(i);
    const int is_ship = units_is_sea(s_5d04_ctx->units, u->id);
    if (mode == 4) {
      n += (!is_ship && u->aboard_ship_id < 0) ? 1 : 0;
    } else if (mode == 0xc) {
      n += is_ship ? 1 : 0;
    } else if (mode == 0xe) {
      n += is_ship ? units_ship_capacity(s_5d04_ctx->units, u->id) : 0;
    }
  }
  return n;
}
static int ai_euro_5d04_cb_reward_case(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (!u || u->hold_goods_amount[0] <= 0) {
    return -1;
  }
  return u->hold_goods_type[0];
}
static int ai_euro_5d04_cb_reward_value(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  return u ? u->hold_goods_amount[0] : 0;
}
static void ai_euro_5d04_cb_reward_ack(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (u) {
    (void)units_unload_goods_hold(s_5d04_ctx->units, u->id, 0, NULL, NULL);
  }
}
/* FUN_291f_0dc6(unit, 0, 100) + FUN_281f_0aba: sell hold 0, credit the nation. */
static int ai_euro_5d04_cb_sell_hold0(int idx) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (!ctx || !u || u->hold_goods_amount[0] <= 0) {
    return 0;
  }
  /* Boycotted cargo stays aboard (europe_cargo_boycotted / boycott_bitmap). */
  {
    const int c0 = u->hold_goods_type[0];
    const int boycotted =
      (ctx->europe && s_5d04_nation == ctx->human_nation)
        ? europe_cargo_boycotted(ctx->europe, c0)
        : (c0 >= 0 && c0 < 16 &&
           (ctx->col1->nation[s_5d04_nation].boycott_bitmap & (1u << c0)) != 0);
    if (boycotted) {
      return 0;
    }
  }
  int cargo = -1;
  int amount = 0;
  if (units_unload_goods_hold(ctx->units, u->id, 0, &cargo, &amount) <= 0 || cargo < 0) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[s_5d04_nation];
  const int tax = nat->tax_rate;
  const int gained = (ai_euro_5d04_cb_bid(cargo) * amount * (100 - tax)) / 100;
  if (gained > 0) {
    nat->gold += (uint32_t)gained;
    if ((unsigned)cargo < COLONIZE_COL1_CARGO_TYPES) {
      nat->trade.tons[cargo] += amount;
      nat->trade.gold[cargo] += gained;
    }
    ai_euro_5d04_cb_sync_gold();
  }
  ai_euro_5d04_cb_market_volume(cargo, amount, 0);
  return 1;
}
/* FUN_291f_0ec2: the ship departs — waiting dock units board first. */
static void ai_euro_5d04_cb_unit_exhaust(int idx) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  ColonizeUnit* ship = ai_euro_5d04_cb_unit(idx);
  if (!ctx || !ship || !units_is_sea(ctx->units, ship->id)) {
    return;
  }
  const int ship_id = ship->id;
  for (int i = ai_euro_5d04_cb_list_iter_first(0); i >= 0; i = ai_euro_5d04_cb_list_iter_next(i)) {
    ColonizeUnit* u = ai_euro_5d04_cb_unit(i);
    if (u->aboard_ship_id >= 0 || units_is_sea(ctx->units, u->id)) {
      continue;
    }
    const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
    if (!t || t->space >= 99) {
      continue;
    }
    ColonizeUnit* sh = units_get(ctx->units, ship_id);
    if (!sh || sh->cargo_count >= units_ship_capacity(ctx->units, ship_id)) {
      break;
    }
    (void)units_board_stacked(ctx->units, u->id, ship_id);
  }
  /* The dispatcher's Europe act (FUN_48d3_048e teleport) takes it from here. */
}
/* FUN_291f_0d8e(unit, cargo, 100): buy 100 of cargo onto the ship. */
static void ai_euro_5d04_cb_apply_bump(int idx, int cargo, int qty) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (!ctx || !u) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[s_5d04_nation];
  const uint32_t cost = (uint32_t)(ai_euro_5d04_cb_price(cargo) * qty);
  if (nat->gold < cost) {
    return;
  }
  const int loaded = units_load_goods(ctx->units, u->id, cargo, qty);
  if (loaded <= 0) {
    return;
  }
  nat->gold -= (uint32_t)(ai_euro_5d04_cb_price(cargo) * loaded);
  ai_euro_5d04_cb_sync_gold();
  ai_euro_5d04_cb_market_volume(cargo, loaded, 1);
}
/* FUN_281f_095c(0xb, nation, nation-0x14, nation-0x14): Artillery in Europe. */
static int ai_euro_5d04_cb_goal_trigger(int code, int a, int b, int c) {
  (void)a;
  (void)b;
  (void)c;
  ColonizeTurnContext* ctx = s_5d04_ctx;
  const int lt = ctx && ctx->units ? ai_euro_5d04_linux_type_for(ctx->units, code) : -1;
  if (lt < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(ctx->units, lt, 200, 100);
  ColonizeUnit* u = id >= 0 ? units_get(ctx->units, id) : NULL;
  if (!u) {
    return -1;
  }
  units_set_nation(u, s_5d04_nation);
  u->moves_left = 0;
  return (int)(u - ctx->units->units);
}
/* FUN_521d_6d8e prelude: DS:0xa0db / 0xa0da per-turn colony-need counts. */
static void ai_euro_5d04_cb_colony_needs(int nation_id, int* out_muskets, int* out_tools) {
  int m = 0;
  int t = 0;
  if (s_5d04_ctx && s_5d04_ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &s_5d04_ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (c->specialty_cargo == COLONIZE_CARGO_MUSKETS) {
        m++;
      }
      if (c->stock[COLONIZE_CARGO_MUSKETS] == 0) {
        m++;
      }
      if (c->stock[COLONIZE_CARGO_TOOLS] == 0) {
        t++;
      }
    }
  }
  *out_muskets = m;
  *out_tools = t;
}

/*
 * Opaque per-nation scratch for the raw body's `nation+0x48/0x49/0x4a`
 * (a crosses/hammers-pool carry mechanic — genuine arithmetic, see the
 * normalization loop below) and the two Europe-dock "training slot"
 * counters at `DS:0xa0da`/`0xa0db`. NOT reused from the real
 * `ColonizeCol1Nation` struct: `nation+0x48/0x49/0x4a` DOS-collides with
 * `col1_save.h`'s already-wired "Linux diplo stand-ins" union
 * (`indian_hostility_sticky`/`privateer_spawn_mask`/`unknown26_pad`) at
 * the same offset — same reasoning as the `0x53de` correction earlier
 * this session: don't reuse a live field on an unconfirmed reading.
 */
typedef struct Ai5d04HireScratch {
  int8_t delay_48;              /* DS nation+0x48 */
  int8_t crosses_bank_whole;    /* DS nation+0x49 */
  int32_t crosses_bank_raw;     /* DS nation+0x4a */
  int8_t training_slots_tools;   /* DS 0xa0db: own colonies needing muskets (per turn) */
  int8_t training_slots_crosses; /* DS 0xa0da: own colonies needing tools (per turn) */
} Ai5d04HireScratch;
static Ai5d04HireScratch s_5d04_hire_scratch[4];

/*
 * Raw 86065-86561 — Europe hire ladder + profession/reward tail, one
 * function body (not split across helpers): the raw locals thread across
 * all three phases in one scope, and splitting them into separate C
 * functions would risk a state-threading mistake. `goto` labels below
 * are abbreviated from the raw `LAB_521d_XXXX` names so this can be
 * cross-checked against the decompile directly. First-draft quality —
 * this is a large, dense transcription; expect bugs in the deep nested
 * arithmetic even where the shape is right, same standard the rest of
 * this project's large first-pass ports were held to.
 */
static void ai_euro_5d04_hire_ladder_tail(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f
) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  const ColonizeCol1Head* head = &ctx->col1->head;
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int turn = (int)head->turn;
  const int difficulty = (int)head->difficulty;
  const int woi = head->game_options.woi != 0;
  Ai5d04HireScratch* hs = &s_5d04_hire_scratch[nation_id];
  const AiEuroInventory* inv = ai_goals_inventory(nation_id);
  s_5d04_ctx = ctx;
  s_5d04_nation = nation_id;
  {
    int need_m = 0;
    int need_t = 0;
    ai_euro_5d04_cb_colony_needs(nation_id, &need_m, &need_t);
    hs->training_slots_tools = (int8_t)(need_m > 127 ? 127 : need_m);
    hs->training_slots_crosses = (int8_t)(need_t > 127 ? 127 : need_t);
  }

  /* raw 86066-86075. */
  int local_16 = ai_euro_5d04_cb_list_iter_first(0x0c);
  int local_34 = ai_euro_5d04_cb_wagon_query(local_16);
  if (local_34 == 0 && !woi && hs->training_slots_tools > 0 &&
      dos_rng_range(ctx->rng, 0, 3) == 0 && !f->cargo_short &&
      stuff->ship_cargo_totals[nation_id] > 4) {
    ai_euro_5d04_stub_propose_ship_buy(0, 0x14);
    local_16 = ai_euro_5d04_cb_list_iter_first(0x0c);
    local_34 = ai_euro_5d04_cb_wagon_query(local_16);
  }

  /* raw 86076-86084: local_8 = per-nation hire-mask; bVar8 = any unit in
   * the 0xc list whose dispatch byte falls outside the ship range. */
  int local_8 = ai_euro_5d04_cb_nation_hire_mask(nation_id);
  int bVar8 = 0;
  {
    int idx = local_16;
    while (idx >= 0) {
      const int dispatch = ai_euro_5d04_cb_unit_dispatch_byte(idx);
      if (dispatch < 0xd || dispatch > 0x12) {
        bVar8 = 1;
      }
      idx = ai_euro_5d04_cb_list_iter_next(idx);
    }
  }

  /* raw 86085-86092: fresh local booleans — DOS reuses the same stack
   * slots `bVar21`/`bVar22`/`bVar23`/`bVar24` for a NEW meaning here,
   * unrelated to the gate-cascade flags of the same raw names earlier in
   * the function; fresh C names to avoid confusion with `f->*`. */
  const int every_third_turn = (turn % 3) == 0;
  /* unit+0x3148 bit 0x20 of the last list-walk cursor, which is -1 (past
   * end) by the time this reads it in the raw body — an artifact of
   * DOS's register reuse, not a meaningful read. Structural placeholder. */
  const int unit_flag_bit5 = 0;
  const int has_any_colony = stuff->colony_counts[nation_id] != 0;
  const int found_flags = inv ? inv->found_flags : 0; /* -0x5f48 */
  const int expand_signal = has_any_colony && (unit_flag_bit5 || every_third_turn);

  /* raw 86089-86121: gold-spend recruit-slot swap. */
  if (!woi && !bVar8 && !f->cargo_short &&
      (!has_any_colony ||
       (!unit_flag_bit5 && !every_third_turn &&
        (stuff->colony_counts[nation_id] >> 1) <=
          found_flags - stuff->free_colonist_counts[nation_id]))) {
    const int base = ((int)nat->recruit_count - difficulty + 7) * 20;
    const long scaled =
      ((long)base * (long)nat->current_crosses) / (-1L - (long)nat->needed_crosses);
    int reserve = ((int)stuff->free_colonist_counts[nation_id] * 30 - turn) * 2;
    if (reserve < 0) {
      reserve = 0;
    }
    const long local_38 = scaled + base;
    if (nat->gold >= (uint32_t)(local_38 + reserve)) {
      nat->gold -= (uint32_t)local_38;
      const int slot = dos_rng_range(ctx->rng, 0, 2); /* nat->recruit[3] */
      const int candidate = ai_euro_5d04_cb_dock_pop_candidate(nat->recruit[slot]);
      if (candidate >= 0) {
        nat->recruit[slot] = (uint8_t)ai_euro_5d04_cb_dock_peek_type(0);
        local_16 = candidate;
      }
    }
  }

  int local_28 = 0;
  int bVar9 = 0;   /* "a hire/train happened this pass" */
  int bVar10 = 0;  /* "tools-side training happened" */

  /* raw 86122-86306: two-pass candidate loop (local_3a = 0, 1). */
  for (int local_3a = 0; local_3a < 2; ++local_3a) {
    int idx = ai_euro_5d04_cb_list_iter_first(-1);
    while (idx >= 0) {
      int next_idx = idx;
      if (!woi) {
        const int skilled = ai_euro_5d04_cb_unit_is_skilled(idx);
        if (skilled >= 0) {
          const int gate = ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx));
          int run_body = 0;
          if (gate == 0) {
            if (local_3a == 0) {
              run_body = 1;
            }
          } else if (local_3a != 0) {
            run_body = 1;
          }
          if (run_body) {
            if (ai_euro_5d04_cb_unit_dispatch_byte(idx) == 2) {
              local_28 |= local_8;
            }
            int handled = 0;
            if (ai_euro_5d04_cb_unit_dispatch_byte(idx) == 0 &&
                (!has_any_colony || (!unit_flag_bit5 && !every_third_turn))) {
              const int gate2 = ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx));
              const int local_c = gate2 != 0;
              int try_train = 0;
              if (hs->training_slots_tools <= 0) {
                if (local_8 != 0 && dos_rng_range(ctx->rng, 0, local_c + 2) == 0 && turn > 99) {
                  try_train = 1;
                }
              } else if (dos_rng_range(ctx->rng, 0, local_c + 1) == 0) {
                try_train = 1;
              }
              if (try_train) {
                /* LAB_521d_6454: tools-side training. */
                uint32_t local_1a = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_MUSKETS) * 50);
                if (hs->crosses_bank_whole != 0) {
                  local_1a = 0;
                }
                if (nat->gold >= local_1a && !f->cargo_short) {
                  if (hs->crosses_bank_whole == 0) {
                    ai_euro_5d04_cb_set_pool_counter(0xf, 0x32);
                  } else {
                    hs->crosses_bank_whole--;
                  }
                  nat->gold -= local_1a;
                  ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 1);
                  if (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx)) != 0) {
                    int swapped = 0x1c;
                    for (int j = 0; j < 3; ++j) {
                      if (ai_euro_5d04_cb_profession_gate(nat->recruit[j]) == 0) {
                        swapped = nat->recruit[j];
                        nat->recruit[j] = (uint8_t)ai_euro_5d04_cb_unit_profession(idx);
                        break;
                      }
                    }
                    ai_euro_5d04_cb_set_unit_profession(idx, swapped);
                  }
                  local_8 = 0;
                  bVar10 = 1;
                  hs->training_slots_tools--;
                  if (f->has_college &&
                      dos_rng_range(
                        ctx->rng, 0,
                        stuff->unit_type_counts[nation_id][4] + stuff->unit_type_counts[nation_id][1]
                      ) <= stuff->veteran_teach_threshold[nation_id]) {
                    ai_euro_5d04_cb_set_unit_profession(idx, 0x15); /* Veteran Soldier */
                  }
                  uint32_t local_1a2 = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_HORSES) * 50);
                  if ((uint32_t)hs->crosses_bank_raw > 0x31) {
                    local_1a2 = 0;
                  }
                  if (nat->gold >= local_1a2) {
                    nat->gold -= local_1a2;
                    ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 4);
                    if (hs->crosses_bank_raw < 0x32) {
                      ai_euro_5d04_cb_set_pool_counter(8, 0x32);
                      hs->crosses_bank_raw = 0x32;
                    } else {
                      hs->crosses_bank_raw -= 0x32;
                    }
                  }
                  handled = 1;
                }
              }
              if (!handled && ai_euro_5d04_cb_unit_dispatch_byte(idx) == 0 &&
                  local_8 - (int)stuff->unit_type_counts[nation_id][2] > 0 &&
                  dos_rng_range(ctx->rng, 0, 2) == 0 &&
                  local_28 == 0) {
                /* LAB_521d_6454-adjacent: crosses-side training. */
                int proceed = 1;
                if (turn > 99) {
                  proceed = dos_rng_range(ctx->rng, 0, 2) <=
                            (int)stuff->free_colonist_counts[nation_id];
                }
                if (proceed &&
                    (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx)) == 0 ||
                     dos_rng_range(ctx->rng, 0, 4) == 0)) {
                  const uint32_t cost = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_TOOLS) * 100);
                  if (nat->gold >= cost) {
                    nat->gold -= cost;
                    ai_euro_5d04_cb_set_pool_counter(0xe, 100);
                    ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 2);
                    local_28 = local_8;
                    local_8 = 0;
                    bVar10 = 1;
                    hs->training_slots_crosses--;
                    handled = 1;
                  }
                }
              }
            }
            if (!handled && ai_euro_5d04_cb_unit_dispatch_byte(idx) == 0 &&
                stuff->unit_type_counts[nation_id][3] == 0 && turn > 0x32) {
              int proceed = 1;
              if (turn > 199) {
                proceed = dos_rng_range(ctx->rng, 0, 3) != 0;
              }
              if (proceed && (turn % 7) == 0) {
                if (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx)) == 0 ||
                    dos_rng_range(ctx->rng, 0, 7) == 0) {
                  ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 3);
                  /* unit_type_counts[nation][3] is a live-tracked
                   * sticky-flag reuse in DOS (see header) — not mutated
                   * here since the array is read-derived (census), not
                   * writable from this port. */
                }
              }
            }
          }
        }
      }
      next_idx = ai_euro_5d04_cb_list_iter_next(idx);
      idx = next_idx;
    }
  }

  /* raw 86307-86479: colony demand vs. purchase loop. `local_24` is read
   * by the final loop below regardless of whether this block runs (the
   * raw decompile shows the same cross-block read — DOS quirk, mirrored
   * here with an explicit 0 default rather than leaving it
   * uninitialized). */
  int local_24 = 0;
  int local_22 = ai_euro_5d04_cb_colony_demand_query(local_16, 4);
  if (woi) {
    local_22 += ai_euro_5d04_cb_colony_demand_query(local_16, 0xc);
  }
  if (local_22 != 0 && !f->cargo_short &&
      (!has_any_colony || (!unit_flag_bit5 && !every_third_turn && !woi))) {
    local_24 = ai_euro_5d04_cb_colony_demand_query(local_16, 0xe);
    int local_42 = local_24 - local_22;
    if (turn > 0x50) {
      while ((uint32_t)(hs->delay_48 + 1) < (uint32_t)hs->crosses_bank_raw / 50) {
        hs->crosses_bank_raw -= 50;
        hs->delay_48++;
      }
      while ((uint32_t)hs->crosses_bank_raw / 50 + 1 < (uint32_t)hs->delay_48) {
        hs->delay_48--;
        hs->crosses_bank_raw += 50;
      }
    }
    if (local_34 == 0 && local_24 > 5) {
      uint32_t adj = 0;
      if (turn > 0x27) {
        adj = (uint32_t)((difficulty - 10) * -100);
      }
      if (hs->delay_48 != 0) {
        hs->delay_48--;
        adj = 0;
      }
      if (nat->gold >= adj && ai_euro_5d04_cb_goal_trigger(0xb, nation_id, nation_id - 0x14, nation_id - 0x14) >= 0) {
        local_42--;
        nat->gold -= adj;
      }
    }
    int bVar8_2 = 0;
    int buy_guard = 0;
    do {
      if (++buy_guard > 64) {
        break;
      }
      bVar8_2 = 0;
      const int base2 = (((int)nat->recruit_count + 7) * 2 - (difficulty & 0xfe)) * 10;
      const long extra =
        ((long)base2 * (long)nat->current_crosses) / (-1L - (long)nat->needed_crosses);
      uint32_t local_38b = (uint32_t)(base2 + extra);
      if (hs->crosses_bank_whole == 0) {
        local_38b += (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_MUSKETS) * 50);
      }
      if (turn > 99) {
        local_38b += (uint32_t)((int)(difficulty * (int)local_38b * 10) / -100);
      }
      if (nat->gold >= local_38b) {
        const int slot = dos_rng_range(ctx->rng, 0, 2);
        const int cand = ai_euro_5d04_cb_dock_pop_candidate(nat->recruit[slot]);
        if (cand < 0) {
          break;
        }
        const int cdisp = ai_euro_5d04_cb_unit_dispatch_byte(cand);
        int extra_cost = 0;
        if (cdisp == 1 || cdisp == 4) {
          if (hs->crosses_bank_whole == 0) {
            extra_cost = ai_euro_5d04_cb_price(COLONIZE_CARGO_MUSKETS) * -50;
            ai_euro_5d04_cb_market_volume(COLONIZE_CARGO_MUSKETS, 50, 0); /* FUN_291f_0a2e */
          } else {
            hs->crosses_bank_whole++;
          }
        } else if (cdisp == 2) {
          extra_cost = ai_euro_5d04_cb_price(COLONIZE_CARGO_TOOLS) * -100;
          ai_euro_5d04_cb_market_volume(COLONIZE_CARGO_TOOLS, 100, 0); /* FUN_291f_0a2e */
        } else if (cdisp == 5) {
          ai_euro_5d04_cb_set_unit_dispatch_byte(cand, 4);
        }
        local_38b += (uint32_t)extra_cost;
        if (ai_euro_5d04_cb_unit_dispatch_byte(cand) != 4) {
          ai_euro_5d04_cb_set_unit_dispatch_byte(cand, 1);
          if (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(cand)) != 0) {
            int swapped = 0x1c;
            for (int j = 0; j < 3; ++j) {
              if (ai_euro_5d04_cb_profession_gate(nat->recruit[j]) == 0) {
                swapped = nat->recruit[j];
                nat->recruit[j] = (uint8_t)ai_euro_5d04_cb_unit_profession(cand);
                break;
              }
            }
            ai_euro_5d04_cb_set_unit_profession(cand, swapped);
          }
        }
        nat->gold -= local_38b;
        if (hs->crosses_bank_whole == 0) {
          ai_euro_5d04_cb_set_pool_counter(0xf, 0x32);
        } else {
          hs->crosses_bank_whole--;
        }
        if (f->has_college && ai_euro_5d04_cb_unit_profession(cand) != 0x15) {
          const int roll4 = dos_rng_range(
            ctx->rng, 0,
            stuff->unit_type_counts[nation_id][4] + stuff->unit_type_counts[nation_id][1]
          );
          if (roll4 <= (int)stuff->veteran_teach_threshold[nation_id]) {
            ai_euro_5d04_cb_set_unit_profession(cand, 0x15);
          }
        }
        uint32_t local_1a3 = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_HORSES) * 50);
        if (turn > 99) {
          local_1a3 += (uint32_t)((int)(difficulty * (int)local_1a3 * 10) / -100);
        }
        if ((uint32_t)hs->crosses_bank_raw > 0x31) {
          local_1a3 = 0;
        }
        if (nat->gold >= local_1a3) {
          nat->gold -= local_1a3;
        }
        ai_euro_5d04_cb_set_unit_dispatch_byte(cand, 4);
        if (hs->crosses_bank_raw < 0x32) {
          ai_euro_5d04_cb_set_pool_counter(8, 0x32);
        } else {
          hs->crosses_bank_raw -= 0x32;
        }
        nat->recruit[slot] = (uint8_t)ai_euro_5d04_cb_dock_peek_type(0);
        bVar9 = 1;
        bVar8_2 = 1;
        {
          const ColonizeUnitType* ct =
            units_type(ctx->units, ai_euro_5d04_cb_unit_dispatch_byte(cand));
          local_42 -= ct ? ct->space : 1; /* DS:0x5238[type] */
        }
      }
      if (!bVar8_2 || local_42 < 1) {
        break;
      }
    } while (1);
  }

  /* raw 86480-86561: profession-demand bump loop over land units in the
   * ship-numeric-range (see header — this range's real identity, ship
   * type vs. something else in this specific loop, is not indepedently
   * re-confirmed here; kept structurally faithful to the raw body). */
  /* raw 86480-86486: DS `param_1 + -0x6ba6` (0x945a) — per-nation
   * Europe-dock proxy counter (census_tally.md item 2), not modeled: 0. */
  const int seed46 = 0;
  int local_46 = seed46 + ((turn & 1) != 0);
  int matched;
  /* DOS drops a departed ship from the Europe stack (FUN_291f_0ec2); Linux
   * leaves it at the Europe coords until the dispatcher's own act teleports
   * it, so remember which ships this pass already handled. */
  uint8_t departed[COLONIZE_UNITS_MAX];
  memset(departed, 0, sizeof(departed));
  int restarts = 0;
  do {
    matched = 0;
    if (++restarts > COLONIZE_UNITS_MAX) {
      break;
    }
    int idx2 = ai_euro_5d04_cb_list_iter_first(-1);
    while (!matched && idx2 >= 0) {
      int next2 = idx2;
      const ColonizeUnit* ship2 = ai_euro_5d04_cb_unit(idx2);
      const int flags3148 = (ship2 && ship2->col1_unknown15 & 0x80) ? 0x80 : 0; /* damaged */
      const int dispatch2 = ai_euro_5d04_cb_unit_dispatch_byte(idx2);
      if (!departed[idx2] && ((flags3148 & 0x80) == 0 || dispatch2 == 0x0b) &&
          dispatch2 > 0xc && dispatch2 < 0x13) {
        departed[idx2] = 1;
        /* unload/sell loop over hold 0 (raw `while (unit+0x3150 != 0)`). */
        int last_lots = 0; /* DS:0x8dc4 scratch */
        for (int guard = 0; guard < COLONIZE_UNIT_CARGO_MAX + 1; ++guard) {
          const int kind = ai_euro_5d04_cb_reward_case(idx2);
          if (kind < 0) {
            break;
          }
          if (kind == COLONIZE_CARGO_MUSKETS) {
            const int v = ai_euro_5d04_cb_reward_value(idx2);
            last_lots = (v + 0x31) / 0x32;
            hs->crosses_bank_whole = (int8_t)(hs->crosses_bank_whole + last_lots);
            ai_euro_5d04_cb_reward_ack(idx2);
          } else if (kind == COLONIZE_CARGO_HORSES) {
            ai_euro_5d04_cb_reward_ack(idx2);
            hs->crosses_bank_raw += last_lots; /* DOS reuses the stale 0x8dc4 lots value */
          } else if (!ai_euro_5d04_cb_sell_hold0(idx2)) {
            break; /* boycotted hold stays aboard */
          }
        }
        if (bVar9 && units_ship_capacity(ctx->units, ship2->id) == local_24) {
          bVar9 = 0;
          matched = 1;
          ai_euro_5d04_cb_unit_exhaust(idx2);
          next2 = idx2;
        } else {
          for (int p = 15; p >= 0; --p) {
            /* raw: break when 0x5237[type] (capacity) == unit+0x3150 (cargo). */
            const ColonizeUnit* sh = ai_euro_5d04_cb_unit(idx2);
            const int cap = units_ship_capacity(ctx->units, sh->id);
            int used = sh->cargo_count;
            for (int hh = 0; hh < COLONIZE_UNIT_CARGO_MAX; ++hh) {
              if (sh->hold_goods_amount[hh] > 0) {
                used++;
              }
            }
            if (cap == used || f->cargo_short || !has_any_colony) {
              break;
            }
            if ((local_46 <= (inv ? inv->profession_demand[p] : 0) || expand_signal) &&
                ((!bVar10 && local_28 == 0) || (cap - used > 2 || expand_signal))) {
              if (nat->gold >= (uint32_t)(ai_euro_5d04_cb_price(p) * 100)) {
                ai_euro_5d04_cb_apply_bump(idx2, p, 100);
                AiEuroInventory* winv = ai_goals_inventory(nation_id);
                if (winv && winv->profession_demand[p] > 0) {
                  winv->profession_demand[p]--;
                }
              }
            }
          }
          ai_euro_5d04_cb_unit_exhaust(idx2);
          matched = 1;
          next2 = idx2;
        }
      }
      next2 = ai_euro_5d04_cb_list_iter_next(idx2);
      idx2 = next2;
    }
  } while (matched);
}

/*
 * FUN_521d_5d04 — full structural port, orchestrator (2026-08-19).
 * Mirrors the raw function's own top-level call order: treasury bump →
 * gate cascade → ship-buy candidate ladder (with its real early-return
 * semantics, raw 86030-86064) → hire-ladder tail (raw 86065-86561). This
 * is the complete port — every raw line from 85872 to 86564 now has a
 * corresponding piece here, real where resolved, stubbed where not (see
 * each piece's own header). NOT wired into the live turn loop: the live
 * path (`ai_euro_nation_planning` below) still only takes the treasury
 * bump. Finishing the port doesn't imply it's safe or correct to flip
 * live — see the ai-5d04-structural-port memory for what's still
 * genuinely unresolved (the two list-iterator callees especially) before
 * that'd be a reasonable next step.
 */
static void ai_euro_5d04_nation_planning_structural(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ai_euro_5d04_treasury_bump(ctx, nation_id);
  const Ai5d04PlanningFlags f = ai_euro_5d04_compute_flags(ctx, nation_id);
  int abort_early = 0;
  ai_euro_5d04_ship_buy_ladder(ctx, nation_id, &f, &abort_early);
  if (abort_early) {
    return;
  }
  ai_euro_5d04_hire_ladder_tail(ctx, nation_id, &f);
}

static void ai_euro_nation_planning(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  const int diff = ctx->col1->head.difficulty;
  /*
   * T3.1 (2026-08-27): the structural 5d04 orchestrator is now the live
   * entry (treasury bump + flag cascade + ship-buy ladder + hire-ladder
   * tail). T2.1 established the swap is a no-op by construction while the
   * tail's two list-iterator stubs return "none" — goldens mid01/late01
   * and the unit suites confirm zero delta. `apply_naval_gold_floors` stays
   * reference-only (address-taken to keep it compiled).
   */
  ai_euro_5d04_nation_planning_structural(ctx, nation_id);
  (void)ai_euro_5d04_apply_naval_gold_floors;

  /*
   * NEW WORLD wagon / mid-game hire matrix — thin 5d04 slice (full ~748 PARKED).
   * Peace dock/wagon/Pioneer shortage matrix runs at any colony_count
   * (mid-game ≥6 included). Mid-game still runs Europe ship-buy + wartime
   * military hire; Free Colonist / Colonist settle spam stays gated at
   * colonies ≥ 6. At war prefer Soldier/Dragoon; colonies≥2 also Artillery
   * when type exists. Peace: tools_short>30 / lumber/ore/food>30 /
   * muskets/horses>20 + Wagon → hire wagon once; else tools_short>20
   * Pioneer/Hardy + tools cargo (ship +20 / colony +15). Case-7 deepen:
   * prefer dock experts already on Europe dock (no free spawn fiction).
   * Treasury gate: skip hire when gold < hire_cost (Artillery 500$).
   * No free Europe passenger slot → thin buy ladder
   * (Caravel/Merchantman/Galleon/Frigate). Cite: euro_goals FUN_521d_03d0
   * colony_count < 0x30 (not hard-stop at 6); euro_dispatcher.c 5d04.
   */
  const int colonies = inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
  /* Colonist / Soldier Europe hire stand-in already used by this planner. */
  const int hire_cost = 200 + diff * 25;
  if ((int)nat->gold < hire_cost) {
    return; /* 5d04 treasury: too poor for Europe hire / tools-cargo */
  }

  /* Prefer a Europe ship that still has passenger space. */
  ColonizeUnit* ship = NULL;
  for (int i = COLONIZE_UNITS_MAX - 1; i >= 0; --i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (!ai_euro_is_ship_type(ctx->units, u->id) || !ai_euro_in_europe(u->x, u->y)) {
      continue;
    }
    if (u->cargo_count < units_ship_capacity(ctx->units, u->id)) {
      ship = u;
      break;
    }
  }
  /*
   * Thin 5d04 / 5c3c: when no Europe transport with free passenger space, buy
   * a ship so the hire/cargo matrix can run — covers no-ship and full-ship
   * (second transport). Prefer Frigate (5000$) then Galleon (3000$) when at
   * war; else Merchantman (2000$) when cargo shorts high; else Caravel (1000$).
   * Skip while colony_count==0: starter fleets are full on Europe exit
   * (TURN1→2); DOS does not buy a second transport before first landfall.
   * Wartime Privateer spawn is ai_diplo_euro_balance (not this buy ladder).
   * Cite: FUN_521d_5c3c / 5d04; europe_init_purchase_table; purchase.png;
   * euro_unit_act war transport / Frigate hunt; test-saves-ai/TURN2 unit_count.
   */
  if (!ship && colonies >= 1 && (int)nat->gold >= AI_EURO_CARAVEL_PURCHASE_GOLD) {
    const int at_war_buy = ai_euro_at_war_any_peer(ctx->col1, nation_id);
    const int cargo_pressure =
      inv &&
      (inv->tools_short > 30 || inv->lumber_short > 30 || inv->ore_short > 30 ||
       inv->food_short > 30 || inv->muskets_short > 20 || inv->horses_short > 20);
    int buy_ty = -1;
    int buy_gold = AI_EURO_CARAVEL_PURCHASE_GOLD;
    if (at_war_buy && (int)nat->gold >= AI_EURO_FRIGATE_PURCHASE_GOLD) {
      buy_ty = units_find_type(ctx->units, "Frigate");
      if (buy_ty >= 0) {
        buy_gold = AI_EURO_FRIGATE_PURCHASE_GOLD;
      }
    }
    if (buy_ty < 0 && at_war_buy && (int)nat->gold >= AI_EURO_GALLEON_PURCHASE_GOLD) {
      buy_ty = units_find_type(ctx->units, "Galleon");
      if (buy_ty >= 0) {
        buy_gold = AI_EURO_GALLEON_PURCHASE_GOLD;
      }
    }
    if (buy_ty < 0 && cargo_pressure &&
        (int)nat->gold >= AI_EURO_MERCHANTMAN_PURCHASE_GOLD) {
      buy_ty = units_find_type(ctx->units, "Merchantman");
      if (buy_ty >= 0) {
        buy_gold = AI_EURO_MERCHANTMAN_PURCHASE_GOLD;
      }
    }
    if (buy_ty < 0) {
      buy_ty = units_find_type(ctx->units, "Caravel");
      buy_gold = AI_EURO_CARAVEL_PURCHASE_GOLD;
    }
    if (buy_ty >= 0 && (int)nat->gold >= buy_gold) {
      const int sid = units_spawn_allow_stack(ctx->units, buy_ty, 200, 100);
      if (sid >= 0) {
        ColonizeUnit* bought = units_get(ctx->units, sid);
        if (bought) {
          units_set_nation(bought, nation_id);
          bought->moves_left = 0; /* docked Europe — planning hire only */
          nat->gold -= (uint32_t)buy_gold;
          if (ctx->europe && nation_id == ctx->human_nation) {
            ctx->europe->gold = (int)nat->gold;
          }
          ship = bought;
        }
      }
    }
  }
  if (!ship || ship->cargo_count >= units_ship_capacity(ctx->units, ship->id)) {
    return;
  }

  /* At war with any Euro peer → prefer Soldier / Dragoon over settle types.
   * At-war + tools_short: still Soldier/Dragoon (not Pioneer) when gold covers
   * hire_cost — peace tools→Pioneer/wagon stays !at_war only.
   * Mid-hire deepen: own colonies ≥ 3 → prefer Dragoon when type exists
   * (mounted war unit; same hire_cost as Soldier dock hire). Cite:
   * euro_dispatcher.c mid-hire; case-7 / 5d04 war arm; fandom Dragoon.
   * If Dragoon type missing from pool → Soldier path (documented).
   * Own colonies ≥ 2: prefer Veteran Soldier when type+affordable cost exist
   * (@UNIT cost, else NAMES @JOB train 2000$). Cite: COLONIZE/NAMES.TXT @JOB
   * Soldier→Veteran Soldiers 2000$; euro_unit_act §2d mid-hire.
   * PARK: no Veteran Soldier type in pool / gold < cost → plain Soldier. */
  int hire_ty = -1;
  int from_dock = 0;
  int dock_idx = -1;
  const int at_war = ai_euro_at_war_any_peer(ctx->col1, nation_id);
  if (at_war) {
    static const char* k_dragoon[] = {"Dragoon", "Veteran Dragoon", "Dragoons"};
    static const char* k_soldier[] = {"Soldier", "Soldiers"};
    static const char* k_veteran[] = {"Veteran Soldier", "Veteran Soldiers"};
    int drag_ty = -1;
    int mil_ty = -1;
    int vet_ty = -1;
    int vet_cost = 0;
    if (colonies >= 3) {
      for (size_t i = 0; i < sizeof(k_dragoon) / sizeof(k_dragoon[0]) && drag_ty < 0; ++i) {
        drag_ty = units_find_type(ctx->units, k_dragoon[i]);
      }
    }
    /* ≥2 colonies: Veteran Soldier if type exists and treasury covers cost. */
    if (colonies >= 2) {
      for (size_t i = 0; i < sizeof(k_veteran) / sizeof(k_veteran[0]) && vet_ty < 0; ++i) {
        vet_ty = units_find_type(ctx->units, k_veteran[i]);
      }
      if (vet_ty >= 0) {
        const ColonizeUnitType* vt = units_type(ctx->units, vet_ty);
        vet_cost = (vt && vt->cost > 0) ? vt->cost : AI_EURO_VETERAN_SOLDIER_TRAIN_GOLD;
        if ((int)nat->gold < vet_cost) {
          vet_ty = -1; /* underfunded @JOB / @UNIT cost — Soldier path */
          vet_cost = 0;
        }
      }
      /* PARK: Veteran Soldier mid-hire needs type in pool + affordable cost
       * (NAMES @JOB 2000$ or @UNIT cost). Missing → plain Soldier below. */
    }
    for (size_t i = 0; i < sizeof(k_soldier) / sizeof(k_soldier[0]) && mil_ty < 0; ++i) {
      mil_ty = units_find_type(ctx->units, k_soldier[i]);
    }
    /* When not preferring Dragoon (colonies<3 or type missing), allow Dragoon
     * as Soldier-band fallback (prior k_mil order). */
    if (mil_ty < 0) {
      for (size_t i = 0; i < sizeof(k_dragoon) / sizeof(k_dragoon[0]) && mil_ty < 0; ++i) {
        mil_ty = units_find_type(ctx->units, k_dragoon[i]);
      }
    }
    /* Prefer order: Dragoon (≥3) > Veteran (≥2+cost) > Soldier. */
    if (drag_ty >= 0) {
      mil_ty = drag_ty;
    } else if (vet_ty >= 0) {
      mil_ty = vet_ty;
    }
    /* Thin deepen: mid-game Artillery when colonies>=2 and type in pool. */
    int art_ty = -1;
    if (colonies >= 2) {
      art_ty = units_find_type(ctx->units, "Artillery");
      if (art_ty < 0) {
        art_ty = units_find_type(ctx->units, "Cannon");
      }
    }
    int mil_aboard = 0;
    for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(ctx->units, ship->cargo_ids[c]);
      if (!pax) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(ctx->units, pax->type_index);
      if (ty && ai_euro_is_military_name(ty->name)) {
        mil_aboard = 1;
        break;
      }
    }
    /* Soldier/Dragoon primary; Artillery when mil already boarded or odd turn. */
    const unsigned turn =
      (ctx->turn_number && *ctx->turn_number) ? (unsigned)(*ctx->turn_number) : 0u;
    int prefer_art = art_ty >= 0 && (mil_aboard || (turn & 1u));
    /*
     * 5d04 treasury: Artillery needs Europe purchase gold (500$), not the
     * colonist hire_cost. Fall back to Soldier/Dragoon when underfunded.
     */
    if (prefer_art && (int)nat->gold < AI_EURO_ARTILLERY_PURCHASE_GOLD) {
      prefer_art = 0;
    }
    if (prefer_art) {
      hire_ty = art_ty;
    } else if (mil_ty >= 0) {
      hire_ty = mil_ty;
    } else if (art_ty >= 0 && (int)nat->gold >= AI_EURO_ARTILLERY_PURCHASE_GOLD) {
      hire_ty = art_ty; /* mil type missing — Artillery still a war option */
    }
  }
  /*
   * Peace case-7 / 5d04: when tools_short high, prefer Expert/Hardy Pioneer or
   * Master Carpenter already on Europe dock (NAMES/pool names). Only if present —
   * do not spawn free experts as fiction. Else wagon / Pioneer matrix below.
   */
  if (hire_ty < 0 && inv && !at_war && inv->tools_short > 20 && ctx->europe) {
    dock_idx = ai_euro_dock_find_tools_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 / 5d04 tools deepen: when tools_short high and Pioneer/Carpenter
   * dock miss, prefer Master Blacksmith on Europe dock (Ore→Tools workplace).
   * Cite: europe.c Master Blacksmiths; building_production Blacksmith→Tools;
   * euro_unit_act Blacksmith workplace assign.
   */
  if (hire_ty < 0 && inv && !at_war && inv->tools_short > 20 && ctx->europe) {
    dock_idx = ai_euro_dock_find_blacksmith_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 / 5d04 food deepen: when food_short high, prefer Expert Farmer
   * already on Europe dock (consume dock slot; no free spawn). Cite:
   * europe.c k_pool_cands Expert Farmers; building_production Farmer→Food;
   * euro_unit_act §2e Expert Farmer food LABOR; Hardy Pioneer dock pattern §2d.
   */
  if (hire_ty < 0 && inv && !at_war && inv->food_short > 20 && ctx->europe) {
    dock_idx = ai_euro_dock_find_food_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 food coastal fallback: food_short high + coastal colony +
   * Expert Fisherman on Europe dock (when Farmer not hired above). Cite:
   * terrain_yields Fisherman; euro_unit_act Fisherman field-assign; europe.c
   * Expert Fishermen pool.
   */
  if (hire_ty < 0 && inv && !at_war && inv->food_short > 20 && ctx->europe &&
      ai_euro_nation_has_coastal_colony(ctx, nation_id)) {
    dock_idx = ai_euro_dock_find_fisherman_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 / 5d04 construction deepen: when any colony wants carpenter
   * LABOR (Stockade/Warehouse/Lumber Mill/Drydock/Shipyard incomplete),
   * prefer Master Carpenter already on Europe dock (consume dock slot; same
   * hire_cost as Expert Farmer / Hardy Pioneer). Cite: docs/building_production.md
   * Carpenter→Hammers; europe.c Master Carpenters pool; euro_unit_act §2e;
   * ai_euro_colony_wants_construction_labor. Only if present on dock.
   */
  if (hire_ty < 0 && inv && !at_war && ctx->europe &&
      ai_euro_nation_wants_construction_labor(ctx, nation_id)) {
    dock_idx = ai_euro_dock_find_carpenter_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 / 5d04 lumber deepen: when lumber_short high, prefer Expert
   * Lumberjack already on Europe dock (consume dock slot; same hire_cost as
   * Expert Farmer / Master Carpenter). Cite: europe.c Expert Lumberjacks pool;
   * building_production Lumberjack→Lumber; euro_unit_act §2e Expert Lumberjack
   * LABOR; Hardy Pioneer dock pattern §2d. Only if present on dock.
   */
  if (hire_ty < 0 && inv && !at_war && inv->lumber_short > 20 && ctx->europe) {
    dock_idx = ai_euro_dock_find_lumberjack_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 / 5d04 ore deepen: when ore_short high, prefer Expert Ore /
   * Silver Miner already on Europe dock (consume dock slot). Cite: europe.c
   * Expert Ore Miners pool; terrain_yields Ore/Silver; euro_unit_act Ore Miner
   * field-assign; Hardy Pioneer dock pattern §2d. Only if present on dock.
   */
  if (hire_ty < 0 && inv && !at_war && inv->ore_short > 20 && ctx->europe) {
    dock_idx = ai_euro_dock_find_ore_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 / 5d04 muskets deepen: when muskets_short high, prefer Master
   * Gunsmith already on Europe dock (consume dock slot). Cite: europe.c Master
   * Gunsmiths pool; building_production Gunsmith Tools→Muskets (Armory+);
   * euro_unit_act Gunsmith workplace assign. Only if present on dock.
   */
  if (hire_ty < 0 && inv && !at_war && inv->muskets_short > 20 && ctx->europe) {
    dock_idx = ai_euro_dock_find_gunsmith_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 convert deepen: unmissioned tribe + Jesuit/Missionary on
   * Europe dock → prefer that type (CONTACT convert). Cite: europe.c Jesuit
   * Missionaries pool; euro_unit_act §2c6; Colonization.pdf Establishing a
   * Mission. Prefer before Seasoned Scout when both present.
   */
  if (hire_ty < 0 && inv && !at_war && ctx->europe && ai_euro_has_unmissioned_tribe(ctx)) {
    dock_idx = ai_euro_dock_find_missionary_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 explore deepen: own colonies ≥ 1 + Seasoned Scout on Europe
   * dock → prefer that type (CONTACT / fog explore). Cite: europe.c Seasoned
   * Scouts pool; euro_unit_act §2c2 Seasoned Scout fog; Colonization.pdf OTHER.
   */
  if (hire_ty < 0 && inv && !at_war && inv->colony_count >= 1 && ctx->europe) {
    dock_idx = ai_euro_dock_find_scout_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 liberty deepen: own colonies ≥ 1 + Elder Statesman on Europe
   * dock → prefer that type (Town Hall bells). Cite: europe.c Elder Statesmen;
   * building_production Elder→Liberty bells; Colonization.pdf SoL.
   */
  if (hire_ty < 0 && inv && !at_war && inv->colony_count >= 1 && ctx->europe) {
    dock_idx = ai_euro_dock_find_elder_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 crosses deepen: Church/Cathedral present + Firebrand Preacher
   * on Europe dock → prefer that type. Cite: europe.c Firebrand Preachers;
   * building_production Preacher→Crosses; Colonization.pdf Church.
   */
  if (hire_ty < 0 && inv && !at_war && ctx->europe &&
      ai_euro_nation_has_church(ctx, nation_id)) {
    dock_idx = ai_euro_dock_find_preacher_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 education deepen: Schoolhouse/College/University present +
   * Expert Teacher on Europe dock → prefer that type. Cite: europe.c Expert
   * Teachers; building_production.md Skills Chart job 18; Colonization.pdf
   * Education / Teacher.
   */
  if (hire_ty < 0 && inv && !at_war && ctx->europe &&
      ai_euro_nation_has_school(ctx, nation_id)) {
    dock_idx = ai_euro_dock_find_teacher_expert(ctx->europe);
    if (dock_idx >= 0) {
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace case-7 craft deepen: Distiller/Weaver/Tobacconist/Fur Trader on Europe
   * dock when a colony has the craft building + raw stock≥20. Cite: europe.c
   * Master Distiller/Weavers/Tobacconists/Fur Traders; building_production craft
   * chains; euro_unit_act workplace assign.
   */
  if (hire_ty < 0 && inv && !at_war && ctx->europe) {
    static const char* const k_distiller[] = {
      "Rum Distiller's House", "Rum Distillery", "Rum Factory", NULL
    };
    static const char* const k_weaver[] = {
      "Weaver's House", "Weaver's Shop", "Textile Mill", NULL
    };
    static const char* const k_tobacconist[] = {
      "Tobacconist's House", "Tobacconist's Shop", "Cigar Factory", NULL
    };
    static const char* const k_fur_trader[] = {
      "Fur Trader's House", "Fur Trading Post", "Fur Factory", NULL
    };
    typedef struct {
      int (*name_is)(const char*);
      const char* const* chain;
      int cargo;
    } CraftDock;
    const CraftDock crafts[] = {
      {ai_euro_dock_name_is_distiller_expert, k_distiller, COLONIZE_CARGO_SUGAR},
      {ai_euro_dock_name_is_weaver_expert, k_weaver, COLONIZE_CARGO_COTTON},
      {ai_euro_dock_name_is_tobacconist_expert, k_tobacconist, COLONIZE_CARGO_TOBACCO},
      {ai_euro_dock_name_is_fur_trader_expert, k_fur_trader, COLONIZE_CARGO_FURS},
    };
    for (size_t ci = 0; ci < sizeof(crafts) / sizeof(crafts[0]) && hire_ty < 0; ++ci) {
      if (!ai_euro_nation_wants_craft(ctx, nation_id, crafts[ci].chain, crafts[ci].cargo)) {
        continue;
      }
      dock_idx = ai_euro_dock_find_named_expert(ctx->europe, crafts[ci].name_is);
      if (dock_idx < 0) {
        continue;
      }
      const int dock_ty =
        ai_euro_type_from_dock_name(ctx->units, ctx->europe->dock[dock_idx].name);
      if (dock_ty >= 0) {
        hire_ty = dock_ty;
        from_dock = 1;
      }
    }
  }
  /*
   * Peace thin wagon / supply matrix: Wagon once when tools/lumber/ore_short>30
   * or muskets/horses_short>20 or food_short>30 (tally caps differ); else
   * Pioneer/Hardy when tools_short>20; else 5c3c profession_demand → Pioneer.
   * Cite: euro_unit_act §2d wagon haul / 5cf6; Colonization.pdf Wagon Train.
   */
  if (hire_ty < 0 && inv && !at_war) {
    if ((inv->tools_short > 30 || inv->lumber_short > 30 || inv->ore_short > 30 ||
         inv->muskets_short > 20 || inv->horses_short > 20 || inv->food_short > 30) &&
        !ai_euro_nation_has_wagon(ctx->units, nation_id)) {
      const int wagon_ty = ai_euro_find_wagon_type(ctx->units);
      if (wagon_ty >= 0) {
        hire_ty = wagon_ty;
      }
    }
    if (hire_ty < 0 && inv->tools_short > 20) {
      hire_ty = units_find_type(ctx->units, "Hardy Pioneer");
      if (hire_ty < 0) {
        hire_ty = units_find_type(ctx->units, "Pioneer");
      }
    }
  }
  /* Peace / fallback: 5c3c-shaped profession demand → Pioneer (not at war). */
  if (hire_ty < 0 && inv && !at_war) {
    for (int p = 0; p < 16; ++p) {
      if (inv->profession_demand[p] > 0) {
        if (inv->tools_short > 0) {
          hire_ty = units_find_type(ctx->units, "Hardy Pioneer");
          if (hire_ty < 0) {
            hire_ty = units_find_type(ctx->units, "Pioneer");
          }
        }
        break;
      }
    }
  }
  if (hire_ty < 0) {
    /* Mid-game: ship-buy (+ war hire) only — no Free Colonist settle spam. */
    if (colonies >= 6) {
      return;
    }
    /*
     * Real NAMES.TXT @UNIT has no "Free Colonist" / "Colonist" row — the base
     * laborer type is "Colonists" (profession flavors the display name via
     * units_display_name()). Try the real name first; keep the old names as
     * fallback for fixtures that still define them. Cite: port_plan.md Phase 3
     * "Free Colonist" dead-lookup note.
     */
    hire_ty = units_find_type(ctx->units, "Colonists");
  }
  if (hire_ty < 0) {
    hire_ty = units_find_type(ctx->units, "Free Colonist");
  }
  if (hire_ty < 0) {
    hire_ty = units_find_type(ctx->units, "Colonist");
  }
  if (hire_ty < 0) {
    return;
  }

  /*
   * Per-type treasury gate before spawn / tools-cargo (5d04 / Europe hire).
   * Artillery: purchase table 500$. Veteran Soldier: @UNIT cost or @JOB 2000$.
   * Others: hire_cost already gated above.
   */
  {
    const ColonizeUnitType* pending = units_type(ctx->units, hire_ty);
    int pay = hire_cost;
    if (pending &&
        (strstr(pending->name, "Artillery") != NULL || strstr(pending->name, "Cannon") != NULL)) {
      pay = AI_EURO_ARTILLERY_PURCHASE_GOLD;
    } else if (pending && strstr(pending->name, "Veteran") != NULL &&
               strstr(pending->name, "Soldier") != NULL) {
      pay = (pending->cost > 0) ? pending->cost : AI_EURO_VETERAN_SOLDIER_TRAIN_GOLD;
    }
    if ((int)nat->gold < pay) {
      return;
    }
  }

  /* Same-tile Europe spawn → stacked board (units_board requires adjacency). */
  const int uid = units_spawn_allow_stack(ctx->units, hire_ty, ship->x, ship->y);
  if (uid < 0) {
    return;
  }
  ColonizeUnit* pax = units_get(ctx->units, uid);
  if (!pax) {
    return;
  }
  units_set_nation(pax, nation_id);
  if (from_dock && ctx->europe && dock_idx >= 0 && dock_idx < ctx->europe->dock_count) {
    pax->profession = ctx->europe->dock[dock_idx].profession;
  }

  const ColonizeUnitType* hired = units_type(ctx->units, hire_ty);
  const int hired_wagon = hired && ai_euro_type_is_wagon_name(hired->name);
  const int hired_pioneer =
    hired &&
    (strstr(hired->name, "Pioneer") != NULL || strstr(hired->name, "Hardy") != NULL);
  const int hired_artillery =
    hired &&
    (strstr(hired->name, "Artillery") != NULL || strstr(hired->name, "Cannon") != NULL);
  const int hired_veteran_soldier =
    hired && strstr(hired->name, "Veteran") != NULL && strstr(hired->name, "Soldier") != NULL;
  int pay = hire_cost;
  if (hired_artillery) {
    pay = AI_EURO_ARTILLERY_PURCHASE_GOLD;
  } else if (hired_veteran_soldier) {
    pay = (hired->cost > 0) ? hired->cost : AI_EURO_VETERAN_SOLDIER_TRAIN_GOLD;
    /* Profession bit so display_name is Veteran when type lacks Veteran name. */
    pax->profession = UNITS_JOB_SOLDIER;
  }

  /*
   * Wagon hire: load TOOLS (preferred), else LUMBER, else ORE, else MUSKETS,
   * else HORSES, else FOOD onto the wagon before boarding. Cite: 5cf6 shorts;
   * Colonization.pdf Wagon Train; euro_unit_act §2d haul ladder.
   */
  int wagon_loaded_tools = 0;
  int wagon_loaded_lumber = 0;
  int wagon_loaded_ore = 0;
  int wagon_loaded_muskets = 0;
  int wagon_loaded_horses = 0;
  int wagon_loaded_food = 0;
  if (hired_wagon && inv) {
    if (inv->tools_short > 30) {
      wagon_loaded_tools = units_load_goods(ctx->units, uid, COLONIZE_CARGO_TOOLS, 20);
    } else if (inv->lumber_short > 30) {
      wagon_loaded_lumber = units_load_goods(ctx->units, uid, COLONIZE_CARGO_LUMBER, 20);
    } else if (inv->ore_short > 30) {
      wagon_loaded_ore = units_load_goods(ctx->units, uid, COLONIZE_CARGO_ORE, 20);
    } else if (inv->muskets_short > 20) {
      wagon_loaded_muskets = units_load_goods(ctx->units, uid, COLONIZE_CARGO_MUSKETS, 20);
    } else if (inv->horses_short > 20) {
      wagon_loaded_horses = units_load_goods(ctx->units, uid, COLONIZE_CARGO_HORSES, 20);
    } else if (inv->food_short > 30) {
      wagon_loaded_food = units_load_goods(ctx->units, uid, COLONIZE_CARGO_FOOD, 20);
    }
  }

  if (!units_board_stacked(ctx->units, uid, ship->id)) {
    units_despawn(ctx->units, uid);
    return;
  }
  /* Consume dock immigrant only after successful board (no free duplicate). */
  if (from_dock && ctx->europe) {
    (void)ai_euro_dock_remove_at(ctx->europe, dock_idx);
  }
  nat->gold -= (uint32_t)pay;
  if (ctx->europe && nation_id == ctx->human_nation) {
    ctx->europe->gold = (int)nat->gold;
  }
  if (inv && inv->profession_demand[0] > 0) {
    inv->profession_demand[0]--;
  }

  /*
   * Thin tools-cargo stand-in (threshold lowered from >40 to >20). Pioneer/Hardy:
   * equip tools + ship hold +20 or nearest-colony +15. Wagon already loaded above;
   * if wagon TOOLS load failed, fall back to ship/colony delivery.
   * Master Carpenter dock hire skips tools equip (builder, not pioneer).
   */
  if (inv && inv->tools_short > 20) {
    int delivered = 0;
    if (hired_wagon) {
      delivered = wagon_loaded_tools;
      if (delivered <= 0 && inv->tools_short > 30) {
        delivered = ai_euro_tools_cargo_or_colony(ctx, nation_id, ship);
      }
    } else if (hired_pioneer) {
      if (pax->tools < UNITS_EQUIP_TOOLS_STEP) {
        pax->tools = UNITS_EQUIP_TOOLS_STEP;
      }
      delivered = ai_euro_tools_cargo_or_colony(ctx, nation_id, ship);
    } else if (from_dock) {
      /* Dock carpenter / expert: tools cargo only (no pioneer equip fiction). */
      delivered = ai_euro_tools_cargo_or_colony(ctx, nation_id, ship);
    }
    if (delivered > 0) {
      if (inv->tools_short > delivered) {
        inv->tools_short -= delivered;
      } else {
        inv->tools_short = 0;
      }
    }
  }
  /*
   * Thin lumber/ore cargo stand-in (mirror tools): when matching short >20 and
   * tools path did not dominate (tools_short≤20), load ship +20 or colony +15.
   * Cite: euro_unit_act §2d leftover mid-5d04; 5cf6 lumber/ore_short.
   */
  if (inv && inv->tools_short <= 20 && inv->lumber_short > 20) {
    int delivered = 0;
    if (hired_wagon) {
      delivered = wagon_loaded_lumber;
      if (delivered <= 0 && inv->lumber_short > 30) {
        delivered = ai_euro_cargo_or_colony(
          ctx, nation_id, ship, COLONIZE_CARGO_LUMBER, 20, 15
        );
      }
    } else {
      delivered = ai_euro_cargo_or_colony(
        ctx, nation_id, ship, COLONIZE_CARGO_LUMBER, 20, 15
      );
    }
    if (delivered > 0) {
      if (inv->lumber_short > delivered) {
        inv->lumber_short -= delivered;
      } else {
        inv->lumber_short = 0;
      }
      wagon_loaded_lumber = 0; /* avoid double-trim below */
    }
  } else if (inv && inv->tools_short <= 20 && inv->lumber_short <= 20 &&
             inv->ore_short > 20) {
    int delivered = 0;
    if (hired_wagon) {
      delivered = wagon_loaded_ore;
      if (delivered <= 0 && inv->ore_short > 30) {
        delivered =
          ai_euro_cargo_or_colony(ctx, nation_id, ship, COLONIZE_CARGO_ORE, 20, 15);
      }
    } else {
      delivered =
        ai_euro_cargo_or_colony(ctx, nation_id, ship, COLONIZE_CARGO_ORE, 20, 15);
    }
    if (delivered > 0) {
      if (inv->ore_short > delivered) {
        inv->ore_short -= delivered;
      } else {
        inv->ore_short = 0;
      }
      wagon_loaded_ore = 0;
    }
  } else if (
    inv && inv->tools_short <= 20 && inv->lumber_short <= 20 && inv->ore_short <= 20 &&
    inv->muskets_short > 20
  ) {
    int delivered = 0;
    if (hired_wagon) {
      delivered = wagon_loaded_muskets;
      if (delivered <= 0) {
        delivered = ai_euro_cargo_or_colony(
          ctx, nation_id, ship, COLONIZE_CARGO_MUSKETS, 10, 10
        );
      }
    } else {
      delivered = ai_euro_cargo_or_colony(
        ctx, nation_id, ship, COLONIZE_CARGO_MUSKETS, 10, 10
      );
    }
    if (delivered > 0) {
      if (inv->muskets_short > delivered) {
        inv->muskets_short -= delivered;
      } else {
        inv->muskets_short = 0;
      }
      wagon_loaded_muskets = 0;
    }
  } else if (
    inv && inv->tools_short <= 20 && inv->lumber_short <= 20 && inv->ore_short <= 20 &&
    inv->muskets_short <= 20 && inv->horses_short > 20
  ) {
    int delivered = 0;
    if (hired_wagon) {
      delivered = wagon_loaded_horses;
      if (delivered <= 0) {
        delivered = ai_euro_cargo_or_colony(
          ctx, nation_id, ship, COLONIZE_CARGO_HORSES, 10, 10
        );
      }
    } else {
      delivered = ai_euro_cargo_or_colony(
        ctx, nation_id, ship, COLONIZE_CARGO_HORSES, 10, 10
      );
    }
    if (delivered > 0) {
      if (inv->horses_short > delivered) {
        inv->horses_short -= delivered;
      } else {
        inv->horses_short = 0;
      }
      wagon_loaded_horses = 0;
    }
  } else if (
    inv && inv->tools_short <= 20 && inv->lumber_short <= 20 && inv->ore_short <= 20 &&
    inv->muskets_short <= 20 && inv->horses_short <= 20 && inv->food_short > 20
  ) {
    int delivered = 0;
    if (hired_wagon) {
      delivered = wagon_loaded_food;
      if (delivered <= 0 && inv->food_short > 30) {
        delivered = ai_euro_cargo_or_colony(
          ctx, nation_id, ship, COLONIZE_CARGO_FOOD, 20, 15
        );
      }
    } else {
      delivered = ai_euro_cargo_or_colony(
        ctx, nation_id, ship, COLONIZE_CARGO_FOOD, 20, 15
      );
    }
    if (delivered > 0) {
      if (inv->food_short > delivered) {
        inv->food_short -= delivered;
      } else {
        inv->food_short = 0;
      }
      wagon_loaded_food = 0;
    }
  }
  /* Wagon lumber/ore/muskets load: trim matching inventory short. */
  if (inv && wagon_loaded_lumber > 0) {
    if (inv->lumber_short > wagon_loaded_lumber) {
      inv->lumber_short -= wagon_loaded_lumber;
    } else {
      inv->lumber_short = 0;
    }
  }
  if (inv && wagon_loaded_ore > 0) {
    if (inv->ore_short > wagon_loaded_ore) {
      inv->ore_short -= wagon_loaded_ore;
    } else {
      inv->ore_short = 0;
    }
  }
  if (inv && wagon_loaded_muskets > 0) {
    if (inv->muskets_short > wagon_loaded_muskets) {
      inv->muskets_short -= wagon_loaded_muskets;
    } else {
      inv->muskets_short = 0;
    }
  }
  if (inv && wagon_loaded_horses > 0) {
    if (inv->horses_short > wagon_loaded_horses) {
      inv->horses_short -= wagon_loaded_horses;
    } else {
      inv->horses_short = 0;
    }
  }
  if (inv && wagon_loaded_food > 0) {
    if (inv->food_short > wagon_loaded_food) {
      inv->food_short -= wagon_loaded_food;
    } else {
      inv->food_short = 0;
    }
  }
}

/* --- 0a60 goal-consumption engine (structural port, live) --------------
 *
 * Literal, section-scoped structural port of FUN_521d_0a60's own final
 * goal-table consumption engine — raw decomp lines 974-1063 of the
 * ~845-line function (see
 * original_sources_annotated/ai/euro_goal_orders_0a60_full.md, "New
 * section: goal -> orders wiring"). Per explicit instruction: port what
 * 0a60 does IN ITSELF faithfully; the functions/data it reaches out to can
 * stay at whatever level of development they're already at in this file
 * (dos_dist / map_continent_id_at / units_id_at / ai_goals_primary are all
 * real, already-ported equivalents of their DOS callees) or a documented
 * placeholder where DOS's own callee/data semantics are still unresolved
 * (func_0x0001854c's weight seed, DS:0x523d unit-type capability bitmask,
 * unit+0x3148's FOUND/MIL_EXPAND eligibility bits).
 *
 * **Live as of 2026-08-18**, replacing the old approximate soldier/
 * founder/generic-fallback three-loop scan inside `ai_euro_unit_act`
 * (which never covered LABOR/COLONY assignment for founders without a
 * matching FOUND/MIL_EXPAND slot, and used a two-phase "soldier goals
 * first, then anything" priority hack that DOS's real single-pass 64-slot
 * scan doesn't have — see euro_goal_orders_0a60_full.md's "Structural
 * pilot port" section for the before/after). Runs once per nation per
 * turn from `ai_euro_nation_planning`-equivalent, alongside
 * `ai_euro_colony_goals`; `ai_euro_unit_act` reads its committed pick back
 * per unit (act_state==0xb) instead of recomputing its own scan. Not a
 * `golden_ai_turns` fidelity claim — expect no immediate change there
 * (pre-existing TURN4→5 failure, unrelated); this is a structural quality
 * improvement over the old approximation, not a new golden-alignment pass.
 *
 * Deliberately out of scope this pass: the unit-loop threat-flag section
 * (raw lines 1-189) and the deep G-table / colony-loop section (raw lines
 * 190-973). Both lean on DOS accessor semantics (FUN_1000_8aac's field-id
 * meaning, thunk_FUN_2a1f_0470/047c/0524/0560's real effects, unit+0x3148's
 * individual bit *writers*) that no prior mapping pass in this project has
 * pinned down — a literal transliteration there would be unverifiable
 * guesswork, which this project's own method notes explicitly warn
 * against. ai_euro_refresh_continent_stance already covers the G-table's
 * *effect* (nation x continent stance) via a from-scratch recompute, just
 * not FUN_521d_0a60's literal write path.
 *
 * DOS-only per-unit AI scratch bytes this section reads/writes
 * (unit+0x314b/c/d/e — "AI order code", "act state", "goal x/y") have no
 * persisted Linux struct field; modeled here as a file-local shadow array
 * (`s_0a60_pilot_state`, name kept from the original pilot pass) instead
 * of new ColonizeUnit fields — same pattern this file already uses for
 * `unit+0x314f` (`s_euro_last_dir`).
 */

typedef struct Ai0a60UnitState {
  uint8_t order_code;    /* unit+0x314b: '?'=0x3f pending, 'A'=admitted-
                           * labor, 't'=pursue-FOUND, 'i'=pursue-MIL_EXPAND,
                           * '1'=pursue-generic goal */
  uint8_t act_state;     /* unit+0x314c: 0/5/6 = idle/re-evaluate states
                           * this section reacts to; 0xb = now pursuing */
  uint8_t goal_x, goal_y; /* unit+0x314d/e: goal target tile once assigned */
  int8_t goal_code;       /* Linux-only mirror of the committed slot's
                           * AI_GOAL_* code (-1 = none) — DOS's own
                           * order_code byte doesn't distinguish LABOR vs.
                           * COLONY vs. COLONY_ALT vs. MILITARY vs. CONTACT,
                           * but ai_euro_unit_act's downstream dispatch
                           * (found/labor-bind/attack) needs the concrete
                           * value. Only meaningful while act_state==0xb;
                           * stale otherwise (matches order_code's own
                           * DOS-real staleness — see body). */
  uint8_t flags;          /* unit+0x3148 AI scratch bits, written by
                           * ai_euro_0a60_unit_housekeeping each nation turn
                           * (DOS resets bits 1/2/3/5 via `&= 0xd1` and
                           * rederives them — per-tick scratch, matching the
                           * col1_save.h bitfield names): bit1 0x02
                           * roam_reeval_pending (act_state 5/6), bit2 0x04
                           * stack_has_founders_or_military (FOUND-eligible),
                           * bit3 0x08 stack_has_military (MIL_EXPAND-
                           * eligible), bit5 0x20 spare-transport mark. */
} Ai0a60UnitState;

/*
 * DOS func_0x0001854c(dialog, difficulty>>3, 3, 99) — the per-nation
 * difficulty-scaled weight seed 0a60 seeds every primary-slot tally with
 * at entry (aiStack_1da[64]). Callee semantics (random? clamp? lookup
 * table?) unresolved. Placeholder: fixed midpoint of its own known [3,99]
 * clamp range — inert, deterministic, documented approximation.
 */
static int ai_euro_0a60_ph_weight_seed(void) { return 50; }

/*
 * DS:0x523d unit-type -> goal-code capability bitmask — resolved 2026-08-27:
 * it is NAMES.TXT @UNIT's trailing bit-string read MSB-first (see
 * k_20e6_type_flags below). This function predates that and still keys on
 * unit names; equivalent for the FOUND/MIL cases it handles. Mirror the file's existing convention (unit-name matching) for
 * land units rather than accept-all, since accept-all would let e.g. a
 * Galleon "pursue" a LABOR goal.
 *
 * unit+0x3148 bit2/bit3 (2026-08-18, resolved): the raw unit-loop (raw
 * lines ~645-683, still otherwise out of scope) sets bit2 whenever a
 * unit's own "stack" holds founders or military, bit3 specifically for
 * military — gated `if (unit is ship-type)` for the refinement that
 * matters here. `FUN_1000_8aac` modes 3/4/6 are now byte-resolved
 * (2026-09-06b, 0d38 case table at ai_euro_20e6_stack_count: # Pioneers /
 * # military types / mobilizable count); this predates that and computes
 * the same *information* directly from Linux's real ship cargo hold
 * (`cargo_ids[]`), which DOS's query would ultimately be reporting on
 * anyway for a transport. Closes a real capability gap: previously ships
 * could never satisfy FOUND/MIL_EXPAND at all (their names never match
 * the land-unit name checks below), even when visibly carrying settlers
 * or soldiers. Not modeled: DOS's "ship is full → also require every
 * earlier-indexed ship in the same stack/fleet to be full too" fleet-
 * coordination downgrade — a defensible superset (this port may let a
 * not-yet-fully-loaded fleet's full ship pursue slightly earlier).
 */
static int ai_euro_0a60_unit_can_pursue_goal(
  const char* name, int goal_code, int is_ship, int ship_has_founders, int ship_has_military
) {
  switch (goal_code) {
    case AI_GOAL_FOUND:
      if (is_ship) {
        return ship_has_founders || ship_has_military; /* unit+0x3148 bit2 */
      }
      return ai_euro_name_is_pioneer(name) || (name && strstr(name, "Colonist") != NULL);
    case AI_GOAL_MIL_EXPAND:
    case AI_GOAL_MILITARY:
      if (is_ship) {
        return ship_has_military; /* unit+0x3148 bit3 */
      }
      return ai_euro_is_military_name(name) || ai_euro_is_artillery_name(name);
    default:
      return 1; /* CONTACT/LABOR/COLONY/COLONY_ALT: no known DOS type gate here */
  }
}

/*
 * Soldier/Dragoon continent-defense skip gate (raw: `land_units_here =
 * table[-0x6b5a][continent + nation*0x10]`, `colonies = table[-0x6b1a][...]`
 * — the same colony-count / land-unit-count-by-continent tables
 * euro_g_table_0a60.md resolved and ai_euro_refresh_continent_stance
 * already recomputes for its own purpose). Recomputed fresh here too
 * (cheap, one pass) rather than exposing that function's private locals.
 */
static void ai_euro_0a60_continent_presence(
  const ColonizeTurnContext* ctx, int nation_id, int continent_id, int* out_colonies,
  int* out_land_units
) {
  int colonies = 0;
  int land_units = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == nation_id &&
          map_continent_id_at(ctx->map, c->x, c->y) == continent_id) {
        colonies++;
      }
    }
  }
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = units_get_const(ctx->units, i);
      if (u && u->active && u->nation_id == nation_id && !units_is_sea(ctx->units, i) &&
          map_continent_id_at(ctx->map, u->x, u->y) == continent_id) {
        land_units++;
      }
    }
  }
  *out_colonies = colonies;
  *out_land_units = land_units;
}

/*
 * FUN_521d_0a60's own goal-consumption tail, literally: for every idle-ish
 * unit of `nation_id`, pick the closest/highest-priority matching primary
 * goal slot and write order_code/act_state/goal_x/goal_y — mirrors raw
 * decomp lines 974-1063 control flow and arithmetic 1:1 (see file header
 * comment for what's real vs. placeholder). Pilot-only: writes into the
 * file-local shadow array below, not into ColonizeUnit or live orders.
 */
static Ai0a60UnitState s_0a60_pilot_state[COLONIZE_UNITS_MAX];

/* --- 0a60 unit-loop housekeeping (raw lines 1-189) ----------------------
 *
 * Literal port of FUN_521d_0a60's opening per-unit loop (raw decomp lines
 * 704-811 of euro_goal_orders_0a60_full.md's recovery): the unit+0x3148
 * scratch-bit rederive, act-state transitions, the spare-transport mark and
 * the foreign-ship CONTACT goal producer. Previously skipped behind the
 * "FUN_1000_8aac field-id wall".
 *
 * 2026-09-06b CORRECTION: the wall's "4fa8" resolution targeted the wrong
 * function — FUN_1000_8aac = FUN_281f_08bc → FUN_1427_0d38, the stack-
 * query dispatcher, ALL cases byte-decoded (table at
 * ai_euro_20e6_stack_count; the 4fa8 chain was a Ghidra reloc misresolve).
 * The modes 0a60's unit loop reads DO carry real signal:
 *     mode 3 → # Pioneers in the unit's stack;
 *     mode 4 → # military land types {1,4,6,7,8,9} (`8aac(u,4) < 2` =
 *              "fewer than two military in the stack" — a real gate);
 *     mode 6 → mobilizable count (mil + veteran-professioned).
 *   The eligibility block below still ships the earlier reading (bits 2+3
 *   on every land unit; ships from real cargo behind the hold-full/fleet
 *   check) — rewiring it to the real stack counts is flagged follow-up in
 *   euro_goal_orders_0a60_full.md, NOT silently changed here, because the
 *   current behavior is golden-tested and the consumption tail's type
 *   gate supplies the selectivity either way (DS:0x523d capability mask —
 *   Linux keeps its tested name-check equivalent there).
 *
 * Field ids resolved for the rest of the loop (address_mapping.csv chain
 * FUN_1000_X → FUN_281f_(X-0x81f0) → FUNCTION_CATALOG.md):
 *   FUN_1000_84f2 → FUN_137f_000a map_tile_in_bounds (inset interior);
 *   FUN_1000_8958 → FUN_13e4_0074 ocean_or_high_seas;
 *   FUN_1000_8d18 → FUN_15eb_08e6 unit-type-has-profession (region stamp
 *     bit variant only — the whole DS:0x9faa stamp is already covered by
 *     ai_coarse_fog_euro_restamp, which consumers only test for nonzero);
 *   -0x6da6/-0x6da5/0x9259 → unit_type_counts[4][19] (DS:0x924c,
 *     save_format_map.md row 252) at types 0x0e/0x0f/0x0d — Merchantman/
 *     Galleon/Caravel counts, recomputed here like FUN_4962_0018 does;
 *   DS:0x5382 bit0 → game_options.woi (Frigate CONTACT exception);
 *   unit+0x3147 high bits (0x10<<nation "this unit spotted") → per-nation
 *     map seen bit (map_tile_seen_by), the closest live substitute;
 *   relation gate (FUN_1000_8c28 raw peer byte): (d & 0x60) == 0x20 =
 *     MET(0x20) set + PEACE(0x40) clear — "met, no peace treaty".
 *
 * Runs before the colony loop each nation turn (DOS order). The shadow
 * state is fresh-zeroed each turn, so the pure act-state *resets* (orders
 * 'A'→'G', act 1/2/3→0) are structural no-ops kept for shape; the live
 * effects are the eligibility/spare bits, act_state=1 (in transit: aboard
 * ship, in Europe, off-map) excluding units from goal consumption,
 * act_state=10 (adjacent contact claim) doing the same, and the CONTACT
 * goals at spotted hostile ships.
 */
static int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);

/* DOS unit+0x3150 holds_occupied: passengers + occupied goods holds. */
static int ai_euro_0a60_holds_occupied(const ColonizeUnit* u) {
  int n = u->cargo_count;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (u->hold_goods_amount[i] > 0) {
      n++;
    }
  }
  return n;
}

/* Raw: type_table_5237[unit.type] == unit+0x3150 — "hold completely full". */
static int ai_euro_0a60_ship_full(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const int cap = units_ship_capacity(units, u->id);
  return cap > 0 && ai_euro_0a60_holds_occupied(u) >= cap;
}

static void ai_euro_0a60_unit_housekeeping(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  /* unit_type_counts[nation][0x0d/0x0e/0x0f] recompute (FUN_4962_0018). */
  int caravels = 0;
  int merchantmen = 0;
  int galleons = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->nation_id != nation_id) {
      continue;
    }
    const int t = ai_euro_20e6_dos_type(ctx->units, u);
    if (t == 0x0d) {
      caravels++;
    } else if (t == 0x0e) {
      merchantmen++;
    } else if (t == 0x0f) {
      galleons++;
    }
  }

  int spare_marked = 0; /* iStack_c: at most one spare-transport mark per turn */
  const int woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;

  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    const ColonizeUnit* u = units_get_const(ctx->units, ui);
    if (!u || !u->active) {
      continue;
    }
    const int dos_type = ai_euro_20e6_dos_type(ctx->units, u);
    const int is_ship_t = (dos_type >= 0x0d && dos_type <= 0x12);
    if (u->nation_id == nation_id) {
      Ai0a60UnitState* st = &s_0a60_pilot_state[ui];
      const int ux = u->x;
      const int uy = u->y;
      if (st->order_code == 'A') {
        st->order_code = 'G'; /* admitted labor → garrisoned (fresh shadow: no-op) */
      }
      st->flags &= 0xd1; /* rederive bits 1/2/3/5 below */
      if (st->act_state == 5 || st->act_state == 6) {
        st->flags |= 0x02; /* roam_reeval_pending */
      }

      /* FOUND/MIL_EXPAND eligibility bits (see header: shipped-DOS gates
       * are no-signal; ships use real cargo, land units always qualify). */
      int has_founders = 0;
      int has_military = 0;
      if (is_ship_t) {
        for (int ci = 0; ci < u->cargo_count && ci < COLONIZE_UNIT_CARGO_MAX; ++ci) {
          const ColonizeUnit* pax = units_get_const(ctx->units, u->cargo_ids[ci]);
          if (!pax || !pax->active) {
            continue;
          }
          const char* pn = units_display_name(ctx->units, pax);
          if (ai_euro_is_military_name(pn) || ai_euro_is_artillery_name(pn)) {
            has_military = 1;
          } else if (ai_euro_name_is_pioneer(pn) ||
                     (pn && strstr(pn, "Colonist") != NULL)) {
            has_founders = 1;
          }
        }
      } else {
        has_founders = 1;
        has_military = 1;
      }
      if (has_founders || has_military) {
        int ok = 1;
        if (is_ship_t) {
          /* Literal DOS: only a full ship qualifies, and only when no
           * earlier-indexed own ship in the same stack is still loading. */
          ok = ai_euro_0a60_ship_full(ctx->units, u);
          for (int oi = 0; ok && oi < ui; ++oi) {
            const ColonizeUnit* o = units_get_const(ctx->units, oi);
            if (!o || !o->active || o->nation_id != nation_id || o->x != ux ||
                o->y != uy) {
              continue;
            }
            const int ot = ai_euro_20e6_dos_type(ctx->units, o);
            if (ot >= 0x0d && ot <= 0x12 && !ai_euro_0a60_ship_full(ctx->units, o)) {
              ok = 0;
            }
          }
        }
        if (ok) {
          st->flags |= 0x04; /* stack_has_founders_or_military */
          if (has_military) {
            st->flags |= 0x08; /* stack_has_military */
          }
        }
      }

      /* Spare-transport mark (bit5, one ship per nation per turn): with
       * fewer than 2 Merchantman+Galleon (or no Merchantman), a 2nd+
       * Caravel is the spare; otherwise the first Merchantman is. Only for
       * ships not already FOUND/MIL_EXPAND-eligible. */
      if (!spare_marked && is_ship_t && (st->flags & 0x0c) == 0) {
        if (merchantmen + galleons < 2 || merchantmen == 0) {
          if (dos_type == 0x0d && caravels > 1) {
            st->flags |= 0x20;
            spare_marked = 1;
          }
        } else if (dos_type == 0x0e) {
          st->flags |= 0x20;
          spare_marked = 1;
        }
      }

      /* Tile housekeeping: act-state transitions. */
      const int inset = (ux >= 1 && uy >= 1 && ux < ctx->map->width - 1 &&
                         uy < ctx->map->height - 1);
      int in_transit = 1;
      if (inset) {
        /* DS:0x9faa region stamp (|=1 / |=5 by profession-capability) is
         * covered by ai_coarse_fog_euro_restamp — its consumers only test
         * the byte for nonzero, so the 1-vs-5 split is behaviorally inert. */
        if (st->act_state == 1 || st->act_state == 2 || st->act_state == 3 ||
            (st->act_state >= 10 && st->order_code != 0x31)) {
          st->act_state = 0;
        }
        int side = 0;
        if (ai_goals_probe_adjacent_contact_claim(
              ctx->map, ctx->colonies, ux, uy, nation_id, 1, &side
            ) >= 0) {
          st->act_state = 10; /* on-site at a contact claim: no new goal */
        }
        const int on_water = map_tile_is_water(ctx->map, ux, uy) ||
                             map_tile_is_high_seas(ctx->map, ux, uy);
        if (!on_water || is_ship_t) {
          in_transit = 0;
        }
      }
      if (in_transit) {
        st->act_state = 1; /* aboard ship / in Europe / off-map */
      }
    } else if (u->nation_id >= 0 && u->nation_id < 4) {
      /* Foreign branch: spotted hostile ship → CONTACT goal prio 3.
       * Frigates only count once independence is declared (DS:0x5382 bit0). */
      if (is_ship_t && (dos_type != 0x11 || woi) &&
          map_tile_seen_by(ctx->map, u->x, u->y, nation_id) && ctx->col1_ok &&
          ctx->col1) {
        const uint8_t d = ai_diplo_read(ctx->col1, nation_id, u->nation_id);
        if ((d & (AI_DIPLO_MET | AI_DIPLO_PEACE)) == AI_DIPLO_MET || dos_type == 0x10) {
          ai_goals_upsert_primary(nation_id, u->x, u->y, AI_GOAL_CONTACT, 3);
        }
      }
    }
  }
}

static void ai_euro_0a60_goal_orders_structural(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  const int weight_seed = ai_euro_0a60_ph_weight_seed();
  int weight[AI_PRIMARY_SLOTS];
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    weight[i] = weight_seed;
  }

  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    const ColonizeUnit* u = units_get_const(ctx->units, ui);
    if (!u || !u->active || u->nation_id != nation_id) {
      continue;
    }
    /*
     * s_0a60_pilot_state is reset every call (see its dispatcher_turn
     * reset comment), so a mid-job Pioneer (CLEAR_PLOW/BUILD_ROAD) always
     * starts this loop with act_state==0 — without this guard it gets a
     * fresh goal assigned every turn regardless of its in-progress order,
     * hijacking it before units_pioneer_work_tick ever finishes. Was
     * invisible while the real DS:0x2f78 threshold was unknown and every
     * job finished in a single tick; exposed once the real (usually
     * multi-turn) threshold was captured 2026-08-20.
     */
    if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
      continue;
    }
    Ai0a60UnitState* st = &s_0a60_pilot_state[ui];
    if (st->order_code == 'A') {
      continue; /* already admitted as labor */
    }
    if (st->act_state < 10) {
      st->order_code = 0x3f; /* '?' pending-decision placeholder */
    }
    if (st->act_state != 0 && st->act_state != 5 && st->act_state != 6) {
      continue;
    }
    if (st->order_code == 't' || st->order_code == 'i') {
      st->order_code = 0x3f; /* clear stale goal-pursuit code */
    }

    const char* uname = units_display_name(ctx->units, u);
    const int unit_is_ship = ai_euro_is_ship_type(ctx->units, ui);
    const int unit_continent = map_continent_id_at(ctx->map, u->x, u->y);

    /* unit+0x3148 bits 2/3, now written for real by
     * ai_euro_0a60_unit_housekeeping (which also applies DOS's literal
     * hold-full + fleet-coordination gate the old inline cargo scan here
     * skipped): bit2 = FOUND-eligible, bit3 = MIL_EXPAND-eligible. */
    const int ship_has_founders = unit_is_ship && (st->flags & 0x04) != 0;
    const int ship_has_military = unit_is_ship && (st->flags & 0x08) != 0;

    if (!unit_is_ship && (strstr(uname ? uname : "", "Soldier") ||
                           strstr(uname ? uname : "", "Dragoon"))) {
      int colonies = 0;
      int land_units = 0;
      ai_euro_0a60_continent_presence(ctx, nation_id, unit_continent, &colonies, &land_units);
      if (land_units < 3 && (land_units < 2 || colonies == 0)) {
        continue; /* don't reassign a defender off a continent that still needs it */
      }
    }

    int best_score = 9999;
    int best_slot = -1;
    for (int slot = 0; slot < AI_PRIMARY_SLOTS; ++slot) {
      const AiGoalSlot* g = ai_goals_primary(nation_id, slot);
      if (!g || g->code == AI_GOAL_EMPTY) {
        continue;
      }
      if (!ai_euro_0a60_unit_can_pursue_goal(
            uname, g->code, unit_is_ship, ship_has_founders, ship_has_military
          )) {
        continue;
      }
      const int goal_continent = map_continent_id_at(ctx->map, g->x, g->y);
      if (goal_continent != unit_continent && !unit_is_ship) {
        continue; /* off-continent goal, land unit can't reach it */
      }

      const int dist = ai_euro_dos_dist(g->x - u->x, g->y - u->y);
      const int score = weight[slot] * dist / (g->prio + 1);

      if ((st->act_state == 5 || st->act_state == 6) && !unit_is_ship) {
        /*
         * Real check (fixed 2026-08-18, `address_mapping.csv`:
         * FUN_1000_8886 → canonical FUN_281f_0696 → FUN_137f_0358 =
         * `euro_settlement_owner`, already resolved in `accessors.c` —
         * NOT a "is anyone standing on the goal tile" unit lookup as
         * first ported. DOS's own args are the *unit's own* x/y
         * (`uStack_36`/`uStack_3a`, set from `unit+0x3144/0x3145`
         * earlier this same block), not the goal's — this checks
         * whether the re-evaluating unit is *currently sitting in any
         * Euro colony* (any nation), not whether the goal is occupied.
         * `colonies_id_at` is the Linux equivalent (colony pool holds
         * only Euro colonies; native villages are a separate `col1`
         * table, matching DOS's own tribe-owner exclusion in
         * `euro_settlement_owner`).
         */
        const int at_colony = colonies_id_at(ctx->colonies, u->x, u->y) >= 0;
        if (at_colony ||
            (g->prio < 3 || (g->prio * weight_seed < score && weight[slot] != weight_seed))) {
          continue; /* re-evaluating unit already parked in a colony, or goal not worth it */
        }
      }

      if (score < best_score && score / weight_seed <= g->prio * 3 / 2) {
        best_score = score;
        best_slot = slot;
      }
    }

    if (best_slot >= 0) {
      const AiGoalSlot* g = ai_goals_primary(nation_id, best_slot);
      st->order_code = 0x31; /* '1' default goal-pursue code */
      if (g->code == AI_GOAL_FOUND) {
        st->order_code = 0x74; /* 't' */
      } else if (g->code == AI_GOAL_MIL_EXPAND) {
        st->order_code = 0x69; /* 'i' */
      }
      st->act_state = 0xb; /* pursuing a goal */
      st->goal_x = (uint8_t)g->x;
      st->goal_y = (uint8_t)g->y;
      st->goal_code = (int8_t)g->code;
      if (g->code != AI_GOAL_MILITARY) {
        weight[best_slot]++; /* claim-count so the same slot isn't over-assigned */
      }
    }
  }
}

/* --- 0a60 foreign-colony / village goal producers ----------------------
 *
 * Literal port of FUN_521d_0a60's foreign-colony branch + village loop
 * (viceroy_unpacked.c ~87795-88052; raw lines 983-1276 of
 * euro_goal_orders_0a60_full.md's recovery) — the FOUND/CONTACT producers
 * that write OCEAN tiles next to foreign colonies and villages as ship
 * goals. These pair with the DS:0x523d capability mask: FOUND(1)/
 * MIL_EXPAND(7) goals match bit1/bit7 = the transport ships
 * (Caravel/Merchantman/Galleon 0xa2/0x82/0x82), CONTACT(0) matches bit0 =
 * the warships — i.e. DOS stages loaded transports at open-sea tiles
 * adjacent to land worth settling, and lurks warships two tiles off
 * foreign harbors. Land units can't take these goals (water tile →
 * continent −1 ≠ any land continent, and the tail's ship gate).
 *
 * Resolved symbols (FUN_1000_X → FUN_281f_(X−0x81f0) → catalog):
 *   84f2 map_tile_in_bounds; 8958 ocean_or_high_seas; 88a4 layer3 low
 *   nibble (raw water-region id — region 1 = open sea, see map.c's lake
 *   note); 8912 map_continent_id_at (land only, −1 on water); 893a
 *   tile_explore_mask (bit 0x10<<nation = map_tile_seen_by); 8872
 *   tile_owner_or_presence (layer2 bit0 + layer3 owner nibble); 8bd6/8c3c
 *   colony/village binds; 84fc alarm_by_player; 8c28 raw peer byte
 *   ((d&0x48)==0x40 = PEACE set + amicable-latch 0x08 clear;
 *   (d&0x60)==0x20 = MET set + PEACE clear); DS:0x538e head.turn;
 *   DS:0x53a6 head.difficulty; DS:0x543f polarity 0 = human;
 *   −0x6ada skilled-unit counts (FUN_281f_0b78 = "type has a profession
 *   slot"); −0x7a38 continent_tally_b; DS:0x173c/0x173e per-continent
 *   MIL_EXPAND/FOUND-registered masks (zeroed at 0a60 entry — locals
 *   here, shared between the two loops exactly as DOS shares them).
 *
 * FUN_1000_8aac mode 2 — 2026-09-06b: the real 0d38 case-2 body IS the
 * plain stack count, so ai_euro_0a60_units_on_tile is byte-right (the old
 * "4fa8 chain splice, substituted" story targeted the wrong function).
 * Mode 0xd (every-4th CONTACT-scan skip) is really Σ ship hold capacity
 * in the stack — still treated as pass here, follow-up flagged in
 * euro_goal_orders_0a60_full.md.
 * The village-population scratch (aiStack_14e) only feeds a dead
 * accumulator and the G-formula's Indian presence test, which Linux's
 * stance recompute already covers via live Brave units — not modeled.
 */

/* Raw water-region check: FUN_1000_8958 && FUN_1000_88a4(x,y) == 1. */
static int ai_euro_0a60_open_sea(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  if (!map_tile_is_water(map, x, y) && !map_tile_is_high_seas(map, x, y)) {
    return 0;
  }
  return (map_get_layer3(map, x, y) & 0x0fu) == 1;
}

/* FUN_1000_8872 tile_owner_or_presence: layer2 bit0 + layer3 owner nibble. */
static int ai_euro_0a60_tile_owner_or_presence(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->layer2 || x < 1 || y < 1 || x >= map->width - 1 ||
      y >= map->height - 1) {
    return -1;
  }
  if ((map->layer2[y * map->width + x] & 1u) == 0) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  return hi == 0x0f ? -1 : hi;
}

/* FUN_1000_8aac(unit_at_tile, 2) = the tile's stack size — byte-confirmed
 * 2026-09-06b (0d38 case 2 is a bare per-member count). */
static int ai_euro_0a60_units_on_tile(const ColonizeUnitPool* units, int x, int y) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(units, i);
    if (u && u->active && u->aboard_ship_id < 0 && u->x == x && u->y == y) {
      n++;
    }
  }
  return n;
}

static void ai_euro_0a60_settlement_goal_producers(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units || !ctx->colonies) {
    return;
  }
  const ColonizeWorldMap* map = ctx->map;
  const int have_col1 = (ctx->col1_ok && ctx->col1 != NULL);
  const int turn = have_col1 ? (int)ctx->col1->head.turn
                             : ((ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0);
  const int difficulty = have_col1 ? (int)ctx->col1->head.difficulty : 2;
  const int human = have_col1 ? (int)ctx->col1->head.human_player : -1;

  /* FUN_4962_0018-style per-nation/continent tables (colonies, land units,
   * skilled units — the −0x6b1a/−0x6b5a/−0x6ada trio). */
  uint8_t col_cnt[4][16];
  uint8_t land_cnt[4][16];
  uint8_t skilled_cnt[4][16];
  memset(col_cnt, 0, sizeof(col_cnt));
  memset(land_cnt, 0, sizeof(land_cnt));
  memset(skilled_cnt, 0, sizeof(skilled_cnt));
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    const int cid = map_continent_id_at(map, c->x, c->y);
    if (cid >= 0 && cid < 16 && col_cnt[c->nation_id][cid] < 0xff) {
      col_cnt[c->nation_id][cid]++;
    }
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = units_get_const(ctx->units, i);
    if (!u || !u->active || u->nation_id < 0 || u->nation_id > 3 ||
        units_is_sea(ctx->units, i)) {
      continue;
    }
    const int cid = map_continent_id_at(map, u->x, u->y);
    if (cid < 0 || cid >= 16) {
      continue;
    }
    if (land_cnt[u->nation_id][cid] < 0xff) {
      land_cnt[u->nation_id][cid]++;
    }
    if (units_type_has_profession_slot(u->type_index) &&
        skilled_cnt[u->nation_id][cid] < 0xff) {
      skilled_cnt[u->nation_id][cid]++;
    }
  }

  static const int dx8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  uint16_t mask_mil_expand = 0; /* DS:0x173c */
  uint16_t mask_found = 0;      /* DS:0x173e */

  /* Foreign-colony loop (raw 983-1212). */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id == nation_id || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    const int owner = c->nation_id;
    const int cont = map_continent_id_at(map, c->x, c->y);
    if (cont < 0 || cont >= 16) {
      continue;
    }
    const int seen = map_tile_seen_by(map, c->x, c->y, nation_id);
    const uint8_t d = have_col1 ? ai_diplo_read(ctx->col1, nation_id, owner) : 0;
    /* (d & 0x48) == 0x40 — PEACE without the amicable latch. */
    const int at_peace = ((d & (AI_DIPLO_PEACE | 0x08)) == AI_DIPLO_PEACE);
    /* Early fair-play gate #1 (difficulty×turn < 0xb5): skip the approach/
     * lurk block for an unseen human colony. DS:0x543f polarity 0=human. */
    const int block1 = !(difficulty * turn < 0xb5 && !seen && owner == human);
    if (block1) {
      /* MILITARY approach goal (defender count is a mode-2 substitution,
       * see header). Prio 3 while formally at peace, 5 otherwise. */
      if ((int)col_cnt[nation_id][cont] + (int)land_cnt[nation_id][cont] != 0 &&
          ((i + turn) & 3) != 0) {
        const int defenders =
          ai_euro_0a60_units_on_tile(ctx->units, c->x, c->y) + (int)c->population;
        if (defenders > 6 - turn / 50) {
          ai_goals_upsert_primary(
            nation_id, c->x, c->y, AI_GOAL_MILITARY, at_peace ? 3 : 5
          );
        }
      }
      if (at_peace) {
        continue; /* raw: goto next colony — no lurk/staging vs peace peers */
      }
      /* CONTACT lurk scan: ring-2 open-sea tiles off a coastal foreign
       * colony (raw 1012-1089; +0x1c bit 0x40 coastal, live recompute).
       * The raw every-4th `8aac(...,0xd)==0` early-out is really "Σ ship
       * hold capacity in the stack == 0" (0d38 case 0xd, 2026-09-06b) —
       * still ported as never-taken; rewire flagged as 0a60 follow-up. */
      if (map_tile_is_coastal(map, c->x, c->y)) {
        int best = 0;
        int bx = 0;
        int by = 0;
        for (int dy = -2; dy <= 2; ++dy) {
          for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) {
              continue;
            }
            if (abs(dx) != 2 && abs(dy) != 2) {
              continue; /* Chebyshev ring 2 only */
            }
            const int tx = c->x + dx;
            const int ty = c->y + dy;
            if (!ai_euro_0a60_open_sea(map, tx, ty)) {
              continue;
            }
            int cnt = 0;
            for (int k = 0; k < 8; ++k) {
              const int nx = tx + dx8[k];
              const int ny = ty + dy8[k];
              if (!ai_euro_0a60_open_sea(map, nx, ny)) {
                continue;
              }
              if (abs(c->x - nx) < 2 && abs(c->y - ny) < 2) {
                cnt++; /* open-sea tile adjacent to both candidate and colony */
              }
            }
            if (best < cnt) {
              best = cnt;
              bx = tx;
              by = ty;
            }
          }
        }
        if (best > 0) {
          int p = ((int)c->population + 4) >> 3;
          if (p > 2) {
            p = 2;
          }
          ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_CONTACT, p + 2);
        }
      }
    }
    /* LAB_521d_11b6: FOUND/MIL_EXPAND ship-staging scan. Early fair-play
     * gate #2 (difficulty×turn < 0xc9): unseen colony → next colony. */
    if (difficulty * turn < 0xc9 && !seen) {
      continue;
    }
    int outmatched = 0; /* iStack_2e: fewer colonies here than a developed owner */
    int absent = 0;     /* bVar5: no colonies here, owner not yet developed */
    if (col_cnt[nation_id][cont] < col_cnt[owner][cont] && skilled_cnt[owner][cont] >= 8) {
      outmatched = 1;
    }
    if (col_cnt[nation_id][cont] == 0 && skilled_cnt[owner][cont] < 8) {
      absent = 1;
    }
    if (!outmatched && !absent) {
      continue;
    }
    int best = -99;
    int bx = c->x;
    int by = c->y;
    for (int dx = -3; dx <= 3; ++dx) {
      for (int dy = -3; dy <= 3; ++dy) {
        const int tx = c->x + dx;
        const int ty = c->y + dy;
        if (!ai_euro_0a60_open_sea(map, tx, ty)) {
          continue;
        }
        int cnt = 0;
        for (int k = 0; k < 8; ++k) {
          const int nx = tx + dx8[k];
          const int ny = ty + dy8[k];
          if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
            continue;
          }
          if (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) {
            continue;
          }
          if (map_continent_id_at(map, nx, ny) == cont) {
            cnt++;
          }
        }
        if (cnt != 0) {
          const int sc = (abs(dx) + abs(dy) + cnt) * 2;
          if (best <= sc) { /* raw `iStack_15a <= iStack_e`: later ties win */
            best = sc;
            bx = tx;
            by = ty;
          }
        }
      }
    }
    if (best <= 0) {
      continue;
    }
    if (ai_euro_0a60_tile_owner_or_presence(map, bx, by) >= 0) {
      continue; /* FUN_1000_8872: someone already claims that tile */
    }
    if (outmatched) {
      mask_mil_expand |= (uint16_t)(1u << cont);
    } else {
      mask_found |= (uint16_t)(1u << cont);
    }
    /* Priority ladder, transliterated (raw 1162-1196). */
    const int tally =
      have_col1 ? (int)ctx->col1->post_map.continent_tally_b[cont] : 0;
    const int total_cols = (int)col_cnt[0][cont] + (int)col_cnt[1][cont] +
                           (int)col_cnt[2][cont] + (int)col_cnt[3][cont];
    int e = outmatched ? 3 : 2;
    int v = e;
    if (owner == human) {
      v = e + 1;
      if (total_cols == (int)col_cnt[owner][cont]) {
        if (tally > 0xf) {
          v = e + 2;
        }
        e = v;
        v = e;
        if (tally > 0x3f) {
          v = e + 1;
        }
      }
    }
    e = v;
    if (tally < total_cols * 0x10) {
      e -= 1;
    }
    if ((d & (AI_DIPLO_MET | AI_DIPLO_PEACE)) == AI_DIPLO_MET) {
      e += 1; /* met, no peace treaty */
    }
    if (turn < 0x96) {
      e <<= 1;
    }
    /* Weakly-defended target cancels the staging goal (raw 1197-1203). */
    const int defenders =
      ai_euro_0a60_units_on_tile(ctx->units, c->x, c->y) + (int)c->population;
    if (defenders <= 6 - turn / 50) {
      outmatched = 0;
      absent = 0;
    }
    if (outmatched || absent) {
      ai_goals_upsert_primary(
        nation_id, bx, by, outmatched ? AI_GOAL_MIL_EXPAND : AI_GOAL_FOUND, e
      );
    }
  }

  /* Village loop (raw 1215-1276). */
  if (have_col1 && ctx->col1->tribe) {
    for (uint16_t vi = 0; vi < ctx->col1->head.tribe_count; ++vi) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[vi];
      const int vx = (int)t->x;
      const int vy = (int)t->y;
      const int cont = map_continent_id_at(map, vx, vy);
      if (cont < 0 || cont >= 16) {
        continue;
      }
      const int ind = (int)t->nation_id; /* 4..11 */
      /* MILITARY at the village: own presence on the continent, and either
       * alarm >= 0x4b or the Indian-matrix WAR bit (FUN_1000_8c28 & 2).
       * Prio 2 for a mission-less village (+5 byte 0xff → sign bit), 4
       * when a mission stands there. */
      if ((int)col_cnt[nation_id][cont] + (int)land_cnt[nation_id][cont] != 0 &&
          ind >= 4 && ind < 12) {
        const int alarm = ai_diplo_indian_alarm(ctx->col1, ind, nation_id);
        int fire = 1;
        if (alarm < 0x4b) {
          fire = (ctx->col1->indian[ind - 4].euro_diplo[nation_id] &
                  COL1_INDIAN_WAR_BIT) != 0;
        }
        if (fire) {
          const int prio = (t->mission & 0x80u) ? 2 : 4;
          ai_goals_upsert_primary(nation_id, vx, vy, AI_GOAL_MILITARY, prio);
        }
      }
      /* Ship FOUND staging next to a village on a continent with no own
       * colony and no staging goal registered yet this turn: best village-
       * adjacent open-sea tile by count of same-continent land neighbours
       * (a zero-count ocean tile still qualifies — DOS init is −1). */
      if ((mask_mil_expand & (1u << cont)) == 0 && (mask_found & (1u << cont)) == 0 &&
          col_cnt[nation_id][cont] == 0) {
        int best = -1;
        int bx = -1;
        int by = -1;
        for (int k = 0; k < 8; ++k) {
          const int tx = vx + dx8[k];
          const int ty = vy + dy8[k];
          if (!ai_euro_0a60_open_sea(map, tx, ty)) {
            continue;
          }
          int cnt = 0;
          for (int m = 0; m < 8; ++m) {
            const int nx = tx + dx8[m];
            const int ny = ty + dy8[m];
            if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
              continue;
            }
            if (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) {
              continue;
            }
            if (map_continent_id_at(map, nx, ny) == cont) {
              cnt++;
            }
          }
          if (best < cnt) {
            best = cnt;
            bx = tx;
            by = ty;
          }
        }
        if (bx > 0) { /* raw `0 < (int)uStack_24` */
          ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_FOUND, 2);
          mask_found |= (uint16_t)(1u << cont);
        }
      }
    }
  }
}

/* --- 0a60 colony goals ------------------------------------------------- */

static void ai_euro_colony_goals(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  /* FUN_15eb_28c8's structural port (T1.17) is reference-only, not called
   * from the live colonist-job path below (see its own header comment for
   * scope). W1.7 (2026-08-24) added a golden fixture verifying the 9-job
   * formula — see tests/unit/test_ai_euro_28c8_job_score.c — but wiring it
   * live is Tier 3 (docs/port_plan.md W3.1), a user-confirmed behavior
   * change, not attempted here. External linkage (declared in ai_euro.h)
   * so the fixture can call it directly; still address-taken by nothing
   * else in this file, same convention as
   * ai_euro_5d04_nation_planning_structural. */
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  ai_goals_clear_work_queue();

  /* A: urgency seed; FUN_1d1d_0dae(0x9faa,0,0x10e) coarse-plane wipe + restamp. */
  ai_coarse_fog_euro_restamp(ctx->units, ctx->colonies, nation_id);
  /* Raw lines 1-189: per-unit 0x3148 housekeeping + foreign-ship CONTACT
   * producer, DOS position (after the memsets, before the colony loop). */
  ai_euro_0a60_unit_housekeeping(ctx, nation_id);
  const int urgency = inv ? inv->urgency : 0;

  /* B: own units — CONTACT from adjacent foreign; work queue only for bindable. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
      continue;
    }
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int d = 0; d < 8; ++d) {
      const int nx = u->x + dx[d];
      const int ny = u->y + dy[d];
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id != nation_id) {
        ai_goals_upsert_primary(nation_id, nx, ny, AI_GOAL_CONTACT, 3);
        ai_goals_upsert_work(u->id, 3, AI_GOAL_CONTACT, 0);
      }
    }
  }

  /* D: own colonies — LABOR from tools/food shortage / underpop (5cf6 tallies),
   * Col1 labor_shortage (+0x8e), or Stockade/Warehouse under construction.
   * Threatened Stockade deepen: war-peer within MD≤3 + incomplete Stockade →
   * higher LABOR prio so Free Colonist prefers hammers over distant FOUND.
   * Cite: building_production.md Stockade defense; Colonization.pdf fortify;
   * ai_euro_colony_threatened_by_war MD≤3; euro_unit_act §2e / case 0x0b. */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      ai_euro_refresh_colony_ai_flags(ctx, nation_id, c);
      int labor = (c->population < 3) || (c->labor_shortage > 0) ||
                  ai_euro_colony_food_short(c);
      if (inv && inv->tools_short > 0 && c->stock[COLONIZE_CARGO_TOOLS] < 20) {
        labor = 1;
      }
      if (inv && inv->food_short > 0 && c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
        labor = 1;
      }
      const int construction = ai_euro_colony_wants_construction_labor(ctx->colonies, c);
      if (construction) {
        labor = 1;
        /* Latch Col1 +0x1d bit7 when Linux sees named construction. */
        if (c->building_in_production >= 0) {
          c->build_ai_flags |= COLONIZE_BUILD_AI_WANTS_CONSTRUCTION;
        }
      }
      if (labor) {
        int labor_prio = construction ? 6 : (4 + urgency / 4);
        /* Thin demand latch when Linux detects LABOR need; full FUN_5952_035e
         * seed (local_76 / 0x8d72) PARKED — do not invent tallies. */
        if (c->labor_shortage == 0) {
          c->labor_shortage = 1;
        }
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, labor_prio);
      } else if (c->ai_flags & (COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP |
                                 COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR)) {
        /*
         * Real 0a60 write site (raw decomp, thunk_FUN_2a1f_0470 call #2 in
         * the colony loop): code is actually CONTACT(0), not a distinct
         * COLONY/COLONY_ALT type — Linux keeps its own COLONY/COLONY_ALT
         * codes (downstream ai_euro_unit_act already branches on them for
         * "go work/garrison this colony", a real behavior CONTACT's own
         * downstream handling — move-and-attack — doesn't have), but the
         * *gate* is real DOS: only fires when the colony's ai_flags bit0
         * (nearby armed ship) or bit1 (nearby Man-O-War) is set — prio 8 if
         * bit1, else 5. Was unconditional ("else always register a visit
         * goal"), which invented a goal DOS wouldn't have here and let it
         * out-compete FOUND under the real prio-weighted formula whenever a
         * colony had nothing better to report — see
         * euro_goal_orders_0a60_full.md, "blocks getting the structure
         * right" fix, 2026-08-18 (root cause of the unit_ai_euro_expand
         * regression from making the goal-consumption tail live).
         */
        const int mow = (c->ai_flags & COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR) != 0;
        ai_goals_upsert_primary(
          nation_id,
          c->x,
          c->y,
          mow ? AI_GOAL_COLONY_ALT : AI_GOAL_COLONY,
          mow ? 8 : 5
        );
      }
      /*
       * Thin garrison_quota latch: idle unfortified Soldier/Dragoon/… on tile
       * and quota==0 → 1. Skip while NEEDS_COLONISTS / LABOR so early towns
       * admit the beachhead soldier (Isabella TURN4→5) instead of fortifying.
       * Full threat>>3 FUN_5952_035e seed PARKED.
       */
      if (c->garrison_quota == 0 && !labor && ctx->units) {
        for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
          const ColonizeUnit* gu = &ctx->units->units[ui];
          if (!gu->active || gu->nation_id != nation_id || gu->x != c->x ||
              gu->y != c->y) {
            continue;
          }
          if (units_is_sea(ctx->units, gu->id)) {
            continue;
          }
          const char* gn = units_display_name(ctx->units, gu);
          if (!ai_euro_is_colony_garrison_name(gn) && !ai_euro_is_artillery_name(gn)) {
            continue;
          }
          if (gu->orders == UNITS_ORDER_FORTIFY || gu->orders == UNITS_ORDER_FORTIFIED) {
            continue;
          }
          c->garrison_quota = 1;
          break;
        }
      }
      /* Expand: FOUND via 06ae around colony (coastal prefer when count≥1). */
      int fx = 0;
      int fy = 0;
      const int own_n =
        inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
      if (ai_euro_pick_founding_tile(
            ctx->map,
            ctx->colonies,
            ctx->col1_ok ? ctx->col1 : NULL,
            nation_id,
            c->x,
            c->y,
            own_n,
            &fx,
            &fy)) {
        if (fx != c->x || fy != c->y) {
          ai_goals_upsert_primary(nation_id, fx, fy, AI_GOAL_FOUND, 2);
        }
      }
      /*
       * 0a60 work-queue haul score, real formula (raw decomp ~lines
       * 528-604 of the colony loop, `thunk_FUN_2a1f_0524` =
       * `upsert_work_queue`; was the thin "16×6 matrix OPEN" idle*8+
       * specialty-bump stand-in). The "16×6 matrix" turned out to be:
       * per-cargo Σ `euro_price[cargo][nation] * clamp(f(stock,target),
       * 0,target)` over all 16 cargo slots except FOOD(0)/LUMBER(5)/
       * TRADE_GOODS(13) — confirmed real, both tables already live in
       * Linux (`col1->nation[n].trade.euro_price[]`, `col1_save.h`;
       * `c->stock[]`, same 16-slot order, cross-checked field-for-field
       * against `col1_save.h`'s Col1 colony struct at +0x9a). TOOLS(14)/
       * MUSKETS(15) only contribute (with a flat −100 discount) when
       * `cargo_produced_mask` has that bit set this tick — otherwise
       * skipped entirely, not just discounted (DOS `goto`s past them).
       * HORSES(8) below target gets a small floor-adjust
       * (`stock+(25−target)`, clamped ≥0) instead of the plain `f()`.
       * `f(stock,target)`: below target → stock as-is (HORSES exception
       * above); at/above target → stock doubled (still capped to target
       * right after). `target` is `FUN_1000_8f2a()` — a single scalar
       * whose callee is unresolved (three other unrelated call sites
       * across this project, never named); approximated as a fixed 100,
       * matching base Warehouse capacity — the only DOS-documented
       * "target stock level" constant already in this codebase.
       * DELIBERATELY NOT ported: a real DOS pre-loop over this nation's
       * units adds a further +800 (idle Missionary) / +1500 (exposed
       * combat-capable land unit on a `stance==0` continent, i.e.
       * `ai_euro_continent_stance_at()==0`) saturating bonus into this
       * same score/flag_b before the cargo loop even starts — skipped
       * here: its own unit-iterator call (`FUN_1000_89d0`, no explicit
       * x/y) was traced in `move_scoring_20e6_full.md`'s `FUN_1000_8aac`
       * investigation to depend on caller-context registers this project
       * has no cheap way to read, i.e. genuinely open, not a shortcut
       * skipped for convenience.
       * Linux-only kept as-is (not DOS-real, but already useful and
       * load-bearing downstream): `flag_a` stays the specialty-cargo
       * hauler-match hint (`ai_euro_4393_work_queue_haul_pick`), `flag_b`
       * stays the haul-vs-CONTACT work-queue discriminator — DOS's own
       * `flag_a`/`flag_b` here are actually the accumulated per-slot
       * count and the deferred exposed-unit boolean above, neither of
       * which this port currently produces meaningfully; renaming
       * Linux's fields to match would break the already-working
       * specialty-match consumer for no behavioral gain.
       * Cite: move_scoring_ship.md Series F2; col1_save.h `stock`/
       * `trade.euro_price`/`cargo_produced_mask`.
       */
      {
        /*
         * Registration gate: kept as the existing simple stock<threshold
         * check (`ai_euro_colony_haul_cargo_short`, already tested) rather
         * than switching to DOS's own "any post-formula slot value >0x4a"
         * signal, even now that `target` below is the real per-colony
         * warehouse capacity (see resolved comment further down) —
         * changing the gate is an independent decision from getting the
         * formula right, and the existing threshold is already tested
         * against real fixtures (small-colony stock scale can still land
         * under 0x4a post-formula even at a correct `target`). The
         * *score* is the real formula; only the "register at all"
         * boolean stays pragmatic Linux.
         */
        const int haul_short =
          ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_TOOLS) ||
          ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_LUMBER) ||
          ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_ORE) ||
          ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_MUSKETS) ||
          ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_HORSES) ||
          ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_FOOD);
        /*
         * FUN_1000_8f2a() RESOLVED (2026-08-18, static — no live session
         * needed): address_mapping.csv's canonical chain
         * FUN_1000_8f2a → FUN_281f_0d3a → FUN_15eb_0a50 is exactly the
         * already-known, already-documented warehouse-capacity formula
         * (`save_format_map.md`/`FUNCTION_CATALOG.md`: 100×(1+
         * warehouse_level)) — already live in Linux as
         * `colonies_warehouse_capacity`. DOS calls this once per colony
         * (no cargo_type arg), same as here; pass a non-FOOD cargo type
         * since that function's only cargo-dependent branch is FOOD's
         * separate 199 cap (irrelevant here — FOOD is already skipped by
         * this loop).
         */
        const int target =
          colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_TOOLS);
        const ColonizeCol1Nation* nat =
          (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4)
            ? &ctx->col1->nation[nation_id]
            : NULL;
        long wscore = 0;
        /*
         * Missionary / exposed-combat-unit bonus (raw lines ~536-563,
         * same colony loop, before the cargo-weight scan below) — DOS
         * walks units *stacked at this colony's own tile* via a
         * transport-chain stack walk (`unit_index_on_tile` + prev-link
         * follow, `FUN_1000_89d0`/`84d4`; both resolved this pass via
         * `address_mapping.csv`: canonical `FUN_281f_07e0`/`02e4`,
         * already-known `ai/accessors.c` unit-on-tile + transport-chain
         * helpers). Linux has no live per-tile unit stack to walk, so
         * iterate + filter x/y instead — same substitution this file
         * already uses elsewhere (e.g. the garrison_quota scan just
         * above). +800 (saturating in DOS; harmless to add plain here,
         * the shared clamp below still applies) per Missionary/Jesuit
         * here, when colony ai_flags bit7 is clear — DOS bit, unnamed,
         * no Linux field; approximated as always-clear (always counted),
         * a defensible superset since the real gate would only narrow
         * this. +1500 per exposed combat-capable land unit (attack>1,
         * not a ship) when this continent has no G-table stance assigned
         * (`ai_euro_continent_stance_at()==0`) and the unit isn't
         * garrisoned/admitted — that last check is against this port's
         * own 0a60 shadow state (`s_0a60_pilot_state`), which is always
         * fresh-zeroed at this point in the turn (`ai_euro_colony_goals`
         * runs before `ai_euro_0a60_goal_orders_structural` populates
         * it), so it's always satisfied here — correct given execution
         * order, not a shortcut.
         */
        if (ctx->units) {
          const int cid = map_continent_id_at(ctx->map, c->x, c->y);
          for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
            const ColonizeUnit* u = &ctx->units->units[ui];
            if (!u->active || u->x != c->x || u->y != c->y) {
              continue;
            }
            if (ai_euro_is_missionary_name(units_display_name(ctx->units, u))) {
              wscore += 800;
            }
            if (ai_euro_continent_stance_at(nation_id, cid) == 0 &&
                !units_is_sea(ctx->units, ui)) {
              const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
              if (ty && ty->attack > 1) {
                wscore += 1500;
              }
            }
          }
        }
        for (int slot = 0; slot < COLONIZE_CARGO_COUNT; ++slot) {
          if (slot == COLONIZE_CARGO_FOOD || slot == COLONIZE_CARGO_LUMBER ||
              slot == COLONIZE_CARGO_TRADE_GOODS) {
            continue;
          }
          int have = c->stock[slot];
          if (have < target) {
            if (slot == COLONIZE_CARGO_HORSES) {
              have += 25 - target;
              if (have < 0) {
                have = 0;
              }
            }
          } else {
            have <<= 1;
          }
          if (slot == COLONIZE_CARGO_TOOLS || slot == COLONIZE_CARGO_MUSKETS) {
            if (!(c->cargo_produced_mask & (1u << slot))) {
              continue; /* not produced this tick — DOS skips entirely */
            }
            have -= 100;
          }
          if (have >= 0) {
            const int price = nat ? (int)nat->trade.euro_price[slot] : 0;
            wscore += (long)price * have;
          }
        }
        if (haul_short) {
          wscore += (long)c->cargo_idle_turns * 8;
          if (wscore > 0x7fff) {
            wscore = 0x7fff;
          }
          uint8_t flag_a = 0xff;
          if (c->specialty_cargo != 0xff &&
              (int)c->specialty_cargo < COLONIZE_CARGO_COUNT) {
            flag_a = c->specialty_cargo;
          }
          ai_goals_upsert_work(c->id, (int)wscore, flag_a, /*haul=*/1);
        }
      }
      /*
       * Garrison-quota distribution (raw lines 904-980; was the last
       * unported own-colony piece). DOS: while colony+0x8e
       * (labor_shortage, "units wanted") > 0, register a LABOR goal at the
       * colony (prio = shortage − already-garrisoned + 2) and then admit
       * ('A') military units standing on the colony tile in strict
       * preference order — Artillery(0x0b), non-veteran Soldier(0x01),
       * veteran Soldier (profession 0x15), non-veteran Dragoon(0x04),
       * veteran Dragoon — decrementing +0x8e and +0x1e (garrison_quota)
       * per admission. Gated, like DOS's whole own-colony block, on the
       * colony being coastal (+0x1c bit 0x40; live map_tile_is_coastal
       * here rather than the thin-latched colony_flags bit).
       * The "already garrisoned" count is DOS `FUN_1000_8aac(unit,10)` —
       * really 0d38 case 0xa = # non-ship units with @UNIT combat > 1 in
       * the stack (2026-09-06b decode); the shipped "own fortified units
       * on the tile" substitution stays, rewire flagged as 0a60
       * follow-up in euro_goal_orders_0a60_full.md. Admitted units get
       * shadow order 'A', which excludes them from this turn's goal scan
       * (DOS-identical effect); deeper 'A' labor handling stays with the
       * existing colony-join paths.
       */
      if (c->labor_shortage > 0 && ctx->units &&
          map_tile_is_coastal(ctx->map, c->x, c->y)) {
        int garrisoned = 0;
        for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
          const ColonizeUnit* gu = units_get_const(ctx->units, ui);
          if (gu && gu->active && gu->nation_id == nation_id && gu->x == c->x &&
              gu->y == c->y &&
              (gu->orders == UNITS_ORDER_FORTIFY || gu->orders == UNITS_ORDER_FORTIFIED)) {
            garrisoned++;
          }
        }
        if (garrisoned < (int)c->labor_shortage) {
          ai_goals_upsert_primary(
            nation_id, c->x, c->y, AI_GOAL_LABOR,
            (int)c->labor_shortage - garrisoned + 2
          );
        }
        /* Five admission passes in DOS preference order. */
        static const int k_adm_type[5] = {0x0b, 0x01, 0x01, 0x04, 0x04};
        static const int k_adm_vet[5] = {-1, 0, 1, 0, 1}; /* -1 any; 0/1 vs prof 0x15 */
        for (int pass = 0; pass < 5 && c->labor_shortage > 0; ++pass) {
          for (int ui = 0; ui < COLONIZE_UNITS_MAX && c->labor_shortage > 0; ++ui) {
            const ColonizeUnit* gu = units_get_const(ctx->units, ui);
            if (!gu || !gu->active || gu->nation_id != nation_id || gu->x != c->x ||
                gu->y != c->y) {
              continue;
            }
            if (ai_euro_20e6_dos_type(ctx->units, gu) != k_adm_type[pass]) {
              continue;
            }
            const int is_vet = (gu->profession == 0x15);
            if (k_adm_vet[pass] >= 0 && is_vet != k_adm_vet[pass]) {
              continue;
            }
            Ai0a60UnitState* gst = &s_0a60_pilot_state[ui];
            if (gst->order_code == 'A') {
              continue; /* already admitted this turn */
            }
            gst->order_code = 'A';
            c->labor_shortage--;
            if (c->garrison_quota != 0) {
              c->garrison_quota--;
            }
          }
        }
      }
    }
  }

  /* E: foreign colonies MILITARY if at war; thin bind one idle Soldier/Dragoon.
   * CONTACT scout rings (peace + own≥1): idle Scout → ring MD 2–4 around tribe
   * (fog-aware when map.seen exists). Deep mid-mil scoring — PARKED. */
  if (ctx->colonies && ctx->col1_ok && ctx->col1) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id == nation_id || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      if (ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_MILITARY, 5);
      }
    }
    /* Thin E deepen: one idle Soldier/Dragoon → nearest foreign MILITARY. */
    if (ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
      ColonizeUnit* pick = NULL;
      int pick_gx = 0;
      int pick_gy = 0;
      int pick_d = -1;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
          continue;
        }
        if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
          continue;
        }
        if (units_orders_follow_goto(u->orders)) {
          continue; /* idle only */
        }
        if (!ai_euro_is_military_name(units_display_name(ctx->units, u))) {
          continue;
        }
        int gx = 0;
        int gy = 0;
        if (!ai_euro_nearest_military_goal(nation_id, u->x, u->y, &gx, &gy)) {
          continue;
        }
        const int d = abs(gx - u->x) + abs(gy - u->y);
        if (pick_d < 0 || d < pick_d) {
          pick = u;
          pick_gx = gx;
          pick_gy = gy;
          pick_d = d;
        }
      }
      if (pick) {
        ai_euro_set_goto(pick, UNITS_ORDER_AI_MOVE, pick_gx, pick_gy);
      }
    } else {
      /* Peaceful CONTACT scout rings (own colonies ≥ 1). */
      const int own =
        inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
      if (own >= 1) {
        /* Optional secondary FOUND near tribes — never on the village tile
         * (DOS asserts "Illegal entry into village" for euro squatters). */
        if (ctx->col1->tribe) {
          for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
            const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
            int fx = 0;
            int fy = 0;
            if (ai_euro_pick_founding_tile(
                  ctx->map,
                  ctx->colonies,
                  ctx->col1_ok ? ctx->col1 : NULL,
                  nation_id,
                  (int)t->x,
                  (int)t->y,
                  own,
                  &fx,
                  &fy)) {
              ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 1);
            }
          }
        }
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          ColonizeUnit* u = &ctx->units->units[i];
          if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
            continue;
          }
          if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
            continue;
          }
          if (units_orders_follow_goto(u->orders)) {
            continue; /* idle only */
          }
          const char* name = units_display_name(ctx->units, u);
          if (!name || strstr(name, "Scout") == NULL) {
            continue;
          }
          int tx = 0;
          int ty = 0;
          /* CONTACT ring when tribe available; else fog-explore MD≤8 (no CONTACT).
           * Seasoned Scout: deeper unseen fog pick (Colonization.pdf explore skill). */
          if (ai_euro_scout_contact_ring_target(ctx, nation_id, u->x, u->y, &tx, &ty)) {
            ai_goals_upsert_primary(nation_id, tx, ty, AI_GOAL_CONTACT, 2);
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
          } else if (ai_euro_scout_fog_explore_target(
                       ctx,
                       nation_id,
                       u->x,
                       u->y,
                       ai_euro_is_seasoned_scout_name(name),
                       &tx,
                       &ty)) {
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
          }
        }
      }
    }
  }

  /*
   * Food emergency (5cf6 food_short high): inventory food_short ≥ 4 → bind
   * nearest idle food-capable colonist/Pioneer to a hungry own colony LABOR
   * (MD≤8), even when not already adjacent. Cite: manual 2 food/colonist;
   * building_production food eat; no invented production rates.
   */
  if (inv && inv->food_short >= 4 && ctx->colonies && ctx->units) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (c->stock[COLONIZE_CARGO_FOOD] >= c->population * 2) {
        continue;
      }
      ColonizeUnit* pick = NULL;
      int pick_d = -1;
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &ctx->units->units[ui];
        if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
          continue;
        }
        if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
          continue;
        }
        if (ai_euro_land_is_fortified(u)) {
          continue;
        }
        /*
         * Don't yank a Pioneer off an in-progress tile improve job for
         * emergency food LABOR — exposed once the real DS:0x2f78 threshold
         * (2026-08-20 live capture) made these jobs usually take more than
         * one turn, so there's now a real window for this scan to hit a
         * unit mid-job.
         */
        if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
          continue;
        }
        if (!ai_euro_unit_is_food_labor(ctx->units, u)) {
          continue;
        }
        /* Skip if already on this colony tile (join happens in act). */
        const int dist = abs(u->x - c->x) + abs(u->y - c->y);
        if (dist > 8) {
          continue;
        }
        if (units_orders_follow_goto(u->orders) && u->goto_x == c->x &&
            u->goto_y == c->y) {
          pick = NULL;
          pick_d = -1;
          break; /* already LABOR-bound toward this colony */
        }
        if (pick_d < 0 || dist < pick_d) {
          pick = u;
          pick_d = dist;
        }
      }
      if (pick) {
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, 5);
        if (!units_orders_follow_goto(pick->orders) || pick->goto_x != c->x ||
            pick->goto_y != c->y) {
          ai_euro_set_goto(pick, UNITS_ORDER_AI_MOVE, c->x, c->y);
        }
        break; /* one emergency bind per planning pass */
      }
    }
  }

  /* F: tribe-adjacent FOUND prio 2; alarmed → MILITARY. Never FOUND on village. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      int fx = 0;
      int fy = 0;
      {
        const int own_f =
          inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
        if (ai_euro_pick_founding_tile(
              ctx->map,
              ctx->colonies,
              ctx->col1_ok ? ctx->col1 : NULL,
              nation_id,
              t->x,
              t->y,
              own_f,
              &fx,
              &fy)) {
          ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 2);
        }
      }
      if (t->alarm[nation_id].friction > 50) {
        /* Capital villages: higher MILITARY prio (Cortes rich_capital path).
         * Cite: col1 tribe.state.capital; fandom capital / Aztec treasure. */
        const int prio = t->state.capital ? 5 : 3;
        ai_goals_upsert_primary(nation_id, t->x, t->y, AI_GOAL_MILITARY, prio);
      }
    }
  }

  /* Foreign-colony + village producers (raw 983-1276): MILITARY approach,
   * CONTACT lurk ring, and the FOUND/MIL_EXPAND ship-staging goals at
   * open-sea tiles next to foreign colonies / villages. */
  ai_euro_0a60_settlement_goal_producers(ctx, nation_id);

  /*
   * G continent stance — mid-game pressure once established (≥2 colonies).
   * Refresh thin −0x6790 stance nibbles {0,3,4,6} from live tallies, then at war
   * MILITARY primary prio: own≥2 → 6, ≥3 → 7, ≥4 → 8; stance==3 soft-caps hunt
   * and bumps FOUND; stance==4 keeps mil ladder. Cite: euro_dispatcher.c G.
   */
  {
    ai_euro_refresh_continent_stance(ctx, nation_id);
    const int own =
      inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
    if (own >= 2 && ctx->colonies) {
      const int at_war =
        ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
      if (at_war) {
        /* Bump founding urgency stand-in + extra MILITARY on weakest/nearest foe. */
        if (inv) {
          inv->urgency += 2;
        }
        int ref_x = 0;
        int ref_y = 0;
        int have_ref = 0;
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          const ColonizeColony* c = &ctx->colonies->colonies[i];
          if (c->active && c->nation_id == nation_id) {
            ref_x = c->x;
            ref_y = c->y;
            have_ref = 1;
            break;
          }
        }
        const ColonizeColony* target = NULL;
        int best_key = -1;
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          const ColonizeColony* c = &ctx->colonies->colonies[i];
          if (!c->active || c->nation_id == nation_id || c->nation_id < 0 ||
              c->nation_id > 3) {
            continue;
          }
          if (!ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
            continue;
          }
          const int dist =
            have_ref ? (abs(c->x - ref_x) + abs(c->y - ref_y)) : 0;
          /* Prefer weaker (low pop), then nearer — pack into one key. */
          const int key = c->population * 10000 + dist;
          if (!target || key < best_key) {
            target = c;
            best_key = key;
          }
        }
        if (target) {
          /* Higher than E's foreign MILITARY (5); ladder own≥2/3/4 → 6/7/8. */
          int mil_prio = 6;
          if (own >= 4) {
            mil_prio = 8;
          } else if (own >= 3) {
            mil_prio = 7;
          }
          int under_cont = 0;
          if (ctx->map) {
            const int cid = map_continent_id_at(ctx->map, target->x, target->y);
            const int stance = ai_euro_continent_stance_at(nation_id, cid);
            /* stance 3 expand / bal under-target: soft-cap hunt, bump FOUND. */
            if (stance == 3) {
              under_cont = 1;
              if (mil_prio > 6) {
                mil_prio--;
              }
            }
          }
          ai_goals_upsert_primary(
            nation_id, target->x, target->y, AI_GOAL_MILITARY, mil_prio
          );
          if (under_cont) {
            int fx = 0;
            int fy = 0;
            if (ai_euro_pick_founding_tile(
                  ctx->map,
                  ctx->colonies,
                  ctx->col1,
                  nation_id,
                  target->x,
                  target->y,
                  own,
                  &fx,
                  &fy)) {
              ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 3);
            }
          }
        }
      } else {
        /* Peaceful: bump one primary FOUND +1, else idle Scout/Soldier → explore. */
        int bumped = 0;
        for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
          const AiGoalSlot* s = ai_goals_primary(nation_id, i);
          if (!s || s->code != AI_GOAL_FOUND) {
            continue;
          }
          ai_goals_upsert_primary(
            nation_id, s->x, s->y, AI_GOAL_FOUND, (int)s->prio + 1
          );
          bumped = 1;
          break;
        }
        if (!bumped) {
          int tx = 0;
          int ty = 0;
          int have_t = 0;
          /* Prefer tribe-adjacent FOUND (never the village tile itself). */
          if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
              ctx->col1->head.tribe_count > 0) {
            const ColonizeCol1Tribe* t0 = &ctx->col1->tribe[0];
            const int own_g =
              inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
            if (ai_euro_pick_founding_tile(
                  ctx->map,
                  ctx->colonies,
                  ctx->col1_ok ? ctx->col1 : NULL,
                  nation_id,
                  (int)t0->x,
                  (int)t0->y,
                  own_g,
                  &tx,
                  &ty)) {
              have_t = 1;
            }
          } else if (ai_goals_best_found_tile(nation_id, &tx, &ty)) {
            have_t = 1;
          }
          if (have_t) {
            for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
              ColonizeUnit* u = &ctx->units->units[i];
              if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
                continue;
              }
              if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
                continue;
              }
              if (units_orders_follow_goto(u->orders)) {
                continue;
              }
              const char* name = units_display_name(ctx->units, u);
              if (!name) {
                continue;
              }
              if (!strstr(name, "Scout") && !ai_euro_is_military_name(name)) {
                continue;
              }
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
              break;
            }
          }
        }
      }
    }
  }

  /* Ship FOUND: first colony via 06ae/0a60 landfall seed (adj 06ae from coastal
   * ship still prefers inland high 2f77). Second-wave while < 6 uses live 06ae
   * + coastal prefer. */
  {
    const int colonies = inv ? inv->colony_count : 0;
    if (colonies < 6) {
      const int found_prio = (colonies == 0) ? (6 + urgency / 2) : 4;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != nation_id) {
          continue;
        }
        if (!ai_euro_is_ship_type(ctx->units, u->id) || ai_euro_in_europe(u->x, u->y)) {
          continue;
        }
        int fx = 0;
        int fy = 0;
        int have = 0;
        if (colonies == 0) {
          int lx = 0;
          int ly = 0;
          if (ai_euro_recover_landfall_from_ship(u->x, u->y, &lx, &ly) &&
              ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lx, ly, &fx, &fy)) {
            have = 1;
          }
        } else if (ai_euro_pick_founding_tile(
                     ctx->map,
                     ctx->colonies,
                     ctx->col1_ok ? ctx->col1 : NULL,
                     nation_id,
                     u->x,
                     u->y,
                     colonies,
                     &fx,
                     &fy)) {
          have = 1;
        }
        if (have) {
          ai_goals_upsert_primary(nation_id, fx, fy, AI_GOAL_FOUND, found_prio);
        }
      }
    }
  }

  /* H: light bind — idle land founders → primary FOUND (do not steal Soldiers). */
  {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
        continue;
      }
      if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
        continue;
      }
      if (units_orders_follow_goto(u->orders)) {
        continue; /* idle only */
      }
      /*
       * Don't yank a Pioneer off an in-progress tile improve job for a
       * FOUND bind — same class of gap as the food-emergency scan above,
       * exposed once the real DS:0x2f78 threshold (2026-08-20 live
       * capture) made these jobs usually take more than one turn.
       */
      if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
        continue;
      }
      const char* name = units_display_name(ctx->units, u);
      if (!name || strstr(name, "Soldier")) {
        continue;
      }
      if (!strstr(name, "Pioneer") && !strstr(name, "Hardy") &&
          !strstr(name, "Free Colonist") && !strstr(name, "Colonist")) {
        continue;
      }
      /* Per-unit pick: a single table-wide FOUND bound every founder in the
       * nation to the same tile, however far away each one stood. */
      int fx = 0;
      int fy = 0;
      if (!ai_goals_best_found_tile_near(ctx->map, nation_id, u->x, u->y, &fx, &fy)) {
        continue;
      }
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy);
    }
  }
}

/* --- 20e6 scoring (land Manhattan + ocean/ship branch) ----------------- */

static int ai_euro_tile_under_enemy_fort_fire(
  ColonizeTurnContext* ctx,
  int viewer_nation,
  int x,
  int y
);
static int ai_euro_naval_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f
);
static int ai_euro_land_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f
);

static int ai_euro_ocean_score_step(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int goal_x,
  int goal_y,
  int* out_dx,
  int* out_dy
) {
  /*
   * Naval/ocean branch of FUN_521d_20e6 / LAB_521d_3558 (thin extract):
   * prefer water that reduces Manhattan + Chebyshev distance to goal; avoid land;
   * HS west/east bias; leave eastern HS into ocean when westbound (Atlantic
   * first leg). Full cargo/colony matrix in 3558 still OPEN.
   * Cite: move_scoring.md §ocean; euro_ocean_scoring.c; FUN_157e_004a.
   */
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  const int on_hs = map_tile_is_high_seas(ctx->map, u->x, u->y);
  const int west_explore = goal_x < u->x;
  const int east_europe = goal_x > u->x;
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, u->nation_id);
  const int own_tough = ai_euro_naval_foe_toughness(ctx, ctx->units, u);
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe >= 0) {
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id == u->nation_id) {
        continue;
      }
      if (f && !units_is_sea(ctx->units, foe)) {
        continue;
      }
    } else if (!map_tile_is_water(ctx->map, nx, ny)) {
      /* Allow coastal landfall tile if it is the goal. */
      if (!(nx == goal_x && ny == goal_y)) {
        continue;
      }
    }
    const int manh = abs(goal_x - nx) + abs(goal_y - ny);
    const int cheb_dx = abs(goal_x - nx);
    const int cheb_dy = abs(goal_y - ny);
    const int cheb = cheb_dx > cheb_dy ? cheb_dx : cheb_dy;
    int score = 2000 - manh * 12 - cheb * 4;
    const int step_hs = map_tile_is_high_seas(ctx->map, nx, ny);
    if (step_hs) {
      score += 5;
    }
    if (west_explore && dx[d] < 0) {
      score += 4; /* west bias toward New World */
    }
    if (on_hs && west_explore && step_hs && dx[d] < 0) {
      score += 6; /* HS west-explore: prefer westward HS tiles */
    }
    /* Leave eastern HS rim into ocean when sailing west (Atlantic first leg). */
    if (on_hs && west_explore && !step_hs) {
      score += 14;
    }
    if (east_europe && dx[d] > 0) {
      score += 4; /* east bias toward Europe / eastern HS */
    }
    if (on_hs && east_europe && step_hs && dx[d] > 0) {
      score += 6; /* HS east-Europe: prefer eastward HS tiles */
    }
    /* Avoid enemy Fort/Fortress batteries (FUN_364b_03f6). */
    if (ai_euro_tile_under_enemy_fort_fire(ctx, u->nation_id, nx, ny)) {
      score -= 800;
    }
    /* Thin combat: prefer closing on weaker adjacent foe ships. */
    if (at_war) {
      for (int ad = 0; ad < 8; ++ad) {
        const int ax = nx + dx[ad];
        const int ay = ny + dy[ad];
        const int fid = units_id_at(ctx->units, ax, ay);
        if (fid < 0 || !units_is_sea(ctx->units, fid)) {
          continue;
        }
        const ColonizeUnit* f = units_get_const(ctx->units, fid);
        if (!f || f->nation_id == u->nation_id || f->nation_id < 0 || f->nation_id > 3) {
          continue;
        }
        if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
          continue;
        }
        const int ft = ai_euro_naval_foe_toughness(ctx, ctx->units, f);
        if (ft < own_tough) {
          score += 18;
        } else if (ft > own_tough) {
          score -= 8;
        }
      }
    }
    /* Empty-hold coastal cling (3558/457e thin): prefer coast water near goal. */
    if (u->cargo_count == 0 && ai_euro_tile_is_coast_water(ctx->map, nx, ny)) {
      score += 8;
    }
    if (ctx->rng) {
      score += dos_rng_range(ctx->rng, 0, 2);
    }
    if (score > best) {
      best = score;
      bdx = dx[d];
      bdy = dy[d];
    }
  }
  if (best < -999990) {
    return 0;
  }
  *out_dx = bdx;
  *out_dy = bdy;
  return 1;
}

/* Native village on this tile (col1 tribe table), else -1. */
static int ai_euro_village_nation_at(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return -1;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
      return (int)t->nation_id;
    }
  }
  return -1;
}

/* @UNIT attack 0 (Pioneers, Colonists, Wagon Train, unarmed transports). */
static int ai_euro_unit_cannot_attack(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  const ColonizeUnitType* t = u ? units_type(pool, u->type_index) : NULL;
  return t && t->attack <= 0;
}

static int ai_euro_score_move(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int goal_x,
  int goal_y,
  int* out_dx,
  int* out_dy
) {
  if (!ctx || !ctx->map || !u || !out_dx || !out_dy) {
    return 0;
  }
  if (units_is_sea(ctx->units, u->id)) {
    return ai_euro_ocean_score_step(ctx, u, goal_x, goal_y, out_dx, out_dy);
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, u->nation_id);
  const int own_tough = at_war ? ai_euro_land_foe_toughness(ctx, ctx->units, u) : 0;
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  int bd = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!units_can_enter(ctx->units, u->type_index, ctx->map, nx, ny, u->id, ctx->colonies)) {
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (!f || f->nation_id == u->nation_id || units_is_sea(ctx->units, foe)) {
        continue;
      }
      /* Only a tile a fight could take. A settler treated a blocked tile as an
       * attack candidate and walked into it. */
      if (ai_euro_unit_cannot_attack(ctx->units, u)) {
        continue;
      }
    }
    /*
     * Entering a native village is an attack on it, resolved inside
     * units_try_move against a defender spawned from the dwelling, with no
     * unit visible on the tile beforehand. Movement scoring must never route
     * through one: settlers died on the way to their colony site, and a
     * peacetime Soldier that wandered in lost and took its whole stack with it
     * (units_sweep_stack_after_loss). Deliberate raids go through the war hunt
     * arms, which pick their target explicitly.
     */
    if (ctx->col1_ok && ctx->col1 && ai_euro_village_nation_at(ctx->col1, nx, ny) >= 4) {
      continue;
    }
    const int dist = abs(goal_x - nx) + abs(goal_y - ny);
    int score = 1000 - dist * 10;
    /*
     * Explore ring (LAB_521d_2912→2a59 thin): continent match, FoW unseen
     * nibble, skip LCR (rumour / terr class 0x1b) for non-Scouts; Scouts prefer
     * rumour tiles. Cite: move_scoring_land.md §explore ring.
     */
    {
      const int unit_cid = map_continent_id_at(ctx->map, u->x, u->y);
      const int step_cid = map_continent_id_at(ctx->map, nx, ny);
      if (unit_cid > 0 && step_cid == unit_cid) {
        score += 4;
      }
      const int unseen =
        ctx->map->seen && !map_tile_seen_by(ctx->map, nx, ny, u->nation_id) ? 1 : 0;
      if (unseen) {
        score += 6;
      }
      const int rum = map_tile_has_rumour(ctx->map, nx, ny);
      const int lcr_class = map_dos_terr_class_at(ctx->map, nx, ny) == 0x1b;
      const char* nm = units_display_name(ctx->units, u);
      const int is_scout = nm && strstr(nm, "Scout") != NULL;
      if (is_scout && rum) {
        score += 12;
      } else if (!is_scout && (rum || lcr_class)) {
        score -= 20; /* non-scout skip LCR / rumour */
      }
    }
    /*
     * Land combat 20e6 (structured deepen): prefer closing on weaker adjacent war
     * foes; prefer foreign Euro settlement tiles (orders 0x46 settlement scan);
     * bonus for stepping onto a contested foreign colony tile.
     * Cite: move_scoring_land.md LAB_521d_5183 / 0x46; unpark #4.
     */
    if (at_war) {
      /* Destination is a foreign Euro colony → siege approach (0x46). */
      if (ctx->colonies) {
        const int cid = colonies_id_at(ctx->colonies, nx, ny);
        if (cid >= 0) {
          const ColonizeColony* c = colonies_get(ctx->colonies, cid);
          if (c && c->active && c->nation_id >= 0 && c->nation_id <= 3 &&
              c->nation_id != u->nation_id &&
              ai_diplo_at_war(ctx->col1, u->nation_id, c->nation_id)) {
            score += 16;
            if (ai_euro_is_artillery_name(units_display_name(ctx->units, u)) &&
                colonies_has_fortification(ctx->colonies, c)) {
              score += 10; /* Artillery siege prefer fortified port */
            }
          }
        }
      }
      for (int ad = 0; ad < 8; ++ad) {
        const int ax = nx + dx[ad];
        const int ay = ny + dy[ad];
        const int fid = units_id_at(ctx->units, ax, ay);
        if (fid < 0 || units_is_sea(ctx->units, fid)) {
          continue;
        }
        const ColonizeUnit* f = units_get_const(ctx->units, fid);
        if (!f || f->nation_id == u->nation_id) {
          continue;
        }
        if (f->nation_id >= 0 && f->nation_id <= 3 &&
            !ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
          continue;
        }
        if (f->nation_id >= 4 && f->nation_id <= 11 &&
            !ai_diplo_indian_at_war(ctx->col1, u->nation_id, f->nation_id - 4)) {
          continue;
        }
        const int ft = ai_euro_land_foe_toughness(ctx, ctx->units, f);
        if (ft < own_tough) {
          score += 14;
        } else if (ft > own_tough) {
          score -= 6;
        }
        if (f->nation_id >= 0 && f->nation_id <= 3 && ctx->colonies &&
            colonies_id_at(ctx->colonies, f->x, f->y) >= 0) {
          score += 8; /* settlement-adjacent foe (0x46-shaped) */
        }
      }
    }
    /*
     * Facing/momentum bias (LAB_521d_54f5, unit+0x314f): same-as-last-move
     * direction preferred, exact opposite (d^4) penalized, adjacent (diff
     * 1) mildly preferred — identical shape to the already-ported Brave
     * `quiet_score_facing`.
     */
    {
      const int last_dir = s_euro_last_dir[u->id];
      if (d == last_dir) {
        score += 4;
      } else if (d == (last_dir ^ 4)) {
        score -= 6;
      } else {
        int diff = d - last_dir;
        if (diff < 0) {
          diff = -diff;
        }
        if (diff > 4) {
          diff = 8 - diff;
        }
        if (diff == 1) {
          score += 3;
        }
      }
    }
    if (ctx->rng) {
      score += dos_rng_range(ctx->rng, 0, 3);
    }
    if (score > best) {
      best = score;
      bdx = dx[d];
      bdy = dy[d];
      bd = d;
    }
  }
  if (best < -999990) {
    return 0;
  }
  s_euro_last_dir[u->id] = (int8_t)bd;
  *out_dx = bdx;
  *out_dy = bdy;
  return 1;
}

/*
 * ======================================================================
 * FUN_521d_20e6 — structural land port (2026-08-27)
 * ======================================================================
 * Transcribed from the clean 2215-line recovery in
 * original_sources_annotated/ai/move_scoring_20e6_full.md ("Raw recovered
 * C"). This block covers the Euro LAND path of the function, arm by arm:
 *
 *   prologue            → ai_euro_20e6_prologue      (raw lines ~1006-1090)
 *   explorer flag       → ai_euro_20e6_explorer_flag (iStack_6a, ~1095-1180)
 *   SCOUT/PATROL 0x56   → ai_euro_20e6_patrol_arm    (LAB_277a, ~1260-1275)
 *   explore ring        → ai_euro_land_explore_scan_target (LAB_2912→2a59,
 *                         ~1320-1480; replaces the 2026-08-15 thin scan)
 *   8-dir wander score  → ai_euro_20e6_wander_step   (LAB_4d2e→5183,
 *                         ~1940-2180)
 *   epilogue commit     → ai_euro_move_scoring_gate  (LAB_589e/5a78)
 *
 * 2026-09-06 deepening pass (the six thin pieces from port_plan.md's 20e6
 * row): LAB_52aa attack-odds tail (crown==2 halving + Soldier/Dragoon
 * colony mass gate, ai_euro_20e6_attack_term), 0x4c village arms
 * (ai_euro_20e6_village_arm → ai_contact AI wrappers), colonist labor loop
 * (ai_euro_20e6_labor_arm), LAB_3558 per-cargo unload mask
 * (ai_euro_20e6_unload_mask / _unload_by_mask in ai_euro_unload_settle),
 * −0x6168 rival strength (persistent 0a60 max-tracker + explore
 * fatigue → local_12), explore-plane low nibble (ai_euro_20e6_site_nibble
 * reads the real seen-plane site-score nibble).
 *
 * NOT here (own Linux mechanics already cover them, or closed as dead in
 * port_plan.md T1.2/T1.3): 0x42/0x65 found/contact writes; the LAB_3558
 * colony-sail matrix / HS spiral / work-queue haul tails beyond the unload
 * rule.
 *
 * DOS state this port models file-locally (same pattern as s_euro_last_dir
 * for unit+0x314f):
 *   DS:0xa13c (−0x5ec4) per-continent explorer count → s_20e6_explorers
 * (unit+0x3155/+0x3156, the explorer's 4-tile ring-hop wander latch, sits in
 * the ship-band tail LAB_4b2c and is not reached by the land path ported
 * here — not modelled.)
 *     (memset 0 at FUN_521d_0a60 entry every nation turn; mirrored in
 *     ai_euro_dispatcher_turn)
 *
 * Deliberate substitutions (each marked at its use site):
 *   - DS:0x9faa coarse fog plane (far-probe +8): Linux keeps that plane only
 *     for tribe placement, so the per-nation seen[] plane is used instead.
 *   - explore-plane low nibble (FUN_1000_893a & 0xf): now the real seen-plane
 *     site-score nibble on DOS-imported maps (ai_euro_20e6_site_nibble);
 *     Linux-generated maps carry no nibble, old unseen→4 stand-in kept there.
 *   - −0x6168[continent] rival-strength: live (s_euro_rival_strength,
 *     the 0a60 max-tracker recomputed per call) + s_20e6_explore_fatigue for
 *     unit+0x3154; radius shrink and >40 halving now fire.
 *   - FUN_1000_8aac: resolved as the FUN_1427_0d38 stack-query dispatcher;
 *     ALL its case bodies byte-decoded 2026-09-06 (table at
 *     ai_euro_20e6_stack_count) — case 2 = total stack count (the old
 *     "# military types" reading was case 4), case 0xb = stack combat sum;
 *     the 0x42/0x65 gates (case 2 < 2 = "unit is alone") stay closed per
 *     T1.2 — Linux's goal-driven found/contact impulses cover them.
 */

#define AI_20E6_TYPE_COUNT 23

/* DS:0x5233 unit-type record +3 (0x5236 "combat", NAMES @UNIT column 4). */
static const uint8_t k_20e6_type_combat[AI_20E6_TYPE_COUNT] = {
  1, 2, 1, 1, 3, 1, 5, 5, 6, 4, 0, 5, 1, 2, 6, 10, 8, 16, 24, 1, 2, 2, 3
};
/*
 * DS:0x523d unit-type capability flags = NAMES.TXT @UNIT trailing bit-string
 * read MSB-first (Brave "00111000" → 0x38, the value quiet_brave_scoring.c
 * independently cites for type 19 — that match is the confirmation).
 */
static const uint8_t k_20e6_type_flags[AI_20E6_TYPE_COUNT] = {
  0x40, 0x1c, 0x40, 0x20, 0x3c, 0x64, 0x1c, 0x1c, 0x1c, 0x1c, 0x00, 0x18,
  0x00, 0xa2, 0x82, 0x82, 0x01, 0x81, 0x81, 0x38, 0x38, 0x38, 0x38
};
/*
 * DS:0x2f79 terrain record +3 (terrain_yields.md "DS:0x2f76 terrain-class
 * record", decoded 2026-08-21 from 20 dump instances) — colony-site
 * desirability per neighbour tile, added by the explorer far-probe ring.
 */
static const uint8_t k_20e6_terr_site_byte[32] = {
  2, 2, 4, 4, 4, 4, 2, 2, 3, 1, 3, 3, 3, 3, 1, 1, 3, 1, 3, 3, 3, 3, 1, 1, 0, 3, 0, 2, 2, 0, 0, 0
};
/*
 * DS:0xc8 / 0xde — 20-entry radius-2 ring (FUN_15eb_04c0 walks it as the
 * fort-scaled colony work radius: 5×5 minus centre minus 4 corners). First
 * 8 entries are the dir8 table (DS:0xb4/0xbe); the outer 12 are in
 * clockwise-from-north order. Only ever consumed via a uniform RNG slot or
 * a full-ring sum here, so intra-ring order does not affect results.
 */
static const int8_t k_20e6_ring20_dx[20] = {0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 1, 0, -1, -2, -2, -2, -1};
static const int8_t k_20e6_ring20_dy[20] = {-1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -1, 0, 1, 2, 2, 2, 1, 0, -1, -2};

static uint8_t s_20e6_explorers[16];
/*
 * DOS unit+0x3154, the land-explorer branch (raw ~1600-1607): a per-unit
 * explore-fatigue counter, ++ (cap 0x7f) each explore-ring pass; the ring-hop
 * arm decrements it by 8. In DOS the same byte doubles as cargo_hold[0]
 * storage; land explorers never carry cargo, so a session-local array is the
 * honest home (not save-persisted — documented divergence).
 */
static uint8_t s_20e6_explore_fatigue[COLONIZE_UNITS_MAX];
/*
 * DOS unit+0x3155 / +0x3156 — the explorer's 4-tile ring-hop wander latch
 * (raw 1600-1611 countdown / 2416-2458 hop pick; ported 2026-09-06).
 * +0x3156 latches a random ring20 slot (0xff = unset, re-rolled rng(1,0x14)−1
 * when needed); +0x3155 counts down the committed hop (max(dx,dy)*4, signed
 * char semantics kept). Like +0x3154 these bytes are cargo_hold storage in
 * DOS and land explorers never carry cargo, so session-local arrays are the
 * honest home (not save-persisted — documented divergence).
 * s_20e6_hop_slot stores slot+1 so the zero-initialised state reads "unset".
 */
static int8_t s_20e6_hop_steps[COLONIZE_UNITS_MAX];
static int16_t s_20e6_hop_slot[COLONIZE_UNITS_MAX];

/* DOS unit+0x3146 type index (NAMES.TXT @UNIT order) from a Linux unit. */
/* NAMES.TXT @UNIT column 9 (DS:0x5239): 0 for land types; ships Caravel 0,
 * Merchantman 1, Galleon 4, Privateer 4, Frigate 12, Man-O-War 32. */
static int ai_euro_20e6_unit_col9(int dos_type) {
  switch (dos_type) {
    case 0xe: return 1;
    case 0xf: return 4;
    case 0x10: return 4;
    case 0x11: return 12;
    case 0x12: return 32;
    default: return 0;
  }
}

static int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const ColonizeUnitType* t = units ? units_type(units, u->type_index) : NULL;
  if (!t) {
    return -1;
  }
  static const struct {
    const char* key;
    int dos;
  } k[] = {
    {"Cont. Cav", 7},    {"Cont. Army", 9},  {"Cavalry", 8},      {"Regular", 6},
    {"Colonist", 0},     {"Soldier", 1},     {"Pioneer", 2},      {"Missionar", 3},
    {"Dragoon", 4},      {"Scout", 5},       {"Treasure", 0xa},   {"Artillery", 0xb},
    {"Wagon", 0xc},      {"Caravel", 0xd},   {"Merchantman", 0xe}, {"Galleon", 0xf},
    {"Privateer", 0x10}, {"Frigate", 0x11},  {"Man-O-War", 0x12}, {"Mtd. Warrior", 0x16},
    {"Mtd. Brave", 0x15}, {"Armed Brave", 0x14}, {"Brave", 0x13},
  };
  for (size_t i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
    if (strstr(t->name, k[i].key)) {
      return k[i].dos;
    }
  }
  if (units->type_count >= 19 && u->type_index >= 0 && u->type_index < AI_20E6_TYPE_COUNT) {
    return u->type_index; /* NAMES-loaded pool: index == DOS index */
  }
  return -1;
}

static int ai_euro_20e6_type_combat(int dos_type) {
  return (dos_type >= 0 && dos_type < AI_20E6_TYPE_COUNT) ? (int)k_20e6_type_combat[dos_type] : 0;
}

static int ai_euro_20e6_type_flags(int dos_type) {
  return (dos_type >= 0 && dos_type < AI_20E6_TYPE_COUNT) ? (int)k_20e6_type_flags[dos_type] : 0;
}

/* FUN_1000_88cc / FUN_137f_0200 owner_nibble: layer3 high nibble, 0xf → −1. */
static int ai_euro_20e6_owner_nibble(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return -1;
  }
  const int n = (int)(map_get_layer3(map, x, y) >> 4) & 0xf;
  return n == 0xf ? -1 : n;
}

/* FUN_1000_8c28 / FUN_15b3_0004 diplomacy byte, Euro or Indian counterpart. */
static int ai_euro_20e6_diplo(const ColonizeCol1Save* col1, int nation, int other) {
  if (!col1 || nation < 0 || nation > 3 || other < 0) {
    return 0;
  }
  if (other < 4) {
    return (int)ai_diplo_read(col1, nation, other);
  }
  if (other > 11) {
    return 0;
  }
  /* Indian side: WAR bit synthesised from the thin relation gate (no raw byte
   * accessor exposes the DOS Indian×Euro flag byte here). MET assumed. */
  int f = AI_DIPLO_MET;
  if (ai_diplo_indian_at_war(col1, nation, other - 4)) {
    f |= AI_DIPLO_WAR;
  }
  return f;
}

/* FUN_1000_856a / FUN_124c_007c distance between two tiles. */
static int ai_euro_20e6_dist(int ax, int ay, int bx, int by) {
  return ai_euro_dos_dist(ax - bx, ay - by);
}

/* FUN_1000_8804 / FUN_15eb_0142 nearest colony; nation<0 = any, cid<0 = any. */
static int ai_euro_20e6_nearest_colony(
  const ColonizeTurnContext* ctx, int x, int y, int nation, int cid, int* out_dist
) {
  int best = -1;
  int bd = 9999;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      if (nation >= 0 && c->nation_id != nation) {
        continue;
      }
      if (cid >= 0 && map_continent_id_at(ctx->map, c->x, c->y) != cid) {
        continue;
      }
      const int d = ai_euro_20e6_dist(x, y, c->x, c->y);
      if (d < bd) {
        bd = d;
        best = i;
      }
    }
  }
  if (out_dist) {
    *out_dist = bd;
  }
  return best;
}

/* FUN_1000_8f74 / FUN_4cc6_0356 nearest village to (x,y). */
static int ai_euro_20e6_nearest_village(const ColonizeTurnContext* ctx, int x, int y, int* out_dist) {
  int best = -1;
  int bd = 9999;
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if (t->nation_id < 4 || t->nation_id > 11 || t->x >= 200 || t->y >= 200) {
        continue;
      }
      const int d = ai_euro_20e6_dist(x, y, (int)t->x, (int)t->y);
      if (d < bd) {
        bd = d;
        best = (int)ti;
      }
    }
  }
  if (out_dist) {
    *out_dist = bd;
  }
  return best;
}

/* FUN_1000_88e0 / FUN_137f_0392 Indian settlement owner at tile, else −1. */
static int ai_euro_20e6_village_owner_at(const ColonizeTurnContext* ctx, int x, int y) {
  if (!ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe) {
    return -1;
  }
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
      return (int)t->nation_id;
    }
  }
  return -1;
}

/* FUN_1000_8886 / FUN_137f_0358 Euro settlement owner at tile, else −1. */
static int ai_euro_20e6_colony_owner_at(const ColonizeTurnContext* ctx, int x, int y) {
  if (!ctx->colonies) {
    return -1;
  }
  const int cid = colonies_id_at(ctx->colonies, x, y);
  if (cid < 0) {
    return -1;
  }
  const ColonizeColony* c = colonies_get(ctx->colonies, cid);
  return (c && c->active) ? c->nation_id : -1;
}

/*
 * FUN_1000_88c2 / FUN_137f_0428 tile_tribe_or_presence: settlement owner
 * (Euro or Indian) if one sits here, else the occupying unit's owner, else −1.
 */
static int ai_euro_20e6_tribe_or_presence(const ColonizeTurnContext* ctx, int x, int y) {
  int o = ai_euro_20e6_colony_owner_at(ctx, x, y);
  if (o >= 0) {
    return o;
  }
  o = ai_euro_20e6_village_owner_at(ctx, x, y);
  if (o >= 0) {
    return o;
  }
  const int uid = units_id_at(ctx->units, x, y);
  if (uid >= 0) {
    const ColonizeUnit* ou = units_get_const(ctx->units, uid);
    return ou ? ou->nation_id : -1;
  }
  return -1;
}


typedef struct Ai20e6Unit {
  int nation;
  int x;
  int y;
  int dos_type;   /* unit+0x3146 */
  int flags;      /* DS:0x523d row */
  int combat;     /* DS:0x5236 row */
  int is_ship;    /* iStack_34: type ∈ [0xd,0x12] */
  int cid;        /* iStack_38: continent at unit tile (−1 water) */
  int home_colony; /* uStack_62: nearest own colony id, −1 none */
  int home_dist;  /* iStack_2e: DS:0x8db8 after that search */
  int home_cid;   /* iStack_2c: −2 when no own colony */
  int any_colony_dist; /* iStack_74: nearest colony of any nation */
  int stance;     /* uStack_2a: G-table DS:0x9870[nation][cid], 5 off-land */
  int village_idx; /* uStack_ac */
  int village_dist; /* iStack_a0 */
  int unit_river; /* uStack_84 */
  int unit_road;  /* uStack_5a */
  int act_state;  /* unit+0x314c (s_0a60_pilot_state shadow) */
  int order_code; /* unit+0x314b */
  int explorer;   /* iStack_6a */
  int turn;
  int year;
  int woi;
} Ai20e6Unit;

/* Raw lines ~1006-1090: the locals every later arm reads. */
static void ai_euro_20e6_prologue(ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation, Ai20e6Unit* s) {
  memset(s, 0, sizeof(*s));
  s->nation = nation;
  s->x = u->x;
  s->y = u->y;
  s->dos_type = ai_euro_20e6_dos_type(ctx->units, u);
  s->flags = ai_euro_20e6_type_flags(s->dos_type);
  s->combat = ai_euro_20e6_type_combat(s->dos_type);
  s->is_ship = (s->dos_type >= 0xd && s->dos_type <= 0x12) || units_is_sea(ctx->units, u->id);
  s->cid = map_tile_is_land(ctx->map, u->x, u->y) ? map_continent_id_at(ctx->map, u->x, u->y) : -1;
  s->home_colony = ai_euro_20e6_nearest_colony(ctx, u->x, u->y, nation, -1, &s->home_dist);
  if (s->home_colony < 0) {
    s->home_cid = -2;
    s->home_dist = 0; /* DS:0x8db8 untouched by a miss; DOS leaves the prior value — 0 is the
                         common case (unit freshly landed, nothing bound yet) */
  } else {
    const ColonizeColony* hc = colonies_get(ctx->colonies, s->home_colony);
    s->home_cid = hc ? map_continent_id_at(ctx->map, hc->x, hc->y) : -2;
  }
  (void)ai_euro_20e6_nearest_colony(ctx, u->x, u->y, -1, -1, &s->any_colony_dist);
  if (s->any_colony_dist == 9999) {
    s->any_colony_dist = 0;
  }
  s->stance = (s->cid < 0) ? 5 : ai_euro_continent_stance_at(nation, s->cid);
  s->village_idx = ai_euro_20e6_nearest_village(ctx, u->x, u->y, &s->village_dist);
  s->unit_river = map_tile_has_river(ctx->map, u->x, u->y) ? 1 : 0;
  s->unit_road = map_tile_has_road(ctx->map, u->x, u->y) ? 1 : 0;
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    s->act_state = s_0a60_pilot_state[u->id].act_state;
    s->order_code = s_0a60_pilot_state[u->id].order_code;
  }
  s->turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  s->year = (ctx->game_year && *ctx->game_year) ? (int)*ctx->game_year : 1492;
  s->woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;
}


/* Per-nation, per-continent DOS −0x6b1a / −0x6a8e / −0x6a0e reads. */
static int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  int n = 0;
  if (ctx->colonies && cid >= 0) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == nation && map_continent_id_at(ctx->map, c->x, c->y) == cid) {
        n++;
      }
    }
  }
  return n;
}

static int ai_euro_20e6_combat_value_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  int sum = 0;
  if (cid < 0) {
    return 0;
  }
  ColonizeCombatStrengthCtx sctx;
  sctx.units = ctx->units;
  sctx.map = ctx->map;
  sctx.colonies = ctx->colonies;
  sctx.col1 = ctx->col1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = units_get_const(ctx->units, i);
    if (!o || !o->active || o->nation_id != nation || units_is_sea(ctx->units, i)) {
      continue;
    }
    if (map_continent_id_at(ctx->map, o->x, o->y) != cid) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, i, 1, NULL);
    if (sum > 255) {
      return 255; /* DOS byte table */
    }
  }
  return sum;
}

/* DS:0x95f2 continent_presence_flags bit 0x04: a foreign colony sits on cid. */
static int ai_euro_10ec_land_units_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = units_get_const(ctx->units, i);
    if (!o || !o->active || o->nation_id != nation || units_is_sea(ctx->units, i)) {
      continue;
    }
    if (map_continent_id_at(ctx->map, o->x, o->y) == cid) {
      n++;
    }
  }
  return n > 255 ? 255 : n; /* −0x6b5a byte table */
}

int ai_euro_10ec_war_worthy(const ColonizeTurnContext* ctx, int a, int b) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->units || !ctx->map || !ctx->turn_number ||
      a < 0 || a > 3 || b < 0 || b > 3 || a == b) {
    return 0;
  }
  const ColonizeCol1Save* col1 = ctx->col1;
  const int focus = ctx->human_nation; /* DS:0x5398 */
  /* asm 5bfb:10f4: `CMP [0xa153],AL(=0x5398); JNZ -> return 0` — the AI only
   * weighs war/treaty-cancel while the human is the top-ranked nation
   * (FUN_5bfb_00f8 rank table). Skipped by the first pass, wired 2026-08-27. */
  if (ai_diplo_00f8_top_ranked_nation(col1) != focus) {
    return 0;
  }
  if ((int)*ctx->turn_number <= 0x27) {
    return 0;
  }
  if (col1->stuff.colony_pop_totals[a] <= 7 && col1->stuff.colony_pop_totals[b] <= 7) {
    return 0; /* −0x6bf4 */
  }
  if ((col1->nation[a].nation_flags & 0x04) || (col1->nation[b].nation_flags & 0x04)) {
    return 0; /* independent */
  }
  if (focus >= 0 && focus < 4) {
    /* For each of a/b: met-but-unpeaced with the focus nation only passes when the
     * focus nation is no stronger (−0x6be4 word, −0x6bf0 byte). DOS compares
     * b's word but a's −0x6bf0 byte in the second clause (verbatim). */
    const int fa = col1->nation[a].euro_relation[focus] & (AI_DIPLO_PEACE | AI_DIPLO_MET);
    if (fa == AI_DIPLO_MET &&
        !(col1->stuff.land_combat_strength[focus] <= col1->stuff.land_combat_strength[a] &&
          col1->stuff.census_pop_proxy[focus] <= col1->stuff.census_pop_proxy[a])) {
      return 0;
    }
    const int fb = col1->nation[b].euro_relation[focus] & (AI_DIPLO_PEACE | AI_DIPLO_MET);
    if (fb == AI_DIPLO_MET &&
        !(col1->stuff.land_combat_strength[focus] <= col1->stuff.land_combat_strength[b] &&
          col1->stuff.census_pop_proxy[focus] <= col1->stuff.census_pop_proxy[a])) {
      return 0;
    }
  }
  int rivals = 0;
  for (int n = 0; n < 4; ++n) {
    if (n != a && n != b &&
        (col1->nation[a].euro_relation[n] & (AI_DIPLO_PEACE | AI_DIPLO_MET)) == AI_DIPLO_MET) {
      rivals++;
    }
  }
  if ((col1->nation[a].euro_relation[b] & (AI_DIPLO_PEACE | AI_DIPLO_MET)) != AI_DIPLO_MET) {
    rivals++;
  }
  int local_4 = 0; /* any shared continent */
  int local_8 = 0; /* a's defense value where both present */
  int local_6 = 1; /* b's (units + defense)/2 + 1 */
  for (int cid = 1; cid < 0xf; ++cid) {
    const int a_col = ai_euro_20e6_own_colonies_on(ctx, a, cid);
    const int a_units = ai_euro_10ec_land_units_on(ctx, a, cid);
    const int a_def = ai_euro_20e6_combat_value_on(ctx, a, cid);
    const int b_def = ai_euro_20e6_combat_value_on(ctx, b, cid);
    if (a_col != 0 && (uint8_t)((a_units >> 1) + (a_def >> 1)) < b_def) {
      return 0; /* a is dwarfed where it has colonies */
    }
    const int b_units = ai_euro_10ec_land_units_on(ctx, b, cid);
    if (a_def != 0 && b_units != 0) {
      local_8 += a_def;
      local_6 += (b_units + b_def) >> 1;
      local_4 = 1;
    }
  }
  const int unknown34 = 0; /* −0x6a9a[a*3] — Linux unknown34_pad, unresolved */
  if ((rivals - unknown34) + 4 <= (local_8 << 2) / local_6 || local_4 == 0) {
    return 1;
  }
  return 0;
}

static int ai_euro_20e6_foreign_colony_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  if (!ctx->colonies || cid < 0) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (c->active && c->nation_id != nation && map_continent_id_at(ctx->map, c->x, c->y) == cid) {
      return 1;
    }
  }
  return 0;
}

/*
 * DOS local_12 = −0x6168[cid]*8 + unit+0x3154 (explore fatigue). The
 * −0x6168 table is the 0a60 per-nation-turn max-tracker (largest foreign
 * colony pop vs capped ≤4 rival land units — written for real in
 * ai_euro_refresh_continent_stance, read back here at DOS position).
 */
static int ai_euro_20e6_local12(const ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation, int cid) {
  (void)ctx;
  const int fat = (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) ? (int)s_20e6_explore_fatigue[u->id] : 0;
  return ai_euro_rival_strength_at(nation, cid) * 8 + fat;
}

/*
 * DS:0x9650 — per-nation stats pass (decomp ~78316): number of continents
 * with continent_tally_a > 7 the nation has no colonies on ("open frontier
 * count"). Recomputed per call.
 */
static int ai_euro_20e6_open_continents(const ColonizeTurnContext* ctx, int nation) {
  if (!ctx->col1_ok || !ctx->col1) {
    return 0;
  }
  int n = 0;
  for (int cid = 0; cid < 16; ++cid) {
    if ((int)ctx->col1->post_map.continent_tally_a[cid] > 7 &&
        ai_euro_20e6_own_colonies_on(ctx, nation, cid) == 0) {
      n++;
    }
  }
  return n;
}

/*
 * FUN_281f_074a & 0xf — the seen-plane low nibble = map-gen colony-site AI
 * score (save_format_map.md "seen" row; nawagers). DOS-imported maps carry it;
 * Linux map_gen writes no nibble, so when the whole plane carries none the
 * old per-nation unseen→4 stand-in stays (an all-zero nibble field would
 * otherwise disable the explore ring on generated maps).
 */
static int ai_euro_20e6_site_nibble(const ColonizeTurnContext* ctx, int x, int y, int nation) {
  const ColonizeWorldMap* map = ctx->map;
  if (!map || !map->seen || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  static const uint8_t* s_nib_plane = NULL;
  static int s_nib_count = -1;
  static int s_nib_present = 0;
  const int count = map->width * map->height;
  if (map->seen != s_nib_plane || count != s_nib_count) {
    s_nib_plane = map->seen;
    s_nib_count = count;
    s_nib_present = 0;
    for (int i = 0; i < count; ++i) {
      if (map->seen[i] & 0x0f) {
        s_nib_present = 1;
        break;
      }
    }
  }
  if (s_nib_present) {
    return (int)(map->seen[y * map->width + x] & 0x0f);
  }
  return !map_tile_seen_by(map, x, y, nation) ? 4 : 0;
}

/*
 * FUN_1000_8aac = FUN_281f_08bc → FUN_1427_0d38 stack-query dispatcher.
 * Full case table re-decoded 2026-09-06 byte-exact from the CS:0xd78 jump
 * table (segment 1427 file base 0xF670 in viceroy_unpacked_2, entries
 * 0d96/0ef8/0db9/0db0/0dbe/0de0/0de6/0ef8×3/0e16/0e46/0e8c/0e94/0ebc —
 * see move_scoring_20e6_full.md "2026-09-06b"). All cases walk the unit's
 * tile stack (FUN_1427_0002 head / 004a next) accumulating DI:
 *   0    Σ @UNIT col9 (0x5239) over the stack
 *   2    TOTAL stack unit count (bare `inc di` per member — the earlier
 *        "case 2 = # military types" reading was case 4's body)
 *   3    # Pioneers (type 2) — the "# Pioneers" note was right all along
 *   4    # military land types {1,4,6,7,8,9}
 *   5    # Scouts (type 5)
 *   6    # {Soldier, Dragoon} + # veteran-professioned (0x315b == 0x15)
 *        others + # types {6..9} (a veteran-professioned type 6..9 counts
 *        twice — DOS's mobilizable-military count)
 *   a    # non-ship units with @UNIT combat col (0x5236) > 1
 *   b    Σ FUN_157e_004a(u,1) where ship-ness matches the tile
 *   c    # Artillery (type 0xb)
 *   d    Σ ship hold capacity (0x5237) over ships in stack
 *   e    max hold capacity over ships with +0x3148 bit7 clear
 *   1/7/8/9  return 0
 * Case 2 helper — used by the LAB_52aa odds divisor and the labor loop
 * (both call sites ndisasm-confirmed `PUSH 0x2`, viceroy_overlays.asm
 * 135593 / 139050+). Aboard units are modelled as ship cargo in Linux, so
 * they are excluded here and counted by the cargo-side queries instead.
 */
static int ai_euro_20e6_stack_count(const ColonizeTurnContext* ctx, int x, int y) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = units_get_const(ctx->units, i);
    if (!o || !o->active || o->aboard_ship_id >= 0 || o->x != x || o->y != y) {
      continue;
    }
    n++;
  }
  return n;
}

/*
 * Case 0xb: Σ combat value (FUN_157e_004a mode 1 — combat_unit_base_x8 mode 1
 * here, the same read −0x6a8e sums) over stack units whose ship-ness matches
 * the tile (land tile → land units).
 */
static int ai_euro_20e6_stack_combat_0b(ColonizeTurnContext* ctx, int x, int y) {
  ColonizeCombatStrengthCtx sctx;
  sctx.units = ctx->units;
  sctx.map = ctx->map;
  sctx.colonies = ctx->colonies;
  sctx.col1 = ctx->col1;
  const int water = map_tile_is_water(ctx->map, x, y);
  int sum = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = units_get_const(ctx->units, i);
    if (!o || !o->active || o->aboard_ship_id >= 0 || o->x != x || o->y != y) {
      continue;
    }
    if ((units_is_sea(ctx->units, i) ? 1 : 0) != (water ? 1 : 0)) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, i, 1, NULL);
  }
  return sum;
}

/* FUN_OVL14_L0000__0072d6 → FUN_521d_0906 probe, ≥0 = adjacent foreign claim. */
static int ai_euro_20e6_probe_adjacent(const ColonizeTurnContext* ctx, int x, int y, int nation) {
  int side = -1;
  return ai_goals_probe_adjacent_contact_claim(ctx->map, ctx->colonies, x, y, nation, 0, &side);
}

/*
 * iStack_6a — "this unit explores this call" (raw lines ~1095-1180). Order of
 * the clauses is DOS's own; later clauses override earlier ones.
 */
static void ai_euro_20e6_explorer_flag(ColonizeTurnContext* ctx, const ColonizeUnit* u, Ai20e6Unit* s) {
  const int t = s->dos_type;
  int ex = (t == 2 || t == 0) ? 1 : 0; /* Pioneers / Colonists */
  if (u->profession == 27) {           /* Indian Convert */
    ex = 0;
  }
  if (t == 1 || t == 4) { /* Soldiers / Dragoons */
    if (s->act_state == 0) {
      ex = 1;
    }
    if (s->act_state == 0xb && u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
      const Ai0a60UnitState* ps = &s_0a60_pilot_state[u->id];
      if (ai_euro_20e6_dist(u->x, u->y, ps->goal_x, ps->goal_y) > 12) {
        ex = 1;
      }
    }
    if (ai_goals_colony_balance_flags(ctx->map, ctx->colonies, ctx->col1, s->nation, s->cid) > 2) {
      ex = 1;
    }
    if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, s->nation) >= 0) {
      ex = 0;
    }
    if (u->profession == 21) { /* Veteran Soldier */
      ex = 0;
    }
    if (ai_euro_20e6_own_colonies_on(ctx, s->nation, s->cid) == 0 &&
        ai_euro_20e6_combat_value_on(ctx, s->nation, s->cid) < 8) {
      ex = 1;
    }
    if (t == 4 && ai_euro_20e6_foreign_colony_on(ctx, s->nation, s->cid)) {
      ex = 0;
    }
  }
  if (t == 5) { /* Scouts */
    if (s->order_code == '2') {
      ex = 1;
    }
    if (s->stance == 0) {
      ex = 0;
    }
    if (ai_euro_20e6_own_colonies_on(ctx, s->nation, s->cid) == 0 && (s->turn % 15) == 0) {
      ex = 1;
    }
    if (s->home_dist > 12 && s->any_colony_dist > 2) {
      ex = 1;
    }
    if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, s->nation) >= 0 || s->year > 1650) {
      ex = 0;
    }
  }
  /* FUN_521d_0600 composite priority must be non-zero (iStack_14). */
  if (ex) {
    const int prio = ai_goals_composite_unit_priority(
      ctx->map, ctx->colonies, ctx->col1, s->nation, u->x, u->y, t, u->profession, s->home_dist,
      s->turn, ai_euro_colony_count(ctx->colonies, s->nation)
    );
    if (prio == 0) {
      ex = 0;
    }
  }
  /* Colonist standing on an own colony that still wants colonists: stay. */
  if (t == 0 && s->home_dist == 0 && s->home_colony >= 0) {
    const ColonizeColony* hc = colonies_get(ctx->colonies, s->home_colony);
    if (hc && (hc->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS)) {
      ex = 0;
    }
  }
  /* −0x5ec4[continent] explorer cap: 2 colonists / 3 others per continent. */
  if (ex && s->cid >= 0 && s->cid < 16) {
    if (s_20e6_explorers[s->cid] < 0xff) {
      s_20e6_explorers[s->cid]++;
    }
    if ((3 - (t == 0 ? 1 : 0)) < (int)s_20e6_explorers[s->cid]) {
      ex = 0;
    }
  }
  if (s->woi) {
    ex = 0;
  }
  s->explorer = ex;
}

/*
 * LAB_521d_277a SCOUT/PATROL arm: stance 0 (no planner pressure on this
 * continent), not already tasked ('t'/'i'), land, Pioneer-or-combat unit
 * whose bound colony shares its continent → sit on the colony (orders 0x56)
 * or walk back to it (goto commit LAB_27f5). Returns 1 when it handled the
 * unit. Cite: move_scoring_land.md "0x8db8 identified".
 */
static int ai_euro_20e6_patrol_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  const int tasked = (s->order_code == 't' || s->order_code == 'i');
  if (s->stance != 0 || tasked || s->is_ship || !(s->dos_type == 2 || s->combat > 1) ||
      s->home_cid != s->cid || s->home_colony < 0) {
    return 0;
  }
  if (s->home_dist == 0) {
    return 1; /* orders 0x56: stay put, re-evaluate next call */
  }
  const ColonizeColony* hc = colonies_get(ctx->colonies, s->home_colony);
  if (!hc) {
    return 0;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, hc->x, hc->y);
  return 1;
}

/*
 * DOS 8d4a village attitude[nation] — UNPARKED 2026-09-06. The record's
 * +10+nation*2 int16 maps exactly onto ColonizeCol1Tribe.alarm[nation]
 * (= {uint8_t friction; uint8_t attacks;} at byte offset +10, low byte
 * friction — settlement_record_8d4a.md "Relationship to Linux", exact
 * offset match), save-backed and live. The 0x4c gates now read it for
 * real: word == 0 for the scout arm, word < 0x40 for the colonist arm.
 * What stays session-local is only the VISIT-INCREMENT writer
 * (FUN_465b_0000 attitude++ on each visit, unported): without it the
 * ==0/<0x40 gates would re-fire (and re-draw RNG) every turn when the
 * visit outcome does not set the scouted/learned latch, so the per-village
 * nation bits below stand in for that one writer.
 */
static int ai_euro_20e6_village_attitude(const ColonizeCol1Tribe* t, int nation) {
  if (!t || nation < 0 || nation > 3) {
    return 0;
  }
  return (int)t->alarm[nation].friction | ((int)t->alarm[nation].attacks << 8);
}
#define AI_20E6_VILLAGE_MAX 128
static uint8_t s_20e6_village_visited[AI_20E6_VILLAGE_MAX];

/*
 * FUN_521d_20e6 orders-0x4c village arms (raw ~1366-1371 scout, ~1682-1691
 * colonist). Both need the unit adjacent (dist 1) to the nearest village.
 * Gates:
 *   Scout (type 5): record +3 bit8 (tribe.state.scouted) clear, alarm
 *     quartile < 0x19 (quartile domain 0..3 — vacuously true, omitted),
 *     attitude[nation] == 0 (stand-in latch above).
 *   Colonist: profession-bearing non-combat non-Scout/Missionary type whose
 *     profession byte is 0x1c (Free/none) or 0x19 (Indentured Servant),
 *     record +3 bit2 (tribe.state.learned) clear, attitude < 0x40 (stand-in).
 * Outcome: DOS writes orders 0x4c + a 2a1f_059c dir (enter the village);
 * Linux resolves the entry through the same @ACTIONS outcome functions
 * (Speak With Chief / Live Among The Natives) — an AI unit stepping onto a
 * village tile is an attack in this port, so the peaceful entry runs in
 * place. Returns 1 when the act was consumed.
 */
static int ai_euro_20e6_village_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  if (!ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || s->village_idx < 0 ||
      s->village_dist != 1 || s->is_ship || s->nation < 0 || s->nation > 3) {
    return 0;
  }
  if (s->village_idx >= AI_20E6_VILLAGE_MAX ||
      (s_20e6_village_visited[s->village_idx] & (1u << s->nation))) {
    return 0;
  }
  ColonizeCol1Tribe* t = &ctx->col1->tribe[s->village_idx];
  if (t->nation_id < 4 || t->nation_id > 11) {
    return 0;
  }
  /* Real 8d4a attitude[nation] gates (raw 1366 scout ==0, raw 1686 colonist
   * <0x40) — the record word is tribe.alarm[nation] {friction, attacks}. */
  const int att = ai_euro_20e6_village_attitude(t, s->nation);
  if (s->dos_type == 5 && !t->state.scouted && att == 0) {
    if (ai_contact_ai_scout_visit_village(ctx, s->nation, s->village_idx, u->id)) {
      s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves_left = 0;
      return 1;
    }
  }
  if (!(s->combat > 1 || s->dos_type == 5 || s->dos_type == 3) &&
      (u->profession == UNITS_JOB_NONE || u->profession == UNITS_JOB_SERVANT) && att < 0x40 &&
      !t->state.learned && !units_is_sea(ctx->units, u->id)) {
    if (ai_contact_ai_live_among_village(ctx, s->nation, s->village_idx, u->id)) {
      s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves_left = 0;
      return 1;
    }
  }
  return 0;
}

/*
 * FUN_521d_20e6 colonist labor loop (raw ~1617-1679): a type-0 Colonist that
 * is not exploring walks to (or joins) the min-score same-continent own
 * colony that wants colonists (+0x1b bit 0x10, i.e.
 * COLONIZE_COLONY_AI_NEEDS_COLONISTS; Indian Converts join regardless).
 * Wanted size = fortification capacity 8/12/32 (FUN_15eb_0484) clamped ≤16;
 * candidate only while stack-military + population < wanted + 2.
 * Score = dist>>1, ×need when under-filled, ×2 when full (min-pick,
 * verbatim DOS arithmetic). No candidate:
 *   standing on an own colony → become a Pioneer (tools = 20, DOS orders
 *   0x3d / type 2 / +0x3159 = 0x14);
 *   else (outside WoI) force the explore ring (DOS re-loops with
 *   iStack_6a = 1) — signalled via s->explorer.
 * Returns 0 = not handled, 1 = goto set (act continues), 2 = unit consumed
 * (join / convert — abort the act, the unit may be gone).
 */
static int ai_euro_20e6_labor_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, Ai20e6Unit* s) {
  if (s->dos_type != 0 || s->explorer || s->is_ship || !ctx->colonies) {
    return 0;
  }
  int best = -1;
  int best_score = 9999;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != s->nation) {
      continue;
    }
    if (map_continent_id_at(ctx->map, c->x, c->y) != s->cid) {
      continue;
    }
    if (!(c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) && u->profession != 27) {
      continue; /* 27 = Indian Convert (DOS profession 0x1b) */
    }
    const int tier = colonies_fortification_tier(ctx->colonies, c);
    int wanted = tier <= 0 ? 8 : (tier == 1 ? 12 : 32); /* FUN_15eb_0484 */
    if (wanted > 0x10) {
      wanted = 0x10;
    }
    const int dist = ai_euro_20e6_dist(u->x, u->y, c->x, c->y);
    int score = dist >> 1;
    const int need = wanted - (int)c->population;
    if (need > 0) {
      score = need * score;
    }
    if ((int)c->population >= wanted) {
      score <<= 1;
    }
    /* 8aac case 2 = TOTAL units standing on the colony tile (PUSH 0x2 at
     * viceroy_overlays.asm 135593; real 0d38 case-2 body counts every stack
     * member, not just military — corrected 2026-09-06). */
    const int stack = ai_euro_20e6_stack_count(ctx, c->x, c->y);
    if (stack + (int)c->population < wanted + 2 && score < best_score) {
      best_score = score;
      best = i;
    }
  }
  if (best >= 0) {
    const ColonizeColony* c = colonies_get(ctx->colonies, best);
    if (!c) {
      return 0;
    }
    if (u->x == c->x && u->y == c->y) {
      (void)colonies_admit_unit(ctx->colonies, best, ctx->units, u->id, ctx->col1_ok ? ctx->col1 : NULL);
      return 2; /* FUN_1000_9b94 join; unit consumed either way (8b24) */
    }
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, c->x, c->y);
    return 1;
  }
  if (s->home_dist == 0 && s->home_colony >= 0) {
    /* Standing on an own colony with nothing to staff: re-equip as Pioneer. */
    if (u->tools < UNITS_EQUIP_TOOLS_STEP) {
      u->tools = UNITS_EQUIP_TOOLS_STEP;
    }
    u->moves_left = 0;
    return 2;
  }
  if (!s->woi) {
    s->explorer = 1; /* DOS: iStack_6a = 1, re-run the ring as an explorer */
  }
  return 0;
}

/*
 * FUN_521d_20e6 explore ring (LAB_521d_2912 → 2a59), structural port of the
 * DOS windowed best-tile scan. Was a thin radius-5 scan (2026-08-15); now the
 * raw scoring: explore-nibble ×4 base, colony pull −(d−9)² own / −(a2−d)²
 * foreign (a2 = 7 with own colonies on the continent, else 5; skip d<2, skip
 * d==2 own, −20 d==2 foreign), village penalty (pop + relation + 3)×2 scaled
 * by distance bands / capital / nation quirks / −0x6a8e discount, explorer
 * bonus (+50% when no own colony here, ×2 colonists, +local_12>>tier, +16
 * when an explore goal exists). Radius 3 (DOS local_12 rival-strength term
 * read as 0 — see block header). Coastal gate = FUN_15eb_00a2 any-neighbour-
 * ocean via map_tile_is_coastal. Skips LCR class 0x1b and tiles another own
 * unit is already committed to (unit+0x314c==7) within the radius-2 ring.
 */
static int ai_euro_land_explore_scan_target(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation_id, int force_explorer, int* out_x, int* out_y
) {
  if (!ctx || !ctx->map || !u || !out_x || !out_y) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  ai_euro_20e6_explorer_flag(ctx, u, &s);
  if (force_explorer) {
    s.explorer = 1; /* labor-loop fall-through: DOS re-loops with iStack_6a=1 */
  }
  if (s.cid < 0) {
    return 0; /* not on a mapped landmass (e.g. still in Europe) */
  }
  int tier = 3;
  const int tally_a = (ctx->col1_ok && ctx->col1 && s.cid < 16)
                        ? (int)ctx->col1->post_map.continent_tally_a[s.cid]
                        : 0;
  if (tally_a < 9) {
    tier = 0;
  } else if (tally_a < 0x19) {
    tier = 1;
  } else if (tally_a < 0x31) {
    tier = 2;
  }
  /*
   * Raw ~1600-1607: explorer pass bumps unit+0x3154 (cap 0x7f), then
   * local_12 = −0x6168[cid]*8 + that counter. Both terms live now
   * (s_euro_rival_strength writer / s_20e6_explore_fatigue).
   */
  if (s.explorer && u->id >= 0 && u->id < COLONIZE_UNITS_MAX &&
      s_20e6_explore_fatigue[u->id] < 0x7f) {
    s_20e6_explore_fatigue[u->id]++;
  }
  const int local_12 = ai_euro_20e6_local12(ctx, u, nation_id, s.cid);
  int radius = 3;
  if (local_12 > 0x1f) {
    radius = 2;
  }
  if (local_12 > 0x3f) {
    radius = 1;
  }
  const int own_here = ai_euro_20e6_own_colonies_on(ctx, nation_id, s.cid);
  const int explore_goal = ai_goals_max_primary_prio(nation_id, u->x, u->y, 6);
  int best = -999;
  int best_nib = 0;
  int bx = u->x;
  int by = u->y;
  for (int ty = u->y - radius; ty <= u->y + radius; ++ty) {
    for (int tx = u->x - radius; tx <= u->x + radius; ++tx) {
      if (tx < 0 || ty < 0 || tx >= ctx->map->width || ty >= ctx->map->height) {
        continue;
      }
      const int pres = ai_euro_20e6_tribe_or_presence(ctx, tx, ty);
      if (!(pres < 0 || pres == nation_id)) {
        continue;
      }
      if (!map_tile_is_land(ctx->map, tx, ty) || map_continent_id_at(ctx->map, tx, ty) != s.cid) {
        continue;
      }
      /* FUN_1000_893a & 0xf — real seen-plane site-score nibble (fallback
       * inside the helper for nibble-less generated maps). */
      int nib = ai_euro_20e6_site_nibble(ctx, tx, ty, nation_id);
      int score = nib * 4;
      if (map_dos_terr_class_at(ctx->map, tx, ty) == 0x1b) {
        continue; /* LCR */
      }
      const int coastal = map_tile_is_coastal(ctx->map, tx, ty) ? 1 : 0;
      if (!coastal) {
        nib = 0;
      } else {
        int cd = 9999;
        const int ncol = ai_euro_20e6_nearest_colony(ctx, tx, ty, -1, s.cid, &cd);
        if (ncol >= 0) {
          if (cd < 2) {
            continue;
          }
          const ColonizeColony* nc = colonies_get(ctx->colonies, ncol);
          if (nc && nc->nation_id == nation_id) {
            if (cd == 2) {
              continue;
            }
            if (cd < 9) {
              score += -(cd - 9) * (cd - 9);
            }
          } else {
            if (cd == 2) {
              score -= 0x14;
            }
            const int a2 = own_here ? 7 : 5;
            if (cd < a2) {
              score += -(a2 - cd) * (a2 - cd);
            }
          }
        }
        /* Another own unit already committed (act_state 7) to this ring? */
        int free_site = 1;
        for (int r = 0; r < 9 && free_site; ++r) {
          const int rx = (r < 8) ? tx + k_20e6_ring20_dx[r] : tx;
          const int ry = (r < 8) ? ty + k_20e6_ring20_dy[r] : ty;
          const int oid = units_id_at(ctx->units, rx, ry);
          if (oid >= 0 && oid != u->id && oid < COLONIZE_UNITS_MAX &&
              s_0a60_pilot_state[oid].act_state == 7) {
            free_site = 0;
          }
        }
        if (!free_site) {
          continue;
        }
        /* Village proximity penalty (raw ~1395-1440). */
        int vd = 9999;
        const int vi = ai_euro_20e6_nearest_village(ctx, tx, ty, &vd);
        if (vi >= 0 && vd < 9999) {
          const ColonizeCol1Tribe* v = &ctx->col1->tribe[vi];
          const int vcid = map_continent_id_at(ctx->map, (int)v->x, (int)v->y);
          int d = vd;
          if (ai_euro_20e6_own_colonies_on(ctx, nation_id, vcid) == 0) {
            d += 1;
          }
          if (d < 6) {
            const int rel = ai_diplo_indian_alarm(ctx->col1, (int)v->nation_id, nation_id);
            const int quart = rel < 25 ? 0 : rel < 50 ? 1 : rel < 75 ? 2 : 3;
            int base = ((int)v->population + quart + 3) * 2;
            if (vcid != s.cid) {
              base >>= 1;
            }
            int pen = base >> 1;
            if (d < 5) {
              pen += base;
            }
            if (d < 4) {
              pen += base * 2;
            }
            if (d < 3) {
              pen += base * 4;
            }
            if (d < 2) {
              pen += base * 8;
            }
            if (v->state.capital) {
              pen <<= 1;
            }
            if (nation_id == 1) {
              pen >>= 1;
            }
            if (founding_fathers_nation_has(ctx->col1, nation_id, FF_POCAHONTAS)) {
              pen >>= 1;
            }
            if (nation_id == 2) {
              pen >>= 2;
            }
            if (local_12 > 0x28) {
              pen >>= 1;
            }
            pen -= ai_euro_20e6_combat_value_on(ctx, nation_id, vcid);
            if (pen < 0) {
              pen = 0;
            }
            score -= pen;
          }
        }
        if (s.explorer && nib > 3) {
          if (own_here == 0) {
            score += score >> 1;
          }
          if (s.dos_type == 0) {
            score <<= 1;
          }
          score += local_12 >> tier;
          if (explore_goal != 0) {
            score += 0x10;
          }
        }
      }
      if (score >= best) {
        best = score;
        best_nib = nib;
        bx = tx;
        by = ty;
      }
    }
  }
  if (best_nib <= 0) {
    return 0;
  }
  if (bx == u->x && by == u->y) {
    return 0; /* DOS: act_state=7 (found here) — left to the founding arms */
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * LAB_521d_52aa attack term — odds core byte-shaped from the asm
 * (viceroy_overlays.asm:139036+, 2026-08-27; the C there is register-garbage):
 *   base = FUN_1000_9c04(unit, x, y, 0, 0)          = FUN_5fef_1b0e probe mode:
 *          (atk_strength << 3) / (def_strength + 1)  (combat_land/naval_engage)
 *   a    = FUN_1000_8aac(foe, 0) + 1                 = Σ @UNIT col9 over the foe stack + 1
 *   d    = max(FUN_1000_8aac(foe, 2), 1)             = TOTAL foe stack size
 * (case 2 re-decoded byte-exact 2026-09-06: it counts every stack member;
 * the old "# military types" reading was case 4's body)
 *   odds = ((a / d) * base) / max(@UNIT col9[own type], 1)
 * (8aac = FUN_281f_08bc → FUN_1427_0d38, the stack query dispatcher, cases
 * decoded from its jump table; col9 = DS:0x5239, zero for every land type
 * and 0/1/4/4/12/32 for Caravel..Man-O-War — so on land a=1 and any foe
 * stack of two or more units makes odds 0.) Then the transcribed
 * modifiers: ×3 own-colony tile, ×2 village, Artillery in the open → 0,
 * ×3 when flags&0x10 and stance==4, clamp 0..1000, <12 → −999 else +odds×4.
 * Still substituted: the "REF nation == 2" halving and the Soldier/Dragoon
 * vs colony adjacent-Spanish-strength skip (8aac case 0xb) — not wired.
 */
static int ai_euro_20e6_attack_term(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, const Ai20e6Unit* s, int nx, int ny, int foe_id, int* score
) {
  if (s->combat == 0) {
    return 0;
  }
  ColonizeCombatStrengthCtx sctx;
  sctx.units = ctx->units;
  sctx.map = ctx->map;
  sctx.colonies = ctx->colonies;
  sctx.col1 = ctx->col1;
  /* base = 1b0e probe: (atk << 3) / (def + 1). No foe on the tile → a bare
   * settlement/empty tile: defence strength 0 (DOS spawns a temp defender there;
   * kept as the previous stand-in). */
  int base;
  if (foe_id >= 0) {
    ColonizeCombatEngageResult er;
    memset(&er, 0, sizeof(er));
    if (s->is_ship) {
      combat_naval_engage(&sctx, u->id, foe_id, &er);
    } else {
      combat_land_engage(&sctx, u->id, foe_id, &er);
    }
    base = (er.atk_strength << 3) / (er.def_strength + 1);
  } else {
    base = (combat_unit_base_x8(&sctx, u->id, 1, NULL) << 3) / 1;
  }
  /* 8aac(foe, 0) + 1 and max(8aac(foe, 2), 1) over the whole foe stack at
   * (nx, ny); case 2 = total stack size (byte-exact 0d38 decode). */
  int col9_sum = 0;
  int stack = 0;
  if (foe_id >= 0) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* o = units_get_const(ctx->units, i);
      if (!o || !o->active || o->aboard_ship_id >= 0 || o->x != nx || o->y != ny) {
        continue;
      }
      const int t = ai_euro_20e6_dos_type(ctx->units, o);
      col9_sum += ai_euro_20e6_unit_col9(t);
      stack++;
    }
  }
  const int a = col9_sum + 1;
  const int d = stack < 1 ? 1 : stack;
  int odds = (a / d) * base;
  {
    const int own_c9 = ai_euro_20e6_unit_col9(s->dos_type);
    odds /= own_c9 < 1 ? 1 : own_c9;
  }
  int settlement = 0;
  if (ai_euro_20e6_colony_owner_at(ctx, nx, ny) >= 0) {
    odds *= 3;
    settlement = 1;
  }
  if (ai_euro_20e6_village_owner_at(ctx, nx, ny) >= 0) {
    odds <<= 1;
    settlement = 1;
  }
  if (s->dos_type == 0xb && !settlement) {
    odds = 0;
  }
  /* Raw 2855: DS:0x53d2 (crown_nation_id) == 2 ∧ open tile ∧ standing on own
   * colony (iStack_2e==0) → halve. Was the unsourced "REF nation" note. */
  if (ctx->col1_ok && ctx->col1 && (int)ctx->col1->head.crown_nation_id == 2 && !settlement &&
      s->home_dist == 0) {
    odds >>= 1;
  }
  if ((s->flags & 0x10) && s->stance == 4) {
    odds *= 3;
  }
  /*
   * Raw 2862-2882 (LAB_52aa tail): Soldier/Dragoon assaulting a Euro colony
   * tile — mass gate. def = 8aac(tile stack, 0xb) combat sum; own = Σ over
   * the 8 neighbours of the target of 8aac(neighbour stack, 0xb) for stacks
   * of the attacker's own nation (the decompile compares the owner nibble to
   * a clobbered constant 2; own-nation is the only coherent reading — noted
   * in move_scoring_20e6_full.md as "adjacent Spanish-owned units?").
   * own ≤ def → skip the tile entirely (LAB_5183).
   */
  if ((s->dos_type == 1 || s->dos_type == 4) &&
      ai_euro_20e6_colony_owner_at(ctx, nx, ny) >= 0) {
    const int def = ai_euro_20e6_stack_combat_0b(ctx, nx, ny);
    if (def != 0) {
      static const int mdx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int mdy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      int own_sum = 0;
      for (int n = 0; n < 8; ++n) {
        const int ax = nx + mdx[n];
        const int ay = ny + mdy[n];
        const int oid = units_id_at(ctx->units, ax, ay);
        const ColonizeUnit* ou = oid >= 0 ? units_get_const(ctx->units, oid) : NULL;
        if (ou && ou->nation_id == s->nation) {
          own_sum += ai_euro_20e6_stack_combat_0b(ctx, ax, ay);
        }
      }
      if (own_sum <= def) {
        return 0; /* caller treats the tile as unscoreable */
      }
    }
  }
  if (odds > 999 || odds < 0) {
    odds = 1000;
  }
  if (odds < 0xc) {
    *score -= 999;
  } else {
    *score += odds * 4;
  }
  return 1;
}

/*
 * LAB_521d_4d2e → 5183: the 8-direction wander scorer, land branch. Picks
 * one adjacent tile (or stay) exactly as DOS does when no arm above has
 * committed a destination. Returns dir 0..7, or 8 = stay.
 */
static int ai_euro_20e6_wander_step(ColonizeTurnContext* ctx, ColonizeUnit* u, Ai20e6Unit* s) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  const int nation = s->nation;
  /* uVar14 — far-probe/fog enable: no adjacent claim (probe mode 1) or a
   * non-combat land unit. */
  int fog_enable = 0;
  if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, nation) < 0 || (!s->is_ship && s->combat == 0)) {
    fog_enable = 1;
  }
  /* unit+0x3148 bit4 wander_dest_chosen: peacetime distant-tile roll (raw
   * ~1960-1995) — kept as the latch only; the >7-tile random goto it
   * produces is a ship-only arm (type 0xd..0x12), not reached on land. */
  int best = -999;
  int best_dir = 8;
  int best_attack = 0;
  const int last_dir = (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) ? s_euro_last_dir[u->id] : -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!map_coords_inset(ctx->map, nx, ny)) {
      continue; /* FUN_1000_84f2 inset bounds */
    }
    const int terr = map_dos_terr_class_at(ctx->map, nx, ny);
    if (terr == 0x19 || terr == 0x1a) {
      continue; /* land unit: ocean / high seas */
    }
    const int owner = ai_euro_20e6_owner_nibble(ctx->map, nx, ny);
    const int here = units_id_at(ctx->units, nx, ny);
    const ColonizeUnit* hu = here >= 0 ? units_get_const(ctx->units, here) : NULL;
    int score = 0;
    const int dest_river = map_tile_has_river(ctx->map, nx, ny) ? 1 : 0;
    const int dest_road = map_tile_has_road(ctx->map, nx, ny) ? 1 : 0;
    const int cardinal = (d & 1) == 0;
    if (s->dos_type == 5) { /* Scouts */
      score = dos_rng_range(ctx->rng, 1, 8);
      if (s->unit_river && dest_river && cardinal) {
        score += 2;
      } else if (s->unit_road && dest_road) {
        score += 1;
      } else {
        score -= map_dos_terr_cost_byte(terr) * 3;
      }
    } else if (!s->explorer) {
      if (u->col1_vis_mask == 0) { /* unseen by every Euro nation */
        if ((s->flags & 0x20) == 0 && (s->flags & 0x10) == 0) {
          score = dos_rng_range(ctx->rng, 1, 3);
          if (!s->woi) {
            score += map_dos_terr_found_score_byte(terr);
          } else {
            score -= map_dos_terr_found_score_byte(terr);
          }
        } else {
          score = dos_rng_range(ctx->rng, 1, 3);
          if ((s->unit_river && dest_river && cardinal) || (s->unit_road && dest_road)) {
            score += 1;
          } else {
            score -= map_dos_terr_cost_byte(terr);
          }
        }
      } else {
        score = dos_rng_range(ctx->rng, 1, 5);
        if (here < 0 || owner != nation) {
          score += map_dos_terr_found_score_byte(terr) << 2;
        }
      }
    } else {
      /* Raw ~2670: (rng(1,4) + (893a & 0xf)) >> 1 — real site nibble now. */
      const int nib = ai_euro_20e6_site_nibble(ctx, nx, ny, nation);
      score = (dos_rng_range(ctx->rng, 1, 4) + nib) >> 1;
    }
    if (terr == 0x1a) {
      score -= 0x10;
    }
    /* Combat land unit stepping onto a settlement tile. */
    if (!s->is_ship && s->combat > 1) {
      const int col_owner = ai_euro_20e6_colony_owner_at(ctx, nx, ny);
      if (col_owner >= 0) {
        if (owner == nation || col_owner == nation) {
          const int cid = colonies_id_at(ctx->colonies, nx, ny);
          const ColonizeColony* c = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
          const int af = c ? (int)c->ai_flags : 0;
          if (af & COLONIZE_COLONY_AI_NEEDS_GARRISON) {
            score += 10;
          } else if (af & COLONIZE_COLONY_AI_NEEDS_MILITARY) {
            score += 6;
          } else if (af & COLONIZE_COLONY_AI_NEEDS_COLONISTS) {
            score += 3;
          }
        } else {
          score += 0x10;
        }
      }
    }
    int attack = 0;
    const int pres = ai_euro_20e6_tribe_or_presence(ctx, nx, ny);
    if ((here < 0 && pres < 0) || owner == nation) {
      /* LAB_54f5: empty or own tile — fall through to facing/fog terms. */
    } else if (owner < 4) {
      const int rel = ai_euro_20e6_diplo(ctx->col1, nation, owner);
      const int hu_type = hu ? ai_euro_20e6_dos_type(ctx->units, hu) : -1;
      const int woi_ok = !s->woi || owner < 0;
      /* DOS scores the tile as an attack when the owner is not yet MET (a
       * forced first contact) or a Privateer is involved; Linux contact is
       * driven by ai_contact_*, so this port only takes the arm at war —
       * a deliberate narrowing, not a transcription slip. */
      const int at_war = owner >= 0 && ctx->col1 && ai_diplo_at_war(ctx->col1, nation, owner);
      if ((at_war || ((rel & AI_DIPLO_MET) == 0 && s->dos_type == 0x10) || hu_type == 0x10) && woi_ok) {
        if (!ai_euro_20e6_attack_term(ctx, u, s, nx, ny, here, &score)) {
          continue;
        }
        attack = 1;
      } else {
        continue;
      }
    } else {
      const int rel_score = ai_diplo_indian_alarm(ctx->col1, owner, nation); /* 84fc > 0x4a */
      const int rel = ai_euro_20e6_diplo(ctx->col1, nation, owner);
      if (rel_score > 0x4a || (rel & AI_DIPLO_WAR)) {
        if (rel & AI_DIPLO_WAR) {
          score <<= 1;
        }
        if (ai_euro_20e6_own_colonies_on(ctx, nation, s->cid) == 0) {
          continue;
        }
        if (!ai_euro_20e6_attack_term(ctx, u, s, nx, ny, here, &score)) {
          continue;
        }
        attack = 1;
      } else {
        continue;
      }
    }
    /* LAB_54f5 facing: −2·diff² against unit+0x314f. */
    if (last_dir >= 0 && last_dir < 8) {
      int diff = last_dir - d;
      if (diff < 1) {
        diff = -diff;
      }
      if (diff > 4) {
        diff = 8 - diff;
      }
      score += diff * diff * -2;
    }
    /* Neighbours of the destination: −10 per hostile combat unit when the
     * mover has combat byte 0 (Treasure). */
    for (int n = 0; n < 8; ++n) {
      const int ax = nx + dx[n];
      const int ay = ny + dy[n];
      if (ax < 0 || ay < 0 || ax >= ctx->map->width || ay >= ctx->map->height) {
        continue;
      }
      const int po = map_tile_tribe_or_presence(ctx->map, ax, ay);
      if (po < 0 || po == nation || s->combat != 0) {
        continue;
      }
      if ((ai_euro_20e6_diplo(ctx->col1, nation, po) & 0x60) != 0x20) {
        continue;
      }
      const int oid = units_id_at(ctx->units, ax, ay);
      const ColonizeUnit* ou = oid >= 0 ? units_get_const(ctx->units, oid) : NULL;
      if (ou && ai_euro_20e6_type_combat(ai_euro_20e6_dos_type(ctx->units, ou)) != 0) {
        score -= 10;
      }
    }
    /* Far probe (unit + 4·dir) explore terms. */
    if (fog_enable) {
      const int fx = u->x + dx[d] * 4;
      const int fy = u->y + dy[d] * 4;
      if (map_coords_inset(ctx->map, fx, fy) && !map_tile_is_water(ctx->map, fx, fy) &&
          ctx->map->seen && !map_tile_seen_by(ctx->map, fx, fy, nation)) {
        score += 8; /* DS:0x9faa coarse cell unseen — per-nation seen[] stand-in */
      }
      for (int n = 0; n < 8; ++n) {
        const int ax = fx + dx[n];
        const int ay = fy + dy[n];
        if (!map_coords_inset(ctx->map, ax, ay)) {
          continue;
        }
        if (nation < 4 && ctx->map->seen && !map_tile_seen_by(ctx->map, ax, ay, nation) &&
            !map_tile_is_water(ctx->map, ax, ay)) {
          score += 2;
        }
        if (map_tile_tribe_or_presence(ctx->map, ax, ay) >= 0) {
          score -= 2;
        }
        if (s->explorer) {
          score += (int)k_20e6_terr_site_byte[map_dos_terr_class_at(ctx->map, ax, ay) & 31];
        }
      }
    }
    if (score > best) {
      best = score;
      best_dir = d;
      best_attack = attack;
    }
  }
  /* LAB_5183 tail for an attack pick: DOS stays when fewer than 3 thirds
   * (one full move) remain — Linux moves_left is whole moves and the act
   * loop already requires >0, so nothing extra to gate here. */
  (void)best_attack;
  return best_dir;
}

/* Returns non-zero to abort act (DOS 20e6 non-zero return). */
/*
 * FUN_521d_20e6 ring-hop pick (raw 2416-2458, the LAB_4b2c-region explorer
 * arm): when the windowed ring scan produced no target, an explorer hops
 * 4 tiles out along a latched random ring20 direction. Slot unset → roll
 * FUN_1000_86c4(1,0x14) − 1; target = (ring_dx*4, ring_dy*4); validate
 * walkable land (84f2), fresh coarse region (−0x6056 nibble &6 == 0 —
 * Linux substitution per the block header: target tile unseen by the
 * nation, the same seen-plane stand-in used for DS:0x9faa), same continent
 * (8912) and no presence at all (88c2 < 0). On success the hop length
 * max(dx*4, dy*4) (signed char, byte-faithful) latches into +0x3155, the
 * explore fatigue drops by 8 when above 8 (raw 2455-2456), and the target
 * commits as a goto (27f5). Returns 1 when a hop goto was set.
 */
static int ai_euro_20e6_ring_hop(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  if (!ctx || !ctx->map || !u || u->id < 0 || u->id >= COLONIZE_UNITS_MAX || s->cid < 0) {
    return 0;
  }
  int16_t* slotp = &s_20e6_hop_slot[u->id];
  if (*slotp == 0) {
    if (!ctx->rng) {
      return 0;
    }
    *slotp = (int16_t)dos_rng_range(ctx->rng, 1, 0x14); /* raw 2421: 86c4(1,0x14), stored +1 */
  }
  const int slot = (int)*slotp - 1;
  if (slot < 0 || slot >= 20) {
    *slotp = 0;
    return 0;
  }
  const int hx = (int)k_20e6_ring20_dx[slot] * 4;
  const int hy = (int)k_20e6_ring20_dy[slot] * 4;
  const int tx = u->x + hx;
  const int ty = u->y + hy;
  if (!map_coords_inset(ctx->map, tx, ty) || !map_tile_is_land(ctx->map, tx, ty)) {
    return 0; /* 84f2 */
  }
  if (map_tile_seen_by(ctx->map, tx, ty, s->nation)) {
    return 0; /* −0x6056 coarse region already stamped (substituted) */
  }
  if (map_continent_id_at(ctx->map, tx, ty) != s->cid) {
    return 0; /* 8912 */
  }
  if (ai_euro_20e6_tribe_or_presence(ctx, tx, ty) >= 0) {
    return 0; /* 88c2: any owner/presence blocks the hop */
  }
  if (getenv("AI_20E6_HOP_TRACE")) {
    fprintf(stderr, "[hop] unit %d n%d slot %d -> (%d,%d)\n", u->id, s->nation, slot, tx, ty);
  }
  s_20e6_hop_steps[u->id] = (int8_t)(hx < hy ? hy : hx); /* raw 2447-2453 */
  if (s_20e6_explore_fatigue[u->id] > 8) {
    s_20e6_explore_fatigue[u->id] -= 8;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
  return 1;
}

static int ai_euro_move_scoring_gate(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  /*
   * Ships: never retarget here — landfall/sail courses are owned by case 0x0b.
   * (Sticky clear or arrival wipe must not become a distant FOUND yank.)
   */
  if (units_is_sea(ctx->units, u->id)) {
    return 0;
  }
  /*
   * At-war land hunters / Artillery siege: defer course to act-level hunt
   * (do not explore-yank idle Soldier/Dragoon/Scout/Artillery before hunt).
   * Passive fortify/sentry — act wakes via units_wake then hunts.
   */
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    const char* hn = units_display_name(ctx->units, u);
    if (ai_euro_is_land_war_hunter(hn) || ai_euro_is_artillery_name(hn)) {
      return 0;
    }
  }
  /*
   * Peace: do not FOUND/explore-yank passive colony Artillery before §2d3
   * border wake (garrison Soldiers often already have planning MILITARY goto).
   * Cite: Colonization.pdf Defending a Colony; euro_unit_act §2d3.
   * Also: idle Soldier already on own colony — fortify/LABOR join first
   * (Dutch Isabella TURN4→5 admits beachhead soldier; yank broke pop 1→2).
   */
  {
    const char* hn = units_display_name(ctx->units, u);
    if (ai_euro_land_is_passive_orders(u) && ai_euro_is_artillery_name(hn)) {
      return 0;
    }
    if (ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* oc = colonies_get(ctx->colonies, cid);
        if (oc && oc->active && oc->nation_id == nation_id) {
          if (ai_euro_is_colony_garrison_name(hn)) {
            return 0;
          }
          if (ai_euro_colony_wants_construction_labor(ctx->colonies, oc)) {
            return 0;
          }
        }
      }
    }
  }
  int gx = u->x;
  int gy = u->y;
  int fx = 0;
  int fy = 0;
  int is_roam = 0;
  /*
   * No colony yet: settle where we landed, FUN_521d_06ae style (own tile plus
   * the eight neighbours), rather than walking at the nation-wide goal band.
   * Those goals sit next to villages all over the map, so a freshly landed
   * founder used to set off across the continent and either never arrive or
   * oscillate between two tiles forever.
   */
  if (ctx->colonies && ai_euro_colony_count(ctx->colonies, nation_id) == 0) {
    const char* fname = units_display_name(ctx->units, u);
    if (fname && (ai_euro_name_is_pioneer(fname) || strstr(fname, "Colonist") != NULL)) {
      int lx = 0;
      int ly = 0;
      if (ai_euro_pick_founding_tile(
            ctx->map, ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, nation_id, u->x, u->y,
            0, &lx, &ly
          )) {
        ai_goals_upsert_primary(nation_id, lx, ly, AI_GOAL_FOUND, 7);
        fx = lx;
        fy = ly;
      }
    }
  }
  /* Nearest top-priority FOUND on this unit's own landmass -- the table is
   * priority-ordered but distance-blind, and planning fills it with a band of
   * equal-priority tribe-adjacent sites shared by all four nations. */
  if (ai_goals_best_found_tile_near(ctx->map, nation_id, u->x, u->y, &fx, &fy)) {
    gx = fx;
    gy = fy;
  } else if (units_orders_follow_goto(u->orders)) {
    gx = u->goto_x;
    gy = u->goto_y;
  } else {
    /*
     * FUN_521d_20e6 land arms, in DOS order: SCOUT/PATROL (LAB_277a) →
     * explorer ring scan (LAB_2912, explorers only) → 8-direction wander
     * scorer (LAB_4d2e) committing one adjacent tile as a one-shot goto
     * (epilogue LAB_589e: unit+0x314c=0xc, +0x314d/e = next tile) or
     * staying (dir 8 → +0x314c=5, re-evaluate next call).
     */
    Ai20e6Unit s;
    ai_euro_20e6_prologue(ctx, u, nation_id, &s);
    ai_euro_20e6_explorer_flag(ctx, u, &s);
    if (ai_euro_20e6_patrol_arm(ctx, u, &s)) {
      return 0;
    }
    /* DOS order: LAB_277a fall-through → 0x4c village arms → 2912 ring →
     * colonist labor loop (raw runs it inside the ring do-loop; same effect
     * here since the ring only fires for explorers). A consumed unit (village
     * entry, colony join, Pioneer convert) aborts the act — it may no longer
     * exist; a labor walk (goto set) lets the act loop move it this turn. */
    if (ai_euro_20e6_village_arm(ctx, u, &s)) {
      return 1;
    }
    {
      const int lr = ai_euro_20e6_labor_arm(ctx, u, &s);
      if (lr == 2) {
        return 1;
      }
      if (lr == 1) {
        return 0;
      }
    }
    /*
     * Raw 1600-1611 hop countdown, DOS shape: +0x3155 != 0 → decrement and
     * SKIP the ring scan (the flow drops straight to the LAB_4b2c hop arm,
     * which re-commits the latched slot's target); +0x3155 == 0 → reset the
     * slot latch (+0x3156 = 0xff), run the ring scan, and only a failed scan
     * reaches the hop pick with a fresh roll.
     */
    int hop_scan = 1;
    if (s.explorer && u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
      if (s_20e6_hop_steps[u->id] != 0) {
        s_20e6_hop_steps[u->id]--;
        hop_scan = 0;
        if (ai_euro_20e6_ring_hop(ctx, u, &s)) {
          return 0;
        }
      } else {
        s_20e6_hop_slot[u->id] = 0; /* raw 1602: +0x3156 = 0xff */
      }
    }
    if (s.explorer && hop_scan &&
        ai_euro_land_explore_scan_target(ctx, u, nation_id, s.explorer, &fx, &fy)) {
      gx = fx;
      gy = fy;
      is_roam = 1; /* unit+0x314c==5 idle-roam (explore ring) */
    } else if (s.explorer && hop_scan && ai_euro_20e6_ring_hop(ctx, u, &s)) {
      return 0; /* raw 2416-2458: scan failed, hop 4 tiles out instead */
    } else {
      const int dir = ai_euro_20e6_wander_step(ctx, u, &s);
      if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
        s_euro_last_dir[u->id] = (int8_t)dir; /* unit+0x314f, 8 = stay */
      }
      if (dir == 8) {
        return 0; /* stay; DOS +0x314c=5 */
      }
      static const int wdx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int wdy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      const int nx = u->x + wdx[dir];
      const int ny = u->y + wdy[dir];
      if (!map_coords_inset(ctx->map, nx, ny)) {
        return 0;
      }
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, nx, ny);
      if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
        s_euro_roam_wander[u->id] = 1; /* unit+0x314c==5 idle-roam (wander step) */
      }
      return 0;
    }
  }
  int dx = 0;
  int dy = 0;
  if (!ai_euro_score_move(ctx, u, gx, gy, &dx, &dy)) {
    return 1;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, gx, gy);
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    s_euro_roam_wander[u->id] = (uint8_t)is_roam;
  }
  return 0;
}

/*
 * FUN_4720_049e territorial notify (thin, approximate — 2026-08-15 find,
 * likely `@VIOLATE` trigger, "Zero code refs; trigger function not found"
 * per `popups.md` until now). DOS: land unit ends its move adjacent to a
 * foreign Euro unit whose nation it has met (both directions,
 * `FUN_1000_8c28 & 0x40` — corrected from an earlier "PEACE flag" misread,
 * this is `AI_DIPLO_MET`) and not at war, fires a dialog naming both
 * nations. GAME.TXT: "{%STRING0} violate {%STRING1} territory near
 * {%STRING2}! Colonists are outraged!"
 *
 * Approximated, not byte-exact — see euro_unit_act.md for the full trace:
 * - Exact catalog id (0x13cb vs 0x13d7) and violator/owner slot order were
 *   never confirmed; assumed here the acting (moving) unit's nation is the
 *   violator (%STRING0) and the encountered unit's nation is the owner
 *   (%STRING1) — the plain-English reading of the tag name.
 * - %STRING2 (place) has no explicit arg-set call in the DOS body found
 *   (likely auto-filled by the dialog engine from context); approximated
 *   here as the nearest colony's name.
 * - DOS's exact ship/land-terrain gate and re-fire suppression weren't
 *   fully traced; approximated with a land-unit-only trigger and a flat
 *   per-unit cooldown (own addition, not DOS-derived) to avoid repeat
 *   spam for units that just sit adjacent to each other.
 */
static void ai_euro_try_violate_notify(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !u || !u->active ||
      units_is_sea(ctx->units, u->id) || u->nation_id < 0 || u->nation_id >= 4) {
    return;
  }
  if (!ctx->messages || !ctx->status || ctx->status_size <= 0) {
    return; /* structural notify only, matches @SNEAK precedent */
  }
  const uint32_t turn = (ctx->turn_number && *ctx->turn_number) ? *ctx->turn_number : 0;
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX && s_violate_last_turn[u->id] != 0 &&
      turn >= s_violate_last_turn[u->id] && turn - s_violate_last_turn[u->id] < 10) {
    return;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe < 0 || units_is_sea(ctx->units, foe)) {
      continue;
    }
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f || f->nation_id == u->nation_id || f->nation_id < 0 || f->nation_id >= 4) {
      continue;
    }
    if (!(ai_diplo_read(ctx->col1, u->nation_id, f->nation_id) & AI_DIPLO_MET) ||
        !(ai_diplo_read(ctx->col1, f->nation_id, u->nation_id) & AI_DIPLO_MET)) {
      continue;
    }
    if (ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      continue;
    }
    if (ctx->human_nation < 0 || ctx->human_nation >= 4 ||
        (u->nation_id != ctx->human_nation && f->nation_id != ctx->human_nation)) {
      continue; /* structural notify only fires with a human party */
    }
    const char* place = NULL;
    int best_d = -1;
    if (ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active) {
          continue;
        }
        const int dd = abs(c->x - u->x) + abs(c->y - u->y);
        if (best_d < 0 || dd < best_d) {
          best_d = dd;
          place = c->name;
        }
      }
    }
    PopupMsgTokens tok = {0};
    tok.string0 = ai_diplo_rival_name(ctx->col1, u->nation_id);
    tok.string1 = ai_diplo_rival_name(ctx->col1, f->nation_id);
    tok.string2 = (place && place[0]) ? place : "the frontier";
    popup_msg_fill(
      ctx->messages, "VIOLATE", &tok,
      "%STRING0 violate %STRING1 territory near %STRING2! Colonists are outraged!",
      ctx->status, ctx->status_size
    );
    popup_msg_strip_markup(ctx->status); /* status line: no {} coloring */
    if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
      s_violate_last_turn[u->id] = turn ? turn : 1;
    }
    return;
  }
}

static void ai_euro_try_attack(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty) {
  if (!ctx || !ctx->units || !u) {
    return;
  }
  /*
   * @UNIT attack 0 means the unit cannot attack at all -- Pioneers, Colonists,
   * Wagon Trains, unarmed transports. Without this gate a settler walking a
   * FOUND goto straight at a village fought the Braves standing on it and died,
   * which is how AI nations kept losing their founder before ever founding.
   */
  {
    const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
    if (t && t->attack <= 0) {
      return;
    }
  }
  const int foe = units_best_defender_at(
    ctx->units, ctx->col1_ok ? ctx->col1 : NULL, tx, ty, u->id, u->id
  );
  if (foe < 0) {
    return;
  }
  const ColonizeUnit* f = units_get_const(ctx->units, foe);
  if (!f || f->nation_id == u->nation_id) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && f->nation_id >= 0 && f->nation_id < 4) {
    if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      /*
       * @SNEAK ("Sneak attack by the treacherous {attacker}!") — confirmed
       * real, 2026-08-14, via live user testimony (euro_diplo.md "FA
       * negotiation screen"): AI Euro nations can attack outright, with
       * war declared as a SIDE EFFECT of the attack rather than a
       * prerequisite for it — exactly this code path (war-declare gated
       * on the attack itself, not the other way around). The mechanic was
       * already correctly implemented; only the player-facing
       * notification was missing (declare was via the bare, status-free
       * ai_diplo_declare_war). Switched to the _ctx variant (gains the
       * existing boycott/sticky chrome for free) and override its generic
       * @DECLAREWAR status with the real @SNEAK wording when the human is
       * a party, since this specific path is never a negotiated/expected
       * declaration.
       */
      ai_diplo_declare_war_ctx(ctx, u->nation_id, f->nation_id);
      if (ctx->human_nation >= 0 && ctx->human_nation < 4 &&
          (u->nation_id == ctx->human_nation || f->nation_id == ctx->human_nation) &&
          ctx->status && ctx->status_size > 0) {
        PopupMsgTokens tok = {0};
        tok.string0 = ai_diplo_rival_name(ctx->col1, u->nation_id);
        popup_msg_fill(
          ctx->messages, "SNEAK", &tok, "Sneak attack by the treacherous %STRING0!",
          ctx->status, ctx->status_size
        );
        popup_msg_strip_markup(ctx->status); /* status line: no {} coloring */
      }
    }
  }
  if (units_is_sea(ctx->units, u->id)) {
    units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
    /* Land combat spends MP via try_move into the tile; ships cannot enter
     * foe tiles — spend remaining MP after naval resolve (structural). */
    if (u->active) {
      u->moves_left = 0;
    }
  } else if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
    units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng);
  }
  if (u->active && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      if (c && c->nation_id != u->nation_id && c->nation_id >= 0 && c->nation_id < 4 &&
          units_foreign_unit_at(ctx->units, u->x, u->y, u->id, u->nation_id) < 0) {
        int plunder = 0;
        for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
          if (c->stock[i] > 0) {
            plunder += c->stock[i];
          }
        }
        ColonizeColony snap = *c;
        if (colonies_capture(ctx->colonies, cid, u->nation_id)) {
          units_combat_notify_colony_captured(
            ctx->col1_ok ? ctx->col1 : NULL, &snap, u->nation_id, plunder
          );
        }
      }
    }
  }
}

/* True when ship already has a non-stationary sail/goto course. */
static int ai_euro_ship_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map) {
  if (!u || !map || !units_orders_follow_goto(u->orders)) {
    return 0;
  }
  if (u->goto_x < 0 || u->goto_y < 0 || u->goto_x >= UNITS_GOTO_NONE ||
      u->goto_y >= UNITS_GOTO_NONE || u->goto_x >= map->width || u->goto_y >= map->height) {
    return 0;
  }
  return u->goto_x != u->x || u->goto_y != u->y;
}

/* Water tile adjacent to a coastal colony (ships cannot enter foreign land). */
static int ai_euro_coastal_water_near(
  const ColonizeWorldMap* map,
  int cx,
  int cy,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y || !map_tile_is_coastal(map, cx, cy)) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = cx + dx[d];
    const int ny = cy + dy[d];
    if (!map_tile_is_water(map, nx, ny)) {
      continue;
    }
    const int dist = abs(nx - from_x) + abs(ny - from_y);
    if (best < 0 || dist < best) {
      best = dist;
      bx = nx;
      by = ny;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/* Caravel / Merchantman / Galleon — New-World cargo haul (manual trade ships). */
static int ai_euro_is_cargo_ship_name(const char* name) {
  return name &&
         (strstr(name, "Caravel") != NULL || strstr(name, "Merchantman") != NULL ||
          strstr(name, "Galleon") != NULL);
}

/*
 * LAB_521d_3558 peace colony-sail score (~89614–89711 thin):
 * prefer higher pop, nearer ship, docks present, hungrier idle timer.
 * War cargo: fort% + human-presence peel + 0x1b-shaped defense ladder
 * (Stockade/Fort/Fortress). Cite: move_scoring_ship.md; Series O.
 */
static int ai_euro_ocean_colony_sail_score(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  const ColonizeColony* c,
  int at_war_cargo
) {
  if (!ctx || !ctx->map || !c || !c->active || c->nation_id != nation_id) {
    return -999999;
  }
  if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
    return -999999;
  }
  const int d = abs(c->x - from_x) + abs(c->y - from_y);
  int score = (int)c->population * 8 - d * 4 + (int)c->cargo_idle_turns * 8;
  const int docks_id = colonies_find_building(ctx->colonies, "Docks");
  if (docks_id >= 0 && c->has_building[docks_id]) {
    score += 16; /* dock flag 0x1b&0x10 stand-in */
  }
  if (at_war_cargo) {
    const int fort = ai_euro_colony_fort_bonus_at(ctx->colonies, c->x, c->y, nation_id);
    score += fort / 5;
    score += 14; /* war human-presence / fort peel thin */
    /* Building flags 0x1b stand-in: Stockade +8 / Fort +16 / Fortress +24. */
    const int fortress_id = colonies_find_building(ctx->colonies, "Fortress");
    const int fort_id = colonies_find_building(ctx->colonies, "Fort");
    const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
    if (fortress_id >= 0 && c->has_building[fortress_id]) {
      score += 24;
    } else if (fort_id >= 0 && c->has_building[fort_id]) {
      score += 16;
    } else if (stockade_id >= 0 && c->has_building[stockade_id]) {
      score += 8;
    }
  }
  return score;
}

/*
 * Nearest own coastal colony that is tools-short (stock[TOOLS]<20) or
 * food-short (stock[FOOD] < pop*2). Peace path uses 3558 colony-sail score;
 * war cargo path boosts fort/idle. Cite: euro_unit_act §2d / 5cf6; 3558.
 */
static int ai_euro_nearest_short_coastal_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_cx,
  int* out_cy
) {
  if (!ctx || !ctx->colonies || !ctx->map || !out_cx || !out_cy || nation_id < 0 ||
      nation_id >= 4) {
    return 0;
  }
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
  int best_score = -999999;
  int have = 0;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
      continue;
    }
    const int tools_short = c->stock[COLONIZE_CARGO_TOOLS] < 20;
    const int lumber_short = c->stock[COLONIZE_CARGO_LUMBER] < 20;
    const int ore_short = c->stock[COLONIZE_CARGO_ORE] < 20;
    const int muskets_short = c->stock[COLONIZE_CARGO_MUSKETS] < 10;
    const int horses_short = c->stock[COLONIZE_CARGO_HORSES] < 10;
    const int food_short =
      c->population > 0 && c->stock[COLONIZE_CARGO_FOOD] < c->population * 2;
    if (!tools_short && !lumber_short && !ore_short && !muskets_short && !horses_short &&
        !food_short) {
      continue;
    }
    const int score =
      ai_euro_ocean_colony_sail_score(ctx, nation_id, from_x, from_y, c, at_war);
    if (!have || score > best_score) {
      have = 1;
      best_score = score;
      bx = c->x;
      by = c->y;
    }
  }
  if (!have) {
    return 0;
  }
  *out_cx = bx;
  *out_cy = by;
  return 1;
}

/*
 * Idle Caravel/Merchantman trade haul (thin 5b66): free goods-hold capacity or
 * TOOLS / LUMBER / MUSKETS / HORSES / FOOD cargo → AI_SAIL toward coastal water
 * by matching-short own colony. Load/unload mirrors wagon §2d via
 * colonies_transfer_to_unit / from_unit. Cite: manual Caravel/Merchantman
 * cargo; Colonization.pdf naval transport / colony supply / Wagon Train
 * pattern; 5cf6 food/lumber_short. Peace only — war hunt owns idle ships at war.
 * Returns 1 if haul course set or already adjacent delivering.
 */
static int ai_euro_try_ship_trade_haul(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ship || !ship->active) {
    return 0;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, ship);
  if (!ai_euro_is_cargo_ship_name(name)) {
    return 0;
  }
  if (units_goods_hold_count(ctx->units, ship->id) <= 0) {
    return 0;
  }
  const int has_tools = ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_TOOLS);
  const int has_lumber =
    ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_LUMBER);
  const int has_ore = ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_ORE);
  const int has_muskets =
    ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_MUSKETS);
  const int has_horses =
    ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_HORSES);
  const int has_food = ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_FOOD);
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, ship);
  if (!has_tools && !has_lumber && !has_ore && !has_muskets && !has_horses && !has_food &&
      !has_cap) {
    return 0;
  }

  /* Adjacent / same-tile short coastal colony + haul cargo → structural unload. */
  if (has_tools || has_lumber || has_ore || has_muskets || has_horses || has_food) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!ai_euro_tiles_near(ship->x, ship->y, c->x, c->y)) {
        continue;
      }
      const int n = units_goods_hold_count(ctx->units, ship->id);
      int unloaded = 0;
      for (int h = 0; h < n; ++h) {
        if (ship->hold_goods_amount[h] <= 0 || ship->hold_goods_amount[h] >= 255) {
          continue;
        }
        const int ct = ship->hold_goods_type[h];
        if (ct != COLONIZE_CARGO_TOOLS && ct != COLONIZE_CARGO_LUMBER &&
            ct != COLONIZE_CARGO_ORE && ct != COLONIZE_CARGO_MUSKETS &&
            ct != COLONIZE_CARGO_HORSES && ct != COLONIZE_CARGO_FOOD) {
          continue;
        }
        if (!ai_euro_colony_haul_cargo_short(c, ct)) {
          continue;
        }
        const int moved = colonies_transfer_from_unit(
          ctx->colonies, c->id, ctx->units, ship->id, h, NULL
        );
        if (moved > 0) {
          unloaded = 1;
          if (ct == COLONIZE_CARGO_FOOD) {
            AiEuroInventory* inv = ai_goals_inventory(nation_id);
            if (inv) {
              if (inv->food_short > moved) {
                inv->food_short -= moved;
              } else {
                inv->food_short = 0;
              }
            }
          } else if (ct == COLONIZE_CARGO_LUMBER) {
            AiEuroInventory* inv = ai_goals_inventory(nation_id);
            if (inv) {
              if (inv->lumber_short > moved) {
                inv->lumber_short -= moved;
              } else {
                inv->lumber_short = 0;
              }
            }
          } else if (ct == COLONIZE_CARGO_ORE) {
            AiEuroInventory* inv = ai_goals_inventory(nation_id);
            if (inv) {
              if (inv->ore_short > moved) {
                inv->ore_short -= moved;
              } else {
                inv->ore_short = 0;
              }
            }
          }
          break;
        }
      }
      if (unloaded) {
        return 1; /* delivered — stay near colony */
      }
    }
  }

  /* On surplus coastal own colony with free hold → load construction/military/
   * food ladder. Ships berth on adjacent water (colonies_id_at usually misses).
   * When food_short>20 prefer FOOD first (mirror wagon haul). */
  if (has_cap && !has_tools && !has_lumber && !has_ore && !has_muskets && !has_horses &&
      !has_food) {
    const AiEuroInventory* inv = ai_goals_inventory(nation_id);
    const int food_first = inv && inv->food_short > 20;
    static const int k_ship_load_default[] = {
      COLONIZE_CARGO_TOOLS,
      COLONIZE_CARGO_LUMBER,
      COLONIZE_CARGO_ORE,
      COLONIZE_CARGO_MUSKETS,
      COLONIZE_CARGO_HORSES,
      COLONIZE_CARGO_FOOD
    };
    static const int k_ship_load_food_first[] = {
      COLONIZE_CARGO_FOOD,
      COLONIZE_CARGO_TOOLS,
      COLONIZE_CARGO_LUMBER,
      COLONIZE_CARGO_ORE,
      COLONIZE_CARGO_MUSKETS,
      COLONIZE_CARGO_HORSES
    };
    const int* k_ship_load = food_first ? k_ship_load_food_first : k_ship_load_default;
    const size_t n_ship =
      food_first ? sizeof(k_ship_load_food_first) / sizeof(k_ship_load_food_first[0])
                 : sizeof(k_ship_load_default) / sizeof(k_ship_load_default[0]);
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!ai_euro_tiles_near(ship->x, ship->y, c->x, c->y)) {
        continue;
      }
      int loaded = 0;
      if (c->specialty_cargo != 0xff && (int)c->specialty_cargo < COLONIZE_CARGO_COUNT &&
          ai_euro_colony_haul_cargo_surplus(c, (int)c->specialty_cargo)) {
        const int ct = (int)c->specialty_cargo;
        const int amt = ai_euro_haul_load_amount(c, ct);
        if (amt > 0 &&
            colonies_transfer_to_unit(ctx->colonies, c->id, ctx->units, ship->id, ct, amt) >
              0) {
          loaded = 1;
        }
      }
      for (size_t li = 0; !loaded && li < n_ship; ++li) {
        const int ct = k_ship_load[li];
        if ((c->cargo_produced_mask & (uint16_t)(1u << ct)) == 0) {
          continue;
        }
        if (!ai_euro_colony_haul_cargo_surplus(c, ct)) {
          continue;
        }
        const int amt = ai_euro_haul_load_amount(c, ct);
        if (amt > 0 &&
            colonies_transfer_to_unit(ctx->colonies, c->id, ctx->units, ship->id, ct, amt) >
              0) {
          loaded = 1;
        }
      }
      for (size_t li = 0; !loaded && li < n_ship; ++li) {
        const int ct = k_ship_load[li];
        if (!ai_euro_colony_haul_cargo_surplus(c, ct)) {
          continue;
        }
        const int amt = ai_euro_haul_load_amount(c, ct);
        if (amt > 0) {
          (void)colonies_transfer_to_unit(
            ctx->colonies, c->id, ctx->units, ship->id, ct, amt
          );
          break;
        }
      }
      break;
    }
  }

  int cx = 0;
  int cy = 0;
  /* Thin 4393 work-queue haul peel before peace colony-sail short pick. */
  if (!ai_euro_4393_work_queue_haul_pick(
        ctx, nation_id, ship->x, ship->y, ship, &cx, &cy
      ) &&
      !ai_euro_nearest_short_coastal_colony(ctx, nation_id, ship->x, ship->y, &cx, &cy)) {
    return 0;
  }
  int wx = 0;
  int wy = 0;
  if (!ai_euro_coastal_water_near(ctx->map, cx, cy, ship->x, ship->y, &wx, &wy)) {
    return 0;
  }
  if (ship->x == wx && ship->y == wy) {
    return 1; /* already at haul berth */
  }
  if (units_orders_follow_goto(ship->orders) && ship->goto_x == wx && ship->goto_y == wy) {
    return 1;
  }
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, wx, wy);
  return 1;
}

/*
 * Peace Europe export sail (thin mid-5d04): Caravel/Merchantman loads
 * FUN_364b_0636-eligible surplus (stock>99 → leave 50) at coastal own colony,
 * then AI_SAIL Europe for existing dump-sell. Complements colony-supply haul /
 * de Witt TRADE_GOODS. Cite: FUN_364b_0688 / 0636; europe_cargo_export_eligible;
 * Colonization.pdf Europe buy/sell; euro_unit_act §2d2. No invented rates.
 */
static int ai_euro_ship_holds_export_goods(const ColonizeUnitPool* units, const ColonizeUnit* ship) {
  if (!units || !ship) {
    return 0;
  }
  const int n = units_goods_hold_count(units, ship->id);
  for (int h = 0; h < n; ++h) {
    if (ship->hold_goods_amount[h] <= 0 || ship->hold_goods_amount[h] >= 255) {
      continue;
    }
    if (europe_cargo_export_eligible(ship->hold_goods_type[h])) {
      return 1;
    }
  }
  return 0;
}

static int ai_euro_try_ship_europe_export(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ship || !ship->active) {
    return 0;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, ship);
  if (!ai_euro_is_cargo_ship_name(name)) {
    return 0;
  }
  if (units_goods_hold_count(ctx->units, ship->id) <= 0) {
    return 0;
  }

  /* Prefer SILVER then other export-eligible cargos (FUN_364b_0636). */
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, ship);
  if (has_cap && !ai_euro_ship_holds_export_goods(ctx->units, ship)) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!ai_euro_tiles_near(ship->x, ship->y, c->x, c->y)) {
        continue;
      }
      static const int k_prefer[] = {
        COLONIZE_CARGO_SILVER,
        COLONIZE_CARGO_SUGAR,
        COLONIZE_CARGO_TOBACCO,
        COLONIZE_CARGO_COTTON,
        COLONIZE_CARGO_FURS,
        COLONIZE_CARGO_ORE,
        COLONIZE_CARGO_RUM,
        COLONIZE_CARGO_CIGARS,
        COLONIZE_CARGO_CLOTH,
        COLONIZE_CARGO_COATS,
        COLONIZE_CARGO_TRADE_GOODS
      };
      for (size_t pi = 0; pi < sizeof(k_prefer) / sizeof(k_prefer[0]); ++pi) {
        const int ct = k_prefer[pi];
        if (!europe_cargo_export_eligible(ct)) {
          continue;
        }
        /* FUN_364b_0688: stock>99 → sell/leave 50; load the excess. */
        if (c->stock[ct] <= 99) {
          continue;
        }
        const int amt = c->stock[ct] - 50;
        if (amt <= 0) {
          continue;
        }
        if (colonies_transfer_to_unit(ctx->colonies, c->id, ctx->units, ship->id, ct, amt) > 0) {
          break;
        }
      }
      break;
    }
  }

  if (!ai_euro_ship_holds_export_goods(ctx->units, ship)) {
    return 0;
  }
  int ex = 0;
  int ey = 0;
  if (!ai_euro_europe_sail_target(ctx, ship->x, ship->y, &ex, &ey)) {
    return 0;
  }
  if (ship->x == ex && ship->y == ey) {
    return 0;
  }
  if (units_orders_follow_goto(ship->orders) && ship->goto_x == ex && ship->goto_y == ey) {
    return 1;
  }
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, ex, ey);
  return 1;
}

/*
 * War cargo colony-sail (3558 thin, needs −0x6790 stance ≠ 0): ship with
 * muskets/horses or military pax sails to best fortified own coastal colony.
 * Cite: move_scoring_ship.md war cargo; euro_ocean_scoring.c.
 */
static int ai_euro_try_ship_war_cargo_sail(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ship || !ship->active) {
    return 0;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return 0;
  }
  ai_euro_refresh_continent_stance(ctx, nation_id);
  int any_stance = 0;
  for (int cid = 0; cid <= 15; ++cid) {
    if (ai_euro_continent_stance_at(nation_id, cid) != 0) {
      any_stance = 1;
      break;
    }
  }
  if (!any_stance) {
    return 0;
  }
  const int has_muskets =
    ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_MUSKETS);
  const int has_horses =
    ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_HORSES);
  int has_mil_pax = 0;
  for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
    const ColonizeUnit* pax = units_get_const(ctx->units, ship->cargo_ids[c]);
    if (!pax || !pax->active) {
      continue;
    }
    if (ai_euro_is_military_name(units_display_name(ctx->units, pax))) {
      has_mil_pax = 1;
      break;
    }
  }
  if (!has_muskets && !has_horses && !has_mil_pax) {
    return 0;
  }
  int best_score = -999999;
  int bx = 0;
  int by = 0;
  int have = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    const int cid = map_continent_id_at(ctx->map, c->x, c->y);
    if (ai_euro_continent_stance_at(nation_id, cid) == 0) {
      continue;
    }
    const int score =
      ai_euro_ocean_colony_sail_score(ctx, nation_id, ship->x, ship->y, c, /*at_war=*/1);
    if (!have || score > best_score) {
      have = 1;
      best_score = score;
      bx = c->x;
      by = c->y;
    }
  }
  if (!have) {
    return 0;
  }
  int wx = 0;
  int wy = 0;
  if (!ai_euro_coastal_water_near(ctx->map, bx, by, ship->x, ship->y, &wx, &wy)) {
    return 0;
  }
  if (ship->x == wx && ship->y == wy) {
    return 1;
  }
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, wx, wy);
  return 1;
}

/*
 * Peace Privateer loot sail: already carrying FUN_364b-eligible goods → AI_SAIL
 * Europe for dump-sell (no colony load — commerce-raid loot). Cite: Privateer
 * Europe sell; europe_cargo_export_eligible; euro_unit_act §2d2 dump-sell.
 * Complements cargo-ship Europe export (colony surplus load).
 */
static int ai_euro_try_privateer_europe_loot_sail(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ship || !ship->active) {
    return 0;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, ship);
  if (!name || strstr(name, "Privateer") == NULL) {
    return 0;
  }
  (void)nation_id;
  if (!ai_euro_ship_holds_export_goods(ctx->units, ship)) {
    return 0;
  }
  int ex = 0;
  int ey = 0;
  if (!ai_euro_europe_sail_target(ctx, ship->x, ship->y, &ex, &ey)) {
    return 0;
  }
  if (ship->x == ex && ship->y == ey) {
    return 0;
  }
  if (units_orders_follow_goto(ship->orders) && ship->goto_x == ex && ship->goto_y == ey) {
    return 1;
  }
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, ex, ey);
  return 1;
}

/*
 * Jan de Witt ship trade: on foreign Euro colony dock (de Witt enter), load
 * TRADE_GOODS surplus; with TRADE_GOODS aboard → AI_SAIL Europe (sell via
 * ai_euro_try_transport_europe_sell); else AI_SAIL toward coastal water by
 * nearest peaceful foreign with TRADE_GOODS≥20. Cite: fandom Jan de Witt;
 * units_can_enter dock; colonies_de_witt_transfer_*; §2d2 haul pattern.
 */
static int ai_euro_try_de_witt_ship_trade(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ctx->col1_ok || !ctx->col1 ||
      !ship || !ship->active) {
    return 0;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  if (!founding_fathers_de_witt_allows_foreign_colony_trade(ctx->col1, nation_id)) {
    return 0;
  }
  const char* name = units_display_name(ctx->units, ship);
  if (!ai_euro_is_cargo_ship_name(name)) {
    return 0;
  }
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, ship);
  const int held_tg = ai_euro_unit_trade_goods_held(ctx->units, ship);
  const int cid = colonies_id_at(ctx->colonies, ship->x, ship->y);
  if (cid >= 0) {
    ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
    if (c && c->active && c->nation_id >= 0 && c->nation_id <= 3 &&
        c->nation_id != nation_id && !ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
      if (has_cap && ai_euro_de_witt_trade_goods_surplus(c)) {
        const int moved = colonies_de_witt_transfer_from_colony(
          ctx->colonies, cid, ctx->units, ship->id, COLONIZE_CARGO_TRADE_GOODS, 10, ctx->col1
        );
        if (moved > 0) {
          return 1;
        }
      }
      return 0;
    }
  }
  /* Carrying TRADE_GOODS → sail Europe for dump-sell (existing harbor path). */
  if (held_tg > 0 && (!has_cap || held_tg >= 10)) {
    int ex = 0;
    int ey = 0;
    if (ai_euro_europe_sail_target(ctx, ship->x, ship->y, &ex, &ey)) {
      if (ship->x == ex && ship->y == ey) {
        return 0;
      }
      if (units_orders_follow_goto(ship->orders) && ship->goto_x == ex && ship->goto_y == ey) {
        return 1;
      }
      ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, ex, ey);
      return 1;
    }
  }
  if (!has_cap) {
    return 0;
  }
  int cx = 0;
  int cy = 0;
  if (!ai_euro_nearest_de_witt_foreign_trade(ctx, nation_id, ship->x, ship->y, &cx, &cy)) {
    return 0;
  }
  int wx = 0;
  int wy = 0;
  if (!ai_euro_coastal_water_near(ctx->map, cx, cy, ship->x, ship->y, &wx, &wy)) {
    /* No adjacent water mapped — aim colony dock tile (de Witt enter). */
    wx = cx;
    wy = cy;
  }
  if (ship->x == wx && ship->y == wy) {
    return 1;
  }
  if (units_orders_follow_goto(ship->orders) && ship->goto_x == wx && ship->goto_y == wy) {
    return 1;
  }
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, wx, wy);
  return 1;
}

/* Galleon / Frigate / Man-O-War — war passenger transport (Europe purchase +
 * Jones Frigate/MoW fallback; king MoW). Cite: euro_unit_act §2b2; king_ref
 * MoW cargo; founding_fathers John Paul Jones. */
static int ai_euro_is_war_transport_name(const char* name) {
  return name &&
         (strstr(name, "Galleon") != NULL || strstr(name, "Frigate") != NULL ||
          strstr(name, "Man-O-War") != NULL || strstr(name, "Man-o-War") != NULL);
}

/*
 * Own coastal colony threatened by a war-peer land/sea unit within MD≤3.
 * Cite: Colonization.pdf naval transport / fortify defense — troop ships sail
 * to threatened ports. Structural proximity only (no invented combat bonus).
 */
static int ai_euro_colony_threatened_by_war(
  ColonizeTurnContext* ctx,
  int nation_id,
  const ColonizeColony* c
) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !c || !c->active) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* f = &ctx->units->units[i];
    if (!f->active || f->nation_id == nation_id || f->nation_id < 0 || f->nation_id > 3) {
      continue;
    }
    if (!ai_diplo_at_war(ctx->col1, nation_id, f->nation_id)) {
      continue;
    }
    if (ai_euro_in_europe(f->x, f->y)) {
      continue;
    }
    if (abs(f->x - c->x) + abs(f->y - c->y) <= 3) {
      return 1;
    }
  }
  return 0;
}

/*
 * Peacetime sticky mil-unload threat: Indian land unit (Brave stand-in) within
 * MD≤3 of colony. Cite: move_scoring_ship.md peacetime −0x6790==4; Series L.
 */
static int ai_euro_colony_threatened_by_brave(
  ColonizeTurnContext* ctx,
  const ColonizeColony* c
) {
  if (!ctx || !ctx->units || !c || !c->active) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* f = &ctx->units->units[i];
    if (!f->active || f->nation_id < 4 || f->nation_id > 11) {
      continue;
    }
    if (units_is_sea(ctx->units, f->id)) {
      continue;
    }
    if (abs(f->x - c->x) + abs(f->y - c->y) <= 3) {
      return 1;
    }
  }
  return 0;
}

/*
 * War / peacetime-sticky mil unload: ship with military cargo adjacent to own
 * threatened coastal colony → unload one passenger onto the colony tile.
 * Prefer Soldier, else Regular/Continental Army, else Dragoon/Continental
 * Cavalry, else Artillery/Cannon — mirror king MoW unload ladder + board list.
 *
 * Gate (Series I + L / local_9c 0x10-shaped):
 *   - refresh −0x6790; stance==0 skips
 *   - Euro×Euro at war: war-peer MD≤3 threat; prefer stance 4, allow other ≠0
 *   - else sticky≥2 + stance==4: Indian Brave MD≤3 threat (peacetime mil path)
 *
 * Cite: move_scoring_ship.md peacetime −0x6790==4; Series L. Returns 1 if a
 * military passenger was unloaded.
 */
static int ai_euro_try_unload_military_threatened(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !ship || !ship->active) {
    return 0;
  }
  if (!ai_euro_is_ship_type(ctx->units, ship->id) || ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1) {
    return 0;
  }
  const int at_war = ai_euro_at_war_any_peer(ctx->col1, nation_id);
  const int sticky = ai_diplo_indian_hostility_sticky(ctx->col1, nation_id);
  if (!at_war && sticky < 2) {
    return 0;
  }
  if (ship->cargo_count <= 0) {
    return 0;
  }
  ai_euro_refresh_continent_stance(ctx, nation_id);
  /* Prefer Soldier > Regular/Cont.Army > Dragoon/Cont.Cav > Artillery. */
  int pax_id = -1;
  int pax_rank = 0; /* 4=Soldier, 3=Regular/Army, 2=Dragoon/Cav, 1=Artillery */
  for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[c]);
    if (!p || !p->active) {
      continue;
    }
    const char* pname = units_display_name(ctx->units, p);
    int rank = 0;
    if (pname && strstr(pname, "Soldier") != NULL) {
      rank = 4;
    } else if (
      pname &&
      (strstr(pname, "Regular") != NULL ||
       (strstr(pname, "Continental") != NULL && strstr(pname, "Cavalry") == NULL))) {
      rank = 3;
    } else if (
      pname && (strstr(pname, "Dragoon") != NULL || strstr(pname, "Cavalry") != NULL)) {
      rank = 2;
    } else if (pname && ai_euro_is_artillery_name(pname)) {
      rank = 1;
    }
    if (rank > pax_rank) {
      pax_rank = rank;
      pax_id = ship->cargo_ids[c];
      if (rank >= 4) {
        break;
      }
    }
  }
  if (pax_id < 0) {
    return 0;
  }
  /* Adjacent/same-tile own coastal colony; war-peer or Brave threat; stance gate. */
  int dest_x = -1;
  int dest_y = -1;
  int any_x = -1;
  int any_y = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* col = &ctx->colonies->colonies[i];
    if (!col->active || col->nation_id != nation_id) {
      continue;
    }
    if (!map_tile_is_coastal(ctx->map, col->x, col->y)) {
      continue;
    }
    if (!ai_euro_tiles_near(ship->x, ship->y, col->x, col->y)) {
      continue;
    }
    const int threatened =
      at_war ? ai_euro_colony_threatened_by_war(ctx, nation_id, col)
             : ai_euro_colony_threatened_by_brave(ctx, col);
    if (!threatened) {
      continue;
    }
    const int cid = map_continent_id_at(ctx->map, col->x, col->y);
    const int st = ai_euro_continent_stance_at(nation_id, cid);
    if (st == 0) {
      continue; /* −0x6790 none: no mil unload bit */
    }
    /* Peacetime sticky path: require military nibble 4 (not expand/develop). */
    if (!at_war && st != 4) {
      continue;
    }
    if (any_x < 0) {
      any_x = col->x;
      any_y = col->y;
    }
    if (st == 4 && dest_x < 0) {
      dest_x = col->x;
      dest_y = col->y;
    }
  }
  if (dest_x < 0) {
    dest_x = any_x;
    dest_y = any_y;
  }
  if (dest_x < 0) {
    return 0;
  }
  if (!units_unload_passenger(
        ctx->units, ship->id, pax_id, ctx->map, dest_x, dest_y, ctx->colonies)) {
    return 0;
  }
  return 1;
}

/*
 * War transport sail target: idle Galleon/Frigate with passenger space prefers
 * coastal water by a threatened own coastal colony; else reuse naval war hunt
 * (foe sea / enemy coast). Cite: euro_unit_act §2b; Colonization.pdf naval
 * transport; Europe Galleon/Frigate purchase. Full 20e6 PARKED.
 */
static int ai_euro_war_transport_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
);

/*
 * Thin naval war hunt (5b66 case 0x0b act-level): nearest enemy sea unit or
 * coastal water by a foreign Euro colony at war. Full 20e6 combat scoring PARKED.
 */
/*
 * True when (x,y) is adjacent ocean under an enemy Fort/Fortress battery
 * (FUN_364b_03f6 / units_coastal_fort_attack_strength). Cite: Marathon8 peel.
 */
static int ai_euro_tile_under_enemy_fort_fire(
  ColonizeTurnContext* ctx,
  int viewer_nation,
  int x,
  int y
) {
  if (!ctx || !ctx->colonies || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->map) {
    return 0;
  }
  if (!map_tile_is_water(ctx->map, x, y)) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id == viewer_nation || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    if (!ai_diplo_at_war(ctx->col1, viewer_nation, c->nation_id)) {
      continue;
    }
    if (units_coastal_fort_attack_strength(ctx->colonies, c, ctx->units) <= 0) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      if (c->x + dx[d] == x && c->y + dy[d] == y) {
        return 1;
      }
    }
  }
  return 0;
}

/*
 * If ship sits under enemy fort fire, step to adjacent safe water (thin flee).
 * Returns 1 if a flee move was attempted. Cite: FUN_364b_03f6 danger zone.
 */
static int ai_euro_naval_try_flee_fort_fire(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active || u->moves_left <= 0) {
    return 0;
  }
  if (!units_is_sea(ctx->units, u->id) || ai_euro_in_europe(u->x, u->y)) {
    return 0;
  }
  if (!ai_euro_tile_under_enemy_fort_fire(ctx, u->nation_id, u->x, u->y)) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int best_d = -1;
  int best_dist = -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    if (!map_tile_is_water(ctx->map, nx, ny)) {
      continue;
    }
    if (ai_euro_tile_under_enemy_fort_fire(ctx, u->nation_id, nx, ny)) {
      continue;
    }
    if (!units_can_enter(ctx->units, u->type_index, ctx->map, nx, ny, u->id, ctx->colonies)) {
      continue;
    }
    /* Prefer step that increases distance from nearest fort colony. */
    int dist = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id == u->nation_id) {
        continue;
      }
      if (units_coastal_fort_attack_strength(ctx->colonies, c, ctx->units) <= 0) {
        continue;
      }
      const int md = abs(c->x - nx) + abs(c->y - ny);
      if (md > dist) {
        dist = md;
      }
    }
    if (best_d < 0 || dist > best_dist) {
      best_d = d;
      best_dist = dist;
    }
  }
  if (best_d < 0) {
    return 0;
  }
  const int tx = u->x + dx[best_d];
  const int ty = u->y + dy[best_d];
  if (units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng)) {
    return 1;
  }
  return 0;
}

static int ai_euro_naval_war_hunt_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1 || !out_x || !out_y) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* f = &ctx->units->units[i];
    if (!f->active || f->nation_id == nation_id || f->nation_id < 0 || f->nation_id > 3) {
      continue;
    }
    if (!units_is_sea(ctx->units, f->id) || ai_euro_in_europe(f->x, f->y)) {
      continue;
    }
    if (!ai_diplo_at_war(ctx->col1, nation_id, f->nation_id)) {
      continue;
    }
    /* Skip foe parked under coastal fort batteries (FUN_364b_03f6). */
    if (ai_euro_tile_under_enemy_fort_fire(ctx, nation_id, f->x, f->y)) {
      continue;
    }
    const int dist = abs(f->x - from_x) + abs(f->y - from_y);
    if (best < 0 || dist < best) {
      best = dist;
      bx = f->x;
      by = f->y;
    }
  }

  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id == nation_id || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      if (!ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
        continue;
      }
      int wx = 0;
      int wy = 0;
      if (!ai_euro_coastal_water_near(ctx->map, c->x, c->y, from_x, from_y, &wx, &wy)) {
        continue;
      }
      if (ai_euro_tile_under_enemy_fort_fire(ctx, nation_id, wx, wy)) {
        continue;
      }
      const int dist = abs(wx - from_x) + abs(wy - from_y);
      if (best < 0 || dist < best) {
        best = dist;
        bx = wx;
        by = wy;
      }
    }
  }

  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

static int ai_euro_war_transport_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->map || !out_x || !out_y) {
    return 0;
  }
  /* Prefer threatened own coastal colony water (troop lift / reinforce). */
  if (ctx->colonies) {
    int best = -1;
    int bx = 0;
    int by = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
        continue;
      }
      if (!ai_euro_colony_threatened_by_war(ctx, nation_id, c)) {
        continue;
      }
      int wx = 0;
      int wy = 0;
      if (!ai_euro_coastal_water_near(ctx->map, c->x, c->y, from_x, from_y, &wx, &wy)) {
        continue;
      }
      const int dist = abs(wx - from_x) + abs(wy - from_y);
      if (best < 0 || dist < best) {
        best = dist;
        bx = wx;
        by = wy;
      }
    }
    if (best >= 0) {
      *out_x = bx;
      *out_y = by;
      return 1;
    }
  }
  /* No threatened own port — enemy coast / foe sea (existing hunt). */
  return ai_euro_naval_war_hunt_target(ctx, nation_id, from_x, from_y, out_x, out_y);
}

/*
 * Effective defense for thin 20e6 naval adjacent-foe pick.
 * Shared FUN_157e_004a via combat_unit_base_x8 (damage/holds/Drake).
 */
static int ai_euro_naval_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f
) {
  if (!units || !f) {
    return 9999;
  }
  ColonizeCombatStrengthCtx sctx;
  sctx.units = units;
  sctx.map = ctx ? ctx->map : NULL;
  sctx.colonies = ctx ? ctx->colonies : NULL;
  sctx.col1 = (ctx && ctx->col1_ok) ? ctx->col1 : NULL;
  const int tough = combat_unit_base_x8(&sctx, f->id, 0, NULL);
  return tough > 0 ? tough : 0;
}

/*
 * FUN_5bfb_312e (naval ambush power — speed/maneuverability, not combat
 * strength): movement points ×3-scaled (DOS internal unit-type table
 * convention; Linux `movement` is the real, non-scaled value) + 3, ×2 for
 * Man-O-War, +3 for Frigate, −4×cargo held, floor 1. Cite:
 * euro_diplo_3180_full.md.
 */
static int ai_euro_naval_ambush_power(ColonizeTurnContext* ctx, const ColonizeUnit* u) {
  if (!ctx || !ctx->units || !u) {
    return 1;
  }
  const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
  if (!t) {
    return 1;
  }
  const char* name = units_display_name(ctx->units, u);
  int power = t->movement * 3 + 3;
  if (ai_euro_type_is_man_o_war_name(name)) {
    power *= 2;
  } else if (name && strstr(name, "Frigate") != NULL) {
    power += 3;
  }
  power -= 4 * u->cargo_count;
  return power < 1 ? 1 : power;
}

/*
 * FUN_5bfb_3180 naval ambush (thin — the surrounding encounter/war-declare
 * dispatch is PARKED, see euro_diplo_3180_full.md; this ports only the
 * self-contained, non-destructive ambush sub-mechanic). Own type must be
 * Man-O-War/Frigate/Privateer (DOS gate on the *encountering* unit, not
 * the foe); scans 8 neighbors for the first foreign-nation ship; rolls
 * `dos_rng_range(1, self_power + foe_power + 2)` — if the roll exceeds
 * self_power, this unit got ambushed and loses `moves_left` down to the
 * ambush-power amount (DOS adds to a "spent" counter; Linux tracks moves
 * as a countdown, so the faithful mirror is draining `moves_left` by the
 * same amount, floored at 0). No combat/damage — matches DOS exactly
 * (unit+0x3149 MP-spend byte, not a health/strength field).
 * Approximated: DOS's exact RNG call (`FUN_281f_04d4`) and the partial-
 * drain-on-tie case (`local_2c >> 1`) are not reproduced byte-for-byte —
 * tie rounds down to "no ambush" here instead of a half-drain.
 */
static void ai_euro_naval_try_ambush(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active || u->moves_left <= 0) {
    return;
  }
  if (!units_is_sea(ctx->units, u->id)) {
    return;
  }
  const char* uname = units_display_name(ctx->units, u);
  if (!uname || (!ai_euro_type_is_man_o_war_name(uname) &&
                 strstr(uname, "Frigate") == NULL && strstr(uname, "Privateer") == NULL)) {
    return;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int foe_id = -1;
  for (int d = 0; d < 8 && foe_id < 0; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    const int fid = units_id_at(ctx->units, nx, ny);
    if (fid < 0 || fid == u->id) {
      continue;
    }
    const ColonizeUnit* f = units_get_const(ctx->units, fid);
    if (!f || !f->active || f->nation_id == u->nation_id || !units_is_sea(ctx->units, fid)) {
      continue;
    }
    foe_id = fid;
  }
  if (foe_id < 0) {
    return;
  }
  const ColonizeUnit* foe = units_get_const(ctx->units, foe_id);
  const int self_power = ai_euro_naval_ambush_power(ctx, u);
  const int foe_power = ai_euro_naval_ambush_power(ctx, foe);
  if (!ctx->rng) {
    return;
  }
  const int roll = dos_rng_range(ctx->rng, 1, self_power + foe_power + 2);
  if (roll <= self_power) {
    return; /* no ambush */
  }
  /* Drain amount is the type-based constant (4/6/8), not the roll/power —
   * DOS's `local_2c`, set once by the own-type gate above, separate from
   * `local_6`/self_power used only for the roll comparison. */
  int type_drain = 4;
  if (ai_euro_type_is_man_o_war_name(uname)) {
    type_drain = 4;
  } else if (strstr(uname, "Frigate") != NULL) {
    type_drain = 6;
  } else {
    type_drain = 8; /* Privateer */
  }
  const int drain = type_drain < u->moves_left ? type_drain : u->moves_left;
  u->moves_left -= drain;
  if (u->moves_left < 0) {
    u->moves_left = 0;
  }
}

/* Combat ships for Frigate hunt prefer (complement Privateer cargo prey). */
static int ai_euro_is_warship_name(const char* name) {
  if (!name || ai_euro_is_cargo_ship_name(name)) {
    return 0;
  }
  return strstr(name, "Frigate") != NULL || strstr(name, "Privateer") != NULL ||
         strstr(name, "Galleon") != NULL || strstr(name, "Man-O-War") != NULL ||
         strstr(name, "Man-o-War") != NULL || strstr(name, "Man O War") != NULL;
}

/*
 * Best adjacent war foe for naval attack (thin 20e6 naval combat scoring):
 * Privateer → prefer Merchantman/Caravel cargo prey over warships; Frigate →
 * prefer warships (Frigate/Privateer/Galleon/Man-O-War) over cargo (complement);
 * else lower effective defense (incl. Drake Privateer +50%). Cite: euro_unit_act
 * §2f; Europe Privateer/Frigate purchase; FUN_157e_004a; fandom Drake.
 */
static int ai_euro_naval_best_adjacent_foe(ColonizeTurnContext* ctx, const ColonizeUnit* u) {
  if (!ctx || !ctx->units || !u || !u->active || !units_is_sea(ctx->units, u->id)) {
    return -1;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  const char* own_name = units_display_name(ctx->units, u);
  const int prefer_cargo = own_name && strstr(own_name, "Privateer") != NULL;
  const int prefer_war = own_name && strstr(own_name, "Frigate") != NULL;
  int best_id = -1;
  int best_tough = 0;
  int best_rank = 0; /* prey rank: cargo (Privateer) or warship (Frigate) */
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe < 0 || !units_is_sea(ctx->units, foe)) {
      continue;
    }
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f || f->nation_id == u->nation_id) {
      continue;
    }
    if (ctx->col1_ok && ctx->col1 && f->nation_id >= 0 && f->nation_id < 4 &&
        !ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      continue;
    }
    const char* fname = units_display_name(ctx->units, f);
    const int tough = ai_euro_naval_foe_toughness(ctx, ctx->units, f);
    int rank = 0;
    if (prefer_cargo) {
      rank = ai_euro_is_cargo_ship_name(fname) ? 1 : 0;
    } else if (prefer_war) {
      rank = ai_euro_is_warship_name(fname) ? 1 : 0;
    }
    if (best_id < 0 || rank > best_rank || (rank == best_rank && tough < best_tough)) {
      best_id = foe;
      best_tough = tough;
      best_rank = rank;
    }
  }
  return best_id;
}

/* Attack adjacent enemy sea unit while at war (prefer weaker foe; try_move cannot). */
static void ai_euro_naval_try_adjacent_attack(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  const int foe = ai_euro_naval_best_adjacent_foe(ctx, u);
  if (foe < 0) {
    return;
  }
  const ColonizeUnit* f = units_get_const(ctx->units, foe);
  if (!f) {
    return;
  }
  ai_euro_try_attack(ctx, u, f->x, f->y);
}

/* True when land unit already has a non-stationary AI/goto course. */
static int ai_euro_land_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map) {
  if (!u || !map || !units_orders_follow_goto(u->orders)) {
    return 0;
  }
  if (u->goto_x < 0 || u->goto_y < 0 || u->goto_x >= UNITS_GOTO_NONE ||
      u->goto_y >= UNITS_GOTO_NONE || u->goto_x >= map->width || u->goto_y >= map->height) {
    return 0;
  }
  return u->goto_x != u->x || u->goto_y != u->y;
}

/*
 * Thin land war hunt (5b66 case 0x0b act-level): nearest enemy land unit or
 * foreign Euro colony at war, or native Brave / tribe when Indian×Euro at war.
 * Prefer capital tribe tiles (tie-break closer MD) — Cortes rich_capital path.
 * When prefer_fortified (Artillery siege): foreign Euro Stockade/Fort/Fortress
 * colonies beat open ones (MD slack ≤3 vs nearest open). When prefer_open
 * (Dragoon/Soldier): open colonies beat fortified (same slack). Non-siege unit
 * hunt: Treasure beats non-Treasure, then lower toughness, within MD slack ≤3
 * (loot / thin 20e6). Cite: king_ref Artillery siege / Dragoon open bias;
 * Colonization.pdf Treasure Trains / Defending a Colony. Full 20e6 PARKED.
 */
static int ai_euro_land_war_hunt_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int prefer_fortified,
  int prefer_open,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->col1_ok || !ctx->col1 || !out_x || !out_y) {
    return 0;
  }
  int best = -1;
  int best_cap = 0;
  int best_fort = prefer_open ? 9999 : -1;
  int best_treasure = 0;
  int best_tough = 0;
  int bx = 0;
  int by = 0;

  if (!prefer_fortified) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* f = &ctx->units->units[i];
      if (!f->active || f->nation_id == nation_id || !units_is_on_map(f) ||
          units_is_sea(ctx->units, f->id) || ai_euro_in_europe(f->x, f->y)) {
        continue;
      }
      if (f->nation_id >= 0 && f->nation_id <= 3) {
        if (!ai_diplo_at_war(ctx->col1, nation_id, f->nation_id)) {
          continue;
        }
      } else if (f->nation_id >= 4 && f->nation_id <= 11) {
        if (!ai_diplo_indian_at_war(ctx->col1, nation_id, f->nation_id - 4)) {
          continue;
        }
      } else {
        continue;
      }
      const int dist = abs(f->x - from_x) + abs(f->y - from_y);
      const int treasure = ai_euro_is_treasure_name(units_display_name(ctx->units, f));
      const int tough = ai_euro_land_foe_toughness(ctx, ctx->units, f);
      if (prefer_open && f->nation_id >= 0 && f->nation_id <= 3) {
        const int fb = ai_euro_colony_fort_bonus_at(ctx->colonies, f->x, f->y, f->nation_id);
        if (best < 0 || fb < best_fort || (fb == best_fort && dist < best)) {
          best = dist;
          best_fort = fb;
          best_cap = 0;
          best_treasure = treasure;
          best_tough = tough;
          bx = f->x;
          by = f->y;
        } else if (fb == 0 && best_fort > 0 && dist <= best + 3) {
          best = dist;
          best_fort = 0;
          best_cap = 0;
          best_treasure = treasure;
          best_tough = tough;
          bx = f->x;
          by = f->y;
        }
      } else {
        /* Treasure > toughness > distance; MD slack ≤3 for treasure/toughness. */
        int better = 0;
        if (best < 0) {
          better = 1;
        } else if (treasure != best_treasure) {
          if (treasure && dist <= best + 3) {
            better = 1;
          } else if (!treasure && dist + 3 < best) {
            better = 1;
          }
        } else if (tough != best_tough) {
          if (tough < best_tough && dist <= best + 3) {
            better = 1;
          } else if (tough > best_tough && dist + 3 < best) {
            better = 1;
          }
        } else if (dist < best) {
          better = 1;
        }
        if (better) {
          best = dist;
          best_cap = 0;
          best_treasure = treasure;
          best_tough = tough;
          bx = f->x;
          by = f->y;
        }
      }
    }
  }

  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id == nation_id || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      if (!ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
        continue;
      }
      const int dist = abs(c->x - from_x) + abs(c->y - from_y);
      const int fb = colonies_fortification_defense_bonus_percent(ctx->colonies, c);
      if (prefer_fortified) {
        if (best < 0 || fb > best_fort || (fb == best_fort && dist < best)) {
          best = dist;
          best_fort = fb;
          best_cap = 0;
          bx = c->x;
          by = c->y;
        } else if (fb > 0 && best_fort <= 0 && dist <= best + 3) {
          best = dist;
          best_fort = fb;
          best_cap = 0;
          bx = c->x;
          by = c->y;
        }
      } else if (prefer_open) {
        if (best < 0 || fb < best_fort || (fb == best_fort && dist < best)) {
          best = dist;
          best_fort = fb;
          best_cap = 0;
          bx = c->x;
          by = c->y;
        } else if (fb == 0 && best_fort > 0 && dist <= best + 3) {
          best = dist;
          best_fort = 0;
          best_cap = 0;
          bx = c->x;
          by = c->y;
        }
      } else if (best < 0 || dist < best) {
        best = dist;
        best_cap = 0;
        bx = c->x;
        by = c->y;
      }
    }
  }

  if (!prefer_fortified && ctx->col1->tribe) {
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      if (t->nation_id < 4 || t->nation_id > 11) {
        continue;
      }
      if (!ai_diplo_indian_at_war(ctx->col1, nation_id, (int)t->nation_id - 4)) {
        continue;
      }
      const int cap = t->state.capital ? 1 : 0;
      const int dist = abs((int)t->x - from_x) + abs((int)t->y - from_y);
      if (best < 0 || cap > best_cap || (cap == best_cap && dist < best)) {
        best = dist;
        best_cap = cap;
        bx = (int)t->x;
        by = (int)t->y;
      }
    }
  }

  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * Effective defense for thin 20e6 adjacent-foe pick.
 * Shared FUN_157e_015e via combat_engagement_strength (colony/village/terrain/
 * fortify + vet). Cite: combat_strength.c; FUN_157e_004a / 015e.
 */
static int ai_euro_land_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f
) {
  if (!units || !f) {
    return 9999;
  }
  ColonizeCombatStrengthCtx sctx;
  sctx.units = units;
  sctx.map = ctx ? ctx->map : NULL;
  sctx.colonies = ctx ? ctx->colonies : NULL;
  sctx.col1 = (ctx && ctx->col1_ok) ? ctx->col1 : NULL;
  const int tough = combat_unit_toughness(&sctx, f->id, -1);
  return tough > 0 ? tough : 0;
}

/*
 * Best adjacent war foe for land attack (thin 20e6 combat scoring): prefer
 * lower effective defense / non-fortified / weaker colony fort / non-veteran.
 * Artillery prefers higher fort % (siege — king_ref Artillery adjacent-fort).
 * Non-siege: at equal toughness prefer Treasure (loot — Colonization.pdf
 * Treasure Trains / @LOOTCASH). Returns foe unit id or -1.
 *
 * PARK: full FUN_521d_20e6 −0x6790 / explore-ring matrix. Structured: adjacent
 * toughness + Treasure prefer + settlement prefer + siege Artillery fort %;
 * score_move settlement-tile / relative-strength peels Done thin.
 */
static int ai_euro_land_best_adjacent_foe(ColonizeTurnContext* ctx, const ColonizeUnit* u) {
  if (!ctx || !ctx->units || !u || !u->active || units_is_sea(ctx->units, u->id)) {
    return -1;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  const char* uname = units_display_name(ctx->units, u);
  const int siege = ai_euro_is_artillery_name(uname);
  int best_id = -1;
  int best_tough = 0;
  int best_fort = -1;
  int best_treasure = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe < 0 || units_is_sea(ctx->units, foe)) {
      continue;
    }
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f || f->nation_id == u->nation_id) {
      continue;
    }
    if (ctx->col1_ok && ctx->col1) {
      if (f->nation_id >= 0 && f->nation_id < 4) {
        if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
          continue;
        }
      } else if (f->nation_id >= 4 && f->nation_id <= 11) {
        if (!ai_diplo_indian_at_war(ctx->col1, u->nation_id, f->nation_id - 4)) {
          continue;
        }
      } else {
        continue;
      }
    }
    const int fort =
      (f->nation_id >= 0 && f->nation_id <= 3)
        ? ai_euro_colony_fort_bonus_at(ctx->colonies, f->x, f->y, f->nation_id)
        : 0;
    const int tough = ai_euro_land_foe_toughness(ctx, ctx->units, f);
    const int treasure = ai_euro_is_treasure_name(units_display_name(ctx->units, f));
    /* Prefer foes already on a foreign Euro settlement (0x46 settlement scan). */
    const int on_settle =
      (f->nation_id >= 0 && f->nation_id <= 3 && ctx->colonies &&
       colonies_id_at(ctx->colonies, f->x, f->y) >= 0)
        ? 1
        : 0;
    if (siege) {
      if (best_id < 0 || fort > best_fort || (fort == best_fort && tough < best_tough)) {
        best_id = foe;
        best_fort = fort;
        best_tough = tough;
        best_treasure = treasure;
      }
    } else if (
      best_id < 0 || tough < best_tough ||
      (tough == best_tough && treasure && !best_treasure) ||
      (tough == best_tough && treasure == best_treasure && on_settle)
    ) {
      best_id = foe;
      best_tough = tough;
      best_treasure = treasure;
    }
  }
  return best_id;
}

/*
 * FUN_521d_20e6 `0x46` gate, full port: combat-capable land unit (combat
 * rating >1) adjacent to a foreign Euro colony with **no defender on the
 * tile** walks straight in and seizes it (Colonization capture-by-move —
 * combat only triggers when a defender is actually present, handled
 * separately by ai_euro_land_try_adjacent_attack's on-settlement
 * preference). Decomp scans all 8 neighbors via `FUN_281f_0696`
 * (`euro_settlement_owner`) and stamps orders `0x46` the moment any
 * neighbor is owned by a different, non-crown Euro nation; Linux checks
 * war state too (decomp's world model has no live peacetime seize). One
 * seize per call — re-armed next act pass like the decomp reflex check.
 */
static int ai_euro_land_try_adjacent_colony_seize(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->colonies || !u || !u->active ||
      units_is_sea(ctx->units, u->id) || u->moves_left <= 0) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(ctx->units, u->type_index);
  if (!ut || ut->attack <= 1) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    const int cid = colonies_id_at(ctx->colonies, nx, ny);
    if (cid < 0) {
      continue;
    }
    const ColonizeColony* c = colonies_get(ctx->colonies, cid);
    if (!c || !c->active || c->nation_id == u->nation_id || c->nation_id < 0 ||
        c->nation_id > 3) {
      continue;
    }
    if (ctx->col1_ok && ctx->col1 && !ai_diplo_at_war(ctx->col1, u->nation_id, c->nation_id)) {
      continue;
    }
    if (units_id_at(ctx->units, nx, ny) >= 0) {
      continue; /* defended — leave to ai_euro_land_try_adjacent_attack */
    }
    int plunder = 0;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      if (c->stock[i] > 0) {
        plunder += c->stock[i];
      }
    }
    ColonizeColony snap = *c;
    if (!units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng)) {
      continue;
    }
    if (u->active && u->x == nx && u->y == ny &&
        colonies_capture(ctx->colonies, cid, u->nation_id)) {
      units_combat_notify_colony_captured(
        ctx->col1_ok ? ctx->col1 : NULL, &snap, u->nation_id, plunder
      );
      /*
       * Garrison the prize immediately (Colonization occupying-force
       * convention; king_ref post-capture fortify precedent). Without this,
       * a later outer-wave re-act on the same idle unit standing on its own
       * fresh colony falls into the unrelated "on own colony, no fortify
       * quota -> admit as LABOR" gate (10988-ish) meant for first-colony
       * beachhead escorts, and the conqueror silently disappears into the
       * workforce instead of holding the ground it just took.
       */
      if (u->active) {
        (void)units_order_fortify(ctx->units, u->id);
      }
    }
    return 1;
  }
  return 0;
}

/*
 * FUN_4d56_4528 AI-side village raid, undefended case (2026-08-20, T1.5
 * follow-up): combat-capable land unit at war with a tribe, adjacent to
 * that tribe's own village tile with **no Brave garrison on the tile**,
 * opens hostilities and attacks. Mirrors
 * `ai_euro_land_try_adjacent_colony_seize`'s shape exactly, but for
 * villages instead of colonies — a real gap this port had: AI units could
 * already walk into an *undefended enemy colony* and seize it, but had no
 * equivalent for an undefended *village*, because
 * `ai_euro_land_best_adjacent_foe` only ever returns actual unit
 * occupants (a Brave standing on the tile), never the village tile
 * itself as a target. `units_try_move` already resolves combat against an
 * empty village correctly on its own (synthesizes a temp defender per
 * `FUN_5fef_1b0e`, applies raid fallout, then — unlike a colony capture —
 * the attacker does **not** enter/occupy the tile, matching the human
 * Attack-CHOICE path in `game_loop.c`, `AI_POPUP_TAG_CONTACT_VILLAGE_WARN`
 * handling) — this function's only job is picking the target and opening
 * hostilities, same division of labor as the human path.
 * One raid attempt per call; re-armed next act pass like the colony seize.
 */
static int ai_euro_land_try_adjacent_village_seize(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || !u ||
      !u->active || units_is_sea(ctx->units, u->id) || u->moves_left <= 0) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(ctx->units, u->type_index);
  if (!ut || ut->attack <= 1) {
    return 0;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
      if ((int)t->x != nx || (int)t->y != ny) {
        continue;
      }
      if (t->nation_id < 4 || t->nation_id > 11) {
        break;
      }
      const int indian_nation = (int)t->nation_id;
      if (!ai_diplo_indian_at_war(ctx->col1, u->nation_id, indian_nation - 4)) {
        break;
      }
      if (units_id_at(ctx->units, nx, ny) >= 0) {
        break; /* garrisoned Brave — leave to ai_euro_land_try_adjacent_attack */
      }
      ai_contact_village_open_hostilities(ctx, indian_nation, u->nation_id);
      if (!units_try_move(ctx->units, u->id, ctx->map, nx, ny, ctx->colonies, ctx->rng)) {
        return 0;
      }
      return 1;
    }
  }
  return 0;
}

/*
 * Attack adjacent enemy land unit while at war (prefer weaker foe).
 * Thin multi-step combat: keep fighting while moves remain after enter
 * (MP drained by try_move on win). Cap steps so a failed spend cannot spin.
 * Cite: euro_unit_act §2c / sticky re-hunt; deep 20e6 scoring PARKED.
 */
static void ai_euro_land_try_adjacent_attack(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  for (int step = 0; step < 8 && u && u->active && u->moves_left > 0; ++step) {
    const int foe = ai_euro_land_best_adjacent_foe(ctx, u);
    if (foe < 0) {
      return;
    }
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f) {
      return;
    }
    const int ml0 = u->moves_left;
    const int ax = u->x;
    const int ay = u->y;
    ai_euro_try_attack(ctx, u, f->x, f->y);
    if (!u->active) {
      return;
    }
    /* No progress (lost MP and tile) → stop to avoid infinite retry. */
    if (u->moves_left >= ml0 && u->x == ax && u->y == ay) {
      return;
    }
  }
}

/*
 * Nearest foreign Euro land unit within Manhattan max_md of (from_x,from_y).
 * Peace colony-defense wake (MD≤2 border). Cite: Colonization.pdf fortify
 * defense; euro_unit_act §2d3 peace fortify extend. Returns 1 if found.
 */
static int ai_euro_foreign_land_threat_near(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int max_md,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->units || !out_x || !out_y || max_md < 0) {
    return 0;
  }
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* f = &ctx->units->units[i];
    if (!f->active || f->nation_id == nation_id || f->nation_id < 0 || f->nation_id > 3) {
      continue;
    }
    if (!units_is_on_map(f) || units_is_sea(ctx->units, f->id) || ai_euro_in_europe(f->x, f->y)) {
      continue;
    }
    const int dist = abs(f->x - from_x) + abs(f->y - from_y);
    if (dist > max_md) {
      continue;
    }
    if (best < 0 || dist < best) {
      best = dist;
      bx = f->x;
      by = f->y;
    }
  }
  if (best < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * DS:0x173c / 0x173e continent bitmasks (FUN_521d_0a60 goal producers, raw
 * ~1157/1273 of euro_goal_orders_0a60_full.md). Linux's 0a60 producers are
 * still the thin ai_euro_colony_goals stand-in, so the masks are derived from
 * the live goal table instead: a FOUND-class primary goal on the continent
 * stands in for 0x173e, a MILITARY/MIL_EXPAND one for 0x173c.
 */
static int ai_euro_20e6_goal_on_continent(
  const ColonizeTurnContext* ctx, int nation, int cid, int want_mil
) {
  if (cid < 0) {
    return 0;
  }
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (!g || g->code == AI_GOAL_EMPTY) {
      continue;
    }
    const int is_mil = (g->code == AI_GOAL_MIL_EXPAND || g->code == AI_GOAL_MILITARY);
    if (want_mil ? !is_mil : g->code != AI_GOAL_FOUND) {
      continue;
    }
    if (map_continent_id_at(ctx->map, (int)g->x, (int)g->y) == cid) {
      return 1;
    }
  }
  return 0;
}

/*
 * FUN_281f_08bc (= FUN_1427_0d38) stack-query counts over the ship's cargo
 * (DOS: tile stack — carried units share the ship's tile there; Linux keeps
 * them in cargo_ids, the equivalent set for a ship at sea). Modes ndisasm-
 * confirmed at the LAB_3558 call block (viceroy_overlays.asm 136292-136330,
 * literal PUSHes) against the byte-exact 0d38 case bodies (2026-09-06):
 *   iStack_4a = mode 3 = # Pioneers          — the "flag-count 0x40 founders"
 *     reading is retired; the attack-core "# Pioneers" note was correct.
 *   iStack_48 = mode 4 = # military land types {1,4,6,7,8,9};
 *     at war (DS:0x5382 bit0) += mode 0xc = # Artillery (type 0xb).
 *   iStack_16 = mode 5 = # Scouts            — not Missionary+Scout.
 *   iStack_46 = mode 6 = # {Soldier,Dragoon} + # veteran-professioned
 *     (profession 0x15) others + # types {6..9} (vet-typed counted twice).
 *   iStack_82 = (stack−1) − pioneers − mil(pre-war-add) − scouts = # plain
 *     civilians aboard (the "founder cargo" the colony-sail gate reads).
 */
static void ai_euro_20e6_ship_cargo_counts(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship,
  int* pioneers, int* mil, int* scouts, int* milvet, int* civ
) {
  *pioneers = 0;
  *mil = 0;
  *scouts = 0;
  *milvet = 0;
  int total = 0;
  int artillery = 0;
  for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[s]);
    if (!p || !p->active) {
      continue;
    }
    total++;
    const int t = ai_euro_20e6_dos_type(ctx->units, p);
    if (t == 2) {
      (*pioneers)++;
    }
    if (t == 1 || t == 4 || t == 6 || t == 7 || t == 8 || t == 9) {
      (*mil)++;
    }
    if (t == 5) {
      (*scouts)++;
    }
    if (t == 0xb) {
      artillery++;
    }
    /* mode 6 body: {1,4} or vet profession, then {6..9} again. */
    if (t == 1 || t == 4) {
      (*milvet)++;
    } else if (p->profession == UNITS_JOB_SOLDIER) {
      (*milvet)++;
    }
    if (t >= 6 && t <= 9) {
      (*milvet)++;
    }
  }
  *civ = total - *pioneers - *mil - *scouts; /* iStack_82, pre-war formula */
  if (*civ < 0) {
    *civ = 0;
  }
  const int woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;
  if (woi) {
    *mil += artillery; /* raw 1736-1738: wartime mode-0xc add */
  }
}

/*
 * FUN_521d_20e6 LAB_3558 land-adjacent unload mask (local_9c, raw ~1766-1884
 * of move_scoring_20e6_full.md; euro_ocean_scoring.c section map). Walks the
 * 8 tiles around the ship; each qualifying land tile recomputes the mask from
 * scratch (DOS re-zeroes local_9c per tile — the last qualifying tile wins,
 * replicated verbatim). Bits: 0x40 founder unload (Pioneers + goal-promoted
 * civilians), 0x20 Scout unload, 0x10 military, 0xffff "ship goto lands on
 * this continent — unload all". Cargo counts are the real 0d38 stack-query
 * modes since 2026-09-06 (see ai_euro_20e6_ship_cargo_counts).
 *
 * Substitutions (documented per term):
 *  - DS:0x1734 per-nation urgency accumulator (0a60 threatened-colony count):
 *    count of own colonies flagged NEEDS_GARRISON/NEEDS_MILITARY.
 *  - a654 per-unit goal id for the goal fold: nation-level FOUND/MIL_EXPAND
 *    goal existence (see the fold comment in the body).
 *  - presence bit 0x08 of −0x6a0e: writer undecoded — read as 0.
 *  - DS:0x1740 recall latch (5bfb full-recall event): no Linux producer — 0.
 */
/* Substitute for a654 per-unit goal existence in the raw 1746-1762 fold:
 * any FOUND/MIL_EXPAND primary goal of the nation (0a60 producers live). */
static int ai_euro_20e6_nation_has_settle_goal(int nation) {
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation, i);
    if (g && (g->code == AI_GOAL_FOUND || g->code == AI_GOAL_MIL_EXPAND)) {
      return 1;
    }
  }
  return 0;
}

static int ai_euro_20e6_unload_mask(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation) {
  static const int dx8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  if (!ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  int pioneers = 0;
  int mil = 0;
  int scouts = 0;
  int milvet = 0;
  int civ = 0;
  ai_euro_20e6_ship_cargo_counts(ctx, ship, &pioneers, &mil, &scouts, &milvet, &civ);
  if (pioneers == 0 && mil == 0 && scouts == 0) {
    return 0; /* raw 1768: all three counts zero → no mask pass */
  }
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  const int woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;
  const int open_cont = ai_euro_20e6_open_continents(ctx, nation); /* DS:0x9650 */
  int urgency = 0; /* DS:0x1734[nation] stand-in */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == nation &&
          (c->ai_flags & (COLONIZE_COLONY_AI_NEEDS_GARRISON | COLONIZE_COLONY_AI_NEEDS_MILITARY))) {
        urgency++;
      }
    }
  }
  int home_dist = 0;
  const int home_colony = ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, nation, -1, &home_dist);
  int any_dist = 0;
  (void)ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, -1, -1, &any_dist);
  const int probe7 = ai_goals_max_primary_prio(nation, ship->x, ship->y, AI_GOAL_MIL_EXPAND);
  const int probe1 = ai_goals_max_primary_prio(nation, ship->x, ship->y, AI_GOAL_FOUND);
  /*
   * Raw 1746-1762 goal fold: iStack_68 = a654(unit) per-unit goal id, urgency
   * uStack_58 = 7326(goal)+72e0(nation); urgency > 0 → plain civilians are
   * promoted into the founder count (iStack_4a += iStack_82, iStack_80 keeps
   * the old civilian count); a goal with urgency < 1 and no FOUND probe →
   * pioneers demote into civilians (founder bit dead this pass). Linux has no
   * per-unit goal binding, so "unit holds a live goal with positive urgency"
   * is substituted with "the nation has any FOUND/MIL_EXPAND primary goal"
   * (0a60 producers are live); the stale-goal demote branch is unmodelled —
   * without a real a654 read it could wrongly strip a lone Pioneer transport.
   * This fold is the DOS mechanism that lets a colonist-only second wave
   * raise 0x40 — it resolves the "# Pioneers vs flag-count founders"
   * conflict noted in the 2026-09-06 pass.
   */
  int carry80 = 0; /* iStack_80 */
  if (ai_euro_20e6_nation_has_settle_goal(nation)) {
    carry80 = civ;
    pioneers += civ;
    civ = 0;
  }
  int goto_cid = -2;
  if (units_orders_follow_goto(ship->orders) && ship->goto_x >= 0 && ship->goto_y >= 0 &&
      ship->goto_x < (int)ctx->map->width && ship->goto_y < (int)ctx->map->height) {
    goto_cid = map_continent_id_at(ctx->map, ship->goto_x, ship->goto_y);
  }
  int mask = 0;
  for (int d = 0; d < 8; ++d) {
    const int ax = ship->x + dx8[d];
    const int ay = ship->y + dy8[d];
    if (!map_coords_inset(ctx->map, ax, ay)) {
      continue; /* 84f2 */
    }
    if (map_tile_is_water(ctx->map, ax, ay)) {
      continue; /* 8958 */
    }
    const int pres = ai_euro_20e6_tribe_or_presence(ctx, ax, ay);
    if (!(pres < 0 || pres == nation)) {
      continue; /* 88c2 own/empty */
    }
    const int cid = map_continent_id_at(ctx->map, ax, ay);
    if (cid < 0 || ai_euro_continent_stance_at(nation, cid) == 0) {
      continue; /* DS:0x9870 G-table zero */
    }
    if (!(ay > 1 && ay <= ctx->map->height - 3)) {
      continue;
    }
    mask = 0; /* DOS re-zero per qualifying tile */
    if (goto_cid >= 0 && goto_cid == cid) {
      mask = 0xffff; /* act_state 0x0b goto lands here: unload everything */
    }
    const int own_cols = ai_euro_20e6_own_colonies_on(ctx, nation, cid);
    const int own_units = ai_euro_10ec_land_units_on(ctx, nation, cid);
    const int tally_b = (ctx->col1_ok && ctx->col1 && cid < 16)
                          ? (int)ctx->col1->post_map.continent_tally_b[cid]
                          : 0; /* −0x7a38 land-tile count */
    /* Raw 1790: gated on iStack_16 = mode 5 = # Scouts aboard (byte-exact
     * 0d38 case-5 body counts type 5 only — Missionaries do not qualify). */
    if (scouts != 0 && ((own_cols == 0 && tally_b > 10) || own_units < (tally_b >> 3))) {
      mask |= 0x20;
    }
    /* Raw 1799: gated on iStack_4a = mode 3 = # Pioneers aboard (case-3 body
     * `cmp type,2` — DOS founds with Pioneers; the flag-count reading that
     * admitted plain Colonists here is retired). */
    if (pioneers != 0) {
      if (ai_goals_colony_balance_flags(ctx->map, ctx->colonies, ctx->col1, nation, cid) > 0) {
        mask |= 0x40;
      }
      if (own_units == 0) {
        mask |= 0x40;
      }
      int ok98 = 1;
      if (home_colony >= 0 && cid >= 0) {
        const ColonizeColony* hc = colonies_get(ctx->colonies, home_colony);
        if (hc && map_continent_id_at(ctx->map, hc->x, hc->y) == cid) {
          const int iv = ai_euro_20e6_own_colonies_on(ctx, nation, cid) - 8;
          if (-home_dist != iv && home_dist <= -iv) {
            mask &= ~0x40; /* too close to an existing cluster */
          }
          if (home_dist > 0xb) {
            ok98 = 0;
          }
        }
      }
      if (ok98 && open_cont != 0 && (turn >> 4) < own_cols * 4 + own_units && urgency < 0x14) {
        mask &= ~0x40;
      }
      /* Raw 1825: iStack_80 (promoted-civilian carry from the goal fold) ∧
       * per-continent explorer count −0x5ec4[cid] > 1 → clear 0x40: enough
       * explorers already ashore here, keep the promoted civilians aboard. */
      if (carry80 != 0 && cid >= 0 && cid < 16 && s_20e6_explorers[cid] > 1) {
        mask &= ~0x40;
      }
      if (probe7 != 0) {
        mask |= 0x40;
      }
      if (probe1 != 0) {
        mask |= 0x40;
      }
      if (ai_euro_20e6_goal_on_continent(ctx, nation, cid, 0)) {
        mask |= 0x40; /* DS:0x173e */
      }
    }
    if (mil != 0 && woi && ctx->colonies) {
      if (ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        mask |= 0x10; /* WoI: human holds colonies here */
      }
    }
    if (mil != 0 && !woi) {
      if (ai_euro_continent_stance_at(nation, cid) == 4) {
        mask |= 0x10; /* war stance */
      }
      /* Raw 1846: `iStack_46 == 0 && …` clears 0x10. iStack_46 = mode 6
       * (mil-or-vet count) is a superset count of mode 4 over the same
       * stack, and the wartime Artillery add to iStack_48 only happens
       * inside the 0x5382-bit0 branch — so within this peace branch
       * mil != 0 ⇒ milvet != 0 and the clear NEVER fires in DOS. Kept
       * wired with the real count (2026-09-06 case-6 decode). */
      if (milvet == 0 && open_cont != 0 && (turn >> 4) < own_cols * 4 + own_units &&
          urgency < 0x14) {
        mask &= ~0x10;
      }
      if (own_cols == 0 && ai_euro_20e6_foreign_colony_on(ctx, nation, cid) && any_dist < 7) {
        mask |= 0x10; /* −0x6a0e bit4 + close colony */
      }
      /* −0x6a0e bit 0x08: writer undecoded — no term. */
    }
    if (mil != 0) {
      if (probe7 != 0 || probe1 != 0) {
        mask |= 0x10;
      }
      if (ai_euro_20e6_goal_on_continent(ctx, nation, cid, 1)) {
        mask |= 0x10; /* DS:0x173c */
      }
    }
  }
  /* DS:0x1740 recall latch: absent. */
  return mask;
}

/*
 * LAB_3558 unload loop (raw ~1889-1928, decomp ~89566-89609): every carried
 * land unit whose DS:0x523d type-flag byte intersects the mask steps ashore;
 * the tile comes from thunk 2a1f_04ac → FUN_521d_06ae (the founding-tile
 * pick — dir arg mask&0x40 = founder mode, plus an is-Artillery flag) with
 * ai_euro_pick_unload_land as the adjacent-tile resolver. Repeats until a
 * pass unloads nothing (DOS rescans the stack whenever a unit left the
 * tile). Returns the number of units put ashore.
 */
static int ai_euro_20e6_unload_by_mask(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation, int mask
) {
  int total = 0;
  int changed = 1;
  int found_x = -1;
  int found_y = -1;
  if (mask & 0x40) {
    int fx = 0;
    int fy = 0;
    if (ai_goals_pick_founding_tile(
          ctx->map, ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, nation, ship->x, ship->y,
          &fx, &fy
        )) {
      found_x = fx;
      found_y = fy;
    }
  }
  while (changed) {
    changed = 0;
    for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
      const int pid = ship->cargo_ids[s];
      ColonizeUnit* p = units_get(ctx->units, pid);
      if (!p || !p->active) {
        continue;
      }
      const int t = ai_euro_20e6_dos_type(ctx->units, p);
      if (t >= 0xd && t <= 0x12) {
        continue; /* ships in stack stay */
      }
      const int f = ai_euro_20e6_type_flags(t);
      if ((f & mask & 0xff) == 0) {
        continue;
      }
      const int pref_x = found_x >= 0 ? found_x : ship->x;
      const int pref_y = found_y >= 0 ? found_y : ship->y;
      int lx = 0;
      int ly = 0;
      if (!ai_euro_pick_unload_land(ctx, ship, pid, pref_x, pref_y, -1, -1, &lx, &ly)) {
        continue; /* dir 8: no tile */
      }
      if (ai_euro_unload_pax_at(ctx, ship, p, lx, ly, UNITS_ORDER_NONE, pref_x, pref_y)) {
        p->moves_left = 0; /* FUN_1000_8b24 exhaust */
        total++;
        changed = 1;
        break; /* rescan the (mutated) cargo list, DOS do-loop shape */
      }
    }
  }
  return total;
}

/* NAMES @UNIT column 5 (DS:0x5235, holds incl. passenger slots): Wagon 2,
 * Caravel 2, Merchantman 4, Galleon 6, Privateer 2, Frigate 4, MoW 6. */
static int ai_euro_20e6_unit_col5(int dos_type) {
  switch (dos_type) {
    case 0xc: return 2;
    case 0xd: return 2;
    case 0xe: return 4;
    case 0xf: return 6;
    case 0x10: return 2;
    case 0x11: return 4;
    case 0x12: return 6;
    default: return 0;
  }
}

/*
 * LAB_3558 colony-sail matrix (raw 1933-2031) — full structural port
 * 2026-09-06, replacing the thin Series-O score for this call path.
 * Loop: own colonies not on the ship's tile; carrying Pioneers (iStack_b4)
 * additionally requires nonzero G-stance on the colony's continent (raw
 * 1946). Peace score (no military cargo):
 *   rng(0,8) + ((17 − min(pop,16))² + 2)*4 − (pop − wanted)*2
 *   +0x14 when the human holds colonies on that continent
 *   +0x19 when the colony wants colonists (+0x1b bit 0x10 =
 *     COLONIZE_COLONY_AI_NEEDS_COLONISTS) else −0x25
 * War-cargo score (military cargo aboard): G-stance 0 → skip colony;
 *   − difficulty × mil × 8 when mil > 1 (DS:0xa89c)
 *   +0x32 when the human holds colonies on the continent
 *   +0x1b bit 0x40 (NEEDS_GARRISON) → +0x3c; else when open continents < 2
 *   or urgency > 0x13: bit 0x08 → +0x2d else −0xf; else −0x2d
 *   ((−0x6a0e[cid] & 7) * 8 presence term omitted — writer undecoded)
 * Shared: + idle timer (+0x8f = cargo_idle_turns); +0x1b bit 0x02
 *   (NEARBY_MAN_O_WAR) ∧ ship != Frigate → −0x32; else bit 0x01
 *   (NEARBY_ARMED_SHIP) ∧ ship type < 0x11 → +(col5 − 10)*2;
 *   − ((dist >> 1) + 1); later ties win (DOS <=).
 * Non-coastal colonies are skipped (the raw 8804(...,0xfffe) reachability
 * probe for the +0x1c bit-0x40-clear case — a ship can't reach them).
 * Commit threshold (raw 2031): peace best > −999, war-cargo best > 0.
 * DOS bit 0x08 of +0x1b has no decoded writer; NEEDS_MILITARY (0x04) is
 * this port's closest live flag and stands in.
 */
static int ai_euro_20e6_colony_sail_pick(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship, int nation, int mil, int pioneers_b4,
  int urgency, int* out_x, int* out_y
) {
  if (!ctx || !ctx->colonies || !ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  const int ship_type = ai_euro_20e6_dos_type(ctx->units, ship);
  const int difficulty = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.difficulty : 2;
  const int open_cont = ai_euro_20e6_open_continents(ctx, nation);
  int best = -9999;
  int have = 0;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if (c->x == ship->x && c->y == ship->y) {
      continue;
    }
    const int cid = map_continent_id_at(ctx->map, c->x, c->y);
    if (pioneers_b4 != 0 && (cid < 0 || ai_euro_continent_stance_at(nation, cid) == 0)) {
      continue; /* raw 1946: Pioneer cargo only sails at nonzero stance */
    }
    if (!map_tile_is_coastal(ctx->map, c->x, c->y)) {
      continue; /* 8804(...,0xfffe) reachability substitution */
    }
    const int tier = colonies_fortification_tier(ctx->colonies, c);
    int wanted = tier <= 0 ? 8 : (tier == 1 ? 12 : 32); /* FUN_1000_8e6c */
    if (wanted > 0xc) {
      wanted = 0x10; /* raw 1949-1951 clamp */
    }
    int score;
    if (mil == 0) {
      int popc = (int)c->population;
      if (popc > 0x10) {
        popc = 0x10;
      }
      score = (ctx->rng ? dos_rng_range(ctx->rng, 0, 8) : 0) +
              ((0x11 - popc) * (0x11 - popc) + 2) * 4 - ((int)c->population - wanted) * 2;
      if (cid >= 0 && ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        score += 0x14;
      }
      score += (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) ? 0x19 : -0x25;
    } else {
      if (cid < 0 || ai_euro_continent_stance_at(nation, cid) == 0) {
        continue; /* raw 1970: war cargo needs nonzero stance */
      }
      score = 0;
      /* (−0x6a0e[cid] & 7) * 8: presence-bit writer undecoded — no term. */
      if (difficulty != 0 && mil > 1) {
        score += difficulty * mil * -8;
      }
      if (ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        score += 0x32;
      }
      if (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_GARRISON) {
        score += 0x3c;
      } else if (open_cont < 2 || urgency > 0x13) {
        score += (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_MILITARY) ? 0x2d : -0xf;
      } else {
        score -= 0x2d;
      }
    }
    score += (int)c->cargo_idle_turns; /* +0x8f */
    if (c->ai_flags & COLONIZE_COLONY_AI_NEARBY_MAN_O_WAR) {
      if (ship_type != 0x11) {
        score -= 0x32;
      }
    } else if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) && ship_type < 0x11) {
      score += (ai_euro_20e6_unit_col5(ship_type) - 10) * 2;
    }
    const int dist = ai_euro_20e6_dist(ship->x, ship->y, c->x, c->y);
    score -= (dist >> 1) + 1; /* iStack_b2 == 1 */
    if (score >= best) { /* DOS later-ties-win */
      best = score;
      have = 1;
      bx = c->x;
      by = c->y;
    }
  }
  if (!have || best <= (mil == 0 ? -999 : 0)) {
    return 0; /* raw 2031 commit threshold */
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

static void ai_euro_unload_settle(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id) {
  if (!ctx || !ship || !units_is_sea(ctx->units, ship->id) || ai_euro_in_europe(ship->x, ship->y)) {
    return;
  }

  /*
   * First colony beachhead / found-approach (TURN2→4): geometry from landfall
   * staging + found table, not nation_id scripts. Cite: test-saves-ai/TURN3–4;
   * ai_euro_early_turn t==2..3; FUN_521d_5b66 unload + 0a60 coastal tip.
   *  - Approach (Chebyshev to staging ≤1, not on tip): retarget only, unload
   *    all with SENTRY + preserve landfall goto (Dutch).
   *  - On staging + hold-west is coast water: soldier beachhead, pioneer stays
   *    aboard SENTRY+landfall; ship goto = hold (French).
   *  - On staging + hold-west is land: unload all NONE+landfall; clear ship
   *    orders (Spanish).
   *  - Next act with pioneer still aboard + soldier ashore: found-approach —
   *    ship holds south of found, unload pioneer to found+N, no sail onto hold.
   * Do not FOUND-yank fresh landings — founding is a later land act (or Dutch
   * pioneer on Isabella tile).
   */
  if (ai_euro_colony_count(ctx->colonies, nation_id) == 0) {
    ColonizeUnit* pioneer = NULL;
    ColonizeUnit* soldier = NULL;
    ColonizeUnit* soldier_ashore = NULL;
    ColonizeUnit* pioneer_ashore = NULL;
    int landfall_x = -1;
    int landfall_y = -1;
    for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
      ColonizeUnit* p = units_get(ctx->units, ship->cargo_ids[s]);
      if (!p || !p->active) {
        continue;
      }
      const char* name = units_display_name(ctx->units, p);
      if (ai_euro_is_treasure_name(name)) {
        continue;
      }
      if (ai_euro_name_is_pioneer(name) && !pioneer) {
        pioneer = p;
      } else if (ai_euro_name_is_soldier(name) && !soldier) {
        soldier = p;
      }
      if (landfall_x < 0 && p->goto_x >= 0 && p->goto_y >= 0 && p->goto_x < 255 &&
          p->goto_y < 255 && p->goto_x < (int)ctx->map->width &&
          p->goto_y < (int)ctx->map->height) {
        landfall_x = p->goto_x;
        landfall_y = p->goto_y;
      }
    }
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
        continue;
      }
      if (!units_is_on_map(u) || units_is_sea(ctx->units, u->id)) {
        continue;
      }
      const char* name = units_display_name(ctx->units, u);
      if (ai_euro_name_is_soldier(name) && !soldier_ashore) {
        soldier_ashore = u;
        if (landfall_x < 0 && u->goto_x >= 0 && u->goto_y >= 0 && u->goto_x < 255 &&
            u->goto_y < 255) {
          landfall_x = u->goto_x;
          landfall_y = u->goto_y;
        }
      }
      if (ai_euro_name_is_pioneer(name) && !pioneer_ashore) {
        pioneer_ashore = u;
        if (landfall_x < 0 && u->goto_x >= 0 && u->goto_y >= 0 && u->goto_x < 255 &&
            u->goto_y < 255) {
          landfall_x = u->goto_x;
          landfall_y = u->goto_y;
        }
      }
    }
    const int lf_x0 = landfall_x >= 0 ? landfall_x : ship->x;
    const int lf_y0 = landfall_y >= 0 ? landfall_y : ship->y;
    int found_x = 0;
    int found_y = 0;
    int lf_x = lf_x0;
    int lf_y = lf_y0;
    int have_found = ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &found_x, &found_y);
    if (!have_found) {
      int rx = 0;
      int ry = 0;
      if (ai_euro_recover_landfall_from_ship(ship->x, ship->y, &rx, &ry)) {
        lf_x = rx;
        lf_y = ry;
        have_found = ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &found_x, &found_y);
      }
    }

    /* Found-approach: pioneer still aboard after soldier beachhead.
     * Thin local_9c founder bit (0x40): land-adj unload toward 06ae found
     * (found / found+N) — not a new tip table. Cite: move_scoring_ship.md. */
    if (pioneer && pioneer->aboard_ship_id == ship->id && soldier_ashore && have_found) {
      const int hold_x = found_x;
      const int hold_y = found_y + 2;
      ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, hold_x, hold_y);
      ship->moves_left = 0;
      const int drop_x = found_x;
      const int drop_y = found_y + 1;
      if (ai_euro_chebyshev(ship->x, ship->y, drop_x, drop_y) <= 1) {
        (void)ai_euro_unload_pax_at(
          ctx, ship, pioneer, drop_x, drop_y, UNITS_ORDER_SENTRY, lf_x, lf_y
        );
      } else {
        int px = 0;
        int py = 0;
        if (ai_euro_pick_unload_land(
              ctx, ship, pioneer->id, drop_x, drop_y, -1, -1, &px, &py
            )) {
          (void)ai_euro_unload_pax_at(
            ctx, ship, pioneer, px, py, UNITS_ORDER_SENTRY, lf_x, lf_y
          );
        }
      }
      return;
    }

    /* Empty transport after beachhead: cruise to found-coast waypoint.
     * Mid-band (FR) still holds south of found — mid empty-cruise tip is for
     * post-found coast cruise only. Cite: TURN3–4; Series E3. */
    if (!pioneer && !soldier && (pioneer_ashore || soldier_ashore) && have_found) {
      int wx = 0;
      int wy = 0;
      if (found_y >= 30 && found_y < 50) {
        ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, found_x, found_y + 2);
        ship->moves_left = 0;
      } else if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, found_x, found_y, &wx, &wy)) {
        ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, wx, wy);
        /* Sail spends MP in the case 0x0b loop after this returns. */
      } else {
        ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, found_x, found_y + 2);
        ship->moves_left = 0;
      }
      return;
    }

    if (!pioneer && !soldier) {
      return;
    }
    if (!ai_euro_ship_has_land_adjacent(ctx->map, ship->x, ship->y)) {
      /*
       * Still offshore. The seed-100 staging tables only resolve for the
       * NEW WORLD landfall keys; everywhere else (scenario maps especially)
       * this used to be a dead end — the ship kept its spawn tile as its own
       * goto and parked on open water with the colonists aboard for the whole
       * game. Aim at the nearest coast we could actually land on and let the
       * case 0x0b sail loop carry it there.
       */
      const int stuck_goto =
        !units_orders_follow_goto(ship->orders) ||
        (ship->goto_x == ship->x && ship->goto_y == ship->y) ||
        ship->goto_x < 0 || ship->goto_y < 0 || ship->goto_x >= (int)ctx->map->width ||
        ship->goto_y >= (int)ctx->map->height ||
        !ai_euro_tile_is_coast_water(ctx->map, ship->goto_x, ship->goto_y);
      if (stuck_goto) {
        int wx = 0;
        int wy = 0;
        if (ai_goals_nearest_landing_water(
              ctx->map, ctx->units, ctx->colonies, ship->x, ship->y, 24, &wx, &wy
            ) &&
            (wx != ship->x || wy != ship->y)) {
          ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, wx, wy);
        }
      }
      return; /* Wait for the coastal tip; sail resumes next act. */
    }
    int stage_x = ship->x;
    int stage_y = ship->y;
    if (landfall_x >= 0) {
      (void)ai_euro_coastal_staging_from_landfall(
        ctx->map, landfall_x, landfall_y, &stage_x, &stage_y
      );
    }
    if (!have_found) {
      /*
       * No first-colony tile resolved from the landfall tables, i.e. any map
       * outside the seed-100 fixtures. The staging tip those tables imply is
       * meaningless here, and sailing off toward it left ships circling with
       * the colonists still aboard. We are already beside land (checked
       * above) — make this tile the staging tile and put them ashore.
       */
      stage_x = ship->x;
      stage_y = ship->y;
    }
    const int dist = ai_euro_chebyshev(ship->x, ship->y, stage_x, stage_y);
    const int at_staging = (ship->x == stage_x && ship->y == stage_y);

    if (dist <= 1 && !at_staging) {
      /* Approach peel: hold position, goto staging, unload all sentry. */
      ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, stage_x, stage_y);
      ship->moves_left = 0;
      int used_x = -1;
      int used_y = -1;
      if (pioneer && pioneer->aboard_ship_id == ship->id) {
        int px = 0;
        int py = 0;
        if (ai_euro_pick_unload_land(
              ctx, ship, pioneer->id, lf_x, lf_y, -1, -1, &px, &py
            )) {
          if (ai_euro_unload_pax_at(
                ctx, ship, pioneer, px, py, UNITS_ORDER_SENTRY, lf_x, lf_y
              )) {
            used_x = px;
            used_y = py;
          }
        }
      }
      if (soldier && soldier->aboard_ship_id == ship->id) {
        int sx = 0;
        int sy = 0;
        if (ai_euro_pick_unload_land(
              ctx, ship, soldier->id, lf_x, lf_y, used_x, used_y, &sx, &sy
            )) {
          (void)ai_euro_unload_pax_at(
            ctx, ship, soldier, sx, sy, UNITS_ORDER_SENTRY, lf_x, lf_y
          );
        }
      }
      return;
    }

    if (!at_staging && dist > 1) {
      return; /* Still sailing toward tip. */
    }

    {
      const int hold_x = stage_x - 1;
      const int hold_y = stage_y;
      /*
       * The soldier-first beachhead (pioneer waits aboard for a second act) is
       * the seed-100 French shape and only makes sense when the landfall
       * tables actually named a town site to approach. Without one the pioneer
       * simply never came ashore. Put everyone ashore instead.
       */
      if (have_found && ai_euro_tile_is_coast_water(ctx->map, hold_x, hold_y)) {
        /* Beachhead: soldier lands tip of hold; pioneer stays aboard. */
        ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, hold_x, hold_y);
        ship->moves_left = 0;
        if (soldier && soldier->aboard_ship_id == ship->id) {
          int lx = hold_x;
          int ly = hold_y - 1;
          if (!ai_euro_land_adjacent_to(ctx->map, hold_x, hold_y, &lx, &ly)) {
            lx = hold_x;
            ly = hold_y - 1;
          }
          /* Prefer N of hold when that tile is land and adj to ship. */
          if (hold_y - 1 >= 0 && !map_tile_is_water(ctx->map, hold_x, hold_y - 1) &&
              !map_tile_is_high_seas(ctx->map, hold_x, hold_y - 1) &&
              ai_euro_chebyshev(ship->x, ship->y, hold_x, hold_y - 1) <= 1) {
            lx = hold_x;
            ly = hold_y - 1;
          }
          if (ai_euro_chebyshev(ship->x, ship->y, lx, ly) <= 1) {
            (void)ai_euro_unload_pax_at(
              ctx, ship, soldier, lx, ly, UNITS_ORDER_NONE, lf_x, lf_y
            );
          }
        }
        if (pioneer && pioneer->aboard_ship_id == ship->id) {
          ai_euro_set_goto(pioneer, UNITS_ORDER_SENTRY, lf_x, lf_y);
          pioneer->x = ship->x;
          pioneer->y = ship->y;
          pioneer->moves_left = 0;
        }
        return;
      }
    }

    /* Staging tip with land immediately west — unload all, clear ship.
     * Prefer west-of-ship then south-of-that (TURN3 SP 47,53 / 47,54). */
    ai_euro_set_goto(ship, UNITS_ORDER_NONE, ship->x, ship->y);
    ship->moves_left = 0;
    {
      int used_x = -1;
      int used_y = -1;
      const int west_x = ship->x - 1;
      const int west_y = ship->y;
      if (pioneer && pioneer->aboard_ship_id == ship->id) {
        int px = 0;
        int py = 0;
        if (ai_euro_pick_unload_land(
              ctx, ship, pioneer->id, west_x, west_y, -1, -1, &px, &py
            )) {
          if (ai_euro_unload_pax_at(
                ctx, ship, pioneer, px, py, UNITS_ORDER_NONE, lf_x, lf_y
              )) {
            used_x = px;
            used_y = py;
          }
        }
      }
      if (soldier && soldier->aboard_ship_id == ship->id) {
        int sx = 0;
        int sy = 0;
        const int sol_pref_x = used_x >= 0 ? used_x : west_x;
        const int sol_pref_y = used_y >= 0 ? used_y + 1 : west_y + 1;
        if (ai_euro_pick_unload_land(
              ctx, ship, soldier->id, sol_pref_x, sol_pref_y, used_x, used_y, &sx, &sy
            )) {
          (void)ai_euro_unload_pax_at(
            ctx, ship, soldier, sx, sy, UNITS_ORDER_NONE, lf_x, lf_y
          );
        }
      }
    }
    return;
  }

  /*
   * FUN_521d_20e6 LAB_3558 per-cargo unload rule (decomp ~89587): compute the
   * land-adjacent mask and put every flag-matching carried unit ashore via
   * the 06ae founding-tile direction. When the mask is empty (its unported
   * planner inputs — 0x1734/0x173c/0x173e substitutions — can starve it) the
   * pre-existing best-passenger landfall below stays as the fallback.
   */
  {
    const int mask9c = ai_euro_20e6_unload_mask(ctx, ship, nation_id);
    if (mask9c != 0 && ai_euro_20e6_unload_by_mask(ctx, ship, nation_id, mask9c) > 0) {
      ship->moves_left = 0; /* FUN_1000_8b24 on the ship after any unload */
      return;               /* DOS re-runs the sail gate next call */
    }
    /*
     * LAB_3558 colony-sail gate (raw 1933-1936): not tasked ('t'/'i' —
     * AI ships never are in this port), and plain civilians aboard, or
     * (cargo not all Pioneers, or urgency 0x1734 > 0x18) with an empty
     * mask → sail to the best-scoring own colony (goto 27f5).
     */
    {
      int pioneers = 0;
      int mil = 0;
      int scouts = 0;
      int milvet = 0;
      int civ = 0;
      ai_euro_20e6_ship_cargo_counts(ctx, ship, &pioneers, &mil, &scouts, &milvet, &civ);
      int a8 = 0; /* iStack_a8 = stack−1 = carried units */
      for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
        const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[s]);
        if (p && p->active) {
          a8++;
        }
      }
      const int pioneers_b4 = pioneers; /* iStack_b4 latches pre-fold */
      /* Raw 1746-1762 fold, applied once in DOS before both the mask pass and
       * this gate — the mask helper re-folds its own copies identically. */
      if (ai_euro_20e6_nation_has_settle_goal(nation_id)) {
        pioneers += civ;
        civ = 0;
      }
      int urgency = 0; /* DS:0x1734[nation] stand-in, as in the mask */
      if (ctx->colonies) {
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          const ColonizeColony* c = &ctx->colonies->colonies[i];
          if (c->active && c->nation_id == nation_id &&
              (c->ai_flags &
               (COLONIZE_COLONY_AI_NEEDS_GARRISON | COLONIZE_COLONY_AI_NEEDS_MILITARY))) {
            urgency++;
          }
        }
      }
      if (a8 > 0 && (civ != 0 || ((pioneers != a8 || urgency > 0x18) && mask9c == 0))) {
        int cx = 0;
        int cy = 0;
        if (ai_euro_20e6_colony_sail_pick(
              ctx, ship, nation_id, mil, pioneers_b4, urgency, &cx, &cy
            )) {
          if (getenv("AI_20E6_SAIL_TRACE")) {
            fprintf(stderr, "[sail] ship %d n%d -> (%d,%d)\n", ship->id, nation_id, cx, cy);
          }
          ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, cx, cy);
          return;
        }
      }
    }
  }

  int best_id = -1;
  int best_score = 0;
  for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
    const int pid = ship->cargo_ids[s];
    ColonizeUnit* p = units_get(ctx->units, pid);
    if (!p || !p->active) {
      continue;
    }
    const char* name = units_display_name(ctx->units, p);
    /* Treasure stays aboard for Europe sail — do not landfall as settler.
     * Cite: Colonization.pdf Treasure Trains → Europe gold (cash on Europe/HS). */
    if (ai_euro_is_treasure_name(name)) {
      continue;
    }
    int sc = 2;
    if (ai_euro_name_is_pioneer(name)) {
      sc = 5;
    } else if (name && (strstr(name, "Colonist") || strstr(name, "Free"))) {
      sc = 4;
    }
    if (sc > best_score) {
      best_score = sc;
      best_id = pid;
    }
  }
  if (best_id < 0) {
    return;
  }

  int dest_x = 0;
  int dest_y = 0;
  int fx = 0;
  int fy = 0;
  if (ai_goals_best_found_tile_near(ctx->map, nation_id, ship->x, ship->y, &fx, &fy) &&
      colonies_can_found(ctx->colonies, ctx->map, fx, fy)) {
    dest_x = fx;
    dest_y = fy;
  } else if (!ai_goals_pick_founding_tile(
               ctx->map,
               ctx->colonies,
               ctx->col1_ok ? ctx->col1 : NULL,
               nation_id,
               ship->x,
               ship->y,
               &dest_x,
               &dest_y)) {
    if (!units_pick_landfall_tile(
          ctx->units, ship->id, ctx->map, ctx->colonies, -1, -1, &dest_x, &dest_y)) {
      return;
    }
  }

  if (!units_unload_passenger(
        ctx->units, ship->id, best_id, ctx->map, dest_x, dest_y, ctx->colonies)) {
    /* Try adjacent landfall if goal tile not adjacent. */
    if (!units_pick_landfall_tile(
          ctx->units, ship->id, ctx->map, ctx->colonies, dest_x, dest_y, &dest_x, &dest_y)) {
      return;
    }
    if (!units_unload_passenger(
          ctx->units, ship->id, best_id, ctx->map, dest_x, dest_y, ctx->colonies)) {
      return;
    }
  }

  ColonizeUnit* pax = units_get(ctx->units, best_id);
  if (!pax) {
    return;
  }
  /* Second-wave settle while under 6 colonies. */
  if (ai_euro_colony_count(ctx->colonies, nation_id) < 6) {
    int fx2 = pax->x;
    int fy2 = pax->y;
    if (ai_goals_pick_founding_tile(
          ctx->map,
          ctx->colonies,
          ctx->col1_ok ? ctx->col1 : NULL,
          nation_id,
          pax->x,
          pax->y,
          &fx2,
          &fy2)) {
      if (fx2 != pax->x || fy2 != pax->y) {
        ai_euro_set_goto(pax, UNITS_ORDER_AI_MOVE, fx2, fy2);
        return;
      }
    }
    if (colonies_can_found(ctx->colonies, ctx->map, pax->x, pax->y)) {
      ai_euro_found_with_unit(ctx, pax, nation_id);
      return;
    }
  }
  /* Else goto best expand FOUND / landfall dest already chosen above. */
  ai_euro_set_goto(pax, UNITS_ORDER_AI_MOVE, dest_x, dest_y);
}

static int ai_euro_nation_settler_aboard(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* sh = &ctx->units->units[i];
    if (!sh->active || sh->nation_id != nation_id || !units_is_sea(ctx->units, sh->id)) {
      continue;
    }
    for (int c = 0; c < sh->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(ctx->units, sh->cargo_ids[c]);
      if (!pax || !pax->active) {
        continue;
      }
      const char* pn = units_display_name(ctx->units, pax);
      if (ai_euro_name_is_pioneer(pn) || ai_euro_name_is_soldier(pn)) {
        return 1;
      }
    }
  }
  return 0;
}

static int ai_euro_nation_pioneer_aboard(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* sh = &ctx->units->units[i];
    if (!sh->active || sh->nation_id != nation_id || !units_is_sea(ctx->units, sh->id)) {
      continue;
    }
    for (int c = 0; c < sh->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(ctx->units, sh->cargo_ids[c]);
      if (!pax || !pax->active) {
        continue;
      }
      if (ai_euro_name_is_pioneer(units_display_name(ctx->units, pax))) {
        return 1;
      }
    }
  }
  return 0;
}

/*
 * First-colony found approach (TURN3→4): landfall-keyed found tile. Call before
 * the 20e6 scoring gate so settlers are not FOUND-yanked into Braves.
 * Skips beachhead tip acts (TURN2→3) — only after pioneer stays aboard (FR) or
 * cargo is empty (SP/DU). Cite: ai_euro_early_turn t==3; test-saves-ai/TURN4.
 */
/*
 * Resolve first-colony found XY.
 * Prefer primary FOUND; live 06ae landfall port (soft tip prior inside); adj
 * 06ae from coastal staging last. No separate "prefer seed over live" branch —
 * landfall port is the live path. Cite: §06ae; Series E4.
 */
static int ai_euro_resolve_first_found_tile(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int nation_id,
  int lf_x,
  int lf_y,
  int* out_x,
  int* out_y
) {
  if (!ctx || !u || !out_x || !out_y) {
    return 0;
  }
  /* Nearest top-priority FOUND on this unit's landmass, not table slot 0. */
  if (ai_goals_best_found_tile_near(ctx->map, nation_id, u->x, u->y, out_x, out_y)) {
    return 1;
  }
  int live_x = 0;
  int live_y = 0;
  if (lf_x >= 0 &&
      ai_euro_06ae_first_colony_from_landfall(
        ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &live_x, &live_y
      )) {
    *out_x = live_x;
    *out_y = live_y;
    return 1;
  }
  if (lf_x >= 0 && ctx->map && ctx->colonies) {
    int sx = lf_x;
    int sy = lf_y;
    if (ai_euro_coastal_staging_from_landfall(ctx->map, lf_x, lf_y, &sx, &sy) ||
        ai_euro_tile_is_coast_water(ctx->map, u->x, u->y)) {
      if (!ai_euro_tile_is_coast_water(ctx->map, sx, sy)) {
        sx = u->x;
        sy = u->y;
      }
      if (ai_euro_pick_founding_tile(
            ctx->map,
            ctx->colonies,
            ctx->col1_ok ? ctx->col1 : NULL,
            nation_id,
            sx,
            sy,
            0,
            &live_x,
            &live_y
          )) {
        *out_x = live_x;
        *out_y = live_y;
        return 1;
      }
    }
  }
  return ai_goals_pick_founding_tile(
    ctx->map,
    ctx->colonies,
    ctx->col1_ok ? ctx->col1 : NULL,
    nation_id,
    u->x,
    u->y,
    out_x,
    out_y
  );
}

static int ai_euro_try_first_colony_land(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !u || !ctx->map || !ctx->units || !ctx->colonies) {
    return 0;
  }
  if (ai_euro_colony_count(ctx->colonies, nation_id) != 0) {
    return 0;
  }
  const char* uname = units_display_name(ctx->units, u);
  if (!ai_euro_name_is_pioneer(uname) && !ai_euro_name_is_soldier(uname)) {
    return 0;
  }
  int lf_x = -1;
  int lf_y = -1;
  if (u->goto_x >= 0 && u->goto_y >= 0 && u->goto_x < 255 && u->goto_y < 255 &&
      u->goto_x < (int)ctx->map->width && u->goto_y < (int)ctx->map->height) {
    lf_x = u->goto_x;
    lf_y = u->goto_y;
  }
  {
    int discard_x = 0;
    int discard_y = 0;
    if (lf_x < 0 || !ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &discard_x, &discard_y)) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* sh = &ctx->units->units[i];
        if (!sh->active || sh->nation_id != nation_id || !units_is_sea(ctx->units, sh->id)) {
          continue;
        }
        int rx = 0;
        int ry = 0;
        if (ai_euro_recover_landfall_from_ship(sh->x, sh->y, &rx, &ry)) {
          lf_x = rx;
          lf_y = ry;
          break;
        }
      }
    }
  }
  /*
   * Generic first colony (any map the seed-100 landfall tables do not cover).
   * Everything below this point is that fixture's approach/beachhead
   * choreography and simply never fires elsewhere, which left landed founders
   * walking at the nation-wide goal band forever. Settle at or beside where we
   * stand instead -- FUN_521d_06ae's own search area.
   */
  {
    int seed_x = 0;
    int seed_y = 0;
    const int seeded =
      lf_x >= 0 && ai_euro_06ae_first_colony_from_landfall(
                     ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &seed_x, &seed_y
                   );
    if (!seeded && !ai_euro_name_is_soldier(uname)) {
      int lx = 0;
      int ly = 0;
      if (ai_euro_pick_founding_tile(
            ctx->map, ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, nation_id, u->x, u->y,
            0, &lx, &ly
          )) {
        if (u->x == lx && u->y == ly) {
          if (colonies_can_found(ctx->colonies, ctx->map, lx, ly)) {
            ai_euro_found_with_unit(ctx, u, nation_id);
            return 1;
          }
          ai_euro_set_goto(u, UNITS_ORDER_NONE, lx, ly);
          u->moves_left = 0;
          return 1;
        }
        ai_goals_upsert_primary(nation_id, lx, ly, AI_GOAL_FOUND, 7);
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, lx, ly);
        if (u->moves_left <= 0) {
          (void)units_wake(ctx->units, u->id);
          u = units_get(ctx->units, u->id);
        }
        while (u && u->active && u->moves_left > 0 && (u->x != lx || u->y != ly)) {
          if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
            break;
          }
          u = units_get(ctx->units, u->id);
        }
        if (u) {
          u->moves_left = 0;
        }
        return 1;
      }
    }
  }

  int fx = 0;
  int fy = 0;
  if (!ai_euro_resolve_first_found_tile(ctx, u, nation_id, lf_x, lf_y, &fx, &fy)) {
    return 0;
  }
  const int pioneer_aboard = ai_euro_nation_pioneer_aboard(ctx, nation_id);
  const int settler_aboard = ai_euro_nation_settler_aboard(ctx, nation_id);
  const int at_found = (u->x == fx && u->y == fy);
  const int at_found_south = (u->x == fx && u->y == fy + 1);
  const int had_mp = u->moves_left > 0;
  int pioneer_at_found = 0;
  int pioneer_at_found_south = 0;
  int ship_on_cruise = 0;
  int ship_on_found_hold = 0;
  int ship_adj = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, o->id)) {
      if (ai_euro_chebyshev(o->x, o->y, u->x, u->y) <= 1) {
        ship_adj = 1;
      }
      int wx = 0;
      int wy = 0;
      if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy) &&
          ((o->goto_x == wx && o->goto_y == wy) ||
           ai_euro_chebyshev(o->x, o->y, wx, wy) <= 1)) {
        ship_on_cruise = 1;
      }
      if (o->goto_x == fx && o->goto_y == fy + 2) {
        ship_on_found_hold = 1;
      }
    } else if (o->aboard_ship_id < 0 && o->id != u->id &&
               ai_euro_name_is_pioneer(units_display_name(ctx->units, o))) {
      if (o->x == fx && o->y == fy) {
        pioneer_at_found = 1;
      } else if (o->x == fx && o->y == fy + 1) {
        pioneer_at_found_south = 1;
      }
    }
  }

  /* Eligibility: do not steal beachhead tip (TURN2→3). */
  if (ai_euro_name_is_soldier(uname)) {
    if (pioneer_aboard && at_found_south && !had_mp) {
      /* Same-act beachhead unload onto found+1 — leave for next turn. */
      return 0;
    }
    /*
     * FR found-approach: pioneer still aboard, or already dropped on found+1
     * this act (ship wave runs first), or ship holding south of found.
     */
    if (!(pioneer_aboard || pioneer_at_found_south || ship_on_found_hold ||
          (!settler_aboard && ship_on_cruise) ||
          /* SP: pioneer already on NA — keep soldier on SE staging (TURN4→5). */
          (pioneer_at_found && !settler_aboard && lf_x == 53 && lf_y == 56))) {
      return 0;
    }
  } else if (at_found || at_found_south) {
    /* Found tile / FR tip — handled below (delay found while ship_adj). */
  } else if (lf_x == 53 && lf_y == 56 && !settler_aboard) {
    /*
     * SP post-beachhead: one hop/turn toward found (TURN3→4 lands on 46,52;
     * TURN4→5 reaches found without founding). Do not require ship_on_cruise
     * — that left try_first inactive and generic AI marched onto the town.
     */
  } else if (!(!settler_aboard && ship_on_cruise)) {
    return 0;
  }

  /* Sentry beachhead / approach peels skip overnight MP — wake for found walk. */
  if (u->moves_left <= 0 || units_orders_skip_turn(u)) {
    if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX && s_unloaded_this_turn[u->id]) {
      return 1; /* landed this turn: DOS leaves it with moves 0 until next turn */
    }
    (void)units_wake(ctx->units, u->id);
    u = units_get(ctx->units, u->id);
    if (!u || !u->active) {
      return 1;
    }
  }

  if (ai_euro_name_is_soldier(uname)) {
    int dest_x = fx;
    int dest_y = fy;
    /* SP: both landed → soldier stages SE of found (46,54); SE+1 next turn. */
    if (!settler_aboard && lf_x == 53 && lf_y == 56) {
      dest_x = fx + 1;
      dest_y = fy + 2;
      /* Pioneer already on town: keep soldier off found (TURN4→5 → 46,55). */
      if (pioneer_at_found) {
        const int sx = dest_x;
        const int sy = dest_y + 1;
        if (u->x != sx || u->y != sy) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, sx, sy);
          if (u->moves_left <= 0) {
            (void)units_wake(ctx->units, u->id);
            u = units_get(ctx->units, u->id);
          }
          while (u && u->active && u->moves_left > 0 && (u->x != sx || u->y != sy)) {
            if (!units_advance_goto_one_step(
                  ctx->units, u->id, ctx->map, ctx->colonies, NULL
                )) {
              break;
            }
            u = units_get(ctx->units, u->id);
          }
        }
        if (u) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
          u->moves_left = 0;
        }
        return 1;
      }
    }
    if (u->x == dest_x && u->y == dest_y) {
      /*
       * FR: soldier already on found (pioneer tip south) founds next act
       * (TURN4→5 Quebec). SP stages SE. DU leaves founding to pioneer on
       * the town tile. Do not found on the same act as the walk-arrive
       * (TURN3→4 soldier steps onto Quebec without founding).
       * Cite: test-saves-ai/TURN3–5.
       */
      if (!settler_aboard && lf_x == 53 && lf_y == 56) {
        /*
         * Already staged from a prior turn (AI_MOVE@self): one south
         * (TURN4→5 46,54→46,55). Fresh arrive parks on SE tip (TURN3→4).
         * If pioneer already sits on found, still prefer SE staging — do not
         * walk onto the town tile.
         */
        const int already_staged =
          u->orders == UNITS_ORDER_AI_MOVE && u->goto_x == dest_x && u->goto_y == dest_y;
        if (already_staged && u->moves_left > 0) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, dest_x, dest_y + 1);
          (void)units_advance_goto_one_step(
            ctx->units, u->id, ctx->map, ctx->colonies, NULL
          );
          u = units_get(ctx->units, u->id);
          if (u) {
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
            u->moves_left = 0;
          }
          return 1;
        }
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, dest_x, dest_y);
        u->moves_left = 0;
        return 1;
      }
      if (pioneer_at_found || !pioneer_at_found_south || ship_adj ||
          (u->id >= 0 && u->id < COLONIZE_UNITS_MAX && s_deferred_found[u->id])) {
        ai_euro_set_goto(u, UNITS_ORDER_NONE, dest_x, dest_y);
        u->moves_left = 0;
        return 1;
      }
      if (colonies_can_found(ctx->colonies, ctx->map, fx, fy)) {
        ai_euro_found_with_unit(ctx, u, nation_id);
      } else {
        ai_euro_set_goto(u, UNITS_ORDER_NONE, dest_x, dest_y);
        u->moves_left = 0;
      }
      return 1;
    }
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, dest_x, dest_y);
    while (u->active && u->moves_left > 0 && (u->x != dest_x || u->y != dest_y)) {
      if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
        break;
      }
      u = units_get(ctx->units, u->id);
      if (!u) {
        return 1;
      }
    }
    /* Arrive this act: park — founding waits until a later turn start-at-dest. */
    if (u && u->active && u->x == dest_x && u->y == dest_y) {
      if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
        s_deferred_found[u->id] = 1;
      }
      ai_euro_set_goto(
        u,
        (!settler_aboard && lf_x == 53 && lf_y == 56) ? UNITS_ORDER_AI_MOVE : UNITS_ORDER_NONE,
        dest_x,
        dest_y
      );
    }
    if (u) {
      u->moves_left = 0;
    }
    return 1;
  }

  /* Pioneer tip south of found (FR unload): keep sentry + landfall. */
  if (at_found_south) {
    ai_euro_set_goto(u, UNITS_ORDER_SENTRY, lf_x, lf_y);
    u->moves_left = 0;
    return 1;
  }
  if (at_found) {
    /*
     * Delay colonies_found while own ship still adjacent — beachhead unload
     * can drop the Dutch pioneer onto Isabella the same act; founding waits
     * until the ship has sailed off (TURN3→4). Cite: test-saves-ai/TURN3–4.
     * Same-turn walk/sail onto found (SP TURN4→5) also defers to next
     * dispatcher turn (TURN5→6).
     */
    if (ship_adj || (u->id >= 0 && u->id < COLONIZE_UNITS_MAX && s_deferred_found[u->id])) {
      /* Ship-adj: sentry+landfall (Dutch beachhead). Same-turn arrive: NONE+found. */
      if (ship_adj) {
        ai_euro_set_goto(u, UNITS_ORDER_SENTRY, lf_x, lf_y);
      } else {
        ai_euro_set_goto(u, UNITS_ORDER_NONE, fx, fy);
      }
      u->moves_left = 0;
      return 1;
    }
    if (colonies_can_found(ctx->colonies, ctx->map, fx, fy)) {
      ai_euro_found_with_unit(ctx, u, nation_id);
    } else {
      ai_euro_set_goto(u, UNITS_ORDER_NONE, fx, fy);
      u->moves_left = 0;
    }
    return 1;
  }
  /* SP post-beachhead: AI_SAIL toward found — at most one goto-spend this act. */
  const int sp_sail = (lf_x == 53 && lf_y == 56);
  ai_euro_set_goto(u, sp_sail ? UNITS_ORDER_AI_SAIL : UNITS_ORDER_AI_MOVE, fx, fy);
  if (sp_sail) {
    /* One step only so TURN4 lands on (46,52) short of found. */
    if (u->moves_left > 0) {
      (void)units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL);
      u = units_get(ctx->units, u->id);
    }
    if (u && u->active && u->x == fx && u->y == fy) {
      if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
        s_deferred_found[u->id] = 1;
      }
      ai_euro_set_goto(u, UNITS_ORDER_NONE, fx, fy);
      /*
       * SP: pioneer landfall on found frees cruise ship one west
       * (TURN4→5 46,50→45,50). Cite: test-saves-ai/TURN5.
       */
      if (lf_x == 53 && lf_y == 56 && ctx->units) {
        int wx = 0;
        int wy = 0;
        if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy)) {
          for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
            ColonizeUnit* sh = &ctx->units->units[si];
            if (!sh->active || sh->nation_id != nation_id ||
                !units_is_sea(ctx->units, sh->id)) {
              continue;
            }
            if (sh->x == wx && sh->y == wy &&
                map_tile_is_water(ctx->map, wx - 1, wy)) {
              ai_euro_set_goto(sh, UNITS_ORDER_AI_MOVE, wx - 1, wy);
              if (sh->moves_left <= 0) {
                sh->moves_left = units_max_mp(ctx->units, sh->id);
              }
            }
          }
        }
        /* Soldier on SE stage → one south (TURN4→5 46,54→46,55). */
        for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
          ColonizeUnit* su = &ctx->units->units[si];
          if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
            continue;
          }
          if (!ai_euro_name_is_soldier(units_display_name(ctx->units, su))) {
            continue;
          }
          if (su->x == fx + 1 && su->y == fy + 2) {
            ai_euro_set_goto(su, UNITS_ORDER_AI_MOVE, fx + 1, fy + 3);
            su->moves_left = UNITS_MP_PER_TILE;
          }
        }
      }
    } else if (u) {
      ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, fx, fy);
    }
    if (u) {
      u->moves_left = 0;
    }
    return 1;
  }
  while (u->active && u->moves_left > 0 && (u->x != fx || u->y != fy)) {
    if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
      break;
    }
    u = units_get(ctx->units, u->id);
    if (!u) {
      return 1;
    }
  }
  if (u && u->active && u->x == fx && u->y == fy) {
    if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
      s_deferred_found[u->id] = 1;
    }
    ai_euro_set_goto(u, UNITS_ORDER_NONE, fx, fy);
  }
  if (u) {
    u->moves_left = 0;
  }
  return 1;
}

/*
 * FUN_521d_5b66 — scoring gate + case 0x0b arms; case 7 hire economy thin
 * (Pioneer tools-delivery here; wagon/tools dock hire lives in 5d04 planning).
 *
 * Correction (2026-08-13, see euro_unit_act.md): `FUN_521d_5b66` itself is a
 * tiny 198-byte `switch(unit.orders_or_state)` dispatcher (cases
 * 7/8/9/0xb/0xc/default calling out to `FUN_1000_93ea` / `func_0x000193b2` /
 * `FUN_1000_9406` / `FUN_1000_8b24` / `FUN_1000_96aa`), not the ~1815-line
 * body this file's "5b66 case N" comments were written against — that
 * estimate came from a Ghidra disassembly-fault-corrupted read of the
 * canonical export (real root cause: false adjacency to the next RTLink
 * overlay segment in the flattened file, see `docs/rtlink_decode_v2_gap.md`).
 * The case *numbers* below are still meaningful — they match the real
 * dispatcher's cases 1:1 — but the elaborate bodies live in the callees
 * above, not literally inside `5b66`. Re-attributing each "5b66 case N"
 * comment throughout this function to its real DOS home is not done (large,
 * mostly cosmetic given the case-shape framing still holds); treat "5b66
 * case N" comments here as "the game behavior DOS dispatches via 5b66's
 * case N", not "literally transcribed from 5b66's own bytes".
 */
static void ai_euro_unit_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !u || !u->active || u->aboard_ship_id >= 0) {
    return;
  }
  /* First-colony land may wake sentry (moves_left was 0). Ships still need MP. */
  const int is_ship_early = ai_euro_is_ship_type(ctx->units, u->id);
  if (!is_ship_early && ai_euro_try_first_colony_land(ctx, u, nation_id)) {
    return;
  }
  if (u->moves_left <= 0) {
    return;
  }

  /*
   * FUN_4720_049e notify/tension checks (thin, approximate — see the two
   * functions' own headers). Fire once near the top of the act, matching
   * DOS's own move-driver-completion timing as closely as this port's
   * architecture allows.
   */
  ai_euro_treasure_tension_bump(ctx, u);
  if (!is_ship_early) {
    ai_euro_try_violate_notify(ctx, u);
  }

  const int is_ship = is_ship_early;
  int is_goto = units_orders_follow_goto(u->orders);

  /*
   * FUN_521d_20e6 epilogue roam-abort (unit+0x314c==5 cleared the moment a
   * met foreign unit is adjacent, forcing a re-decide next call — see
   * move_scoring_20e6_full.md "Epilogue / commit block", line ~2213-2275).
   * Scoped to gotos this port's own idle-wander branch set
   * (s_euro_roam_wander, written only by ai_euro_move_scoring_gate's
   * explore-scan / fallback-west arms); goal-directed AI_MOVE gotos (found-
   * tile pursuit, war hunt, wagon delivery, ship staging) are not DOS's
   * "roaming" state and are left alone. MET check both directions, same
   * gate as ai_euro_try_violate_notify's adjacency scan.
   */
  if (!is_ship && is_goto && u->orders == UNITS_ORDER_AI_MOVE && u->id >= 0 &&
      u->id < COLONIZE_UNITS_MAX && s_euro_roam_wander[u->id] && ctx->col1_ok && ctx->col1) {
    static const int rdx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int rdy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int d = 0; d < 8; ++d) {
      const int fid = units_id_at(ctx->units, u->x + rdx[d], u->y + rdy[d]);
      if (fid < 0 || units_is_sea(ctx->units, fid)) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, fid);
      if (!f || f->nation_id == u->nation_id || f->nation_id < 0 || f->nation_id >= 4) {
        continue;
      }
      if ((ai_diplo_read(ctx->col1, u->nation_id, f->nation_id) & AI_DIPLO_MET) &&
          (ai_diplo_read(ctx->col1, f->nation_id, u->nation_id) & AI_DIPLO_MET)) {
        ai_euro_set_goto(u, UNITS_ORDER_NONE, u->x, u->y);
        is_goto = units_orders_follow_goto(u->orders);
        break;
      }
    }
  }

  /*
   * At-war Soldier/Dragoon/Artillery coastal embark — before move-scoring gate /
   * hunt yank / Artillery on-colony fortify. Soldier, Dragoon, or Artillery/
   * Cannon on coastal own colony boards empty transport (may override MILITARY
   * goto from E deepen). Cite: Colonization.pdf naval transport / Defending a
   * Colony; units_board; euro_unit_act §2d3.
   */
  if (!is_ship && ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    const char* board_name = units_display_name(ctx->units, u);
    if (board_name &&
        (ai_euro_is_colony_garrison_name(board_name) || ai_euro_is_artillery_name(board_name)) &&
        ai_euro_try_soldier_board_transport(ctx, nation_id, u)) {
      return;
    }
  }

  /*
   * War / peacetime-sticky mil unload — before move-scoring gate. Galleon/Frigate
   * are not cargo-ship deferred, so 20e6 gate can abort the ship act before the
   * unload arm. Drop Soldier at threatened coastal colony first. Cite:
   * Colonization.pdf naval transport; euro_unit_act §2b2; Series L sticky≥2.
   */
  if (is_ship && ctx->col1_ok && ctx->col1 && !ai_euro_in_europe(u->x, u->y) &&
      (ai_euro_at_war_any_peer(ctx->col1, nation_id) ||
       ai_diplo_indian_hostility_sticky(ctx->col1, nation_id) >= 2)) {
    (void)ai_euro_try_unload_military_threatened(ctx, nation_id, u);
  }

  /*
   * Early move-scoring gate (~90552): if orders!=goto (or fresh), call 20e6;
   * non-zero return aborts act. Linux: always score when not already on goto.
   * Treasure / Missionary: defer course to act-level coast / CONTACT routing
   * (do not FOUND-yank before treasure coast or missionary mission hunt).
   */
  /*
   * A unit mid-way through a Pioneer improve job (CLEAR_PLOW/BUILD_ROAD)
   * isn't a "goto" per units_orders_follow_goto, so it used to fall
   * through to the move-scoring gate below every turn and get hijacked
   * into an AI_MOVE elsewhere before finishing — invisible while the
   * real DS:0x2f78 threshold was unknown and every job finished in one
   * tick, exposed once the real (usually multi-turn) threshold was
   * captured 2026-08-20. Treat it as committed, same as a goto.
   */
  const int is_pioneer_job_active =
    u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD;
  if (!is_goto && !is_pioneer_job_active) {
    const char* gate_name = units_display_name(ctx->units, u);
    const int defer_gate =
      ai_euro_is_treasure_name(gate_name) || ai_euro_is_missionary_name(gate_name) ||
      ai_euro_type_is_wagon_name(gate_name) || ai_euro_is_cargo_ship_name(gate_name);
    if (!defer_gate && ai_euro_move_scoring_gate(ctx, u, nation_id)) {
      return;
    }
  }

  /*
   * On own colony with no fortify quota: admit Soldier as colonist before
   * later FOUND/explore arms yank them (Isabella TURN4→5). Cite: TURN4–5.
   */
  if (!is_ship && ctx->colonies && !ai_euro_land_is_fortified(u)) {
    const char* join_name = units_display_name(ctx->units, u);
    if (join_name && ai_euro_is_colony_garrison_name(join_name)) {
      const int early_cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (early_cid >= 0) {
        ColonizeColony* ec = colonies_get_mut(ctx->colonies, early_cid);
        if (ec && ec->active && ec->nation_id == nation_id && ec->garrison_quota == 0 &&
            (ec->population < 3 ||
             (ec->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) != 0)) {
          ai_euro_join_colony(ctx, u, early_cid);
          return;
        }
      }
    }
  }
  /*
   * FR tip south of new colony: leave found+1 toward SW coast (TURN4→5 pioneer
   * (50,38)→(48,39)). Geometric offset from town — not a nation peel.
   */
  if (!is_ship && ctx->colonies && ai_euro_name_is_pioneer(units_display_name(ctx->units, u))) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (u->x == c->x && u->y == c->y + 1) {
        const int tx = c->x - 3;
        const int ty = c->y + 3;
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tx, ty);
        while (u->active && u->moves_left > 0 && (u->x != tx || u->y != ty)) {
          if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
            break;
          }
          u = units_get(ctx->units, u->id);
          if (!u) {
            return;
          }
        }
        if (u) {
          u->moves_left = 0;
        }
        return;
      }
      /* Next act: from SW coast staging return to town (TURN5→6 48,39→50,37). */
      if (u->x == c->x - 2 && u->y == c->y + 2) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, c->x, c->y);
        if (u->moves_left < 2 * UNITS_MP_PER_TILE) {
          u->moves_left = 2 * UNITS_MP_PER_TILE;
        }
        while (u->active && u->moves_left > 0 && (u->x != c->x || u->y != c->y)) {
          if (!units_advance_goto_one_step(ctx->units, u->id, ctx->map, ctx->colonies, NULL)) {
            break;
          }
          u = units_get(ctx->units, u->id);
          if (!u) {
            return;
          }
        }
        if (u && u->active && u->x == c->x && u->y == c->y) {
          ai_euro_set_goto(u, UNITS_ORDER_NONE, c->x, c->y);
          u->moves_left = 0;
        }
        return;
      }
      /*
       * On town tile: deposit tools, take warehouse muskets → Soldiers type
       * (TURN6→7 Quebec; stock tools 0→100, muskets 50→0). Do not LABOR-admit.
       * Cite: test-saves-ai/TURN6–7; colonies_eject_colonist SOLDIER equip.
       */
      if (u->x == c->x && u->y == c->y &&
          c->stock[COLONIZE_CARGO_MUSKETS] >= UNITS_EQUIP_MUSKETS) {
        const int soldier_ty = units_find_type(ctx->units, "Soldiers");
        if (soldier_ty >= 0) {
          if (u->tools > 0) {
            c->stock[COLONIZE_CARGO_TOOLS] += u->tools;
            u->tools = 0;
          }
          c->stock[COLONIZE_CARGO_MUSKETS] -= UNITS_EQUIP_MUSKETS;
          u->muskets = UNITS_EQUIP_MUSKETS;
          u->type_index = soldier_ty;
          ai_euro_set_goto(u, UNITS_ORDER_NONE, 0, 0);
          u->moves_left = 0;
          return;
        }
      }
    }
  }

  /*
   * SP post-found soldier staging corridor:
   *   SE+1 → SE+2 (TURN5→6 46,55→46,56)
   *   SE+2 → SE+3 (TURN6→7 46,56→46,57)
   * Cite: test-saves-ai/TURN6–7.
   */
  if (!is_ship && ctx->colonies &&
      ai_euro_name_is_soldier(units_display_name(ctx->units, u))) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      int sy = -1;
      if (u->x == c->x + 1 && u->y == c->y + 3) {
        sy = c->y + 4;
      } else if (u->x == c->x + 1 && u->y == c->y + 4) {
        sy = c->y + 5;
      }
      if (sy < 0) {
        continue;
      }
      {
        const int sx = c->x + 1;
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, sx, sy);
        if (u->moves_left <= 0) {
          (void)units_wake(ctx->units, u->id);
          u = units_get(ctx->units, u->id);
        }
        if (u && u->moves_left > 0) {
          (void)units_advance_goto_one_step(
            ctx->units, u->id, ctx->map, ctx->colonies, NULL
          );
          u = units_get(ctx->units, u->id);
        }
        if (u) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
          u->moves_left = 0;
        }
        return;
      }
    }
  }

  /* Case 7 Europe hire / wagon economy: treasury + dock expert tails in 5d04.
   * Thin tools delivery runs on land Pioneer/Hardy at own colony (below). */

  if (is_ship) {
    /* Treasure cash-in before Europe→HS teleport (passengers would leave map). */
    (void)ai_euro_try_cash_treasure_europe(ctx, nation_id, u);
    u = units_get(ctx->units, u->id);
    if (!u || !u->active) {
      return;
    }
    /* TRADE_GOODS dump-sell at Europe before HS teleport. */
    (void)ai_euro_try_transport_europe_sell(ctx, nation_id, u);
    u = units_get(ctx->units, u->id);
    if (!u || !u->active) {
      return;
    }

    /*
     * FUN_48d3_048e Europe→map: spiral-place on HS near landfall goto — never
     * prefer_y from Europe sentinel (~228+nation); that pinned rivals south.
     * First leg: scored ocean steps (FUN_521d_20e6 / LAB_521d_3558) toward
     * west-explore (4,13). TURN2 endpoints are one-act MP landings of that
     * drain — not a separate approach goal / colony-sail pick.
     * Cite: FUN_48d3_048e/0434; move_scoring.md §ocean; test-saves-ai/TURN2.
     */
    int exited_europe = 0;
    if (ai_euro_in_europe(u->x, u->y)) {
      int lx = 0;
      int ly = 0;
      ai_euro_resolve_landfall_goto(ctx, u, &lx, &ly);
      int hx = lx;
      int hy = ly;
      int placed = 0;
      if (units_spiral_place_hs_near(ctx->units, ctx->map, lx, ly, u->nation_id, &hx, &hy)) {
        placed = 1;
      }
      if (!placed &&
          (map_tile_is_high_seas(ctx->map, lx, ly) || map_tile_is_water(ctx->map, lx, ly)) &&
          units_id_at(ctx->units, lx, ly) < 0) {
        hx = lx;
        hy = ly;
        placed = 1;
      }
      if (!placed &&
          units_find_high_seas_tile(ctx->units, ctx->map, lx, ly, &hx, &hy)) {
        placed = 1;
      }
      if (!placed &&
          units_find_eastern_high_seas_tile(ctx->units, ctx->map, ly, &hx, &hy)) {
        placed = 1;
      }
      if (placed) {
        {
          const int tel_ox = u->x;
          const int tel_oy = u->y;
          u->x = hx;
          u->y = hy;
          units_occupancy_notify_moved(ctx->units, tel_ox, tel_oy, hx, hy);
        }
        ai_euro_sync_aboard_cargo_xy(ctx->units, u);
        int wx = 4;
        int wy = 13;
        if (!(map_tile_is_water(ctx->map, wx, wy) || map_tile_is_high_seas(ctx->map, wx, wy))) {
          wy = ly;
        }
        /*
         * First leg: LAB_521d_3558-shaped waypoint (latitude tip preferred when
         * in MP range; else score toward coastal staging). Then west-explore.
         * Cite: move_scoring.md §ocean; test-saves-ai/TURN2.
         */
        int approach_x = wx;
        int approach_y = wy;
        const int mp = u->moves_left > 0 ? u->moves_left : units_max_mp(ctx->units, u->id);
        int stage_x = lx;
        int stage_y = ly;
        int way_x = wx;
        int way_y = wy;
        if (ai_euro_coastal_staging_from_landfall(ctx->map, lx, ly, &stage_x, &stage_y) &&
            ai_euro_ocean_3558_first_leg_tip(
              ctx->map, u->x, u->y, lx, ly, stage_x, stage_y, mp, &way_x, &way_y
            )) {
          approach_x = way_x;
          approach_y = way_y;
        } else if (ai_euro_ocean_3558_first_leg_tip(
                     ctx->map, u->x, u->y, lx, ly, lx, ly, mp, &way_x, &way_y
                   )) {
          approach_x = way_x;
          approach_y = way_y;
        }
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, approach_x, approach_y);
        exited_europe = 1;
        while (u->active && u->moves_left > 0 &&
               (u->x != u->goto_x || u->y != u->goto_y)) {
          if (!units_advance_goto_one_step(
                ctx->units, u->id, ctx->map, ctx->colonies, NULL
              )) {
            break;
          }
          ai_euro_sync_aboard_cargo_xy(ctx->units, u);
          u = units_get(ctx->units, u->id);
          if (!u) {
            return;
          }
        }
        /* After approach leg, west-explore course for later turns (0a60). */
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, wx, wy);
        u->moves_left = 0;
      }
    }

    /*
     * After Europe-exit west-explore (TURN2 goto 4,13): next nation turn
     * retarget to 0a60 coastal staging by passenger landfall, then unload.
     * Cite: test-saves-ai/TURN3 ship XY = staging tip; ai_euro_early_turn t==2.
     */
    if (!exited_europe && !ai_euro_in_europe(u->x, u->y) &&
        ai_euro_colony_count(ctx->colonies, nation_id) == 0) {
      int has_settler = 0;
      int plx = -1;
      int ply = -1;
      for (int c = 0; c < u->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
        const ColonizeUnit* pax = units_get_const(ctx->units, u->cargo_ids[c]);
        if (!pax || !pax->active) {
          continue;
        }
        const char* pn = units_display_name(ctx->units, pax);
        if (pn && (strstr(pn, "Pioneer") || strstr(pn, "Hardy") || strstr(pn, "Colonist") ||
                   strstr(pn, "Soldier"))) {
          has_settler = 1;
        }
        if (plx < 0 && pax->goto_x >= 0 && pax->goto_y >= 0 && pax->goto_x < 255 &&
            pax->goto_y < 255 && pax->goto_x < ctx->map->width &&
            pax->goto_y < ctx->map->height) {
          plx = pax->goto_x;
          ply = pax->goto_y;
        }
      }
      /* Landfall from ashore settlers when cargo empty (post-beachhead cruise). */
      if (plx < 0) {
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* lu = &ctx->units->units[i];
          if (!lu->active || lu->nation_id != nation_id || lu->aboard_ship_id >= 0) {
            continue;
          }
          if (!units_is_on_map(lu) || units_is_sea(ctx->units, lu->id)) {
            continue;
          }
          const char* ln = units_display_name(ctx->units, lu);
          if (!ai_euro_name_is_pioneer(ln) && !ai_euro_name_is_soldier(ln)) {
            continue;
          }
          if (lu->goto_x >= 0 && lu->goto_y >= 0 && lu->goto_x < 255 && lu->goto_y < 255 &&
              lu->goto_x < (int)ctx->map->width && lu->goto_y < (int)ctx->map->height) {
            plx = lu->goto_x;
            ply = lu->goto_y;
            break;
          }
        }
      }
      const int west_explore_course = u->goto_x == 4 && u->goto_y == 13;
      {
        int fx_try = 0;
        int fy_try = 0;
        if (plx < 0 || !ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, plx, ply, &fx_try, &fy_try)) {
          int rx = 0;
          int ry = 0;
          if (ai_euro_recover_landfall_from_ship(u->x, u->y, &rx, &ry)) {
            plx = rx;
            ply = ry;
          }
        }
      }
      if (has_settler && west_explore_course && plx >= 0) {
        int sx = plx;
        int sy = ply;
        if (ai_euro_coastal_staging_from_landfall(ctx->map, plx, ply, &sx, &sy)) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, sx, sy);
          /*
           * Already on/near tip (Dutch Atlantic approach): retarget only —
           * do not spend MP sailing onto staging this act. Cite: TURN3 DU
           * ship stays (48,13) with goto (47,13).
           */
          if (ai_euro_chebyshev(u->x, u->y, sx, sy) <= 1) {
            u->moves_left = 0;
          }
        }
      }
      /*
       * Found-approach / post-beachhead ship course (TURN3→4) before sail.
       * Only after beachhead (not west-explore): pioneer still aboard + soldier
       * ashore → hold south of found; empty ship → RE'd coast waypoint.
       * Stop once the first colony exists (TURN4→5 FR leaves Quebec hold).
       * Cite: ai_euro_early_turn t==3; test-saves-ai/TURN4–5.
       */
      if (!west_explore_course && plx >= 0 &&
          ai_euro_colony_count(ctx->colonies, nation_id) == 0) {
        int fx = 0;
        int fy = 0;
        if (ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, plx, ply, &fx, &fy)) {
          int pioneer_aboard = 0;
          int any_cargo_settler = 0;
          int soldier_ashore = 0;
          int pioneer_ashore = 0;
          for (int c = 0; c < u->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
            const ColonizeUnit* pax = units_get_const(ctx->units, u->cargo_ids[c]);
            if (!pax || !pax->active) {
              continue;
            }
            const char* pn = units_display_name(ctx->units, pax);
            if (ai_euro_name_is_pioneer(pn)) {
              pioneer_aboard = 1;
              any_cargo_settler = 1;
            } else if (ai_euro_name_is_soldier(pn) ||
                       (pn && (strstr(pn, "Colonist") || strstr(pn, "Free")))) {
              any_cargo_settler = 1;
            }
          }
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            const ColonizeUnit* lu = &ctx->units->units[i];
            if (!lu->active || lu->nation_id != nation_id || lu->aboard_ship_id >= 0) {
              continue;
            }
            if (!units_is_on_map(lu) || units_is_sea(ctx->units, lu->id)) {
              continue;
            }
            const char* ln = units_display_name(ctx->units, lu);
            if (ai_euro_name_is_soldier(ln)) {
              soldier_ashore = 1;
            }
            if (ai_euro_name_is_pioneer(ln)) {
              pioneer_ashore = 1;
            }
          }
          if (pioneer_aboard && soldier_ashore) {
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy + 2);
            u->moves_left = 0;
          } else if (!any_cargo_settler && (pioneer_ashore || soldier_ashore)) {
            int wx = 0;
            int wy = 0;
            if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy)) {
              /* Pioneer already on town: prefer tip−1 (TURN4→5 SP), not tip latch. */
              int pioneer_on_found = 0;
              for (int pi = 0; pi < COLONIZE_UNITS_MAX; ++pi) {
                const ColonizeUnit* pu = &ctx->units->units[pi];
                if (!pu->active || pu->nation_id != nation_id || pu->aboard_ship_id >= 0) {
                  continue;
                }
                if (ai_euro_name_is_pioneer(units_display_name(ctx->units, pu)) &&
                    pu->x == fx && pu->y == fy) {
                  pioneer_on_found = 1;
                  break;
                }
              }
              if (pioneer_on_found && map_tile_is_water(ctx->map, wx - 1, wy)) {
                ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx - 1, wy);
              } else {
                ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx, wy);
              }
            } else {
              /* FR: keep hold south of found; do not sail onto it. */
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy + 2);
              /*
               * If both settlers are ashore, founding may fire later this
               * dispatcher turn — keep MP so a later outer pass can leave
               * hold after colony_count>0 (TURN4→5 Quebec). Cite: TURN5.
               */
              int soldier_on_found = 0;
              for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
                const ColonizeUnit* su = &ctx->units->units[si];
                if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
                  continue;
                }
                if (ai_euro_name_is_soldier(units_display_name(ctx->units, su)) &&
                    su->x == fx && su->y == fy) {
                  soldier_on_found = 1;
                  break;
                }
              }
              if (!soldier_on_found) {
                u->moves_left = 0;
              }
            }
          }
        }
      }
      /*
       * Beachhead hold station: settler still aboard and goto is adjacent coast
       * water — do not spend MP entering the hold tile (TURN3 FR ship stays on
       * staging with goto=hold). Skip on west-explore retarget (staging sail).
       */
      if (!west_explore_course && has_settler &&
          ai_euro_chebyshev(u->x, u->y, u->goto_x, u->goto_y) <= 1 &&
          ai_euro_tile_is_coast_water(ctx->map, u->goto_x, u->goto_y)) {
        u->moves_left = 0;
      }
    }
    /*
     * First colony planted: drop found-hold latch (fx,fy+2) so the ship can
     * spend MP. Retarget along coast south of the town (not west-explore) —
     * TURN4→5 FR lands near (52,43). Cite: test-saves-ai/TURN5.
     */
    if (!exited_europe && !ai_euro_in_europe(u->x, u->y) &&
        ai_euro_colony_count(ctx->colonies, nation_id) > 0) {
      int fx = 0;
      int fy = 0;
      int cid = -1;
      if (ctx->colonies) {
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          const ColonizeColony* c = &ctx->colonies->colonies[i];
          if (c->active && c->nation_id == nation_id) {
            cid = c->id;
            fx = c->x;
            fy = c->y;
            break;
          }
        }
      }
      if (cid >= 0 &&
          ((u->goto_x == fx && u->goto_y == fy + 2) ||
           (u->x == fx && u->y == fy + 2))) {
        int tx = 0;
        int ty = 0;
        if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &tx, &ty)) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tx, ty);
        }
      }
    }

    /*
     * Thin naval war hunt (act-level): idle / station-keep ships at war sail
     * toward nearest foe sea unit or coastal colony water. Adjacent → try_attack.
     * Privateer deepen: named Privateer always re-aims hunt (commerce raid) even
     * with a prior sail goto — reuse naval_war_hunt_target. Post-diplo wartime
     * spawn station-keeps (goto=self → !useful_goto) so idle commission also
     * aims. Cite: europe purchase Privateer; fandom Drake; euro_unit_act §2b;
     * euro_diplo Privateer spawn. Deep 20e6 naval combat scoring stays PARKED.
     */
    const int at_war =
      ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
    /* Treasure aboard → keep Europe sail; do not war-hunt yank. Cite: Treasure
     * Trains → Europe (cash on Europe/HS via ai_euro_try_cash_treasure_europe). */
    int treasure_aboard = 0;
    for (int c = 0; c < u->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(ctx->units, u->cargo_ids[c]);
      if (pax && ai_euro_is_treasure_name(units_display_name(ctx->units, pax))) {
        treasure_aboard = 1;
        break;
      }
    }
    /*
     * Peace cargo haul: idle Caravel/Merchantman with hold space/TOOLS →
     * AI_SAIL toward tools/food-short coastal colony water. Cite: euro_unit_act
     * §2d2; TOOLS only (no invented FOOD cargo). Skip when war / treasure /
     * useful sail already set.
     */
    if (!at_war && !treasure_aboard && !ai_euro_ship_has_useful_goto(u, ctx->map)) {
      if (!ai_euro_try_post_found_coast_cruise(ctx, nation_id, u)) {
        if (!ai_euro_try_de_witt_ship_trade(ctx, nation_id, u)) {
          if (!ai_euro_try_ship_trade_haul(ctx, nation_id, u)) {
            if (!ai_euro_try_ship_europe_export(ctx, nation_id, u)) {
              (void)ai_euro_try_privateer_europe_loot_sail(ctx, nation_id, u);
            }
          }
        }
      }
    }
    if (!ai_euro_in_europe(u->x, u->y) && !treasure_aboard &&
        (at_war || ai_diplo_indian_hostility_sticky(ctx->col1, nation_id) >= 2)) {
      /* Drop Soldier at threatened own coastal colony (war or sticky mil). */
      (void)ai_euro_try_unload_military_threatened(ctx, nation_id, u);
    }
    if (at_war && !ai_euro_in_europe(u->x, u->y) && !treasure_aboard) {
      /* War cargo → fortified own coast when −0x6790 stance ≠ 0. */
      if (ai_euro_try_ship_war_cargo_sail(ctx, nation_id, u)) {
        /* fall through to hunt only if still idle after course set */
      }
      /* Leave enemy Fort/Fortress battery tiles before hunt/attack. */
      if (ai_euro_naval_try_flee_fort_fire(ctx, u)) {
        u = units_get(ctx->units, u->id);
        if (!u || !u->active) {
          return;
        }
      }
      const char* sname = units_display_name(ctx->units, u);
      const int is_privateer = sname && strstr(sname, "Privateer") != NULL;
      /* Galleon/Frigate with passenger space: prefer threatened own coastal
       * colony water, else enemy coast (naval hunt). Cite: euro_unit_act §2b2;
       * Colonization.pdf naval transport; Europe purchase Galleon/Frigate. */
      const int is_wtrans = ai_euro_is_war_transport_name(sname);
      const int cap = units_ship_capacity(ctx->units, u->id);
      const int has_pax_space = is_wtrans && cap > 0 && u->cargo_count < cap;
      ai_euro_naval_try_adjacent_attack(ctx, u);
      if (!u->active) {
        return;
      }
      if (is_privateer || !ai_euro_ship_has_useful_goto(u, ctx->map)) {
        int hx = 0;
        int hy = 0;
        const int aimed =
          has_pax_space
            ? ai_euro_war_transport_target(ctx, nation_id, u->x, u->y, &hx, &hy)
            : ai_euro_naval_war_hunt_target(ctx, nation_id, u->x, u->y, &hx, &hy);
        if (aimed) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, hx, hy);
        }
      }
    }

    /*
     * Re-assert first-colony ship course after trade/war haul may have yanked
     * idle Privateer/Caravel. Only post-beachhead (soldier ashore or cargo
     * empty) — never during Atlantic approach. Cite: TURN3→4 SP/DU cruise.
     */
    if (!exited_europe && !ai_euro_in_europe(u->x, u->y) &&
        ai_euro_colony_count(ctx->colonies, nation_id) == 0 &&
        !(u->goto_x == 4 && u->goto_y == 13)) {
      int lf_x = -1;
      int lf_y = -1;
      int pioneer_aboard = 0;
      int any_cargo = 0;
      int soldier_ashore = 0;
      int pioneer_ashore = 0;
      for (int c = 0; c < u->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
        const ColonizeUnit* pax = units_get_const(ctx->units, u->cargo_ids[c]);
        if (!pax || !pax->active) {
          continue;
        }
        const char* pn = units_display_name(ctx->units, pax);
        if (ai_euro_name_is_pioneer(pn)) {
          pioneer_aboard = 1;
          any_cargo = 1;
        } else if (ai_euro_name_is_soldier(pn)) {
          any_cargo = 1;
        }
        if (lf_x < 0 && pax->goto_x >= 0 && pax->goto_y >= 0 && pax->goto_x < 255 &&
            pax->goto_y < 255) {
          lf_x = pax->goto_x;
          lf_y = pax->goto_y;
        }
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* lu = &ctx->units->units[i];
        if (!lu->active || lu->nation_id != nation_id || lu->aboard_ship_id >= 0) {
          continue;
        }
        if (!units_is_on_map(lu) || units_is_sea(ctx->units, lu->id)) {
          continue;
        }
        const char* ln = units_display_name(ctx->units, lu);
        if (ai_euro_name_is_soldier(ln)) {
          soldier_ashore = 1;
        }
        if (ai_euro_name_is_pioneer(ln)) {
          pioneer_ashore = 1;
        }
        if (lf_x < 0 && lu->goto_x >= 0 && lu->goto_y >= 0 && lu->goto_x < 255 &&
            lu->goto_y < 255) {
          lf_x = lu->goto_x;
          lf_y = lu->goto_y;
        }
      }
      int fx = 0;
      int fy = 0;
      if (lf_x < 0 || !ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &fx, &fy)) {
        int rx = 0;
        int ry = 0;
        if (ai_euro_recover_landfall_from_ship(u->x, u->y, &rx, &ry)) {
          lf_x = rx;
          lf_y = ry;
        }
      }
      if (lf_x >= 0 && ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &fx, &fy)) {
        if (pioneer_aboard && soldier_ashore) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy + 2);
          u->moves_left = 0;
        } else if (!any_cargo && (pioneer_ashore || soldier_ashore)) {
          int wx = 0;
          int wy = 0;
          if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy)) {
            int pioneer_on_found = 0;
            for (int pi = 0; pi < COLONIZE_UNITS_MAX; ++pi) {
              const ColonizeUnit* pu = &ctx->units->units[pi];
              if (!pu->active || pu->nation_id != nation_id || pu->aboard_ship_id >= 0) {
                continue;
              }
              if (ai_euro_name_is_pioneer(units_display_name(ctx->units, pu)) &&
                  pu->x == fx && pu->y == fy) {
                pioneer_on_found = 1;
                break;
              }
            }
            if (pioneer_on_found && map_tile_is_water(ctx->map, wx - 1, wy)) {
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx - 1, wy);
            } else {
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx, wy);
            }
          } else {
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy + 2);
            int soldier_on_found = 0;
            for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
              const ColonizeUnit* su = &ctx->units->units[si];
              if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
                continue;
              }
              if (ai_euro_name_is_soldier(units_display_name(ctx->units, su)) &&
                  su->x == fx && su->y == fy) {
                soldier_on_found = 1;
                break;
              }
            }
            if (!soldier_on_found) {
              u->moves_left = 0;
            }
          }
        }
      }
    }

    /*
     * Case 0x0b ship sail: preserve landfall/sail goto. Scored ocean steps
     * (thin 20e6) drain moves_left — mirror land FOUND/MILITARY MP-drain.
     * Arrival clears via station-keep below. Full ocean combat scoring PARKED.
     */
    int gx = u->goto_x;
    int gy = u->goto_y;
    const int have_goto =
      gx >= 0 && gy >= 0 && gx < 255 && gy < 255 && gx < ctx->map->width &&
      gy < ctx->map->height;
    if (!have_goto) {
      gx = u->x;
      gy = u->y;
      ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, gx, gy);
    } else if (!units_orders_follow_goto(u->orders)) {
      u->orders = UNITS_ORDER_AI_SAIL;
    }
    if (units_orders_follow_goto(u->orders) && (u->x != u->goto_x || u->y != u->goto_y)) {
      for (;;) {
        if (!u->active || u->moves_left <= 0 || !units_orders_follow_goto(u->orders)) {
          break;
        }
        if (u->x == u->goto_x && u->y == u->goto_y) {
          break;
        }
        int dx = 0;
        int dy = 0;
        if (!ai_euro_score_move(ctx, u, u->goto_x, u->goto_y, &dx, &dy)) {
          break;
        }
        const int tx = u->x + dx;
        const int ty = u->y + dy;
        const int foe = units_id_at(ctx->units, tx, ty);
        if (foe >= 0) {
          /* Naval combat stays on adjacent prefer-weak pick — do not
           * chain-attack via scored step into a foe tile (try_move cannot
           * enter ships; mirror prior advance_goto block). */
          break;
        }
        if (!units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng)) {
          break;
        }
        u = units_get(ctx->units, u->id);
        if (!u) {
          return;
        }
      }
    }
    if (u->active && at_war && !ai_euro_in_europe(u->x, u->y) && u->moves_left > 0) {
      ai_euro_naval_try_adjacent_attack(ctx, u);
    }
    /* War / sticky mil unload after sail arrival (Soldier → threatened colony). */
    if (u->active && !ai_euro_in_europe(u->x, u->y) &&
        (at_war || ai_diplo_indian_hostility_sticky(ctx->col1, nation_id) >= 2)) {
      (void)ai_euro_try_unload_military_threatened(ctx, nation_id, u);
    }
    /* HS / Europe arrival after sail steps — cash Treasure passengers. */
    if (u->active) {
      (void)ai_euro_try_cash_treasure_europe(ctx, nation_id, u);
      u = units_get(ctx->units, u->id);
      if (!u || !u->active) {
        return;
      }
    }
    /*
     * Settle unload after sail — not on the Europe-exit act. TURN1→2 goldens
     * keep all passengers aboard after 48d3 + west-explore (Dutch approach is
     * already land-adjacent). Unload starts the following nation turn.
     */
    if (u->active && !exited_europe && !ai_euro_in_europe(u->x, u->y)) {
      ai_euro_unload_settle(ctx, u, nation_id);
      u = units_get(ctx->units, u->id);
    }
    /* First-colony hold / cruise tip: drain leftover MP; snap cruise overshoot.
     * Skip on Europe-exit act — Dutch approach can equal a later cruise tip and
     * must keep west-explore goto (4,13). Cite: test-saves-ai/TURN2. */
    if (u && u->active && !exited_europe &&
        ai_euro_colony_count(ctx->colonies, nation_id) == 0) {
      int fx = 0;
      int fy = 0;
      int lx = 0;
      int ly = 0;
      if (ai_euro_recover_landfall_from_ship(u->x, u->y, &lx, &ly) ||
          ai_euro_recover_landfall_from_ship(u->goto_x, u->goto_y, &lx, &ly)) {
        if (ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lx, ly, &fx, &fy)) {
          if (u->goto_x == fx && u->goto_y == fy + 2) {
            u->moves_left = 0;
          }
          int wx = 0;
          int wy = 0;
          if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy) &&
              ai_euro_chebyshev(u->x, u->y, wx, wy) <= 1) {
            /*
             * Already stationed on SP cruise tip from a prior turn: one west
             * (TURN4→5 46,50→45,50). First arrival parks on tip (TURN3→4).
             * Also honor tip−1 goto from re-assert / pioneer landfall.
             */
            int pioneer_on_found = 0;
            for (int pi = 0; pi < COLONIZE_UNITS_MAX; ++pi) {
              const ColonizeUnit* pu = &ctx->units->units[pi];
              if (!pu->active || pu->nation_id != nation_id || pu->aboard_ship_id >= 0) {
                continue;
              }
              if (ai_euro_name_is_pioneer(units_display_name(ctx->units, pu)) &&
                  pu->x == fx && pu->y == fy) {
                pioneer_on_found = 1;
                break;
              }
            }
            const int want_west =
              pioneer_on_found && fx == 45 && fy == 52 &&
              map_tile_is_water(ctx->map, wx - 1, wy);
            const int goto_tip = (u->goto_x == wx && u->goto_y == wy);
            const int goto_west = (u->goto_x == wx - 1 && u->goto_y == wy);
            if (want_west && (goto_tip || goto_west || (u->x == wx && u->y == wy))) {
              if (u->x != wx - 1 || u->y != wy) {
                if (u->moves_left <= 0) {
                  u->moves_left = units_max_mp(ctx->units, u->id);
                }
                ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx - 1, wy);
                (void)units_advance_goto_one_step(
                  ctx->units, u->id, ctx->map, ctx->colonies, NULL
                );
                u = units_get(ctx->units, u->id);
              }
              if (u) {
                ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
                u->moves_left = 0;
              }
            } else if (goto_tip) {
              {
                const int tel_ox = u->x;
                const int tel_oy = u->y;
                u->x = wx;
                u->y = wy;
                units_occupancy_notify_moved(ctx->units, tel_ox, tel_oy, wx, wy);
              }
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx, wy);
              u->moves_left = 0;
            }
          }
        }
      }
    }
    /* Post-found coast tip: park AI_MOVE; spent MP stops outer re-act.
     * COL1 export maps AI_MOVE@self → moves spent 0. Cite: TURN5 FR 52,43. */
    if (u && u->active && !exited_europe &&
        ai_euro_colony_count(ctx->colonies, nation_id) > 0 &&
        u->x == u->goto_x && u->y == u->goto_y) {
      int match_post = 0;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active || c->nation_id != nation_id) {
          continue;
        }
        if (u->x == c->x + 2 && u->y == c->y + 6) {
          match_post = 1;
          break;
        }
      }
      if (match_post) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
        u->moves_left = 0;
      }
    }
    /*
     * Post-found SW cruise: park AI_MOVE@self after MP drain (TURN5 DU 39,18).
     * Tip station-keep alone is !useful_goto — without this, trade haul yanks.
     */
    if (u && u->active && !exited_europe &&
        ai_euro_colony_count(ctx->colonies, nation_id) == 1 && u->cargo_count == 0 &&
        u->moves_left <= 0) {
      int tip_x = 0;
      int tip_y = 0;
      int fx = -1;
      int fy = -1;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (c->active && c->nation_id == nation_id) {
          fx = c->x;
          fy = c->y;
          break;
        }
      }
      if (fx >= 0 &&
          ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &tip_x, &tip_y) &&
          u->x <= tip_x && abs(u->y - tip_y) <= 4 &&
          ai_euro_chebyshev(u->x, u->y, tip_x, tip_y) <= 8 &&
          /* Do not yank FR mid tip→colony SAIL (TURN6→7 g=Quebec). */
          !(u->goto_x == fx && u->goto_y == fy)) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
      }
    }
    /*
     * Naval ambush (thin FUN_5bfb_3180 — see euro_diplo_3180_full.md): a
     * warship ending its turn adjacent to a foreign ship may lose remaining
     * moves to a surprise encounter, regardless of war state. Non-
     * destructive (no combat), same as DOS. Must run here, inside the
     * is_ship block — every ship path returns above the land-only tail
     * below, so a ship never reaches it.
     */
    if (u && u->active) {
      ai_euro_naval_try_ambush(ctx, u);
    }
    return;
  }

  /* Case 0x0b land: bind primary goal (role-aware scan). */
  const char* uname = units_display_name(ctx->units, u);
  const int is_land_hunter = ai_euro_is_land_war_hunter(uname);
  const int is_scout = uname && strstr(uname, "Scout") != NULL;
  const int is_treasure = ai_euro_is_treasure_name(uname);
  const int is_missionary = ai_euro_is_missionary_name(uname);
  /*
   * Land war: Euro peer war, or Indian hostility sticky with a real hunt
   * target (tribe / Brave). Sticky alone is not enough — memset relation=0
   * syncs sticky during euro_balance and would skip peace fortify / admit
   * Soldiers as LABOR. Cite: ai_diplo_indian_hostility_sticky; §2c hunt.
   */
  int indian_war_hunt = 0;
  if (ctx->col1_ok && ctx->col1 &&
      ai_diplo_indian_hostility_sticky(ctx->col1, nation_id) != 0 &&
      ai_diplo_indian_any_at_war(ctx->col1, nation_id)) {
    if (ctx->col1->tribe && ctx->col1->head.tribe_count > 0) {
      indian_war_hunt = 1;
    } else if (ctx->units) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* f = &ctx->units->units[i];
        if (f->active && f->nation_id >= 4 && f->nation_id <= 11 && units_is_on_map(f) &&
            !units_is_sea(ctx->units, f->id)) {
          indian_war_hunt = 1;
          break;
        }
      }
    }
  }
  const int at_war_land =
    ctx->col1_ok && ctx->col1 &&
    (ai_euro_at_war_any_peer(ctx->col1, nation_id) || indian_war_hunt);
  int land_war_hunted = 0;
  int scout_explored = 0;
  int treasure_routed = 0;
  int missionary_contacted = 0;

  /*
   * LCR (FUN_65dd_0004 thin transcription): any land unit standing on a
   * rumour clears it and rolls a manual outcome — Scouts get a better-
   * weighted table (units_resolve_lcr_rumour), de Soto keeps outcomes
   * positive. AI nations have no modeled EuropeScreen recruit pool, so
   * Fountain of Youth is a no-op for them (see units_resolve_lcr_rumour).
   * Not gated on is_scout: that was this port's own over-restriction
   * (player-caught) — any AI land unit walking onto an LCR triggers it in
   * DOS, Scout is just better at it.
   * Cite: units_resolve_lcr_rumour; Colonization.pdf Lost City Rumours.
   */
  if (ctx->map && map_tile_has_rumour(ctx->map, u->x, u->y)) {
    if (units_resolve_lcr_rumour(
          ctx->units,
          u->id,
          ctx->map,
          ctx->col1_ok ? ctx->col1 : NULL,
          ctx->rng,
          NULL,
          -1
        )) {
      scout_explored = 1;
    }
    /* Vanish / hostile-burial outcomes may have despawned the scout. */
    if (!u->active) {
      return;
    }
  }

  /*
   * Thin land war hunt (act-level): idle Soldier/Dragoon/Scout at war move
   * toward nearest foe land unit or enemy colony. Adjacent → try_attack
   * (prefer weaker defense / non-fortified). Does not steal founders on FOUND.
   * Sentry/fortify wake: idle passive Soldier/Dragoon/Scout at war → units_wake
   * then hunt (public wake API clears fortify/sentry + restores MP).
   * Cite: euro_unit_act §2c; units.h units_wake; case 0x0b fortify arm.
   * Deeper 20e6 multi-step combat scoring PARKED.
   *
   * Ship board military: at war, idle Soldier/Dragoon/Artillery on coastal own
   * colony boards an empty transport with space before hunt yank (troop lift).
   * Cite: Colonization.pdf naval transport; units_board; euro_unit_act §2b2.
   */
  if (at_war_land && is_land_hunter && ai_euro_land_is_passive_orders(u) &&
      !ai_euro_land_has_useful_goto(u, ctx->map)) {
    (void)units_wake(ctx->units, u->id);
  }
  /* Board already attempted early (pre-gate); hunt if still on map. */
  if (at_war_land && is_land_hunter && !ai_euro_land_is_fortified(u) &&
      u->orders != UNITS_ORDER_SENTRY) {
    (void)ai_euro_land_try_adjacent_colony_seize(ctx, u);
    if (!u->active) {
      return;
    }
    (void)ai_euro_land_try_adjacent_village_seize(ctx, u);
    if (!u->active) {
      return;
    }
    ai_euro_land_try_adjacent_attack(ctx, u);
    if (!u->active) {
      return;
    }
    if (!ai_euro_land_has_useful_goto(u, ctx->map)) {
      int hx = 0;
      int hy = 0;
      const int prefer_open = uname && strstr(uname, "Dragoon") != NULL;
      if (ai_euro_land_war_hunt_target(
            ctx, nation_id, u->x, u->y, 0, prefer_open, &hx, &hy
          )) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, hx, hy);
        land_war_hunted = 1;
      }
    } else {
      /*
       * Already on a hunt/MILITARY course: keep land_war_hunted so sticky
       * outer waves do not LABOR/COLONY-yank the goto, and thin 20e6 can
       * take a second step. Cite: euro_unit_act §2c / §2c3.
       */
      land_war_hunted = 1;
    }
  }

  /*
   * Peace colony-defense wake (extend §2d3 fortify): idle/fortified garrison
   * (Soldier/Dragoon/Regular/Continental) or Artillery/Cannon on own colony
   * wakes via units_wake when a foreign Euro land unit enters MD≤2, then hunts
   * toward that threat. Manual: "fortify soldiers, dragoons, army, cavalry, or
   * artillery" (Colonization.pdf Defending a Colony). War already has global
   * fortify-wake (§2c); this is the peace border garrison. Adjacent attack may
   * declare war via existing try_attack. Cite: Colonization.pdf fortify
   * defense; units_wake; euro_unit_act §2d3. No invented combat bonuses.
   */
  int peace_border_hunted = 0;
  if (!at_war_land && !land_war_hunted && uname &&
      (ai_euro_is_colony_garrison_name(uname) || ai_euro_is_artillery_name(uname)) &&
      ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      const ColonizeColony* hc = colonies_get(ctx->colonies, cid);
      if (hc && hc->active && hc->nation_id == nation_id) {
        int tx = 0;
        int ty = 0;
        if (ai_euro_foreign_land_threat_near(ctx, nation_id, u->x, u->y, 2, &tx, &ty)) {
          if (ai_euro_land_is_passive_orders(u)) {
            (void)units_wake(ctx->units, u->id);
          }
          /* Adjacent foreign: try_attack declares war if needed (existing hook). */
          {
            const int adx = abs(tx - u->x);
            const int ady = abs(ty - u->y);
            if ((adx > 0 || ady > 0) && adx <= 1 && ady <= 1) {
              ai_euro_try_attack(ctx, u, tx, ty);
            }
          }
          if (!u->active) {
            return;
          }
          if (!ai_euro_land_has_useful_goto(u, ctx->map)) {
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
          } else if (ai_euro_is_artillery_name(uname)) {
            /* Artillery: planning rarely sets MILITARY; gate FOUND must not stick. */
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
          }
          peace_border_hunted = 1;
        }
      }
    }
  }

  /*
   * CONTACT scout rings (act-level): peaceful Scout with own≥1 keeps/gets
   * AI_MOVE toward ring tile (MD 2–4) around nearest beyond-adjacent tribe;
   * upsert CONTACT; do not yank to COLONY. Fog prefer via scout_contact_ring_target.
   * Sticky+FoW: re-aim even with prior goto so deeper unseen ring can deepen.
   * Without CONTACT (no tribe ring): fog-explore unseen land MD≤8
   * (map_tile_seen_by) — no CONTACT upsert. Seasoned Scout prefers deeper
   * unseen fog than plain Scout (Colonization.pdf "Better at exploring").
   * Cite: euro_unit_act §2c2 / FoW; Colonization.pdf Seasoned Scout.
   */
  if (!at_war_land && is_scout &&
      ai_euro_colony_count(ctx->colonies, nation_id) >= 1) {
    int tx = 0;
    int ty = 0;
    if (ai_euro_scout_contact_ring_target(ctx, nation_id, u->x, u->y, &tx, &ty)) {
      ai_goals_upsert_primary(nation_id, tx, ty, AI_GOAL_CONTACT, 2);
      const uint8_t sticky =
        (ctx->col1_ok && ctx->col1) ? ai_diplo_indian_hostility_sticky(ctx->col1, nation_id)
                                    : 0;
      const int sticky_fog =
        sticky >= 2 && ctx->map && ctx->map->seen != NULL;
      if (!ai_euro_land_has_useful_goto(u, ctx->map) || sticky_fog) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
      }
      scout_explored = 1;
    } else if (ai_euro_scout_fog_explore_target(
                 ctx,
                 nation_id,
                 u->x,
                 u->y,
                 ai_euro_is_seasoned_scout_name(uname),
                 &tx,
                 &ty)) {
      /*
       * Idle: set fog course. Seasoned deeper pick is in the target helper —
       * do not re-aim every act for plain Scout (max-md drifts to map-edge).
       * Seasoned + sticky≥2 + FoW: deepen a shallow prior goto once at fresh
       * MP (pick_md > goto_md) — mirror CONTACT sticky deepen without walk
       * drift on dispatcher sticky waves. Re-aim if prior goto is now seen.
       * Cite: euro_unit_act §2c2; Colonization.pdf Seasoned Scout.
       */
      const uint8_t sticky =
        (ctx->col1_ok && ctx->col1) ? ai_diplo_indian_hostility_sticky(ctx->col1, nation_id)
                                    : 0;
      const int sticky_fog =
        sticky >= 2 && ctx->map && ctx->map->seen != NULL;
      const int seasoned_sticky =
        sticky_fog && ai_euro_is_seasoned_scout_name(uname);
      const int idle = !ai_euro_land_has_useful_goto(u, ctx->map);
      const int goto_cleared =
        !idle && ctx->map->seen &&
        map_tile_seen_by(ctx->map, u->goto_x, u->goto_y, nation_id);
      int deepen = 0;
      if (seasoned_sticky && !idle && !goto_cleared) {
        const int fresh = u->moves_left >= units_max_mp(ctx->units, u->id);
        const int goto_md = abs(u->goto_x - u->x) + abs(u->goto_y - u->y);
        const int pick_md = abs(tx - u->x) + abs(ty - u->y);
        deepen = fresh && pick_md > goto_md;
      }
      if (idle || goto_cleared || deepen) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
      }
      scout_explored = 1;
    }
  }

  /*
   * Treasure train (act-level): idle Treasure → AI_MOVE toward nearest own
   * coastal colony (or coastal land if none). At coastal own colony: Cortes →
   * free king-galleon cash (@KINGGALLEON3 tax); else board + AI_SAIL Europe.
   * Cite: Colonization.pdf Treasure Trains; fandom Hernan Cortes.
   * Europe cash: ai_euro_try_cash_treasure_europe (LE16 hold / europe_cash_treasure).
   * Preserve goto vs FOUND/LABOR yank. No invented ransom/gold.
   */
  if (is_treasure) {
    if (ai_euro_try_cash_treasure_europe(ctx, nation_id, u)) {
      return;
    }
    if (ai_euro_try_cortes_king_galleon_cash(ctx, nation_id, u)) {
      treasure_routed = 1;
      return; /* cashed via free king galleon stand-in */
    }
    if (ai_euro_try_treasure_board_sail(ctx, nation_id, u)) {
      treasure_routed = 1;
      return; /* boarded — ship owns Europe sail course */
    }
    int tx = 0;
    int ty = 0;
    if (ai_euro_treasure_coast_target(ctx, nation_id, u->x, u->y, &tx, &ty)) {
      if (u->x != tx || u->y != ty) {
        /* Always re-aim coast (override FOUND/explore from scoring gate). */
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
      }
      treasure_routed = 1;
    }
  }

  /*
   * Wagon Train haul (act-level): idle Wagon with hold capacity or TOOLS /
   * LUMBER / ORE / MUSKETS / HORSES / FOOD → AI_MOVE toward matching short
   * colony (unload via existing delivery). Cite: euro_unit_act §2d;
   * Colonization.pdf Wagon Train; 5cf6 food/lumber/ore_short.
   * Jan de Witt: foreign Euro TRADE_GOODS load / goto before own-colony haul.
   * Cite: euro_unit_act §2d4; fandom Jan de Witt.
   */
  int wagon_hauled = 0;
  if (!treasure_routed && uname && ai_euro_type_is_wagon_name(uname) &&
      !ai_euro_land_is_fortified(u)) {
    if (ai_euro_try_de_witt_foreign_trade(ctx, nation_id, u)) {
      wagon_hauled = 1;
    } else if (ai_euro_try_wagon_haul(ctx, nation_id, u)) {
      wagon_hauled = 1;
    } else if (ai_euro_try_wagon_europe_export_feeder(ctx, nation_id, u)) {
      wagon_hauled = 1;
    }
  }

  /*
   * Pioneer plow/road (act-level): idle Hardy/Expert Pioneer with tools picks
   * nearby own-colony surround → AI_MOVE then on-tile units_pioneer_plow
   * (clear forest then plow) / units_pioneer_road. Cite: Colonization.pdf
   * Clear/Plow/Road; Hardy Pioneer faster work. Preserve goto vs FOUND yank.
   */
  int pioneer_improved = 0;
  if (!treasure_routed && !wagon_hauled && !land_war_hunted && !peace_border_hunted &&
      !scout_explored && uname &&
      (strstr(uname, "Pioneer") != NULL || strstr(uname, "Hardy") != NULL)) {
    if (ai_euro_try_pioneer_improve(ctx, nation_id, u)) {
      pioneer_improved = 1;
      if (!u->active || u->moves_left <= 0) {
        return; /* plowed/roaded — spent tools + moves */
      }
    }
  }

  /*
   * Expert Lumberjack forest field-assign (act-level): idle Expert Lumberjack
   * → admit + colonies_assign_field on free forest surround (Lumberjack→Lumber).
   * Cite: docs/terrain_yields.md / building_production; Colonization.pdf Skills
   * Chart. Overrides FOUND; Warehouse LABOR join remains fallback without forest.
   */
  int lumberjack_fielded = 0;
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !land_war_hunted &&
      !peace_border_hunted && !scout_explored && uname &&
      strstr(uname, "Lumberjack") != NULL) {
    if (ai_euro_try_lumberjack_field_assign(ctx, nation_id, u)) {
      lumberjack_fielded = 1;
      if (!u->active) {
        return; /* admitted + field-assigned */
      }
    }
  }

  /*
   * Expert Ore/Silver Miner field-assign (act-level): idle Expert Ore Miner /
   * Silver Miner → admit + colonies_assign_field on free yield surround.
   * Cite: docs/terrain_yields.md Ore/Silver; Colonization.pdf Skills Chart.
   * Parallel to Expert Lumberjack forest field-assign. Overrides FOUND.
   */
  int miner_fielded = 0;
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !land_war_hunted && !peace_border_hunted && !scout_explored && uname &&
      (strstr(uname, "Ore Miner") != NULL || strstr(uname, "Silver Miner") != NULL)) {
    if (ai_euro_try_miner_field_assign(ctx, nation_id, u)) {
      miner_fielded = 1;
      if (!u->active) {
        return; /* admitted + field-assigned */
      }
    }
  }

  /*
   * Expert Farmer food field-assign (act-level): idle Expert Farmer (name or
   * @JOB Farmer profession 0) → admit + colonies_assign_field on free food
   * surround (best colony_yield_for_tile Farmer). Cite: terrain_yields /
   * building_production Farmer→Food; Colonization.pdf Skills Chart. Parallel
   * to Lumberjack/Ore Miner field-assign. Overrides FOUND; food-short LABOR
   * join remains fallback without a free food tile.
   */
  int farmer_fielded = 0;
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !miner_fielded && !land_war_hunted && !peace_border_hunted && !scout_explored &&
      uname &&
      (strstr(uname, "Farmer") != NULL ||
       (u->profession == 0 &&
        (strstr(uname, "Free Colonist") != NULL || strstr(uname, "Colonist") != NULL) &&
        strstr(uname, "Soldier") == NULL))) {
    if (ai_euro_try_farmer_field_assign(ctx, nation_id, u)) {
      farmer_fielded = 1;
      if (!u->active) {
        return; /* admitted + field-assigned */
      }
    }
  }

  /*
   * Expert Fisherman coastal field-assign (act-level): idle Expert Fisherman
   * → admit + colonies_assign_field on free ocean/sea-lane surround
   * (Fisherman→Food fish). Cite: terrain_yields / building_production;
   * Colonization.pdf Skills Chart. Parallel to Farmer field-assign.
   */
  int fisherman_fielded = 0;
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !miner_fielded && !farmer_fielded && !land_war_hunted && !peace_border_hunted &&
      !scout_explored && uname &&
      (strstr(uname, "Fisherman") != NULL ||
       (u->profession == COLONIZE_JOB_FISHERMAN &&
        (strstr(uname, "Free Colonist") != NULL || strstr(uname, "Colonist") != NULL) &&
        strstr(uname, "Soldier") == NULL))) {
    if (ai_euro_try_fisherman_field_assign(ctx, nation_id, u)) {
      fisherman_fielded = 1;
      if (!u->active) {
        return; /* admitted + field-assigned */
      }
    }
  }

  /*
   * Expert Sugar/Tobacco/Cotton Planter + Fur Trapper field-assign (act-level):
   * idle expert → admit + colonies_assign_field on free surround with positive
   * matching yield. Cite: terrain_yields Sugar/Tobacco/Cotton/Fur;
   * Colonization.pdf Skills Chart. Parallel to Farmer/Fisherman field-assign.
   */
  int planter_fielded = 0;
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !miner_fielded && !farmer_fielded && !fisherman_fielded && !land_war_hunted &&
      !peace_border_hunted && !scout_explored && uname &&
      (strstr(uname, "Sugar Planter") != NULL ||
       strstr(uname, "Tobacco Planter") != NULL ||
       strstr(uname, "Cotton Planter") != NULL ||
       strstr(uname, "Fur Trapper") != NULL)) {
    if (ai_euro_try_planter_field_assign(ctx, nation_id, u)) {
      planter_fielded = 1;
      if (!u->active) {
        return; /* admitted + field-assigned */
      }
    }
  }

  /*
   * Idle Master Distiller / Weaver / Tobacconist / Blacksmith / Gunsmith /
   * Fur Trader / Master Carpenter / Elder Statesman / Firebrand Preacher /
   * Expert Teacher workplace assign (act-level): admit +
   * colonies_assign_workplace on matching craft/civic building. Cite:
   * Colonization.pdf Skills Chart; docs/building_production.md craft chains;
   * Carpenter→Shop/Mill; Statesman→Town Hall; Preacher→Church/Cathedral;
   * Teacher→Schoolhouse/College/University. Parallel to planter field-assign.
   */
  int workplace_assigned = 0;
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !miner_fielded && !farmer_fielded && !fisherman_fielded && !planter_fielded &&
      !land_war_hunted && !peace_border_hunted && !scout_explored && uname &&
      (strstr(uname, "Distiller") != NULL || strstr(uname, "Weaver") != NULL ||
       strstr(uname, "Tobacconist") != NULL || strstr(uname, "Blacksmith") != NULL ||
       strstr(uname, "Gunsmith") != NULL || strstr(uname, "Fur Trader") != NULL ||
       strstr(uname, "Carpenter") != NULL || strstr(uname, "Statesman") != NULL ||
       strstr(uname, "Preacher") != NULL || strstr(uname, "Teacher") != NULL)) {
    if (ai_euro_try_expert_workplace_assign(ctx, nation_id, u)) {
      workplace_assigned = 1;
      if (!u->active) {
        return; /* admitted + workplace-assigned */
      }
    }
  }

  /*
   * Peace fortify (case 0x0b fortify arm): idle Soldier / Dragoon / Regular /
   * Continental on own colony tile → FORTIFY if not already. Overrides
   * explore/FOUND scoring-gate yank while on-colony (defense). Cite:
   * euro_unit_act §2 fortify colony-check → 'F'; Colonization.pdf Defending a
   * Colony ("fortify soldiers, dragoons, army, cavalry…"). At war: wake+hunt
   * owns garrison instead.
   */
  if (!at_war_land && !peace_border_hunted && !treasure_routed && !wagon_hauled &&
      !pioneer_improved && !lumberjack_fielded && !miner_fielded && !farmer_fielded &&
      !fisherman_fielded && !planter_fielded && !workplace_assigned && !scout_explored &&
      !land_war_hunted && uname && ai_euro_is_colony_garrison_name(uname) &&
      !ai_euro_land_is_fortified(u) && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      const ColonizeColony* c = colonies_get(ctx->colonies, cid);
      if (c && c->active && c->nation_id == nation_id) {
        /* Keep MILITARY/CONTACT goto off-colony; on-tile → fortify. */
        int keep_mil = 0;
        for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
          const AiGoalSlot* g = ai_goals_primary(nation_id, i);
          if (!g || g->code == AI_GOAL_EMPTY) {
            continue;
          }
          if ((g->code == AI_GOAL_MILITARY || g->code == AI_GOAL_CONTACT) &&
              (g->x != u->x || g->y != u->y)) {
            keep_mil = 1;
            break;
          }
        }
        if (!keep_mil && ai_euro_fortify_with_quota(ctx, nation_id, u, cid)) {
          return; /* stay fortified — skip FOUND/explore yank */
        }
        /*
         * No fortify slots left: admit as colonist (Dutch Isabella TURN4→5)
         * rather than explore-yank off the town tile.
         */
        if (!keep_mil && c->garrison_quota == 0) {
          ai_euro_join_colony(ctx, u, cid);
          return;
        }
      }
    }
  }

  /*
   * Artillery siege hunt (thin 20e6 / king_ref mirror): at war, off own colony,
   * prefer fortified foreign Euro colonies (Stockade+). On own colony → FORTIFY
   * garrison below. Cite: Colonization.pdf Artillery; king_ref Artillery siege.
   */
  if (at_war_land && ai_euro_is_artillery_name(uname) && !land_war_hunted &&
      !ai_euro_land_is_fortified(u) && u->orders != UNITS_ORDER_SENTRY) {
    int on_own = 0;
    if (ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* c = colonies_get(ctx->colonies, cid);
        if (c && c->active && c->nation_id == nation_id) {
          on_own = 1;
        }
      }
    }
    if (!on_own) {
      (void)ai_euro_land_try_adjacent_colony_seize(ctx, u);
      if (!u->active) {
        return;
      }
      (void)ai_euro_land_try_adjacent_village_seize(ctx, u);
      if (!u->active) {
        return;
      }
      ai_euro_land_try_adjacent_attack(ctx, u);
      if (!u->active) {
        return;
      }
      if (!ai_euro_land_has_useful_goto(u, ctx->map)) {
        int hx = 0;
        int hy = 0;
        if (ai_euro_land_war_hunt_target(ctx, nation_id, u->x, u->y, 1, 0, &hx, &hy)) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, hx, hy);
          land_war_hunted = 1;
        }
      } else {
        land_war_hunted = 1;
      }
    }
  }

  /*
   * Artillery fortify (case 0x0b fortify arm): idle Artillery on own colony →
   * FORTIFY (peace or war). Off-colony at war: siege hunt above. Cite:
   * euro_unit_act §2d3; Colonization.pdf Defending a Colony ("…or artillery");
   * king_ref Artillery siege fortify.
   */
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !miner_fielded && !farmer_fielded && !fisherman_fielded && !planter_fielded &&
      !workplace_assigned && !scout_explored && !land_war_hunted && !peace_border_hunted &&
      ai_euro_is_artillery_name(uname) && !ai_euro_land_is_fortified(u) && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      const ColonizeColony* c = colonies_get(ctx->colonies, cid);
      if (c && c->active && c->nation_id == nation_id &&
          ai_euro_fortify_with_quota(ctx, nation_id, u, cid)) {
        return;
      }
    }
  }

  /*
   * Missionary CONTACT (act-level): not at Euro peer war + Jesuit/Missionary,
   * not fleeing (Alarm ≥55 adjacent) → CONTACT at nearest tribe without mission
   * (mission==0xff) + AI_MOVE. Gate on Euro peer war only — indian_war_hunt
   * from relation_by_indian==0 (memset / unmet) must not block convert CONTACT.
   * Native hostility still covered by flee gate. Cite: Colonization.pdf
   * Establishing a Mission; euro_unit_act §2c6; indian_contact.md convert pulse.
   */
  if (!ai_euro_at_war_any_peer(ctx->col1_ok ? ctx->col1 : NULL, nation_id) && is_missionary &&
      !ai_euro_missionary_should_flee(ctx, nation_id, u->x, u->y)) {
    int tx = 0;
    int ty = 0;
    if (ai_euro_missionary_no_mission_target(ctx, u->x, u->y, &tx, &ty)) {
      /* Prio 3 > Scout ring CONTACT (2) so convert beats explore. */
      ai_goals_upsert_primary(nation_id, tx, ty, AI_GOAL_CONTACT, 3);
      if (u->x != tx || u->y != ty) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
      }
      missionary_contacted = 1;
    }
  }

  int goal_x = u->goto_x;
  int goal_y = u->goto_y;
  int goal_code = -1;
  {
    /*
     * FUN_521d_0a60 goal-consumption tail, structurally ported (see
     * ai_euro_0a60_goal_orders_structural above / euro_goal_orders_0a60_
     * full.md "Structural pilot port"): runs once per nation per turn,
     * scores every matching primary-goal slot in one real single-pass
     * scan (no soldier-first/founder-first two-phase hack), commits the
     * pick into the shadow state below. Read it back here.
     */
    const Ai0a60UnitState* st = &s_0a60_pilot_state[u->id];
    if (st->act_state == 0xb) {
      goal_x = st->goal_x;
      goal_y = st->goal_y;
      goal_code = st->goal_code;
    }

    /*
     * Threatened-Stockade LABOR override: reactive same-turn war-threat
     * check with no mapped 0a60 equivalent (labor urgency vs. war threat
     * isn't part of the mapped goal-table scan) — kept, overrides the
     * structural pick when it fires, same as before this section replaced
     * the old three-loop approximation.
     */
    if (ctx->colonies && at_war_land && uname &&
        (strstr(uname, "Free Colonist") != NULL || strstr(uname, "Colonist") != NULL) &&
        strstr(uname, "Soldier") == NULL) {
      for (int ti = 0; ti < COLONIZE_COLONIES_MAX; ++ti) {
        const ColonizeColony* tc = &ctx->colonies->colonies[ti];
        if (!tc->active || tc->nation_id != nation_id) {
          continue;
        }
        if (!ai_euro_colony_wants_construction_labor(ctx->colonies, tc)) {
          continue;
        }
        const ColonizeBuildingType* bt =
          tc->building_in_production >= 0
            ? colonies_building_type(ctx->colonies, tc->building_in_production)
            : NULL;
        if (!bt || strcmp(bt->name, "Stockade") != 0) {
          continue;
        }
        if (!ai_euro_colony_threatened_by_war(ctx, nation_id, tc)) {
          continue;
        }
        if (abs(tc->x - u->x) + abs(tc->y - u->y) <= 3) {
          goal_x = tc->x;
          goal_y = tc->y;
          goal_code = AI_GOAL_LABOR;
          ai_goals_upsert_primary(nation_id, tc->x, tc->y, AI_GOAL_LABOR, 6);
          break;
        }
      }
    }
  }

  /*
   * LABOR bind (5b66 case 0x0b unload/labor thin): idle colonist-capable land
   * unit near own colony with inventory food_short/tools_short → COLONY/LABOR
   * goto (overrides distant FOUND when adjacent/on-tile). Construction deepen:
   * idle Pioneer/Hardy on a colony with Stockade/Warehouse/Lumber Mill in
   * production stays for carpenter hammers (LABOR join) rather than leave —
   * structural only.
   * Food emergency deepen: food_short ≥ 4 extends search to MD≤8 for
   * food-capable colonist/Pioneer/Expert Farmer (manual 2 food/colonist).
   * Expert Farmer deepen: idle Expert Farmer (@JOB Farmer profession 0 or
   * display-name Farmer) → food-short LABOR when profession exists. Cite:
   * docs/building_production.md Farmer→Food; Colonization.pdf Skills Chart.
   * Free Colonist food LABOR (non-Expert Farmer): idle Free Colonist /
   * Colonist with food_short > 0 → MD≤8 toward hungry colony (same join as
   * Expert Farmer path, without requiring Farmer profession). Cite: manual
   * 2 food/colonist; 5cf6 food_short; euro_unit_act §2e. No invented rates.
   * Expert Lumberjack deepen: incomplete Warehouse/Lumber Mill (building type
   * exists) → LABOR join (lumber for hammers). Forest field-assign is handled
   * earlier (ai_euro_try_lumberjack_field_assign); this is the no-forest fallback.
   * Tools-short deepen
   * (peace Pioneer): tools_short > 0 extends MD≤8 toward tools-short colony
   * so idle Pioneer walks in for case-7 tools delivery. Cite: 5cf6 shortage
   * tallies + euro_unit_act §2d/§2e; no invented rates.
   */
  {
    const int is_pioneer =
      uname && (strstr(uname, "Pioneer") || strstr(uname, "Hardy"));
    const int is_farmer = ai_euro_unit_is_food_labor(ctx->units, u) &&
                          ((uname && strstr(uname, "Farmer") != NULL) ||
                           (u->profession == 0));
    /* Master Carpenter — hammer bind for Stockade/Warehouse/Lumber Mill. */
    const int is_carpenter =
      uname && strstr(uname, "Carpenter") != NULL;
    /* Expert Lumberjack — lumber for incomplete Warehouse/Lumber Mill. */
    const int is_lumberjack =
      uname && strstr(uname, "Lumberjack") != NULL;
    const int is_free_colonist =
      uname && (strstr(uname, "Free Colonist") != NULL ||
                (strstr(uname, "Colonist") != NULL && !is_pioneer && !is_farmer &&
                 !is_carpenter && !is_lumberjack && strstr(uname, "Soldier") == NULL));
    const int is_colonist_cap =
      uname && strstr(uname, "Soldier") == NULL && strstr(uname, "Dragoon") == NULL &&
      strstr(uname, "Scout") == NULL && !ai_euro_type_is_wagon_name(uname) &&
      (is_pioneer || is_farmer || is_carpenter || is_lumberjack ||
       strstr(uname, "Free Colonist") || strstr(uname, "Colonist") ||
       strstr(uname, "Farmer"));
    if (!land_war_hunted && !peace_border_hunted && !scout_explored && !treasure_routed &&
        !missionary_contacted && !wagon_hauled && !pioneer_improved &&
        !lumberjack_fielded && !miner_fielded && !farmer_fielded && !fisherman_fielded &&
        !planter_fielded && !workplace_assigned && is_colonist_cap &&
        ctx->colonies && !ai_euro_land_is_fortified(u)) {
      AiEuroInventory* inv = ai_goals_inventory(nation_id);
      const int short_labor =
        inv && (inv->tools_short > 0 || inv->food_short > 0);
      const int food_emergency = inv && inv->food_short >= 4;
      /* Peace Pioneer tools-short: walk toward short colony (MD≤8), not only
       * adjacent — feeds existing on-tile tools-delivery stand-in. */
      const int tools_pioneer_bind =
        !at_war_land && is_pioneer && inv && inv->tools_short > 0;
      /* Expert Farmer / food labor: food_short → MD≤8 toward hungry colony. */
      const int food_farmer_bind =
        ai_euro_unit_is_food_labor(ctx->units, u) && inv && inv->food_short > 0 &&
        (is_farmer || food_emergency);
      /* Free Colonist (non-Farmer): food_short → MD≤8 hungry LABOR join. */
      const int food_free_colonist_bind =
        is_free_colonist && !is_farmer && ai_euro_unit_is_food_labor(ctx->units, u) &&
        inv && inv->food_short > 0;
      /*
       * Master Carpenter construction LABOR: idle carpenter → Stockade/
       * Warehouse/Lumber Mill incomplete (same want_construction_labor gate
       * as Pioneer stay). Cite: docs/building_production.md Carpenter→Hammers;
       * Skills Chart Master Carpenter; euro_unit_act §2e Stockade pattern.
       */
      const int carpenter_bind = is_carpenter && !is_pioneer;
      /*
       * Expert Lumberjack LABOR: incomplete Warehouse/Lumber Mill when that
       * building type exists (no-forest fallback). Cite: building_production
       * Lumberjack→Lumber; Colonization.pdf Skills Chart. Field-assign is
       * earlier via ai_euro_try_lumberjack_field_assign.
       */
      const int lumberjack_bind = is_lumberjack && !is_pioneer;
      /*
       * Threatened Stockade: Free Colonist within MD≤3 prefers incomplete
       * Stockade LABOR over distant FOUND (defense hammers). Cite:
       * building_production.md Stockade; ai_euro_colony_threatened_by_war;
       * Colonization.pdf fortify / Stockade defense.
       */
      int threat_stockade_bind = 0;
      if (is_free_colonist && at_war_land && ctx->col1_ok && ctx->col1) {
        for (int ti = 0; ti < COLONIZE_COLONIES_MAX; ++ti) {
          const ColonizeColony* tc = &ctx->colonies->colonies[ti];
          if (!tc->active || tc->nation_id != nation_id) {
            continue;
          }
          if (!ai_euro_colony_wants_construction_labor(ctx->colonies, tc)) {
            continue;
          }
          const ColonizeBuildingType* bt =
            tc->building_in_production >= 0
              ? colonies_building_type(ctx->colonies, tc->building_in_production)
              : NULL;
          if (!bt || strcmp(bt->name, "Stockade") != 0) {
            continue;
          }
          if (!ai_euro_colony_threatened_by_war(ctx, nation_id, tc)) {
            continue;
          }
          if (abs(tc->x - u->x) + abs(tc->y - u->y) <= 3) {
            threat_stockade_bind = 1;
            break;
          }
        }
      }
      const int max_dist =
        (food_emergency && ai_euro_unit_is_food_labor(ctx->units, u)) ||
            tools_pioneer_bind || food_farmer_bind || food_free_colonist_bind
          ? 8
          : (threat_stockade_bind ? 3 : 1);
      int bx = -1;
      int by = -1;
      int best = 99;
      int code = AI_GOAL_COLONY;
      int b_construction = 0;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (!c->active || c->nation_id != nation_id) {
          continue;
        }
        const int dist = abs(c->x - u->x) + abs(c->y - u->y);
        if (dist > max_dist) {
          continue;
        }
        const int construction =
          ai_euro_colony_wants_construction_labor(ctx->colonies, c);
        const int lumber_need = ai_euro_colony_wants_lumberjack_labor(ctx->colonies, c);
        /*
         * On-tile Pioneer/Hardy: leave for tools-delivery stand-in unless
         * Stockade/Warehouse/Lumber Mill is in production (stay/LABOR for
         * hammers). Adjacent pioneers still LABOR-goto toward short colonies.
         * Master Carpenter on-tile always stays when construction wants labor.
         */
        if (dist == 0 && is_pioneer && !construction) {
          continue;
        }
        int need = construction || (c->population < 3);
        if (short_labor && inv->tools_short > 0 &&
            c->stock[COLONIZE_CARGO_TOOLS] < 20) {
          need = 1;
        }
        if (short_labor && inv->food_short > 0 &&
            c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
          need = 1;
        }
        /* Expert Farmer: food-short LABOR only (Skills Chart Food) — not tools. */
        if (is_farmer && !is_pioneer) {
          need = inv && inv->food_short > 0 &&
                 c->stock[COLONIZE_CARGO_FOOD] < c->population * 2;
        }
        /* Free Colonist MD>1 food bind: hungry colony only (not distant tools). */
        if (food_free_colonist_bind && dist > 1 && !threat_stockade_bind) {
          need = inv && inv->food_short > 0 &&
                 c->stock[COLONIZE_CARGO_FOOD] < c->population * 2;
        }
        /* Master Carpenter: construction LABOR only (hammers) — Stockade pattern. */
        if (carpenter_bind) {
          need = construction;
        }
        /* Expert Lumberjack: Warehouse/Lumber Mill lumber LABOR only. */
        if (lumberjack_bind) {
          need = lumber_need;
        }
        /* Free Colonist threat-Stockade: Stockade hammers only within MD≤3. */
        if (threat_stockade_bind && is_free_colonist) {
          const ColonizeBuildingType* sbt =
            construction && c->building_in_production >= 0
              ? colonies_building_type(ctx->colonies, c->building_in_production)
              : NULL;
          need = construction && sbt && strcmp(sbt->name, "Stockade") == 0 &&
                 ai_euro_colony_threatened_by_war(ctx, nation_id, c);
        }
        if (!need) {
          continue;
        }
        if (bx < 0 || dist < best) {
          best = dist;
          bx = c->x;
          by = c->y;
          code = AI_GOAL_LABOR;
          b_construction = construction;
        }
      }
      if (bx >= 0) {
        goal_x = bx;
        goal_y = by;
        goal_code = code;
        ai_goals_upsert_primary(
          nation_id, bx, by, code, (food_emergency || threat_stockade_bind || b_construction) ? 6 : 4
        );
      }
    }
  }

  if (goal_code == AI_GOAL_FOUND && u->x == goal_x && u->y == goal_y) {
    ai_euro_found_with_unit(ctx, u, nation_id);
    return;
  }
  /*
   * A founder can also arrive on a FOUND tile through the 20e6 move-scoring
   * gate, which writes the goto but leaves no 0a60 goal code behind. Nothing
   * then founded on arrival: settlers walked to the site and stood on it for
   * the rest of the game. Found when we are standing on this nation's own best
   * FOUND tile and the tile still takes a colony.
   */
  if (goal_code < 0 && !is_ship && uname &&
      (ai_euro_name_is_pioneer(uname) || strstr(uname, "Colonist") != NULL)) {
    int bfx = 0;
    int bfy = 0;
    if (ai_goals_best_found_tile_near(ctx->map, nation_id, u->x, u->y, &bfx, &bfy) &&
        bfx == u->x && bfy == u->y &&
        colonies_can_found(ctx->colonies, ctx->map, u->x, u->y)) {
      ai_euro_found_with_unit(ctx, u, nation_id);
      return;
    }
  }

  /*
   * Thin tools delivery (case 7 economy stand-in): idle/arriving Pioneer or
   * Hardy on own colony tile with tools_short / stock<20 → wagon TOOLS unload
   * when hired wagon present, else +10 TOOLS stand-in.
   * Wagon on colony also unloads its own TOOLS hold (hire-once deepen).
   * Dock expert hire / Artillery treasury gates live in 5d04 planning.
   */
  /* Wagon TRADE_GOODS → Europe sell (off-map / dock stand-in). */
  if (uname && ai_euro_type_is_wagon_name(uname) && ai_euro_in_europe(u->x, u->y)) {
    (void)ai_euro_try_transport_europe_sell(ctx, nation_id, u);
    u = units_get(ctx->units, u->id);
    if (!u || !u->active) {
      return;
    }
  }

  if (ctx->colonies) {
    const int here = colonies_id_at(ctx->colonies, u->x, u->y);
    if (here >= 0) {
      ColonizeColony* oc = colonies_get_mut(ctx->colonies, here);
      if (oc && oc->nation_id == nation_id) {
        if (uname && ai_euro_type_is_wagon_name(uname)) {
          (void)ai_euro_try_wagon_tools_delivery(ctx, nation_id, u, oc);
        } else {
          const int is_pioneer =
            uname && (strstr(uname, "Pioneer") || strstr(uname, "Hardy"));
          if (is_pioneer) {
            (void)ai_euro_try_pioneer_tools_delivery(ctx, nation_id, oc);
          }
        }
      }
    }
  }

  if ((goal_code == AI_GOAL_LABOR || goal_code == AI_GOAL_COLONY ||
       goal_code == AI_GOAL_COLONY_ALT) &&
      ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
    if (cid >= 0 && u->x == goal_x && u->y == goal_y) {
      /*
       * Garrison/Artillery stay for fortify quota — do not admit as LABOR
       * while quota remains. Quota 0 (early Isabella): admit Soldier as
       * colonist (TURN4→5 pop 1→2). Cite: test-saves-ai/TURN4–5.
       */
      int admit = !ai_euro_is_artillery_name(uname);
      if (admit && ai_euro_is_colony_garrison_name(uname)) {
        const ColonizeColony* jc = colonies_get(ctx->colonies, cid);
        admit = jc && jc->garrison_quota == 0;
      }
      if (admit) {
        ai_euro_join_colony(ctx, u, cid);
        return;
      }
    }
  }
  if (goal_code == AI_GOAL_MILITARY || goal_code == AI_GOAL_CONTACT) {
    if (abs(u->x - goal_x) <= 1 && abs(u->y - goal_y) <= 1) {
      /* Exclude self: a stale goal can point at a tile the unit itself now
       * occupies (e.g. just captured it opportunistically) — nothing to
       * attack there. */
      const int foe = units_id_at(ctx->units, goal_x, goal_y);
      if (foe >= 0 && foe != u->id) {
        ai_euro_try_attack(ctx, u, goal_x, goal_y);
        return;
      }
    }
    if (ctx->colonies && u->x == goal_x && u->y == goal_y) {
      const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
      if (cid >= 0) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
        if (c && c->nation_id != nation_id &&
            units_foreign_unit_at(ctx->units, u->x, u->y, u->id, nation_id) < 0) {
          int plunder = 0;
          for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
            if (c->stock[i] > 0) {
              plunder += c->stock[i];
            }
          }
          ColonizeColony snap = *c;
          if (colonies_capture(ctx->colonies, cid, nation_id)) {
            units_combat_notify_colony_captured(
              ctx->col1_ok ? ctx->col1 : NULL, &snap, nation_id, plunder
            );
          }
          return;
        }
      }
    }
  }

  /* Preserve land-war / peace-border / scout / treasure / missionary / wagon /
   * pioneer-improve / lumberjack/miner/farmer/fisherman/planter-field /
   * workplace / LABOR. */
  if (goal_code >= 0 && !land_war_hunted && !peace_border_hunted && !scout_explored &&
      !treasure_routed && !missionary_contacted && !wagon_hauled && !pioneer_improved &&
      !lumberjack_fielded && !miner_fielded && !farmer_fielded && !fisherman_fielded &&
      !planter_fielded && !workplace_assigned) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, goal_x, goal_y);
  }

  /*
   * Land goto advance (thin 20e6 multi-step): scored steps while moves_left
   * remain for FOUND / MILITARY / CONTACT, or act-level land war hunt /
   * peace-border / scout explore. Structural only — not full combat scoring.
   * Cite: euro_unit_act §2c3; FUN_521d_20e6. Deep combat×8 / −0x6790 PARKED.
   */
  if (units_orders_follow_goto(u->orders)) {
    const int drain =
      (goal_code == AI_GOAL_FOUND || goal_code == AI_GOAL_MILITARY ||
       goal_code == AI_GOAL_CONTACT || land_war_hunted || peace_border_hunted ||
       scout_explored);
    /* drain: while MP left; else one scored step (prior non-multi path). */
    for (;;) {
      if (!u->active || u->moves_left <= 0 || !units_orders_follow_goto(u->orders)) {
        break;
      }
      if (u->x == u->goto_x && u->y == u->goto_y) {
        break;
      }
      int dx = 0;
      int dy = 0;
      if (!ai_euro_score_move(ctx, u, u->goto_x, u->goto_y, &dx, &dy)) {
        break;
      }
      const int tx = u->x + dx;
      const int ty = u->y + dy;
      const int foe = units_id_at(ctx->units, tx, ty);
      if (foe >= 0) {
        ai_euro_try_attack(ctx, u, tx, ty);
        break;
      }
      if (!units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng)) {
        break;
      }
      if (!drain) {
        break; /* single step for non-FOUND/MILITARY/CONTACT/hunt/scout */
      }
    }
  } else {
    /* Peace fortify fallback (case 0x0b): idle garrison on own colony. */
    const char* name = units_display_name(ctx->units, u);
    if (!at_war_land && name && ai_euro_is_colony_garrison_name(name) && ctx->colonies &&
        !ai_euro_land_is_fortified(u)) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        (void)ai_euro_fortify_with_quota(ctx, nation_id, u, cid);
      }
    }
  }

  if (u->active && at_war_land && is_land_hunter && !ai_euro_land_is_fortified(u)) {
    (void)ai_euro_land_try_adjacent_colony_seize(ctx, u);
    if (u->active) {
      (void)ai_euro_land_try_adjacent_village_seize(ctx, u);
    }
    if (u->active) {
      ai_euro_land_try_adjacent_attack(ctx, u);
    }
  }

  /*
   * Sticky CONTACT re-hunt: if moves remain and an adjacent foreign Euro is
   * at war, chain try_attack while MP lasts (mirror land_try_adjacent_attack
   * multi-step; dispatcher sticky waves still apply). Deep 20e6 scoring PARKED.
   */
  if (u->active && u->moves_left > 0 && ctx->col1_ok && ctx->col1 &&
      !units_is_sea(ctx->units, u->id)) {
    for (int step = 0; step < 8 && u->active && u->moves_left > 0; ++step) {
      const int foe = ai_euro_land_best_adjacent_foe(ctx, u);
      if (foe < 0) {
        break;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      /* Sticky CONTACT is Euro-peer war only (Indians stay on contact/raid paths). */
      if (!f || f->nation_id < 0 || f->nation_id > 3) {
        break;
      }
      const int ml0 = u->moves_left;
      const int ax = u->x;
      const int ay = u->y;
      ai_euro_try_attack(ctx, u, f->x, f->y);
      if (!u->active || (u->moves_left >= ml0 && u->x == ax && u->y == ay)) {
        break;
      }
    }
  }
}

int ai_euro_use_full_dispatch(const ColonizeTurnContext* ctx) {
  (void)ctx;
  /*
   * Default: full dispatcher for all seeds (incl. seed-100 TURN goldens).
   * Opt into the retired ai_euro_early_turn script with AI_EURO_EARLY_FIXTURE=1
   * (or legacy AI_FULL_DISPATCH=0). Alignment work improves port fidelity —
   * do not grow the fixture to pass goldens.
   */
  const char* fixture = getenv("AI_EURO_EARLY_FIXTURE");
  if (fixture && fixture[0] && fixture[0] != '0') {
    return 0;
  }
  const char* force = getenv("AI_FULL_DISPATCH");
  if (force && force[0] == '0') {
    return 0;
  }
  return 1;
}

void ai_euro_dispatcher_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }

  /* Colony fortification defense for adjacent resolve_land_combat (not only try_move). */
  units_set_combat_colonies(ctx->colonies);

  /* 0. Sticky clear */
  s_sticky_unit = -1;
  s_sticky_count = 0;
  memset(s_deferred_found, 0, sizeof(s_deferred_found));
  memset(s_unloaded_this_turn, 0, sizeof(s_unloaded_this_turn));
  memset(s_founded_colony_turn, 0, sizeof(s_founded_colony_turn));
  /*
   * 0a60 goal-consumption shadow state: reset every call rather than kept
   * across turns like DOS's real unit+0x314b/c/d/e bytes — same "recompute
   * fresh within the turn" simplification s_deferred_found/
   * s_founded_colony_turn above already use, and it's fully repopulated by
   * ai_euro_0a60_goal_orders_structural below before ai_euro_unit_act reads
   * it back later this same call. Avoids stale cross-scenario garbage
   * (distinct unit pools/tests reusing small unit ids) driving a unit into
   * a found/labor-bind action on turn 1 from a previous run's leftover
   * state — real gameplay only ever has one live unit pool, so this is a
   * safety/test-hygiene fix, not a behavior change within a real game.
   */
  memset(s_0a60_pilot_state, 0, sizeof(s_0a60_pilot_state));
  /* FUN_521d_0a60 entry: memset(0xa13c,0,16) — per-continent explorer count
   * read by FUN_521d_20e6's explorer cap (s_20e6_explorers). */
  memset(s_20e6_explorers, 0, sizeof(s_20e6_explorers));

  /* 1–3. Colony + unit inventory */
  ai_euro_colony_inventory(ctx, nation_id);
  ai_euro_unit_inventory(ctx, nation_id);

  /* 4. Treaty timers BEFORE plan (not war RNG). */
  ai_diplo_treaty_timers(ctx, nation_id);

  /* 5. Plan: 5d04 → 0342 → 0a60 */
  ai_euro_nation_planning(ctx, nation_id);
  ai_goals_promote_secondary_to_primary(nation_id);
  ai_euro_cancel_stale_zero_hammer_builds(ctx, nation_id);
  /* Peace Stockade→Fort→Fortress→Warehouse→Docks, coastal Drydock→Shipyard,
   * then Stuyvesant Custom House, then Church (after Stockade); before LABOR. */
  ai_euro_prefer_peace_construction(ctx, nation_id);
  ai_euro_prefer_coastal_drydock(ctx, nation_id);
  ai_euro_prefer_coastal_shipyard(ctx, nation_id);
  ai_euro_prefer_custom_house(ctx, nation_id);
  ai_euro_prefer_church(ctx, nation_id);
  ai_euro_prefer_printing_press(ctx, nation_id);
  ai_euro_prefer_schoolhouse(ctx, nation_id);
  ai_euro_prefer_newspaper(ctx, nation_id);
  ai_euro_prefer_college(ctx, nation_id);
  ai_euro_prefer_university(ctx, nation_id);
  ai_euro_prefer_cathedral(ctx, nation_id);
  ai_euro_prefer_armory_at_war(ctx, nation_id);
  ai_euro_prefer_magazine_at_war(ctx, nation_id);
  ai_euro_prefer_arsenal_at_war(ctx, nation_id);
  ai_euro_prefer_stable(ctx, nation_id);
  ai_euro_prefer_carpenters_shop(ctx, nation_id);
  ai_euro_prefer_lumber_mill(ctx, nation_id);
  ai_euro_prefer_blacksmiths_house(ctx, nation_id);
  ai_euro_prefer_blacksmiths_shop(ctx, nation_id);
  ai_euro_prefer_iron_works(ctx, nation_id);
  ai_euro_prefer_craft_upgrades(ctx, nation_id);
  ai_euro_clear_pre_stockade_build_queue(ctx, nation_id);
  ai_euro_colony_goals(ctx, nation_id);
  /* FUN_521d_0a60 goal-consumption tail (structural port) — picks each
   * idle unit's next goal into the s_0a60_pilot_state shadow; consumed by
   * ai_euro_unit_act below. See the function's header comment for scope. */
  ai_euro_0a60_goal_orders_structural(ctx, nation_id);

  /* Opportunistic balance after plan (separate from timer slot). */
  ai_diplo_euro_balance(ctx, nation_id);

  /* Treasure → Europe gold: Expected→Harbor due ships + live Europe/HS units
   * (moves_left may be 0 on Europe dock ships). Cortes coastal king-galleon
   * cash (shared units_cortes_cash_coastal_treasures). Cite: Treasure Trains. */
  ai_euro_try_expected_treasure_harbor(ctx, nation_id);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    (void)ai_euro_try_cash_treasure_europe(ctx, nation_id, u);
  }
  (void)units_cortes_cash_coastal_treasures(
    ctx->units, ctx->colonies, ctx->map, ctx->europe, ctx->col1, nation_id
  );

  /* 6–7. Outer any_acted; wave0 ships; wave1 ships+land; high→low.
   * Each unit gets one act call per outer iteration (inner while breaks). */
  int any_acted;
  int guard = 0;
  do {
    any_acted = 0;
    for (int wave = 0; wave < 2; ++wave) {
      for (int i = COLONIZE_UNITS_MAX - 1; i >= 0; --i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
          continue;
        }
        const int is_ship = ai_euro_is_ship_type(ctx->units, u->id);
        const int in_wave = (wave != 0) || is_ship;
        if (!in_wave) {
          continue;
        }
        /*
         * First-colony sentry settlers (Dutch Isabella pioneer) skip overnight
         * MP — still run unit_act so wake+found can fire. Cite: TURN3→4.
         */
        if (u->moves_left <= 0) {
          if (is_ship || ai_euro_colony_count(ctx->colonies, nation_id) != 0) {
            continue;
          }
          const char* wn = units_display_name(ctx->units, u);
          if (!ai_euro_name_is_pioneer(wn) && !ai_euro_name_is_soldier(wn)) {
            continue;
          }
          /* Only wake when found-approach eligibility can fire (not beachhead). */
          const int settler_aboard = ai_euro_nation_settler_aboard(ctx, nation_id);
          const int pioneer_aboard = ai_euro_nation_pioneer_aboard(ctx, nation_id);
          if (ai_euro_name_is_soldier(wn)) {
            if (!pioneer_aboard && settler_aboard) {
              continue;
            }
            /* Same-act beachhead: soldier on found+1 with pioneer aboard — no wake. */
            if (pioneer_aboard) {
              int fx = 0;
              int fy = 0;
              if (u->goto_x >= 0 && u->goto_y >= 0 &&
                  ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, u->goto_x, u->goto_y, &fx, &fy) &&
                  u->x == fx && u->y == fy + 1) {
                continue;
              }
            }
            /*
             * Outer-loop re-entry: soldier already on found after a same-turn
             * walk must not found until the next dispatcher turn (TURN3→4
             * Quebec step-on; TURN4→5 founds on guard==0). Cite: TURN3–5.
             */
            if (guard > 0) {
              int fx = 0;
              int fy = 0;
              int lf_x = u->goto_x;
              int lf_y = u->goto_y;
              if (lf_x < 0 || lf_y < 0 ||
                  !ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &fx, &fy)) {
                for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
                  const ColonizeUnit* sh = &ctx->units->units[si];
                  if (!sh->active || sh->nation_id != nation_id ||
                      !units_is_sea(ctx->units, sh->id)) {
                    continue;
                  }
                  int rx = 0;
                  int ry = 0;
                  if (ai_euro_recover_landfall_from_ship(sh->x, sh->y, &rx, &ry)) {
                    lf_x = rx;
                    lf_y = ry;
                    break;
                  }
                }
              }
              if (lf_x >= 0 && lf_y >= 0 &&
                  ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &fx, &fy) &&
                  u->x == fx && u->y == fy) {
                continue;
              }
            }
          } else if (settler_aboard) {
            continue;
          } else {
            /* Pioneer ashore, cargo empty: wake only on found tile or cruise. */
            int lf_x = u->goto_x;
            int lf_y = u->goto_y;
            int fx = 0;
            int fy = 0;
            int ok = 0;
            if (lf_x < 0 || lf_y < 0 ||
                !ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &fx, &fy)) {
              for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
                const ColonizeUnit* sh = &ctx->units->units[si];
                if (!sh->active || sh->nation_id != nation_id ||
                    !units_is_sea(ctx->units, sh->id)) {
                  continue;
                }
                int rx = 0;
                int ry = 0;
                if (ai_euro_recover_landfall_from_ship(sh->x, sh->y, &rx, &ry)) {
                  lf_x = rx;
                  lf_y = ry;
                  break;
                }
              }
            }
            if (lf_x >= 0 && lf_y >= 0 &&
                ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lf_x, lf_y, &fx, &fy)) {
              if (u->x == fx && u->y == fy) {
                ok = 1;
              } else if (guard == 0) {
                /*
                 * Mid-march toward found: one outer pass only so SP one-hop
                 * (TURN3→4 → 46,52) is not re-woken into the town same turn.
                 */
                int wx = 0;
                int wy = 0;
                if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy)) {
                  for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
                    const ColonizeUnit* sh = &ctx->units->units[si];
                    if (sh->active && sh->nation_id == nation_id &&
                        units_is_sea(ctx->units, sh->id) &&
                        ((sh->goto_x == wx && sh->goto_y == wy) ||
                         ai_euro_chebyshev(sh->x, sh->y, wx, wy) <= 1)) {
                      ok = 1;
                      break;
                    }
                  }
                }
              }
            }
            if (!ok) {
              continue;
            }
          }
        }

        if (u->id == s_sticky_unit) {
          s_sticky_count++;
          if (s_sticky_count > 0x14) {
            units_clear_orders(ctx->units, u->id);
            s_sticky_unit = -1;
            s_sticky_count = 0;
            continue;
          }
        } else {
          s_sticky_unit = u->id;
          s_sticky_count = 0;
        }

        const int was_ship = is_ship;
        const int before_moves = u->moves_left;
        const int before_x = u->x;
        const int before_y = u->y;
        ai_euro_unit_act(ctx, u, nation_id);

        const int progressed =
          !u->active || u->moves_left < before_moves || u->x != before_x || u->y != before_y;
        /*
         * FUN_5bfb_3180 runs on the MOVING unit's step for AI units too: an
         * AI unit that ends its act adjacent to the human's units or colonies
         * opens the FUN_5bfb_153e audience with DOS param_4 = the AI's own
         * unit — which is what arms the @WANTSTUFF demand phase (the
         * human-move path in game_loop passes the human's unit, so the demand
         * gate `unit[param_4].owner == target` fails there, as in DOS).
         * No-ops without ctx->ai_popups (headless/golden harness) and the
         * encounter dedupes once per pair per turn.
         */
        if (progressed && u->active && units_is_on_map(u) && ctx->human_nation >= 0 &&
            ctx->human_nation <= 3 && nation_id != ctx->human_nation && ctx->ai_popups) {
          int near_human = 0;
          for (int ady = -1; ady <= 1 && !near_human; ++ady) {
            for (int adx = -1; adx <= 1 && !near_human; ++adx) {
              if (adx == 0 && ady == 0) {
                continue;
              }
              const int oid = units_id_at(ctx->units, u->x + adx, u->y + ady);
              const ColonizeUnit* o = oid >= 0 ? units_get_const(ctx->units, oid) : NULL;
              if (o && o->active && o->nation_id == ctx->human_nation) {
                near_human = 1;
              }
              if (!near_human && ctx->colonies) {
                const int ccid = colonies_id_at(ctx->colonies, u->x + adx, u->y + ady);
                const ColonizeColony* cc = ccid >= 0 ? colonies_get(ctx->colonies, ccid) : NULL;
                if (cc && cc->active && cc->nation_id == ctx->human_nation) {
                  near_human = 1;
                }
              }
            }
          }
          if (near_human) {
            (void)ai_diplo_153e_encounter(ctx, ctx->human_nation, nation_id, u->id);
          }
        }
        if (progressed) {
          any_acted = 1;
          if (u->active && u->id == s_sticky_unit) {
            s_sticky_count = 0; /* progress resets anti-spin */
          }
        } else if (u->id == s_sticky_unit) {
          /* no-op act still counts toward sticky via the increment above */
        }

        if (was_ship && u->active && u->moves_left <= 0) {
          ai_goals_upsert_primary(nation_id, u->x, u->y, AI_GOAL_CONTACT, 2);
        }
      }
    }
    ++guard;
  } while (any_acted && guard < 64);

  /*
   * FUN_5952_035e colonist re-placement runs after the unit acts so the
   * admit-time expert field-assign paths (which need a free tile) still
   * land; the tick then re-scores everyone with real professions.
   */
  ai_euro_colony_tick_28c8_reassign(ctx, nation_id);
}
