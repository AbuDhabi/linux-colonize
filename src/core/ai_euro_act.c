/*
 * Euro AI — ai_euro_unit_act stage functions (ai_euro_act_*) and the DOS ship-act path
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Sections:
 *   FUN_5952_035e equip pick (shared with the goals equip arm)
 *   ai_euro_act_pioneer_corridor / ai_euro_act_soldier_staging
 *   Ship band stages: europe_exit, first_colony_course, war_trade, sail, arrival
 *   Land band stages: hunt_scout, treasure, roles, fortify, goal_consume, goal_dispatch
 *   ai_euro_act_ship / ai_euro_act_land band drivers
 *   DOS ship-act path: 09dc gate, 20e6 ship dos, 479b goal walk
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
int ai_euro_5952_equip_pick(const ColonizeColony* c, int target) {
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
AiEuroActStatus ai_euro_act_pioneer_corridor(struct ai_euro_act_ctx* a) {
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
AiEuroActStatus ai_euro_act_soldier_staging(struct ai_euro_act_ctx* a) {
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
        if (u->aboard_ship_id < 0 && units_is_on_map(u)) {
          units_tile_stack_arrive(ctx->units, u->id);
        }
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
        u->id >= 0 && u->id < COLONIZE_UNITS_MAX && ai_euro_s_euro_ship_route_latch[u->id];
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
          ai_euro_s_euro_ship_route_latch[u->id] = 1;
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
              if (u->aboard_ship_id < 0 && units_is_on_map(u)) {
                units_tile_stack_arrive(ctx->units, u->id);
              }
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
   * Colonization.pdf Wagon Train; 5cf6 food_short.
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
    const int is_free_colonist =
      ukind == UNITS_KIND_COLONIST &&
      (u->profession == 19 ||
       (!is_pioneer && !is_farmer && !is_carpenter));
    const int is_colonist_cap =
      ukind != UNITS_KIND_SOLDIER && ukind != UNITS_KIND_DRAGOON &&
      ukind != UNITS_KIND_SCOUT && !ai_euro_type_is_wagon_name(ukind) &&
      (is_pioneer || is_farmer || is_carpenter ||
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
void ai_euro_act_ship(struct ai_euro_act_ctx* a) {
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
void ai_euro_act_land(struct ai_euro_act_ctx* a) {
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
int ai_euro_ship_dos_enabled(void) {
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
  ai_euro_s_euro_last_dir[id] = (int8_t)u->last_dir;
  int attack = 0;
  const int dir = ai_euro_20e6_wander_step(ctx, u, &s, &attack, NULL);
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[shipdos] unit %d n%d (%d,%d) mp %d 4d2e dir %d attack %d\n", id, nation_id,
            u->x, u->y, u->moves, dir, attack);
  }
  if (dir >= 0 && dir < 8 && attack) {
    ai_euro_s_euro_last_dir[id] = (int8_t)dir;
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
    ai_euro_s_euro_last_dir[id] = (int8_t)dir;
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
void ai_euro_act_ship_dos(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
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
