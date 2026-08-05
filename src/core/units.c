#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/strutil.h"
#include "core/unit_chrome.h"
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
    (void)tools;
    (void)guns;

    ColonizeUnitType* t = &pool->types[pool->type_count++];
    str_copy_trunc(t->name, sizeof(t->name), line);
    /* NAMES.TXT @UNIT icon is 1-based (DOS / MAPEDIT style); ICONS.SS blit is 0-based. */
    t->icon_sprite = icon > 0 ? icon - 1 : -1;
    t->movement = movement > 0 ? movement : 1;
    t->attack = attack;
    t->defense = defense;
    t->cargo = cargo;
    t->cost = cost;
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
  slot->tools = 0;
  slot->muskets = 0;
  slot->horses = 0;
  if (strstr(type->name, "Pioneer") != NULL) {
    slot->tools = UNITS_EQUIP_TOOLS_MAX;
  } else if (strstr(type->name, "Dragoon") != NULL || strstr(type->name, "Cavalry") != NULL) {
    slot->muskets = UNITS_EQUIP_MUSKETS;
    slot->horses = UNITS_EQUIP_HORSES;
  } else if (
    strstr(type->name, "Soldier") != NULL || strstr(type->name, "Regular") != NULL ||
    strstr(type->name, "Army") != NULL
  ) {
    slot->muskets = UNITS_EQUIP_MUSKETS;
  } else if (strstr(type->name, "Scout") != NULL) {
    slot->horses = UNITS_EQUIP_HORSES;
  }
  slot->home_tribe_id = -1;
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
  unit->muskets = 0;
  unit->horses = 0;
  unit->home_tribe_id = -1;
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
  if (unit) {
    tools = unit->tools > 0 ? unit->tools : 0;
    muskets = unit->muskets > 0 ? unit->muskets : 0;
    horses = unit->horses > 0 ? unit->horses : 0;
  }
  if (tools == 0 && muskets == 0 && horses == 0 && type) {
    /* Fallback for units spawned before gear fields were set. */
    if (strstr(type->name, "Pioneer") != NULL) {
      tools = UNITS_EQUIP_TOOLS_MAX;
    } else if (strstr(type->name, "Dragoon") != NULL || strstr(type->name, "Cavalry") != NULL) {
      muskets = UNITS_EQUIP_MUSKETS;
      horses = UNITS_EQUIP_HORSES;
    } else if (
      strstr(type->name, "Soldier") != NULL || strstr(type->name, "Regular") != NULL ||
      strstr(type->name, "Army") != NULL
    ) {
      muskets = UNITS_EQUIP_MUSKETS;
    } else if (strstr(type->name, "Scout") != NULL) {
      horses = UNITS_EQUIP_HORSES;
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

static int g_units_last_combat = 0;

int units_last_combat_outcome(void) {
  return g_units_last_combat;
}

static int units_foreign_at(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int mover_id,
  int mover_nation
) {
  if (!pool) {
    return -1;
  }
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
    return u->id;
  }
  return -1;
}

bool units_resolve_land_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
) {
  g_units_last_combat = 0;
  ColonizeUnit* atk = units_get(pool, attacker_id);
  ColonizeUnit* def = units_get(pool, defender_id);
  if (!atk || !def || !atk->active || !def->active) {
    return false;
  }
  if (units_is_sea(pool, attacker_id) || units_is_sea(pool, defender_id)) {
    return false;
  }
  const ColonizeUnitType* at = units_type(pool, atk->type_index);
  const ColonizeUnitType* dt = units_type(pool, def->type_index);
  if (!at || !dt) {
    return false;
  }
  int attack = at->attack;
  int defense = dt->defense;
  if (def->orders == UNITS_ORDER_FORTIFIED || def->orders == UNITS_ORDER_FORTIFY) {
    defense *= 2;
  }
  if (attack < 0) {
    attack = 0;
  }
  if (defense < 0) {
    defense = 0;
  }
  const int total = attack + defense;
  bool atk_wins = false;
  if (total <= 0) {
    /* Both helpless — attacker takes the tile. */
    atk_wins = true;
  } else if (!rng) {
    /* Deterministic: attacker wins if attack >= defense. */
    atk_wins = attack >= defense;
  } else {
    const int roll = dos_rng_range(rng, 1, total);
    atk_wins = roll <= attack;
  }
  if (atk_wins) {
    units_despawn(pool, defender_id);
    g_units_last_combat = 1;
    return true;
  }
  units_despawn(pool, attacker_id);
  g_units_last_combat = -1;
  return false;
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

  /* Friendly stacks OK; foreign on-map unit blocks (combat via units_try_move). */
  if (units_foreign_at(pool, x, y, mover_id, mover_nation) >= 0) {
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

bool units_can_afford_move_cost(const ColonizeUnitPool* pool, int unit_id, int cost) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (!unit || !unit->active || unit->moves_left <= 0) {
    return false;
  }
  if (cost <= unit->moves_left) {
    return true;
  }
  /* Full allotment remaining (DOS: spent MP byte == 0) → always allow. */
  const ColonizeUnitType* type = units_type(pool, unit->type_index);
  const int max_mp = type && type->movement > 0 ? type->movement : 1;
  if (unit->moves_left >= max_mp) {
    return true;
  }
  /* Partial overspend needs an RNG roll in units_try_move — not guaranteed. */
  return false;
}

bool units_try_move(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
) {
  g_units_last_combat = 0;
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

  const int foe = units_foreign_at(pool, dest_x, dest_y, unit_id, unit->nation_id);
  if (foe >= 0) {
    /* Land attack into occupied tile; naval / mixed still blocked. */
    if (units_is_sea(pool, unit_id) || units_is_sea(pool, foe)) {
      return false;
    }
    if (!units_resolve_land_combat(pool, unit_id, foe, rng)) {
      return false; /* attacker lost / despawned */
    }
    unit = units_get(pool, unit_id);
    if (!unit) {
      return false;
    }
  }
  if (!units_can_enter(pool, unit->type_index, map, dest_x, dest_y, unit_id, colonies)) {
    return false;
  }

  const int cost = units_move_cost(pool, unit_id, map, dest_x, dest_y);
  const int remaining = unit->moves_left;
  const ColonizeUnitType* type = units_type(pool, unit->type_index);
  const int max_mp = type && type->movement > 0 ? type->movement : 1;
  const bool full_mp = remaining >= max_mp;

  bool allow = false;
  if (cost <= remaining || full_mp) {
    allow = true;
  } else if (rng) {
    /* DOS FUN_465b: range(1, cost); succeed if roll <= remaining. */
    const int roll = dos_rng_range(rng, 1, cost > 0 ? cost : 1);
    allow = roll <= remaining;
  } else {
    return false;
  }

  /*
   * DOS adds the full terrain cost to spent MP before the allow/deny gate for
   * non-combat moves — including failed partial-overspend rolls.
   */
  unit->moves_left = remaining - cost;
  if (unit->moves_left < 0) {
    unit->moves_left = 0;
  }
  if (!allow) {
    return false;
  }

  /* Moving cancels sentry / fortify; Go-To cleared only on arrival elsewhere. */
  if (unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
      unit->orders == UNITS_ORDER_FORTIFIED) {
    unit->orders = UNITS_ORDER_NONE;
  }

  unit->x = dest_x;
  unit->y = dest_y;
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

static int units_sign_i(int v) {
  if (v < 0) {
    return -1;
  }
  if (v > 0) {
    return 1;
  }
  return 0;
}

static int units_chebyshev(int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0);
  const int dy = abs(y1 - y0);
  return dx > dy ? dx : dy;
}

/* Octile-style distance (FUN_124c_0040): max + min/2. */
static int units_octile(int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0);
  const int dy = abs(y1 - y0);
  const int mx = dx > dy ? dx : dy;
  const int mn = dx < dy ? dx : dy;
  return mx + mn / 2;
}

void units_clear_orders(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u) {
    return;
  }
  u->orders = UNITS_ORDER_NONE;
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
}

bool units_orders_skip_turn(const ColonizeUnit* unit) {
  if (!unit || !unit->active) {
    return false;
  }
  return unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFIED;
}

bool units_set_orders(ColonizeUnitPool* pool, int unit_id, int orders) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  if (orders == UNITS_ORDER_FORTIFY || orders == UNITS_ORDER_FORTIFIED) {
    if (!units_is_on_map(u) || units_is_sea(pool, unit_id)) {
      return false;
    }
  }
  if (orders == UNITS_ORDER_SENTRY) {
    /* Map sentry or already aboard (Europe/cargo path uses raw orders=1). */
    if (!units_is_on_map(u) && u->aboard_ship_id < 0) {
      return false;
    }
  }
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
  u->orders = orders;
  if (orders == UNITS_ORDER_SENTRY || orders == UNITS_ORDER_FORTIFY ||
      orders == UNITS_ORDER_FORTIFIED) {
    u->moves_left = 0;
  }
  return true;
}

bool units_order_fortify(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  if (u->orders == UNITS_ORDER_FORTIFIED) {
    return true;
  }
  return units_set_orders(pool, unit_id, UNITS_ORDER_FORTIFY);
}

bool units_order_sentry(ColonizeUnitPool* pool, int unit_id) {
  return units_set_orders(pool, unit_id, UNITS_ORDER_SENTRY);
}

bool units_disband(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  /* Unboard passengers first if disbanding a ship — despawn handles cargo in
   * units_despawn paths; keep simple: refuse sea with cargo for T0. */
  if (units_is_sea(pool, unit_id) && u->cargo_count > 0) {
    return false;
  }
  if (u->aboard_ship_id >= 0) {
    /* Leave ship hold then despawn. */
    ColonizeUnit* ship = units_get(pool, u->aboard_ship_id);
    if (ship) {
      for (int i = 0; i < ship->cargo_count; ++i) {
        if (ship->cargo_ids[i] == unit_id) {
          for (int j = i; j < ship->cargo_count - 1; ++j) {
            ship->cargo_ids[j] = ship->cargo_ids[j + 1];
          }
          ship->cargo_count--;
          break;
        }
      }
    }
    u->aboard_ship_id = -1;
  }
  return units_despawn(pool, unit_id);
}

bool units_wake(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  const int prev = u->orders;
  units_clear_orders(pool, unit_id);
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  if (type) {
    u->moves_left = type->movement;
  }
  return prev == UNITS_ORDER_SENTRY || prev == UNITS_ORDER_FORTIFY ||
         prev == UNITS_ORDER_FORTIFIED || prev == UNITS_ORDER_GOTO;
}

bool units_set_goto(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || !map) {
    return false;
  }
  if (dest_x < 0 || dest_y < 0 || dest_x >= (int)map->width || dest_y >= (int)map->height) {
    return false;
  }
  if (u->x == dest_x && u->y == dest_y) {
    units_clear_orders(pool, unit_id);
    return true;
  }
  if (!units_can_enter(pool, u->type_index, map, dest_x, dest_y, unit_id, colonies)) {
    return false;
  }
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = dest_x;
  u->goto_y = dest_y;
  return true;
}

#define UNITS_FLOOD_W 16
#define UNITS_FLOOD_INF 0x3fff
#define UNITS_FLOOD_QMAX 256

static bool units_flood_next_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int gx,
  int gy,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !out_x || !out_y) {
    return false;
  }
  const int origin_x = gx - UNITS_FLOOD_W / 2;
  const int origin_y = gy - UNITS_FLOOD_W / 2;
  int cost[UNITS_FLOOD_W][UNITS_FLOOD_W];
  for (int y = 0; y < UNITS_FLOOD_W; ++y) {
    for (int x = 0; x < UNITS_FLOOD_W; ++x) {
      cost[y][x] = UNITS_FLOOD_INF;
    }
  }

  int qx[UNITS_FLOOD_QMAX];
  int qy[UNITS_FLOOD_QMAX];
  int qh = 0;
  int qt = 0;

  const int dx0 = gx - origin_x;
  const int dy0 = gy - origin_y;
  if (dx0 < 0 || dy0 < 0 || dx0 >= UNITS_FLOOD_W || dy0 >= UNITS_FLOOD_W) {
    return false;
  }
  if (!units_can_enter(pool, u->type_index, map, gx, gy, unit_id, colonies)) {
    return false;
  }
  cost[dy0][dx0] = 1;
  qx[qt] = gx;
  qy[qt] = gy;
  qt = (qt + 1) % UNITS_FLOOD_QMAX;

  int expansions = 0;
  while (qh != qt && expansions < 225) {
    const int cx = qx[qh];
    const int cy = qy[qh];
    qh = (qh + 1) % UNITS_FLOOD_QMAX;
    ++expansions;
    const int lx = cx - origin_x;
    const int ly = cy - origin_y;
    const int base = cost[ly][lx];
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int nx = cx + dx;
        const int ny = cy + dy;
        const int nlx = nx - origin_x;
        const int nly = ny - origin_y;
        if (nlx < 0 || nly < 0 || nlx >= UNITS_FLOOD_W || nly >= UNITS_FLOOD_W) {
          continue;
        }
        if (!units_can_enter(pool, u->type_index, map, nx, ny, unit_id, colonies)) {
          continue;
        }
        const int edge = units_move_cost(pool, unit_id, map, nx, ny);
        const int nc = base + (edge > 0 ? edge : 1);
        if (nc < cost[nly][nlx]) {
          cost[nly][nlx] = nc;
          const int next_t = (qt + 1) % UNITS_FLOOD_QMAX;
          if (next_t != qh) {
            qx[qt] = nx;
            qy[qt] = ny;
            qt = next_t;
          }
        }
      }
    }
  }

  const int ulx = u->x - origin_x;
  const int uly = u->y - origin_y;
  if (ulx < 0 || uly < 0 || ulx >= UNITS_FLOOD_W || uly >= UNITS_FLOOD_W) {
    return false;
  }
  if (cost[uly][ulx] >= UNITS_FLOOD_INF) {
    return false;
  }

  int best_x = -1;
  int best_y = -1;
  int best_cost = cost[uly][ulx];
  int best_tie = 1 << 30;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      const int nx = u->x + dx;
      const int ny = u->y + dy;
      const int nlx = nx - origin_x;
      const int nly = ny - origin_y;
      if (nlx < 0 || nly < 0 || nlx >= UNITS_FLOOD_W || nly >= UNITS_FLOOD_W) {
        continue;
      }
      const int c = cost[nly][nlx];
      if (c >= best_cost) {
        continue;
      }
      if (!units_can_enter(pool, u->type_index, map, nx, ny, unit_id, colonies)) {
        continue;
      }
      const int step_cost = units_move_cost(pool, unit_id, map, nx, ny);
      if (!units_can_afford_move_cost(pool, unit_id, step_cost)) {
        continue;
      }
      const int tie = units_octile(nx, ny, gx, gy);
      if (best_x < 0 || c < best_cost || (c == best_cost && tie < best_tie)) {
        best_cost = c;
        best_tie = tie;
        best_x = nx;
        best_y = ny;
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

#define UNITS_BFS_MAX 2048

static bool units_bfs_next_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int gx,
  int gy,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !map || !out_x || !out_y) {
    return false;
  }
  const int w = (int)map->width;
  const int h = (int)map->height;
  if (w <= 0 || h <= 0 || w * h > 128 * 128) {
    /* Huge maps: fall back without allocating a full visit grid. */
    return false;
  }

  const int cells = w * h;
  uint8_t* visited = (uint8_t*)calloc((size_t)cells, 1);
  int* parent = (int*)malloc((size_t)cells * sizeof(int));
  if (!visited || !parent) {
    free(visited);
    free(parent);
    return false;
  }
  for (int i = 0; i < cells; ++i) {
    parent[i] = -1;
  }

  int* qx = (int*)malloc((size_t)UNITS_BFS_MAX * sizeof(int));
  int* qy = (int*)malloc((size_t)UNITS_BFS_MAX * sizeof(int));
  if (!qx || !qy) {
    free(visited);
    free(parent);
    free(qx);
    free(qy);
    return false;
  }

  int qh = 0;
  int qt = 0;
  const int start = u->y * w + u->x;
  visited[start] = 1;
  qx[qt] = u->x;
  qy[qt] = u->y;
  qt++;

  bool found = false;
  while (qh < qt && qt < UNITS_BFS_MAX) {
    const int cx = qx[qh];
    const int cy = qy[qh];
    ++qh;
    if (cx == gx && cy == gy) {
      found = true;
      break;
    }
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int nx = cx + dx;
        const int ny = cy + dy;
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
          continue;
        }
        const int ni = ny * w + nx;
        if (visited[ni]) {
          continue;
        }
        if (!units_can_enter(pool, u->type_index, map, nx, ny, unit_id, colonies)) {
          continue;
        }
        visited[ni] = 1;
        parent[ni] = cy * w + cx;
        if (qt < UNITS_BFS_MAX) {
          qx[qt] = nx;
          qy[qt] = ny;
          qt++;
        }
      }
    }
  }

  bool ok = false;
  if (found) {
    int cur = gy * w + gx;
    int prev = parent[cur];
    while (prev >= 0 && prev != start) {
      cur = prev;
      prev = parent[cur];
    }
    if (prev == start) {
      *out_x = cur % w;
      *out_y = cur / w;
      if (units_can_afford_move_cost(
            pool, unit_id, units_move_cost(pool, unit_id, map, *out_x, *out_y)
          )) {
        ok = true;
      }
    }
  }

  free(visited);
  free(parent);
  free(qx);
  free(qy);
  return ok;
}

static bool units_greedy_next_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int gx,
  int gy,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !out_x || !out_y) {
    return false;
  }
  const int sdx = units_sign_i(gx - u->x);
  const int sdy = units_sign_i(gy - u->y);
  const int try_dx[5] = {sdx, sdx, 0, sdx, -sdx};
  const int try_dy[5] = {sdy, 0, sdy, -sdy, sdy};

  int best_x = -1;
  int best_y = -1;
  int best_score = 1 << 30;
  for (int i = 0; i < 5; ++i) {
    if (try_dx[i] == 0 && try_dy[i] == 0) {
      continue;
    }
    const int nx = u->x + try_dx[i];
    const int ny = u->y + try_dy[i];
    if (!units_can_enter(pool, u->type_index, map, nx, ny, unit_id, colonies)) {
      continue;
    }
    const int step_cost = units_move_cost(pool, unit_id, map, nx, ny);
    if (!units_can_afford_move_cost(pool, unit_id, step_cost)) {
      continue;
    }
    const int score = units_octile(nx, ny, gx, gy) * 10 + step_cost;
    if (score < best_score) {
      best_score = score;
      best_x = nx;
      best_y = ny;
    }
  }
  if (best_x < 0) {
    /* Full 8-neighbor fallback. */
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int nx = u->x + dx;
        const int ny = u->y + dy;
        if (!units_can_enter(pool, u->type_index, map, nx, ny, unit_id, colonies)) {
          continue;
        }
        const int step_cost = units_move_cost(pool, unit_id, map, nx, ny);
        if (!units_can_afford_move_cost(pool, unit_id, step_cost)) {
          continue;
        }
        const int score = units_octile(nx, ny, gx, gy) * 10 + step_cost;
        if (score < best_score) {
          best_score = score;
          best_x = nx;
          best_y = ny;
        }
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

bool units_next_goto_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || !map || !out_x || !out_y) {
    return false;
  }
  if (u->orders != UNITS_ORDER_GOTO) {
    return false;
  }
  const int gx = u->goto_x;
  const int gy = u->goto_y;
  if (gx < 0 || gy < 0 || gx >= (int)map->width || gy >= (int)map->height ||
      gx >= UNITS_GOTO_NONE || gy >= UNITS_GOTO_NONE) {
    return false;
  }
  if (u->x == gx && u->y == gy) {
    return false;
  }

  const int adx = abs(gx - u->x);
  const int ady = abs(gy - u->y);

  /* Adjacent: sign-step (FUN_6662_0086). */
  if (units_chebyshev(u->x, u->y, gx, gy) < 2) {
    const int nx = u->x + units_sign_i(gx - u->x);
    const int ny = u->y + units_sign_i(gy - u->y);
    if (units_can_enter(pool, u->type_index, map, nx, ny, unit_id, colonies) &&
        units_can_afford_move_cost(
          pool, unit_id, units_move_cost(pool, unit_id, map, nx, ny)
        )) {
      *out_x = nx;
      *out_y = ny;
      return true;
    }
    return units_greedy_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y);
  }

  /* Near: both axes within 6 — destination cost flood (FUN_6662_00f2). */
  if (adx <= 6 && ady <= 6) {
    if (units_flood_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y)) {
      return true;
    }
    return units_greedy_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y);
  }

  /* Far: uniform BFS first step, else greedy. */
  if (units_bfs_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y)) {
    return true;
  }
  /* Intermediate waypoint within flood range, then flood. */
  {
    int wx = u->x + (adx > 6 ? units_sign_i(gx - u->x) * 6 : (gx - u->x));
    int wy = u->y + (ady > 6 ? units_sign_i(gy - u->y) * 6 : (gy - u->y));
    if (wx < 0) {
      wx = 0;
    }
    if (wy < 0) {
      wy = 0;
    }
    if (wx >= (int)map->width) {
      wx = (int)map->width - 1;
    }
    if (wy >= (int)map->height) {
      wy = (int)map->height - 1;
    }
    if (units_flood_next_step(pool, unit_id, map, colonies, wx, wy, out_x, out_y)) {
      return true;
    }
  }
  return units_greedy_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y);
}

bool units_advance_goto_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || !map) {
    return false;
  }
  if (u->orders != UNITS_ORDER_GOTO) {
    return false;
  }
  const int gx = u->goto_x;
  const int gy = u->goto_y;
  if (gx < 0 || gy < 0 || gx >= UNITS_GOTO_NONE || gy >= UNITS_GOTO_NONE) {
    units_clear_orders(pool, unit_id);
    return false;
  }
  if (u->x == gx && u->y == gy) {
    units_clear_orders(pool, unit_id);
    return false;
  }
  if (u->moves_left <= 0) {
    return false;
  }
  int nx = -1;
  int ny = -1;
  if (!units_next_goto_step(pool, unit_id, map, colonies, &nx, &ny)) {
    return false;
  }
  if (!units_try_move(pool, unit_id, map, nx, ny, colonies, NULL)) {
    return false;
  }
  u = units_get(pool, unit_id);
  if (u && u->x == gx && u->y == gy) {
    units_clear_orders(pool, unit_id);
  }
  return true;
}

bool units_advance_goto(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
) {
  bool moved = false;
  while (units_advance_goto_one_step(pool, unit_id, map, colonies)) {
    moved = true;
  }
  return moved;
}

int units_advance_all_goto_one_step(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
) {
  if (!pool || !map) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->orders != UNITS_ORDER_GOTO || !units_is_on_map(u)) {
      continue;
    }
    if (units_advance_goto_one_step(pool, u->id, map, colonies)) {
      n++;
    }
  }
  return n;
}

int units_advance_all_goto(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
) {
  if (!pool || !map) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->orders != UNITS_ORDER_GOTO || !units_is_on_map(u)) {
      continue;
    }
    if (units_advance_goto(pool, u->id, map, colonies)) {
      n++;
    }
  }
  return n;
}

bool units_is_pioneer(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || units_is_sea(pool, unit_id)) {
    return false;
  }
  /* Skill alone is not enough — plow/road require carried tools. */
  int tools = 0;
  units_founder_loot(pool, unit_id, &tools, NULL, NULL);
  return tools > 0;
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

bool units_pick_landfall_tile(
  const ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int prefer_x,
  int prefer_y,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!pool || !ship || !map || !out_x || !out_y || !units_is_sea(pool, ship_id)) {
    return false;
  }
  int pax_type = -1;
  int pax_id = -1;
  if (ship->cargo_count > 0) {
    pax_id = ship->cargo_ids[0];
    const ColonizeUnit* pax = units_get_const(pool, pax_id);
    if (pax) {
      pax_type = pax->type_index;
    }
  }
  if (pax_type < 0) {
    return false;
  }

  static const int k_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int k_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  const bool have_prefer = prefer_x >= 0 && prefer_y >= 0;
  int best_x = -1;
  int best_y = -1;
  int best_d = 0x7fffffff;
  for (int d = 0; d < 8; ++d) {
    const int nx = ship->x + k_dx[d];
    const int ny = ship->y + k_dy[d];
    if (!map_tile_is_land(map, nx, ny) || map_tile_is_water(map, nx, ny)) {
      continue;
    }
    if (!units_can_enter(pool, pax_type, map, nx, ny, pax_id, colonies)) {
      continue;
    }
    int dist = 0;
    if (have_prefer) {
      const int dx = nx - prefer_x;
      const int dy = ny - prefer_y;
      dist = dx * dx + dy * dy;
    }
    if (best_x < 0 || dist < best_d) {
      best_x = nx;
      best_y = ny;
      best_d = dist;
    }
  }
  if (best_x < 0) {
    return false;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

int units_landfall_unload_all(
  ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!pool || !ship || !map || !units_is_sea(pool, ship_id) || ship->cargo_count <= 0) {
    return 0;
  }
  const int saved_sel = pool->selected_id;
  int n = 0;
  /* Snapshot ids — cargo_ids shift as we unload. */
  int ids[COLONIZE_UNIT_CARGO_MAX];
  const int count = ship->cargo_count < COLONIZE_UNIT_CARGO_MAX ? ship->cargo_count
                                                               : COLONIZE_UNIT_CARGO_MAX;
  for (int i = 0; i < count; ++i) {
    ids[i] = ship->cargo_ids[i];
  }
  for (int i = 0; i < count; ++i) {
    ColonizeUnit* pax = units_get(pool, ids[i]);
    if (!pax || pax->aboard_ship_id != ship_id) {
      continue;
    }
    /* Wake sentry so unload does not leave orders=1 ashore. */
    if (pax->orders == 1) {
      pax->orders = 0;
    }
    if (units_unload_passenger(pool, ship_id, ids[i], map, dest_x, dest_y, colonies)) {
      n++;
    }
  }
  pool->selected_id = saved_sel;
  return n;
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
  const bool armed = unit->muskets > 0;
  const bool mounted = unit->horses > 0;
  const bool has_tools = unit->tools > 0;
  if (armed && mounted) {
    if (unit->profession == UNITS_JOB_DRAGOON) {
      snprintf(buf, sizeof(buf), "Veteran Dragoon");
      return buf;
    }
    return "Dragoon";
  }
  if (armed) {
    if (unit->profession == UNITS_JOB_SOLDIER) {
      snprintf(buf, sizeof(buf), "Veteran Soldier");
      return buf;
    }
    return "Soldier";
  }
  if (mounted) {
    if (unit->profession == UNITS_JOB_SCOUT) {
      snprintf(buf, sizeof(buf), "Seasoned Scout");
      return buf;
    }
    return "Scout";
  }
  if (has_tools) {
    if (unit->profession == UNITS_JOB_PIONEER) {
      snprintf(buf, sizeof(buf), "Hardy Pioneer");
      return buf;
    }
    return "Pioneer";
  }
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

int units_working_colonist_sprite(
  const ColonizeUnitPool* pool,
  int unit_type_index,
  int profession
) {
  if (profession == UNITS_JOB_PIONEER) {
    return UNITS_ICON_HARDY_PIONEER_WORK;
  }
  if (profession == UNITS_JOB_SOLDIER) {
    return UNITS_ICON_VETERAN_SOLDIER_WORK;
  }
  const ColonizeUnitType* type = units_type(pool, unit_type_index);
  return type ? type->icon_sprite : -1;
}

int units_map_sprite(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (!unit) {
    return -1;
  }
  const ColonizeUnitType* type = units_type(pool, unit->type_index);
  if (!type) {
    return -1;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(pool, unit_id, &tools, &muskets, &horses);
  if (muskets > 0 && horses > 0) {
    return (unit->profession == UNITS_JOB_DRAGOON) ? UNITS_ICON_VETERAN_DRAGOON
                                                   : UNITS_ICON_DRAGOON;
  }
  if (muskets > 0) {
    return (unit->profession == UNITS_JOB_SOLDIER) ? UNITS_ICON_VETERAN_SOLDIER
                                                  : UNITS_ICON_SOLDIER;
  }
  if (horses > 0) {
    return (unit->profession == UNITS_JOB_SCOUT) ? UNITS_ICON_SEASONED_SCOUT : UNITS_ICON_SCOUT;
  }
  if (tools > 0) {
    return (unit->profession == UNITS_JOB_PIONEER) ? UNITS_ICON_HARDY_PIONEER
                                                  : UNITS_ICON_PIONEER;
  }
  return type->icon_sprite;
}

int units_display_type_index(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (!unit) {
    return -1;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(pool, unit_id, &tools, &muskets, &horses);
  /* Col1 @UNIT indices: match equipment → displayed type for chrome placement. */
  if (muskets > 0 && horses > 0) {
    const int t = units_find_type(pool, "Dragoons");
    return t >= 0 ? t : 4;
  }
  if (muskets > 0) {
    const int t = units_find_type(pool, "Soldiers");
    return t >= 0 ? t : 1;
  }
  if (horses > 0) {
    const int t = units_find_type(pool, "Scouts");
    return t >= 0 ? t : 5;
  }
  if (tools > 0) {
    const int t = units_find_type(pool, "Pioneers");
    return t >= 0 ? t : 2;
  }
  return unit->type_index;
}

static int units_count_on_map_tile(const ColonizeUnitPool* pool, int x, int y) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (units_is_on_map(u) && u->x == x && u->y == y) {
      n++;
    }
  }
  return n;
}

/* Prefer selected unit on the tile; else highest id (drawn last previously). */
static int units_top_on_map_tile(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  bool selected_visible
) {
  int top = -1;
  int top_id = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->id == pool->selected_id && !selected_visible && u->orders != UNITS_ORDER_GOTO) {
      continue;
    }
    if (u->id == pool->selected_id) {
      return u->id;
    }
    if (u->id > top_id) {
      top_id = u->id;
      top = u->id;
    }
  }
  return top;
}

void units_render_on_map(
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* nation_sheet,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  bool selected_visible,
  const ColonizeWorldMap* fog_map,
  int fog_nation
) {
  if (!pool || !nation_sheet || !framebuffer) {
    return;
  }

  /* One sprite per tile (top unit); stack chrome when more share the tile. */
  bool visited[COLONIZE_UNITS_MAX];
  memset(visited, 0, sizeof(visited));

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* unit = &pool->units[i];
    if (!units_is_on_map(unit) || visited[i]) {
      continue;
    }
    if (fog_map && !map_tile_seen_by(fog_map, unit->x, unit->y, fog_nation)) {
      continue;
    }
    const int sx = unit->x - view_x;
    const int sy = unit->y - view_y;
    if (sx < 0 || sy < 0 || sx >= view_cols || sy >= view_rows) {
      continue;
    }

    /* Mark all on-map units on this tile visited. */
    for (int j = 0; j < COLONIZE_UNITS_MAX; ++j) {
      const ColonizeUnit* u = &pool->units[j];
      if (units_is_on_map(u) && u->x == unit->x && u->y == unit->y) {
        visited[j] = true;
      }
    }

    const int top_id = units_top_on_map_tile(pool, unit->x, unit->y, selected_visible);
    if (top_id < 0) {
      continue;
    }
    const ColonizeUnit* top = units_get_const(pool, top_id);
    if (!top) {
      continue;
    }

    const int sprite = units_map_sprite(pool, top->id);
    if (sprite < 0 || sprite >= nation_sheet->sprite_count) {
      continue;
    }

    const int px = origin_x + sx * tile_w;
    const int py = origin_y + sy * tile_h;
    const int dtype = units_display_type_index(pool, top->id);
    const int on_tile = units_count_on_map_tile(pool, top->x, top->y);
    const bool stacked = on_tile > 1;
    const bool aboard = top->aboard_ship_id >= 0;

    unit_chrome_blit_unit(
      framebuffer,
      font,
      nation_sheet,
      sprite,
      px,
      py,
      dtype,
      top->nation_id,
      top->orders,
      stacked,
      aboard
    );
  }
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
    ship->orders = UNITS_ORDER_GOTO;
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
