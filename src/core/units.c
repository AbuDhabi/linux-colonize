#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

static void units_trim(char* s) {
  char* start = s;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  if (start != s) {
    memmove(s, start, strlen(start) + 1);
  }
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
    s[--n] = '\0';
  }
}

static bool units_parse_int_field(const char** cursor, int* out) {
  while (**cursor == ' ' || **cursor == '\t' || **cursor == ',') {
    ++(*cursor);
  }
  if (**cursor == '\0') {
    return false;
  }
  char* end = NULL;
  long v = strtol(*cursor, &end, 10);
  if (end == *cursor) {
    return false;
  }
  *out = (int)v;
  *cursor = end;
  return true;
}

static ColonizeUnit* units_slot(ColonizeUnitPool* pool) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (!pool->units[i].active) {
      return &pool->units[i];
    }
  }
  return NULL;
}

bool units_load_types(ColonizeUnitPool* pool, const ColonizeMsgCatalog* names) {
  if (!pool || !names) {
    return false;
  }
  pool->type_count = 0;

  const ColonizeMsgSection* section = assets_msg_find(names, "UNIT");
  if (!section) {
    diag_warn("NAMES.TXT missing @UNIT section.");
    return false;
  }

  for (int i = 0; i < section->line_count && pool->type_count < COLONIZE_UNIT_TYPES_MAX; ++i) {
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
    units_trim(line);

    const char* p = comma + 1;
    int icon = 0;
    int movement = 0;
    int attack = 0;
    int defense = 0;
    int cargo = 0;
    int size = 0;
    int cost = 0;
    int tools = 0;
    int guns = 0;
    int hull = 0;
    if (!units_parse_int_field(&p, &icon) || !units_parse_int_field(&p, &movement) ||
        !units_parse_int_field(&p, &attack) || !units_parse_int_field(&p, &defense) ||
        !units_parse_int_field(&p, &cargo) || !units_parse_int_field(&p, &size) ||
        !units_parse_int_field(&p, &cost) || !units_parse_int_field(&p, &tools) ||
        !units_parse_int_field(&p, &guns) || !units_parse_int_field(&p, &hull)) {
      continue;
    }
    (void)size;
    (void)cost;
    (void)tools;
    (void)guns;

    ColonizeUnitType* t = &pool->types[pool->type_count++];
    snprintf(t->name, sizeof(t->name), "%s", line);
    /* NAMES.TXT @UNIT icon is 1-based (DOS / MAPEDIT style); ICONS.SS blit is 0-based. */
    t->icon_sprite = icon > 0 ? icon - 1 : -1;
    t->movement = movement > 0 ? movement : 1;
    t->attack = attack;
    t->defense = defense;
    t->cargo = cargo;
    t->domain = hull > 0 ? COLONIZE_UNIT_DOMAIN_SEA : COLONIZE_UNIT_DOMAIN_LAND;
  }

  diag_info("Loaded %d unit types from NAMES.TXT @UNIT", pool->type_count);
  return pool->type_count > 0;
}

void units_reset(ColonizeUnitPool* pool) {
  if (!pool) {
    return;
  }
  memset(pool->units, 0, sizeof(pool->units));
  pool->unit_count = 0;
  pool->selected_id = -1;
  pool->next_id = 1;
}

int units_find_type(const ColonizeUnitPool* pool, const char* name) {
  if (!pool || !name) {
    return -1;
  }
  for (int i = 0; i < pool->type_count; ++i) {
    if (strcmp(pool->types[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

int units_spawn(ColonizeUnitPool* pool, int type_index, int x, int y) {
  if (!pool || type_index < 0 || type_index >= pool->type_count) {
    return -1;
  }
  if (units_id_at(pool, x, y) >= 0) {
    return -1;
  }
  return units_spawn_allow_stack(pool, type_index, x, y);
}

int units_spawn_allow_stack(ColonizeUnitPool* pool, int type_index, int x, int y) {
  if (!pool || type_index < 0 || type_index >= pool->type_count) {
    return -1;
  }
  ColonizeUnit* slot = units_slot(pool);
  if (!slot) {
    return -1;
  }
  const ColonizeUnitType* type = &pool->types[type_index];
  slot->id = pool->next_id++;
  slot->type_index = type_index;
  slot->x = x;
  slot->y = y;
  slot->moves_left = type->movement;
  slot->active = true;
  slot->nation_id = 0;
  slot->aboard_ship_id = -1;
  slot->cargo_count = 0;
  memset(slot->cargo_ids, 0, sizeof(slot->cargo_ids));
  memset(slot->hold_goods_type, 0, sizeof(slot->hold_goods_type));
  memset(slot->hold_goods_amount, 0, sizeof(slot->hold_goods_amount));
  slot->orders = 0;
  slot->goto_x = 0xFF;
  slot->goto_y = 0xFF;
  slot->profession = UNITS_JOB_NONE;
  slot->tools = (strstr(type->name, "Pioneer") != NULL) ? 100 : 0;
  pool->unit_count++;
  diag_info("Spawned unit id=%d type=%s at (%d,%d)", slot->id, type->name, x, y);
  return slot->id;
}

bool units_is_on_map(const ColonizeUnit* unit) {
  return unit && unit->active && unit->aboard_ship_id < 0;
}

static void units_clear_slot(ColonizeUnit* unit) {
  unit->active = false;
  unit->id = -1;
  unit->type_index = -1;
  unit->x = 0;
  unit->y = 0;
  unit->moves_left = 0;
  unit->nation_id = 0;
  unit->aboard_ship_id = -1;
  unit->cargo_count = 0;
  memset(unit->cargo_ids, 0, sizeof(unit->cargo_ids));
  memset(unit->hold_goods_type, 0, sizeof(unit->hold_goods_type));
  memset(unit->hold_goods_amount, 0, sizeof(unit->hold_goods_amount));
  unit->orders = 0;
  unit->goto_x = 0xFF;
  unit->goto_y = 0xFF;
  unit->profession = UNITS_JOB_NONE;
  unit->tools = 0;
}

bool units_despawn(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* unit = units_get(pool, unit_id);
  if (!unit) {
    return false;
  }
  /* Passengers ride with the ship — despawn them if this is a carrier. */
  if (unit->cargo_count > 0) {
    for (int i = 0; i < unit->cargo_count; ++i) {
      ColonizeUnit* pax = units_get(pool, unit->cargo_ids[i]);
      if (pax) {
        units_clear_slot(pax);
        if (pool->unit_count > 0) {
          pool->unit_count--;
        }
        if (pool->selected_id == unit->cargo_ids[i]) {
          pool->selected_id = -1;
        }
      }
    }
  }
  /* If this unit is aboard a ship, remove it from that ship's hold. */
  if (unit->aboard_ship_id >= 0) {
    ColonizeUnit* ship = units_get(pool, unit->aboard_ship_id);
    if (ship) {
      for (int i = 0; i < ship->cargo_count; ++i) {
        if (ship->cargo_ids[i] == unit_id) {
          for (int j = i + 1; j < ship->cargo_count; ++j) {
            ship->cargo_ids[j - 1] = ship->cargo_ids[j];
          }
          ship->cargo_count--;
          break;
        }
      }
    }
  }
  units_clear_slot(unit);
  if (pool->unit_count > 0) {
    pool->unit_count--;
  }
  if (pool->selected_id == unit_id) {
    pool->selected_id = -1;
  }
  return true;
}

bool units_is_sea(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (!unit) {
    return false;
  }
  const ColonizeUnitType* type = units_type(pool, unit->type_index);
  return type && type->domain == COLONIZE_UNIT_DOMAIN_SEA;
}

bool units_on_high_seas(const ColonizeWorldMap* map, int x, int y) {
  return map_tile_is_high_seas(map, x, y);
}

void units_founder_loot(
  const ColonizeUnitPool* pool,
  int unit_id,
  int* out_tools,
  int* out_muskets,
  int* out_horses
) {
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  const ColonizeUnitType* type = unit ? units_type(pool, unit->type_index) : NULL;
  if (type) {
    /* NAMES.TXT tools/guns fields are build costs, not carried gear.
       Match classic founding transfers by unit role. */
    if (strstr(type->name, "Pioneer") != NULL) {
      tools = 100;
    } else if (strstr(type->name, "Dragoon") != NULL || strstr(type->name, "Cavalry") != NULL) {
      muskets = 50;
      horses = 50;
    } else if (
      strstr(type->name, "Soldier") != NULL || strstr(type->name, "Regular") != NULL ||
      strstr(type->name, "Army") != NULL
    ) {
      muskets = 50;
    } else if (strstr(type->name, "Scout") != NULL) {
      horses = 50;
    }
  }
  if (out_tools) {
    *out_tools = tools;
  }
  if (out_muskets) {
    *out_muskets = muskets;
  }
  if (out_horses) {
    *out_horses = horses;
  }
}

int units_id_at(const ColonizeUnitPool* pool, int x, int y) {
  if (!pool) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (units_is_on_map(u) && u->x == x && u->y == y) {
      return u->id;
    }
  }
  return -1;
}

ColonizeUnit* units_get(ColonizeUnitPool* pool, int unit_id) {
  if (!pool || unit_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (u->active && u->id == unit_id) {
      return u;
    }
  }
  return NULL;
}

const ColonizeUnit* units_get_const(const ColonizeUnitPool* pool, int unit_id) {
  if (!pool || unit_id < 0) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (u->active && u->id == unit_id) {
      return u;
    }
  }
  return NULL;
}

const ColonizeUnitType* units_type(const ColonizeUnitPool* pool, int type_index) {
  if (!pool || type_index < 0 || type_index >= pool->type_count) {
    return NULL;
  }
  return &pool->types[type_index];
}

bool units_can_enter(
  const ColonizeUnitPool* pool,
  int type_index,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int mover_id,
  const ColonizeColonyPool* colonies
) {
  if (!pool || type_index < 0 || type_index >= pool->type_count || !map) {
    return false;
  }
  if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return false;
  }

  int mover_nation = -1;
  const ColonizeUnit* mover = (mover_id >= 0) ? units_get_const(pool, mover_id) : NULL;
  if (mover) {
    mover_nation = mover->nation_id;
  }

  /* Friendly stacks OK; foreign on-map unit blocks. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->id == mover_id) {
      continue;
    }
    if (mover_nation >= 0 && u->nation_id == mover_nation) {
      continue;
    }
    return false;
  }

  const ColonizeUnitType* type = &pool->types[type_index];
  const bool water = map_tile_is_water(map, x, y);
  if (type->domain == COLONIZE_UNIT_DOMAIN_SEA) {
    if (water) {
      return true;
    }
    /* Own-nation colony dock: ships may enter coastal settlement tiles. */
    if (colonies && map_tile_is_land(map, x, y) && mover_nation >= 0) {
      const int cid = colonies_id_at(colonies, x, y);
      const ColonizeColony* col = colonies_get(colonies, cid);
      if (col && col->active && col->nation_id == mover_nation) {
        return true;
      }
    }
    return false;
  }
  return map_tile_is_land(map, x, y);
}

int units_move_cost(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y
) {
  if (!map) {
    return 1;
  }
  if (units_is_sea(pool, unit_id)) {
    return 1;
  }
  return map_move_cost_at(map, dest_x, dest_y);
}

bool units_try_move(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
) {
  ColonizeUnit* unit = units_get(pool, unit_id);
  if (!unit || !map) {
    return false;
  }
  if (unit->aboard_ship_id >= 0) {
    return false;
  }
  if (unit->moves_left <= 0) {
    return false;
  }
  if (unit->x == dest_x && unit->y == dest_y) {
    return false;
  }
  const int dx = dest_x - unit->x;
  const int dy = dest_y - unit->y;
  if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || (dx == 0 && dy == 0)) {
    return false;
  }
  if (!units_can_enter(pool, unit->type_index, map, dest_x, dest_y, unit_id, colonies)) {
    return false;
  }
  const int cost = units_move_cost(pool, unit_id, map, dest_x, dest_y);
  if (unit->moves_left < cost) {
    return false;
  }
  unit->x = dest_x;
  unit->y = dest_y;
  unit->moves_left -= cost;
  /* Keep passengers' coordinates mirrored to the ship for debugging / unload. */
  for (int i = 0; i < unit->cargo_count; ++i) {
    ColonizeUnit* pax = units_get(pool, unit->cargo_ids[i]);
    if (pax) {
      pax->x = dest_x;
      pax->y = dest_y;
    }
  }
  return true;
}

bool units_is_pioneer(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || units_is_sea(pool, unit_id)) {
    return false;
  }
  if (u->profession == UNITS_JOB_PIONEER) {
    return true;
  }
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  return type && strstr(type->name, "Pioneer") != NULL;
}

#define UNITS_PIONEER_TOOL_COST 20

bool units_pioneer_plow(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !map || !units_is_pioneer(pool, unit_id)) {
    if (err && err_size) {
      snprintf(err, err_size, "Select a Pioneer");
    }
    return false;
  }
  if (u->moves_left <= 0) {
    if (err && err_size) {
      snprintf(err, err_size, "No moves left");
    }
    return false;
  }
  if (u->tools < UNITS_PIONEER_TOOL_COST) {
    if (err && err_size) {
      snprintf(err, err_size, "Need tools");
    }
    return false;
  }
  if (!map_tile_is_land(map, u->x, u->y) || map_tile_is_high_seas(map, u->x, u->y)) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot plow here");
    }
    return false;
  }
  const int pedia = map_pedia_terrain_index_at(map, u->x, u->y);
  /* Arctic / mountains (hills are plowable). */
  if (pedia == 24 || pedia == 27) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot plow here");
    }
    return false;
  }
  if (map_tile_is_plowed(map, u->x, u->y)) {
    if (err && err_size) {
      snprintf(err, err_size, "Already plowed");
    }
    return false;
  }
  if (pedia >= 8 && pedia <= 23) {
    map_tile_clear_forest(map, u->x, u->y);
  }
  map_tile_set_plowed(map, u->x, u->y, true);
  u->tools -= UNITS_PIONEER_TOOL_COST;
  u->moves_left = 0;
  if (err && err_size) {
    snprintf(err, err_size, "Plowed (-%d tools)", UNITS_PIONEER_TOOL_COST);
  }
  return true;
}

bool units_pioneer_road(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !map || !units_is_pioneer(pool, unit_id)) {
    if (err && err_size) {
      snprintf(err, err_size, "Select a Pioneer");
    }
    return false;
  }
  if (u->moves_left <= 0) {
    if (err && err_size) {
      snprintf(err, err_size, "No moves left");
    }
    return false;
  }
  if (u->tools < UNITS_PIONEER_TOOL_COST) {
    if (err && err_size) {
      snprintf(err, err_size, "Need tools");
    }
    return false;
  }
  if (!map_tile_is_land(map, u->x, u->y) || map_tile_is_high_seas(map, u->x, u->y)) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot build road here");
    }
    return false;
  }
  if (map_tile_has_road(map, u->x, u->y)) {
    if (err && err_size) {
      snprintf(err, err_size, "Already a road");
    }
    return false;
  }
  map_tile_set_road(map, u->x, u->y, true);
  u->tools -= UNITS_PIONEER_TOOL_COST;
  u->moves_left = 0;
  if (err && err_size) {
    snprintf(err, err_size, "Road built (-%d tools)", UNITS_PIONEER_TOOL_COST);
  }
  return true;
}

static bool units_adjacent(int ax, int ay, int bx, int by) {
  const int dx = ax - bx;
  const int dy = ay - by;
  if (dx == 0 && dy == 0) {
    return false;
  }
  return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}

int units_ship_capacity(const ColonizeUnitPool* pool, int ship_id) {
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!ship || !units_is_sea(pool, ship_id)) {
    return 0;
  }
  const ColonizeUnitType* type = units_type(pool, ship->type_index);
  if (!type || type->cargo <= 0) {
    return 0;
  }
  return type->cargo > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX : type->cargo;
}

bool units_is_transport(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u)) {
    return false;
  }
  if (units_is_sea(pool, unit_id)) {
    return units_goods_hold_count(pool, unit_id) > 0;
  }
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  if (!type) {
    return false;
  }
  return strstr(type->name, "Wagon") != NULL && type->cargo > 0;
}

int units_goods_hold_count(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    return 0;
  }
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  if (!type || type->cargo <= 0) {
    return 0;
  }
  /* Commodity holds share the @UNIT cargo count with passenger slots conceptually;
   * goods use the same slot count (passengers occupy separate cargo_ids). */
  return type->cargo > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX : type->cargo;
}

int units_first_goods_hold(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    return -1;
  }
  const int n = units_goods_hold_count(pool, unit_id);
  for (int i = 0; i < n; ++i) {
    if (u->hold_goods_amount[i] > 0 && u->hold_goods_amount[i] < 255) {
      return i;
    }
  }
  return -1;
}

int units_load_goods(ColonizeUnitPool* pool, int unit_id, int cargo_type, int amount) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !units_is_transport(pool, unit_id)) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT || amount <= 0) {
    return 0;
  }
  const int n = units_goods_hold_count(pool, unit_id);
  int loaded = 0;
  /* Prefer stacking into a matching partial hold. */
  for (int i = 0; i < n && amount > 0; ++i) {
    if (u->hold_goods_amount[i] <= 0 || u->hold_goods_amount[i] >= 255) {
      continue;
    }
    if (u->hold_goods_type[i] != cargo_type) {
      continue;
    }
    const int room = 100 - u->hold_goods_amount[i];
    if (room <= 0) {
      continue;
    }
    const int add = amount < room ? amount : room;
    u->hold_goods_amount[i] += add;
    amount -= add;
    loaded += add;
  }
  for (int i = 0; i < n && amount > 0; ++i) {
    if (u->hold_goods_amount[i] > 0 && u->hold_goods_amount[i] < 255) {
      continue;
    }
    const int add = amount < 100 ? amount : 100;
    u->hold_goods_type[i] = cargo_type;
    u->hold_goods_amount[i] = add;
    amount -= add;
    loaded += add;
  }
  return loaded;
}

int units_unload_goods_hold(
  ColonizeUnitPool* pool,
  int unit_id,
  int hold_index,
  int* out_cargo_type,
  int* out_amount
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !units_is_transport(pool, unit_id)) {
    return 0;
  }
  const int n = units_goods_hold_count(pool, unit_id);
  if (hold_index < 0 || hold_index >= n) {
    return 0;
  }
  const int amt = u->hold_goods_amount[hold_index];
  if (amt <= 0 || amt >= 255) {
    return 0;
  }
  const int ctype = u->hold_goods_type[hold_index];
  if (out_cargo_type) {
    *out_cargo_type = ctype;
  }
  if (out_amount) {
    *out_amount = amt;
  }
  u->hold_goods_amount[hold_index] = 0;
  u->hold_goods_type[hold_index] = 0;
  return amt;
}

bool units_board(ColonizeUnitPool* pool, int land_unit_id, int ship_id) {
  ColonizeUnit* land = units_get(pool, land_unit_id);
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!land || !ship) {
    return false;
  }
  if (units_is_sea(pool, land_unit_id) || !units_is_sea(pool, ship_id)) {
    return false;
  }
  if (land->aboard_ship_id >= 0 || ship->aboard_ship_id >= 0) {
    return false;
  }
  const int cap = units_ship_capacity(pool, ship_id);
  if (cap <= 0 || ship->cargo_count >= cap) {
    return false;
  }
  if (!units_adjacent(land->x, land->y, ship->x, ship->y)) {
    return false;
  }
  land->aboard_ship_id = ship_id;
  land->x = ship->x;
  land->y = ship->y;
  land->moves_left = 0;
  land->orders = 1; /* sentry aboard */
  ship->cargo_ids[ship->cargo_count++] = land_unit_id;
  if (pool->selected_id == land_unit_id) {
    pool->selected_id = ship_id;
  }
  diag_info("Unit %d boarded ship %d (cargo %d/%d)", land_unit_id, ship_id, ship->cargo_count, cap);
  return true;
}

bool units_board_stacked(ColonizeUnitPool* pool, int land_unit_id, int ship_id) {
  ColonizeUnit* land = units_get(pool, land_unit_id);
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!land || !ship) {
    return false;
  }
  if (units_is_sea(pool, land_unit_id) || !units_is_sea(pool, ship_id)) {
    return false;
  }
  if (land->aboard_ship_id >= 0 || ship->aboard_ship_id >= 0) {
    return false;
  }
  const int cap = units_ship_capacity(pool, ship_id);
  if (cap <= 0 || ship->cargo_count >= cap) {
    return false;
  }
  land->aboard_ship_id = ship_id;
  land->x = ship->x;
  land->y = ship->y;
  land->moves_left = 0;
  land->orders = 1; /* sentry aboard */
  ship->cargo_ids[ship->cargo_count++] = land_unit_id;
  return true;
}

static bool units_remove_from_cargo(ColonizeUnit* ship, int pax_id) {
  if (!ship) {
    return false;
  }
  for (int i = 0; i < ship->cargo_count; ++i) {
    if (ship->cargo_ids[i] != pax_id) {
      continue;
    }
    for (int j = i + 1; j < ship->cargo_count; ++j) {
      ship->cargo_ids[j - 1] = ship->cargo_ids[j];
    }
    ship->cargo_count--;
    return true;
  }
  return false;
}

bool units_unload_passenger(
  ColonizeUnitPool* pool,
  int ship_id,
  int pax_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  ColonizeUnit* pax = units_get(pool, pax_id);
  if (!ship || !pax || !map || !units_is_sea(pool, ship_id)) {
    return false;
  }
  if (pax->aboard_ship_id != ship_id) {
    return false;
  }
  /* Adjacent landfall, or same tile (colony dock). */
  if (!(ship->x == dest_x && ship->y == dest_y) &&
      !units_adjacent(ship->x, ship->y, dest_x, dest_y)) {
    return false;
  }
  if (!units_can_enter(pool, pax->type_index, map, dest_x, dest_y, pax_id, colonies)) {
    return false;
  }
  if (!units_remove_from_cargo(ship, pax_id)) {
    return false;
  }
  pax->aboard_ship_id = -1;
  pax->x = dest_x;
  pax->y = dest_y;
  pax->orders = 0;
  const ColonizeUnitType* type = units_type(pool, pax->type_index);
  if (pax->moves_left <= 0) {
    pax->moves_left = type ? type->movement : 1;
  }
  pool->selected_id = pax_id;
  diag_info("Unloaded unit %d from ship %d to (%d,%d)", pax_id, ship_id, dest_x, dest_y);
  return true;
}

bool units_unload(
  ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship || ship->cargo_count <= 0) {
    return false;
  }
  return units_unload_passenger(
    pool, ship_id, ship->cargo_ids[0], map, dest_x, dest_y, colonies
  );
}

int units_first_cargo_with_moves(const ColonizeUnitPool* pool, int ship_id) {
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!ship) {
    return -1;
  }
  for (int i = 0; i < ship->cargo_count; ++i) {
    const ColonizeUnit* pax = units_get_const(pool, ship->cargo_ids[i]);
    if (pax && pax->moves_left > 0) {
      return pax->id;
    }
  }
  return -1;
}

int units_disembark_all(ColonizeUnitPool* pool, int ship_id, int x, int y) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship || !units_is_sea(pool, ship_id)) {
    return 0;
  }
  int n = 0;
  while (ship->cargo_count > 0) {
    const int pax_id = ship->cargo_ids[0];
    ColonizeUnit* pax = units_get(pool, pax_id);
    if (!units_remove_from_cargo(ship, pax_id)) {
      break;
    }
    if (pax) {
      pax->aboard_ship_id = -1;
      pax->x = x;
      pax->y = y;
      pax->orders = 0;
      n++;
    }
  }
  diag_info("Disembarked %d units from ship %d at (%d,%d)", n, ship_id, x, y);
  return n;
}

int units_collect_tile_stack(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int* out_ids,
  int out_max
) {
  if (!pool || !out_ids || out_max <= 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX && n < out_max; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (units_is_on_map(u) && u->x == x && u->y == y) {
      out_ids[n++] = u->id;
    }
  }
  /* Passengers of ships on this tile (may already share x,y). */
  for (int i = 0; i < COLONIZE_UNITS_MAX && n < out_max; ++i) {
    const ColonizeUnit* ship = &pool->units[i];
    if (!ship->active || ship->nation_id != nation_id || ship->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(pool, ship->id) || ship->x != x || ship->y != y) {
      continue;
    }
    for (int c = 0; c < ship->cargo_count && n < out_max; ++c) {
      const int pid = ship->cargo_ids[c];
      bool listed = false;
      for (int k = 0; k < n; ++k) {
        if (out_ids[k] == pid) {
          listed = true;
          break;
        }
      }
      if (!listed && units_get_const(pool, pid)) {
        out_ids[n++] = pid;
      }
    }
  }
  return n;
}

int units_export_cargo_types(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_types,
  int out_max
) {
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!ship || !out_types || out_max <= 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < ship->cargo_count && n < out_max; ++i) {
    const ColonizeUnit* pax = units_get_const(pool, ship->cargo_ids[i]);
    if (pax) {
      out_types[n++] = pax->type_index;
    }
  }
  return n;
}

bool units_despawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_id,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max
) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship || !units_is_sea(pool, ship_id)) {
    return false;
  }
  if (out_type_index) {
    *out_type_index = ship->type_index;
  }
  if (out_name && out_name_size > 0) {
    const ColonizeUnitType* type = units_type(pool, ship->type_index);
    snprintf(out_name, out_name_size, "%s", type ? type->name : "Ship");
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    *out_cargo_count = units_export_cargo_types(pool, ship_id, out_cargo_types, cargo_max);
  } else if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = ship->hold_goods_type[i];
      out_hold_goods_amount[i] = ship->hold_goods_amount[i];
    }
    for (int i = n; i < hold_max; ++i) {
      out_hold_goods_type[i] = 0;
      out_hold_goods_amount[i] = 0;
    }
  }
  return units_despawn(pool, ship_id);
}

static int units_spawn_aboard(ColonizeUnitPool* pool, int type_index, ColonizeUnit* ship) {
  if (!pool || !ship || type_index < 0 || type_index >= pool->type_count) {
    return -1;
  }
  if (ship->cargo_count >= COLONIZE_UNIT_CARGO_MAX) {
    return -1;
  }
  ColonizeUnit* slot = units_slot(pool);
  if (!slot) {
    return -1;
  }
  const ColonizeUnitType* type = &pool->types[type_index];
  slot->id = pool->next_id++;
  slot->type_index = type_index;
  slot->x = ship->x;
  slot->y = ship->y;
  slot->moves_left = 0;
  slot->active = true;
  slot->nation_id = ship->nation_id;
  slot->aboard_ship_id = ship->id;
  slot->cargo_count = 0;
  memset(slot->cargo_ids, 0, sizeof(slot->cargo_ids));
  memset(slot->hold_goods_type, 0, sizeof(slot->hold_goods_type));
  memset(slot->hold_goods_amount, 0, sizeof(slot->hold_goods_amount));
  slot->orders = 1; /* sentry aboard */
  slot->goto_x = 0xFF;
  slot->goto_y = 0xFF;
  slot->profession = UNITS_JOB_NONE;
  ship->cargo_ids[ship->cargo_count++] = slot->id;
  pool->unit_count++;
  return slot->id;
}

int units_spawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_type_index,
  int x,
  int y,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
) {
  /* Allow stacking: harbor return / Europe berth may share a water tile. */
  const int ship_id = units_spawn_allow_stack(pool, ship_type_index, x, y);
  if (ship_id < 0) {
    return -1;
  }
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship) {
    return -1;
  }
  int cap = units_ship_capacity(pool, ship_id);
  /* Test / incomplete @UNIT rows may list cargo 0; still allow boarding passengers. */
  if (cap <= 0) {
    cap = COLONIZE_UNIT_CARGO_MAX;
  }
  const int n = cargo_count < 0 ? 0 : cargo_count;
  for (int i = 0; i < n && ship->cargo_count < cap; ++i) {
    if (!cargo_types) {
      break;
    }
    if (units_spawn_aboard(pool, cargo_types[i], ship) < 0) {
      break;
    }
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      ship->hold_goods_type[i] = hold_goods_type[i];
      ship->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  return ship_id;
}

/* Discoverer/Explorer: experts for all. Else French→Hardy Pioneer, Spanish→Veteran Soldier. */
static void units_starter_skills(int nation_id, int difficulty, int* pioneer_job, int* soldier_job) {
  const bool easy = difficulty <= 1;
  const bool hardy = easy || nation_id == 1;
  const bool veteran = easy || nation_id == 2;
  if (pioneer_job) {
    *pioneer_job = hardy ? UNITS_JOB_PIONEER : UNITS_JOB_NONE;
  }
  if (soldier_job) {
    *soldier_job = veteran ? UNITS_JOB_SOLDIER : UNITS_JOB_NONE;
  }
}

const char* units_display_name(const ColonizeUnitPool* pool, const ColonizeUnit* unit) {
  static char buf[48];
  if (!unit) {
    return "Unit";
  }
  const ColonizeUnitType* ut = pool ? units_type(pool, unit->type_index) : NULL;
  if (unit->profession == UNITS_JOB_PIONEER) {
    snprintf(buf, sizeof(buf), "Hardy Pioneer");
    return buf;
  }
  if (unit->profession == UNITS_JOB_SOLDIER) {
    snprintf(buf, sizeof(buf), "Veteran Soldier");
    return buf;
  }
  if (ut && strcmp(ut->name, "Pioneers") == 0) {
    return "Pioneer";
  }
  if (ut && strcmp(ut->name, "Soldiers") == 0) {
    return "Soldier";
  }
  return ut ? ut->name : "Unit";
}

int units_spawn_euro_starter_fleet(
  ColonizeUnitPool* pool,
  int nation_id,
  int difficulty,
  int x,
  int y,
  int goto_x,
  int goto_y
) {
  if (!pool || nation_id < 0 || nation_id > 3) {
    return -1;
  }
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 4) {
    difficulty = 4;
  }

  int pioneer_type = units_find_type(pool, "Pioneers");
  if (pioneer_type < 0) {
    pioneer_type = units_find_type(pool, "Colonists");
  }
  const int soldier_type = units_find_type(pool, "Soldiers");
  int ship_type = units_find_type(pool, "Caravel");
  if (nation_id == 3) {
    const int merchant = units_find_type(pool, "Merchantman");
    if (merchant >= 0) {
      ship_type = merchant;
    }
  }
  if (ship_type < 0 || pioneer_type < 0) {
    return -1;
  }

  const int ship_id = units_spawn_allow_stack(pool, ship_type, x, y);
  if (ship_id < 0) {
    return -1;
  }
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship) {
    return -1;
  }
  ship->nation_id = nation_id;
  ship->profession = UNITS_JOB_NONE;
  if (goto_x >= 0 && goto_x < 255 && goto_y >= 0 && goto_y < 255) {
    ship->orders = 12; /* goto landfall */
    ship->goto_x = goto_x;
    ship->goto_y = goto_y;
  }

  int pioneer_job = UNITS_JOB_NONE;
  int soldier_job = UNITS_JOB_NONE;
  units_starter_skills(nation_id, difficulty, &pioneer_job, &soldier_job);

  const int cargo_types[2] = {pioneer_type, soldier_type >= 0 ? soldier_type : pioneer_type};
  const int cargo_jobs[2] = {pioneer_job, soldier_type >= 0 ? soldier_job : pioneer_job};
  const int cargo_n = soldier_type >= 0 ? 2 : 1;
  for (int i = 0; i < cargo_n; ++i) {
    const int pid = units_spawn_aboard(pool, cargo_types[i], ship);
    if (pid < 0) {
      diag_warn("starter fleet: failed to board passenger %d for nation %d", i, nation_id);
      continue;
    }
    ColonizeUnit* pax = units_get(pool, pid);
    if (!pax) {
      continue;
    }
    pax->nation_id = nation_id;
    pax->profession = cargo_jobs[i];
    pax->orders = 1; /* sentry aboard */
    pax->goto_x = goto_x >= 0 ? goto_x : 0xFF;
    pax->goto_y = goto_y >= 0 ? goto_y : 0xFF;
  }

  diag_info(
    "Euro starter fleet nation=%d ship=%d cargo=%d at (%d,%d) skills p=%d s=%d",
    nation_id,
    ship_id,
    ship->cargo_count,
    x,
    y,
    pioneer_job,
    soldier_job
  );
  return ship_id;
}

void units_end_turn(ColonizeUnitPool* pool) {
  if (!pool) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active) {
      continue;
    }
    const ColonizeUnitType* type = units_type(pool, u->type_index);
    if (type) {
      u->moves_left = type->movement;
    }
  }
}

static bool units_find_land_tile(const ColonizeWorldMap* map, int start_x, int start_y, int* out_x, int* out_y) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  if (map_tile_is_land(map, start_x, start_y)) {
    *out_x = start_x;
    *out_y = start_y;
    return true;
  }
  for (int radius = 1; radius < 32; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (abs(dx) != radius && abs(dy) != radius) {
          continue;
        }
        const int x = start_x + dx;
        const int y = start_y + dy;
        if (map_tile_is_land(map, x, y)) {
          *out_x = x;
          *out_y = y;
          return true;
        }
      }
    }
  }
  return false;
}

bool units_find_water_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int occupant_id,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  if (map_tile_is_water(map, start_x, start_y)) {
    const int other = pool ? units_id_at(pool, start_x, start_y) : -1;
    if (other < 0 || other == occupant_id) {
      *out_x = start_x;
      *out_y = start_y;
      return true;
    }
  }
  for (int radius = 1; radius < 48; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (abs(dx) != radius && abs(dy) != radius) {
          continue;
        }
        const int x = start_x + dx;
        const int y = start_y + dy;
        if (!map_tile_is_water(map, x, y)) {
          continue;
        }
        const int other = pool ? units_id_at(pool, x, y) : -1;
        if (other >= 0 && other != occupant_id) {
          continue;
        }
        *out_x = x;
        *out_y = y;
        return true;
      }
    }
  }
  return false;
}

bool units_find_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }

  int best_x = -1;
  int best_y = -1;
  int best_d = 1 << 30;
  for (int y = 0; y < (int)map->height; ++y) {
    for (int x = 0; x < (int)map->width; ++x) {
      if (!map_tile_is_high_seas(map, x, y)) {
        continue;
      }
      if (pool && units_id_at(pool, x, y) >= 0) {
        continue;
      }
      const int dx = x - start_x;
      const int dy = y - start_y;
      const int d = dx * dx + dy * dy;
      if (d < best_d) {
        best_d = d;
        best_x = x;
        best_y = y;
      }
    }
  }
  if (best_x < 0) {
    return false;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

bool units_find_eastern_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int prefer_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }

  /*
   * Western rim of the eastern high-seas band: high-seas tiles whose west
   * neighbour is not high seas (Atlantic approach), excluding the map's
   * western border strip. Prefer latitude near prefer_y.
   */
  int best_x = -1;
  int best_y = -1;
  int best_score = -1;
  const int east_min_x = map->width / 2;

  for (int y = 0; y < (int)map->height; ++y) {
    for (int x = east_min_x; x < (int)map->width; ++x) {
      if (!map_tile_is_high_seas(map, x, y)) {
        continue;
      }
      if (map_tile_is_high_seas(map, x - 1, y)) {
        continue; /* interior of eastern high seas — not the western edge */
      }
      if (pool && units_id_at(pool, x, y) >= 0) {
        continue;
      }
      /* Closer latitude wins; tie-break westward (smaller x). */
      const int score = 100000 - abs(y - prefer_y) * 1000 - x;
      if (score > best_score) {
        best_score = score;
        best_x = x;
        best_y = y;
      }
    }
  }

  if (best_x < 0) {
    /* Fallback: any eastern high seas near prefer_y, then any high seas / water. */
    best_score = -1;
    for (int y = 0; y < (int)map->height; ++y) {
      for (int x = east_min_x; x < (int)map->width; ++x) {
        if (!map_tile_is_high_seas(map, x, y)) {
          continue;
        }
        if (pool && units_id_at(pool, x, y) >= 0) {
          continue;
        }
        const int score = 100000 - abs(y - prefer_y) * 1000 - x;
        if (score > best_score) {
          best_score = score;
          best_x = x;
          best_y = y;
        }
      }
    }
  }

  if (best_x < 0) {
    if (units_find_high_seas_tile(pool, map, map->width - 1, prefer_y, out_x, out_y)) {
      return true;
    }
    return units_find_water_tile(pool, map, map->width - 1, prefer_y, -1, out_x, out_y);
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

void units_new_world_start(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
  int difficulty
) {
  if (!pool) {
    return;
  }
  units_reset(pool);
  if (!map) {
    return;
  }
  if (nation_id < 0 || nation_id > 3) {
    nation_id = 0;
  }

  int sx = start_x;
  int sy = start_y;
  if (!units_find_eastern_high_seas_tile(pool, map, start_y, &sx, &sy)) {
    return;
  }

  const int ship_id = units_spawn_euro_starter_fleet(
    pool, nation_id, difficulty, sx, sy, start_x, start_y
  );
  if (ship_id < 0) {
    return;
  }
  /* Human starts with ship selected; clear goto until player sets course (orders keep landfall dest). */
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (ship) {
    ship->orders = 0;
  }
  pool->selected_id = ship_id;
}

bool units_deploy_colonist(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  const char* immigrant_name
) {
  (void)immigrant_name;
  if (!pool || !map) {
    return false;
  }
  int colonist_type = units_find_type(pool, "Colonists");
  if (colonist_type < 0) {
    return false;
  }
  if (!units_can_enter(pool, colonist_type, map, x, y, -1, NULL)) {
    return false;
  }
  const int id = units_spawn(pool, colonist_type, x, y);
  if (id < 0) {
    return false;
  }
  pool->selected_id = id;
  return true;
}

int units_map_sprite(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* unit = NULL;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (pool->units[i].active && pool->units[i].id == unit_id) {
      unit = &pool->units[i];
      break;
    }
  }
  if (!unit) {
    return -1;
  }
  const ColonizeUnitType* type = units_type(pool, unit->type_index);
  if (!type) {
    return -1;
  }
  return type->icon_sprite;
}

void units_render_on_map(
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* nation_sheet,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  bool selected_visible
) {
  if (!pool || !nation_sheet || !framebuffer) {
    return;
  }

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* unit = &pool->units[i];
    if (!units_is_on_map(unit)) {
      continue;
    }
    /* Blink: omit selected unit on off frames (tile cursor stays hidden). */
    if (unit->id == pool->selected_id && !selected_visible) {
      continue;
    }
    const int sx = unit->x - view_x;
    const int sy = unit->y - view_y;
    if (sx < 0 || sy < 0 || sx >= view_cols || sy >= view_rows) {
      continue;
    }

    const int sprite = units_map_sprite(pool, unit->id);
    if (sprite < 0 || sprite >= nation_sheet->sprite_count) {
      continue;
    }

    const int px = origin_x + sx * tile_w;
    const int py = origin_y + sy * tile_h;
    ss_blit_sprite(nation_sheet, sprite, framebuffer, px, py);
  }
}
