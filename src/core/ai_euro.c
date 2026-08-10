#include "core/ai_euro.h"

#include "core/ai_diplo.h"
#include "core/ai_goals.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/colony_production.h"
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
  /* +0x1c thin: starvation / wagon / coastal / small-colony. */
  {
    const int pop = c->colonist_count > 0 ? c->colonist_count : c->population;
    const int need = pop * 2; /* TURN_FOOD_PER_COLONIST */
    if (c->stock[COLONIZE_CARGO_FOOD] < need) {
      c->colony_flags |= COLONIZE_COLONY_FLAG_STARVATION;
    } else {
      c->colony_flags =
        (uint8_t)(c->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_STARVATION);
    }
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
  /* Defense chain before storage/docks so Fort % live after Stockade. */
  const int prefer_def[] = {stockade_id, fort_id, fortress_id, warehouse_id, docks_id};
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
    const int has_wh =
      warehouse_id >= 0 && warehouse_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[warehouse_id];
    const int use_exp =
      has_wh && whe_id >= 0 && ai_euro_colony_near_warehouse_cap(ctx->colonies, c);
    const int* prefer = use_exp ? prefer_exp : prefer_def;
    const size_t nprefer =
      use_exp ? (sizeof(prefer_exp) / sizeof(prefer_exp[0]))
              : (sizeof(prefer_def) / sizeof(prefer_def[0]));
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int pick = -1;
    for (size_t p = 0; p < nprefer; ++p) {
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
 * Capitol prefer (5d04): Stockade owned, no Capitol, idle → Capitol when
 * buildable. Liberty bells / SoL. Cite: building_production.md Capitol;
 * Colonization.pdf. After Iron Works; before craft upgrades. Peace or war.
 */
static void ai_euro_prefer_capitol(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int stockade_id = colonies_find_building(ctx->colonies, "Stockade");
  const int fort_id = colonies_find_building(ctx->colonies, "Fort");
  const int fortress_id = colonies_find_building(ctx->colonies, "Fortress");
  const int capitol_id = colonies_find_building(ctx->colonies, "Capitol");
  const int exp_id = colonies_find_building(ctx->colonies, "Capitol Expansion");
  if (capitol_id < 0) {
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
    if (capitol_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[capitol_id]) {
      continue;
    }
    if (exp_id >= 0 && exp_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[exp_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == capitol_id) {
        ok = 1;
        break;
      }
    }
    if (!ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, capitol_id);
  }
}

/*
 * Capitol Expansion prefer (5d04): Capitol owned, no Expansion, idle → Expansion.
 * Cite: building_production.md Capitol Expansion. After Capitol.
 */
static void ai_euro_prefer_capitol_expansion(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || nation_id < 0 || nation_id >= 4) {
    return;
  }
  const int capitol_id = colonies_find_building(ctx->colonies, "Capitol");
  const int exp_id = colonies_find_building(ctx->colonies, "Capitol Expansion");
  if (exp_id < 0 || capitol_id < 0) {
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
    if (capitol_id >= COLONIZE_BUILDING_TYPES_MAX || !c->has_building[capitol_id]) {
      continue;
    }
    if (exp_id < COLONIZE_BUILDING_TYPES_MAX && c->has_building[exp_id]) {
      continue;
    }
    int buildable[COLONIZE_BUILDING_TYPES_MAX];
    const int n =
      colonies_list_buildable(ctx->colonies, c->id, buildable, COLONIZE_BUILDING_TYPES_MAX, &opts);
    int ok = 0;
    for (int b = 0; b < n; ++b) {
      if (buildable[b] == exp_id) {
        ok = 1;
        break;
      }
    }
    if (!ok) {
      continue;
    }
    (void)colonies_set_construction(ctx->colonies, c->id, exp_id);
  }
}

/*
 * Craft house/shop/factory prefer (5d04): House→Shop→Factory for rum/cotton/
 * tobacco/fur when raw stock≥20. Factories need Adam Smith. Cite:
 * building_production craft chains; dock craft hire stock≥20 gate. After
 * Capitol Expansion.
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
  ctx->europe->gold = (int)nat->gold;
  ctx->europe->tax_percent = (int)nat->tax_rate;
  europe_tick_voyages(ctx->europe, ctx->units);
  nat->gold = (uint32_t)(ctx->europe->gold < 0 ? 0 : ctx->europe->gold);
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
    int worked = 0;
    if (want_plow) {
      if (units_pioneer_plow(ctx->units, u->id, ctx->map, err, sizeof(err))) {
        worked = 1;
      } else if (ai_euro_pioneer_tile_can_road(ctx->map, tx, ty) &&
                 units_pioneer_road(ctx->units, u->id, ctx->map, err, sizeof(err))) {
        worked = 1;
      }
    } else if (units_pioneer_road(ctx->units, u->id, ctx->map, err, sizeof(err))) {
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
  if (strstr(dock_name, "Fisherman") || strstr(dock_name, "Fishermen")) {
    ty = units_find_type(units, "Expert Fisherman");
    if (ty < 0) {
      ty = units_find_type(units, "Fisherman");
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
  if (strstr(dock_name, "Ore Miner")) {
    ty = units_find_type(units, "Expert Ore Miner");
    if (ty < 0) {
      ty = units_find_type(units, "Ore Miner");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Silver Miner")) {
    ty = units_find_type(units, "Expert Silver Miner");
    if (ty < 0) {
      ty = units_find_type(units, "Silver Miner");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Gunsmith")) {
    ty = units_find_type(units, "Master Gunsmith");
    if (ty < 0) {
      ty = units_find_type(units, "Gunsmith");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Blacksmith")) {
    ty = units_find_type(units, "Master Blacksmith");
    if (ty < 0) {
      ty = units_find_type(units, "Blacksmith");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
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
      ty = units_find_type(units, "Free Colonist");
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
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Elder Statesman") || strstr(dock_name, "Elder Statesmen")) {
    ty = units_find_type(units, "Elder Statesman");
    if (ty < 0) {
      ty = units_find_type(units, "Statesman");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Firebrand") || strstr(dock_name, "Preacher")) {
    ty = units_find_type(units, "Firebrand Preacher");
    if (ty < 0) {
      ty = units_find_type(units, "Preacher");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Teacher")) {
    ty = units_find_type(units, "Expert Teacher");
    if (ty < 0) {
      ty = units_find_type(units, "Teacher");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Distiller")) {
    ty = units_find_type(units, "Master Distiller");
    if (ty < 0) {
      ty = units_find_type(units, "Distiller");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Weaver")) {
    ty = units_find_type(units, "Master Weaver");
    if (ty < 0) {
      ty = units_find_type(units, "Weaver");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Tobacconist")) {
    ty = units_find_type(units, "Master Tobacconist");
    if (ty < 0) {
      ty = units_find_type(units, "Tobacconist");
    }
    if (ty < 0) {
      ty = units_find_type(units, "Free Colonist");
    }
    return ty;
  }
  if (strstr(dock_name, "Fur Trader")) {
    ty = units_find_type(units, "Master Fur Trader");
    if (ty < 0) {
      ty = units_find_type(units, "Fur Trader");
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
   * Wartime Privateer spawn is ai_diplo_euro_balance (not this buy ladder).
   * Cite: FUN_521d_5c3c / 5d04; europe_init_purchase_table; purchase.png;
   * euro_unit_act war transport / Frigate hunt.
   */
  if (!ship && (int)nat->gold >= AI_EURO_CARAVEL_PURCHASE_GOLD) {
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
          if (ctx->europe) {
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
  if (ctx->europe) {
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
                  ((c->colony_flags & COLONIZE_COLONY_FLAG_STARVATION) != 0);
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
        /* Thin demand latch when Linux detects LABOR need; full FUN_5952_035e
         * seed (local_76 / 0x8d72) PARKED — do not invent tallies. */
        if (c->labor_shortage == 0) {
          c->labor_shortage = 1;
        }
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, labor_prio);
      } else {
        /* euro_dispatcher: COLONY code 5|8 — 8 if +0x1b bit1 (MoW). */
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
       * and quota==0 → 1. Full threat>>3 FUN_5952_035e seed PARKED.
       */
      if (c->garrison_quota == 0 && ctx->units) {
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
              ctx->map, ctx->colonies, nation_id, t->x, t->y, own_f, &fx, &fy)) {
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

  /*
   * G continent stance (thin) — mid-game pressure once established (≥2 colonies).
   * At war MILITARY primary prio: own≥2 → 6, ≥3 → 7, ≥4 → 8 — thin stand-in for
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
          /* Higher than E's foreign MILITARY (5); ladder own≥2/3/4 → 6/7/8. */
          int mil_prio = 6;
          if (own >= 4) {
            mil_prio = 8;
          } else if (own >= 3) {
            mil_prio = 7;
          }
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
          /* Prefer tribe-adjacent FOUND (never the village tile itself). */
          if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
              ctx->col1->head.tribe_count > 0) {
            const ColonizeCol1Tribe* t0 = &ctx->col1->tribe[0];
            const int own_g =
              inv ? inv->colony_count : ai_euro_colony_count(ctx->colonies, nation_id);
            if (ai_euro_pick_founding_tile(
                  ctx->map, ctx->colonies, nation_id, (int)t0->x, (int)t0->y, own_g, &tx,
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
   * Naval/ocean branch of FUN_521d_20e6 (thin extract): prefer water tiles
   * that reduce Chebyshev/Manhattan distance to goal; avoid land; slight
   * preference for high-seas / west when goal is west of ship.
   * West-explore deepen (0a60 / Atlantic HS): when ship is already on HS and
   * goto is westward, prefer westward HS steps — structural score only, no
   * invented MP (full ocean 20e6 still PARKED / R5).
   * East-Europe deepen: when goto is eastward (Treasure/Europe exit /
   * eastern HS), prefer eastward HS steps — complement west-explore.
   * Combat deepen (FUN_157e_004a / thin 20e6): when at war, bonus for steps
   * that sit adjacent to a weaker foe ship (prefer engage); small penalty
   * adjacent to tougher foe. Cite: Colonization.pdf Treasure Trains → Europe;
   * units_find_eastern_high_seas_tile; FUN_157e_004a.
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
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, u->nation_id);
  const int own_tough = at_war ? ai_euro_land_foe_toughness(ctx, ctx->units, u) : 0;
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
    /* Thin land combat 20e6: prefer closing on weaker adjacent war foes. */
    if (at_war) {
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
        const int ft = ai_euro_land_foe_toughness(ctx, ctx->units, f);
        if (ft < own_tough) {
          score += 14;
        } else if (ft > own_tough) {
          score -= 6;
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

/* Caravel / Merchantman / Galleon — New-World cargo haul (manual trade ships). */
static int ai_euro_is_cargo_ship_name(const char* name) {
  return name &&
         (strstr(name, "Caravel") != NULL || strstr(name, "Merchantman") != NULL ||
          strstr(name, "Galleon") != NULL);
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
 * At war: ship with military cargo adjacent to own threatened coastal colony →
 * unload one passenger onto the colony tile (reinforce). Prefer Soldier, else
 * Regular/Continental Army, else Dragoon/Continental Cavalry, else
 * Artillery/Cannon — mirror king MoW unload ladder + board list. Complements
 * board + war-transport sail-to-threatened-port. Cite: Colonization.pdf naval
 * transport / Defending a Colony; euro_unit_act §2b2; king_ref MoW unload
 * Regular-prefer else Dragoon; units_unload_passenger. Returns 1 if a military
 * passenger was unloaded.
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
 * Occupied goods holds — Col1 unit+0x0c / DS:0x3150 holds_occupied.
 * FUN_157e_004a subtracts this from ship combat×8 for type 0x0d..0x12.
 */
static int ai_euro_ship_holds_occupied(const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    const int amt = u->hold_goods_amount[i];
    if (amt > 0 && amt < 255) {
      ++n;
    }
  }
  return n;
}

/*
 * Effective defense for thin 20e6 naval adjacent-foe pick.
 * FUN_157e_004a peels:
 *   - Privateer (type 0x0b) + ship_damaged (0x3148 bit7 / col1_unknown15 bit7) → −2
 *   - ship band: subtract holds_occupied (0x3150)
 *   - Drake Privateer +50% (×3/2) when FF owned — mirrors units_drake_scale_strength
 * Cite: FUNCTION_CATALOG FUN_157e_004a; fandom Drake; col1_save.h ship_damaged.
 */
static int ai_euro_naval_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f
) {
  if (!units || !f) {
    return 9999;
  }
  const ColonizeUnitType* t = units_type(units, f->type_index);
  int def = t ? t->defense : 0;
  if (def < 0) {
    def = 0;
  }
  /* FUN_157e_004a: type==0x0b Privateer + damaged bit → base −2 before ×8. */
  if (t && strstr(t->name, "Privateer") != NULL && (f->col1_unknown15 & 0x80u) != 0) {
    def -= 2;
    if (def < 0) {
      def = 0;
    }
  }
  /* FUN_157e_004a: ship type band subtracts holds_occupied (0x3150). */
  def -= ai_euro_ship_holds_occupied(f);
  if (def < 0) {
    def = 0;
  }
  if (t && strstr(t->name, "Privateer") != NULL && ctx && ctx->col1_ok && ctx->col1 &&
      founding_fathers_nation_has(ctx->col1, f->nation_id, FF_FRANCIS_DRAKE)) {
    def = (def * 3) / 2;
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
 * Matches units_resolve_land_combat_ff fort%: own-colony Stockade/Fort/Fortress
 * percent bonus replaces fortified ×2 when present.
 * FUN_157e_004a peel: Soldier/Dragoon with profession 0x15 (UNITS_JOB_SOLDIER)
 * → +50% (×3/2). Cite: building_production.md; FUN_157e_004a; fandom Washington
 * veteran. PARK: combat×8 table scale / damage-byte (land N/A).
 */
static int ai_euro_land_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f
) {
  if (!units || !f) {
    return 9999;
  }
  const ColonizeUnitType* t = units_type(units, f->type_index);
  int def = t ? t->defense : 0;
  if (def < 0) {
    def = 0;
  }
  int fort_bonus = 0;
  if (ctx && ctx->colonies && f->nation_id >= 0 && f->nation_id <= 3) {
    fort_bonus = ai_euro_colony_fort_bonus_at(ctx->colonies, f->x, f->y, f->nation_id);
  }
  if (fort_bonus > 0) {
    def = def + (def * fort_bonus) / 100;
  } else if (ai_euro_land_is_fortified(f)) {
    def *= 2;
  }
  if (t && f->profession == UNITS_JOB_SOLDIER &&
      (strstr(t->name, "Soldier") != NULL || strstr(t->name, "Dragoon") != NULL ||
       strstr(t->name, "Continental") != NULL)) {
    def = (def * 3) / 2;
  }
  return def;
}

/*
 * Best adjacent war foe for land attack (thin 20e6 combat scoring): prefer
 * lower effective defense / non-fortified / weaker colony fort / non-veteran.
 * Artillery prefers higher fort % (siege — king_ref Artillery adjacent-fort).
 * Non-siege: at equal toughness prefer Treasure (loot — Colonization.pdf
 * Treasure Trains / @LOOTCASH). Returns foe unit id or -1.
 *
 * PARK: deep FUN_521d_20e6 combat scoring (terrain/artillery tables,
 * multi-hex threat weights, −0x6790) — thin adjacent-toughness pick + 2-step
 * goto advance only. Vet/Drake from FUN_157e_004a Done above; no invented
 * combat×8 / damage-byte mods.
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
    if (siege) {
      if (best_id < 0 || fort > best_fort || (fort == best_fort && tough < best_tough)) {
        best_id = foe;
        best_fort = fort;
        best_tough = tough;
        best_treasure = treasure;
      }
    } else if (
      best_id < 0 || tough < best_tough ||
      (tough == best_tough && treasure && !best_treasure)
    ) {
      best_id = foe;
      best_tough = tough;
      best_treasure = treasure;
    }
  }
  return best_id;
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
        (ai_euro_is_colony_garrison_name(board_name) || ai_euro_is_artillery_name(board_name)) &&
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
      /*
       * FUN_48d3_048e: place on HS near landfall goto — never prefer_y from
       * Europe sentinel (~228+nation); that pinned rivals to southern ice.
       */
      int lx = 0;
      int ly = 0;
      ai_euro_resolve_landfall_goto(ctx, u, &lx, &ly);
      int hx = lx;
      int hy = ly;
      int placed = 0;
      if ((map_tile_is_high_seas(ctx->map, lx, ly) || map_tile_is_water(ctx->map, lx, ly)) &&
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
        u->x = hx;
        u->y = hy;
        ai_euro_sync_aboard_cargo_xy(ctx->units, u);
        int fx = 0;
        int fy = 0;
        if (ai_goals_best_found_tile(nation_id, &fx, &fy)) {
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, fx, fy);
        } else {
          /* Sail west from landfall toward New World coast (same latitude). */
          const int tx = lx > 8 ? lx - 8 : (hx > 2 ? hx - 8 : 0);
          const int ty = ly;
          ai_euro_set_goto(u, UNITS_ORDER_AI_SAIL, tx < 0 ? 0 : tx, ty);
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
      if (!ai_euro_try_de_witt_ship_trade(ctx, nation_id, u)) {
        if (!ai_euro_try_ship_trade_haul(ctx, nation_id, u)) {
          if (!ai_euro_try_ship_europe_export(ctx, nation_id, u)) {
            (void)ai_euro_try_privateer_europe_loot_sail(ctx, nation_id, u);
          }
        }
      }
    }
    if (at_war && !ai_euro_in_europe(u->x, u->y) && !treasure_aboard) {
      /* Drop Soldier at threatened own coastal colony before hunt sail. */
      (void)ai_euro_try_unload_military_threatened(ctx, nation_id, u);
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

  if ((goal_code == AI_GOAL_LABOR || goal_code == AI_GOAL_COLONY ||
       goal_code == AI_GOAL_COLONY_ALT) &&
      ctx->colonies) {
    const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
    if (cid >= 0 && u->x == goal_x && u->y == goal_y) {
      /* Garrison/Artillery stay for fortify quota — do not admit as LABOR. */
      if (!ai_euro_is_colony_garrison_name(uname) && !ai_euro_is_artillery_name(uname)) {
        ai_euro_join_colony(ctx, u, cid);
        return;
      }
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
    ai_euro_land_try_adjacent_attack(ctx, u);
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

  /* Colony fortification defense for adjacent resolve_land_combat (not only try_move). */
  units_set_combat_colonies(ctx->colonies);

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
  ai_euro_prefer_capitol(ctx, nation_id);
  ai_euro_prefer_capitol_expansion(ctx, nation_id);
  ai_euro_prefer_craft_upgrades(ctx, nation_id);
  ai_euro_colony_goals(ctx, nation_id);

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
