#include "core/colony.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/font.h"
#include "core/ss.h"
#include "core/units.h"
#include "platform/diagnostics.h"

/* ICONS.SS #0–3: European colonies by fortification (none / stockade / fort / fortress). */
#define COLONY_MAP_ICON_STOCKADE 0
#define COLONY_MAP_ICON_FORT 1
#define COLONY_MAP_ICON_FORTRESS 2
#define COLONY_MAP_ICON_NONE 3

static int colony_map_icon(const ColonizeColonyPool* pool, const ColonizeColony* c) {
  if (!pool || !c) {
    return COLONY_MAP_ICON_NONE;
  }
  const int fortress = colonies_find_building(pool, "Fortress");
  if (fortress >= 0 && c->has_building[fortress]) {
    return COLONY_MAP_ICON_FORTRESS;
  }
  const int fort = colonies_find_building(pool, "Fort");
  if (fort >= 0 && c->has_building[fort]) {
    return COLONY_MAP_ICON_FORT;
  }
  const int stockade = colonies_find_building(pool, "Stockade");
  if (stockade >= 0 && c->has_building[stockade]) {
    return COLONY_MAP_ICON_STOCKADE;
  }
  return COLONY_MAP_ICON_NONE;
}

static void colony_blit_map_icon(
  const ColonizeSpriteSheet* icons,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int tile_px,
  int tile_py,
  int tile_w,
  int tile_h
) {
  if (!icons || sprite < 0 || sprite >= icons->sprite_count) {
    return;
  }
  const ColonizeSprite* sp = &icons->sprites[sprite];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  /* 21×16 markers: center on the 16×16 tile. */
  const int px = tile_px + (tile_w - sp->width) / 2;
  const int py = tile_py + (tile_h - sp->height) / 2;
  ss_blit_sprite(icons, sprite, framebuffer, px, py);
}

static void colony_trim(char* s) {
  char* start = s;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
    s[--n] = '\0';
  }
}

void colonies_init(ColonizeColonyPool* pool) {
  if (!pool) {
    return;
  }
  memset(pool, 0, sizeof(*pool));
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      pool->colonies[i].tiles[t] = -1;
    }
  }
}

bool colonies_load_names(ColonizeColonyPool* pool, const char* colony_txt_path) {
  if (!pool || !colony_txt_path) {
    return false;
  }
  pool->name_count = 0;
  pool->name_next = 0;

  FILE* f = fopen(colony_txt_path, "r");
  if (!f) {
    diag_warn("Cannot open %s for colony names", colony_txt_path);
    return false;
  }

  char line[128];
  bool in_english = false;
  while (fgets(line, sizeof(line), f)) {
    colony_trim(line);
    if (line[0] == '@') {
      in_english = (strncmp(line + 1, "ENGLISH", 7) == 0);
      continue;
    }
    if (!in_english) {
      continue;
    }
    if (line[0] == '\0' || line[0] == ';') {
      continue;
    }
    /* Lines may have a year suffix: "Jamestown,1607" — strip it. */
    char* comma = strchr(line, ',');
    if (comma) {
      *comma = '\0';
    }
    colony_trim(line);
    if (line[0] == '\0') {
      continue;
    }
    if (pool->name_count >= COLONIZE_COLONY_NAMES_MAX) {
      break;
    }
    snprintf(
      pool->names[pool->name_count],
      COLONIZE_COLONY_NAME_MAX,
      "%s",
      line
    );
    pool->name_count++;
  }
  fclose(f);
  diag_info("Loaded %d colony names from %s", pool->name_count, colony_txt_path);
  return pool->name_count > 0;
}

bool colonies_load_buildings(ColonizeColonyPool* pool, const ColonizeMsgCatalog* names) {
  if (!pool || !names) {
    return false;
  }
  pool->building_type_count = 0;

  const ColonizeMsgSection* section = assets_msg_find(names, "BUILDING");
  if (!section) {
    diag_warn("NAMES.TXT missing @BUILDING section.");
    return false;
  }

  for (int i = 0; i < section->line_count && pool->building_type_count < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    char line[COLONIZE_MSG_LINE_LEN];
    snprintf(line, sizeof(line), "%s", section->lines[i]);
    if (line[0] == ';' || line[0] == '\0') {
      continue;
    }
    char* semi = strchr(line, ';');
    if (semi) {
      *semi = '\0';
    }
    char* comma = strchr(line, ',');
    if (!comma) {
      continue;
    }
    *comma = '\0';
    colony_trim(line);
    if (line[0] == '\0') {
      continue;
    }

    const char* p = comma + 1;
    int hammers = 0;
    int tools_cost = 0;
    int a = 0;
    int b = 0;
    int min_pop = 0;
    /* name, hammers, tools, ?, ?, min_population — trailing fields optional. */
    sscanf(p, " %d , %d , %d , %d , %d", &hammers, &tools_cost, &a, &b, &min_pop);
    (void)a;
    (void)b;

    ColonizeBuildingType* t = &pool->building_types[pool->building_type_count++];
    snprintf(t->name, sizeof(t->name), "%s", line);
    t->hammers = hammers;
    t->tools_cost = tools_cost;
    t->min_population = min_pop;
  }

  diag_info("Loaded %d building types from NAMES.TXT @BUILDING", pool->building_type_count);
  return pool->building_type_count > 0;
}

int colonies_find_building(const ColonizeColonyPool* pool, const char* name) {
  if (!pool || !name) {
    return -1;
  }
  for (int i = 0; i < pool->building_type_count; ++i) {
    if (strcmp(pool->building_types[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

const ColonizeBuildingType* colonies_building_type(const ColonizeColonyPool* pool, int type_index) {
  if (!pool || type_index < 0 || type_index >= pool->building_type_count) {
    return NULL;
  }
  return &pool->building_types[type_index];
}

bool colonies_can_found(
  const ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y
) {
  if (!pool || !map) {
    return false;
  }
  if (!map_tile_is_land(map, x, y)) {
    return false;
  }
  if (colonies_id_at(pool, x, y) >= 0) {
    return false;
  }
  return true;
}

static const char* colonies_next_name(ColonizeColonyPool* pool) {
  if (pool->name_count == 0) {
    return "New Colony";
  }
  const char* n = pool->names[pool->name_next % pool->name_count];
  pool->name_next++;
  return n;
}

static void colonies_grant_building(ColonizeColonyPool* pool, ColonizeColony* slot, const char* name) {
  const int idx = colonies_find_building(pool, name);
  if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX) {
    slot->has_building[idx] = true;
  }
}

/*
 * Classic free starters: craft houses + carpenter + town hall.
 * Warehouse, Stockade, and Docks are buildable (not free).
 * Coastal colonies without Docks show BUILDING.SS #45 coast placeholder;
 * without Stockade the screen draws fence art (BUILDING.SS #16).
 */
static void colonies_grant_starters(ColonizeColonyPool* pool, ColonizeColony* slot) {
  static const char* k_starters[] = {
    "Town Hall",
    "Carpenter's Shop",
    "Blacksmith's House",
    "Weaver's House",
    "Tobacconist's House",
    "Rum Distiller's House",
    "Fur Trader's House",
  };
  for (size_t i = 0; i < sizeof(k_starters) / sizeof(k_starters[0]); ++i) {
    colonies_grant_building(pool, slot, k_starters[i]);
  }
}

int colonies_found(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int founder_type_index,
  int tools,
  int muskets,
  int horses
) {
  if (!colonies_can_found(pool, map, x, y)) {
    return -1;
  }
  if (pool->colony_count >= COLONIZE_COLONIES_MAX) {
    diag_warn("Colony pool full");
    return -1;
  }

  ColonizeColony* slot = NULL;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (!pool->colonies[i].active) {
      slot = &pool->colonies[i];
      break;
    }
  }
  if (!slot) {
    return -1;
  }

  memset(slot, 0, sizeof(*slot));
  slot->id = pool->next_id++;
  slot->x = x;
  slot->y = y;
  slot->nation_id = 0;
  slot->building_in_production = -1;
  slot->active = true;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
    slot->tiles[t] = -1;
  }
  snprintf(slot->name, sizeof(slot->name), "%s", colonies_next_name(pool));
  colonies_grant_starters(pool, slot);

  if (tools > 0) {
    slot->stock[COLONIZE_CARGO_TOOLS] += tools;
  }
  if (muskets > 0) {
    slot->stock[COLONIZE_CARGO_MUSKETS] += muskets;
  }
  if (horses > 0) {
    slot->stock[COLONIZE_CARGO_HORSES] += horses;
  }
  /* New colonies start with a little food in the warehouse stub. */
  slot->stock[COLONIZE_CARGO_FOOD] = 200;

  if (founder_type_index >= 0 && slot->colonist_count < COLONIZE_COLONY_POP_MAX) {
    ColonizeColonist* c = &slot->colonists[slot->colonist_count++];
    c->active = true;
    c->unit_type_index = founder_type_index;
    c->building_type = colonies_find_building(pool, "Town Hall");
    c->field_job = -1;
    slot->population = slot->colonist_count;
  } else {
    slot->population = 0;
  }

  /* Default first project so carpenter hammers have a target. */
  {
    const int stockade = colonies_find_building(pool, "Stockade");
    if (stockade >= 0 && !slot->has_building[stockade]) {
      slot->building_in_production = stockade;
    }
  }

  pool->colony_count++;
  diag_info(
    "Founded colony '%s' at (%d,%d) pop=%d tools=%d muskets=%d horses=%d",
    slot->name,
    x,
    y,
    slot->population,
    slot->stock[COLONIZE_CARGO_TOOLS],
    slot->stock[COLONIZE_CARGO_MUSKETS],
    slot->stock[COLONIZE_CARGO_HORSES]
  );
  return slot->id;
}

const ColonizeColony* colonies_get(const ColonizeColonyPool* pool, int colony_id) {
  if (!pool || colony_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].id == colony_id) {
      return &pool->colonies[i];
    }
  }
  return NULL;
}

ColonizeColony* colonies_get_mut(ColonizeColonyPool* pool, int colony_id) {
  if (!pool || colony_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].id == colony_id) {
      return &pool->colonies[i];
    }
  }
  return NULL;
}

int colonies_id_at(const ColonizeColonyPool* pool, int x, int y) {
  if (!pool) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].x == x && pool->colonies[i].y == y) {
      return pool->colonies[i].id;
    }
  }
  return -1;
}

/* Surround order: N, NE, E, SE, S, SW, W, NW. */
static const int k_field_dx[COLONIZE_COLONY_FIELD_TILES] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int k_field_dy[COLONIZE_COLONY_FIELD_TILES] = {-1, -1, 0, 1, 1, 1, 0, -1};

bool colonies_field_tile_delta(int tile_index, int* out_dx, int* out_dy) {
  if (tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES) {
    return false;
  }
  if (out_dx) {
    *out_dx = k_field_dx[tile_index];
  }
  if (out_dy) {
    *out_dy = k_field_dy[tile_index];
  }
  return true;
}

int colonies_field_tile_index(int dx, int dy) {
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES; ++i) {
    if (k_field_dx[i] == dx && k_field_dy[i] == dy) {
      return i;
    }
  }
  return -1;
}

int colonies_colonist_tile(const ColonizeColony* colony, int colonist_index) {
  if (!colony || colonist_index < 0) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES; ++i) {
    if ((int)colony->tiles[i] == colonist_index) {
      return i;
    }
  }
  return -1;
}

static void colonies_clear_colonist_tile(ColonizeColony* col, int colonist_index) {
  if (!col) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES; ++i) {
    if ((int)col->tiles[i] == colonist_index) {
      col->tiles[i] = -1;
    }
  }
}

bool colonies_assign_workplace(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int building_type
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count) {
    return false;
  }
  ColonizeColonist* c = &col->colonists[colonist_index];
  if (!c->active) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (!col->has_building[building_type]) {
    return false;
  }
  colonies_clear_colonist_tile(col, colonist_index);
  c->field_job = -1;
  c->building_type = building_type;
  return true;
}

bool colonies_assign_field(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int tile_index,
  int field_job
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count) {
    return false;
  }
  if (tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES) {
    return false;
  }
  if (field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return false;
  }
  ColonizeColonist* c = &col->colonists[colonist_index];
  if (!c->active) {
    return false;
  }
  /* Evict prior worker on this tile. */
  const int prev = (int)col->tiles[tile_index];
  if (prev >= 0 && prev < col->colonist_count && prev != colonist_index) {
    col->colonists[prev].field_job = -1;
  }
  colonies_clear_colonist_tile(col, colonist_index);
  col->tiles[tile_index] = (int8_t)colonist_index;
  c->building_type = -1;
  c->field_job = field_job;
  return true;
}

bool colonies_clear_field(ColonizeColonyPool* pool, int colony_id, int tile_index) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col) {
    return false;
  }
  if (tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES) {
    return false;
  }
  const int who = (int)col->tiles[tile_index];
  col->tiles[tile_index] = -1;
  if (who >= 0 && who < col->colonist_count) {
    col->colonists[who].field_job = -1;
  }
  return true;
}

bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (col->has_building[building_type]) {
    return false;
  }
  const ColonizeBuildingType* bt = &pool->building_types[building_type];
  if (bt->min_population > 0 && col->population < bt->min_population) {
    return false;
  }
  col->building_in_production = building_type;
  return true;
}

bool colonies_clear_construction(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col) {
    return false;
  }
  col->building_in_production = -1;
  return true;
}

int colonies_construction_gold_cost(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
) {
  if (!pool || !colony || colony->building_in_production < 0) {
    return 0;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(pool, colony->building_in_production);
  if (!bt || bt->hammers <= 0) {
    return 0;
  }
  const int rem = bt->hammers - colony->hammers;
  return rem > 0 ? rem : 0;
}

int colonies_construction_tools_needed(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
) {
  if (!pool || !colony || colony->building_in_production < 0) {
    return 0;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(pool, colony->building_in_production);
  if (!bt || bt->tools_cost <= 0) {
    return 0;
  }
  const int have = colony->stock[COLONIZE_CARGO_TOOLS];
  const int need = bt->tools_cost - have;
  return need > 0 ? need : 0;
}

bool colonies_try_complete_building(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool || col->building_in_production < 0) {
    return false;
  }
  const int bid = col->building_in_production;
  const ColonizeBuildingType* bt = colonies_building_type(pool, bid);
  if (!bt || bt->hammers <= 0 || col->hammers < bt->hammers) {
    return false;
  }
  if (bt->tools_cost > 0 && col->stock[COLONIZE_CARGO_TOOLS] < bt->tools_cost) {
    return false;
  }
  if (bt->tools_cost > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] -= bt->tools_cost;
  }
  if (bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX) {
    col->has_building[bid] = true;
  }
  col->hammers = 0;
  col->building_in_production = -1;
  return true;
}

bool colonies_buy_construction(ColonizeColonyPool* pool, int colony_id, int* gold) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool || !gold || col->building_in_production < 0) {
    return false;
  }
  const ColonizeBuildingType* bt =
    colonies_building_type(pool, col->building_in_production);
  if (!bt || bt->hammers <= 0) {
    return false;
  }
  if (bt->tools_cost > 0 && col->stock[COLONIZE_CARGO_TOOLS] < bt->tools_cost) {
    return false;
  }
  const int gold_cost = colonies_construction_gold_cost(pool, col);
  if (*gold < gold_cost) {
    return false;
  }
  const int prev_hammers = col->hammers;
  *gold -= gold_cost;
  col->hammers = bt->hammers;
  if (!colonies_try_complete_building(pool, colony_id)) {
    *gold += gold_cost;
    col->hammers = prev_hammers;
    return false;
  }
  return true;
}

int colonies_list_buildable(
  const ColonizeColonyPool* pool,
  int colony_id,
  int* out_ids,
  int out_max
) {
  if (!pool || !out_ids || out_max <= 0) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(pool, colony_id);
  if (!col) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < pool->building_type_count && n < out_max; ++i) {
    if (col->has_building[i]) {
      continue;
    }
    const ColonizeBuildingType* bt = &pool->building_types[i];
    if (bt->name[0] == '\0') {
      continue;
    }
    if (bt->min_population > 0 && col->population < bt->min_population) {
      continue;
    }
    /* Skip zero-hammer fluff / duplicates that aren't real projects. */
    if (bt->hammers <= 0) {
      continue;
    }
    out_ids[n++] = i;
  }
  return n;
}

int colonies_warehouse_capacity(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type
) {
  if (!colony) {
    return 0;
  }
  if (cargo_type == COLONIZE_CARGO_FOOD) {
    return 199;
  }
  int cap = 100;
  if (pool) {
    const int wh = colonies_find_building(pool, "Warehouse");
    const int whe = colonies_find_building(pool, "Warehouse Expansion");
    if (wh >= 0 && colony->has_building[wh]) {
      cap += 100;
    }
    if (whe >= 0 && colony->has_building[whe]) {
      cap += 100;
    }
  }
  return cap;
}

int colonies_transfer_to_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int cargo_type,
  int amount
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT || amount <= 0) {
    return 0;
  }
  if (col->stock[cargo_type] < amount) {
    amount = col->stock[cargo_type];
  }
  if (amount <= 0) {
    return 0;
  }
  const int loaded = units_load_goods(units, unit_id, cargo_type, amount);
  if (loaded > 0) {
    col->stock[cargo_type] -= loaded;
  }
  return loaded;
}

int colonies_transfer_from_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  bool* out_warehouse_full
) {
  if (out_warehouse_full) {
    *out_warehouse_full = false;
  }
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units) {
    return 0;
  }
  int ctype = -1;
  int amt = 0;
  /* Peek without clearing — unload helper clears; reload remainder if capped. */
  const ColonizeUnit* u = units_get_const(units, unit_id);
  if (!u) {
    return 0;
  }
  const int n = units_goods_hold_count(units, unit_id);
  if (hold_index < 0 || hold_index >= n) {
    return 0;
  }
  amt = u->hold_goods_amount[hold_index];
  ctype = u->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255 || ctype < 0 || ctype >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  const int cap = colonies_warehouse_capacity(pool, col, ctype);
  const int room = cap - col->stock[ctype];
  if (room <= 0) {
    if (out_warehouse_full) {
      *out_warehouse_full = true;
    }
    return 0;
  }
  const int move = amt < room ? amt : room;
  int got_type = 0;
  int got_amt = 0;
  if (units_unload_goods_hold(units, unit_id, hold_index, &got_type, &got_amt) <= 0) {
    return 0;
  }
  col->stock[ctype] += move;
  if (move < got_amt) {
    /* Put remainder back into the same hold. */
    units_load_goods(units, unit_id, ctype, got_amt - move);
    if (out_warehouse_full) {
      *out_warehouse_full = true;
    }
  }
  return move;
}

int colonies_best_load_cargo(const ColonizeColony* colony) {
  if (!colony) {
    return -1;
  }
  /* Rough Europe bid ranking; exclude horses/tools/muskets (manual L-key). */
  static const int k_value[COLONIZE_CARGO_COUNT] = {
    1,  /* food */
    5,  /* sugar */
    4,  /* tobacco */
    3,  /* cotton */
    5,  /* furs */
    0,  /* lumber — rarely sold */
    4,  /* ore */
    20, /* silver */
    0,  /* horses — excluded */
    8,  /* rum */
    8,  /* cigars */
    7,  /* cloth */
    7,  /* coats */
    2,  /* trade goods */
    0,  /* tools — excluded */
    0   /* muskets — excluded */
  };
  int best = -1;
  int best_v = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if (k_value[c] <= 0 || colony->stock[c] <= 0) {
      continue;
    }
    if (k_value[c] > best_v || (k_value[c] == best_v && colony->stock[c] > colony->stock[best])) {
      best_v = k_value[c];
      best = c;
    }
  }
  return best;
}

/* Draw ICONS.SS colony settlement (#0–3 by fortification) with name below the tile. */
void colonies_render_on_map(
  const ColonizeColonyPool* pool,
  const ColonizeSpriteSheet* icons,
  ColonizeFramebuffer8* framebuffer,
  const ColonizeFont* font,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y
) {
  if (!pool || !framebuffer) {
    return;
  }

  const bool have_icon = icons && icons->sprite_count > COLONY_MAP_ICON_NONE;

  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active) {
      continue;
    }
    const int sx = c->x - view_x;
    const int sy = c->y - view_y;
    if (sx < 0 || sy < 0 || sx >= view_cols || sy >= view_rows) {
      continue;
    }

    const int px = origin_x + sx * tile_w;
    const int py = origin_y + sy * tile_h;

    if (have_icon) {
      colony_blit_map_icon(icons, colony_map_icon(pool, c), framebuffer, px, py, tile_w, tile_h);
    } else {
      /* Icons missing: tiny cyan marker so colonies stay findable. */
      for (int row = py; row < py + 2 && row < framebuffer->height; ++row) {
        if (row < 0) {
          continue;
        }
        for (int col = px; col < px + 2 && col < framebuffer->width; ++col) {
          if (col < 0) {
            continue;
          }
          framebuffer->pixels[row * framebuffer->width + col] = 11;
        }
      }
    }

    /* Colony name label below tile. */
    if (font) {
      font_draw_text(font, framebuffer, px, py + tile_h + 1, c->name, 15);
    }
  }
}
