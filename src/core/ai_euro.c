#include "core/ai_euro.h"

#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/units.h"

#include <stdio.h>
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
  if (!pool || !c || !c->active || c->building_in_production < 0) {
    return 0;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(pool, c->building_in_production);
  if (!bt || bt->name[0] == '\0') {
    return 0;
  }
  return strcmp(bt->name, "Stockade") == 0 || strcmp(bt->name, "Warehouse") == 0 ||
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
 * building_in_production (< 0) → prefer Stockade → Warehouse → (coastal) Docks
 * via colonies_list_buildable + colonies_set_construction. Cite:
 * docs/fandom_col1994.md Defense Stockade→Fort→Fortress / Storage Warehouse /
 * Naval Docks→Drydock→Shipyard; docs/building_production.md Stockade 64h /
 * Warehouse 80h / Dock 52h. No invented hammer/gold buyouts — queue only.
 */
static void ai_euro_prefer_peace_construction(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  const int warehouse_id = colonies_find_building(ctx->colonies, "Warehouse");
  const int docks_id = colonies_find_building(ctx->colonies, "Docks");
  if (stockade_id < 0 && warehouse_id < 0 && docks_id < 0) {
    return;
  }
  const int prefer[] = {stockade_id, warehouse_id, docks_id};
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
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int pick = -1;
    for (size_t p = 0; p < sizeof(prefer) / sizeof(prefer[0]); ++p) {
      const int want = prefer[p];
      if (want < 0) {
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

/* Sentry / fortify / fortified — wake-eligible passive land orders. */
static int ai_euro_land_is_passive_orders(const ColonizeUnit* u) {
  return u &&
         (u->orders == UNITS_ORDER_SENTRY || u->orders == UNITS_ORDER_FORTIFY ||
          u->orders == UNITS_ORDER_FORTIFIED);
}

/*
 * FUN_521d_06ae founding pick with second-colony coastal prefer.
 * When colony_count >= 1, bias score toward map_tile_is_coastal foundable tiles
 * (Docks / port access — fandom Docks coastal gate; lose-all-ports war rule).
 * First colony (count==0) keeps plain 06ae via ai_goals_pick_founding_tile.
 * Cite: euro_goals.c pick_best_adjacent_founding_tile; move_scoring.md §06ae;
 * docs/fandom_col1994.md Docks + Independence port colonies.
 */
static int ai_euro_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int x,
  int y,
  int colony_count,
  int* out_x,
  int* out_y
) {
  if (colony_count < 1) {
    return ai_goals_pick_founding_tile(map, colonies, nation_id, x, y, out_x, out_y);
  }
  if (!map || !out_x || !out_y) {
    return 0;
  }
  static const int k_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
  static const int k_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};
  int best_dir = -1;
  int best_score = -1;
  for (int dir = 0; dir <= 8; ++dir) {
    const int nx = x + k_dx[dir];
    const int ny = y + k_dy[dir];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    if (map_tile_is_water(map, nx, ny)) {
      continue;
    }
    if (map_pedia_terrain_index_at(map, nx, ny) == 24) {
      continue;
    }
    if (colonies && !colonies_can_found(colonies, map, nx, ny)) {
      continue;
    }
    int score = 10;
    score += colony_yield_for_tile(map, nx, ny, COLONIZE_JOB_FARMER) * 3;
    score += colony_yield_for_tile(map, nx, ny, COLONIZE_JOB_FISHERMAN);
    if (dir == 8) {
      score += 2;
    }
    if (map_tile_has_river(map, nx, ny)) {
      score += 3;
    }
    /* Second+ colony: prefer coastal foundable (port / Docks eligibility). */
    if (map_tile_is_coastal(map, nx, ny)) {
      score += 10;
    }
    if (score > best_score) {
      best_score = score;
      best_dir = dir;
    }
  }
  if (best_dir < 0) {
    return 0;
  }
  *out_x = x + k_dx[best_dir];
  *out_y = y + k_dy[best_dir];
  return 1;
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
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id);
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
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id);
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
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id);
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
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id);
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
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id);
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
 * Fur Trader → admit + colonies_assign_workplace on matching craft chain.
 * Cite: Colonization.pdf Skills Chart; docs/building_production.md
 * Distiller/Weaver/Tobacconist/Blacksmith/Gunsmith (Armory→Magazine→Arsenal)/
 * Fur Trader (House→Trading Post→Factory) chains; colonies_assign_workplace.
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
    const int idx = colonies_admit_unit(ctx->colonies, best_cid, ctx->units, u->id);
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
  /* Consume Treasure after cash attempt — same as Expected→Harbor disembark. */
  (void)units_despawn(ctx->units, treasure->id);
  return credited;
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
  ctx->europe->gold = (int)nat->gold;
  ctx->europe->tax_percent = (int)nat->tax_rate;
  europe_tick_voyages(ctx->europe, ctx->units);
  nat->gold = (uint32_t)(ctx->europe->gold < 0 ? 0 : ctx->europe->gold);
}

/*
 * At war: idle Soldier, Dragoon, or Artillery/Cannon on own coastal colony
 * boards an empty transport with passenger space (units_board /
 * units_board_stacked). Complements war-transport sail-to-threatened-port.
 * Skip embark when the colony is already threatened (stay to defend; unload
 * drops troops there). Artillery boards before on-colony fortify (same early
 * act arm). Cite: Colonization.pdf naval transport / Defending a Colony
 * ("fortify soldiers, dragoons, army, cavalry, or artillery"); euro_unit_act
 * §2b2 / §2d3 ship board; existing Treasure board APIs. Empty = cargo_count==0.
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
      (strstr(name, "Soldier") == NULL && strstr(name, "Dragoon") == NULL &&
       strstr(name, "Artillery") == NULL && strstr(name, "Cannon") == NULL)) {
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

/* Wagon carries TOOLS in any hold. */
static int ai_euro_wagon_has_tools(const ColonizeUnitPool* units, const ColonizeUnit* w) {
  return ai_euro_wagon_has_cargo_type(units, w, COLONIZE_CARGO_TOOLS);
}

/*
 * Colony short on haul cargo: TOOLS stock<20 (5cf6), MUSKETS/HORSES stock<10
 * (inventory muskets_short band; horses same structural threshold), FOOD
 * stock < pop*2 (5cf6 food_short / manual 2 food/colonist). Cite:
 * euro_unit_act §2d; ai_euro_colony_inventory; Colonization.pdf Wagon Train.
 */
static int ai_euro_colony_haul_cargo_short(const ColonizeColony* c, int cargo_type) {
  if (!c || !c->active) {
    return 0;
  }
  if (cargo_type == COLONIZE_CARGO_TOOLS) {
    return c->stock[COLONIZE_CARGO_TOOLS] < 20;
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
 * Surplus load gate: tools≥40 / muskets≥20 / horses≥20 (2× short threshold);
 * FOOD ≥ pop*4 (2× food_short floor). Cite: euro_unit_act §2d; 5cf6 food_short;
 * no invented absolute FOOD stock rates.
 */
static int ai_euro_colony_haul_cargo_surplus(const ColonizeColony* c, int cargo_type) {
  if (!c || !c->active) {
    return 0;
  }
  if (cargo_type == COLONIZE_CARGO_TOOLS) {
    return c->stock[COLONIZE_CARGO_TOOLS] >= 40;
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
 * Load chunk: tools 20 / muskets|horses 10 (short thresholds); FOOD = one turn
 * of colony consumption (pop * TURN_FOOD_PER_COLONIST). Cite: manual 2
 * food/colonist; Colonization.pdf Wagon Train cargo; no invented rates.
 */
static int ai_euro_haul_load_amount(const ColonizeColony* c, int cargo_type) {
  if (cargo_type == COLONIZE_CARGO_TOOLS) {
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
 * Nearest own colony short on cargo_type (or any TOOLS/MUSKETS/HORSES/FOOD when
 * cargo_type < 0). Cite: euro_unit_act §2d wagon haul; 5cf6 shortage tallies;
 * Colonization.pdf Wagon Train.
 */
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
  int best = -1;
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
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_MUSKETS) ||
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_HORSES) ||
                   ai_euro_colony_haul_cargo_short(c, COLONIZE_CARGO_FOOD);
    }
    if (!short_here) {
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

/* Legacy tools-short helper (ship coastal haul + callers). */
static int ai_euro_nearest_tools_short_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
) {
  return ai_euro_nearest_haul_short_colony(
    ctx, nation_id, from_x, from_y, COLONIZE_CARGO_TOOLS, out_x, out_y
  );
}

/*
 * Idle Wagon Train haul (thin 5b66/5d04): free hold capacity or TOOLS /
 * MUSKETS / HORSES / FOOD cargo → AI_MOVE toward nearest matching short own
 * colony (existing unload delivery). On surplus colony with empty capacity,
 * load that cargo via colonies_transfer_to_unit. Cite: euro_unit_act §2d;
 * Colonization.pdf Wagon Train cargo; COLONIZE_CARGO_* + 5cf6 food_short;
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
  const int has_muskets =
    ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_MUSKETS);
  const int has_horses =
    ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_HORSES);
  const int has_food =
    ai_euro_wagon_has_cargo_type(ctx->units, wagon, COLONIZE_CARGO_FOOD);
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, wagon);
  if (!has_tools && !has_muskets && !has_horses && !has_food && !has_cap) {
    return 0;
  }
  /* Prefer cargo already aboard when picking short target. */
  int prefer_cargo = -1;
  if (has_tools) {
    prefer_cargo = COLONIZE_CARGO_TOOLS;
  } else if (has_muskets) {
    prefer_cargo = COLONIZE_CARGO_MUSKETS;
  } else if (has_horses) {
    prefer_cargo = COLONIZE_CARGO_HORSES;
  } else if (has_food) {
    prefer_cargo = COLONIZE_CARGO_FOOD;
  }
  /* On own colony with surplus + free hold → load before haul
   * (tools>muskets>horses>food). Cite: Colonization.pdf Wagon Train. */
  if (has_cap && prefer_cargo < 0) {
    const int cid = colonies_id_at(ctx->colonies, wagon->x, wagon->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      if (c && c->active && c->nation_id == nation_id) {
        static const int k_load_order[] = {
          COLONIZE_CARGO_TOOLS,
          COLONIZE_CARGO_MUSKETS,
          COLONIZE_CARGO_HORSES,
          COLONIZE_CARGO_FOOD
        };
        for (size_t i = 0; i < sizeof(k_load_order) / sizeof(k_load_order[0]); ++i) {
          const int ct = k_load_order[i];
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
  if (!ai_euro_nearest_haul_short_colony(
        ctx, nation_id, wagon->x, wagon->y, prefer_cargo, &tx, &ty)) {
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
 * Pioneer plow/road tile improve planner.
 * Cite: Colonization.pdf Clear/Plow/Road; Hardy Pioneer "Clears forest, plows
 * fields, and builds roads faster" — prefer Hardy when both idle (faster work,
 * not invented yields). units_pioneer_plow clears forest then plows in one API
 * call (real order); units_pioneer_road sets road bit. Requires map.improve.
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
 * On-tile: units_pioneer_plow (clear+plow) or units_pioneer_road. Off-tile:
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
    if (want_plow) {
      if (units_pioneer_plow(ctx->units, u->id, ctx->map, err, sizeof(err))) {
        return 1;
      }
      /* Plow failed (race) — try road if still improvable. */
      if (ai_euro_pioneer_tile_can_road(ctx->map, tx, ty) &&
          units_pioneer_road(ctx->units, u->id, ctx->map, err, sizeof(err))) {
        return 1;
      }
      return 0;
    }
    if (units_pioneer_road(ctx->units, u->id, ctx->map, err, sizeof(err))) {
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
      }
      return;
    }
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
    const ColonizeColony* c = &ctx->colonies->colonies[i];
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
 * wagon), unload hold TOOLS / MUSKETS / HORSES / FOOD onto matching short
 * colony via colonies_transfer_from_unit — structural cargo only (no invented
 * stock). Cite: euro_unit_act §2d wagon matrix; Colonization.pdf Wagon Train;
 * 5cf6 food_short. Unpark #4 remainders PARKED.
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
    return 1;
  }
  return 0;
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
   * Unload TOOLS / MUSKETS / HORSES / FOOD when colony is short on that cargo.
   * Cite: euro_unit_act §2d; COLONIZE_CARGO_* haul deepen; 5cf6 food_short.
   */
  const int n = units_goods_hold_count(ctx->units, wagon->id);
  int moved_total = 0;
  int moved_tools = 0;
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
      if (ct != COLONIZE_CARGO_TOOLS && ct != COLONIZE_CARGO_MUSKETS &&
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
 * Deeper wagon / hire / treasury matrix remains OPEN (unpark #4).
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
 * Europe purchase table Artillery gold (europe_init_purchase_table /
 * original_screenshots/europe/purchase.png) — used for 5d04 war Artillery hire.
 */
#define AI_EURO_ARTILLERY_PURCHASE_GOLD 500

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

/* Resolve dock immigrant name → unit type (strip trailing 's' for pool plurals). */
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
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Expert Farmer")) {
    ty = units_find_type(units, "Expert Farmer");
    if (ty < 0) {
      ty = units_find_type(units, "Farmer");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Expert Lumberjack") || strstr(dock_name, "Lumberjack")) {
    ty = units_find_type(units, "Expert Lumberjack");
    if (ty < 0) {
      ty = units_find_type(units, "Lumberjack");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
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

/* Stock +20 TOOLS on ship holds, else +15 to nearest own colony. Returns delivered. */
static int ai_euro_tools_cargo_or_colony(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
) {
  if (!ctx || !ship) {
    return 0;
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
  return delivered;
}

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
   * NEW WORLD wagon / mid-game hire matrix — thin 5d04 slice (full ~748 PARKED).
   * Europe-dock board while colony_count < 6; at war prefer Soldier/Dragoon;
   * colonies>=2 also Artillery when type exists. Peace: tools_short>30 + Wagon
   * type → hire wagon once; else tools_short>20 prefer Pioneer/Hardy + tools
   * cargo stand-in (ship +20 / colony +15). Case-7 deepen: prefer Hardy/Expert
   * Pioneer or Master Carpenter already on Europe dock (no free spawn fiction).
   * **`lumber_short>20`:** prefer Expert Lumberjack on Europe dock (same consume
   * pattern). Treasury gate (5d04 / Europe hire): skip hire + tools-cargo when gold is
   * below the real cost already used in code (colonist hire_cost, or Artillery
   * purchase 500$ from Europe purchase table).
   */
  const int colonies = inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
  if (colonies >= 6) {
    return;
  }
  /* Colonist / Soldier Europe hire stand-in already used by this planner. */
  const int hire_cost = 200 + diff * 25;
  if ((int)nat->gold < hire_cost) {
    return; /* 5d04 treasury: too poor for Europe hire / tools-cargo */
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
   * Peace thin wagon / tools matrix: Wagon once when tools_short>30; else
   * Pioneer/Hardy when tools_short>20; else 5c3c profession_demand → Pioneer.
   */
  if (hire_ty < 0 && inv && !at_war) {
    if (inv->tools_short > 30 && !ai_euro_nation_has_wagon(ctx->units, nation_id)) {
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
  pax->nation_id = nation_id;
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
   * Wagon hire: load TOOLS onto the wagon before boarding (aboard units are
   * off-map so units_is_transport would fail). Else Pioneer tools ride ship.
   */
  int wagon_loaded = 0;
  if (inv && inv->tools_short > 30 && hired_wagon) {
    wagon_loaded = units_load_goods(ctx->units, uid, COLONIZE_CARGO_TOOLS, 20);
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
  if (ctx->europe) {
    ctx->europe->gold = (int)nat->gold;
  }
  if (inv && inv->profession_demand[0] > 0) {
    inv->profession_demand[0]--;
  }

  /*
   * Thin tools-cargo stand-in (threshold lowered from >40 to >20). Pioneer/Hardy:
   * equip tools + ship hold +20 or nearest-colony +15. Wagon already loaded above;
   * if wagon load failed, fall back to ship/colony delivery.
   * Master Carpenter dock hire skips tools equip (builder, not pioneer).
   */
  if (inv && inv->tools_short > 20) {
    int delivered = 0;
    if (hired_wagon) {
      delivered = wagon_loaded;
      if (delivered <= 0) {
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

  /* D: own colonies — LABOR from tools/food shortage / underpop (5cf6 tallies)
   * or Stockade/Warehouse under construction (carpenter hammers bind).
   * Threatened Stockade deepen: war-peer within MD≤3 + incomplete Stockade →
   * higher LABOR prio so Free Colonist prefers hammers over distant FOUND.
   * Cite: building_production.md Stockade defense; Colonization.pdf fortify;
   * ai_euro_colony_threatened_by_war MD≤3; euro_unit_act §2e. */
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
      if (inv && inv->food_short > 0 && c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
        labor = 1;
      }
      const int construction = ai_euro_colony_wants_construction_labor(ctx->colonies, c);
      if (construction) {
        labor = 1;
      }
      if (labor) {
        int labor_prio = 4 + urgency / 4;
        /* Stockade under threat: bump LABOR above distant FOUND (prio 2) and
         * founder H-bind so Free Colonist stays for defense hammers. */
        if (construction && ctx->col1_ok && ctx->col1 &&
            ai_euro_at_war_any_peer(ctx->col1, nation_id) &&
            ai_euro_colony_threatened_by_war(ctx, nation_id, c)) {
          const ColonizeBuildingType* bt =
            c->building_in_production >= 0
              ? colonies_building_type(ctx->colonies, c->building_in_production)
              : NULL;
          if (bt && strcmp(bt->name, "Stockade") == 0) {
            labor_prio = 6;
          }
        }
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, labor_prio);
      } else {
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_COLONY, 2);
      }
      /* Expand: FOUND via 06ae around colony (coastal prefer when count≥1). */
      int fx = 0;
      int fy = 0;
      const int own_n =
        inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
      if (ai_euro_pick_founding_tile(
            ctx->map, ctx->colonies, nation_id, c->x, c->y, own_n, &fx, &fy)) {
        if (fx != c->x || fy != c->y) {
          ai_goals_upsert_primary(nation_id, fx, fy, AI_GOAL_FOUND, 2);
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
        /* Optional secondary FOUND at tribe tiles (F may raise prio later). */
        if (ctx->col1->tribe) {
          for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
            const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
            int fx = 0;
            int fy = 0;
            if (ai_euro_pick_founding_tile(
                  ctx->map,
                  ctx->colonies,
                  nation_id,
                  (int)t->x,
                  (int)t->y,
                  own,
                  &fx,
                  &fy)) {
              ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 1);
            } else {
              ai_goals_upsert_secondary(nation_id, (int)t->x, (int)t->y, AI_GOAL_FOUND, 1);
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

  /* F: tribe-adjacent FOUND prio 2; alarmed → MILITARY. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      int fx = 0;
      int fy = 0;
      {
        const int own_f =
          inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
        if (ai_euro_pick_founding_tile(
              ctx->map, ctx->colonies, nation_id, t->x, t->y, own_f, &fx, &fy)) {
          ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 2);
        } else {
          ai_goals_upsert_secondary(nation_id, t->x, t->y, AI_GOAL_FOUND, 1);
        }
      }
      if (t->alarm[nation_id].friction > 50) {
        ai_goals_upsert_primary(nation_id, t->x, t->y, AI_GOAL_MILITARY, 3);
      }
    }
  }

  /*
   * G continent stance (thin) — mid-game pressure once established (≥2 colonies).
   * own≥3 + at war: further MILITARY primary prio bump (6→7) — thin stand-in for
   * −0x6790 nation×continent table (decomp ∈ {0,3,4,6}); no invented gold.
   * Deep −0x6790 table stays PARKED.
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
          /* Higher than E's foreign MILITARY (5); own≥3 deepen → 7. */
          const int mil_prio = (own >= 3) ? 7 : 6;
          ai_goals_upsert_primary(
            nation_id, target->x, target->y, AI_GOAL_MILITARY, mil_prio
          );
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
        if (ai_euro_pick_founding_tile(
              ctx->map, ctx->colonies, nation_id, u->x, u->y, colonies, &fx, &fy)) {
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
   * West-explore deepen (0a60 / Atlantic HS): when ship is already on HS and
   * goto is westward, prefer westward HS steps — structural score only, no
   * invented MP (full ocean 20e6 still PARKED / R5).
   */
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  const int on_hs = map_tile_is_high_seas(ctx->map, u->x, u->y);
  const int west_explore = goal_x < u->x;
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
   * explore-yank idle Soldier/Dragoon/Scout before hunt can wake+set AI_MOVE).
   * Passive fortify/sentry — act wakes via units_wake then hunts.
   */
  if (ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id) &&
      ai_euro_is_land_war_hunter(units_display_name(ctx->units, u))) {
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

/* Caravel / Merchantman — New-World cargo haul (manual trade ships). */
static int ai_euro_is_cargo_ship_name(const char* name) {
  return name && (strstr(name, "Caravel") != NULL || strstr(name, "Merchantman") != NULL);
}

/*
 * Nearest own coastal colony that is tools-short (stock[TOOLS]<20) or
 * food-short (stock[FOOD] < pop*2). Cite: euro_unit_act §2d / 5cf6 tallies;
 * sail destination for cargo-ship haul — no invented cargo types.
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
    const int tools_short = c->stock[COLONIZE_CARGO_TOOLS] < 20;
    const int food_short =
      c->population > 0 && c->stock[COLONIZE_CARGO_FOOD] < c->population * 2;
    if (!tools_short && !food_short) {
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
  *out_cx = bx;
  *out_cy = by;
  return 1;
}

/*
 * Idle Caravel/Merchantman trade haul (thin 5b66): free goods-hold capacity or
 * TOOLS / FOOD cargo → AI_SAIL toward coastal water by tools/food-short own
 * colony. TOOLS/FOOD load/unload mirrors wagon §2d via
 * colonies_transfer_to_unit / from_unit. Cite: manual Caravel/Merchantman
 * cargo; Colonization.pdf naval transport / colony supply / Wagon Train
 * pattern; 5cf6 food_short. Peace only — war hunt owns idle ships at war.
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
  const int has_food = ai_euro_wagon_has_cargo_type(ctx->units, ship, COLONIZE_CARGO_FOOD);
  const int has_cap = ai_euro_wagon_has_hold_capacity(ctx->units, ship);
  if (!has_tools && !has_food && !has_cap) {
    return 0;
  }

  /* Adjacent / same-tile short coastal colony + TOOLS/FOOD → structural unload. */
  if (has_tools || has_food) {
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
        if (ct != COLONIZE_CARGO_TOOLS && ct != COLONIZE_CARGO_FOOD) {
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
          }
          break;
        }
      }
      if (unloaded) {
        return 1; /* delivered — stay near colony */
      }
    }
  }

  /* On surplus coastal own colony with free hold → load TOOLS then FOOD.
   * Ships berth on adjacent water (colonies_id_at usually misses). */
  if (has_cap && !has_tools && !has_food) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (!ai_euro_tiles_near(ship->x, ship->y, c->x, c->y)) {
        continue;
      }
      static const int k_ship_load[] = {COLONIZE_CARGO_TOOLS, COLONIZE_CARGO_FOOD};
      for (size_t li = 0; li < sizeof(k_ship_load) / sizeof(k_ship_load[0]); ++li) {
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
  if (!ai_euro_nearest_short_coastal_colony(ctx, nation_id, ship->x, ship->y, &cx, &cy)) {
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

/* Galleon / Frigate — war passenger transport (Europe purchase table). */
static int ai_euro_is_war_transport_name(const char* name) {
  return name && (strstr(name, "Galleon") != NULL || strstr(name, "Frigate") != NULL);
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
 * At war: ship with Soldier cargo adjacent to own threatened coastal colony →
 * unload one Soldier onto the colony tile (reinforce). Complements board +
 * war-transport sail-to-threatened-port. Cite: Colonization.pdf naval
 * transport / Defending a Colony; euro_unit_act §2b2; units_unload_passenger
 * (same path as king MoW unload / settle landfall). No invented combat bonus.
 * Returns 1 if a Soldier was unloaded.
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
  if (!ctx->col1_ok || !ctx->col1 || !ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
    return 0;
  }
  if (ship->cargo_count <= 0) {
    return 0;
  }
  /* Prefer a Soldier passenger when present. */
  int pax_id = -1;
  for (int c = 0; c < ship->cargo_count && c < COLONIZE_UNIT_CARGO_MAX; ++c) {
    const ColonizeUnit* p = units_get_const(ctx->units, ship->cargo_ids[c]);
    if (!p || !p->active) {
      continue;
    }
    const char* pname = units_display_name(ctx->units, p);
    if (pname && strstr(pname, "Soldier") != NULL) {
      pax_id = ship->cargo_ids[c];
      break;
    }
  }
  if (pax_id < 0) {
    return 0;
  }
  /* Adjacent/same-tile own coastal colony threatened by war-peer. */
  int dest_x = -1;
  int dest_y = -1;
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
    if (!ai_euro_colony_threatened_by_war(ctx, nation_id, col)) {
      continue;
    }
    dest_x = col->x;
    dest_y = col->y;
    break;
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
 * PARKED: FUN_157e_004a vet/Drake/damage combat×8 mods — no unit damage byte
 * wired yet; prefer lower type defense only (closest real hook).
 */
static int ai_euro_naval_foe_toughness(const ColonizeUnitPool* units, const ColonizeUnit* f) {
  if (!units || !f) {
    return 9999;
  }
  const ColonizeUnitType* t = units_type(units, f->type_index);
  int def = t ? t->defense : 0;
  if (def < 0) {
    def = 0;
  }
  return def;
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
 * else lower type defense. PARKED: FUN_157e_004a vet/Drake/damage combat×8.
 * Cite: euro_unit_act §2f; Europe Privateer/Frigate purchase; fandom Drake.
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
    const int tough = ai_euro_naval_foe_toughness(ctx->units, f);
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

/* Effective defense for thin 20e6 adjacent-foe pick (fortified ×2). */
static int ai_euro_land_foe_toughness(const ColonizeUnitPool* units, const ColonizeUnit* f) {
  if (!units || !f) {
    return 9999;
  }
  const ColonizeUnitType* t = units_type(units, f->type_index);
  int def = t ? t->defense : 0;
  if (def < 0) {
    def = 0;
  }
  if (ai_euro_land_is_fortified(f)) {
    def *= 2;
  }
  return def;
}

/*
 * Best adjacent war foe for land attack (thin 20e6 combat scoring): prefer
 * lower effective defense / non-fortified. Returns foe unit id or -1.
 *
 * PARK: deep FUN_521d_20e6 combat scoring (vet/terrain/artillery tables,
 * multi-hex threat weights, −0x6790) — thin adjacent-toughness pick + 2-step
 * goto advance only. Do not invent combat×8 / damage-byte mods here.
 */
static int ai_euro_land_best_adjacent_foe(ColonizeTurnContext* ctx, const ColonizeUnit* u) {
  if (!ctx || !ctx->units || !u || !u->active || units_is_sea(ctx->units, u->id)) {
    return -1;
  }
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int best_id = -1;
  int best_tough = 0;
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
    const int tough = ai_euro_land_foe_toughness(ctx->units, f);
    if (best_id < 0 || tough < best_tough) {
      best_id = foe;
      best_tough = tough;
    }
  }
  return best_id;
}

/* Attack adjacent enemy land unit while at war (prefer weaker foe). */
static void ai_euro_land_try_adjacent_attack(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  const int foe = ai_euro_land_best_adjacent_foe(ctx, u);
  if (foe < 0) {
    return;
  }
  const ColonizeUnit* f = units_get_const(ctx->units, foe);
  if (!f) {
    return;
  }
  ai_euro_try_attack(ctx, u, f->x, f->y);
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
    const char* name = units_display_name(ctx->units, p);
    /* Treasure stays aboard for Europe sail — do not landfall as settler.
     * Cite: Colonization.pdf Treasure Trains → Europe gold (cash on Europe/HS). */
    if (ai_euro_is_treasure_name(name)) {
      continue;
    }
    int sc = 2;
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
  if (ai_goals_best_found_tile(nation_id, &fx, &fy) &&
      colonies_can_found(ctx->colonies, ctx->map, fx, fy)) {
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
 * FUN_521d_5b66 — scoring gate + case 0x0b arms; case 7 hire economy thin
 * (Pioneer tools-delivery here; wagon/tools dock hire lives in 5d04 planning).
 */
static void ai_euro_unit_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (!ctx || !u || !u->active || u->moves_left <= 0 || u->aboard_ship_id >= 0) {
    return;
  }

  const int is_ship = ai_euro_is_ship_type(ctx->units, u->id);
  const int is_goto = units_orders_follow_goto(u->orders);

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
        (strstr(board_name, "Soldier") != NULL || strstr(board_name, "Dragoon") != NULL ||
         strstr(board_name, "Artillery") != NULL || strstr(board_name, "Cannon") != NULL) &&
        ai_euro_try_soldier_board_transport(ctx, nation_id, u)) {
      return;
    }
  }

  /*
   * War military unload — before move-scoring gate. Galleon/Frigate are not
   * cargo-ship deferred, so 20e6 gate can abort the ship act before the war
   * unload arm. Drop Soldier at threatened coastal colony first. Cite:
   * Colonization.pdf naval transport; euro_unit_act §2b2.
   */
  if (is_ship && ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id) &&
      !ai_euro_in_europe(u->x, u->y)) {
    (void)ai_euro_try_unload_military_threatened(ctx, nation_id, u);
  }

  /*
   * Early move-scoring gate (~90552): if orders!=goto (or fresh), call 20e6;
   * non-zero return aborts act. Linux: always score when not already on goto.
   * Treasure / Missionary: defer course to act-level coast / CONTACT routing
   * (do not FOUND-yank before treasure coast or missionary mission hunt).
   */
  if (!is_goto) {
    const char* gate_name = units_display_name(ctx->units, u);
    const int defer_gate =
      ai_euro_is_treasure_name(gate_name) || ai_euro_is_missionary_name(gate_name) ||
      ai_euro_type_is_wagon_name(gate_name) || ai_euro_is_cargo_ship_name(gate_name);
    if (!defer_gate && ai_euro_move_scoring_gate(ctx, u, nation_id)) {
      return;
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
      (void)ai_euro_try_ship_trade_haul(ctx, nation_id, u);
    }
    if (at_war && !ai_euro_in_europe(u->x, u->y) && !treasure_aboard) {
      /* Drop Soldier at threatened own coastal colony before hunt sail. */
      (void)ai_euro_try_unload_military_threatened(ctx, nation_id, u);
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
    /* War reinforce unload after sail arrival (Soldier → threatened colony). */
    if (u->active && at_war && !ai_euro_in_europe(u->x, u->y)) {
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
    if (u->active && !ai_euro_in_europe(u->x, u->y)) {
      ai_euro_unload_settle(ctx, u, nation_id);
    }
    return;
  }

  /* Case 0x0b land: bind primary goal (role-aware scan). */
  const char* uname = units_display_name(ctx->units, u);
  const int is_land_hunter = ai_euro_is_land_war_hunter(uname);
  const int is_scout = uname && strstr(uname, "Scout") != NULL;
  const int is_treasure = ai_euro_is_treasure_name(uname);
  const int is_missionary = ai_euro_is_missionary_name(uname);
  const int at_war_land =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
  int land_war_hunted = 0;
  int scout_explored = 0;
  int treasure_routed = 0;
  int missionary_contacted = 0;

  /*
   * Thin LCR (FUN_65dd_0004 scaffold): Scout standing on rumour clears it;
   * de Soto → reveal radius only. No invented gold / FoY / hostile table.
   * Cite: units_resolve_lcr_rumour; Colonization.pdf Lost City Rumours.
   */
  if (is_scout && ctx->map && map_tile_has_rumour(ctx->map, u->x, u->y)) {
    if (units_resolve_lcr_rumour(
          ctx->units,
          u->id,
          ctx->map,
          ctx->col1_ok ? ctx->col1 : NULL,
          ctx->rng
        )) {
      scout_explored = 1;
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
   * Peace colony-defense wake (extend §2d3 fortify): idle/fortified Soldier or
   * Dragoon on own colony wakes via units_wake when a foreign Euro land unit
   * enters MD≤2, then hunts toward that threat. Manual: "fortify soldiers,
   * dragoons, army, cavalry, or artillery" (Colonization.pdf Defending a
   * Colony). War already has global fortify-wake (§2c); this is the peace
   * border garrison. Adjacent attack may declare war via existing try_attack.
   * Cite: Colonization.pdf fortify defense; units_wake; euro_unit_act §2d3.
   * No invented combat bonuses.
   */
  int peace_border_hunted = 0;
  if (!at_war_land && !land_war_hunted && uname &&
      (strstr(uname, "Soldier") != NULL || strstr(uname, "Dragoon") != NULL) &&
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
        const ColonizeUnitType* uty = units_type(ctx->units, u->type_index);
        const int fresh = uty && u->moves_left >= uty->movement;
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
   * coastal colony (or coastal land if none). At coastal own colony with ship
   * space → board + AI_SAIL Europe (eastern HS / east water). Cite:
   * Colonization.pdf Treasure Trains — park coastal → Galleon / king transport.
   * Europe cash: ai_euro_try_cash_treasure_europe (LE16 hold / europe_cash_treasure).
   * Preserve goto vs FOUND/LABOR yank. No invented ransom/gold.
   */
  if (is_treasure) {
    if (ai_euro_try_cash_treasure_europe(ctx, nation_id, u)) {
      return;
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
   * MUSKETS / HORSES / FOOD → AI_MOVE toward matching short colony (unload via
   * existing delivery). Cite: euro_unit_act §2d; Colonization.pdf Wagon Train;
   * 5cf6 food_short.
   */
  int wagon_hauled = 0;
  if (!treasure_routed && uname && ai_euro_type_is_wagon_name(uname) &&
      !ai_euro_land_is_fortified(u)) {
    if (ai_euro_try_wagon_haul(ctx, nation_id, u)) {
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
   * Fur Trader workplace assign (act-level): admit + colonies_assign_workplace
   * on matching craft building. Cite: Colonization.pdf Skills Chart;
   * docs/building_production.md Distiller/Weaver/Tobacconist/Blacksmith/
   * Gunsmith/Fur Trader craft chains. Parallel to planter field-assign.
   */
  int workplace_assigned = 0;
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !miner_fielded && !farmer_fielded && !fisherman_fielded && !planter_fielded &&
      !land_war_hunted && !peace_border_hunted && !scout_explored && uname &&
      (strstr(uname, "Distiller") != NULL || strstr(uname, "Weaver") != NULL ||
       strstr(uname, "Tobacconist") != NULL || strstr(uname, "Blacksmith") != NULL ||
       strstr(uname, "Gunsmith") != NULL || strstr(uname, "Fur Trader") != NULL)) {
    if (ai_euro_try_expert_workplace_assign(ctx, nation_id, u)) {
      workplace_assigned = 1;
      if (!u->active) {
        return; /* admitted + workplace-assigned */
      }
    }
  }

  /*
   * Peace fortify (case 0x0b fortify arm): idle Soldier on own colony tile →
   * FORTIFY if not already. Overrides explore/FOUND scoring-gate yank while
   * on-colony (defense). Cite: euro_unit_act §2 fortify colony-check → 'F';
   * Colonization.pdf fortify defense. At war: wake+hunt owns soldiers instead.
   */
  if (!at_war_land && !peace_border_hunted && !treasure_routed && !wagon_hauled &&
      !pioneer_improved && !lumberjack_fielded && !miner_fielded && !farmer_fielded &&
      !fisherman_fielded && !planter_fielded && !workplace_assigned && !scout_explored &&
      !land_war_hunted && uname && strstr(uname, "Soldier") != NULL &&
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
        if (!keep_mil) {
          units_order_fortify(ctx->units, u->id);
          return; /* stay fortified — skip FOUND/explore yank */
        }
      }
    }
  }

  /*
   * Artillery fortify after siege (case 0x0b fortify arm): idle Artillery on
   * own/captured colony → FORTIFY. Artillery is not a land war hunter (no
   * wake+hunt), so garrison holds at peace and at war. Cite: euro_unit_act
   * fortify colony-check → 'F'; Colonization.pdf fortify defense / Artillery
   * siege; mirror king post-capture fortify (Regular) for Euro Artillery.
   */
  if (!treasure_routed && !wagon_hauled && !pioneer_improved && !lumberjack_fielded &&
      !miner_fielded && !farmer_fielded && !fisherman_fielded && !planter_fielded &&
      !workplace_assigned && !scout_explored && !land_war_hunted && !peace_border_hunted &&
      uname && (strstr(uname, "Artillery") != NULL || strstr(uname, "Cannon") != NULL) &&
      !ai_euro_land_is_fortified(u) && ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
    if (cid >= 0) {
      const ColonizeColony* c = colonies_get(ctx->colonies, cid);
      if (c && c->active && c->nation_id == nation_id) {
        units_order_fortify(ctx->units, u->id);
        return;
      }
    }
  }

  /*
   * Missionary CONTACT (act-level): peace + Jesuit/Missionary, not fleeing
   * (Alarm ≥55 adjacent) → CONTACT at nearest tribe without mission
   * (mission==0xff) + AI_MOVE. Convert when adjacent is ai_contact.
   * Idle Jesuit prefers convert CONTACT over Scout explore / FOUND yank
   * (missionary_contacted preserves goto). Cite: Colonization.pdf Establishing
   * a Mission; euro_unit_act §2c6; indian_contact.md convert pulse.
   */
  if (!at_war_land && is_missionary &&
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
    const int is_soldier = uname && strstr(uname, "Soldier");
    const int is_founder =
      uname && !is_soldier &&
      (strstr(uname, "Pioneer") || strstr(uname, "Hardy") || strstr(uname, "Free Colonist") ||
       strstr(uname, "Colonist"));

    /* Soldiers: MILITARY/CONTACT first; founders: FOUND over LABOR/COLONY —
     * except threatened Stockade LABOR (Free Colonist MD≤3) beats distant FOUND. */
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
      int threat_stockade_labor = 0;
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
            threat_stockade_labor = 1;
            ai_goals_upsert_primary(nation_id, tc->x, tc->y, AI_GOAL_LABOR, 6);
            break;
          }
        }
      }
      if (!threat_stockade_labor) {
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
        }
      }
      if (bx >= 0) {
        goal_x = bx;
        goal_y = by;
        goal_code = code;
        ai_goals_upsert_primary(
          nation_id, bx, by, code, (food_emergency || threat_stockade_bind) ? 5 : 4
        );
      }
    }
  }

  if (goal_code == AI_GOAL_FOUND && u->x == goal_x && u->y == goal_y) {
    ai_euro_found_with_unit(ctx, u, nation_id);
    return;
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
    /* Peace fortify fallback (case 0x0b): idle Soldier on own colony only. */
    const char* name = units_display_name(ctx->units, u);
    if (!at_war_land && name && strstr(name, "Soldier") && ctx->colonies &&
        !ai_euro_land_is_fortified(u)) {
      const int cid = colonies_id_at(ctx->colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* c = colonies_get(ctx->colonies, cid);
        if (c && c->active && c->nation_id == nation_id) {
          units_order_fortify(ctx->units, u->id);
        }
      }
    }
  }

  if (u->active && at_war_land && is_land_hunter && !ai_euro_land_is_fortified(u)) {
    ai_euro_land_try_adjacent_attack(ctx, u);
  }

  /*
   * Sticky CONTACT re-hunt: if moves remain and an adjacent foreign Euro is
   * at war, try_attack the weakest adjacent foe once more (dispatcher sticky
   * waves still apply). Deep 20e6 multi-step combat scoring PARKED.
   */
  if (u->active && u->moves_left > 0 && ctx->col1_ok && ctx->col1 &&
      !units_is_sea(ctx->units, u->id)) {
    const int foe = ai_euro_land_best_adjacent_foe(ctx, u);
    if (foe >= 0) {
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      /* Sticky CONTACT is Euro-peer war only (Indians stay on contact/raid paths). */
      if (f && f->nation_id >= 0 && f->nation_id <= 3) {
        ai_euro_try_attack(ctx, u, f->x, f->y);
      }
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
  /* Peace Stockade→Warehouse→Docks, coastal Drydock→Shipyard, then Stuyvesant
   * Custom House; before LABOR. */
  ai_euro_prefer_peace_construction(ctx, nation_id);
  ai_euro_prefer_coastal_drydock(ctx, nation_id);
  ai_euro_prefer_coastal_shipyard(ctx, nation_id);
  ai_euro_prefer_custom_house(ctx, nation_id);
  ai_euro_colony_goals(ctx, nation_id);

  /* Opportunistic balance after plan (separate from timer slot). */
  ai_diplo_euro_balance(ctx, nation_id);

  /* Treasure → Europe gold: Expected→Harbor due ships + live Europe/HS units
   * (moves_left may be 0 on Europe dock ships). Cite: Treasure Trains. */
  ai_euro_try_expected_treasure_harbor(ctx, nation_id);
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    (void)ai_euro_try_cash_treasure_europe(ctx, nation_id, u);
  }

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
