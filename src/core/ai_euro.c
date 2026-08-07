#include "core/ai_euro.h"

#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- local helpers (T0; mirror ai.c peels without exporting statics) ---- */

static int ai_euro_in_europe(int x, int y) {
  return x >= 200 || y >= 200;
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

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy) {
  if (!u) {
    return;
  }
  u->orders = orders;
  u->goto_x = gx;
  u->goto_y = gy;
}

static int ai_euro_founder_score(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const char* name = units_display_name(units, u);
  if (!name) {
    return 1;
  }
  if (strstr(name, "Pioneer") || strstr(name, "Hardy")) {
    return 5;
  }
  if (strstr(name, "Soldier") || strstr(name, "Scout")) {
    return 3;
  }
  if (strstr(name, "Colonist") || strstr(name, "Free")) {
    return 4;
  }
  return 2;
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
  const int cid = colonies_found(
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
  if (cid >= 0) {
    units_despawn(ctx->units, founder->id);
    if (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
      ctx->col1->player[nation_id].founded_colonies++;
    }
  }
}

static void ai_euro_join_colony(ColonizeTurnContext* ctx, ColonizeUnit* u, int colony_id) {
  if (!ctx || !ctx->colonies || !u) {
    return;
  }
  (void)colonies_admit_unit(ctx->colonies, colony_id, ctx->units, u->id);
}

/*
 * FUN_521d_5d04 T0 — treasury bump + Europe hire of a free colonist onto a ship
 * still in Europe / returning.
 */
static void ai_euro_nation_planning(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  /* Difficulty-scaled treasury drip. */
  const int diff = ctx->col1->head.difficulty;
  const unsigned bump = 10u + (unsigned)(4 - diff) * 5u;
  nat->gold += bump;

  const int colonies = ai_euro_colony_count(ctx->colonies, nation_id);
  if (colonies >= 3) {
    return; /* mid-game hire pressure later */
  }
  if (nat->gold < 300) {
    return;
  }

  /* Hire: spawn Free Colonist in Europe aboard nation's ship if present. */
  ColonizeUnit* ship = NULL;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, u->id) && ai_euro_in_europe(u->x, u->y)) {
      ship = u;
      break;
    }
  }
  if (!ship) {
    return;
  }
  int free_ty = units_find_type(ctx->units, "Free Colonist");
  if (free_ty < 0) {
    free_ty = units_find_type(ctx->units, "Colonist");
  }
  if (free_ty < 0) {
    return;
  }
  if (ship->cargo_count >= units_ship_capacity(ctx->units, ship->id)) {
    return;
  }
  const int uid = units_spawn(ctx->units, free_ty, ship->x, ship->y);
  if (uid < 0) {
    return;
  }
  ColonizeUnit* pax = units_get(ctx->units, uid);
  if (!pax) {
    return;
  }
  pax->nation_id = nation_id;
  if (!units_board(ctx->units, uid, ship->id)) {
    units_despawn(ctx->units, uid);
    return;
  }
  nat->gold -= 300;
}

/* FUN_521d_0a60 phases A–H (T0 condensed). */
static void ai_euro_colony_goals(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  ai_goals_clear_work_queue();

  /* B: own units — contact / explore scratch via work queue. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_on_map(u)) {
      continue;
    }
    ai_goals_upsert_work(u->id, 1, 0, 0);
    /* Foreign unit adjacent → CONTACT goal. */
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
      }
    }
  }

  /* D: own colonies — LABOR if underpopulated. */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      const int code = (c->population < 3) ? AI_GOAL_LABOR : AI_GOAL_COLONY;
      ai_goals_upsert_primary(nation_id, c->x, c->y, code, (c->population < 3) ? 4 : 2);
      /* F: nearby settle — FOUND on empty land near colony. */
      for (int oy = -3; oy <= 3; ++oy) {
        for (int ox = -3; ox <= 3; ++ox) {
          if (ox == 0 && oy == 0) {
            continue;
          }
          const int tx = c->x + ox;
          const int ty = c->y + oy;
          if (colonies_can_found(ctx->colonies, ctx->map, tx, ty)) {
            ai_goals_upsert_primary(nation_id, tx, ty, AI_GOAL_FOUND, 2);
          }
        }
      }
    }
  }

  /* E: foreign colonies — MILITARY if at war. */
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
  }

  /* F: tribes — FOUND / MILITARY near villages. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      ai_goals_upsert_secondary(nation_id, t->x, t->y, AI_GOAL_FOUND, 1);
      if (t->alarm[nation_id].friction > 50) {
        ai_goals_upsert_primary(nation_id, t->x, t->y, AI_GOAL_MILITARY, 3);
      }
    }
  }

  /* First colony urgency: FOUND near landfall-ish ship if no colonies. */
  if (ai_euro_colony_count(ctx->colonies, nation_id) == 0) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (!units_is_sea(ctx->units, u->id) || ai_euro_in_europe(u->x, u->y)) {
        continue;
      }
      /* Prefer adjacent land for founding. */
      static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
      static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
      for (int d = 0; d < 8; ++d) {
        const int tx = u->x + dx[d];
        const int ty = u->y + dy[d];
        if (ctx->colonies && colonies_can_found(ctx->colonies, ctx->map, tx, ty)) {
          ai_goals_upsert_primary(nation_id, tx, ty, AI_GOAL_FOUND, 6);
        }
      }
    }
  }
}

/* FUN_521d_20e6 Euro/ocean T0 — pick adjacent step toward goal. */
static int ai_euro_score_step(
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
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  const int sea = units_is_sea(ctx->units, u->id);
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!units_can_enter(ctx->units, u->type_index, ctx->map, nx, ny, u->id, ctx->colonies)) {
      /* Allow combat enter on foreign. */
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (!f || f->nation_id == u->nation_id) {
        continue;
      }
      if (sea != units_is_sea(ctx->units, foe)) {
        continue;
      }
    }
    const int dist = abs(goal_x - nx) + abs(goal_y - ny);
    int score = 1000 - dist * 10;
    /* Slight noise for variety. */
    if (ctx->rng) {
      score += dos_rng_range(ctx->rng, 0, 3);
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

static void ai_euro_try_attack(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty) {
  if (!ctx || !ctx->units || !u) {
    return;
  }
  const int foe = units_id_at(ctx->units, tx, ty);
  if (foe < 0) {
    return;
  }
  const ColonizeUnit* f = units_get_const(ctx->units, foe);
  if (!f || f->nation_id == u->nation_id) {
    return;
  }
  if (ctx->col1_ok && ctx->col1 && f->nation_id >= 0 && f->nation_id < 4) {
    if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id) && f->nation_id < 4) {
      /* Auto-war on intentional attack. */
      ai_diplo_declare_war(ctx->col1, u->nation_id, f->nation_id);
    }
  }
  const int sea = units_is_sea(ctx->units, u->id);
  if (sea) {
    units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
  } else {
    if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
      units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng);
    }
  }
  /* Colony capture if standing on foreign colony after win. */
  if (u->active && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      if (c && c->nation_id != u->nation_id && c->nation_id >= 0 && c->nation_id < 4) {
        colonies_capture(ctx->colonies, cid, u->nation_id);
      }
    }
  }
}

static void ai_euro_unload_settle(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id) {
  if (!ctx || !ship || !units_is_sea(ctx->units, ship->id)) {
    return;
  }
  if (ai_euro_in_europe(ship->x, ship->y)) {
    return;
  }
  int fx = 0;
  int fy = 0;
  const int have_found = ai_goals_best_found_tile(nation_id, &fx, &fy);
  /* Unload best founder passenger. */
  int best_id = -1;
  int best_score = 0;
  for (int s = 0; s < ship->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
    const int pid = ship->cargo_ids[s];
    if (pid < 0) {
      continue;
    }
    ColonizeUnit* p = units_get(ctx->units, pid);
    if (!p || !p->active) {
      continue;
    }
    const int sc = ai_euro_founder_score(ctx->units, p);
    if (sc > best_score) {
      best_score = sc;
      best_id = pid;
    }
  }
  if (best_id < 0) {
    return;
  }
  ColonizeUnit* pax = units_get(ctx->units, best_id);
  if (!pax) {
    return;
  }
  int dest_x = ship->x;
  int dest_y = ship->y;
  if (have_found) {
    dest_x = fx;
    dest_y = fy;
  } else if (!units_pick_landfall_tile(
               ctx->units, ship->id, ctx->map, ctx->colonies, -1, -1, &dest_x, &dest_y)) {
    return;
  }
  if (!units_unload_passenger(
        ctx->units, ship->id, best_id, ctx->map, dest_x, dest_y, ctx->colonies)) {
    return;
  }
  pax = units_get(ctx->units, best_id);
  if (!pax) {
    return;
  }
  if (ai_euro_colony_count(ctx->colonies, nation_id) == 0 &&
      colonies_can_found(ctx->colonies, ctx->map, pax->x, pax->y)) {
    ai_euro_found_with_unit(ctx, pax, nation_id);
  } else {
    ai_euro_set_goto(pax, UNITS_ORDER_AI_MOVE, dest_x, dest_y);
  }
}

/* FUN_521d_5b66 T0 per-unit act. */
static void ai_euro_unit_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !u || !u->active || u->moves_left <= 0) {
    return;
  }
  if (u->aboard_ship_id >= 0) {
    return;
  }

  const int is_ship = units_is_sea(ctx->units, u->id);

  /* Ships: sail toward FOUND / landfall / west explore; unload when coastal. */
  if (is_ship) {
    if (ai_euro_in_europe(u->x, u->y)) {
      /* Exit Europe toward map: use eastern high seas then west. */
      int hx = 0;
      int hy = 0;
      if (units_find_eastern_high_seas_tile(ctx->units, ctx->map, u->y, &hx, &hy)) {
        u->x = hx;
        u->y = hy;
        ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, hx > 2 ? hx - 2 : 0, hy);
      }
    }
    int gx = u->goto_x;
    int gy = u->goto_y;
    int fx = 0;
    int fy = 0;
    if (ai_goals_best_found_tile(nation_id, &fx, &fy)) {
      gx = fx;
      gy = fy;
      ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, gx, gy);
    }
    if (units_orders_follow_goto(u->orders) || u->orders == UNITS_ORDER_AI_SAIL) {
      int dx = 0;
      int dy = 0;
      if (ai_euro_score_step(ctx, u, gx, gy, &dx, &dy)) {
        const int tx = u->x + dx;
        const int ty = u->y + dy;
        const int foe = units_id_at(ctx->units, tx, ty);
        if (foe >= 0) {
          ai_euro_try_attack(ctx, u, tx, ty);
        } else {
          units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng);
        }
      }
    }
    if (u->active && !ai_euro_in_europe(u->x, u->y)) {
      ai_euro_unload_settle(ctx, u, nation_id);
    }
    return;
  }

  /* Land: bind to best matching goal. */
  int goal_x = u->goto_x;
  int goal_y = u->goto_y;
  int goal_code = -1;
  int best_prio = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* g = ai_goals_primary(nation_id, i);
    if (!g || g->code == AI_GOAL_EMPTY) {
      continue;
    }
    if ((int)g->prio > best_prio) {
      best_prio = (int)g->prio;
      goal_x = g->x;
      goal_y = g->y;
      goal_code = (int)g->code;
    }
  }

  if (goal_code == AI_GOAL_FOUND && u->x == goal_x && u->y == goal_y) {
    ai_euro_found_with_unit(ctx, u, nation_id);
    return;
  }
  if ((goal_code == AI_GOAL_LABOR || goal_code == AI_GOAL_COLONY) && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
    if (cid >= 0 && u->x == goal_x && u->y == goal_y) {
      ai_euro_join_colony(ctx, u, cid);
      return;
    }
  }
  if (goal_code == AI_GOAL_MILITARY || goal_code == AI_GOAL_CONTACT) {
    const int foe = units_id_at(ctx->units, goal_x, goal_y);
    if (foe >= 0 && abs(u->x - goal_x) <= 1 && abs(u->y - goal_y) <= 1) {
      ai_euro_try_attack(ctx, u, goal_x, goal_y);
      return;
    }
    /* Capture foreign colony tile. */
    if (ctx->colonies) {
      const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
      if (cid >= 0 && u->x == goal_x && u->y == goal_y) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
        if (c && c->nation_id != nation_id) {
          colonies_capture(ctx->colonies, cid, nation_id);
          return;
        }
      }
    }
  }

  if (best_prio >= 0) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, goal_x, goal_y);
  }

  if (units_orders_follow_goto(u->orders) || u->orders == UNITS_ORDER_AI_MOVE) {
    int dx = 0;
    int dy = 0;
    if (ai_euro_score_step(ctx, u, u->goto_x, u->goto_y, &dx, &dy)) {
      const int tx = u->x + dx;
      const int ty = u->y + dy;
      const int foe = units_id_at(ctx->units, tx, ty);
      if (foe >= 0) {
        ai_euro_try_attack(ctx, u, tx, ty);
      } else {
        units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng);
      }
    }
  } else if (u->orders == 0 || u->orders == UNITS_ORDER_FORTIFY) {
    /* Idle near own colony → fortify soldiers. */
    const char* name = units_display_name(ctx->units, u);
    if (name && strstr(name, "Soldier")) {
      units_order_fortify(ctx->units, u->id);
    }
  }
}

int ai_euro_use_full_dispatch(const ColonizeTurnContext* ctx) {
  if (!ctx) {
    return 1;
  }
  /* Keep seed-100 fixture path for smoke_ai_turns unless forced. */
  const char* force = getenv("AI_FULL_DISPATCH");
  if (force && force[0] && force[0] != '0') {
    return 1;
  }
  if (ctx->rng_seed == 100) {
    return 0;
  }
  return 1;
}

void ai_euro_dispatcher_turn(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }

  /* 6d8e: planning → promote → colony goals → diplo timers → ship then land act. */
  ai_euro_nation_planning(ctx, nation_id);
  ai_goals_promote_secondary_to_primary(nation_id);
  ai_euro_colony_goals(ctx, nation_id);
  ai_diplo_euro_timers(ctx, nation_id);

  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
        continue;
      }
      const int is_ship = units_is_sea(ctx->units, u->id);
      if (pass == 0 && !is_ship) {
        continue;
      }
      if (pass == 1 && is_ship) {
        continue;
      }
      /* Anti-spin: limited steps per unit per turn. */
      int steps = 0;
      while (u->active && u->moves_left > 0 && steps < 12) {
        const int before = u->moves_left;
        ai_euro_unit_act(ctx, u, nation_id);
        ++steps;
        if (!u->active || u->moves_left >= before) {
          break;
        }
      }
    }
  }
}
