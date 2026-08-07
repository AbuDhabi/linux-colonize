#include "core/ai_euro.h"

#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/units.h"

#include <stdlib.h>
#include <string.h>

/* Sticky anti-spin stand-ins for DS:0x2d12 / DS:0x2d14. */
static int s_sticky_unit = -1;
static int s_sticky_count = 0;

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

static int ai_euro_is_military_name(const char* name) {
  if (!name) {
    return 0;
  }
  return strstr(name, "Soldier") != NULL || strstr(name, "Dragoon") != NULL ||
         strstr(name, "Regular") != NULL;
}

/* Soldier / Dragoon / Scout (and Regular) — land war hunt only; not founders. */
static int ai_euro_is_land_war_hunter(const char* name) {
  if (!name) {
    return 0;
  }
  return ai_euro_is_military_name(name) || strstr(name, "Scout") != NULL;
}

static int ai_euro_land_is_fortified(const ColonizeUnit* u) {
  return u && (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED);
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
 * Thin E scout explore target (CONTACT scout rings PARKED).
 * Prefer tribe-adjacent secondary FOUND stand-in (or tribe xy), else farthest
 * map corner from first own colony (unexplored-ish stand-in).
 */
static int ai_euro_scout_explore_target(
  ColonizeTurnContext* ctx,
  int nation_id,
  int* out_x,
  int* out_y
) {
  if (!ctx || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }

  int ref_x = 0;
  int ref_y = 0;
  int have_ref = 0;
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == nation_id) {
        ref_x = c->x;
        ref_y = c->y;
        have_ref = 1;
        break;
      }
    }
  }

  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe && ctx->col1->head.tribe_count > 0) {
    int best_d = -1;
    int bx = 0;
    int by = 0;
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      int tx = (int)t->x;
      int ty = (int)t->y;
      int fx = 0;
      int fy = 0;
      if (ctx->map &&
          ai_goals_pick_founding_tile(
            ctx->map, ctx->colonies, nation_id, (int)t->x, (int)t->y, &fx, &fy)) {
        tx = fx;
        ty = fy;
      }
      const int d = have_ref ? (abs(tx - ref_x) + abs(ty - ref_y)) : 0;
      if (best_d < 0 || d > best_d) {
        best_d = d;
        bx = tx;
        by = ty;
      }
    }
    *out_x = bx;
    *out_y = by;
    return 1;
  }

  /* No tribes: farthest map corner from own colony. */
  if (ctx->map && ctx->map->width > 0 && ctx->map->height > 0 && have_ref) {
    const int corners[4][2] = {
      {0, 0},
      {ctx->map->width - 1, 0},
      {0, ctx->map->height - 1},
      {ctx->map->width - 1, ctx->map->height - 1}
    };
    int best_d = -1;
    int bx = corners[0][0];
    int by = corners[0][1];
    for (int i = 0; i < 4; ++i) {
      const int d = abs(corners[i][0] - ref_x) + abs(corners[i][1] - ref_y);
      if (d > best_d) {
        best_d = d;
        bx = corners[i][0];
        by = corners[i][1];
      }
    }
    *out_x = bx;
    *out_y = by;
    return 1;
  }
  return 0;
}

static void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy) {
  if (!u) {
    return;
  }
  u->orders = orders;
  u->goto_x = gx;
  u->goto_y = gy;
}

static int ai_euro_is_ship_type(const ColonizeUnitPool* units, int unit_id) {
  /* Dispatcher ship wave: sea domain (SHIP_A..C stand-in). */
  return units_is_sea(units, unit_id);
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
 * Thin 5b66 case 7 economy stand-in: Pioneer/Hardy on own colony with tools
 * shortage adds +10 stock[TOOLS] (cap 100) once per act; trims tools_short /
 * urgency. Full wagon / hire / treasury matrix remains PARKED.
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
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    /* 5cf6-shaped shortage tallies. */
    if (c->stock[COLONIZE_CARGO_TOOLS] < 20) {
      inv->tools_short += 20 - c->stock[COLONIZE_CARGO_TOOLS];
    }
    if (c->stock[COLONIZE_CARGO_MUSKETS] < 10) {
      inv->muskets_short += 10 - c->stock[COLONIZE_CARGO_MUSKETS];
    }
    if (c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
      inv->food_short += (c->population * 2) - c->stock[COLONIZE_CARGO_FOOD];
    }
    if (c->building_in_production >= 0) {
      inv->found_flags++;
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

static void ai_euro_nation_planning(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  const int diff = ctx->col1->head.difficulty;
  const unsigned bump = 10u + (unsigned)(4 - diff) * 5u + (inv ? (unsigned)inv->urgency : 0u);
  nat->gold += bump;

  /*
   * NEW WORLD wagon / mid-game hire matrix — PARKED (DOS 5d04 after early dock).
   * Thin mid-hire: Europe-dock board while colony_count < 6; at war prefer Soldier/Dragoon;
   * with colonies>=2 also Artillery (Cannon fallback) when type exists — after Soldier
   * already aboard, or every other hire turn. Thin tools-cargo stand-in below when
   * tools_short > 40 (ship holds +20 or nearest-colony +15). Full 5d04 matrix PARKED.
   */
  const int colonies = inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
  if (colonies >= 6) {
    return;
  }
  const int hire_cost = 200 + diff * 25;
  if ((int)nat->gold < hire_cost) {
    return;
  }

  ColonizeUnit* ship = NULL;
  for (int i = COLONIZE_UNITS_MAX - 1; i >= 0; --i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (ai_euro_is_ship_type(ctx->units, u->id) && ai_euro_in_europe(u->x, u->y)) {
      ship = u;
      break;
    }
  }
  if (!ship || ship->cargo_count >= units_ship_capacity(ctx->units, ship->id)) {
    return;
  }

  /* At war with any Euro peer → prefer Soldier / Dragoon over settle types. */
  int hire_ty = -1;
  const int at_war = ai_euro_at_war_any_peer(ctx->col1, nation_id);
  if (at_war) {
    static const char* k_mil[] = {
      "Soldier", "Veteran Soldier", "Soldiers", "Dragoon", "Veteran Dragoon", "Dragoons"
    };
    int mil_ty = -1;
    for (size_t i = 0; i < sizeof(k_mil) / sizeof(k_mil[0]) && mil_ty < 0; ++i) {
      mil_ty = units_find_type(ctx->units, k_mil[i]);
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
    const int prefer_art = art_ty >= 0 && (mil_aboard || (turn & 1u));
    if (prefer_art) {
      hire_ty = art_ty;
    } else if (mil_ty >= 0) {
      hire_ty = mil_ty;
    } else if (art_ty >= 0) {
      hire_ty = art_ty; /* mil type missing — Artillery still a war option */
    }
  }
  /* Peace / fallback: 5c3c-shaped profession demand → Pioneer, else Free Colonist. */
  if (hire_ty < 0 && inv) {
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
    hire_ty = units_find_type(ctx->units, "Free Colonist");
  }
  if (hire_ty < 0) {
    hire_ty = units_find_type(ctx->units, "Colonist");
  }
  if (hire_ty < 0) {
    return;
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
  pax->nation_id = nation_id;
  if (!units_board_stacked(ctx->units, uid, ship->id)) {
    units_despawn(ctx->units, uid);
    return;
  }
  nat->gold -= (uint32_t)hire_cost;
  if (inv && inv->profession_demand[0] > 0) {
    inv->profession_demand[0]--;
  }

  /*
   * Thin NEW WORLD wagon / tools-cargo hire stand-in (full 5d04 wagon matrix PARKED).
   * When tools_short > 40 and hired Pioneer/Hardy: ensure passenger tools; stock +20
   * TOOLS onto Europe ship goods holds if transport slots exist; else +15 tools to
   * nearest own colony (wagon delivery without a wagon unit).
   */
  if (inv && inv->tools_short > 40) {
    const ColonizeUnitType* hired = units_type(ctx->units, hire_ty);
    const int hired_pioneer =
      hired &&
      (strstr(hired->name, "Pioneer") != NULL || strstr(hired->name, "Hardy") != NULL);
    if (hired_pioneer) {
      if (pax->tools < UNITS_EQUIP_TOOLS_STEP) {
        pax->tools = UNITS_EQUIP_TOOLS_STEP;
      }
      int delivered = 0;
      if (units_goods_hold_count(ctx->units, ship->id) > 0) {
        delivered = units_load_goods(ctx->units, ship->id, COLONIZE_CARGO_TOOLS, 20);
      }
      if (delivered <= 0 && ctx->colonies) {
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
          int stock = nearest->stock[COLONIZE_CARGO_TOOLS] + 15;
          if (stock > 100) {
            stock = 100;
          }
          nearest->stock[COLONIZE_CARGO_TOOLS] = stock;
          delivered = 15;
        }
      }
      if (delivered > 0) {
        if (inv->tools_short > delivered) {
          inv->tools_short -= delivered;
        } else {
          inv->tools_short = 0;
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
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  ai_goals_clear_work_queue();

  /* A: urgency seed (coarse-fog wipe skipped — Linux fog is separate). */
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

  /* D: own colonies — LABOR from tools shortage / underpop. */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      int labor = (c->population < 3);
      if (inv && inv->tools_short > 0 && c->stock[COLONIZE_CARGO_TOOLS] < 20) {
        labor = 1;
      }
      if (labor) {
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, 4 + urgency / 4);
      } else {
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_COLONY, 2);
      }
      /* Expand: FOUND via 06ae around colony. */
      int fx = 0;
      int fy = 0;
      if (ai_goals_pick_founding_tile(
            ctx->map, ctx->colonies, nation_id, c->x, c->y, &fx, &fy)) {
        if (fx != c->x || fy != c->y) {
          ai_goals_upsert_primary(nation_id, fx, fy, AI_GOAL_FOUND, 2);
        }
      }
    }
  }

  /* E: foreign colonies MILITARY if at war; thin bind one idle Soldier/Dragoon.
   * Full CONTACT scout rings / deep mid-mil scoring — PARKED.
   * Thin E scout explore: peaceful + own≥1 → idle Scout → tribe/FOUND. */
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
      /* Peaceful scout explore rings stand-in (own colonies ≥ 1). */
      const int own =
        inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
      if (own >= 1) {
        /* Optional secondary FOUND at tribe tiles (F may raise prio later). */
        if (ctx->col1->tribe) {
          for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
            const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
            int fx = 0;
            int fy = 0;
            if (ai_goals_pick_founding_tile(
                  ctx->map, ctx->colonies, nation_id, (int)t->x, (int)t->y, &fx, &fy)) {
              ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 1);
            } else {
              ai_goals_upsert_secondary(nation_id, (int)t->x, (int)t->y, AI_GOAL_FOUND, 1);
            }
          }
        }
        int tx = 0;
        int ty = 0;
        if (ai_euro_scout_explore_target(ctx, nation_id, &tx, &ty)) {
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
            ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
          }
        }
      }
    }
  }

  /* F: tribe-adjacent FOUND prio 2; alarmed → MILITARY. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      int fx = 0;
      int fy = 0;
      if (ai_goals_pick_founding_tile(
            ctx->map, ctx->colonies, nation_id, t->x, t->y, &fx, &fy)) {
        ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 2);
      } else {
        ai_goals_upsert_secondary(nation_id, t->x, t->y, AI_GOAL_FOUND, 1);
      }
      if (t->alarm[nation_id].friction > 50) {
        ai_goals_upsert_primary(nation_id, t->x, t->y, AI_GOAL_MILITARY, 3);
      }
    }
  }

  /*
   * G continent stance (thin) — mid-game pressure once established (≥2 colonies).
   * Deep nation×continent stance table (−0x6790 ∈ {0,3,4,6}) stays PARKED.
   */
  {
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
          /* Higher than E's foreign MILITARY (5). */
          ai_goals_upsert_primary(nation_id, target->x, target->y, AI_GOAL_MILITARY, 6);
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
          /* Prefer tribe-adjacent secondary FOUND stand-in (tribe tile). */
          if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
              ctx->col1->head.tribe_count > 0) {
            tx = ctx->col1->tribe[0].x;
            ty = ctx->col1->tribe[0].y;
            have_t = 1;
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

  /* Ship FOUND via 06ae: first colony (high prio) or second-wave while < 6. */
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
        if (ai_goals_pick_founding_tile(
              ctx->map, ctx->colonies, nation_id, u->x, u->y, &fx, &fy)) {
          ai_goals_upsert_primary(nation_id, fx, fy, AI_GOAL_FOUND, found_prio);
        }
      }
    }
  }

  /* H: light bind — idle land founders → primary FOUND (do not steal Soldiers). */
  {
    int fx = 0;
    int fy = 0;
    if (ai_goals_best_found_tile(nation_id, &fx, &fy)) {
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
        if (!name || strstr(name, "Soldier")) {
          continue;
        }
        if (!strstr(name, "Pioneer") && !strstr(name, "Hardy") &&
            !strstr(name, "Free Colonist") && !strstr(name, "Colonist")) {
          continue;
        }
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, fx, fy);
      }
    }
  }
}

/* --- 20e6 scoring (land Manhattan + ocean/ship branch) ----------------- */

static int ai_euro_ocean_score_step(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int goal_x,
  int goal_y,
  int* out_dx,
  int* out_dy
) {
  /*
   * Naval/ocean branch of FUN_521d_20e6 (thin extract): prefer water tiles
   * that reduce Chebyshev/Manhattan distance to goal; avoid land; slight
   * preference for high-seas / west when goal is west of ship.
   */
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + dx[d];
    const int ny = u->y + dy[d];
    if (!map_coords_inset(ctx->map, nx, ny) &&
        (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height)) {
      continue;
    }
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
    int dist = abs(goal_x - nx) + abs(goal_y - ny);
    int score = 2000 - dist * 12;
    if (map_tile_is_high_seas(ctx->map, nx, ny)) {
      score += 5;
    }
    if (goal_x < u->x && dx[d] < 0) {
      score += 4; /* west bias toward New World */
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
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
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
    }
    const int dist = abs(goal_x - nx) + abs(goal_y - ny);
    int score = 1000 - dist * 10;
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

/* Returns non-zero to abort act (DOS 20e6 non-zero return). */
static int ai_euro_move_scoring_gate(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  /*
   * Ships: never retarget here — landfall/sail courses are owned by case 0x0b.
   * (Sticky clear or arrival wipe must not become a distant FOUND yank.)
   */
  if (units_is_sea(ctx->units, u->id)) {
    return 0;
  }
  /*
   * At-war land hunters: defer course to act-level land war hunt (do not
   * explore-yank idle Soldier/Dragoon/Scout before hunt can set AI_MOVE).
   */
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id) &&
      ai_euro_is_land_war_hunter(units_display_name(ctx->units, u)) &&
      !ai_euro_land_is_fortified(u)) {
    return 0;
  }
  int gx = u->x;
  int gy = u->y;
  int fx = 0;
  int fy = 0;
  if (ai_goals_best_found_tile(nation_id, &fx, &fy)) {
    gx = fx;
    gy = fy;
  } else if (units_orders_follow_goto(u->orders)) {
    gx = u->goto_x;
    gy = u->goto_y;
  } else {
    gx = u->x > 2 ? u->x - 2 : 0;
    gy = u->y;
  }
  int dx = 0;
  int dy = 0;
  if (!ai_euro_score_move(ctx, u, gx, gy, &dx, &dy)) {
    return 1;
  }
  ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, gx, gy);
  return 0;
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
    if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      ai_diplo_declare_war(ctx->col1, u->nation_id, f->nation_id);
    }
  }
  if (units_is_sea(ctx->units, u->id)) {
    units_resolve_naval_combat(ctx->units, u->id, foe, ctx->rng);
  } else if (units_resolve_land_combat(ctx->units, u->id, foe, ctx->rng)) {
    units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng);
  }
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

/*
 * Thin naval war hunt (5b66 case 0x0b act-level): nearest enemy sea unit or
 * coastal water by a foreign Euro colony at war. Full 20e6 combat scoring PARKED.
 */
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

/* Attack adjacent enemy sea unit while at war (try_move cannot step onto ships). */
static void ai_euro_naval_try_adjacent_attack(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !u || !u->active || !units_is_sea(ctx->units, u->id)) {
    return;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
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
    ai_euro_try_attack(ctx, u, nx, ny);
    return;
  }
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
 * foreign Euro colony tile at war. Full 20e6 land combat scoring PARKED.
 */
static int ai_euro_land_war_hunt_target(
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
    if (!units_is_on_map(f) || units_is_sea(ctx->units, f->id) || ai_euro_in_europe(f->x, f->y)) {
      continue;
    }
    if (!ai_diplo_at_war(ctx->col1, nation_id, f->nation_id)) {
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
      const int dist = abs(c->x - from_x) + abs(c->y - from_y);
      if (best < 0 || dist < best) {
        best = dist;
        bx = c->x;
        by = c->y;
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

/* Attack adjacent enemy land unit while at war. */
static void ai_euro_land_try_adjacent_attack(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  if (!ctx || !ctx->units || !u || !u->active || units_is_sea(ctx->units, u->id)) {
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
    if (!f || f->nation_id == u->nation_id) {
      continue;
    }
    if (ctx->col1_ok && ctx->col1 && f->nation_id >= 0 && f->nation_id < 4 &&
        !ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
      continue;
    }
    ai_euro_try_attack(ctx, u, nx, ny);
    return;
  }
}

static void ai_euro_unload_settle(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id) {
  if (!ctx || !ship || !units_is_sea(ctx->units, ship->id) || ai_euro_in_europe(ship->x, ship->y)) {
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
    int sc = 2;
    const char* name = units_display_name(ctx->units, p);
    if (name && (strstr(name, "Pioneer") || strstr(name, "Hardy"))) {
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
  if (ai_goals_best_found_tile(nation_id, &fx, &fy)) {
    dest_x = fx;
    dest_y = fy;
  } else if (!ai_goals_pick_founding_tile(
               ctx->map, ctx->colonies, nation_id, ship->x, ship->y, &dest_x, &dest_y)) {
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
  /* First colony + second-wave settle while under 6 colonies. */
  if (ai_euro_colony_count(ctx->colonies, nation_id) < 6) {
    int fx2 = pax->x;
    int fy2 = pax->y;
    if (ai_goals_pick_founding_tile(
          ctx->map, ctx->colonies, nation_id, pax->x, pax->y, &fx2, &fy2)) {
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

/*
 * FUN_521d_5b66 — scoring gate + case 0x0b arms; case 7 hire/economy PARKED
 * (thin Pioneer tools-delivery stand-in only; full wagon matrix in 5d04).
 */
static void ai_euro_unit_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !u || !u->active || u->moves_left <= 0 || u->aboard_ship_id >= 0) {
    return;
  }

  const int is_ship = ai_euro_is_ship_type(ctx->units, u->id);
  const int is_goto = units_orders_follow_goto(u->orders);

  /*
   * Early move-scoring gate (~90552): if orders!=goto (or fresh), call 20e6;
   * non-zero return aborts act. Linux: always score when not already on goto.
   */
  if (!is_goto) {
    if (ai_euro_move_scoring_gate(ctx, u, nation_id)) {
      return;
    }
  }

  /* Case 7 Europe hire / full wagon economy — PARKED (5d04). Thin tools
   * delivery runs on land Pioneer/Hardy at own colony (below). */

  if (is_ship) {
    if (ai_euro_in_europe(u->x, u->y)) {
      int hx = 0;
      int hy = 0;
      if (units_find_eastern_high_seas_tile(ctx->units, ctx->map, u->y, &hx, &hy)) {
        u->x = hx;
        u->y = hy;
        int fx = 0;
        int fy = 0;
        if (ai_goals_best_found_tile(nation_id, &fx, &fy)) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, fx, fy);
        } else {
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, hx > 2 ? hx - 8 : 0, hy);
        }
      }
    }

    /*
     * Thin naval war hunt (act-level): idle / station-keep ships at war sail
     * toward nearest foe sea unit or coastal colony water. Adjacent → try_attack.
     * Deep 20e6 naval combat scoring stays PARKED.
     */
    const int at_war =
      ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
    if (at_war && !ai_euro_in_europe(u->x, u->y)) {
      ai_euro_naval_try_adjacent_attack(ctx, u);
      if (!u->active) {
        return;
      }
      if (!ai_euro_ship_has_useful_goto(u, ctx->map)) {
        int hx = 0;
        int hy = 0;
        if (ai_euro_naval_war_hunt_target(ctx, nation_id, u->x, u->y, &hx, &hy)) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, hx, hy);
        }
      }
    }

    /*
     * Case 0x0b ship sail: preserve landfall/sail goto. advance_goto clears
     * orders+goto on arrival — station-keep there instead of yanking to a
     * distant FOUND (AMERICA smoke measures squared dist to goto).
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
      units_advance_goto(ctx->units, u->id, ctx->map, ctx->colonies, ctx->rng);
    }
    if (u->active && at_war && !ai_euro_in_europe(u->x, u->y)) {
      ai_euro_naval_try_adjacent_attack(ctx, u);
    }
    if (u->active && !ai_euro_in_europe(u->x, u->y)) {
      ai_euro_unload_settle(ctx, u, nation_id);
    }
    return;
  }

  /* Case 0x0b land: bind primary goal (role-aware scan). */
  const char* uname = units_display_name(ctx->units, u);
  const int is_land_hunter = ai_euro_is_land_war_hunter(uname);
  const int is_scout = uname && strstr(uname, "Scout") != NULL;
  const int at_war_land =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
  int land_war_hunted = 0;
  int scout_explored = 0;

  /*
   * Thin land war hunt (act-level): idle Soldier/Dragoon/Scout at war move
   * toward nearest foe land unit or enemy colony. Adjacent → try_attack.
   * Does not steal founders on FOUND. Deep 20e6 land combat scoring PARKED.
   */
  if (at_war_land && is_land_hunter && !ai_euro_land_is_fortified(u)) {
    ai_euro_land_try_adjacent_attack(ctx, u);
    if (!u->active) {
      return;
    }
    if (!ai_euro_land_has_useful_goto(u, ctx->map)) {
      int hx = 0;
      int hy = 0;
      if (ai_euro_land_war_hunt_target(ctx, nation_id, u->x, u->y, &hx, &hy)) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, hx, hy);
        land_war_hunted = 1;
      }
    }
  }

  /*
   * Thin E scout explore (act-level): peaceful Scout with own≥1 keeps/gets
   * AI_MOVE toward tribe FOUND / farthest corner; do not yank to COLONY.
   * Full CONTACT scout rings PARKED.
   */
  if (!at_war_land && is_scout &&
      ai_euro_colony_count(ctx->colonies, nation_id) >= 1) {
    int tx = 0;
    int ty = 0;
    if (ai_euro_scout_explore_target(ctx, nation_id, &tx, &ty)) {
      if (!ai_euro_land_has_useful_goto(u, ctx->map)) {
        ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
      }
      scout_explored = 1;
    }
  }

  int goal_x = u->goto_x;
  int goal_y = u->goto_y;
  int goal_code = -1;
  {
    const int is_soldier = uname && strstr(uname, "Soldier");
    const int is_founder =
      uname && !is_soldier &&
      (strstr(uname, "Pioneer") || strstr(uname, "Hardy") || strstr(uname, "Free Colonist") ||
       strstr(uname, "Colonist"));

    /* Soldiers: MILITARY/CONTACT first; founders: FOUND over LABOR/COLONY. */
    if (is_soldier) {
      for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
        const AiGoalSlot* g = ai_goals_primary(nation_id, i);
        if (!g || g->code == AI_GOAL_EMPTY) {
          continue;
        }
        if (g->code == AI_GOAL_MILITARY || g->code == AI_GOAL_CONTACT) {
          goal_x = g->x;
          goal_y = g->y;
          goal_code = (int)g->code;
          break;
        }
      }
    } else if (is_founder) {
      for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
        const AiGoalSlot* g = ai_goals_primary(nation_id, i);
        if (!g || g->code == AI_GOAL_EMPTY) {
          continue;
        }
        if (g->code == AI_GOAL_FOUND || g->code == AI_GOAL_MIL_EXPAND) {
          goal_x = g->x;
          goal_y = g->y;
          goal_code = (int)g->code;
          break;
        }
      }
    }
    if (goal_code < 0) {
      for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
        const AiGoalSlot* g = ai_goals_primary(nation_id, i);
        if (!g || g->code == AI_GOAL_EMPTY) {
          continue;
        }
        goal_x = g->x;
        goal_y = g->y;
        goal_code = (int)g->code;
        break; /* highest prio is slot 0 after ordered upsert */
      }
    }
  }

  if (goal_code == AI_GOAL_FOUND && u->x == goal_x && u->y == goal_y) {
    ai_euro_found_with_unit(ctx, u, nation_id);
    return;
  }

  /*
   * Thin tools delivery (case 7 economy stand-in): idle/arriving Pioneer or
   * Hardy on own colony tile with tools_short / stock<20 → +10 TOOLS.
   * Prefer LABOR/COLONY arrive; also covers idle-on-colony before join.
   * Full wagon matrix PARKED.
   */
  {
    const int is_pioneer =
      uname && (strstr(uname, "Pioneer") || strstr(uname, "Hardy"));
    if (is_pioneer && ctx->colonies) {
      const int here = colonies_id_at(ctx->colonies, u->x, u->y);
      if (here >= 0) {
        ColonizeColony* oc = colonies_get_mut(ctx->colonies, here);
        if (oc && oc->nation_id == nation_id) {
          (void)ai_euro_try_pioneer_tools_delivery(ctx, nation_id, oc);
        }
      }
    }
  }

  if ((goal_code == AI_GOAL_LABOR || goal_code == AI_GOAL_COLONY) && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
    if (cid >= 0 && u->x == goal_x && u->y == goal_y) {
      ai_euro_join_colony(ctx, u, cid);
      return;
    }
  }
  if (goal_code == AI_GOAL_MILITARY || goal_code == AI_GOAL_CONTACT) {
    if (abs(u->x - goal_x) <= 1 && abs(u->y - goal_y) <= 1) {
      const int foe = units_id_at(ctx->units, goal_x, goal_y);
      if (foe >= 0) {
        ai_euro_try_attack(ctx, u, goal_x, goal_y);
        return;
      }
    }
    if (ctx->colonies && u->x == goal_x && u->y == goal_y) {
      const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
      if (cid >= 0) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
        if (c && c->nation_id != nation_id) {
          colonies_capture(ctx->colonies, cid, nation_id);
          return;
        }
      }
    }
  }

  /* Preserve land-war hunt / scout-explore goto; do not yank via COLONY. */
  if (goal_code >= 0 && !land_war_hunted && !scout_explored) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, goal_x, goal_y);
  }

  if (units_orders_follow_goto(u->orders)) {
    int dx = 0;
    int dy = 0;
    if (ai_euro_score_move(ctx, u, u->goto_x, u->goto_y, &dx, &dy)) {
      const int tx = u->x + dx;
      const int ty = u->y + dy;
      const int foe = units_id_at(ctx->units, tx, ty);
      if (foe >= 0) {
        ai_euro_try_attack(ctx, u, tx, ty);
      } else {
        units_try_move(ctx->units, u->id, ctx->map, tx, ty, ctx->colonies, ctx->rng);
      }
    }
  } else {
    /* Fortify soldiers near own colony (case 0x0b fortify arm). */
    const char* name = units_display_name(ctx->units, u);
    if (name && strstr(name, "Soldier") && ctx->colonies) {
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        const ColonizeColony* c = &ctx->colonies->colonies[i];
        if (c->active && c->nation_id == nation_id &&
            abs(c->x - u->x) <= 1 && abs(c->y - u->y) <= 1) {
          units_order_fortify(ctx->units, u->id);
          break;
        }
      }
    }
  }

  if (u->active && at_war_land && is_land_hunter && !ai_euro_land_is_fortified(u)) {
    ai_euro_land_try_adjacent_attack(ctx, u);
  }

  /*
   * Sticky CONTACT re-hunt: if moves remain and an adjacent foreign Euro is
   * at war, try_attack once more (dispatcher sticky waves still apply).
   * Deep 20e6 combat scoring PARKED.
   */
  if (u->active && u->moves_left > 0 && ctx->col1_ok && ctx->col1 &&
      !units_is_sea(ctx->units, u->id)) {
    static const int dx8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int d = 0; d < 8; ++d) {
      const int nx = u->x + dx8[d];
      const int ny = u->y + dy8[d];
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (!f || f->nation_id < 0 || f->nation_id > 3 || f->nation_id == nation_id) {
        continue;
      }
      if (!ai_diplo_at_war(ctx->col1, nation_id, f->nation_id)) {
        continue;
      }
      ai_euro_try_attack(ctx, u, nx, ny);
      break;
    }
  }
}

int ai_euro_use_full_dispatch(const ColonizeTurnContext* ctx) {
  if (!ctx) {
    return 1;
  }
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

  /* 0. Sticky clear */
  s_sticky_unit = -1;
  s_sticky_count = 0;

  /* 1–3. Colony + unit inventory */
  ai_euro_colony_inventory(ctx, nation_id);
  ai_euro_unit_inventory(ctx, nation_id);

  /* 4. Treaty timers BEFORE plan (not war RNG). */
  ai_diplo_treaty_timers(ctx, nation_id);

  /* 5. Plan: 5d04 → 0342 → 0a60 */
  ai_euro_nation_planning(ctx, nation_id);
  ai_goals_promote_secondary_to_primary(nation_id);
  ai_euro_colony_goals(ctx, nation_id);

  /* Opportunistic balance after plan (separate from timer slot). */
  ai_diplo_euro_balance(ctx, nation_id);

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
        if (!in_wave || u->moves_left <= 0) {
          continue;
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
}
