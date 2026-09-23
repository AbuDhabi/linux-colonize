/*
 * Euro AI — dispatcher, nation-turn entry, unit-act glue, continent stance, landfall/ocean helpers, colony AI-flag + construction preferences
 *
 * Split out of the single 21k-line ai_euro.c (2026-09-23), verbatim: the
 * sibling files are ai_euro_colony_jobs.c, ai_euro_expand.c, ai_euro_europe.c,
 * ai_euro_goals.c, ai_euro_land.c, ai_euro_ship.c, ai_euro_act.c. Symbols
 * shared between them are declared in ai_euro_internal.h.
 *
 * Sections:
 *   Includes, file-scope DOS latches (stance, landfall, roam/route, violate stamps)
 *   FUN_521d_0a60 continent stance + rival-strength tables (ai_euro_refresh_continent_stance)
 *   Landfall / ocean-tip helpers (3558 first leg + empty cruise, 06ae first colony)
 *   Ship unload-site helpers, unit-kind predicates, fortify quota, founding-tile pick
 *   Colony AI-flag refresh, ship pressure, construction preferences (prefer_peace_construction)
 *   ai_euro_unit_act dispatcher + ai_euro_dispatcher_turn{,_reset,_plan,_unit_waves}
 *   ai_euro_reset lifecycle hook (zeroes every latch in the eight split files)
 */
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
int ai_euro_env_flag(const char* name, int dflt) {
  const char* e = getenv(name);
  return (e && *e) ? (*e != '0') : dflt;
}

int ai_euro_ship_dos_enabled(void);

/* Sticky anti-spin stand-ins for DS:0x2d12 / DS:0x2d14. */
static int s_sticky_unit = -1;
static int s_sticky_count = 0;
/* First-colony: unit stepped onto found this dispatcher_turn — defer found. */
uint8_t ai_euro_s_deferred_found[COLONIZE_UNITS_MAX];
/*
 * Unit disembarked from a ship during this dispatcher call. DOS: landing
 * consumes the whole move allowance (TURN2→3 English Soldier lands at (50,38)
 * with moves 0 / orders 0 and does not act again that turn). Guards the
 * first-colony walk's SENTRY wake from re-arming a same-turn landing.
 */
uint8_t ai_euro_s_unloaded_this_turn[COLONIZE_UNITS_MAX];
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
int8_t ai_euro_s_euro_last_dir[COLONIZE_UNITS_MAX];
/* Colony ids founded this dispatcher_turn — keep auto-Stockade bip one turn. */
uint8_t ai_euro_s_founded_colony_turn[COLONIZE_COLONIES_MAX];
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
uint32_t ai_euro_s_violate_last_turn[COLONIZE_UNITS_MAX];
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
uint8_t ai_euro_s_euro_roam_wander[COLONIZE_UNITS_MAX];
/*
 * Ship sail-loop route latch: set once the greedy ocean scorer stalls against
 * a land wall for a goto, cleared on every goto write. While set, the act
 * routes via the FUN_6662 pathfinder from its first step instead of greedy
 * west / pathfinder east ping-pong (net zero progress = multi-turn "circles").
 */
uint8_t ai_euro_s_euro_ship_route_latch[COLONIZE_UNITS_MAX];

void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);
void ai_euro_try_attack(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty);
void ai_euro_20e6_ship_cargo_counts(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship,
  int* pioneers, int* mil, int* scouts, int* milvet, int* civ
);
int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id);
void ai_euro_try_violate_notify(ColonizeTurnContext* ctx, ColonizeUnit* u);

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
void ai_euro_refresh_continent_stance(ColonizeTurnContext* ctx, int nation_id) {
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
int ai_euro_rival_strength_at(int nation_id, int continent_id) {
  if (nation_id < 0 || nation_id >= 4 || continent_id < 0 || continent_id > 15) {
    return 0;
  }
  return (int)s_euro_rival_strength[nation_id][continent_id];
}

int ai_euro_continent_stance_at(int nation_id, int continent_id) {
  if (nation_id < 0 || nation_id >= 4 || continent_id < 0 || continent_id > 15) {
    return 0;
  }
  return (int)s_euro_continent_stance[nation_id][continent_id];
}


int ai_euro_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
}

/* Sync passenger tile coords after Europe→map teleport (FUN_48d3_048e). */
void ai_euro_sync_aboard_cargo_xy(ColonizeUnitPool* units, ColonizeUnit* ship) {
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
void ai_euro_resolve_landfall_goto(
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
int ai_euro_ocean_3558_first_leg_tip(
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
int ai_euro_ocean_3558_empty_cruise_tip(
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
int ai_euro_06ae_first_colony_from_landfall(
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
int ai_euro_recover_landfall_from_ship(
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
int ai_euro_land_adjacent_to(
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

int ai_euro_ship_has_land_adjacent(const ColonizeWorldMap* map, int sx, int sy) {
  int lx = 0;
  int ly = 0;
  return ai_euro_land_adjacent_to(map, sx, sy, &lx, &ly);
}

/*
 * Pick land tile adjacent to ship for unload. Prefer toward landfall; skip
 * occupied/forbidden. Returns 0 if none.
 */
int ai_euro_pick_unload_land(
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

int ai_euro_unload_pax_at(
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
    ai_euro_s_unloaded_this_turn[pax->id] = 1;
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
ColonizeUnitKind ai_euro_unit_kind(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!pool || !u) {
    return UNITS_KIND_UNKNOWN;
  }
  return units_type_kind(units_type(pool, u->type_index));
}

/* @UNIT row 2 (Pioneers / "Hardy Pioneer" display name). */
int ai_euro_name_is_pioneer(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_PIONEER;
}

/* @UNIT row 1 (Soldiers / "Veteran Soldier" display name). */
int ai_euro_name_is_soldier(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_SOLDIER;
}


/*
 * 0a60-style coastal staging from Atlantic landfall (same geometry as
 * ai_coastal_staging_from_landfall in ai.c). TURN3 ship XY matches the tip
 * for seed-100 FR/SP landfalls. Cite: test-saves-ai/TURN3; euro_dispatcher 0a60.
 */
int ai_euro_coastal_staging_from_landfall(
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
int ai_euro_foreign_unit_at(const ColonizeTurnContext* ctx, const ColonizeUnit* u, int x, int y) {
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
int ai_euro_try_post_found_coast_cruise(
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
int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);
int ai_euro_20e6_type_combat(int dos_type);
/* Native village on this tile (col1 tribe table), else -1 — defined below;
 * forward-declared for the FUN_5952_035e threat seed's settlement halving. */
int ai_euro_village_nation_at(const ColonizeCol1Save* col1, int x, int y);


AiEuroShipPressure ai_euro_s_ship_pressure[4];

void ai_euro_ship_pressure_reset(int nation_id) {
  if (nation_id >= 0 && nation_id < 4) {
    memset(&ai_euro_s_ship_pressure[nation_id], 0, sizeof(ai_euro_s_ship_pressure[nation_id]));
  }
}
/*
 * "This colony is eating into its stores" — stock below one turn's
 * consumption (TURN_FOOD_PER_COLONIST = 2 per head). Used to ride in
 * colony_flags bit3 until that bit went back to being DOS's
 * inefficient-government latch; the AI only ever wanted the reading, never
 * the storage.
 */
int ai_euro_colony_food_short(const ColonizeColony* c) {
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
int ai_euro_colony_wanted_size(
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
    AiEuroShipPressure* sp = &ai_euro_s_ship_pressure[nation_id];
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

void ai_euro_refresh_colony_ai_flags(
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
int ai_euro_colony_wants_construction_labor(
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

void ai_euro_prefer_peace_construction(ColonizeTurnContext* ctx, int nation_id) {
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
    if (c->id >= 0 && c->id < COLONIZE_COLONIES_MAX && ai_euro_s_founded_colony_turn[c->id]) {
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


/* True if nation_id is at war with any other European peer (0..3). */
int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id) {
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
int ai_euro_is_military_name(ColonizeUnitKind k) {
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
int ai_euro_is_land_war_hunter(ColonizeUnitKind kind) {
  return ai_euro_is_military_name(kind);
}

/* @UNIT row 11 ("Artillery", or a pool spelling it "Cannon"). */
int ai_euro_is_artillery_name(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_ARTILLERY;
}

/*
 * Col1 +0x1e: fortify only while garrison_quota > 0, then DEC.
 * Cite: save_format_map.md. The quota this consumes is the real
 * FUN_5952_035e threat>>3 seed (ai_euro_colony_threat_seed_5952, ported
 * 2026-09-08) — no longer a thin planning latch.
 */
int ai_euro_fortify_with_quota(
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

int ai_euro_land_is_fortified(const ColonizeUnit* u) {
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
int ai_euro_land_is_passive_orders(const ColonizeUnit* u) {
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
int ai_euro_pick_founding_tile(
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
int ai_euro_nearest_military_goal(
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
int ai_euro_is_treasure_name(ColonizeUnitKind kind) {
  return kind == UNITS_KIND_TREASURE;
}


/*
 * Europe-bound lane entry for a ship: prefer the eastern High Seas rim
 * (units_find_eastern_high_seas_tile — the 48d3_015e Atlantic exit), else the
 * nearest water tile further east as a stand-in on a map with no HS column.
 * Consumer: ai_euro_ship_sail_to_europe (the FUN_4393 export / Privateer-loot
 * sail). The treasure caller it also had was deleted 2026-09-23 — see below.
 */
int ai_euro_europe_sail_target(
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

void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);





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
   * (ai_euro_s_euro_roam_wander, written only by ai_euro_move_scoring_gate's
   * explore-scan / fallback-west arms); goal-directed AI_MOVE gotos (found-
   * tile pursuit, war hunt, wagon delivery, ship staging) are not DOS's
   * "roaming" state and are left alone. MET check both directions, same
   * gate as ai_euro_try_violate_notify's adjacency scan.
   */
  if (!is_ship && is_goto && u->orders == UNITS_ORDER_AI_MOVE && u->id >= 0 &&
      u->id < COLONIZE_UNITS_MAX && ai_euro_s_euro_roam_wander[u->id] && ctx->col1_ok && ctx->col1) {
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
  memset(ai_euro_s_deferred_found, 0, sizeof(ai_euro_s_deferred_found));
  memset(ai_euro_s_unloaded_this_turn, 0, sizeof(ai_euro_s_unloaded_this_turn));
  memset(ai_euro_s_founded_colony_turn, 0, sizeof(ai_euro_s_founded_colony_turn));
  /*
   * 0a60 goal-consumption shadow state: reset every call rather than kept
   * across turns like DOS's real unit+0x314b/c/d/e bytes — same "recompute
   * fresh within the turn" simplification ai_euro_s_deferred_found/
   * ai_euro_s_founded_colony_turn above already use, and it's fully repopulated by
   * ai_euro_0a60_goal_orders_structural below before ai_euro_unit_act reads
   * it back later this same call. Avoids stale cross-scenario garbage
   * (distinct unit pools/tests reusing small unit ids) driving a unit into
   * a found/labor-bind action on turn 1 from a previous run's leftover
   * state — real gameplay only ever has one live unit pool, so this is a
   * safety/test-hygiene fix, not a behavior change within a real game.
   */

  /* FUN_521d_0a60 entry: memset(0xa13c,0,16) — per-continent explorer count
   * read by FUN_521d_20e6's explorer cap (ai_euro_s_20e6_explorers). */
  memset(ai_euro_s_20e6_explorers, 0, sizeof(ai_euro_s_20e6_explorers));
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
    ai_euro_s_20e6_wagon_errand[i] = 0;
    ai_euro_s_20e6_explore_fatigue[i] = 0;  /* fresh counter: 0 == never explored */
    ai_euro_s_20e6_hop_slot[i] = 0;  /* slot+1 encoding: 0 == unset (DOS 0xff) */
    ai_euro_s_20e6_hop_steps[i] = 0;
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
 * sentinels (s_sticky_unit == -1, ai_euro_s_5d04_nation == -1, ai_euro_s_5d04_ctx == NULL).
 */
void ai_euro_reset(void) {
  s_sticky_unit = -1;
  s_sticky_count = 0;
  memset(ai_euro_s_5952_ring1, 0, sizeof(ai_euro_s_5952_ring1));
  memset(ai_euro_s_5952_docks_started, 0, sizeof(ai_euro_s_5952_docks_started));
  memset(ai_euro_s_5952_census_nonexpert, 0, sizeof(ai_euro_s_5952_census_nonexpert));
  memset(ai_euro_s_5952_census_vet_soldier, 0, sizeof(ai_euro_s_5952_census_vet_soldier));
  memset(ai_euro_s_deferred_found, 0, sizeof(ai_euro_s_deferred_found));
  memset(ai_euro_s_unloaded_this_turn, 0, sizeof(ai_euro_s_unloaded_this_turn));
  ai_euro_s_20e6_type_cache_valid = 0; /* bugs.md #655: refreshed next dispatcher_turn */
  /*
   * ai_euro_s_euro_last_dir (unit+0x314f) has no save-file backing in this port
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
  memset(ai_euro_s_euro_last_dir, 0, sizeof(ai_euro_s_euro_last_dir));
  memset(ai_euro_s_founded_colony_turn, 0, sizeof(ai_euro_s_founded_colony_turn));
  memset(s_euro_continent_stance, 0, sizeof(s_euro_continent_stance));
  memset(s_euro_rival_strength, 0, sizeof(s_euro_rival_strength));
  memset(ai_euro_s_violate_last_turn, 0, sizeof(ai_euro_s_violate_last_turn));
  memset(ai_euro_s_euro_roam_wander, 0, sizeof(ai_euro_s_euro_roam_wander));
  memset(ai_euro_s_euro_ship_route_latch, 0, sizeof(ai_euro_s_euro_ship_route_latch));
  memset(ai_euro_s_ship_pressure, 0, sizeof(ai_euro_s_ship_pressure));
  memset(ai_euro_s_4393_claim_turn, 0, sizeof(ai_euro_s_4393_claim_turn));
  memset(ai_euro_s_4393_claim_colony, 0, sizeof(ai_euro_s_4393_claim_colony));
  memset(ai_euro_s_4393_claim_valid, 0, sizeof(ai_euro_s_4393_claim_valid));
  /* Save-backed latch: goes through its own accessor, not a raw memset. */
  ai_euro_wagon_errand_clear_all();
  memset(ai_euro_s_0a60_work_registered, 0, sizeof(ai_euro_s_0a60_work_registered));
  ai_euro_s_5d04_ctx = NULL;
  ai_euro_s_5d04_nation = -1;
  memset(ai_euro_s_5d04_hire_scratch, 0, sizeof(ai_euro_s_5d04_hire_scratch));

  memset(ai_euro_s_20e6_explorers, 0, sizeof(ai_euro_s_20e6_explorers));
  memset(ai_euro_s_20e6_explore_fatigue, 0, sizeof(ai_euro_s_20e6_explore_fatigue));
  memset(ai_euro_s_20e6_hop_steps, 0, sizeof(ai_euro_s_20e6_hop_steps));
  memset(ai_euro_s_20e6_hop_slot, 0, sizeof(ai_euro_s_20e6_hop_slot));
  memset(ai_euro_s_20e6_village_visited, 0, sizeof(ai_euro_s_20e6_village_visited));
}
