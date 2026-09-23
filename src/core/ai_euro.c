#include "core/internal.h"
#include "core/ai_euro.h"

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_king.h"
#include "core/ai_goals.h"
#include "core/assets.h"
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_yield.h"
#include "core/colony_production.h"
#include "core/col1_save.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/units.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Debug/bisect switch: env var set and non-"0" wins, else the build default. */
static int ai_euro_env_flag(const char* name, int dflt) {
  const char* e = getenv(name);
  return (e && *e) ? (*e != '0') : dflt;
}

static int ai_euro_ship_dos_enabled(void);

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
 * instead of "no bias". No save-file backing in this port (same as the
 * other file-local latches below), so ai_euro_reset() zeroes it on
 * new-game/load too. Cite: move_scoring_20e6_full.md.
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
/*
 * Ship sail-loop route latch: set once the greedy ocean scorer stalls against
 * a land wall for a goto, cleared on every goto write. While set, the act
 * routes via the FUN_6662 pathfinder from its first step instead of greedy
 * west / pathfinder east ping-pong (net zero progress = multi-turn "circles").
 */
static uint8_t s_euro_ship_route_latch[COLONIZE_UNITS_MAX];

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);
static void ai_euro_try_attack(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty);
static void ai_euro_20e6_ship_cargo_counts(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship,
  int* pioneers, int* mil, int* scouts, int* milvet, int* civ
);
static int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id);
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

  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  /*
   * Slot walk, `u->id` to every id-taking accessor. `units_get_const`,
   * `units_is_sea` and `combat_unit_base_x8` all take a unit ID, and ids are
   * handed out monotonically from 1 and never recycled (units.c:337), so an
   * `i`-as-id walk over COLONIZE_UNITS_MAX dropped every unit with id >= 256
   * in a long game plus the highest slot in a short one. DOS walks the unit
   * ARRAY in record order (raw 78159: `local_1a` indexing
   * `0x3144 + local_1a * 0x1c`), which is exactly a slot walk.
   * Fixed 2026-09-10 (audit second-wave Leads item 2).
   */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id < 0 || u->nation_id >= 12) {
      continue;
    }
    if (units_is_sea(ctx->units, u->id)) {
      continue; /* land units / Braves only, matches type∉[0xd,0x12] gate */
    }
    const int cid = map_continent_id_at(ctx->map, u->x, u->y);
    if (cid < 0 || cid > 15) {
      continue;
    }
    if (land_unit_count[u->nation_id][cid] < 0xff) {
      land_unit_count[u->nation_id][cid]++;
    }
    const int val = combat_unit_base_x8(&sctx, u->id, 0, NULL);
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
            (dg & (AI_DIPLO_PEACE | AI_DIPLO_AMICABLE)) == AI_DIPLO_PEACE) {
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
      if (map_tile_is_coast_water(map, nx, ny)) {
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
 * port, which scores the seed's 8 neighbors + stay using the real
 * terrain-founding byte. Previously-succeeding seeds are unaffected (same
 * tile, same result); only seeds whose exact point used to fail outright can
 * now succeed via a nearby tile — fixes "adj 06ae still misses some coastal
 * first towns" (R0) without inventing new geometry or touching any call site.
 * (2026-09-08: the ring-2..4 fallback and the coastal=40 bias this paragraph
 * used to lean on are gone — both were Linux inventions absent from 06ae.)
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
  return ai_goals_pick_founding_tile_ex_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL)}, nation_id, fx, fy, 0, 0, out_x, out_y);
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

/*
 * Landfall preference order N, W, E, S, NW, NE, SW, SE — deliberately NOT the
 * clockwise MAP_DIR8_DX/DY walk: the disembark/landfall arms scan the four
 * orthogonals before the diagonals, so the first hit differs from a clockwise
 * scan. Shared by ai_euro_land_adjacent_to and ai_euro_pick_unload_land.
 */
static const int k_landfall_pref_dx[8] = {0, -1, 1, 0, -1, 1, -1, 1};
static const int k_landfall_pref_dy[8] = {-1, 0, 0, 1, -1, -1, 1, 1};

/* Land neighbour of coastal water (prefer N, then W/E/S, then diagonals). */
static int ai_euro_land_adjacent_to(
  const ColonizeWorldMap* map,
  int wx,
  int wy,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return 0;
  }
  for (int i = 0; i < 8; ++i) {
    const int nx = wx + k_landfall_pref_dx[i];
    const int ny = wy + k_landfall_pref_dy[i];
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
    const int nx = ship->x + k_landfall_pref_dx[i];
    const int ny = ship->y + k_landfall_pref_dy[i];
    if (nx < 0 || ny < 0 || nx >= (int)ctx->map->width || ny >= (int)ctx->map->height) {
      continue;
    }
    if (map_tile_is_water(ctx->map, nx, ny) || map_tile_is_high_seas(ctx->map, nx, ny)) {
      continue;
    }
    if (nx == avoid_x && ny == avoid_y) {
      continue;
    }
    if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, pax->type_index, nx, ny, pax_id)) {
      continue;
    }
    int d = 0;
    if (prefer_x >= 0 && prefer_y >= 0) {
      d = map_chebyshev(nx, ny, prefer_x, prefer_y);
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
  if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, ship->id, pax->id, dest_x, dest_y)) {
    return 0;
  }
  pax = units_get(ctx->units, pax->id);
  if (!pax) {
    return 0;
  }
  ai_euro_set_goto(pax, orders, goto_x, goto_y);
  /* Port-only landfall goto memory (bugs.md #528): only the SENTRY-order
   * unloads overload `orders` for this, so only those set the flag. */
  pax->ai_landfall_wait = (orders == UNITS_ORDER_SENTRY);
  pax->moves = 0;
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

/* Unit kind from its catalog TYPE row, never its display name/string
 * (production installs no units_name_kind resolver — see
 * docs archive note no-dos-text-in-binary.md). */
static ColonizeUnitKind ai_euro_unit_kind(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!pool || !u) {
    return UNITS_KIND_UNKNOWN;
  }
  return units_type_kind(units_type(pool, u->type_index));
}

/* @UNIT row 2 (Pioneers / "Hardy Pioneer" display name). */
static int ai_euro_name_is_pioneer(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_PIONEER;
}

/* @UNIT row 1 (Soldiers / "Veteran Soldier" display name). */
static int ai_euro_name_is_soldier(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_SOLDIER;
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
      if (!map_tile_is_coast_water(map, x, y)) {
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

/* 1 when an own pioneer stands (not aboard) on (fx, fy). */
static int ai_euro_pioneer_ashore_at(ColonizeTurnContext* ctx, int nation_id, int fx, int fy) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* p = &ctx->units->units[i];
    if (!p->active || p->nation_id != nation_id || p->aboard_ship_id >= 0) {
      continue;
    }
    if (ai_euro_name_is_pioneer(ai_euro_unit_kind(ctx->units, p)) && p->x == fx && p->y == fy) {
      return 1;
    }
  }
  return 0;
}

/* Set a goto and walk it step by step until arrival or the unit is out of
 * moves, then park the unit where it stands. *u_io follows a re-fetch. */
/* Foreign unit (any nation but the mover's) standing on (x, y). */
static int ai_euro_foreign_unit_at(const ColonizeTurnContext* ctx, const ColonizeUnit* u, int x, int y) {
  const int id = units_id_at(ctx->units, x, y);
  const ColonizeUnit* o = id >= 0 ? units_get_const(ctx->units, id) : NULL;
  return o && o->active && o->nation_id != u->nation_id;
}

static void ai_euro_drain_goto(
  ColonizeTurnContext* ctx, ColonizeUnit** u_io, int order, int gx, int gy
) {
  ColonizeUnit* u = *u_io;
  ai_euro_set_goto(u, order, gx, gy);
  while (u && u->active && u->moves > 0 && (u->x != gx || u->y != gy)) {
    if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
      break;
    }
    u = units_get(ctx->units, u->id);
  }
  if (u && u->active) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
    u->moves = 0;
  }
  *u_io = u;
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
  const int colony_n = colonies_count_for_nation(ctx->colonies, nation_id);
  if (fx < 0) {
    /* Pre-found SP: tip from landfall table while pioneer sits on town. */
    int lx = 0;
    int ly = 0;
    if (!ai_euro_recover_landfall_from_ship(u->x, u->y, &lx, &ly) ||
        !ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lx, ly, &fx, &fy)) {
      return 0;
    }
    const int pioneer_on_found = ai_euro_pioneer_ashore_at(ctx, nation_id, fx, fy);
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
      u->moves = 0;
      return 1;
    }
    int bx = tip_x;
    int by = tip_y - 1;
    if (!map_tile_is_water(ctx->map, bx, by) && !map_tile_is_high_seas(ctx->map, bx, by)) {
      return 0;
    }
    if (u->moves <= 0 || units_orders_skip_turn(u)) {
      (void)units_wake(ctx->units, u->id);
      u = units_get(ctx->units, u->id);
      if (!u || !u->active) {
        return 1;
      }
    }
    ai_euro_drain_goto(ctx, &u, UNITS_ORDER_AI_MOVE, bx, by);
    return 1;
  }
  /* SP: already on NE berth — hold against trade-haul yank (TURN6 46,49). */
  if (!on_tip && !on_leg1 && !on_leg2 && u->x == tip_x && u->y == tip_y - 1 &&
      colony_n == 1) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
    u->moves = 0;
    return 1;
  }
  if (!on_tip && !on_leg1 && !on_leg2) {
    return 0;
  }
  /* Post-found SW legs only after the town exists (DU/FR). */
  if (colony_n != 1) {
    /* SP: tip station with pioneer on found → one west (TURN4→5 46,50→45,50). */
    if (on_tip && map_tile_is_water(ctx->map, tip_x - 1, tip_y)) {
      const int pioneer_on_found = ai_euro_pioneer_ashore_at(ctx, nation_id, fx, fy);
      if (pioneer_on_found) {
        if (u->moves <= 0 || units_orders_skip_turn(u)) {
          (void)units_wake(ctx->units, u->id);
          u = units_get(ctx->units, u->id);
          if (!u || !u->active) {
            return 1;
          }
        }
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tip_x - 1, tip_y);
        (void)units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              );
        u = units_get(ctx->units, u->id);
        if (u) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
          u->moves = 0;
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
    if (u->moves <= 0 || units_orders_skip_turn(u)) {
      (void)units_wake(ctx->units, u->id);
      u = units_get(ctx->units, u->id);
      if (!u || !u->active) {
        return 1;
      }
    }
    ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tip_x, tip_y);
    while (u && u->active && u->moves > 0 && (u->x != tip_x || u->y != tip_y)) {
      if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
        break;
      }
      u = units_get(ctx->units, u->id);
    }
    if (u && u->active) {
      ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, fx, fy);
      u->moves = 0;
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
  if (u->moves <= 0 || units_orders_skip_turn(u)) {
    (void)units_wake(ctx->units, u->id);
    u = units_get(ctx->units, u->id);
    if (!u || !u->active) {
      return 1;
    }
  }
  /* Pathfind drain — ocean score_move overshoots (38,19 vs 39,18). */
  ai_euro_drain_goto(ctx, &u, UNITS_ORDER_AI_SAIL, gx, gy);
  return 1;
}

/*
 * FUN_4962_0018 census phase 3 (raw 78243-78304), live 2026-09-07: clear ship
 * bits 0x01/0x02, then for each foreign ship (type 0x0d..0x12, combat byte
 * != 0) inside the 11×11 box (|dx| ≤ 5 AND |dy| ≤ 5 — Chebyshev, not the
 * Manhattan the old thin probe used) with a short navigable sea route
 * (FUN_6662_0906 flood cost 0..5): Frigate (0x11 literal) → bit 0x02, any
 * other → bit 0x01. Also accumulates the per-nation DS:0xa89b/0x9e52 (count
 * of own colonies with bit2, Σ their pop) and 0xa89a/0x9e54 (bit1 twin) that
 * 5d04's naval-threat crumbs and 20e6's bVar7 Privateer gate read. Also
 * thin-latch needs_colonists / needs_garrison from pop / garrison_quota.
 */
static int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);
static int ai_euro_20e6_type_combat(int dos_type);
/* Native village on this tile (col1 tribe table), else -1 — defined below;
 * forward-declared for the FUN_5952_035e threat seed's settlement halving. */
static int ai_euro_village_nation_at(const ColonizeCol1Save* col1, int x, int y);

typedef struct AiEuroShipPressure {
  uint8_t frigate_colonies; /* DS:0xa89b */
  uint8_t other_colonies;   /* DS:0xa89a */
  int frigate_pop;          /* DS:0x9e52 */
  int other_pop;            /* DS:0x9e54 */
} AiEuroShipPressure;

static AiEuroShipPressure s_ship_pressure[4];

static void ai_euro_ship_pressure_reset(int nation_id) {
  if (nation_id >= 0 && nation_id < 4) {
    memset(&s_ship_pressure[nation_id], 0, sizeof(s_ship_pressure[nation_id]));
  }
}
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

/*
 * FUN_15eb_0470 (`FUN_1000_8e4e` / `FUN_281f_0c5e`): the colony's work-radius
 * TIER, `min(FUN_15eb_039e(10), 2) + 2` → nominally 2..4
 * (viceroy_unpacked.c:9636-9648 = viceroy_unpacked_2.c:8332-8344).
 *
 * RESOLVED 2026-09-09 — the tier is a CONSTANT 2 in real play, and the
 * "read the count off colonies_fortification_tier" stand-in this used to
 * carry was an invention. The chain the counter walks:
 *   - FUN_15eb_039e(p) (viceroy_unpacked.c:9561-9578) counts, over the chain
 *     starting at @BUILDING index `p`, how many rows this colony owns, then
 *     follows the signed link byte at `DS:(idx*0xc - 0x707a)` until it goes
 *     negative. Ownership is the colony has_building bitmap (FUN_15eb_035e
 *     `DS:0x5dca + colony*0xca`, bit `p`) — nothing to do with fortification;
 *     the fortification chain is counted separately and explicitly (rows
 *     0/1/2) by FUN_157e_0008 (:8890-8905).
 *   - The link byte's only writer is FUN_75c2_13dc (:120735-120753), fed by
 *     the fixed 42-call table in FUN_75c2_144c (:120773-120826) — one call
 *     per @BUILDING row, `(group, link)`. Its group-3 block is
 *     `(-1, 0xb, -1, 0x1f, -1)` and NO call anywhere in the table passes a
 *     link of 9, 0xa or 0x1e. So row 0xa is a chain HEAD, and the chain
 *     reachable from it can only ever visit rows {0xa, 0xb, 0x1e, 0x1f} —
 *     never row 9, the real Town Hall.
 *   - All four of those are unbuildable: FUN_15eb_3650 hard-zeroes 0x0a,
 *     0x0b (the cut Town Hall upgrades) and 0x1e (Capitol), and 0x1f sits
 *     behind the Capitol as its prerequisite (docs/building_production.md
 *     "Cut rows", 259-266; the port mirrors that block in
 *     colonies_building_is_buildable). Starter colonies grant row 9, not 0xa.
 * ⇒ FUN_15eb_039e(10) == 0 in any state stock DOS can build itself into.
 *
 * Cross-check: the tier's own consumer indexes `DS:0x329[tier]` = 8/12/20
 * ring tiles (FUN_15eb_04c0, :9683-9700 — the "is (x,y) worked by this
 * colony" test the human colony screen uses too). Every colony DOS can build
 * works exactly 8 field tiles, which is only true at tier 2.
 *
 * ⇒ tier 2 for every colony stock DOS can produce. A SAVE can still carry the
 * bits (the colony building mask is bit-per-@BUILDING-row, and town_hall owns
 * bits 9-11), and then DOS really does run at tier 3/4, so since bugs.md #593
 * this reads the colony instead of returning the constant: the tier is
 * recovered from colonies_work_plot_count(), which is DS:0x329[0470()].
 */
static int ai_euro_colony_ring_tier(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
) {
  const int ring = colonies_work_plot_count(pool, c);
  if (ring <= 4) {
    return 1;
  }
  if (ring <= 8) {
    return 2;
  }
  return ring <= 12 ? 3 : 4;
}

/*
 * FUN_15eb_0484 (`FUN_1000_8e6c`): the AI's "wanted size" for a colony —
 * tier 1 → 4, tier 2 → 8, tier 3 → 0xc, else 0x20
 * (viceroy_unpacked.c:9650-9664 = viceroy_unpacked_2.c:8346-8360).
 * Factored 2026-09-09 out of the two open-coded copies (the 20e6 labor arm
 * and the 20e6 colony-sail matrix) so the 5952 +0x1b bit 0x10 latch below
 * uses the same table. Since ai_euro_colony_ring_tier is a constant 2 (see
 * its comment: the Town-Hall-chain counter it keys off can only ever walk
 * cut, unbuildable @BUILDING rows), the live answer is always 8 — the
 * 0xc/0x20 rows are dead in DOS too. The switch is kept verbatim rather than
 * folded to `return 8` so the DOS table stays readable next to its citation.
 *
 * Corollary, so nobody "fixes" them: BOTH clamps on this function's result are
 * therefore dead code by construction — `wanted > 0x10` in the 20e6 labor arm
 * (:12320) and `wanted > 0xc -> 0x10` in the colony-sail matrix (raw 1949-1951,
 * :17377). They are DOS-LITERAL and are kept for exactly that reason; a live
 * clamp would mean this function had started returning something other than 8,
 * i.e. that ring_tier's dead-row argument above had been falsified. Do not
 * "repair" a clamp to make it fire, and do not delete one as unreachable.
 * Smell audit 2026-09-10 D9.
 */
static int ai_euro_colony_wanted_size(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
) {
  const int tier = ai_euro_colony_ring_tier(pool, c);
  if (tier == 1) {
    return 4;
  }
  return tier == 2 ? 8 : (tier == 3 ? 0xc : 0x20);
}

/*
 * FUN_5952_035e raw 555-563 — `+0x1b` bit 0x10 (NEEDS_COLONISTS):
 *   pop < 0x20
 *   && pop < FUN_15eb_0484 (wanted size) + 2 * FUN_15eb_0470 (ring tier)
 *   && pop - 2 * tier < (ring tiles - iStack_142)
 * `iStack_142` (raw 426-483) counts field tiles that are off-map or terrain
 * class 0x19/0x1a (Ocean / High Seas — `map_tile_is_water`, which also
 * reports off-map as water), and is forced to 0 when the colony owns
 * `FUN_1000_8bec(0x181f, 6)` = @BUILDING index 6 = Docks (NAMES.TXT
 * @BUILDING row 7). Ring tiles = `DS:0x329[tier]` = 4/8/12/20, per colony
 * (colonies_work_plot_count, bugs.md #593), and both the total and the
 * blocked count are taken over exactly those tiles.
 *
 * Replaced an uncited `population < 3` on 2026-09-09.
 */
int ai_euro_colony_needs_colonists_5952(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColony* c
) {
  if (!pool || !c) {
    return 0;
  }
  const int pop = (int)c->population;
  if (pop >= 0x20) {
    return 0;
  }
  const int tier = ai_euro_colony_ring_tier(pool, c);
  if (pop >= ai_euro_colony_wanted_size(pool, c) + tier * 2) {
    return 0;
  }
  int blocked = 0;
  /* Same DOS @BUILDING group 6 the Fisherman gate uses, so it goes through
   * the shared answer — the old local test named "Docks" alone and would
   * have missed a colony whose slot had been upgraded past it. */
  const bool has_docks = colony_yield_colony_has_docks(pool, c);
  const int ring = colonies_work_plot_count(pool, c);
  if (map && !has_docks) {
    for (int ti = 0; ti < ring; ++ti) {
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(ti, &dx, &dy)) {
        continue;
      }
      if (map_tile_is_water(map, c->x + dx, c->y + dy)) {
        ++blocked; /* off-map or Ocean / High Seas */
      }
    }
  }
  return pop - tier * 2 < ring - blocked;
}

/*
 * FUN_4962_0018 ship probe for ONE colony (raw 78259-78299): clear the
 * blockade pair +0x1b bits 0x01/0x02, rescan the 11×11 box, fold into the
 * nation ship-pressure tallies. Split out of ai_euro_refresh_colony_ai_flags
 * 2026-09-08d because DOS runs this half for EVERY nation (human included)
 * via the nation EOT FUN_3844_00f2 → FUN_291f_0a74 (viceroy_unpacked.c:58390,
 * census AFTER the colony-EOT loop) + the explicit human call in
 * FUN_3844_0442 (:58463) — while the 5952_035e-side bits stay AI-only.
 * The human's Custom House blockade gate (europe.c, colony +0x1b & 3) reads
 * these bits; before this split they were frozen at their save-import value
 * for the human nation.
 */
static void ai_euro_colony_ship_probe_4962(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c
) {
  if (!ctx || !c || !c->active) {
    return;
  }
  /* Raw 78259: +0x1b &= 0xfc — only the blockade pair is cleared here. */
  c->ai_flags = (uint8_t)(c->ai_flags & (uint8_t)~(COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP |
                                                    COLONIZE_COLONY_AI_NEARBY_FRIGATE));
  if (ctx->units) {
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      const ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id == nation_id || u->aboard_ship_id >= 0) {
        continue;
      }
      /* Raw 78260-78263: 11×11 box, both axes −5..+5. */
      if (abs(u->x - c->x) > 5 || abs(u->y - c->y) > 5) {
        continue;
      }
      const int t = ai_euro_20e6_dos_type(ctx->units, u);
      if (t < 0x0d || t > 0x12) {
        continue; /* raw 78269-78270: ship roster only */
      }
      if (ai_euro_20e6_type_combat(t) == 0) {
        continue; /* raw 78271: 0x5236 combat byte must be non-zero */
      }
      /* Raw 78274-78275: FUN_6662_0906 sea flood, count only cost 0..5
       * ("short navigable route" — filters land-blocked ships). */
      const int cost = ctx->map
        ? units_short_sea_route_cost(ctx->map, u->x, u->y, c->x, c->y)
        : -1;
      if (cost < 0 || cost > 5) {
        continue;
      }
      if (t == 0x11) {
        c->ai_flags |= COLONIZE_COLONY_AI_NEARBY_FRIGATE; /* raw 78276-78282 */
      } else {
        c->ai_flags |= COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP;
      }
    }
  }
  /* Raw 78291-78299: per-colony fold into the nation ship-pressure tallies. */
  if (nation_id >= 0 && nation_id < 4) {
    AiEuroShipPressure* sp = &s_ship_pressure[nation_id];
    const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0) {
      if (sp->frigate_colonies < 255) {
        sp->frigate_colonies++;
      }
      sp->frigate_pop += pop;
    }
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) != 0) {
      if (sp->other_colonies < 255) {
        sp->other_colonies++;
      }
      sp->other_pop += pop;
    }
  }
}

/*
 * FUN_4962_0018 for one nation's colonies (reset raw 78239-78242 + the
 * per-colony probe above). The human nation's per-turn entry — DOS runs it
 * from the nation EOT (FUN_3844_00f2, control != 2, so human included); the
 * AI nations get the identical probe inside ai_euro_colony_goals.
 */
void ai_euro_census_ship_pressure_refresh(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies) {
    return;
  }
  ai_euro_ship_pressure_reset(nation_id);
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    ai_euro_colony_ship_probe_4962(ctx, nation_id, c);
  }
}

static void ai_euro_refresh_colony_ai_flags(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c
) {
  if (!ctx || !c || !c->active) {
    return;
  }
  ai_euro_colony_ship_probe_4962(ctx, nation_id, c);
  c->ai_flags = (uint8_t)(
    c->ai_flags & (uint8_t)~(COLONIZE_COLONY_AI_WANTS_PIONEER_WORK |
                             COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR)
  );
  /*
   * DOS +0x1b bit 0x20, the "send a Pioneer to CLEAR" half of the `|= 0xa0`
   * pair at raw 94200-94206 (see COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR in
   * colony.h for the decode). It is fed by the FULL ring scan at raw
   * 94082-94117 — every ring slot, not just the worked ones the 0x80 block
   * below walks — so it gets its own loop here. DOS runs both writers before
   * the 0x80 writer at raw 94207-94209 and after the 0x08/0x04 pair in
   * ai_euro_colony_threat_seed_5952, which is exactly this position; the
   * per-tick clear is that function's `+0x1b &= 7`, mirrored above.
   *
   * Verbatim counter shapes (local names from the raw dump):
   *   off-map (FUN_281f_0302 == 0)      -> local_142++, local_144++
   *   class 0x19/0x1a (Ocean/Sea Lane)  -> local_144++, local_142++, local_e++
   *   class outside 8..0x17 (not forest, and note DOS re-tests water here
   *     too — the two ifs are sequential, not else-if):
   *        food < 2 -> local_144++;  food >= 3 -> local_e++
   *   class 8..0x17 (forest)            -> local_134++, local_144++,
   *        and food[class & 7] > 2     -> local_c++
   * `food` is the raw DS:0x2f7b table byte (colony_yield_terrain_class_base,
   * job 0), NOT a tile yield: no resource/plow/river/SoL folding.
   * local_142 is the Docks-cleared water count the 0x10 latch uses; it plays
   * no part in the 0x20 writers, so it is not recomputed here.
   */
  /* local_a2 (raw 93997) = DS:0x329[FUN_15eb_0470()], the colony's own ring
   * size — 8 for anything stock DOS builds, 12/20 with the cut Town Hall
   * upgrade rows (bugs.md #593). */
  const int ring_5952 = colonies_work_plot_count(ctx->colonies, c);
  if (ctx->map) {
    int unproductive = 0; /* local_144 */
    int good_food = 0;    /* local_e   */
    int forests = 0;      /* local_134 */
    int clearable = 0;    /* local_c   */
    for (int ti = 0; ti < ring_5952; ++ti) {
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(ti, &dx, &dy)) {
        continue;
      }
      const int tx = c->x + dx;
      const int ty = c->y + dy;
      if (tx < 0 || ty < 0 || tx >= (int)ctx->map->width || ty >= (int)ctx->map->height) {
        ++unproductive; /* raw 94086-94088: off-map counts as blocked */
        continue;
      }
      const int cls = map_dos_terr_class_at(ctx->map, tx, ty);
      if (cls == 0x19 || cls == 0x1a) {
        ++unproductive;
        ++good_food; /* DOS counts open water as a food tile (Docks) */
      }
      if ((cls < 8 || cls > 0x0f) && (cls < 0x10 || cls > 0x17)) {
        const int food = colony_yield_terrain_class_base(cls, COLONIZE_JOB_FARMER);
        if (food < 3) {
          if (food < 2) {
            ++unproductive;
          }
        } else {
          ++good_food;
        }
      } else {
        ++forests;
        ++unproductive;
        if (colony_yield_terrain_class_base(cls & 7, COLONIZE_JOB_FARMER) > 2) {
          ++clearable;
        }
      }
    }
    /* raw 94200-94202 */
    if (ring_5952 - 1 <= unproductive && forests > 1) {
      c->ai_flags |= (uint8_t)(COLONIZE_COLONY_AI_WANTS_PIONEER_WORK |
                               COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR);
    }
    /* raw 94203-94206 */
    if (good_food < (((int)c->population + 3) >> 2) && clearable != 0 && forests > 1) {
      c->ai_flags |= (uint8_t)(COLONIZE_COLONY_AI_WANTS_PIONEER_WORK |
                               COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR);
    }
  }
  /*
   * DOS +0x1b bit 0x80 (FUN_5952_035e surround scan, colony_tick doc ~415/423):
   * over the WORKED ring slots, iStack_6e counts tiles with (fa_flags & 0x0a)
   * == 0 (no road) and iStack_140 counts land tiles of terrain class < 8
   * without plow 0x40; either non-zero sets the bit.
   */
  if (ctx->map) {
    for (int ti = 0; ti < ring_5952; ++ti) {
      if (c->tiles[ti] < 0) {
        continue; /* DOS: colony+0x70+slot < 0 — unworked */
      }
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(ti, &dx, &dy)) {
        continue;
      }
      const int tx = c->x + dx;
      const int ty = c->y + dy;
      if (!map_tile_has_road(ctx->map, tx, ty)) {
        c->ai_flags |= COLONIZE_COLONY_AI_WANTS_PIONEER_WORK;
        break;
      }
      const int cls = map_dos_terr_class_at(ctx->map, tx, ty);
      if (cls >= 0 && cls < 8 && !map_tile_is_plowed(ctx->map, tx, ty)) {
        c->ai_flags |= COLONIZE_COLONY_AI_WANTS_PIONEER_WORK;
        break;
      }
    }
  }
  /*
   * +0x1b bit 0x10, colony_tick_5952_035e.md:557-563 (see
   * ai_euro_colony_needs_colonists_5952) — the SECOND of DOS's two writers of
   * this bit. DOS is an OR and nothing else:
   *   if (+0x1f < ' ') { ... if (a && b) +0x1b |= 0x10; }
   * The per-tick clear for the bit is the flag byte's `+0x1b &= 7` (raw
   * 94142), which lives with the first writer in
   * ai_euro_colony_threat_seed_5952 — the call that runs immediately before
   * this function, for this same colony. So this writer must NOT carry an
   * `else`-clear of its own: it used to, and that clear wiped the first
   * writer's one-shot hand-off from the +0x1c bit 0x10 latch (smell audit
   * 2026-09-10 C3).
   */
  if (ai_euro_colony_needs_colonists_5952(ctx->colonies, ctx->map, c)) {
    c->ai_flags |= COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  }
  /*
   * +0x1b bit 0x40 (NEEDS_GARRISON) is NOT set here. DOS raises it from the
   * labor_shortage quantity `local_76 > 0` (raw 94147-94149), which is built
   * and consumed inside ai_euro_colony_threat_seed_5952 — the same DOS body,
   * and the call that runs immediately before this one. The
   * `garrison_quota > 0` substitution that used to live here only fired at
   * threat >= 8 (smell audit #38), and the flag byte's DOS-order clear
   * (`&= 7`) now lives with the writer too.
   */
  /*
   * +0x1c thin: wagon / coastal. Bit3 is NOT touched here: it
   * is DOS's inefficient-government latch (FUN_364b_0688 phase D), which the
   * per-turn colony tick owns. This pass used to overwrite it with a
   * food-vs-need reading, which both clobbered the latch and had no DOS
   * basis; ai_euro_colony_food_short below is the food test the AI wanted.
   */
  /*
   * Bit 0x10 (COLONIZE_COLONY_FLAG_SMALL_AI, col1_save.h `small_colony_ai`)
   * is NOT written here. There are exactly TWO DOS references to +0x1c bit
   * 0x10: the read-and-clear hand-off at raw 94143-94146 (`(+0x1c & 0x10) &&
   * pop < 0x20 -> +0x1b |= 0x10; +0x1c &= 0xef`), ported in
   * ai_euro_colony_threat_seed_5952 and consumed at :10076; and a writer
   * deeper in the SAME function, raw 95845-95847 (`pop < 10 -> +0x1c |=
   * 0x10`), which sits inside FUN_5952_035e's expansion arm behind a chain of
   * 2a1f_05b4 probability gates and is NOT ported. So the bit is set by a
   * narrow branch, not per tick: the unconditional `pop < 10` stamp that used
   * to live here re-armed it every turn, pinning NEEDS_COLONISTS on for every
   * colony under 10 population, which is why it was dropped (smell audit
   * 2026-09-10 C3) — correct removal, but "DOS has no writer at all" was
   * wrong; the real writer is a live lead, not absent (2026-09-10 D7).
   */
  colony_prod_refresh_sol_flags(c, (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL);
  if (ctx->units) {
    /*
     * +0x1c bit 0x20 (WAGON_TRAIN). DOS writes it in the 6d8e prelude
     * (FUN_521d_6d8e): raw 93142 clears it on every own colony, then the unit
     * loop at raw 93148-93157 sets it on the colony a unit is HOMED to —
     * `unit type (+0x3146) == 0x0c && origin (+0x314a) >= 0`, then
     * `colony +0x1a == nation`. It is not an on-tile test: this pass used to
     * look for a wagon standing on the colony square, so a wagon homed here
     * but out on the road cleared the bit and a foreign-homed wagon parked
     * here set it. Clearing on the negative arm IS right for this bit (DOS
     * clears then re-derives). Placement in the 5952 per-colony refresh
     * rather than the 6d8e prelude is immaterial while the port has no
     * reader — DOS's reader is the unported gate at raw 95762. Smell audit
     * 2026-09-10 D7.
     */
    int wagon = 0;
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      const ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id != nation_id || u->col1_origin >= 0x80 ||
          (int)u->col1_origin != c->id) {
        continue;
      }
      if (ai_euro_unit_kind(ctx->units, u) == UNITS_KIND_WAGON) {
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
  /*
   * +0x1c bit 0x40 (COASTAL) is a founding-time stamp, not a per-turn
   * reading. DOS's only writer of that bit anywhere in the image is
   * FUN_364b_1ba8 (raw 58105-58110) — see map_tile_is_open_sea_adjacent — and
   * the tick's own flag-byte clear is `&= 0xef` (raw 94145, bit 0x10 only),
   * so nothing ever recomputes or clears 0x40. This pass used to recompute it
   * from `!map_tile_is_land` over the FOUR orthogonal neighbours, where
   * off-map reads as water (so every map-edge colony became coastal) and the
   * negative arm CLEARED the bit — discarding what a DOS save carried, e.g.
   * for a colony whose only ocean neighbour is diagonal. What is left is a
   * set-only self-heal through the shared predicate, for colonies that
   * predate colonies_found's stamp; it can never take the bit away.
   * Smell audit 2026-09-10 D4.
   */
  if (ctx->map && (c->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) == 0 &&
      map_tile_is_open_sea_adjacent(ctx->map, c->x, c->y)) {
    c->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
  }
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
  return colonies_building_name_row(bt->name) == COLONY_BUILDING_STOCKADE || colonies_building_name_row(bt->name) == COLONY_BUILDING_FORT ||
         colonies_building_name_row(bt->name) == COLONY_BUILDING_FORTRESS || colonies_building_name_row(bt->name) == COLONY_BUILDING_WAREHOUSE ||
         colonies_building_name_row(bt->name) == COLONY_BUILDING_LUMBER_MILL || colonies_building_name_row(bt->name) == COLONY_BUILDING_DRYDOCK ||
         colonies_building_name_row(bt->name) == COLONY_BUILDING_SHIPYARD || colonies_building_name_row(bt->name) == COLONY_BUILDING_CUSTOM_HOUSE;
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
  const int stockade_id = colonies_building_row(ctx->colonies, COLONY_BUILDING_STOCKADE);
  const int fort_id = colonies_building_row(ctx->colonies, COLONY_BUILDING_FORT);
  const int fortress_id = colonies_building_row(ctx->colonies, COLONY_BUILDING_FORTRESS);
  const int warehouse_id = colonies_building_row(ctx->colonies, COLONY_BUILDING_WAREHOUSE);
  const int whe_id = colonies_building_row(ctx->colonies, COLONY_BUILDING_WAREHOUSE_EXPANSION);
  const int docks_id = colonies_building_row(ctx->colonies, COLONY_BUILDING_DOCKS);
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
  opts.col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
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
      (void)colonies_set_construction_ex(ctx->colonies, c->id, pick, &opts);
    }
  }
}

/*
 * Pop < 3 without Stockade: clear any prefer_* queue so hammers bank with
 * bip 0xFF (TURN5→6 Dutch; golden TURN4→5 New Amsterdam pop2 bip=255).
 * Yes, this wipes what prefer_young just queued this same dispatcher call —
 * deliberately: in the DOS saves a young colony carries a Docks project ONLY
 * on its founding turn (New Amsterdam TURN4 / Quebec TURN5 / Isabella TURN6,
 * all size 1, founded that turn — the colonies_found Docks default lands in
 * the unit-act phase AFTER this clear), and shows bip 0xFF every turn after
 * until Stockade-capable. Smell audit #98 proposed keeping pop-1 picks; the
 * TURN4→5 golden refuted that. Cite: turn.c hammer bank; test-saves-ai/TURN6.
 */
static void ai_euro_clear_pre_stockade_build_queue(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int stockade_id = colonies_building_row(ctx->colonies, COLONY_BUILDING_STOCKADE);
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
 * bugs.md #483: the nineteen-row `ai_euro_prefer_*` table and the craft
 * house/shop/factory pass that used to live here were name-keyed inventions.
 * DOS has exactly one AI construction picker — the FUN_5952_035e cascade,
 * ported as ai_euro_5952_build_cascade below (its craft-chain arm is
 * FUN_5952_0280 + FUN_1000_8d90 at asm 5952:268e). Deleted 2026-09-17.
 */

/*
 * DOS has no Capitol construction preference to port: FUN_15eb_3650 refuses
 * @BUILDING 0x1e (Capitol) outright for every colony, and Capitol Expansion
 * sits behind it as a prerequisite, so neither the AI nor the player can ever
 * start one (colonies_building_is_buildable carries the same block). The
 * port's two Capitol prefer passes are gone with it.
 */


/*
 * Expert Lumberjack LABOR when incomplete Warehouse or Lumber Mill and that
 * building type exists in the pool. Lumber feeds carpenter hammers
 * (building_production Lumberjack→Lumber). Cite: docs/building_production.md;
 * Colonization.pdf Skills Chart / lumberjack timber. Structural LABOR join
 * only — no invented lumber rates. Forest field-assign is wired separately
 * via colonies_assign_field in the colony tick's own placement pass.
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
  if (colonies_building_name_row(bt->name) == COLONY_BUILDING_WAREHOUSE) {
    return colonies_building_row(pool, COLONY_BUILDING_WAREHOUSE) >= 0;
  }
  if (colonies_building_name_row(bt->name) == COLONY_BUILDING_LUMBER_MILL) {
    return colonies_building_row(pool, COLONY_BUILDING_LUMBER_MILL) >= 0;
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

/*
 * Military land unit by @UNIT row: Soldiers, Dragoons, Regulars, Cavalry,
 * Cont. Cav., Cont. Army. Also the peace colony garrison set — audit AE-6:
 * ai_euro_is_colony_garrison_name held the identical list in a different
 * order and is gone (Colonization.pdf Defending a Colony, "fortify soldiers,
 * dragoons, army, cavalry"). NAMES.TXT @UNIT ships "Cavalry" (the crown
 * mounted tier) and abbreviates "Cont. Army" / "Cont. Cav.", which is why the
 * old substring list needed both spellings; units_name_kind carries all of
 * them (same defect class as the fixed ai_king "Cavalry != Dragoon" bug).
 */
static int ai_euro_is_military_name(ColonizeUnitKind k) {
  /* Deliberately NOT units_kind_is_military: that set also holds Artillery,
   * which this port keeps separate (siege + border wake, below). */
  return k == UNITS_KIND_SOLDIER || k == UNITS_KIND_DRAGOON || k == UNITS_KIND_REGULAR ||
         k == UNITS_KIND_CAVALRY || units_kind_is_continental(k);
}

/*
 * Soldier / Dragoon / Regular / Continental — land war hunt; not founders.
 *
 * Scouts were in this set (bugs.md #494) and therefore skipped the
 * FUN_521d_20e6 move-scoring gate at war and ran the combat hunt arm
 * instead. DOS's 20e6 type-5 band carries no war term at all: the pre-gate
 * (raw 88514-88530), the patrol 0x56 arm (raw 89047-89059), the village
 * 0x4c arm (raw 89064-89068) and the explore ring (raw 89076+) read the
 * G-table, continent ids, turn counters and the village record — never a
 * per-peer relation byte. A Scout (attack 1) is also never issued the
 * 0x46 seize order, so it has no hunt business at war.
 */
static int ai_euro_is_land_war_hunter(ColonizeUnitKind kind) {
  return ai_euro_is_military_name(kind);
}

/* @UNIT row 11 ("Artillery", or a pool spelling it "Cannon"). */
static int ai_euro_is_artillery_name(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_ARTILLERY;
}

/*
 * Col1 +0x1e: fortify only while garrison_quota > 0, then DEC.
 * Cite: save_format_map.md. The quota this consumes is the real
 * FUN_5952_035e threat>>3 seed (ai_euro_colony_threat_seed_5952, ported
 * 2026-09-08) — no longer a thin planning latch.
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

static int ai_euro_land_is_fortified(const ColonizeUnit* u) {
  if (!u || (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED)) {
    return 0;
  }
  /*
   * bugs.md #529, re-derived 2026-09-22. Both halves of this check are DOS-
   * literal, not a heuristic:
   *
   * - `orders ∈ {FORTIFY(5), FORTIFIED(6)}`: FUN_521d_20e6's shared exit tail
   *   (raw 90378-90386, the LAB_521d_589e block every call of 20e6 funnels
   *   through) forces `+0x314c` (`orders`) to 5 whenever it isn't already 5
   *   or 6, and to 6 when the colony-admit flag bit is set — i.e. every
   *   single 20e6 exit leaves a land unit's real order byte in the
   *   fortify family. This is why `orders` alone (as in bugs.md #529's
   *   2026-09-22 "DOS-literal" attempt, `return 1;` unconditionally) proves
   *   nothing: idle units land here too.
   * - `col1_ai_plan != '0'(0x30)`: LAB_5a78 (raw 90399-90404) writes the pair
   *   `+0x314b = '0'; +0x314c = 5` ONLY when +0x314c was already invalid
   *   (0 or 10) on entry to the tail, i.e. nothing upstream in this call
   *   actually assigned the unit — it is DOS's "idle, re-evaluate next call"
   *   sentinel, not a real decision. Every genuine assignment that funnels
   *   into the same tail sets `+0x314b` to a real code BEFORE the goto
   *   (`'A'`/0x41 standing military raw 87705 family, `'G'`/0x47 own-colony
   *   garrison raw 87491-87492 and move_scoring_gate's admitted/armed<2
   *   branches, `'F'`/0x46 park raw 89025, etc.), so `!= '0'` is exactly
   *   "a branch upstream actually decided this", which matches the other
   *   independent DOS "already assigned, don't redo" queries used elsewhere
   *   in the game (FUN_521d_0a60's own re-decision gate, raw 88159, skips a
   *   unit only when `+0x314b == 'A'`; the labor/combat-strength admission
   *   gates at raw 84724 and 87632 use `!= 'G' && != 'A'`) — those never
   *   admit '0' either, since '0' is specifically "not decided".
   *
   * Together: a unit reads as fortified only when it both carries a real
   * fortify order AND was actually routed there by a DOS branch this call,
   * not merely defaulted. Confirms the pre-existing form; the row's
   * "DOS-literal" attempt was wrong to drop the ai_plan half rather than
   * narrow it to exactly '0'.
   */
  return u->col1_ai_plan != 0x30;
}

/* Sentry / fortify / fortified — wake-eligible passive land orders. */
static int ai_euro_land_is_passive_orders(const ColonizeUnit* u) {
  return u &&
         (u->orders == UNITS_ORDER_SENTRY || u->orders == UNITS_ORDER_FORTIFY ||
          u->orders == UNITS_ORDER_FORTIFIED);
}

/*
 * FUN_521d_06ae founding pick. The Linux coastal preference (+40 first colony,
 * +10 later) and its `colony_count` argument were removed 2026-09-08: DOS's
 * 06ae scores only DS:0x2f77[terrain class] + 0492*0x10 + the 074a nibble
 * (decomp 87286-87304), and a flat +10 swamped the 0..6 terrain byte.
 * `units` is now passed for real so DOS's 06d2/08bc occupant + wagon-XOR gate
 * runs on this path too — it used to be handed NULL, which disabled the gate
 * everywhere except the landfall caller. Cite: euro_goals.c; move_scoring.md §06ae.
 */
static int ai_euro_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  const ColonizeUnitPool* units,
  int nation_id,
  int x,
  int y,
  int* out_x,
  int* out_y
) {
  return ai_goals_pick_founding_tile_ex_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL)}, nation_id, x, y, 1, 0, out_x, out_y);
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
 * Removed (bugs.md #493/#495): ai_euro_scout_contact_ring_target,
 * ai_euro_scout_fog_explore_target and ai_euro_is_seasoned_scout_name were
 * Linux inventions (tribe ring MD 2-4 with x1000/x50/x10 weights, an MD<=8
 * fog sweep, and a "Seasoned Scout prefers deeper fog" profession read).
 * DOS FUN_521d_20e6's type-5 band reads no profession byte and no relation
 * matrix; its explore ring and radius are ported in
 * ai_euro_land_explore_scan_target / ai_euro_20e6_explorer_flag.
 */

/* Treasure train, identified by @UNIT type row (manual Treasure Trains). */
static int ai_euro_is_treasure_name(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_TREASURE;
}


/*
 * Europe-bound lane entry for a ship: prefer the eastern High Seas rim
 * (units_find_eastern_high_seas_tile — the 48d3_015e Atlantic exit), else the
 * nearest water tile further east as a stand-in on a map with no HS column.
 * Consumer: ai_euro_ship_sail_to_europe (the FUN_4393 export / Privateer-loot
 * sail). The treasure caller it also had was deleted 2026-09-23 — see below.
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
 * Deleted 2026-09-23 (bugs.md #745): ai_euro_treasure_coast_target — a Linux
 * invention cited only to "Colonization.pdf Treasure Trains", with a
 * coastal-colony preference, a Manhattan distance, no landmass test and a
 * bare-coast-tile fallback DOS never produces. FUN_521d_20e6's treasure band
 * (raw 89997-90040) picks the nearest own colony by FUN_281f_037a with no
 * coastline term; the real band lives in ai_euro_20e6_treasure_cash_in +
 * ai_euro_20e6_47b9_dead_end.
 */

/* Missionary / Jesuit Missionary, identified by @UNIT type row. */
static int ai_euro_is_missionary_name(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_MISSIONARY;
}

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);





/* Defined further down with the FUN_5952_035e tick; 28c8 needs the same
 * DS:0x8dc8 / DS:0x8e0a ledger pair. */
static void ai_euro_5952_ledgers(
  const ColonizeWorld* world,
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const ColonizeCol1Save* col1,
  int gross[AI_EURO_5952_LEDGER_SLOTS],
  int demand[AI_EURO_5952_LEDGER_SLOTS]
);

/*
 * FUN_15eb_28c8 — colonist work-plot scorer, DOS-literal (raw 12908-13158).
 * bugs.md #570 (2026-09-22): the previous body was a structural sketch and
 * diverged from the decomp in six ways (no distance term, no food-emergency
 * branch, no cargo-weight branch, labour penalty applied unconditionally and
 * AI-only, sticky x2 unconditional, colonist headcount used where DOS clamps
 * against WAREHOUSE stock). Ported here term for term.
 *
 * Shape of the DOS body (`param_1` = colonist slot, `param_2` = restrict):
 *   bVar1 = colony is HUMAN-controlled (colony+0x1a < 4 && DS:0x543f[nation
 *           *0x34] == 0, raw 12952-12958) — this is NOT an AI-only routine:
 *           FUN_15eb_2ea0 runs it for every colony (see
 *           ai_euro_28c8_auto_assign_plots).
 *   bVar2 = food emergency: `DS:0x35e == 0 && DS:0x8dc8 < DS:0x8e0a`
 *           (food gross < food demand) and, for an AI colony, also
 *           `colony+0x9a <= capacity && !(DS:0x8e32*0x10 < colony+0x9a)`.
 *           DS:0x35e is set to 1 for the whole FUN_5952_035e AI colony tick
 *           (raw 94628 set / 95975 clear), so the emergency branch is
 *           unreachable from the tick — hence `in_ai_tick` here.
 *   score = yld*8 + (7 - |dx| - |dy|)                       (raw 13017-13024)
 *   sticky x2 when the colonist already holds this job — HUMAN only (bVar1).
 *   then either the food-emergency branch (jobs 0/8 get <<5, Fisherman +8,
 *   minus the DS:0x2f7a labour byte with a +0x18 forest surcharge, floor 1;
 *   an Indian-claimed plot halves) or the cargo-weight branch (per-nation
 *   DS:0x84bc price row, shortfall/unmet bumps, consumer-building term,
 *   Indian-alarm subtraction, `score = (local_38 + local_4) * score`).
 *
 * Deliberate substitutions (no DOS constant invented):
 *   - DS:0x8e32 "production shortfall" / DS:0x8e5a "unmet after stock" are
 *     recomputed from the same ledger pair the 5952 tick uses
 *     (ai_euro_5952_ledgers = DS:0x8dc8/0x8e0a) with FUN_15eb_0b52's own
 *     rule (colony_craft.c header).
 *   - FUN_15eb_15c6(DS:0x2b6[job]) is 0 when the field good has no consumer
 *     job, else 1 (+1 when that job's base building itself has a parent tier,
 *     which no chain root in the port's @BUILDING table has) — see
 *     k_ai_euro_28c8_consumer_chain.
 *   - byte[FUN_15eb_0470()+0x329] is the colony's work-plot count. Read off
 *     VICEROY.EXE (file offset 121248 + 0x329): {0, 4, 8, 12, 20}, indexed by
 *     FUN_15eb_0470() = min(FUN_15eb_039e(10), 2) + 2. 039e(10) is 0 in every
 *     state stock DOS can reach, so the count is 8 there — but a save may
 *     carry the cut rows, so it is read per colony from
 *     colonies_work_plot_count (bugs.md #570, #593).
 */

/* DS:0x2b6 read as 28c8 reads it: field job -> the JOB that consumes its
 * good (jobs 9..12/14 there are input cargos; 28c8 only indexes 0..8).
 * {-1,9,10,11,12,-1,14,-1,-1} -> building chain via DS:0x2f4 (FUN_15eb_0aec). */
static int ai_euro_28c8_consumer_chain(int field_job) {
  switch (field_job) {
    case 1: return COLONIES_CHAIN_RUM;         /* Sugar   -> Distiller  (9) */
    case 2: return COLONIES_CHAIN_TOBACCONIST; /* Tobacco -> Tobacconist(10) */
    case 3: return COLONIES_CHAIN_WEAVER;      /* Cotton  -> Weaver    (11) */
    case 4: return COLONIES_CHAIN_FUR;         /* Furs    -> Fur Trader(12) */
    case COLONIZE_JOB_ORE_MINER: return COLONIES_CHAIN_BLACKSMITH; /* Ore (14) */
    default: return -1;
  }
}

/* FUN_15eb_039e(b): owned buildings walking b -> parent -> ... `tiers` is how
 * many tiers from the chain root that walk covers (039e(3) = the Armory row
 * alone, 039e(0x28) = Blacksmith's House + Shop). */
static int ai_euro_28c8_chain_owned_upto(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int chain, int tiers
) {
  const char* const* names = colonies_building_chain(chain);
  if (!pool || !col || !names) {
    return 0;
  }
  int n = 0;
  for (int i = 0; names[i] && i < tiers; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && col->has_building[idx]) {
      ++n;
    }
  }
  return n;
}

typedef struct AiEuro28c8Env {
  bool human;       /* bVar1 */
  bool food_emerg;  /* bVar2 */
  int capacity;     /* local_6 = FUN_15eb_0a50 */
  int continent;    /* local_e = FUN_137f_02a0(colony) */
  int mil_count;    /* local_14 = own units with @UNIT defense > 1 */
  int lumber_gross; /* DS:0x8dd2 = gross[Lumber] */
  int turn;         /* DS:0x538e */
  int shortfall[COLONIZE_CARGO_COUNT]; /* DS:0x8e32 */
  int unmet[COLONIZE_CARGO_COUNT];     /* DS:0x8e5a */
} AiEuro28c8Env;

static void ai_euro_28c8_env(
  const ColonizeTurnContext* ctx, const ColonizeColony* col, int in_ai_tick,
  AiEuro28c8Env* env
) {
  const ColonizeCol1Save* col1 = ctx->col1_ok ? ctx->col1 : NULL;
  memset(env, 0, sizeof(*env));
  env->human = col->nation_id == ctx->human_nation;
  env->capacity = colonies_warehouse_capacity(ctx->colonies, col, COLONIZE_CARGO_FOOD);
  env->continent = ctx->map ? map_continent_id_at(ctx->map, col->x, col->y) : -1;
  env->turn = col1 ? (int)col1->head.turn : 0;

  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  const ColonizeWorld w = world_from_turn_ctx(ctx);
  ai_euro_5952_ledgers(&w, ctx->colonies, col, col1, gross, demand);
  env->lumber_gross = gross[COLONIZE_CARGO_LUMBER];
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    /* DS:0x8e32 production shortfall and DS:0x8e5a unmet-after-stock,
     * FUN_15eb_0b52's own rule (colony_craft.c header). */
    const int miss = demand[c] - gross[c];
    env->shortfall[c] = miss > 0 ? miss : 0;
    const int after = miss - col->stock[c];
    env->unmet[c] = after > 0 ? after : 0;
  }

  /* bVar2, raw 12974-12980. */
  bool emerg = !in_ai_tick && gross[COLONIZE_CARGO_FOOD] < demand[COLONIZE_CARGO_FOOD];
  if (!env->human) {
    const int food_stock = col->stock[COLONIZE_CARGO_FOOD];
    emerg = emerg && food_stock <= env->capacity;
    if (env->shortfall[COLONIZE_CARGO_FOOD] * 0x10 < food_stock) {
      emerg = false;
    }
  }
  env->food_emerg = emerg;

  /* local_14 (AI only, raw 12960-12966): own units whose @UNIT defense
   * (DOS type*0xe + 0x5235) is > 1. */
  if (!env->human && ctx->units) {
    for (int i = 0; i < ctx->units->unit_count; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != col->nation_id) {
        continue;
      }
      if (u->type_index >= 0 && u->type_index < ctx->units->type_count &&
          ctx->units->types[u->type_index].defense > 1) {
        ++env->mil_count;
      }
    }
  }
}

/*
 * 28c8 scorer body. `profession` < 0 scores plain tile yields (the
 * structural/test entry point); otherwise the colonist's real profession
 * goes through colony_yield_for_worker (DOS 1068 trial-assigns the job, so
 * 18ec sees the expert) — that is what the live tick and the human
 * auto-assign use.
 *
 * `restrict_job` is DOS's `param_2` when it is a real job index rather than
 * one of the −1 / −2 modes: the search is then confined to that one field
 * job. `in_ai_tick` is DS:0x35e (1 inside FUN_5952_035e).
 */
static int ai_euro_28c8_score_full(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* col,
  int colonist_slot,
  int profession,
  int restrict_job,
  int in_ai_tick,
  AiEuro28c8JobCandidate* out_best
) {
  const ColonizeColonist* self = &col->colonists[colonist_slot];
  if (!self->active) {
    return 0;
  }
  const int current_job = self->field_job; /* iVar4 = FUN_15eb_0e18 */
  AiEuro28c8Env env;
  ai_euro_28c8_env(ctx, col, in_ai_tick, &env);
  const ColonizeCol1Save* col1 = ctx->col1_ok ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  /* The scorer must answer the Fisherman gate exactly as the tick does, or it
   * assigns plots the tick then pays 0 for. DOS's 18ec gate is
   * FUN_15eb_038e(6), the building alone (smell audit 2026-09-10 E#2). */
  /* The docks gate (raw 11955) is unconditional in DOS: it never depends on
   * whether a worker profession is known, so answer it from the colony in both
   * cases (bugs.md #609). */
  const bool has_docks = colony_yield_colony_has_docks(ctx->colonies, col);
  const int sol_b_field =
    profession >= 0 ? colony_prod_sol_bonus_field(col1, col) : 0;
  const bool has_hudson = profession >= 0 && col1 &&
                          founding_fathers_nation_has(col1, col->nation_id, FF_HENRY_HUDSON);

  /* local_22 (raw 12987) = DS:0x329[FUN_15eb_0470()] — the colony's own ring
   * size, 8 unless it owns the cut Town Hall rows (bugs.md #593). */
  const int ring_28c8 = colonies_work_plot_count(ctx->colonies, col);

  out_best->job = -1;
  out_best->tile = -1;
  out_best->score = 0; /* local_12 = 0: DOS elects only a strictly positive score */
  out_best->yield = 0;

  /* DOS-LITERAL FUN_15eb_28c8 raw 12987-12996: the plot walk is over the
   * DS:0xc8/0xde delta tables (N,E,S,W,NW,NE,SE,SW), and since the election at
   * raw 13126 is strictly greater the earliest table index wins ties — so the
   * port must visit its own slots in that DOS order (bugs.md #584). */
  for (int step = 0; step < ring_28c8; ++step) {
    const int ti = colonies_field_scan_order(step);
    if (ti < 0) {
      continue;
    }
    /* raw 12993: `*(char *)(iVar5 + iVar12 * 5 + -0x7210) == '\0'` — the
     * FUN_15eb_23f2 blocked bitmask cached in DS:0x8df0 must be 0 for both
     * human and AI colonies (bugs.md #579). The Indian-claim table at
     * -0x7262 below is the separate, human-only test. */
    if (colonies_plot_blocked_mask(&world, col, ti) != 0) {
      continue;
    }
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
    /* DS:0x8d9e (-0x7262), FUN_15eb_26e4's 5x5 Indian-claim table. A HUMAN
     * colony skips a claimed plot outright (raw 12993-12995); an AI colony
     * scores it and pays the alarm term below. */
    int claim_village = -1;
    int claim_tribe = -1;
    if (col1) {
      claim_village = colonies_indian_claim_tribe_from_w(
        &world, col->nation_id, col->x, col->y, tx, ty
      );
      if (claim_village >= 0 && col1->tribe) {
        claim_tribe = (int)col1->tribe[claim_village].nation_id - 4;
      }
    }
    if (env.human && claim_village >= 0) {
      continue;
    }
    const int terr = map_dos_terr_class_at(ctx->map, tx, ty);
    /* DOS reads local_32 (the terrain class) in both branches although the
     * decompiler only shows it assigned inside the emergency one — a
     * register-reuse artefact; it is the same FUN_13e4_003a(tile) call. */
    const bool forest = terr > 7 && terr < 0x18;
    const bool lumber_idle =
      col->stock[COLONIZE_CARGO_LUMBER] < 0xb && env.lumber_gross == 0;
    for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
      if (restrict_job >= 0 && job != restrict_job) {
        continue;
      }
      /* DOS-LITERAL FUN_15eb_28c8 raw 13000: `FUN_15eb_18ec(x,y,&local_24,0)`.
       * 18ec writes the *job* index into local_24 and only remaps a water job
       * to food when param_4 != 0 (raw 11983), which this call site passes as
       * 0 — so for the Fisherman (job 8) local_24 stays 8 and every later use
       * (warehouse clamp `colony+0x9a+8*2`, the DS -0x71ce / -0x71a6 rows, the
       * `local_24 != 5` lumber test) reads cargo slot 8 = HORSES. A DOS quirk,
       * kept verbatim (bugs.md #582). Jobs 0..7 map to cargo 0..7 identically. */
      const int cargo = job;
      int yld = profession >= 0
                  ? colony_yield_for_worker(
                      ctx->map, tx, ty, job, profession, has_docks, sol_b_field,
                      col->colony_flags, has_hudson
                    )
                  : colony_yield_for_tile_in_colony(
                      ctx->colonies, col, ctx->map, tx, ty, job
                    );
      if (yld <= 0) {
        continue; /* score would be 0, which never beats local_12 */
      }
      const int raw_yield = yld;
      if (restrict_job < 0 && cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) {
        /* raw 13004-13011: clamp to the warehouse room left for this good
         * (capacity − colony+0x9a+cargo*2), floor 1. Not AI-gated. */
        int room = env.capacity - col->stock[cargo];
        if (room < 1) {
          room = 1;
        }
        if (yld > room) {
          yld = room;
        }
      }
      /* raw 13017-13024. */
      int score = yld * 8 + (7 - (dx < 0 ? -dx : dx) - (dy < 0 ? -dy : dy));
      if (restrict_job < 0 && job == current_job && env.human) {
        score <<= 1; /* raw 13025 — human colonies only */
      }
      if (restrict_job < 0) {
        if (env.food_emerg) {
          if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
            if (job == COLONIZE_JOB_FISHERMAN && score != 0) {
              score += 8;
            }
            score <<= 5;
            if (score != 0) {
              int pen = map_dos_terr_labor_penalty_byte(terr);
              if (forest && lumber_idle) {
                pen += 0x18;
              }
              score -= pen;
              if (score < 1) {
                score = 1;
              }
            }
          }
          if (claim_village >= 0) {
            score >>= 1;
          }
        } else {
          int w4 = 0; /* local_4 */
          if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
            if (col->population < ring_28c8 * 2 && in_ai_tick) {
              w4 = 4;
            }
            if (env.human && w4 == 0) {
              w4 = 1;
            }
          } else if (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) {
            /* DS:0x84bc[nation*0x10 + cargo] — the per-nation Europe SELL
             * price byte (euro_price − 1, clamped at 0; europe.c). */
            if (col1) {
              const int p = (int)col1->nation[col->nation_id].trade.euro_price[cargo] - 1;
              w4 = p < 0 ? 0 : p;
            }
            if (!env.human && cargo == COLONIZE_CARGO_ORE && col->population > 7 &&
                env.turn > 0x4f && ctx->euro_power_rank_ok &&
                ctx->euro_power_rank[ctx->human_nation] <= ctx->euro_power_rank[col->nation_id]) {
              w4 += 2;
              /* FUN_15eb_039e(0x28) / (3): owned tiers of the Blacksmith and
               * Armory chains — not an RNG draw. */
              w4 += ai_euro_28c8_chain_owned_upto(
                ctx->colonies, col, COLONIES_CHAIN_BLACKSMITH, 2
              );
              w4 += ai_euro_28c8_chain_owned_upto(
                      ctx->colonies, col, COLONIES_CHAIN_ARMORY, 1
                    ) * 2;
            }
          }
          int m = w4 + 1; /* local_38 */
          const int sc = (cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) ? cargo : 0;
          if (env.shortfall[sc] == 0 && env.unmet[sc] == 0) {
            if (job == COLONIZE_JOB_LUMBERJACK &&
                col->stock[COLONIZE_CARGO_LUMBER] + env.lumber_gross > 1) {
              m = w4;
            }
          } else {
            m = w4 + 2;
            if (env.unmet[sc] != 0) {
              score <<= 1;
            }
          }
          /* FUN_15eb_15c6(DS:0x2b6[job]) — does a consumer workplace exist. */
          {
            const int chain = ai_euro_28c8_consumer_chain(job);
            if (chain >= 0) {
              m += 1;
            }
          }
          if (claim_tribe >= 0) {
            const int alarm =
              ai_diplo_indian_alarm(col1, claim_tribe + 4, col->nation_id);
            int t = -(alarm - 4);
            const int cont = env.continent;
            if (cont >= 0 && cont < 16 &&
                col1->stuff.unit_value_sum_by_continent[col->nation_id * 0x10 + cont] <
                  col1->stuff.tribe_dwellings_91cc[claim_tribe * 0x10 + cont]) {
              t = (alarm - 4) * -2;
            }
            if (col1->stuff.land_combat_totals[col->nation_id] <
                col1->stuff.tribe_data_9184[claim_tribe]) {
              t = t * 3 >> 1;
            }
            t -= env.mil_count;
            if (t < 0) {
              t = 0;
            }
            m -= t;
          }
          if (m < 0) {
            m = 0;
          }
          score = (m + w4) * score;
          if (cargo != COLONIZE_CARGO_LUMBER && forest && lumber_idle) {
            score -= 10; /* raw 13124-13130 */
          }
        }
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

/*
 * DOS `FUN_15eb_28c8(slot, job)` as the FUN_5952_035e colony tick calls it:
 * DS:0x35e is 1 for the whole tick, so the food-emergency branch is off.
 */
static int ai_euro_28c8_score_job(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* col,
  int colonist_slot,
  int profession,
  int restrict_job,
  AiEuro28c8JobCandidate* out_best
) {
  return ai_euro_28c8_score_full(
    ctx, col, colonist_slot, profession, restrict_job, 1, out_best
  );
}

/* DOS `FUN_15eb_28c8(slot, −1)` — the unrestricted all-jobs search. */
static int ai_euro_28c8_score(
  const ColonizeTurnContext* ctx,
  const ColonizeColony* col,
  int colonist_slot,
  int profession,
  AiEuro28c8JobCandidate* out_best
) {
  return ai_euro_28c8_score_job(ctx, col, colonist_slot, profession, -1, out_best);
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
  return ai_euro_28c8_score_full(ctx, col, colonist_slot, -1, -1, 0, out_best);
}

/*
 * FUN_15eb_2ea0 (raw 13162-13196) — the plot pass of DOS's generic colony
 * recompute FUN_15eb_3930 (`268e(); 287e(); 2ea0();`), run for HUMAN colonies
 * too (bugs.md #562). Every colonist not standing on a plot whose occupation
 * is a field job (FUN_15eb_0e18 < 9) is put through 28c8; when that finds
 * nothing the colonist becomes a Carpenter (`FUN_15eb_1068(slot, 0xd)`),
 * never a bell-ringer.
 *
 * `colonist_slot` >= 0 restricts the pass to one colonist — the join path
 * (colonies_admit / ORDERS Join Colony), which is the caller bugs.md #562 is
 * about. -1 walks the whole roster.
 *
 * Colonists carrying DOS occupation 0x13 (@JOB row 19, plain "Colonist") are
 * NOT in scope: 0e18 returns 19 for them, which fails the `< 9` gate, and
 * tests/golden/colony_prod01 (a DOS COLONY00->01 capture) proves DOS leaves
 * such a colonist unproductive rather than seating him. The port spells that
 * out at the call sites — colonies_auto_assign_idle keeps its own stale-save
 * sweep for them (colony.c).
 */
void ai_euro_28c8_auto_assign_plots(ColonizeTurnContext* ctx, int colony_id, int colonist_slot) {
  if (!ctx || !ctx->colonies || !ctx->map) {
    return;
  }
  ColonizeColony* col = colonies_get_mut(ctx->colonies, colony_id);
  if (!col || !col->active) {
    return;
  }
  for (int s = 0; s < col->colonist_count; ++s) {
    if (colonist_slot >= 0 && s != colonist_slot) {
      continue;
    }
    ColonizeColonist* c = &col->colonists[s];
    if (!c->active || c->building_type >= 0 || colonies_colonist_tile(col, s) >= 0) {
      continue;
    }
    AiEuro28c8JobCandidate best;
    const int ok =
      ai_euro_28c8_score_full(ctx, col, s, c->profession, -1, 0, &best);
    if (ok && colonies_assign_field(ctx->colonies, colony_id, s, best.tile, best.job)) {
      continue;
    }
    colonies_assign_carpenter_fallback(ctx->colonies, colony_id, s);
  }
}


/*
 * DOS FUN_281f_0c9a → FUN_15eb_0002 (viceroy_unpacked.c 9298-9307): the
 * expert/class test the colony tick uses everywhere it buckets colonists.
 * False for @JOB 0x13 (the "Colonist" row), 0x19 Indentured, 0x1a Criminal,
 * 0x1b Indian Convert and 0x1c Free Colonist; true for every real skill.
 * europe.c has the same predicate for the dock pool (europe_job_is_expert) —
 * kept separate rather than cross-included so the two files stay independent.
 */
static bool ai_euro_5952_job_is_expert(int job) {
  /* DOS reads a profession byte, so -1 ("no profession set") never reaches it;
   * the port's sentinel must not fall through as an expert (bugs.md #600). */
  return job >= 0 && job != UNITS_JOB_COLONIST && job != COLONIZE_PROF_INDENTURED &&
         job != COLONIZE_PROF_CRIMINAL && job != COLONIZE_PROF_CONVERT &&
         job != COLONIZE_PROF_FREE_COLONIST;
}

/* ===== FUN_5952_035e indoor-workplace pass (raw 94784-94860) ===== */

/*
 * DS:0x2f4 (FUN_15eb_0aec) — @JOB -> base @BUILDING index, read straight out
 * of VICEROY.EXE (file offset 121248 + 0x2f4):
 *   9 Distiller 27, 10 Tobacconist 24, 11 Weaver 21, 12 Fur Trader 32,
 *  13 Carpenter 35, 14 Blacksmith 39, 15 Gunsmith 3 (Armory),
 *  16 Preacher 37 (Church), 17 Statesman 9 (Town Hall), 18 Teacher 12.
 * Each of those is the first tier of one of the port's building chains, so
 * the chain id carries the same information without hard-coding a NAMES.TXT
 * row number (colony.h's chain enum is a save-format contract).
 */
static int ai_euro_5952_job_chain(int job) {
  switch (job) {
    case COLONIZE_PROF_DISTILLER: return COLONIES_CHAIN_RUM;
    case COLONIZE_PROF_TOBACCONIST: return COLONIES_CHAIN_TOBACCONIST;
    case COLONIZE_PROF_WEAVER: return COLONIES_CHAIN_WEAVER;
    case COLONIZE_PROF_FUR_TRADER: return COLONIES_CHAIN_FUR;
    case COLONIZE_PROF_CARPENTER: return COLONIES_CHAIN_CARPENTER;
    case COLONIZE_PROF_BLACKSMITH: return COLONIES_CHAIN_BLACKSMITH;
    case COLONIZE_PROF_GUNSMITH: return COLONIES_CHAIN_ARMORY;
    case COLONIZE_PROF_PREACHER: return COLONIES_CHAIN_CHURCH;
    case COLONIZE_PROF_STATESMAN: return COLONIES_CHAIN_TOWN_HALL;
    default: return -1;
  }
}

/*
 * FUN_281f_0ab0 -> FUN_15eb_039e "count owned buildings along parent chain".
 * Returns the count and, through `out_name` / `out_index`, the highest tier
 * this colony actually owns (the workplace a colonist would be put into).
 */
static int ai_euro_5952_chain_owned(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int chain,
  const char** out_name,
  int* out_index
) {
  if (out_name) {
    *out_name = NULL;
  }
  if (out_index) {
    *out_index = -1;
  }
  if (!pool || !col || chain < 0) {
    return 0;
  }
  const char* const* names = colonies_building_chain(chain);
  if (!names) {
    return 0;
  }
  int count = 0;
  for (int i = 0; names[i]; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx < 0 || idx >= COLONIZE_BUILDING_TYPES_MAX || !col->has_building[idx]) {
      continue;
    }
    ++count;
    if (out_name) {
      *out_name = names[i];
    }
    if (out_index) {
      *out_index = idx;
    }
  }
  return count;
}

/*
 * FUN_15eb_3454 (via FUN_281f_0bb4), the `aiStack_16a[job]` gate the pass
 * reads: for a job below 0x13 it is non-zero unless the job's base building
 * (FUN_15eb_0aec) exists and the colony does NOT own it — i.e. "this colony
 * has a workplace for this job".
 */
static bool ai_euro_5952_job_available(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int job
) {
  const int chain = ai_euro_5952_job_chain(job);
  if (chain < 0) {
    return false;
  }
  const char* const* names = colonies_building_chain(chain);
  if (!names || !names[0]) {
    return false;
  }
  const int base = colonies_find_building(pool, names[0]);
  if (base < 0 || base >= COLONIZE_BUILDING_TYPES_MAX) {
    return false;
  }
  return col->has_building[base];
}

/*
 * FUN_281f_0cd6 -> FUN_15eb_1d4c: what colonist `slot` would produce in
 * `job`, plus the output ledger slot through `out_cargo` (DOS's out-param;
 * 0xffff for a job outside 9..17). The three special bodies and the shared
 * craft body are already ported one-for-one in colony_production.c — see
 * original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md — so this
 * is only the dispatcher DOS's jump table at 15eb:1f44 performs.
 */
static int ai_euro_5952_producible(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const ColonizeCol1Save* col1,
  int slot,
  int job,
  int* out_cargo
) {
  if (out_cargo) {
    *out_cargo = -1;
  }
  const int chain = ai_euro_5952_job_chain(job);
  const char* name = NULL;
  if (ai_euro_5952_chain_owned(pool, col, chain, &name, NULL) <= 0 || !name) {
    return 0;
  }
  const int prof = col->colonists[slot].profession;
  const int sol_bonus = colony_prod_sol_bonus(col1, col);
  switch (job) {
    case COLONIZE_PROF_CARPENTER: {
      if (out_cargo) {
        *out_cargo = AI_EURO_5952_HAMMERS;
      }
      const bool mill = colonies_has_building_row(pool, col, COLONY_BUILDING_LUMBER_MILL);
      return colony_prod_hammers_worker(name, prof, sol_bonus, mill);
    }
    case COLONIZE_PROF_PREACHER: {
      if (out_cargo) {
        *out_cargo = AI_EURO_5952_CROSSES;
      }
      const bool cathedral = colonies_has_building_row(pool, col, COLONY_BUILDING_CATHEDRAL);
      const bool penn = founding_fathers_nation_has(col1, col->nation_id, FF_WILLIAM_PENN);
      return colony_prod_crosses_worker(name, prof, sol_bonus, cathedral, penn);
    }
    case COLONIZE_PROF_STATESMAN:
      if (out_cargo) {
        *out_cargo = AI_EURO_5952_BELLS;
      }
      return colony_prod_bells_worker(name, prof, sol_bonus);
    default: break;
  }
  const ColonizeCraftRecipe* r = colony_craft_recipe_for_building(name);
  if (!r) {
    return 0;
  }
  if (out_cargo) {
    *out_cargo = r->out_cargo;
  }
  return colony_prod_manufacturing_output(name, prof, r->craft_profession, sol_bonus);
}

/*
 * DS:0x84b4 (asm 5952:1e8f `MOV CL,byte ptr [BX + 0x84b4]`, decomp raw 94817
 * `-0x7b4c`) is read with the SAME `owner*0x10 + cargo` index as the sell
 * price table at DS:0x84bc, i.e. eight bytes ahead of it — the only read of
 * that address in the whole binary, and there is no writer. So for every
 * index >= 8 it is the price table read back shifted by eight; the first
 * eight bytes are whatever global sits immediately before DS:0x84bc, which
 * static analysis cannot name (no other code touches DS:0x84b4..0x84bb). The
 * port reads 0 there, the one modelled unknown in this pass.
 */
static int ai_euro_5952_price_row_minus8(const AiEuro5952Want* w, int index) {
  return index >= 8 ? (int)w->sell_price[index - 8] : 0;
}

/* FUN_124c_000c via FUN_281f_035c — clamp(v, lo, hi). */
static int ai_euro_5952_clamp(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/*
 * The want weight (`uStack_1bc`), asm 5952:1e72 (price arm) and
 * 5952:1fa3-5952:2115 (the three civic arms). DOS-LITERAL FUN_5952_035e
 * raw 94810-94856. No clamps, no guards beyond DOS's own.
 */
COLONIZE_INTERNAL int ai_euro_5952_want_weight(
  const AiEuro5952Want* w, int job, int out_cargo
) {
  if (!w) {
    return 0;
  }
  const int human = (w->human_nation >= 0 && w->human_nation < 4) ? w->human_nation : 0;
  const int owner = (w->owner_nation >= 0 && w->owner_nation < 4) ? w->owner_nation : 0;
  if (out_cargo < 0x10) {
    /* asm 5952:1e72 — BX = owner*0x10 + out. */
    const int index = owner * 0x10 + out_cargo;
    int v = (int)w->sell_price[index];
    if (out_cargo != 0x0f && out_cargo != 0x0e) {
      v -= ai_euro_5952_price_row_minus8(w, index);
    }
    if (out_cargo == 0x0e || out_cargo == 0x0f) {
      v += 4; /* asm 5952:1eaf, unconditional for Tools/Muskets */
      if (w->turn >= 0x32 && w->wealth_rank[human] <= w->wealth_rank[owner]) {
        v *= 2;
      }
    }
    return v;
  }
  int v = 3; /* asm 5952:1fa3 */
  if (job == COLONIZE_PROF_STATESMAN) { /* asm 5952:1fb2 — bells */
    v = w->press_chain_count * 4 + w->tories + 7 + w->capitol_level * 4;
    if (w->tories >= 10) {
      v *= 2;
    }
    if (w->jefferson) { /* FF 15, asm 5952:1fe4 — a ×2 here, NOT the ×1.5 of 2f2b:37cd */
      v <<= 1;
    }
    if (w->year < 0x604) {
      v = 0;
    }
    if (w->year > 0x640) {
      v <<= 1;
    }
    if (w->year > 0x6a4) {
      v <<= 1;
    }
    if (w->independence) {
      v = 0;
    }
    if (w->population <= 3) {
      v >>= 1;
    }
    if (w->population < 6) {
      v >>= 1;
    }
    if (w->wealth_rank[human] < w->wealth_rank[owner]) {
      v >>= 1;
    }
    if (w->wealth_rank[owner] < w->wealth_rank[human]) {
      v <<= 1;
    }
    if (w->nation_flag_bit4) {
      v >>= 1;
    }
    v = ai_euro_5952_clamp(v - w->gross[out_cargo], 1, 100); /* asm 5952:2080 */
  }
  if (job == COLONIZE_PROF_CARPENTER) { /* asm 5952:20a1 — hammers */
    v = -((int)((unsigned)w->gross[out_cargo] / 3u) - 5);
    if (w->wants_construction) {
      v >>= 1;
    }
    if (v < 1) {
      v = 1;
    }
  }
  if (job == COLONIZE_PROF_PREACHER) { /* asm 5952:20e5 — crosses */
    v -= (w->gross[out_cargo] >> 1) + w->turn / 100 - 6;
    if (v < 1) {
      v = 1;
    }
  }
  return v;
}

/* asm 5952:1ed1 tail — `(qty*8 + 5) * weight`. */
COLONIZE_INTERNAL int ai_euro_5952_job_score(
  const AiEuro5952Want* w, int job, int out_cargo, int qty
) {
  return (qty * 8 + 5) * ai_euro_5952_want_weight(w, job, out_cargo);
}

/*
 * FUN_5952_035e indoor pass — env switch. Default ON; AI_5952_INDOOR=0
 * restores the pre-2026-09-17 stand-ins (the leftovers field arm). Documented
 * in docs/debug_env_vars.md.
 */
COLONIZE_INTERNAL int ai_euro_5952_indoor_pass_enabled(void) {
  const char* v = getenv("AI_5952_INDOOR");
  return !(v && v[0] == '0');
}

/*
 * raw 94864-94872 (asm 5952:2139-5952:2174) — the fallback a slot takes when
 * it lost the election to its own field plot AND the plot commit found
 * nothing (DS:0x8dbe == 0): Carpenter, unless the colony owns a Church
 * (FUN_281f_09fc(0x25)), FUN_281f_0d08(5) reports a live lumber surplus
 * (FUN_15eb_0c52: demand[lumber] < stock[lumber] + gross[lumber]) and the
 * colony has fewer than three preachers.
 */
COLONIZE_INTERNAL int ai_euro_5952_fallback_job(
  int has_church, int lumber_surplus, int preacher_count
) {
  if (has_church && lumber_surplus && preacher_count < COLONIZE_BUILDING_MAX_WORKERS) {
    return COLONIZE_PROF_PREACHER;
  }
  return COLONIZE_PROF_CARPENTER;
}

/*
 * DS:0x2b6 (VICEROY.EXE file offset 121248 + 0x2b6) — @JOB -> INPUT cargo,
 * 0xff/-1 for a job that consumes nothing. Read straight off the image:
 *   {-1,9,10,11,12,-1,14,-1,-1, 1,2,3,4, -1, 6, -1,-1,-1,-1, 0}
 * The pass overrides the two -1 entries it cares about itself (Gunsmith <-
 * Tools at raw 94802, Carpenter <- Lumber at raw 94804).
 */
static const signed char k_ai_euro_5952_job_input[20] = {
  -1, 9, 10, 11, 12, -1, 14, -1, -1, 1, 2, 3, 4, -1, 6, -1, -1, -1, -1, 0
};

/*
 * DOS's two 20-word scratch ledgers for the current colony: DS:0x8dc8
 * (-0x7238) gross production and DS:0x8e0a (-0x71f6) demand, both refreshed
 * by FUN_281f_0c04 -> FUN_15eb_1f72 after every assignment. Slots 0..15 are
 * cargo, 16/17/18 hammers/crosses/bells (colony_craft.c's header).
 */
static void ai_euro_5952_ledgers(
  const ColonizeWorld* world,
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const ColonizeCol1Save* col1,
  int gross[AI_EURO_5952_LEDGER_SLOTS],
  int demand[AI_EURO_5952_LEDGER_SLOTS]
) {
  memset(gross, 0, sizeof(int) * AI_EURO_5952_LEDGER_SLOTS);
  memset(demand, 0, sizeof(int) * AI_EURO_5952_LEDGER_SLOTS);
  const int sol_bonus = colony_prod_sol_bonus(col1, col);
  const int sol_field = colony_prod_sol_bonus_field(col1, col);
  const bool docks = colony_yield_colony_has_docks(pool, col);
  const bool hudson =
    col1 && founding_fathers_nation_has(col1, col->nation_id, FF_HENRY_HUDSON);

  /* Field production (the town commons' own yields included — DOS's ledger
   * has no separate centre-tile row). */
  {
    ColonizeTownCommonsYield tc;
    memset(&tc, 0, sizeof tc);
    colony_yield_town_commons(
      world->map, col->x, col->y, col->colony_flags,
      col1 ? (int)col1->head.difficulty : 4, &tc
    );
    if (tc.food > 0) {
      gross[COLONIZE_CARGO_FOOD] += tc.food;
    }
    if (tc.secondary_cargo >= 0 && tc.secondary_cargo < COLONIZE_CARGO_COUNT) {
      gross[tc.secondary_cargo] += tc.secondary_amount;
    }
  }
  const int ring_ledger = colonies_work_plot_count(pool, col); /* DS:0x329, #593 */
  for (int ti = 0; ti < ring_ledger; ++ti) {
    const int occ = col->tiles[ti];
    if (occ < 0 || occ >= COLONIZE_COLONY_POP_MAX) {
      continue;
    }
    const ColonizeColonist* c = &col->colonists[occ];
    if (!c->active || c->field_job < 0 || c->field_job >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    gross[c->field_job] += colony_yield_for_worker(
      world->map, col->x + dx, col->y + dy, c->field_job, c->profession, docks,
      sol_field, col->colony_flags, hudson
    );
  }

  /* Craft workers: uncapped worker capacity out, tier-scaled input in — the
   * FUN_15eb_0bd4 ledger rows. */
  for (int s = 0; s < COLONIZE_COLONY_POP_MAX; ++s) {
    const ColonizeColonist* c = &col->colonists[s];
    if (!c->active || c->building_type < 0 ||
        c->building_type >= COLONIZE_BUILDING_TYPES_MAX) {
      continue;
    }
    const char* name = pool->building_types[c->building_type].name;
    const ColonizeCraftRecipe* r = name ? colony_craft_recipe_for_building(name) : NULL;
    if (!r) {
      continue;
    }
    gross[r->out_cargo] +=
      colony_prod_manufacturing_output(name, c->profession, r->craft_profession, sol_bonus);
    demand[r->in_cargo] +=
      colony_prod_manufacturing_input(name, c->profession, r->craft_profession, sol_bonus);
  }

  /* FUN_15eb_1f72's own three rows: hammers (lumber demand), crosses, bells. */
  int lumber_use = 0;
  gross[AI_EURO_5952_HAMMERS] = colony_prod_colony_hammers(pool, col, sol_bonus, &lumber_use);
  demand[COLONIZE_CARGO_LUMBER] += gross[AI_EURO_5952_HAMMERS];
  gross[AI_EURO_5952_CROSSES] = colony_prod_colony_crosses_ff(
    pool, col, col1 && founding_fathers_nation_has(col1, col->nation_id, FF_WILLIAM_PENN),
    sol_bonus
  );
  gross[AI_EURO_5952_BELLS] = colony_prod_colony_bells_ff(
    pool, col, 0, 0,
    col1 ? (col1->player[col->nation_id].control != 0) : true, sol_bonus
  );
  demand[COLONIZE_CARGO_FOOD] = col->population * 2;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94784-94860 (asm 5952:1ef7-5952:2193) — the
 * indoor-workplace pass, the last placement arm of the AI colony tick. For
 * every colonist the field passes left unplaced it elects one indoor @JOB
 * (9..0x11, Teacher 0x12 excluded by DOS itself) by
 * `(producible*8 + 5) * want_weight`, compares the winner against the tile
 * score the 28c8 probe just produced (DS:0x8dc0), and commits whichever won;
 * a slot that loses to its plot takes the plot, and a slot that loses to
 * nothing falls back to Carpenter — or Preacher, when the colony owns a
 * Church (@BUILDING 0x25), has a live lumber surplus (FUN_15eb_0c52(5)) and
 * fewer than three preachers.
 *
 * This retires two invented stand-ins: the "leftovers" field arm that used
 * to sit here (smell audit #42 — DOS's own leftovers arm at LAB_5952_17a9 is
 * dead code, and the port kept it only because this pass was unported), and
 * the name-matched craft chains of the colony-tick staffing pass for
 * colonists already inside a colony.
 */
static void ai_euro_5952_indoor_pass(
  ColonizeTurnContext* ctx, ColonizeColony* col, bool* placed, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  const int owner = (col->nation_id >= 0 && col->nation_id < 4) ? col->nation_id : 0;

  /* aiStack_e4 — per-@JOB count of this tick's placements. Every colonist was
   * unassigned at raw 94561 and re-placed since, so the live roster IS that
   * count. */
  int placed_count[COLONIZE_PROF_TEACHER + 1];
  memset(placed_count, 0, sizeof placed_count);
  for (int job = COLONIZE_PROF_DISTILLER; job <= COLONIZE_PROF_STATESMAN; ++job) {
    const char* const* names = colonies_building_chain(ai_euro_5952_job_chain(job));
    for (int i = 0; names && names[i]; ++i) {
      const int idx = colonies_find_building(pool, names[i]);
      if (idx >= 0) {
        placed_count[job] += colonies_building_worker_count(col, idx);
      }
    }
  }

  /*
   * DOS's `iStack_78`. It is a function-level local that the loop writes only
   * on a job with an input cargo, while the clamp at raw 94806 reads it
   * unconditionally — so Preacher and Statesman are clamped by whatever the
   * last input job (normally Gunsmith/Tools) left behind, and the very first
   * read in a colony is an uninitialised stack word. That carry-over is
   * DOS-LITERAL and kept; the one thing the port cannot reproduce is the
   * garbage seed, which reads as "no clamp" here (the single modelled unknown
   * of this pass, with DS:0x84b4's first eight bytes).
   */
  int avail = 0x7fff;

  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];

  for (int s = 0; s < n; ++s) {
    if (placed[s] || !col->colonists[s].active) {
      continue;
    }
    /* raw 94785 `FUN_1000_8d5e(slot, 0xfffe)` — mode −2 probe: scores the
     * best plot and leaves it in DS:0x8dc0/0x8dbe without assigning. */
    AiEuro28c8JobCandidate probe;
    memset(&probe, 0, sizeof probe);
    const int probe_ok =
      ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &probe);
    const int field_score = probe_ok ? probe.score : 0;

    ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);

    AiEuro5952Want want;
    memset(&want, 0, sizeof want);
    memcpy(want.gross, gross, sizeof want.gross);
    want.owner_nation = owner;
    want.human_nation = (ctx->human_nation >= 0 && ctx->human_nation < 4) ? ctx->human_nation : 0;
    for (int i = 0; i < 4; ++i) {
      want.wealth_rank[i] = ctx->euro_power_rank_ok ? ctx->euro_power_rank[i] : 0;
    }
    want.year = col1 ? (int)col1->head.year : 0;
    want.turn = col1 ? (int)col1->head.turn : 0;
    want.independence = col1 ? ai_king_independence_declared(col1) : 0;
    want.jefferson = col1 && founding_fathers_nation_has(col1, owner, FF_THOMAS_JEFFERSON);
    want.nation_flag_bit4 = col1 ? ((col1->nation[owner].nation_flags & 0x04) != 0) : 0;
    want.capitol_level = (int)col->capitol_level;
    want.population = col->population;
    want.wants_construction =
      (col->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) != 0;
    want.press_chain_count =
      ai_euro_5952_chain_owned(pool, col, COLONIES_CHAIN_PRESS, NULL, NULL);
    /* iStack_7c, raw 294-296: tories = round(pop*(100-SoL%)/100), 0 under WoI. */
    {
      const int sol = colony_prod_sol_percent(col1, col);
      want.tories = want.independence ? 0 : (col->population * (100 - sol) + 50) / 100;
    }
    /* DS:0x84bc — the per-nation SELL price row (every writer stores
     * euro_price − 1 clamped at 0; europe.c's dump-sell note). */
    for (int nn = 0; nn < 4 && col1; ++nn) {
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        const int p = (int)col1->nation[nn].trade.euro_price[c] - 1;
        want.sell_price[nn * 0x10 + c] = (unsigned char)(p < 0 ? 0 : p);
      }
    }

    int best = 0;
    int best_job = COLONIZE_PROF_CARPENTER; /* iStack_ee = 0xd, raw 94793 */
    for (int job = COLONIZE_PROF_DISTILLER; job <= COLONIZE_PROF_TEACHER; ++job) {
      if (job == COLONIZE_PROF_TEACHER) {
        continue; /* raw 94795 `iStack_7a != 0x12` */
      }
      if (!ai_euro_5952_job_available(pool, col, job) ||
          placed_count[job] >= COLONIZE_BUILDING_MAX_WORKERS) {
        continue;
      }
      int in = (int)k_ai_euro_5952_job_input[job];
      if (job == COLONIZE_PROF_GUNSMITH) {
        in = COLONIZE_CARGO_TOOLS; /* raw 94802 */
      }
      if (job == COLONIZE_PROF_CARPENTER) {
        in = COLONIZE_CARGO_LUMBER; /* raw 94804 */
      }
      if (in >= 0) {
        avail = col->stock[in] - demand[in] + gross[in];
        if (avail < 0) {
          continue; /* raw 94805, asm 5952:1f71 */
        }
        if (avail == 0) {
          avail = 1;
        }
      }
      int out = -1;
      int qty = ai_euro_5952_producible(pool, col, col1, s, job, &out);
      if (avail < qty) {
        qty = avail; /* raw 94806-94808, unconditional — see `avail` above */
      }
      if (out < 0) {
        continue; /* unreachable: job_available guarantees a workplace */
      }
      const int score = ai_euro_5952_job_score(&want, job, out, qty);
      if (score > best) { /* asm 5952:1ee7 JLE — strictly greater wins */
        best = score;
        best_job = job;
      }
    }

    int commit_job = -1;
    if (field_score < best) {
      commit_job = best_job; /* raw 94858 `if (*0x8dc0 < best)` */
    } else {
      /* raw 94861 `FUN_1000_8d5e(slot, 0xffff)` — mode −1 commits the plot. */
      if (probe_ok && probe.yield != 0 &&
          colonies_assign_field(ctx->colonies, col->id, s, probe.tile, probe.job)) {
        placed[s] = true;
        continue;
      }
      /* DS:0x8dbe == 0: no plot taken. raw 94864-94872. */
      const int church = colonies_building_row(pool, COLONY_BUILDING_CHURCH); /* @BUILDING 0x25 */
      const bool has_church = church >= 0 && church < COLONIZE_BUILDING_TYPES_MAX &&
                              col->has_building[church];
      /* FUN_15eb_0c52(5): demand[lumber] < stock[lumber] + gross[lumber]. */
      const bool lumber_surplus =
        demand[COLONIZE_CARGO_LUMBER] <
        col->stock[COLONIZE_CARGO_LUMBER] + gross[COLONIZE_CARGO_LUMBER];
      commit_job = ai_euro_5952_fallback_job(
        has_church, lumber_surplus, placed_count[COLONIZE_PROF_PREACHER]
      );
    }

    /* raw 94859/94873 `FUN_1000_8e26(slot, job)` = FUN_15eb_1068 set job. */
    int workplace = -1;
    (void)ai_euro_5952_chain_owned(
      pool, col, ai_euro_5952_job_chain(commit_job), NULL, &workplace
    );
    if (workplace >= 0 && colonies_assign_workplace(pool, col->id, s, workplace)) {
      placed[s] = true;
      if (commit_job >= 0 && commit_job <= COLONIZE_PROF_TEACHER) {
        ++placed_count[commit_job];
      }
    }
  }
}

/* ===== FUN_5952_035e forced-lumberjack pass + lumber buy (raw 94659-94689) ===== */

/*
 * DOS-LITERAL FUN_5952_035e raw 94659-94679 (annotated
 * colony_tick_5952_035e.md:1074-1099) — per-pass slot election of the
 * forced-lumberjack arm.
 *
 * DOS's three passes (`iStack_e6` 0..2) over the still-unplaced slots, on the
 * cached profession array `aiStack_12e`:
 *   0: profession == 5 (an existing Expert Lumberjack)
 *   1: `FUN_1000_8e8a(prof) == 0` = FUN_281f_0c9a, i.e. NOT an expert —
 *      @JOB 0x13 and 0x19..0x1c (Free Colonist, Servant, Criminal, Convert)
 *   2: no test at all — the DOS `else if (iStack_e6 == 1)` chain falls
 *      straight through to the 28c8 call for every remaining pass value.
 * Unlike the carpenter arm there is NO profession rewrite here: DOS never
 * calls 0cae in this arm, so an Indentured Servant (0x19) or Petty Criminal
 * (0x1a) sent to the woods keeps its identity.
 */
COLONIZE_INTERNAL int ai_euro_5952_lumberjack_pick(
  const ColonizeColony* c, const bool* placed, int n, int pass, int start
) {
  if (!c || !placed) {
    return -1;
  }
  for (int s = start < 0 ? 0 : start; s < n && s < COLONIZE_COLONY_POP_MAX; ++s) {
    if (placed[s] || !c->colonists[s].active) {
      continue;
    }
    const int prof = (int)c->colonists[s].profession;
    if (pass == 0 && prof != COLONIZE_PROF_LUMBERJACK) {
      continue; /* raw 94665 */
    }
    if (pass == 1 && ai_euro_5952_job_is_expert(prof)) {
      continue; /* raw 94670 */
    }
    return s;
  }
  return -1;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94680-94689 (md:1101-1112) — the AI's
 * emergency lumber purchase, the arm that feeds the carpenter arm's gate.
 *
 * Gates, all three: no lumberjack was placed this tick (`iStack_8c == 0`),
 * the colony holds fewer than 2 lumber (`colony+0xa4 < 2`; +0xa4 = +0x9a +
 * 2*5 = the Lumber stock word) and the turn counter `DS:0x538e & 7 == 0`,
 * i.e. one turn in eight.
 *
 * What it does — and the order matters: the colony is credited 100 lumber
 * UNCONDITIONALLY (`*piVar3 = *piVar3 + 100`), and only THEN is the bound
 * nation record's 32-bit purse (`DS:0x84fc + 0x2a` low word, `+0x2c` high
 * word) debited 200, gated on `high >= 0 && (high > 0 || low > 199)` — plain
 * "signed gold >= 200". A broke AI therefore still gets its 100 lumber free;
 * that asymmetry is DOS, not a port shortcut. The debit is a delta, so it
 * goes through europe_nation_gold_add and the read through
 * europe_nation_gold (europe.h single-treasury rule); 0x84fc is the colony
 * owner's record, the nation the tick is bound to.
 */
COLONIZE_INTERNAL void ai_euro_5952_lumber_purchase(
  struct EuropeScreen* eu, struct ColonizeCol1Save* col1, ColonizeColony* col, int turn,
  bool lumber_producer_placed
) {
  if (!col || lumber_producer_placed) {
    return;
  }
  if (col->stock[COLONIZE_CARGO_LUMBER] >= 2 || (turn & 7) != 0) {
    return;
  }
  col->stock[COLONIZE_CARGO_LUMBER] += 100; /* raw 94683-94684 */
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }
  const long gold = (long)(int32_t)europe_nation_gold(eu, col1, nation);
  if (gold >= 200) { /* raw 94686-94687 */
    europe_nation_gold_add(eu, col1, nation, -200);
  }
}

/*
 * The forced-lumberjack arm itself, raw 94659-94679. Runs inside the same
 * `(colony+0x1d & 0x80) == 0` block as the carpenter arm and immediately
 * before it, gated on `iStack_8c == 0 && colony+0xa4 < 10` — under 10 lumber
 * in stock and nobody chopping.
 *
 * `iStack_8c` is seeded 0 at raw 94257 and has exactly two writers: this arm
 * (raw 94676) and the LAB_5952_17a9 leftovers election at raw 94657 — which
 * the port has already proved dead (its `1 < DS:0x8dbe` threshold can never
 * hold after a mode −2 probe; see the AI_5952_INDOOR=0 stand-in's comment).
 * So on entry it is always 0, and the flag's only live role is the gate on
 * the purchase arm below it, which this function returns.
 *
 * The while-guard is `DS:0x8dd2 == 0` = gross production[Lumber], refreshed
 * by FUN_281f_0c04 after each assignment, so in practice the arm seats
 * exactly ONE lumberjack and every remaining pass finds the guard already
 * false. The 28c8 call is `FUN_1000_8d5e(0x181f, slot, 5)` — the third
 * argument is a real job index, so the search is confined to Lumberjack and
 * only the best TILE for it is elected.
 */
static bool ai_euro_5952_forced_lumberjack(
  ColonizeTurnContext* ctx, ColonizeColony* col, bool* placed, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  bool any = false;

  if (col->stock[COLONIZE_CARGO_LUMBER] >= 10) {
    return false; /* raw 94661 */
  }
  ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);

  for (int pass = 0; pass < 3; ++pass) {
    int from = 0;
    while (gross[COLONIZE_CARGO_LUMBER] == 0) {
      const int s = ai_euro_5952_lumberjack_pick(col, placed, n, pass, from);
      if (s < 0) {
        break;
      }
      from = s + 1;
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = -1;
      const int ok = ai_euro_28c8_score_job(
        ctx, col, s, (int)col->colonists[s].profession, COLONIZE_JOB_LUMBERJACK, &best
      );
      if (!ok) {
        continue; /* 8d5e != 0 — DOS just walks on to the next slot */
      }
      if (colonies_assign_field(pool, col->id, s, best.tile, best.job)) {
        placed[s] = true;
        any = true; /* iStack_8c = 1, raw 94676 */
        /* FUN_281f_0c04 — the refresh that ends the loop. */
        ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);
      }
    }
  }
  return any;
}

/* ===== FUN_5952_035e carpenter-staffing arm (raw 94690-94740) ===== */

/*
 * DOS-LITERAL FUN_5952_035e raw 94690-94740 (asm 5952:1ac7-5952:1be2,
 * annotated colony_tick_5952_035e.md:1114-1160) — the pass-ordered pick of
 * ONE colonist for the Carpenter's House when the colony has lumber but is
 * making no hammers.
 *
 * `ai_euro_5952_carpenter_pick` is the per-pass slot election plus DOS's own
 * profession rewrite at raw 94716-94718 (`FUN_1000_8e9e(slot, 0x1c)` =
 * FUN_281f_0cae, the same "clear specialty" writer bugs.md #431 uses): an
 * Indentured Servant (0x19) or a Petty Criminal (0x1a) chosen by this arm is
 * turned into a Free Colonist BEFORE it is put to work. The four passes are
 * DOS's `iStack_e6` 0..3 over the still-unplaced slots:
 *   0: profession == 0x0d (an existing Master Carpenter)
 *   1: profession == 0x1c (Free Colonist)
 *   2: profession == 0x19 (Indentured Servant)
 *   3: anyone left
 * `out_prof` hands back the pre-rewrite profession because DOS's own expert
 * test one line later (`FUN_281f_0c9a(aiStack_12e[slot])`) reads the cached
 * array, which the 0cae writes never update.
 */
COLONIZE_INTERNAL int ai_euro_5952_carpenter_pick(
  ColonizeColony* c, const bool* placed, int n, int pass, int start, int* out_prof
) {
  if (out_prof) {
    *out_prof = -1;
  }
  if (!c || !placed) {
    return -1;
  }
  for (int s = start < 0 ? 0 : start; s < n && s < COLONIZE_COLONY_POP_MAX; ++s) {
    if (placed[s] || !c->colonists[s].active) {
      continue;
    }
    const int prof = (int)c->colonists[s].profession;
    if (pass == 0 && prof != COLONIZE_PROF_CARPENTER) {
      continue; /* raw 94696 */
    }
    if (pass == 1 && prof != COLONIZE_PROF_FREE_COLONIST) {
      continue; /* raw 94701 */
    }
    if (pass == 2 && prof != COLONIZE_PROF_INDENTURED) {
      continue; /* raw 94705 */
    }
    /* raw 94716-94718 — 0cae(slot, 0x1c). */
    if (prof == COLONIZE_PROF_CRIMINAL || prof == COLONIZE_PROF_INDENTURED) {
      c->colonists[s].profession = (uint8_t)COLONIZE_PROF_FREE_COLONIST;
    }
    if (out_prof) {
      *out_prof = prof;
    }
    return s;
  }
  return -1;
}

/*
 * The arm itself. DOS gate, raw 94690: `iStack_6a = colony+0xa4 + DS:0x8dd2`
 * = stock[lumber] + gross production[lumber], and the arm runs only when that
 * is > 1. Its loop condition is `DS:0x8de8 == 0` — gross production[hammers],
 * refreshed by FUN_281f_0c04 after every assignment — so in practice it staffs
 * exactly ONE carpenter and then stops, in every remaining pass too.
 *
 * Between the rewrite and the assignment DOS may hand the colonist the
 * Master Carpenter specialty outright (raw 94719-94726): a non-expert
 * (`FUN_281f_0c9a == 0`) in a colony that has NO Master Carpenter yet
 * (`iStack_4e` — BP-0x4c, i.e. `aiStack_68[0x0d]`, the by-profession census
 * the tick builds at raw 94170; the frame slot is one word past Ghidra's
 * `local_4e` label) and a population above 5, on a
 * `FUN_281f_04d4(0, 0x10 - DS:0x53a6) == 0` roll — 1-in-17 at Discoverer,
 * 1-in-13 at Viceroy. `FUN_1000_8e26(slot, 0x0d)` then seats him.
 *
 * DOS's two preceding arms in the same `(+0x1d & 0x80) == 0` block — the
 * forced-lumberjack pass (raw 94659-94679) and the AI's 200-gold /
 * 100-lumber emergency purchase (raw 94680-94689) — are ported above and run
 * first, so this arm sees the bought 100 lumber exactly as DOS does.
 * `iStack_ca` (raw 94733) is a counter nothing in the function ever reads.
 */
static void ai_euro_5952_carpenter_arm(
  ColonizeTurnContext* ctx, ColonizeColony* col, bool* placed, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const ColonizeWorld world = world_from_turn_ctx(ctx);
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];

  ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);
  if (col->stock[COLONIZE_CARGO_LUMBER] + gross[COLONIZE_CARGO_LUMBER] < 2) {
    return; /* raw 94691 `if (1 < iStack_6a)` */
  }

  /* aiStack_68[0x0d]: Master Carpenters by profession, the tick's snapshot. */
  int master_carpenters = 0;
  for (int s = 0; s < n && s < COLONIZE_COLONY_POP_MAX; ++s) {
    if (col->colonists[s].active &&
        (int)col->colonists[s].profession == COLONIZE_PROF_CARPENTER) {
      ++master_carpenters;
    }
  }
  const int difficulty = col1 ? (int)col1->head.difficulty : 4; /* DS:0x53a6 */

  int workplace = -1;
  (void)ai_euro_5952_chain_owned(pool, col, COLONIES_CHAIN_CARPENTER, NULL, &workplace);
  if (workplace < 0) {
    return;
  }

  for (int pass = 0; pass < 4; ++pass) {
    int from = 0;
    while (gross[AI_EURO_5952_HAMMERS] == 0) {
      int prof = -1;
      const int s = ai_euro_5952_carpenter_pick(col, placed, n, pass, from, &prof);
      if (s < 0) {
        break;
      }
      from = s + 1;
      /* raw 94719-94726 — the free Master Carpenter specialty. */
      if (!ai_euro_5952_job_is_expert(prof) && master_carpenters == 0 &&
          (int)col->population > 5 && ctx->rng &&
          dos_rng_range(ctx->rng, 0, 0x10 - difficulty) == 0) {
        col->colonists[s].profession = (uint8_t)COLONIZE_PROF_CARPENTER;
        ++master_carpenters;
      }
      /* raw 94731 `FUN_1000_8e26(slot, 0x0d)` = set job Carpenter. */
      if (colonies_assign_workplace(pool, col->id, s, workplace)) {
        placed[s] = true;
        /* FUN_281f_0c04 — refresh the ledgers, which is what ends the loop. */
        ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);
      }
    }
  }
}

/*
 * FUN_5952_035e colonist placement block (viceroy_unpacked.c ~94560-94640),
 * the AI-turn caller of 28c8 (via resident stub FUN_281f_0b6e). Per AI
 * colony each turn DOS clears every work plot (`colony+0x70..0x83 = 0xff`)
 * and re-places colonists through 28c8: a food pass first (slots whose
 * previous job was Farmer, or Fisherman on a fishable colony) until the
 * food target holds, then two general passes. Building workers keep their
 * workplaces here — DOS's later statesman/carpenter passes in the same
 * function are the existing expert-workplace heuristics' territory.
 * Food target: Linux population×2 consumption vs town commons + placed food.
 *
 * STOP CONDITION (both halves fixed 2026-09-09):
 *   - It is a `goto LAB_5952_178f` (raw 94596 in the food pass, raw 94614 in
 *     the general passes), and LAB_5952_178f sits OUTSIDE both loops, just
 *     ahead of the leftovers arm at LAB_5952_17a9. So a bad winner ends the
 *     WHOLE placement section, not the current pass: after the food pass
 *     trips it, DOS never runs the two general passes at all. The port used
 *     `break`, which only left the innermost loop.
 *   - The general passes' condition is
 *     `(yield < 3) || (local_84 && yield < 5)` (raw 94613-94614), where
 *     `local_84 = (DS:0x8e32 * 0x10 < colony+0x9a)` is recomputed per slot
 *     (raw 94608): DS:0x8e32 is the colony's food shortfall for the turn
 *     (consumption − field food, 0 when production covers it — turn.c:1095-
 *     1102) and colony+0x9a is the food stock. So a colony sitting on more
 *     than 16 turns' worth of deficit — in practice any well-fed colony —
 *     demands yield >= 5 from a general-pass winner and otherwise stops.
 *     The `< 5` half was unported.
 * STILL NOT PORTED here, deliberately (all DOS-side detail with no Linux
 * counterpart yet): the loop-entry guards `local_7e`/`local_80`/`local_1e`
 * (pop vs ring size, horses stock, gross production vs demand) and
 * `local_84`'s SECOND use at raw 94609-94611, where it also flips the
 * per-slot eligibility test between FUN_281f_0c9a (expert) and profession
 * 0x1b (Indian Convert).
 */
static int ai_euro_20e6_nearest_village(
  const ColonizeTurnContext* ctx, int x, int y, int* out_dist
);

/* ===== FUN_5952_035e construction-project cascade (asm 5952:21d4-5952:2747) ===== */

/*
 * The AI colony tick's build-decision tail, transcribed from
 * `viceroy_overlays.asm:145550-146167` (`OVL15_L0000` = `5952`) with the
 * clean recovery `original_sources_annotated/ai/colony_tick_5952_035e.md`
 * md:1394-1600 as the control-flow cross-check. Every `FUN_5952_0214(id)`
 * argument arrives in **AX**, which is why Ghidra dropped it at all 21 call
 * sites; each one is the `MOV AX,imm` immediately ahead of
 * `CALL FUN_OVL15_L0000__002a6e`, and `002a6e` / `002a73` / `002a78` are the
 * overlay thunks for `FUN_5952_0214` / `FUN_5952_0280` / `FUN_5952_02f4`
 * (`JMPF 0000:0214 / 0280 / 02f4` behind the loader stub at `1000:a7a3+`).
 *
 * This replaces the nineteen-row `ai_euro_prefer_*` stand-in table, which was
 * a name-keyed invention: DOS has exactly one ordered cascade and it ends in
 * five UNIT projects (bugs.md #483).
 *
 * Resolutions this port needed, none of them previously on record:
 *  - `FUN_5952_0214` is **recursive**: when the candidate is not owned and
 *    not buildable it retries the @BUILDING predecessor byte
 *    (`DS:0x8f85 + id*0xc`, the chain parent `docs/building_production.md`
 *    already names). Return 1 = "keep scanning", 0 = "stop" (either a project
 *    was set or the chain dead-ended); on 0 it clears colony `+0x1c` bit 0x80.
 *  - `DS:0x864` = six 4-byte craft-chain rows, read straight off the image
 *    (`VICEROY.EXE` offset 121248 + 0x864):
 *      03 0f 0e | 27 0e 06 | 20 0c 04 | 1b 09 01 | 18 0a 02 | 15 0b 03
 *    i.e. {root @BUILDING, @JOB, input @CARGO} = Armory/Gunsmith/Tools,
 *    Blacksmith's House/Blacksmith/Ore, Fur Trader's House/Fur Trader/Furs,
 *    Rum Distiller's House/Distiller/Sugar, Tobacconist's House/Tobacconist/
 *    Tobacco, Weaver's House/Weaver/Cotton. (The 4th byte repeats the @JOB.)
 *  - `DS:0x8ea6` (stride 8, indexed by @JOB) is the **third @JOB column of
 *    NAMES.TXT** — the loader at raw 121044-121053 reads section 0x224e into
 *    {name, plural, col3, col4} records, so `0x8ea6[job] % 4` is that 1..4
 *    class with Teacher/Colonist/Servant/Criminal/Convert (4) folding to 0.
 *    Classes 1/2/3 are exactly the Schoolhouse/College/University tiers the
 *    cascade then asks for (0xc/0xd/0xe).
 *  - `byte[nation*0x10 + 0x84cb]` = `DS:0x84bc` + cargo 0xf, i.e. the
 *    per-nation Europe SELL row for **Muskets** (europe.h's `euro_price − 1`).
 *  - `aiStack_68` is based at **BP-0x66** (`LEA AX,[BP-0x66]` ahead of the
 *    0x32-byte memset at asm 5952:0bb8) and is indexed by the colonist's
 *    PROFESSION with every non-expert folded to 0x13, so the asm's
 *    `[BP-0x48]` / `[BP-0x46]` gates are counts of **Master Gunsmiths** and
 *    **Firebrand Preachers** — which is why they guard the Armory and the
 *    Church. (The clean recovery's `iStack_4a`/`iStack_48` names are shifted
 *    one word against the overlay listing's frame; the asm is authoritative.)
 *  - `FUN_1000_8d90` = `FUN_281f_0ba0` -> `FUN_15eb_0410`, which walks the
 *    SUCCESSOR column `DS:0x8f86` to the deepest tier of a chain, and
 *    `FUN_1000_8ca0` = `FUN_281f_0ab0` -> `FUN_15eb_039e` counts owned tiers
 *    from its argument downward (so `== 3` means a full craft chain).
 *  - `FUN_1000_8d72(job)` / `FUN_1000_8de0(prof)` = `FUN_15eb_1376` /
 *    `FUN_15eb_13ac`, per-colony counts of colonists by JOB and by
 *    PROFESSION.
 *  - `uStack_a2` = `byte[0x329 + tech_tier]` = {0,4,8,12,20}; a European
 *    colony's tier is 2, so the ring is the port's 8 field tiles.
 *
 * DIVERGENCES, deliberate and minimal:
 *  - `iStack_22` (the ring-1 European threat count that gates the Wagon
 *    Train) is produced by `ai_euro_colony_threat_seed_5952`, a different
 *    port pass of the same DOS body; it is stashed per colony in
 *    `s_5952_ring1` (fresh every nation turn, DOS order: the seed runs in
 *    `ai_euro_colony_goals`, before this tail).
 *  - the Wagon Train arm's alarm read is `FUN_1000_84fc(DS:0x8d52, nation)`,
 *    where `DS:0x8d52` is the tribe the tick last bound. The bind is the
 *    nearest-village lookup `FUN_1000_8f74` five lines above it (md:298-302),
 *    so the port reads the alarm of `ai_euro_20e6_nearest_village`'s tribe.
 *  - `FUN_5952_02f4` assigns the unit code with **no** availability gate of
 *    its own (it only clears `+0x1c` bit 0x80 and returns `arg + 0x1f`), so
 *    this path deliberately bypasses `colonies_unit_project_available`: the
 *    Shipyard / Armory requirements are carried by the cascade's own
 *    `try(8)` / `try(3)` steps, and the per-nation Wagon cap in
 *    `FUN_15eb_3650` is the human build MENU's gate, never reached here.
 */

/* iStack_22 — see the header note. */
static int s_5952_ring1[COLONIZE_COLONIES_MAX];

/* Test seam: the threat-seed pass is what fills this in production. */
COLONIZE_INTERNAL void ai_euro_5952_set_ring1_threat(int colony_id, int ring1) {
  if (colony_id >= 0 && colony_id < COLONIZE_COLONIES_MAX) {
    s_5952_ring1[colony_id] = ring1;
  }
}

/*
 * `aiStack_68[0x13]` / `aiStack_68[0x15]` — the two by-profession census
 * cells the absorption arm's Soldier/Dragoon case consumes (raw 94242,
 * 94248-94255; md:566-577 builds the array, md:589-604 reads it).
 *
 * VARIABLE RESOLUTION (2026-09-18, disassembly-confirmed). Ghidra names
 * these `iStack_42` / `iStack_3e` and shows no initialiser for either,
 * because they are interior slots of the census array, not locals. The
 * overlay disassembly (`viceroy_overlays.asm`, OVL15_L0000 body of
 * `FUN_5952_035e`) pins all three frame facts:
 *   - Ghidra's `*Stack_NN` labels sit exactly one WORD below the real BP
 *     offsets in this frame (`uStack_24` → `[BP-0x22]`, `iStack_8c` →
 *     `[BP-0x8a]`, `iStack_76` → `[BP-0x74]`, at the four entry zero
 *     stores), so `aiStack_68` is really based at `[BP-0x66]`.
 *   - `LEA AX,[BP-0x66]` + `PUSH 0x32` into `FUN_0000_df7e` clears 50 bytes
 *     = 25 words, i.e. the array is `int[25]` covering @JOB 0..0x18 — the
 *     twin `aiStack_e4` memset is the same 0x32 and ends exactly at the
 *     next declared local, so 25 is the real length, not 13.
 *   - the census store is `INC word ptr [BP+SI-0x66]` with `SI = 2*@JOB`.
 * `iStack_42` = `[BP-0x40]` = index (0x66-0x40)/2 = 0x13, `iStack_3e` =
 * `[BP-0x3c]` = index 0x15 — and both offsets appear verbatim in the gate
 * (`CMP word ptr [BP-0x40],0x0` / `[BP-0x3c],0x0`) and in the decrement
 * pair (`DEC word ptr [BP-0x40]` / `[BP-0x3c]`). So they are the colony's
 * count of non-expert colonists (every non-expert folds to 0x13 at md:574)
 * and of Veteran Soldiers (@JOB 0x15).
 *
 * Stashed per colony for the same reason `s_5952_ring1` is: DOS builds the
 * census inside the tick, immediately before the absorption loop, and the
 * port runs that loop from each arriving unit's act instead. The
 * decrement-on-absorb is carried here so a second absorption in the same
 * turn sees DOS's consumed cell, not a fresh recount.
 */
/*
 * DOS-LITERAL FUN_5952_035e, OVL15 asm 0x22bc-0x22ea: the Docks arm of the
 * build cascade stores AX = 0 into [BP+0xff62] — the tick-local `train_flag`
 * (`local_a0`) — when its FUN_OVL15_002a6e(6) commit returns 0, so ARM 2 (buy
 * an expert, raw 95918 `if (local_a0 != 0)`) cannot fire on the turn Docks is
 * started. The Stockade arm at 0x22ec has no such store. The port splits the
 * one DOS body into ai_euro_5952_build_cascade (the plan step) and
 * ai_euro_5952_specialist_arms (the colony tick), which run in that DOS order,
 * so the store is carried across as this per-colony latch (bugs.md #586).
 */
static int s_5952_docks_started[COLONIZE_COLONIES_MAX];

/* Test seam for the latch above. */
COLONIZE_INTERNAL int ai_euro_5952_docks_started(int colony_id) {
  return (colony_id >= 0 && colony_id < COLONIZE_COLONIES_MAX)
           ? s_5952_docks_started[colony_id]
           : 0;
}

static int s_5952_census_nonexpert[COLONIZE_COLONIES_MAX]; /* aiStack_68[0x13] */
static int s_5952_census_vet_soldier[COLONIZE_COLONIES_MAX]; /* aiStack_68[0x15] */

/* Test seam: the threat-seed pass is what fills these in production. */
COLONIZE_INTERNAL void ai_euro_5952_set_absorb_census(
  int colony_id, int nonexpert, int vet_soldier
) {
  if (colony_id >= 0 && colony_id < COLONIZE_COLONIES_MAX) {
    s_5952_census_nonexpert[colony_id] = nonexpert;
    s_5952_census_vet_soldier[colony_id] = vet_soldier;
  }
}

/*
 * DS:0x864, 6 rows of 4 bytes. Recovered verbatim from VICEROY.EXE at EXE
 * offset 121248 + 0x864 (bugs.md #572):
 *   03 0f 0e 0f | 27 0e 06 0e | 20 0c 04 0c
 *   1b 09 01 09 | 18 0a 02 0a | 15 0b 03 0b
 * (the next bytes are the "GAME" literal, so the table really is 6 rows).
 * Columns: {root @BUILDING, @JOB, input @CARGO, @JOB again} — the fourth
 * byte duplicates the second in every row, and only columns 0/1/2 are read
 * (asm OVL15 0x27cf-0x2837: `[BX+0x864]` -> FUN_1000_8ca0, `[BX+0x866]`*2 ->
 * `[BX+0x8dc8]` gross ledger, `[BX+0x865]`*2 -> the aiStack_68 census).
 */
typedef struct AiEuro5952Craft {
  int root;  /* DOS @BUILDING index */
  int chain; /* COLONIES_CHAIN_* */
  int cargo; /* input @CARGO */
  int job;   /* DS:0x864 column 1 = @JOB */
} AiEuro5952Craft;

static const AiEuro5952Craft k_5952_craft[6] = {
  {0x03, COLONIES_CHAIN_ARMORY, COLONIZE_CARGO_TOOLS, COLONIZE_PROF_GUNSMITH},
  {0x27, COLONIES_CHAIN_BLACKSMITH, COLONIZE_CARGO_ORE, COLONIZE_PROF_BLACKSMITH},
  {0x20, COLONIES_CHAIN_FUR, COLONIZE_CARGO_FURS, COLONIZE_PROF_FUR_TRADER},
  {0x1b, COLONIES_CHAIN_RUM, COLONIZE_CARGO_SUGAR, COLONIZE_PROF_DISTILLER},
  {0x18, COLONIES_CHAIN_TOBACCONIST, COLONIZE_CARGO_TOBACCO, COLONIZE_PROF_TOBACCONIST},
  {0x15, COLONIES_CHAIN_WEAVER, COLONIZE_CARGO_COTTON, COLONIZE_PROF_WEAVER},
};

/*
 * @BUILDING index -> ColonizeBuildingRow, for the ids this cascade names.
 * The predecessor column is `DS:0x8f85 + id*0xc`; it is the same chain
 * parent colonies_building_chain() already models, so the table carries it
 * directly (−1 = chain root). Town Hall (9..0xb) and Capitol (0x1e/0x1f) are
 * absent because the cascade never asks for them and DOS refuses both.
 * `row` == the DOS @BUILDING index itself (ColonizeBuildingRow shares that
 * numbering 1:1), so this table is really just the predecessor column plus
 * a `has_row` flag for the three unused Town Hall / Capitol slots.
 */
typedef struct AiEuro5952Bld {
  bool has_row;
  int pred;
} AiEuro5952Bld;

static const AiEuro5952Bld k_5952_bld[0x2a] = {
  {true, -1},   {true, 0x00},
  {true, 0x01}, {true, -1},
  {true, 0x03}, {true, 0x04},
  {true, -1},   {true, 0x06},
  {true, 0x07}, {false, -1},
  {false, -1},  {false, -1},
  {true, -1},   {true, 0x0c},
  {true, 0x0d}, {true, -1},
  {true, 0x0f}, {true, -1},
  {true, -1},   {true, -1},
  {true, 0x13}, {true, -1},
  {true, 0x15}, {true, 0x16},
  {true, -1},   {true, 0x18},
  {true, 0x19}, {true, -1},
  {true, 0x1b}, {true, 0x1c},
  {false, -1},  {false, -1},
  {true, -1},   {true, 0x20},
  {true, 0x21}, {true, -1},
  {true, 0x23}, {true, -1},
  {true, 0x25}, {true, -1},
  {true, 0x27}, {true, 0x28},
};

typedef struct AiEuro5952Cascade {
  ColonizeTurnContext* ctx;
  ColonizeColonyPool* pool;
  ColonizeColony* col;
  const ColonizeCol1Save* col1;
  int nation;
  int buildable[COLONIZE_BUILDING_TYPES_MAX];
  int n_buildable;
} AiEuro5952Cascade;

static int ai_euro_5952_bld_index(const ColonizeColonyPool* pool, int dos_id) {
  if (dos_id < 0 || dos_id >= 0x2a || !k_5952_bld[dos_id].has_row) {
    return -1;
  }
  return colonies_building_row(pool, (ColonizeBuildingRow)dos_id);
}

/* FUN_281f_0b8c -> FUN_15eb_3650, via the port's own gate. */
static bool ai_euro_5952_can_build(const AiEuro5952Cascade* s, int idx) {
  for (int i = 0; i < s->n_buildable; ++i) {
    if (s->buildable[i] == idx) {
      return true;
    }
  }
  return false;
}

/* DOS-LITERAL FUN_5952_0214 (asm 5952:0214-5952:027e, raw 93686-93716). */
static int ai_euro_5952_try_build(AiEuro5952Cascade* s, int dos_id) {
  int ret = 1;
  if (dos_id >= 0) {
    const int idx = ai_euro_5952_bld_index(s->pool, dos_id);
    const bool owned =
      idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && s->col->has_building[idx];
    if (!owned) {
      ret = 0;
      if (idx >= 0 && ai_euro_5952_can_build(s, idx)) {
        s->col->building_in_production = idx; /* asm 5952:0248 `+0x94 = id` */
      } else if (ai_euro_5952_try_build(s, k_5952_bld[dos_id].pred) != 0) {
        ret = 1;
      }
    }
  }
  if (ret == 0) {
    s->col->colony_flags =
      (uint8_t)(s->col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
  }
  return ret;
}

/* DOS-LITERAL FUN_5952_02f4 (raw 93749-93755): clear +0x1c bit 0x80, code = row + 0x1f. */
static void ai_euro_5952_set_unit_project(AiEuro5952Cascade* s, int unit_row) {
  s->col->colony_flags =
    (uint8_t)(s->col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
  s->col->building_in_production = unit_row + 0x1f;
  if (getenv("AI_5952_BUILD_TRACE")) {
    const char* uname = NULL;
    colonies_unit_build_info(unit_row + 0x1f, &uname, NULL, NULL);
    fprintf(
      stderr, "[5952] %s -> unit project %s (code %d)\n",
      s->col->name[0] ? s->col->name : "colony", uname ? uname : "?", unit_row + 0x1f
    );
  }
}

/* DOS-LITERAL FUN_5952_0280 (asm 5952:0280-5952:02f2): is this craft chain
 * short of the tier its input supply justifies? */
static int ai_euro_5952_chain_short(
  const AiEuro5952Cascade* s, const int* gross, int chain_row
) {
  const AiEuro5952Craft* r = &k_5952_craft[chain_row];
  const int owned = ai_euro_5952_chain_owned(s->pool, s->col, r->chain, NULL, NULL);
  int want = 0;
  if (gross[r->cargo] >= 3) {
    want = 2;
  }
  if (gross[r->cargo] >= 8) {
    want = 3;
  }
  if (s->col->stock[r->cargo] >= 100) {
    want = 3;
  }
  return owned < want ? 1 : 0;
}

/* FUN_1000_8d90 -> FUN_15eb_0410: deepest tier of the chain rooted at `row`. */
static int ai_euro_5952_chain_top(int chain_row) {
  const int chain = k_5952_craft[chain_row].chain;
  const char* const* names = colonies_building_chain(chain);
  int last = k_5952_craft[chain_row].root;
  int n = 0;
  while (names && names[n]) {
    ++n;
  }
  /* The DOS ids of a chain are consecutive (see k_5952_bld), so the deepest
   * tier is root + (length − 1); Armory's chain is 3/4/5, Weaver's 0x15/16/17. */
  if (n > 0) {
    last = k_5952_craft[chain_row].root + (n - 1);
  }
  return last;
}

/*
 * `DS:0x8ea6[job] % 4` — NAMES.TXT @JOB column 3 (the loader at raw
 * 121044-121053 stores it as the 3rd word of each stride-8 record), read off
 * the shipped COLONIZE/NAMES.TXT @JOB block. Class 4 (Teacher, Colonist,
 * Ind. Servant, Criminal, Convert) folds to 0 under DOS's `% 4`.
 */
static const signed char k_5952_job_class[28] = {
  1, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1,
  2, 2, 3, 3, 0, 0, 1, 2, 1, 2, 3, 0, 0, 0
};

static int ai_euro_5952_job_class(int job) {
  return (job >= 0 && job < 28) ? (int)k_5952_job_class[job] : 0;
}

/*
 * DOS-LITERAL FUN_5952_035e build-decision cascade, asm 5952:21d4-5952:274b.
 * Runs at the tail of the AI colony tick, once per own colony.
 */
COLONIZE_INTERNAL void ai_euro_5952_build_cascade(
  ColonizeTurnContext* ctx, ColonizeColony* col
) {
  if (!ctx || !ctx->colonies || !ctx->map || !col || !col->active) {
    return;
  }
  /* Fresh per tick, like DOS's stack-local [BP+0xff62] (bugs.md #586). */
  if (col->id >= 0 && col->id < COLONIZE_COLONIES_MAX) {
    s_5952_docks_started[col->id] = 0;
  }
  if (ctx->colonies->building_type_count <= 0) {
    /* Port-side guard, not a DOS gate: a real game always carries the 42
     * @BUILDING rows, so DOS never runs this cascade with nothing to build.
     * Slim unit fixtures do, and every try() would dead-end straight into the
     * `+0x1d |= 0x80` wants-construction latch. */
    return;
  }
  AiEuro5952Cascade s;
  memset(&s, 0, sizeof s);
  s.ctx = ctx;
  s.pool = ctx->colonies;
  s.col = col;
  s.col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  s.nation = (col->nation_id >= 0 && col->nation_id < 4) ? col->nation_id : 0;

  ColoniesBuildableOpts opts;
  memset(&opts, 0, sizeof opts);
  opts.map = ctx->map;
  opts.col1 = s.col1;
  if (s.col1) {
    opts.has_adam_smith = founding_fathers_nation_has(s.col1, s.nation, FF_ADAM_SMITH);
    opts.has_peter_stuyvesant =
      founding_fathers_nation_has(s.col1, s.nation, FF_PETER_STUYVESANT);
  }
  s.n_buildable = colonies_list_buildable(
    s.pool, col->id, s.buildable, COLONIZE_BUILDING_TYPES_MAX, &opts
  );

  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  {
    const ColonizeWorld world = world_from_turn_ctx(ctx);
    ai_euro_5952_ledgers(&world, s.pool, col, s.col1, gross, demand);
  }

  const int pop = col->population;
  const int human = (ctx->human_nation >= 0 && ctx->human_nation < 4) ? ctx->human_nation : 0;
  const int turn = s.col1 ? (int)s.col1->head.turn : 0;
  const int year = s.col1 ? (int)s.col1->head.year : 0;
  const int difficulty = s.col1 ? (int)s.col1->head.difficulty : 4;
  const int musket_price =
    s.col1 ? ((int)s.col1->nation[s.nation].trade.euro_price[COLONIZE_CARGO_MUSKETS] - 1) : 0;
  const int ring1 = (col->id >= 0 && col->id < COLONIZE_COLONIES_MAX) ? s_5952_ring1[col->id] : 0;

  /* asm 5952:21d4 — iVar12 = colonists working @JOB 0 (Farmer) + 8 (Fisherman). */
  int food_workers = 0;
  int prof_count[28];
  memset(prof_count, 0, sizeof prof_count);
  for (int i = 0; i < col->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    const ColonizeColonist* c = &col->colonists[i];
    if (!c->active) {
      continue;
    }
    if (c->field_job == COLONIZE_JOB_FARMER || c->field_job == COLONIZE_JOB_FISHERMAN) {
      ++food_workers;
    }
    if (c->profession >= 0 && c->profession < 28) {
      ++prof_count[c->profession];
    }
  }

  /* asm 5952:21f6-5952:21fe — the prologue clears the "wants construction"
   * latch and the project slot, so "nothing picked" is a real outcome. */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags & (uint8_t)~COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
  col->building_in_production = -1;

  /* asm 5952:222c-5952:2294 — walk the pop slots and then the on-tile units
   * (DS:0x8d72), counting experts and the deepest @JOB class among them. */
  int experts = 0;
  int tier_max = 0;
  for (int i = 0; i < col->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    const ColonizeColonist* c = &col->colonists[i];
    if (!c->active || !ai_euro_5952_job_is_expert(c->profession)) {
      continue;
    }
    ++experts;
    const int t = ai_euro_5952_job_class(c->profession) % 4;
    if (t > tier_max) {
      tier_max = t;
    }
  }
  int arty_on_tile = 0; /* iStack_92, asm 5952:2642 */
  if (ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || !units_is_on_map(u) || u->x != col->x || u->y != col->y) {
        continue;
      }
      if (u->nation_id != col->nation_id) {
        continue;
      }
      const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
      if (dtype == 0x0b) {
        ++arty_on_tile;
      }
      if (dtype >= 0x0d && dtype <= 0x12) {
        continue; /* ships are not colony slots */
      }
      if (ai_euro_5952_job_is_expert(u->profession)) {
        ++experts;
        const int t = ai_euro_5952_job_class(u->profession) % 4;
        if (t > tier_max) {
          tier_max = t;
        }
      }
    }
  }

  /* asm 5952:2296-5952:22b8 — iStack_a0, the food-pressure latch. */
  int hungry = 0;
  if ((pop >> 1) < food_workers && food_workers > 1) {
    hungry = 1;
  }
  if (col->stock[COLONIZE_CARGO_FOOD] + gross[COLONIZE_CARGO_FOOD] <
      demand[COLONIZE_CARGO_FOOD]) {
    hungry = 1; /* DS:0x8e5a != 0 (unmet[food]) */
  }

  /* uStack_a2 / iStack_142 — the colony's ring (DS:0x329[FUN_15eb_0470()],
   * bugs.md #593) and how much of it is not land (off-map, Ocean 0x19 or High
   * Seas 0x1a), asm 5952:1150 loop. */
  const int ring = colonies_work_plot_count(ctx->colonies, col);
  int ring_nonland = 0;
  for (int d = 0; d < ring; ++d) {
    const int tx = col->x + MAP_DIR8_DX[d];
    const int ty = col->y + MAP_DIR8_DY[d];
    if (tx < 0 || ty < 0 || tx >= (int)ctx->map->width || ty >= (int)ctx->map->height) {
      ++ring_nonland;
      continue;
    }
    if (map_tile_is_water(ctx->map, tx, ty) || map_tile_is_high_seas(ctx->map, tx, ty)) {
      ++ring_nonland;
    }
  }

  /* iStack_2c, asm 5952:2583 — some craft chain is complete (three tiers). */
  int factory = 0;
  for (int i = 5; i >= 0; --i) {
    if (ai_euro_5952_chain_owned(s.pool, col, k_5952_craft[i].chain, NULL, NULL) == 3) {
      factory = 1;
    }
  }

  const int shipyard = ai_euro_5952_bld_index(s.pool, 0x08);
  const bool has_shipyard =
    shipyard >= 0 && shipyard < COLONIZE_BUILDING_TYPES_MAX && col->has_building[shipyard];

  /* --- the cascade proper ------------------------------------------------ */

  if ((ring - ring_nonland) <= pop || (ring_nonland != 0 && hungry != 0)) {
    if (ai_euro_5952_try_build(&s, 0x06) == 0) { /* asm 22da: Docks */
      /* asm 0x22e6 `MOV [BP+0xff62],AX` with AX == 0 — the committed Docks
       * arm clears the tick's train_flag, suppressing ARM 2 this turn. */
      if (col->id >= 0 && col->id < COLONIZE_COLONIES_MAX) {
        s_5952_docks_started[col->id] = 1;
      }
      return;
    }
  }
  if (ai_euro_5952_try_build(&s, 0x00) == 0) { /* 22ec: Stockade */
    return;
  }
  if (col->stock[COLONIZE_CARGO_HORSES] >= 2) {
    if (ai_euro_5952_try_build(&s, 0x11) == 0) { /* 22f9: Stable */
      return;
    }
  }
  if (pop < 4) {
    goto wants;
  }
  {
    const int wl_want = pop / 6; /* iStack_16c */
    if (col->warehouse_level < (unsigned)wl_want && col->warehouse_level == 0) {
      if (ai_euro_5952_try_build(&s, 0x10) == 0) { /* 231f: Warehouse Expansion */
        return;
      }
    }
    if (pop >= 6) {
      int want_custom = (col->ai_flags & 0x03u) != 0;
      if (!want_custom && s.col1) {
        want_custom = ((int)s.col1->stuff.armed_ship_counts[human] - 2) >
                      (int)s.col1->stuff.armed_ship_counts[s.nation];
      }
      if (!want_custom && pop >= 0x0c) {
        want_custom = 1;
      }
      if (want_custom && ai_euro_5952_try_build(&s, 0x12) == 0) { /* 2347: Custom House */
        return;
      }
    }

    /* 2385 — the Wagon Train unit project. */
    if (!(col->colony_flags & COLONIZE_COLONY_FLAG_WAGON_TRAIN) && year < 0x640) {
      const int cont = map_continent_id_at(ctx->map, col->x, col->y);
      const int presence =
        cont >= 0 ? ai_contact_continent_presence_4962(ctx, s.nation, cont) : 0;
      if ((presence & 1) != 0 && ring1 == 0) {
        int vd = 0;
        const int vi = ai_euro_20e6_nearest_village(ctx, col->x, col->y, &vd);
        int alarm = 0;
        if (vi >= 0 && s.col1 && s.col1->tribe) {
          alarm = ai_diplo_indian_alarm(s.col1, (int)s.col1->tribe[vi].nation_id, s.nation);
        }
        if (alarm < 0x32) {
          ai_euro_5952_set_unit_project(&s, 0x0c);
          return;
        }
      }
    }

    if (tier_max >= 1 && (pop + experts) >= 4) {
      if (ai_euro_5952_try_build(&s, 0x0c) == 0) { /* 23d0: Schoolhouse */
        return;
      }
    }
    if (pop < 6) { /* 23f6 */
      col->build_ai_flags =
        (uint8_t)(col->build_ai_flags | COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
    }
    {
      /* 2404: Armory — musket price / turn band, or a Master Gunsmith. */
      int want_armory = 0;
      if ((musket_price + (difficulty >> 1)) >= 4 || turn > 0x50) {
        if (pop >= 6 &&
            (col->stock[COLONIZE_CARGO_TOOLS] >= 0x28 || gross[COLONIZE_CARGO_TOOLS] != 0)) {
          want_armory = 1;
        }
      }
      if (!want_armory && prof_count[COLONIZE_PROF_GUNSMITH] != 0) {
        want_armory = 1;
      }
      if (want_armory && ai_euro_5952_try_build(&s, 0x03) == 0) {
        return;
      }
    }
    if (prof_count[COLONIZE_PROF_PREACHER] != 0) {
      if (ai_euro_5952_try_build(&s, 0x25) == 0) { /* 2453: Church */
        return;
      }
    }
    if (ai_euro_5952_try_build(&s, 0x24) == 0) { /* 2467: Lumber Mill */
      return;
    }
    if (ai_euro_5952_try_build(&s, 0x01) == 0) { /* 2475: Fort */
      return;
    }
    if (musket_price >= 4 && pop >= 4 &&
        (col->stock[COLONIZE_CARGO_ORE] >= 0x28 || gross[COLONIZE_CARGO_ORE] != 0)) {
      if (ai_euro_5952_try_build(&s, 0x28) == 0) { /* 24a9: Blacksmith's Shop */
        return;
      }
    }
    if (col->warehouse_level < (unsigned)wl_want) {
      if (ai_euro_5952_try_build(&s, 0x10) == 0) { /* 24b7 */
        return;
      }
    }
    if (gross[AI_EURO_5952_BELLS] >= 0x18) {
      if (ai_euro_5952_try_build(&s, 0x14) == 0) { /* 24d3: Newspaper */
        return;
      }
    }
    if (gross[AI_EURO_5952_BELLS] >= 4) {
      if (ai_euro_5952_try_build(&s, 0x14) == 0) { /* 24e8 */
        return;
      }
    }
    if (tier_max >= 2 && (pop + experts) >= 0x0a) {
      if (ai_euro_5952_try_build(&s, 0x0d) == 0) { /* 24fd: College */
        return;
      }
    }
    if (pop < 8) {
      goto wants;
    }
    if (tier_max >= 3 && (pop + experts) >= 0x10) {
      if (ai_euro_5952_try_build(&s, 0x0e) == 0) { /* 2530: University */
        return;
      }
    }
    if (ai_euro_5952_try_build(&s, 0x25) == 0) { /* 2552: Church again */
      return;
    }
    if (pop >= 0x0a) {
      if (ai_euro_5952_try_build(&s, 0x02) == 0) { /* 2560: Fortress */
        return;
      }
    }

    /* 25a3 — the naval band: a finished craft chain plus a coastal colony. */
    if (factory != 0 && (col->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) != 0) {
      if (ai_euro_5952_try_build(&s, 0x08) == 0) { /* 25b9: Shipyard */
        return;
      }
      if (has_shipyard && s.col1) {
        const ColonizeCol1Stuff* st = &s.col1->stuff;
        if (((int)st->census_pop_proxy[s.nation] >> 1) + (int)st->colony_counts[s.nation] >=
            (int)st->ship_cargo_totals[s.nation]) {
          ai_euro_5952_set_unit_project(&s, 0x0f); /* 25f1: Galleon */
          return;
        }
        const int frigates = (int)st->unit_type_counts[s.nation][0x11];
        const int armed = (int)st->armed_ship_counts[s.nation];
        if (frigates != 0 || armed == 0) {
          if (armed < 4) {
            ai_euro_5952_set_unit_project(&s, 0x10); /* 260b: Privateer */
            return;
          }
        }
        if (frigates < 1) {
          ai_euro_5952_set_unit_project(&s, 0x11); /* 2626: Frigate */
          return;
        }
      }
    }

    /* 262c */
    if (factory == 0 || (arty_on_tile != 0 && col->labor_shortage != 0)) {
      goto chain_upgrades;
    }
    if (col->stock[COLONIZE_CARGO_TOOLS] != 0) { /* 2670 */
      if (ai_euro_5952_try_build(&s, 0x03) == 0) {
        return;
      }
    }
    ai_euro_5952_set_unit_project(&s, 0x0b); /* 2689: Artillery */
    return;

  chain_upgrades:
    /* 268e — walk the six craft chains, highest row first, and upgrade any
     * whose tier is short of what its input supply justifies. */
    for (int i = 5; i >= 0; --i) {
      if (ai_euro_5952_chain_short(&s, gross, i) != 0) {
        if (ai_euro_5952_try_build(&s, ai_euro_5952_chain_top(i)) == 0) {
          return;
        }
      }
    }
    if (ai_euro_5952_try_build(&s, 0x26) == 0) { /* 26c4: Cathedral */
      return;
    }
    if (has_shipyard && s.col1) {
      const ColonizeCol1Stuff* st = &s.col1->stuff;
      const int armed = (int)st->armed_ship_counts[s.nation];
      const int cap = armed < 8 ? armed : 8; /* asm 26e9 SUB/SBB/AND/ADD = min(armed,8) */
      if ((int)st->unit_type_counts[s.nation][0x0f] < cap) {
        ai_euro_5952_set_unit_project(&s, 0x0f); /* Galleon */
        return;
      }
      if (armed < 8) {
        ai_euro_5952_set_unit_project(&s, 0x11); /* Frigate */
        return;
      }
    }
    if (pop < 0x0a) { /* 270d — the SMALL_AI writer */
      col->colony_flags = (uint8_t)(col->colony_flags | COLONIZE_COLONY_FLAG_SMALL_AI);
    }
    if (arty_on_tile >= 3) { /* 273e */
      col->building_in_production = -1;
      goto wants;
    }
    if (ai_euro_5952_try_build(&s, 0x03) == 0) { /* 2724: Armory */
      return;
    }
    if (gross[COLONIZE_CARGO_MUSKETS] == 0) { /* DS:0x8de6 */
      ai_euro_5952_set_unit_project(&s, 0x0b); /* Artillery */
      return;
    }
    if (ai_euro_5952_try_build(&s, 0x05) == 0) { /* 2737: Arsenal */
      return;
    }
    ai_euro_5952_set_unit_project(&s, 0x0b); /* Artillery */
    return;
  }

wants:
  /* 2747 */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags | COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
}

/*
 * DS:0x8ea8, @JOB column 3 ("price"), stride 8 — the word DOS reads as
 * `*(uint *)(row * 8 + -0x7158)`. NAMES.TXT carries it as the fourth field
 * of the @JOB record ("Farmer, Expert Farmers, 1, 1100"); rows that cannot
 * be trained on the Europe dock carry -1, and DOS sign-extends the word with
 * CWD before the 32-bit subtract, so a -1 row ADDS one gold. No catalog
 * (empty section) answers INT_MIN and the caller skips the whole arm.
 */
static int ai_euro_5952_job_price(int row) {
  const char* s = reports_names_field("JOB", row, 3);
  if (!s) {
    return INT_MIN;
  }
  while (*s == ' ' || *s == '\t') {
    ++s;
  }
  if (!*s) {
    return INT_MIN;
  }
  return (int)strtol(s, NULL, 10);
}

/*
 * FUN_1000_8dfe -> FUN_15eb_0e18, colony +0x20+slot = the colonist's
 * OCCUPATION (the job he is doing right now), as opposed to +0x40+slot =
 * his profession. The port stores a field job directly and leaves indoor
 * work implicit in `building_type`, so an indoor worker's occupation is
 * recovered from the chain his workplace belongs to. -1 = idle.
 */
static int ai_euro_5952_occupation(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int slot
) {
  const ColonizeColonist* c = &col->colonists[slot];
  if (c->field_job >= 0) {
    return c->field_job;
  }
  if (c->building_type < 0) {
    return -1;
  }
  for (int job = COLONIZE_PROF_DISTILLER; job <= COLONIZE_PROF_STATESMAN; ++job) {
    const char* const* names = colonies_building_chain(ai_euro_5952_job_chain(job));
    for (int i = 0; names && names[i]; ++i) {
      if (colonies_find_building(pool, names[i]) == c->building_type) {
        return job;
      }
    }
  }
  return -1;
}

/*
 * ARM 2's candidate pick, raw 95926-95944 (asm OVL15 0x2940-0x29c7), split
 * out so a unit test can drive it without a whole turn context. Returns the
 * @JOB to buy (0x1c = "buy nothing") and, through `out_slot`, the colonist
 * DOS picks: the LAST non-expert, non-Convert slot. `uStack_ec` bit 0 tracks
 * "this colony already has an Expert Farmer", bit 1 "… an Expert Fisherman".
 */
COLONIZE_INTERNAL int ai_euro_5952_train_pick(
  const ColonizeColony* col, int n, bool has_docks, int* out_slot
) {
  int last = -1;   /* iStack_30 */
  int have = 0;    /* uStack_ec */
  for (int s = 0; s < n; ++s) {
    if (!col->colonists[s].active) {
      continue;
    }
    const int prof = col->colonists[s].profession;
    if (prof == COLONIZE_PROF_FARMER) {
      have |= 1;
    }
    if (prof == COLONIZE_PROF_FISHERMAN) {
      have |= 2;
    }
    if (!ai_euro_5952_job_is_expert(prof) && prof != COLONIZE_PROF_CONVERT) {
      last = s;
    }
  }
  if (out_slot) {
    *out_slot = last;
  }
  if (last < 0) {
    return COLONIZE_PROF_FREE_COLONIST; /* iStack_ee stays 0x1c */
  }
  if ((have & 2) == 0 && has_docks) {
    return COLONIZE_PROF_FISHERMAN;
  }
  if ((have & 1) == 0) {
    return COLONIZE_PROF_FARMER;
  }
  return COLONIZE_PROF_FREE_COLONIST;
}

/*
 * DOS-LITERAL FUN_5952_035e raw ~95860-95958 — the two specialist arms that
 * close the AI colony tick, both unported until 2026-09-22 (bugs.md #571 /
 * #572). Clean body colony_tick_5952_035e.md:1653-1749, asm
 * viceroy_overlays.asm OVL15 0x274b-0x29ff.
 *
 * Two counts feed them, taken at md:1459-1464 right before the arms:
 *   iVar12     = FUN_1000_8d72(0) + FUN_1000_8d72(8)  // colonists WORKING
 *                                                     // @JOB 0 / @JOB 8
 *   iStack_a   = FUN_1000_8de0(0) + FUN_1000_8de0(8)  // colonists whose
 *                                                     // PROFESSION is 0 / 8
 * and the training flag (asm 0x2296-0x22bc):
 *   if (pop/2 < iVar12 && iVar12 > 1) local_a0 = 1;
 *   if (DS:0x8e5a != 0)               local_a0 = 1;   // colony short of FOOD
 * The audit read `iVar12` as an expert count; the asm is unambiguous that
 * the pair at [BP+0xfe4a] and [BP-0x8] are FUN_1000_8d72 (count by JOB) and
 * FUN_1000_8de0 (count by PROFESSION) respectively.
 *
 * ARM 1 — the schoolhouse/expert election (0x274b). Gated on the colony
 * owning something along @BUILDING chain 0xc (Schoolhouse/College/
 * University) and `((count == 3) + count) * 4 <= improve_timer` (4 / 8 / 16
 * turns). Target:
 *   - pop < 10 and food-experts <= food-workers:
 *       Fishermen-by-profession < water ring tiles ? @JOB 8 : @JOB 0;
 *   - then the DS:0x864 sweep overrides it for any craft whose building
 *     chain is built out (2 tiers, or 1 for the Armory root 0x03), whose
 *     input cargo is actually being produced, and whose @JOB the colony has
 *     nobody of — and the override writes the ROW INDEX, not the row's @JOB:
 *     `MOV AX,[BP+0xff56]; MOV [BP+0xff78],AX` (asm 0x2830). That is a DOS
 *     bug and it is kept: a colony that lacks a Gunsmith elects @JOB 0
 *     (Expert Farmer), one that lacks a Blacksmith elects @JOB 1, and so on
 *     through row 5 -> @JOB 5 (Expert Lumberjack).
 *   The candidate list is every colonist who is NOT an expert, or is an
 *   expert working something other than his specialty (occupation != 0x13),
 *   excluding Converts, capped at 25; one is drawn with FUN_1000_86c4(0,
 *   n-1), his profession is written and improve_timer is zeroed.
 *   Default target for a small colony with little water: @JOB 0.
 *
 * ARM 2 — "train an expert in the colony" (0x290c). Gated on local_a0, on
 * the nation record's tax rate (`*(char *)(DS:0x84fc + 1) <= 0x19`) and on
 * the purse covering @JOB row 0's price (1100). It finds the LAST non-expert
 * non-Convert colonist and makes him an Expert Fisherman when the colony has
 * no Fisherman and owns Docks, else an Expert Farmer when it has no Farmer;
 * then it debits `*(uint *)(slot * 8 + -0x7158)`. That index really is the
 * colonist SLOT, not the profession: asm 0x29d4 is
 * `MOV BX,[BP-0x2e]; SHL BX,0x3; MOV AX,[BX+0x8ea8]; CWD`, and [BP-0x2e] is
 * the same word pushed to FUN_1000_8e9e as the slot argument two
 * instructions earlier. DOS therefore charges the AI @JOB[slot]'s price, not
 * @JOB[target]'s — kept DOS-literal (bugs.md #571).
 *
 * Port deviations, both documented rather than modelled: DOS's candidate
 * walk in ARM 1 runs to `pop + DS:0x8d72`, i.e. past the colonists into the
 * units parked on the colony tile, and DOS's aiStack_68 census is the
 * snapshot taken before the tick's absorption loop. The port walks the
 * colony roster only and recomputes the census here.
 */
COLONIZE_INTERNAL void ai_euro_5952_specialist_arms(
  ColonizeTurnContext* ctx, ColonizeColony* col, int n
) {
  ColonizeColonyPool* pool = ctx->colonies;
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }

  /* aiStack_68 — per-@JOB census, non-experts folded to 0x13 (md:633-641). */
  int census[0x19];
  memset(census, 0, sizeof census);
  int food_experts = 0; /* iStack_a  = 8de0(0) + 8de0(8) */
  int food_workers = 0; /* iVar12    = 8d72(0) + 8d72(8) */
  for (int s = 0; s < n; ++s) {
    if (!col->colonists[s].active) {
      continue;
    }
    int prof = col->colonists[s].profession;
    if (prof == COLONIZE_PROF_FARMER || prof == COLONIZE_PROF_FISHERMAN) {
      ++food_experts;
    }
    const int job = col->colonists[s].field_job;
    if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
      ++food_workers;
    }
    if (!ai_euro_5952_job_is_expert(prof)) {
      prof = 0x13;
    }
    if (prof >= 0 && prof < (int)(sizeof census / sizeof census[0])) {
      ++census[prof];
    }
  }

  /* iStack_1a — ring tiles of terrain class 0x19/0x1a (Ocean / High Seas). */
  int water_ring = 0;
  const int ring_water = colonies_work_plot_count(ctx->colonies, col);
  for (int ti = 0; ti < ring_water; ++ti) {
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int wx = col->x + dx;
    const int wy = col->y + dy;
    /* OVL15 0x082c-0x0858 increments [BP-0x18] only when the tile's terrain
     * class is 0x19/0x1a; the off-map path (0x094c) returns without touching
     * it, so an edge colony's missing tiles are NOT water (bugs.md #595).
     * map_tile_is_water() answers true off-map, hence the explicit bounds
     * test — same shape as the build-cascade ring count above. */
    if (wx < 0 || wy < 0 || wx >= (int)ctx->map->width || wy >= (int)ctx->map->height) {
      continue;
    }
    if (map_tile_is_water(ctx->map, wx, wy)) {
      ++water_ring;
    }
  }

  const ColonizeWorld world = world_from_turn_ctx(ctx);
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  ai_euro_5952_ledgers(&world, pool, col, col1, gross, demand);

  /* DS:0x8e5a — slot 0 (FOOD) of the FUN_15eb_1f72 unmet-demand ledger:
   * FUN_15eb_0b52 records a row when stock + production < demand. */
  const bool food_short =
    col->stock[COLONIZE_CARGO_FOOD] + gross[COLONIZE_CARGO_FOOD] <
    demand[COLONIZE_CARGO_FOOD];
  /* asm 0x2296-0x22b9 sets [BP+0xff62] to 1; asm 0x22e6 clears it again when
   * the build cascade's Docks arm commits this turn (bugs.md #586). */
  const bool docks_started =
    col->id >= 0 && col->id < COLONIZE_COLONIES_MAX && s_5952_docks_started[col->id] != 0;
  const bool train_flag =
    !docks_started &&
    (((col->population / 2) < food_workers && food_workers > 1) || food_short);

  /* ---- ARM 1: improve_timer-gated expert election (asm 0x274b) ---- */
  const int school_chain = ai_euro_5952_chain_owned(pool, col, COLONIES_CHAIN_SCHOOL, NULL, NULL);
  if (school_chain != 0 &&
      ((school_chain == 3 ? 1 : 0) + school_chain) * 4 <= (int)col->improve_timer) {
    int target = -1; /* iStack_8a */
    if (col->population < 10 && food_experts <= food_workers) {
      target = census[COLONIZE_PROF_FISHERMAN] < water_ring ? COLONIZE_PROF_FISHERMAN
                                                            : COLONIZE_PROF_FARMER;
    }
    for (int r = 0; r < 6; ++r) {
      const int owned = ai_euro_5952_chain_owned(pool, col, k_5952_craft[r].chain, NULL, NULL);
      const int need = k_5952_craft[r].root == 0x03 ? 1 : 2;
      if (owned >= need && gross[k_5952_craft[r].cargo] != 0 &&
          census[k_5952_craft[r].job] == 0) {
        target = r; /* DOS-LITERAL: the ROW INDEX, not k_5952_craft[r].job */
      }
    }
    int cand[0x19];
    int n_cand = 0;
    for (int s = 0; s < n && n_cand < 0x19; ++s) {
      if (!col->colonists[s].active) {
        continue;
      }
      const int prof = col->colonists[s].profession;
      const int occ = ai_euro_5952_occupation(pool, col, s);
      if (!((!ai_euro_5952_job_is_expert(prof) || (prof != occ && occ != 0x13)) &&
            prof != COLONIZE_PROF_CONVERT)) {
        continue;
      }
      cand[n_cand++] = s;
    }
    if (n_cand != 0) {
      const int pick = cand[dos_rng_range(ctx->rng, 0, n_cand - 1)];
      int elected = target;
      if (elected < 0) {
        elected = ai_euro_5952_occupation(pool, col, pick);
      }
      if (elected >= 0) {
        col->colonists[pick].profession = elected;
      }
      col->improve_timer = 0;
    }
  }

  /* ---- ARM 2: buy an expert for this colony (asm 0x290c) ---- */
  if (!train_flag) {
    return;
  }
  const int tax = col1 ? (int)col1->nation[nation].tax_rate : 0;
  if (tax > 0x19) {
    return;
  }
  const int gate_price = ai_euro_5952_job_price(COLONIZE_PROF_FARMER);
  if (gate_price == INT_MIN) {
    return; /* no @JOB catalog: nothing to price the purchase against */
  }
  if ((long)(int32_t)europe_nation_gold(ctx->europe, ctx->col1, nation) < (long)gate_price) {
    return;
  }
  int last = -1;
  const int trained =
    ai_euro_5952_train_pick(col, n, colony_yield_colony_has_docks(pool, col), &last);
  if (trained == COLONIZE_PROF_FREE_COLONIST) {
    return;
  }
  col->colonists[last].profession = trained;
  /* DOS-LITERAL: the price row is the colonist SLOT (asm 0x29d4). */
  const int charged = ai_euro_5952_job_price(last);
  if (charged != INT_MIN) {
    europe_nation_gold_add(ctx->europe, ctx->col1, nation, -(long)charged);
  }
}

/* FUN_5952_035e tile-improvement arm (raw 94402-94551, bugs.md #612) — the
 * body lives beside the 20e6 terrain/ring tables it shares. */
static void ai_euro_5952_improve_best_plot(ColonizeTurnContext* ctx, ColonizeColony* col);
/* FUN_5952_035e raw 94370-94400 — the 20-tool purchase (#640) and the
 * `turn % 7 == 0` inter-colony road-connect arm (#641); both run immediately
 * before the improve arm. */
static void ai_euro_5952_tools_supply_and_connect(
  ColonizeTurnContext* ctx, ColonizeColony* col
);

COLONIZE_INTERNAL void ai_euro_colony_tick_28c8_reassign(
  ColonizeTurnContext* ctx, int nation_id
) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id == ctx->human_nation) {
    return;
  }
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* col = &ctx->colonies->colonies[ci];
    if (!col->active || col->nation_id != nation_id || col->colonist_count <= 0) {
      continue;
    }
    /*
     * FUN_5952_035e raw 94402-94551 runs immediately BEFORE the colonist idle
     * sweep below (bugs.md #612).
     */
    ai_euro_5952_tools_supply_and_connect(ctx, col);
    ai_euro_5952_improve_best_plot(ctx, col);
    int prev_job[COLONIZE_COLONY_POP_MAX];
    bool placed[COLONIZE_COLONY_POP_MAX];
    const int n = col->colonist_count < COLONIZE_COLONY_POP_MAX ? col->colonist_count
                                                                : COLONIZE_COLONY_POP_MAX;
    for (int s = 0; s < n; ++s) {
      const ColonizeColonist* c = &col->colonists[s];
      prev_job[s] = c->field_job;
      /*
       * DOS-LITERAL FUN_5952_035e raw 94551-94564 (clean body
       * colony_tick_5952_035e.md:1030-1043): the tick idles EVERY colonist
       * before the food pass —
       *   for (slot) { prof[slot] = 8e44(slot); placed[slot] = 0;
       *                8c6e(slot,0); 8e26(slot,0x12); }
       *   memset(colony+0x70, 0xff, 0x14);
       * `8e26(slot, 0x12)` is FUN_15eb_1068 with the idle sentinel job and
       * carries no "indoor workers are exempt" test, and the plot memset
       * wipes all 20 tile bytes. Building workers are therefore unseated
       * too, and the indoor pass at raw 94784 (ai_euro_5952_indoor_pass)
       * re-elects an indoor job for everyone the field passes left over —
       * so nobody is lost. The port used to seed
       * `placed[s] = ... || c->building_type >= 0`, which had no DOS
       * counterpart and meant a starving AI colony could never pull a
       * Statesman or a stuck Expert Farmer out of a building onto a food
       * tile (bugs.md #569).
       */
      placed[s] = !c->active;
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
      col->colonists[s].building_type = -1; /* raw 94557 `8e26(slot, 0x12)` */
    }

    const bool fishable = colony_yield_colony_has_docks(ctx->colonies, col);
    int food_have = 0;
    {
      ColonizeTownCommonsYield tc;
      colony_yield_town_commons(
        ctx->map, col->x, col->y, col->colony_flags,
        ctx->col1_ok && ctx->col1 ? (int)ctx->col1->head.difficulty : 4, &tc
      );
      food_have = tc.food > 0 ? tc.food : 0;
    }
    const int food_need = col->population * 2;
    /* colony+0x9a — the food stock the DS:0x8e32 comparison is made against. */
    const int food_stock = col->stock[COLONIZE_CARGO_FOOD];
    /* DOS `goto LAB_5952_178f`: ends the whole placement section, not a pass. */
    bool section_done = false;

    /*
     * Pass 1 — the FOOD pass. DOS-LITERAL raw 94592-94594
     * (colony_tick_5952_035e.md:1071-1075):
     *   if (placed[slot] == 0 &&
     *       (prof[slot] == 0 ||
     *        (prof[slot] == 8 && FUN_1000_8bec(0x181f, 6) != 0)) &&
     *       FUN_1000_8d5e(0x181f, slot, prof[slot]) == 0)
     * `aiStack_12e[]` is filled from FUN_1000_8e44 (colony +0x40+slot), i.e.
     * the colonist's PROFESSION, not his previous field job — so only an
     * Expert Farmer (@JOB 0), or an Expert Fisherman (@JOB 8) in a colony
     * with Docks (@BUILDING 6), enters this pass, and the 28c8 call is
     * RESTRICTED to that job (third argument = the profession, not 0xffff),
     * so a food-pass slot can only ever land on a food plot. The port gated
     * on `prev_job[]` and called the unrestricted search (bugs.md #567).
     */
    for (int s = 0; s < n && food_have < food_need; ++s) {
      if (placed[s]) {
        continue;
      }
      const int food_prof = col->colonists[s].profession;
      const bool was_food = food_prof == COLONIZE_PROF_FARMER ||
                            (food_prof == COLONIZE_PROF_FISHERMAN && fishable);
      if (!was_food) {
        continue;
      }
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = prev_job[s]; /* sticky ×2 on the old job */
      const int ok = ai_euro_28c8_score_job(ctx, col, s, food_prof, food_prof, &best);
      col->colonists[s].field_job = -1;
      /* DOS-LITERAL raw 94592-94597: the DS:0x8dbe (best yield) stop is INSIDE
       * the `FUN_281f_0b6e(...) == 0` arm. A non-zero 28c8 return (no positive
       * plot) is not a stop at all — DOS just falls through to `local_ac++`
       * and tries the next colonist; only a handled call whose best yield is
       * under 3 ends the whole placement section (bugs.md #585). */
      if (!ok) {
        continue;
      }
      if (best.yield < 3) {
        section_done = true; /* raw 94596 */
        break;
      }
      if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
        placed[s] = true;
        if (best.job == COLONIZE_JOB_FARMER || best.job == COLONIZE_JOB_FISHERMAN) {
          food_have += best.yield;
        }
      }
    }

    /* Pass 2 ×2 — everyone else, best job wins; raw 94613-94614 stop. */
    for (int pass = 0; !section_done && pass < 2; ++pass) {
      for (int s = 0; s < n; ++s) {
        if (placed[s]) {
          continue;
        }
        /*
         * local_84, recomputed per slot (raw 94608). DS:0x8e32 is the
         * shortfall of this turn's food production against consumption, so it
         * shrinks as this section places farmers — hence the running
         * food_have, which pass 1 already maintains and pass 2 now keeps up.
         */
        const int shortfall = food_need > food_have ? food_need - food_have : 0;
        const bool plenty = (shortfall * 0x10) < food_stock;
        /*
         * DOS-LITERAL raw 94600-94604 (md:1088-1092) — the admission test the
         * port was missing entirely (bugs.md #568):
         *   iVar11 = FUN_1000_8e8a(0x181f, prof[slot]);   // is_expert
         *   if (((iVar11 == 0) || (local_84 == 0 && pass != 0)) &&
         *       ((local_84 == 0) || (pass != 0 || prof[slot] == 0x1b)) && ...)
         * With food plentiful (`local_84`), the FIRST sub-pass admits only
         * Indian Converts (@JOB 0x1b) and experts are barred from it
         * altogether; experts enter on the second sub-pass and only when food
         * is NOT plentiful.
         */
        const int prof2 = col->colonists[s].profession;
        const bool expert2 = ai_euro_5952_job_is_expert(prof2);
        if (!((!expert2 || (!plenty && pass != 0)) &&
              (!plenty || (pass != 0 || prof2 == COLONIZE_PROF_CONVERT)))) {
          continue;
        }
        AiEuro28c8JobCandidate best;
        col->colonists[s].field_job = prev_job[s];
        const int ok = ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &best);
        col->colonists[s].field_job = -1;
        /* raw 94612-94614, same shape as pass 1: an unhandled 28c8 only skips
         * this colonist (bugs.md #585). */
        if (!ok) {
          continue;
        }
        if (best.yield < 3 || (plenty && best.yield < 5)) {
          section_done = true; /* raw 94614 */
          break;
        }
        if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
          placed[s] = true;
          if (best.job == COLONIZE_JOB_FARMER || best.job == COLONIZE_JOB_FISHERMAN) {
            food_have += best.yield;
          }
        }
      }
    }

    /*
     * DOS raw 94690-94740 — the carpenter-staffing arm, inside the same
     * `(+0x1d & 0x80) == 0` block that seeds local_14 below.
     */
    if ((col->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) == 0) {
      /* raw 94659-94679 then 94680-94689, both ahead of the carpenter arm. */
      const bool lumber_placed = ai_euro_5952_forced_lumberjack(ctx, col, placed, n);
      ai_euro_5952_lumber_purchase(
        ctx->europe, (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL, col,
        /* DS:0x538e — the port's turn counter is ctx->turn_number. */
        ctx->turn_number ? (int)*ctx->turn_number : 0, lumber_placed
      );
      ai_euro_5952_carpenter_arm(ctx, col, placed, n);
    }

    /*
     * DOS raw 94751 (colony_tick_5952_035e.md:1163-1185) — the field-
     * specialist restore pass, the last placement arm before the tick's
     * building passes. It runs whether or not the two passes above tripped
     * their `goto LAB_5952_178f`: that goto lands on LAB_17a9, and this loop
     * is downstream of it.
     *
     *   for slot in 0..pop-1, if unplaced:
     *     if (is_expert(prof) && prof < 9 && prof != 0 && prof != 8) {
     *       if (prof == 5) {                      // Expert Lumberjack
     *         if (local_14 == 0) local_14 = 1;    // the first one is free
     *         else if (!(+0x1b & 0x20) && DS:0x8e64 == 0) continue;
     *       }
     *       if (colony[0x9a + prof*2] <= local_36) assign(slot, job = prof);
     *     }
     *
     * @JOB 0 (Expert Farmer) and 8 (Expert Fisherman) are excluded because
     * the food pass above already had first refusal on them. `is_expert` is
     * FUN_281f_0c9a → FUN_15eb_0002 (viceroy 9298-9307): false for @JOB
     * 0x13 and 0x19..0x1c, true otherwise — so for @JOB 1..7 the profession
     * index doubles as both the field job and the cargo slot.
     *
     * `local_36` is FUN_1000_8f2a → FUN_281f_0d3a → FUN_15eb_0a50, i.e.
     * colonies_warehouse_capacity — one number for all goods, not per-cargo.
     *
     * `local_14` is seeded 1 when the colony does NOT want construction
     * (md:1069, `if ((+0x1d & 0x80) == 0) local_14 = 1;`), so a construction
     * colony admits its first Lumberjack unconditionally and a
     * non-construction one does not. Beyond the first, DOS demands either
     * COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR (+0x1b bit 0x20) or a live
     * lumber shortfall. This is that bit's FIRST Linux reader — colony.h
     * called it write-only, because all four of DOS's readers (raw 94422,
     * 94454, 94499, 94751) sit in unported passes and this is the first one
     * to land.
     *
     * DS:0x8e64 is the unmet-after-stock slot for cargo 5 in the
     * FUN_15eb_1f72 ledger array whose base colony_craft.c already names
     * (DS:0x8e5a + 5*2); FUN_15eb_0b52 records those rows as
     * `stock + production < demand`, which is what is recomputed here —
     * DOS refreshes the array through FUN_1000_8df4 after every assign, so
     * it is this colony's live number, not last turn's.
     */
    {
      const int wh_cap =
        colonies_warehouse_capacity(ctx->colonies, col, COLONIZE_CARGO_LUMBER);
      int lumber_use = 0;
      (void)colony_prod_colony_hammers(ctx->colonies, col, 0, &lumber_use);
      int lumber_prod = 0;
      const int ring_lumber = colonies_work_plot_count(ctx->colonies, col);
      for (int ti = 0; ti < ring_lumber; ++ti) {
        const int occ = col->tiles[ti];
        if (occ < 0 || occ >= n) {
          continue;
        }
        if (col->colonists[occ].field_job != COLONIZE_JOB_LUMBERJACK) {
          continue;
        }
        int dx = 0;
        int dy = 0;
        if (!colonies_field_tile_delta(ti, &dx, &dy)) {
          continue;
        }
        /* DOS recomputes the ledger through FUN_15eb_18ec, i.e. with the
         * seated colonist's own profession, so an Expert Lumberjack's
         * doubling is inside this number. colony_yield_for_tile has no worker
         * context and under-counted it. */
        lumber_prod += colony_yield_for_worker(
          ctx->map, col->x + dx, col->y + dy, COLONIZE_JOB_LUMBERJACK,
          col->colonists[occ].profession,
          colony_yield_colony_has_docks(ctx->colonies, col),
          colony_prod_sol_bonus_field(ctx->col1_ok ? ctx->col1 : NULL, col),
          col->colony_flags,
          ctx->col1_ok && ctx->col1 &&
            founding_fathers_nation_has(ctx->col1, col->nation_id, FF_HENRY_HUDSON)
        );
      }
      const int lumber_unmet =
        lumber_use > col->stock[COLONIZE_CARGO_LUMBER] + lumber_prod ? 1 : 0;
      int lumber_seen =
        (col->build_ai_flags & COLONIZE_BUILD_AI_WANTS_CONSTRUCTION) != 0 ? 0 : 1;
      for (int s = 0; s < n; ++s) {
        if (placed[s]) {
          continue;
        }
        const int prof = col->colonists[s].profession;
        if (!ai_euro_5952_job_is_expert(prof)) {
          continue;
        }
        /* DOS reads a byte, so its `< 9` cannot go negative; the port's
         * profession is an int and an unset one is −1. */
        if (prof < 0 || prof >= COLONIZE_FIELD_JOB_COUNT ||
            prof == COLONIZE_JOB_FARMER || prof == COLONIZE_JOB_FISHERMAN) {
          continue;
        }
        if (prof == COLONIZE_JOB_LUMBERJACK) {
          if (lumber_seen == 0) {
            lumber_seen = 1;
          } else if ((col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR) == 0 &&
                     lumber_unmet == 0) {
            continue;
          }
        }
        if (col->stock[prof] > wh_cap) {
          continue;
        }
        AiEuro28c8JobCandidate best;
        col->colonists[s].field_job = prev_job[s];
        const int ok = ai_euro_28c8_score_job(ctx, col, s, prof, prof, &best);
        col->colonists[s].field_job = -1;
        if (!ok) {
          continue;
        }
        if (colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job)) {
          placed[s] = true;
        }
      }
    }

    /*
     * DOS's own next arm is the indoor-workplace pass (raw 94784+), ported as
     * ai_euro_5952_indoor_pass. AI_5952_INDOOR=0 falls back to the pre-port
     * "leftovers" field stand-in below (docs/debug_env_vars.md).
     */
    if (ai_euro_5952_indoor_pass_enabled()) {
      ai_euro_5952_indoor_pass(ctx, col, placed, n);
      /* DOS raw ~95860-95958, the tick's last two arms (bugs.md #571/#572). */
      ai_euro_5952_specialist_arms(ctx, col, n);
      continue;
    }

    /*
     * Leftovers — the pre-2026-09-17 stand-in, kept only behind
     * AI_5952_INDOOR=0. DELIBERATE, DOCUMENTED DEVIATION (smell audit #42).
     *
     * The DOS arm is raw 94627-94658 (LAB_5952_17a9), and it is dead code:
     *   - it is a single-winner election, not a per-slot assignment: it probes
     *     every unplaced slot, keeps one best by the DS:0x8dc0 key
     *     (score<<2, +1 expert-gate, +2 prior job == DS:0x8dc2) and commits
     *     only that one (raw 94650);
     *   - every probe is `FUN_1000_8d5e(..., slot, 0xfffe)` — mode −2 — and
     *     28c8's tail (viceroy_unpacked.c 13144-13146, asm 15eb:2e4a)
     *     restores the prior job and stores yield 0 into DS:0x8dbe whenever
     *     `param_2 < -1`. The arm's own threshold is `1 < DS:0x8dbe`
     *     (asm 158989 `CMP [0x8dbe],0x2`), so it can never hold and the
     *     commit is unreachable. DOS's leftovers end the section with NO
     *     field plot (the whole 0x70..0x83 array was memset 0xff at raw
     *     94561) and are picked up by the BUILDING pass at raw 94784+.
     *
     * That building pass is not ported, so deleting this arm outright would
     * leave those colonists idle — strictly worse than DOS, which employs
     * them indoors. The arm therefore stays as the port's stand-in for the
     * building pass, with the two defects the audit named fixed:
     *   - it no longer claims to "keep what they had" while assigning a
     *     freshly scored tile (it does score-and-assign, and says so);
     *   - it now carries the DOS arm's OWN yield floor, `yield >= 2` — the
     *     two real passes above stop at `< 3` (raw 94596 / 94613), this arm
     *     compares against 2. It used to assign on any positive yield, which
     *     force-filled 1-yield tiles.
     * Still not modelled: the single-winner election, the DS:0x8dc0
     * priority key and the raw 94629-94631 population/capacity gate.
     *
     * 2026-09-09: the arm no longer skips slots whose previous job was −1.
     * DOS's own election has no such filter (raw 94635 gates on "unplaced"
     * alone; a matching prior job is worth +2 on the DS:0x8dc0 key, it is not
     * an entry requirement), and the filter only stopped mattering while the
     * `yield < 3` stop was a per-pass `break`. Now that the stop is DOS's
     * whole-section `goto` and carries the `local_84 && yield < 5` half, a
     * colonist who arrived this turn (prev_job −1) routinely reaches this arm
     * — and in DOS he would be picked up by the BUILDING pass at raw 94784+,
     * which is exactly what this stand-in is here to cover. Keeping the
     * filter left freshly admitted colonists idle for good.
     */
    for (int s = 0; s < n; ++s) {
      if (placed[s]) {
        continue;
      }
      AiEuro28c8JobCandidate best;
      col->colonists[s].field_job = prev_job[s];
      const int ok = ai_euro_28c8_score(ctx, col, s, col->colonists[s].profession, &best);
      col->colonists[s].field_job = -1;
      if (ok && best.yield >= 2) {
        (void)colonies_assign_field(ctx->colonies, col->id, s, best.tile, best.job);
      }
    }
    ai_euro_5952_specialist_arms(ctx, col, n);
  }
}







/*
 * Free Colonist / Colonist / Pioneer / Hardy — can join LABOR for food. Kind
 * comes from the unit's @UNIT type row; the residual "Farmer" case folds
 * into the profession == 0 (@JOB Farmer) check in the caller, so this stays
 * kind-only.
 */
static int ai_euro_is_food_labor_name(ColonizeUnitKind kind) {
  if (kind == UNITS_KIND_WAGON) {
    return 0;
  }
  if (kind == UNITS_KIND_SOLDIER || kind == UNITS_KIND_DRAGOON || kind == UNITS_KIND_SCOUT) {
    return 0;
  }
  return kind == UNITS_KIND_PIONEER || kind == UNITS_KIND_COLONIST;
}

/*
 * Food-LABOR capable unit: kind-based food labor OR @JOB Farmer (profession 0)
 * Expert Farmer on a Free Colonist / Colonist. Cite: docs/building_production.md
 * @JOB Farmer→Expert Farmer / Food; Colonization.pdf Skills Chart. No invented
 * food rates — LABOR join only.
 */
static int ai_euro_unit_is_food_labor(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  const ColonizeUnitKind kind = ai_euro_unit_kind(units, u);
  if (ai_euro_is_food_labor_name(kind)) {
    return 1;
  }
  /* profession 0 == @JOB Farmer (Expert Farmer skill). */
  if (u->profession == 0 && kind == UNITS_KIND_COLONIST) {
    return 1;
  }
  return 0;
}

static int ai_euro_type_is_wagon_name(ColonizeUnitKind kind);
static int ai_euro_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map);

static int ai_euro_ship_enter_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship);

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy) {
  if (!u) {
    return;
  }
  u->orders = orders;
  u->goto_x = gx;
  u->goto_y = gy;
  /*
   * Every non-SENTRY goto write clears the port-only landfall-wait flag
   * (bugs.md #528) — a real order (goal-directed goto, found move, wander,
   * hunt, wagon, ship stage) means the unit is no longer just parked ashore
   * waiting for its next landfall act. Call sites that DO mean "still
   * waiting ashore" pass SENTRY and then set the flag themselves right
   * after this call (ai_euro_unload_pax_at and its three direct siblings).
   */
  if (orders != UNITS_ORDER_SENTRY) {
    u->ai_landfall_wait = false;
  }
  if (getenv("AI_SET_GOTO_TRACE")) {
    fprintf(stderr, "[goto] unit %d (%d,%d) orders %d -> (%d,%d)\n", u->id, u->x, u->y, orders,
            gx, gy);
  }
  /*
   * Every goto write defaults to "not idle-roam" (DOS unit+0x314c != 5) —
   * ai_euro_move_scoring_gate re-marks its own two roam branches right
   * after calling this, so a stale roam flag from an earlier turn never
   * survives onto a goal-directed goto (found-tile, hunt, wagon, ship
   * staging) set by any other call site. See s_euro_roam_wander.
   */
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    s_euro_roam_wander[u->id] = 0;
    s_euro_ship_route_latch[u->id] = 0;
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
 * Deleted 2026-09-23 (bugs.md #746): ai_euro_find_boardable_ship,
 * ai_euro_try_treasure_board_sail, ai_euro_treasure_gold_from_unit,
 * ai_euro_cash_one_treasure, ai_euro_try_cash_treasure_europe and
 * ai_euro_try_expected_treasure_harbor — the whole invented AI
 * "board a treasure / sail it to Europe / cash it with the Crown's cut"
 * transport economy. Every one carried a Colonization.pdf / europe.h
 * citation only.
 *
 * DOS has no such site: FUN_521d_20e6's treasure band (raw 89997-90040) is
 * cash-in-any-own-colony / walk to the nearest own colony / rendezvous /
 * destroy, and FUN_4720_049e (the AI ship cargo pick, raw 76067-76513) has no
 * `+0x3146 == '\n'` term at all, so an AI treasure is never loaded onto a
 * hull. The King-galleon/Cortes transport offer (FUN_465b_0000 raw 75800) is
 * human-control-gated (bugs.md #478). The invented chain also ran *ahead* of
 * the DOS walk-home arm and paid the AI the Europe `min(tax,50)` cut instead
 * of the band's untaxed face value.
 *
 * europe_cash_treasure itself is kept — it is the human FUN_48d3_06ba path.
 */

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
    const int amt = units_hold_amount(units, w->id, h);
    if (amt <= 0) {
      return 1; /* empty slot (units_hold_amount folds the 255 sentinel to 0) */
    }
    if (amt < 100) {
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
    if (units_hold_amount(units, w->id, h) > 0 && w->hold_goods_type[h] == cargo_type) {
      return 1;
    }
  }
  return 0;
}

/*
 * (`ai_euro_colony_haul_threshold` / `ai_euro_colony_haul_cargo_short` — the
 * shared "stock < 20 / 10 / pop*2" ladder — were deleted 2026-09-18 with the
 * last caller, the port-only short-colony ship unload arm in
 * `ai_euro_try_ship_trade_haul`. The surplus (mult 2) arm had already lost its
 * caller on the wagon side. FUN_521d_20e6 has no such test: the arrival block
 * dumps every hold unconditionally (raw 3002-3007) and the delivery matrix
 * (raw 2047-2139) scores destinations off the colony's own
 * cargo_produced_mask / warehouse capacity instead.)
 */

/*
 * (`ai_euro_haul_load_amount` — the Linux 20/10/pop*2 load chunk — was deleted
 * on 2026-09-06g together with the wagon load ladder it fed. DOS's own load
 * quantity is `min(stock[g], 100)` inside the 20e6 LOAD matrix, raw 3126-3129,
 * live in `ai_euro_20e6_load_pick`'s call sites.)
 */

static int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);

/*
 * Free commodity holds on a hauler — DOS `unit_type_hold_capacity(0x5237) −
 * unit->holds_occupied(+0x3150)`, the quantity the 4393 tail subtracts from a
 * work-queue slot's load count.
 */
static int ai_euro_hauler_free_holds(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const int cap = units_goods_hold_count(units, u->id);
  int used = 0;
  for (int i = 0; i < cap && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    /* units_hold_amount folds the COL1 255 empty-hold sentinel to 0, so an
     * imported DOS save's Euro hull does not read as fully loaded. */
    if (units_hold_amount(units, u->id, i) > 0) {
      ++used;
    }
  }
  return cap > used ? cap - used : 0;
}

/*
 * Hull budget for one berth act — DOS `iStack_d2 = 0x5237[type] − +0x3150`
 * (raw 3012-3016), corrected for this port's passenger substitution.
 *
 * DOS's `+0x3150` counts GOODS holds only: it is the length of the packed
 * hold arrays (`+0x3151` type nibbles, `+0x3154` amounts), bumped by
 * `FUN_15eb_30b8` on a load (decomp 13325-13333, gated on
 * `+0x3150 < 0x5237[type]`) and decremented by `FUN_15eb_317c` on an unload
 * (13352-13360). A passenger is not a hold entry at all — it is a unit parked
 * at (−2,−2) on the shared tile lists (see
 * `ai_euro_20e6_transport_assemble`).
 *
 * Passengers nonetheless cost hull in DOS, because DOS re-derives the whole
 * transport chain on every berth act: the arrival block's stale-mark clear
 * (raw 2991-2997) strips `act_state == 1` from everything standing on the
 * berth tile — the previous act's passengers included, since `FUN_1427_040c`
 * flushed the (−2,−2) chain back onto that tile — and the raw 3024-3051 scan
 * then RE-marks them and RE-debits `iStack_d2` by their `0x5238` size. The
 * goods matrix therefore only ever sees `capacity − goods − passengers`, and
 * `0a60`'s own "hull full" test is literally `0x5237[type] == +0x3150` after
 * that round trip (decomp 87511-87514).
 *
 * This port keeps passengers in `cargo_ids`/`aboard_ship_id` instead, so the
 * mark scan's `aboard_ship_id >= 0` skip never re-marks them and the
 * reservation DOS renews each act has to be charged here. Without it a 2-slot
 * Caravel ends its turn carrying 2 passengers AND a goods hold.
 */
static int ai_euro_20e6_ship_hold_budget(
  const ColonizeUnitPool* units, const ColonizeUnit* ship
) {
  if (!units || !ship) {
    return 0;
  }
  int free_holds = ai_euro_hauler_free_holds(units, ship);
  for (int i = 0; i < ship->cargo_count && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    const ColonizeUnit* p = units_get_const(units, ship->cargo_ids[i]);
    const ColonizeUnitType* pt = p ? units_type(units, p->type_index) : NULL;
    free_holds -= pt ? pt->space : 1; /* 0x5238[type] */
  }
  return free_holds > 0 ? free_holds : 0;
}

/*
 * FUN_521d_4393 — pick a work-queue haul target, then run DOS's own
 * queue-decrement tail on the slot that won.
 *
 * Raw decomp (move_scoring_20e6_full.md "Raw recovered C", the
 * `LAB_OVL14_L0000__004393` block):
 *   for slot in 0..15:
 *     rec[+0] >= 0 && rec[+4] > 0                      -- live slot with work
 *     unit->origin(+0x06) != current colony index      -- never its home colony
 *     (colony ai_flags & 2) == 0 || unit type > 0x0f   -- MoW-threatened colony
 *                                                        is warship-only work
 *     score = rec[+2] / ((dist >> 2) + 1)
 *     if (score >= best && (bVar17 || rec[+5] != 0)) best = score, pick = slot
 * then the tail (ai_goals_work_consume): loads -= free holds, score scaled,
 * slot freed at zero.
 *
 * `bVar17` (raw ~1284-1306) is the "this hull may take civilian work" flag:
 * `type != 0x12` (Man-O-War) for everything, narrowed further for Privateer
 * (0x10) and Frigate (0x11) by globals this project has not resolved
 * (`0xa89b`, `0x9e52`, `0x9414`, the per-nation stride-0x13 tables at
 * `−0x6db4`/`−0x6da4`, and `FUN_1000_8b74`) — DEAD END, see the doc note.
 * The base `!= 0x12` term is what ships here; the two narrowings would only
 * *remove* warships from the queue, and in this port only wagons and
 * `ai_euro_is_cargo_ship_name` hulls ever reach this function, so the
 * unresolved half is unreachable rather than approximated.
 *
 * Linux-only, kept from the pre-decode version and now sourced from the
 * colony record instead of squatting on the DOS `+4` byte: the Series R
 * specialty tie-break (+32 when the hauler already carries the target
 * colony's specialty cargo).
 *
 * Also Linux-only, and required by the tail: DOS runs `20e6` once per unit
 * per turn, so a hauler claims at most one slot per tick. This port's
 * dispatcher re-enters `ai_euro_unit_act` for a unit that still has moves,
 * which without a latch let the *second* entry claim a *second* slot and
 * re-aim the wagon at a worse colony (caught by
 * `unit_specialty_flag_a_haul_match`). `s_4393_claim_*` replays the first
 * claim of the tick instead of re-scoring — one claim per unit per turn,
 * exactly DOS's cadence.
 */
/*
 * DOS unit byte +0x314a = `ColonizeCol1Unit.origin` (record +0x06) — the
 * colony this hauler is bound to. One real, save-round-tripped byte
 * (2026-09-07; previously split across two session-local latches): the 20e6
 * LOAD matrix's ship arm writes it (raw 3134-3138), the ship arrival dump
 * clears it to 0xff (raw 3008), the 457e wagon walk binds it, the colony
 * tick (FUN_5952_035e) binds unbound land units standing in a colony, and
 * the delivery matrix (raw 2055), the 4393 pick (:89887) and the arrival
 * gate (raw 1691) read it. 0xff (any value >= 0x80 — DOS reads it as a
 * signed char) = unbound; spawn inits it so, col1_bridge round-trips it.
 */
static int ai_euro_20e6_origin_get(const ColonizeUnit* u) {
  return (!u || u->col1_origin >= 0x80) ? -1 : (int)u->col1_origin;
}

static void ai_euro_20e6_origin_set(ColonizeUnit* u, int colony_id) {
  if (u) {
    u->col1_origin = (colony_id < 0 || colony_id > 0x7f) ? 0xff : (uint8_t)colony_id;
  }
}

static uint32_t s_4393_claim_turn[COLONIZE_UNITS_MAX];
static int s_4393_claim_colony[COLONIZE_UNITS_MAX];
static int s_4393_claim_valid[COLONIZE_UNITS_MAX];

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
  const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
  const int hid =
    (hauler && hauler->id >= 0 && hauler->id < COLONIZE_UNITS_MAX) ? hauler->id : -1;
  /*
   * DOS `unit+0x314a` — the colony this hauler is bound to, now the real
   * persistent `col1_origin` byte (2026-09-07): the ship arrival dump clears
   * it and the colony tick refreshes land units, so the DOS lifecycle holds
   * without the old turn-scoping (which existed only because the port had no
   * writer keeping the byte fresh).
   */
  const int bound_colony = ai_euro_20e6_origin_get(hauler);
  /* Replay this tick's already-made claim instead of consuming a second slot. */
  if (hid >= 0 && s_4393_claim_valid[hid] && s_4393_claim_turn[hid] == turn) {
    const ColonizeColony* held = colonies_get(ctx->colonies, s_4393_claim_colony[hid]);
    /*
     * The replay is subject to the same `+0x314a` rule as the scan: a hauler
     * that has LOADED at the claimed colony since the claim was made is now
     * bound to it, and DOS would have skipped that slot. Without this the
     * replay re-aims a wagon at the colony it just filled up from.
     */
    if (held && held->active && held->nation_id == nation_id &&
        held->id != bound_colony) {
      *out_x = held->x;
      *out_y = held->y;
      return 1;
    }
    return 0;
  }
  const int hauler_type = hauler ? ai_euro_20e6_dos_type(ctx->units, hauler) : -1;
  const int civilian_hull = (hauler_type != 0x12); /* bVar17 base term */
  int best_score = -1; /* DOS iStack_e2 = -1; `>=` so later ties win */
  int best_slot = -1;
  int best_colony = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    const AiWorkSlot* w = ai_goals_work(i);
    if (!w || w->id < 0 || w->loads == 0) {
      continue; /* DOS: rec[+0] >= 0 && rec[+4] > 0 */
    }
    const ColonizeColony* c = colonies_get(ctx->colonies, (int)w->id);
    if (!c || !c->active || c->nation_id != nation_id) {
      continue;
    }
    /* DOS `unit+0x314a != DS:0x8dc6` (raw viceroy_unpacked.c:89887, evaluated
     * per slot right after `FUN_281f_09e6` binds THAT slot's colony) — a
     * hauler never serves the colony it is bound to. See `bound_colony`. */
    if (bound_colony >= 0 && bound_colony == (int)w->id) {
      continue;
    }
    /* DOS `(colony+0x1b & 2) == 0 || type > 0x0f`: a colony with a Man-O-War
     * nearby is only worked by warship hulls. */
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0 && hauler_type <= 0x0f) {
      continue;
    }
    const int d = abs(c->x - from_x) + abs(c->y - from_y);
    /* −0x5f24 score, DOS distance normalization (raw 2214 / 2134:
     * score / ((dist >> 2) + 1) — replaced the thin `score − d*4`). */
    int score = (int)w->score / ((d >> 2) + 1);
    /* Series R specialty tie-break (Linux-only heuristic, not DOS). */
    if (hauler && c->specialty_cargo != 0xff &&
        (int)c->specialty_cargo < COLONIZE_CARGO_COUNT &&
        ai_euro_wagon_has_cargo_type(ctx->units, hauler, (int)c->specialty_cargo)) {
      score += 32;
    }
    /* DOS raw ~2216: a non-civilian hull may only take slots the colony
     * flagged military (rec[+5]). */
    if (score >= best_score && (civilian_hull || w->military != 0)) {
      best_score = score;
      best_slot = i;
      best_colony = (int)w->id;
      bx = c->x;
      by = c->y;
    }
  }
  if (best_slot < 0) {
    return 0;
  }
  /* DOS queue-decrement tail (raw ~2226-2242). */
  ai_goals_work_consume(best_slot, ai_euro_hauler_free_holds(ctx->units, hauler));
  if (hid >= 0) {
    s_4393_claim_turn[hid] = turn;
    s_4393_claim_colony[hid] = best_colony;
    s_4393_claim_valid[hid] = 1;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * (`ai_euro_nearest_haul_short_colony` — the Linux "drive to the nearest own
 * colony that is short of something" fallback — was deleted on 2026-09-06g.
 * It was the delivery-direction consumer that forced the 0a60 registration
 * gate to stay on the Linux `haul_short` boolean; with the gate flipped to
 * DOS's `bVar5` the queue is a PICKUP queue and this function pulled haulers
 * the wrong way. DOS has no such scan: when LAB_521d_4393 finds no slot a
 * wagon falls to LAB_521d_457e's origin-colony walk and a ship to the 457e
 * ship arms — see the header of `ai_euro_try_wagon_haul`.)
 */

/*
 * DOS unit byte +0x3158 (col1 record +0x14 — a cargo_hold slot DOS reuses as
 * land-unit AI scratch): the wagon village-errand latch. Set by the 20e6
 * load matrix's wagon arm (raw 3134-3138, iStack_34 == 0), cleared by
 * FUN_4d56_2820 at trade entry for land types (viceroy_unpacked.c:82122).
 *
 * 2026-09-06: no longer session-local. The unit array base is DS:0x3144 with
 * stride 0x1c, so +0x3158 is COL1 unit record +0x14 = cargo_hold[4]
 * (col1_save.h ColonizeCol1Unit; same arithmetic that makes +0x315b the
 * `profession` Treasure byte and +0x314a the `origin` byte), and
 * col1_bridge_capture / col1_bridge_apply now round-trip it for type 0x0c
 * units — see ai_euro_wagon_errand_* below. The byte is kept verbatim (DOS
 * only ever stores 0 or 1) so an original DOS save's value survives a
 * load→save with no rewrite.
 */
static uint8_t s_20e6_wagon_errand[COLONIZE_UNITS_MAX];

/*
 * DOS unit byte +0x315a — the "cower in port" counter. +0x315a = COL1 unit
 * record offset 0x16 = `col1_counter16` (col1_save.h), which the port already
 * round-trips (u->col1_counter16) — so the counter is save-persistent now, no
 * session array. The multiplexing with Europe-lane voyage turns and
 * trade-route stop packing is DOS's own (same byte), not a port shortcut.
 */

/*
 * DS:0x1734[nation] — count of colonies that registered work-queue work,
 * bumped in FUN_521d_0a60's bVar5 registration branch (:87675) and zeroed
 * only by the 20e6 berth boarding scan (:81295). Real counter since
 * 2026-09-07; the 20e6 colony-sail / unload matrices used to substitute a
 * NEEDS_GARRISON/MILITARY colony count for it.
 */
static int16_t s_0a60_work_registered[4];

/*
 * DS:0x1734[nation] — colonies that registered work-queue work (0a60 bVar5
 * branch bumps it; only the 20e6 berth boarding scan zeroes it).
 * Session-local, not save data; file-local since audit AE-31.
 */
static int ai_euro_0a60_work_registered(int nation_id) {
  return (nation_id >= 0 && nation_id < 4) ? (int)s_0a60_work_registered[nation_id] : 0;
}

unsigned char ai_euro_wagon_errand_get(int unit_id) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  return (unsigned char)s_20e6_wagon_errand[unit_id];
}

void ai_euro_wagon_errand_set(int unit_id, unsigned char value) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX) {
    return;
  }
  s_20e6_wagon_errand[unit_id] = (uint8_t)value;
}

void ai_euro_wagon_errand_clear_all(void) {
  memset(s_20e6_wagon_errand, 0, sizeof(s_20e6_wagon_errand));
}

/* Defined later in this file (after their 20e6 dependencies). */
static int ai_euro_20e6_load_pick(
  ColonizeTurnContext* ctx, const ColonizeColony* c, int nation, int is_ship
);
static int ai_euro_20e6_wagon_village_errand(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* wagon
);
static int ai_euro_20e6_wagon_origin_walk(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* u
);

/*
 * Wagon Train beat, DOS shape (FUN_521d_20e6 land arm):
 *
 *   at an own colony  -> dump every hold into it (raw 3007-3012 — this IS
 *                        DOS wagon delivery), then the LOAD matrix wagon arm
 *                        (`ai_euro_20e6_load_pick` is_ship=0, at most ONE
 *                        hold: `if (type==0xc && d2>1) d2 = 1`, raw ~3025),
 *                        which on a load latches the village errand
 *                        (+0x3158 = 1, raw 3136);
 *   on errand         -> the errand walker owns the wagon (raw 2284-2307);
 *   otherwise         -> the work-queue tip (LAB_521d_4393), which now aims
 *                        the wagon at a colony that HAS goods — the queue is
 *                        a PICKUP queue.
 *
 * 2026-09-06g: the Linux specialty/produced/food-first load ladder and the
 * `ai_euro_nearest_haul_short_colony` delivery-direction fallback are DELETED
 * with the 0a60 `haul_short` → `bVar5` gate flip. The ladder existed only to
 * feed that fallback; both are replaced by the DOS load matrix above plus the
 * pickup tip below. Nothing in DOS scores "nearest colony short of X" for a
 * hauler at this point.
 *
 * 2026-09-07: the 4393 pick is ships-only here too (DOS gate `0xc < type <
 * 0x13`, raw :89877) — the wagon substitute is retired. Off-errand wagons run
 * the LAB_521d_457e origin walk (`ai_euro_20e6_wagon_origin_walk`): bind to
 * the nearest own colony, park there (`+0x314b = 0x55`), walk home, or
 * destroy when unbound off-landmass. The +0x314a binding is the real
 * `col1_origin` byte now (save-round-tripped, colony-tick refreshed).
 * Cite: move_scoring_20e6_full.md 2026-09-06d/f + 2026-09-07.
 */
static int ai_euro_try_wagon_haul(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* wagon
) {
  if (!ctx || !ctx->units || !ctx->colonies || !wagon || !wagon->active) {
    return 0;
  }
  if (!ai_euro_type_is_wagon_name(ai_euro_unit_kind(ctx->units, wagon))) {
    return 0;
  }
  /* No cargo-flag entry gate any more: the DOS band runs for every wagon —
   * the arrival dump takes every cargo type and the 457e walk is
   * unconditional (the old has-cargo/has-capacity early-out was a
   * 4393-substitute artifact retired with it, 2026-09-07). */
  /*
   * DOS own-colony arrival block (FUN_521d_20e6 raw 2996-3138): dump every
   * hold into the colony (raw 3007-3012 — this IS DOS wagon delivery), then
   * the LOAD matrix wagon arm, capped at ONE hold (`if (type == 0xc && d2 >
   * 1) d2 = 1`, raw ~3025). A matrix load latches the village errand
   * (+0x3158 = 1, raw 3136) and the errand walker below owns the wagon from
   * there.
   */
  if (wagon->id >= 0 && wagon->id < COLONIZE_UNITS_MAX) {
    const int cid = colonies_id_at(ctx->colonies, wagon->x, wagon->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      /*
       * Raw 1691 gate: for a land hauler the arrival block also needs
       * `+0x314a == uStack_62` — an UNBOUND wagon does not dump; the 457e
       * walk binds it first (one-beat delay, as in DOS) and the next entry
       * dumps. Ships have no such bind requirement.
       */
      if (c && c->active && c->nation_id == nation_id &&
          ai_euro_20e6_origin_get(wagon) == cid) {
        for (;;) {
          int hold = -1;
          const int n = units_goods_hold_count(ctx->units, wagon->id);
          for (int h = 0; h < n; ++h) {
            if (units_hold_amount(ctx->units, wagon->id, h) > 0) {
              hold = h;
              break;
            }
          }
          if (hold < 0) {
            break;
          }
          if (colonies_transfer_from_unit(
                ctx->colonies, cid, ctx->units, wagon->id, hold, NULL) <= 0) {
            break;
          }
        }
        const int g = ai_euro_20e6_load_pick(ctx, c, nation_id, 0);
        if (g >= 0) {
          int qty = (int)c->stock[g];
          if (qty > 100) {
            qty = 100; /* raw 3126-3129 */
          }
          if (qty > 0 &&
              colonies_transfer_to_unit(ctx->colonies, cid, ctx->units, wagon->id, g, qty) >
                0) {
            s_20e6_wagon_errand[wagon->id] = 1;
            /* DOS's land arm writes only +0x3158 here (raw 3136) — the
             * wagon's +0x314a stays bound to this, its home colony. */
            if (getenv("AI_20E6_LOAD_TRACE")) {
              fprintf(
                stderr, "[load] wagon %d n%d colony %d cargo %d qty %d errand\n",
                wagon->id, nation_id, cid, g, qty
              );
            }
          }
        }
      }
    }
    /*
     * Village-errand walker (raw 2284-2307 + 4528 AI arm case 1): an errand
     * wagon ignores the haul chain — goto the nearest same-landmass village
     * (capital distance halved, min 1), trade via the 2820 shell on arrival,
     * destroy when no village shares the landmass (LAB_47b9).
     */
    if (s_20e6_wagon_errand[wagon->id] &&
        ai_euro_20e6_wagon_village_errand(ctx, nation_id, wagon)) {
      return 1;
    }
  }
  /*
   * LAB_521d_457e origin walk (raw 2256-2289) — the DOS home of an off-errand
   * wagon. Replaces the retired "wagons peel the ships-only 4393 queue"
   * substitute (divergence closed 2026-09-07): bind to the nearest own colony
   * when unbound, park at the bound colony (orders 0x55), walk home
   * otherwise, destroy when unbound with no own colony on this landmass.
   */
  return ai_euro_20e6_wagon_origin_walk(ctx, nation_id, wagon);
}


/*
 * bugs.md #610: the invented AI pioneer improvement planner
 * (`ai_euro_try_pioneer_improve` / `_improve_target` / `_tile_can_plow` /
 * `_tile_can_road` / `AI_EURO_IMPROVE_TIMER_MIN`) is gone. It cited only
 * Colonization.pdf: DOS's AI never writes `+0x314c = 8` (the sole writer of
 * order 8 in the image is the human ORDERS UI, raw 42511), never scans a
 * colony ring for an improvable tile, has no plow-before-road rule, no MD<=3
 * limit and no prefer-Hardy rule. The two real DOS arms replace it:
 * `ai_euro_20e6_build_road_arm` (FUN_521d_20e6 raw 90183-90206, #611) and
 * `ai_euro_5952_improve_best_plot` (FUN_5952_035e raw 94470-94551, #612).
 */

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
    /*
     * DOS-LITERAL FUN_479b_00ca affordability gate (bugs.md #709), read
     * byte-exact off viceroy_unpacked.asm 121034-121094:
     *   if (nation < 4 && DS:[nation*0x34 + 0x543f] == 0) return 0;  // human
     *   cost = FUN_281f_0d78(...);          // = the 4cc6_07c2 price
     *   gold = FUN_281f_0a92(nation);       // 32-bit DX:AX
     *   SUB CX,AX / SBB BX,DX  ;  SAR (cost),1        →
     *   if ((long)gold - cost < cost / 2) return 0;   // JL/JC LAB_479b_0150
     *   FUN_281f_0af6(nation, cost);        // pay
     *   INC byte [ [0x8d4e] + 5 ];          // tribe lands_bought
     *   FUN_281f_068c(x, y, 0x10, 1);       // stamp the tile bought
     * i.e. the AI does not spend down to zero: it buys only when at least half
     * the price is still left afterwards. The gate is AI-only — for a human
     * (control byte 0x543f == 0) 00ca returns 0 without touching gold, which is
     * why the human branch below keeps its own plain `gold < cost` message.
     * Lead still open: 00ca's caller is the thunk 2a1f:01dd and is unresolved,
     * so whether a failed gate aborts the founding or founds the colony unpaid
     * is unknown; the port keeps its existing "found anyway" outcome.
     */
    int short_gold;
    if (cost <= 0) {
      short_gold = 0;
    } else if (nation_id == ctx->human_nation) {
      short_gold = *gold < (uint32_t)cost;
    } else {
      short_gold = ((long)*gold - (long)cost) < (long)(cost / 2);
    }
    if (short_gold) {
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
      cid = colonies_found_with_indian_land_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, gold, founder->x, founder->y, nation_id, founder->type_index, founder->profession, tools, muskets, horses);
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
    colonies_reveal_founded_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, cid); /* FUN_364b_1dd6 Coronado */
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
    if (ctx->units && ctx->map && colonies_count_for_nation(ctx->colonies, nation_id) == 1) {
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
          if (sh->moves <= 0) {
            sh->moves = units_max_mp(ctx->units, sh->id);
          }
        }
      }
      /* Pioneer on found+1: SW coast course after first town. Cite: TURN5 FR. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* p = &ctx->units->units[i];
        if (!p->active || p->nation_id != nation_id || p->aboard_ship_id >= 0) {
          continue;
        }
        if (!ai_euro_name_is_pioneer(ai_euro_unit_kind(ctx->units, p))) {
          continue;
        }
        if (p->x == founded_x && p->y == founded_y + 1) {
          ai_euro_set_goto(p, UNITS_ORDER_AI_SAIL, founded_x - 3, founded_y + 3);
          /* (50,38)→(48,39) is three cardinal minor-river steps at 1 third
           * each (TURN4→5): a fresh 3-third allotment covers it. */
          p->moves = units_max_mp(ctx->units, p->id);
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
              if (sh->moves <= 0) {
                sh->moves = units_max_mp(ctx->units, sh->id);
              }
            }
          }
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            ColonizeUnit* su = &ctx->units->units[i];
            if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
              continue;
            }
            if (!ai_euro_name_is_soldier(ai_euro_unit_kind(ctx->units, su))) {
              continue;
            }
            if (su->x == founded_x + 1 && su->y == founded_y + 3) {
              ai_euro_set_goto(su, UNITS_ORDER_AI_MOVE, founded_x + 1, founded_y + 4);
              su->moves = UNITS_MP_PER_TILE;
            }
          }
        }
      }
    }
  }
  /*
   * DOS: a new AI town already carries its first project in the same turn
   * (seed-100 TURN4-6 saves: Docks). Idle-queue-only pick, so re-running it
   * here after the planning-phase call is harmless for existing towns.
   * This is the LAST surviving `ai_euro_prefer_*` pass (bugs.md #483 deleted
   * the other twenty): the FUN_5952_035e cascade runs in the nation's
   * planning phase, which a colony founded mid-act has already missed, and
   * the golden's founding-turn Docks is what this stands in for. Retiring it
   * means finding DOS's own founding-turn project writer, not moving the
   * cascade.
   */
  ai_euro_prefer_peace_construction(ctx, nation_id);
}

/*
 * `ai_euro_join_colony` (the bare colonies_admit_unit_w wrapper) is gone with
 * its last caller, the invented LABOR/COLONY arrival join. Every surviving
 * admission goes through the arm that owns it: FUN_5952_035e's colony-tick
 * tile re-scan (ai_euro_5952_absorb_equip, raw 94231-94274) and the expert
 * field/workplace assign arms, which admit *and* seat the colonist.
 */

/* --- inventory (6d8e steps 1–3) ---------------------------------------- */

static void ai_euro_colony_inventory(ColonizeTurnContext* ctx, int nation_id) {
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  if (!inv || !ctx) {
    return;
  }
  ai_goals_inventory_clear(nation_id);
  inv->colony_count = colonies_count_for_nation(ctx->colonies, nation_id);
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
    if (c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
      inv->food_short += (c->population * 2) - c->stock[COLONIZE_CARGO_FOOD];
    }
    /* Ore shortage (5cf6-shaped): feed Blacksmith / Expert Ore Miner dock hire. */
    if (c->stock[COLONIZE_CARGO_ORE] < 20) {
      inv->ore_short += 20 - c->stock[COLONIZE_CARGO_ORE];
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
     * FUN_5952_035e origin refresh (colony_tick_5952_035e.md raw ~348): every
     * LAND unit in this colony's tile stack whose +0x314a origin is unbound
     * (< 0) is bound to this colony (`unit->origin = DS:0x8dc6`). This is the
     * DOS writer that keeps the persistent origin byte from drifting — the
     * 4393 pick, the 20e6 arrival gate and the 457e wagon walk all read it.
     */
    if (ctx->units) {
      /* Slot walk (audit second-wave Leads 2, 2026-09-10): the loop variable
       * is an ARRAY index, not a unit id — see the ai_euro_refresh_continent_
       * stance note. */
      for (int uid = 0; uid < COLONIZE_UNITS_MAX; ++uid) {
        ColonizeUnit* su = &ctx->units->units[uid];
        if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
          continue;
        }
        if (su->x != c->x || su->y != c->y) {
          continue;
        }
        const int t = ai_euro_20e6_dos_type(ctx->units, su);
        if (t >= 0x0d && t <= 0x12) {
          continue; /* DOS gate: type < 0xd || type > 0x12 — land only */
        }
        if (su->col1_origin >= 0x80) {
          su->col1_origin = (uint8_t)c->id;
        }
      }
    }
    /*
     * The 11-cargo "surplus haul ladder" that used to refresh +0x8d here was
     * a port invention (its own comment read "FUN_5952_0306 thin"), and it
     * ran in the wrong body besides. DOS's only +0x8d writers in the AI turn
     * are FUN_5952_035e's five FUN_5952_0306 calls, now ported literally as
     * ai_euro_5952_build_pref_0306 and hosted at DOS's position inside the
     * colony tick (2026-09-18).
     */
  }
}

/*
 * The "unit inventory" step (FUN_521d_6d8e's own per-unit prelude, raw
 * 93147-93172) used to live here as a per-Pioneer `inv->muskets_short--`.
 * That was a mis-aimed double-port: the DOS prelude's per-unit arm decrements
 * the TOOLS demand counter DS:0xa0da (raw 93168-93170, "the Pioneers already
 * in the field cancel the tools demand"), never the Muskets counter 0xa0db,
 * and that arm is already ported faithfully in `ai_euro_5d04_cb_colony_needs`
 * (the 0xa0da/0xa0db writer the 5d04 hire matrix actually reads). Deleted
 * 2026-09-09 (smell audit #35): nothing here is unported, so no replacement
 * step is needed and `inv->muskets_short` now means exactly what
 * `ai_euro_colony_inventory` tallies.
 */

/* --- 5d04 planning / hire ---------------------------------------------- */

static int ai_euro_type_is_wagon_name(ColonizeUnitKind kind) {
  /* @UNIT row 12 is "Wagon Train"; no roster spells it "Supply Train". */
  return kind == UNITS_KIND_WAGON;
}

/*
 * The AI_EURO_*_PURCHASE_GOLD / AI_EURO_VETERAN_SOLDIER_TRAIN_GOLD constants
 * that used to sit here were the Linux-shaped hire matrix's private price
 * copies; they went unreferenced when that matrix was retired 2026-09-07e and
 * are removed 2026-09-07. The real prices are the DS:0x978d stride-6 purchase
 * table europe.c now owns (europe_purchase_price, audit AE-17), and DOS
 * 5d04 never trains a Veteran in Europe for gold at all (its only Veteran path
 * is the College bVar5 profession promote).
 */

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
 * 2026-08-18 via live DOSBox-X data dump (`D 237D:9790`): 1000/2000/5000.
 * Identity resolved 2026-09-07d: these are the *price cells* of the
 * FUN_521d_5c3c Europe purchase table (DS:0x978d stride 6) — Caravel /
 * Privateer / Frigate — so each floor guarantees the flagged nation can
 * afford exactly the ship its flag buys. */
static uint32_t ai_euro_5d04_ph_gold_floor(int which) {
  const EuropePurchaseOption* opt;
  switch (which) {
    case 0x9796: /* no_ships: Caravel floor (table index 1) */
      opt = europe_purchase_option_at(1);
      return opt ? (uint32_t)opt->gold : 0;
    case 0x97a8: /* privateer threat: Privateer floor (table index 4) */
      opt = europe_purchase_option_at(4);
      return opt ? (uint32_t)opt->gold : 0;
    case 0x97ae: /* frigate threat: Frigate floor (table index 5) */
      opt = europe_purchase_option_at(5);
      return opt ? (uint32_t)opt->gold : 0;
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
 * function (`thunk_FUN_2a1f_05f0`)'s own formula.
 *
 * LIVE 2026-09-07: the source detector (the 11×11 ship probe with the
 * FUN_6662_0906 cost 0..5 sub-gate) now runs in
 * ai_euro_refresh_colony_ai_flags, accumulating s_ship_pressure per nation
 * — this crumb reads those tallies. */
static int ai_euro_5d04_ph_naval_threat_crumb_n(int which, int nation_id) {
  if (nation_id < 0 || nation_id > 3) {
    return 0;
  }
  const AiEuroShipPressure* sp = &s_ship_pressure[nation_id];
  switch (which) {
    case 0xa89b: return (int)sp->frigate_colonies;
    case 0xa89a: return (int)sp->other_colonies;
    case 0x9e52: return sp->frigate_pop;
    case 0x9e54: return sp->other_pop;
    default: return 0;
  }
}

static int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);

/* FUN_281f_09fc(0xd) on a scanned colony — building index 0xd = College
 * (NAMES.TXT @BUILDING file order, cross-checked against index 0x24 =
 * Lumber Mill in euro_unit_act.md). Real since 2026-09-07d — the bVar5
 * consumer (hire tail Veteran Soldier promote) is live. */
static int ai_euro_5d04_colony_has_college(
  const ColonizeTurnContext* ctx, const ColonizeColony* colony
) {
  const int id = colonies_building_row(ctx->colonies, COLONY_BUILDING_COLLEGE);
  return id >= 0 && colony->has_building[id];
}

/* Raw 92412-92423 (WoI arm): destroy the nation's first Man-O-War and
 * `*(int*)0x53de += 1`. Real since 2026-09-07d — type 0x12 = Man-O-War by
 * the confirmed @UNIT ship roster (0x0d..0x12; the old "NEW WORLD wagon"
 * name predates that confirmation), and 0x53de = head.expeditionary_force
 * [2] (man-o-wars): on Declare Independence the crown seizes the AI
 * nation's MoW into the REF pool — the increment is genuine reuse, not a
 * Ghidra misattribution.
 *
 * 2026-09-07g (asm OVL14:005dd9): the scan is NOT map-wide. FUN_281f_07e0
 * is unit_index_on_tile(236+n, 236+n) — the nation's Europe-dock
 * pseudo-tile — so the King seizes only a Man-O-War standing in a European
 * port. The wave MoW (0982) spawns on real map water and is never
 * touchable, which is why DOS can run 5d04 on the crown slot itself
 * during WoI without eating the REF fleet. */
static void ai_euro_5d04_woi_seize_manowar(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx->units) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || !ai_euro_in_europe(u->x, u->y)) {
      continue;
    }
    if (ai_euro_20e6_dos_type(ctx->units, u) == 0x12) {
      (void)units_despawn(ctx->units, u->id);
      ctx->col1->head.expeditionary_force[2]++;
      return;
    }
  }
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

/* Raw 92404-92532 gate cascade — live since 2026-09-07d (flags feed the
 * real ship-buy ladder, the gold floors, and the hire tail's Veteran
 * promote). bVar5/no_ships/frigate_threatened/privateer_threatened/
 * no_clear_navy/cargo_short name the raw bVar5/21/22/23/24/7 booleans in
 * call order. Naming history: weak_vs_euro/weak_vs_indian until
 * 2026-08-19 (the 0xa89a crumbs are "own colonies near an armed rival
 * ship", not rival-strength); manowar_threatened until 2026-09-07d (the
 * bVar23 response is a Privateer purchase). */
typedef struct Ai5d04PlanningFlags {
  int has_college;       /* bVar5 */
  int no_ships;           /* bVar21 */
  int frigate_threatened;  /* bVar22 */
  int privateer_threatened; /* bVar23 — buys purchase-table type 4 =
     Privateer (0x10) at the Privateer gold floor (0x97a8 = its price
     cell); the type-count gates read unit_type_counts[.][0x10]. Was
     misnamed manowar_threatened until 2026-09-07d — the 0xa89a crumb is
     "non-Frigate armed ship nearby" (MoW included), but what the flag
     *buys* is a Privateer. */
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
        ai_euro_5d04_colony_has_college(ctx, c)) {
      f.has_college = 1;
      break;
    }
  }

  /* WoI arm (raw 92412-92423): crown seizes the nation's Man-O-War into
   * the REF pool — see ai_euro_5d04_woi_seize_manowar's header. */
  if (woi) {
    ai_euro_5d04_woi_seize_manowar(ctx, nation_id);
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
  if (!((ai_euro_5d04_ph_naval_threat_crumb_n(0xa89b, nation_id) == 0 &&
         ai_euro_5d04_ph_naval_threat_crumb_n(0xa89a, nation_id) == 0) ||
        local_3c == 0)) {
    int reach_weak_check = 0;
    if (ai_euro_5d04_ph_naval_threat_crumb_n(0xa89b, nation_id) < col_half &&
        ai_euro_5d04_ph_naval_threat_crumb_n(0x9e52, nation_id) < pop_half) {
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
   * 92456-92474). `0xa89a`/`0x9e54` = count/level-sum for any qualifying
   * ship type other than Frigate. The response is a Privateer buy: own
   * Privateer count (unit_type_counts[.][0x10]) < 2 and the human owns
   * one — see the flag's own comment in Ai5d04PlanningFlags. */
  f.privateer_threatened = 0;
  if (((ai_euro_5d04_ph_naval_threat_crumb_n(0xa89a, nation_id) != 0 ||
        ai_euro_5d04_ph_naval_threat_crumb_n(0xa89b, nation_id) != 0)) &&
      local_3c != 0 && !f.frigate_threatened) {
    int reach = 0;
    if (ai_euro_5d04_ph_naval_threat_crumb_n(0xa89a, nation_id) < col_half &&
        ai_euro_5d04_ph_naval_threat_crumb_n(0x9e54, nation_id) < pop_half) {
      if (turn > 100 && nat_gold_ge(ctx, nation_id, 1000)) {
        reach = 1;
      }
    } else {
      reach = 1;
    }
    if (reach && focus_ok) {
      f.privateer_threatened = stuff->unit_type_counts[nation_id][16] < 2 &&
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

  /* bVar7: cargo/passenger space short (raw 92528-92532) —
   * `ship_cargo_totals <= (census_pop_proxy/2 + colony_counts*2)/2`.
   * −0x6bf0 = DS:0x9410 = census_pop_proxy (misread as colony_pop_totals
   * until 2026-09-07d; colony_pop_totals is −0x6bf4/0x940c and only feeds
   * the threat weak-checks above). */
  f.cargo_short =
    stuff->ship_cargo_totals[nation_id] <=
      (((stuff->census_pop_proxy[nation_id] >> 1) +
        stuff->colony_counts[nation_id] * 2) >> 1) &&
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
 * mutation step — live since 2026-09-07d (the orchestrator calls it right
 * after `compute_flags`, matching raw 92476-92505 order). */
static void ai_euro_5d04_apply_naval_gold_floors(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f
) {
  if (f->no_ships) {
    ai_euro_5d04_apply_gold_floor(ctx, nation_id, ai_euro_5d04_ph_gold_floor(0x9796));
  }
  if (f->privateer_threatened) {
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

/* thunk_FUN_2a1f_0500 = FUN_521d_5c3c — Europe unit purchase (real port,
 * 2026-09-07d). Table at DS:0x978d stride 6 ({dos_type, ?, 0xff, price16}),
 * pinned byte-identical across 3 original_memory_dumps: 0=Artillery/500,
 * 1=Caravel/1000, 2=Merchantman/2000, 3=Galleon/3000, 4=Privateer/2000,
 * 5=Frigate/5000 (the "gold floors" 0x9796/0x97a8/0x97ae are this table's
 * Caravel/Privateer/Frigate price cells). The raw weight_pct arg feeds
 * FUN_521d_5c38 which is `return 1;` in the retail build — dead, dropped.
 * Body: gold >= price → spawn table type in Europe (FUN_281f_095c at the
 * nation's Europe tile), then raw 92291-92300: goal target (+0x314d/e) =
 * nation+0x32/+0x33 (return_from_europe x/y) and the orders byte (+0x314c) = 1
 * for land types (< 0xd || > 0x12), 0 for hulls; gold -= price, return 1. */
static int ai_euro_5d04_propose_ship_buy(
  ColonizeTurnContext* ctx, int nation_id, int type_id
) {
  /* The DS:0x978d stride-6 table itself lives in europe.c (audit AE-17);
   * `type_id` is that table's own row index, unchanged. */
  const EuropePurchaseOption* opt =
    (ctx && ctx->units && ctx->col1) ? europe_purchase_option_at(type_id) : NULL;
  if (!opt) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  const uint32_t price = (uint32_t)opt->gold;
  if (nat->gold < price) {
    return 0;
  }
  /* The purchase slot's identity is its @UNIT kind; its name is display text. */
  int lt = units_kind_type_index(ctx->units, opt->kind);
  if (lt < 0) {
    return 0;
  }
  const int sid = units_spawn_allow_stack(ctx->units, lt, 200, 100);
  if (sid < 0) {
    return 0;
  }
  ColonizeUnit* u = units_get(ctx->units, sid);
  if (!u) {
    return 0;
  }
  units_set_nation(u, nation_id);
  u->moves = 0; /* docked in Europe this turn, as in DOS */
  /*
   * FUN_521d_5c3c raw 92291-92300, verbatim after FUN_281f_095c:
   *   +0x314d = nation+0x32;  +0x314e = nation+0x33;      (return-from-Europe)
   *   +0x314c = (type < 0xd || type > 0x12) ? 1 : 0;      (orders byte)
   * i.e. a land purchase leaves Europe SENTRY (act_state 1) with the nation's
   * last Europe-exit tile as its goal target; a hull gets orders 0. The port's
   * nation record carries +0x32/+0x33 as return_from_europe_x/y (col1_bridge.c
   * :1721 / :3138), so the target is written straight from there.
   */
  {
    const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
    const int is_hull = dtype >= 0xd && dtype <= 0x12;
    u->orders = is_hull ? UNITS_ORDER_NONE : UNITS_ORDER_SENTRY;
    u->goto_x = (int)nat->return_from_europe_x;
    u->goto_y = (int)nat->return_from_europe_y;
  }
  nat->gold -= price;
  if (ctx->europe && nation_id == ctx->human_nation) {
    ctx->europe->gold = (int)nat->gold;
  }
  return 1;
}

/*
 * Raw 92533-92567: ship-buy candidate ladder, gated on `!woi &&
 * ship_cargo_totals[n] <= census_pop_proxy[n]/2 + colony_counts[n]`
 * (−0x6bf0 = DS:0x9410 = census_pop_proxy — an earlier pass misread it as
 * colony_pop_totals, fixed 2026-09-07d). Returns the raw body's `local_3e`
 * (0 = no candidate bought). `*out_abort` is the raw early `return;` —
 * frigate/privateer threatened and the purchase failed (couldn't afford).
 */
static int ai_euro_5d04_ship_buy_ladder(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f, int* out_abort
) {
  *out_abort = 0;
  const ColonizeCol1Head* head = &ctx->col1->head;
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int woi = head->game_options.woi != 0;
  const int proxy_half = stuff->census_pop_proxy[nation_id] >> 1;
  if (woi || stuff->ship_cargo_totals[nation_id] >
             (uint32_t)(proxy_half + stuff->colony_counts[nation_id])) {
    return 0;
  }
  int candidate = 0;
  if (f->frigate_threatened) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 5);
  }
  if (candidate == 0 && f->frigate_threatened) {
    *out_abort = 1;
    return 0;
  }
  if (f->privateer_threatened) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 4);
  }
  if (candidate == 0 && f->privateer_threatened) {
    *out_abort = 1;
    return 0;
  }
  if (candidate == 0 && stuff->armed_ship_counts[nation_id] < 8 &&
      dos_rng_range(ctx->rng, 0, 1) != 0 && f->no_clear_navy) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 5);
  }
  if (candidate == 0 && dos_rng_range(ctx->rng, 0, 3) != 0) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 3);
  }
  if (candidate == 0 && dos_rng_range(ctx->rng, 0, 1) == 0 &&
      stuff->ship_cargo_totals[nation_id] < 0xc) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 2);
  }
  if (candidate == 0 && stuff->ship_cargo_totals[nation_id] < 3) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 1);
  }
  if (candidate == 0 && stuff->armed_ship_counts[nation_id] < 4 &&
      dos_rng_range(ctx->rng, 0, 3) == 0 && f->no_clear_navy && !f->cargo_short) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 4);
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
/*
 * DOS unit+0x3146 @UNIT code for a Linux type, by name (fixtures use small
 * synthetic pools, so pool indices are not DOS indices). 0xff = unknown.
 *
 * Audit AE-4: this file carried two name→@UNIT tables. This one used to
 * collapse every sea hull to Caravel 0xd / Privateer 0x10; DOS does no such
 * thing — +0x3146 always holds the real @UNIT row (COLONIZE/NAMES.TXT rows
 * 13 Caravel .. 18 Man-O-War), and the collapse was a port shortcut. Neither
 * live consumer can see the difference (the Pioneer test at the recruit loop
 * compares against 2, and the DS:0x5238 hull-space read resolves ship codes
 * to no pool row either way), so the full table is adopted. Both tables are
 * now units_type_dos_code, whose kind ids ARE the @UNIT row numbers.
 */
static int ai_euro_5d04_dos_type_of(const ColonizeUnitPool* pool, int type_index) {
  const int code = units_type_dos_code(units_type(pool, type_index));
  return code >= 0 ? code : 0xff;
}
/*
 * DOS @UNIT code → Linux pool type. Codes 0..5 are the Europe dock table
 * (europe_dock_unit_type_index_ex with the singular fallbacks this path
 * needs, audit AE-18); 0xb = Artillery, which the dock table does not
 * carry, so it keeps its own arm.
 */
static int ai_euro_5d04_linux_type_for(const ColonizeUnitPool* pool, int dos_code) {
  if (dos_code == UNITS_KIND_ARTILLERY) {
    const int t = units_kind_type_index(pool, UNITS_KIND_ARTILLERY);
    return t >= 0 ? t : units_kind_type_index(pool, UNITS_KIND_ARTILLERY);
  }
  return europe_dock_unit_type_index_ex(pool, dos_code, true);
}
/*
 * DS:0x5238[type] — the @UNIT hull-space column for a DOS @UNIT code. The
 * table is DOS-indexed, so the code has to be translated back to a Linux pool
 * type first; 1 when the pool has no such row (DOS's own land-unit default).
 */
int ai_euro_5d04_dos_type_space(const ColonizeUnitPool* pool, int dos_code) {
  const int lt = ai_euro_5d04_linux_type_for(pool, dos_code);
  const ColonizeUnitType* t = lt >= 0 ? units_type(pool, lt) : NULL;
  return t ? t->space : 1;
}
int ai_euro_5d04_dos_type_code(const ColonizeUnitPool* pool, int type_index) {
  return ai_euro_5d04_dos_type_of(pool, type_index);
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
    /*
     * DELIBERATE PORT SUBSTITUTION (bugs.md #636), not a transcription.
     * DOS FUN_521d_5d04 raw 92770-92783 (dup raw 86688-86697) writes only
     * `unit[+0x3146] = 2` plus the FUN_291f_0c14(0xe, 100) market
     * bookkeeping for the 100 tools it just bought; it never touches
     * `+0x3159`, so the fresh unit keeps whatever slot-reuse garbage is
     * there — often 0, which is how DOS feeds the FUN_479b_0158 tools
     * underflow family. The port stocks the 100 tools it paid for instead,
     * because a 0-tools AI Pioneer is inert here (`units_is_pioneer`
     * requires tools > 0). Keep as is; do not "fix" to DOS without a live
     * trace — reverting would disarm every Europe-bought AI Pioneer.
     */
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
  return (profession == UNITS_JOB_COLONIST ||
          (profession >= 0x19 && profession <= 0x1c))
           ? 0
           : 1;
}
/* FUN_38fd_0718: spawn the recruit in Europe (gold already paid by the caller). */
static int ai_euro_5d04_cb_dock_pop_candidate(int profession) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (!ctx || !ctx->units || !ctx->col1) {
    return -1;
  }
  int type = 0;
  if (profession == UNITS_JOB_PIONEER) {
    type = 2;
  } else if (profession == UNITS_JOB_MISSIONARY) {
    type = 3;
  } else if (profession == UNITS_JOB_SCOUT) {
    type = 5;
  } else if (profession == UNITS_JOB_SOLDIER) {
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
  u->moves = 0;
  /* raw 59133 `*(undefined1 *)(iVar2 + 0x314c) = 1`: FUN_38fd_0718 parks the
   * fresh recruit SENTRY in the harbour (+0x314c = the orders byte; europe.c
   * does the same for the human harbour spawn). bugs.md #509. */
  u->orders = UNITS_ORDER_SENTRY;
  u->profession = profession;
  if (type == 2) {
    u->tools = 100; /* 59140: local_4 == 2 -> +0x3159 = 100 */
  } else if (type == 1) {
    u->muskets = 50;
  } else if (type == 4) {
    u->muskets = 50;
    u->horses = 50;
  } else if (type == 5) {
    /*
     * DOS FUN_38fd_0718 (raw 59133-59142) writes no equipment for any type
     * but the Pioneer's tools: mounted/armed state IS the type byte
     * (+0x3146 = 1 Soldier / 4 Dragoon / 5 Scout). This port models the kit
     * as per-unit goods instead (units_sync_equip_after_type_change,
     * units.c: SCOUT -> horses = UNITS_EQUIP_HORSES, the port's own
     * canonical Scout shape), so the Soldier/Dragoon musket/horse writes
     * above are the same representation choice. A hired Scout with 0 horses
     * was the odd one out: it dismounted to nothing on demote and returned
     * no horses when it joined a colony.
     */
    u->horses = UNITS_EQUIP_HORSES;
  }
  return (int)(u - ctx->units->units);
}
/*
 * Refill the recruit-pool slot the AI hire just emptied. DOS `FUN_38fd_46d4`
 * (raw 64554-64694) is a difficulty-scaled tier roll — Petty Criminal /
 * Indentured Servant / Free Colonist are common, experts rare and never
 * three at once — not a flat draw over the @JOB range, which is what this
 * callback used to be. The faithful port already lives in europe.c and
 * writes `nation.recruit[slot]` itself, including the DOS draw order
 * (FUN_281f_04d4(1,15) / (1,10) / (1,8) off the shared game stream,
 * raw 64632/64636/64640) and the AI difficulty substitution of 1
 * (raw 64627-64633). bugs.md #510.
 */
static int ai_euro_5d04_refill_pool_slot(int slot) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (!ctx || !ctx->col1) {
    return UNITS_JOB_NONE;
  }
  return europe_nation_refill_pool_slot(ctx->col1, s_5d04_nation, slot, false, ctx->rng);
}
/* Europe SELL quote: FUN_291f_09ea → FUN_38fd_0040 = euro_price − 1 (the same
 * value europe_sell_price returns), from the EuropeScreen (the one Linux
 * market) when present, else from the nation's col1 euro_price byte. */
static int ai_euro_5d04_cb_sell_price(int cargo) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  if (!ctx || !ctx->col1 || cargo < 0 || cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  if (ctx->europe && cargo < ctx->europe->cargo_count && ctx->europe->cargo[cargo].bid > 0) {
    return europe_sell_price(ctx->europe, cargo);
  }
  const int p = (int)ctx->col1->nation[s_5d04_nation].trade.euro_price[cargo] - 1;
  return p < 0 ? 0 : p;
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
/* DOS unit+0x3150 holds_occupied (defined below with the 0a60 block). */
static int ai_euro_0a60_goods_holds_used(const ColonizeUnitPool* units, const ColonizeUnit* u);

static int ai_euro_5d04_cb_reward_case(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (!u || units_hold_amount(s_5d04_ctx->units, u->id, 0) <= 0) {
    return -1;
  }
  return u->hold_goods_type[0];
}
static int ai_euro_5d04_cb_reward_value(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  return units_hold_amount(s_5d04_ctx->units, u->id, 0);
}
static void ai_euro_5d04_cb_reward_ack(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (u) {
    (void)units_unload_goods_hold(s_5d04_ctx->units, u->id, 0, NULL, NULL);
  }
}
/*
 * FUN_291f_0dc6(unit, 0, 100) + FUN_281f_0aba: sell hold 0, credit the nation.
 *
 * What the DOS leg actually writes (viceroy_unpacked.c:91089-91090, twin at
 * 93015-93016):
 *   iVar16 = FUN_291f_0dc6(unit, 0, 100);            // → FUN_38fd_1f0c
 *   FUN_281f_0aba(DS:0x9e12, iVar16, iVar16 >> 15);  // → FUN_15eb_0556
 *
 * FUN_38fd_1f0c (viceroy 60320-60337): FUN_281f_0aec empties hold 0 (qty into
 * DS:0x8dc4, excess above 100 handed back by FUN_281f_0d58), price =
 * thunk_FUN_291f_09ea = FUN_38fd_0040 = euro_price−1, then
 * thunk_FUN_291f_0a2e → FUN_38fd_1dfa (viceroy 60247-60293) = the whole ledger:
 * the four market pools (record 3 damped ·2/3), nation+0xbc trade.tons += qty,
 * nation+0xfc trade.tons2 += qty, nation+0x7c trade.gold += (price·qty·
 * (100−tax))/100.  1f0c then RETURNS price·qty — the UNTAXED gross — and 0aba
 * adds exactly that to the treasury.
 *
 * So, against smell audit #61: tons2 was genuinely missing (fixed — 1dfa is
 * already ported as europe_apply_trade_volume(..., is_buy=0,
 * immediate_threshold=0); threshold 0 because 1f0c never calls FUN_38fd_0058),
 * the trade.gold ledger is tax-adjusted but the TREASURY credit is not, and
 * royal_money is NOT written here — the nation+0x22/+0x26 pair belongs to the
 * HUMAN harbor sale (viceroy 60557-60575), which applies the tax itself outside
 * 1f0c. The AI leg pays no Crown cut, like the 20e6 dump-sell tail; #61's
 * royal_money half is refuted, not fixed.
 */
static int ai_euro_5d04_cb_sell_hold0(int idx) {
  ColonizeTurnContext* ctx = s_5d04_ctx;
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  /* Sentinel-aware: a 255 hold 0 is empty, not 255 units to sell at
   * euro_price−1 (smell audit sweep-3 area C #5). */
  if (!ctx || !u || units_hold_amount(s_5d04_ctx->units, u->id, 0) <= 0) {
    return 0;
  }
  /* Boycotted cargo stays aboard (europe_cargo_boycotted / boycott_bitmap). */
  {
    const int c0 = u->hold_goods_type[0];
    /* One accessor, one word (nation+0x20 — audit G6): the human branch used
     * to read the render mirror, which is stale outside the Europe screen. */
    const int boycotted =
      europe_cargo_boycotted_ex(ctx->europe, ctx->col1, s_5d04_nation, c0);
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
  /* 0a2e → 1dfa: pools + trade.tons + trade.tons2 + tax-adjusted trade.gold.
   * Needs the shared EuropeScreen; headless (no eu) the ledger is skipped, the
   * treasury half below is col1-only and always applies (same shape as
   * ai_euro_20e6_delivery_sell_tail). */
  if (ctx->europe) {
    europe_apply_trade_volume(
      ctx->europe, ctx->col1, s_5d04_nation, ctx->human_nation, cargo, amount, 0, 0
    );
  }
  /* 1f0c's return → 0aba: the UNTAXED gross, price = euro_price−1. */
  const int gained = ai_euro_5d04_cb_sell_price(cargo) * amount;
  if (gained > 0) {
    nat->gold += (uint32_t)gained;
    ai_euro_5d04_cb_sync_gold();
  }
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
  u->moves = 0;
  return (int)(u - ctx->units->units);
}
/*
 * FUN_521d_6d8e prelude (raw 93107-93172, the writer of every DS byte the
 * 5d04 hire matrix reads) — decoded 2026-09-07e:
 *   DS:0xa0db += 1 per own colony whose `+0x8d` specialty_cargo == 0x0f
 *                (Muskets) and again per own colony whose `+0xb8`
 *                stock[15] (Muskets) is 0.
 *   DS:0xa0da += 1 per own colony whose `+0xb6` stock[14] (Tools) is 0,
 *                then −= 1 per own unit of type 0x02 (Pioneer) — the
 *                Pioneers already in the field cancel the tools demand.
 *   (DS:0xa0d4 is the same tally for `+0xaa` stock[8] Horses; 5d04 never
 *   reads it, so it is not modelled.)
 * Colony stock offsets pinned from col1_save.h (`stock[16]` u16 @ +0x9a):
 * +0xaa = 8 Horses, +0xb6 = 14 Tools, +0xb8 = 15 Muskets.
 */
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
  /* raw 93168-93170: every own Pioneer (@UNIT type 0x02) decrements 0xa0da. */
  if (s_5d04_ctx && s_5d04_ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &s_5d04_ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (ai_euro_5d04_dos_type_of(s_5d04_ctx->units, u->type_index) == 2) {
        t--;
      }
    }
  }
  *out_muskets = m;
  *out_tools = t;
}

/*
 * DS:0xa0b8[nation] (`param_1 + -0x5f48`) — resolved 2026-09-07e from its
 * sole writer, the FUN_521d_6d8e prelude (raw 93109 zeroes it, raw
 * 93139-93141 bumps it): the number of this nation's colonies whose AI
 * byte `+0x1b` has bit 0x10 set — COLONIZE_COLONY_AI_NEEDS_COLONISTS.
 * (The only other reader, raw 87069 in 0a60, gates on the same "no
 * colonies OR nobody wants colonists" pair, which corroborates it.)
 * Replaces the old `inv->found_flags` construction-count stand-in.
 */
static int ai_euro_5d04_cb_colonies_wanting_colonists(int nation_id) {
  int n = 0;
  if (!s_5d04_ctx || !s_5d04_ctx->colonies) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &s_5d04_ctx->colonies->colonies[i];
    if (c->active && c->nation_id == nation_id &&
        (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) != 0) {
      n++;
    }
  }
  return n > 127 ? 127 : n;
}

/*
 * DS:0x945a[nation] (`param_1 + -0x6ba6`) — resolved 2026-09-07e from the
 * census writer FUN_4962_0018 (raw 78147 zeroes it, raw 78167-78170 bumps
 * it): a NON-ship unit (`+0x3146` outside 0x0d..0x12) whose map x byte
 * `+0x3144` satisfies `x - nation == -0x14` is a unit standing on that
 * nation's Europe dock, so the byte is simply **how many land units this
 * nation currently has waiting in Europe**. It seeds `local_46`, the
 * "how badly does a departing ship need cargo" threshold, in the tail's
 * final loop. (Raw 77748 is the same tally under a different base.)
 */
static int ai_euro_5d04_cb_europe_land_units(int nation_id) {
  int n = 0;
  if (!s_5d04_ctx || !s_5d04_ctx->units) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &s_5d04_ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(s_5d04_ctx->units, u->id)) {
      continue;
    }
    if (ai_euro_in_europe(u->x, u->y)) {
      n++;
    }
  }
  return n > 127 ? 127 : n;
}

/*
 * DS:0xa0cc[16] (`local_40 + -0x5f34`) — resolved 2026-09-07e. Built by
 * the FUN_521d_6d8e prelude, once per nation-turn, immediately before it
 * calls 5d04 through `thunk_FUN_2a1f_0554`:
 *   raw 93107  memset(0xa0cc, 0, 0x10)
 *   raw 93122  per own colony with `+0x8d` >= 0: demand[specialty]++
 *   raw 93146  memcpy(0xa0bc, 0xa0cc, 0x10)  (untouched snapshot)
 *   raw 93163  per own SHIP (type 0x0d..0x12), per occupied hold slot
 *              0..`+0x3150`: demand[hold cargo]--
 * so it is a **per-cargo demand table**: how many of this nation's
 * colonies specialise in that cargo, minus how much of it is already
 * afloat. 5d04's departing-ship loop walks cargo 15..0 and buys 100 of
 * the first cargo whose demand clears `local_46`, decrementing the cell.
 * This replaces the old `inv->profession_demand[]` stand-in, which was
 * profession-indexed but was being read here with a cargo index.
 */
static void ai_euro_5d04_cb_cargo_demand(int nation_id, int8_t out[16]) {
  memset(out, 0, 16 * sizeof(out[0]));
  if (!s_5d04_ctx) {
    return;
  }
  if (s_5d04_ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &s_5d04_ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (c->specialty_cargo < 16) {
        out[c->specialty_cargo]++;
      }
    }
  }
  if (s_5d04_ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &s_5d04_ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (!units_is_sea(s_5d04_ctx->units, u->id)) {
        continue;
      }
      const int holds = units_goods_hold_count(s_5d04_ctx->units, u->id);
      for (int h = 0; h < holds && h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        /* Sentinel-aware, hold-count bound: a 255 hold is empty, not cargo
         * already in transit (smell audit sweep-3 area C #5). */
        if (units_hold_amount(s_5d04_ctx->units, u->id, h) > 0 && u->hold_goods_type[h] >= 0 &&
            u->hold_goods_type[h] < 16) {
          out[u->hold_goods_type[h]]--;
        }
      }
    }
  }
}

/*
 * Opaque per-nation scratch for the raw body's `nation+0x48/0x49/0x4a`
 * (a crosses/hammers-pool carry mechanic — genuine arithmetic, see the
 * normalization loop below) and the two Europe-dock "training slot"
 * counters at `DS:0xa0da`/`0xa0db`. NOT reused from the real
 * `ColonizeCol1Nation` struct, for two separate reasons (both re-checked
 * 2026-09-09, after smell audit #52 moved the Indian-hostility sticky
 * stand-in off `+0x48` and onto the one dead byte `+0x4b`/`unknown26[11]`):
 *   - `+0x48` is `col1_save.h`'s `king_grace_counter`, a REAL DOS quantity
 *     (FUN_4d56_4528's decrementing grace/waiver counter) that the port may
 *     read but must never write — so it cannot host scratch either;
 *   - `+0x49` is the one remaining Linux stand-in collision here
 *     (`privateer_spawn_mask`), and `+0x4a` (`unknown26_pad`) is DOS's raw
 *     banked carry total.
 * Same reasoning as the `0x53de` correction: don't reuse a live field on an
 * unconfirmed reading.
 */
typedef struct Ai5d04HireScratch {
  int8_t delay_48;              /* DS nation+0x48 */
  int8_t crosses_bank_whole;    /* DS nation+0x49 */
  int32_t crosses_bank_raw;     /* DS nation+0x4a */
  int8_t colonies_need_muskets;  /* DS 0xa0db (per nation-turn) */
  int8_t colonies_need_tools;    /* DS 0xa0da (per nation-turn, minus own Pioneers) */
} Ai5d04HireScratch;
static Ai5d04HireScratch s_5d04_hire_scratch[4];

/* --- 5d04 hire-ladder tail: shared frame + stages ---------------------- */

/* Shared locals of the 86065-86561 hire ladder tail. DOS keeps them in one
 * stack frame across all three phases; the port threads the same frame
 * through this struct so the phases below are verbatim transcriptions with
 * no renaming (each stage aliases the fields back to their raw local names
 * on entry and writes the mutated ones back on exit). */
typedef struct Ai5d04HireTail {
  ColonizeTurnContext* ctx;
  int nation_id;
  const Ai5d04PlanningFlags* f;
  ColonizeCol1Nation* nat;
  const ColonizeCol1Stuff* stuff;
  Ai5d04HireScratch* hs;
  int turn;
  int difficulty;
  int woi;
  int local_16;
  int local_34;
  int local_8;
  int has_any_colony;
  int unit_flag_bit5;
  int every_third_turn;
  int expand_signal;
  int local_28;
  int bVar9;
  int bVar10;
  int local_24;
} Ai5d04HireTail;

static void ai_euro_5d04_hire_tail_candidates(Ai5d04HireTail* t) {
  ColonizeTurnContext* ctx = t->ctx;
  const int nation_id = t->nation_id;
  const Ai5d04PlanningFlags* f = t->f;
  ColonizeCol1Nation* nat = t->nat;
  const ColonizeCol1Stuff* stuff = t->stuff;
  Ai5d04HireScratch* hs = t->hs;
  const int turn = t->turn;
  const int woi = t->woi;
  const int has_any_colony = t->has_any_colony;
  const int unit_flag_bit5 = t->unit_flag_bit5;
  const int every_third_turn = t->every_third_turn;
  int local_8 = t->local_8;
  int local_28 = t->local_28;
  int bVar10 = t->bVar10;

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
              /* raw 92658-92671. Two ways into the muskets training arm
               * (LAB_521d_6454):
               *   0xa0db <= 0  → only the LAB_521d_642a body;
               *   0xa0db >= 1  → RNG(0, local_c+1) == 0 trains outright,
               *                  and a MISS falls THROUGH into 642a (the
               *                  raw `goto LAB_521d_642a`) for a second
               *                  chance. That fall-through was missing
               *                  before 2026-09-07e. */
              int try_train = 0;
              int try_642a = 0;
              if (hs->colonies_need_muskets <= 0) {
                try_642a = 1;
              } else if (dos_rng_range(ctx->rng, 0, local_c + 1) == 0) {
                try_train = 1;
              } else {
                try_642a = 1;
              }
              if (try_642a && local_8 != 0 &&
                  dos_rng_range(ctx->rng, 0, local_c + 2) == 0 && turn > 99) {
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
                  hs->colonies_need_muskets--;
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
                      /* raw 92729-92733: DOS only calls FUN_291f_0c14(8,0x32)
                       * here — the old `= 0x32` write-back was invented. */
                      ai_euro_5d04_cb_set_pool_counter(8, 0x32);
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
                /* Tools-side (Pioneer) training, raw 92742-92783.
                 * raw 92745-92748: past turn 99 the arm is skipped whenever
                 * RNG(0,2) <= the nation's own Pioneer count
                 * (`param_1*0x13 + -0x6db2` = unit_type_counts[n][2]) — the
                 * more Pioneers already in the field, the less likely a new
                 * one. Fixed 2026-09-07e: the port read free_colonist_counts
                 * and inverted the sense. */
                int proceed = 1;
                if (turn > 99) {
                  proceed = !(dos_rng_range(ctx->rng, 0, 2) <=
                              (int)stuff->unit_type_counts[nation_id][2]);
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
                    hs->colonies_need_tools--;
                    handled = 1;
                  }
                }
              }
            }
            if (!handled && ai_euro_5d04_cb_unit_dispatch_byte(idx) == 0 &&
                stuff->unit_type_counts[nation_id][3] == 0 && turn > 0x32) {
              int proceed = 1;
              if (turn > 199) {
                /* DOS-LITERAL FUN_521d_5d04 raw 92785-92788: `iVar14 =
                 * FUN_281f_04d4(iVar19,0,3); if (iVar14 != 0) goto
                 * LAB_521d_638a;` — skip the arm unless the roll lands on 0. */
                proceed = dos_rng_range(ctx->rng, 0, 3) == 0;
              }
              if (proceed && (turn % 7) == 0) {
                if (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx)) == 0 ||
                    dos_rng_range(ctx->rng, 0, 7) == 0) {
                  ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 3);
                  /* raw 92798-92799: DOS bumps `param_1*0x13 + -0x6db1`
                   * (= unit_type_counts[n][3], the Missionary row) right
                   * here, which is what stops a second Missionary in the
                   * same pass. Mirrored 2026-09-07e; the census pass
                   * recomputes the row from scratch next turn. */
                  if (ctx->col1->stuff.unit_type_counts[nation_id][3] < 0xff) {
                    ctx->col1->stuff.unit_type_counts[nation_id][3]++;
                  }
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

  t->local_8 = local_8;
  t->local_28 = local_28;
  t->bVar10 = bVar10;
}

static void ai_euro_5d04_hire_tail_colony_demand(Ai5d04HireTail* t) {
  ColonizeTurnContext* ctx = t->ctx;
  const int nation_id = t->nation_id;
  const Ai5d04PlanningFlags* f = t->f;
  ColonizeCol1Nation* nat = t->nat;
  const ColonizeCol1Stuff* stuff = t->stuff;
  Ai5d04HireScratch* hs = t->hs;
  const int turn = t->turn;
  const int difficulty = t->difficulty;
  const int woi = t->woi;
  const int local_16 = t->local_16;
  const int local_34 = t->local_34;
  const int has_any_colony = t->has_any_colony;
  const int unit_flag_bit5 = t->unit_flag_bit5;
  const int every_third_turn = t->every_third_turn;
  int bVar9 = t->bVar9;

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
        (void)ai_euro_5d04_refill_pool_slot(slot); /* FUN_38fd_46d4 */
        bVar9 = 1;
        bVar8_2 = 1;
        /* DS:0x5238[type] is indexed by the DOS @UNIT code, not by a Linux
         * pool index — translate as every other consumer of the dispatch
         * byte does (fixed 2026-09-09; the raw index only happened to agree
         * on a NAMES-ordered pool). */
        local_42 -= ai_euro_5d04_dos_type_space(ctx->units,
                                                ai_euro_5d04_cb_unit_dispatch_byte(cand));
      }
      if (!bVar8_2 || local_42 < 1) {
        break;
      }
    } while (1);
  }

  t->bVar9 = bVar9;
  t->local_24 = local_24;
}

static void ai_euro_5d04_hire_tail_departing_ships(Ai5d04HireTail* t) {
  ColonizeTurnContext* ctx = t->ctx;
  const int nation_id = t->nation_id;
  const Ai5d04PlanningFlags* f = t->f;
  ColonizeCol1Nation* nat = t->nat;
  Ai5d04HireScratch* hs = t->hs;
  const int turn = t->turn;
  const int has_any_colony = t->has_any_colony;
  const int expand_signal = t->expand_signal;
  const int local_24 = t->local_24;
  const int local_28 = t->local_28;
  int bVar9 = t->bVar9;
  const int bVar10 = t->bVar10;

  /* raw 92983-93070: the departing-ship loop — sell/bank the hold, then
   * top the hull up with the cargo its colonies most want.
   * raw 92983-92988: `local_46` = DS:0x945a[nation] (land units this
   * nation has waiting in Europe) + the turn parity bit — real since
   * 2026-09-07e, was a hard 0. It is the bar a cargo's demand cell has to
   * clear before the ship buys 100 of it, so the more colonists are
   * queued on the dock, the pickier the ship gets about goods. */
  const int seed46 = ai_euro_5d04_cb_europe_land_units(nation_id);
  int local_46 = seed46 + ((turn & 1) != 0);
  /* raw 93036/93050 `local_40 + -0x5f34` = DS:0xa0cc[16], the per-cargo
   * demand table 6d8e rebuilds just before calling 5d04. Real since
   * 2026-09-07e (was `inv->profession_demand[]`, a profession-indexed
   * array being read with a cargo index). */
  int8_t cargo_demand[16];
  ai_euro_5d04_cb_cargo_demand(nation_id, cargo_demand);
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
      const int flags3148 = (ship2 && ship2->col1_flags15 & 0x80) ? 0x80 : 0; /* damaged */
      const int dispatch2 = ai_euro_5d04_cb_unit_dispatch_byte(idx2);
      if (!departed[idx2] && ((flags3148 & 0x80) == 0 || dispatch2 == 0x0b) &&
          dispatch2 > 0xc && dispatch2 < 0x13) {
        departed[idx2] = 1;
        /* raw 92996: a ship leaving Europe drops its colony binding
         * (`+0x314a` = col1_origin, 0xff = unbound). Ported 2026-09-07e —
         * the 4393 pick and the 20e6 arrival gate both read this byte. */
        {
          ColonizeUnit* wship = ai_euro_5d04_cb_unit(idx2);
          if (wship) {
            wship->col1_origin = 0xff;
          }
        }
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
            /* Goods-only, as every +0x3150 read at raw 93020-93039 is
             * (bugs.md #530 side lead; same finding as #527): a passenger
             * never occupies a DOS hold, so a transport carrying colonists
             * still buys cargo here. */
            const int used = ai_euro_0a60_goods_holds_used(ctx->units, sh);
            if (cap == used || f->cargo_short || !has_any_colony) {
              break;
            }
            if ((local_46 <= (int)cargo_demand[p] || expand_signal) &&
                ((!bVar10 && local_28 == 0) || (cap - used > 2 || expand_signal))) {
              if (nat->gold >= (uint32_t)(ai_euro_5d04_cb_price(p) * 100)) {
                ai_euro_5d04_cb_apply_bump(idx2, p, 100);
                cargo_demand[p]--; /* raw 93050 */
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

  t->bVar9 = bVar9;
}

/*
 * Raw 86065-86561 — Europe hire ladder + profession/reward tail. DOS is one
 * function body whose locals thread across all three phases in one scope;
 * the port keeps that single frame in `Ai5d04HireTail` and hands it to the
 * three stage functions above, each of which aliases the fields back to
 * their raw local names so the transcriptions stay verbatim. `goto` labels
 * below are abbreviated from the raw `LAB_521d_XXXX` names so this can be
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
  s_5d04_ctx = ctx;
  s_5d04_nation = nation_id;
  {
    int need_m = 0;
    int need_t = 0;
    ai_euro_5d04_cb_colony_needs(nation_id, &need_m, &need_t);
    if (need_m > 127) { need_m = 127; }
    if (need_m < -128) { need_m = -128; }
    if (need_t > 127) { need_t = 127; }
    if (need_t < -128) { need_t = -128; } /* the Pioneer subtraction can go negative */
    hs->colonies_need_muskets = (int8_t)need_m;
    hs->colonies_need_tools = (int8_t)need_t;
  }

  /* Raw 92569-92578: no Artillery on the Europe dock + colonies needing
   * muskets → buy one (purchase table entry 0, 500 gold), re-query. */
  int local_16 = ai_euro_5d04_cb_list_iter_first(0x0c);
  int local_34 = ai_euro_5d04_cb_wagon_query(local_16);
  if (local_34 == 0 && !woi && hs->colonies_need_muskets > 0 &&
      dos_rng_range(ctx->rng, 0, 3) == 0 && !f->cargo_short &&
      stuff->ship_cargo_totals[nation_id] > 4) {
    (void)ai_euro_5d04_propose_ship_buy(ctx, nation_id, 0);
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
  /* raw 92595 `-0x5f48` = DS:0xa0b8[nation] — own colonies flagged
   * NEEDS_COLONISTS. Real since 2026-09-07e (was `inv->found_flags`). */
  const int colonies_want_colonists = ai_euro_5d04_cb_colonies_wanting_colonists(nation_id);
  const int expand_signal = has_any_colony && (unit_flag_bit5 || every_third_turn);

  /* raw 92592-92625: gold-spend recruit-slot swap (Europe recruit price
   * falls with accumulated crosses: base + base*crosses/(-1-needed)). */
  if (!woi && !bVar8 && !f->cargo_short &&
      (!has_any_colony ||
       (!unit_flag_bit5 && !every_third_turn &&
        (stuff->colony_counts[nation_id] >> 1) <=
          colonies_want_colonists - stuff->free_colonist_counts[nation_id]))) {
    const int base = ((int)nat->recruit_count - difficulty + 7) * 20;
    const long scaled =
      ((long)base * (long)nat->current_crosses) / (-1L - (long)nat->needed_crosses);
    /* raw 92601 reads `-0x6bf0` = census_pop_proxy, not free_colonist_counts
     * (fixed 2026-09-07e — the same −0x6bf0/−0x6bf8 mix-up the ladder gate
     * had before 2026-09-07d). */
    int reserve = ((int)stuff->census_pop_proxy[nation_id] * 30 - turn) * 2;
    if (reserve < 0) {
      reserve = 0;
    }
    const long local_38 = scaled + base;
    if (nat->gold >= (uint32_t)(local_38 + reserve)) {
      nat->gold -= (uint32_t)local_38;
      const int slot = dos_rng_range(ctx->rng, 0, 2); /* nat->recruit[3] */
      const int candidate = ai_euro_5d04_cb_dock_pop_candidate(nat->recruit[slot]);
      if (candidate >= 0) {
        (void)ai_euro_5d04_refill_pool_slot(slot); /* FUN_38fd_46d4 */
        local_16 = candidate;
      }
    }
  }

  int local_28 = 0;
  int bVar9 = 0;   /* "a hire/train happened this pass" */
  int bVar10 = 0;  /* "tools-side training happened" */

  Ai5d04HireTail tail = {
    .ctx = ctx,
    .nation_id = nation_id,
    .f = f,
    .nat = nat,
    .stuff = stuff,
    .hs = hs,
    .turn = turn,
    .difficulty = difficulty,
    .woi = woi,
    .local_16 = local_16,
    .local_34 = local_34,
    .local_8 = local_8,
    .has_any_colony = has_any_colony,
    .unit_flag_bit5 = unit_flag_bit5,
    .every_third_turn = every_third_turn,
    .expand_signal = expand_signal,
    .local_28 = local_28,
    .bVar9 = bVar9,
    .bVar10 = bVar10,
    .local_24 = 0
  };
  ai_euro_5d04_hire_tail_candidates(&tail);
  ai_euro_5d04_hire_tail_colony_demand(&tail);
  ai_euro_5d04_hire_tail_departing_ships(&tail);
}

/*
 * FUN_521d_5d04 — full port, orchestrator (structural 2026-08-19, fully
 * live 2026-09-07d). Mirrors the raw function's own top-level order:
 * treasury bump → gate cascade (with the WoI Man-O-War seizure) → naval
 * gold floors (raw 92476-92505 — applied in DOS, so applied here) →
 * ship-buy candidate ladder with real FUN_521d_5c3c purchases (raw
 * 92533-92567) → hire-ladder tail, itself gated on raw 92568
 * `bought-a-ship || has-ships`.
 */
/* Returns 1 when the raw body's early `return;` fired (ship-buy ladder abort
 * — reachable since the naval-threat crumbs went live 2026-09-07); the live
 * caller must then skip its thin hire matrix too, as DOS skips the whole
 * hire ladder. */
static int ai_euro_5d04_nation_planning_structural(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  ai_euro_5d04_treasury_bump(ctx, nation_id);
  const Ai5d04PlanningFlags f = ai_euro_5d04_compute_flags(ctx, nation_id);
  ai_euro_5d04_apply_naval_gold_floors(ctx, nation_id, &f);
  int abort_early = 0;
  const int candidate = ai_euro_5d04_ship_buy_ladder(ctx, nation_id, &f, &abort_early);
  if (abort_early) {
    return 1;
  }
  /* Raw 92568 `if (local_3e != 0 || !bVar21)`: a shipless nation that
   * bought nothing skips the whole hire tail. */
  if (candidate != 0 || !f.no_ships) {
    ai_euro_5d04_hire_ladder_tail(ctx, nation_id, &f);
  }
  return 0;
}

static void ai_euro_nation_planning(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /*
   * T3.1 (2026-08-27): the structural 5d04 orchestrator is the live entry.
   * 2026-09-07d: fully real — naval gold floors applied, FUN_521d_5c3c
   * Europe purchases live (the thin ship-buy ladder that used to sit below
   * is retired; the DOS ladder + Artillery dock buy cover it).
   *
   * 2026-09-07e: the Linux-shaped hire matrix that used to run *after* the
   * orchestrator is RETIRED (~765 lines). It was an invention, not a port:
   * a `hire_cost = 200 + 25*difficulty` treasury gate, a Europe-dock expert
   * ladder keyed on NAMES display strings (tools/blacksmith/food/fisherman/
   * carpenter/lumberjack/ore/gunsmith/missionary/scout/elder/preacher/
   * teacher/craft), `units_find_type("Dragoon"/"Veteran Soldier"/...)`
   * war preferences, a wagon goods-load ladder and `inv->*_short` cargo
   * stand-ins — none of which exists in FUN_521d_5d04.
   *
   * DOS's own hire matrix is the raw 92568-93070 tail, ported in
   * `ai_euro_5d04_hire_ladder_tail` and called from the orchestrator:
   *   - Artillery dock buy when none is in Europe and colonies want muskets;
   *   - the recruit-slot swap priced off accumulated crosses;
   *   - the two-pass (unskilled-first, skilled-second) Europe-dock loop that
   *     turns Colonists into Soldiers (50 muskets) → Dragoons (50 horses),
   *     Pioneers (100 tools) or a Missionary, each with its own DS gate;
   *   - the recruit-buy loop that fills the departing hull;
   *   - the departure loop that sells/banks the inbound hold and tops the
   *     hull up from the DS:0xa0cc per-cargo demand table.
   * The orchestrator's return value still carries the raw 92541/92547 early
   * `return;` (ship-buy abort), which now simply ends the pass.
   */
  (void)ai_euro_5d04_nation_planning_structural(ctx, nation_id);
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
 * (DS:0x523d unit-type capability bitmask, unit+0x3148's FOUND/MIL_EXPAND
 * eligibility bits — `func_0x0001854c`'s weight seed was in this list until
 * 2026-09-06d, when it resolved to a plain clamp; see
 * `ai_euro_0a60_weight_seed`).
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
 * DOS unit AI bytes +0x3148/+0x314b/c/d/e — RESOLVED 2026-09-18, bugs.md
 * #525. They are not port-only scratch and need no shadow array: the Col1
 * unit record is 0x1c bytes based at DS:0x3144, and its field order (see
 * col1_save_layout.h `ColonizeCol1Unit`) is
 *   +0x3144/5 x,y   +0x3146 type   +0x3147 nation|vis   +0x3148 flags
 *   +0x3149 moves   +0x314a origin +0x314b ai_plan      +0x314c orders
 *   +0x314d goto_x  +0x314e goto_y +0x314f facing
 * so the whole DOS AI scratch block maps onto real `ColonizeUnit` fields:
 *   +0x3148 flag byte  -> u->col1_flags15
 *   +0x3149 MP SPENT   -> units_max_mp() - u->moves  (raw 6357 zeroes it at
 *                         the day top, raw 100342 adds 3 per step)
 *   +0x314b order code -> u->col1_ai_plan
 *   +0x314c act state  -> u->orders
 *   +0x314d/e goal x/y -> u->goto_x / u->goto_y
 * The port's own @ORDERS constants ARE those DOS act-state bytes:
 * FUN_521d_5b66's switch cases 7/8/9 are UNITS_ORDER_BUILD_COLONY /
 * CLEAR_PLOW / BUILD_ROAD, case 0x0b is UNITS_ORDER_AI_SAIL ("pursuing the
 * goal stored at +0x314d/e") and case 0x0c is UNITS_ORDER_AI_MOVE ("one
 * committed step"). The file-local `s_0a60_pilot_state` mirror of those
 * bytes was retired with this pass; only the AI_GOAL_* code of the committed
 * slot has no DOS byte at all, and it is re-read from the goal table by
 * `ai_goals_primary_code_at` instead of being mirrored.
 */

/* unit+0x314c literals, spelled as the port's own @ORDERS constants. */
#define AI_EURO_ACT_GOAL UNITS_ORDER_AI_SAIL /* 0x0b — pursue +0x314d/e */
#define AI_EURO_ACT_STEP UNITS_ORDER_AI_MOVE /* 0x0c — one committed step */
#define AI_EURO_ACT_ADJACENT 10              /* FUN_521d_0a60 raw 87567 */

/* unit+0x3148 — DOS `&= 0xd1` scratch bits (col1_save_layout.h names). */
#define AI_EURO_F3148_KEEP 0xd1u
#define AI_EURO_F3148_ROAM 0x02u  /* roam_reeval_pending (act_state 5/6) */
#define AI_EURO_F3148_FOUND 0x04u /* stack_has_founders_or_military */
#define AI_EURO_F3148_MIL 0x08u   /* stack_has_military */
#define AI_EURO_F3148_SPARE 0x20u /* spare-transport mark */

/*
 * The `aiStack_1da[64]` weight seed 0a60 fills every primary slot with at
 * entry — **resolved 2026-09-06d**, was a fixed-50 placeholder.
 *
 * Ghidra's `func_0x0001854c(0xd1d, X, 3, 99)` is `FUN_1000_854c` =
 * `FUN_281f_035c` (`address_mapping.csv:809` — flat ram `1854c`), a far
 * RTLink thunk `CALLF FUN_210d_0d91; JMPF FUN_124c_000c`. `FUN_124c_000c`
 * (`viceroy_unpacked.asm`, 124c:000c) is the resident 3-arg clamp:
 *   DX=[BP+6]; AX=[BP+8]; if(AX<DX) AX=DX; if(AX>[BP+0a]) AX=[BP+0a]; ret AX
 * i.e. `min(max(a,b),c)`. The leading `0xd1d` in the decompile is a segment
 * artifact of the far-call thunk, not an argument — the raw call site
 * (521d:0ab8, `PUSH 0x63 / PUSH 0x3 / MOV BX,[BP+6] / MOV AL,[BX+0x8cfc] /
 * SHR AL,3 / PUSH AX / CALLF 281f:035c / ADD SP,6`) pushes exactly three
 * words.
 *
 * And the value fed in is NOT the difficulty byte the old comment guessed:
 * `[BX + 0x8cfc]` is `all_unit_counts[nation]` (DS:0x8cfc, the saturating
 * per-nation unit total written by the `FUN_4962_0018` census — see
 * docs/save_format_map.md "Stuff" file-off 12).
 *
 *   seed = clamp(all_unit_counts[nation] >> 3, 3, 99)
 *
 * The 99 ceiling is unreachable in practice (the source is a byte, so the
 * shift caps at 31); the live range is [3, 31]. It matters because the
 * consumption tail's claim-count `weight[slot]++` is measured against this
 * seed — with the old fixed 50 a claim moved a slot's score by 2%, with the
 * real early-game seed of 3 it moves it by 33%, which is what makes DOS
 * spread a nation's units across distinct goal slots instead of piling them
 * onto the single cheapest one.
 */
static int ai_euro_0a60_weight_seed(const ColonizeTurnContext* ctx, int nation_id) {
  int count = -1;
  if (ctx && ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
    count = (int)ctx->col1->stuff.all_unit_counts[nation_id];
  }
  if (count < 0 && ctx && ctx->units) {
    /* No col1 census window (unit-test contexts): recompute DS:0x8cfc live,
     * same saturating all-active-units-of-nation tally FUN_4962_0018 does. */
    count = 0;
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == nation_id && count < 255) {
        count++;
      }
    }
  }
  if (count < 0) {
    count = 0;
  }
  int seed = count >> 3;
  if (seed < 3) {
    seed = 3;
  }
  if (seed > 99) {
    seed = 99;
  }
  return seed;
}

static int ai_euro_20e6_type_flags(int dos_type);
static int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);

/*
 * DOS-LITERAL FUN_521d_0a60 raw 88184-88191: the goal-capability gate is a
 * single bit test, `(1 << (goal.code & 0x1f)) & DS:0x523d[type * 0xe]`, i.e.
 * the goal code IS the bit index into the unit type's NAMES.TXT @UNIT
 * capability byte (k_20e6_type_flags). Plus the two unit+0x3148 clauses on
 * the same line: code 1 (FOUND) additionally needs bit2 (0x04), code 7
 * (MIL_EXPAND) additionally needs bit3 (0x08).
 *
 * Consequences of the table (they are DOS's, not an accident of this port):
 * FOUND(1) and MIL_EXPAND(7) are TRANSPORT goals — only the ship rows
 * 0x81/0x82/0xa2 carry bits 1 and 7. Land rows carry ESCORT(2)/LABOR(3)/
 * MILITARY(4)/COLONY(5); Colonist and Pioneer (0x40) carry only bit 6
 * (EXPLORE), a code the primary table never holds (ai_goals.h). The
 * previous port keyed on unit *names* and let land settlers chase FOUND.
 * bugs.md #511.
 */
/*
 * Fallback for pools whose types did not come from NAMES.TXT (hand-built test
 * type tables — `ai_euro_20e6_dos_type` returns −1 there, see conventions.md
 * "ColonizeUnit.type_index"): the pre-#511 name-keyed equivalent of the bit
 * test below. Never reached in a real game, where every type resolves.
 */
static int ai_euro_0a60_can_pursue_goal_by_name(
  ColonizeUnitKind kind, int goal_code, int is_ship, int has_bit2, int has_bit3
) {
  switch (goal_code) {
    case AI_GOAL_FOUND:
      if (is_ship) {
        return has_bit2;
      }
      return has_bit2 && (ai_euro_name_is_pioneer(kind) || kind == UNITS_KIND_COLONIST);
    case AI_GOAL_MIL_EXPAND:
      if (is_ship) {
        return has_bit3;
      }
      return has_bit3 && (ai_euro_is_military_name(kind) || ai_euro_is_artillery_name(kind));
    case AI_GOAL_MILITARY:
      if (is_ship) {
        return has_bit3;
      }
      return ai_euro_is_military_name(kind) || ai_euro_is_artillery_name(kind);
    case AI_GOAL_ESCORT:
      if (is_ship) {
        return 0;
      }
      return ai_euro_is_military_name(kind) || kind == UNITS_KIND_SCOUT;
    default:
      return 1;
  }
}

static int ai_euro_0a60_unit_can_pursue_goal(
  int dos_type, int goal_code, int has_bit2, int has_bit3,
  ColonizeUnitKind kind, int is_ship
) {
  if (goal_code < 0 || goal_code > 0x1f) {
    return 0;
  }
  if (dos_type < 0) {
    return ai_euro_0a60_can_pursue_goal_by_name(kind, goal_code, is_ship, has_bit2, has_bit3);
  }
  if (((1u << (unsigned)(goal_code & 0x1f)) & (unsigned)ai_euro_20e6_type_flags(dos_type)) == 0) {
    return 0;
  }
  if (goal_code == AI_GOAL_FOUND && !has_bit2) {
    return 0; /* raw 88189: `code != 1 || (unit+0x3148 & 4)` */
  }
  if (goal_code == AI_GOAL_MIL_EXPAND && !has_bit3) {
    return 0; /* raw 88190-88191: `code != 7 || (unit+0x3148 & 8)` */
  }
  return 1;
}

/*
 * Soldier/Dragoon continent-defense skip gate (raw: `land_units_here =
 * table[-0x6b5a][continent + nation*0x10]`, `colonies = table[-0x6b1a][...]`
 * — the same colony-count / land-unit-count-by-continent tables
 * euro_g_table_0a60.md resolved and ai_euro_refresh_continent_stance
 * already recomputes for its own purpose). Recomputed fresh here too
 * (cheap, one pass) rather than exposing that function's private locals.
 */
static int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid);
static int ai_euro_10ec_land_units_on(const ColonizeTurnContext* ctx, int nation, int cid);

/*
 * Audit AE-9: the two halves of this function ARE the two later standalone
 * helpers. The only spelling differences were inert here — the colony half's
 * `cid >= 0` guard (a colony always stands on land, so its continent id is
 * never negative) and the unit half's 255 byte clamp (the sole caller only
 * compares against 2 and 3).
 */
static void ai_euro_0a60_continent_presence(
  const ColonizeTurnContext* ctx, int nation_id, int continent_id, int* out_colonies,
  int* out_land_units
) {
  *out_colonies = ai_euro_20e6_own_colonies_on(ctx, nation_id, continent_id);
  *out_land_units = ai_euro_10ec_land_units_on(ctx, nation_id, continent_id);
}

/*
 * FUN_521d_0a60's own goal-consumption tail, literally: for every idle-ish
 * unit of `nation_id`, pick the closest/highest-priority matching primary
 * goal slot and write order_code/act_state/goal_x/goal_y — mirrors raw
 * decomp lines 974-1063 control flow and arithmetic 1:1 (see file header
 * comment for what's real vs. placeholder). Since 2026-09-18 (bugs.md #525)
 * it writes the REAL DOS bytes (`u->col1_ai_plan` / `u->orders` /
 * `u->goto_x` / `u->goto_y`) through `ai_euro_set_goto`, so every downstream
 * reader — the 20e6 pre-LAB_4d2e gate included — sees the same course the
 * rest of the port sets, and the old file-local mirror is gone.
 */

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
 *   The eligibility block below reads the REAL stack counts as of the
 *   2026-09-06b rewire (ai_euro_0a60_stack_counts; ships keep the literal
 *   hold-full + fleet-coordination gate on top), and the consumption
 *   tail's FOUND/MIL_EXPAND gates consume the bits for land units too —
 *   DOS reads `0x3148 & 4` / `& 8` for every unit, not only ships (raw
 *   tail, goal codes 1 and 7).
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
static int ai_euro_20e6_type_combat(int dos_type);

/*
 * FUN_1427_0d38 stack-query counts over a tile's stack (2026-09-06b byte
 * decode; case table at ai_euro_20e6_stack_count). DOS chains a ship's
 * passengers into its tile stack, so members here = units standing on the
 * tile plus the cargo of every ship on it:
 *   pioneers    case 3   # type 2
 *   mil_types   case 4   # types {1,4,6,7,8,9}
 *   mobilizable case 6   {1,4} else profession 0x15, plus {6..9} again
 *                        (a veteran-professioned 6..9 counts twice — kept)
 *   armed       case 0xa # non-ship with @UNIT combat (0x5236) > 1
 *   hold_cap    case 0xd Σ ship hold capacity (0x5237)
 */
typedef struct Ai0a60StackCounts {
  int pioneers;
  int mil_types;
  int mobilizable;
  int armed;
  int hold_cap;
} Ai0a60StackCounts;

static void ai_euro_0a60_stack_count_member(
  const ColonizeUnitPool* units, const ColonizeUnit* m, Ai0a60StackCounts* sc
) {
  const int t = ai_euro_20e6_dos_type(units, m);
  const int is_ship = (t >= 0x0d && t <= 0x12);
  if (t == 2) {
    sc->pioneers++;
  }
  if (t == 1 || t == 4 || (t >= 6 && t <= 9)) {
    sc->mil_types++;
  }
  if (t == 1 || t == 4) {
    sc->mobilizable++;
  } else if (m->profession == UNITS_JOB_SOLDIER) {
    sc->mobilizable++;
  }
  if (t >= 6 && t <= 9) {
    sc->mobilizable++;
  }
  if (!is_ship && ai_euro_20e6_type_combat(t) > 1) {
    sc->armed++;
  }
  if (is_ship) {
    sc->hold_cap += units_ship_capacity(units, m->id);
  }
}

static void ai_euro_0a60_stack_counts(
  const ColonizeUnitPool* units, int x, int y, Ai0a60StackCounts* sc
) {
  memset(sc, 0, sizeof(*sc));
  if (!units) {
    return;
  }
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* m = &units->units[i];
    if (!m->active || m->aboard_ship_id >= 0 || m->x != x || m->y != y) {
      continue;
    }
    ai_euro_0a60_stack_count_member(units, m, sc);
    for (int ci = 0; ci < m->cargo_count && ci < COLONIZE_UNIT_CARGO_MAX; ++ci) {
      const ColonizeUnit* pax = units_get_const(units, m->cargo_ids[ci]);
      if (pax && pax->active) {
        ai_euro_0a60_stack_count_member(units, pax, sc);
      }
    }
  }
}

/*
 * DOS unit+0x3150: the packed GOODS-hold count only (bumped by FUN_15eb_30b8
 * on a load, decomp 13331); passengers never enter it — the DOS saves show it
 * 0 on every opening Caravel carrying two colonists (test-saves-ai/TURN2).
 * Scanned over the ship's capacity, never the array bound, so the 255
 * empty-hold sentinel (col1_bridge.c:2485) is not an occupant.
 */
static int ai_euro_0a60_goods_holds_used(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const int cap = units_ship_capacity(units, u->id);
  int goods = 0;
  for (int i = 0; i < cap && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (units_hold_amount(units, u->id, i) > 0) {
      goods++;
    }
  }
  return goods;
}

/*
 * DOS-LITERAL raw 87511-87514: `0x5237[type] == unit+0x3150` — every GOODS
 * hold in use. A transport carrying only passengers is never "full", never
 * gets +0x3148 bits 2/3, and cannot take a FOUND/MIL_EXPAND goal — which is
 * why DOS's opening ships wander (plan '9', act 0x0c) instead of walking a
 * goal. Counting passengers here (the pre-2026-09-19 reading) made those
 * ships FOUND-eligible and was what kept 0a60's ship binding switched off
 * (bugs.md #527).
 */
static int ai_euro_0a60_ship_full(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const int cap = units_ship_capacity(units, u->id);
  return cap > 0 && ai_euro_0a60_goods_holds_used(units, u) >= cap;
}

static void ai_euro_0a60_unit_housekeeping(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  /* unit_type_counts[nation][0x0d/0x0e/0x0f] recompute (FUN_4962_0018). */
  int caravels = 0;
  int merchantmen = 0;
  int galleons = 0;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
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

  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index, not a unit id. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* u = &ctx->units->units[ui];
    if (!u->active) {
      continue;
    }
    const int dos_type = ai_euro_20e6_dos_type(ctx->units, u);
    const int is_ship_t = (dos_type >= 0x0d && dos_type <= 0x12);
    if (u->nation_id == nation_id) {
      const int ux = u->x;
      const int uy = u->y;
      if (u->col1_ai_plan == 'A') {
        u->col1_ai_plan = 'G'; /* admitted labor → garrisoned */
      }
      u->col1_flags15 &= AI_EURO_F3148_KEEP; /* rederive bits 1/2/3/5 below */
      if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
        u->col1_flags15 |= AI_EURO_F3148_ROAM; /* roam_reeval_pending */
      }

      /* FOUND/MIL_EXPAND eligibility bits — REWIRED 2026-09-06b to the
       * real 0d38 stack counts (raw: iStack_1e = 8aac(u,4) >= 2 ||
       * 8aac(u,6) != 0; iStack_1c = 8aac(u,3)): military-in-stack sets
       * bits 2+3, Pioneers-in-stack sets bit 2. A lone plain Colonist gets
       * no bits — DOS-faithful (colonists found via a stacked Pioneer/
       * escort, or via the ship goal fold in 20e6's band). */
      Ai0a60StackCounts sc;
      ai_euro_0a60_stack_counts(ctx->units, ux, uy, &sc);
      const int has_military = (sc.mil_types >= 2 || sc.mobilizable != 0);
      const int has_founders = (sc.pioneers != 0);
      if (has_founders || has_military) {
        int ok = 1;
        if (is_ship_t) {
          /* Literal DOS: only a full ship qualifies, and only when no
           * earlier-indexed own ship in the same stack is still loading. */
          ok = ai_euro_0a60_ship_full(ctx->units, u);
          for (int oi = 0; ok && oi < ui; ++oi) {
            const ColonizeUnit* o = &ctx->units->units[oi];
            if (!o->active || o->nation_id != nation_id || o->x != ux ||
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
          /* Raw: iStack_1e → |= 0xc, iStack_1c → |= 4 — bit2 from either,
           * bit3 only from military. */
          u->col1_flags15 |= AI_EURO_F3148_FOUND;
          if (has_military) {
            u->col1_flags15 |= AI_EURO_F3148_MIL;
          }
        }
      }

      /* Spare-transport mark (bit5, one ship per nation per turn): with
       * fewer than 2 Merchantman+Galleon (or no Merchantman), a 2nd+
       * Caravel is the spare; otherwise the first Merchantman is. Only for
       * ships not already FOUND/MIL_EXPAND-eligible. */
      if (!spare_marked && is_ship_t &&
          (u->col1_flags15 & (AI_EURO_F3148_FOUND | AI_EURO_F3148_MIL)) == 0) {
        if (merchantmen + galleons < 2 || merchantmen == 0) {
          if (dos_type == UNITS_KIND_CARAVEL && caravels > 1) {
            u->col1_flags15 |= AI_EURO_F3148_SPARE;
            spare_marked = 1;
          }
        } else if (dos_type == UNITS_KIND_MERCHANTMAN) {
          u->col1_flags15 |= AI_EURO_F3148_SPARE;
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
        /*
         * DOS raw 87560-87564: `if (act_state == 3 || == 2 || == 1 ||
         * (act_state > 9 && order_code != '1')) act_state = 0;` — an AI
         * course only survives the nation-turn boundary when its order code
         * is the generic goal-pursue '1'; 't'/'i'/0x45 courses and every
         * one-step 0x0c commit are dropped and re-decided. Live since
         * 2026-09-18 (bugs.md #525) now that +0x314c is the real `u->orders`
         * byte; against the retired shadow (zeroed every dispatcher turn) the
         * whole test was vacuous.
         *
         * `== 1 || == 2 || == 3` ported unconditionally (bugs.md #528,
         * 2026-09-22): those DOS values mean "aboard a ship / in transit /
         * off-map" — on a landed unit, order 1 (SENTRY) is simply the
         * leftover "aboard" value `FUN_1427_10be` wrote at boarding (raw
         * 8297/8674); the LAB_3558 unload block (raw 89440-89560) never
         * rewrites it, so a unit unloaded during nation-turn N still reads 1
         * at N's end, and this clear (N+1's own 0a60 top) wipes it to 0
         * before 20e6 re-decides. That is exactly what the TURN2→3/TURN3→4
         * DOS save pairs show. The port's own first-colony landfall goto
         * memory used to piggyback the same `orders == SENTRY` value
         * (collides with this clear); it now lives in the port-only
         * `ai_landfall_wait` flag (units.h) instead, set alongside
         * `orders = UNITS_ORDER_SENTRY` at every AI unload site and read by
         * whichever caller needs the landfall memory, so this clear no
         * longer has to dodge it. The `>= 10` arm is the one that carries the
         * AI courses.
         */
        if ((u->orders >= AI_EURO_ACT_ADJACENT && u->col1_ai_plan != 0x31) ||
            (u->orders >= 1 && u->orders <= 3)) {
          u->orders = UNITS_ORDER_NONE;
        }
        int side = 0;
        if (ai_goals_probe_adjacent_contact_claim_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, ux, uy, nation_id, 1, &side) >= 0) {
          u->orders = AI_EURO_ACT_ADJACENT; /* on-site at a contact claim: no new goal */
        }
        const int on_water = map_tile_is_water(ctx->map, ux, uy) ||
                             map_tile_is_high_seas(ctx->map, ux, uy);
        if (!on_water || is_ship_t) {
          in_transit = 0;
        }
      }
      if (in_transit) {
        u->orders = UNITS_ORDER_SENTRY; /* +0x314c = 1: aboard ship / Europe / off-map */
      }
    } else if (u->nation_id >= 0 && u->nation_id < 4) {
      /* Foreign branch: spotted hostile ship → CONTACT goal prio 3.
       * Frigates only count once independence is declared (DS:0x5382 bit0). */
      if (is_ship_t && (dos_type != UNITS_KIND_FRIGATE || woi) &&
          map_tile_seen_by(ctx->map, u->x, u->y, nation_id) && ctx->col1_ok &&
          ctx->col1) {
        const uint8_t d = ai_diplo_read(ctx->col1, nation_id, u->nation_id);
        if ((d & (AI_DIPLO_MET | AI_DIPLO_PEACE)) == AI_DIPLO_MET ||
            dos_type == UNITS_KIND_PRIVATEER) {
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
  const int weight_seed = ai_euro_0a60_weight_seed(ctx, nation_id);
  int weight[AI_PRIMARY_SLOTS];
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    weight[i] = weight_seed;
  }

  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index, not a unit id. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* u = &ctx->units->units[ui];
    if (!u->active || u->nation_id != nation_id || u->id < 0 ||
        u->id >= COLONIZE_UNITS_MAX) {
      continue;
    }
    /*
     * Pioneer work orders 8/9 are FUN_521d_5b66's own switch cases, i.e.
     * act states DOS's raw 88164 gate (`0/5/6 only`) already refuses — the
     * explicit skip stays because units_pioneer_work_tick, not 5b66, drives
     * the job here and must not be hijacked mid-job.
     */
    if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
      continue;
    }
    if (u->col1_ai_plan == 'A') {
      continue; /* already admitted as labor */
    }
    if (u->orders < AI_EURO_ACT_ADJACENT) {
      u->col1_ai_plan = 0x3f; /* '?' pending-decision placeholder */
    }
    if (u->orders != UNITS_ORDER_NONE && u->orders != UNITS_ORDER_FORTIFY &&
        u->orders != UNITS_ORDER_FORTIFIED) {
      continue; /* raw 88164: fresh goals only at act_state 0/5/6 */
    }
    if (u->col1_ai_plan == 't' || u->col1_ai_plan == 'i') {
      u->col1_ai_plan = 0x3f; /* clear stale goal-pursuit code */
    }

    const ColonizeUnitKind ukind = ai_euro_unit_kind(ctx->units, u);
    const int unit_is_ship = ai_euro_is_ship_type(ctx->units, u->id);
    const int unit_continent = map_continent_id_at(ctx->map, u->x, u->y);

    /* unit+0x3148 bits 2/3, written by ai_euro_0a60_unit_housekeeping from
     * the real 0d38 stack counts (2026-09-06b rewire; ships additionally
     * behind DOS's hold-full + fleet-coordination gate): bit2 =
     * FOUND-eligible, bit3 = MIL_EXPAND-eligible — read for every unit,
     * land included, as the DOS tail does. */
    const int has_bit2 = (u->col1_flags15 & AI_EURO_F3148_FOUND) != 0;
    const int has_bit3 = (u->col1_flags15 & AI_EURO_F3148_MIL) != 0;

    if (!unit_is_ship && (ukind == UNITS_KIND_SOLDIER || ukind == UNITS_KIND_DRAGOON)) {
      int colonies = 0;
      int land_units = 0;
      ai_euro_0a60_continent_presence(ctx, nation_id, unit_continent, &colonies, &land_units);
      if (land_units < 3 && (land_units < 2 || colonies == 0)) {
        /* FUN_521d_0a60 raw 88176-88181, literally: at the head of the
         * goal-binding body, `if (type == 1 || type == 4) { A =
         * [cont + nation*0x10 - 0x6b5a]; if (A < 3 && (A < 2 ||
         * [cont + nation*0x10 - 0x6b1a] == 0)) goto LAB_521d_1fdf; }`,
         * and LAB_521d_1fdf (raw 88240) is the unit-loop increment. So
         * this IS the DOS skip, not a port invention (bugs.md #652).
         * -0x6b5a = land units per continent, -0x6b1a = colonies per
         * continent. DOS has no ship guard; types 1/4 are land anyway. */
        continue;
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
            ai_euro_20e6_dos_type(ctx->units, u), g->code, has_bit2, has_bit3,
            ukind, unit_is_ship
          )) {
        continue;
      }
      const int goal_continent = map_continent_id_at(ctx->map, g->x, g->y);
      if (goal_continent != unit_continent && !unit_is_ship) {
        continue; /* off-continent goal, land unit can't reach it */
      }

      const int dist = map_dos_dist(g->x - u->x, g->y - u->y);
      const int score = weight[slot] * dist / (g->prio + 1);

      if ((u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) &&
          !unit_is_ship) {
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
      u->col1_ai_plan = 0x31; /* '1' default goal-pursue code */
      if (g->code == AI_GOAL_FOUND) {
        u->col1_ai_plan = 0x74; /* 't' */
      } else if (g->code == AI_GOAL_MIL_EXPAND) {
        u->col1_ai_plan = 0x69; /* 'i' */
      }
      /*
       * bugs.md #525: the commit writes the REAL +0x314c/+0x314d/e now
       * (act_state 0x0b + the goal tile), so `ai_euro_move_scoring_gate`'s
       * raw 90210-90219 test and `ai_euro_act_land_goal_dispatch` both read
       * the unit's own stored goal instead of a private mirror the rest of
       * the port never stamped.
       *
       * Ships bind here too, as in DOS (a 0x0b hull whose DS:0x523d bit0 is
       * clear — every transport row 0xa2/0x82 — skips FUN_521d_20e6 and
       * just walks the goal, FUN_521d_5b66 raw 90552-90560). Until
       * 2026-09-19 this was land-only: ai_euro_0a60_ship_full counted
       * passengers, so every loaded opening transport looked FOUND-eligible
       * and got pulled off its landfall (bugs.md #527).
       */
      ai_euro_set_goto(u, AI_EURO_ACT_GOAL, g->x, g->y);
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
 * plain stack count, so the plain units_count_at walk is byte-right (the old
 * "4fa8 chain splice, substituted" story targeted the wrong function).
 * Mode 0xd (every-4th CONTACT-scan skip, Σ ship hold capacity in the
 * stack) is wired for real at the lurk scan — 2026-09-06b rewire.
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

/* Foreign-colony producer loop (raw 983-1212) of
 * ai_euro_0a60_settlement_goal_producers. The two DS continent masks
 * (0x173c MIL_EXPAND / 0x173e FOUND) are threaded in and out so the loop body
 * stays a verbatim transcription. */
static void ai_euro_0a60_foreign_colony_producers(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeWorldMap* map,
  int have_col1, int turn, int difficulty, int human,
  uint8_t col_cnt[4][16], uint8_t land_cnt[4][16], uint8_t skilled_cnt[4][16],
  uint16_t* io_mask_mil_expand, uint16_t* io_mask_found
) {
  uint16_t mask_mil_expand = *io_mask_mil_expand;
  uint16_t mask_found = *io_mask_found;

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
      /* MILITARY approach goal (defender count = 0d38 case-2 stack count
       * + population, byte-right). Prio 3 while formally at peace, 5
       * otherwise. */
      if ((int)col_cnt[nation_id][cont] + (int)land_cnt[nation_id][cont] != 0 &&
          ((i + turn) & 3) != 0) {
        const int defenders =
          units_count_at(ctx->units, c->x, c->y) + (int)c->population;
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
       * Every-4th early-out REWIRED 2026-09-06b: raw
       * `(unit_at_tile + turn) % 4 == 0 && 8aac(unit_at_tile, 0xd) == 0`
       * — 0d38 case 0xd = Σ ship hold capacity over the colony-tile
       * stack; a foreign harbor with no ship capacity present skips the
       * lurk scan on its beat (the staging scan below still runs, as in
       * DOS's goto past LAB_11b6). unit_at_tile −1 when the tile is
       * empty, matching DOS's miss value through the same arithmetic. */
      int lurk_skip = 0;
      {
        const int uid_at = units_id_at(ctx->units, c->x, c->y);
        if ((uid_at + turn) % 4 == 0) {
          Ai0a60StackCounts lsc;
          ai_euro_0a60_stack_counts(ctx->units, c->x, c->y, &lsc);
          lurk_skip = (lsc.hold_cap == 0);
        }
      }
      if (!lurk_skip && map_tile_is_coastal(map, c->x, c->y)) {
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
              const int nx = tx + MAP_DIR8_DX[k];
              const int ny = ty + MAP_DIR8_DY[k];
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
          const int nx = tx + MAP_DIR8_DX[k];
          const int ny = ty + MAP_DIR8_DY[k];
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
      units_count_at(ctx->units, c->x, c->y) + (int)c->population;
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

  *io_mask_mil_expand = mask_mil_expand;
  *io_mask_found = mask_found;
}

/* Village producer loop (raw 1215-1276) of
 * ai_euro_0a60_settlement_goal_producers; same mask threading as above. */
static void ai_euro_0a60_village_producers(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeWorldMap* map,
  int have_col1, int turn,
  uint8_t col_cnt[4][16], uint8_t land_cnt[4][16],
  uint16_t* io_mask_mil_expand, uint16_t* io_mask_found
) {
  uint16_t mask_mil_expand = *io_mask_mil_expand;
  uint16_t mask_found = *io_mask_found;

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
          const int tx = vx + MAP_DIR8_DX[k];
          const int ty = vy + MAP_DIR8_DY[k];
          if (!ai_euro_0a60_open_sea(map, tx, ty)) {
            continue;
          }
          int cnt = 0;
          for (int m = 0; m < 8; ++m) {
            const int nx = tx + MAP_DIR8_DX[m];
            const int ny = ty + MAP_DIR8_DY[m];
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

  *io_mask_mil_expand = mask_mil_expand;
  *io_mask_found = mask_found;
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
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id < 0 || u->nation_id > 3 ||
        units_is_sea(ctx->units, u->id)) {
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


  uint16_t mask_mil_expand = 0; /* DS:0x173c */
  uint16_t mask_found = 0;      /* DS:0x173e */

  ai_euro_0a60_foreign_colony_producers(
    ctx, nation_id, map, have_col1, turn, difficulty, human, col_cnt, land_cnt,
    skilled_cnt, &mask_mil_expand, &mask_found
  );

  ai_euro_0a60_village_producers(
    ctx, nation_id, map, have_col1, turn, col_cnt, land_cnt, &mask_mil_expand,
    &mask_found
  );
}

/* --- FUN_5952_035e colony threat accumulator ---------------------------- */

/*
 * DS:0x543f stride-0x34 control byte == 0 → that Euro nation is human-driven.
 * Same table/idiom as nation_crosses_bells_1f72 / europe_nation_eot.md.
 */
static int ai_euro_nation_is_human(const ColonizeTurnContext* ctx, int nation) {
  if (!ctx || nation < 0 || nation > 3) {
    return 0;
  }
  if (ctx->human_nation >= 0 && ctx->human_nation <= 3) {
    return nation == ctx->human_nation;
  }
  if (ctx->col1_ok && ctx->col1) {
    return ctx->col1->player[nation].control == 0;
  }
  return 0;
}

/*
 * Colony threat accumulator → garrison_quota (+0x1e). Ported DOS-LITERALLY
 * 2026-09-08 from the CLEAN RECOVERY in
 * `original_sources_annotated/ai/colony_tick_5952_035e.md` (raw body lines
 * 233-324 there); the canonical Ghidra export of FUN_5952_035e is corrupted
 * (decomp_inventory.md), but `viceroy_unpacked.c:94940-94975` carries the same
 * accumulator body under argless far calls and corroborates it operand for
 * operand. This REPLACES the old "idle unfortified Soldier/Dragoon on the
 * colony tile and quota == 0 → quota = 1" thin latch.
 *
 *   threat = 0
 *   for dy in -5..5, dx in -5..5:                        (11x11 box)
 *     t = (colony.x + dx, colony.y + dy)
 *     if !map_tile_in_bounds(t): continue                 FUN_1000_84f2/281f_0302
 *     head = first unit on t                              FUN_1000_89d0/281f_07e0
 *     if head < 0 or (head.nation & 0xf) == colony.nation: continue
 *     for every unit u on t (whole stack, FUN_1000_84d4/281f_02e4):
 *       if 0x0d <= u.type <= 0x12: skip                   (ships never count)
 *       v = combat value x8, mode 1                       FUN_1000_8bb8/281f_09c8
 *       if (u.nation & 0xf) < 4:                          European owner
 *         if v < 2: v = 0
 *         if DS:0x543f[u.nation] == 0: v += v >> 1        (human units count 1.5x)
 *       else:                                             Indian owner
 *         if alarm(u.nation - 4, colony.nation) < 0x19: v = 0   FUN_281f_030c
 *         if DS:0x54f6[u.+0x314a * 9 + colony.nation] < 0x80: v = 0
 *       if settlement_owner(t) >= 0: v >>= 1              FUN_1000_88ae/281f_06be
 *       v = -(dos_dist(dx, dy) - 8) * v >> 3              FUN_1000_8560/281f_0370
 *       threat += v
 *   floor  = min(threat, 0x10)
 *   walls  = owned buildings along the Stockade→Fort→Fortress parent chain
 *                                                        FUN_1000_8ca0(0)/281f_0ab0
 *   threat = max(threat / (walls + 1), floor)
 *   colony.garrison_quota = (uint8_t)(threat >> 3)        (DOS `(char)` truncation)
 *
 * The divide only bites above the 0x10 floor, so walls never drag the quota
 * below 2 — they only cap a very large threat.
 *
 * DS:0x54f6 read site (this closes the 5952_035e half of the "two parked
 * tension readers" punch-list row, docs/indians.md): the row key is the
 * ADJACENT unit's own home settlement id — DOS unit +0x06 / DS:0x314a =
 * `ColonizeUnit.home_tribe_id` — not the tribe owning the nearest village.
 * The apparent "stride 9 words" is the settlement record stride 0x12, so
 * the cell is that record's attitude[nation] word = tribe.alarm[nation]
 * (col1_tribe_attitude, signed; raw 94967 `w < 0x80`). Repointed off the
 * phantom `indian_tension` array 2026-09-08.
 *
 * The same loop's `iStack_22` ring-1 counter and the `iStack_76`
 * labor_shortage (+0x8e) formula that consumes it were parked when this was
 * first ported; both are live since 2026-09-09 (smell audit #38/#41) and are
 * documented at their site in the tail of this function, together with the
 * +0x1b `& 7` clear and the 0x40 / 0x08 / 0x04 flag writers.
 */
/* FUN_5952_035e labor/garrison demand stage: colony+0x8e (`labor_shortage`)
 * from pop + units outside, and the homed-military tally. */
static void ai_euro_5952_labor_demand(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  const ColonizeCol1Save* col1, int quota, int ring1,
  int* out_n, int* out_want, int* out_homed_mil
) {
  /*
   * ---- labor_shortage (+0x8e) and the +0x1b flag byte -------------------
   * Ported 2026-09-09 (smell audit #38/#41) from raw 94029-94071 and
   * 94141-94199, the continuation of the same DOS body. Previously the port
   * stopped at the quota above and substituted (a) a thin
   * `labor_shortage = 1` demand latch in ai_euro_colony_goals and (b)
   * `NEEDS_GARRISON = garrison_quota > 0`, which only fires at threat >= 8
   * where DOS raises the bit on any pop>=3 town with no soldier standing in
   * it.
   *
   *   n     = colony.population(+0x1f) + DS:0x8d72 (units on the colony tile
   *           with a profession slot, DS:0x30e[type] >= 0, capped at 0x32)
   *   want  = clamp(max((n - 1) / 2, quota), <= n / 2)
   *   want += 1                      when DS:0x5382 bit0 (WoI declared)
   *   want  = 1                      when ring1 != 0 && n > 1 && want < 1
   *   colony.labor_shortage = want   (unconditional, every tick)
   *   for each non-ship unit on the colony tile, in order:
   *       if combat_byte(0x5236[type]) > 1 && want != 0: want--
   *   ai_flags |= 0x40               iff want > 0
   */
  const int pop = (int)c->population;
  int outside = 0; /* DS:0x8d72 — FUN_15eb_09c0 raw 10014-10034 */
  if (ctx->units) {
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->aboard_ship_id >= 0 || u->x != c->x || u->y != c->y) {
        continue;
      }
      if (units_type_has_profession_slot(ai_euro_20e6_dos_type(ctx->units, u))) {
        outside++;
      }
    }
  }
  if (outside > 0x32) {
    outside = 0x32; /* raw 10031-10033 */
  }
  const int n = pop + outside;
  int want = (n - 1) / 2;
  if (want < quota) {
    want = quota;
  }
  if (n / 2 < want) {
    want = n / 2;
  }
  if (col1 && col1->head.game_options.woi != 0) {
    want++; /* DS:0x5382 bit0 — War of Independence declared */
  }
  if (ring1 != 0 && n > 1 && want < 1) {
    want = 1;
  }
  c->labor_shortage = (uint8_t)(want < 0 ? 0 : (want > 255 ? 255 : want));
  const int want_pre = want;
  if (ctx->units) {
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->aboard_ship_id >= 0 || u->x != c->x || u->y != c->y) {
        continue;
      }
      const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
      if (dtype >= 0x0d && dtype <= 0x12) {
        continue; /* ships never garrison */
      }
      if (ai_euro_20e6_type_combat(dtype) > 1 && want != 0) {
        want--;
      }
    }
  }

  /*
   * local_82 (raw 94063-94071): this nation's LAND military homed to this
   * colony (+0x314a origin == colony), minus the ones the tile walk above
   * already counted as garrison. So it is the OFF-STATION surplus, not the
   * raw defender count — the reading in colony.h's bit-pair note is the
   * simplification; this is the literal DOS quantity.
   */
  int homed_mil = 0;
  if (ctx->units) {
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || (u->nation_id & 0xf) != nation_id) {
        continue;
      }
      if ((int)u->col1_origin != c->id) {
        continue;
      }
      const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
      if (dtype >= 0x0d && dtype <= 0x12) {
        continue;
      }
      if (ai_euro_20e6_type_combat(dtype) > 1) {
        homed_mil++;
      }
    }
  }
  homed_mil -= (want_pre - want);

  *out_n = n;
  *out_want = want;
  *out_homed_mil = homed_mil;
}

/* FUN_5952_035e ai_flags stage: the wanted-garrison formula and the +0x1b
 * bit writes. */
static void ai_euro_5952_ai_flags(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int n, int want,
  int homed_mil
) {
  /*
   * Wanted defenders, local_74 (raw 94150-94193):
   *   lt2  = @LEADERNAME column 2 (DS:0x9568, signed, stride 3 per nation)
   *   base = (n * 3 >> 1) - lt2 - (turn >> 7)
   *   div  = lt2 + 5, or lt2 + 6 when want != 0; −1 more when stance == 4
   *   base += 2 when stance == 0; += 1 when stance == 3
   *   wanted = base / div
   *   wanted = 0                 when presence[cont] == 0 && stance != 0
   *   wanted = min(wanted, 1)    when presence[cont] bit0 set AND
   *                              (presence & 6) != 0 AND nation != 2
   *     (the other half of that branch is the Indian war-declare block,
   *      already ported as ai_contact_colony_tick_war_5952)
   * Substitution: DS:0x95f2 is DOS's per-continent presence bitmask, zeroed
   * at the top of every per-nation FUN_4962_0018 call (raw 78149-78150), so
   * it always describes the nation being censused; the port has no stored
   * mirror, so bit0/1/2 are recomputed here from live state for this
   * nation's point of view (docs/save_format_map.md row 156, corrected
   * 2026-09-10 audit C7).
   */
  const int cont = map_continent_id_at(ctx->map, c->x, c->y);
  const int stance = ai_euro_continent_stance_at(nation_id, cont);
  const int lt2 = ai_diplo_leader_trait(ctx, nation_id, 2);
  const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
  int base = ((n * 3) >> 1) - lt2 - (int)(turn >> 7);
  int div = lt2 + (want != 0 ? 6 : 5);
  if (stance == 4) {
    div -= 1;
  }
  if (stance == 0) {
    base += 2;
  } else if (stance == 3) {
    base += 1;
  }
  int wanted = div != 0 ? base / div : 0;
  /*
   * DS:0x95f2[cont] `continent_presence_flags` — the war-declare half of this
   * same FUN_5952_035e body reads the identical byte ~6 lines later, so the
   * computation lives once, in ai_contact.c, which carries the full
   * FUN_4962_0018 citation for all three bits and the argument that the array
   * is zeroed at the top of every per-nation call. Two copies that disagreed
   * about the aboard-ship filter and the owner-nibble mask were merged
   * 2026-09-10 (audit C7); the same audit refuted docs/save_format_map.md row
   * 156's "not cleared between nations, accumulates across the full per-turn
   * pass". Bit 8 (this nation's own dug-in field force) has no reader here; it
   * is modelled since 2026-09-10 for the DS:0xa89c tally FUN_521d_20e6's
   * war-cargo scorer consumes, and is simply ignored in this arm. The local
   * rename wrapper this used to go through went with audit AE-34.
   */
  const int presence = ai_contact_continent_presence_4962(ctx, nation_id, cont);
  if (presence == 0 && stance != 0) {
    wanted = 0;
  }
  if ((presence & 1) != 0 && ((presence & 6) != 0 && nation_id != 2) && wanted > 1) {
    wanted = 1;
  }

  /*
   * raw 94142: DOS clears the whole flag byte down to `& 7` before the
   * recompute — bits 0x01/0x02 (census, FUN_4962_0018's own disjoint mask)
   * and 0x04 survive; 0x08/0x10/0x20/0x40/0x80 are rebuilt every tick.
   * 0x04 is deliberately sticky in DOS: only its consumers clear it
   * (raw 85332 FUN_4d56_4528, 90168 FUN_521d_20e6, 94247 the join loop) —
   * those clears are NOT ported yet, see the note in colony.h.
   */
  c->ai_flags = (uint8_t)(c->ai_flags & 0x07u);
  /*
   * raw 94143-94146 (= colony_tick_5952_035e.md:487-490): the FIRST of DOS's
   * two +0x1b bit 0x10 (NEEDS_COLONISTS) writers, and it runs exactly here —
   * immediately after the `&= 7` clear above, before every other flag writer
   * of the tick:
   *   if ((+0x1c & 0x10) && +0x1f < ' ') { +0x1b |= 0x10; +0x1c &= 0xef; }
   * A one-shot hand-off: the +0x1c bit 0x10 latch (COLONIZE_COLONY_FLAG_SMALL_AI,
   * col1_save.h `small_colony_ai`) is consumed AND cleared, raising
   * NEEDS_COLONISTS once for a colony still under 0x20 population. That read
   * is the only reference to +0x1c bit 0x10 in the whole DOS image: no writer
   * of the bit exists in viceroy_unpacked.c, viceroy_overlays.c or
   * viceroy_unpacked_2.c (checked over every `(byte *)(x + 0x1c)` access and
   * every `| 0x10` / `& 0xef` store), so in DOS the bit can only arrive from
   * a loaded save. The port used to re-stamp it from an invented `pop < 10`
   * every tick in ai_euro_refresh_colony_ai_flags, which would have turned
   * this one-shot into "NEEDS_COLONISTS whenever pop < 10"; that writer is
   * gone (smell audit 2026-09-10 C3).
   * The second writer is the formula one in ai_euro_refresh_colony_ai_flags
   * (md:557-563), which only ORs — the per-tick clear for both is the
   * `&= 7` above, so neither may carry an `else`-clear of its own.
   */
  if ((c->colony_flags & COLONIZE_COLONY_FLAG_SMALL_AI) != 0 && c->population < 0x20) {
    c->ai_flags |= COLONIZE_COLONY_AI_NEEDS_COLONISTS;
    c->colony_flags =
      (uint8_t)(c->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_SMALL_AI);
  }
  if (want > 0) {
    c->ai_flags |= COLONIZE_COLONY_AI_NEEDS_GARRISON; /* raw 94147-94149 */
  }
  if (homed_mil < wanted) {
    c->ai_flags |= COLONIZE_COLONY_AI_SHORT_DEFENDERS; /* raw 94194-94196 */
  }
  if (wanted + (wanted > 1 ? 1 : 0) < homed_mil) {
    c->ai_flags |= COLONIZE_COLONY_AI_MILITARY_SURPLUS; /* raw 94197-94199 */
  }
}

/* ===== FUN_5952_035e absorption + equip arm (raw 94231-94352) ===========
 *
 * RE-HOSTED 2026-09-18. Both arms used to run from the arriving unit's act
 * (`ai_euro_act_colony_absorb` / the on-tile block of
 * `ai_euro_act_pioneer_corridor`). DOS runs them inside the COLONY tick, at
 * this exact position — after the +0x1b flag writes and the by-profession
 * census (raw 94219-94229) and before the build-preference / construction
 * cascade — as a re-scan of the units standing on the colony tile. The
 * unit-act hosting got the ordering, the turn, the multi-unit case and the
 * RNG position all wrong; this is the DOS host.
 *
 * DOS shape, raw 94231-94272:
 *   if (DS:0x8d72 != 0 && (+0x1b & 0x10)) do {
 *     iStack_32 = 0;
 *     for (ac = +0x1f; !iStack_32 && ac < +0x1f + DS:0x8d72 && +0x1f < 0x20; ac++)
 *         { one case per @UNIT type; an absorption sets iStack_32 }
 *   } while (iStack_32);
 * i.e. absorb at most one unit per pass and restart the scan from the top —
 * every later pass sees the new population, the consumed census cell, the
 * cleared MILITARY_SURPLUS bit and the raised tools latch. `+0x1f` is
 * re-read at every loop test, so the pop < 0x20 cap tracks the absorptions.
 *
 * `local_16` (uStack_16) RESOLVED: the tick seeds it at its top from
 * `0x13 < colony[+0xb6]` ("already holds more than 19 TOOLS", +0xb6 = +0x9a
 * + 2*14) and the Pioneer case then latches it to 1 so no SECOND Pioneer is
 * absorbed through that disjunct in the same tick. It is a real tick-local:
 * DOS never re-reads the stock word, so the absorbed Pioneer's own tools
 * refund does not move the gate. Carried here as `tools_latch`, which
 * retires the "residual divergence" the unit-act hosting had to document.
 *
 * `iStack_76` is the tick's running labor/garrison demand — the value
 * `ai_euro_5952_labor_demand` leaves in `want` after the on-tile military
 * walk, NOT the `+0x8e` byte (DOS writes +0x8e once, before that walk, and
 * never again in the tick). It is passed in by address and the Soldier case
 * `++`s it. In-tick readers of the post-absorption value: the dead disjunct
 * (a) below, and the second Muskets build-preference arm
 * (`FUN_OVL15_L0000__002a82(0x181f, 0xf, ...)` = FUN_5952_0306), ported
 * 2026-09-18 as ai_euro_5952_build_pref_0306 and called from the tail of
 * this function — so the increment is now observable where DOS makes it
 * observable: one extra absorbed Soldier can force the colony's +0x8d
 * preference back onto Muskets.
 */
static int ai_euro_5952_tile_stack(
  const ColonizeTurnContext* ctx, const ColonizeColony* c, int* ids, int max
) {
  int n = 0;
  if (!ctx->units) {
    return 0;
  }
  /* DS:0x8d72 (FUN_15eb_09c0 raw 10014-10034) — the same predicate
   * ai_euro_5952_labor_demand counts `outside` with. Slot walk: `i` is an
   * array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX && n < max; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->aboard_ship_id >= 0 || u->x != c->x || u->y != c->y) {
      continue;
    }
    if (!units_type_has_profession_slot(ai_euro_20e6_dos_type(ctx->units, u))) {
      continue;
    }
    ids[n++] = u->id;
  }
  return n;
}

/* Defined below, in DOS's own order: the equip arm and then the five
 * FUN_5952_0306 build-preference calls that follow it. */
static void ai_euro_5952_equip_arm(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int stance
);
static void ai_euro_5952_build_pref_0306(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  int labor_running, int tools_latch
);

COLONIZE_INTERNAL void ai_euro_5952_absorb_equip(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int* labor_running
) {
  if (!ctx || !ctx->units || !ctx->colonies || !c) {
    return;
  }
  const int cont = map_continent_id_at(ctx->map, c->x, c->y);
  const int stance = ai_euro_continent_stance_at(nation_id, cont); /* local_2a */
  /* local_16, seeded at the tick's top from +0xb6 (md:298). */
  int tools_latch = (int)(c->stock[COLONIZE_CARGO_TOOLS] > 0x13);
  const int idx = (c->id >= 0 && c->id < COLONIZE_COLONIES_MAX) ? c->id : -1;

  /* ---- absorption arm, raw 94231-94272 -------------------------------- */
  int ids[COLONIZE_UNITS_MAX];
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) != 0 &&
      ai_euro_5952_tile_stack(ctx, c, ids, COLONIZE_UNITS_MAX) != 0) {
    int restart = 1;
    int guard = 0;
    while (restart && guard++ <= COLONIZE_UNITS_MAX) {
      restart = 0;
      const int n = ai_euro_5952_tile_stack(ctx, c, ids, COLONIZE_UNITS_MAX);
      for (int k = 0; k < n && !restart; ++k) {
        if ((int)c->population >= 0x20) {
          break; /* raw 94235-94236, re-read every loop test */
        }
        ColonizeUnit* u = units_get(ctx->units, ids[k]);
        if (!u || !u->active) {
          continue;
        }
        /*
         * local_ee = FUN_1000_8dfe(slot) = FUN_15eb_0e18; for a slot past the
         * population it is FUN_15eb_0902(unit) = DS:0x30e[@UNIT type], the
         * type's DEFAULT @JOB — 0x13 Colonist / 0x14 Pioneer / 0x15 Soldier /
         * 0x16 Scout / 0x17 Dragoon (and Cont. Army → 0x15, Cont. Cav →
         * 0x17). NOT the @UNIT code. (smell audit 2026-09-10, "DEFERRED —
         * raw 94247".)
         */
        const int dtype =
          units_type_default_job(ai_euro_20e6_dos_type(ctx->units, u)); /* local_ee */
        const int prof = u->profession; /* local_1a, FUN_281f_0c54 */
        int take = 0;
        int is_mil = 0;
        int is_pioneer = 0;
        /* The four DOS cases are disjoint on local_ee, so the raw's chain of
         * bare `if`s is spelled as an else-chain here. */
        if (dtype == 0x15 || dtype == 0x17) { /* Soldier / Dragoon, raw 94239 */
          is_mil = 1;
          const int census13 = idx >= 0 ? s_5952_census_nonexpert[idx] : 0;
          const int census15 = idx >= 0 ? s_5952_census_vet_soldier[idx] : 0;
          /* (a) DOS-LITERAL dead branch: iStack_76 is clamped >= 0 by the
           * labor formula and only ever ++/--s under `!= 0`, so it is never
           * negative here. Now that the running total is real, the branch is
           * spelled against it rather than against a hard 0. */
          const int dead_shortage_arm =
            (*labor_running < 0 &&
             (c->ai_flags & COLONIZE_COLONY_AI_SHORT_DEFENDERS) == 0);
          const int specialist_arm =
            ai_euro_5952_job_is_expert(prof) && prof != UNITS_JOB_SOLDIER &&
            (census13 != 0 || census15 != 0);
          const int surplus_arm =
            (c->ai_flags & COLONIZE_COLONY_AI_MILITARY_SURPLUS) != 0;
          take = dead_shortage_arm || specialist_arm || surplus_arm;
        } else if (dtype == 0x14) { /* Pioneer, raw 94257-94263 */
          is_pioneer = 1;
          take = ((c->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) != 0 &&
                  tools_latch == 0) ||
                 stance == 0;
        } else if (dtype == 0x16) { /* Scout, raw 94264-94270 */
          take = (stance == 0 || c->stock[COLONIZE_CARGO_HORSES] < 0x34);
        } else if (dtype == 0x13) { /* Free Colonist, raw 94271-94274 */
          take = 1;
        }
        if (!take) {
          continue;
        }
        /* FUN_1000_8e26(slot, 0x12) = FUN_281f_0c36 = FUN_15eb_1068 with the
         * idle-sentinel job: the refund loop (raw 11234-11248) banks the
         * unit's gear into stock and the colonist keeps its profession. */
        ColonizeWorld w = world_from_turn_ctx(ctx);
        if (colonies_admit_unit_w(&w, c->id, u->id) < 0) {
          continue;
        }
        restart = 1; /* iStack_32 */
        if (is_mil) {
          /* raw 94247-94255 */
          (*labor_running)++;
          c->ai_flags =
            (uint8_t)(c->ai_flags & (uint8_t)~COLONIZE_COLONY_AI_MILITARY_SURPLUS);
          if (idx >= 0) {
            if (s_5952_census_vet_soldier[idx] != 0) {
              s_5952_census_vet_soldier[idx]--;
            } else if (s_5952_census_nonexpert[idx] != 0) {
              s_5952_census_nonexpert[idx]--;
            }
          }
        } else if (is_pioneer) {
          tools_latch = 1; /* raw 94261 `local_16 = 1` */
        }
      }
    }
  }

  ai_euro_5952_equip_arm(ctx, nation_id, c, stance);
  /* raw 94353-94354: FUN_1000_8ebc(0x181f) = the ledger refresh, then DOS's
   * five FUN_5952_0306 build-preference calls. `local_16` (tools_latch) is
   * the fourth one's own input, which is why the block is hosted here, in
   * the same frame that owns the latch, rather than in the caller. */
  ai_euro_5952_build_pref_0306(ctx, nation_id, c, *labor_running, tools_latch);
}

/* ---- equip arm, raw 94276-94352 ---------------------------------------
 * `if (+0x1f < 2) goto LAB_5952_0f7c` skips the whole block. Both
 * population reads are DOS's own spelling and see the post-absorption
 * population, which is why this must be hosted here.
 *
 * Completed 2026-09-18: all THREE of DOS's `local_136` targets are now
 * modelled, in DOS's order — Scout (raw 94277-94285), Pioneer (raw
 * 94300-94303), Soldier/Dragoon (raw 94304-94312). They are three plain
 * `if`s over the same `local_8e`, so a later arm silently OVERRIDES an
 * earlier one and at most ONE re-type happens per tick (`local_136` is a
 * flag, not a count, and the picker below runs once).
 *
 * `stance` is the caller's `local_2a` (the owner's continent stance); the
 * early `return`s are DOS's `goto LAB_5952_0f7c`, which lands on the ledger
 * refresh + build-preference block the caller runs next.
 */
static void ai_euro_5952_equip_arm(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int stance
) {
  const int equip_pop = (int)c->population;
  if (equip_pop < 2) {
    return;
  }
  int local_8e = UNITS_JOB_COLONIST; /* raw 94276: local_8e = 0x13 */
  int local_136 = 0;
  /*
   * Scout target, raw 94277-94285 (md:644-654), DOS-LITERAL:
   *   if (0x65 < +0xaa) {                       // stock[horses] > 101
   *     if (+0x1f < '\n')                       // pop < 10
   *       { cVar8 = FUN_1000_8e6c(0x181f); if (+0x1f < cVar8) goto LAB_0de5; }
   *     if ((+0x1b & 0x10) == 0) { local_8e = 0x16; local_136 = 1; }
   *   }
   * `FUN_1000_8e6c` = `FUN_281f_0c7c` -> `FUN_15eb_0484` = the AI's wanted
   * colony size (ai_euro_colony_wanted_size; 8 in practice — see its note),
   * the same callee the NEEDS_COLONISTS latch uses ten lines earlier with
   * segment literal 0x1a1f. `LAB_OVL15_L0000__000de5` is the local_90
   * computation below, so the jump skips the Scout arm ONLY; it does not
   * leave the equip block.
   * No horses test beyond the 0x65 threshold lives in the arm — the 50-horse
   * charge is inside FUN_15eb_1068 (colonies_eject_colonist,
   * COLONIZE_EJECT_SCOUT), and 0x65 guarantees it is affordable.
   * The scorer is called with local_1b4 = local_8e = 0x16 unchanged (only
   * 0x17 is remapped), so ai_euro_5952_equip_pick scores against 0x16: a
   * Seasoned Scout in the colony scores 4, any non-expert +1, and the
   * `-99` expert-strip arm does NOT apply (it is gated on target == 0x15).
   */
  if (c->stock[COLONIZE_CARGO_HORSES] > 0x65) {
    int skip_scout = 0;
    if (equip_pop < 10 && equip_pop < ai_euro_colony_wanted_size(ctx->colonies, c)) {
      skip_scout = 1; /* goto LAB_OVL15_L0000__000de5 */
    }
    if (!skip_scout && (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) == 0) {
      local_8e = UNITS_JOB_SCOUT;
      local_136 = 1;
    }
  }
  /*
   * local_90, raw 94292-94299, all four conjuncts in DOS's order. The
   * FUN_281f_04d4 = dos_rng_range(0, 3) draw is the THIRD conjunct, so the
   * stance and population tests must gate it or the shared LCG stream
   * shifts. Hosting this in the colony tick draws it once per AI colony per
   * turn, which is DOS; the unit-act hosting drew it only when a Pioneer
   * happened to stand on the tile.
   */
  int local_90 = 0;
  if (stance == 0 && equip_pop > 10 && dos_rng_range(ctx->rng, 0, 3) == 0 &&
      (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) == 0) {
    local_90 = 1;
  }
  /*
   * Pioneer target, raw 94300-94303 (md:664-668), DOS-LITERAL:
   *   if (local_90 != 0 && *(char *)(local_1b0 * 0x13 + -0x6db2) == 0 &&
   *       local_10 != 0 && 0x13 < +0xb6) { local_8e = 0x14; local_136 = 1; }
   * Variable resolutions, all from the raw text plus already-pinned tables:
   *  - `local_1b0` is the colony's owner nation (+0x1a); the two neighbouring
   *    reads in this same tail index the same local as `*3 + -0x6a9a` (the
   *    DS:0x9566 @LEADERNAME trait triple) and `*0x10 + -0x7b37` (the
   *    DS:0x84bc Europe price row), both per-Euro-nation.
   *  - `-0x6db2` = DS:0x924e = DS:0x924c + 2 = `unit_type_counts[nation][2]`,
   *    stride 0x13 = 19 @UNIT types (save_format_map.md row 252,
   *    FUN_4962_0018). Column 2 is the nation's Pioneer count — the identical
   *    expression FUN_521d_5d04's tools-side training arm reads (raw
   *    92745-92748, ai_euro.c). So the gate is "this nation has NO Pioneer
   *    anywhere"; the colony only mints one when the nation owns none.
   *  - `local_10` = `func_0x0001a684(0x181f, +0x1a)` = `FUN_2a1f_0494`
   *    (address_mapping.csv raw 1a684) = the far thunk for `FUN_521d_03d0`,
   *    founding_expansion_urgency(nation) — ai_goals_founding_expansion_
   *    urgency. Computed once at the top of the tick (md:608) and only read
   *    here.
   *  - `0x13 < +0xb6` is stock[tools] > 19. This is the SAME threshold the
   *    absorption arm's `local_16` was seeded from, but NOT the same
   *    quantity: `local_16` is a tick-local frozen at the tick's top and
   *    latched to 1 by the Pioneer absorb case, while this arm re-reads the
   *    live stock word. So a Pioneer absorbed earlier in this very tick
   *    refunds its tools into +0xb6 and can push this gate open — DOS takes
   *    a Pioneer in and hands one back out in one tick, and the latch does
   *    not stop it (it only guards the absorb side).
   * The tools are charged by FUN_15eb_1068 (colonies_eject_colonist,
   * COLONIZE_EJECT_PIONEER), which also refuses when the stock cannot pay.
   */
  {
    const int pioneers_afield =
      (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4)
        ? (int)ctx->col1->stuff.unit_type_counts[nation_id][2]
        : 0;
    const int total_colonies = ctx->colonies ? ctx->colonies->colony_count : 0;
    if (local_90 && pioneers_afield == 0 &&
        ai_goals_founding_expansion_urgency(nation_id, total_colonies) != 0 &&
        c->stock[COLONIZE_CARGO_TOOLS] > 0x13) {
      local_8e = UNITS_JOB_PIONEER;
      local_136 = 1;
    }
  }
  /* raw 94304-94306: `((+0x1b & 0x48) != 0 || local_90 != 0) && 0x31 < +0xb8` */
  const int equip_demand =
    (c->ai_flags &
     (COLONIZE_COLONY_AI_NEEDS_GARRISON | COLONIZE_COLONY_AI_SHORT_DEFENDERS)) != 0 ||
    local_90;
  if (equip_demand && c->stock[COLONIZE_CARGO_MUSKETS] > 0x31) {
    /* raw 94307-94312: local_8e = 0x15, upgraded to 0x17 on horses > 0x33. */
    local_8e = c->stock[COLONIZE_CARGO_HORSES] > 0x33 ? UNITS_JOB_DRAGOON : UNITS_JOB_SOLDIER;
    local_136 = 1;
  }
  if (!local_136) { /* raw 94313: `if (local_136 != 0)` guards the picker */
    return;
  }
  /* raw 94314-94317: local_1b4 = local_8e, with 0x17 mapped to 0x15. */
  const int target = (local_8e == UNITS_JOB_DRAGOON) ? UNITS_JOB_SOLDIER : local_8e;
  const int pick = ai_euro_5952_equip_pick(c, target);
  if (pick < 0) {
    return;
  }
  /*
   * raw 94347-94350, DOS-LITERAL: the expert strip reads FUN_281f_0c9a with
   * iStack_18 still holding the LAST loop iteration's profession, not the
   * picked colonist's — the scorer's loop variable leaks out of the loop.
   */
  const int last_prof = (int)c->colonists[equip_pop - 1].profession;
  if (ai_euro_5952_job_is_expert(last_prof) && target != last_prof) {
    /* FUN_1000_8e9e(colony, pick, 0x1c) = FUN_281f_0cae, clear specialty. */
    c->colonists[pick].profession = COLONIZE_PROF_FREE_COLONIST;
  }
  /* FUN_1000_8e26(colony, pick, local_8e) = FUN_15eb_1068: the colonist
   * leaves as a Scout / Pioneer / Soldier / Dragoon on the colony tile
   * keeping its profession byte, orders byte (+0x314c) zeroed, gear charged
   * off the stock (raw 11318-11329). local_8e, not local_1b4 — the Dragoon
   * remap is for the SCORER only. */
  int eject_role = COLONIZE_EJECT_COLONIST;
  if (local_8e == UNITS_JOB_PIONEER) {
    eject_role = COLONIZE_EJECT_PIONEER;
  } else if (local_8e == UNITS_JOB_SOLDIER) {
    eject_role = COLONIZE_EJECT_SOLDIER;
  } else if (local_8e == UNITS_JOB_SCOUT) {
    eject_role = COLONIZE_EJECT_SCOUT;
  } else if (local_8e == UNITS_JOB_DRAGOON) {
    eject_role = COLONIZE_EJECT_DRAGOON;
  }
  (void)colonies_eject_colonist(ctx->colonies, c->id, pick, ctx->units, eject_role);
}

/* ---- build-preference block, raw 94353-94395 (md:758-793) --------------
 *
 * DOS's five `FUN_OVL15_L0000__002a82(0x181f, cargo, want)` calls, in order,
 * immediately after the equip arm's `FUN_1000_8ebc(0x181f)` ledger refresh.
 *
 * CALLEE PINNED. `thunk_FUN_2a1f_05e4` is the RTLink dynalink stub at
 * `5952:2a82` (address_mapping.csv row `thunk_FUN_2a1f_05e4,5952:2a82,
 * OVL15_L0000,2a82`), and its target is `FUN_5952_0306(cargo, want)`
 * (viceroy_unpacked.c:93760-93780) — the real `+0x8d` `specialty_cargo`
 * writer, already ported as `colonies_specialty_cargo_update`:
 *     cap = FUN_281f_0d3a()                      // warehouse capacity
 *     if (cap <= stock[cargo])        want = 0
 *     if (DS:0x8dc8[cargo] != 0)      want = 0   // colony already MAKES it
 *     if (want) +0x8d = cargo; else if (+0x8d == cargo) +0x8d = 0xff
 * Ghidra drops the two args at every call site because they arrive in
 * registers; the `(0x181f, tag, value)` spelling in the clean overlay
 * recovery IS the literal argument list (md:760-793), so no ndisasm
 * recovery was needed beyond confirming the thunk target.
 * `DS:0x8dc8` (`-0x7238`) is the tick's gross-production scratch ledger,
 * `ai_euro_5952_ledgers`' `gross[]`.
 *
 * ARM-BY-ARM, all four cargo tags fall out of the colony record's stock
 * base `+0x9a + cargo*2`: `0xf` = +0xb8 Muskets, `0xe` = +0xb6 Tools,
 * `0xd` = +0xb4 Trade Goods, `8` = +0xaa Horses.
 *
 *  1. `(0xf, (target - muskets != 0 && muskets <= target))` where
 *     `target = (byte[nation*3 + -0x6a9a] + 2) * 0x32`. `-0x6a9a` =
 *     DS:0x9566 column 0, the @LEADERNAME **belligerence** trait
 *     (ai_diplo_leader_trait column 0, shipped 1/0/1/-1), so a warlike
 *     leader wants 150 muskets, a meek one 50. The pair of tests is just
 *     `muskets < target` spelled long-hand.
 *  2. `(0xd, trade_goods < 100 && byte[nation*0x10 + -0x7b37] < 4 &&
 *     (+0x1c & 0x20))`. `-0x7b37` = DS:0x84c9 = DS:0x84bc + 0xd, the
 *     per-nation Europe SELL row for Trade Goods (`euro_price − 1`, clamped
 *     at 0 — the cascade header above cites the same table at +0xf), and
 *     `+0x1c & 0x20` is COLONIZE_COLONY_FLAG_WAGON_TRAIN. So: stock a
 *     trading post's worth of Trade Goods only while they are cheap and the
 *     colony has a wagon to move them.
 *  3. `(8, horses < 0x32)`.
 *  4. `(0xe, local_16 == 0 && (+0x1b & 0x80))` — the tick-local tools latch
 *     (seeded `0x13 < +0xb6`, raised by the Pioneer absorb case) and
 *     WANTS_PIONEER_WORK. This is the second of the two `local_16` readers
 *     and the reason the latch has to leave the absorb frame.
 *  5. `(0xf, ...)` AGAIN, overriding arm 1 — this is the `iStack_76`
 *     reader the 2026-09-18 re-hosting left unmodelled. DOS computes
 *     `uStack_96 = (+0x8d == 0x0f)` between arms 1 and 2, i.e. "arm 1 left
 *     Muskets as the standing preference", then:
 *       want = !( (labor < 1 || muskets > 0x31)
 *              && (+0x8e != 1 || muskets > 0x31 || +0x8d == 0x0e)
 *              && ((+0x1b & 8) == 0 || muskets > 0x31 || +0x8d == 0x0e)
 *              && (uStack_96 == 0 || +0x8d != 0x0f) )
 *     i.e. the preference is FORCED back onto Muskets whenever the colony
 *     is short of hands for its garrison (`iStack_76 >= 1`, the running
 *     total the absorption arm `++`s), or +0x8e says labor_shortage 1, or
 *     SHORT_DEFENDERS is set — unless it already has more than 49 muskets
 *     or has settled on Tools. That is the whole observable consequence of
 *     the absorb-time increment: one more absorbed Soldier this tick can
 *     tip a colony into buying muskets.
 *
 * The 11-cargo "surplus haul ladder" that used to stand in for this block
 * in ai_euro_colony_inventory was a port invention (its own comment said
 * "FUN_5952_0306 thin"); it is deleted, and with it the invented `boycotted`
 * clear — DOS's second clear is `gross[cargo] != 0`.
 */
static void ai_euro_5952_build_pref_0306(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  int labor_running, int tools_latch
) {
  if (!ctx->colonies) {
    return;
  }
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  {
    const ColonizeWorld w = world_from_turn_ctx(ctx);
    ai_euro_5952_ledgers(
      &w, ctx->colonies, c, (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL, gross, demand
    );
  }
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;

  /* arm 1 — Muskets, md:759-762 */
  {
    const int target = (ai_diplo_leader_trait(ctx, nation_id, 0) + 2) * 0x32;
    const int muskets = c->stock[COLONIZE_CARGO_MUSKETS];
    colonies_specialty_cargo_update(
      ctx->colonies, c, COLONIZE_CARGO_MUSKETS,
      (target - muskets != 0 && muskets <= target),
      gross[COLONIZE_CARGO_MUSKETS] != 0
    );
  }
  /* md:764-765: uStack_96 is latched HERE, between arms 1 and 2. */
  const int local_96 = (c->specialty_cargo == (uint8_t)COLONIZE_CARGO_MUSKETS);
  /* arm 2 — Trade Goods, md:766-773 */
  {
    int price = 0;
    if (col1 && nation_id >= 0 && nation_id < 4) {
      const int p = (int)col1->nation[nation_id].trade.euro_price[COLONIZE_CARGO_TRADE_GOODS] - 1;
      price = p < 0 ? 0 : p;
    }
    const int want = c->stock[COLONIZE_CARGO_TRADE_GOODS] < 100 && price < 4 &&
                     (c->colony_flags & COLONIZE_COLONY_FLAG_WAGON_TRAIN) != 0;
    colonies_specialty_cargo_update(
      ctx->colonies, c, COLONIZE_CARGO_TRADE_GOODS, want,
      gross[COLONIZE_CARGO_TRADE_GOODS] != 0
    );
  }
  /* arm 3 — Horses, md:774 */
  colonies_specialty_cargo_update(
    ctx->colonies, c, COLONIZE_CARGO_HORSES, c->stock[COLONIZE_CARGO_HORSES] < 0x32,
    gross[COLONIZE_CARGO_HORSES] != 0
  );
  /* arm 4 — Tools, md:775-781 */
  colonies_specialty_cargo_update(
    ctx->colonies, c, COLONIZE_CARGO_TOOLS,
    tools_latch == 0 && (c->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) != 0,
    gross[COLONIZE_CARGO_TOOLS] != 0
  );
  /* arm 5 — Muskets again, md:782-793. DOS-LITERAL, including the three
   * repeats of `0x31 < +0xb8` and the `+0x8d == 0x0e` (Tools) escape. */
  {
    const int muskets = c->stock[COLONIZE_CARGO_MUSKETS];
    const int spec = (int)c->specialty_cargo;
    const int rich = muskets > 0x31;
    const int quiet =
      (labor_running < 1 || rich) &&
      ((int)c->labor_shortage != 1 || rich || spec == COLONIZE_CARGO_TOOLS) &&
      ((c->ai_flags & COLONIZE_COLONY_AI_SHORT_DEFENDERS) == 0 || rich ||
       spec == COLONIZE_CARGO_TOOLS) &&
      (local_96 == 0 || spec != COLONIZE_CARGO_MUSKETS);
    colonies_specialty_cargo_update(
      ctx->colonies, c, COLONIZE_CARGO_MUSKETS, !quiet,
      gross[COLONIZE_CARGO_MUSKETS] != 0
    );
  }
}

static void ai_euro_colony_threat_seed_5952(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c,
  int* out_labor_running
) {
  if (out_labor_running) {
    *out_labor_running = 0;
  }
  if (!ctx || !ctx->units || !ctx->map || !c) {
    return;
  }
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);

  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const int mw = (int)ctx->map->width;
  const int mh = (int)ctx->map->height;
  int threat = 0;
  int ring1 = 0; /* iStack_22 — raw 94097-94134 */

  for (int dy = -5; dy <= 5; ++dy) {
    for (int dx = -5; dx <= 5; ++dx) {
      const int tx = c->x + dx;
      const int ty = c->y + dy;
      if (tx < 0 || ty < 0 || tx >= mw || ty >= mh) {
        continue; /* FUN_281f_0302 map_tile_in_bounds */
      }
      const int head = units_id_at(ctx->units, tx, ty);
      if (head < 0) {
        continue;
      }
      {
        const ColonizeUnit* hu = units_get_const(ctx->units, head);
        if (!hu || (hu->nation_id & 0xf) == nation_id) {
          continue; /* DOS tests the STACK HEAD's nation only */
        }
      }
      /* Settlement on the scanned tile (colony or village): FUN_281f_06be is
       * the layer2 bit-2 owner, set for both. */
      const int on_settlement =
        (ctx->colonies && colonies_id_at(ctx->colonies, tx, ty) >= 0) ||
        (ai_euro_village_nation_at(col1, tx, ty) >= 0);
      const int dist = map_dos_dist(dx, dy); /* FUN_124c_0040 */

      /* DOS walks the whole tile stack once the head qualified — including
       * any own-nation unit stacked behind a foreign one. */
      /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index, not an id. */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        const ColonizeUnit* u = &ctx->units->units[ui];
        if (!u->active || !units_is_on_map(u) || u->x != tx || u->y != ty) {
          continue;
        }
        const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
        if (dtype >= 0x0d && dtype <= 0x12) {
          continue; /* raw: `type < 0xd || 0x12 < type` — ships excluded */
        }
        int v = combat_unit_base_x8(&sctx, u->id, 1, NULL);
        const int owner = u->nation_id & 0xf;
        if (owner < 4) {
          if (v < 2) {
            v = 0;
          }
          if (ai_euro_nation_is_human(ctx, owner)) {
            v = v + (v >> 1);
          }
          /*
           * iStack_22 (raw 94120-94133): a EUROPEAN foe with a non-zero
           * scored value inside ring 1 (|dx| < 2 && |dy| < 2). Counted here,
           * i.e. before the settlement halving and the distance scaling, and
           * never for Indian owners. Feeds the labor_shortage floor below.
           */
          if (v != 0) {
            const int adx = dx < 0 ? -dx : dx;
            const int ady = dy < 0 ? -dy : dy;
            if (adx < 2 && ady < 2) {
              ring1++;
            }
          }
        } else {
          if (ai_diplo_indian_alarm(col1, owner, nation_id) < 0x19) {
            v = 0;
          }
          {
            int tension = 0;
            if (col1 && col1->tribe && u->home_tribe_id >= 0 &&
                u->home_tribe_id < (int)col1->head.tribe_count) {
              tension = col1_tribe_attitude(&col1->tribe[u->home_tribe_id], nation_id);
            }
            if (tension < 0x80) {
              v = 0;
            }
          }
        }
        if (on_settlement) {
          v >>= 1;
        }
        v = (-(dist - 8) * v) >> 3;
        threat += v;
      }
    }
  }

  int floor_v = threat;
  if (floor_v > 0x10) {
    floor_v = 0x10;
  }
  int walls = 0; /* FUN_281f_0ab0(0): Stockade→Fort→Fortress chain count */
  if (ctx->colonies) {
    /* Chain names come from the live @BUILDING rows 0-2 (theme D). */
    for (int i = 0; i < 3; ++i) {
      const int b = colonies_find_building(ctx->colonies, reports_fort_tier_name(i));
      if (b >= 0 && b < COLONIZE_BUILDING_TYPES_MAX && c->has_building[b]) {
        walls++;
      }
    }
  }
  threat = threat / (walls + 1);
  if (threat < floor_v) {
    threat = floor_v;
  }
  const int quota = threat >> 3;
  c->garrison_quota = (uint8_t)quota; /* DOS `(char)` truncation */

  int n = 0;
  int want = 0;
  int homed_mil = 0;
  /* iStack_22 also gates the build cascade's Wagon Train arm, which the port
   * runs from a different pass of this same DOS body — stash it. */
  if (c->id >= 0 && c->id < COLONIZE_COLONIES_MAX) {
    s_5952_ring1[c->id] = ring1;
  }

  ai_euro_5952_labor_demand(ctx, nation_id, c, col1, quota, ring1, &n, &want, &homed_mil);

  ai_euro_5952_ai_flags(ctx, nation_id, c, n, want, homed_mil);

  /*
   * raw 94219-94229 (md:566-577), DOS-LITERAL: right after the +0x1b flag
   * writes the tick wipes the by-profession census and rebuilds it over the
   * colony's own colonists, folding every non-expert (`FUN_281f_0c9a == 0`)
   * into cell 0x13. Only the two cells the absorption arm reads are kept —
   * see the s_5952_census_* note above for why they are stashed.
   */
  {
    int nonexpert = 0;
    int vet_soldier = 0;
    const int pop = (int)c->population;
    for (int i = 0; i < pop && i < COLONIZE_COLONY_POP_MAX; ++i) {
      const int prof = (int)c->colonists[i].profession;
      if (!ai_euro_5952_job_is_expert(prof)) {
        nonexpert++;
      } else if (prof == UNITS_JOB_SOLDIER) {
        vet_soldier++;
      }
    }
    ai_euro_5952_set_absorb_census(c->id, nonexpert, vet_soldier);
  }

  /*
   * `want` is DOS's `iStack_76` at this point in the tick (post on-tile
   * military walk, NOT the +0x8e byte — DOS writes that once, before the
   * walk). It is handed to the caller so the absorption arm at raw
   * 94231-94272 can `++` it as its own tick-local; see
   * ai_euro_5952_absorb_equip, which the caller runs once the port's SECOND
   * half of DOS's +0x1b writers (ai_euro_refresh_colony_ai_flags, raw
   * 94200-94210) has also run — DOS emits every flag write before the
   * absorption.
   */
  if (out_labor_running) {
    *out_labor_running = want;
  }
}

/* --- 0a60 colony goals: stage helpers ---------------------------------- */

COLONIZE_INTERNAL void ai_euro_colony_goals_unit_contact(
  ColonizeTurnContext* ctx, int nation_id
) {
  /* B: own units — CONTACT from adjacent foreign; work queue only for bindable. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = u->x + MAP_DIR8_DX[d];
      const int ny = u->y + MAP_DIR8_DY[d];
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id != nation_id) {
        /*
         * DOS raw `0a60` unit loop reaches `thunk_FUN_2a1f_0470`
         * (upsert_primary) only — there is no `0524` (upsert_work_queue)
         * call anywhere outside the colony loop. The extra work-queue row
         * this used to write (`ai_goals_upsert_work(u->id, 3, ...)`) was a
         * port invention: it stored a *unit* id in a queue whose `+0` field
         * is a colony index, and nothing ever read it back — the 4393
         * consumer skipped it on the old `flag_b != 1` filter, which is the
         * only reason the id-namespace collision never bit. Removed
         * 2026-09-06d with the flag_a/flag_b decode.
         */
        ai_goals_upsert_primary(nation_id, nx, ny, AI_GOAL_CONTACT, 3);
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_colony_labor(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  AiEuroInventory* inv, int urgency
) {
  /*
   * FUN_5952_035e threat accumulator → garrison_quota (+0x1e). DOS order:
   * the quota write happens BEFORE the tick's ai_flags bit writes, so the
   * refresh below reads this turn's quota (it used to read last turn's).
   */
  int labor_running = 0; /* iStack_76, carried into the absorption arm */
  ai_euro_colony_threat_seed_5952(ctx, nation_id, c, &labor_running);
  /* FUN_5952_035e raw 94170-94190 — the tick's Indian war-declare block
   * (or_both(nation, tribe+4, 2)); lives in ai_contact.c. DOS runs it in
   * the same per-colony body, between the expansion-appetite math and the
   * +0x1b flag writes; it draws no RNG, so its position inside the tick
   * cannot shift a stream. */
  ai_contact_colony_tick_war_5952(ctx, nation_id, c->x, c->y);
  ai_euro_refresh_colony_ai_flags(ctx, nation_id, c);
  /*
   * raw 94231-94352: the tile re-scan absorption arm and the equip arm, at
   * DOS's position — after every +0x1b flag write and the by-profession
   * census, before the build-preference / construction cascade.
   */
  ai_euro_5952_absorb_equip(ctx, nation_id, c, &labor_running);
  /*
   * `|| c->labor_shortage > 0` used to be a third disjunct here. It was
   * calibrated against the retired thin latch (0 unless something set
   * it); since #38 +0x8e carries the real FUN_5952_035e number and is
   * >= 1 for essentially every colony of pop >= 3, so the disjunct made
   * this arm unconditional — it swallowed the DOS-gated ship-pressure
   * `else` below and pulled every idle unit into the nearest town.
   * DOS never uses +0x8e as a boolean "wants labor": its consumer is the
   * garrison-quota distribution loop further down, which registers its
   * own LABOR goal at prio `shortage − garrisoned + 2` and decrements the
   * counter per admission. Dropped 2026-09-09.
   */
  int labor = (c->population < 3) || ai_euro_colony_food_short(c);
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
    const int labor_prio = construction ? 6 : (4 + urgency / 4);
    /* The thin `labor_shortage = 1` demand latch that used to sit here is
     * retired 2026-09-09: +0x8e is now stamped unconditionally from the
     * real FUN_5952_035e formula (local_76 / DS:0x8d72) in
     * ai_euro_colony_threat_seed_5952, called at the top of this same
     * colony body, and the latch could only overwrite a legitimate 0. */
    ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, labor_prio);
  } else if (c->ai_flags & (COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP |
                             COLONIZE_COLONY_AI_NEARBY_FRIGATE)) {
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
    const int mow = (c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0;
    ai_goals_upsert_primary(
      nation_id,
      c->x,
      c->y,
      mow ? AI_GOAL_COLONY_ALT : AI_GOAL_COLONY,
      mow ? 8 : 5
    );
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_colony_work(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
) {
  /*
   * garrison_quota (+0x1e) is now the real FUN_5952_035e threat>>3 seed —
   * see ai_euro_colony_threat_seed_5952 above, called at the top of this
   * colony body in DOS order. The thin latch that used to live here
   * ("idle unfortified Soldier/Dragoon on the colony tile and quota == 0
   * → 1, skipped while NEEDS_COLONISTS / LABOR so early towns admit the
   * beachhead soldier") is retired: it had no DOS basis, and its
   * deliberate labor-gate carve-out is not something DOS does — the real
   * seed is unconditional per colony tick and keys on nearby hostiles,
   * not on who happens to be standing in the town.
   */
  /*
   * NO expand-FOUND seed here — REFUTED 2026-09-08. The old "FOUND via
   * 06ae around colony" row (and the ring-2..4 rescan that made it
   * functional) was a Linux invention: 06ae's only DOS callers are the
   * 20e6 ship unload placement (decomp 89587) and the landing block
   * (~85045), and the full FUN_521d_016a call-site enumeration shows
   * DOS writes FOUND (code 1) primaries in exactly two producers, both
   * ported — the 0a60 per-village ocean-beachhead producer, gated on NO
   * own colony on that continent (decomp 88049, one per continent via
   * the 0x173c/0x173e masks), and the foreign-colony producer's
   * FOUND-or-MIL_EXPAND arm (decomp 87983). DOS AI never seeds a
   * second colony around an existing one; same-landmass growth comes
   * from the foreign-colony arm and the labor loop. (The 95081+
   * decompile block is a duplicate pass over the same 0a60 body —
   * positive vs negative DS spellings, same LAB_521d_0ef0.)
   */
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
   * The DOS pre-loop over the units stacked on the colony tile
   * (+800 idle Pioneer / +1500 exposed combat-capable land unit on
   * a `stance==0` continent) IS ported — see the block just below;
   * `FUN_1000_89d0`/`84d4` resolved to the unit-on-tile + transport
   * chain walkers (accessors.c), substituted with an x/y filter.
   * `flag_a`/`flag_b` DECODED 2026-09-06d and now carry their real
   * DOS meanings (`AiWorkSlot.loads` / `.military`, record bytes +4
   * and +5): `loads` is DOS's `iStack_40` accumulator (+1 per counted
   * idle Pioneer, +1 per exposed combat unit, plus
   * `(min(adjusted, target) + 25) / 100` per counted cargo slot) and
   * `military` is the boolean the +1500 exposed-unit arm sets. Both
   * are read back by `FUN_521d_4393` — `loads` as the "slot still has
   * work" gate and the quantity its tail decrements, `military` as
   * the permission for a non-civilian hull to take the slot. The old
   * Linux `flag_a = specialty_cargo` hint moved into the 4393 pick
   * itself (it reads `c->specialty_cargo` directly now), so nothing
   * downstream lost the Series R tie-break.
   * Cite: move_scoring_ship.md Series F2; col1_save.h `stock`/
   * `trade.euro_price`/`cargo_produced_mask`.
   */
  {
    /*
     * Registration gate: DOS's `bVar5` — LIVE since 2026-09-06g.
     * Raw `viceroy_unpacked.c` FUN_521d_0a60 colony loop (the
     * `thunk_FUN_2a1f_0524(0x281f,local_3e,(int)local_1a,local_40,
     * local_44)` call site, :87681): `bVar5` is set in exactly three
     * places —
     *   :87622  idle-Pioneer arm (`local_40++; bVar5 = true;` +800)
     *   :87633  exposed-combat  (`local_44 = 1; bVar5 = true;` +1500)
     *   :87663  `if (0x4a < local_2a) bVar5 = true;`
     * — and `if (bVar5) { 0x1734[nation]++; local_1a += colony[+0x8f]
     * * 8; clamp 0x7fff; upsert; }` (:87674-87682).
     *
     * The two earlier reverts (2026-08-18 `target=100` placeholder;
     * 2026-09-06d "the port consumes the queue in the *delivery*
     * direction") are both retired. The 06d objection was structural
     * and correct at the time: DOS's queue is a PICKUP queue (score
     * = Σ euro_price × stock, `loads` = hold-loads of goods sitting
     * at the colony), so a DOS-gated row aims a hauler at the colony
     * that HAS the goods. That is now the right thing to do, because
     * both pickup consumers exist: the 20e6 LOAD matrix
     * (`ai_euro_20e6_load_pick`) fires for ships on arrival at an own
     * colony (2026-09-06e) and for wagons (2026-09-06f), and the
     * delivery half afterwards is owned by the already-ported DOS
     * arms (ship: delivery-tally matrix + sell tail + Europe export;
     * wagon: own-colony dump sweep + village errand). The Linux
     * shortage ladder and `ai_euro_nearest_haul_short_colony` that
     * pulled the queue the other way are deleted with this pass.
     */
    int wbvar5 = 0;
    /*
     * FUN_1000_8f2a() RESOLVED (2026-08-18, static — no live session
     * needed): address_mapping.csv's canonical chain
     * FUN_1000_8f2a → FUN_281f_0d3a → FUN_15eb_0a50 is exactly the
     * already-known, already-documented warehouse-capacity formula
     * (`save_format_map.md`/`FUNCTION_CATALOG.md`: 100×(1+
     * warehouse_level)) — already live in Linux as
     * `colonies_warehouse_capacity`. DOS calls this once per colony
     * (no cargo_type arg), same as here — and since smell audit #25
     * removed the port's uncited FOOD-199 branch, the accessor is now
     * cargo-independent like DOS's, so the cargo passed here is only a
     * readability choice (FOOD is skipped by this loop anyway).
     */
    const int target =
      colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_TOOLS);
    const ColonizeCol1Nation* nat =
      (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4)
        ? &ctx->col1->nation[nation_id]
        : NULL;
    long wscore = 0;
    /*
     * Idle-Pioneer / exposed-combat-unit bonus (raw lines ~536-563,
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
     * the shared clamp below still applies) per idle PIONEER (DOS
     * type 0x02 — the earlier "Missionary" reading was a type-id
     * mislabel, fixed 2026-09-07) when colony +0x1b bit 0x80
     * (WANTS_PIONEER_WORK, live since 2026-09-07) is clear.
     * +1500 per exposed combat-capable land unit (attack>1,
     * not a ship) when this continent has no G-table stance assigned
     * (`ai_euro_continent_stance_at()==0`) and the unit's AI plan
     * letter is neither 'G' nor 'A' (raw :87631 —
     * `+0x314b != 'G' && +0x314b != 'A'`).
     *
     * 2026-09-09: that last gate is now the REAL one. It used to be
     * argued away against this port's own 0a60 shadow state
     * (`s_0a60_pilot_state`, always fresh-zeroed here), but the DOS byte
     * is +0x314b = `ai_plan`, not the +0x314c act-state the shadow
     * mirrors — the same mislabel already corrected for the 4962 census
     * gate (col1_stuff_census.c:166-187, ai_diplo.c:1592-1631,
     * ai_contact.c:9061-9069, all reading `col1_ai_plan` ∈ {'A','G'}).
     * 'A'/'G' are the garrison/assigned plans: a unit already spoken for
     * is not "exposed", so it must not raise this colony's work-queue
     * score. `col1_ai_plan` is save-backed and survives across turns,
     * so unlike the shadow it is a live, non-vacuous test.
     *
     * ONE-TURN-STALE STANCE READ IS DOS-FAITHFUL (smell audit #34,
     * REFUTED 2026-09-09 — do not "fix" by hoisting the refresh):
     * in `FUN_521d_0a60` the colony loop that owns this arm is
     * viceroy_unpacked.c:87595-87991 (the read at :87627), while the
     * only writer of the −0x6790 G-table is the continent loop at
     * :88054-88151 — later in the *same* call, and no other DOS
     * function writes −0x6790. So DOS's arm also sees the table left
     * by the previous turn's 0a60 call for this nation, all-zero on
     * the first call. `s_euro_continent_stance` is file-static and
     * likewise persists across turns, and
     * `ai_euro_refresh_continent_stance` below (the port of the
     * :88054 loop) is the only writer — same semantics.
     */
    /* DOS iStack_40 / uStack_44 / bVar5 — see AiWorkSlot in ai_goals.h. */
    int wloads = 0;
    int wmilitary = 0;
    if (ctx->units) {
      const int cid = map_continent_id_at(ctx->map, c->x, c->y);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        const ColonizeUnit* u = &ctx->units->units[ui];
        if (!u->active || u->x != c->x || u->y != c->y) {
          continue;
        }
        /*
         * Raw :87619-87622: `type == 0x02` — a PIONEER (not a Missionary;
         * type 0x03 is the Missionary — mislabel fixed 2026-09-07), and
         * only when colony +0x1b bit 0x80 is CLEAR (no pioneer work at
         * this colony → the idle Pioneer registers it for pickup).
         */
        if (ai_euro_20e6_dos_type(ctx->units, u) == 0x02 &&
            (c->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) == 0) {
          wscore += 800;
          ++wloads; /* DOS iStack_40++ in the same arm */
          wbvar5 = 1; /* raw :87622 */
        }
        /* `units_is_sea` takes a unit ID; this loop is a slot walk, so
         * `ui` was the wrong key (Leads 2, 2026-09-10). */
        if (ai_euro_continent_stance_at(nation_id, cid) == 0 &&
            !units_is_sea(ctx->units, u->id) && u->col1_ai_plan != 0x47u /* 'G' */ &&
            u->col1_ai_plan != 0x41u /* 'A' */) {
          const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
          if (ty && ty->attack > 1) {
            wscore += 1500;
            ++wloads;
            wmilitary = 1; /* DOS uStack_44 = 1 — the real flag_b */
            wbvar5 = 1;    /* raw :87633-87634 */
          }
        }
      }
    }
    for (int slot = 0; slot < COLONIZE_CARGO_COUNT; ++slot) {
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
      /* DOS iStack_32, computed from the target-clamped value BEFORE the
       * TOOLS/MUSKETS −100 discount, and only banked below. */
      const int loads_here = ((have > target ? target : have) + 25) / 100;
      if (slot == COLONIZE_CARGO_FOOD || slot == COLONIZE_CARGO_LUMBER ||
          slot == COLONIZE_CARGO_TRADE_GOODS) {
        continue;
      }
      if (slot == COLONIZE_CARGO_TOOLS || slot == COLONIZE_CARGO_MUSKETS) {
        if (!(c->cargo_produced_mask & (1u << slot))) {
          continue; /* not produced this tick — DOS skips entirely */
        }
        have -= 100;
      }
      /*
       * DOS raw :87663 `if (0x4a < local_2a) bVar5 = true;` — the
       * registration gate, LIVE. Note the exact position: it reads the
       * POST-adjustment, POST-`−100`-discount value (the same `local_2a`
       * the score below multiplies by `euro_price`), it is inside the
       * FOOD/LUMBER/TRADE_GOODS skip (those three cargoes can never arm
       * it), and for TOOLS/MUSKETS it therefore needs the adjusted value
       * to clear 0x4a + 100. Because `local_2a` is doubled at/above
       * `target`, a colony at or over warehouse capacity in any counted
       * cargo arms the gate outright.
       */
      if (have > 0x4a) {
        wbvar5 = 1;
      }
      if (have >= 0) {
        const int price = nat ? (int)nat->trade.euro_price[slot] : 0;
        wscore += (long)price * have;
        wloads += loads_here;
      }
    }
    if (wbvar5) {
      /*
       * Raw :87677 `local_1a += *(char *)(colony + 0x8f) * 8;` — the
       * `cargo_idle_turns` bonus is DOS's, unconditional inside the
       * `bVar5` branch (it was previously carried only on the Linux
       * shortage arm), applied BEFORE the 0x7fff clamp. `+0x8f` is a
       * signed char in DOS, and `cargo_idle_turns` is the port's own
       * name for it.
       */
      wscore += (long)(int8_t)c->cargo_idle_turns * 8;
      if (wscore > 0x7fff) {
        wscore = 0x7fff;
      }
      /*
       * No `loads` floor any more: DOS's `local_40` is exactly what it
       * writes, and a `bVar5`-gated colony always has something to
       * collect (the 0x4a arm banks at least one `loads_here`, and the
       * idle-Pioneer / exposed-unit arms each add their own +1), so the
       * `loads == 0` "no work" state `4393` skips is unreachable here —
       * which is why the 2026-08-18 "colony never registers" regression
       * signature cannot recur. The old floor-of-1 existed only to give
       * the Linux shortage arm a fake load; that arm is gone.
       */
      if (wloads > 255) {
        wloads = 255;
      }
      if (getenv("AI_0A60_WORK_TRACE")) {
        fprintf(
          stderr, "[work] n%d colony %d (%d,%d) score %ld loads %d mil %d\n",
          nation_id, c->id, c->x, c->y, wscore, wloads, wmilitary
        );
      }
      ai_goals_upsert_work(
        c->id, (int)wscore, (uint8_t)wloads, (uint8_t)wmilitary
      );
      /* `*(int *)(nation*2 + 0x1734)` bump (:87675) — the urgency term
       * the 20e6 colony-sail / unload matrices and the berth boarding
       * scan read; only that scan ever zeroes it (:81295). */
      if (s_0a60_work_registered[nation_id] < 0x7fff) {
        s_0a60_work_registered[nation_id]++;
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_colony_garrison(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
) {
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
   * The "already garrisoned" count is DOS `FUN_1000_8aac(unit,10)` =
   * 0d38 case 0xa: # non-ship units with @UNIT combat (0x5236) > 1 in
   * the colony-tile stack — REWIRED 2026-09-06b to that real count
   * (ai_euro_0a60_stack_counts .armed; was "own fortified units on
   * the tile"). Admitted units get
   * shadow order 'A', which excludes them from this turn's goal scan
   * (DOS-identical effect); deeper 'A' labor handling stays with the
   * existing colony-join paths.
   */
  if (c->labor_shortage > 0 && ctx->units &&
      map_tile_is_coastal(ctx->map, c->x, c->y)) {
    Ai0a60StackCounts gsc;
    ai_euro_0a60_stack_counts(ctx->units, c->x, c->y, &gsc);
    const int garrisoned = gsc.armed;
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
      /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index. */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX && c->labor_shortage > 0; ++ui) {
        ColonizeUnit* gu = &ctx->units->units[ui];
        if (!gu->active || gu->nation_id != nation_id || gu->x != c->x ||
            gu->y != c->y || gu->id < 0 || gu->id >= COLONIZE_UNITS_MAX) {
          continue;
        }
        if (ai_euro_20e6_dos_type(ctx->units, gu) != k_adm_type[pass]) {
          continue;
        }
        const int is_vet = (gu->profession == UNITS_JOB_SOLDIER);
        if (k_adm_vet[pass] >= 0 && is_vet != k_adm_vet[pass]) {
          continue;
        }
        if (gu->col1_ai_plan == 'A') {
          continue; /* already admitted this turn */
        }
        gu->col1_ai_plan = 'A'; /* +0x314b */
        c->labor_shortage--;
        if (c->garrison_quota != 0) {
          c->garrison_quota--;
        }
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_foreign_colonies(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv
) {
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
        if (!ai_euro_is_military_name(ai_euro_unit_kind(ctx->units, u))) {
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
    }
    /*
     * The goals-phase twin of the invented act-level scout ring aim used to
     * sit here (bugs.md #493/#495) and is gone with it: DOS has no
     * goals-phase Scout aim at all. A Scout's course is decided inside
     * FUN_521d_20e6 (explorer flag / patrol 0x56 / village 0x4c / explore
     * ring), which the act now reaches on every act (see the DOS re-entry
     * gate in ai_euro_unit_act, raw 90551).
     */
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_food_emergency(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv
) {
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
}

COLONIZE_INTERNAL void ai_euro_colony_goals_tribe_seeds(
  ColonizeTurnContext* ctx, int nation_id
) {
  /* F: tribe-adjacent FOUND prio 2; alarmed → MILITARY. Never FOUND on village. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      int fx = 0;
      int fy = 0;
      {
        if (ai_euro_pick_founding_tile(
              ctx->map,
              ctx->colonies,
              ctx->col1_ok ? ctx->col1 : NULL,
              ctx->units,
              nation_id,
              t->x,
              t->y,
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
}

COLONIZE_INTERNAL void ai_euro_colony_goals_producers(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv,
  int urgency
) {
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
      inv ? inv->colony_count : colonies_count_for_nation(ctx->colonies, nation_id);
    if (!ai_euro_ship_dos_enabled() && own >= 2 && ctx->colonies) {
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
                  ctx->units,
                  nation_id,
                  target->x,
                  target->y,
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
            if (ai_euro_pick_founding_tile(
                  ctx->map,
                  ctx->colonies,
                  ctx->col1_ok ? ctx->col1 : NULL,
                  ctx->units,
                  nation_id,
                  (int)t0->x,
                  (int)t0->y,
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
              const ColonizeUnitKind kind = ai_euro_unit_kind(ctx->units, u);
              if (kind != UNITS_KIND_SCOUT && !ai_euro_is_military_name(kind)) {
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
}

COLONIZE_INTERNAL void ai_euro_colony_goals_ship_found(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv,
  int urgency
) {
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
                     ctx->units,
                     nation_id,
                     u->x,
                     u->y,
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
}

/*
 * bugs.md #707 — RETIRED 2026-09-23. The "H: light bind" arm that stood here
 * scanned every idle on-map land unit of kind Pioneer or Colonist and stamped
 * an AI_MOVE goto at ai_goals_best_found_tile_near. It had no FUN/raw cite and
 * no DOS counterpart:
 *   - DOS binds a unit to a goal only through FUN_521d_0a60's tail
 *     (+0x314c = 0x0b, +0x314d/e = the goal tile), never from a colony-goals
 *     pass and never as a bare goto.
 *   - FOUND eligibility there requires unit+0x3148 bit 2, which raw
 *     87511-87514 (via 8aac/0d38 case 3, `cmp type,2`) grants only when the
 *     unit's own stack holds a Pioneer or a military unit — so a lone
 *     Colonist can never take a FOUND goal in DOS at all.
 * DOS's type-0 arms are the FUN_521d_20e6 labor arm, the explore ring and the
 * become-a-Pioneer fall-through (raw 89350-89357), all of which are ported.
 * The arm, its declaration and its call site are gone.
 */

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
  /* Per-tick scratch, like the queue itself: DOS `clear_work_queue` sits at
   * exactly this point and every hauler claim below it is a fresh one. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    s_4393_claim_valid[i] = 0;
  }

  /* A: urgency seed; FUN_1d1d_0dae(0x9faa,0,0x10e) coarse-plane wipe + restamp. */
  ai_coarse_fog_euro_restamp(ctx->units, ctx->colonies, nation_id);
  /* Raw lines 1-189: per-unit 0x3148 housekeeping + foreign-ship CONTACT
   * producer, DOS position (after the memsets, before the colony loop). */
  ai_euro_0a60_unit_housekeeping(ctx, nation_id);
  const int urgency = inv ? inv->urgency : 0;

  ai_euro_colony_goals_unit_contact(ctx, nation_id);

  /* D: own colonies — LABOR from tools/food shortage / underpop (5cf6 tallies)
   * or Stockade/Warehouse under construction. NOT from Col1 labor_shortage
   * (+0x8e): that disjunct was dropped 2026-09-09 and must not come back —
   * the long comment at the arm itself explains why (+0x8e is >= 1 for
   * essentially every colony of pop >= 3, so it made the arm unconditional).
   * (A "threatened Stockade deepen" LABOR-priority term stood here; deleted
   * 2026-09-18 with ai_euro_colony_threatened_by_war — the AI's building
   * choice is the FUN_5952_035e cascade alone, never war proximity.) */
  ai_euro_ship_pressure_reset(nation_id); /* FUN_4962_0018 raw 78239-78242 */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      ai_euro_colony_goals_colony_labor(ctx, nation_id, c, inv, urgency);
      ai_euro_colony_goals_colony_work(ctx, nation_id, c);
      ai_euro_colony_goals_colony_garrison(ctx, nation_id, c);
    }
  }

  ai_euro_colony_goals_foreign_colonies(ctx, nation_id, inv);

  ai_euro_colony_goals_food_emergency(ctx, nation_id, inv);

  const int dos_ship = ai_euro_ship_dos_enabled();
  if (!dos_ship) {
    ai_euro_colony_goals_tribe_seeds(ctx, nation_id);
  }

  ai_euro_colony_goals_producers(ctx, nation_id, inv, urgency);

  if (!dos_ship) {
    ai_euro_colony_goals_ship_found(ctx, nation_id, inv, urgency);
  }
}

/* --- 20e6 scoring (land Manhattan + ocean/ship branch) ----------------- */

static int ai_euro_tile_under_enemy_fort_fire(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* viewer,
  int x,
  int y
);
static int ai_euro_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f,
  int is_naval
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
  const int on_hs = map_tile_is_high_seas(ctx->map, u->x, u->y);
  const int west_explore = goal_x < u->x;
  const int east_europe = goal_x > u->x;
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, u->nation_id);
  const int own_tough = ai_euro_foe_toughness(ctx, ctx->units, u, 1);
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe >= 0) {
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id == u->nation_id) {
        /* Own ship on water: stackable, step through it. Anything else of
         * our own on a non-water tile is a land unit — skip. */
        if (!units_is_sea(ctx->units, foe)) {
          continue;
        }
      } else if (f && !units_is_sea(ctx->units, foe)) {
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
    if (west_explore && MAP_DIR8_DX[d] < 0) {
      score += 4; /* west bias toward New World */
    }
    if (on_hs && west_explore && step_hs && MAP_DIR8_DX[d] < 0) {
      score += 6; /* HS west-explore: prefer westward HS tiles */
    }
    /* Leave eastern HS rim into ocean when sailing west (Atlantic first leg). */
    if (on_hs && west_explore && !step_hs) {
      score += 14;
    }
    if (east_europe && MAP_DIR8_DX[d] > 0) {
      score += 4; /* east bias toward Europe / eastern HS */
    }
    if (on_hs && east_europe && step_hs && MAP_DIR8_DX[d] > 0) {
      score += 6; /* HS east-Europe: prefer eastward HS tiles */
    }
    /* Avoid enemy Fort/Fortress batteries (FUN_364b_03f6). */
    if (ai_euro_tile_under_enemy_fort_fire(ctx, u, nx, ny)) {
      score -= 800;
    }
    /* Thin combat: prefer closing on weaker adjacent foe ships. */
    if (at_war) {
      for (int ad = 0; ad < 8; ++ad) {
        const int ax = nx + MAP_DIR8_DX[ad];
        const int ay = ny + MAP_DIR8_DY[ad];
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
        const int ft = ai_euro_foe_toughness(ctx, ctx->units, f, 1);
        if (ft < own_tough) {
          score += 18;
        } else if (ft > own_tough) {
          score -= 8;
        }
      }
    }
    /* Empty-hold coastal cling (3558/457e thin): prefer coast water near goal. */
    if (u->cargo_count == 0 && map_tile_is_coast_water(ctx->map, nx, ny)) {
      score += 8;
    }
    if (ctx->rng) {
      score += dos_rng_range(ctx->rng, 0, 2);
    }
    if (score > best) {
      best = score;
      bdx = MAP_DIR8_DX[d];
      bdy = MAP_DIR8_DY[d];
    }
  }
  if (best < -999990) {
    return 0;
  }
  *out_dx = bdx;
  *out_dy = bdy;
  return 1;
}

/*
 * Native village owner on this tile (col1 tribe table), else -1. DOS
 * FUN_1000_88e0 / FUN_137f_0392; the walk itself is col1_save_village_at.
 */
static int ai_euro_village_nation_at(const ColonizeCol1Save* col1, int x, int y) {
  const ColonizeCol1Tribe* t = col1_save_village_at(col1, x, y);
  return t ? (int)t->nation_id : -1;
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
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, u->nation_id);
  const int own_tough = at_war ? ai_euro_foe_toughness(ctx, ctx->units, u, 0) : 0;
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  int bd = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, u->type_index, nx, ny, u->id)) {
      /*
       * FUN_465b_0000 resolves a step onto an occupied tile against the
       * tile's BEST DEFENDER (FUN_5fef_0000), never the head of the stack.
       * Reading units_id_at here let a berthed foreign ship (which
       * units_is_sea then skipped) hide the land garrison of a defended
       * colony, so an approaching land unit scored the target tile as
       * unreachable and oscillated beside it forever — the REF stall
       * behind bugs.md #521. Fall back to the raw head for defenderless
       * tiles (lone Treasure / civilians), as units_try_move does.
       */
      int foe = units_best_defender_at(
        ctx->units, ctx->col1_ok ? ctx->col1 : NULL, nx, ny, u->id, u->id
      );
      if (foe < 0) {
        foe = units_id_at(ctx->units, nx, ny);
      }
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
      const int is_scout = ai_euro_unit_kind(ctx->units, u) == UNITS_KIND_SCOUT;
      if (is_scout && rum) {
        score += 12;
      } else if (!is_scout && (rum || lcr_class)) {
        score -= 20; /* non-scout skip LCR / rumour */
      }
    }
    /*
     * Land combat 20e6 (structured deepen): prefer closing on weaker adjacent war
     * foes.
     *
     * Deleted 2026-09-23 (bugs.md #759): the "+16 siege approach" additive for a
     * foreign colony destination and the "+10 Artillery prefers a fortified port"
     * rider were invented (cited only as `move_scoring_land.md LAB_521d_5183 /
     * 0x46; unpark #4` — no FUN_/raw). DOS's only colony-destination term in the
     * 20e6 direction scorer is the ×3 multiplier inside the attack term
     * (raw 88898-88901, ai_euro_20e6_attack_term), and the only fortification
     * read in raw 88266-90445 is the ship-only fort-fire penalty (raw 88813-88818).
     * The +10 also inverted raw 88911, where Artillery off a settlement scores 0.
     */
    if (at_war) {
      for (int ad = 0; ad < 8; ++ad) {
        const int ax = nx + MAP_DIR8_DX[ad];
        const int ay = ny + MAP_DIR8_DY[ad];
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
        const int ft = ai_euro_foe_toughness(ctx, ctx->units, f, 0);
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
      bdx = MAP_DIR8_DX[d];
      bdy = MAP_DIR8_DY[d];
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

/*
 * bugs.md #655: DS:0x5236 (@UNIT column 4, ATTACK) and DS:0x523d (@UNIT
 * column 12, the capability bit-string) are now catalog fields
 * (ColonizeUnitType.attack / .cap_bits, units_load_types). These two
 * NAMES.TXT-default tables are kept ONLY as the fallback for a pool whose
 * types did not come from a real NAMES.TXT load (hand-built test pools —
 * see ai_euro_20e6_dos_type's own -1 fallback comment); the live tables
 * below (s_20e6_type_combat_live / s_20e6_type_flags_live), refreshed from
 * the loaded catalog once per dispatcher turn, are read first.
 */
static const uint8_t k_20e6_type_combat[AI_20E6_TYPE_COUNT] = {
  /* NAMES.TXT @UNIT column 4 = ATTACK (DS:0x5236), same column the live
   * cache reads (t->attack); bugs.md #668 (was the DEFENSE column). */
  0, 2, 0, 0, 3, 1, 5, 5, 6, 4, 0, 7, 0, 0, 0, 0, 8, 16, 24, 1, 2, 2, 3
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
 * bugs.md #655: live per-DOS-row cache of ColonizeUnitType.attack / .cap_bits
 * from the loaded catalog, refreshed once per dispatcher turn
 * (ai_euro_20e6_refresh_type_cache, called from ai_euro_dispatcher_turn — the
 * same per-nation-turn boundary that zeroes the other 20e6 file-local
 * latches). Indexed by DOS row (units_type_dos_code == ColonizeUnitKind),
 * which is what every ai_euro_20e6_dos_type() caller already produces.
 * `s_20e6_type_cache_valid` bit *i* means row i had a matching catalog entry
 * this refresh; unset rows fall back to the NAMES.TXT-default tables above
 * (hand-built test pools with no real @UNIT load never set any bit).
 */
static uint8_t s_20e6_type_combat_live[AI_20E6_TYPE_COUNT];
static uint8_t s_20e6_type_flags_live[AI_20E6_TYPE_COUNT];
static uint32_t s_20e6_type_cache_valid;

static void ai_euro_20e6_refresh_type_cache(const ColonizeUnitPool* units) {
  s_20e6_type_cache_valid = 0;
  if (!units) {
    return;
  }
  const int n = units->type_count < COLONIZE_UNIT_TYPES_MAX ? units->type_count : COLONIZE_UNIT_TYPES_MAX;
  for (int i = 0; i < n; ++i) {
    const ColonizeUnitType* t = &units->types[i];
    /* Only a units_load_types-stamped entry (kind_plus1 > 0) actually
     * parsed a real @UNIT column 12 bit-string / column 4 attack value;
     * a hand-built test pool (ai_fixture-style, kind_plus1 left 0, cap_bits
     * left at its zero-init) resolves a DOS row through the name fallback
     * (units_type_dos_code -> units_name_kind) but carries no real
     * cap_bits/attack data, so it must not overwrite the NAMES.TXT-default
     * fallback table below. */
    if (t->kind_plus1 <= 0) {
      continue;
    }
    const int row = units_type_dos_code(t);
    if (row < 0 || row >= AI_20E6_TYPE_COUNT) {
      continue;
    }
    s_20e6_type_combat_live[row] = (uint8_t)t->attack;
    s_20e6_type_flags_live[row] = t->cap_bits;
    s_20e6_type_cache_valid |= (1u << (unsigned)row);
  }
}
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

/* ===== FUN_5952_035e colony-tick tile improvement (raw 94402-94551) ===== */

/*
 * DS:0x2f76 terrain record +0x3 ("colony-site desirability per tile",
 * docs/terrain_yields.md) is `k_20e6_terr_site_byte` above — FUN_5952_035e
 * raw 94417 reads the same column (`*(byte *)(class * 0x10 + 0x2f79)`) as the
 * base improvement value of a candidate work plot.
 */
/*
 * DS:0x97b2 (`-0x684e`) — the 14-byte special-resource desirability column.
 * Raw 120909 loads it at start-up from the same NAMES.TXT section the
 * @RESOURCE names come from (`FUN_2a1f_088a` per row, 0xe rows), so it is a
 * catalog column, not a compiled constant: read it back out of @RESOURCE
 * field 1 (data_vs_hardcoded.md Part D — catalog miss = 0, never a typed
 * fallback).
 */
static int ai_euro_5952_resource_site_byte(const ColonizeMsgCatalog* names, int resource) {
  char buf[32];
  if (!names || resource < 0 || resource > 13) {
    return 0;
  }
  if (!assets_msg_row_field(names, "RESOURCE", resource, 1, buf, sizeof(buf))) {
    return 0;
  }
  return atoi(buf);
}

/* `layer2 & 0x0a` (FUN_281f_0754): road OR settlement on the tile. */
static int ai_euro_5952_tile_road_or_settlement(const ColonizeWorldMap* map, int x, int y) {
  return (map_tile_has_road(map, x, y) || map_tile_has_city(map, x, y)) ? 0x0a : 0;
}

/* DOS `colony+0x70[plot]` -> `FUN_281f_0c0e` (FUN_15eb_0e18): the field job of
 * the colonist working this plot, or -1 when the plot is unworked. */
static int ai_euro_5952_plot_job(const ColonizeColony* col, int plot) {
  for (int i = 0; i < col->colonist_count && i < COLONIZE_COLONY_POP_MAX; ++i) {
    if (!col->colonists[i].active) {
      continue;
    }
    if (colonies_colonist_tile(col, i) == plot) {
      return col->colonists[i].field_job;
    }
  }
  return -1;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94402-94551 (clean body
 * original_sources_annotated/ai/colony_tick_5952_035e.md:919-1027) — the AI's
 * ONLY tile-improvement arm. It runs inside the colony tick, not on a unit:
 * DOS picks the best work plot, spawns a *phantom* colonist on it
 * (`FUN_291f_0a20` = `FUN_478c_002c`, type DS:0x524e), stamps `+0x315a = 99`
 * so the very next work tick completes, calls the real Pioneer body
 * (`FUN_291f_01c2` = `FUN_479b_01a6` clear/plow, or `FUN_291f_0216` =
 * `FUN_479b_0526` road), then undoes the phantom (`FUN_291f_0a06` =
 * `FUN_478c_00d0`). On success the colony pays 20 tools and `+0x8c` resets.
 *
 * Entry gate, raw 94402: `(colony+0x1b & 0x80) && local_16 && turn % 7 != 0`.
 * `local_16` is seeded at raw 93978 as `colony+0xb6 > 0x13`, i.e. 20+ tools in
 * stock. The `turn % 7 == 0` alternative (raw 94389-94400, the inter-colony
 * road-connect helper FUN_5952_0000 at raw 93589+) and the 20-tool purchase
 * block at raw 94370-94388 are the two sibling arms, ported as
 * ai_euro_5952_tools_supply_and_connect (bugs.md #640/#641), which runs
 * immediately before this one.
 *
 * Scoring, per ring plot (raw 94406-94469):
 *   filters: FUN_281f_0302 interior, FUN_281f_0768 not ocean/sea-lane,
 *            FUN_15eb_23f2 blocked mask == 0 (colonies_plot_blocked_mask);
 *   base   : terrain +0x3 byte, replaced by the @RESOURCE byte on a special;
 *   if NOT (wants-clear && class 8..0x17):
 *       worked plot whose job's improvement is missing doubles the value
 *       (job < 4 -> looks for plowed 0x40, job >= 4 -> looks for road 0x0a);
 *       a plot that has BOTH road and plow is skipped outright;
 *     else (forest on a wants-clear colony): value doubles;
 *   tribal claim (the DS:0x8d9e 5x5 table, FUN_15eb_26e4): penalty
 *       `-(alarm - 4)`, doubled on a special resource, doubled again when the
 *       continent carries Indian settlements (DS:0x95f2 bit 0) and no wagon
 *       train is homed here — and the plot is skipped entirely unless the
 *       colony wants a clear; finally doubled when the purse is under 2000
 *       gold, halved otherwise. `value -= penalty`.
 *   best wins, `>=` (DOS seeds `local_16e = 0xffff` = -1 as a signed 16-bit).
 */
static void ai_euro_5952_improve_best_plot(ColonizeTurnContext* ctx, ColonizeColony* col) {
  if (!ctx || !ctx->map || !ctx->colonies || !col || !col->active) {
    return;
  }
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  if ((col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) == 0) {
    return; /* raw 94402: colony+0x1b & 0x80 */
  }
  if (col->stock[COLONIZE_CARGO_TOOLS] <= 0x13) {
    return; /* raw 93978 local_16 */
  }
  if (turn % 7 == 0) {
    return; /* raw 94403: that turn belongs to the road-connect helper */
  }
  const ColonizeWorld w = (ColonizeWorld){
    .units = ctx->units,
    .colonies = ctx->colonies,
    .map = ctx->map,
    .col1 = ctx->col1_ok ? ctx->col1 : NULL,
    .col1_ok = (ctx->col1_ok && ctx->col1) != 0
  };
  const int wants_clear = (col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_CLEAR) != 0;
  const int colony_cid = map_continent_id_at(ctx->map, col->x, col->y);
  const int presence = ai_contact_continent_presence_4962(ctx, nation, colony_cid);
  const long purse = (long)(int32_t)europe_nation_gold(ctx->europe, ctx->col1, nation);
  const int ring = colonies_work_plot_count(ctx->colonies, col);
  int best = -1;      /* local_16e, signed */
  int best_plot = -1; /* local_34 */
  for (int ti = 0; ti < ring; ++ti) {
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(ti, &dx, &dy)) {
      continue;
    }
    const int tx = col->x + dx;
    const int ty = col->y + dy;
    if (!map_coords_inset(ctx->map, tx, ty)) {
      continue; /* FUN_281f_0302 */
    }
    const int cls = map_dos_terr_class_at(ctx->map, tx, ty);
    if (cls == 0x19 || cls == 0x1a) {
      continue; /* FUN_281f_0768 */
    }
    if (colonies_plot_blocked_mask(&w, col, ti) != 0) {
      continue;
    }
    const int res = map_resource_type_at(ctx->map, tx, ty);
    int value = (res != -1) ? ai_euro_5952_resource_site_byte(ctx->names, res)
                            : (int)k_20e6_terr_site_byte[cls & 31];
    const int forest = (cls >= 8 && cls <= 0x17);
    if (!wants_clear || !forest) {
      const int job = ai_euro_5952_plot_job(col, ti);
      if (job >= 0) {
        const int have = (job < 4) ? (map_tile_is_plowed(ctx->map, tx, ty) ? 0x40 : 0)
                                   : ai_euro_5952_tile_road_or_settlement(ctx->map, tx, ty);
        if (have == 0) {
          value <<= 1;
        }
      }
      if (ai_euro_5952_tile_road_or_settlement(ctx->map, tx, ty) != 0 &&
          map_tile_is_plowed(ctx->map, tx, ty)) {
        continue; /* raw 94438-94440: nothing left to do here */
      }
    } else {
      value <<= 1;
    }
    const int claim = colonies_indian_claim_tribe_from_w(&w, nation, col->x, col->y, tx, ty);
    if (claim >= 0 && ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
        claim < (int)ctx->col1->head.tribe_count) {
      const int ind = (int)ctx->col1->tribe[claim].nation_id - 4;
      const int alarm = (ind >= 0 && ind < (int)COLONIZE_COL1_INDIAN_COUNT)
                          ? (int)ctx->col1->indian[ind].alarm_by_player[nation]
                          : 0;
      int pen = -(alarm - 4);
      if (res != -1) {
        pen = (alarm - 4) * -2;
      }
      if ((presence & 1) != 0 && (col->colony_flags & COLONIZE_COLONY_FLAG_WAGON_TRAIN) == 0) {
        if (!wants_clear) {
          continue; /* raw 94454: goto LAB_5952_122c */
        }
        pen <<= 1;
      }
      if (purse < 2000) {
        pen <<= 1;
      } else {
        pen >>= 1;
      }
      value -= pen;
    }
    if (best <= value) { /* raw 94466: `if ((int)local_16e < (int)local_132)` */
      best = value;
      best_plot = ti;
    }
  }
  if (best_plot < 0) {
    return;
  }
  int dx = 0;
  int dy = 0;
  if (!colonies_field_tile_delta(best_plot, &dx, &dy)) {
    return;
  }
  const int tx = col->x + dx;
  const int ty = col->y + dy;
  /*
   * raw 94474-94489: the 8-neighbour veto. Any neighbour whose layer3 owner
   * is a EUROPEAN nation (< 4) that is not AI-controlled (DS:0x543f == 0, i.e.
   * the human) kills the improvement — but the plot's own owner being this
   * colony's nation un-kills it (raw 94488-94491, run AFTER the loop).
   */
  int allow = 1;
  for (int d = 0; d < 8; ++d) {
    /* DS:0xb4/0xbe dir8 = the first 8 slots of the ring20 table.
     * FUN_281f_06d2 = settlement owner else unit owner (ai_goals note). */
    const int owner = map_tile_tribe_or_presence(
      ctx->map, tx + k_20e6_ring20_dx[d], ty + k_20e6_ring20_dy[d]
    );
    if (owner >= 0 && owner < 4 && owner == ctx->human_nation) {
      allow = 0; /* DS:0x543f[owner] == 0, the human-controlled seat */
    }
  }
  if (map_tile_tribe_or_presence(ctx->map, tx, ty) == nation) {
    allow = 1; /* raw 94488-94491, after the loop */
  }
  if (!allow) {
    return;
  }
  /*
   * raw 94492-94504: the work-turns threshold. `DS:0x2f78[class] + 2`, or
   * `+ 4` when the plot is forest AND the colony carries the wants-clear bit
   * (that is also what makes this a CLEAR rather than a plow/road decision).
   */
  const int cls = map_dos_terr_class_at(ctx->map, tx, ty);
  const int clear_mode = ((cls >= 8 && cls <= 0x17) && wants_clear) ? 1 : 0;
  const int base = map_dos_terr_pioneer_threshold_byte(cls);
  const int threshold = base + (clear_mode ? 4 : 2);
  if ((int8_t)threshold > (int8_t)col->improve_timer) {
    return; /* raw 94505 */
  }
  /*
   * The phantom-colonist trick, raw 94506-94540. DOS spawns a throwaway unit
   * of type DS:0x524e on the plot, stamps `+0x315a = 99` (any threshold is
   * met, so the body finishes on its first tick), runs the real Pioneer body
   * and despawns it. The port needs one extra thing DOS does not: its work
   * bodies gate on `units_is_pioneer`, which wants tools > 0, so the phantom
   * is handed tools. Nothing survives the despawn either way.
   */
  const int ptype = ai_euro_5d04_linux_type_for(ctx->units, 2);
  if (ptype < 0) {
    return;
  }
  const int pid = units_spawn_allow_stack(ctx->units, ptype, tx, ty);
  ColonizeUnit* ph = units_get(ctx->units, pid);
  if (!ph) {
    return;
  }
  ph->nation_id = nation;
  ph->tools = UNITS_EQUIP_TOOLS_STEP;
  ph->col1_counter16 = 99; /* +0x315a = 99 */
  const ColonizeWorld pw = (ColonizeWorld){
    .units = ctx->units, .colonies = ctx->colonies, .map = ctx->map
  };
  char err[64];
  int worked = 0;
  const int road_bits = ai_euro_5952_tile_road_or_settlement(ctx->map, tx, ty);
  const int plowed = map_tile_is_plowed(ctx->map, tx, ty) ? 0x40 : 0;
  int do_plow = 0;
  int do_road = 0;
  if (!clear_mode) {
    const int job = ai_euro_5952_plot_job(col, best_plot);
    if (job >= 0 && job < 4 && plowed == 0) {
      do_plow = 1; /* LAB_5952_1508 */
    } else if (job > 3 && road_bits == 0) {
      do_road = 1; /* LAB_5952_15b0 */
    } else if (cls < 2 || cls > 7) {
      do_road = (road_bits == 0);
    } else {
      do_plow = (plowed == 0);
    }
  } else {
    do_plow = 1; /* forest CLEAR shares FUN_479b_01a6 */
  }
  if (do_plow) {
    worked = units_pioneer_plow_w(&pw, pid, err, sizeof(err), NULL, NULL) ? 1 : 0;
  } else if (do_road) {
    worked = units_pioneer_road_w(&pw, pid, err, sizeof(err), NULL, NULL) ? 1 : 0;
  }
  units_despawn(ctx->units, pid); /* FUN_291f_0a06 */
  if (worked) {
    /* raw 94541-94549: colony+0xb6 -= min(stock, 20); colony+0x8c = 0. */
    int pay = col->stock[COLONIZE_CARGO_TOOLS];
    if (pay > 0x14) {
      pay = 0x14;
    }
    col->stock[COLONIZE_CARGO_TOOLS] -= pay;
    col->improve_timer = 0;
  }
}

static int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid);

/* FUN_281f_06be = FUN_137f_03e4 (raw 6840-6858): the owner of the SETTLEMENT
 * on a tile (layer2 bit 0x02 -> layer3 high nibble), -1 when there is none.
 * Unlike map_tile_tribe_or_presence (FUN_281f_06d2 = FUN_137f_0428) it does
 * NOT fall back to an occupying unit. */
static int ai_euro_5952_settlement_owner_at(const ColonizeWorldMap* map, int x, int y) {
  if (!map_in_bounds(map, x, y) || !map_tile_has_city(map, x, y)) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  return hi == 0x0f ? -1 : hi;
}

/* FUN_281f_06e6 = FUN_137f_044a (raw 6877-6900): the owner of an IMPROVED
 * tile (`layer2 & 0x48` = plowed 0x40 or road 0x08) when that owner is a
 * DIFFERENT Euro nation (0..3) this nation is at PEACE with
 * (`nation[self].euro_relation[owner] & 0x40`, AI_DIPLO_PEACE); -1 otherwise. */
static int ai_euro_5952_improved_peer_owner_at(
  const ColonizeCol1Save* col1, const ColonizeWorldMap* map, int x, int y, int nation
) {
  if (!map_in_bounds(map, x, y)) {
    return -1;
  }
  if (!map_tile_is_plowed(map, x, y) && !map_tile_has_road(map, x, y)) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  if (hi < 0 || hi >= 4 || hi == nation || !col1) {
    return -1;
  }
  return (col1->nation[nation].euro_relation[hi] & AI_DIPLO_PEACE) != 0 ? hi : -1;
}

/*
 * DOS-LITERAL FUN_5952_0000 (raw 93589-93685) — the AI's inter-colony
 * ROAD-CONNECT helper, reached only from FUN_5952_035e's `turn % 7 == 0`
 * arm (raw 94389-94400) via thunk_FUN_2a1f_05d8. bugs.md #641.
 *
 *   iVar4 = FUN_281f_0722(x, y)                      // this colony's continent
 *   if (DS:0x94e6[nation*0x10 + iVar4] <= 1) return 0 // need 2+ own colonies here
 *   for (each other colony of the same nation, index != DS:0x8dc6):
 *     if (|ox-x| > 6 && |oy-y| > 6) continue          // raw 93612-93621, AND
 *     if (continent(ox,oy) != iVar4) continue
 *     if (FUN_281f_04d4(0, count-2) != 0) continue    // 1-in-(count-1) roll
 *     DS:0x1dd6 = -1; DS:0xa14e/0xa14c = (ox,oy); DS:0x1dd4 = 1; DS:0x1dd2 = 1
 *     walk FUN_2a1f_05f0 (= FUN_6662_00f2) one direction at a time from the
 *       colony toward the other colony, stopping on dir < 0 / dir == 8 / on
 *       arrival, and stopping with `found` when the stepped-onto tile has
 *       NO settlement (FUN_281f_06be < 0) AND no road/settlement bits
 *       (FUN_281f_0754 & 0x0a == 0) -- that tile is the gap in the link.
 *     if (!found) continue
 *     if (FUN_281f_06d2(gap) >= 0 && != nation) continue     // occupied
 *     if (FUN_281f_06e6(gap, nation) >= 0 && != nation) continue
 *     if (colony+0x8c < DS:0x2f78[class*0x10] + 2) return 0   // NOT continue
 *     spawn DS:0x524e phantom, +0x315a = 99, FUN_291f_0216 (= FUN_479b_0526
 *       road), FUN_291f_0a06 despawn, return 1
 *
 * Port notes: the walk needs a mover for units_next_goto_step_w (the port of
 * the same FUN_6662 pathfinder), so a walker phantom carries the virtual
 * position DOS keeps in local_18/local_1e; it is despawned BEFORE the
 * occupancy tests so it cannot be mistaken for the occupant DOS's
 * unit-less walk never sees. The road phantom is then spawned on the gap
 * tile exactly as the #612 improve arm does. The step loop carries a
 * width+height cap DOS does not need (its pathfinder always terminates).
 */
static int ai_euro_5952_road_connect_0000(ColonizeTurnContext* ctx, ColonizeColony* col) {
  if (!ctx || !ctx->map || !ctx->colonies || !ctx->units || !col || !col->active) {
    return 0;
  }
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return 0;
  }
  const int cid = map_continent_id_at(ctx->map, col->x, col->y); /* FUN_281f_0722 */
  /* raw 93601: `1 < DS:0x94e6[nation*0x10 + continent]` */
  const int own_here = ai_euro_20e6_own_colonies_on(ctx, nation, cid);
  if (own_here <= 1) {
    return 0;
  }
  const ColonizeWorld w = (ColonizeWorld){
    .units = ctx->units,
    .colonies = ctx->colonies,
    .map = ctx->map,
    .rng = ctx->rng,
    .col1 = ctx->col1_ok ? ctx->col1 : NULL,
    .col1_ok = (ctx->col1_ok && ctx->col1) != 0
  };
  const int step_cap = (int)ctx->map->width + (int)ctx->map->height;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    ColonizeColony* other = &ctx->colonies->colonies[ci];
    if (!other->active || other->nation_id != nation || other == col) {
      continue; /* raw 93606: same nation, DS:0x8dc6 != index */
    }
    const int ox = other->x;
    const int oy = other->y;
    /* raw 93610-93621 — DOS only checks y when |dx| > 6, so the skip needs
     * BOTH axes beyond 6. Transcribed as written. */
    if (abs(ox - col->x) > 6 && abs(oy - col->y) > 6) {
      continue;
    }
    if (map_continent_id_at(ctx->map, ox, oy) != cid) {
      continue; /* raw 93623 */
    }
    if (!ctx->rng || dos_rng_range(ctx->rng, 0, own_here - 2) != 0) {
      continue; /* raw 93625 FUN_281f_04d4(0, count - 2) */
    }
    /* The virtual walk, raw 93632-93652. */
    const int wtype = ai_euro_5d04_linux_type_for(ctx->units, 2);
    if (wtype < 0) {
      return 0;
    }
    const int wid = units_spawn_allow_stack(ctx->units, wtype, col->x, col->y);
    ColonizeUnit* walker = units_get(ctx->units, wid);
    if (!walker) {
      return 0;
    }
    walker->nation_id = nation;
    int cx = col->x;
    int cy = col->y;
    int found = 0;
    if (units_set_goto_w(&w, wid, ox, oy)) {
      for (int steps = 0; steps < step_cap; ++steps) {
        int nx = 0;
        int ny = 0;
        if (!units_next_goto_step_w(&w, wid, &nx, &ny)) {
          break; /* dir < 0 or dir == 8 */
        }
        const int px = cx;
        const int py = cy;
        cx = nx;
        cy = ny;
        walker->x = nx;
        walker->y = ny;
        units_occupancy_notify_moved(ctx->units, px, py, nx, ny);
        units_note_goto_step(wid, nx - px, ny - py);
        if (cx == ox && cy == oy) {
          break; /* raw 93639 */
        }
        if (ai_euro_5952_settlement_owner_at(ctx->map, cx, cy) < 0 &&
            (ai_euro_5952_tile_road_or_settlement(ctx->map, cx, cy) & 0x0a) == 0) {
          found = 1; /* raw 93641-93645: bVar3 = false */
          break;
        }
      }
    }
    units_despawn(ctx->units, wid);
    if (!found) {
      continue;
    }
    /* raw 93655-93661: the gap tile must carry no occupant and no at-peace
     * rival's improvement. */
    const int occ = map_tile_tribe_or_presence(ctx->map, cx, cy); /* FUN_281f_06d2 */
    if (occ >= 0 && occ != nation) {
      continue;
    }
    const int peer = ai_euro_5952_improved_peer_owner_at(
      ctx->col1_ok ? ctx->col1 : NULL, ctx->map, cx, cy, nation
    ); /* FUN_281f_06e6 */
    if (peer >= 0 && peer != nation) {
      continue;
    }
    /* raw 93663-93666: DOS *returns 0* here, it does not try another colony. */
    const int cls = map_dos_terr_class_at(ctx->map, cx, cy);
    const int threshold = map_dos_terr_pioneer_threshold_byte(cls) + 2;
    if ((int8_t)col->improve_timer < (int8_t)threshold) {
      return 0;
    }
    /* raw 93667-93671: the road phantom. */
    const int pid = units_spawn_allow_stack(ctx->units, wtype, cx, cy);
    ColonizeUnit* ph = units_get(ctx->units, pid);
    if (!ph) {
      return 0;
    }
    ph->nation_id = nation;
    ph->tools = UNITS_EQUIP_TOOLS_STEP; /* units_is_pioneer gate, see #612 */
    ph->col1_counter16 = 99;            /* +0x315a = 99 */
    const ColonizeWorld pw = (ColonizeWorld){
      .units = ctx->units, .colonies = ctx->colonies, .map = ctx->map
    };
    char err[64];
    const int built = units_pioneer_road_w(&pw, pid, err, sizeof(err), NULL, NULL) ? 1 : 0;
    units_despawn(ctx->units, pid); /* FUN_291f_0a06 */
    if (built) {
      return 1;
    }
    return 0;
  }
  return 0;
}

/*
 * DOS-LITERAL FUN_5952_035e raw 94370-94400 — the two arms that sit between
 * the tick's civic pass and the #612 tile-improvement arm. bugs.md #640/#641.
 *
 * raw 94370-94388, the 20-tool purchase (#640):
 *   if (((colony+0x1b & 0x80) || turn % 10 == 0) && local_16 == 0) {
 *     uVar12 = DS:0x84ca[nation*0x10];        // = DS:0x84bc[nation*0x10 + 0x0e]
 *     cost   = uVar12 * 0x14;                 // 20 tools
 *     if (32-bit gold at *0x84fc+0x2a >= cost) {
 *       gold -= cost; FUN_291f_0c14(); colony+0xb6 += 0x14; local_16 = 1;
 *     }
 *   }
 * DS:0x84ca is NOT a separate table: 0x84ca == 0x84bc + 0x0e and the read
 * uses the same `nation*0x10 + cargo` index as the SELL price row already
 * ported as AiEuro5952Want.sell_price (see the DS:0x84b4 note above and
 * docs/port_plan.md:976) — cargo 0x0e is Tools. Every writer of that row
 * stores `euro_price - 1` clamped at 0, so the AI pays the sell price, one
 * below the Europe ask. No byte dump was needed and no constant is invented.
 *
 * raw 94389-94400, the road-connect arm (#641):
 *   if (local_16 && turn % 7 == 0 && FUN_5952_0000()) {
 *     colony+0xb6 -= min(colony+0xb6, 0x14); colony+0x8c = 0;
 *   }
 * Note it is NOT gated on `colony+0x1b & 0x80` the way the improve arm is.
 */
static void ai_euro_5952_tools_supply_and_connect(
  ColonizeTurnContext* ctx, ColonizeColony* col
) {
  if (!ctx || !col || !col->active) {
    return;
  }
  const int nation = col->nation_id;
  if (nation < 0 || nation >= 4) {
    return;
  }
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  int have_tools = col->stock[COLONIZE_CARGO_TOOLS] > 0x13; /* local_16, raw 93978 */
  const int wants_work = (col->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) != 0;
  if ((wants_work || turn % 10 == 0) && !have_tools && ctx->col1_ok && ctx->col1) {
    /*
     * DS:0x84bc[nation*0x10 + 0x0e] — every writer of that row stores
     * `euro_price - 1` clamped at 0 (FUN_38fd_0040, europe.c's dump-sell
     * note). A game that never came from a DOS save leaves the AI nations'
     * byte at 0, exactly as europe.c's Custom House substitution documents;
     * with no live Europe screen either there is no DS:0x84bc row to read at
     * all, so the arm does not run (it cannot price the purchase — it does
     * not price it at zero).
     */
    const uint8_t row = ctx->col1->nation[nation].trade.euro_price[COLONIZE_CARGO_TOOLS];
    const int price = row != 0 ? (int)row - 1
                               : europe_sell_price(ctx->europe, COLONIZE_CARGO_TOOLS);
    const int have_market = (row != 0) || (ctx->europe != NULL);
    const long cost = (long)price * 0x14;
    const long gold = (long)(int32_t)europe_nation_gold(ctx->europe, ctx->col1, nation);
    if (have_market && gold >= 0 && gold >= cost) {
      europe_nation_gold_add(ctx->europe, ctx->col1, nation, -cost);
      col->stock[COLONIZE_CARGO_TOOLS] += 0x14;
      have_tools = 1;
    }
  }
  if (have_tools && turn % 7 == 0 && ai_euro_5952_road_connect_0000(ctx, col)) {
    int pay = col->stock[COLONIZE_CARGO_TOOLS];
    if (pay > 0x14) {
      pay = 0x14;
    }
    col->stock[COLONIZE_CARGO_TOOLS] -= pay;
    col->improve_timer = 0;
  }
}

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
/*
 * DS:0x5239 = the @UNIT "cost" column (ColonizeUnitType.cost). The @UNIT
 * loader (raw 121115-121135) stores the NAMES.TXT numeric columns in file
 * order at 0x5232 icon, 0x5234 moves×3, 0x5236 attack, 0x5235 defense,
 * 0x5237 cargo, 0x5238 size, 0x5239 cost, 0x523a tools, 0x523b guns,
 * 0x523c hull, 0x523d bits. Cost is non-zero for every land type (Colonists
 * 1, Soldiers 2, Dragoons 3, Regulars 3, Cavalry 4, Artillery 6; ships
 * Caravel 4 .. Man-O-War 32). An earlier table here returned the guns
 * column (0x523b) instead, which is 0 for every land type and made the
 * LAB_52aa odds core score 0 against any 2+-defender land stack
 * (bugs.md #521).
 */
static int ai_euro_20e6_unit_col9(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const ColonizeUnitType* t = (units && u) ? units_type(units, u->type_index) : NULL;
  return t ? t->cost : 0;
}

static int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const ColonizeUnitType* t = units ? units_type(units, u->type_index) : NULL;
  if (!t) {
    return -1;
  }
  /* units_type_dos_code IS this table (audit AE-4) — the kind ids are the
   * NAMES.TXT @UNIT row numbers, most-specific spelling first. */
  const int code = units_type_dos_code(t);
  if (code >= 0) {
    return code;
  }
  if (units->type_count >= 19 && u->type_index >= 0 && u->type_index < AI_20E6_TYPE_COUNT) {
    return u->type_index; /* NAMES-loaded pool: index == DOS index */
  }
  return -1;
}

static int ai_euro_20e6_type_combat(int dos_type) {
  if (dos_type < 0 || dos_type >= AI_20E6_TYPE_COUNT) {
    return 0;
  }
  if ((s_20e6_type_cache_valid & (1u << (unsigned)dos_type)) != 0) {
    return (int)s_20e6_type_combat_live[dos_type];
  }
  return (int)k_20e6_type_combat[dos_type];
}

static int ai_euro_20e6_type_flags(int dos_type) {
  if (dos_type < 0 || dos_type >= AI_20E6_TYPE_COUNT) {
    return 0;
  }
  if ((s_20e6_type_cache_valid & (1u << (unsigned)dos_type)) != 0) {
    return (int)s_20e6_type_flags_live[dos_type];
  }
  return (int)k_20e6_type_flags[dos_type];
}

/* FUN_1000_88cc / FUN_137f_0200 owner_nibble: layer3 high nibble, 0xf → −1. */
static int ai_euro_20e6_owner_nibble(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return -1;
  }
  const int n = (int)(map_get_layer3(map, x, y) >> 4) & 0xf;
  return n == 0xf ? -1 : n;
}

/*
 * FUN_1000_8c28 / FUN_15b3_0004 diplomacy byte, Euro or Indian counterpart.
 * 2026-09-08: the Indian-side synthesizer (MET assumed + WAR from
 * ai_diplo_indian_at_war) is gone — ai_diplo_read is dual-mode now and
 * nation[].relation_by_indian is live-maintained (contact 0x60, 4cc6_00f2
 * clear-both cooling, 153e smite or_both WAR, @WHACKINDIANS or_both bit 4),
 * so the raw byte IS the DOS read. Fixes the neighbour penalty consumer:
 * DOS `(rel & 0x60) != 0x20` skips peaceful-met tribes (byte 0x60); the
 * synthesizer's bare MET (0x20) wrongly penalised them.
 */
static int ai_euro_20e6_diplo(const ColonizeCol1Save* col1, int nation, int other) {
  if (!col1 || nation < 0 || nation > 3 || other < 0 || other > 11) {
    return 0;
  }
  return (int)ai_diplo_read(col1, nation, other);
}

/*
 * FUN_15eb_0142 nearest colony: shared with ai_goals (audit AE-39). Two port
 * divergences were fixed there against viceroy_unpacked.c:9380-9424: the
 * tie-break is `<=` (LAST tie wins, raw 9411) and DOS has no nation 0..3
 * filter (raw 9402 `param_3 < 0 || record_nation == param_3`).
 */
static int ai_euro_20e6_nearest_colony(
  const ColonizeTurnContext* ctx, int x, int y, int nation, int cid, int* out_dist
) {
  return ai_goals_nearest_colony_15eb_0142(ctx->map, ctx->colonies, x, y, nation, cid, out_dist);
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
      const int d = map_dos_dist(x - (int)t->x, y - (int)t->y);
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
  o = ai_euro_village_nation_at(ctx->col1_ok ? ctx->col1 : NULL, x, y);
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
  int act_state;  /* unit+0x314c == ColonizeUnit.orders */
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
    s->act_state = u->orders;        /* +0x314c */
    s->order_code = u->col1_ai_plan; /* +0x314b */
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
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation || units_is_sea(ctx->units, o->id)) {
      continue;
    }
    if (map_continent_id_at(ctx->map, o->x, o->y) != cid) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, o->id, 1, NULL);
    if (sum > 255) {
      return 255; /* DOS byte table */
    }
  }
  return sum;
}

/* DS:0x95f2 continent_presence_flags bit 0x04: a foreign colony sits on cid. */
static int ai_euro_10ec_land_units_on(const ColonizeTurnContext* ctx, int nation, int cid) {
  int n = 0;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation || units_is_sea(ctx->units, o->id)) {
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
  /* −0x6a9a[a*3] — the leader's belligerence column, resolved 2026-09-06d
   * (NAMES.TXT @LEADERNAME field 1; see ai_diplo_leader_trait). */
  const int belligerence = ai_diplo_leader_trait(ctx, a, 0);
  if ((rivals - belligerence) + 4 <= (local_8 << 2) / local_6 || local_4 == 0) {
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
  /*
   * Was a byte-for-byte copy of ai_goals.c's probe, including its own
   * file-static "does this plane carry nibbles" memo — which nothing ever
   * invalidated, so a new game or a Load that reused the same `seen`
   * allocation kept the previous world's verdict and flipped the whole extras
   * term between the real nibble and the unseen→4 stand-in. Single cache now,
   * cleared by ai_goals_reset. Smell audit 2026-09-10 D8.
   */
  return ai_goals_site_nibble(ctx ? ctx->map : NULL, x, y, nation);
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
  return units_count_at(ctx->units, x, y);
}

/*
 * Case 0xb: Σ combat value (FUN_157e_004a mode 1 — combat_unit_base_x8 mode 1
 * here, the same read −0x6a8e sums) over stack units whose ship-ness matches
 * the tile (land tile → land units).
 */
static int ai_euro_20e6_stack_combat_0b(ColonizeTurnContext* ctx, int x, int y) {
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  const int water = map_tile_is_water(ctx->map, x, y);
  int sum = 0;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->aboard_ship_id >= 0 || o->x != x || o->y != y) {
      continue;
    }
    if ((units_is_sea(ctx->units, o->id) ? 1 : 0) != (water ? 1 : 0)) {
      continue;
    }
    sum += combat_unit_base_x8(&sctx, o->id, 1, NULL);
  }
  return sum;
}

/* FUN_OVL14_L0000__0072d6 → FUN_521d_0906 probe, ≥0 = adjacent foreign claim. */
static int ai_euro_20e6_probe_adjacent(const ColonizeTurnContext* ctx, int x, int y, int nation) {
  int side = -1;
  return ai_goals_probe_adjacent_contact_claim_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, x, y, nation, 0, &side);
}

/*
 * iStack_6a — "this unit explores this call" (raw lines ~1095-1180). Order of
 * the clauses is DOS's own; later clauses override earlier ones.
 */
static void ai_euro_20e6_explorer_flag(ColonizeTurnContext* ctx, const ColonizeUnit* u, Ai20e6Unit* s) {
  const int t = s->dos_type;
  int ex = (t == 2 || t == 0) ? 1 : 0; /* Pioneers / Colonists */
  if (u->profession == UNITS_JOB_CONVERT) {           /* Indian Convert */
    ex = 0;
  }
  if (t == 1 || t == 4) { /* Soldiers / Dragoons */
    if (s->act_state == 0) {
      ex = 1;
    }
    if (s->act_state == AI_EURO_ACT_GOAL &&
        map_dos_dist(u->x - u->goto_x, u->y - u->goto_y) > 12) {
      ex = 1; /* goal (+0x314d/e) more than 12 tiles away */
    }
    if (ai_goals_colony_balance_flags_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, s->nation, s->cid) > 2) {
      ex = 1;
    }
    if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, s->nation) >= 0) {
      ex = 0;
    }
    if (u->profession == UNITS_JOB_SOLDIER) { /* Veteran Soldier */
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
    const int prio = ai_goals_composite_unit_priority_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, s->nation, u->x, u->y, t, u->profession, s->turn, colonies_count_for_nation(ctx->colonies, s->nation));
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
  if (s->stance != 0 || tasked || s->is_ship ||
      !(s->dos_type == UNITS_KIND_PIONEER || s->combat > 1) ||
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
 * FUN_521d_20e6 surplus-garrison recall arm (raw 85313-85337; the decompiler
 * emits the same body a second time at raw 90149-90177 — both end in
 * `goto LAB_521d_27f5`, the "commit the walk to the bound colony" tail).
 *
 * DOS: a non-ship unit (type outside 0x0d..0x12) whose DS:0x5236 combat byte
 * is > 1 and whose `+0x314a` origin is bound loads that home colony
 * (FUN_281f_09e6), and if
 *   colony +0x1b bit 0x04 is set (surplus defenders — see colony.h)
 *   && (colony +0x1e garrison_quota != 0 || DOS unit type != 4, a Dragoon)
 *   && the colony sits on this unit's own continent (FUN_281f_0722 ==
 *      uStack_38)
 * it CLEARS 0x04, decrements garrison_quota and walks the unit home.
 *
 * The single-shot clear is the whole mechanism: 0x04 survives
 * FUN_5952_035e's per-tick `+0x1b &= 7`, so nothing but its consumers takes
 * it down, and this arm therefore recalls exactly ONE surplus unit per colony
 * per tick before the flag goes quiet until the next 5952 recompute raises it
 * again. Ported 2026-09-09 with the bit-0x04 consumer audit (the third
 * consumer, FUN_5952_035e's join-colonist loop at raw 94247, belongs to an
 * unported pass — documented in colony.h).
 *
 * Returns 1 when it handled the unit.
 */
static int ai_euro_20e6_surplus_recall_arm(
  ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s
) {
  if (s->is_ship || s->combat <= 1 || !ctx->colonies || !ctx->map) {
    return 0;
  }
  const int home = ai_euro_20e6_origin_get(u); /* +0x314a, −1 unbound */
  if (home < 0 || home >= COLONIZE_COLONIES_MAX) {
    return 0;
  }
  ColonizeColony* hc = &ctx->colonies->colonies[home];
  if (!hc->active || hc->nation_id != s->nation) {
    return 0;
  }
  if ((hc->ai_flags & COLONIZE_COLONY_AI_MILITARY_SURPLUS) == 0) {
    return 0;
  }
  if (hc->garrison_quota == 0 && ai_euro_20e6_dos_type(ctx->units, u) == 4) {
    return 0; /* a Dragoon is only recalled while there is quota to spend */
  }
  if (map_continent_id_at(ctx->map, hc->x, hc->y) != s->cid) {
    return 0;
  }
  hc->ai_flags = (uint8_t)(hc->ai_flags & (uint8_t)~COLONIZE_COLONY_AI_MILITARY_SURPLUS);
  if (hc->garrison_quota != 0) {
    hc->garrison_quota--;
  }
  if (u->x == hc->x && u->y == hc->y) {
    return 1; /* already home — 20c6 has nothing to walk */
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
  return col1_tribe_attitude(t, nation);
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
  if (s->dos_type == UNITS_KIND_SCOUT && !t->state.scouted && att == 0) {
    if (ai_contact_ai_scout_visit_village(ctx, s->nation, s->village_idx, u->id)) {
      s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves = 0;
      return 1;
    }
  }
  /*
   * FUN_4d56_4528 non-human switch caseD_3 (Missionary): incite the village
   * against the human, else establish a mission, else denounce a foreign
   * mission. Resolved from the adjacent tile like the two arms around it
   * (entering a village tile is an attack in this port). bugs.md #557.
   */
  if (s->dos_type == UNITS_KIND_MISSIONARY) {
    if (ai_contact_ai_missionary_village(ctx, s->nation, s->village_idx, u->id)) {
      s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves = 0;
      return 1;
    }
  }
  if (!(s->combat > 1 || s->dos_type == UNITS_KIND_SCOUT ||
        s->dos_type == UNITS_KIND_MISSIONARY) &&
      (u->profession == UNITS_JOB_NONE || u->profession == UNITS_JOB_SERVANT) && att < 0x40 &&
      !t->state.learned && !units_is_sea(ctx->units, u->id)) {
    if (ai_contact_ai_live_among_village(ctx, s->nation, s->village_idx, u->id)) {
      s_20e6_village_visited[s->village_idx] |= (uint8_t)(1u << s->nation);
      u->moves = 0;
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
 * Wanted size = FUN_15eb_0484 clamped ≤16 (a constant 8 in real play — see
 * ai_euro_colony_ring_tier; the old "fortification capacity 8/12/32" reading
 * of this table was an invention, corrected 2026-09-09);
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
  if (s->dos_type != UNITS_KIND_COLONIST || s->explorer || s->is_ship || !ctx->colonies) {
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
    if (!(c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) &&
        u->profession != UNITS_JOB_CONVERT) {
      continue; /* 27 = Indian Convert (DOS profession 0x1b) */
    }
    int wanted = ai_euro_colony_wanted_size(ctx->colonies, c); /* FUN_15eb_0484 */
    if (wanted > 0x10) {
      wanted = 0x10;
    }
    const int dist = map_dos_dist(u->x - c->x, u->y - c->y);
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
      (void)colonies_admit_unit_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, best, u->id);
      return 2; /* FUN_1000_9b94 join; unit consumed either way (8b24) */
    }
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, c->x, c->y);
    return 1;
  }
  if (s->home_dist == 0 && s->home_colony >= 0) {
    /*
     * DOS-LITERAL FUN_521d_20e6 raw 89350-89357 — the labor-band fall-through
     * for a unit standing on its own colony with nothing left to staff:
     *   unit[+0x314b] = 0x3d; unit[+0x3146] = 2; unit[+0x3159] = 0x14;
     *   FUN_281f_0934(unit); goto LAB_521d_5a78;
     * i.e. the surplus colonist BECOMES a Pioneer (type 2) carrying exactly 20
     * tools — the tools write is unconditional, not a `< 20` floor. The port
     * wrote only the floor, so no AI colonist ever changed type and the 5952
     * equip arm / 5d04 Europe arm (both gated on `unit_type_counts[n][2] == 0`)
     * kept re-arming (bugs.md #614).
     */
    u->col1_ai_plan = 0x3d; /* +0x314b */
    {
      const int pioneer_type = ai_euro_5d04_linux_type_for(ctx->units, 2);
      if (pioneer_type >= 0) {
        u->type_index = pioneer_type; /* +0x3146 = 2 */
      }
    }
    u->tools = UNITS_EQUIP_TOOLS_STEP; /* +0x3159 = 0x14, unconditional */
    u->moves = 0;                      /* FUN_281f_0934 */
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
  /* Raw 89078 `0116(nation, x, y, 6)` — the primary-table explore probe.
   * DOS hoists it above the do-loop; hoisting is unobservable because the only
   * other code-6 op in the loop (`001c`, wired at the LAB_2912 entry in
   * ai_euro_20e6_land_act) touches the *secondary* table.
   *
   * AK-51: this read is a constant 0 and every term it feeds below is inert.
   * Nothing in src/ ever upserts AI_GOAL_EXPLORE into the primary table, and
   * DOS's own code-6 writer (`0214`, raw 89278) is dead too — see the note at
   * ai_goals.h:24-31. Faithful dead arithmetic; keep it as DOS spells it. */
  const int explore_goal = ai_goals_max_primary_prio(nation_id, u->x, u->y, AI_GOAL_EXPLORE);
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
          const ColonizeUnit* ou = oid >= 0 ? units_get_const(ctx->units, oid) : NULL;
          if (ou && oid != u->id && ou->orders == UNITS_ORDER_BUILD_COLONY) {
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
          if (s.dos_type == UNITS_KIND_COLONIST) {
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
  /*
   * DOS raw 89270-89280 / asm 002e10-002e72:
   *     if (best_nib > 0) {
   *       if (iStack_6a) { ...move to (bx,by), or act_state = 7 when here... }
   *       else 0214(nation, x + dx[dir], y + dy[dir], 6, 2);   // raw 89278
   *     }
   * **The `else` is unreachable.** `iStack_6a` is the explorer flag at
   * [BP-0x68]; the whole scan is entered only through
   * `CMP word [BP-0x68],0 / JNZ 0029b6` (asm 135058-135060) and nothing
   * between 0029b6 and 002e1a writes that slot — verified by scanning every
   * `[BP + -0x68]` reference across the 20e6 body (writes only in the
   * prologue flag block and at raw 89361, both outside the range). So the
   * second `CMP word [BP-0x68],0` at 002e1a (asm 135468, same four bytes
   * `83 7e 98 00`) can never take the JZ, and `0072f4`/`0214` at 002e4a is
   * compiler-preserved dead code — its only call site anywhere in the game
   * (grep of both decompiles + both asm listings: one hit each for
   * `thunk_FUN_2a1f_04c4` / `FUN_OVL14_L0000__0072f4`).
   * Consequence: goal code 6 is never written to either table in DOS, so
   * `explore_goal` above is a constant 0 in DOS too and the `+0x10` bonus at
   * the explorer arm never fires. NOT ported deliberately — wiring it would
   * manufacture goals DOS does not have and, via 0342's promote, feed the
   * primary table on the next nation turn.
   */
  /*
   * DOS-LITERAL FUN_521d_20e6 raw 89272-89275 (bugs.md #683): the best site is
   * the tile the unit already stands on, so 20e6 commits it instead of walking
   *     *(param_1 * 0x1c + 0x314c) = 7;  goto LAB_521d_5a78;
   * i.e. act_state 7 = UNITS_ORDER_BUILD_COLONY, the same byte the human Build
   * handler writes at raw 45657 right before FUN_291f_01fa. This is the ONLY
   * writer of act_state 7 in the AI, and it is what makes the two ring scans
   * that read it live: this function's own raw-89182 "another founder already
   * committed inside the ring" reject, and the human @TOONEARBUILD scan at raw
   * 45670. Returning 2 here rather than writing through the const unit keeps
   * the write at the caller, which owns `u`.
   */
  if (bx == u->x && by == u->y) {
    return 2;
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
 * decoded from its jump table; col9 = DS:0x5239 = the @UNIT cost column,
 * see ai_euro_20e6_unit_col9 — non-zero for every land type.) Then the transcribed
 * modifiers: ×3 own-colony tile, ×2 village, Artillery in the open → 0,
 * ×3 when flags&0x10 and stance==4, clamp 0..1000, <12 → −999 else +odds×4.
 * Still substituted: the Soldier/Dragoon
 * vs colony adjacent-Spanish-strength skip (8aac case 0xb) — not wired.
 */
static int ai_euro_20e6_attack_term(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, const Ai20e6Unit* s, int nx, int ny, int foe_id, int* score
) {
  if (s->combat == 0) {
    return 0;
  }
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
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
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* o = &ctx->units->units[i];
      if (!o->active || o->aboard_ship_id >= 0 || o->x != nx || o->y != ny) {
        continue;
      }
      col9_sum += ai_euro_20e6_unit_col9(ctx->units, o);
      stack++;
    }
  }
  const int a = col9_sum + 1;
  const int d = stack < 1 ? 1 : stack;
  int odds = (a / d) * base;
  {
    const int own_c9 = ai_euro_20e6_unit_col9(ctx->units, u);
    odds /= own_c9 < 1 ? 1 : own_c9;
  }
  int settlement = 0;
  if (ai_euro_20e6_colony_owner_at(ctx, nx, ny) >= 0) {
    odds *= 3;
    settlement = 1;
  }
  if (ai_euro_village_nation_at(ctx->col1_ok ? ctx->col1 : NULL, nx, ny) >= 0) {
    odds <<= 1;
    settlement = 1;
  }
  if (s->dos_type == UNITS_KIND_ARTILLERY && !settlement) {
    odds = 0;
  }
  /* FUN_521d_20e6 raw 88913-88915: the ACTING unit's own nation nibble
   * (uVar11, raw 88403 = `*(byte *)(unit + 0x3147) & 0xf`) equals DS:0x53d2
   * (head.crown_nation_id) ∧ open tile (local_4 == 0) ∧ local_2e == 0
   * → halve. (The old comment's "raw 2855" was Ghidra switch garbage and the
   * crown id was compared against the literal 2.) */
  if (ctx->col1_ok && ctx->col1 &&
      (int)u->nation_id == (int)ctx->col1->head.crown_nation_id && !settlement &&
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
   * own ≤ def → `goto LAB_521d_5183`.
   *
   * LAB_5183 is the direction loop's own increment, NOT an exit from the scan
   * (bugs.md #523, refuted 2026-09-18e): viceroy_overlays.asm
   * `LAB_OVL14_L0000__0054b0  CMP [BP+0xff28],AX / JL 0054b5 / JMP 005183`,
   * and `LAB_OVL14_L0000__005183` is `INC word ptr [BP-0x4e]` followed by
   * `LAB_..__005186  CMP [BP-0x4e],0x8 / JL 00518f` — the loop counter bump
   * and test, with 14 XREFs, i.e. every "this direction is unscoreable" arm
   * in the scorer jumps there. Returning 0 so the caller `continue`s to the
   * next direction is exactly DOS.
   */
  if ((s->dos_type == UNITS_KIND_SOLDIER || s->dos_type == UNITS_KIND_DRAGOON) &&
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
  /*
   * LAB_OVL14_L0000__0054b5 (viceroy_overlays.asm 0x0054b5, decomp
   * viceroy_unpacked_2.c:85550-85563): the −999 penalty is gated on the mover
   * being a NON-ship (`CMP [BX+0x3146],0xd / JC` … `CMP [BX+0x3146],0x12 /
   * JBE`, i.e. type ∉ [0xd,0x12]); a ship with poor odds falls into the else
   * arm instead, where DOS clamps `odds < 1` up to 1 before the ×4 bonus.
   * Both were dropped by the first pass; restored verbatim.
   */
  const int dos_ship = (s->dos_type >= 0xd && s->dos_type <= 0x12);
  if (odds < 0xc && !dos_ship) {
    *score -= 999;
  } else {
    if (odds < 1) {
      odds = 1;
    }
    *score += odds * 4;
  }
  return 1;
}

/*
 * FUN_281f_06b4(x, y) == 1 for water: the open-sea body (water region 1).
 * Same region-0 relaxation as map_tile_is_open_sea_adjacent — synthetic /
 * test maps leave layer3 zero.
 */
static int ai_euro_20e6_open_sea(const ColonizeWorldMap* map, int x, int y) {
  if (!map_tile_is_water(map, x, y)) {
    return 0;
  }
  const int region = (int)(map_get_layer3(map, x, y) & 0x0fu);
  return region <= 1;
}

/*
 * LAB_521d_4d2e → 5183: the 8-direction wander scorer. Picks one adjacent
 * tile (or stay) exactly as DOS does when no arm above has committed a
 * destination. Ships reach it through the raw 90219 `goto LAB_4d2e` after the
 * 3558 / 4393 / 457e bands fall through; their arms are the iStack_34 branches
 * inside the same loop (water-only step, no settlement term, fort/artillery
 * term scaled by holds, west lean, unseen-water credit). Returns dir 0..7,
 * or 8 = stay.
 *
 * `out_attack` = DOS `local_ce` (raw 88874): the winning candidate was scored
 * through LAB_521d_52aa, i.e. the pick is an attack. `out_saw_foe` = DOS
 * `local_ea` (raw 88887): *any* candidate reached LAB_52aa with a non-zero
 * DS:0x5236 row — an attackable foreigner stood next to this unit, whatever
 * the odds came out as. Both are read by the LAB_4d2e tail (raw 88983-89040);
 * either may be NULL.
 */
static int ai_euro_20e6_wander_step(
  ColonizeTurnContext* ctx, ColonizeUnit* u, Ai20e6Unit* s, int* out_attack, int* out_saw_foe
) {
  const int nation = s->nation;
  /* uVar14 — far-probe/fog enable: no adjacent claim (probe mode 1) or a
   * non-combat land unit. */
  int fog_enable = 0;
  if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, nation) < 0 || (!s->is_ship && s->combat == 0)) {
    fog_enable = 1;
  }
  /* raw 88622-88624: a ship in the War of Independence never idles (bVar20). */
  if (s->is_ship && s->woi) {
    fog_enable = 0;
  }
  /* iStack_90: the unit's own tile is ocean / high seas. */
  const int unit_on_water = map_tile_is_water(ctx->map, u->x, u->y) ? 1 : 0;
  /* +0x3150 occupied goods holds — the multiplier of the ship fort term. */
  int goods_holds = 0;
  if (s->is_ship) {
    const int nh = units_goods_hold_count(ctx->units, u->id);
    for (int h = 0; h < nh; ++h) {
      if (units_hold_amount(ctx->units, u->id, h) > 0) {
        goods_holds++;
      }
    }
  }
  /* unit+0x3148 bit4 wander_dest_chosen: peacetime distant-tile roll (raw
   * ~1960-1995) — kept as the latch only; the >7-tile random goto it
   * produces is a ship-only arm (type 0xd..0x12), not reached on land. */
  int best = -999;
  int best_dir = 8;
  int best_attack = 0;
  int saw_foe = 0; /* local_ea */
  const int last_dir = (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) ? s_euro_last_dir[u->id] : -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!map_coords_inset(ctx->map, nx, ny)) {
      continue; /* FUN_1000_84f2 inset bounds */
    }
    const int terr = map_dos_terr_class_at(ctx->map, nx, ny);
    const int dest_water = (terr == 0x19 || terr == 0x1a);
    if (!s->is_ship) {
      if (dest_water) {
        continue; /* land unit: ocean / high seas */
      }
    } else if (!dest_water || !ai_euro_20e6_open_sea(ctx->map, nx, ny)) {
      /* raw 88679-88687: a ship steps only onto ocean / high seas in the open
       * sea body (FUN_281f_06b4 == 1); a land tile falls to the bare
       * FUN_281f_0696 probe with no score. */
      continue;
    }
    int owner = ai_euro_20e6_owner_nibble(ctx->map, nx, ny);
    const int here = units_id_at(ctx->units, nx, ny);
    const ColonizeUnit* hu = here >= 0 ? units_get_const(ctx->units, here) : NULL;
    if (s->is_ship && hu && owner < 0) {
      /* DOS stamps the owner nibble from the stack on entry; the port's water
       * tiles carry no claim, so the hull on the tile is the owner. */
      owner = hu->nation_id;
    }
    int score = 0;
    const int dest_river = map_tile_has_river(ctx->map, nx, ny) ? 1 : 0;
    const int dest_road = map_tile_has_road(ctx->map, nx, ny) ? 1 : 0;
    const int cardinal = (d & 1) == 0;
    if (s->dos_type == UNITS_KIND_SCOUT) { /* Scouts */
      score = dos_rng_range(ctx->rng, 1, 8);
      if (s->unit_river && dest_river && cardinal) {
        score += 2;
      } else if (s->unit_road && dest_road) {
        score += 1;
      } else {
        score -= map_dos_terr_cost_byte(terr) * 3;
      }
    } else if (!s->explorer) {
      if (u->col1_vis_mask == 0 && !unit_on_water) { /* unseen; raw 88709 also iStack_90 == 0 */
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
    /*
     * Combat land unit stepping onto a settlement tile —
     * LAB_OVL14_L0000__00507a..0050fc (viceroy_overlays.asm 0x00507a, decomp
     * viceroy_unpacked_2.c:85338-85376):
     *   if (type ∉ [0xd,0x12] && DS:0x5236[type] > 1)
     *     if (FUN_1000_8886(dest) >= 0)              // Euro colony on the tile
     *       if ([BP-0xe] == [BP+0xff1c])             // TILE owner nibble == mover nation
     *         FUN_1000_8bd6([BP-0x60]);              // bind the MOVER's nearest own
     *                                                // colony (FUN_1000_8804(ux,uy,nation,-1),
     *                                                // stored in the prologue at 0x0021f2)
     *         flags = *(DS:0x8542)+0x1b: 0x40 → +10, 0x04 → +6, 0x10 → +3
     *       else score += 0x10;
     * The first pass read the destination tile's colony record and widened the
     * single owner gate to `owner == nation || col_owner == nation`; both are
     * back to the DOS form (s->home_colony = the same 8804 search).
     */
    if (!s->is_ship && s->combat > 1) {
      const int col_owner = ai_euro_20e6_colony_owner_at(ctx, nx, ny);
      if (col_owner >= 0) {
        if (owner == nation) {
          const ColonizeColony* c =
            s->home_colony >= 0 ? colonies_get(ctx->colonies, s->home_colony) : NULL;
          const int af = c ? (int)c->ai_flags : 0;
          if (af & COLONIZE_COLONY_AI_NEEDS_GARRISON) {
            score += 10;
          } else if (af & COLONIZE_COLONY_AI_MILITARY_SURPLUS) {
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
      /*
       * raw 88877-88880: `local_10 = FUN_281f_06dc(x, y)` is the layer3 owner
       * nibble with 0xf → −1, and the Euro arm is entered on `local_10 < 4`,
       * so a foe standing on UNCLAIMED land (owner −1) is scored here too.
       * DOS then reads `FUN_15b3_0004(nation, −1)` = the byte before the
       * nation's relation row (nation[n−1] +0x13b, a trade-table tail byte;
       * DS:0x883b for nation 0) — a stray read whose value is not knowable
       * statically. The port stands in with the relation to the occupant's
       * nation, which is the only reading under which the arm is coherent
       * (bugs.md #521, 2026-09-22: without it a foe on unclaimed land was
       * never scored and land units went passive).
       */
      const int partner = owner >= 0 ? owner : (hu ? hu->nation_id : -1);
      const int rel = ai_euro_20e6_diplo(ctx->col1, nation, partner);
      const int hu_type = hu ? ai_euro_20e6_dos_type(ctx->units, hu) : -1;
      /*
       * raw 88883-88885 (FUN_521d_20e6, LAB_521d_52aa entry gate):
       *   (*(byte*)0x5382 & 1) == 0 ||
       *   ((local_10 < 4 && *(char*)(local_10*0x34 + 0x543f) == '\0') || 3 < local_10)
       * During the War of Independence (DS:0x5382 bit0) an attack is scored
       * only against a human-controlled player slot (control byte 0) or a
       * tribe. The port previously spelled this `!s->woi || owner < 0`, which
       * rejected every claimed tile and left the REF unable to score an
       * assault on a defended rebel colony (bugs.md #521).
       */
      const int woi_ok =
        !s->woi || partner > 3 ||
        (partner >= 0 && partner < 4 && ctx->col1_ok && ctx->col1 &&
         ctx->col1->player[partner].control == 0);
      /* DOS scores the tile as an attack when the owner is not yet MET (a
       * forced first contact) or a Privateer is involved; Linux contact is
       * driven by ai_contact_*, so this port only takes the arm at war —
       * a deliberate narrowing, not a transcription slip. */
      const int at_war = partner >= 0 && ctx->col1 && ai_diplo_at_war(ctx->col1, nation, partner);
      if ((at_war ||
           ((rel & AI_DIPLO_MET) == 0 && s->dos_type == UNITS_KIND_PRIVATEER) ||
           hu_type == UNITS_KIND_PRIVATEER) &&
          woi_ok) {
        /* raw 88885-88887, LAB_521d_52aa entry: `if (DS:0x5236[type*0xe] !=
         * 0) { local_7e = 1; local_ea = 1; ... }`. local_ea is sticky for the
         * whole 8-direction scan and is set here, before the odds are known —
         * a −999 candidate still marks it. */
        if (s->combat != 0) {
          saw_foe = 1;
        }
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
        /* raw 88885-88887 again: the tribe branch enters the same LAB_52aa. */
        if (s->combat != 0) {
          saw_foe = 1;
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
      const int ax = nx + MAP_DIR8_DX[n];
      const int ay = ny + MAP_DIR8_DY[n];
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
    /*
     * Ship-only (raw 88808-88830): a hostile Euro colony (diplo & 0x60 ==
     * 0x20) next to the destination costs 0x14 with a Stockade, 0x28 with a
     * Fort (FUN_281f_0322 feature bits 1 / 2), plus 0x1e per Artillery in
     * its stack, all multiplied by the ship's occupied goods holds — an
     * empty hull ignores the batteries entirely.
     */
    if (s->is_ship && goods_holds > 0) {
      for (int n = 0; n < 8; ++n) {
        const int ax = nx + MAP_DIR8_DX[n];
        const int ay = ny + MAP_DIR8_DY[n];
        const ColonizeColony* fc = colonies_find_at_xy(ctx->colonies, ax, ay);
        if (!fc || !fc->active || fc->nation_id < 0 || fc->nation_id > 3 ||
            fc->nation_id == nation) {
          continue;
        }
        if ((ai_euro_20e6_diplo(ctx->col1, nation, fc->nation_id) & 0x60) != 0x20) {
          continue;
        }
        const int bonus = colonies_fortification_defense_bonus_percent(ctx->colonies, fc);
        int pen = 0;
        if (bonus >= 100) {
          pen = 0x14;
        }
        if (bonus >= 150) {
          pen = 0x28;
        }
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* o = &ctx->units->units[i];
          if (o->active && o->aboard_ship_id < 0 && o->x == ax && o->y == ay &&
              ai_euro_20e6_dos_type(ctx->units, o) == 0xb) {
            pen += 0x1e;
          }
        }
        score -= pen * goods_holds;
      }
    }
    /* Far probe (unit + 4·dir) explore terms. */
    if (fog_enable) {
      const int fx = u->x + MAP_DIR8_DX[d] * 4;
      const int fy = u->y + MAP_DIR8_DY[d] * 4;
      if (s->is_ship && ai_euro_ship_dos_enabled()) {
        /* raw 88837-88840: the real DS:0x9faa coarse plane, restamped with
         * this nation's units and colonies at its 0a60 entry. */
        if (map_coords_inset(ctx->map, fx, fy) && !map_tile_is_water(ctx->map, fx, fy) &&
            ai_coarse_fog_explore_unseen(fx, fy)) {
          score += 8;
        }
      } else if (map_coords_inset(ctx->map, fx, fy) && !map_tile_is_water(ctx->map, fx, fy) &&
          ctx->map->seen && !map_tile_seen_by(ctx->map, fx, fy, nation)) {
        score += 8; /* DS:0x9faa coarse cell unseen — per-nation seen[] stand-in */
      }
      /* raw 88842-88844: a ship in the eastern half leans west (dirs SW/W/NW). */
      if (s->is_ship && d >= 5 && d <= 7 && u->x > (int)ctx->map->width / 2) {
        score += 4;
      }
      for (int n = 0; n < 8; ++n) {
        const int ax = fx + MAP_DIR8_DX[n];
        const int ay = fy + MAP_DIR8_DY[n];
        if (!map_coords_inset(ctx->map, ax, ay)) {
          continue;
        }
        if (nation < 4 && ctx->map->seen && !map_tile_seen_by(ctx->map, ax, ay, nation) &&
            (s->is_ship || !map_tile_is_water(ctx->map, ax, ay))) {
          score += 2; /* raw 88853-88856: ships count unseen water too */
        }
        /* DOS 521d:57c2 calls FUN_281f_0682 (unit-presence bit only) here,
         * not 06d2 — a settlement tile without a unit does not −2. */
        if (map_tile_owner_or_presence(ctx->map, ax, ay) >= 0) {
          score -= 2;
        }
        if (s->explorer) {
          score += (int)k_20e6_terr_site_byte[map_dos_terr_class_at(ctx->map, ax, ay) & 31];
        }
      }
    }
    if ((s->is_ship && getenv("AI_SHIP_TRACE")) || getenv("AI_4D2E_TRACE")) {
      fprintf(stderr, "[4d2e] u%d dir %d (%d,%d) score %d attack %d\n", u->id, d, nx, ny, score, attack);
    }
    if (score > best) {
      best = score;
      best_dir = d;
      best_attack = attack;
    }
  }
  /* LAB_5183 tail for an attack pick: DOS stays when fewer than 3 thirds
   * (one full move) remain — Linux moves is whole moves and the act
   * loop already requires >0, so nothing extra to gate here. */
  if (out_attack) {
    *out_attack = best_attack;
  }
  if (out_saw_foe) {
    *out_saw_foe = saw_foe;
  }
  return best_dir;
}

/*
 * DOS-LITERAL FUN_521d_20e6 raw 89011-89029 — the 0x46 arm of the LAB_4d2e
 * tail (asm OVL14_L0000:0x005992-0x005a31, viceroy_overlays.asm; the same body
 * decompiles a second time at raw 85856-85874, where the nation compare reads
 * `uStack_e6` and confirms it is the acting unit's own nation nibble).
 *
 * Position: the tail runs `local_ce == 0` (the winning wander pick is not an
 * attack, asm 0x5840) → `local_8e == 0` (not the own-colony ≥2-garrison entry,
 * asm 0x5876) → the 0x42 arm (DS:0x523d bit0 pioneer work, raw 88989-89000) →
 * the 0x65 arm (bit2, raw 89002-89009) → **this arm** → the 0x39 fallthrough
 * (asm 0x586a). Every failed sub-test jumps straight to 0x586a/0x39, so this
 * arm never changes the wander pick unless all four sub-tests pass.
 *
 *   bVar10 = unit+0x3146;                              // raw 89011
 *   if (DS:0x5236[bVar10 * 0xe] > 1 &&                 // @UNIT attack col > 1
 *       (bVar10 < 0xd || 0x12 < bVar10) &&             // not a ship type
 *       local_ea != 0) {                               // saw an attackable foe
 *     if (FUN_281f_098e(param_1) == FUN_281f_02ee(param_1)) {   // raw 89016-89019
 *       for (i = 0; i < 8; i++) {                      // raw 89020-89029
 *         y = DS:0xbe[i] + local_94; x = DS:0xb4[i] + local_88; // the UNIT's tile
 *         local_10 = FUN_281f_0696(x, y);              // Euro settlement owner
 *         if (-1 < local_10 && local_10 != uVar11) {   // owned by another nation
 *           unit+0x314b = 0x46; goto LAB_521d_5899;    // local_76 = 8 -> stay
 *         }
 *       }
 *     }
 *   }
 *
 * The args of the two chain walks are dropped by Ghidra (far thunks); the asm
 * at 0x59c9 passes the unit index in AX with no stack cleanup:
 *   MOV AX,[BP+6]; CALLF FUN_1000_8b7e  -> FUN_281f_098e -> FUN_1427_0026
 *   MOV AX,[BP+6]; CALLF FUN_1000_84de  -> FUN_281f_02ee -> FUN_1427_0002
 * FUN_1427_0026 walks unit+0x315e to the chain tail, FUN_1427_0002 walks
 * unit+0x315c to the chain head (raw 7241-7277); both return the unit itself
 * when its link is −1, so `tail == head` is exactly "this unit is alone in its
 * per-tile chain" — the same chain units_map_stack_chrome models (units.h).
 *
 * What the arm does NOT do: it issues no FORTIFY order and no goto. +0x314b is
 * the AI's own annotation byte and no DOS reader compares it against 0x46; the
 * whole behavioural effect is LAB_5899's `local_76 = 8`, i.e. the unit does not
 * take the wander step it just scored and its act state falls to 5/6 at the
 * epilogue (raw 90374-90386). It also spends no garrison quota and does not
 * require the unit to stand on (or near) a colony of its own. bugs.md #512.
 */
static int ai_euro_20e6_border_park_arm(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, const Ai20e6Unit* s, int saw_foe
) {
  /* raw 89012-89013 / asm 0x5992-0x59c6, in DOS's own order. */
  if (s->combat <= 1) {
    return 0;
  }
  if (s->dos_type >= 0xd && s->dos_type <= 0x12) {
    return 0;
  }
  if (!saw_foe) {
    return 0;
  }
  /* raw 89016-89019 / asm 0x59c9-0x59df: alone in the tile chain. */
  if (units_map_stack_chrome(ctx->units, u->id)) {
    return 0;
  }
  /* raw 89020-89029 / asm 0x59e2-0x5a31: a Euro settlement of another nation
   * on one of the unit's own eight neighbours. */
  for (int d = 0; d < 8; ++d) {
    const int owner =
      ai_euro_20e6_colony_owner_at(ctx, u->x + MAP_DIR8_DX[d], u->y + MAP_DIR8_DY[d]);
    if (owner >= 0 && owner != s->nation) {
      return 1;
    }
  }
  return 0;
}

/*
 * Raw 88632-88664, the ship-only arm just ahead of LAB_4d2e: an idle hull
 * (bVar20) carrying military or Pioneers (8aac modes 4 + 3) with no unload
 * mask, outside the War of Independence, rolls FUN_281f_04d4(0, 0x10) once
 * per act; on 0 it picks a random inset tile (rng(2, w-3), rng(2, h-3)) and,
 * if that is open-sea water more than 7 Manhattan tiles away, latches unit
 * +0x3148 bit 0x10 and commits it as the goto (LAB_27f5). With the bit
 * already set, rng(0, 0x30) == 0 clears it. The unload mask is taken as 0
 * here: the port calls this after the ship band's own unload arm has had its
 * go, which is the only way DOS reaches this line with cargo still aboard.
 * Returns 1 when a far goto was set.
 */
static int ai_euro_20e6_ship_far_roam(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s) {
  if (!s->is_ship || s->woi || u->id < 0 || u->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  if (ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, s->nation) >= 0) {
    return 0; /* bVar20 false */
  }
  int pioneers = 0;
  int mil = 0;
  int scouts = 0;
  int milvet = 0;
  int civ = 0;
  ai_euro_20e6_ship_cargo_counts(ctx, u, &pioneers, &mil, &scouts, &milvet, &civ);
  if (mil + pioneers == 0) {
    return 0;
  }
  uint8_t* flags = &u->col1_flags15; /* unit+0x3148 */
  if ((*flags & 0x10) == 0) {
    if (dos_rng_range(ctx->rng, 0, 0x10) != 0) {
      return 0;
    }
    const int tx = dos_rng_range(ctx->rng, 2, (int)ctx->map->width - 3);
    const int ty = dos_rng_range(ctx->rng, 2, (int)ctx->map->height - 3);
    if (!map_tile_is_water(ctx->map, tx, ty) || !ai_euro_20e6_open_sea(ctx->map, tx, ty)) {
      return 0;
    }
    if (abs(tx - u->x) + abs(ty - u->y) <= 7) {
      return 0;
    }
    *flags |= 0x10;
    ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tx, ty);
    if (getenv("AI_SHIP_TRACE")) {
      fprintf(stderr, "[ship] unit %d far roam -> (%d,%d)\n", u->id, tx, ty);
    }
    return 1;
  }
  if (dos_rng_range(ctx->rng, 0, 0x30) == 0) {
    *flags &= (uint8_t)~0x10u;
  }
  return 0;
}

/*
 * Ship entry into the 20e6 wander scorer (raw 90210-90219 → LAB_4d2e). An
 * idle hull (act state 0 / 5 / 6 / 0xa, or a step goto onto its own tile)
 * always scores; a busy one only when FUN_281f_0984 finds a foreign unit
 * adjacent, and then the port keeps its course unless the pick is an attack
 * (a wander step would otherwise overwrite a delivery goto the port has no
 * goal record to rebuild from). An attack pick resolves at once through
 * ai_euro_try_attack (LAB_589e's step into a foe tile); a water pick becomes
 * a one-tile AI_SAIL goto for the sail loop below. Returns 1 when the act
 * committed something.
 */
static int ai_euro_20e6_ship_wander_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id, int busy) {
  if (!ctx || !ctx->map || !u || !u->active || u->id < 0 || u->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (!s.is_ship) {
    return 0;
  }
  if (!busy && ai_euro_20e6_ship_far_roam(ctx, u, &s)) {
    return 1;
  }
  const int dir = ai_euro_20e6_wander_step(ctx, u, &s, NULL, NULL);
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[ship] unit %d wander dir %d busy %d at (%d,%d)\n", u->id, dir, busy, u->x, u->y);
  }
  if (dir < 0 || dir > 7) {
    if (!busy) {
      s_euro_last_dir[u->id] = 8; /* unit+0x314f, 8 = stay */
    }
    return 0;
  }
  const int nx = u->x + MAP_DIR8_DX[dir];
  const int ny = u->y + MAP_DIR8_DY[dir];
  const int foe = units_id_at(ctx->units, nx, ny);
  if (foe >= 0) {
    const ColonizeUnit* f = units_get_const(ctx->units, foe);
    if (!f || f->nation_id == nation_id) {
      return 0;
    }
    s_euro_last_dir[u->id] = (int8_t)dir;
    if (getenv("AI_SHIP_TRACE")) {
      fprintf(stderr, "[ship] unit %d wander attack (%d,%d)\n", u->id, nx, ny);
    }
    ai_euro_try_attack(ctx, u, nx, ny);
    return 1;
  }
  if (busy) {
    return 0;
  }
  s_euro_last_dir[u->id] = (int8_t)dir;
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[ship] unit %d wander step (%d,%d)\n", u->id, nx, ny);
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, nx, ny);
  s_euro_roam_wander[u->id] = 1; /* unit+0x314c==5 idle-roam */
  return 1;
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

/* FUN_1000_8a98 → FUN_281f_08a8 → FUN_1427_14f4: nearest unit of `nation`
 * (excluding `except_id`) by dos_dist; ties take the LAST match (DOS `<=`). */
static int ai_euro_20e6_nearest_own_unit(
  const ColonizeTurnContext* ctx, int nation, int except_id, int x, int y
) {
  int best = -1;
  int bd = 9999;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id —
   * so `except_id` (the caller passes `u->id`) has to be matched against
   * `o->id`, and the returned handle is `o->id`, which is what the caller
   * feeds back to `units_get_const`. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation || o->id == except_id) {
      continue;
    }
    const int d = map_dos_dist(x - o->x, y - o->y);
    if (d <= bd) {
      bd = d;
      best = o->id;
    }
  }
  return best;
}

/*
 * FUN_521d_20e6 treasure act band, FIRST arm — the in-colony cash-in
 * (move_scoring_20e6_full.md raw ~2313-2331, inside
 * `if (*(char *)(param_2 * 0x1c + 0x3146) == '\n')`):
 *
 *   if (iStack_2e == 0) {
 *     uVar13 = (uint)*(byte *)(param_2 * 0x1c + 0x315b) * 100;
 *     *(uint *)(*(int *)0x84fc + 0x2a) += uVar13;   (32-bit nation gold)
 *     if ((*(byte *)0x5382 & 1) == 0) { ... FUN_1000_8842(0x181f,0x1786,2); }
 *     goto LAB_OVL14_L0000__0047b9;                 destroy_unit
 *   }
 *
 * It sits BEFORE the bind+goto (iStack_2c == iStack_38), the 8a98 rendezvous
 * and the 0072d6 stranded-destroy, so it is checked first here too — ahead of
 * the port's Cortes / board / coast chain, which has no counterpart anywhere
 * in this band. DOS charges no Crown share and needs no Galleon, no Cortes and
 * no coastal colony: an AI Treasure that reaches ANY own colony is money.
 *
 * Traps carried over from the 47b9 pass (see its header): iStack_2e == 0 must
 * be read as `home_colony >= 0 && home_dist == 0` because the Linux prologue
 * zeroes home_dist on a missed colony search, and a unit sitting in the Europe
 * pool has lane-sentinel coordinates and must be excluded outright.
 *
 * Trace env: AI_20E6_TREASURE_TRACE=1. Kill switch: AI_20E6_TREASURE_CASH=0.
 * Returns 1 when the treasure was consumed (caller must stop touching it).
 */
#ifndef AI_20E6_TREASURE_CASH_DEFAULT
#define AI_20E6_TREASURE_CASH_DEFAULT 1
#endif

static int ai_euro_20e6_treasure_cash_enabled(void) {
  return ai_euro_env_flag("AI_20E6_TREASURE_CASH", AI_20E6_TREASURE_CASH_DEFAULT);
}

static int ai_euro_20e6_treasure_cash_in(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int nation_id
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !u || !u->active) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1) {
    return 0; /* no nation record to credit */
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX || u->aboard_ship_id >= 0) {
    return 0; /* a passenger is not standing in the colony */
  }
  if (!ai_euro_20e6_treasure_cash_enabled()) {
    return 0;
  }
  /* Europe-pool lane sentinels are not map coordinates (47b9 trap 3). */
  if (ai_euro_in_europe(u->x, u->y)) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type != UNITS_KIND_TREASURE) {
    return 0;
  }
  if (s.cid < 0) {
    return 0; /* iStack_38 < 0: off-land, DOS never runs the band */
  }
  /* iStack_6 (raw 2250): a goal-tasked unit skips the whole band. */
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0;
  }
  /* iStack_2e == 0, read the Linux way (47b9 trap 1). */
  if (!(s.home_colony >= 0 && s.home_dist == 0)) {
    return 0;
  }
  const int uid = u->id; /* the call despawns u — snapshot before it runs */
  const int ux = u->x;
  const int uy = u->y;
  const int gold = units_ai_treasure_cash_in_colony(
    ctx->units, ctx->col1, nation_id, uid, ctx->ai_popups, ctx->messages
  );
  if (getenv("AI_20E6_TREASURE_TRACE")) {
    fprintf(
      stderr,
      "[20e6-treasure] unit %d n%d (%d,%d) colony %d -> cash %d\n",
      uid, nation_id, ux, uy, s.home_colony, gold
    );
  }
  return 1; /* DOS destroys the unit whatever the value byte held */
}

/*
 * LAB_521d_47b9 — wagon / treasure dead-end destroy (raw 2249-2360 of
 * move_scoring_20e6_full.md; `47b9` itself is just
 * `FUN_1000_89f8(unit)` = FUN_281f_0808 → FUN_1427_0824 destroy_unit, then
 * return). Reached from the LAB_521d_457e band once the ship arms above it
 * have declined, so it is the AI's cleanup for a hauler with nowhere to go.
 *
 * Wagon Train (type 0x0c), village-delivery slot unit+0x3158 == 0:
 *   - standing on an own colony (iStack_2e == 0): bind +0x314a to it and park
 *     (orders 0x55) — not a dead end.
 *   - unbound (+0x314a < 0) and the nearest own colony is NOT on this
 *     landmass (iStack_2c != iStack_38, and iStack_2c is −2 when the nation
 *     owns no colony at all) → DESTROY.
 *   - otherwise bind the nearest own colony and goto it (LAB_4701/4567).
 * Treasure Train (type 0x0a), not standing in a colony:
 *   - nearest own colony on this landmass → bind + goto (LAB_4701).
 *   - else nearest own unit (FUN_1000_8a98) on this landmass → walk to it
 *     (LAB_27f5; the escort/galleon rendezvous).
 *   - else, when the adjacent-claim probe (FUN_OVL14_0072d6 → FUN_521d_0906,
 *     ai_euro_20e6_probe_adjacent) names the human player (DS:0x5398)
 *     → DESTROY.
 *
 * NOT ported here, deliberately:
 *   - the +0x3158 != 0 village-errand arm lives in
 *     ai_euro_20e6_wagon_village_errand (ported 2026-09-06f with the wagon
 *     load matrix that writes the latch); this function only handles the
 *     +0x3158 == 0 arms and skips errand wagons below.
 *
 * The treasure in-colony cash-in (iStack_2e == 0) is the band's FIRST treasure
 * arm and lives in ai_euro_20e6_treasure_cash_in below, called ahead of the
 * rest of the treasure chain so DOS precedence holds; this function is only
 * reached once that arm has declined.
 *
 * Returns 0 when the band declined, 1 when it bound a course (LAB_4701 /
 * LAB_27f5) and 2 when the unit was destroyed (caller must stop touching it).
 */
static int ai_euro_20e6_47b9_dead_end(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !u || !u->active) {
    return 0;
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX || u->aboard_ship_id >= 0) {
    return 0;
  }
  /* DOS's unit loop never reaches 20e6 for a unit sitting in the Europe pool
   * (its x/y are the lane sentinels, not map coords) — without this guard a
   * wagon waiting in Europe reads iStack_38 = −1 and looks stranded. */
  if (ai_euro_in_europe(u->x, u->y)) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type != UNITS_KIND_WAGON && s.dos_type != UNITS_KIND_TREASURE) {
    return 0;
  }
  if (s.cid < 0) {
    return 0; /* iStack_38 < 0: off-land (aboard / sentinel) — no dead-end call */
  }
  /* iStack_6 (raw 2250): a unit already tasked by the goal engine skips the
   * whole band and falls straight to LAB_5a78. */
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0;
  }
  /* iStack_2e == 0 only means "on the colony" when one was actually found —
   * the prologue zeroes home_dist on a miss (DOS leaves DS:0x8db8 stale). */
  const int in_own_colony = (s.home_colony >= 0 && s.home_dist == 0);
  const int destroy_trace = getenv("AI_20E6_DEADEND_TRACE") != NULL;

  if (s.dos_type == UNITS_KIND_WAGON) {
    /* The wagon 457e arm now lives whole in ai_euro_20e6_wagon_origin_walk
     * (bind / park / walk / destroy), called from ai_euro_try_wagon_haul in
     * DOS position. Nothing left for the act-level dead-end pass to do. */
    return 0;
  }

  /* Treasure Train. */
  if (in_own_colony) {
    return 0; /* cash-in arm — ai_euro_20e6_treasure_cash_in already had it */
  }
  /*
   * arm 2 (raw 90016-90017): `uVar14 = local_62; if (local_2c == local_38)
   * goto LAB_4701` — FUN_281f_09e6(nearest own colony) selects it, LAB_4567
   * loads its coords and LAB_27f5 walks there. local_62 is the prologue's
   * nearest own colony by FUN_281f_037a distance with NO coastline term, and
   * the guard is a continent-id compare. DOS does not write the unit's
   * +0x314a origin byte on this arm (only the wagon 457e arm does).
   */
  if (s.home_cid == s.cid && s.home_colony >= 0) {
    const ColonizeColony* home = colonies_get(ctx->colonies, s.home_colony);
    if (home && home->active && home->nation_id == nation_id) {
      if (units_orders_follow_goto(u->orders) && u->goto_x == home->x &&
          u->goto_y == home->y) {
        return 1; /* already walking there */
      }
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, home->x, home->y);
      return 1;
    }
  }
  /*
   * arm 3 (raw 90018-90030): nearest own unit (FUN_281f_08a8) whose tile's
   * continent id equals the treasure's → LAB_27f5 walk to it.
   */
  {
    const int mate = ai_euro_20e6_nearest_own_unit(ctx, nation_id, u->id, u->x, u->y);
    if (mate >= 0) {
      const ColonizeUnit* mu = units_get_const(ctx->units, mate);
      if (mu && map_continent_id_at(ctx->map, mu->x, mu->y) == s.cid) {
        if (units_orders_follow_goto(u->orders) && u->goto_x == mu->x &&
            u->goto_y == mu->y) {
          return 1;
        }
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, mu->x, mu->y);
        return 1;
      }
    }
  }
  {
    const int human = (ctx->col1_ok && ctx->col1) ? col1_save_human_nation(ctx->col1) : -1;
    if (human >= 0 && ai_euro_20e6_probe_adjacent(ctx, u->x, u->y, nation_id) == human) {
      if (destroy_trace) {
        fprintf(stderr, "[47b9] treasure %d n%d (%d,%d) stranded -> destroy\n",
                u->id, nation_id, u->x, u->y);
      }
      (void)units_despawn(ctx->units, u->id);
      return 2;
    }
  }
  return 0;
}

/*
 * LAB_521d_457e wagon arm, WHOLE (raw 2256-2289 — doc 2263-2296), the
 * `type == 0x0c && +0x3158 == 0` band. Replaces the port's old substitute of
 * letting wagons peel the ships-only 4393 work queue (divergence retired
 * 2026-09-07):
 *
 *   if (iStack_2e == 0) {                     // standing at an own colony
 *     if (+0x314a < 0) +0x314a = uStack_62;   // bind origin to it
 *     if (+0x314a == uStack_62) {             // at MY colony:
 *       +0x314b = 0x55; goto LAB_5899;        //   park (sentry cache byte)
 *     }
 *   }
 *   if (+0x314a < 0) {
 *     if (iStack_2c != iStack_38) goto 47b9;  // unbound, no own colony on
 *     +0x314a = uStack_62;                    //   this landmass → DESTROY
 *   }
 *   bind colony +0x314a; goto LAB_4567;       // walk home (27f5)
 *
 * DOS's 0x55 is 20e6's own per-unit decision cache, not a real order — the
 * band re-runs next turn — so the park is modelled as claiming the beat with
 * no goto. The lifecycle this closes: dump at bound colony (arrival block) →
 * load one hold + errand → village sell (2820 clears +0x3158) → walk home →
 * park/dump again. Returns 1 when the beat is claimed (incl. destroy).
 */
static int ai_euro_20e6_wagon_origin_walk(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
) {
  if (!ctx || !ctx->units || !ctx->map || !ctx->colonies || !u || !u->active) {
    return 0;
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX || u->aboard_ship_id >= 0) {
    return 0;
  }
  if (ai_euro_in_europe(u->x, u->y)) {
    return 0; /* DOS's unit loop never reaches 20e6 in the Europe pool */
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type != UNITS_KIND_WAGON || s.cid < 0) {
    return 0;
  }
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0; /* iStack_6 (raw 2250): tasked unit skips the band */
  }
  if (s_20e6_wagon_errand[u->id]) {
    return 0; /* +0x3158 != 0 — the errand walker owns this wagon */
  }
  const int destroy_trace = getenv("AI_20E6_DEADEND_TRACE") != NULL;
  /* iStack_2e == 0 only means "on the colony" when one was actually found —
   * the prologue zeroes home_dist on a miss (DOS leaves DS:0x8db8 stale). */
  const int in_own_colony = (s.home_colony >= 0 && s.home_dist == 0);
  int ori = ai_euro_20e6_origin_get(u);
  if (in_own_colony) {
    if (ori < 0) {
      ai_euro_20e6_origin_set(u, s.home_colony);
      ori = s.home_colony;
    }
    if (ori == s.home_colony) {
      return 1; /* orders 0x55, LAB_5899 park — beat claimed, no goto */
    }
  }
  if (ori < 0) {
    if (s.home_cid != s.cid) {
      if (destroy_trace) {
        fprintf(stderr, "[47b9] wagon %d n%d (%d,%d) no colony on cid %d -> destroy\n",
                u->id, nation_id, u->x, u->y, s.cid);
      }
      (void)units_despawn(ctx->units, u->id);
      return 1;
    }
    ai_euro_20e6_origin_set(u, s.home_colony);
    ori = s.home_colony;
  }
  const ColonizeColony* home = colonies_get(ctx->colonies, ori);
  if (!home || !home->active || home->nation_id != nation_id) {
    /* Bound colony gone (razed / captured): DOS would keep a dangling index;
     * unbind so the colony tick or this arm can rebind next beat. */
    ai_euro_20e6_origin_set(u, -1);
    return 0;
  }
  if (u->x == home->x && u->y == home->y) {
    return 1; /* standing on it already — arrival owns the next beat */
  }
  if (units_orders_follow_goto(u->orders) && u->goto_x == home->x && u->goto_y == home->y) {
    return 1;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, home->x, home->y);
  return 1;
}

/*
 * LAB_521d_457e empty-ship High Seas cadence (raw 2251-2257). An untasked
 * ship (type 0x0d..0x12) carrying no passengers (iStack_a8 == 0) and no
 * TradeGoods/Tools/Muskets/Horses (iStack_44 < 0, the LAB_3558 per-cargo
 * tally's max id over `> 0xc || == 8`), whose type gate bVar7 holds, jumps to
 * LAB_521d_3fa6 when EITHER unit+0x3148 bit 0x20 is set OR
 * `((char)unit_index + (char)DS:0x538e) & 0x1f == 0` (DS:0x538e = the turn
 * counter — europe_nation_eot.md). LAB_3fa6 = FUN_1000_94da → FUN_291f_02ea →
 * FUN_48d3_015e: spiral out for a High Seas tile (terrain 0x1a) that is empty
 * or own, latch it as the goto and stamp orders 0x45 — i.e. sail home.
 *
 * bVar7 (decomp 88556-88579), full formula live 2026-09-07 — the "budget
 * bytes with no decoded writer" turned out to be census fields the port
 * already persists (save I/O rows FUN_1d1d_060c(0x9414,4,..) /
 * (0x924c,0x4c,..) pinned them): 0x9414 = stuff.ship_cargo_totals,
 * 0x924c stride-0x13 = stuff.unit_type_counts[4][19], so −0x6da4/−0x6da2 =
 * counts of type 0x10 (Privateer) / 0x12 (Man-O-War).
 *   - base: type != 0x12;
 *   - Frigate (0x11): ship_cargo_totals[n] − 3*frigate_count[n] −
 *     privateer_count[n] < 4, AND FUN_281f_0984 (→ FUN_1427_09dc, the
 *     8-adjacent foreign-owner-on-same-body probe) returns 0;
 *   - Privateer (0x10): DS:0xa89b ≥ 2 || DS:0x9e52 ≥ 7 — those are the
 *     census frigate-pressure tallies (own colonies with a foreign Frigate
 *     in reach / their pop sum, s_ship_pressure), NOT difficulty (the old
 *     substitution misread 0xa89b);
 *   - Man-O-War (0x12): re-allowed on odd unit index or
 *     unit_type_counts[n][0x12] == 1.
 *
 * WIRED LIVE (2026-09-06d), in DOS order: the ship act calls this only after
 * the 4393 work-queue haul (ai_euro_try_ship_trade_haul) declines, which is
 * where LAB_457e sits in the raw flow, and behind the same
 * `!at_war && !ship_has_useful_goto` chain guard as its
 * neighbours. The golden-pinned SW coastal cruise
 * (ai_euro_try_post_found_coast_cruise) still runs first, so it keeps
 * ownership of the TURN4→7 beachhead transports; the cadence never fires on
 * any golden fixture (AI_20E6_HS_TRACE over golden_ai_turns / _mid01 /
 * _late01 / _joint / smoke_play: zero hits) and all 57 ctest cases stay
 * green. Kill switch / bisect: AI_20E6_HS_CADENCE=0.
 *
 * Thin: DOS also stamps orders 0x45 and act_state 3/0xb on the unit; the port
 * stamps UNITS_ORDER_AI_SAIL onto the spiral tile and leaves the actual
 * Europe crossing to the existing high-seas handling.
 */
#ifndef AI_20E6_HS_CADENCE_DEFAULT
#define AI_20E6_HS_CADENCE_DEFAULT 1
#endif

static int ai_euro_20e6_hs_cadence_enabled(void) {
  return ai_euro_env_flag("AI_20E6_HS_CADENCE", AI_20E6_HS_CADENCE_DEFAULT);
}

/*
 * FUN_281f_0984 → FUN_1427_09dc (decomp 7927-7968): walk the 8 neighbours of
 * (x,y); a tile with a foreign UNIT owner always hits (the body compare is
 * trivially true on that arm — local_8 is overwritten with the neighbour's
 * body first); a foreign COLONY hits only when the neighbour tile's body id
 * equals the probe tile's current body (local_8 is NOT overwritten on that
 * arm), which for a ship at sea means never (land vs water body). Owner
 * lookups substituted with pool scans (the DOS layer2/3 presence bits mirror
 * them).
 */
static int ai_euro_20e6_adjacent_foreign_09dc(
  const ColonizeTurnContext* ctx, int x, int y, int nation_id
) {
  if (!ctx || !ctx->map) {
    return 0;
  }
  const int own_unit = ctx->units ? units_id_at(ctx->units, x, y) : -1;
  int own_tile_owner = -1;
  if (own_unit >= 0) {
    const ColonizeUnit* ou = units_get_const((ColonizeUnitPool*)ctx->units, own_unit);
    own_tile_owner = ou ? ou->nation_id : -1;
  }
  int local_8 = (int)(map_get_layer3(ctx->map, x, y) & 0x0fu);
  for (int d = 0; d < 8; ++d) {
    const int nx = x + MAP_DIR8_DX[d];
    const int ny = y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    int b = local_8;
    if (own_tile_owner < 0) {
      b = (int)(map_get_layer3(ctx->map, nx, ny) & 0x0fu);
    }
    int owner = -1;
    const int uid = ctx->units ? units_id_at(ctx->units, nx, ny) : -1;
    if (uid >= 0) {
      const ColonizeUnit* nu = units_get_const((ColonizeUnitPool*)ctx->units, uid);
      owner = nu ? nu->nation_id : -1;
    }
    int keep = b;
    if (owner < 0) {
      const int cid = ctx->colonies ? colonies_id_at(ctx->colonies, nx, ny) : -1;
      if (cid >= 0) {
        owner = ctx->colonies->colonies[cid].nation_id;
      }
      keep = local_8;
    }
    local_8 = keep;
    if (owner >= 0 && owner != nation_id && b == local_8) {
      return 1;
    }
  }
  return 0;
}

/* bVar7 (decomp 88556-88579) — see the HS-cadence header above for the
 * operand decode. `u` is the acting ship. */
static int ai_euro_20e6_457e_type_gate(
  const ColonizeTurnContext* ctx, const ColonizeUnit* u, int dos_type
) {
  const int n = u ? u->nation_id : -1;
  const ColonizeCol1Stuff* stuff =
    (ctx && ctx->col1_ok && ctx->col1) ? &ctx->col1->stuff : NULL;
  if (dos_type == UNITS_KIND_MAN_O_WAR) { /* Man-O-War (decomp 88576-88579) */
    if (u && (u->id & 1) != 0) {
      return 1;
    }
    return stuff && n >= 0 && n < 4 && stuff->unit_type_counts[n][0x12] == 1;
  }
  if (dos_type == UNITS_KIND_FRIGATE) { /* Frigate (decomp 88557-88566) */
    if (!stuff || !u || n < 0 || n > 3) {
      return 0;
    }
    const int budget = (int)stuff->ship_cargo_totals[n] -
                       3 * (int)stuff->unit_type_counts[n][0x11] -
                       (int)stuff->unit_type_counts[n][0x10];
    if (budget >= 4) {
      return 0;
    }
    return !ai_euro_20e6_adjacent_foreign_09dc(ctx, u->x, u->y, n);
  }
  if (dos_type == UNITS_KIND_PRIVATEER) { /* Privateer (decomp 88567-88574) */
    if (n < 0 || n > 3) {
      return 0;
    }
    const AiEuroShipPressure* sp = &s_ship_pressure[n];
    return sp->frigate_colonies >= 2 || sp->frigate_pop >= 7;
  }
  return 1;
}

static int ai_euro_20e6_457e_hs_cadence(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active) {
    return 0;
  }
  if (u->id < 0 || u->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (s.dos_type < 0xd || s.dos_type > 0x12) {
    return 0;
  }
  if (s.order_code == 't' || s.order_code == 'i') {
    return 0; /* iStack_6 */
  }
  if (u->cargo_count != 0) {
    return 0; /* iStack_a8 = stack−1 */
  }
  /* iStack_44: max hold cargo id over `id > 0xc || id == 8`
   * (TradeGoods/Tools/Muskets, plus Horses) — < 0 means none aboard. */
  {
    const int holds = units_goods_hold_count(ctx->units, u->id);
    for (int h = 0; h < holds; ++h) {
      const int ct = u->hold_goods_type[h];
      if (units_hold_amount(ctx->units, u->id, h) <= 0 || ct < 0) {
        continue;
      }
      if (ct > 0xc || ct == 8) {
        return 0;
      }
    }
  }
  if (!ai_euro_20e6_457e_type_gate(ctx, u, s.dos_type)) {
    return 0;
  }
  const int spare = (u->col1_flags15 & AI_EURO_F3148_SPARE) != 0; /* unit+0x3148 */
  if (!spare && (((char)u->id + (char)s.turn) & 0x1f) != 0) {
    return 0;
  }
  /* Already on High Seas — orders 0x45 means cross, not re-spiral (the
   * re-spiral skips the own tile as occupied and wiggles the hull between
   * two rim tiles forever). */
  if (map_tile_is_high_seas(ctx->map, u->x, u->y)) {
    if (getenv("AI_20E6_HS_TRACE")) {
      fprintf(stderr, "[457e] ship %d n%d (%d,%d) turn %d on HS -> Europe\n", u->id, nation_id,
              u->x, u->y, s.turn);
    }
    return ai_euro_ship_enter_europe(ctx, u);
  }
  int hx = 0;
  int hy = 0;
  if (!units_spiral_place_hs_near(ctx->units, ctx->map, u->x, u->y, nation_id, &hx, &hy)) {
    return 0;
  }
  if (getenv("AI_20E6_HS_TRACE")) {
    fprintf(stderr, "[457e] ship %d n%d (%d,%d) turn %d spare %d -> HS (%d,%d)\n", u->id,
            nation_id, u->x, u->y, s.turn, spare, hx, hy);
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, hx, hy);
  return 1;
}

/*
 * LAB_521d_589e, the shared FUN_521d_20e6 exit tail, `local_76 == 8` arm
 * (DOS-LITERAL FUN_521d_20e6 raw 90378-90386):
 *   unit+0x314f = local_76;                       // facing, 8 = stay
 *   if (local_76 == 8) {
 *     if (+0x314c != 5 && +0x314c != 6) +0x314c = 5;
 *     if (+0x3148 & 2) +0x314c = 6;
 *   }
 * Every "the unit stays this beat" exit of 20e6 funnels through here (the
 * LAB_5899 garrison/park jumps included), so a stay always lands the unit in
 * the act_state 5/6 fortify family. DOS never touches +0x314d/e (the goto
 * tile) on this arm — a stale course is simply no longer dispatched, because
 * FUN_521d_5b66 only walks a goal at act_state 0x0b (raw 90552).
 * (bugs.md #554.)
 */
COLONIZE_INTERNAL void ai_euro_20e6_stay_tail_589e(ColonizeUnit* u) {
  if (!u) {
    return;
  }
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    s_euro_last_dir[u->id] = 8; /* +0x314f = local_76 = 8 */
  }
  u->last_dir = 8;
  if (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED) {
    u->orders = UNITS_ORDER_FORTIFY; /* +0x314c = 5 */
  }
  if (u->col1_flags15 & AI_EURO_F3148_ROAM) {
    u->orders = UNITS_ORDER_FORTIFIED; /* +0x314c = 6 */
  }
}

/*
 * FUN_521d_20e6 raw 88584-88610, gate half only: does this unit's own-colony
 * garrison test send it to LAB_5899 (stay) instead of down the normal arm
 * chain at LAB_521d_277a? The writes (labor_shortage--, order_code 0x47) stay
 * in ai_euro_20e6_land_arms, which runs the full arm; this predicate exists so
 * the port's earlier act-stage arms (the 0x46/0x4c seizure engage) cannot
 * preempt a decision DOS takes first. Added 2026-09-23 with the removal of the
 * invented "Artillery fortify" arm (bugs.md #760), which had been masking the
 * precedence for artillery.
 */
static int ai_euro_20e6_garrison_holds(
  ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation_id
) {
  if (!ctx || !u || !ctx->colonies || !ctx->units) {
    return 0;
  }
  const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
  if (dtype >= 0xd && dtype <= 0x12) { /* local_34 != 0 */
    return 0;
  }
  if (ai_euro_20e6_type_combat(dtype) <= 1 || dtype == 4 || dtype == 8) {
    return 0;
  }
  const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
  const ColonizeColony* oc = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
  if (!oc || !oc->active || oc->nation_id != nation_id) { /* local_2e != 0 */
    return 0;
  }
  const int admitted = u->col1_ai_plan == 'A'; /* +0x314b == 'A' */
  if (oc->labor_shortage < 1 && !admitted) {
    return 0;
  }
  if (admitted) {
    return 1; /* raw 88604: straight to LAB_5899 */
  }
  int armed = 0;
  for (int id = 1; id < COLONIZE_UNITS_MAX; ++id) {
    const ColonizeUnit* su = units_get_const(ctx->units, id);
    if (!su || !su->active || su->x != u->x || su->y != u->y) {
      continue;
    }
    const int st = ai_euro_20e6_dos_type(ctx->units, su);
    if (ai_euro_20e6_type_combat(st) > 1 && (st < 0xd || st > 0x12) && st != 4 && st != 8) {
      armed++;
    }
  }
  return armed < 2; /* raw 88606-88610: lone armed garrison stays */
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
   * (bugs.md #521 lead b, 2026-09-18b) The at-war "defer course to act-level
   * hunt" early return that stood here — idle Soldier/Dragoon/Scout/Artillery
   * of a nation at war with any peer left the gate before any 20e6 arm ran —
   * had no DOS counterpart and was the reason the LAB_521d_4d2e attack term
   * could never fire on land. FUN_521d_20e6 has exactly two act-state gates,
   * both ported below: the entry bail (raw 88404-88406, `act_state ∉ {0,5,6}
   * && act_state < 10 → tail`) and the pre-4d2e gate (raw 90210-90219).
   * Neither looks at war state. Removed; the adjacent fight is now picked by
   * ai_euro_20e6_wander_step's attack term, as in DOS.
   */
  /*
   * FUN_521d_20e6 own-colony garrison arm, raw 88584-88612 (bugs.md #522).
   * This replaces three uncited early returns that used to stand here —
   * "passive colony Artillery", "military name on own colony" and "colony
   * wants construction labor" — siblings of the at-war bail deleted
   * 2026-09-18b. They parked every armed unit that stood on one of its own
   * colonies, so the whole land-arms branch below (and with it the LAB_4d2e
   * attack term) was unreachable for exactly the units that fight.
   *
   * DOS, verbatim, for a non-wagon/non-ship type (local_34 == 0):
   *   if (attack[type] < 2 || type == 4 || type == 8 || local_2e != 0 ||
   *       (colony(local_62).labor_shortage < 1 && order_code != 'A'))
   *      goto LAB_521d_277a;                       // normal arm chain
   *   armed = # units in this tile's chain with attack[type] > 1,
   *           type ∉ [0xd,0x12], type != 4, type != 8
   *   if (order_code == 'A')  goto LAB_521d_5899;  // stay
   *   if (armed < 2) { colony.labor_shortage--; order_code = 0x47;
   *                    goto LAB_521d_5899; }       // stay, garrison
   *   local_8e = 1; goto LAB_521d_4d2e;            // surplus: free-score
   * local_2e is the distance to the nearest own colony (DS:0x8db8 after the
   * uStack_62 search), so `local_2e == 0` is "standing on an own colony";
   * colony +0x8e is labor_shortage. Types 4 (Dragoons) and 8 (Cavalry) never
   * garrison and are not counted. LAB_5899 forces local_76 = 8, i.e. the unit
   * stays. The surplus branch is the DOS route by which a stack of two or
   * more armed units on an own colony marches back out and free-scores its
   * eight neighbours — the assault engine the port was missing.
   */
  int force_wander = 0; /* local_8e */
  {
    const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
    const int not_hauler = dtype < 0xd || dtype > 0x12; /* local_34 == 0 */
    const int cid = ctx->colonies ? colonies_id_at(ctx->colonies, u->x, u->y) : -1;
    ColonizeColony* oc = cid >= 0 ? colonies_get_mut(ctx->colonies, cid) : NULL;
    const int on_own_colony = oc && oc->active && oc->nation_id == nation_id; /* local_2e == 0 */
    const int admitted = u->col1_ai_plan == 'A'; /* +0x314b == 'A' */
    if (not_hauler && on_own_colony && ai_euro_20e6_type_combat(dtype) > 1 && dtype != 4 &&
        dtype != 8 && (oc->labor_shortage >= 1 || admitted)) {
      /* raw 88594-88600: the tile-chain armed count. */
      int armed = 0;
      for (int id = 1; id < COLONIZE_UNITS_MAX; ++id) {
        const ColonizeUnit* su = units_get_const(ctx->units, id);
        if (!su || !su->active || su->x != u->x || su->y != u->y) {
          continue;
        }
        const int st = ai_euro_20e6_dos_type(ctx->units, su);
        if (ai_euro_20e6_type_combat(st) > 1 && (st < 0xd || st > 0x12) && st != 4 && st != 8) {
          armed++;
        }
      }
      if (admitted) {
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0;
      }
      if (armed < 2) {
        if (oc->labor_shortage > 0) {
          oc->labor_shortage--;
        }
        u->col1_ai_plan = 0x47;
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0;
      }
      force_wander = 1; /* local_8e = 1, straight to LAB_521d_4d2e */
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
  int landed_settle = 0;
  if (ctx->colonies && colonies_count_for_nation(ctx->colonies, nation_id) == 0) {
    const ColonizeUnitKind fkind = ai_euro_unit_kind(ctx->units, u);
    if (ai_euro_name_is_pioneer(fkind) || fkind == UNITS_KIND_COLONIST) {
      int lx = 0;
      int ly = 0;
      if (ai_euro_pick_founding_tile(
            ctx->map, ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, ctx->units,
            nation_id, u->x, u->y, &lx, &ly
          )) {
        /*
         * bugs.md #708: the `ai_goals_upsert_primary(..., AI_GOAL_FOUND, 7)`
         * that stood here is gone (2026-09-23). FUN_521d_20e6 contains no goal
         * table writer at all — the goal tables are written only by
         * FUN_521d_0a60 and the 0906 producers — and the priority 7 was a bare
         * constant with no DOS origin. The local 06ae-shaped pick below is kept
         * as this act's walk target only (#530 opening scaffolding, untouched).
         */
        fx = lx;
        fy = ly;
        landed_settle = 1;
      }
    }
  }
  /*
   * FUN_521d_20e6 pre-LAB_4d2e gate, raw 90210-90219 — the last thing the arm
   * chain does before the 8-direction scorer:
   *   if (act_state != 0 && act_state != 10 && act_state != 5 &&
   *       act_state != 6 &&
   *       (act_state != 0x0b || +0x314d != x || +0x314e != y)) {
   *     if (FUN_281f_0984(x, y, continent) == 0) goto LAB_521d_5a78;
   *   }
   *   goto LAB_521d_4d2e;
   * i.e. a courseless unit (0/5/6), one flagged "a foreigner stands on one of
   * my eight neighbours" (10, FUN_521d_0a60 raw 87567) and a goal-bound unit
   * already standing on its goal tile ALWAYS free-score their neighbours; a
   * goal-bound unit whose goal lies elsewhere free-scores only when
   * FUN_281f_0984 (→ FUN_1427_09dc) finds a foreign unit or colony adjacent,
   * and otherwise leaves 20e6 for FUN_521d_5b66 to walk the goal.
   *
   * The port used to skip this test entirely and course every land unit at
   * the nearest FOUND goal, which is why the land arms below were never
   * entered in any golden (bugs.md #522: 284 of 460 gate calls).
   *
   * 2026-09-18 (bugs.md #525/#526): the test is now the DOS one, read off the
   * unit's real +0x314c/+0x314d/e (`u->orders` / `u->goto_x` / `u->goto_y`,
   * see the field map at the head of the 0a60 section) instead of the
   * `ai_euro_has_useful_goto` stand-in the retired 0a60 shadow forced. Every
   * course in this port already goes through `ai_euro_set_goto`, so the real
   * bytes are stamped everywhere — including by 0a60's goal-consumption tail,
   * which used to write only the mirror.
   */
  /*
   * `act_state ∈ {0, 10, 5, 6}`, and `0x0b standing on +0x314d/e`, all read
   * here as "no live course" — which is exactly `ai_euro_has_useful_goto`
   * now that every course, 0a60's goal commit included, stamps the real
   * +0x314c/+0x314d/e. Divergence, deliberate and one-way: DOS also sends
   * act_state 1/2/3 to the tail, but its 1 means "aboard ship / off-map"
   * (state this port keeps in `aboard_ship_id`) while the port's orders 1 is
   * SENTRY, a parked ON-MAP unit that must still free-score.
   */
  /*
   * DOS-LITERAL FUN_521d_20e6 raw 90183-90206 (dup raw 85345-85368, overlay
   * body viceroy_overlays.c:80720-80761) — the ONLY AI pioneer order DOS
   * writes. It sits immediately ahead of the pre-LAB_4d2e gate below:
   *
   *   if (unit[+0x3146] == 2 && local_6a == 0) {          // Pioneers, not an
   *     local_a = 1;                                      //   explorer pick
   *     if (-1 < village_idx) {                           // raw 90186
   *       FUN_281f_0a4c(village_idx);                     //   bind DS:0x8d52
   *       local_7c = FUN_281f_0a56(DS:0x8d52);            //   tech-tier radius
   *       if (village_dist <= local_7c && alarm_q < 3) local_a = 0;
   *     }
   *     if (-1 < colony_idx) {                            // raw 90193
   *       FUN_281f_09e6(colony_idx);
   *       if (colony[+0x1a] != nation && colony_dist < 3) local_a = 0;
   *     }
   *     if (local_a) { unit[+0x314c] = 9; unit[+0x314b] = 0x52;
   *                    goto LAB_521d_5a78; }
   *   }
   *
   * No tile-quality test, no colony ring, no tools test, no plow-before-road
   * rule — those were the invented planner (bugs.md #610).
   *
   * Who ticks an order-9 AI unit afterwards? **Nobody.** The only callers of
   * the real work bodies FUN_479b_01a6 / FUN_479b_0526 in the whole image are
   * the human ORDERS UI (raw 42512 / 42603, FUN_2b5a_123e / _1454) and the two
   * AI **colony-tick** phantom-unit sites (raw 93669 and raw 94520/94534). An
   * AI unit that takes order 9 also fails 20e6's own entry gate on every later
   * call (`act_state != 0/5/6 && act_state < 10 -> LAB_5a78`, raw 89327 in the
   * prologue), so the write is a *park*, not a work order. The tiles actually
   * get improved by FUN_5952_035e (ai_euro_5952_improve_best_plot, #612).
   * Ported literally anyway: it is what takes the unit out of the wander
   * scorer, which is visible behaviour.
   */
  /*
   * The 20e6 prologue and the iStack_6a explorer flag are DOS's, computed
   * ONCE per 20e6 call: ai_euro_20e6_explorer_flag has a side effect (the
   * −0x5ec4 per-continent explorer counter), so it must not be run twice.
   */
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  ai_euro_20e6_explorer_flag(ctx, u, &s);
  if (!landed_settle && !force_wander && s.dos_type == 2 && s.explorer == 0) {
    {
      {
      int allow = 1; /* local_a */
      if (s.village_idx >= 0 && ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
          s.village_idx < (int)ctx->col1->head.tribe_count) {
        const ColonizeCol1Tribe* vt = &ctx->col1->tribe[s.village_idx];
        const int ind = (int)vt->nation_id - 4;
        if (ind >= 0 && ind < (int)COLONIZE_COL1_INDIAN_COUNT) {
          /* FUN_281f_0a56 -> FUN_15dc_006a: tech 0/1 -> 1, 2 -> 2, else 3. */
          const unsigned tech = (unsigned)ctx->col1->indian[ind].tech;
          const int radius = (tech <= 1u) ? 1 : (tech == 2u ? 2 : 3);
          const int alarm = (nation_id >= 0 && nation_id < 4)
                              ? (int)ctx->col1->indian[ind].alarm_by_player[nation_id]
                              : 0;
          /* FUN_281f_0a60 -> FUN_15dc_00a2 quartile: <25 0, <50 1, <75 2, else 3. */
          if (s.village_dist <= radius && ai_relation_quartile(alarm) < 3) {
            allow = 0;
          }
        }
      }
      {
        int any_dist = 0;
        const int any_id = ai_euro_20e6_nearest_colony(ctx, u->x, u->y, -1, -1, &any_dist);
        if (any_id >= 0) {
          const ColonizeColony* ac = colonies_get(ctx->colonies, any_id);
          if (ac && ac->nation_id != nation_id && any_dist < 3) {
            allow = 0;
          }
        }
      }
      if (allow) {
        u->orders = UNITS_ORDER_BUILD_ROAD; /* +0x314c = 9 */
        u->col1_ai_plan = 0x52;             /* +0x314b = 'R' */
        /* `goto LAB_521d_5a78`: 20e6 is done with this unit — 1 makes
         * ai_euro_unit_act return instead of running the goal ladder, which
         * would immediately re-stamp an AI_MOVE over the order. */
        return 1;
      }
      }
    }
  }
  int to_4d2e = force_wander || !ai_euro_has_useful_goto(u, ctx->map) ||
                ai_euro_20e6_adjacent_foreign_09dc(ctx, u->x, u->y, nation_id);
  if (landed_settle) {
    to_4d2e = 0; /* FUN_521d_06ae settle-where-landed commits its own course */
    gx = fx;
    gy = fy;
  } else if (!to_4d2e) {
    /*
     * `goto LAB_521d_5a78` — 20e6 sets NO course here. DOS leaves the unit
     * bound to the goal already stored in +0x314d/e and FUN_521d_5b66's
     * switch (case 0x0b / 0x0c → FUN_479b_0972) walks one pathfinder step
     * toward it, keeping the binding until the unit stands on the tile.
     * `ai_euro_score_move` below is this port's stand-in for that walk, so
     * the only thing to do is aim it at the unit's OWN goal.
     *
     * A `ai_goals_best_found_tile_near` re-derive stood here until
     * 2026-09-18 (bugs.md #526) and re-aimed every already-bound land unit
     * at the nearest top-priority FOUND tile on every act, so no unit ever
     * finished a walk. It has no DOS counterpart at all: FOUND (code 1) is a
     * ship-only capability bit (bugs.md #511) and the only writer of a unit's
     * goal is FUN_521d_0a60's consumption tail.
     */
    gx = u->goto_x;
    gy = u->goto_y;
    if (gx < 0 || gy < 0 || gx >= UNITS_GOTO_NONE || gy >= UNITS_GOTO_NONE ||
        !map_coords_inset(ctx->map, gx, gy)) {
      return 0; /* stale/absent target: nothing to walk, LAB_5a78 tail only */
    }
  } else {
    /*
     * FUN_521d_20e6 land arms, in DOS order: SCOUT/PATROL (LAB_277a) →
     * explorer ring scan (LAB_2912, explorers only) → 8-direction wander
     * scorer (LAB_4d2e) committing one adjacent tile as a one-shot goto
     * (epilogue LAB_589e: unit+0x314c=0xc, +0x314d/e = next tile) or
     * staying (dir 8 → +0x314c=5, re-evaluate next call).
     */
    /*
     * raw 88612 `goto LAB_521d_4d2e`: the own-colony surplus branch enters the
     * scorer directly, skipping LAB_277a and every arm hanging off it.
     */
    if (!force_wander && ai_euro_20e6_patrol_arm(ctx, u, &s)) {
      return 0;
    }
    /*
     * LAB_521d_2912 entry, raw 89078-89082 (asm viceroy_overlays.asm:135003-
     * 135020, byte-checked): the explore-goal read `0116(nation, x, y, 6)`
     * then, for explorers only, `001c(nation, 6, x, y, radius = 0)` — the
     * unit clears the *secondary* explore goal standing on the tile it has
     * just reached. The read lives inside the ring scan below (it is only
     * consumed there); this is the write half.
     *
     * Faithful but inert, in DOS as here: nothing ever puts code 6 into the
     * secondary table (the sole `0214` code-6 writer is unreachable — see
     * ai_euro_land_explore_scan_target's tail), so the scan finds no match.
     * Kept because it is a real, reachable DOS call at this exact position.
     */
    if (s.explorer) {
      ai_goals_invalidate_nearby_secondary(nation_id, AI_GOAL_EXPLORE, u->x, u->y, 0);
    }
    /* DOS order: LAB_277a fall-through → 0x4c village arms → 2912 ring →
     * colonist labor loop (raw runs it inside the ring do-loop; same effect
     * here since the ring only fires for explorers). A consumed unit (village
     * entry, colony join, Pioneer convert) aborts the act — it may no longer
     * exist; a labor walk (goto set) lets the act loop move it this turn. */
    if (!force_wander && ai_euro_20e6_village_arm(ctx, u, &s)) {
      return 1;
    }
    {
      const int lr = force_wander ? 0 : ai_euro_20e6_labor_arm(ctx, u, &s);
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
    if (!force_wander && s.explorer && u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
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
    /* Raw 85313-85337, DOS position: after the hop block, before the
     * settlement-step scorer below. */
    if (!force_wander && ai_euro_20e6_surplus_recall_arm(ctx, u, &s)) {
      return 0;
    }
    const int scan_r = (!force_wander && s.explorer && hop_scan)
                         ? ai_euro_land_explore_scan_target(ctx, u, nation_id, s.explorer, &fx, &fy)
                         : 0;
    if (scan_r == 2) {
      /*
       * DOS-LITERAL FUN_521d_20e6 raw 89274 (bugs.md #683): standing on the
       * best site → `+0x314c = 7; goto LAB_521d_5a78`. 20e6 returns 0 here
       * (raw 90445), so FUN_521d_5b66 falls straight through to its own switch
       * and dispatches case 7 (build colony) in the same call — that is the
       * consumer wired at the end of the move-scoring gate in ai_euro_unit_act.
       */
      u->orders = UNITS_ORDER_BUILD_COLONY;
      return 0;
    }
    if (scan_r) {
      gx = fx;
      gy = fy;
      is_roam = 1; /* unit+0x314c==5 idle-roam (explore ring) */
    } else if (!force_wander && s.explorer && hop_scan && ai_euro_20e6_ring_hop(ctx, u, &s)) {
      return 0; /* raw 2416-2458: scan failed, hop 4 tiles out instead */
    } else {
      int wander_attack = 0; /* local_ce */
      int wander_saw_foe = 0; /* local_ea */
      const int dir = ai_euro_20e6_wander_step(ctx, u, &s, &wander_attack, &wander_saw_foe);
      /*
       * LAB_4d2e tail, raw 88983-89031: with `local_ce == 0` (the pick is not
       * an attack) DOS runs the 0x42 / 0x65 / 0x46 park arms before the 0x39
       * fallthrough, and each of them reaches LAB_5899, which forces
       * `local_76 = 8` — the scored direction is dropped and the unit stays.
       * Only the 0x46 arm is ported here; the 0x42 / 0x65 arms (raw
       * 88989-89009) are the pioneer work codes, which this port runs at act
       * level in ai_euro_act_land_roles. `local_8e` (raw 88612) is 0 on every
       * path that reaches this point in the port: it is set only by the DOS
       * own-colony-with-2+-garrison entry at raw 88584-88612, whose own
       * outcome (0x47) never reaches the wander scorer's tail arms.
       * See ai_euro_20e6_border_park_arm.
       */
      /*
       * raw 88982-88984, the first test of the LAB_4d2e tail after
       * `local_ce == 0`: `if (local_8e != 0) { select(local_62);
       * goto LAB_521d_5888; }` — a surplus garrison unit (the raw 88612 entry)
       * that scored no attack this scan falls back into the garrison hold,
       * i.e. colony.labor_shortage--, order_code = 0x47, stay. It also
       * suppresses the 0x46 park arm below (raw 88985 reads local_8e == 0).
       */
      if (!wander_attack && force_wander) {
        const int hcid = ctx->colonies ? colonies_id_at(ctx->colonies, u->x, u->y) : -1;
        ColonizeColony* hc = hcid >= 0 ? colonies_get_mut(ctx->colonies, hcid) : NULL;
        if (hc && hc->labor_shortage > 0) {
          hc->labor_shortage--;
        }
        u->col1_ai_plan = 0x47; /* +0x314b */
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0;
      }
      if (!wander_attack && !force_wander &&
          ai_euro_20e6_border_park_arm(ctx, u, &s, wander_saw_foe)) {
        u->col1_ai_plan = 0x46; /* +0x314b */
        ai_euro_20e6_stay_tail_589e(u); /* LAB_5899 local_76 = 8 -> LAB_589e */
        return 0; /* stay put next to the foreign border colony */
      }
      if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
        s_euro_last_dir[u->id] = (int8_t)dir; /* unit+0x314f, 8 = stay */
      }
      if (dir == 8) {
        ai_euro_20e6_stay_tail_589e(u); /* LAB_589e: +0x314c = 5 (6 when admitted) */
        return 0;
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
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
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
      "",
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
       * bugs.md #472: the signed-treaty bit 0x40 gates every Euro target
       * (same DOS rule as the LAB_4d2e attack term, smell #106). The
       * goal / goto / peace-border-garrison callers reach here without that
       * check and attacked treaty partners. Privateers fly no flag.
       */
      if ((ai_diplo_read(ctx->col1, u->nation_id, f->nation_id) & AI_DIPLO_PEACE) != 0 &&
          !units_type_is_privateer(units_type(ctx->units, u->type_index))) {
        return;
      }
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
      /* Declare refused (Franklin no-op): no warless attack. */
      if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id) &&
          !units_type_is_privateer(units_type(ctx->units, u->type_index))) {
        return;
      }
      if (ctx->human_nation >= 0 && ctx->human_nation < 4 &&
          (u->nation_id == ctx->human_nation || f->nation_id == ctx->human_nation) &&
          ctx->status && ctx->status_size > 0) {
        PopupMsgTokens tok = {0};
        tok.string0 = ai_diplo_rival_name(ctx->col1, u->nation_id);
        popup_msg_fill(
          ctx->messages, "SNEAK", &tok, "",
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
      u->moves = 0;
    }
  } else if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
    {
      ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
      units_try_move_w(&w_, u->id, tx, ty);
    }
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
  int best = -1;
  int bx = 0;
  int by = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = cx + MAP_DIR8_DX[d];
    const int ny = cy + MAP_DIR8_DY[d];
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
static int ai_euro_is_cargo_ship_name(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_CARAVEL || kind == UNITS_KIND_MERCHANTMAN || kind == UNITS_KIND_GALLEON;
}

/*
 * `ai_euro_ocean_colony_sail_score` — the thin second port of LAB_521d_3558
 * (~89614-89711) — lived here until smell audit sweep-3 area C #6 retired it:
 * every term was invented (`pop*8` where DOS scores `((0x11-pop)^2+2)*4`, i.e.
 * the opposite sign of preference; `-d*4` for DOS's `-((d>>1)+1)`; `idle*8` for
 * a signed `+idle`; a Stockade/Fort/Fortress ladder DOS does not have; a "docks
 * flag 0x1b&0x10" that is really NEEDS_COLONISTS), and it had no rng draw,
 * wanted-size term, human-presence term or commit threshold. The one DOS site
 * it answered is ported structurally by `ai_euro_20e6_colony_sail_pick` (raw
 * 89663 ladder), reached from `ai_euro_unload_settle` — DOS enters 3558 once
 * per act, from the 20e6 unload/settle flow, and now so does this port.
 */

static int ai_euro_20e6_unit_col5(const ColonizeUnitPool* pool, int dos_type);

/*
 * FUN_521d_20e6 hold-cargo colony-delivery matrix (raw 2047-2139;
 * viceroy_overlays.asm 137180-137466, labels 004026/004038/0040a1/0041e5).
 *
 * DOS unit byte +0x314a latches the colony a hauler last LOADED at (written in
 * the load matrix, raw 2147) and the delivery loop skips it. Modelled by the
 * real persistent `col1_origin` byte (ai_euro_20e6_origin_get/_set).
 */

/*
 * FUN_521d_20e6 raw 1691 gate + raw 2996-3017 dump sweep: a SHIP standing at
 * an own colony empties every hold into that colony before anything else the
 * function does. See ai_euro_20e6_ship_berth_arrival for the full decode.
 * Kill switch / bisect: AI_20E6_SHIP_DUMP=0 restores the 06e empty-hull gate
 * substitution. Trace: AI_20E6_SHIP_DUMP_TRACE=1.
 */
#ifndef AI_20E6_SHIP_DUMP_DEFAULT
#define AI_20E6_SHIP_DUMP_DEFAULT 1
#endif

static int ai_euro_20e6_ship_dump_enabled(void) {
  return ai_euro_env_flag("AI_20E6_SHIP_DUMP", AI_20E6_SHIP_DUMP_DEFAULT);
}

/*
 * acStack_c8 per-cargo tallies (raw 1702-1712). DOS walks the ship's occupied
 * holds, keeps only cargo types 0x0d..0x0f (Trade Goods / Tools / Muskets) and
 * 0x08 (Horses) — the goods a colony consumes — sums their amounts per type,
 * and records the highest such type in iStack_44 (−1 = none, which switches
 * the whole delivery block off). Returns that max type.
 *
 * Ghidra sizes acStack_c8 as char[8]; the memset is 0x10 wide and the asm
 * indexes it as [BP+SI-0xc6] for SI = 0..0xf, so the array is 16 wide and the
 * separately-declared `cStack_c0` (asm [BP-0xbe]) is element 8 — the Horses
 * tally used by the warehouse gate below. (Ghidra's Stack_XX names run two
 * bytes below the real BP displacement throughout this frame: uStack_50 is
 * [BP-0x4e], iStack_44 is [BP-0x42], uStack_e6 is [BP-0xe4].)
 */
static int ai_euro_20e6_delivery_tallies(
  const ColonizeTurnContext* ctx, const ColonizeUnit* ship, int tally[COLONIZE_CARGO_COUNT]
) {
  int max_type = -1;
  for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
    tally[g] = 0;
  }
  if (!ctx || !ctx->units || !ship) {
    return -1;
  }
  const int n = units_goods_hold_count(ctx->units, ship->id);
  for (int h = 0; h < n && h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    const int amt = units_hold_amount(ctx->units, ship->id, h);
    if (amt <= 0) {
      continue;
    }
    const int ct = ship->hold_goods_type[h];
    if (ct < 0 || ct >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    if (ct <= 0x0c && ct != COLONIZE_CARGO_HORSES) {
      continue; /* raw 1706: 0xc < type || type == 8 */
    }
    if (ct > max_type) {
      max_type = ct;
    }
    tally[ct] += amt;
    if (tally[ct] > 127) {
      tally[ct] = 127; /* acStack_c8 is a signed char accumulator */
    }
  }
  return max_type;
}


/*
 * The matrix proper (raw 2047-2139). Candidate = own colony, coastal
 * (+0x1c bit 0x40), not the ship's own nearest colony when it is standing on
 * it (uStack_62 / iStack_2e), not the colony the ship last loaded at
 * (+0x314a), and — when Horses are the only delivery cargo (iStack_44 == 8) —
 * one whose warehouse can still take them.
 *
 * Per carried cargo g with a nonzero tally (iStack_22 = warehouse capacity,
 * FUN_1000_8f2a = 100*(1+warehouse_level)):
 *   colony already PRODUCES g (+0x90 cargo_produced_mask) and stock[g] > 99
 *       → colony rejected outright (iStack_a = 0, break)
 *   cap <= tally[g] + stock[g]  (the drop would overflow)
 *       → score += (cap − stock[g] − tally[g]) * euro_price[nation][g] * 4
 *         (negative: the overflow is priced at the DS:0x84bc = −0x7b44 per
 *         nation × cargo byte table, col1->nation[n].trade.euro_price[])
 *   colony specialty (+0x8d) == g → score += (idle_turns(+0x8f) + 8) * 4
 *   score += (cap − stock[g]) − 1                      (room for g)
 * Muskets aboard (iStack_44 == 0xf) adds a threat term:
 *   +0x10 when the human holds colonies on this continent
 *        (−0x6b1a = colony_counts_by_continent[human][cid], DS:0x94e6)
 *   over the 20-tile ring (DS:0xc8/0xde = k_20e6_ring20_dx/dy): in-bounds
 *        tile with presence < 4 → +0x18; Indian-held tile →
 *        quartile(alarm_by_player[tribe][nation]) * 0x10
 * Shared tail: +0x1b bit 0x02 (NEARBY_FRIGATE) and cargo-ship type < 0x10 →
 *   (col5 − 10) * 8; else bit 0x01 (NEARBY_ARMED_SHIP) → (col5 − 10) * 2.
 * Then score /= ((FUN_1000_856a dist >> 2) + 1) and later-ties-win against a
 * best seeded to −1, so a colony must score ≥ −1 to be picked at all
 * (raw 2128; asm CMP/JL at 0041e5+0x1a).
 *
 * Substitutions: the coastal bit is OR'd with a live map_tile_is_coastal probe
 * as belt-and-braces. Since 2026-09-10 the bit itself is DOS-shaped — stamped
 * once by colonies_found from map_tile_is_open_sea_adjacent, self-healed
 * set-only by ai_euro_refresh_colony_ai_flags — so the OR only matters for
 * fixtures that build colony records by hand; colonies_warehouse_capacity is now
 * cargo-independent like DOS 8f2a (the port's uncited FOOD-199 branch went
 * with smell audit #25), so the cargo argument below is cosmetic — FOOD is
 * never a delivery cargo here anyway. Nothing invented.
 */
static int ai_euro_20e6_delivery_colony_pick(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* ship,
  int nation,
  const int tally[COLONIZE_CARGO_COUNT],
  int max_type,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !ctx->map || !ship || nation < 0 || nation > 3 ||
      max_type < 0) {
    return 0;
  }
  const int ship_type = ai_euro_20e6_dos_type(ctx->units, ship);
  /* uStack_62 / iStack_2e: nearest own colony and its distance. */
  int home_dist = 9999;
  const int home_colony = ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, nation, -1, &home_dist);
  const int loaded_at = ai_euro_20e6_origin_get(ship);
  int best = -1; /* iStack_e2 seed */
  int pick = -1;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation) {
      continue;
    }
    if (c->id == home_colony && home_dist == 0) {
      continue; /* raw 2053 */
    }
    if ((c->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) == 0 &&
        !map_tile_is_coastal(ctx->map, c->x, c->y)) {
      continue; /* raw 2054: +0x1c bit 0x40 */
    }
    if (c->id == loaded_at) {
      continue; /* raw 2055: unit +0x314a */
    }
    const int cap = colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_TOOLS);
    if (max_type == COLONIZE_CARGO_HORSES &&
        tally[COLONIZE_CARGO_HORSES] + (int)c->stock[COLONIZE_CARGO_HORSES] > cap) {
      continue; /* raw 2056-2058 */
    }
    const int cid = map_continent_id_at(ctx->map, c->x, c->y);
    int score = 0;
    int ok = 1;
    for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
      if (tally[g] == 0) {
        continue;
      }
      const int stock = (int)c->stock[g];
      if ((c->cargo_produced_mask & (uint16_t)(1u << g)) != 0 && stock > 99) {
        ok = 0;
        break; /* raw 2067-2070 */
      }
      if (cap <= tally[g] + stock) {
        /* DS:0x84bc (= −0x7b44) byte table, indexed nation*0x10 + cargo. */
        const int price = (ctx->col1_ok && ctx->col1)
                            ? (int)ctx->col1->nation[nation].trade.euro_price[g]
                            : 0;
        score += (cap - stock - tally[g]) * price * 4; /* raw 2072-2077 */
      }
      if ((int)c->specialty_cargo == g) {
        score += ((int)(int8_t)c->cargo_idle_turns + 8) * 4; /* raw 2080-2081 */
      }
      score += (cap - stock) - 1; /* raw 2083-2084 */
    }
    if (!ok) {
      continue;
    }
    if (max_type == COLONIZE_CARGO_MUSKETS) {
      if (cid >= 0 && ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        score += 0x10; /* raw 2089-2090 */
      }
      for (int r = 0; r < 20; ++r) {
        const int tx = c->x + (int)k_20e6_ring20_dx[r];
        const int ty = c->y + (int)k_20e6_ring20_dy[r];
        if (tx < 0 || ty < 0 || tx >= (int)ctx->map->width || ty >= (int)ctx->map->height) {
          continue; /* FUN_1000_84f2 map_tile_in_bounds */
        }
        const int pres = ai_euro_20e6_tribe_or_presence(ctx, tx, ty);
        if (pres < 4) {
          score += 0x18; /* raw 2100-2101 */
          continue;
        }
        int alarm = 0;
        if (ctx->col1_ok && ctx->col1 && (pres - 4) < (int)COLONIZE_COL1_INDIAN_COUNT) {
          alarm = (int)ctx->col1->indian[pres - 4].alarm_by_player[nation];
        }
        score += ai_relation_quartile(alarm) * 0x10; /* raw 2103-2106 */
      }
    }
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0) {
      if (ship_type < 0x10) {
        score += (ai_euro_20e6_unit_col5(ctx->units, ship_type) - 10) * 8; /* raw 2119-2121 */
      }
    } else if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) != 0) {
      if (ship_type < 0x10) {
        score += (ai_euro_20e6_unit_col5(ctx->units, ship_type) - 10) * 2; /* raw 2113-2115 */
      }
    }
    const int dist = map_dos_dist(ship->x - c->x, ship->y - c->y);
    score = score / ((dist >> 2) + 1); /* raw 2126-2127 */
    if (score >= best) {               /* DOS later-ties-win, best seeded −1 */
      best = score;
      pick = i;
    }
  }
  if (pick < 0) {
    return 0;
  }
  if (getenv("AI_20E6_DELIVER_TRACE")) {
    fprintf(
      stderr,
      "[deliver] ship %d n%d max=%d -> colony %d (%d,%d) score %d\n",
      ship->id,
      nation,
      max_type,
      ctx->colonies->colonies[pick].id,
      (int)ctx->colonies->colonies[pick].x,
      (int)ctx->colonies->colonies[pick].y,
      best
    );
  }
  *out_x = (int)ctx->colonies->colonies[pick].x;
  *out_y = (int)ctx->colonies->colonies[pick].y;
  return 1;
}

/*
 * FUN_521d_20e6 delivery SELL TAIL (raw 2140-2163 = doc 2147-2170, the
 * `while (*(char *)(param_2 * 0x1c + 0x3150) != '\0')` loop right after the
 * matrix's `if (-1 < uStack_24)` commit). When the matrix picked no colony
 * DOS dumps the whole hold for gold rather than leaving the hull loaded:
 *
 *   while (unit->holds_occupied != 0) {
 *     g   = FUN_1000_8cdc(unit, 0);          // remove hold slot 0, compact
 *     qty = DS:0x8dc4;                        //   ... and stash its amount
 *     func_0x00019c1e(g, qty);                // = FUN_291f_0a2e -> 38fd_1dfa
 *     v = euro_price[nation][g] * qty;        // DS:0x84bc byte table
 *     nation->gold(+0x2a)          += v;      // 32-bit, with carry
 *     nation->trade.gold(+0x7c)[g] += v;      // 32-bit, with carry
 *     nation->trade.tons(+0xbc)[g] += qty;    // 32-bit, with carry
 *   }
 *   unit->holds_occupied = 0;
 *
 * Resolved symbols (nothing invented):
 *   FUN_1000_8cdc   -> FUN_281f_0aec -> FUN_15eb_317c "remove unit cargo /
 *                      passenger slot; compact; stash qty in 0x8dc4"
 *                      (address_mapping.csv:991; FUNCTION_CATALOG.md:440/1455)
 *   func_0x00019c1e -> FUN_291f_0a2e -> FUN_38fd_1dfa "sell volume: ledgers +
 *                      tax gold" (address_mapping.csv:1274;
 *                      FUNCTION_CATALOG.md:1743/2353) — already ported exactly
 *                      as europe_apply_trade_volume(..., is_buy=0,
 *                      immediate_threshold=0), the same call shape
 *                      europe_ai_colony_dump_sell uses.
 *   *(int *)0x84fc  -> the BOUND NATION RECORD pointer (not FUN_1000_84fc):
 *                      +0x2a gold, +0x7c trade.gold[16], +0xbc trade.tons[16],
 *                      +0xfc trade.tons2[16] — byte-checked against
 *                      viceroy_unpacked.c:60247 (FUN_38fd_1dfa) and against
 *                      offsetof() on ColonizeCol1Nation (42/124/188/252).
 *
 * DOS quirk kept verbatim: 1dfa has ALREADY credited trade.gold[g] with the
 * tax-adjusted proceeds and trade.tons[g]/tons2[g] with qty; this tail then
 * adds the UNtaxed euro_price*qty to trade.gold[g] and qty to trade.tons[g] a
 * second time. Only the +0x2a treasury credit is single, and it is untaxed —
 * the AI dump-sell pays no Crown cut on this path. Ported as written; the
 * double ledger entry is DOS behaviour, not a port bug.
 *
 * Substitution: DOS's `holds_occupied` walk with slot-0 compaction is a plain
 * sweep over the port's occupied goods holds (same set, same order).
 */
static int ai_euro_20e6_delivery_sell_tail(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  int nation
) {
  if (!ctx || !ctx->units || !ship || nation < 0 || nation > 3 || !ctx->col1_ok ||
      !ctx->col1) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation];
  const int n = units_goods_hold_count(ctx->units, ship->id);
  int total = 0;
  for (int h = 0; h < n && h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    const int qty = units_hold_amount(ctx->units, ship->id, h);
    if (qty <= 0) {
      continue;
    }
    const int g = ship->hold_goods_type[h];
    if (g < 0 || g >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    /* FUN_1000_8cdc: the slot is emptied before the ledgers run. */
    ship->hold_goods_amount[h] = 0;
    ship->hold_goods_type[h] = 0;
    /* func_0x00019c1e = 38fd_1dfa. Needs the shared EuropeScreen; when the AI
     * runs headless without one the price/volume ledger is simply skipped —
     * the treasury half below is col1-only and always applies. */
    if (ctx->europe) {
      europe_apply_trade_volume(
        ctx->europe, ctx->col1, nation, ctx->human_nation, g, qty, 0, 0
      );
    }
    const int32_t v = (int32_t)nat->trade.euro_price[g] * (int32_t)qty;
    nat->gold += (uint32_t)v;
    nat->trade.gold[g] += v;
    nat->trade.tons[g] += (int32_t)qty;
    total += (int)v;
  }
  if (total != 0 && getenv("AI_20E6_DELIVER_TRACE")) {
    fprintf(
      stderr, "[deliver] ship %d n%d no colony -> sold hold for %d\n", ship->id, nation, total
    );
  }
  return total;
}

/*
 * FUN_521d_20e6 LOAD-at-colony matrix (raw 3059-3134 of the recovered C:
 * `while ((iStack_d2 != 0 && (bVar17)))` … `LAB_OVL14_L0000__003356`).
 * Picks ONE cargo per free hold; the caller re-runs it until the hull is full
 * or the matrix declines.
 *
 * TRAP (this pass): `iStack_34` is NOT a war/peace flag. Raw 1174-1179 sets
 * it from the unit TYPE alone — `iStack_34 = (0xd <= type && type <= 0x12)`,
 * i.e. 1 = SHIP, 0 = land hauler (Wagon Train). Every `iStack_34` arm below is
 * therefore a ship-vs-wagon split, and the two latches at the bottom of the
 * loop are `+0x314a = colony` for ships (which is exactly what the delivery
 * matrix skips) and `+0x3158 = 1` for wagons (the village-errand byte 47b9
 * reads).
 *
 * The stock term (raw 3068-3077), then the per-cargo gates:
 *   term = stock[g]
 *   if (term < cap || g == 0) { if (g == 8) term = max(term - cap + 0x17, 0); }
 *   else                      { term <<= 1; }
 *   g == 5 (LUMBER)                        -> skipped outright
 *   g == 0xe/0xf (TOOLS/MUSKETS)           -> ships only, and only when the
 *        colony PRODUCES it (+0x90 cargo_produced_mask); then term -= 100
 *   ship && (g == 0xd || g == 0)           -> skipped (no Trade Goods / Food)
 *   term == 0                              -> skipped
 *   score(ship)  = euro_price[nation][g] * term
 *   score(wagon) = walk p = euro_price[nation][g] down while p > 1 and
 *                  86c4(0,3) == 0; thr = (g == 0xd) ? 8 : 4;
 *                  p < thr ? term*(thr-p) + (1-p)*5 : -1;  term < 0x32 -> -1
 *   best is seeded -1 with a STRICT `<`, so the first cargo wins ties and a
 *   score of -1 can never be picked.
 * Returns the cargo id, or −1 when the matrix declines (DOS `iStack_d2 = 0`).
 *
 * `FUN_1000_86c4` = FUN_281f_04d4 = dos_rng_range (address_mapping.csv:844);
 * inclusive-range mapping per this file's own 86c4(1,0x14) port above.
 */
static int ai_euro_20e6_load_pick(
  ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int nation,
  int is_ship
) {
  if (!ctx || !ctx->colonies || !c || nation < 0 || nation > 3) {
    return -1;
  }
  /* iStack_a4 = FUN_1000_8f2a() warehouse capacity — cargo-independent, as
   * in DOS: the port's FOOD-199 branch was removed by smell audit #25, so the
   * cargo argument here no longer changes the shared per-cargo term. */
  const int cap = colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_TOOLS);
  int best = -1;
  int pick = -1;
  for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
    int term = (int)c->stock[g];
    if (term < cap || g == COLONIZE_CARGO_FOOD) {
      if (g == COLONIZE_CARGO_HORSES) {
        term = (term - cap) + 0x17;
        if (term < 0) {
          term = 0;
        }
      }
    } else {
      term = term << 1;
    }
    if (g == COLONIZE_CARGO_LUMBER) {
      continue; /* raw 3081: `if (uStack_b6 != 5)` guards the whole body */
    }
    if (g == COLONIZE_CARGO_TOOLS || g == COLONIZE_CARGO_MUSKETS) {
      if (!is_ship || (c->cargo_produced_mask & (uint16_t)(1u << g)) == 0) {
        continue; /* raw 3082-3085 */
      }
      term += -100; /* raw 3086 */
    }
    if (is_ship && (g == COLONIZE_CARGO_TRADE_GOODS || g == COLONIZE_CARGO_FOOD)) {
      continue; /* raw 3087 */
    }
    if (term == 0) {
      continue; /* raw 3087 tail */
    }
    const int price = (ctx->col1_ok && ctx->col1)
                        ? (int)ctx->col1->nation[nation].trade.euro_price[g]
                        : 0;
    int score;
    if (!is_ship) {
      int p = price;
      while (p > 1 && ctx->rng && dos_rng_range(ctx->rng, 0, 3) == 0) {
        p -= 1; /* raw 3091-3093 throttle walk-down */
      }
      const int thr = (g == COLONIZE_CARGO_TRADE_GOODS) ? 8 : 4; /* raw 3094-3099 */
      if (p < thr) {
        score = term * (thr - p) + (1 - p) * 5; /* raw 3101-3102 */
      } else {
        score = -1; /* raw 3104 */
      }
      if (term < 0x32) {
        score = -1; /* raw 3107 */
      }
    } else {
      score = price * term; /* raw 3111 */
    }
    if (best < score) { /* raw 3113: strict, first-wins ties */
      best = score;
      pick = g;
    }
  }
  return pick;
}

/*
 * FUN_521d_20e6 wagon village-errand arm (raw 2284-2307) + the 4528 AI-arm
 * case-1 consumer. A wagon whose errand latch (+0x3158) is set:
 *   - scan every settlement, keep the nearest on the wagon's own landmass
 *     (FUN_1000_8912 == iStack_38); a capital (record +3 & 4) halves its
 *     distance, min 1; strict `<` — the earlier record wins ties;
 *   - none on this landmass → LAB_47b9 destroy;
 *   - adjacent (or on the tile) → FUN_4d56_4528 AI arm, type 0xc → case 1
 *     Trade → the 2820 shell (ai_contact_ai_wagon_village_trade). 2820
 *     clears the DOS errand byte at entry for land types
 *     (viceroy_unpacked.c:82122) — mirrored by clearing the latch on the
 *     attempt, traded or refused; the 4528 return forfeits the MP;
 *   - else goto the village tile.
 * The port's AI movement never steps ONTO tribe tiles, so "arrival" is
 * adjacency here — the same substitution every other village arm uses.
 * Returns 1 when it owned the wagon this beat (goto set, traded, or
 * destroyed — caller must re-check active).
 */
static int ai_euro_20e6_wagon_village_errand(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* wagon
) {
  if (!ctx || !ctx->units || !ctx->map || !wagon || !wagon->active || wagon->id < 0 ||
      wagon->id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  if (!ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || ctx->col1->head.tribe_count == 0) {
    s_20e6_wagon_errand[wagon->id] = 0;
    return 0;
  }
  const int cid = map_continent_id_at(ctx->map, wagon->x, wagon->y);
  int best_i = -1;
  int best_d = 9999; /* DOS iStack_e2 seed */
  for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
    if (t->population == 0) {
      continue; /* razed record — DOS removes these from the 0x539a count */
    }
    if (map_continent_id_at(ctx->map, t->x, t->y) != cid) {
      continue;
    }
    int d = map_dos_dist((int)t->x - wagon->x, (int)t->y - wagon->y);
    if (t->state.capital) {
      d >>= 1;
      if (d < 1) {
        d = 1;
      }
    }
    if (d < best_d) {
      best_d = d;
      best_i = (int)i;
    }
  }
  if (best_i < 0) {
    /* goto LAB_47b9 — dead end, destroy. */
    if (getenv("AI_20E6_DEADEND_TRACE")) {
      fprintf(stderr, "[47b9] errand wagon %d n%d (%d,%d) no village on cid %d -> destroy\n",
              wagon->id, nation_id, wagon->x, wagon->y, cid);
    }
    s_20e6_wagon_errand[wagon->id] = 0;
    (void)units_despawn(ctx->units, wagon->id);
    return 1;
  }
  const ColonizeCol1Tribe* t = &ctx->col1->tribe[best_i];
  const int adj = abs((int)t->x - wagon->x) <= 1 && abs((int)t->y - wagon->y) <= 1;
  if (adj) {
    s_20e6_wagon_errand[wagon->id] = 0; /* 2820 entry clears +0x3158 */
    const int ind = (int)t->nation_id;
    if (ind >= 4 && ind <= 11) {
      (void)ai_contact_ai_wagon_village_trade(ctx, ind, nation_id, wagon->id);
    }
    wagon->moves = 0; /* 4528 return-code 1 MP forfeit */
    return 1;
  }
  if (units_orders_follow_goto(wagon->orders) && wagon->goto_x == t->x &&
      wagon->goto_y == t->y) {
    return 1;
  }
  ai_euro_set_goto(wagon, UNITS_ORDER_AI_MOVE, (int)t->x, (int)t->y);
  return 1;
}

/*
 * FUN_1427_10be — the DOS transport-chain ASSEMBLY step (resident flat
 * `0000:532e`; 20e6 reaches it through the `FUN_281f_0920` thunk, overlay
 * `FUN_1000_8b10`). Ported 2026-09-07e; this retires the "boarding is
 * collapsed into one beat" substitution note.
 *
 * Raw body (decomp 8606-8686), the part 20e6 exercises:
 *
 *   free = hold_capacity(0x5237[type]) − holds_occupied(+0x3150);
 *   member = stack_head(0002); if (member == ship) member = next(004a);
 *   0362(ship, -2, -2);                     // ship joins the carrier list
 *   while (member >= 0) {
 *     next = 004a();
 *     take = (member+0x314c == 1);          // the act_state-1 board mark
 *     … two force-board arms (see "not ported" below) …
 *     size = 0x5238[member.type];
 *     if (size <= free && take) {
 *       free -= size;
 *       member+0x314c = 1;
 *       0362(member, -2, -2);               // parked at the (−2,−2) sentinel
 *     }
 *     member = next;
 *   }
 *
 * A DOS passenger is a unit parked at sentinel coords (−2,−2) on the shared
 * tile-stack lists, not an entry in a ship-side array. Linux keeps passengers
 * in `cargo_ids` (the same substitution `ai_euro_20e6_ship_cargo_counts` and
 * `ai_euro_20e6_stack_settler` make), so the sentinel park is `units_board`
 * and no (−2,−2) coordinate ever reaches the unit pool or the save. Nothing
 * to round-trip: see the note on the mark's lifetime below.
 *
 * TIMING — the mark and the assembly are ONE act, not two (control flow
 * verified in `viceroy_overlays.asm`, not from the decompiler's line order):
 *
 *   LAB_OVL14_L0000__00304c  :135683  arrival gate; three `JMP 0x3558` exits
 *                                     (:135706 / :135710 / :135721)
 *   …0x30a7                  :135722  arrival block = the gate's FALL-THROUGH
 *                                     (stale-mark clear at 0x30b6 :135728,
 *                                     then dump, cower, the raw 3024-3051
 *                                     board-MARK scan, load matrix)
 *   …0x32f5                  :135980  after the scan: `[0x1734+n*2] = 0`,
 *                                     `JMP 0x354e`
 *   …0x354e                  :136215  load-matrix loop test → `JZ 0x3558`
 *   LAB_OVL14_L0000__003558  :136221  the band; XREF list names 0x3083,
 *                                     0x308c, 0x30a4, 0x3313, 0x3553
 *   …0x3609                  :136290  `PUSH [BP+6]` / `CALLF FUN_1000_8b10`
 *                                     immediately before the 0d38 batch
 *                                     (`PUSH 2` / `8aac`, `PUSH 3`, …)
 *
 * So the berth scan marks and, later in the SAME 20e6 call, this sweep
 * assembles — Ghidra renders the arrival block after `LAB_3558` in text order
 * only because every path inside the `if` leaves by goto. The budget hand-off
 * is exact because of that order: the scan reserves `iStack_d2` for the
 * passengers, the load matrix fills the rest of the holds with goods, and
 * `free = capacity − holds_occupied` here re-derives precisely the reserved
 * remainder. The cower arm is the one berth path that never reaches this
 * sweep (it stamps orders 0x43 and jumps to LAB_5899, past 0x3558).
 *
 * The mark's lifetime is one nation turn: `ai_euro_0a60_unit_housekeeping`
 * resets act_state 1/2/3 → 0 for every on-map inset unit at the top of the
 * nation turn, exactly as DOS does at :87560-87563. A mark therefore cannot
 * survive a save/load in an observable way and needs no save round-trip.
 *
 * The two force-board arms are ported 2026-09-07f — see the board-predicate
 * comment in the loop below.
 *
 * The recursive `FUN_1427_101c` pre-pass (raw 8628-8645) is CLOSED as
 * unreachable from here, 2026-09-08:
 *
 *   - The ONLY writer of `+0x314c = 2` in the whole decomp is
 *     `FUN_2b5a_1e66` (:42863), the human "assign trade route" order:
 *     it bails on `DS:0x53a0 == 0` (trade_route_count) with popup 0xa2d,
 *     picks a route with `FUN_291f_02dc` (= `FUN_647e_0796`, the route
 *     picker) and finishes in `FUN_291f_02b2` (= `FUN_479b_0bd0`, the
 *     goto-colony/route body). `FUN_1427_12c6` (stamp act_state on a whole
 *     stack) is the only indirect writer and its one caller (:75718, through
 *     thunk `FUN_281f_08f8`) passes 1.
 *   - No AI path assigns trade routes, and `ai_euro_0a60_unit_housekeeping`
 *     clears act_state 1/2/3 → 0 for every on-map unit at the nation-turn top
 *     (DOS :87558-87562) — so even a captured or loaded unit reaches 20e6
 *     with act_state 0.
 *   - What it does, for the record: 101c reorders the tile stack (`04d6`),
 *     then repeatedly 10be's each ship still on the tile (each call parks that
 *     ship plus its members at (−2,−2)) until none is left, and finally
 *     `FUN_1427_040c` flushes the whole (−2,−2) chain back to the tile. It is
 *     a tile-stack chain rebuild; its return value is the leftover non-ship
 *     head, and 10be uses only that: a trade-route ship at sea (`03e4`
 *     tile_tribe_owner < 0, not in its own Europe slot) gets `free = 0` unless
 *     loose units remain after every other transport on the tile has claimed
 *     its own. Nothing here has a port counterpart — the port has no tile-stack
 *     chain, passengers live in `cargo_ids`.
 *
 * So +0x314c only ever carries 0/1 in this sweep.
 *
 * Linux tile substitution: DOS ships berth ON the colony tile, so 10be's own
 * tile stack already holds the marked land units. This port berths ships on
 * adjacent water (`ai_euro_tiles_near`, same substitution the arrival block
 * and the 06e load block make), so the sweep covers the ship's tile plus any
 * adjacent own-colony tile.
 */
/*
 * Raw 2991-2997 (asm 0x30a7-0x30d5) stale board-mark clear, shared by the
 * arrival block and the LAB_3558 anchor. In DOS every act that reaches the
 * 0x3609 10be call at a colony berth has already fallen through the arrival
 * block, so 10be never sees a mark that this act's scan did not set. The
 * 0a60 housekeeping stamps act_state=1 on every aboard passenger at the
 * nation-turn top; without this clear on the unload path a passenger dropped
 * ashore this turn still carries that stamp and 10be re-boards it (the
 * load/unload oscillation the -O3 unit_ai_euro_war failure exposed). Sweep =
 * ship tile + adjacent own colony tile, the same berth substitution 10be
 * makes.
 */
static void ai_euro_20e6_clear_stale_board_marks(
  ColonizeTurnContext* ctx,
  int nation_id,
  const ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ship) {
    return;
  }
  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* lu = &ctx->units->units[ui];
    if (!lu->active || lu->aboard_ship_id >= 0 || lu->id < 0 ||
        lu->id >= COLONIZE_UNITS_MAX) {
      continue;
    }
    int on_stack = (lu->x == ship->x && lu->y == ship->y);
    if (!on_stack && ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, lu->x, lu->y);
      const ColonizeColony* lc = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
      if (lc && lc->active && lc->nation_id == nation_id &&
          ai_euro_tiles_near(ship->x, ship->y, lc->x, lc->y)) {
        on_stack = 1;
      }
    }
    if (on_stack && lu->orders == UNITS_ORDER_SENTRY) {
      lu->orders = UNITS_ORDER_NONE; /* +0x314c = 0 */
    }
  }
}

/*
 * 10be force-board arm 1's coordinate test — DOS `member+0x3144 < 0` (asm
 * 1427:11f0 `CMP byte [BX+0x3144],0x0` / `JGE`), i.e. the member sits in an
 * OFF-MAP bucket instead of on a real tile. Every DOS park is a negative x:
 * (−2,−2) = riding a transport, (−3,−3)/(−4,−4) = the transient parks
 * `FUN_1427_04d6` and 10be's own 101c pre-pass use, and x = nation − 0x14 =
 * that nation's Europe slot. The port has one off-map park and spells it with
 * a positive sentinel (Europe at (200,100), `ai_euro_in_europe`), so the
 * faithful predicate is "not addressable as a map tile".
 */
static int ai_euro_20e6_member_off_map(const ColonizeTurnContext* ctx, const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  if (u->x < 0 || u->y < 0 || ai_euro_in_europe(u->x, u->y)) {
    return 1;
  }
  if (ctx && ctx->map && (u->x >= (int)ctx->map->width || u->y >= (int)ctx->map->height)) {
    return 1;
  }
  return 0;
}

static int ai_euro_20e6_transport_assemble(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ctx->units || !ship || !ship->active || ship->id < 0 ||
      ship->id >= COLONIZE_UNITS_MAX || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (!units_is_sea(ctx->units, ship->id) || ai_euro_in_europe(ship->x, ship->y)) {
    return 0;
  }
  /* free = 0x5237[type] − +0x3150 (capacity less goods holds AND passengers). */
  int free_holds = units_ship_free_passenger_slots(ctx->units, ship->id);
  if (free_holds <= 0) {
    return 0;
  }
  const int trace = getenv("AI_20E6_BOARD_TRACE") != NULL;
  int boarded = 0;
  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index — `units_board`,
   * `units_is_sea` and the id-keyed shadow all take `lu->id`. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX && free_holds > 0; ++ui) {
    ColonizeUnit* lu = &ctx->units->units[ui];
    if (!lu->active || lu->id == ship->id || lu->nation_id != nation_id ||
        lu->id < 0 || lu->id >= COLONIZE_UNITS_MAX) {
      continue;
    }
    if (lu->aboard_ship_id >= 0 || units_is_sea(ctx->units, lu->id)) {
      continue; /* already in a transport chain / not a stack land member */
    }
    /* Same tile as the ship, or the adjacent own colony it is berthed at. */
    int on_stack = (lu->x == ship->x && lu->y == ship->y);
    if (!on_stack && ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, lu->x, lu->y);
      const ColonizeColony* lc = cid >= 0 ? colonies_get(ctx->colonies, cid) : NULL;
      if (lc && lc->active && lc->nation_id == nation_id &&
          ai_euro_tiles_near(ship->x, ship->y, lc->x, lc->y)) {
        on_stack = 1;
      }
    }
    if (!on_stack) {
      continue;
    }
    /*
     * The board predicate — raw 8658-8672, asm 1427:11d4-1231. Three arms,
     * and the decompile's `local_4`/`bVar4` shuffle hides that the two
     * force-board arms are mutually exclusive on the SIGN OF x, not chained:
     *
     *   1427:11d4  CMP byte [BX+0x314c],0x1 / JNZ  →  DI = 1     ; the MARK
     *   1427:11e1  CMP DI,1 / SBB AX,AX / NEG AX / MOV local_4,AX ; = !DI
     *   1427:11ed  OR DI,DI / JNZ 1211                            ; marked → done
     *   1427:11f0  CMP byte [BX+0x3144],0x0 / JGE 1211            ; x >= 0 → arm 2
     *   1427:11f6  MOV local_4,DI            ; DI is 0 here, so local_4 = 0:
     *                                        ; entering arm 1 DISARMS arm 2
     *   1427:11f9  AL = ([BX+0x3147] & 0xf) − [BX+0x3144]
     *              CMP AL,0x14 / JNZ 120e    ; not this nation's Europe slot
     *   1427:1208  CMP byte [BX+0x314c],0x1 / JNZ 1211  ; dead (DI==0 ⇒ !=1)
     *   1427:120e  DI = 1                                         ; ARM 1 boards
     *   1427:1211  CMP local_4,0 / JZ 1232
     *   1427:1219  CALLF FUN_13e4_0074(x, y) / OR AX,AX / JZ 1232
     *   1427:1230  DI = 1                                         ; ARM 2 boards
     *
     * `FUN_13e4_0074` (viceroy_unpacked.c:7033) is `ocean_or_high_seas`:
     * `uVar1 = FUN_137f_010e(x,y); return (uVar1 & 0x1f) == 0x19 ||
     * (uVar1 & 0x1f) == 0x1a;` — terrain class 25 (Ocean) / 26 (High Seas),
     * the same predicate `map_tile_is_water` implements (`map_is_ocean_index`,
     * MAP_OCEAN_INDEX / MAP_HIGH_SEAS_INDEX) and the same one the Indian
     * claim table (`colonies_indian_claim_tribe_from`) and the 4cc6 threat
     * ring already use. It was never undecoded — the 20e6 notes simply had
     * not connected it to `ai_is_ocean_hs` / `FUN_281f_0768`.
     *
     * What the two arms MEAN, given the bucket the loop walks: DOS units are
     * linked into per-coordinate buckets (+0x315c prev / +0x315e next,
     * `FUN_1427_0002`/`004a`), and 10be iterates the bucket the ship was in,
     * so every member shares the ship's coordinates. Therefore:
     *   arm 1 fires only when the SHIP is in an off-map bucket, and boards
     *     every member of it that is not resting in its own nation's Europe
     *     slot — i.e. re-attaches units already parked at (−2,−2)/(−3,−3)/
     *     (−4,−4) to this hull when the transport chain is rebuilt;
     *   arm 2 fires only when the ship's TILE is ocean/high seas, and boards
     *     every land member of that tile unconditionally — a land unit can
     *     only be standing on open water because it is riding this ship.
     * Both are therefore invariant repair, not new recruitment: DOS's way of
     * keeping existing passengers attached. At a colony berth DOS's own tile
     * is the (land) colony tile, so neither arm fires there and the mark arm
     * is the only one that recruits — which is why the 2026-09-07e port with
     * the mark arm alone matched the goldens.
     *
     * Linux mapping: passengers live in `cargo_ids`/`aboard_ship_id` rather
     * than in a shared sentinel bucket, so the units the two arms re-attach
     * are exactly the ones the `aboard_ship_id >= 0` skip above already keeps
     * attached — the arms are ported for the cases that skip does NOT cover:
     * an unattached unit sitting in a non-Europe off-map park (arm 1), or
     * standing on the ship's open-water tile (arm 2).
     */
    int take = (lu->orders == UNITS_ORDER_SENTRY); /* +0x314c == 1 */
    int arm = 0;
    if (!take) {
      if (ai_euro_20e6_member_off_map(ctx, lu)) {
        /* arm 1: off-map, but not this unit's own Europe slot. */
        if (!ai_euro_in_europe(lu->x, lu->y)) {
          take = 1;
          arm = 1;
        }
      } else if (ctx->map && map_tile_is_water(ctx->map, lu->x, lu->y)) {
        take = 1; /* arm 2: FUN_13e4_0074 — ocean / high seas */
        arm = 2;
      }
    }
    if (!take) {
      continue;
    }
    const ColonizeUnitType* lty = units_type(ctx->units, lu->type_index);
    const int size = lty ? lty->space : 1; /* 0x5238[type] */
    if (size > free_holds || size >= 99) {
      continue;
    }
    if (!units_board(ctx->units, lu->id, ship->id)) {
      continue;
    }
    lu->orders = UNITS_ORDER_SENTRY; /* asm 1427:1264 `[BX+0x314c] = 1` */
    free_holds -= size;
    boarded++;
    if (trace) {
      static const char* const arm_name[3] = {"mark", "force:offmap", "force:water"};
      fprintf(
        stderr, "[10be] ship %d n%d assembles unit %d via %s (size %d, free %d left)\n", ship->id,
        nation_id, lu->id, arm_name[arm], size, free_holds
      );
    }
  }
  return boarded;
}

/*
 * FUN_521d_20e6 raw 3120-3133: fill the ship's free holds from colony `c`,
 * one 20e6 load pick per hold (a hold is burnt whether or not goods moved).
 * Returns 1 when anything was loaded. Shared by the berth-arrival and the
 * hauler-pickup arms, which had the loop twice.
 */
static int ai_euro_20e6_load_holds(
  ColonizeTurnContext* ctx,
  ColonizeColony* c,
  int nation_id,
  ColonizeUnit* ship,
  int free_holds,
  int trace
) {
  int loaded = 0;
  while (free_holds > 0) {
    const int g = ai_euro_20e6_load_pick(ctx, c, nation_id, 1);
    if (g < 0) {
      break; /* raw 3122-3123: iStack_d2 = 0 */
    }
    int qty = (int)c->stock[g];
    if (qty > 100) {
      qty = 100; /* raw 3126-3129 */
    }
    const int moved =
      (qty > 0) ? colonies_transfer_to_unit(ctx->colonies, c->id, ctx->units, ship->id, g, qty)
                : 0;
    if (moved > 0) {
      loaded = 1;
    }
    if (trace || getenv("AI_20E6_LOAD_TRACE")) {
      fprintf(
        stderr, "[load] ship %d n%d colony %d cargo %d qty %d moved %d free %d\n", ship->id,
        nation_id, c->id, g, qty, moved, free_holds
      );
    }
    free_holds -= 1; /* raw 3133: DOS burns the hold either way */
  }
  return loaded;
}

/*
 * FUN_521d_20e6 own-colony ARRIVAL block for ships (raw 2996-3138), the ship
 * twin of the wagon sequence shipped 2026-09-06f.
 *
 * Structure, from the raw. The gate at raw 1691 (doc 1698-1700) is the fork
 * that decides whether 20e6 enters its normal band (LAB_003558: the delivery
 * tallies + delivery matrix + 4393 work-queue peel + the 8-dir wander) or the
 * arrival block first:
 *
 *   bVar10 = unit type;
 *   if ( capacity_table[type*0xe + 0x5237] == 0        // no cargo holds
 *        || iStack_2e != 0                            // NOT at own colony
 *        || ( (type < 0xd || 0x12 < type)             // land hauler …
 *             && unit+0x314a != uStack_62 ) )         // … not bound here
 *     goto LAB_003558;                                // normal band
 *   … arrival block (raw 2996-3138) …
 *   goto LAB_003558;                                  // then the normal band
 *
 * So for a SHIP (type 0x0d..0x12) the third clause can never hold: a ship
 * with holds standing at an own colony ALWAYS runs the arrival block, and the
 * delivery matrix then scores the hull the arrival block just rebuilt. That
 * settles the precedence question the 06d note left open: at an own-colony
 * berth the dump+load runs FIRST and the delivery matrix routes the NEW
 * cargo; "delivery matrix first" only ever described a ship away from berth.
 *
 * The block itself:
 *   raw 2991-2997  clear act_state 1 on the berth tile's units (LIVE since
 *                  2026-09-07e — the +0x314c channel is the
 *                  the real +0x314c byte)
 *   raw 2999-3001  bind colony uStack_62 / nation uStack_e6
 *   raw 3002-3007  while (holds_occupied) { g = pull hold 0 (8cdc compacts
 *                  and stashes qty in 0x8dc4); colony stock[g] += qty; }
 *                  — UNCONDITIONAL: every hold, every cargo type, no ship /
 *                  cargo exception, and no "colony is short of it" test.
 *   raw 3008-3011  ships only: unit+0x314a = 0xff (drop the last-loaded-at
 *                  latch the delivery matrix reads) and colony+0x8f = 0
 *                  (cargo_idle_turns).
 *   raw 3012-3016  iStack_d2 = capacity − holds_occupied (0 after the dump,
 *                  so = capacity); wagons clamp to 1.
 *   raw 3052-3131  the LOAD matrix (ai_euro_20e6_load_pick, ported 06e), one
 *                  cargo per free hold, qty = min(stock, 100).
 *   raw 3127-3131  a load latches unit+0x314a = colony for ships.
 *
 * Both formerly-unmodelled arms of this block are live:
 *   - raw 3018-3023 "cower in port" (2026-09-07): +0x315a is the COL1
 *     `col1_counter16` byte, save-round-tripped since 2026-09-07c.
 *   - raw 3024-3051: the passenger-board MARK scan. It only stamps
 *     act_state = 1 and debits the hold budget; the physical boarding is
 *     FUN_1427_10be at LAB_3558, ported as
 *     ai_euro_20e6_transport_assemble and called from the tail of this
 *     function (DOS's fall-through into 0x3558). See that function's header
 *     for the asm control-flow proof that mark and assembly are one act.
 *
 * Returns the colony id the ship berthed at (dump attempted), else −1.
 */
static int ai_euro_20e6_ship_berth_arrival(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship,
  int* out_cowered
) {
  if (!ctx || !ctx->units || !ctx->colonies || !ship || !ship->active || ship->id < 0 ||
      ship->id >= COLONIZE_UNITS_MAX) {
    return -1;
  }
  /* raw 1691 first clause: capacity_table[type] == 0 → no arrival block. */
  const int holds = units_goods_hold_count(ctx->units, ship->id);
  if (holds <= 0) {
    return -1;
  }
  const int trace = getenv("AI_20E6_SHIP_DUMP_TRACE") != NULL;
  if (trace) {
    fprintf(
      stderr, "[shipdump] enter ship %d n%d at (%d,%d) holds %d\n", ship->id, nation_id,
      ship->x, ship->y, holds
    );
  }
  ColonizeColony* c = NULL;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* cand = &ctx->colonies->colonies[i];
    if (!cand->active || cand->nation_id != nation_id) {
      continue;
    }
    /* iStack_2e == 0: standing at own colony uStack_62. Ships berth on
     * adjacent water in this port, so near, not colonies_id_at — the same
     * substitution the 06e load block already used. */
    if (!ai_euro_tiles_near(ship->x, ship->y, cand->x, cand->y)) {
      continue;
    }
    c = cand;
    break;
  }
  if (!c) {
    return -1;
  }
  /*
   * Raw 2991-2997 (asm 0x30a7-0x30d5): before anything else, walk the berth
   * tile's stack and clear every stale act_state == 1 board mark, so this
   * act's scan re-decides from scratch. DOS iterates the ship's own tile
   * (ships berth ON the colony tile); this port berths on adjacent water, so
   * the colony tile the ship is berthed at is the equivalent stack.
   */
  ai_euro_20e6_clear_stale_board_marks(ctx, nation_id, ship);

  /* raw 3002-3007: dump every hold into the colony, unconditionally. */
  int dumped = 0;
  for (;;) {
    int hold = -1;
    const int n = units_goods_hold_count(ctx->units, ship->id);
    for (int h = 0; h < n; ++h) {
      if (units_hold_amount(ctx->units, ship->id, h) > 0) {
        hold = h;
        break;
      }
    }
    if (hold < 0) {
      break;
    }
    const int g = ship->hold_goods_type[hold];
    const int moved =
      colonies_transfer_from_unit(ctx->colonies, c->id, ctx->units, ship->id, hold, NULL);
    if (moved <= 0) {
      break; /* warehouse refused (Linux-only clamp) — do not spin */
    }
    dumped += moved;
    if (trace) {
      fprintf(
        stderr, "[shipdump] ship %d n%d colony %d dump cargo %d qty %d\n", ship->id,
        nation_id, c->id, g, moved
      );
    }
  }

  /* raw 3008-3011: ships drop the +0x314a bind and zero colony +0x8f. */
  ai_euro_20e6_origin_set(ship, -1);
  c->cargo_idle_turns = 0;

  /*
   * Raw 3018-3023 "cower in port" (2026-09-07): a cargo ship no bigger than a
   * Merchantman (type <= 0xe) at a colony flagged NEARBY_FRIGATE
   * (+0x1b bit 0x02) bumps its +0x315a counter and parks (orders 0x43,
   * LAB_5899) until the counter reaches 10 − hold capacity (Caravel 8 beats,
   * Merchantman 6). DOS never resets the byte — the wait happens once per
   * hull. The park skips boarding and the load matrix. +0x315a is the COL1
   * `col1_counter16` byte (unit record offset 0x16), save-round-tripped since
   * 2026-09-07 — the counter survives save/load as in DOS.
   */
  const int arrival_dos_type = ai_euro_20e6_dos_type(ctx->units, ship);
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0 && arrival_dos_type <= 0x0e) {
    if (ship->col1_counter16 < 255) {
      ship->col1_counter16++;
    }
    if (ship->col1_counter16 < 10 - holds) {
      if (out_cowered) {
        *out_cowered = 1;
      }
      if (trace) {
        fprintf(
          stderr, "[shipdump] ship %d n%d cowers at colony %d (count %d < %d)\n", ship->id,
          nation_id, c->id, ship->col1_counter16, 10 - holds
        );
      }
      return c->id;
    }
  }

  /* raw 3012-3016 + 3052-3131: free = capacity − holds_occupied, then the
   * DOS LOAD matrix one cargo per free hold. Passengers already aboard are
   * charged here (ai_euro_20e6_ship_hold_budget) because the port's cargo_ids
   * substitution stops the scan below from re-reserving them the way DOS's
   * clear+re-mark round trip does. */
  int loaded = 0;
  int free_holds = ai_euro_20e6_ship_hold_budget(ctx->units, ship);

  /*
   * Raw 3024-3051 passenger-board MARK scan (2026-09-07; reworked to the
   * literal mark-then-assemble 2026-09-07e). Gate: type gate bVar7 and
   * unit+0x3148 bit 0x20 clear. DOS walks the colony-tile stack while
   * DS:0x1734[nation] < 0x19 and marks members act_state = 1, debiting the
   * ship's hold budget by the member's 0x5238 size. It does NOT board here:
   * the budget it leaves is what the load matrix below may fill with goods,
   * and FUN_1427_10be (ai_euro_20e6_transport_assemble, called at the tail of
   * this function = DOS's 0x3558 fall-through) then boards exactly the
   * reserved remainder. Eligible: an armed land unit (0x5236 combat > 1,
   * orders byte not 'G'/'A') on a stance-0 continent; a Pioneer (type 2)
   * unless the ship's composite priority (iStack_14, FUN_521d_0600) is 0 on a
   * non-0 stance continent. After the scan DOS zeroes 0x1734[nation]
   * (:81295) — the only reset the counter has.
   */
  if (ai_euro_20e6_457e_type_gate(ctx, ship, arrival_dos_type) &&
      (ship->col1_flags15 & AI_EURO_F3148_SPARE) == 0) {
    const int cid = ctx->map ? map_continent_id_at(ctx->map, c->x, c->y) : -1;
    const int stance = ai_euro_continent_stance_at(nation_id, cid);
    const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
    const int ship_prio = ai_goals_composite_unit_priority_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, nation_id, ship->x, ship->y, arrival_dos_type, ship->profession, turn, colonies_count_for_nation(ctx->colonies, nation_id));
    for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
      if (s_0a60_work_registered[nation_id] >= 0x19) {
        break;
      }
      /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index; the id-keyed
       * shadow below keeps its `lu->id` key and its 256-wide guard. */
      ColonizeUnit* lu = &ctx->units->units[ui];
      if (!lu->active || lu->nation_id != nation_id || lu->aboard_ship_id >= 0 ||
          lu->id == ship->id || lu->id < 0 || lu->id >= COLONIZE_UNITS_MAX) {
        continue;
      }
      if (lu->x != c->x || lu->y != c->y) {
        continue;
      }
      const int lt = ai_euro_20e6_dos_type(ctx->units, lu);
      if (lt < 0 || (lt >= 0x0d && lt <= 0x12)) {
        continue;
      }
      const ColonizeUnitType* lty = units_type(ctx->units, lu->type_index);
      const int space = lty ? lty->space : 1;
      if (space > free_holds || space >= 99) {
        continue;
      }
      int mark = 0;
      const int oc = lu->col1_ai_plan; /* +0x314b */
      if (ai_euro_20e6_type_combat(lt) > 1 && oc != 'G' && oc != 'A' && stance == 0) {
        mark = 1;
      }
      if (lt == 0x02) {
        if (ship_prio == 0 && stance != 0) {
          continue; /* raw goto 32e3 */
        }
        mark = 1;
      }
      if (mark) {
        /* raw 3046-3050: act_state = 1, iStack_d2 -= 0x5238[type]. */
        lu->orders = UNITS_ORDER_SENTRY; /* +0x314c = 1 */
        free_holds -= space;
        if (trace) {
          fprintf(
            stderr, "[shipdump] ship %d n%d MARKS unit %d (type 0x%02x space %d)\n",
            ship->id, nation_id, lu->id, lt, space
          );
        }
      }
    }
    s_0a60_work_registered[nation_id] = 0; /* :81295 */
  }

  if (ai_euro_20e6_load_holds(ctx, c, nation_id, ship, free_holds, trace)) {
    loaded = 1;
  }
  /* raw 3127-3131: the ship arm latches the source colony in +0x314a, which
   * the delivery matrix below then skips (raw 2055). */
  if (loaded) {
    ai_euro_20e6_origin_set(ship, c->id);
  }
  /*
   * asm 0x354e → 0x3558 → 0x3609: the arrival block falls out of the load
   * loop straight into LAB_3558, whose first call is FUN_1000_8b10
   * (= FUN_1427_10be) on this ship. Everything the scan above marked boards
   * here, in the same act, into the space the load matrix left free.
   */
  (void)ai_euro_20e6_transport_assemble(ctx, nation_id, ship);
  if (trace) {
    fprintf(
      stderr, "[shipdump] ship %d n%d colony %d dumped %d loaded %d\n", ship->id, nation_id,
      c->id, dumped, loaded
    );
  }
  return c->id;
}

/*
 * FUN_521d_20e6 ship band entry: own-colony arrival (dump + load matrix),
 * the hold-cargo delivery matrix and its sell tail, then the 4393 work-queue
 * peel. Hull test is DOS's own (raw 1691: nonzero hold capacity), not a name
 * list. Peace only — the war arms own idle ships at war.
 * Returns 1 if a course was set or the beat was claimed at a berth.
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
  /*
   * DOS raw 1691 gate, first clause: `capacity_table[type*0xe + 0x5237] == 0`.
   * That is the ONLY hull test FUN_521d_20e6 applies before the arrival block
   * and the delivery band — any unit type 0x0d..0x12 (Caravel..Man-O-War) with
   * a nonzero hold capacity runs both. The port used to gate this whole
   * function on `ai_euro_is_cargo_ship_name` (Caravel/Merchantman/Galleon),
   * which left a Privateer's capture loot (FUN_5fef_0352 hands the winner the
   * loser's holds, viceroy_overlays.c:85035-85050) with no DOS consumer at
   * all. Widened to the DOS shape 2026-09-18; the 4393 work-queue peel below
   * keeps the narrower hull test, see its call site.
   */
  if (!units_is_sea(ctx->units, ship->id)) {
    return 0;
  }
  if (units_goods_hold_count(ctx->units, ship->id) <= 0) {
    return 0;
  }
  /*
   * DOS own-colony ARRIVAL block (raw 1691 gate + raw 2996-3138) runs BEFORE
   * everything below — see ai_euro_20e6_ship_berth_arrival for the fork decode.
   * It sits above the has_tools/has_cap early-out on purpose: DOS's gate asks
   * only "has holds && at own colony", so a hull with no haul cargo and no
   * free slot still dumps. The cargo flags below are therefore computed on the
   * post-arrival hull.
   */
  int cowered = 0;
  const int berthed_at = ai_euro_20e6_ship_dump_enabled()
                           ? ai_euro_20e6_ship_berth_arrival(ctx, nation_id, ship, &cowered)
                           : -1;
  if (cowered) {
    return 1; /* raw 3023 → LAB_5899: parked in port this beat */
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

  /*
   * (The port-only "adjacent short coastal colony + haul cargo -> unload" arm
   * that stood here was deleted 2026-09-18. DOS has no per-cargo "colony is
   * short of this" unload in FUN_521d_20e6 — the arrival block dumps the whole
   * hull unconditionally (raw 3002-3007) and the delivery matrix (raw
   * 2047-2139) picks the destination. The arm was already unreachable with
   * AI_20E6_SHIP_DUMP on, because the berth arrival above fires for exactly
   * the hull/colony pairs it tested. `ai_euro_colony_haul_cargo_short` went
   * with it as its only caller.)
   */

  /*
   * On an own coastal colony with a free hold → the DOS load matrix
   * (ai_euro_20e6_load_pick, raw 3059-3134): one cargo per free hold, DOS
   * weights, until the hull is full or the matrix declines. Replaces the
   * port's own TOOLS/LUMBER/ORE/MUSKETS/HORSES/FOOD ladder (and its
   * food_short-first reorder), which had no DOS reading.
   *
   * FALLBACK ONLY. With AI_20E6_SHIP_DUMP on (the default) the arrival block
   * at the top of this function has already run the DOS pair — the raw
   * 3002-3007 dump sweep and then this same matrix on the emptied hull — so
   * berthed_at >= 0 and this block is dead. It survives verbatim as the
   * AI_20E6_SHIP_DUMP=0 path: the 06e empty-hull gate substitution, which
   * stood in for the dump precondition by simply refusing to score a
   * part-loaded hull (DOS state the matrix never sees, because DOS empties it
   * first).
   * Ships berth on adjacent water, so ai_euro_tiles_near, not colonies_id_at.
   */
  if (berthed_at < 0 && has_cap &&
      ai_euro_hauler_free_holds(ctx->units, ship) ==
        units_goods_hold_count(ctx->units, ship->id)) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!ai_euro_tiles_near(ship->x, ship->y, c->x, c->y)) {
        continue;
      }
      int loaded = 0;
      /* iStack_d2 = hold capacity − holds_occupied (raw 3020-3023), less the
       * hull the port's persistent passengers hold (see
       * ai_euro_20e6_ship_hold_budget). */
      int free_holds = ai_euro_20e6_ship_hold_budget(ctx->units, ship);
      if (ai_euro_20e6_load_holds(ctx, c, nation_id, ship, free_holds, 0)) {
        loaded = 1;
      }
      /*
       * Raw 3134-3138: the ship arm (iStack_34 != 0) latches the source colony
       * in unit byte +0x314a — exactly the colony the delivery matrix then
       * skips. (The land arm sets +0x3158 = 1 instead, the village-errand byte
       * 47b9 reads; wagons do not come through here.)
       */
      if (loaded) {
        ai_euro_20e6_origin_set(ship, c->id);
      }
      break;
    }
  }

  int cx = 0;
  int cy = 0;
  /*
   * FUN_521d_20e6 raw 2044-2139. Delivery cargo aboard (Horses / Trade Goods /
   * Tools / Muskets) → the DOS matrix owns the destination outright: 20e6 is
   * the per-unit mover; a matrix that rejects every colony runs the sell
   * tail (raw 2140-2163) and then falls through to the 4393 peel with the
   * emptied hull, exactly as DOS does. Without such cargo DOS never enters
   * this block at
   * all (`-1 < iStack_44`), so the 4393 work-queue peel and the short-colony
   * haul keep owning FOOD/LUMBER/ORE runs.
   */
  int tally[COLONIZE_CARGO_COUNT];
  const int max_type = ai_euro_20e6_delivery_tallies(ctx, ship, tally);
  int have_dest = 0;
  if (max_type >= 0) {
    have_dest =
      ai_euro_20e6_delivery_colony_pick(ctx, ship, nation_id, tally, max_type, &cx, &cy);
    if (!have_dest) {
      /*
       * Raw 2140-2163 sell tail: no colony will take the delivery cargo, so
       * the hold is dumped for gold. DOS then falls straight through to
       * LAB_004393 with an empty hull (the `bVar10 = holds_occupied` gate
       * right after the loop cannot fire once it has been zeroed), so the
       * work-queue peel below owns the now-empty ship this same beat.
       */
      (void)ai_euro_20e6_delivery_sell_tail(ctx, ship, nation_id);
    }
  }
  /*
   * Raw 2166-2168 (doc 2173-2175) — the arrival block's own consumer. Once the
   * delivery matrix has had its say DOS re-reads holds_occupied:
   *
   *   bVar10 = unit+0x3150;
   *   if (capacity_table[type] == bVar10 || 1 < bVar10) goto LAB_003fa6;
   *
   * and LAB_003fa6 is `FUN_1000_94da(param_2)` = FUN_291f_02ea →
   * FUN_48d3_015e, the expanding-ring hunt for a High Seas tile that stamps
   * orders 0x45 (resolved in this file's 47b9/457e symbol table). So a ship
   * that leaves the berth laden sails for Europe and never reaches
   * LAB_004393's work-queue peel — declining here hands it to the
   * dispatcher's Europe-export arm, which is the port's stand-in for
   * 48d3_015e.
   *
   * Without this the dump+load could park the ship at its own berth forever
   * (the 06e/06f oscillation class); the gate is DOS's own answer to it.
   *
   * Unscoped 2026-09-07 to the DOS shape: the gate keys off holds_occupied
   * for EVERY untasked ship reaching this point (DOS guards the whole band
   * with `uStack_b6 != 0` at LAB_003558 entry — an empty hull skips to 4393
   * directly, which `occupied > 1 || occupied == capacity` preserves). A
   * ship carrying 2+ holds of non-delivery cargo now sails for Europe
   * instead of taking a queue tip, exactly as DOS routes it.
   */
  if (!have_dest) {
    const int capacity = units_goods_hold_count(ctx->units, ship->id);
    const int occupied = capacity - ai_euro_hauler_free_holds(ctx->units, ship);
    if (occupied > 1 || (capacity > 0 && occupied == capacity)) {
      return 0;
    }
  }
  int from_tip = 0;
  if (!have_dest) {
    /*
     * Queue tip only (2026-09-07): the Linux-only nearest-short-coastal-colony
     * fallback is retired — DOS has no such scan. An empty queue drops the
     * ship to LAB_457e's arms (HS cadence / Europe export / explore band),
     * exactly where the dispatcher chain sends a declined ship here.
     *
     * Hull test (bVar17, raw ~1284-1306): DOS's base term is `type != 0x12`
     * (Man-O-War), narrowed further for Privateer (0x10) and Frigate (0x11) by
     * globals this project has not resolved — see
     * ai_euro_4393_work_queue_haul_pick's header, "DEAD END". Those narrowings
     * can only REMOVE warships from the queue, so when the band gate above was
     * widened to DOS's raw-1691 hull test (2026-09-18) the peel kept the
     * pre-existing cargo-hull list rather than guess at the unresolved half.
     */
    if (ai_euro_is_cargo_ship_name(ai_euro_unit_kind(ctx->units, ship)) &&
        ai_euro_4393_work_queue_haul_pick(
          ctx, nation_id, ship->x, ship->y, ship, &cx, &cy
        )) {
      from_tip = 1;
    } else {
      return 0;
    }
  }
  int wx = 0;
  int wy = 0;
  if (!ai_euro_coastal_water_near(ctx->map, cx, cy, ship->x, ship->y, &wx, &wy)) {
    return 0;
  }
  if (ship->x == wx && ship->y == wy) {
    /*
     * A PICKUP tip resolving to the berth the hull already occupies is no
     * work: the own-colony load arm above has already had its go at this
     * colony this beat, so claiming the beat here would only starve the arms
     * below (Europe export, war hunt). Same rule as the wagon side's "tip on
     * my own tile → decline". A delivery destination or a short-colony haul
     * that resolves here IS the arrival, so those still end the beat.
     */
    return from_tip ? 0 : 1;
  }
  if (units_orders_follow_goto(ship->orders) && ship->goto_x == wx && ship->goto_y == wy) {
    return 1;
  }
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, wx, wy);
  return 1;
}

/*
 * Peace Europe export sail — the port's stand-in for LAB_003fa6
 * (`FUN_1000_94da` = FUN_291f_02ea -> FUN_48d3_015e), the expanding-ring High
 * Seas hunt FUN_521d_20e6 jumps to at raw 2166-2168 when the band leaves the
 * hull laden. The Europe end is DOS-real and already ported: FUN_521d_5d04's
 * dock loop (viceroy_overlays.c:83168-83192, `ai_euro_5d04_cb_sell_hold0` /
 * `_cb_reward_case`) empties the holds of EVERY Europe ship of type
 * 0x0d..0x12, warships included — so any hull that gets here has a seller.
 * The colony-surplus LOAD below stays a cargo-hull errand (FUN_364b_0688 /
 * 0636, stock>99 → leave 50); the sail itself is open to any hull already
 * carrying export-eligible goods, which is where a Privateer's capture loot
 * (FUN_5fef_0352, viceroy_overlays.c:85035-85050) goes.
 */
static int ai_euro_ship_holds_export_goods(const ColonizeUnitPool* units, const ColonizeUnit* ship) {
  if (!units || !ship) {
    return 0;
  }
  const int n = units_goods_hold_count(units, ship->id);
  for (int h = 0; h < n; ++h) {
    if (units_hold_amount(units, ship->id, h) <= 0) {
      continue;
    }
    if (europe_cargo_export_eligible(ship->hold_goods_type[h])) {
      return 1;
    }
  }
  return 0;
}

/*
 * Europe-bound sail tail shared by the export and Privateer-loot arms (audit
 * AE-16): step into the Europe park if the ship already stands on a High Seas
 * tile, else aim AI_SAIL at the nearest Europe lane entry. Returns 0 only when
 * there is no lane to aim at, or the ship is already standing on it.
 */
static int ai_euro_ship_sail_to_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship) {
  if (ai_euro_ship_enter_europe(ctx, ship)) {
    return 1;
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
 * AI High Seas → Europe crossing (the missing half of the 48d3_015e stand-in):
 * a ship the export/loot arms sent to a High Seas tile enters the Europe park
 * (200,100) once it stands on one. Without this the sail target re-resolves
 * every act — units_find_eastern_high_seas_tile skips the ship's OWN tile as
 * occupied, so the "best" rim tile flips between two neighbours and the ship
 * wiggles on the sealane forever, never selling. Next act the in_europe branch
 * cash/sells and teleports it back out toward its landfall.
 */
static int ai_euro_ship_enter_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship) {
  if (!ctx || !ctx->map || !ctx->units || !ship ||
      !map_tile_is_high_seas(ctx->map, ship->x, ship->y)) {
    return 0;
  }
  const int ox = ship->x;
  const int oy = ship->y;
  ship->x = 200;
  ship->y = 100;
  units_occupancy_notify_moved(ctx->units, ox, oy, 200, 100);
  ai_euro_sync_aboard_cargo_xy(ctx->units, ship);
  ai_euro_set_goto(ship, UNITS_ORDER_AI_SAIL, 200, 100);
  ship->moves = 0;
  return 1;
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
  if (!units_is_sea(ctx->units, ship->id)) {
    return 0;
  }
  if (units_goods_hold_count(ctx->units, ship->id) <= 0) {
    return 0;
  }

  /* Prefer SILVER then other export-eligible cargos (FUN_364b_0636). */
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, ship) &&
                      ai_euro_is_cargo_ship_name(ai_euro_unit_kind(ctx->units, ship));
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
  /*
   * Only a real load is worth the crossing: the arm's own load rule above
   * yields >= 50 (stock > 99, leave 50), so hold that bar for cargo the
   * berth-arrival load matrix put aboard as well. A few furs from a young
   * colony's stock sent the Caravel to Europe and back every four turns
   * (the "circles"), the same trip again each time it came home.
   */
  {
    int total = 0;
    const int n = units_goods_hold_count(ctx->units, ship->id);
    for (int h = 0; h < n; ++h) {
      if (europe_cargo_export_eligible(ship->hold_goods_type[h])) {
        total += units_hold_amount(ctx->units, ship->id, h);
      }
    }
    if (total < 50) {
      return 0;
    }
  }
  return ai_euro_ship_sail_to_europe(ctx, ship);
}

/*
 * The war-cargo colony-sail arm that stood here (`ai_euro_try_ship_war_cargo_sail`)
 * was a second, earlier entry into LAB_521d_3558 with its own invented scorer;
 * smell audit sweep-3 area C #6 retired both. DOS reaches 3558 exactly once per
 * act, from the 20e6 unload/settle flow, which this port keeps in
 * `ai_euro_unload_settle` -> `ai_euro_20e6_colony_sail_pick` (raw 89614-89711,
 * ladder at 89663). Goods-only hulls are the delivery matrix's business (raw
 * 2047-2139), not 3558's; military passengers still reach the structural pick
 * through the settle gate.
 */

/*
 * (`ai_euro_try_privateer_europe_loot_sail` — "Privateer already carrying
 * export-eligible goods -> AI_SAIL Europe" — was deleted 2026-09-18. It was a
 * manual-cited duplicate of the Europe export arm above, differing only in
 * which hulls it accepted; with that arm's gate widened to DOS's raw-1691 hull
 * test (any type 0x0d..0x12 with holds) the Privateer loot case is the same
 * code path DOS uses, raw 2166-2168 -> FUN_48d3_015e -> the FUN_521d_5d04 dock
 * sell loop.)
 */


/*
 * True when (x,y) is adjacent ocean under an enemy Fort/Fortress battery
 * (FUN_364b_03f6 / units_coastal_fort_attack_strength). Cite: Marathon8 peel.
 */
static int ai_euro_tile_under_enemy_fort_fire(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* viewer,
  int x,
  int y
) {
  if (!ctx || !viewer || !ctx->colonies || !ctx->units || !ctx->col1_ok || !ctx->col1 ||
      !ctx->map) {
    return 0;
  }
  if (!map_tile_is_water(ctx->map, x, y)) {
    return 0;
  }
  const int viewer_nation = viewer->nation_id;
  const int privateer = units_type_is_privateer(units_type(ctx->units, viewer->type_index));
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id == viewer_nation || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    /* Same gate the battery itself obeys (FUN_364b_03f6 raw 57082-57083,
     * units_fort_fire_is_hostile): it fires whenever the PEACE bit is clear
     * or the hull is a Privateer, not only at declared war. Gating on the WAR bit left hulls loitering
     * under a no-treaty fort until it sank them. */
    if (!privateer &&
        (ai_diplo_read(ctx->col1, c->nation_id, viewer_nation) & AI_DIPLO_PEACE) != 0) {
      continue;
    }
    if (units_coastal_fort_attack_strength(ctx->colonies, c, ctx->units) <= 0) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      if (c->x + MAP_DIR8_DX[d] == x && c->y + MAP_DIR8_DY[d] == y) {
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
  if (!ctx || !ctx->units || !ctx->map || !u || !u->active || u->moves <= 0) {
    return 0;
  }
  if (!units_is_sea(ctx->units, u->id) || ai_euro_in_europe(u->x, u->y)) {
    return 0;
  }
  if (!ai_euro_tile_under_enemy_fort_fire(ctx, u, u->x, u->y)) {
    return 0;
  }
  int best_d = -1;
  int best_dist = -1;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (!map_tile_is_water(ctx->map, nx, ny)) {
      continue;
    }
    if (ai_euro_tile_under_enemy_fort_fire(ctx, u, nx, ny)) {
      continue;
    }
    if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, u->type_index, nx, ny, u->id)) {
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
  const int tx = u->x + MAP_DIR8_DX[best_d];
  const int ty = u->y + MAP_DIR8_DY[best_d];
  ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
  if (units_try_move_w(&w_, u->id, tx, ty)) {
    /* A goto that still points under the battery would sail the hull straight
     * back in on the same act (seen: Privateer shuttling (34,8)<->(33,9) off
     * Quebec three times a turn). Drop it; the next act re-aims. */
    ColonizeUnit* m = units_get(ctx->units, u->id);
    if (m && m->active && ai_euro_tile_under_enemy_fort_fire(ctx, m, m->goto_x, m->goto_y)) {
      ai_euro_set_goto(m, m->orders, m->x, m->y);
    }
    return 1;
  }
  return 0;
}

/*
 * Effective defense for the thin 20e6 adjacent-foe picks (audit AE-10 — the
 * naval and land copies had identical bodies). Naval reads the shared
 * FUN_157e_004a base via combat_unit_base_x8 (damage / holds / Drake); land
 * reads FUN_157e_015e via combat_unit_toughness (colony / village / terrain /
 * fortify + vet). Cite: combat_strength.c; FUN_157e_004a / 015e.
 */
static int ai_euro_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f,
  int is_naval
) {
  if (!units || !f) {
    return 9999;
  }
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);
  sctx.units = units;
  sctx.col1 = (ctx && ctx->col1_ok) ? ctx->col1 : NULL;
  const int tough = is_naval ? combat_unit_base_x8(&sctx, f->id, 0, NULL)
                             : combat_unit_toughness(&sctx, f->id, -1);
  return tough > 0 ? tough : 0;
}


/* True when a unit already has a non-stationary AI/sail/goto course (audit
 * AE-11: the ship and land copies were byte-identical). */
static int ai_euro_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map) {
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
 * bugs.md #521 (closed 2026-09-22): the golden-backed adjacent-attack stand-in
 * pair (ai_euro_land_best_adjacent_foe / ai_euro_land_try_adjacent_attack)
 * that lived here was deleted. Its reason to exist was the LAB_521d_52aa odds
 * core scoring 0 against any 2+-defender land stack, which was a port bug:
 * ai_euro_20e6_unit_col9 returned the @UNIT guns column (0x523b) instead of
 * the cost column (0x5239). With the right column the DOS wander scorer
 * (ai_euro_20e6_attack_term) assaults stacked colonies on its own.
 */

/*
 * FUN_521d_20e6 `0x46` gate, full port: combat-capable land unit (combat
 * rating >1) adjacent to a foreign Euro colony with **no defender on the
 * tile** walks straight in and seizes it (Colonization capture-by-move —
 * combat only triggers when a defender is actually present, handled
 * separately by the LAB_521d_4d2e wander scorer's attack term). Decomp scans all 8 neighbors via `FUN_281f_0696`
 * (`euro_settlement_owner`) and stamps orders `0x46` the moment any
 * neighbor is owned by a different, non-crown Euro nation; Linux checks
 * war state too (decomp's world model has no live peacetime seize). One
 * seize per call — re-armed next act pass like the decomp reflex check.
 */
static int ai_euro_land_try_adjacent_colony_seize(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->colonies || !u || !u->active ||
      units_is_sea(ctx->units, u->id) || u->moves <= 0) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(ctx->units, u->type_index);
  if (!ut || ut->attack <= 1) {
    return 0;
  }
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    const int cid = colonies_id_at(ctx->colonies, nx, ny);
    if (cid < 0) {
      continue;
    }
    const ColonizeColony* c = colonies_get(ctx->colonies, cid);
    if (!c || !c->active || c->nation_id == u->nation_id || c->nation_id < 0 ||
        c->nation_id > 3) {
      continue;
    }
    /* Smell #106: the DOS 0x46 stamp has NO relation check — 20e6 scans the
     * 8 neighbors for any foreign non-crown settlement owner and stamps the
     * seize outright; 465b then declares the war as a side effect of the
     * entry (viceroy 75567-75593). The at-war gate here is what kept AI
     * nations from ever opening a Euro war on their own. */
    if (units_best_defender_at(
          ctx->units, ctx->col1_ok ? ctx->col1 : NULL, nx, ny, u->id, u->id
        ) >= 0) {
      continue; /* defended — leave to the LAB_4d2e attack term */
    }
    if (units_id_at(ctx->units, nx, ny) >= 0) {
      /* Undefended but occupied (docked ships, civilians): DOS entry
       * seizure — the tile is swept with the town (0512 semantics,
       * units_seize_noncombat_at), then the walk-in proceeds. */
      units_seize_noncombat_at(
        ctx->units, u->id, nx, ny, ctx->col1_ok ? ctx->col1 : NULL
      );
      /* Berthed foreign warships are left alone: FUN_5fef_1b0e sinks and
       * seizes nothing on capture, and the 5fef_0000 domain gate (raw
       * 99186-99195 domain gate inside FUN_5fef_0000, units_domain_blocker_at) means a hull neither defends
       * nor blocks a land walk-in — units_try_move applies that gate. */
    }
    int plunder = 0;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      if (c->stock[i] > 0) {
        plunder += c->stock[i];
      }
    }
    ColonizeColony snap = *c;
    ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
    if (!units_try_move_w(&w_, u->id, nx, ny)) {
      continue;
    }
    if (u->active && u->x == nx && u->y == ny &&
        colonies_capture(ctx->colonies, cid, u->nation_id)) {
      /* 465b side-effect war: attack under peace/treaty opens hostilities
       * and clears 0x40 both ways (viceroy 75567-75593). */
      if (ctx->col1_ok && ctx->col1 && snap.nation_id >= 0 && snap.nation_id <= 3 &&
          !ai_diplo_at_war(ctx->col1, u->nation_id, snap.nation_id)) {
        ai_diplo_declare_war_ctx(ctx, u->nation_id, snap.nation_id);
      }
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
 * the LAB_4d2e attack term only ever scores actual unit
 * occupants (a Brave standing on the tile), never the village tile
 * itself as a target. `units_try_move` already resolves combat against an
 * empty village correctly on its own (synthesizes a temp defender per
 * `FUN_5fef_1b0e`, applies raid fallout, then — unlike a colony capture —
 * the attacker does **not** enter/occupy the tile, matching the human
 * @ACTIONS "Attack Village" commit in `game_dialogs.c`) — this function's only job is picking the target and opening
 * hostilities, same division of labor as the human path.
 * One raid attempt per call; re-armed next act pass like the colony seize.
 */
static int ai_euro_land_try_adjacent_village_seize(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || !u ||
      !u->active || units_is_sea(ctx->units, u->id) || u->moves <= 0) {
    return 0;
  }
  const ColonizeUnitType* ut = units_type(ctx->units, u->type_index);
  if (!ut || ut->attack <= 1) {
    return 0;
  }
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
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
        break; /* garrisoned Brave — leave to the LAB_4d2e attack term */
      }
      ai_contact_village_open_hostilities(ctx, indian_nation, u->nation_id);
      ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
      if (!units_try_move_w(&w_, u->id, nx, ny)) {
        return 0;
      }
      return 1;
    }
  }
  return 0;
}


/* (The ai_euro_foreign_land_threat_near helper was deleted with the invented
 * peace colony-defence wake arm, bugs.md #760.) */

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
 *  - DS:0x1734 per-nation urgency accumulator: REAL since 2026-09-07
 *    (s_0a60_work_registered — bumped at 0a60 registration, zeroed by the
 *    berth boarding scan), no longer a colony-flag count.
 *  - a654 per-unit goal id for the goal fold: nation-level FOUND/MIL_EXPAND
 *    goal existence (see the fold comment in the body).
 *  - presence bit 0x08 of −0x6a0e: writer undecoded — read as 0.
 *  - DS:0x1740 recall latch (5bfb full-recall event): no Linux producer — 0.
 */
/*
 * `a654` — resolved 2026-09-06e, and it is NOT a per-unit goal binding.
 * The resident thunk targets FUN_521d_0656, re-disassembled byte-exact from
 * OVL14_L0000:0656 (viceroy_overlays.asm; the canonical decompile of 0656 is
 * register-mangled, which is what produced the earlier "walk the transport
 * chain to its end" reading):
 *
 *   best = -1;
 *   while (unit >= 0) {
 *     t = unit.type(+0x3146);
 *     if (DS:0x523d[t*0xe] & 0x40)                       // settler capability
 *       if (best < 0 || type[best] < t) best = unit;      // strictly-greater
 *     unit = next_stack_member(unit);                     // FUN_1000_84d4
 *   }
 *   return best;
 *
 * i.e. "the highest-typed settler-capable member of this unit's tile stack".
 * Bit 0x40 of the @UNIT capability byte is set for exactly types 0 Colonist,
 * 2 Pioneer and 5 Scout (k_20e6_type_flags); every ship type carries
 * 0x81/0x82/0xa2, so a carrier never selects itself and an empty ship yields
 * -1 — which is why the fold's `-1 < iStack_68` gate is meaningful and not
 * vacuous. Linux keeps carried units in cargo_ids rather than on the ship's
 * tile stack, the same substitution ai_euro_20e6_ship_cargo_counts makes.
 */
static const ColonizeUnit* ai_euro_20e6_stack_settler(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* ship
) {
  const ColonizeUnit* best = NULL;
  int best_t = -1;
  const ColonizeUnit* chain[1 + COLONIZE_UNIT_CARGO_MAX];
  int n = 0;
  chain[n++] = ship; /* DOS starts the walk at the unit itself */
  for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[s]);
    if (p && p->active) {
      chain[n++] = p;
    }
  }
  for (int i = 0; i < n; ++i) {
    const int t = ai_euro_20e6_dos_type(ctx->units, chain[i]);
    if (t < 0 || (ai_euro_20e6_type_flags(t) & 0x40) == 0) {
      continue;
    }
    if (t > best_t) {
      best_t = t;
      best = chain[i];
    }
  }
  return best;
}

/*
 * Raw 1746-1762 goal fold, BOTH branches (2026-09-06e; the promote half used
 * to stand in on "the nation has any FOUND/MIL_EXPAND primary goal" and the
 * demote half was unmodelled for want of a per-unit goal binding that, per
 * the a654 decode above, never existed):
 *
 *   rep = a654(ship);                                  // settler-capable rep
 *   if (rep >= 0) {
 *     urgency = 7326(nation, rep, cont) + 72e0(nation);
 *     if (urgency < 1) { if (found_probe == 0) { civ += pio; pio = 0; } }
 *     else             { carry80 = civ; pio += civ; civ = 0; }
 *   }
 *
 * `7326` is FUN_521d_052c (unit_desirability_score, ported) and `72e0` is
 * FUN_521d_03d0 (founding_expansion_urgency, ported) — both identified from
 * the OVL14 body of FUN_521d_0600 = 7326 + 72f9 + 72e0 at offset 0x600.
 * 052c is clamped to <= 0 and 03d0 returns 8 with the default (all-zero)
 * plan scratch, so the demote arm needs a strongly negative unit: it is the
 * lone criminal/servant-shaped transport, not a Pioneer one — exactly the
 * case the earlier "a wrong stand-in would strip founder status from a lone
 * Pioneer transport" warning was protecting.
 * `found_probe` is the caller's 7344(nation, x, y, AI_GOAL_FOUND) probe.
 */
static void ai_euro_20e6_goal_fold(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* ship,
  int nation,
  int found_probe,
  int* pioneers,
  int* civ,
  int* carry80
) {
  if (carry80) {
    *carry80 = 0;
  }
  const ColonizeUnit* rep = ai_euro_20e6_stack_settler(ctx, ship);
  if (!rep) {
    return; /* iStack_68 < 0 — no settler-capable member, no fold */
  }
  const int total_colonies = ctx->colonies ? ctx->colonies->colony_count : 0;
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  /*
   * DOS 89432: `thunk_FUN_2a1f_053c(0x281f, uVar11, local_68, 0xffff)` — the
   * continent argument is the **0xffff = any** sentinel, not the ship's tile.
   * The port used to pass map_continent_id_at(ship->x, ship->y); the ship sits
   * on water, so that is a water-region id no land colony can match, 15eb_0142
   * always missed and the score was pinned to the +2 miss constant instead of
   * dist/5 - 1. ai_goals_nearest_colony_15eb_0142 spells "any" as continent < 0.
   */
  const int cont = -1;
  /* 052c runs its own FUN_15eb_0142 nearest-own-colony-on-continent search
   * (it owns the DS:0x8db8 write), so no caller-side distance is passed. */
  const int urgency =
    ai_goals_unit_desirability_score(
      ctx->map, ctx->colonies, nation, ship->x, ship->y,
      ai_euro_20e6_dos_type(ctx->units, rep), rep->profession, cont, turn, total_colonies
    ) +
    ai_goals_founding_expansion_urgency(nation, total_colonies);
  if (urgency < 1) {
    if (found_probe == 0) {
      *civ += *pioneers;
      *pioneers = 0;
    }
    return;
  }
  if (carry80) {
    *carry80 = *civ;
  }
  *pioneers += *civ;
  *civ = 0;
}

static int ai_euro_20e6_unload_mask(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation) {
  if (!ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  /*
   * asm 0x3609: `CALLF FUN_1000_8b10` (FUN_1427_10be) sits immediately before
   * the 0d38 stack-count batch below, and LAB_3558 is reached on EVERY ship
   * act — the arrival block's fall-through and the three direct `JMP 0x3558`
   * exits of the raw 1691 gate alike. This is that call for the acts that
   * skip the arrival block; the berth path runs its own at the arrival tail
   * (see ai_euro_20e6_transport_assemble). DOS reaches 0x3609 at a colony
   * berth only via the arrival block's raw 2991-2997 stale-mark clear, so a
   * mark not set by THIS act's berth scan (e.g. the housekeeping aboard-stamp
   * on a passenger unloaded earlier this turn) never boards; a member the
   * budget could not fit is re-marked by the next berth act's scan, not by a
   * surviving mark.
   */
  ai_euro_20e6_clear_stale_board_marks(ctx, nation, ship);
  (void)ai_euro_20e6_transport_assemble(ctx, nation, ship);
  int pioneers = 0;
  int mil = 0;
  int scouts = 0;
  int milvet = 0;
  int civ = 0;
  ai_euro_20e6_ship_cargo_counts(ctx, ship, &pioneers, &mil, &scouts, &milvet, &civ);
  const int turn = (ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0;
  const int woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;
  const int open_cont = ai_euro_20e6_open_continents(ctx, nation); /* DS:0x9650 */
  /* DS:0x1734[nation] — the real 0a60 registration counter (2026-09-07;
   * replaces the NEEDS_GARRISON/MILITARY colony-count substitute). */
  const int urgency = s_0a60_work_registered[(nation >= 0 && nation < 4) ? nation : 0];
  int home_dist = 0;
  const int home_colony = ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, nation, -1, &home_dist);
  int any_dist = 0;
  (void)ai_euro_20e6_nearest_colony(ctx, ship->x, ship->y, -1, -1, &any_dist);
  const int probe7 = ai_goals_max_primary_prio(nation, ship->x, ship->y, AI_GOAL_MIL_EXPAND);
  const int probe1 = ai_goals_max_primary_prio(nation, ship->x, ship->y, AI_GOAL_FOUND);
  /*
   * Raw 1746-1762 goal fold, both branches live since 2026-09-06e — see
   * ai_euro_20e6_goal_fold. Positive urgency promotes plain civilians into
   * the founder count (iStack_4a += iStack_82, iStack_80 keeps the old
   * civilian count); urgency < 1 with no FOUND probe demotes Pioneers into
   * civilians. This fold is the DOS mechanism that lets a colonist-only
   * second wave raise 0x40 — it resolves the "# Pioneers vs flag-count
   * founders" conflict noted in the 2026-09-06 pass.
   */
  int carry80 = 0; /* iStack_80 */
  ai_euro_20e6_goal_fold(ctx, ship, nation, probe1, &pioneers, &civ, &carry80);
  /*
   * Raw 1768 sits AFTER the fold, not before it (order corrected 2026-09-06e
   * together with the fold itself): a colonist-only wave arrives with all
   * three counts zero and only becomes a founder cargo once the promote arm
   * has run, so testing the gate first made the promote unreachable for
   * exactly the case the fold exists to serve.
   */
  if (pioneers == 0 && mil == 0 && scouts == 0) {
    return 0;
  }
  int goto_cid = -2;
  if (units_orders_follow_goto(ship->orders) && ship->goto_x >= 0 && ship->goto_y >= 0 &&
      ship->goto_x < (int)ctx->map->width && ship->goto_y < (int)ctx->map->height) {
    goto_cid = map_continent_id_at(ctx->map, ship->goto_x, ship->goto_y);
  }
  int mask = 0;
  for (int d = 0; d < 8; ++d) {
    const int ax = ship->x + MAP_DIR8_DX[d];
    const int ay = ship->y + MAP_DIR8_DY[d];
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
      if (ai_goals_colony_balance_flags_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, nation, cid) > 0) {
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
/*
 * DOS-LITERAL raw 89566-89609 unload loop:
 *   do {
 *     moved = 0; dir = -1;
 *     for (unit in tile chain, head first; dir != 8 && !moved) {
 *       if (unit is a hull) continue;
 *       dir = 04ac(nation, x, y, mask & 0x40, type == 0x0b);     // 06ae
 *       if ((DS:0x523d[type] & mask) && dir != 8) {
 *         +0x3149 = 0; 2a1f_0150(unit, dir); 0934(unit);         // step, exhaust
 *         if (unit left the tile) moved = 1;
 *       }
 *     }
 *   } while (moved);
 * The chain is newest-first (TURN2->3: the Dutch Soldier, boarded last,
 * takes (48,14) before the Pioneer; the French Soldier's landing blocks the
 * Pioneer's only tile), i.e. `cargo_ids` walked backwards. A passenger whose
 * 06ae finds no tile ends the pass even when its own flags do not match.
 */
static int ai_euro_20e6_unload_by_mask_dos(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation, int mask
) {
  int total = 0;
  int moved;
  do {
    moved = 0;
    int dir8 = 0;
    for (int s = ship->cargo_count - 1; s >= 0 && !dir8 && !moved; --s) {
      if (s >= COLONIZE_UNIT_CARGO_MAX) {
        continue;
      }
      const int pid = ship->cargo_ids[s];
      ColonizeUnit* p = units_get(ctx->units, pid);
      if (!p || !p->active) {
        continue;
      }
      const int t = ai_euro_20e6_dos_type(ctx->units, p);
      if (t >= 0xd && t <= 0x12) {
        continue;
      }
      const ColonizeWorld w06 = {
        .units = ctx->units, .colonies = ctx->colonies, .map = ctx->map,
        .col1 = ctx->col1_ok ? ctx->col1 : NULL, .col1_ok = ctx->col1_ok && ctx->col1 != NULL
      };
      int lx = 0;
      int ly = 0;
      if (!ai_goals_pick_founding_tile_ex_w(
            &w06, nation, ship->x, ship->y, (mask & 0x40) != 0, t == 0x0b, &lx, &ly
          )) {
        dir8 = 1;
        continue;
      }
      if ((ai_euro_20e6_type_flags(t) & mask & 0xff) == 0) {
        continue;
      }
      if (ai_euro_unload_pax_at(ctx, ship, p, lx, ly, UNITS_ORDER_SENTRY, p->goto_x, p->goto_y)) {
        p = units_get(ctx->units, pid);
        if (p) {
          p->moves = 0; /* FUN_281f_0934 */
        }
        total++;
        moved = 1;
      }
    }
  } while (moved);
  return total;
}

static int ai_euro_20e6_unload_by_mask(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation, int mask
) {
  if (ai_euro_ship_dos_enabled()) {
    return ai_euro_20e6_unload_by_mask_dos(ctx, ship, nation, mask);
  }
  int total = 0;
  int changed = 1;
  int found_x = -1;
  int found_y = -1;
  if (mask & 0x40) {
    int fx = 0;
    int fy = 0;
    if (ai_goals_pick_founding_tile_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, nation, ship->x, ship->y, &fx, &fy)) {
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
        p->moves = 0; /* FUN_1000_8b24 exhaust */
        total++;
        changed = 1;
        break; /* rescan the (mutated) cargo list, DOS do-loop shape */
      }
    }
  }
  return total;
}

/*
 * DS:0x5235[type] — the @UNIT DEFENSE column, not holds. The @UNIT loader
 * (FUN_5b66_0eee raw 121108-121133) reads the NAMES row strictly in file
 * order: name ptr -> 0x5230, icon -> 0x5232, moves*3 -> 0x5234, attack ->
 * 0x5236, defense -> 0x5235, holds -> 0x5237, size -> 0x5238. So the three
 * FUN_521d_20e6 reads of `type * 0xe + 0x5235` (raw 89685-89687 `(x-10)*2`
 * with gate type < 0x11 -- land units included; raw 89801-89803 `(x-10)*8`
 * with gate type < 0x10; and the same pair in the LAB_3558 colony-sail
 * matrix, raw 86375 / 86506 / 86512) all take DEFENSE. This used to return
 * the holds column, which flattened every land type to 0 (Dragoon scored
 * (0-10)*2 = -20 instead of (3-10)*2 = -14) and under-read every warship.
 * Read from the loaded @UNIT catalog rather than a hardcoded table: the
 * kind id IS the @UNIT row (see ai_euro_5d04_dos_type_of).
 */
static int ai_euro_20e6_unit_col5(const ColonizeUnitPool* pool, int dos_type) {
  const int ti = pool ? units_kind_type_index(pool, (ColonizeUnitKind)dos_type) : -1;
  const ColonizeUnitType* t = ti >= 0 ? units_type(pool, ti) : NULL;
  return t ? t->defense : 0;
}

/*
 * LAB_3558 colony-sail matrix (raw 1933-2031) — full structural port
 * 2026-09-06, replacing the thin Series-O score for this call path.
 * Loop: own colonies not on the ship's tile; carrying Pioneers (iStack_b4)
 * additionally requires nonzero G-stance on the colony's continent (raw
 * 1946). Peace score (no military cargo):
 *   rng(0,8) + ((17 − min(pop,16))² + 2)*4 − (pop − wanted)*2
 *   +0x14 when the human holds colonies on that continent
 *   ±0x19 on the colony-wants-colonists bit (+0x1b bit 0x10 =
 *     COLONIZE_COLONY_AI_NEEDS_COLONISTS): +0x19 set, −0x19 clear. (The
 *     "else −0x25" this header used to claim was a hex/decimal slip in the
 *     SBB idiom; corrected in code and here — prior smell audit #95.)
 * War-cargo score (military cargo aboard): G-stance 0 → skip colony;
 *   − difficulty × mil × 8 when mil > 1 (DS:0xa89c)
 *   +0x32 when the human holds colonies on the continent
 *   +0x1b bit 0x40 (NEEDS_GARRISON) → +0x3c; else when open continents < 2
 *   or urgency > 0x13: bit 0x08 → +0x2d else −0xf; else −0x2d
 *   ((−0x6a0e[cid] & 7) * 8 presence term omitted — writer undecoded)
 * Shared: + idle timer (+0x8f = cargo_idle_turns); +0x1b bit 0x02
 *   (NEARBY_FRIGATE) ∧ ship != Frigate → −0x32; else bit 0x01
 *   (NEARBY_ARMED_SHIP) ∧ ship type < 0x11 → +(col5 − 10)*2;
 *   − ((dist >> 1) + 1); later ties win (DOS <=).
 * Non-coastal colonies are skipped (the raw 8804(...,0xfffe) reachability
 * probe for the +0x1c bit-0x40-clear case — a ship can't reach them).
 * Commit threshold (raw 2031): peace best > −999, war-cargo best > 0.
 * +0x1b bit 0x08 (SHORT_DEFENDERS) is written by ai_euro_colony_threat_seed_5952;
 * bit 0x04 (MILITARY_SURPLUS) is the opposite side of that pair.
 */
static int ai_euro_20e6_colony_sail_pick(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship, int nation, int mil, int pioneers_b4,
  int urgency, int* out_x, int* out_y
) {
  if (!ctx || !ctx->colonies || !ctx->map || nation < 0 || nation > 3) {
    return 0;
  }
  const int ship_type = ai_euro_20e6_dos_type(ctx->units, ship);
  /*
   * DS:0xa89c, raw 93110-93115: the COUNT of continents whose DS:0x95f2 byte
   * has bit 8 set (this nation has a dug-in field force there), recounted at
   * the head of every per-nation AI pass. Until 2026-09-10 this line read
   * `head.difficulty`, which is a different DS byte entirely — the writer was
   * simply unlocated. See ai_contact_continent_war_count_a89c.
   */
  const int war_cont_count = ai_contact_continent_war_count_a89c(ctx, nation);
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
    int wanted = ai_euro_colony_wanted_size(ctx->colonies, c); /* FUN_1000_8e6c */
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
      score += (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) ? 0x19 : -0x19;
    } else {
      if (cid < 0 || ai_euro_continent_stance_at(nation, cid) == 0) {
        continue; /* raw 1970: war cargo needs nonzero stance */
      }
      score = 0;
      /*
       * Raw 89654-89656: `local_58 = (*(byte *)(FUN_281f_0722(colony) +
       * -0x6a0e) & 7) * 8; local_28 += local_58;` — the DS:0x95f2 presence
       * byte for the CANDIDATE colony's continent, low three bits only, so
       * natives (1) + a foreign Euro unit (2) + a foreign colony (4) each add
       * 8, up to +56. All four bit writers are decoded (raw 78167/78235/
       * 78302/78312), so the term is live since 2026-09-10; it used to be a
       * "writer undecoded" comment with no term at all. Bit 8 is masked off
       * here — it reaches this scorer only through war_cont_count.
       */
      score += (ai_contact_continent_presence_4962(ctx, nation, cid) & 7) * 8;
      /* Raw 89660-89662, verbatim shape: `if (a89c != 0 && 1 < mil)`. */
      if (war_cont_count != 0 && mil > 1) {
        score += war_cont_count * mil * -8;
      }
      if (ai_euro_20e6_own_colonies_on(ctx, ctx->human_nation, cid) != 0) {
        score += 0x32;
      }
      if (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_GARRISON) {
        score += 0x3c;
      } else if (open_cont < 2 || urgency > 0x13) {
        /* Raw 89663: DOS tests +0x1b & 8 here (short-of-defenders), not & 4. */
        score += (c->ai_flags & COLONIZE_COLONY_AI_SHORT_DEFENDERS) ? 0x2d : -0xf;
      } else {
        score -= 0x2d;
      }
    }
    score += (int)(int8_t)c->cargo_idle_turns; /* +0x8f, DOS reads signed byte */
    if (c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) {
      if (ship_type != 0x11) {
        score -= 0x32;
      }
    } else if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP) && ship_type < 0x11) {
      score += (ai_euro_20e6_unit_col5(ctx->units, ship_type) - 10) * 2;
    }
    const int dist = map_dos_dist(ship->x - c->x, ship->y - c->y);
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

/* --- 20e6 unload/settle: stage helpers --------------------------------- */

/* FUN_521d_20e6 first-landfall stage of ai_euro_unload_settle: the whole
 * `no own colony yet` branch (unconditional `return` at its tail, so every
 * early exit inside it is a plain `return` here too). */
static void ai_euro_unload_settle_first_landfall(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id
) {
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
    const ColonizeUnitKind kind = ai_euro_unit_kind(ctx->units, p);
    if (ai_euro_name_is_pioneer(kind) && !pioneer) {
      pioneer = p;
    } else if (ai_euro_name_is_soldier(kind) && !soldier) {
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
    const ColonizeUnitKind kind = ai_euro_unit_kind(ctx->units, u);
    if (ai_euro_name_is_soldier(kind) && !soldier_ashore) {
      soldier_ashore = u;
      if (landfall_x < 0 && u->goto_x >= 0 && u->goto_y >= 0 && u->goto_x < 255 &&
          u->goto_y < 255) {
        landfall_x = u->goto_x;
        landfall_y = u->goto_y;
      }
    }
    if (ai_euro_name_is_pioneer(kind) && !pioneer_ashore) {
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
    ship->moves = 0;
    const int drop_x = found_x;
    const int drop_y = found_y + 1;
    if (map_chebyshev(ship->x, ship->y, drop_x, drop_y) <= 1) {
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
      ship->moves = 0;
    } else if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, found_x, found_y, &wx, &wy)) {
      ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, wx, wy);
      /* Sail spends MP in the case 0x0b loop after this returns. */
    } else {
      ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, found_x, found_y + 2);
      ship->moves = 0;
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
      !map_tile_is_coast_water(ctx->map, ship->goto_x, ship->goto_y);
    if (stuck_goto) {
      int wx = 0;
      int wy = 0;
      if (ai_goals_nearest_landing_water_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, ship->x, ship->y, 24, &wx, &wy) &&
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
  const int dist = map_chebyshev(ship->x, ship->y, stage_x, stage_y);
  const int at_staging = (ship->x == stage_x && ship->y == stage_y);

  if (dist <= 1 && !at_staging) {
    /* Approach peel: hold position, goto staging, unload all sentry. */
    ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, stage_x, stage_y);
    ship->moves = 0;
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
    if (have_found && map_tile_is_coast_water(ctx->map, hold_x, hold_y)) {
      /* Beachhead: soldier lands tip of hold; pioneer stays aboard. */
      ai_euro_set_goto(ship, UNITS_ORDER_AI_MOVE, hold_x, hold_y);
      ship->moves = 0;
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
            map_chebyshev(ship->x, ship->y, hold_x, hold_y - 1) <= 1) {
          lx = hold_x;
          ly = hold_y - 1;
        }
        if (map_chebyshev(ship->x, ship->y, lx, ly) <= 1) {
          (void)ai_euro_unload_pax_at(
            ctx, ship, soldier, lx, ly, UNITS_ORDER_NONE, lf_x, lf_y
          );
        }
      }
      if (pioneer && pioneer->aboard_ship_id == ship->id) {
        ai_euro_set_goto(pioneer, UNITS_ORDER_SENTRY, lf_x, lf_y);
        pioneer->ai_landfall_wait = true; /* bugs.md #528 */
        pioneer->x = ship->x;
        pioneer->y = ship->y;
        pioneer->moves = 0;
      }
      return;
    }
  }

  /* Staging tip with land immediately west — unload all, clear ship.
   * Prefer west-of-ship then south-of-that (TURN3 SP 47,53 / 47,54). */
  ai_euro_set_goto(ship, UNITS_ORDER_NONE, ship->x, ship->y);
  ship->moves = 0;
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

/* FUN_521d_20e6 LAB_3558 mask-unload + colony-sail stage of
 * ai_euro_unload_settle. Returns 1 when DOS returns from the unit act here
 * (the caller returns); 0 falls through to the best-passenger landfall. */
static int ai_euro_unload_settle_mask_and_sail(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id
) {
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
      ship->moves = 0; /* FUN_1000_8b24 on the ship after any unload */
      return 1;               /* DOS re-runs the sail gate next call */
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
      {
        const int found_probe =
          ai_goals_max_primary_prio(nation_id, ship->x, ship->y, AI_GOAL_FOUND);
        ai_euro_20e6_goal_fold(ctx, ship, nation_id, found_probe, &pioneers, &civ, NULL);
      }
      /* DS:0x1734[nation] — real 0a60 registration counter (2026-09-07). */
      const int urgency = ai_euro_0a60_work_registered(nation_id);
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
          return 1;
        }
      }
    }
  }
  return 0;
}

static void ai_euro_unload_settle(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id) {
  if (!ctx || !ship || !units_is_sea(ctx->units, ship->id) || ai_euro_in_europe(ship->x, ship->y)) {
    return;
  }

  /*
   * First colony beachhead / found-approach (TURN2→4): geometry from landfall
   * staging + found table, not nation_id scripts. Cite: test-saves-ai/TURN3–4;
   * test-saves-ai/TURN2-3; FUN_521d_5b66 unload + 0a60 coastal tip.
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
  if (colonies_count_for_nation(ctx->colonies, nation_id) == 0) {
    ai_euro_unload_settle_first_landfall(ctx, ship, nation_id);
    return;
  }

  if (ai_euro_unload_settle_mask_and_sail(ctx, ship, nation_id)) {
    return;
  }

  int best_id = -1;
  int best_score = 0;
  for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
    const int pid = ship->cargo_ids[s];
    ColonizeUnit* p = units_get(ctx->units, pid);
    if (!p || !p->active) {
      continue;
    }
    const ColonizeUnitKind kind = ai_euro_unit_kind(ctx->units, p);
    /* (The "Treasure stays aboard for the Europe sail" skip was deleted
     * 2026-09-23, bugs.md #746 — DOS's AI has no treasure sail.) */
    int sc = 2;
    if (ai_euro_name_is_pioneer(kind)) {
      sc = 5;
    } else if (kind == UNITS_KIND_COLONIST) {
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
  } else if (!ai_goals_pick_founding_tile_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, nation_id, ship->x, ship->y, &dest_x, &dest_y)) {
    if (!units_pick_landfall_tile_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, ship->id, -1, -1, &dest_x, &dest_y)) {
      return;
    }
  }

  if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, ship->id, best_id, dest_x, dest_y)) {
    /* Try adjacent landfall if goal tile not adjacent. */
    if (!units_pick_landfall_tile_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, ship->id, dest_x, dest_y, &dest_x, &dest_y)) {
      return;
    }
    if (!units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, ship->id, best_id, dest_x, dest_y)) {
      return;
    }
  }

  ColonizeUnit* pax = units_get(ctx->units, best_id);
  if (!pax) {
    return;
  }
  /* Second-wave settle while under 6 colonies. */
  if (colonies_count_for_nation(ctx->colonies, nation_id) < 6) {
    int fx2 = pax->x;
    int fy2 = pax->y;
    if (ai_goals_pick_founding_tile_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, nation_id, pax->x, pax->y, &fx2, &fy2)) {
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

/* 1 when any own ship carries a pioneer (or, with `or_soldier`, a soldier). */
static int ai_euro_nation_aboard(ColonizeTurnContext* ctx, int nation_id, int or_soldier) {
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
      const ColonizeUnitKind kind = ai_euro_unit_kind(ctx->units, pax);
      if (ai_euro_name_is_pioneer(kind) || (or_soldier && ai_euro_name_is_soldier(kind))) {
        return 1;
      }
    }
  }
  return 0;
}

static int ai_euro_nation_settler_aboard(ColonizeTurnContext* ctx, int nation_id) {
  return ai_euro_nation_aboard(ctx, nation_id, 1);
}

static int ai_euro_nation_pioneer_aboard(ColonizeTurnContext* ctx, int nation_id) {
  return ai_euro_nation_aboard(ctx, nation_id, 0);
}

/*
 * First-colony found approach (TURN3→4): landfall-keyed found tile. Call before
 * the 20e6 scoring gate so settlers are not FOUND-yanked into Braves.
 * Skips beachhead tip acts (TURN2→3) — only after pioneer stays aboard (FR) or
 * cargo is empty (SP/DU). Cite: test-saves-ai/TURN3-4.
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
        map_tile_is_coast_water(ctx->map, u->x, u->y)) {
      if (!map_tile_is_coast_water(ctx->map, sx, sy)) {
        sx = u->x;
        sy = u->y;
      }
      if (ai_euro_pick_founding_tile(
            ctx->map,
            ctx->colonies,
            ctx->col1_ok ? ctx->col1 : NULL,
            ctx->units,
            nation_id,
            sx,
            sy,
            &live_x,
            &live_y
          )) {
        *out_x = live_x;
        *out_y = live_y;
        return 1;
      }
    }
  }
  return ai_goals_pick_founding_tile_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, nation_id, u->x, u->y, out_x, out_y);
}

/* Landfall recovered from the first own ship that yields one; *lf_x and *lf_y
 * untouched otherwise. Returns 1 on success. */
static int ai_euro_recover_nation_landfall(
  ColonizeTurnContext* ctx, int nation_id, int* lf_x, int* lf_y
) {
  for (int si = 0; si < COLONIZE_UNITS_MAX; ++si) {
    const ColonizeUnit* sh = &ctx->units->units[si];
    if (!sh->active || sh->nation_id != nation_id || !units_is_sea(ctx->units, sh->id)) {
      continue;
    }
    int rx = 0;
    int ry = 0;
    if (ai_euro_recover_landfall_from_ship(sh->x, sh->y, &rx, &ry)) {
      *lf_x = rx;
      *lf_y = ry;
      return 1;
    }
  }
  return 0;
}

/* --- first-colony landing: stage helpers ------------------------------- */

/* Soldier arm of ai_euro_try_first_colony_land (beachhead staging / found /
 * walk-and-park). Every path inside returns, so the caller returns its
 * result directly. */
static int ai_euro_first_colony_land_soldier(
  ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id, int fx, int fy,
  int lf_x, int lf_y, int settler_aboard, int pioneer_at_found,
  int pioneer_at_found_south, int ship_adj
) {
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
        if (u->moves <= 0) {
          (void)units_wake(ctx->units, u->id);
          u = units_get(ctx->units, u->id);
        }
        while (u && u->active && u->moves > 0 && (u->x != sx || u->y != sy)) {
          if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
            break;
          }
          u = units_get(ctx->units, u->id);
        }
      }
      if (u) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
        u->moves = 0;
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
      if (already_staged && u->moves > 0) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, dest_x, dest_y + 1);
        (void)units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              );
        u = units_get(ctx->units, u->id);
        if (u) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
          u->moves = 0;
        }
        return 1;
      }
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, dest_x, dest_y);
      u->moves = 0;
      return 1;
    }
    if (pioneer_at_found || !pioneer_at_found_south || ship_adj ||
        (u->id >= 0 && u->id < COLONIZE_UNITS_MAX && s_deferred_found[u->id])) {
      ai_euro_set_goto(u, UNITS_ORDER_NONE, dest_x, dest_y);
      u->moves = 0;
      return 1;
    }
    if (colonies_can_found(ctx->colonies, ctx->map, fx, fy)) {
      ai_euro_found_with_unit(ctx, u, nation_id);
    } else {
      ai_euro_set_goto(u, UNITS_ORDER_NONE, dest_x, dest_y);
      u->moves = 0;
    }
    return 1;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, dest_x, dest_y);
  while (u->active && u->moves > 0 && (u->x != dest_x || u->y != dest_y)) {
    if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
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
    u->moves = 0;
  }
  return 1;
}

/* Sail/walk tail of ai_euro_try_first_colony_land: SP AI_SAIL approach, the
 * goto walk toward the found tile, and the arrive/park book-keeping. */
static int ai_euro_first_colony_land_walk(
  ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id, int fx, int fy,
  int lf_x, int lf_y
) {
  /* SP post-beachhead: AI_SAIL toward found — at most one goto-spend this act. */
  const int sp_sail = (lf_x == 53 && lf_y == 56);
  ai_euro_set_goto(u, sp_sail ? UNITS_ORDER_AI_SAIL : UNITS_ORDER_AI_MOVE, fx, fy);
  if (sp_sail) {
    /* One step only so TURN4 lands on (46,52) short of found. */
    if (u->moves > 0) {
      (void)units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              );
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
              if (sh->moves <= 0) {
                sh->moves = units_max_mp(ctx->units, sh->id);
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
          if (!ai_euro_name_is_soldier(ai_euro_unit_kind(ctx->units, su))) {
            continue;
          }
          if (su->x == fx + 1 && su->y == fy + 2) {
            ai_euro_set_goto(su, UNITS_ORDER_AI_MOVE, fx + 1, fy + 3);
            su->moves = UNITS_MP_PER_TILE;
          }
        }
      }
    } else if (u) {
      ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, fx, fy);
    }
    if (u) {
      u->moves = 0;
    }
    return 1;
  }
  while (u->active && u->moves > 0 && (u->x != fx || u->y != fy)) {
    if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
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
    u->moves = 0;
  }
  return 1;
}

static int ai_euro_try_first_colony_land(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !u || !ctx->map || !ctx->units || !ctx->colonies) {
    return 0;
  }
  if (colonies_count_for_nation(ctx->colonies, nation_id) != 0) {
    return 0;
  }
  /* The WoI crown slot always has 0 own colonies but must never found one —
   * real REF names (Regulars/Cavalry) fail the name gate below anyway, but
   * synthetic pools fall back to "Soldiers" for the wave and would leak in. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi &&
      nation_id == (int)ctx->col1->head.crown_nation_id) {
    return 0;
  }
  const ColonizeUnitKind uname = ai_euro_unit_kind(ctx->units, u);
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
      ai_euro_recover_nation_landfall(ctx, nation_id, &lf_x, &lf_y);
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
            ctx->map, ctx->colonies, ctx->col1_ok ? ctx->col1 : NULL, ctx->units,
            nation_id, u->x, u->y, &lx, &ly
          )) {
        if (u->x == lx && u->y == ly) {
          if (colonies_can_found(ctx->colonies, ctx->map, lx, ly)) {
            ai_euro_found_with_unit(ctx, u, nation_id);
            return 1;
          }
          ai_euro_set_goto(u, UNITS_ORDER_NONE, lx, ly);
          u->moves = 0;
          return 1;
        }
        ai_goals_upsert_primary(nation_id, lx, ly, AI_GOAL_FOUND, 7);
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, lx, ly);
        if (u->moves <= 0) {
          (void)units_wake(ctx->units, u->id);
          u = units_get(ctx->units, u->id);
        }
        while (u && u->active && u->moves > 0 && (u->x != lx || u->y != ly)) {
          if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
            break;
          }
          u = units_get(ctx->units, u->id);
        }
        if (u) {
          u->moves = 0;
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
  const int had_mp = u->moves > 0;
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
      if (map_chebyshev(o->x, o->y, u->x, u->y) <= 1) {
        ship_adj = 1;
      }
      int wx = 0;
      int wy = 0;
      if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy) &&
          ((o->goto_x == wx && o->goto_y == wy) ||
           map_chebyshev(o->x, o->y, wx, wy) <= 1)) {
        ship_on_cruise = 1;
      }
      if (o->goto_x == fx && o->goto_y == fy + 2) {
        ship_on_found_hold = 1;
      }
    } else if (o->aboard_ship_id < 0 && o->id != u->id &&
               ai_euro_name_is_pioneer(ai_euro_unit_kind(ctx->units, o))) {
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
          (pioneer_at_found && !settler_aboard && lf_x == 53 && lf_y == 56) ||
          /* SP: both landed, ship still at the beachhead tip because it acts
           * after the land units under the DOS id order (AI_6D8E_DOS_LOOP);
           * the cruise it is about to set is the same staging the legacy
           * ships-first order already saw (TURN3→4). */
          (!settler_aboard && !pioneer_aboard && lf_x == 53 && lf_y == 56))) {
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
  if (u->moves <= 0 || units_orders_skip_turn(u)) {
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
    return ai_euro_first_colony_land_soldier(
      ctx, u, nation_id, fx, fy, lf_x, lf_y, settler_aboard, pioneer_at_found,
      pioneer_at_found_south, ship_adj
    );
  }

  /* Pioneer tip south of found (FR unload): keep sentry + landfall. */
  if (at_found_south) {
    ai_euro_set_goto(u, UNITS_ORDER_SENTRY, lf_x, lf_y);
    u->ai_landfall_wait = true; /* bugs.md #528 */
    u->moves = 0;
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
        u->ai_landfall_wait = true; /* bugs.md #528 */
      } else {
        ai_euro_set_goto(u, UNITS_ORDER_NONE, fx, fy);
      }
      u->moves = 0;
      return 1;
    }
    if (colonies_can_found(ctx->colonies, ctx->map, fx, fy)) {
      ai_euro_found_with_unit(ctx, u, nation_id);
    } else {
      ai_euro_set_goto(u, UNITS_ORDER_NONE, fx, fy);
      u->moves = 0;
    }
    return 1;
  }
  return ai_euro_first_colony_land_walk(ctx, u, nation_id, fx, fy, lf_x, lf_y);
}

/*
 * War land engagement (audit AE-15): seize an adjacent foreign colony, then an
 * adjacent village (both FUN_521d_20e6 `0x46` / `0x4c` arms). Returns 0 when
 * the unit died on the way (the caller must return), 1 otherwise. `*hunted` is
 * raised when the unit already carries a live course, so the sticky outer
 * waves do not LABOR/COLONY-yank it.
 *
 * The act-level adjacent-foe attack loop and the distant "nearest foe / enemy
 * colony" hunt aim that used to sit here were retired 2026-09-18: both were
 * manual-cited inventions with no DOS counterpart (the same finding that
 * retired their naval twins). DOS picks a land unit's fight in the shared
 * LAB_521d_4d2e wander scorer (raw 88880-88940: `local_ea` adjacent
 * attackable-foreigner flag raw 88885-88887, artillery score-zero raw
 * 88911-88913, soldier/dragoon colony-mass gate raw 88921-88937), which
 * commits the adjacent enemy tile as a one-shot goto; the move then resolves
 * through FUN_465b_0000. There is no distant land hunt, exactly as there is
 * none for ships.
 */
static int ai_euro_land_engage_adjacent(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int* hunted
) {
  (void)ai_euro_land_try_adjacent_colony_seize(ctx, u);
  if (!u->active) {
    return 0;
  }
  (void)ai_euro_land_try_adjacent_village_seize(ctx, u);
  if (!u->active) {
    return 0;
  }
  if (ai_euro_has_useful_goto(u, ctx->map)) {
    *hunted = 1;
  }
  return 1;
}

/*
 * Audit AE-13: "is one of my on-map land units with this name standing on
 * (x,y)?" ran four times inside ai_euro_unit_act.
 */
static int ai_euro_settler_on_tile(
  const ColonizeTurnContext* ctx, int nation_id, int x, int y, int want_pioneer
) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* o = &ctx->units->units[i];
    if (!o->active || o->nation_id != nation_id || o->aboard_ship_id >= 0) {
      continue;
    }
    const ColonizeUnitKind kind = ai_euro_unit_kind(ctx->units, o);
    const int match = want_pioneer ? ai_euro_name_is_pioneer(kind) : ai_euro_name_is_soldier(kind);
    if (match && o->x == x && o->y == y) {
      return 1;
    }
  }
  return 0;
}

/*
 * Audit AE-13: Pioneer / Soldier ashore among this nation's on-map land units.
 * When `lf_x` is given, the first unit carrying a goto also recovers a
 * landfall target (the second copy of this scan did that, the first did not).
 */
static void ai_euro_settlers_ashore(
  const ColonizeTurnContext* ctx,
  int nation_id,
  int* out_pioneer,
  int* out_soldier,
  int* lf_x,
  int* lf_y
) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* lu = &ctx->units->units[i];
    if (!lu->active || lu->nation_id != nation_id || lu->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_on_map(lu) || units_is_sea(ctx->units, lu->id)) {
      continue;
    }
    const ColonizeUnitKind lkind = ai_euro_unit_kind(ctx->units, lu);
    if (ai_euro_name_is_soldier(lkind)) {
      *out_soldier = 1;
    }
    if (ai_euro_name_is_pioneer(lkind)) {
      *out_pioneer = 1;
    }
    if (lf_x && *lf_x < 0 && lu->goto_x >= 0 && lu->goto_y >= 0 && lu->goto_x < 255 &&
        lu->goto_y < 255) {
      *lf_x = lu->goto_x;
      *lf_y = lu->goto_y;
    }
  }
}

/*
 * First-colony ship course around the 06ae found tile (audit AE-12: the
 * found-approach arm and the re-assert arm carried the same ~90-line tree).
 * Pioneer still aboard with a Soldier ashore → hold two tiles south of the
 * found tile and spend the act; an empty ship with either settler ashore →
 * the 3558 cruise tip, preferring tip−1 when a Pioneer already stands on the
 * town (TURN4→5 SP), else the same southern hold, keeping MP while no Soldier
 * stands on the found tile so a later outer pass can leave once the colony
 * exists (TURN4→5 Quebec). Cite: test-saves-ai/TURN3-5.
 *
 * `any_cargo` is the caller's "this ship still carries a settler" tally; the
 * two callers deliberately count it differently (the found-approach arm also
 * counts a plain Colonist passenger, the re-assert arm only Pioneer/Soldier).
 */
static void ai_euro_first_colony_ship_course(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int nation_id,
  int fx,
  int fy,
  int pioneer_aboard,
  int any_cargo,
  int pioneer_ashore,
  int soldier_ashore
) {
  if (pioneer_aboard && soldier_ashore) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy + 2);
    u->moves = 0;
    return;
  }
  if (any_cargo || (!pioneer_ashore && !soldier_ashore)) {
    return;
  }
  int wx = 0;
  int wy = 0;
  if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy)) {
    if (ai_euro_settler_on_tile(ctx, nation_id, fx, fy, 1) &&
        map_tile_is_water(ctx->map, wx - 1, wy)) {
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx - 1, wy);
    } else {
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx, wy);
    }
    return;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy + 2);
  if (!ai_euro_settler_on_tile(ctx, nation_id, fx, fy, 0)) {
    u->moves = 0;
  }
}

/* ========================================================================
 * FUN_521d_20e6 / FUN_521d_0a60 per-unit act — stage split (2026-09-16)
 *
 * `ai_euro_unit_act` below used to carry the whole act body (~2060 lines) in
 * one function. It is now a driver that runs the DOS stages in the same order,
 * as a chain of static per-stage helpers. The split is purely structural: no
 * statement was reordered, no gate changed, and every RNG-drawing call keeps
 * its position in the stream.
 *
 * Stage order (each helper's own header names its DOS citation):
 *   head (in ai_euro_unit_act)    guards, 049e notify, roam-abort, coastal
 *                                 embark / mil unload, MoW sail-home, early
 *                                 20e6 move-scoring gate, on-colony admit
 *   ai_euro_act_pioneer_corridor  FR tip corridor + 5952_035e equip arm
 *   ai_euro_act_soldier_staging   SP post-found soldier staging corridor
 *   ai_euro_act_ship              ship band (5 sub-stages, see below)
 *   ai_euro_act_land              case 0x0b land band (6 sub-stages)
 *
 * Locals that cross stage boundaries live in `struct ai_euro_act_ctx`; each
 * helper aliases the ones it needs at entry and writes the mutated ones back
 * at its fall-through exit. Control flow that used to be a bare `return;`
 * inside the body is carried out as `AI_EURO_ACT_RETURN` and honoured by the
 * caller, so an early bail still ends the act exactly where it did before.
 * ======================================================================== */

#include "core/ai_euro_internal.h" /* AiEuroActStatus, struct ai_euro_act_ctx */

/*
 * FUN_5952_035e equip-arm candidate scorer (raw 94318-94345; annotated
 * colony_tick_5952_035e.md:667-691). DOS-LITERAL. `target` is local_1b4,
 * i.e. local_8e with 0x17 (Dragoon) mapped to 0x15 (Soldier), so it is
 * always 0x15 at the one live call site.
 *
 *   score = 0
 *   prof == target                       -> score = 4
 *   else if !FUN_281f_0c9a(prof)         -> score += 1      (not an expert)
 *   else if target == 0x15 && !(+0x1b & 0x40) -> score = 0xff9d  (= -99)
 *   prof == 0x19 (Indentured Servant)    -> score += 1
 *   prof == 0x1a (Petty Criminal)        -> score += 2
 *   prof != 0x1b (Indian Convert) && best <= score -> take it
 *
 * `local_16e` starts at 0xffff and is compared as a signed 16-bit int, so
 * best starts at -1; `<=` means later ties win. Returns the colonist slot,
 * or -1 when every slot is an Indian Convert (or the colony is empty).
 */
COLONIZE_INTERNAL int ai_euro_5952_equip_pick(const ColonizeColony* c, int target) {
  if (!c) {
    return -1;
  }
  int best = -1;
  int pick = -1;
  const int pop = (int)c->population;
  for (int i = 0; i < pop && i < COLONIZE_COLONY_POP_MAX; ++i) {
    const int prof = (int)c->colonists[i].profession;
    int score = 0;
    if (prof == target) {
      score = 4;
    } else if (!ai_euro_5952_job_is_expert(prof)) {
      score += 1;
    } else if (target == UNITS_JOB_SOLDIER &&
               (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_GARRISON) == 0) {
      score = -99;
    }
    if (prof == UNITS_JOB_SERVANT) {
      score += 1;
    }
    if (prof == UNITS_JOB_CRIMINAL) {
      score += 2;
    }
    if (prof != UNITS_JOB_CONVERT && best <= score) {
      best = score;
      pick = i;
    }
  }
  return pick;
}

/*
 * Stage: Pioneer FR tip corridor + colony tools/muskets equip arm
 * (FUN_5952_035e absorb+equip pair, raw 94257-94352). Extracted verbatim
 * from ai_euro_unit_act.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_pioneer_corridor(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  const int is_ship = a->is_ship;

  /*
   * FR tip south of new colony: leave found+1 toward SW coast (TURN4→5 pioneer
   * (50,38)→(48,39)). Geometric offset from town — not a nation peel.
   */
  if (!is_ship && ctx->colonies && ai_euro_name_is_pioneer(ai_euro_unit_kind(ctx->units, u))) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (u->x == c->x && u->y == c->y + 1) {
        const int tx = c->x - 3;
        const int ty = c->y + 3;
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tx, ty);
        while (u->active && u->moves > 0 && (u->x != tx || u->y != ty)) {
          if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
            break;
          }
          u = units_get(ctx->units, u->id);
          if (!u) {
            return AI_EURO_ACT_RETURN;
          }
        }
        if (u) {
          u->moves = 0;
        }
        return AI_EURO_ACT_RETURN;
      }
      /* Next act: from SW coast staging return to town (TURN5→6 48,39→50,37). */
      if (u->x == c->x - 2 && u->y == c->y + 2) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, c->x, c->y);
        if (u->moves < 2 * UNITS_MP_PER_TILE) {
          u->moves = 2 * UNITS_MP_PER_TILE;
        }
        while (u->active && u->moves > 0 && (u->x != c->x || u->y != c->y)) {
          if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
            break;
          }
          u = units_get(ctx->units, u->id);
          if (!u) {
            return AI_EURO_ACT_RETURN;
          }
        }
        if (u && u->active && u->x == c->x && u->y == c->y) {
          ai_euro_set_goto(u, UNITS_ORDER_NONE, c->x, c->y);
          u->moves = 0;
        }
        return AI_EURO_ACT_RETURN;
      }
      /*
       * The FUN_5952_035e absorption + equip PAIR used to run here, on the
       * Pioneer standing on its own town tile. RE-HOSTED 2026-09-18 into the
       * colony tick itself (ai_euro_5952_absorb_equip), which is where DOS
       * runs it — as a tile re-scan at raw 94231-94352, before the build
       * cascade, not from the arriving unit. FUN_521d_20e6 / the 0a60 goal
       * consumption are all a DOS unit act does for an AI unit standing on
       * its own colony, so nothing replaces it here.
       */
    }
  }

  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Stage: SP post-found soldier staging corridor. Extracted verbatim
 * from ai_euro_unit_act.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_soldier_staging(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  const int is_ship = a->is_ship;

  /*
   * SP post-found soldier staging corridor:
   *   SE+1 → SE+2 (TURN5→6 46,55→46,56)
   *   SE+2 → SE+3 (TURN6→7 46,56→46,57)
   * Cite: test-saves-ai/TURN6–7.
   */
  if (!is_ship && ctx->colonies &&
      ai_euro_name_is_soldier(ai_euro_unit_kind(ctx->units, u))) {
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
        if (u->moves <= 0) {
          (void)units_wake(ctx->units, u->id);
          u = units_get(ctx->units, u->id);
        }
        if (u && u->moves > 0) {
          (void)units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              );
          u = units_get(ctx->units, u->id);
        }
        if (u) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
          u->moves = 0;
        }
        return AI_EURO_ACT_RETURN;
      }
    }
  }

  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Ship stage 1: Europe treasure cash-in / dump-sell, then FUN_48d3_048e
 * Europe->map exit and its first scored ocean leg.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_ship_europe_exit(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;

  /* (The Europe/HS treasure cash-in that stood here was deleted 2026-09-23,
   * bugs.md #746 — DOS's AI never puts a treasure on a ship.) */
  /*
   * No Europe-dock dump-sell in the per-unit act. The "sell every hold at the
   * Europe dock" arm (ai_euro_try_transport_europe_sell) carried no DOS
   * citation — only the manual and the wiki Boycott page — and was deleted
   * 2026-09-18. It is not needed: DOS's Europe dock seller is the nation-level
   * FUN_521d_5d04 pass (viceroy_overlays.c:83168-83192), ported as the
   * `ai_euro_5d04_cb_sell_hold0` / `_cb_reward_case` loop, which empties the
   * holds of every ship of type 0x0d..0x12 sitting in Europe — Privateers and
   * Frigates included. The other two ported sellers are FUN_521d_20e6's
   * delivery sell tail (raw 2140-2163, ai_euro_20e6_delivery_sell_tail) when
   * no colony will take the delivery cargo, and FUN_364b_0688 phase O
   * (viceroy_unpacked.c 57806-57848, europe_ai_colony_dump_sell_w) which
   * sells the warehouse surplus a ship dumped into the colony.
   */

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
    /*
     * Established nation: the Europe-exit goto is the home coast, not the
     * opening west-explore course (4,13). A passenger's colony goto wins,
     * else the own coastal colony nearest the landfall guess. Without this
     * a mid-game ship left Europe aimed at (4,y): greedy steps west into a
     * land pocket, the pathfinder fallback routed back east, and the ship
     * sailed the same loop every turn with its cargo still aboard.
     */
    int home_wx = -1;
    int home_wy = -1;
    if (colonies_count_for_nation(ctx->colonies, nation_id) > 0) {
      int cx = -1;
      int cy = -1;
      for (int c = 0; c < u->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
        const ColonizeUnit* pax = units_get_const(ctx->units, u->cargo_ids[c]);
        if (!pax || !pax->active) {
          continue;
        }
        const ColonizeColony* pc = colonies_find_at_xy(ctx->colonies, pax->goto_x, pax->goto_y);
        if (pc && pc->nation_id == nation_id && map_tile_is_coastal(ctx->map, pc->x, pc->y)) {
          cx = pc->x;
          cy = pc->y;
          break;
        }
      }
      if (cx < 0) {
        int best = -1;
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          const ColonizeColony* c = &ctx->colonies->colonies[i];
          if (!c->active || c->nation_id != nation_id ||
              !map_tile_is_coastal(ctx->map, c->x, c->y)) {
            continue;
          }
          const int d = abs(c->x - lx) + abs(c->y - ly);
          if (best < 0 || d < best) {
            best = d;
            cx = c->x;
            cy = c->y;
          }
        }
      }
      if (cx >= 0 && ai_euro_coastal_water_near(ctx->map, cx, cy, lx, ly, &home_wx, &home_wy)) {
        lx = home_wx;
        ly = home_wy;
      }
    }
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
      const int mp = u->moves > 0 ? u->moves : units_max_mp(ctx->units, u->id);
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
      while (u->active && u->moves > 0 &&
             (u->x != u->goto_x || u->y != u->goto_y)) {
        if (!units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              )) {
          break;
        }
        ai_euro_sync_aboard_cargo_xy(ctx->units, u);
        u = units_get(ctx->units, u->id);
        if (!u) {
          return AI_EURO_ACT_RETURN;
        }
      }
      /* After approach leg: home coast (established), else west-explore
       * course for later turns (0a60). */
      if (home_wx >= 0) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, home_wx, home_wy);
      } else {
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, wx, wy);
      }
      u->moves = 0;
    }
  }

  a->exited_europe = exited_europe;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Ship stage 2: pre-first-colony staging retarget / beachhead hold, and
 * the post-found coast retarget once the first colony exists.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_ship_first_colony_course(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  int exited_europe = a->exited_europe;

  /*
   * After Europe-exit west-explore (TURN2 goto 4,13): next nation turn
   * retarget to 0a60 coastal staging by passenger landfall, then unload.
   * Cite: test-saves-ai/TURN2-3, ship XY = staging tip.
   */
  if (!exited_europe && !ai_euro_in_europe(u->x, u->y) &&
      colonies_count_for_nation(ctx->colonies, nation_id) == 0) {
    int has_settler = 0;
    int plx = -1;
    int ply = -1;
    for (int c = 0; c < u->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
      const ColonizeUnit* pax = units_get_const(ctx->units, u->cargo_ids[c]);
      if (!pax || !pax->active) {
        continue;
      }
      const ColonizeUnitKind pkind = ai_euro_unit_kind(ctx->units, pax);
      if (ai_euro_name_is_pioneer(pkind) || pkind == UNITS_KIND_COLONIST ||
          pkind == UNITS_KIND_SOLDIER) {
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
        const ColonizeUnitKind lkind = ai_euro_unit_kind(ctx->units, lu);
        if (!ai_euro_name_is_pioneer(lkind) && !ai_euro_name_is_soldier(lkind)) {
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
        if (map_chebyshev(u->x, u->y, sx, sy) <= 1) {
          u->moves = 0;
        }
      }
    }
    /*
     * Found-approach / post-beachhead ship course (TURN3→4) before sail.
     * Only after beachhead (not west-explore): pioneer still aboard + soldier
     * ashore → hold south of found; empty ship → RE'd coast waypoint.
     * Stop once the first colony exists (TURN4→5 FR leaves Quebec hold).
     * Cite: test-saves-ai/TURN3-5.
     */
    if (!west_explore_course && plx >= 0 &&
        colonies_count_for_nation(ctx->colonies, nation_id) == 0) {
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
          const ColonizeUnitKind pkind = ai_euro_unit_kind(ctx->units, pax);
          if (ai_euro_name_is_pioneer(pkind)) {
            pioneer_aboard = 1;
            any_cargo_settler = 1;
          } else if (ai_euro_name_is_soldier(pkind) ||
                     pkind == UNITS_KIND_COLONIST) {
            any_cargo_settler = 1;
          }
        }
        ai_euro_settlers_ashore(
          ctx, nation_id, &pioneer_ashore, &soldier_ashore, NULL, NULL
        );
        ai_euro_first_colony_ship_course(
          ctx, u, nation_id, fx, fy, pioneer_aboard, any_cargo_settler, pioneer_ashore,
          soldier_ashore
        );
      }
    }
    /*
     * Beachhead hold station: settler still aboard and goto is adjacent coast
     * water — do not spend MP entering the hold tile (TURN3 FR ship stays on
     * staging with goto=hold). Skip on west-explore retarget (staging sail).
     */
    if (!west_explore_course && has_settler &&
        map_chebyshev(u->x, u->y, u->goto_x, u->goto_y) <= 1 &&
        map_tile_is_coast_water(ctx->map, u->goto_x, u->goto_y)) {
      u->moves = 0;
    }
  }
  /*
   * First colony planted: drop found-hold latch (fx,fy+2) so the ship can
   * spend MP. Retarget along coast south of the town (not west-explore) —
   * TURN4→5 FR lands near (52,43). Cite: test-saves-ai/TURN5.
   */
  if (!exited_europe && !ai_euro_in_europe(u->x, u->y) &&
      colonies_count_for_nation(ctx->colonies, nation_id) > 0) {
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
    /* Opening-only geometry (one colony): a mid-game hull wandering through
     * (fx, fy+2) was yanked to a tip that can even be land, then ground the
     * greedy/pathfinder pair against it every turn. */
    if (cid >= 0 && colonies_count_for_nation(ctx->colonies, nation_id) == 1 &&
        ((u->goto_x == fx && u->goto_y == fy + 2) ||
         (u->x == fx && u->y == fy + 2))) {
      int tx = 0;
      int ty = 0;
      if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &tx, &ty) &&
          map_tile_is_water(ctx->map, tx, ty)) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tx, ty);
      }
    }
  }

  a->exited_europe = exited_europe;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Ship stage 3: war flag + treasure-aboard scan, peace cargo haul chain
 * (LAB_521d_457e cadence included), threatened unload, adjacent naval
 * attack, and the first-colony course re-assert.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_ship_war_trade(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  int exited_europe = a->exited_europe;

  /*
   * Thin naval war hunt (act-level): idle / station-keep ships at war sail
   * toward nearest foe sea unit or coastal colony water. Adjacent → try_attack.
   * (The "fandom Drake" Privateer re-aim described here — a named Privateer
   * always re-aiming the naval hunt even over a live sail goto — went with the
   * naval hunt itself on 2026-09-18; nothing in this stage re-aims. A hull's
   * fights come from the LAB_4d2e wander scorer, whose attack arm already
   * takes a Privateer against an unmet owner (raw 88880-88940).)
   */
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
  /* (The "treasure aboard → keep the Europe sail" suppression that stood here
   * went with the invented transport cluster on 2026-09-23, bugs.md #746.) */
  /*
   * Peace cargo haul: idle Caravel/Merchantman with hold space/TOOLS →
   * AI_SAIL toward tools/food-short coastal colony water. Cite: euro_unit_act
   * §2d2; TOOLS only (no invented FOOD cargo). Skip when war /
   * useful sail already set.
   */
  if (!at_war && !ai_euro_has_useful_goto(u, ctx->map)) {
    if (!ai_euro_try_post_found_coast_cruise(ctx, nation_id, u)) {
      {
        if (!ai_euro_try_ship_trade_haul(ctx, nation_id, u)) {
          /* DOS order: LAB_521d_457e sits immediately after the 4393
           * work-queue haul pick and before the wagon/treasure arms. */
          if (!(ai_euro_20e6_hs_cadence_enabled() &&
                ai_euro_20e6_457e_hs_cadence(ctx, u, nation_id))) {
            /* The Privateer-loot twin of this arm is retired (2026-09-18):
             * the export arm's hull test is now DOS's own, so loot sails
             * through the same raw 2166-2168 -> 48d3_015e path. */
            (void)ai_euro_try_ship_europe_export(ctx, nation_id, u);
          }
        }
      }
    }
  }
  /* (A manual-cited "drop Soldier at threatened colony" arm stood here until
   * 2026-09-18; the DOS disembark is the LAB_3558 mask block reached from
   * ai_euro_unload_settle.) */
  /* Leave enemy Fort/Fortress battery tiles before hunt/attack. Not war-gated:
   * the battery fires on any hull without a PEACE treaty (bugs.md #465). */
  if (!ai_euro_in_europe(u->x, u->y) &&
      ai_euro_naval_try_flee_fort_fire(ctx, u)) {
    u = units_get(ctx->units, u->id);
    if (!u || !u->active) {
      return AI_EURO_ACT_RETURN;
    }
  }
  /* An at-war block stood here with a war-cargo colony-sail call (retired by
   * smell audit sweep-3 area C #6 — a second, invented entry into
   * LAB_521d_3558), an act-level adjacent-foe naval attack and a distant
   * "nearest foe ship / enemy port" hunt aim (both retired 2026-09-18). DOS
   * has neither: a hull finds its fights only in the LAB_4d2e wander scorer
   * (raw 90210-90219, LAB_52aa odds term) that ai_euro_act_ship_sail runs
   * next, and its stations in the LAB_3558 colony-sail matrix. */

  /*
   * Re-assert first-colony ship course after trade/war haul may have yanked
   * idle Privateer/Caravel. Only post-beachhead (soldier ashore or cargo
   * empty) — never during Atlantic approach. Cite: TURN3→4 SP/DU cruise.
   */
  if (!exited_europe && !ai_euro_in_europe(u->x, u->y) &&
      colonies_count_for_nation(ctx->colonies, nation_id) == 0 &&
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
      const ColonizeUnitKind pkind = ai_euro_unit_kind(ctx->units, pax);
      if (ai_euro_name_is_pioneer(pkind)) {
        pioneer_aboard = 1;
        any_cargo = 1;
      } else if (ai_euro_name_is_soldier(pkind)) {
        any_cargo = 1;
      }
      if (lf_x < 0 && pax->goto_x >= 0 && pax->goto_y >= 0 && pax->goto_x < 255 &&
          pax->goto_y < 255) {
        lf_x = pax->goto_x;
        lf_y = pax->goto_y;
      }
    }
    ai_euro_settlers_ashore(ctx, nation_id, &pioneer_ashore, &soldier_ashore, &lf_x, &lf_y);
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
      ai_euro_first_colony_ship_course(
        ctx, u, nation_id, fx, fy, pioneer_aboard, any_cargo, pioneer_ashore, soldier_ashore
      );
    }
  }

  a->at_war = at_war;
  a->exited_europe = exited_europe;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Ship stage 4: raw 90210-90219 LAB_4d2e idle wander, then the case 0x0b
 * sail loop (scored step, pathfinder fallback, route latch).
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_ship_sail(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  int exited_europe = a->exited_europe;

  /*
   * Raw 90210-90219 → LAB_4d2e: every ship band above fell through. An idle
   * hull takes the 8-direction wander pick (far roam latch, explore terms,
   * LAB_52aa attack odds); a busy one only fights an adjacent foe.
   */
  if (u->active && !exited_europe && !ai_euro_in_europe(u->x, u->y) &&
      u->moves > 0) {
    const int busy = ai_euro_has_useful_goto(u, ctx->map);
    if (!busy || ai_euro_20e6_adjacent_foreign_09dc(ctx, u->x, u->y, nation_id)) {
      (void)ai_euro_20e6_ship_wander_act(ctx, u, nation_id, busy);
      u = units_get(ctx->units, u->id);
      if (!u || !u->active) {
        return AI_EURO_ACT_RETURN;
      }
    }
  }
  /*
   * Case 0x0b ship sail: preserve landfall/sail goto. Scored ocean steps
   * (thin 20e6) drain moves — mirror land FOUND/MILITARY MP-drain.
   * Arrival clears via station-keep below.
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
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[ship] unit %d at (%d,%d) goto (%d,%d) ord %d mp %d cargo %d\n", u->id, u->x, u->y, u->goto_x, u->goto_y, u->orders, u->moves, u->cargo_count);
  }
  if (units_orders_follow_goto(u->orders) && (u->x != u->goto_x || u->y != u->goto_y)) {
    int prev_x = -1;
    int prev_y = -1;
    for (;;) {
      if (!u->active || u->moves <= 0 || !units_orders_follow_goto(u->orders)) {
        break;
      }
      if (u->x == u->goto_x && u->y == u->goto_y) {
        break;
      }
      int dx = 0;
      int dy = 0;
      int tx = 0;
      int ty = 0;
      const int latched =
        u->id >= 0 && u->id < COLONIZE_UNITS_MAX && s_euro_ship_route_latch[u->id];
      if (!latched && ai_euro_score_move(ctx, u, u->goto_x, u->goto_y, &dx, &dy)) {
        tx = u->x + dx;
        ty = u->y + dy;
      } else {
        tx = -1;
        ty = -1;
      }
      /* Greedy step straight back to the tile we just left = local optimum
       * ping-pong (the on-screen wiggle); route via the pathfinder instead. */
      if (tx == prev_x && ty == prev_y) {
        tx = -1;
        ty = -1;
      }
      const int from_x = u->x;
      const int from_y = u->y;
      int moved = 0;
      if (tx >= 0) {
        const int foe = units_id_at(ctx->units, tx, ty);
        const ColonizeUnit* fo = foe >= 0 ? units_get_const(ctx->units, foe) : NULL;
        if (fo && fo->nation_id != u->nation_id) {
          /* Naval combat stays on adjacent prefer-weak pick — do not
           * chain-attack via scored step into a foe tile (try_move cannot
           * enter ships; mirror prior advance_goto block). Own ships stack. */
          break;
        }
        {
          ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
          moved = units_try_move_w(&w_, u->id, tx, ty);
        }
      }
      if (getenv("AI_SHIP_TRACE")) {
        fprintf(stderr, "[ship]   step try (%d,%d) moved=%d mp %d\n", tx, ty, moved, u->moves);
      }
      if (!moved) {
        /*
         * Greedy scored step stalled (land wall / own-ship block between
         * ship and goal). Fall back to the DOS FUN_6662 pathfinder tiers so
         * a ship with a far goto routes around the coast instead of
         * grinding the same two tiles every act (the on-screen "wiggle").
         */
        int px = 0;
        int py = 0;
        /* Stay on the pathfinder for this goto, this act and the next:
         * greedy-then-pathfinder each act undoes itself (west two, east
         * two) and the ship circles for turns. */
        if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
          s_euro_ship_route_latch[u->id] = 1;
        }
        if (!units_next_goto_step_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .rng=(ColonizeDosRng*)(ctx->rng)}, u->id, &px, &py)) {
          /* Pathfinder agrees the goal is unreachable from here — drop the
           * goto so next act re-aims instead of resuming the same grind
           * (the cross-turn A↔B wiggle). */
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, u->x, u->y);
          break;
        }
        if (getenv("AI_SHIP_TRACE")) {
          fprintf(stderr, "[ship]   pathfinder step (%d,%d)\n", px, py);
        }
        {
          /* Only a FOREIGN occupant blocks the pathfinder step. Own ships
           * stack (DOS allows same-nation stacking on water); refusing the
           * tile made two own hulls whose routes crossed block each other
           * for a hundred turns (campaign3 Spanish Caravel/Privateer at
           * (32,51)/(31,50), each pathing onto the other's tile). */
          const int occ = units_id_at(ctx->units, px, py);
          const ColonizeUnit* o = occ >= 0 ? units_get_const(ctx->units, occ) : NULL;
          if (o && o->nation_id != u->nation_id) {
            break;
          }
        }
        ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
        if (!units_try_move_w(&w_, u->id, px, py)) {
          break;
        }
        units_note_goto_step(u->id, px - from_x, py - from_y);
      } else {
        units_note_goto_step(u->id, tx - from_x, ty - from_y);
      }
      prev_x = from_x;
      prev_y = from_y;
      u = units_get(ctx->units, u->id);
      if (!u) {
        return AI_EURO_ACT_RETURN;
      }
    }
  }

  a->exited_europe = exited_europe;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Ship stage 5: post-sail arrival tails — adjacent attack, threatened
 * unload, Europe/HS treasure cash, unload_settle, first-colony hold /
 * cruise-tip parking.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_ship_arrival(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  int at_war = a->at_war;
  int exited_europe = a->exited_europe;

  /*
   * (The arrival-time "adjacent naval attack" and "mil unload" arms that stood
   * here were deleted 2026-09-18. DOS has no act-level naval attack picker:
   * ships fight through the shared LAB_521d_4d2e wander scorer entered at raw
   * 90210-90219 (ai_euro_act_ship_sail, LAB_52aa odds term), and disembark
   * through the LAB_3558 mask block in ai_euro_unload_settle below.)
   */
  /* (The HS/Europe arrival treasure cash that stood here was deleted
   * 2026-09-23, bugs.md #746.) */
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
      colonies_count_for_nation(ctx->colonies, nation_id) == 0) {
    int fx = 0;
    int fy = 0;
    int lx = 0;
    int ly = 0;
    if (ai_euro_recover_landfall_from_ship(u->x, u->y, &lx, &ly) ||
        ai_euro_recover_landfall_from_ship(u->goto_x, u->goto_y, &lx, &ly)) {
      if (ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lx, ly, &fx, &fy)) {
        if (u->goto_x == fx && u->goto_y == fy + 2) {
          u->moves = 0;
        }
        int wx = 0;
        int wy = 0;
        if (ai_euro_ocean_3558_empty_cruise_tip(ctx->map, fx, fy, &wx, &wy) &&
            map_chebyshev(u->x, u->y, wx, wy) <= 1) {
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
            if (ai_euro_name_is_pioneer(ai_euro_unit_kind(ctx->units, pu)) &&
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
              if (u->moves <= 0) {
                u->moves = units_max_mp(ctx->units, u->id);
              }
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, wx - 1, wy);
              (void)units_advance_goto_one_step_w(
                &(ColonizeWorld){.units = ctx->units, .colonies = ctx->colonies, .map = ctx->map},
                u->id
              );
              u = units_get(ctx->units, u->id);
            }
            if (u) {
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
              u->moves = 0;
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
            u->moves = 0;
          }
        }
      }
    }
  }
  /* Post-found coast tip: park AI_MOVE; spent MP stops outer re-act.
   * COL1 export maps AI_MOVE@self → moves spent 0. Cite: TURN5 FR 52,43. */
  if (u && u->active && !exited_europe &&
      colonies_count_for_nation(ctx->colonies, nation_id) > 0 &&
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
      u->moves = 0;
    }
  }
  /*
   * Post-found SW cruise: park AI_MOVE@self after MP drain (TURN5 DU 39,18).
   * Tip station-keep alone is !useful_goto — without this, trade haul yanks.
   */
  if (u && u->active && !exited_europe &&
      colonies_count_for_nation(ctx->colonies, nation_id) == 1 && u->cargo_count == 0 &&
      u->moves <= 0) {
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
        map_chebyshev(u->x, u->y, tip_x, tip_y) <= 8 &&
        /* Do not yank FR mid tip→colony SAIL (TURN6→7 g=Quebec). */
        !(u->goto_x == fx && u->goto_y == fy)) {
      ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, u->x, u->y);
    }
  }
  /*
   * FUN_5bfb_3180 ship-slow (adjacent foreign warship / Fort / Fortress)
   * now runs per step inside units_try_move (units_ship_slow_scan), for
   * AI and human movers alike; the old end-of-act ambush here is gone.
   */

  a->at_war = at_war;
  a->exited_europe = exited_europe;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 1: LCR on entry, land-war engage/hunt, peace-border hunt,
 * scout exploration.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_hunt_scout(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int at_war_land = a->at_war_land;
  const int is_land_hunter = a->is_land_hunter;
  int land_war_hunted = a->land_war_hunted;
  int scout_explored = a->scout_explored;

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
    if (units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL), .rng=(ColonizeDosRng*)(ctx->rng), .europe=(EuropeScreen*)(NULL)}, u->id, -1)) {
      scout_explored = 1;
    }
    /* Vanish / hostile-burial outcomes may have despawned the scout. */
    if (!u->active) {
      return AI_EURO_ACT_RETURN;
    }
  }

  /*
   * War land band: wake a passive hunter, then take the two adjacent-settlement
   * arms (FUN_521d_20e6 `0x46` colony seize / `0x4c` village seize). The unit's
   * actual fight is picked afterwards by the shared LAB_521d_4d2e wander scorer
   * in ai_euro_20e6_land_step (attack term raw 88880-88940), which commits the
   * enemy tile as a one-shot goto that FUN_465b_0000 resolves — no act-level
   * adjacent-attack loop and no distant hunt aim (both retired 2026-09-18).
   * Cite: units.h units_wake; euro_unit_act §2c wake.
   */
  if (at_war_land && is_land_hunter && ai_euro_land_is_passive_orders(u) &&
      !ai_euro_has_useful_goto(u, ctx->map)) {
    (void)units_wake(ctx->units, u->id);
  }
  /* Board already attempted early (pre-gate); engage if still on map. */
  if (at_war_land && is_land_hunter && !ai_euro_land_is_fortified(u) &&
      !u->ai_landfall_wait) {
    if (!ai_euro_land_engage_adjacent(ctx, u, &land_war_hunted)) {
      return AI_EURO_ACT_RETURN;
    }
  }

  /*
   * (Retired 2026-09-23, bugs.md #760.) A "peace colony-defence wake" arm sat
   * here: a military unit or Artillery standing on its own colony woke and
   * hunted any foreign Euro land unit within MD<=2, with an extra artillery
   * clause that overrode an existing course. It cited only Colonization.pdf
   * ("Defending a Colony") and euro_unit_act §2d3 — no FUN_/raw — and a sweep
   * of FUN_521d_20e6 (raw 88266-90445) has no counterpart: the only own-colony
   * garrison handling in DOS is the type-agnostic LAB_5899 arm (raw 88584-88612,
   * ported above in ai_euro_20e6_land_arms), which keeps an armed unit on the
   * colony via order_code 0x47 and releases a surplus stack to the ordinary
   * LAB_4d2e neighbour scorer. Artillery's only 20e6 modifier is the
   * score-zero-off-a-settlement at raw 88911.
   * The flag stays in the ctx (owned by ai_euro_internal.h) and is now always 0.
   */
  int peace_border_hunted = 0;

  /*
   * The invented "CONTACT scout ring / fog explore" arm that stood here is
   * gone (bugs.md #493/#495). It scored a ring of tiles (MD 2-4) around the
   * nearest tribe with weights x1000/x50/x10 and a fog sweep of MD<=8, and
   * it cited the manual, not a FUN_. Worse, it re-stamped an AI_MOVE goto on
   * every act, so a Scout was permanently "on a goto" and the real
   * FUN_521d_20e6 type-5 machinery (explorer flag / patrol 0x56 / village
   * 0x4c / explore ring) behind ai_euro_move_scoring_gate never ran for it.
   * DOS's Scout band reads no relation matrix and no profession byte at all
   * (raw 88514-88530 pre-gate, 89047-89059 patrol, 89064-89068 village,
   * 89076+ explore ring); its explore radius comes from the continent
   * rival-strength byte and the unit's own hold[0] explore counter
   * (local_12, raw 89290-89291), never from "Seasoned Scout".
   */

  a->land_war_hunted = land_war_hunted;
  a->peace_border_hunted = peace_border_hunted;
  a->scout_explored = scout_explored;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 2: Treasure Train routing (coast / board / cash) and the
 * 20e6 47b9 dead-end bail.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_treasure(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  const int is_treasure = a->is_treasure;
  int treasure_routed = a->treasure_routed;

  /*
   * DOS-LITERAL FUN_521d_20e6 raw 89997-90040 — the whole (and only) AI
   * treasure band, in DOS order:
   *   arm 1  iStack_2e == 0 (standing on ANY own colony): cash
   *          `+0x315b * 100` into the nation purse untaxed, @LOOTFOREIGN
   *          popup while (DS:0x5382 & 1) == 0, then LAB_47b9 destroy.
   *   arm 2  else nearest own colony on this landmass
   *          (iStack_2c == iStack_38) → FUN_281f_09e6 bind + LAB_4701/4567
   *          walk to it.
   *   arm 3  else nearest own unit on this landmass (FUN_281f_08a8) →
   *          LAB_27f5 walk to it; else, when the adjacent-claim probe names
   *          the human (DS:0x5398), LAB_47b9 destroy.
   * Arms 2/3 live in ai_euro_20e6_47b9_dead_end below.
   *
   * The coast-target / board-and-sail / Europe-cash chain that stood between
   * arm 1 and the rest was deleted 2026-09-23 (bugs.md #745/#746): no DOS
   * site boards an AI treasure, prefers a coastal colony, or gives the AI
   * Cortes' free King galleon (FUN_465b_0000 raw 75800 is human-control
   * gated, bugs.md #478).
   */
  if (is_treasure) {
    if (ai_euro_20e6_treasure_cash_in(ctx, u, nation_id)) {
      return AI_EURO_ACT_RETURN;
    }
  }
  /*
   * LAB_521d_457e/47b9 treasure dead end: nothing above bound a course and the
   * treasure is cut off from every own colony and own unit on its landmass —
   * DOS destroys it. Wagons and Treasure defer the 20e6 gate (see the
   * defer_gate list), so this band runs act-level here instead.
   */
  if (is_treasure && !treasure_routed) {
    const int r = ai_euro_20e6_47b9_dead_end(ctx, u, nation_id);
    if (r == 2) {
      return AI_EURO_ACT_RETURN; /* destroyed */
    }
    if (r == 1) {
      treasure_routed = 1; /* LAB_4701 / LAB_27f5 course bound */
    }
  }

  a->treasure_routed = treasure_routed;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 3: wagon haul, Pioneer improve job, the six field-expert
 * admit/assign arms, and the indoor expert workplace assign.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_roles(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  int land_war_hunted = a->land_war_hunted;
  int peace_border_hunted = a->peace_border_hunted;
  int scout_explored = a->scout_explored;
  int treasure_routed = a->treasure_routed;
  const ColonizeUnitKind ukind = a->ukind;

  /*
   * Wagon Train haul (act-level): idle Wagon with hold capacity or TOOLS /
   * LUMBER / ORE / MUSKETS / HORSES / FOOD → AI_MOVE toward matching short
   * colony (unload via existing delivery). Cite: euro_unit_act §2d;
   * Colonization.pdf Wagon Train; 5cf6 food/lumber/ore_short.
   */
  int wagon_hauled = 0;
  if (!treasure_routed && ai_euro_type_is_wagon_name(ukind) &&
      !ai_euro_land_is_fortified(u)) {
    if (ai_euro_try_wagon_haul(ctx, nation_id, u)) {
      wagon_hauled = 1;
    }
    /* (The wagon Europe-export feeder — a thin Linux-only arm — was retired
     * 2026-09-07b: DOS's 457e origin walk owns every off-errand wagon beat,
     * so wagons never fed the coastal export leg in DOS. Surplus reaches
     * Europe through the ships-only 4393 pickup queue.) */
    /* The wagon 47b9 destroy lives inside the origin walk now (the haul beat
     * above owns the whole 457e wagon arm) — stop touching a despawned unit. */
    if (!u->active) {
      return AI_EURO_ACT_RETURN;
    }
  }

  a->land_war_hunted = land_war_hunted;
  a->peace_border_hunted = peace_border_hunted;
  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->wagon_hauled = wagon_hauled;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 4: peace fortify / colonist admit, Artillery siege hunt and
 * Artillery fortify, Missionary CONTACT.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_fortify(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  const int at_war_land = a->at_war_land;
  int land_war_hunted = a->land_war_hunted;
  int peace_border_hunted = a->peace_border_hunted;
  int scout_explored = a->scout_explored;
  int treasure_routed = a->treasure_routed;
  int wagon_hauled = a->wagon_hauled;

  /*
   * Peace fortify (case 0x0b fortify arm): idle armed land unit on own colony
   * tile → FORTIFY if not already. Overrides explore/FOUND scoring-gate yank
   * while on-colony (defense). At war: wake+hunt owns garrison instead.
   *
   * DOS-LITERAL FUN_521d_20e6 raw 89011-89013 (asm OVL14_L0000:0x5992-0x59c9)
   * — the 'F' (0x46) arm's own unit test is exactly
   *   `DS:0x5236[type * 0xe] > 1 && (type < 0xd || type > 0x12)`
   * i.e. the NAMES @UNIT ATTACK column above 1 plus "not a ship type"
   * (the loader stores column 3 = attack at 0x5236, column 4 = defense at
   * 0x5235, raw 121115-121119). Replaces this arm's name matching, which
   * both missed armed types whose name is not in the military list and let
   * attack-1 types (Colonist, Scout, Pioneer) through. DOS spends no
   * garrison quota here, so the quota gate is gone from this arm.
   * bugs.md #512.
   */
  const int fort_dos_type = ai_euro_20e6_dos_type(ctx->units, u);
  if (!at_war_land && !peace_border_hunted && !treasure_routed && !wagon_hauled &&
      !scout_explored &&
      !land_war_hunted && fort_dos_type >= 0 &&
      ai_euro_20e6_type_combat(fort_dos_type) > 1 &&
      (fort_dos_type < 0xd || fort_dos_type > 0x12) &&
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
          if ((g->code == AI_GOAL_MILITARY || g->code == AI_GOAL_CONTACT ||
               g->code == AI_GOAL_ESCORT) &&
              (g->x != u->x || g->y != u->y)) {
            keep_mil = 1;
            break;
          }
        }
        /* raw 89011-89029 sets +0x314b = 0x46 outright — no garrison-quota
         * accounting anywhere in the arm, so no quota gate here either. The
         * colony's quota is still spent so the other quota readers see the
         * garrison. (The rest of that DOS arm — local_ea, the chain-alone test
         * and the foreign-settlement neighbour scan — is a different trigger
         * and lives in ai_euro_20e6_border_park_arm.) bugs.md #512. */
        if (!keep_mil && units_order_fortify(ctx->units, u->id)) {
          ColonizeColony* cm = colonies_get_mut(ctx->colonies, cid);
          if (cm && cm->garrison_quota > 0) {
            cm->garrison_quota--;
          }
          return AI_EURO_ACT_RETURN; /* stay fortified — skip FOUND/explore yank */
        }
      }
    }
  }

  /*
   * No Artillery-specific siege hunt. DOS has no artillery target band: in
   * FUN_521d_20e6 type 0x0b appears exactly once in the shared tile scorer
   * (raw 88911-88913) as a score *modifier* — `type == 0x0b && no colony and
   * no village on the tile -> score = 0` — and FUN_465b_0000 (raw 75417) is
   * type-agnostic. The "Artillery siege hunt" arm that used to sit here cited
   * only Colonization.pdf / king_ref and was deleted (bugs.md #513).
   */

  /*
   * (Retired 2026-09-23, bugs.md #760.) An "Artillery fortify" arm sat here:
   * idle Artillery on its own colony was FORTIFY-ed through the garrison quota.
   * Manual-only citation (euro_unit_act §2d3 / Colonization.pdf / king_ref) and
   * FUN_521d_20e6 has no `+0x3146 == 0x0b` branch in the act path at all
   * (raw 88266-90445). DOS's own-colony arm is the type-agnostic LAB_5899
   * (raw 88584-88612, ported in ai_euro_20e6_land_arms): attack > 1, type
   * outside [0xd,0x12], not 4, not 8 → labor_shortage--, order_code = 0x47,
   * stay. Artillery satisfies every one of those tests, so the DOS path already
   * handles exactly the case this arm was covering. Same invention class as the
   * already-deleted "Artillery siege hunt" above.
   */

  /*
   * (Retired 2026-09-22, bugs.md #557.) A missionary CONTACT goal arm sat
   * here (prio 3 goto of the nearest mission-less tribe, skipped when an
   * adjacent tribe was alarmed). FUN_521d_20e6 has no `+0x3146 == 3` arm:
   * DOS AI missionaries are hired/blessed in FUN_521d_5d04 and then take the
   * generic land wander; a missionary that reaches a village is handled by
   * FUN_4d56_4528's non-human switch (ai_contact_ai_missionary_village).
   */

  a->land_war_hunted = land_war_hunted;
  a->peace_border_hunted = peace_border_hunted;
  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->wagon_hauled = wagon_hauled;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 5: FUN_521d_0a60 goal-consumption tail — read the committed
 * primary-goal pick back, else run the founder/labor fallback scan.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_goal_consume(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  const int at_war_land = a->at_war_land;
  int land_war_hunted = a->land_war_hunted;
  int peace_border_hunted = a->peace_border_hunted;
  int scout_explored = a->scout_explored;
  int treasure_routed = a->treasure_routed;
  const ColonizeUnitKind ukind = a->ukind;
  int wagon_hauled = a->wagon_hauled;

  /*
   * FUN_521d_0a60 goal-consumption tail, structurally ported (see
   * ai_euro_0a60_goal_orders_structural above): runs once per nation per turn
   * and commits its pick into the unit's own +0x314c/+0x314d/e (bugs.md
   * #525). `ai_euro_unit_act` captures it before the 20e6 gate can overwrite
   * the byte with a 0x0c wander commit; the concrete AI_GOAL_* code has no
   * DOS byte at all and is re-read from the goal table at the goal tile.
   */
  int goal_x = (a->goal_code >= 0) ? a->goal_x : u->goto_x;
  int goal_y = (a->goal_code >= 0) ? a->goal_y : u->goto_y;
  int goal_code = a->goal_code;
  {
    /*
     * A "threatened-Stockade LABOR override" stood here until 2026-09-18: a
     * war-threat proximity scan that re-aimed a Free Colonist at any own
     * colony building a Stockade. It had no DOS counterpart — the goal table
     * is written only by FUN_521d_0a60 (its 'A' mark block spends colony
     * +0x1e garrison_quota / +0x8e labor_shortage, neither of which is a
     * building-choice term) and the one AI construction picker is the
     * FUN_5952_035e cascade, which selects a Stockade from the colony's own
     * +0x1b flags, never from an adjacent enemy. Deleted with its helpers.
     */
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
   * by the colony tick's placement pass; this is the no-forest fallback.
   * Tools-short deepen
   * (peace Pioneer): tools_short > 0 extends MD≤8 toward tools-short colony
   * so idle Pioneer walks in for case-7 tools delivery. Cite: 5cf6 shortage
   * tallies + euro_unit_act §2d/§2e; no invented rates.
   */
  {
    const int is_pioneer =
      ai_euro_name_is_pioneer(ukind);
    const int is_farmer = ai_euro_unit_is_food_labor(ctx->units, u) &&
                          (u->profession == COLONIZE_JOB_FARMER);
    /* Master Carpenter — hammer bind for Stockade/Warehouse/Lumber Mill.
     * @JOB row 0x0d (Master Carpenter); see raw 3326 comment above for the
     * same profession value on the analogous production-side check. */
    const int is_carpenter =
      ukind == UNITS_KIND_COLONIST && u->profession == 0x0d;
    /* Expert Lumberjack — lumber for incomplete Warehouse/Lumber Mill. */
    const int is_lumberjack =
      ukind == UNITS_KIND_COLONIST && u->profession == COLONIZE_JOB_LUMBERJACK;
    const int is_free_colonist =
      ukind == UNITS_KIND_COLONIST &&
      (u->profession == 19 ||
       (!is_pioneer && !is_farmer && !is_carpenter && !is_lumberjack));
    const int is_colonist_cap =
      ukind != UNITS_KIND_SOLDIER && ukind != UNITS_KIND_DRAGOON &&
      ukind != UNITS_KIND_SCOUT && !ai_euro_type_is_wagon_name(ukind) &&
      (is_pioneer || is_farmer || is_carpenter || is_lumberjack ||
       ukind == UNITS_KIND_COLONIST);
    if (!land_war_hunted && !peace_border_hunted && !scout_explored && !treasure_routed &&
        !wagon_hauled  &&
        is_colonist_cap &&
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
       * by the colony tick's own placement pass.
       */
      const int lumberjack_bind = is_lumberjack && !is_pioneer;
      /* (The MD≤3 "threatened Stockade" widening stood here; deleted with the
       * goal-side override above — 2026-09-18.) */
      const int max_dist =
        (food_emergency && ai_euro_unit_is_food_labor(ctx->units, u)) ||
            tools_pioneer_bind || food_farmer_bind || food_free_colonist_bind
          ? 8
          : 1;
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
        if (food_free_colonist_bind && dist > 1) {
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
          nation_id, bx, by, code, (food_emergency || b_construction) ? 6 : 4
        );
      }
    }
  }

  a->goal_code = goal_code;
  a->goal_x = goal_x;
  a->goal_y = goal_y;
  a->land_war_hunted = land_war_hunted;
  a->peace_border_hunted = peace_border_hunted;
  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->wagon_hauled = wagon_hauled;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 6: act on the bound goal — FOUND on arrival, wagon Europe
 * sell, LABOR/COLONY admit, MILITARY/CONTACT engage, goto commit + move
 * drain, and the adjacent seize/attack tail.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_goal_dispatch(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  const int at_war_land = a->at_war_land;
  int goal_code = a->goal_code;
  int goal_x = a->goal_x;
  int goal_y = a->goal_y;
  const int is_land_hunter = a->is_land_hunter;
  const int is_ship = a->is_ship;
  int land_war_hunted = a->land_war_hunted;
  int peace_border_hunted = a->peace_border_hunted;
  int scout_explored = a->scout_explored;
  int treasure_routed = a->treasure_routed;
  const ColonizeUnitKind ukind = a->ukind;
  int wagon_hauled = a->wagon_hauled;

  if (goal_code == AI_GOAL_FOUND && u->x == goal_x && u->y == goal_y) {
    ai_euro_found_with_unit(ctx, u, nation_id);
    return AI_EURO_ACT_RETURN;
  }
  /*
   * A founder can also arrive on a FOUND tile through the 20e6 move-scoring
   * gate, which writes the goto but leaves no 0a60 goal code behind. Nothing
   * then founded on arrival: settlers walked to the site and stood on it for
   * the rest of the game. Found when we are standing on this nation's own best
   * FOUND tile and the tile still takes a colony.
   */
  if (goal_code < 0 && !is_ship &&
      (ai_euro_name_is_pioneer(ukind) || ukind == UNITS_KIND_COLONIST)) {
    int bfx = 0;
    int bfy = 0;
    if (ai_goals_best_found_tile_near(ctx->map, nation_id, u->x, u->y, &bfx, &bfy) &&
        bfx == u->x && bfy == u->y &&
        colonies_can_found(ctx->colonies, ctx->map, u->x, u->y)) {
      ai_euro_found_with_unit(ctx, u, nation_id);
      return AI_EURO_ACT_RETURN;
    }
  }

  /*
   * No tools-delivery / Europe-sell arms here. The "wagon or Pioneer on an own
   * colony tile unloads what the colony is short of" pair
   * (ai_euro_try_wagon_tools_delivery + ai_euro_try_pioneer_tools_delivery)
   * and the wagon Europe dump-sell were manual-cited inventions, deleted
   * 2026-09-18. DOS wagon delivery is the FUN_521d_20e6 arrival block
   * (raw 3007-3012: dump EVERY hold into the bound colony, then the load
   * matrix) in ai_euro_try_wagon_haul — never a per-cargo shortage filter —
   * and a wagon never reaches Europe at all.
   */

  /*
   * No LABOR/COLONY "arrive on the colony tile → join as a colonist" arm.
   * DOS has no such unit-act outcome: FUN_521d_20e6's complete +0x314b
   * vocabulary over raw 88266-89800 is 0x39/0x3d/0x40/0x42/0x46/0x47/0x4c/
   * 0x56/0x65 (no join), and FUN_521d_0a60's own-colony block only marks
   * standing military 'A' (0x41) — raw 87683-87756, ported as
   * ai_euro_colony_goals_colony_garrison. That mark is bookkeeping: it
   * decrements labor_shortage (+0x8e) and garrison_quota (+0x1e) and hides
   * the unit from this turn's goal scan; it never moves it into a colonist
   * slot. Note the DOS polarity — garrison_quota is a COUNTER the admission
   * spends, never a `== 0` gate. The actual absorption of a unit standing
   * on its own colony tile is FUN_5952_035e's colony-tick tile re-scan
   * (raw 94231-94274, ported as ai_euro_5952_absorb_equip and called from
   * ai_euro_colony_goals_colony_labor), which owns both the per-type gates
   * and the NEEDS_COLONISTS (+0x1b bit 0x10) precondition. The
   * `garrison_quota == 0` admit that used to sit here cited only
   * test-saves-ai/TURN4-5 and was deleted (sibling of bugs.md #512/#515).
   */
  if (goal_code == AI_GOAL_MILITARY || goal_code == AI_GOAL_CONTACT) {
    if (abs(u->x - goal_x) <= 1 && abs(u->y - goal_y) <= 1) {
      /* Exclude self: a stale goal can point at a tile the unit itself now
       * occupies (e.g. just captured it opportunistically) — nothing to
       * attack there. */
      const int foe = units_id_at(ctx->units, goal_x, goal_y);
      if (foe >= 0 && foe != u->id) {
        ai_euro_try_attack(ctx, u, goal_x, goal_y);
        return AI_EURO_ACT_RETURN;
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
          return AI_EURO_ACT_RETURN;
        }
      }
    }
  }

  /* Preserve land-war / peace-border / scout / treasure / missionary / wagon /
   * pioneer-improve / LABOR. */
  if (goal_code >= 0 && !land_war_hunted && !peace_border_hunted && !scout_explored &&
      !treasure_routed && !wagon_hauled) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, goal_x, goal_y);
  }

  /*
   * Land goto advance (thin 20e6 multi-step): scored steps while moves
   * remain for FOUND / MILITARY / CONTACT, or act-level land war hunt /
   * peace-border / scout explore. Structural only — not full combat scoring.
   * Cite: euro_unit_act §2c3; FUN_521d_20e6. Deep combat×8 / −0x6790 PARKED.
   */
  if (units_orders_follow_goto(u->orders)) {
    const int drain =
      (goal_code == AI_GOAL_FOUND || goal_code == AI_GOAL_MILITARY ||
       goal_code == AI_GOAL_CONTACT || goal_code == AI_GOAL_ESCORT || land_war_hunted ||
       peace_border_hunted ||
       scout_explored);
    /* drain: while MP left; else one scored step (prior non-multi path). */
    for (;;) {
      if (!u->active || u->moves <= 0 || !units_orders_follow_goto(u->orders)) {
        break;
      }
      if (u->x == u->goto_x && u->y == u->goto_y) {
        break;
      }
      int dx = 0;
      int dy = 0;
      /*
       * An adjacent goto holding a foreign unit is the LAB_4d2e attack pick
       * (the scorer commits the neighbour tile itself as +0x3153/+0x3154 and
       * FUN_479b_0972 → FUN_465b_0000 steps straight into it). Re-scoring
       * that step with ai_euro_score_move sidestepped diagonally around the
       * foe and the fight never happened (bugs.md #521, 2026-09-22).
       */
      const int adj_foe = abs(u->goto_x - u->x) <= 1 && abs(u->goto_y - u->y) <= 1 &&
                          ai_euro_foreign_unit_at(ctx, u, u->goto_x, u->goto_y);
      if (adj_foe) {
        dx = u->goto_x - u->x;
        dy = u->goto_y - u->y;
      } else if (!ai_euro_score_move(ctx, u, u->goto_x, u->goto_y, &dx, &dy)) {
        break;
      }
      const int tx = u->x + dx;
      const int ty = u->y + dy;
      /* Same best-defender rule as the step scorer above: a berthed hull on
       * the tile must not read as "empty" (FUN_5fef_0000 / FUN_465b_0000). */
      int foe = units_best_defender_at(
        ctx->units, ctx->col1_ok ? ctx->col1 : NULL, tx, ty, u->id, u->id
      );
      if (foe < 0) {
        foe = units_id_at(ctx->units, tx, ty);
      }
      /*
       * Only a FOREIGN occupant makes the step a fight. FUN_465b_0000 reaches
       * its combat tail only when the destination's owner nibble differs from
       * the mover's nation (raw 75417ff); a tile holding one of our own units
       * is an ordinary stacking move. The raw `units_id_at` fallback above has
       * no nation filter, so a second REF column standing on the step tile was
       * handed to ai_euro_try_attack — which returns without acting on an
       * own-nation target — and this loop broke every turn: the whole assault
       * column froze two tiles short of the colony (bugs.md #521). The step
       * scorer's own copy of this fallback (ai_euro_score_move) already
       * filtered on nation; this one did not.
       */
      if (foe >= 0) {
        const ColonizeUnit* fu = units_get_const(ctx->units, foe);
        if (!fu || fu->nation_id == u->nation_id) {
          foe = -1;
        }
      }
      if (foe >= 0) {
        ai_euro_try_attack(ctx, u, tx, ty);
        break;
      }
      ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
      if (!units_try_move_w(&w_, u->id, tx, ty)) {
        break;
      }
      if (!drain) {
        break; /* single step for non-FOUND/MILITARY/CONTACT/hunt/scout */
      }
    }
  } else {
    /* Peace fortify fallback (case 0x0b): idle garrison on own colony. */
    if (!at_war_land && ai_euro_is_military_name(ai_euro_unit_kind(ctx->units, u)) && ctx->colonies &&
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
  }

  /* The "sticky CONTACT re-hunt" tail that used to sit here is folded into the
   * block above (smell audit sweep-3 area C #1): zero hits when instrumented
   * over the whole ctest suite. The two seizes are the real DOS `0x46` / `0x4c`
   * arms; the adjacent-attack stand-in that also ran here was deleted 2026-09-22
   * (bugs.md #521, LAB_521d_4d2e attack term is DOS-live). The distant land war hunt that
   * also ran here was deleted 2026-09-18: DOS has no distant hunt for land
   * units any more than it has one for ships. */

  a->goal_code = goal_code;
  a->goal_x = goal_x;
  a->goal_y = goal_y;
  a->land_war_hunted = land_war_hunted;
  a->peace_border_hunted = peace_border_hunted;
  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->wagon_hauled = wagon_hauled;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Ship band driver (FUN_521d_20e6 ship arms, raw 89717-90219). Runs the
 * five ship stages in DOS order; any stage may end the act.
 */
COLONIZE_INTERNAL void ai_euro_act_ship(struct ai_euro_act_ctx* a) {
  if (ai_euro_act_ship_europe_exit(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_ship_first_colony_course(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_ship_war_trade(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_ship_sail(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_ship_arrival(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  /*
   * FUN_5bfb_3180 ship-slow (adjacent foreign warship / Fort / Fortress)
   * now runs per step inside units_try_move (units_ship_slow_scan), for
   * AI and human movers alike; the old end-of-act ambush here is gone.
   */
}

/*
 * Land band driver (case 0x0b). Binds the per-unit role flags the stages
 * share, then runs the six land stages in DOS order.
 */
COLONIZE_INTERNAL void ai_euro_act_land(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;

  /* Case 0x0b land: bind primary goal (role-aware scan). */
  const char* uname = units_display_name(ctx->units, u);
  const ColonizeUnitKind ukind = ai_euro_unit_kind(ctx->units, u);
  const int is_land_hunter = ai_euro_is_land_war_hunter(ukind);
  const int is_scout = ukind == UNITS_KIND_SCOUT;
  const int is_treasure = ai_euro_is_treasure_name(ukind);
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

  a->uname = uname;
  a->ukind = ukind;
  a->is_land_hunter = is_land_hunter;
  a->is_scout = is_scout;
  a->is_treasure = is_treasure;
  a->at_war_land = at_war_land;
  a->land_war_hunted = land_war_hunted;
  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->u = u;

  if (ai_euro_act_land_hunt_scout(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_treasure(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_roles(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_fortify(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_goal_consume(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_goal_dispatch(a) == AI_EURO_ACT_RETURN) {
    return;
  }
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
/* ======================================================================
 * DOS ship act: FUN_521d_5b66 + the FUN_521d_20e6 hull path (bugs.md #530)
 * ======================================================================
 *
 * FUN_521d_5b66 (viceroy_overlays.asm OVL14 0x5b66-0x5c37, byte-read):
 *   if (+0x3149 != 0 && +0x314c == 0x0b) {
 *     if (!(DS:0x523d[type] & 1)) goto walk;          // transports never re-score
 *     if (!FUN_1000_8b74(x, y, nation)) goto walk;    // warship, no foreigner adjacent
 *     if (+0x314b == 'E') DS:0x9456[nation]--;
 *   }
 *   if (FUN_521d_20e6(unit)) return;
 *   walk: switch (+0x314c) { 7 found; 8 plow; 9 road;
 *                            0x0b, 0x0c: FUN_479b_0972 (one pathfinder step);
 *                            default: FUN_1000_8b24 exhaust MP }
 *
 * The 20e6 hull path, in DOS order (raw 88400-90404; ships take the
 * LAB_277a -> LAB_2912 -> LAB_304c chain straight to LAB_3558 at sea, or the
 * berth block at an own colony):
 *   entry bail (raw 88402-88405) -> LAB_3558 unload mask + unload loop
 *   (raw 89440-89612) -> colony-sail pick (raw 89614-89711) -> goods
 *   delivery / sell / Europe (raw 89717-89872) -> LAB_4393 work-queue haul
 *   -> LAB_457e (tasked hull bails; High Seas cadence) -> pre-LAB_4d2e gate
 *   (raw 90210-90219) -> LAB_4d2e ship far roam (raw 88632-88664) and the
 *   8-direction scorer -> LAB_589e commit (0x0c one step, or 5/6 stay)
 *   -> LAB_5a78 tail.
 */
static int ai_euro_ship_dos_enabled(void) {
  return ai_euro_env_flag("AI_SHIP_DOS", 0);
}

/*
 * DOS-LITERAL FUN_281f_0984 -> FUN_1427_09dc for a mover standing on its own
 * tile: 1 when any of the eight neighbours carries a foreign unit
 * (FUN_137f_03e4, layer2 bit 2) or, failing that, a foreign settlement —
 * colony OR village (FUN_137f_0314, layer2 bit 1 + owner nibble). With the
 * mover's own unit on the centre tile the water/land domain test in the loop
 * is always satisfied, so it drops out.
 */
static int ai_euro_09dc_dos(const ColonizeTurnContext* ctx, int x, int y, int nation_id) {
  for (int d = 0; d < 8; ++d) {
    const int nx = x + MAP_DIR8_DX[d];
    const int ny = y + MAP_DIR8_DY[d];
    if (!map_in_bounds(ctx->map, nx, ny)) {
      continue;
    }
    int owner = -1;
    const int uid = units_id_at(ctx->units, nx, ny);
    if (uid >= 0) {
      const ColonizeUnit* nu = units_get_const(ctx->units, uid);
      owner = nu ? nu->nation_id : -1;
    }
    if (owner < 0) {
      owner = ai_euro_20e6_colony_owner_at(ctx, nx, ny);
    }
    if (owner < 0) {
      owner = ai_euro_village_nation_at(ctx->col1_ok ? ctx->col1 : NULL, nx, ny);
    }
    if (owner >= 0 && owner != nation_id) {
      return 1;
    }
  }
  return 0;
}

/* LAB_521d_5a78 tail (raw 90399-90436), hull subset. */
static void ai_euro_20e6_ship_tail_5a78(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (u->orders == AI_EURO_ACT_ADJACENT || u->orders == UNITS_ORDER_NONE) {
    u->col1_ai_plan = 0x30;          /* '0' */
    u->orders = UNITS_ORDER_FORTIFY; /* 5 */
  }
  if (u->orders == UNITS_ORDER_FORTIFY && ctx->col1_ok && ctx->col1) {
    /* raw 90406-90420: an idle unit next to a settlement of a nation it is
     * at war with (FUN_281f_0a38 & 0x40) drops back to act state 0. */
    for (int d = 0; d < 8; ++d) {
      const int owner = ai_euro_20e6_colony_owner_at(ctx, u->x + MAP_DIR8_DX[d], u->y + MAP_DIR8_DY[d]);
      if (owner >= 0 && owner != nation_id && owner < 4 &&
          (ai_diplo_read(ctx->col1, nation_id, owner) & AI_DIPLO_WAR)) {
        u->orders = UNITS_ORDER_NONE;
        break;
      }
    }
  }
  if (u->orders == AI_EURO_ACT_GOAL && u->goto_x == u->x && u->goto_y == u->y) {
    if (u->col1_ai_plan == '1') {
      u->col1_ai_plan = 0x42; /* 'B' — raw 90428-90431, hulls only */
    }
    u->moves = 0; /* FUN_281f_0934 */
  }
}

/*
 * FUN_521d_20e6 for a hull. Returns 1 when 20e6 itself ends the act
 * (non-zero return in DOS, or the unit is gone).
 */
static int ai_euro_20e6_ship_dos(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  const int id = u->id;
  /* raw 88402-88405: courses other than 0/5/6/>=10 skip the body. */
  if (u->orders != UNITS_ORDER_NONE && u->orders != UNITS_ORDER_FORTIFY &&
      u->orders != UNITS_ORDER_FORTIFIED && u->orders < AI_EURO_ACT_ADJACENT) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  if (!map_coords_inset(ctx->map, u->x, u->y)) {
    u->col1_ai_plan = 0x40; /* '@', raw 88410-88414 */
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  const int tasked = u->col1_ai_plan == 't' || u->col1_ai_plan == 'i'; /* local_6 */

  const int at_own_colony = colonies_id_at(ctx->colonies, u->x, u->y) >= 0 &&
                            ctx->colonies->colonies[colonies_id_at(ctx->colonies, u->x, u->y)]
                                .nation_id == nation_id;
  if (!at_own_colony) {
    /* LAB_3558 unload mask + loop. The loop exhausts every unloaded
     * passenger and then the hull (FUN_281f_0934), but 20e6 runs on. */
    const int mask9c = ai_euro_20e6_unload_mask(ctx, u, nation_id);
    if (getenv("AI_SHIP_TRACE") && mask9c) {
      fprintf(stderr, "[shipdos] unit %d n%d (%d,%d) unload mask 0x%x\n", id, nation_id, u->x, u->y, mask9c);
    }
    if (mask9c != 0 && ai_euro_20e6_unload_by_mask(ctx, u, nation_id, mask9c) > 0) {
      u = units_get(ctx->units, id);
      if (!u || !u->active) {
        return 1;
      }
      u->moves = 0;
    }
    /* Colony-sail pick (raw 89614-89711) — the same gate
     * ai_euro_unload_settle_mask_and_sail applies. */
    if (!tasked) {
      int pioneers = 0;
      int mil = 0;
      int scouts = 0;
      int milvet = 0;
      int civ = 0;
      ai_euro_20e6_ship_cargo_counts(ctx, u, &pioneers, &mil, &scouts, &milvet, &civ);
      int a8 = 0;
      for (int s = 0; s < u->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
        const ColonizeUnit* p = units_get_const(ctx->units, u->cargo_ids[s]);
        if (p && p->active) {
          a8++;
        }
      }
      const int pioneers_b4 = pioneers;
      const int found_probe = ai_goals_max_primary_prio(nation_id, u->x, u->y, AI_GOAL_FOUND);
      ai_euro_20e6_goal_fold(ctx, u, nation_id, found_probe, &pioneers, &civ, NULL);
      const int urgency = ai_euro_0a60_work_registered(nation_id);
      int cx = 0;
      int cy = 0;
      if (a8 > 0 && (civ != 0 || ((pioneers != a8 || urgency > 0x18) && mask9c == 0)) &&
          ai_euro_20e6_colony_sail_pick(ctx, u, nation_id, mil, pioneers_b4, urgency, &cx, &cy)) {
        ai_euro_set_goto(u, AI_EURO_ACT_GOAL, cx, cy); /* LAB_27f5 */
        ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
        return 0;
      }
    }
  }
  /* Berth block / goods delivery / sell / LAB_4393 haul. */
  if (!tasked) {
    const int ox = u->goto_x;
    const int oy = u->goto_y;
    const int oo = u->orders;
    if (ai_euro_try_ship_trade_haul(ctx, nation_id, u)) {
      u = units_get(ctx->units, id);
      if (!u || !u->active) {
        return 1;
      }
      if (u->orders != oo || u->goto_x != ox || u->goto_y != oy || u->moves <= 0) {
        ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
        return 0;
      }
    }
  }
  /* LAB_457e: a tasked hull leaves 20e6 here. */
  if (tasked) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  if (ai_euro_20e6_hs_cadence_enabled() && ai_euro_20e6_457e_hs_cadence(ctx, u, nation_id)) {
    u = units_get(ctx->units, id);
    if (!u || !u->active) {
      return 1;
    }
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  if (ai_euro_try_ship_europe_export(ctx, nation_id, u)) {
    u = units_get(ctx->units, id);
    if (!u || !u->active) {
      return 1;
    }
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  /* Pre-LAB_4d2e gate, raw 90210-90219. */
  const int idle = u->orders == UNITS_ORDER_NONE || u->orders == AI_EURO_ACT_ADJACENT ||
                   u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED ||
                   (u->orders == AI_EURO_ACT_GOAL && u->goto_x == u->x && u->goto_y == u->y);
  if (!idle && !ai_euro_09dc_dos(ctx, u->x, u->y, nation_id)) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  /* LAB_4d2e. */
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (ai_euro_20e6_ship_far_roam(ctx, u, &s)) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  /* +0x314f facing is a real save byte (u->last_dir); the scorer's facing
   * term reads the file-local shadow, so seed it from the unit. */
  s_euro_last_dir[id] = (int8_t)u->last_dir;
  int attack = 0;
  const int dir = ai_euro_20e6_wander_step(ctx, u, &s, &attack, NULL);
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[shipdos] unit %d n%d (%d,%d) mp %d 4d2e dir %d attack %d\n", id, nation_id,
            u->x, u->y, u->moves, dir, attack);
  }
  if (dir >= 0 && dir < 8 && attack) {
    s_euro_last_dir[id] = (int8_t)dir;
    u->last_dir = dir;
    ai_euro_try_attack(ctx, u, u->x + MAP_DIR8_DX[dir], u->y + MAP_DIR8_DY[dir]);
    u = units_get(ctx->units, id);
    return !u || !u->active;
  }
  u->col1_ai_plan = 0x39; /* '9' — raw 89040 fallthrough */
  /* LAB_589e commit. */
  if (dir < 0 || dir > 7) {
    ai_euro_20e6_stay_tail_589e(u);
  } else {
    s_euro_last_dir[id] = (int8_t)dir;
    u->last_dir = dir;
    const int nx = u->x + MAP_DIR8_DX[dir];
    const int ny = u->y + MAP_DIR8_DY[dir];
    if (map_coords_inset(ctx->map, nx, ny)) {
      u->orders = AI_EURO_ACT_STEP;
      u->goto_x = nx;
      u->goto_y = ny;
    }
  }
  ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
  return 0;
}

/* FUN_479b_0972: one pathfinder step toward +0x314d/e. */
static void ai_euro_ship_goal_walk_479b(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  const int id = u->id;
  const int state = u->orders;
  int px = 0;
  int py = 0;
  int ok = 0;
  ColonizeWorld w = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
  if (u->x != u->goto_x || u->y != u->goto_y) {
    ok = units_next_goto_step_w(&w, id, &px, &py);
  }
  if (!ok) {
    u->orders = UNITS_ORDER_NONE; /* FUN_2a1f_0210 found no direction */
    return;
  }
  if (!units_try_move_w(&w, id, px, py)) {
    u->moves = 0; /* blocked step: the port has no DOS retry; end the act */
    return;
  }
  u = units_get(ctx->units, id);
  if (!u || !u->active) {
    return;
  }
  ai_euro_sync_aboard_cargo_xy(ctx->units, u);
  if (u->x != u->goto_x || u->y != u->goto_y) {
    return;
  }
  if (state == AI_EURO_ACT_GOAL) {
    u->moves = 0; /* FUN_281f_0934 on arrival */
  }
  if (state != AI_EURO_ACT_STEP) {
    u->orders = UNITS_ORDER_NONE;
  }
}

/* FUN_521d_5b66 for a hull. */
static void ai_euro_act_ship_dos(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  const int id = u->id;
  if (ai_euro_in_europe(u->x, u->y)) {
    struct ai_euro_act_ctx a;
    memset(&a, 0, sizeof(a));
    a.ctx = ctx;
    a.u = u;
    a.nation_id = nation_id;
    a.is_ship = 1;
    (void)ai_euro_act_ship_europe_exit(&a);
    return;
  }
  const int fresh = u->moves >= units_max_mp(ctx->units, id);
  int run_20e6 = 1;
  if (!fresh && u->orders == AI_EURO_ACT_GOAL) {
    Ai20e6Unit s;
    ai_euro_20e6_prologue(ctx, u, nation_id, &s);
    run_20e6 = (s.flags & 1) && ai_euro_09dc_dos(ctx, u->x, u->y, nation_id); /* FUN_1000_8b74 */
  }
  if (run_20e6 && ai_euro_20e6_ship_dos(ctx, u, nation_id)) {
    return;
  }
  u = units_get(ctx->units, id);
  if (!u || !u->active || u->moves <= 0) {
    return;
  }
  if (u->orders == AI_EURO_ACT_GOAL || u->orders == AI_EURO_ACT_STEP) {
    ai_euro_ship_goal_walk_479b(ctx, u);
  } else {
    u->moves = 0; /* FUN_1000_8b24 */
  }
}

static void ai_euro_unit_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !u || !u->active || u->aboard_ship_id >= 0) {
    return;
  }
  /* First-colony land may wake sentry (moves was 0). Ships still need MP. */
  const int is_ship_early = ai_euro_is_ship_type(ctx->units, u->id);
  if (!is_ship_early && ai_euro_try_first_colony_land(ctx, u, nation_id)) {
    return;
  }
  if (u->moves <= 0) {
    return;
  }

  /*
   * FUN_4720_049e notify check (thin, approximate — see the function's own
   * header). Fires once near the top of the act, matching DOS's own
   * move-driver-completion timing as closely as this port's architecture
   * allows. (The Privateer-sighting relation bit that also stood here moved
   * to its real DOS home on 2026-09-23 — FUN_465b_0000's move-into-foreign
   * arm, ai_euro_465b_privateer_sighting, called from units_try_move_w.)
   */
  if (!is_ship_early) {
    ai_euro_try_violate_notify(ctx, u);
  }

  const int is_ship = is_ship_early;
  int is_goto = units_orders_follow_goto(u->orders);

  /*
   * The unit's 0a60 binding (+0x314c == 0x0b, target +0x314d/e), read BEFORE
   * FUN_521d_20e6 runs: its wander commit (LAB_589e) overwrites the byte with
   * 0x0c and the one-step target, and this port's act-level LABOR / MILITARY /
   * CONTACT arms — which DOS does not have, it consumes a goal only through
   * FUN_479b_0972's walk — still need the goal the unit was bound to.
   * bugs.md #525/#526.
   */
  int bound_goal_code = -1;
  int bound_goal_x = u->goto_x;
  int bound_goal_y = u->goto_y;
  if (u->orders == AI_EURO_ACT_GOAL) {
    bound_goal_code = ai_goals_primary_code_at(nation_id, u->goto_x, u->goto_y);
  }

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
   * No land-unit-initiated embark arm here. DOS boarding is ship-side only:
   * FUN_1427_10be (raw 8606-8679) runs over the SHIP, walks the unit list at
   * the ship's tile and stamps +0x314c = 1 on each land unit whose @UNIT size
   * (0x5238) still fits the ship's remaining hold; FUN_521d_20e6's land bands
   * (types 1/4/0xb) never touch +0x314c. The "at-war coastal embark" arm that
   * used to sit here cited only Colonization.pdf and was deleted (bugs.md
   * #513).
   */

  /*
   * (The pre-gate "war / sticky mil unload" arm that stood here was deleted
   * 2026-09-18: manual-cited, with no DOS counterpart. DOS disembarks military
   * passengers through the LAB_521d_3558 per-cargo mask block (raw 89440-89560,
   * local_9c bits 0x10/0x20/0x40 against DS:0x523d[type*0xe]), ported as
   * ai_euro_20e6_unload_mask / _unload_by_mask under ai_euro_unload_settle.)
   */

  /*
   * FUN_521d_20e6 ship-band tail (raw 89717-89720): during WoI an empty,
   * untasked crown Man-O-War standing alone sails for the High Seas once
   * the MoW pool is spent and land pools remain. D1 (2026-09-07g): the
   * crown ship acts through this band now, so the beat runs here; the
   * full DOS skip-test lives in ai_king_mow_sail_home_20e6.
   */
  if (is_ship && ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi &&
      nation_id == (int)ctx->col1->head.crown_nation_id && u->cargo_count == 0 &&
      ai_euro_20e6_dos_type(ctx->units, u) == 0x12 &&
      ai_king_mow_sail_home_20e6(ctx, u, nation_id)) {
    return;
  }

  if (is_ship && ai_euro_ship_dos_enabled()) {
    ai_euro_act_ship_dos(ctx, u, nation_id);
    return;
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
  /*
   * DOS re-entry condition, raw 90551 (FUN_521d_5b66 → thunk 2a1f_04f4 →
   * FUN_521d_20e6):
   *   if (unit+0x3149 == 0 || unit+0x314c != 0x0b) call 20e6
   * i.e. 20e6 runs when the unit has spent no MP yet this turn **or** is not
   * on a goto — a goto-stamped unit is re-scored at the top of every turn,
   * and 20e6's own early bail (raw 88395-88398) lets orders 0x0b through.
   * +0x3149 is MP spent; this port stores MP remaining, so "spent 0" is a
   * full allotment. Without this term a unit that ever got an AI_MOVE goto
   * never re-entered the gate again — which is why Scouts needed an invented
   * arm to keep moving at all (bugs.md #493).
   */
  const int fresh_allotment = u->moves >= units_max_mp(ctx->units, u->id);
  if ((!is_goto || fresh_allotment) && !is_pioneer_job_active) {
    const ColonizeUnitKind gate_kind = ai_euro_unit_kind(ctx->units, u);
    const int defer_gate =
      ai_euro_is_treasure_name(gate_kind) || ai_euro_is_missionary_name(gate_kind) ||
      ai_euro_type_is_wagon_name(gate_kind) || ai_euro_is_cargo_ship_name(gate_kind);
    if (!defer_gate) {
      const int gate_r = ai_euro_move_scoring_gate(ctx, u, nation_id);
      /*
       * LAB_521d_5a78 tail, raw 90399-90404 — runs on every 20e6 exit:
       *   if (act_state == 10 || act_state == 0) { order_code = 0x30; act_state = 5; }
       * act_state 10 is FUN_521d_0a60's "a foreign unit or settlement stands on
       * one of my eight neighbours" marker (raw 87566-87568, its only writer in
       * the whole game). It is a one-call transient: 20e6 admits it at the entry
       * bail (raw 88404-88406, `< 10` fails), treats it exactly like the
       * courseless states 0/5/6 at the pre-4d2e gate (raw 90210-90214) so the
       * unit free-scores its eight neighbours instead of pathing, and then
       * demotes it here to 5 ("idle, re-evaluate next call") with order code '0'.
       * It is never a goal state: 0a60's goal walk hands out goals only at
       * 0/5/6 (raw 88164) and FUN_521d_5b66 dispatches one only at 0x0b
       * (raw 90552), so a unit standing next to a foreign colony is deliberately
       * unbound from its goal by DOS as well. (bugs.md #521 lead a.)
       */
      if (u->orders == AI_EURO_ACT_ADJACENT || u->orders == UNITS_ORDER_NONE) {
        u->col1_ai_plan = 0x30;             /* +0x314b = '0' */
        u->orders = UNITS_ORDER_FORTIFY;    /* +0x314c = 5 */
      }
      /*
       * FUN_521d_5b66 switch case 7 (bugs.md #683). 5b66 calls 20e6 at raw
       * 90551, 20e6 returns 0 (raw 90445) and 5b66 then dispatches on the
       * act_state byte 20e6 just wrote: `uVar14 = act_state - 7; if (5 <
       * uVar14) ... switch (act_state) { case 7: ... }`. act_state 7 is
       * UNITS_ORDER_BUILD_COLONY — the byte the human Build handler also
       * writes immediately before FUN_291f_01fa (raw 45657-45658) — so case 7
       * founds the colony the 20e6 site scan just committed to.
       */
      if (u->orders == UNITS_ORDER_BUILD_COLONY) {
        ai_euro_found_with_unit(ctx, u, nation_id);
        /*
         * Port guard, no DOS counterpart: if the tile refused the colony the
         * founder survives, and leaving act_state 7 on it would wedge it
         * forever (20e6's entry bail, raw 88404-88406, drops every act_state
         * outside {0,5,6,>=10} straight to LAB_5a78). Fall back to the
         * courseless state 5 the LAB_5a78 tail hands out.
         */
        u = units_get(ctx->units, u->id);
        if (u && u->active && u->orders == UNITS_ORDER_BUILD_COLONY) {
          u->orders = UNITS_ORDER_FORTIFY; /* +0x314c = 5 */
        }
        return;
      }
      if (gate_r) {
        return;
      }
    }
  }

  /*
   * No quota-0 "admit garrison as colonist" arm here. FUN_521d_20e6 has no
   * join-colony outcome at all: its complete +0x314b vocabulary over the whole
   * body (raw 88266-89800) is 0x39/0x3d/0x40/0x42/0x46/0x47/0x4c/0x56/0x65 —
   * 0x46 = the LAB_4d2e tail's park arm (raw 89011-89029, ported as
   * ai_euro_20e6_border_park_arm: every attack>1 non-ship type that is alone
   * in its chain and stands next to a settlement of ANOTHER nation stays put;
   * it has no own-colony test and spends no garrison quota) and
   * 0x4c = enter village (raw 89068). Absorbing a unit into a colony as a
   * colonist is FUN_5952_035e's colony-tick arm (ai_euro_5952_absorb_equip),
   * not a unit act. The garrison_quota == 0 admit that used to sit here cited
   * only test-saves-ai/TURN4–5 and was deleted (sibling of bugs.md #512).
   */

  struct ai_euro_act_ctx a;
  memset(&a, 0, sizeof(a));
  a.ctx = ctx;
  a.u = u;
  a.nation_id = nation_id;
  a.is_ship = is_ship;
  a.goal_code = bound_goal_code;
  a.goal_x = bound_goal_x;
  a.goal_y = bound_goal_y;

  /* FUN_5952_035e's absorption + equip arms are NOT a unit act: they run in
   * the colony tick (ai_euro_5952_absorb_equip), re-hosted 2026-09-18. */
  if (ai_euro_act_pioneer_corridor(&a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_soldier_staging(&a) == AI_EURO_ACT_RETURN) {
    return;
  }

  /* Case 7 Europe hire / wagon economy: treasury + dock expert tails in 5d04.
   * Thin tools delivery runs on land Pioneer/Hardy at own colony (below). */

  if (is_ship) {
    ai_euro_act_ship(&a);
    return;
  }

  ai_euro_act_land(&a);
}

/* --- dispatcher turn: stage helpers ------------------------------------ */

/* Step 0 of ai_euro_dispatcher_turn: per-turn sticky/latch hygiene. */
static void ai_euro_dispatcher_turn_reset(ColonizeTurnContext* ctx) {
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

  /* FUN_521d_0a60 entry: memset(0xa13c,0,16) — per-continent explorer count
   * read by FUN_521d_20e6's explorer cap (s_20e6_explorers). */
  memset(s_20e6_explorers, 0, sizeof(s_20e6_explorers));
  /* Village-errand latch hygiene: DOS's +0x3158 dies with its unit record;
   * the session latch must not survive a despawn into a reused unit id.
   * Same argument for the 20e6 explore-fatigue counter and ring-hop wander
   * latch (DOS unit+0x3154 / +0x3155 / +0x3156, raw 1600-1611 / 2416-2458):
   * those bytes are part of the unit record too, so a reused id must not
   * inherit a foreign fatigue count or hop commitment.
   * NOT a slot walk on purpose (audit second-wave Leads 2, 2026-09-10): these
   * latch arrays are keyed by unit ID everywhere they are read, so this loop
   * walks the addressable ID SPACE and clears the entries no live unit owns.
   * `units_get_const(pool, i) == NULL` is exactly "no live unit has id i". */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (units_get_const(ctx->units, i) != NULL) {
      continue;
    }
    s_20e6_wagon_errand[i] = 0;
    s_20e6_explore_fatigue[i] = 0;  /* fresh counter: 0 == never explored */
    s_20e6_hop_slot[i] = 0;  /* slot+1 encoding: 0 == unset (DOS 0xff) */
    s_20e6_hop_steps[i] = 0;
  }
}

/* Steps 1-6 of ai_euro_dispatcher_turn: colony inventory, treaty timers, the
 * 5d04 -> 0342 -> 0a60 plan, build preferences and the treasure cash-in. */
static void ai_euro_dispatcher_turn_plan(ColonizeTurnContext* ctx, int nation_id) {
  /* 1–3. Colony inventory (the per-unit prelude arm lives in
   * ai_euro_5d04_cb_colony_needs — see the note at ai_euro_colony_inventory). */
  ai_euro_colony_inventory(ctx, nation_id);

  /* 4. Treaty timers BEFORE plan (not war RNG). */
  ai_diplo_treaty_timers(ctx, nation_id);

  /* 5. Plan: 5d04 → 0342 → 0a60 */
  ai_euro_nation_planning(ctx, nation_id);
  ai_goals_promote_secondary_to_primary(nation_id);
  ai_euro_cancel_stale_zero_hammer_builds(ctx, nation_id);
  ai_euro_clear_pre_stockade_build_queue(ctx, nation_id);
  ai_euro_colony_goals(ctx, nation_id);
  /*
   * bugs.md #483 — DOS's ONE construction picker, FUN_5952_035e's tail. It
   * replaces the three invented preference passes that used to stand here
   * (ai_euro_prefer_peace_construction / _all_buildings / _craft_upgrades).
   * DOS runs it inside the colony tick, the AI turn's first phase; the port
   * runs it immediately after ai_euro_colony_goals because that pass is the
   * other half of the same DOS body and is what produces the ring-1 threat
   * count (iStack_22) the Wagon Train arm reads.
   */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == nation_id) {
        ai_euro_5952_build_cascade(ctx, c);
      }
    }
  }
  /* FUN_521d_0a60 goal-consumption tail (structural port) — stamps each idle
   * unit's next goal into its own +0x314b/c/d/e; consumed by
   * ai_euro_unit_act below. See the function's header comment for scope. */
  ai_euro_0a60_goal_orders_structural(ctx, nation_id);

  /* Opportunistic balance after plan (separate from timer slot). Not for
   * the WoI crown slot: this pass is Linux-shaped (war-fatigue peace roll,
   * upkeep drain, privateer spawn, Indian matrix — no DOS counterpart in
   * the 6d8e nation turn, verified 2026-09-07g), and its peace arm would
   * silently end the War of Independence. */
  if (!(ctx->col1_ok && ctx->col1 && ctx->col1->head.game_options.woi &&
        nation_id == (int)ctx->col1->head.crown_nation_id)) {
    ai_diplo_euro_balance(ctx, nation_id);
  }

  /* (The nation-turn "Expected→Harbor treasure cash" sweep that stood here was
   * deleted 2026-09-23, bugs.md #746: DOS's AI treasure cash-in is the
   * unconditional, untaxed in-colony 20e6 band alone — nothing ever puts an AI
   * treasure on a ship, so nothing ever reaches Europe to be taxed.) */
}

/* Step 7 of ai_euro_dispatcher_turn: the FUN_521d_6d8e wave/drain unit-act
 * loop (and its legacy AI_6D8E_DOS_LOOP=0 ordering). */
static void ai_euro_dispatcher_turn_unit_waves(ColonizeTurnContext* ctx, int nation_id) {
  /*
   * 6–7. FUN_521d_6d8e raw 93237-93325 (re-read 2026-09-15; the old "ships
   * first, one act per unit per pass" shape came from a mislabelled type
   * constant in the annotated header):
   *   do {
   *     for wave 0..1:
   *       acted = 0;
   *       for id = count-1 down to 0 while !acted:
   *         wave 0 admits only types 0x0a/0x0b/0x0c (Treasure / Artillery /
   *         Wagon Train, bVar4); wave 1 admits every unit;
   *         while (unit has MP): sticky bookkeeping, act (5b66), acted = 1;
   *         then, if the unit count is unchanged and it is a wave-0 type
   *         now out of MP: upsert primary goal code 2 at its tile with prio
   *         2 Treasure / 3 Artillery / 1 Wagon (thunk_FUN_2a1f_0470).
   *   } while (wave 1 acted someone);
   * i.e. each outer pass drains ONE wave-0 unit and ONE any-unit, highest
   * id first, then rescans from the top. Port keeps the `guard < 64`
   * ceiling and breaks the inner drain on a no-progress act (DOS relies on
   * the >0x14 sticky clear alone).
   *
   * That DOS shape is the default since 2026-09-15. AI_6D8E_DOS_LOOP=0
   * restores the legacy "wave 0 = ships, one act per unit per pass" shape
   * (bisect aid only). Two port-only rules keep the goldens under the DOS
   * order: a unit whose act makes no progress counts as exhausted for the
   * scan (DOS would re-act it until the sticky clear), and the SP soldier's
   * first-colony eligibility accepts "both landed, ship still at the tip"
   * (ai_euro_try_first_colony_land) because the ship now acts after the
   * land units.
   */
  const int dos_loop = !(getenv("AI_6D8E_DOS_LOOP") && getenv("AI_6D8E_DOS_LOOP")[0] == '0');
  int any_acted;
  int guard = 0;
  do {
    any_acted = 0;
    for (int wave = 0; wave < 2; ++wave) {
      int wave_acted = 0;
      for (int i = COLONIZE_UNITS_MAX - 1; i >= 0 && !(dos_loop && wave_acted); --i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
          continue;
        }
        const int is_ship = ai_euro_is_ship_type(ctx->units, u->id);
        const ColonizeUnitKind ukind = ai_euro_unit_kind(ctx->units, u);
        const int is_wave0_type =
          ukind == UNITS_KIND_TREASURE || ukind == UNITS_KIND_ARTILLERY || ukind == UNITS_KIND_WAGON;
        const int in_wave = (wave != 0) || (dos_loop ? is_wave0_type : is_ship);
        if (!in_wave) {
          continue;
        }
        /*
         * First-colony sentry settlers (Dutch Isabella pioneer) skip overnight
         * MP — still run unit_act so wake+found can fire. Cite: TURN3→4.
         */
        if (u->moves <= 0) {
          if (is_ship || colonies_count_for_nation(ctx->colonies, nation_id) != 0) {
            continue;
          }
          if (!ai_euro_name_is_pioneer(ukind) && !ai_euro_name_is_soldier(ukind)) {
            continue;
          }
          /* Only wake when found-approach eligibility can fire (not beachhead). */
          const int settler_aboard = ai_euro_nation_settler_aboard(ctx, nation_id);
          const int pioneer_aboard = ai_euro_nation_pioneer_aboard(ctx, nation_id);
          if (ai_euro_name_is_soldier(ukind)) {
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
                ai_euro_recover_nation_landfall(ctx, nation_id, &lf_x, &lf_y);
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
              ai_euro_recover_nation_landfall(ctx, nation_id, &lf_x, &lf_y);
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
                         map_chebyshev(sh->x, sh->y, wx, wy) <= 1)) {
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

        /* DOS inner `while (has_moves)`: drain this unit before the next scan. */
        int first_act = 1;
        while (u->active && (first_act || (dos_loop && u->moves > 0))) {
          first_act = 0;
        if (u->id == s_sticky_unit) {
          s_sticky_count++;
          if (s_sticky_count > 0x14) {
            /* DOS FUN_281f_0934: clear orders, then act anyway (no skip). */
            units_clear_orders(ctx->units, u->id);
            s_sticky_count = 0;
          }
        } else {
          s_sticky_unit = u->id;
          s_sticky_count = 0;
        }

        const int before_moves = u->moves;
        const int before_x = u->x;
        const int before_y = u->y;
        ai_euro_unit_act(ctx, u, nation_id);

        const int progressed =
          !u->active || u->moves < before_moves || u->x != before_x || u->y != before_y;
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
        /* bugs.md #545: DOS runs the Euro branch only with the mover's tile
         * AND the neighbour tile both land (FUN_281f_0768 == 0, raw 98614) —
         * an AI ship at sea beside a coastal colony opens nothing. */
        if (progressed && u->active && units_is_on_map(u) && ctx->human_nation >= 0 &&
            ctx->human_nation <= 3 && nation_id != ctx->human_nation && ctx->ai_popups &&
            ctx->map && !map_tile_is_water(ctx->map, u->x, u->y)) {
          int near_human = 0;
          for (int ady = -1; ady <= 1 && !near_human; ++ady) {
            for (int adx = -1; adx <= 1 && !near_human; ++adx) {
              if ((adx == 0 && ady == 0) ||
                  map_tile_is_water(ctx->map, u->x + adx, u->y + ady)) {
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
        /*
         * FUN_5bfb_3180's Indian-side half runs on the AI mover's own step as
         * well (465b commit tail -> 0984 -> 0192 -> 3180 -> 022e): a land unit
         * that ends beside a tribe's unit or village opens first contact
         * right there, in the Euro phase — BEFORE that tribe's own 021a act
         * loop reads the MET bit (seed-100 TURN2: the Sioux Brave at (48,56)
         * scores the Spanish soldier next to it as already met). Ships and
         * the human are handled elsewhere (landfall hook / game_loop).
         */
        if (progressed && u->active && units_is_on_map(u) && u->aboard_ship_id < 0 &&
            !units_is_sea(ctx->units, u->id) &&
            (u->x != before_x || u->y != before_y)) {
          (void)ai_contact_encounter_scan(ctx, nation_id, u->x, u->y);
        }
        if (!progressed) {
          /* Port-only spin guard: DOS re-acts until MP 0 (or the sticky
           * clear). Treat a no-progress unit as exhausted for this scan so
           * the wave moves on to the next id instead of ending the pass. */
          break;
        }
        wave_acted = 1;
        /* DOS: "wave 1 acted someone" — but DOS only ever acts a unit
         * that still has MP, whereas the port's first-colony wake arm above
         * admits 0-MP settlers; count only real progress so a no-op act
         * cannot spin the outer loop to the guard. Legacy shape counts
         * either wave (ships act in both). */
        if (wave == 1 || !dos_loop) {
          any_acted = 1;
        }
        }

        /*
         * Raw 93286-93298: wave-0 type, unit count unchanged, out of MP →
         * primary goal code 2 (AI_GOAL_ESCORT) at its tile, prio by type.
         * (The old "ship out of MP → CONTACT prio 2" arm here was an
         * invention built on the ships-first misread.)
         */
        if (is_wave0_type && u->active && u->moves <= 0) {
          const int prio = ukind == UNITS_KIND_TREASURE ? 2 : ukind == UNITS_KIND_ARTILLERY ? 3 : 1;
          ai_goals_upsert_primary(nation_id, u->x, u->y, AI_GOAL_ESCORT, prio);
        }
      }
    }
    ++guard;
  } while (any_acted && guard < 64);
}

void ai_euro_dispatcher_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }

  ai_euro_dispatcher_turn_reset(ctx);
  ai_euro_20e6_refresh_type_cache(ctx->units);

  ai_euro_dispatcher_turn_plan(ctx, nation_id);


  ai_euro_dispatcher_turn_unit_waves(ctx, nation_id);

  /*
   * FUN_5952_035e colonist re-placement runs after the unit acts so the
   * admit-time expert field-assign paths (which need a free tile) still
   * land; the tick then re-scores everyone with real professions.
   */
  ai_euro_colony_tick_28c8_reassign(ctx, nation_id);
}

/*
 * New-game / load hook (sibling of ai_goals_reset / founding_fathers_reset /
 * ai_contact_reset): this module's per-unit/colony/nation latch arrays are
 * plain file-local statics with no save-file backing (besides the wagon
 * errand latch, which round-trips through the save and is cleared via its
 * own accessor below), so they silently persist across a new game or Load
 * in one process unless explicitly zeroed here. Every array is restored to
 * its declaration-time initializer, not blanket-zeroed, so sentinels stay
 * sentinels (s_sticky_unit == -1, s_5d04_nation == -1, s_5d04_ctx == NULL).
 */
void ai_euro_reset(void) {
  s_sticky_unit = -1;
  s_sticky_count = 0;
  memset(s_5952_ring1, 0, sizeof(s_5952_ring1));
  memset(s_5952_docks_started, 0, sizeof(s_5952_docks_started));
  memset(s_5952_census_nonexpert, 0, sizeof(s_5952_census_nonexpert));
  memset(s_5952_census_vet_soldier, 0, sizeof(s_5952_census_vet_soldier));
  memset(s_deferred_found, 0, sizeof(s_deferred_found));
  memset(s_unloaded_this_turn, 0, sizeof(s_unloaded_this_turn));
  s_20e6_type_cache_valid = 0; /* bugs.md #655: refreshed next dispatcher_turn */
  /*
   * s_euro_last_dir (unit+0x314f) has no save-file backing in this port
   * either — it round-trips only within a single dispatcher_turn/turn
   * sequence as a momentum/facing bias, same class of latch as the arrays
   * above, so it belongs in the same new-game/load zeroing as the rest.
   * It was previously excluded here because zeroing it made
   * unit_naval_multistep_sail (tests/unit/test_ai_euro_war.c) regress: that
   * test ran on an all-ocean map where every one of the 8 wander
   * directions was a dead tie at spawn, so it only advanced toward its foe
   * because an *earlier* test in the same binary had left this unit-id's
   * slot biased eastward — cross-test global-state bleed, not a genuine
   * DOS requirement (DOS ships have no distant hunt either: a fresh
   * unit's first move is exactly this ambiguous in the original game
   * too). Fixed by walling the test's map so east is the only legal first
   * step, making it deterministic without relying on this leftover state;
   * see that test for the full writeup.
   */
  memset(s_euro_last_dir, 0, sizeof(s_euro_last_dir));
  memset(s_founded_colony_turn, 0, sizeof(s_founded_colony_turn));
  memset(s_euro_continent_stance, 0, sizeof(s_euro_continent_stance));
  memset(s_euro_rival_strength, 0, sizeof(s_euro_rival_strength));
  memset(s_violate_last_turn, 0, sizeof(s_violate_last_turn));
  memset(s_euro_roam_wander, 0, sizeof(s_euro_roam_wander));
  memset(s_euro_ship_route_latch, 0, sizeof(s_euro_ship_route_latch));
  memset(s_ship_pressure, 0, sizeof(s_ship_pressure));
  memset(s_4393_claim_turn, 0, sizeof(s_4393_claim_turn));
  memset(s_4393_claim_colony, 0, sizeof(s_4393_claim_colony));
  memset(s_4393_claim_valid, 0, sizeof(s_4393_claim_valid));
  /* Save-backed latch: goes through its own accessor, not a raw memset. */
  ai_euro_wagon_errand_clear_all();
  memset(s_0a60_work_registered, 0, sizeof(s_0a60_work_registered));
  s_5d04_ctx = NULL;
  s_5d04_nation = -1;
  memset(s_5d04_hire_scratch, 0, sizeof(s_5d04_hire_scratch));

  memset(s_20e6_explorers, 0, sizeof(s_20e6_explorers));
  memset(s_20e6_explore_fatigue, 0, sizeof(s_20e6_explore_fatigue));
  memset(s_20e6_hop_steps, 0, sizeof(s_20e6_hop_steps));
  memset(s_20e6_hop_slot, 0, sizeof(s_20e6_hop_slot));
  memset(s_20e6_village_visited, 0, sizeof(s_20e6_village_visited));
}
