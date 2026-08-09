#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"

/* Defined later; used by naval hold plunder before combat despawn. */
int units_load_goods(ColonizeUnitPool* pool, int unit_id, int cargo_type, int amount);
bool units_advance_goto_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
);
static void units_occupancy_refresh_tile(ColonizeUnitPool* pool, int x, int y, int except_id);
static void units_map_set_owner_nibble(ColonizeWorldMap* map, int x, int y, int nation_or_ff);
static ColonizeWorldMap* g_units_occupancy_map = NULL;

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
  slot->col1_vis_mask = 0; /* FUN_1427_0992: owner bit via units_set_nation */
  slot->aboard_ship_id = -1;
  slot->cargo_count = 0;
  memset(slot->cargo_ids, 0, sizeof(slot->cargo_ids));
  memset(slot->hold_goods_type, 0, sizeof(slot->hold_goods_type));
  memset(slot->hold_goods_amount, 0, sizeof(slot->hold_goods_amount));
  slot->orders = 0;
  slot->goto_x = 0xFF;
  slot->goto_y = 0xFF;
  slot->follow_unit_id = -1;
  /*
   * FUN_1427_06b4: type cargo>0 → profession 0 (ships/wagons); else 0x1c.
   * Exporting ships as profession 28 makes DOS treat the tile as a land stack
   * and peel the caravel off its transport_chain (sidebar "unloaded").
   */
  slot->profession = type->cargo > 0 ? 0 : UNITS_JOB_NONE;
  slot->tools = 0;
  slot->muskets = 0;
  slot->horses = 0;
  slot->home_tribe_id = -1;
  slot->turns_worked = 0;
  slot->last_dir = 0;
  slot->col1_unknown15 = 0;
  slot->col1_ai_plan = COL1_UNIT_UNKNOWN16_HI_DEFAULT;
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
  pool->unit_count++;
  if (units_is_on_map(slot)) {
    units_occupancy_refresh_tile(pool, slot->x, slot->y, -1);
  }
  diag_info("Spawned unit id=%d type=%s at (%d,%d)", slot->id, type->name, x, y);
  return slot->id;
}

void units_set_nation(ColonizeUnit* unit, int nation_id) {
  if (!unit) {
    return;
  }
  unit->nation_id = nation_id;
  if (nation_id >= 0 && nation_id < 4) {
    /* Euro owner visibility only — clear polluted foreign bits (DOS draw uses hi nibble). */
    unit->col1_vis_mask = (uint8_t)(1u << (nation_id & 3));
  } else {
    /* Natives: not visible through euro fog until observed (EOT / contact). */
    unit->col1_vis_mask = 0;
  }
  /*
   * Spawn sets nation after units_spawn_allow_stack already refreshed occupancy
   * with nation 0 — restamp owner now (FUN_1427_02ca).
   */
  if (g_units_occupancy_map && units_is_on_map(unit) && unit->x < 200 && unit->y < 200) {
    if (nation_id > 3 && g_units_occupancy_map->layer2) {
      const int i = unit->y * g_units_occupancy_map->width + unit->x;
      if (i >= 0 && (size_t)i < g_units_occupancy_map->tile_count &&
          (g_units_occupancy_map->layer2[i] & MAP_OCCUPANCY_HAS_CITY) != 0) {
        return;
      }
    }
    units_map_set_owner_nibble(g_units_occupancy_map, unit->x, unit->y, nation_id);
  }
}

int units_spawn_treasure_train(
  ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int gold
) {
  /*
   * Cite: Colonization.pdf Treasure Trains; NAMES "Treasure"; COL1 cargo_hold
   * [0..1] LE16 gold mirrored in hold_goods_amount (game_loop /
   * ai_euro_treasure_gold_from_unit). Gold amount is caller-supplied — do not
   * invent a conquest rate here (FUN_5fef_31ea / Cortes gate decide that).
   */
  if (!pool || gold < 0) {
    return -1;
  }
  const int ti = units_find_type(pool, "Treasure");
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(pool, ti, x, y);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(pool, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  const unsigned g = (unsigned)gold;
  u->hold_goods_amount[0] = (int)(g & 0xffu);
  u->hold_goods_amount[1] = (int)((g >> 8) & 0xffu);
  return id;
}

int units_tick_treasure_outside_colony(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  char* status,
  size_t status_size
) {
  if (!pool || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  int removed = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(pool, u->type_index);
    if (!ty || !ty->name[0] || strstr(ty->name, "Treasure") == NULL) {
      continue;
    }
    int on_own_colony = 0;
    if (colonies) {
      const int cid = colonies_id_at(colonies, u->x, u->y);
      if (cid >= 0) {
        const ColonizeColony* c = colonies_get(colonies, cid);
        if (c && c->active && c->nation_id == nation_id) {
          on_own_colony = 1;
        }
      }
    }
    if (on_own_colony) {
      u->turns_worked = 0;
      continue;
    }
    /* FUN_3844_0004: unit+0x16++; remove when > 8. */
    if (u->turns_worked < 255) {
      u->turns_worked++;
    }
    if (u->turns_worked <= 8) {
      continue;
    }
    (void)units_despawn(pool, u->id);
    removed++;
  }
  if (removed > 0 && status && status_size > 0) {
    snprintf(
      status,
      status_size,
      "A Treasure Train was lost after too long outside a colony."
    );
  }
  return removed;
}

int units_cortes_cash_coastal_treasures(
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  ColonizeWorldMap* map,
  EuropeScreen* europe,
  ColonizeCol1Save* col1,
  int nation_id
) {
  if (!pool || !colonies || !map || !europe || !col1 || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (!founding_fathers_cortes_free_king_galleon(col1, nation_id)) {
    return 0;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  europe->gold = (int)nat->gold;
  europe->tax_percent = (int)nat->tax_rate;
  int cashed = 0;
  /* Snapshot ids — despawn mutates the pool. */
  int ids[COLONIZE_UNITS_MAX];
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(pool, u->type_index);
    if (!ty || !ty->name[0] || strstr(ty->name, "Treasure") == NULL) {
      continue;
    }
    ids[n++] = u->id;
  }
  for (int i = 0; i < n; ++i) {
    ColonizeUnit* treasure = units_get(pool, ids[i]);
    if (!treasure || !treasure->active) {
      continue;
    }
    const int cid = colonies_id_at(colonies, treasure->x, treasure->y);
    if (cid < 0) {
      continue;
    }
    const ColonizeColony* c = colonies_get(colonies, cid);
    if (!c || !c->active || c->nation_id != nation_id) {
      continue;
    }
    if (!map_tile_is_coastal(map, c->x, c->y)) {
      continue;
    }
    const unsigned lo = (unsigned)(treasure->hold_goods_amount[0] & 0xff);
    const unsigned hi = (unsigned)(treasure->hold_goods_amount[1] & 0xff);
    const int value = (int)(lo | (hi << 8));
    if (value > 0) {
      (void)europe_cash_treasure(europe, value);
      nat->gold = (uint32_t)(europe->gold < 0 ? 0 : europe->gold);
    }
    (void)units_despawn(pool, treasure->id);
    cashed++;
  }
  return cashed;
}

bool units_is_on_map(const ColonizeUnit* unit) {
  /* id < 0 = cleared/ghost slot (tests may flip active without respawn). */
  return unit && unit->active && unit->id >= 0 && unit->aboard_ship_id < 0;
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
  unit->turns_worked = 0;
  unit->last_dir = 0;
  unit->col1_unknown15 = 0;
  unit->col1_ai_plan = 0;
  unit->col1_vis_mask = 0;
}

bool units_despawn(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* unit = units_get(pool, unit_id);
  if (!unit) {
    return false;
  }
  const int ox = unit->x;
  const int oy = unit->y;
  const int was_on_map = units_is_on_map(unit) ? 1 : 0;
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
  if (was_on_map) {
    units_occupancy_refresh_tile(pool, ox, oy, unit_id);
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
static ColonizeEnterReason g_units_last_enter_reason = COLONIZE_ENTER_OK;
static const ColonizeCol1Save* g_units_ff_col1 = NULL;
static ColonizeCol1Save* g_units_fallout_col1 = NULL;
static ColonizeWorldMap* g_units_fallout_map = NULL;
static int g_units_conquest_gold = -1;
static const ColonizeColonyPool* g_units_combat_colonies = NULL;

void units_set_ff_col1(const ColonizeCol1Save* col1) {
  g_units_ff_col1 = col1;
}

void units_set_occupancy_map(ColonizeWorldMap* map) {
  g_units_occupancy_map = map;
}

static int units_tile_has_on_map_unit(const ColonizeUnitPool* pool, int x, int y, int except_id) {
  if (!pool || x < 0 || y < 0 || x >= 200 || y >= 200) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->id == except_id) {
      continue;
    }
    if (u->x == x && u->y == y) {
      return 1;
    }
  }
  return 0;
}

/*
 * FUN_1427_02ca: when a unit is alone on a tile, stamp layer3 owner (nation).
 * Indians skip when the tribe bit is set (FUN_137f_0598). Leaving a tile
 * (FUN_1427_023a) clears presence only — owner nibble stays claimed.
 */
static void units_claim_tile_owner_from_stack(
  ColonizeUnitPool* pool,
  ColonizeWorldMap* map,
  int x,
  int y,
  int except_id
) {
  if (!pool || !map || !map->layer3) {
    return;
  }
  if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return;
  }
  int nation = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->id == except_id) {
      continue;
    }
    if (u->x == x && u->y == y) {
      nation = u->nation_id;
      break;
    }
  }
  if (nation < 0) {
    return;
  }
  if (nation > 3 && map->layer2) {
    const uint8_t l2 = map->layer2[y * map->width + x];
    if ((l2 & MAP_OCCUPANCY_HAS_CITY) != 0) {
      return; /* village tile keeps tribe owner */
    }
  }
  units_map_set_owner_nibble(map, x, y, nation);
}

static void units_occupancy_refresh_tile(ColonizeUnitPool* pool, int x, int y, int except_id) {
  if (!g_units_occupancy_map || !pool) {
    return;
  }
  const int present = units_tile_has_on_map_unit(pool, x, y, except_id);
  map_occupancy_set_layer2(
    g_units_occupancy_map, x, y, MAP_OCCUPANCY_HAS_UNIT, present != 0
  );
  if (present) {
    units_claim_tile_owner_from_stack(pool, g_units_occupancy_map, x, y, except_id);
  }
}

void units_set_native_fallout_context(
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int conquest_gold
) {
  g_units_fallout_col1 = col1;
  g_units_fallout_map = map;
  g_units_conquest_gold = conquest_gold;
}

void units_set_combat_colonies(const ColonizeColonyPool* colonies) {
  g_units_combat_colonies = colonies;
}

int units_last_combat_outcome(void) {
  return g_units_last_combat;
}

ColonizeEnterReason units_last_enter_reason(void) {
  return g_units_last_enter_reason;
}

const char* units_enter_reason_status(ColonizeEnterReason reason) {
  switch (reason) {
  case COLONIZE_ENTER_OK:
  case COLONIZE_ENTER_DOCK:
    return "Moved";
  case COLONIZE_ENTER_LANDFALL:
    return "Landfall";
  case COLONIZE_ENTER_COMBAT_LAND:
  case COLONIZE_ENTER_COMBAT_NAVAL:
    return "Combat";
  case COLONIZE_ENTER_BOUNCE_FOREIGN:
    return "Cannot attack (non-combat unit)";
  case COLONIZE_ENTER_BOUNCE_PEACE:
    return "At peace — cannot attack";
  case COLONIZE_ENTER_BLOCKED_DOMAIN:
    return "Wrong terrain";
  case COLONIZE_ENTER_BLOCKED_EDGE:
    return "Map edge";
  case COLONIZE_ENTER_BLOCKED_HS_SAIL:
    return "Need sail order for high seas";
  case COLONIZE_ENTER_VILLAGE_ILLEGAL:
    return "Illegal entry into village";
  case COLONIZE_ENTER_NO_MP:
    return "No moves left";
  case COLONIZE_ENTER_BLOCKED:
  default:
    return "Move blocked";
  }
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

/*
 * PEDIA George Washington: non-veteran soldier/dragoon who wins combat is
 * automatically upgraded. Name-based type swap (1eca-style) + profession bit
 * so display_name becomes Veteran when @UNIT has no separate Veteran type.
 */
static void units_washington_promote_on_win(
  ColonizeUnitPool* pool,
  ColonizeUnit* winner,
  const ColonizeCol1Save* col1
) {
  if (!pool || !winner || !winner->active || !col1) {
    return;
  }
  if (!founding_fathers_nation_has(col1, winner->nation_id, FF_GEORGE_WASHINGTON)) {
    return;
  }
  const ColonizeUnitType* ut = units_type(pool, winner->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(pool, winner);
  if ((dname && (strstr(dname, "Veteran") || strstr(dname, "Continental"))) ||
      (tname &&
       (strstr(tname, "Veteran") || strstr(tname, "Cont.") || strstr(tname, "Continental")))) {
    return;
  }
  const bool is_dragoon =
    (tname && (strstr(tname, "Dragoon") || strstr(tname, "Cavalry"))) ||
    (dname && (strstr(dname, "Dragoon") || strstr(dname, "Cavalry")));
  const bool is_soldier =
    !is_dragoon &&
    ((tname && strstr(tname, "Soldier") != NULL) || (dname && strstr(dname, "Soldier") != NULL));
  if (!is_soldier && !is_dragoon) {
    return;
  }
  if (is_dragoon) {
    int tgt = units_find_type(pool, "Veteran Dragoon");
    if (tgt < 0) {
      tgt = units_find_type(pool, "Cont. Cav.");
    }
    if (tgt < 0) {
      tgt = units_find_type(pool, "Continental Cavalry");
    }
    if (tgt >= 0) {
      winner->type_index = tgt;
    }
    winner->profession = UNITS_JOB_DRAGOON;
  } else {
    int tgt = units_find_type(pool, "Veteran Soldier");
    if (tgt < 0) {
      tgt = units_find_type(pool, "Cont. Army");
    }
    if (tgt < 0) {
      tgt = units_find_type(pool, "Continental Army");
    }
    if (tgt >= 0) {
      winner->type_index = tgt;
    }
    winner->profession = UNITS_JOB_SOLDIER;
  }
}

/* PEDIA Francis Drake: privateer combat strengths +50% → multiply by 3/2. */
static int units_drake_scale_strength(
  const ColonizeUnitPool* pool,
  const ColonizeUnit* unit,
  int strength,
  const ColonizeCol1Save* col1
) {
  if (!pool || !unit || !col1 || strength <= 0) {
    return strength;
  }
  const ColonizeUnitType* t = units_type(pool, unit->type_index);
  if (!t || strstr(t->name, "Privateer") == NULL) {
    return strength;
  }
  if (!founding_fathers_nation_has(col1, unit->nation_id, FF_FRANCIS_DRAKE)) {
    return strength;
  }
  return (strength * 3) / 2;
}

/* FUN_137f_0228 — set continent high nibble (nation / 0xf unowned). */
static void units_map_set_owner_nibble(ColonizeWorldMap* map, int x, int y, int nation_or_ff) {
  if (!map || !map->layer3 || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return;
  }
  const int i = y * map->width + x;
  const uint8_t low = (uint8_t)(map->layer3[i] & 0x0fu);
  const uint8_t hi = (uint8_t)(((unsigned)nation_or_ff & 0x0fu) << 4);
  map->layer3[i] = (uint8_t)(low | hi);
}

static int units_count_nation_on_tile(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id
) {
  if (!pool) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->aboard_ship_id >= 0) {
      continue;
    }
    if (u->x == x && u->y == y && u->nation_id == nation_id) {
      ++count;
    }
  }
  return count;
}

static bool units_tile_has_tribe(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return false;
  }
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    if ((int)col1->tribe[i].x == x && (int)col1->tribe[i].y == y) {
      return true;
    }
  }
  return false;
}

int col1_destroy_tribe_at(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int x,
  int y
) {
  if (!col1 || !col1->tribe || col1->head.tribe_count == 0) {
    return -1;
  }
  int found = -1;
  int nation_id = -1;
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    if ((int)col1->tribe[i].x == x && (int)col1->tribe[i].y == y) {
      found = (int)i;
      nation_id = (int)col1->tribe[i].nation_id;
      break;
    }
  }
  if (found < 0 || nation_id < 4) {
    return -1;
  }

  const uint16_t old_count = col1->head.tribe_count;
  if (found + 1 < (int)old_count) {
    memmove(
      &col1->tribe[found],
      &col1->tribe[found + 1],
      ((size_t)old_count - (size_t)found - 1u) * sizeof(ColonizeCol1Tribe)
    );
  }
  col1->head.tribe_count = (uint16_t)(old_count - 1u);

  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units->units[i];
      if (!u->active || u->home_tribe_id < 0) {
        continue;
      }
      if (u->home_tribe_id == found) {
        u->home_tribe_id = -1;
      } else if (u->home_tribe_id > found) {
        u->home_tribe_id--;
      }
    }
  }

  if (map) {
    units_map_set_owner_nibble(map, x, y, 0x0f);
  }
  return nation_id;
}

int units_cortes_conquest_treasure_gold(
  const ColonizeCol1Save* col1,
  int attacker_nation_id,
  ColonizeDosRng* rng,
  int rich_capital
) {
  /*
   * Peel FUN_5fef_31ea amount → gold×100 (viceroy_unpacked.c ~101407–101495).
   * Locals: -6 Cortes FF10, -0xa8 Spanish (nation==2), -0xcc rich/capital
   * (callers: tribe.state.capital from fallout), difficulty col1->head.difficulty
   * (bands 0..3; ≥3 → band 3).
   */
  if (!rng || !col1 || attacker_nation_id < 0 || attacker_nation_id > 3) {
    return 0;
  }
  const int cortes =
    founding_fathers_cortes_guarantees_conquest_treasure(col1, attacker_nation_id) ? 1 : 0;
  const int spanish = (attacker_nation_id == 2) ? 1 : 0;
  const int rich = rich_capital ? 1 : 0;
  int diff = (int)col1->head.difficulty;
  if (diff < 0) {
    diff = 0;
  }
  if (diff > 3) {
    diff = 3;
  }

  int amount = 0; /* DOS -0xce before ×100 */
  if (diff == 0) {
    const int hi = ((spanish == 0) ? 3 : 0) + 3;
    const int r0 = dos_rng_range(rng, 0, hi);
    if (r0 == 0 || rich || cortes) {
      amount = dos_rng_range(rng, 2, 4);
    }
    if (rich) {
      amount <<= 1;
    }
    if (cortes) {
      amount += amount >> 1;
    }
  } else if (diff == 1) {
    /* -0x62 set from Spanish but roll is always 04d4(0,2). */
    const int r0 = dos_rng_range(rng, 0, 2);
    if (r0 == 0 || rich || cortes) {
      amount = dos_rng_range(rng, 3, 8);
    }
    if (rich) {
      amount <<= 1;
    }
    if (cortes) {
      amount += amount >> 1;
    }
  } else if (diff == 2) {
    const int lo = rich ? 4 : 2;
    const int hi = rich ? 10 : 6;
    const int r = dos_rng_range(rng, lo, hi);
    amount = (r + (cortes ? 6 : 0) + (spanish ? 3 : 0)) * 10;
  } else {
    /* difficulty ≥3: 16-bit wrap of (cc==0 ? 0xfff7 : 0) + 0x19 → 16 or 25. */
    amount = dos_rng_range(rng, 0, 4) + 2;
    const uint16_t mult16 =
      (uint16_t)((rich ? 0 : 0xfff7) + 0x19 + (cortes ? 10 : 0) + (spanish ? 5 : 0));
    amount *= (int)mult16;
  }
  if (amount <= 0) {
    return 0;
  }
  return amount * 100;
}

/*
 * FUN_5fef_31ea / 1b0e subjugated convert-join threshold (before rng).
 * mission 0xff → ineligible (-1). Else low-nibble must equal attacker.
 * Base 4, Jesuit bit0x10 → 8; Spanish +4; Sepulveda +4; Las Casas −4.
 * Succeed when dos_rng_range(0,12) < threshold. Cite: viceroy ~101155–101184;
 * PEDIA @FATHER23; GAME.TXT @INDIANSLAVES.
 */
static int units_subjugated_convert_join_threshold(
  const ColonizeCol1Save* col1,
  int attacker_nation_id,
  uint8_t mission
) {
  if (!col1 || attacker_nation_id < 0 || attacker_nation_id > 3) {
    return -1;
  }
  if ((int8_t)mission < 0) {
    return -1; /* COL1_TRIBE_MISSION_NONE 0xff */
  }
  if ((mission & COL1_TRIBE_MISSION_NATION_MASK) != (uint8_t)attacker_nation_id) {
    return -1;
  }
  int thr = (mission & COL1_TRIBE_MISSION_JESUIT_BIT) ? 8 : 4;
  if (attacker_nation_id == 2) {
    thr += 4; /* Spanish nation id — same as Cortes peel */
  }
  if (founding_fathers_sepulveda_convert_join_bonus(col1, attacker_nation_id)) {
    thr += 4;
  }
  if (founding_fathers_nation_has(col1, attacker_nation_id, FF_BARTOLOME_DE_LAS_CASAS)) {
    thr -= 4;
  }
  return thr;
}

/* Spawn Colonists + Convert profession (@JOB 27). Returns unit id or -1. */
static int units_spawn_subjugated_convert(
  ColonizeUnitPool* units,
  int x,
  int y,
  int nation_id
) {
  if (!units || nation_id < 0 || nation_id > 3) {
    return -1;
  }
  int ti = units_find_type(units, "Colonists");
  if (ti < 0) {
    ti = units_find_type(units, "Free Colonists");
  }
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(units, ti, x, y);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, nation_id);
  u->profession = 27; /* NAMES @JOB Convert / COLONIZE_PROF_CONVERT */
  return id;
}

bool units_try_native_settlement_fallout(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int attacker_nation_id,
  int defender_nation_id,
  int tile_x,
  int tile_y,
  int gold_amount,
  ColonizeDosRng* rng
) {
  /*
   * Post-win stand-in for FUN_5fef_31ea (structural): destroy native village
   * when the last same-nation Brave leaves the tribe tile after combat win.
   * Convert-join (before destroy) when mission owned by attacker; Cortes
   * treasure after. Non-Cortes unknown amount stays no-spawn.
   */
  if (!col1 || !units || defender_nation_id < 4) {
    return false;
  }
  if (!units_tile_has_tribe(col1, tile_x, tile_y)) {
    return false;
  }
  if (units_count_nation_on_tile(units, tile_x, tile_y, defender_nation_id) > 0) {
    return false;
  }

  /*
   * FUN_5fef_31ea stack-local -0xcc (rich): map to ColonizeCol1TribeState.capital
   * before destroy. Cite: col1_save.h capital bit; fandom capital / Aztec treasure;
   * viceroy_unpacked.c ~101416–101466 (-0xcc doubles / boosts amount).
   * Mission byte (+5): convert-join owner + Jesuit bit0x10.
   */
  int rich_capital = 0;
  uint8_t mission = COL1_TRIBE_MISSION_NONE;
  if (col1->tribe) {
    for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
      if ((int)col1->tribe[i].x == tile_x && (int)col1->tribe[i].y == tile_y) {
        rich_capital = col1->tribe[i].state.capital ? 1 : 0;
        mission = col1->tribe[i].mission;
        break;
      }
    }
  }

  /*
   * Subjugated convert-join before tribe destroy (DOS order: convert then
   * treasure). Cite: FUN_5fef_1b0e ~101155–101184; @INDIANSLAVES 0x1cbf.
   */
  if (attacker_nation_id >= 0 && attacker_nation_id < 4 && rng) {
    const int thr =
      units_subjugated_convert_join_threshold(col1, attacker_nation_id, mission);
    if (thr >= 0) {
      const int roll = dos_rng_range(rng, 0, 12);
      if (roll < thr) {
        (void)units_spawn_subjugated_convert(units, tile_x, tile_y, attacker_nation_id);
      }
    }
  }

  const int tribe_nation = col1_destroy_tribe_at(col1, units, map, tile_x, tile_y);
  if (tribe_nation < 0) {
    return false;
  }

  if (attacker_nation_id >= 0 && attacker_nation_id < 4) {
    /*
     * col1_save.h ColonizeCol1Nation.villages_burned; reports.c scores
     * villages_penalty = -(difficulty+1)*villages_burned. Increment on
     * successful tribe destroy only.
     */
    if (col1->nation[attacker_nation_id].villages_burned < 255u) {
      col1->nation[attacker_nation_id].villages_burned++;
    }
    ai_diplo_indian_relation_delta(col1, tribe_nation, attacker_nation_id, -5);
    ai_diplo_indian_hostility_sync(col1, attacker_nation_id);
  }

  if (attacker_nation_id >= 0 && attacker_nation_id < 4 &&
      founding_fathers_cortes_guarantees_conquest_treasure(col1, attacker_nation_id)) {
    int gold = gold_amount;
    if (gold <= 0) {
      gold = units_cortes_conquest_treasure_gold(
        col1, attacker_nation_id, rng, rich_capital
      );
    }
    if (gold > 0) {
      (void)units_spawn_treasure_train(units, tile_x, tile_y, attacker_nation_id, gold);
    }
  }
  return true;
}

bool units_resolve_lcr_rumour(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng
) {
  /*
   * Thin FUN_65dd_0004 scaffold: Scout on procedural rumour tile clears it.
   * With de Soto (FF 7): always-positive branch = reveal radius (no invented
   * treasure / Fountain of Youth). Without de Soto: clear only; full RNG table
   * PARKED (negative outcomes omitted).
   */
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !map || !units_is_on_map(u)) {
    return false;
  }
  const ColonizeUnitType* t = units_type(pool, u->type_index);
  const bool is_scout =
    (t && strstr(t->name, "Scout") != NULL) || u->profession == UNITS_JOB_SCOUT;
  if (!is_scout) {
    return false;
  }
  if (!map_tile_has_rumour(map, u->x, u->y)) {
    return false;
  }
  if (!map_clear_rumour(map, u->x, u->y)) {
    return false;
  }
  if (col1 && u->nation_id >= 0 && u->nation_id < 4 &&
      founding_fathers_de_soto_lcr_always_positive(col1, u->nation_id)) {
    map_reveal_radius(map, u->x, u->y, u->nation_id, 1);
    (void)rng;
    return true;
  }
  /*
   * PARK: FUN_65dd_0004 full lost-city RNG table (treasure gold, FoY, hostile
   * natives, …) — rumour cleared; outcomes beyond de Soto reveal not ported.
   */
  (void)rng;
  return true;
}

bool units_resolve_land_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
) {
  /*
   * Use g_units_ff_col1 so AI/king/contact callers (ai_euro_try_attack,
   * units_resolve_land_combat) get Washington promote when
   * turn_refresh_moves_for_nation → units_set_ff_col1 has run.
   * Cite: PEDIA/wiki George Washington; docs/fandom_col1994.md; FF elect
   * comment in founding_fathers.c (FF_GEORGE_WASHINGTON).
   */
  return units_resolve_land_combat_ff(pool, attacker_id, defender_id, rng, g_units_ff_col1);
}

bool units_resolve_land_combat_ff(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
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
  /*
   * Colony fortification (Stockade +100% / Fort +150% / Fortress +200%) when
   * defender stands on own Euro colony tile. Cite: building_production.md;
   * fandom Stockade/Fort/Fortress. Stockade replaces Fortify benefit inside —
   * skip fortified ×2 when building bonus applies.
   */
  int fort_bonus = 0;
  if (g_units_combat_colonies) {
    const int cid = colonies_id_at(g_units_combat_colonies, def->x, def->y);
    if (cid >= 0) {
      const ColonizeColony* col = colonies_get(g_units_combat_colonies, cid);
      if (col && col->active && col->nation_id == def->nation_id && col->nation_id >= 0 &&
          col->nation_id <= 3) {
        fort_bonus = colonies_fortification_defense_bonus_percent(g_units_combat_colonies, col);
      }
    }
  }
  if (fort_bonus > 0) {
    defense = defense + (defense * fort_bonus) / 100;
  } else if (def->orders == UNITS_ORDER_FORTIFIED || def->orders == UNITS_ORDER_FORTIFY) {
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
    const int def_x = def->x;
    const int def_y = def->y;
    const int def_nation = def->nation_id;
    const int atk_nation = atk->nation_id;
    /*
     * Treasure capture: credit LE16 hold gold to Euro winner treasury, then
     * despawn. Cite: FUNCTION_CATALOG FUN_5fef_1908; GAME.TXT @LOOTCAPTURE —
     * amount from unit only (no invented ransom). PARK: ransom dialog chrome.
     */
    if (col1 && atk_nation >= 0 && atk_nation <= 3 && dt->name[0] &&
        strstr(dt->name, "Treasure") != NULL) {
      const unsigned lo = (unsigned)(def->hold_goods_amount[0] & 0xff);
      const unsigned hi = (unsigned)(def->hold_goods_amount[1] & 0xff);
      const int loot = (int)(lo | (hi << 8));
      if (loot > 0) {
        ColonizeCol1Save* mut = (ColonizeCol1Save*)col1;
        const uint32_t g = mut->nation[atk_nation].gold;
        const uint32_t add = (uint32_t)loot;
        mut->nation[atk_nation].gold = g > UINT32_MAX - add ? UINT32_MAX : g + add;
      }
    }
    units_despawn(pool, defender_id);
    atk = units_get(pool, attacker_id);
    if (atk) {
      units_washington_promote_on_win(pool, atk, col1);
    }
    if (def_nation >= 4 && g_units_fallout_col1 && g_units_fallout_map) {
      (void)units_try_native_settlement_fallout(
        g_units_fallout_col1,
        pool,
        g_units_fallout_map,
        atk_nation,
        def_nation,
        def_x,
        def_y,
        g_units_conquest_gold,
        rng
      );
    }
    g_units_last_combat = 1;
    return true;
  }
  units_despawn(pool, attacker_id);
  def = units_get(pool, defender_id);
  if (def) {
    units_washington_promote_on_win(pool, def, col1);
  }
  g_units_last_combat = -1;
  return false;
}

int units_plunder_ship_holds(ColonizeUnitPool* pool, int winner_id, int loser_id) {
  if (!pool || winner_id < 0 || loser_id < 0 || winner_id == loser_id) {
    return 0;
  }
  ColonizeUnit* win = units_get(pool, winner_id);
  ColonizeUnit* lose = units_get(pool, loser_id);
  if (!win || !lose || !win->active || !lose->active) {
    return 0;
  }
  if (!units_is_sea(pool, winner_id) || !units_is_sea(pool, loser_id)) {
    return 0;
  }
  /*
   * FUN_5fef_016c-shaped: move commodity holds from loser into winner capacity.
   * Passengers stay with the sinking ship (despawned with loser).
   */
  const int n = units_goods_hold_count(pool, loser_id);
  int moved = 0;
  for (int i = 0; i < n; ++i) {
    const int amt = lose->hold_goods_amount[i];
    const int ctype = lose->hold_goods_type[i];
    if (amt <= 0 || amt >= 255 || ctype < 0 || ctype >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    const int got = units_load_goods(pool, winner_id, ctype, amt);
    if (got > 0) {
      moved += got;
      if (got >= amt) {
        lose->hold_goods_amount[i] = 0;
        lose->hold_goods_type[i] = 0;
      } else {
        lose->hold_goods_amount[i] = amt - got;
      }
    }
  }
  return moved;
}

bool units_resolve_naval_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
) {
  /*
   * Use g_units_ff_col1 so AI/king callers (ai_euro / ai_king naval attack)
   * get Drake privateer *3/2 when turn_refresh_moves_for_nation →
   * units_set_ff_col1 has run. Cite: PEDIA/wiki Francis Drake (+50%);
   * founding_fathers.c FF_FRANCIS_DRAKE (*3/2).
   */
  return units_resolve_naval_combat_ff(pool, attacker_id, defender_id, rng, g_units_ff_col1);
}

bool units_resolve_naval_combat_ff(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
) {
  g_units_last_combat = 0;
  ColonizeUnit* atk = units_get(pool, attacker_id);
  ColonizeUnit* def = units_get(pool, defender_id);
  if (!atk || !def || !atk->active || !def->active) {
    return false;
  }
  if (!units_is_sea(pool, attacker_id) || !units_is_sea(pool, defender_id)) {
    return false;
  }
  const ColonizeUnitType* at = units_type(pool, atk->type_index);
  const ColonizeUnitType* dt = units_type(pool, def->type_index);
  if (!at || !dt) {
    return false;
  }
  int attack = at->attack;
  int defense = dt->defense;
  if (attack < 0) {
    attack = 0;
  }
  if (defense < 0) {
    defense = 0;
  }
  /*
   * FUN_157e_004a peels (naval): Privateer + ship_damaged (0x3148 bit7) → −2;
   * holds_occupied (0x3150 / Col1 unit+0x0c) subtracted for both sides.
   * Cite: viceroy_unpacked.c FUN_157e_004a; col1_save.h ship_damaged.
   */
  if (strstr(at->name, "Privateer") != NULL && (atk->col1_unknown15 & 0x80u) != 0) {
    attack -= 2;
    if (attack < 0) {
      attack = 0;
    }
  }
  if (strstr(dt->name, "Privateer") != NULL && (def->col1_unknown15 & 0x80u) != 0) {
    defense -= 2;
    if (defense < 0) {
      defense = 0;
    }
  }
  {
    int atk_holds = 0;
    int def_holds = 0;
    for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      if (atk->hold_goods_amount[i] > 0 && atk->hold_goods_amount[i] < 255) {
        ++atk_holds;
      }
      if (def->hold_goods_amount[i] > 0 && def->hold_goods_amount[i] < 255) {
        ++def_holds;
      }
    }
    attack -= atk_holds;
    defense -= def_holds;
    if (attack < 0) {
      attack = 0;
    }
    if (defense < 0) {
      defense = 0;
    }
  }
  attack = units_drake_scale_strength(pool, atk, attack, col1);
  defense = units_drake_scale_strength(pool, def, defense, col1);
  const int total = attack + defense;
  bool atk_wins = false;
  if (total <= 0) {
    atk_wins = true;
  } else if (!rng) {
    atk_wins = attack >= defense;
  } else {
    const int roll = dos_rng_range(rng, 1, total);
    atk_wins = roll <= attack;
  }
  if (atk_wins) {
    (void)units_plunder_ship_holds(pool, attacker_id, defender_id);
    units_despawn(pool, defender_id);
    g_units_last_combat = 1;
    return true;
  }
  (void)units_plunder_ship_holds(pool, defender_id, attacker_id);
  units_despawn(pool, attacker_id);
  g_units_last_combat = -1;
  return false;
}

int units_coastal_fort_attack_strength(
  const ColonizeColonyPool* colonies,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units
) {
  if (!colonies || !colony || !colony->active || !units) {
    return 0;
  }
  int tier = 0;
  const int fortress = colonies_find_building(colonies, "Fortress");
  if (fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fortress]) {
    tier = 2;
  } else {
    const int fort = colonies_find_building(colonies, "Fort");
    if (fort >= 0 && fort < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fort]) {
      tier = 1;
    }
  }
  if (tier <= 0) {
    return 0;
  }
  int arty = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || !units_is_on_map(u) || u->x != colony->x || u->y != colony->y) {
      continue;
    }
    if (u->nation_id != colony->nation_id) {
      continue;
    }
    const ColonizeUnitType* t = units_type(units, u->type_index);
    if (!t) {
      continue;
    }
    if (strstr(t->name, "Artillery") != NULL || strstr(t->name, "Cannon") != NULL) {
      arty++;
    }
  }
  /* FUN_364b_03f6: local_12 starts at 1, +1 per artillery → (1+arty)*tier*4. */
  return 4 * tier * (1 + arty);
}

static int units_fort_fire_is_hostile(
  const ColonizeCol1Save* col1,
  int owner_nation,
  const ColonizeUnit* ship,
  const ColonizeUnitType* st
) {
  if (!ship || ship->nation_id == owner_nation) {
    return 0;
  }
  if (st && strstr(st->name, "Privateer") != NULL) {
    return 1;
  }
  if (!col1 || owner_nation < 0 || owner_nation > 3) {
    return 0;
  }
  if (ship->nation_id >= 0 && ship->nation_id <= 3) {
    return ai_diplo_at_war(col1, owner_nation, ship->nation_id);
  }
  if (ship->nation_id >= 4 && ship->nation_id <= 11) {
    return ai_diplo_indian_at_war(col1, owner_nation, ship->nation_id - 4);
  }
  return 0;
}

/*
 * Fort battery vs one ship: attack strength vs ship defense (Drake scales
 * Privateer defense). Winner sink only — no temp attacker to despawn/plunder.
 */
static bool units_fort_vs_ship(
  ColonizeUnitPool* pool,
  int attack_str,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
) {
  ColonizeUnit* def = units_get(pool, defender_id);
  if (!def || !def->active || !units_is_sea(pool, defender_id) || attack_str <= 0) {
    return false;
  }
  const ColonizeUnitType* dt = units_type(pool, def->type_index);
  if (!dt) {
    return false;
  }
  int defense = dt->defense;
  if (defense < 0) {
    defense = 0;
  }
  defense = units_drake_scale_strength(pool, def, defense, col1);
  const int total = attack_str + defense;
  bool atk_wins = false;
  if (total <= 0) {
    atk_wins = true;
  } else if (!rng) {
    atk_wins = attack_str >= defense;
  } else {
    const int roll = dos_rng_range(rng, 1, total);
    atk_wins = roll <= attack_str;
  }
  if (atk_wins) {
    units_despawn(pool, defender_id);
    return true;
  }
  return false;
}

int units_coastal_fort_fire_pulse(
  ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng
) {
  if (!units || !colonies || !map) {
    return 0;
  }
  static const int k_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int k_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int sunk = 0;
  for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
    const ColonizeColony* col = &colonies->colonies[ci];
    if (!col->active) {
      continue;
    }
    const int atk = units_coastal_fort_attack_strength(colonies, col, units);
    if (atk <= 0) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = col->x + k_dx[d];
      const int ny = col->y + k_dy[d];
      if (!map_tile_is_water(map, nx, ny)) {
        continue;
      }
      /* Snapshot ids: combat may despawn mid-scan. */
      int targets[COLONIZE_UNITS_MAX];
      int n_tg = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units->units[i];
        if (!u->active || !units_is_on_map(u) || u->x != nx || u->y != ny) {
          continue;
        }
        if (!units_is_sea(units, u->id)) {
          continue;
        }
        const ColonizeUnitType* st = units_type(units, u->type_index);
        if (!units_fort_fire_is_hostile(col1, col->nation_id, u, st)) {
          continue;
        }
        targets[n_tg++] = u->id;
      }
      for (int t = 0; t < n_tg; ++t) {
        if (units_fort_vs_ship(units, atk, targets[t], rng, col1)) {
          sunk++;
        }
      }
    }
  }
  return sunk;
}

/*
 * DOS DS:0x5236 combat role stand-in: attack > 0 or carried muskets/horses.
 * Non-combat movers bounce off foreign stacks instead of fighting.
 */
static bool units_is_combat_role(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!pool || !u) {
    return false;
  }
  if (u->muskets > 0 || u->horses > 0) {
    return true;
  }
  const ColonizeUnitType* t = units_type(pool, u->type_index);
  return t && t->attack > 0;
}

static bool units_is_wagon_type(const ColonizeUnitPool* pool, int type_index) {
  const ColonizeUnitType* t = units_type(pool, type_index);
  return t && t->name && strstr(t->name, "Wagon") != NULL;
}

static bool units_at_war_for_move(int a, int b) {
  if (a < 0 || b < 0 || a == b) {
    return false;
  }
  /* Natives vs Euro: always fightable when combat role. */
  if (a >= 4 || b >= 4) {
    return true;
  }
  if (!g_units_ff_col1) {
    return true; /* tests / no diplo: allow combat */
  }
  return ai_diplo_at_war(g_units_ff_col1, a, b);
}

static bool units_village_squat_illegal(
  const ColonizeUnitPool* pool,
  const ColonizeUnitType* type,
  const ColonizeUnit* mover,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int mover_nation,
  const ColonizeColonyPool* colonies
) {
  if (!pool || !type || !map || !map->layer2 || mover_nation < 0 || mover_nation >= 4) {
    return false;
  }
  const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
  if (idx >= (size_t)map->width * (size_t)map->height) {
    return false;
  }
  if ((map->layer2[idx] & MAP_OCCUPANCY_HAS_CITY) == 0) {
    return false;
  }
  const int cid = colonies ? colonies_id_at(colonies, x, y) : -1;
  if (cid >= 0) {
    return false;
  }
  const char* n = type->name;
  const int missionary = n && strstr(n, "Missionary") != NULL;
  const int combatish =
    (mover && (mover->muskets > 0 || mover->horses > 0)) ||
    (n &&
     (strstr(n, "Soldier") != NULL || strstr(n, "Scout") != NULL || strstr(n, "Dragoon") != NULL ||
      strstr(n, "Regular") != NULL || strstr(n, "Army") != NULL || strstr(n, "Cavalry") != NULL ||
      strstr(n, "Artillery") != NULL)) ||
    (type->attack > 0);
  return !missionary && !combatish;
}

static void units_try_capture_foreign_colony(
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  int unit_id
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !colonies || units_is_sea(pool, unit_id)) {
    return;
  }
  const int cid = colonies_id_at(colonies, u->x, u->y);
  ColonizeColony* col = colonies_get_mut(colonies, cid);
  if (!col || !col->active) {
    return;
  }
  if (col->nation_id < 0 || col->nation_id > 3 || col->nation_id == u->nation_id) {
    return;
  }
  /* Still contested if a foreign unit remains on the tile. */
  if (units_foreign_at(pool, u->x, u->y, unit_id, u->nation_id) >= 0) {
    return;
  }
  (void)colonies_capture(colonies, cid, u->nation_id);
}

ColonizeEnterReason units_enter_probe(
  const ColonizeUnitPool* pool,
  int type_index,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int mover_id,
  const ColonizeColonyPool* colonies
) {
  g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
  if (!pool || type_index < 0 || type_index >= pool->type_count || !map) {
    return g_units_last_enter_reason;
  }
  if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
    g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_EDGE;
    return g_units_last_enter_reason;
  }

  int mover_nation = -1;
  const ColonizeUnit* mover = (mover_id >= 0) ? units_get_const(pool, mover_id) : NULL;
  if (mover) {
    mover_nation = mover->nation_id;
  }

  const ColonizeUnitType* type = &pool->types[type_index];
  const bool sea = type->domain == COLONIZE_UNIT_DOMAIN_SEA;
  const bool water = map_tile_is_water(map, x, y);
  const bool land = map_tile_is_land(map, x, y);

  const int foe = units_foreign_at(pool, x, y, mover_id, mover_nation);
  if (foe >= 0) {
    const bool foe_sea = units_is_sea(pool, foe);
    if (sea && foe_sea) {
      const ColonizeUnit* fu = units_get_const(pool, foe);
      const int foe_nation = fu ? fu->nation_id : -1;
      if (mover && !units_at_war_for_move(mover_nation, foe_nation)) {
        g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_PEACE;
      } else {
        /* Ships fight on contact (attack may be 0 in @UNIT for transports). */
        g_units_last_enter_reason = COLONIZE_ENTER_COMBAT_NAVAL;
      }
      return g_units_last_enter_reason;
    }
    if (sea != foe_sea) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
      return g_units_last_enter_reason;
    }
    /* Land × land foreign. */
    if (!mover) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
      return g_units_last_enter_reason;
    }
    const ColonizeUnit* fu = units_get_const(pool, foe);
    const int foe_nation = fu ? fu->nation_id : -1;
    if (!units_is_combat_role(pool, mover)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_FOREIGN;
      return g_units_last_enter_reason;
    }
    if (!units_at_war_for_move(mover_nation, foe_nation)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_PEACE;
      return g_units_last_enter_reason;
    }
    g_units_last_enter_reason = COLONIZE_ENTER_COMBAT_LAND;
    return g_units_last_enter_reason;
  }

  if (sea) {
    /*
     * 4720 reason 5: eastward high-seas step without sail/goto intent.
     * Cite: FUN_4720_015c / docs/move_enter.md.
     */
    if (mover && map_tile_is_high_seas(map, x, y) && x > mover->x &&
        !units_orders_follow_goto(mover->orders)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_HS_SAIL;
      return g_units_last_enter_reason;
    }
    if (water) {
      g_units_last_enter_reason = COLONIZE_ENTER_OK;
      return g_units_last_enter_reason;
    }
    if (land && colonies && mover_nation >= 0) {
      const int cid = colonies_id_at(colonies, x, y);
      const ColonizeColony* col = colonies_get(colonies, cid);
      if (col && col->active && col->nation_id == mover_nation) {
        g_units_last_enter_reason = COLONIZE_ENTER_DOCK;
        return g_units_last_enter_reason;
      }
      if (col && col->active && col->nation_id >= 0 && col->nation_id <= 3 && g_units_ff_col1 &&
          founding_fathers_de_witt_allows_foreign_colony_trade(g_units_ff_col1, mover_nation) &&
          !ai_diplo_at_war(g_units_ff_col1, mover_nation, col->nation_id)) {
        g_units_last_enter_reason = COLONIZE_ENTER_DOCK;
        return g_units_last_enter_reason;
      }
      if (col && col->active) {
        g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
        return g_units_last_enter_reason;
      }
    }
    if (land) {
      g_units_last_enter_reason = COLONIZE_ENTER_LANDFALL;
      return g_units_last_enter_reason;
    }
    g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
    return g_units_last_enter_reason;
  }

  if (!land) {
    g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
    return g_units_last_enter_reason;
  }
  if (units_village_squat_illegal(pool, type, mover, map, x, y, mover_nation, colonies)) {
    g_units_last_enter_reason = COLONIZE_ENTER_VILLAGE_ILLEGAL;
    return g_units_last_enter_reason;
  }
  g_units_last_enter_reason = COLONIZE_ENTER_OK;
  return g_units_last_enter_reason;
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
  const ColonizeEnterReason r =
    units_enter_probe(pool, type_index, map, x, y, mover_id, colonies);
  return r == COLONIZE_ENTER_OK || r == COLONIZE_ENTER_DOCK;
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
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    return map_move_cost_at(map, dest_x, dest_y);
  }
  return map_move_cost_step(map, u->x, u->y, dest_x, dest_y);
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

/* Standing military defender on a colony tile (PEDIA Revere "standing soldiers"). */
static bool units_is_standing_soldier(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!pool || !u || !u->active) {
    return false;
  }
  if (u->muskets > 0) {
    return true;
  }
  const ColonizeUnitType* t = units_type(pool, u->type_index);
  const char* n = t ? t->name : NULL;
  const char* d = units_display_name(pool, u);
  if ((n && (strstr(n, "Soldier") || strstr(n, "Dragoon") || strstr(n, "Cavalry") ||
             strstr(n, "Artillery") || strstr(n, "Regular") || strstr(n, "Continental") ||
             strstr(n, "Cont."))) ||
      (d && (strstr(d, "Soldier") || strstr(d, "Dragoon") || strstr(d, "Cavalry") ||
             strstr(d, "Artillery") || strstr(d, "Regular") || strstr(d, "Continental")))) {
    return true;
  }
  return false;
}

static bool units_colony_has_soldier_on_tile(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int colony_nation
) {
  if (!pool || colony_nation < 0) {
    return false;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || !units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->nation_id != colony_nation) {
      continue;
    }
    if (units_is_standing_soldier(pool, u)) {
      return true;
    }
  }
  return false;
}

/*
 * PEDIA Paul Revere: when stepping onto a foreign colony with no map unit and
 * no standing soldiers, auto-arm a colonist from warehouse muskets and fight.
 * Returns true if move may continue (no fight, or attacker won). False if
 * attacker lost / despawned. Requires g_units_ff_col1.
 */
static bool units_revere_defend_colony_tile(
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  int attacker_id,
  int dest_x,
  int dest_y,
  ColonizeDosRng* rng
) {
  if (!pool || !colonies || !g_units_ff_col1) {
    return true;
  }
  ColonizeUnit* atk = units_get(pool, attacker_id);
  if (!atk || !atk->active || units_is_sea(pool, attacker_id)) {
    return true;
  }
  const int cid = colonies_id_at(colonies, dest_x, dest_y);
  ColonizeColony* col = colonies_get_mut(colonies, cid);
  if (!col || !col->active) {
    return true;
  }
  if (col->nation_id < 0 || col->nation_id > 3 || col->nation_id == atk->nation_id) {
    return true;
  }
  const bool has_soldier =
    units_colony_has_soldier_on_tile(pool, dest_x, dest_y, col->nation_id);
  if (!founding_fathers_revere_should_auto_arm(
        g_units_ff_col1, col->nation_id, has_soldier, col->stock[COLONIZE_CARGO_MUSKETS]
      )) {
    return true;
  }
  const int def_id = founding_fathers_revere_auto_arm(colonies, pool, cid);
  if (def_id < 0) {
    return true; /* eject failed — leave tile open (no invented defense) */
  }
  if (!units_resolve_land_combat_ff(pool, attacker_id, def_id, rng, g_units_ff_col1)) {
    return false;
  }
  return units_get(pool, attacker_id) != NULL;
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
  g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
  ColonizeUnit* unit = units_get(pool, unit_id);
  if (!unit || !map) {
    return false;
  }
  if (unit->aboard_ship_id >= 0) {
    return false;
  }
  if (unit->moves_left <= 0) {
    g_units_last_enter_reason = COLONIZE_ENTER_NO_MP;
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

  /* Fortification defense uses defender's colony tile (set before combat). */
  units_set_combat_colonies(colonies);

  const ColonizeEnterReason reason =
    units_enter_probe(pool, unit->type_index, map, dest_x, dest_y, unit_id, colonies);
  g_units_last_enter_reason = reason;

  if (reason == COLONIZE_ENTER_BOUNCE_FOREIGN || reason == COLONIZE_ENTER_BOUNCE_PEACE ||
      reason == COLONIZE_ENTER_BLOCKED_DOMAIN || reason == COLONIZE_ENTER_BLOCKED_EDGE ||
      reason == COLONIZE_ENTER_BLOCKED_HS_SAIL || reason == COLONIZE_ENTER_VILLAGE_ILLEGAL ||
      reason == COLONIZE_ENTER_LANDFALL || reason == COLONIZE_ENTER_NO_MP ||
      reason == COLONIZE_ENTER_BLOCKED) {
    return false;
  }

  if (reason == COLONIZE_ENTER_COMBAT_LAND || reason == COLONIZE_ENTER_COMBAT_NAVAL) {
    const int foe = units_foreign_at(pool, dest_x, dest_y, unit_id, unit->nation_id);
    if (foe < 0) {
      return false;
    }
    bool won = false;
    if (reason == COLONIZE_ENTER_COMBAT_NAVAL) {
      won = units_resolve_naval_combat_ff(pool, unit_id, foe, rng, g_units_ff_col1);
    } else {
      won = units_resolve_land_combat_ff(pool, unit_id, foe, rng, g_units_ff_col1);
    }
    if (!won) {
      return false;
    }
    unit = units_get(pool, unit_id);
    if (!unit) {
      return false;
    }
    /* After win, dest must be clear of foreigners for enter. */
    if (units_foreign_at(pool, dest_x, dest_y, unit_id, unit->nation_id) >= 0) {
      return false;
    }
  } else if (colonies) {
    /* Paul Revere: empty foreign colony tile → auto-arm from muskets + fight.
     * Cite: PEDIA / docs/fandom_col1994.md Paul Revere. */
    if (!units_revere_defend_colony_tile(
          pool, (ColonizeColonyPool*)colonies, unit_id, dest_x, dest_y, rng
        )) {
      return false;
    }
    unit = units_get(pool, unit_id);
    if (!unit) {
      return false;
    }
  }

  /* Dock / OK: domain enterability already confirmed by probe. */
  if (reason != COLONIZE_ENTER_OK && reason != COLONIZE_ENTER_DOCK &&
      reason != COLONIZE_ENTER_COMBAT_LAND && reason != COLONIZE_ENTER_COMBAT_NAVAL) {
    return false;
  }
  if (!units_can_enter(pool, unit->type_index, map, dest_x, dest_y, unit_id, colonies)) {
    /* Combat cleared foe — re-probe should be OK/DOCK now. */
    const ColonizeEnterReason after =
      units_enter_probe(pool, unit->type_index, map, dest_x, dest_y, unit_id, colonies);
    g_units_last_enter_reason = after;
    if (after != COLONIZE_ENTER_OK && after != COLONIZE_ENTER_DOCK) {
      return false;
    }
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

  const int ox = unit->x;
  const int oy = unit->y;
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
  units_occupancy_refresh_tile(pool, ox, oy, unit_id);
  units_occupancy_refresh_tile(pool, dest_x, dest_y, -1);

  /* Wagon / ship-stack on Euro settlement: DOS 465b:08f8 exhaust MP. */
  if (colonies && colonies_id_at(colonies, dest_x, dest_y) >= 0 &&
      units_is_wagon_type(pool, unit->type_index)) {
    unit->moves_left = 0;
  }

  if (colonies) {
    units_try_capture_foreign_colony(pool, (ColonizeColonyPool*)colonies, unit_id);
  }

  g_units_last_enter_reason =
    (reason == COLONIZE_ENTER_DOCK) ? COLONIZE_ENTER_DOCK : COLONIZE_ENTER_OK;
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
  u->follow_unit_id = -1;
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
  u->follow_unit_id = -1;
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

bool units_order_anchor(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeColonyPool* colonies
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || !units_is_sea(pool, unit_id)) {
    return false;
  }
  if (!colonies) {
    return false;
  }
  /* Harbor: own Euro colony on this tile, or adjacent (ship in port approaches). */
  bool in_harbor = false;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies->colonies[i];
    if (!c->active || c->nation_id != u->nation_id) {
      continue;
    }
    const int dx = c->x - u->x;
    const int dy = c->y - u->y;
    if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) {
      in_harbor = true;
      break;
    }
  }
  if (!in_harbor) {
    return false;
  }
  if (u->orders == UNITS_ORDER_FORTIFIED) {
    return true;
  }
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
  u->follow_unit_id = -1;
  u->orders = UNITS_ORDER_FORTIFY;
  u->moves_left = 0;
  return true;
}

bool units_order_sentry(ColonizeUnitPool* pool, int unit_id) {
  return units_set_orders(pool, unit_id, UNITS_ORDER_SENTRY);
}

bool units_order_trade_route(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u)) {
    return false;
  }
  /* Wagons and ships run trade routes; refuse pure foot units without holds. */
  if (!units_is_transport(pool, unit_id)) {
    return false;
  }
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
  u->follow_unit_id = -1;
  u->orders = UNITS_ORDER_TRADE_ROUTE;
  u->moves_left = 0;
  return true;
}

int units_dump_cargo_overboard(
  ColonizeUnitPool* pool,
  int unit_id,
  int* out_cargo_type,
  int* out_amount
) {
  if (!units_is_transport(pool, unit_id)) {
    return 0;
  }
  const int hold = units_first_goods_hold(pool, unit_id);
  if (hold < 0) {
    return 0;
  }
  return units_unload_goods_hold(pool, unit_id, hold, out_cargo_type, out_amount);
}

bool units_pillage(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  ColonizeColonyPool* colonies,
  char* err,
  size_t err_size
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || !map) {
    if (err && err_size) {
      snprintf(err, err_size, "Select a unit");
    }
    return false;
  }
  if (units_is_sea(pool, unit_id)) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot pillage at sea");
    }
    return false;
  }
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  if (!type || type->attack <= 0) {
    if (err && err_size) {
      snprintf(err, err_size, "Need a military unit");
    }
    return false;
  }
  if (u->moves_left <= 0) {
    if (err && err_size) {
      snprintf(err, err_size, "No moves left");
    }
    return false;
  }

  const int cid = colonies ? colonies_id_at(colonies, u->x, u->y) : -1;
  ColonizeColony* col = (cid >= 0) ? colonies_get_mut(colonies, cid) : NULL;
  if (col && col->nation_id != u->nation_id && col->nation_id >= 0 && col->nation_id < 4) {
    /* Loot richest non-food warehouse cargo (thin ORDERS Pillage). */
    int best = -1;
    int best_amt = 0;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      if (c == COLONIZE_CARGO_FOOD) {
        continue;
      }
      if (col->stock[c] > best_amt) {
        best_amt = col->stock[c];
        best = c;
      }
    }
    if (best < 0 || best_amt <= 0) {
      if (err && err_size) {
        snprintf(err, err_size, "Nothing to pillage");
      }
      return false;
    }
    const int take = best_amt < 100 ? best_amt : 100;
    col->stock[best] -= take;
    u->moves_left = 0;
    if (err && err_size) {
      snprintf(err, err_size, "Pillaged %d cargo", take);
    }
    return true;
  }

  /* Non-colony: clear plow / road improvements on the tile. */
  const bool had_plow = map_tile_is_plowed(map, u->x, u->y);
  const bool had_road = map_tile_has_road(map, u->x, u->y);
  if (!had_plow && !had_road) {
    if (err && err_size) {
      snprintf(err, err_size, "Nothing to pillage");
    }
    return false;
  }
  if (had_plow) {
    map_tile_set_plowed(map, u->x, u->y, false);
  }
  if (had_road) {
    map_tile_set_road(map, u->x, u->y, false);
  }
  u->moves_left = 0;
  if (err && err_size) {
    snprintf(err, err_size, "Pillaged improvements");
  }
  return true;
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
  u->follow_unit_id = -1;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = dest_x;
  u->goto_y = dest_y;
  return true;
}

bool units_follow_unit(ColonizeUnitPool* pool, int unit_id, int target_unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  const ColonizeUnit* t = units_get_const(pool, target_unit_id);
  if (!u || !t || !u->active || !t->active) {
    return false;
  }
  if (unit_id == target_unit_id) {
    return false;
  }
  if (!units_is_on_map(u) || !units_is_on_map(t)) {
    return false;
  }
  /* Sea follows sea; land follows land — mixed escort is not a map path. */
  if (units_is_sea(pool, unit_id) != units_is_sea(pool, target_unit_id)) {
    return false;
  }
  u->orders = UNITS_ORDER_FOLLOW;
  u->follow_unit_id = target_unit_id;
  u->goto_x = t->x;
  u->goto_y = t->y;
  return true;
}

bool units_advance_follow_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || u->orders != UNITS_ORDER_FOLLOW) {
    return false;
  }
  const ColonizeUnit* t = units_get_const(pool, u->follow_unit_id);
  if (!t || !t->active || !units_is_on_map(t)) {
    units_clear_orders(pool, unit_id);
    return false;
  }
  /* Already adjacent or stacked — hold FOLLOW, no MP spend. */
  if (u->x == t->x && u->y == t->y) {
    return true;
  }
  const int dx = abs(u->x - t->x);
  const int dy = abs(u->y - t->y);
  if (dx <= 1 && dy <= 1) {
    return true;
  }
  /* Retarget tile goto toward target, one step, restore FOLLOW order. */
  const int tid = u->follow_unit_id;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = t->x;
  u->goto_y = t->y;
  const bool stepped = units_advance_goto_one_step(pool, unit_id, map, colonies, rng);
  u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  /* Re-arm FOLLOW unless the unit was cleared (arrived / blocked clears goto). */
  u->orders = UNITS_ORDER_FOLLOW;
  u->follow_unit_id = tid;
  u->goto_x = t->x;
  u->goto_y = t->y;
  return stepped;
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
  if (!units_orders_follow_goto(u->orders)) {
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
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || !map) {
    return false;
  }
  if (!units_orders_follow_goto(u->orders)) {
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
  if (!units_try_move(pool, unit_id, map, nx, ny, colonies, rng)) {
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
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
) {
  bool moved = false;
  while (units_advance_goto_one_step(pool, unit_id, map, colonies, rng)) {
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
    if (!u->active || !units_orders_follow_goto(u->orders) || !units_is_on_map(u)) {
      continue;
    }
    if (units_advance_goto_one_step(pool, u->id, map, colonies, NULL)) {
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
    if (!u->active || !units_orders_follow_goto(u->orders) || !units_is_on_map(u)) {
      continue;
    }
    if (units_advance_goto(pool, u->id, map, colonies, NULL)) {
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
  int best_score = -0x7fffffff;
  for (int d = 0; d < 8; ++d) {
    const int nx = ship->x + k_dx[d];
    const int ny = ship->y + k_dy[d];
    if (!map_tile_is_land(map, nx, ny) || map_tile_is_water(map, nx, ny)) {
      continue;
    }
    if (!units_can_enter(pool, pax_type, map, nx, ny, pax_id, colonies)) {
      continue;
    }
    /* Settle landfall: skip arctic / occupied (colonies_can_found). */
    if (colonies && !colonies_can_found(colonies, map, nx, ny)) {
      continue;
    }
    int score = 10;
    if (have_prefer) {
      const int dx = nx - prefer_x;
      const int dy = ny - prefer_y;
      score -= (dx * dx + dy * dy);
    }
    if (best_x < 0 || score > best_score) {
      best_x = nx;
      best_y = ny;
      best_score = score;
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
  /* units_slot reuses inactive rows — clear like units_spawn_allow_stack.
   * home_tribe_id must be -1 so Col1 origin exports as 0xff (DOS cargo UI);
   * leftover 0 looks like tribe[0] and breaks passenger treatment. */
  const ColonizeUnitType* type = &pool->types[type_index];
  slot->id = pool->next_id++;
  slot->type_index = type_index;
  slot->x = ship->x;
  slot->y = ship->y;
  slot->moves_left = 0;
  slot->active = true;
  slot->nation_id = 0;
  slot->col1_vis_mask = 0;
  units_set_nation(slot, ship->nation_id);
  slot->aboard_ship_id = ship->id;
  slot->cargo_count = 0;
  memset(slot->cargo_ids, 0, sizeof(slot->cargo_ids));
  memset(slot->hold_goods_type, 0, sizeof(slot->hold_goods_type));
  memset(slot->hold_goods_amount, 0, sizeof(slot->hold_goods_amount));
  slot->orders = 1; /* sentry aboard */
  slot->goto_x = 0xFF;
  slot->goto_y = 0xFF;
  slot->follow_unit_id = -1;
  slot->profession = UNITS_JOB_NONE;
  slot->tools = 0;
  slot->muskets = 0;
  slot->horses = 0;
  slot->home_tribe_id = -1;
  slot->turns_worked = 0;
  slot->last_dir = 0;
  slot->col1_unknown15 = 0;
  slot->col1_ai_plan = COL1_UNIT_UNKNOWN16_HI_DEFAULT;
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

/*
 * Match DOS COLONY00 starters: French→Hardy Pioneer (prof 20); Discoverer/Explorer
 * English get Veteran Soldier (21) but plain pioneer (28). Spanish→Veteran Soldier.
 * (Old: hardy=easy||French wrongly set English Discoverer pioneer to 20.)
 */
static void units_starter_skills(int nation_id, int difficulty, int* pioneer_job, int* soldier_job) {
  const bool easy = difficulty <= 1;
  const bool hardy = nation_id == 1;
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
  units_set_nation(ship, nation_id);
  ship->profession = 0; /* FUN_1427_06b4 transport profession */
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
    units_set_nation(pax, nation_id);
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
  /*
   * Human starts with ship selected and idle. Clear GOTO orders but pin goto to
   * the ship's tile (COLONY00). Keeping landfall in goto with orders=0 made DOS
   * treat the caravel as unloaded / peel transport_chain on select/move.
   */
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (ship) {
    ship->orders = 0;
    ship->goto_x = ship->x;
    ship->goto_y = ship->y;
    for (int c = 0; c < ship->cargo_count; ++c) {
      ColonizeUnit* pax = units_get(pool, ship->cargo_ids[c]);
      if (pax) {
        pax->goto_x = ship->x;
        pax->goto_y = ship->y;
      }
    }
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
