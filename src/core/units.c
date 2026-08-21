#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "platform/diagnostics.h"

/* Defined later; used by naval hold plunder before combat despawn. */
int units_load_goods(ColonizeUnitPool* pool, int unit_id, int cargo_type, int amount);
int units_plunder_ship_holds(ColonizeUnitPool* pool, int winner_id, int loser_id);
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

int units_tick_ship_build_ready(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int human_nation,
  char* status,
  size_t status_size,
  int* want_europe_open
) {
  if (!pool || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  if (want_europe_open) {
    *want_europe_open = 0;
  }
  int completed = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    /* DOS: type > 0x0c && type < 0x13 && type != 0x0b (redundant). */
    if (u->type_index <= 0x0c || u->type_index >= 0x13) {
      continue;
    }
    if ((u->col1_unknown15 & 0x80u) == 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(pool, u->type_index);
    /*
     * DOS type*0xe+0x5235 = NAMES @UNIT combat (Linux defense). Loader writes
     * attack→5236 then combat→5235. Cite: viceroy ~121115; nation_eot_ship_spawn.md.
     */
    int threshold = ty && ty->defense > 0 ? ty->defense : 4;
    /*
     * Past construction threshold with bit7 still set = combat damage (fort/naval).
     * Leave for units_tick_drydock_repair; do not auto-clear.
     */
    if (u->turns_worked >= threshold) {
      continue;
    }
    if (u->turns_worked < 255) {
      u->turns_worked++;
    }
    int on_colony = 0;
    if (colonies && colonies_id_at(colonies, u->x, u->y) >= 0) {
      on_colony = 1;
      if (u->turns_worked < 255) {
        u->turns_worked++;
      }
    }
    if (u->turns_worked < threshold) {
      continue;
    }
    u->col1_unknown15 = (uint8_t)(u->col1_unknown15 & 0x7fu);
    completed++;
    if (nation_id == human_nation && status && status_size > 0) {
      const char* name = (ty && ty->name[0]) ? ty->name : "Ship";
      snprintf(status, status_size, "%s construction complete.", name);
    }
    if (!on_colony && want_europe_open) {
      *want_europe_open = 1;
    }
  }
  return completed;
}

/*
 * Drydock ship repair: clear combat-damage bit7 for nation ships on an own
 * colony that has Drydock. Construction ships (turns_worked < defense thresh)
 * stay on units_tick_ship_build_ready. Cite: building_production.md Drydock;
 * combat.md coastal fort bit7.
 */
int units_tick_drydock_repair(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int human_nation,
  char* status,
  size_t status_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!pool || !colonies || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  const int drydock = colonies_find_building(colonies, "Drydock");
  if (drydock < 0 || drydock >= COLONIZE_BUILDING_TYPES_MAX) {
    return 0;
  }
  int repaired = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(pool, u->id) || (u->col1_unknown15 & 0x80u) == 0) {
      continue;
    }
    const ColonizeUnitType* ty = units_type(pool, u->type_index);
    const int threshold = ty && ty->defense > 0 ? ty->defense : 4;
    /* Still under construction — ship-build tick owns bit7. */
    if (u->turns_worked < threshold) {
      continue;
    }
    const int cid = colonies_id_at(colonies, u->x, u->y);
    const ColonizeColony* col = colonies_get(colonies, cid);
    if (!col || !col->active || col->nation_id != nation_id || !col->has_building[drydock]) {
      continue;
    }
    u->col1_unknown15 = (uint8_t)(u->col1_unknown15 & 0x7fu);
    repaired++;
    if (nation_id == human_nation) {
      const char* ship_name = (ty && ty->name[0]) ? ty->name : "Ship";
      const char* col_name = (col->name[0]) ? col->name : "colony";
      if (status && status_size > 0) {
        snprintf(status, status_size, "%s repaired at Drydock.", ship_name);
      }
      if (ai_popups) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = ship_name;
        tok.string1 = col_name;
        popup_msg_fill(
          messages,
          "REFIT",
          &tok,
          status && status[0] ? status : "Ship repaired.",
          body,
          sizeof(body)
        );
        ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
    }
  }
  return repaired;
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
  if (unit) {
    /* Trust carried gear fields. Do not invent 100 tools from the type name —
     * that hid FUN_479b_0158 wear (depleted pioneers looked fully stocked). */
    tools = unit->tools > 0 ? unit->tools : 0;
    muskets = unit->muskets > 0 ? unit->muskets : 0;
    horses = unit->horses > 0 ? unit->horses : 0;
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
static int g_units_combat_human_nation = -1;
static AiPopupState* g_units_combat_popups = NULL;
static const ColonizeMsgCatalog* g_units_combat_game_txt = NULL;
static ColonizeUnitsMoveWatchFn g_units_move_watch = NULL;
static void* g_units_move_watch_user = NULL;

void units_set_ff_col1(const ColonizeCol1Save* col1) {
  g_units_ff_col1 = col1;
}

void units_set_move_watch(ColonizeUnitsMoveWatchFn fn, void* user) {
  g_units_move_watch = fn;
  g_units_move_watch_user = user;
}

void units_set_combat_human_nation(int human_nation) {
  g_units_combat_human_nation = human_nation;
}

void units_set_combat_popups(AiPopupState* popups, const ColonizeMsgCatalog* game_txt) {
  g_units_combat_popups = popups;
  g_units_combat_game_txt = game_txt;
}

static ColonizeCombatStrengthCtx units_combat_strength_ctx(const ColonizeCol1Save* col1) {
  ColonizeCombatStrengthCtx ctx;
  ctx.units = NULL; /* filled by caller */
  ctx.map = g_units_occupancy_map ? g_units_occupancy_map : g_units_fallout_map;
  ctx.colonies = g_units_combat_colonies;
  ctx.col1 = col1 ? col1 : g_units_ff_col1;
  return ctx;
}

static void units_combat_maybe_present_analysis(
  const ColonizeCol1Save* col1,
  const ColonizeCombatEngagement* eng,
  int atk_nation,
  int def_nation
) {
  if (!eng || !combat_analysis_should_show(col1, atk_nation, def_nation, g_units_combat_human_nation)) {
    return;
  }
  combat_analysis_present_if_hooked(eng);
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
  case COLONIZE_ENTER_BOARD:
    return "Boarded ship";
  case COLONIZE_ENTER_VILLAGE_SHIP:
    return "Village";
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

int units_foreign_unit_at(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int except_unit_id,
  int except_nation_id
) {
  return units_foreign_at(pool, x, y, except_unit_id, except_nation_id);
}

/*
 * FUN_5fef_0000: walk stack for highest engagement toughness vs attacker.
 * Artillery vs Indian attacker ×2; skip type.attack==0.
 */
int units_best_defender_at(
  const ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int x,
  int y,
  int attacker_id,
  int except_id
) {
  if (!pool) {
    return -1;
  }
  const ColonizeUnit* atk = units_get_const(pool, attacker_id);
  const int atk_nat = atk ? atk->nation_id : -1;
  ColonizeCombatStrengthCtx sctx = units_combat_strength_ctx(col1);
  sctx.units = pool;

  int best_id = -1;
  int best_score = -1;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->id == except_id || u->id == attacker_id) {
      continue;
    }
    if (atk_nat >= 0 && u->nation_id == atk_nat) {
      continue;
    }
    if (!combat_unit_is_combat_role(pool, u->id)) {
      continue;
    }
    int score = combat_engagement_strength(&sctx, u->id, attacker_id, NULL);
    const ColonizeUnitType* t = units_type(pool, u->type_index);
    if (t && combat_type_is_artillery_name(t->name) && atk_nat > 3) {
      score *= 2;
    }
    if (score > best_score) {
      best_score = score;
      best_id = u->id;
    }
  }
  /* Fallback: any foreign unit (capture-only stacks with no combat role). */
  if (best_id < 0 && atk) {
    return units_foreign_at(pool, x, y, attacker_id, atk_nat);
  }
  return best_id;
}

int units_spawn_village_temp_defender(
  ColonizeUnitPool* pool,
  const ColonizeCol1Save* col1,
  int village_x,
  int village_y,
  int indian_nation,
  int attacker_id
) {
  if (!pool || !col1 || indian_nation < 4 || indian_nation > 11) {
    return -1;
  }
  /* Real map foe already on the settlement — fight them; no phantom. */
  if (units_best_defender_at(pool, col1, village_x, village_y, attacker_id, attacker_id) >= 0) {
    return -1;
  }

  int tribe_index = -1;
  if (col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->x == village_x && (int)t->y == village_y &&
          (int)t->nation_id == indian_nation) {
        tribe_index = (int)ti;
        break;
      }
    }
  }
  if (tribe_index < 0) {
    return -1;
  }

  /*
   * FUN_5fef_1b0e empty-village arm: type 0x13 Brave; muskets→0x14 Armed;
   * horse_breeding > 0x18 → +2 (Mtd. Braves / Mtd. Warriors).
   */
  const ColonizeCol1Indian* ind = &col1->indian[indian_nation - 4];
  const char* type_name = "Braves";
  if (ind->muskets != 0 && ind->horse_breeding > 0x18) {
    type_name = "Mtd. Warriors";
  } else if (ind->muskets != 0) {
    type_name = "Armed Braves";
  } else if (ind->horse_breeding > 0x18) {
    type_name = "Mtd. Braves";
  }
  int ti = units_find_type(pool, type_name);
  if (ti < 0) {
    ti = units_find_type(pool, "Braves");
  }
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(pool, ti, village_x, village_y);
  ColonizeUnit* u = units_get(pool, id);
  if (!u) {
    return -1;
  }
  u->nation_id = indian_nation;
  u->home_tribe_id = tribe_index;
  u->moves_left = 0;
  return id;
}

void units_finish_village_temp_defender(
  ColonizeUnitPool* pool,
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int temp_id,
  int attacker_won,
  int attacker_nation,
  int village_x,
  int village_y,
  ColonizeDosRng* rng
) {
  if (!pool || temp_id < 0) {
    return;
  }
  /* FUN_291f_0a06-shaped: always undo the phantom if it survived the roll. */
  ColonizeUnit* temp = units_get(pool, temp_id);
  if (temp && temp->active) {
    units_despawn(pool, temp_id);
  }
  if (!attacker_won || !col1 || !col1->tribe) {
    return;
  }
  ColonizeCol1Tribe* tribe = NULL;
  int indian_nation = -1;
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->x == village_x && (int)t->y == village_y && t->nation_id >= 4 &&
        t->nation_id <= 11) {
      tribe = t;
      indian_nation = (int)t->nation_id;
      break;
    }
  }
  if (!tribe || indian_nation < 4) {
    return;
  }
  /*
   * FUN_5fef_1b0e: if population < 2 → destroy dwelling; else population--.
   * Cite: *(tribe+4) check before DEC; FUN_291f_0248 destroy; 31ea fallout.
   */
  if (tribe->population < 2) {
    (void)units_try_native_settlement_fallout(
      col1, pool, map, attacker_nation, indian_nation, village_x, village_y, -1, rng
    );
  } else {
    tribe->population--;
  }
}

static int units_tribe_nation_at(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return -1;
  }
  for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &col1->tribe[ti];
    if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
      return (int)t->nation_id;
    }
  }
  return -1;
}

static int units_combat_human_involved(const ColonizeCol1Save* col1, int nat_a, int nat_b) {
  if (!col1) {
    return g_units_combat_human_nation >= 0 &&
           (nat_a == g_units_combat_human_nation || nat_b == g_units_combat_human_nation);
  }
  const int a_h =
    (nat_a >= 0 && nat_a <= 3 && col1->player[nat_a].control == 0) ||
    (g_units_combat_human_nation >= 0 && nat_a == g_units_combat_human_nation);
  const int b_h =
    (nat_b >= 0 && nat_b <= 3 && col1->player[nat_b].control == 0) ||
    (g_units_combat_human_nation >= 0 && nat_b == g_units_combat_human_nation);
  return a_h || b_h;
}

static const char* units_combat_nation_label(const ColonizeCol1Save* col1, int nation_id) {
  static const char* k_euro[4] = {"English", "French", "Spanish", "Dutch"};
  static const char* k_tribe[8] = {
    "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
  };
  if (nation_id >= 0 && nation_id <= 3) {
    if (col1 && col1->player[nation_id].country_name[0]) {
      return col1->player[nation_id].country_name;
    }
    return k_euro[nation_id];
  }
  if (nation_id >= 4 && nation_id <= 11) {
    return k_tribe[nation_id - 4];
  }
  return "enemy";
}

/* Colony name, else village tribe, else LABELS "Wilderness". */
static const char* units_combat_place_label(
  const ColonizeCol1Save* col1,
  int x,
  int y
) {
  if (g_units_combat_colonies) {
    const int cid = colonies_id_at(g_units_combat_colonies, x, y);
    const ColonizeColony* col = colonies_get(g_units_combat_colonies, cid);
    if (col && col->active && col->name[0]) {
      return col->name;
    }
  }
  if (col1 && col1->tribe) {
    for (uint16_t ti = 0; ti < col1->head.tribe_count; ++ti) {
      const ColonizeCol1Tribe* t = &col1->tribe[ti];
      if ((int)t->x == x && (int)t->y == y && t->nation_id >= 4 && t->nation_id <= 11) {
        return units_combat_nation_label(col1, (int)t->nation_id);
      }
    }
  }
  return "Wilderness";
}

static const char* units_combat_unit_label(
  const ColonizeUnitPool* pool,
  const ColonizeUnit* u
) {
  if (!u) {
    return "unit";
  }
  const ColonizeUnitType* t = units_type(pool, u->type_index);
  if (t && t->name[0]) {
    return t->name;
  }
  return "unit";
}

/*
 * LABELS.TXT "defeat" / "defeats". Nation subjects use "defeat"; unit subjects
 * with type_index ≥ 7 (Cont. Cav.+) use "defeats" (FUN_5fef_1b0e).
 */
static const char* units_combat_defeat_verb(int subject_is_nation_only, int unit_type_index) {
  if (subject_is_nation_only) {
    return "defeat";
  }
  return (unit_type_index >= 0 && unit_type_index < 7) ? "defeat" : "defeats";
}

static void units_combat_enqueue_tok(
  AiPopupTag tag,
  const char* section,
  int nation_a,
  int nation_b,
  int payload,
  const PopupMsgTokens* tok,
  const char* fallback
) {
  if (!g_units_combat_popups || !section) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens local;
  memset(&local, 0, sizeof(local));
  if (tok) {
    local = *tok;
  }
  if (g_units_combat_game_txt) {
    popup_msg_fill(g_units_combat_game_txt, section, &local, fallback, body, sizeof(body));
  } else {
    snprintf(body, sizeof(body), "%s", fallback ? fallback : section);
  }
  (void)ai_popup_enqueue_ok_ctx(
    g_units_combat_popups, tag, nation_a, nation_b, payload, NULL, body
  );
}

static void units_combat_enqueue_section(
  AiPopupTag tag,
  const char* section,
  int nation_a,
  int nation_b,
  const char* string0,
  const char* string1,
  const char* fallback
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = string0;
  tok.string1 = string1;
  units_combat_enqueue_tok(tag, section, nation_a, nation_b, 0, &tok, fallback);
}

bool units_combat_apply_ransom_popup(ColonizeCol1Save* col1, const AiPopupState* popups) {
  if (!popups || popups->result_tag != AI_POPUP_TAG_COMBAT_RANSOM) {
    return false;
  }
  if (!col1 || popups->result_cancelled || popups->result_choice_id != 1) {
    return true; /* Refuse / cancel: no gold */
  }
  const int nation = popups->result_nation_a;
  const int gold = popups->result_payload;
  if (nation < 0 || nation > 3 || gold <= 0) {
    return true;
  }
  const uint32_t g = col1->nation[nation].gold;
  const uint32_t add = (uint32_t)gold;
  col1->nation[nation].gold = g > UINT32_MAX - add ? UINT32_MAX : g + add;
  return true;
}

void units_combat_notify_colony_captured(
  const ColonizeCol1Save* col1,
  const ColonizeColony* colony,
  int capturer_nation,
  int plunder_gold
) {
  if (!colony || !colony->active) {
    return;
  }
  if (!units_combat_human_involved(col1, capturer_nation, colony->nation_id)) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = units_combat_nation_label(col1, capturer_nation);
  tok.string2 = colony->name[0] ? colony->name : "colony";
  if (plunder_gold > 0) {
    tok.number0 = plunder_gold;
    tok.has_number0 = true;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_COLONY,
      "CAPTURED",
      capturer_nation,
      colony->nation_id,
      plunder_gold,
      &tok,
      "Colony captured."
    );
  } else if (
    capturer_nation >= 0 && capturer_nation <= 3 && col1 &&
    col1->player[capturer_nation].control != 0
  ) {
    /* AI capturer vs human: spies report. */
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_COLONY,
      "CAPTURED2",
      capturer_nation,
      colony->nation_id,
      0,
      &tok,
      "Colony captured."
    );
  } else {
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_COLONY,
      "CAPTURED3",
      capturer_nation,
      colony->nation_id,
      0,
      &tok,
      "Colony captured."
    );
  }
}

void units_combat_notify_colony_burned(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
) {
  if (!colony_name || !colony_name[0]) {
    return;
  }
  const int human =
    (victim_nation >= 0 && victim_nation <= 3 && col1 &&
     col1->player[victim_nation].control == 0) ||
    (g_units_combat_human_nation >= 0 && victim_nation == g_units_combat_human_nation);
  if (!human) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = burner_label && burner_label[0] ? burner_label : "Enemies";
  tok.string1 = units_combat_nation_label(col1, victim_nation);
  tok.string3 = colony_name;
  units_combat_enqueue_tok(
    AI_POPUP_TAG_COMBAT_COLONY,
    "BURNED",
    victim_nation,
    -1,
    0,
    &tok,
    "Colony burned."
  );
}

void units_combat_notify_colony_burned_foreign(
  const ColonizeCol1Save* col1,
  const char* colony_name,
  int victim_nation,
  const char* burner_label
) {
  if (!colony_name || !colony_name[0]) {
    return;
  }
  /* Bystander only: a human nation exists and it is not the victim. The
   * victim already gets @BURNED (units_combat_notify_colony_burned); the
   * burner here is always a native tribe (id ≥4), never a euro nation. */
  if (g_units_combat_human_nation < 0 || g_units_combat_human_nation == victim_nation) {
    return;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = burner_label && burner_label[0] ? burner_label : "Enemies";
  tok.string1 = units_combat_nation_label(col1, victim_nation);
  tok.string3 = colony_name;
  units_combat_enqueue_tok(
    AI_POPUP_TAG_INFO,
    "BURNED3",
    g_units_combat_human_nation,
    victim_nation,
    0,
    &tok,
    "Spies report: %STRING0 burn %STRING1 colony at %STRING3."
  );
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

/*
 * FUN_5fef_0352 type demote table (NAMES @UNIT indices):
 * Dragoon→Soldier, Soldier→Colonist, Cont.Cav→Cont.Army, Cavalry→Regulars,
 * Cont.Army→Colonist; Jesuit profession on Colonist demote → Missionary.
 * Returns new type_index or -1 if no demote (destroy path).
 */
static int units_combat_demote_type_index(ColonizeUnitPool* pool, const ColonizeUnit* loser) {
  if (!pool || !loser) {
    return -1;
  }
  const ColonizeUnitType* lt = units_type(pool, loser->type_index);
  if (!lt || !lt->name[0]) {
    return -1;
  }
  const char* n = lt->name;
  const char* dest = NULL;
  if (strstr(n, "Cont") != NULL && strstr(n, "Cav") != NULL) {
    dest = "Cont. Army";
  } else if (strstr(n, "Cavalry") != NULL) {
    dest = "Regulars";
  } else if (strstr(n, "Cont") != NULL && strstr(n, "Army") != NULL) {
    dest = (loser->profession == UNITS_JOB_MISSIONARY) ? "Missionaries" : "Colonists";
  } else if (strstr(n, "Dragoon") != NULL) {
    dest = "Soldiers";
  } else if (strstr(n, "Soldier") != NULL) {
    dest = (loser->profession == UNITS_JOB_MISSIONARY) ? "Missionaries" : "Colonists";
  }
  if (!dest) {
    return -1;
  }
  return units_find_type(pool, dest);
}

/* Align tools/muskets/horses with post-demote type (spawn-shaped). */
static void units_sync_equip_after_type_change(ColonizeUnit* u, const ColonizeUnitType* t) {
  if (!u || !t) {
    return;
  }
  u->tools = 0;
  u->muskets = 0;
  u->horses = 0;
  if (strstr(t->name, "Pioneer") != NULL) {
    u->tools = UNITS_EQUIP_TOOLS_MAX;
  } else if (strstr(t->name, "Dragoon") != NULL || strstr(t->name, "Cavalry") != NULL) {
    u->muskets = UNITS_EQUIP_MUSKETS;
    u->horses = UNITS_EQUIP_HORSES;
  } else if (
    strstr(t->name, "Soldier") != NULL || strstr(t->name, "Regular") != NULL ||
    strstr(t->name, "Army") != NULL
  ) {
    u->muskets = UNITS_EQUIP_MUSKETS;
  } else if (strstr(t->name, "Scout") != NULL) {
    u->horses = UNITS_EQUIP_HORSES;
  }
}

/*
 * FUN_5fef_0352 land demote: change type, @DEMOTE if human-facing.
 * Returns 1 if demoted (unit survives), 0 if no demote mapping.
 */
static int units_demote_combat_type(
  ColonizeUnitPool* pool,
  ColonizeUnit* loser,
  const ColonizeCol1Save* col1,
  int human_facing
) {
  if (!pool || !loser || !loser->active) {
    return 0;
  }
  const int tgt = units_combat_demote_type_index(pool, loser);
  if (tgt < 0 || tgt == loser->type_index) {
    return 0;
  }
  const ColonizeUnitType* old_ty = units_type(pool, loser->type_index);
  const char* old_name = old_ty && old_ty->name[0] ? old_ty->name : "unit";
  loser->type_index = tgt;
  const ColonizeUnitType* nt = units_type(pool, tgt);
  units_sync_equip_after_type_change(loser, nt);
  loser->orders = UNITS_ORDER_NONE;
  loser->moves_left = 0;
  if (human_facing) {
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, loser->nation_id);
    tok.string1 = old_name;
    tok.string2 = nt && nt->name[0] ? nt->name : "colonist";
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_DEMOTE,
      "DEMOTE",
      loser->nation_id,
      -1,
      0,
      &tok,
      "Unit demoted."
    );
  }
  return 1;
}

/*
 * FUN_5fef_172c chance promote: Soldier/Dragoon → veteran profession/type.
 * Washington always promotes (caller); here RNG margin path.
 */
static int units_chance_promote_on_win(
  ColonizeUnitPool* pool,
  ColonizeUnit* winner,
  const ColonizeCol1Save* col1,
  int atk_strength,
  int def_strength,
  int roll,
  ColonizeDosRng* rng
) {
  if (!pool || !winner || !winner->active) {
    return 0;
  }
  if (col1 && founding_fathers_nation_has(col1, winner->nation_id, FF_GEORGE_WASHINGTON)) {
    return 0; /* Washington path already ran / will run */
  }
  const ColonizeUnitType* ut = units_type(pool, winner->type_index);
  const char* tname = ut ? ut->name : NULL;
  const char* dname = units_display_name(pool, winner);
  if ((dname && (strstr(dname, "Veteran") || strstr(dname, "Continental"))) ||
      (tname &&
       (strstr(tname, "Veteran") || strstr(tname, "Cont.") || strstr(tname, "Continental")))) {
    return 0;
  }
  const int is_dragoon =
    (tname && (strstr(tname, "Dragoon") || strstr(tname, "Cavalry"))) ||
    (dname && (strstr(dname, "Dragoon") || strstr(dname, "Cavalry")));
  const int is_soldier =
    !is_dragoon &&
    ((tname && strstr(tname, "Soldier") != NULL) || (dname && strstr(dname, "Soldier") != NULL));
  if (!is_soldier && !is_dragoon) {
    return 0;
  }
  /* Margin: how far roll beat the weaker side. Chance rises with margin. */
  int margin = 0;
  if (atk_strength + def_strength > 0) {
    if (roll <= atk_strength) {
      margin = atk_strength - roll + 1;
    } else {
      margin = roll - atk_strength;
    }
  }
  int difficulty = col1 ? (int)col1->head.difficulty : 2;
  int threshold = 8 + difficulty;
  if (winner->profession == UNITS_JOB_COLONIST || winner->profession == UNITS_JOB_NONE) {
    threshold += 5;
  }
  if (!rng) {
    if (margin < threshold / 2) {
      return 0;
    }
  } else {
    const int r = dos_rng_range(rng, 1, threshold + margin);
    if (r > margin + 2) {
      return 0;
    }
  }
  if (is_dragoon) {
    int tgt = units_find_type(pool, "Veteran Dragoon");
    if (tgt < 0) {
      tgt = units_find_type(pool, "Cont. Cav.");
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
    if (tgt >= 0) {
      winner->type_index = tgt;
    }
    winner->profession = UNITS_JOB_SOLDIER;
  }
  return 1;
}

/*
 * FUN_5fef_0352: artillery damage; Euro-only Colonist/Wagon capture; type demote;
 * else despawn. Natives never capture (winner nation must be 0..3). Pioneers /
 * Missionaries / Scouts are not capture types → destroy. Returns 1 if unit still
 * active (captured/damaged/demoted), 0 if gone.
 */
static int units_apply_land_loss_outcome(
  ColonizeUnitPool* pool,
  int loser_id,
  int winner_id,
  const ColonizeCol1Save* col1,
  int show_popups
) {
  ColonizeUnit* lose = units_get(pool, loser_id);
  ColonizeUnit* win = units_get(pool, winner_id);
  if (!lose || !win || !lose->active) {
    return 0;
  }
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  const ColonizeUnitType* wt = units_type(pool, win->type_index);
  const int human =
    show_popups && units_combat_human_involved(col1, lose->nation_id, win->nation_id);
  const int win_euro = win->nation_id >= 0 && win->nation_id <= 3;
  const int win_can_capture = win_euro && wt && wt->attack > 0;
  const int loser_euro = lose->nation_id >= 0 && lose->nation_id <= 3;

  /* Artillery: first loss → damaged bit7; already damaged → sink. */
  if (lt && combat_type_is_artillery_name(lt->name)) {
    if ((lose->col1_unknown15 & 0x80u) == 0) {
      lose->col1_unknown15 |= 0x80u;
      lose->moves_left = 0;
      if (human) {
        units_combat_enqueue_section(
          AI_POPUP_TAG_COMBAT_SHIP,
          "SHIPDAMAGE",
          lose->nation_id,
          win->nation_id,
          lt->name,
          wt ? wt->name : NULL,
          "Artillery damaged."
        );
      }
      return 1;
    }
  }

  /*
   * Capture-alive (DOS type ∈ {Colonists, Wagon}; Treasure handled in resolve).
   * Winner must be Euro with attack>0 — natives destroy, never nation-flip.
   */
  if (loser_euro && lt && lt->name[0] && win_can_capture) {
    const int is_treasure = strstr(lt->name, "Treasure") != NULL;
    const int is_wagon = strstr(lt->name, "Wagon") != NULL;
    const int is_colonist =
      strstr(lt->name, "Colonist") != NULL && !is_treasure && !is_wagon;
    if (is_treasure) {
      /* Treasure gold handled in resolve; despawn below. */
    } else if (is_wagon) {
      const int from_nat = lose->nation_id;
      int cargo_amt = 0;
      for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
        if (lose->hold_goods_amount[i] > 0 && lose->hold_goods_amount[i] < 255) {
          cargo_amt += lose->hold_goods_amount[i];
        }
      }
      units_set_nation(lose, win->nation_id);
      lose->orders = UNITS_ORDER_NONE;
      lose->moves_left = 0;
      if (human) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, from_nat);
        tok.string1 = units_combat_nation_label(col1, win->nation_id);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_CAPTURE,
          "WAGONCAPTURE",
          win->nation_id,
          from_nat,
          0,
          &tok,
          "Wagon captured."
        );
        if (cargo_amt > 0) {
          tok.number0 = cargo_amt;
          tok.has_number0 = true;
          tok.string1 = "goods";
          tok.string2 = units_combat_nation_label(col1, win->nation_id);
          tok.string3 = wt && wt->name[0] ? wt->name : "forces";
          units_combat_enqueue_tok(
            AI_POPUP_TAG_COMBAT_CAPTURE,
            "CARGOCAPTURE",
            win->nation_id,
            from_nat,
            cargo_amt,
            &tok,
            "Cargo captured."
          );
        }
      }
      return 1;
    } else if (is_colonist) {
      const int from_nat = lose->nation_id;
      /* DOS: Veteran specialty (0x15) → NONE + @COLONISTCAPTURE2. */
      const int stripped_vet = (lose->profession == UNITS_JOB_SOLDIER);
      if (stripped_vet) {
        lose->profession = UNITS_JOB_NONE;
      }
      units_set_nation(lose, win->nation_id);
      lose->orders = UNITS_ORDER_NONE;
      lose->moves_left = 0;
      if (human) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, from_nat);
        tok.string1 = units_combat_nation_label(col1, win->nation_id);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_CAPTURE,
          stripped_vet ? "COLONISTCAPTURE2" : "COLONISTCAPTURE",
          win->nation_id,
          from_nat,
          0,
          &tok,
          "Colonists captured."
        );
      }
      return 1;
    }
  }

  /* Type demote (Soldier→Colonist, Dragoon→Soldier, …); keep nation. */
  if (units_demote_combat_type(pool, lose, col1, human)) {
    return 1;
  }

  units_despawn(pool, loser_id);
  return 0;
}

/* Thin FUN_5fef_0ec0: after combat loss, capture leftover non-combat same-nation stack. */
static void units_sweep_stack_after_loss(
  ColonizeUnitPool* pool,
  int x,
  int y,
  int loser_nation,
  int winner_id,
  int primary_loser_id,
  const ColonizeCol1Save* col1
) {
  if (!pool || loser_nation < 0) {
    return;
  }
  ColonizeUnit* win = units_get(pool, winner_id);
  if (!win || !win->active) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &pool->units[i];
    if (!units_is_on_map(u) || u->x != x || u->y != y) {
      continue;
    }
    if (u->nation_id != loser_nation || u->id == winner_id || u->id == primary_loser_id) {
      continue;
    }
    const ColonizeUnitType* t = units_type(pool, u->type_index);
    if (t && t->attack == 0 && u->nation_id >= 0 && u->nation_id <= 3) {
      (void)units_apply_land_loss_outcome(pool, u->id, winner_id, col1, 0);
    }
  }
}

/* Naval: damage-not-always-sink when margin close; else plunder+despawn. */
static int units_apply_naval_loss_outcome(
  ColonizeUnitPool* pool,
  int loser_id,
  int winner_id,
  int loser_str,
  int winner_str,
  int show_popups,
  const ColonizeCol1Save* col1
) {
  ColonizeUnit* lose = units_get(pool, loser_id);
  ColonizeUnit* win = units_get(pool, winner_id);
  if (!lose || !win || !lose->active) {
    return 0;
  }
  const ColonizeUnitType* lt = units_type(pool, lose->type_index);
  const ColonizeUnitType* wt = units_type(pool, win->type_index);
  const int human =
    show_popups && units_combat_human_involved(col1, lose->nation_id, win->nation_id);

  /* Close fight + weaker type attack: set damaged bit and escape (1b0e ship peel). */
  const int lose_atk = lt ? lt->attack : 0;
  const int win_atk = wt ? wt->attack : 0;
  const int close = loser_str * 2 > winner_str && winner_str > 0;
  if (close && lose_atk < win_atk && (lose->col1_unknown15 & 0x80u) == 0) {
    lose->col1_unknown15 |= 0x80u;
    lose->moves_left = 0;
    /* Finished ship: mark past construction thresh so Drydock (not build tick) repairs. */
    {
      const int thresh = lt && lt->defense > 0 ? lt->defense : 4;
      if (lose->turns_worked < thresh) {
        lose->turns_worked = (uint8_t)thresh;
      }
    }
    if (human) {
      units_combat_enqueue_section(
        AI_POPUP_TAG_COMBAT_SHIP,
        "SHIPDAMAGE",
        lose->nation_id,
        win->nation_id,
        lt ? lt->name : "Ship",
        wt ? wt->name : NULL,
        "Ship damaged."
      );
    }
    return 1;
  }

  (void)units_plunder_ship_holds(pool, winner_id, loser_id);
  if (human) {
    units_combat_enqueue_section(
      AI_POPUP_TAG_COMBAT_SHIP,
      "SHIPSUNK",
      win->nation_id,
      lose->nation_id,
      lt ? lt->name : "Ship",
      wt ? wt->name : NULL,
      "Ship sunk."
    );
  }
  units_despawn(pool, loser_id);
  return 0;
}

static void units_combat_outcome_popups(
  const ColonizeUnitPool* pool,
  int winner_id,
  int loser_id,
  int atk_wins,
  int atk_nation,
  int def_nation,
  int is_naval,
  int ambush,
  const ColonizeCol1Save* col1
) {
  (void)ambush; /* Indian ambush chrome is owned by ai_contact (@INDIANWIN*). */
  if (!units_combat_human_involved(col1, atk_nation, def_nation)) {
    return;
  }
  const ColonizeUnit* win = units_get_const(pool, winner_id);
  const ColonizeUnit* lose = units_get_const(pool, loser_id);
  if (!win || !lose) {
    return;
  }

  if (!is_naval) {
    /*
     * Native attacker: INDIANWIN/LOSE owned by ai_contact ambush chrome.
     * Euro attacker (incl. vs natives): @EUROPEWIN / @EUROPELOSE.
     * Cite: FUN_5fef_1b0e both-euro gate; indian raid ambush fill.
     */
    if (atk_nation < 4) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      const int place_x = atk_wins ? lose->x : win->x;
      const int place_y = atk_wins ? lose->y : win->y;
      tok.string0 = units_combat_nation_label(col1, atk_nation);
      tok.string1 = units_combat_nation_label(col1, def_nation);
      {
        const ColonizeUnit* def_u = atk_wins ? lose : win;
        tok.string2 = units_combat_unit_label(pool, def_u);
      }
      tok.string3 = units_combat_place_label(col1, place_x, place_y);
      if (atk_wins) {
        tok.string4 = units_combat_defeat_verb(1, -1);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_EUROPE,
          "EUROPEWIN",
          atk_nation,
          def_nation,
          0,
          &tok,
          "Victory!"
        );
      } else {
        const ColonizeUnit* def_u = win;
        tok.string4 = units_combat_defeat_verb(0, def_u->type_index);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_EUROPE,
          "EUROPELOSE",
          atk_nation,
          def_nation,
          0,
          &tok,
          "Defeat."
        );
      }
    }
    /*
     * Crown / REF land win vs human → @SEIZURELAND (Royal Army). Privateer
     * naval path uses custom body; peer Euro uses @EUROPEWIN above.
     */
    if (atk_wins && col1 && atk_nation < 4) {
      int human = -1;
      for (int i = 0; i < 4; ++i) {
        if (col1->player[i].control == 0) {
          human = i;
          break;
        }
      }
      const int crown = (human == 0) ? 1 : (human == 1) ? 0 : -1;
      if (crown >= 0 && win->nation_id == crown && lose->nation_id == human) {
        PopupMsgTokens stok;
        memset(&stok, 0, sizeof(stok));
        stok.string0 = units_combat_unit_label(pool, lose);
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_SEIZURE,
          "SEIZURELAND",
          win->nation_id,
          lose->nation_id,
          0,
          &stok,
          "Unit captured by the Royal Army."
        );
      }
    }
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
   * Post-win fallout for FUN_5fef_31ea (structural): destroy native village
   * when explicitly conquering an empty dwelling (caller: village temp Brave
   * arm when population < 2). Convert-join before destroy when mission owned
   * by attacker; Cortes treasure after. Not triggered merely by killing a map
   * Brave on the tile.
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
    if (rich_capital) {
      /*
       * Fandom Capital destroy: hostile tribe surrenders once — hostility
       * reset + peace; no new capital (destroyed). Cite: docs/fandom_col1994.md.
       */
      ai_diplo_indian_capital_surrender(col1, tribe_nation, attacker_nation_id);
    } else {
      ai_diplo_indian_relation_delta(col1, tribe_nation, attacker_nation_id, -5);
      ai_diplo_indian_hostility_sync(col1, attacker_nation_id);
    }
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
      if (units_combat_human_involved(col1, attacker_nation_id, defender_nation_id)) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, attacker_nation_id);
        tok.string1 = "native";
        tok.string2 = "village";
        tok.number0 = gold;
        tok.has_number0 = true;
        units_combat_enqueue_tok(
          AI_POPUP_TAG_COMBAT_LOOT, "LOOT", attacker_nation_id, defender_nation_id, gold, &tok,
          "Treasure recovered from ruins."
        );
      }
    } else if (units_combat_human_involved(col1, attacker_nation_id, defender_nation_id)) {
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = units_combat_nation_label(col1, attacker_nation_id);
      tok.string1 = "native";
      tok.string2 = "village";
      units_combat_enqueue_tok(
        AI_POPUP_TAG_COMBAT_LOOT,
        "LOOT2",
        attacker_nation_id,
        defender_nation_id,
        0,
        &tok,
        "Village burned; natives flee."
      );
    }
  } else if (
    units_combat_human_involved(col1, attacker_nation_id, defender_nation_id) &&
    attacker_nation_id >= 0 && attacker_nation_id < 4
  ) {
    /* Burn without Cortes treasure — GAME.TXT @LOOT2 (not @NOLOOT). */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = units_combat_nation_label(col1, attacker_nation_id);
    tok.string1 = "native";
    tok.string2 = "village";
    units_combat_enqueue_tok(
      AI_POPUP_TAG_COMBAT_LOOT,
      "LOOT2",
      attacker_nation_id,
      defender_nation_id,
      0,
      &tok,
      "Village burned; natives flee."
    );
  }
  return true;
}

/*
 * FUN_65dd_0004 outcome kinds (GAME.TXT @LOSTCITY1..9 / @BURIAL1..3 /
 * @SCREWED). Exact DOS weight table not RE'd — the buckets below are our own
 * choice (PARK), tuned so common finds dominate and rare ones (Cibola,
 * vanish) are uncommon; order matters only in that bucket 0 is what
 * dos_rng_range(NULL, ...) deterministically lands on (tests pass rng=NULL).
 */
typedef enum ColonizeLcrOutcome {
  COLONIZE_LCR_NOTHING = 0,     /* @LOSTCITY6 */
  COLONIZE_LCR_SMALL_TREASURE,  /* @LOSTCITY3 */
  COLONIZE_LCR_CHIEFS_GIFT,     /* @LOSTCITY7 */
  COLONIZE_LCR_BURIAL_MOUNDS,   /* @LOSTCITY4 → @BURIAL1/2/3 / @SCREWED */
  COLONIZE_LCR_TRESPASS_ANGER,  /* @LOSTCITY8 */
  COLONIZE_LCR_SURVIVORS_JOIN,  /* @LOSTCITY9 */
  COLONIZE_LCR_FOUNTAIN_OF_YOUTH, /* @LOSTCITY1 */
  COLONIZE_LCR_VANISHES,        /* @LOSTCITY5 */
  COLONIZE_LCR_CIBOLA           /* @LOSTCITY2 */
} ColonizeLcrOutcome;

/* FUN_4cc6_0356-shaped nearest-tribe scan; -1 if none. */
static int units_lcr_nearest_tribe_nation(const ColonizeCol1Save* col1, int x, int y) {
  if (!col1 || !col1->tribe) {
    return -1;
  }
  int best = -1;
  int best_d = 0x7fffffff;
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* tr = &col1->tribe[i];
    const int dx = x - (int)tr->x;
    const int dy = y - (int)tr->y;
    const int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    if (d < best_d) {
      best_d = d;
      best = tr->nation_id;
    }
  }
  return best;
}

/* Credits both the persisted Col1 nation gold and the live Europe screen
 * cache (only the latter is what the human player can spend mid-session). */
static void units_lcr_credit_gold(
  ColonizeCol1Save* col1,
  EuropeScreen* europe,
  int human_nation,
  int nation,
  int amount
) {
  if (!col1 || amount <= 0 || nation < 0 || nation >= 4) {
    return;
  }
  col1->nation[nation].gold += (uint32_t)amount;
  if (europe && nation == human_nation) {
    europe->gold += amount;
  }
}

bool units_resolve_lcr_rumour(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  ColonizeDosRng* rng,
  EuropeScreen* europe,
  int human_nation
) {
  /*
   * FUN_65dd_0004 thin transcription: Scout on a procedural rumour tile
   * clears it and rolls one of the manual-documented outcomes (treasure /
   * Fountain of Youth / Cibola / survivors join / burial mounds / vanish /
   * nothing). de Soto (FF 7) keeps its "always positive" framing — extended
   * sight is a separate always-on FF effect (see founding_fathers.h); here
   * it restricts the draw to the non-hostile subset instead of a bare
   * reveal-only shortcut. Deep DOS RNG weights / native-attack combat
   * resolution stay PARKed — see per-outcome comments below.
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
  const int x = u->x;
  const int y = u->y;
  const int nation = u->nation_id;
  const bool de_soto = col1 && nation >= 0 && nation < 4 &&
    founding_fathers_de_soto_lcr_always_positive(col1, nation);
  if (de_soto) {
    map_reveal_radius(map, x, y, nation, 1);
  }

  ColonizeLcrOutcome outcome;
  if (de_soto) {
    /* Positive-only draw: treasure / gift / FoY / Cibola / survivors. */
    static const ColonizeLcrOutcome k_positive[] = {
      COLONIZE_LCR_SMALL_TREASURE,
      COLONIZE_LCR_CHIEFS_GIFT,
      COLONIZE_LCR_SURVIVORS_JOIN,
      COLONIZE_LCR_FOUNTAIN_OF_YOUTH,
      COLONIZE_LCR_CIBOLA
    };
    const int n = (int)(sizeof(k_positive) / sizeof(k_positive[0]));
    outcome = k_positive[dos_rng_range(rng, 1, n) - 1];
  } else {
    /* Weighted 1..100 draw; bucket 0 (1..25) is what rng==NULL always picks. */
    const int roll = dos_rng_range(rng, 1, 100);
    if (roll <= 25) {
      outcome = COLONIZE_LCR_NOTHING;
    } else if (roll <= 45) {
      outcome = COLONIZE_LCR_SMALL_TREASURE;
    } else if (roll <= 60) {
      outcome = COLONIZE_LCR_CHIEFS_GIFT;
    } else if (roll <= 72) {
      outcome = COLONIZE_LCR_BURIAL_MOUNDS;
    } else if (roll <= 82) {
      outcome = COLONIZE_LCR_TRESPASS_ANGER;
    } else if (roll <= 90) {
      outcome = COLONIZE_LCR_SURVIVORS_JOIN;
    } else if (roll <= 95) {
      outcome = COLONIZE_LCR_FOUNTAIN_OF_YOUTH;
    } else if (roll <= 98) {
      outcome = COLONIZE_LCR_VANISHES;
    } else {
      outcome = COLONIZE_LCR_CIBOLA;
    }
  }

  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  switch (outcome) {
  case COLONIZE_LCR_NOTHING:
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY6", nation, -1, 0, &tok, "You find nothing but rumors."
    );
    break;
  case COLONIZE_LCR_SMALL_TREASURE: {
    const int gold = dos_rng_range(rng, 3, 24) * 10; /* ~30..240 */
    units_lcr_credit_gold(col1, europe, human_nation, nation, gold);
    tok.has_number0 = true;
    tok.number0 = gold;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY3", nation, -1, gold, &tok,
      "You find the ruins of a lost civilization."
    );
    break;
  }
  case COLONIZE_LCR_CHIEFS_GIFT: {
    const int gold = dos_rng_range(rng, 1, 20) * 4; /* ~4..80 */
    units_lcr_credit_gold(col1, europe, human_nation, nation, gold);
    tok.has_number0 = true;
    tok.number0 = gold;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY7", nation, -1, gold, &tok,
      "A small, friendly tribe offers you a gift."
    );
    break;
  }
  case COLONIZE_LCR_FOUNTAIN_OF_YOUTH:
    /* 8 free dock immigrants (FUN_65dd_0004 8x FUN_291f_0d2c). Human only —
     * AI nations have no modeled EuropeScreen recruit pool (PARK). */
    if (europe && nation == human_nation) {
      for (int i = 0; i < 8; ++i) {
        /* FoY funnels through 4884, not 5e52's 04d4 slot roll — out of scope
         * here; keep deterministic first-filled pick (rng=NULL). */
        (void)europe_immigrant_from_pool(europe, NULL);
      }
    }
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY1", nation, -1, 0, &tok,
      "You have discovered a Fountain of Youth!"
    );
    break;
  case COLONIZE_LCR_CIBOLA: {
    /* Seven Cities of Cibola: big find, needs a Galleon home (treasure
     * train, same mechanic as Cortes conquest treasure). */
    const int gold =
      (dos_rng_range(rng, 1, 6) + (col1 ? col1->head.difficulty : 0) + 5) * 100;
    (void)units_spawn_treasure_train(pool, x, y, nation, gold);
    tok.has_number1 = true;
    tok.number1 = gold;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY2", nation, -1, gold, &tok,
      "You have found one of the Seven Cities of Cibola!"
    );
    break;
  }
  case COLONIZE_LCR_SURVIVORS_JOIN: {
    /* "Desperate survivors... swear allegiance" — a free colonist joins. */
    const int ct = units_find_type(pool, "Colonists");
    if (ct >= 0) {
      const int nid = units_spawn_allow_stack(pool, ct, x, y);
      ColonizeUnit* nu = units_get(pool, nid);
      if (nu) {
        units_set_nation(nu, nation);
      }
    }
    tok.string0 = units_combat_nation_label(col1, nation);
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY9", nation, -1, 0, &tok,
      "Desperate survivors of a former colony join you."
    );
    break;
  }
  case COLONIZE_LCR_TRESPASS_ANGER: {
    const int tribe = units_lcr_nearest_tribe_nation(col1, x, y);
    if (col1 && tribe >= 0 && nation >= 0 && nation < 4) {
      ai_diplo_indian_relation_delta(col1, tribe, nation, -15);
    }
    tok.string0 = units_combat_nation_label(col1, tribe);
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY8", nation, -1, 0, &tok,
      "You are trespassing near sacred native shrines."
    );
    break;
  }
  case COLONIZE_LCR_VANISHES:
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "LOSTCITY5", nation, -1, 0, &tok,
      "Your expedition has vanished without a trace!"
    );
    units_despawn(pool, unit_id);
    break;
  case COLONIZE_LCR_BURIAL_MOUNDS: {
    /* @LOSTCITY4 (Search / Stay clear) auto-resolves as Search — full
     * interactive CHOICE + native-attack combat resolve PARKED. */
    const int tribe = units_lcr_nearest_tribe_nation(col1, x, y);
    const int sub = dos_rng_range(rng, 1, 100);
    if (sub <= 40) {
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "BURIAL1", nation, -1, 0, &tok, "The mounds are cold and empty."
      );
    } else if (sub <= 70) {
      const int gold = dos_rng_range(rng, 2, 12) * 10;
      units_lcr_credit_gold(col1, europe, human_nation, nation, gold);
      tok.has_number0 = true;
      tok.number0 = gold;
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "BURIAL2", nation, -1, gold, &tok, "Within, you find trinkets."
      );
    } else if (sub <= 80) {
      const int gold = (dos_rng_range(rng, 1, 6) + 8) * 100;
      (void)units_spawn_treasure_train(pool, x, y, nation, gold);
      tok.has_number1 = true;
      tok.number1 = gold;
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "BURIAL3", nation, -1, gold, &tok,
        "Within, you find incredible treasure!"
      );
    } else {
      if (col1 && tribe >= 0 && nation >= 0 && nation < 4) {
        ai_diplo_indian_relation_delta(col1, tribe, nation, -25);
      }
      tok.string0 = units_combat_nation_label(col1, tribe);
      units_combat_enqueue_tok(
        AI_POPUP_TAG_INFO, "SCREWED", nation, -1, 0, &tok,
        "These are sacred burial grounds! You must die!"
      );
      /* Thin "you must die": 50/50 the expedition is lost outright rather
       * than resolving full native-attack combat (PARK). */
      if (dos_rng_range(rng, 1, 2) == 1) {
        units_despawn(pool, unit_id);
      }
    }
    break;
  }
  }
  return true;
}

static ColonizeSoundPlayFn g_units_combat_sound_play = NULL;
static ColonizeSoundActiveIdFn g_units_combat_sound_active_id = NULL;

void units_set_combat_music_hooks(
  ColonizeSoundPlayFn play_fn, ColonizeSoundActiveIdFn active_id_fn
) {
  g_units_combat_sound_play = play_fn;
  g_units_combat_sound_active_id = active_id_fn;
}

/*
 * DOS combat engagement (segment 5fef, reached after an attacker/defender
 * tile-adjacency check succeeds) pushes literal id 0x32 — @PICKMUSIC
 * Military sublist, first song (docs/assets.md) — into the BGM-change path
 * (FUN_281f_048e -> FUN_129f_02cc) once a land or naval attack begins. The
 * real driver only restarts playback when the id actually changes
 * (FUN_129f_0318 "cmp [0x9c],id; jz done"); mirror that via the active-id
 * hook so a combat-heavy turn does not restart the track on every attack.
 */
static void units_combat_music_sting(void) {
  if (!g_units_combat_sound_play) {
    return;
  }
  if (!g_units_combat_sound_active_id ||
      g_units_combat_sound_active_id() != SOUND_MILITARY_BGM_ID) {
    g_units_combat_sound_play(SOUND_MILITARY_BGM_ID);
  }
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
  units_combat_music_sting();

  /*
   * FUN_5fef_1b0e / FUN_157e: attacker 004a(mode=1); defender 015e + peels.
   * Cite: viceroy_unpacked.c FUN_157e_004a / 015e / 5fef_1b0e; combat_strength.c.
   */
  ColonizeCombatStrengthCtx sctx = units_combat_strength_ctx(col1);
  sctx.units = pool;
  ColonizeCombatEngageResult er;
  combat_land_engage(&sctx, attacker_id, defender_id, &er);

  ColonizeCombatEngagement eng;
  memset(&eng, 0, sizeof(eng));
  eng.attacker_id = attacker_id;
  eng.defender_id = defender_id;
  eng.is_naval = false;
  eng.atk_strength = er.atk_strength;
  eng.def_strength = er.def_strength;
  eng.atk_flags = er.atk_flags;
  eng.def_flags = er.def_flags;

  /* Combat Analysis before roll — strengths known, outcome not yet decided. */
  units_combat_maybe_present_analysis(col1, &eng, atk->nation_id, def->nation_id);

  const int total = eng.atk_strength + eng.def_strength;
  if (er.force_defender_wins) {
    eng.atk_wins = false;
    eng.roll = eng.atk_strength + 1;
  } else if (total <= 0) {
    eng.atk_wins = true;
    eng.roll = 0;
  } else if (!rng) {
    eng.atk_wins = eng.atk_strength >= eng.def_strength;
    eng.roll = eng.atk_wins ? eng.atk_strength : eng.atk_strength + 1;
  } else {
    eng.roll = dos_rng_range(rng, 1, total);
    eng.atk_wins = eng.roll <= eng.atk_strength;
  }

  const int ambush = (er.atk_flags.flags & COMBAT_FLAG_AMBUSH) != 0;

  if (eng.atk_wins) {
    const int def_x = def->x;
    const int def_y = def->y;
    const int def_nation = def->nation_id;
    const int atk_nation = atk->nation_id;
    /*
     * Treasure capture (FUN_5fef_1908): LE16 gold from unit. Human → ransom
     * Accept/Refuse CHOICE before credit; AI → silent full credit.
     */
    if (col1 && atk_nation >= 0 && atk_nation <= 3 && dt->name[0] &&
        strstr(dt->name, "Treasure") != NULL) {
      const unsigned lo = (unsigned)(def->hold_goods_amount[0] & 0xff);
      const unsigned hi = (unsigned)(def->hold_goods_amount[1] & 0xff);
      const int loot_gold = (int)(lo | (hi << 8));
      if (loot_gold > 0) {
        const int human = units_combat_human_involved(col1, atk_nation, def_nation);
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(col1, def_nation);
        tok.string1 = units_combat_nation_label(col1, atk_nation);
        tok.number0 = loot_gold;
        tok.has_number0 = true;
        if (human && g_units_combat_popups) {
          char body[AI_POPUP_BODY_LEN];
          if (g_units_combat_game_txt) {
            popup_msg_fill(
              g_units_combat_game_txt, "LOOTCAPTURE", &tok, "Treasure captured.", body, sizeof(body)
            );
          } else {
            snprintf(body, sizeof(body), "Treasure worth %d — Accept ransom?", loot_gold);
          }
          const char* choices[2] = {"Refuse", "Accept"};
          const int ids[2] = {0, 1};
          (void)ai_popup_enqueue_choice_ctx(
            g_units_combat_popups,
            AI_POPUP_TAG_COMBAT_RANSOM,
            atk_nation,
            def_nation,
            loot_gold,
            NULL,
            body,
            choices,
            ids,
            2
          );
        } else {
          ColonizeCol1Save* mut = (ColonizeCol1Save*)col1;
          const uint32_t g = mut->nation[atk_nation].gold;
          const uint32_t add = (uint32_t)loot_gold;
          mut->nation[atk_nation].gold = g > UINT32_MAX - add ? UINT32_MAX : g + add;
          if (human) {
            units_combat_enqueue_tok(
              AI_POPUP_TAG_COMBAT_LOOT,
              "LOOTCAPTURE",
              atk_nation,
              def_nation,
              loot_gold,
              &tok,
              "Treasure captured."
            );
          }
        }
      }
    }
    units_combat_outcome_popups(
      pool, attacker_id, defender_id, 1, atk_nation, def_nation, 0, ambush, col1
    );
    (void)units_apply_land_loss_outcome(pool, defender_id, attacker_id, col1, 1);
    units_sweep_stack_after_loss(pool, def_x, def_y, def_nation, attacker_id, defender_id, col1);
    atk = units_get(pool, attacker_id);
    if (atk) {
      units_washington_promote_on_win(pool, atk, col1);
      if (units_chance_promote_on_win(
            pool, atk, col1, eng.atk_strength, eng.def_strength, eng.roll, rng
          )) {
        /* promoted */
      }
    }
    /*
     * Village destroy / pop drain is NOT "no Brave left on tile". DOS only
     * drains dwelling population when the empty-village temp Brave arm wins
     * (units_finish_village_temp_defender). Killing a map Brave on the tile
     * leaves the dwelling intact. Cite: FUN_5fef_1b0e bVar28 path.
     */
    g_units_last_combat = 1;
    return true;
  }
  units_combat_outcome_popups(
    pool, defender_id, attacker_id, 0, atk->nation_id, def->nation_id, 0, ambush, col1
  );
  {
    const int atk_x = atk->x;
    const int atk_y = atk->y;
    const int atk_nation = atk->nation_id;
    (void)units_apply_land_loss_outcome(pool, attacker_id, defender_id, col1, 1);
    units_sweep_stack_after_loss(pool, atk_x, atk_y, atk_nation, defender_id, attacker_id, col1);
  }
  def = units_get(pool, defender_id);
  if (def) {
    units_washington_promote_on_win(pool, def, col1);
    (void)units_chance_promote_on_win(
      pool, def, col1, eng.atk_strength, eng.def_strength, eng.roll, rng
    );
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
  units_combat_music_sting();

  /* FUN_157e_004a + 1b0e difficulty peels for both sides. */
  ColonizeCombatStrengthCtx sctx = units_combat_strength_ctx(col1);
  sctx.units = pool;
  ColonizeCombatEngageResult er;
  combat_naval_engage(&sctx, attacker_id, defender_id, &er);

  ColonizeCombatEngagement eng;
  memset(&eng, 0, sizeof(eng));
  eng.attacker_id = attacker_id;
  eng.defender_id = defender_id;
  eng.is_naval = true;
  eng.atk_strength = er.atk_strength;
  eng.def_strength = er.def_strength;
  eng.atk_flags = er.atk_flags;
  eng.def_flags = er.def_flags;

  /* Combat Analysis before roll — strengths known, outcome not yet decided. */
  units_combat_maybe_present_analysis(col1, &eng, atk->nation_id, def->nation_id);

  const int total = eng.atk_strength + eng.def_strength;
  if (total <= 0) {
    eng.atk_wins = true;
    eng.roll = 0;
  } else if (!rng) {
    eng.atk_wins = eng.atk_strength >= eng.def_strength;
    eng.roll = eng.atk_wins ? eng.atk_strength : eng.atk_strength + 1;
  } else {
    eng.roll = dos_rng_range(rng, 1, total);
    eng.atk_wins = eng.roll <= eng.atk_strength;
  }

  if (eng.atk_wins) {
    units_combat_outcome_popups(
      pool, attacker_id, defender_id, 1, atk->nation_id, def->nation_id, 1, 0, col1
    );
    {
      const ColonizeUnitType* wt = units_type(pool, atk->type_index);
      const int is_priv = wt && wt->name[0] && strstr(wt->name, "Privateer") != NULL;
      const int human = units_combat_human_involved(col1, atk->nation_id, def->nation_id);
      if (human) {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = dt && dt->name[0] ? dt->name : "Ship";
        if (is_priv) {
          /*
           * Privateer prize — not Crown. GAME.TXT @SEIZURE* is Royal Navy /
           * Army wording; use Privateer fallback body with same tag.
           */
          char body[AI_POPUP_BODY_LEN];
          snprintf(
            body,
            sizeof(body),
            "%s captured at sea by a Privateer!",
            tok.string0
          );
          if (g_units_combat_popups) {
            ai_popup_enqueue_ok_ctx(
              g_units_combat_popups,
              AI_POPUP_TAG_COMBAT_SEIZURE,
              atk->nation_id,
              def->nation_id,
              0,
              "Seizure",
              body
            );
          }
        } else {
          /* Crown / warship seizure → Royal Navy @SEIZURESEA. */
          units_combat_enqueue_tok(
            AI_POPUP_TAG_COMBAT_SEIZURE,
            "SEIZURESEA",
            atk->nation_id,
            def->nation_id,
            0,
            &tok,
            "Ship seized at sea by the Royal Navy."
          );
        }
      }
    }
    (void)units_apply_naval_loss_outcome(
      pool, defender_id, attacker_id, eng.def_strength, eng.atk_strength, 1, col1
    );
    g_units_last_combat = 1;
    return true;
  }
  units_combat_outcome_popups(
    pool, defender_id, attacker_id, 0, atk->nation_id, def->nation_id, 1, 0, col1
  );
  (void)units_apply_naval_loss_outcome(
    pool, attacker_id, defender_id, eng.atk_strength, eng.def_strength, 1, col1
  );
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
    /*
     * Mirror naval loss (FUN_5fef_1b0e ship peel): close fight + undamaged →
     * bit7 damage + MP drain; else sink. Cite: coastal_fort_fire.md; units_apply_naval_loss.
     */
    const int close = defense * 2 > attack_str && attack_str > 0;
    if (close && (def->col1_unknown15 & 0x80u) == 0) {
      def->col1_unknown15 |= 0x80u;
      def->moves_left = 0;
      {
        const int thresh = dt->defense > 0 ? dt->defense : 4;
        if (def->turns_worked < thresh) {
          def->turns_worked = (uint8_t)thresh;
        }
      }
      return false; /* ship survives damaged */
    }
    units_despawn(pool, defender_id);
    return true;
  }
  /*
   * Fort miss: surviving ship loses remaining MP. Damaged bit7 only on fort hit
   * (damage-not-sink above). Cite: coastal_fort_fire.md.
   */
  def->moves_left = 0;
  return false;
}

int units_coastal_fort_fire_pulse(
  ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng,
  int human_nation,
  char* status,
  size_t status_size
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
        const ColonizeUnit* before = units_get(units, targets[t]);
        const int ship_nation = before ? before->nation_id : -1;
        const int human_chrome =
          status && status_size > 0 &&
          (col->nation_id == human_nation || ship_nation == human_nation);
        if (units_fort_vs_ship(units, atk, targets[t], rng, col1)) {
          sunk++;
          if (human_chrome) {
            snprintf(status, status_size, "Coastal fort sank a ship.");
          }
        } else if (human_chrome) {
          snprintf(status, status_size, "Coastal fort slowed a ship.");
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

static int units_colony_plunder_stock_sum(const ColonizeColony* col) {
  if (!col) {
    return 0;
  }
  int sum = 0;
  for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
    if (col->stock[i] > 0) {
      sum += col->stock[i];
    }
  }
  return sum;
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
  const int plunder = units_colony_plunder_stock_sum(col);
  const int old_nat = col->nation_id;
  ColonizeColony snap = *col;
  if (!colonies_capture(colonies, cid, u->nation_id)) {
    return;
  }
  (void)old_nat;
  units_combat_notify_colony_captured(g_units_ff_col1, &snap, u->nation_id, plunder);
}

/* Boarding helpers are defined later; enter_probe needs the embark probe. */
int units_find_boardable_ship(const ColonizeUnitPool* pool, int x, int y, int nation_id);

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
      /*
       * Native village (HAS_CITY, no Euro colony): ship abort path in
       * FUN_4d56_4528 — not landfall. Cite: indian_settlement_4528.md.
       */
      if (map->layer2) {
        const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
        if (idx < (size_t)map->width * (size_t)map->height &&
            (map->layer2[idx] & MAP_OCCUPANCY_HAS_CITY) != 0) {
          const int cid = colonies ? colonies_id_at(colonies, x, y) : -1;
          if (cid < 0) {
            g_units_last_enter_reason = COLONIZE_ENTER_VILLAGE_SHIP;
            return g_units_last_enter_reason;
          }
        }
      }
      g_units_last_enter_reason = COLONIZE_ENTER_LANDFALL;
      return g_units_last_enter_reason;
    }
    g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
    return g_units_last_enter_reason;
  }

  if (!land) {
    /*
     * Land → ocean/HS: embark if own ship on dest has room (FUN_4720_015c /
     * 0006). Otherwise domain deny.
     */
    if (mover_nation >= 0 && units_find_boardable_ship(pool, x, y, mover_nation) >= 0) {
      g_units_last_enter_reason = COLONIZE_ENTER_BOARD;
      return g_units_last_enter_reason;
    }
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

  /*
   * Village Attack empty tile (FUN_5fef_1b0e): spawn a temporary Brave from
   * dwelling stocks — do not drag nearby map Braves. Cite: 1b0e local_8a arm.
   */
  int village_temp = -1;
  int village_nation = -1;
  if (g_units_ff_col1 && unit->nation_id >= 0 && unit->nation_id <= 3) {
    village_nation = units_tribe_nation_at(g_units_ff_col1, dest_x, dest_y);
    if (village_nation >= 4) {
      village_temp = units_spawn_village_temp_defender(
        pool, g_units_ff_col1, dest_x, dest_y, village_nation, unit_id
      );
    }
  }

  const ColonizeEnterReason reason =
    units_enter_probe(pool, unit->type_index, map, dest_x, dest_y, unit_id, colonies);
  g_units_last_enter_reason = reason;

  if (reason == COLONIZE_ENTER_BOARD) {
    if (village_temp >= 0) {
      units_despawn(pool, village_temp);
      village_temp = -1;
    }
    const int ship_id = units_find_boardable_ship(pool, dest_x, dest_y, unit->nation_id);
    if (ship_id < 0) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
      return false;
    }
    if (unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
        unit->orders == UNITS_ORDER_FORTIFIED) {
      unit->orders = UNITS_ORDER_NONE;
    }
    const int ox = unit->x;
    const int oy = unit->y;
    if (!units_board(pool, unit_id, ship_id)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
      return false;
    }
    units_occupancy_refresh_tile(pool, ox, oy, unit_id);
    units_occupancy_refresh_tile(pool, dest_x, dest_y, -1);
    g_units_last_enter_reason = COLONIZE_ENTER_BOARD;
    return true;
  }

  if (reason == COLONIZE_ENTER_BOUNCE_FOREIGN || reason == COLONIZE_ENTER_BOUNCE_PEACE ||
      reason == COLONIZE_ENTER_BLOCKED_DOMAIN || reason == COLONIZE_ENTER_BLOCKED_EDGE ||
      reason == COLONIZE_ENTER_BLOCKED_HS_SAIL || reason == COLONIZE_ENTER_VILLAGE_ILLEGAL ||
      reason == COLONIZE_ENTER_LANDFALL || reason == COLONIZE_ENTER_VILLAGE_SHIP ||
      reason == COLONIZE_ENTER_NO_MP || reason == COLONIZE_ENTER_BLOCKED) {
    if (village_temp >= 0) {
      units_despawn(pool, village_temp);
    }
    return false;
  }

  if (reason == COLONIZE_ENTER_COMBAT_LAND || reason == COLONIZE_ENTER_COMBAT_NAVAL) {
    const int foe = units_best_defender_at(
      pool, g_units_ff_col1, dest_x, dest_y, unit_id, unit_id
    );
    if (foe < 0) {
      if (village_temp >= 0) {
        units_despawn(pool, village_temp);
      }
      return false;
    }
    bool won = false;
    if (reason == COLONIZE_ENTER_COMBAT_NAVAL) {
      won = units_resolve_naval_combat_ff(pool, unit_id, foe, rng, g_units_ff_col1);
    } else {
      won = units_resolve_land_combat_ff(pool, unit_id, foe, rng, g_units_ff_col1);
    }
    if (village_temp >= 0 && foe == village_temp) {
      ColonizeCol1Save* mut = (ColonizeCol1Save*)g_units_ff_col1;
      units_finish_village_temp_defender(
        pool,
        mut,
        g_units_fallout_map ? g_units_fallout_map : (ColonizeWorldMap*)map,
        village_temp,
        won ? 1 : 0,
        unit->nation_id,
        dest_x,
        dest_y,
        rng
      );
      village_temp = -1;
    } else if (village_temp >= 0) {
      units_despawn(pool, village_temp);
      village_temp = -1;
    }
    if (!won) {
      return false;
    }
    unit = units_get(pool, unit_id);
    if (!unit) {
      return false;
    }
    /*
     * Native village raid: fight from the adjacent tile and stay there (DOS
     * FUN_4d56_4528 contact). Charge MP as if the step were spent; do not enter.
     */
    if (village_nation >= 4 && unit->nation_id >= 0 && unit->nation_id <= 3) {
      const int cost = units_move_cost(pool, unit_id, map, dest_x, dest_y);
      const int remaining = unit->moves_left;
      const ColonizeUnitType* type = units_type(pool, unit->type_index);
      const int max_mp = type && type->movement > 0 ? type->movement : 1;
      if (cost > remaining && remaining < max_mp && rng) {
        const int roll = dos_rng_range(rng, 1, cost > 0 ? cost : 1);
        if (roll > remaining) {
          unit->moves_left = 0;
          return false;
        }
      }
      unit->moves_left = remaining - cost;
      if (unit->moves_left < 0) {
        unit->moves_left = 0;
      }
      if (unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
          unit->orders == UNITS_ORDER_FORTIFIED) {
        unit->orders = UNITS_ORDER_NONE;
      }
      g_units_last_enter_reason = COLONIZE_ENTER_OK;
      return true;
    }
    /* After win, dest must be clear of foreigners for enter. */
    if (units_foreign_at(pool, dest_x, dest_y, unit_id, unit->nation_id) >= 0) {
      return false;
    }
  } else {
    if (village_temp >= 0) {
      units_despawn(pool, village_temp);
      village_temp = -1;
    }
    if (colonies) {
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

  /* Sentry land units on the departure tile auto-board (colony / ocean stack). */
  if (units_is_sea(pool, unit_id)) {
    (void)units_board_sentries_from_tile(pool, unit_id, ox, oy);
  }

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
  if (g_units_move_watch && units_is_on_map(unit)) {
    g_units_move_watch(
      g_units_move_watch_user,
      pool,
      map,
      colonies,
      unit_id,
      ox,
      oy,
      unit->x,
      unit->y
    );
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
  u->follow_unit_id = -1;
}

bool units_orders_skip_turn(const ColonizeUnit* unit) {
  if (!unit || !unit->active) {
    return false;
  }
  return unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFIED ||
         unit->orders == UNITS_ORDER_CLEAR_PLOW || unit->orders == UNITS_ORDER_BUILD_ROAD;
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

/* DOS dir8 table (DS:0xb4/0xbe), also used project-wide; index^4 = reverse. */
static int units_dir8_index(int dx, int dy) {
  static const int k_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int k_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int i = 0; i < 8; ++i) {
    if (k_dx[i] == dx && k_dy[i] == dy) {
      return i;
    }
  }
  return -1;
}

/*
 * FUN_6662_0f74's own last-taken-step tracker (unit+0x314f), used only for
 * the anti-backtrack wiggle-retry below. Deliberately NOT `ColonizeUnit`'s
 * `last_dir` field — that one is already live-owned by ai.c's Indian native
 * Brave engine (`ai_native_pick_dir`); writing it here would silently
 * corrupt that engine's own bookkeeping for any unit that also takes a
 * goto step. Same shadow-array pattern ai_euro.c already uses for its own
 * Euro `last_dir` equivalent (`s_euro_last_dir`), for the same reason.
 * Zero-initialized (== dir 0/North): a unit's first goto step gets a
 * harmless, self-correcting small bias instead of "no history" — not
 * worth a separate reset hook, matching `s_euro_last_dir`'s own precedent.
 */
static int8_t s_units_goto_last_dir[COLONIZE_UNITS_MAX];

static bool units_greedy_next_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng,
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
  /*
   * FUN_6662_0f74 tail (anti-backtrack wiggle): when the scored fallback's
   * best pick is the exact reverse of the unit's last-taken step, DOS
   * doesn't take it — it rerolls up to 8 random directions instead,
   * accepting the first legal/affordable one (0f74 gates this on
   * unit+0x314c=='\v'/goto-pending; Linux has no live copy of that cache
   * per move_scoring_20e6_full.md, so gate on the live equivalent —
   * already following a goto here). Avoids visible ping-pong between two
   * tiles. Cite: euro_unit_act.md T1.8.
   */
  if (rng != NULL && unit_id >= 0 && unit_id < COLONIZE_UNITS_MAX &&
      units_dir8_index(best_x - u->x, best_y - u->y) ==
        (s_units_goto_last_dir[unit_id] ^ 4) &&
      units_orders_follow_goto(u->orders)) {
    static const int k_wig_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int k_wig_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    int wig_x = -1;
    int wig_y = -1;
    for (int tries = 0; tries < 8 && wig_x < 0; ++tries) {
      const int d = dos_rng_range(rng, 0, 7);
      const int nx = u->x + k_wig_dx[d];
      const int ny = u->y + k_wig_dy[d];
      if (!units_can_enter(pool, u->type_index, map, nx, ny, unit_id, colonies)) {
        continue;
      }
      if (!units_can_afford_move_cost(
            pool, unit_id, units_move_cost(pool, unit_id, map, nx, ny)
          )) {
        continue;
      }
      wig_x = nx;
      wig_y = ny;
    }
    if (wig_x < 0) {
      /* All 8 rerolls rejected: DOS falls through to total failure here. */
      return false;
    }
    best_x = wig_x;
    best_y = wig_y;
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
  ColonizeDosRng* rng,
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
    return units_greedy_next_step(pool, unit_id, map, colonies, rng, gx, gy, out_x, out_y);
  }

  /* Near: both axes within 6 — destination cost flood (FUN_6662_00f2). */
  if (adx <= 6 && ady <= 6) {
    if (units_flood_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y)) {
      return true;
    }
    return units_greedy_next_step(pool, unit_id, map, colonies, rng, gx, gy, out_x, out_y);
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
  return units_greedy_next_step(pool, unit_id, map, colonies, rng, gx, gy, out_x, out_y);
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
    if (u->orders != UNITS_ORDER_TRADE_ROUTE) {
      units_clear_orders(pool, unit_id);
    }
    return false;
  }
  if (u->x == gx && u->y == gy) {
    /* TRADE_ROUTE: stay ordered at stop so caller can advance to next dest. */
    if (u->orders != UNITS_ORDER_TRADE_ROUTE) {
      units_clear_orders(pool, unit_id);
    }
    return false;
  }
  if (u->moves_left <= 0) {
    return false;
  }
  const int ox = u->x;
  const int oy = u->y;
  int nx = -1;
  int ny = -1;
  if (!units_next_goto_step(pool, unit_id, map, colonies, rng, &nx, &ny)) {
    return false;
  }
  if (!units_try_move(pool, unit_id, map, nx, ny, colonies, rng)) {
    return false;
  }
  u = units_get(pool, unit_id);
  if (u) {
    /* FUN_6662_0f74's own tail writes unit+0x314f (last_dir) on every step
     * it commits — feeds the anti-backtrack wiggle check above. Tracked in
     * s_units_goto_last_dir, not ColonizeUnit.last_dir (see that array's
     * own header comment for why). */
    if (unit_id >= 0 && unit_id < COLONIZE_UNITS_MAX) {
      const int d = units_dir8_index(nx - ox, ny - oy);
      if (d >= 0) {
        s_units_goto_last_dir[unit_id] = (int8_t)d;
      }
    }
    if (u->x == gx && u->y == gy && u->orders != UNITS_ORDER_TRADE_ROUTE) {
      units_clear_orders(pool, unit_id);
    }
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

/*
 * DOS FUN_479b_01a6/0526: both clear/plow and road share the same
 * DS:0x2f78 threshold byte (offset +2 of the terrain-class record) +2.
 * Hardy Pioneer (profession 20) halves. Cite: viceroy_unpacked
 * FUN_479b_01a6/0526; live capture 2026-08-20 (was approximated with the
 * move-cost byte, offset +0, before the real +2 byte was captured).
 */
static int units_pioneer_work_needed(const ColonizeUnit* u, const ColonizeWorldMap* map, bool road) {
  (void)road;
  int needed = map_dos_terr_pioneer_threshold_byte(map_dos_terr_class_at(map, u->x, u->y)) + 2;
  if (u->profession == UNITS_JOB_PIONEER) {
    needed >>= 1;
  }
  if (needed < 1) {
    needed = 1;
  }
  return needed;
}

static bool units_pioneer_tile_can_clear_or_plow(
  const ColonizeWorldMap* map,
  int x,
  int y,
  bool* out_clearing
) {
  if (!map_tile_is_land(map, x, y) || map_tile_is_high_seas(map, x, y)) {
    return false;
  }
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  if (pedia == 24 || pedia == 27) {
    return false; /* Arctic / mountains */
  }
  if (pedia >= 8 && pedia <= 23) {
    if (out_clearing) {
      *out_clearing = true;
    }
    return true;
  }
  if (map_tile_is_plowed(map, x, y)) {
    return false;
  }
  if (out_clearing) {
    *out_clearing = false;
  }
  return true;
}

/* FUN_479b_0158: cargo_hold[5] / tools −20; type→Colonist when <20. */
static bool units_pioneer_wear_tools(ColonizeUnitPool* pool, ColonizeUnit* u) {
  if (u->tools >= UNITS_PIONEER_TOOL_COST) {
    u->tools -= UNITS_PIONEER_TOOL_COST;
  } else {
    u->tools = 0;
  }
  if (u->tools < UNITS_PIONEER_TOOL_COST) {
    u->tools = 0;
    u->orders = UNITS_ORDER_NONE;
    if (pool) {
      const int colonist = units_find_type(pool, "Colonists");
      if (colonist >= 0) {
        u->type_index = colonist;
      }
    }
    return true;
  }
  return false;
}

/* @USEDUPTOOLS after demotion (human / unset-human only). */
static void units_pioneer_emit_useduptools(
  const ColonizeUnit* u,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (err && err_size) {
    snprintf(err, err_size, "Tools used up — now a colonist");
  }
  if (!ai_popups || !u) {
    return;
  }
  if (g_units_combat_human_nation >= 0 && u->nation_id != g_units_combat_human_nation) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    messages,
    "USEDUPTOOLS",
    NULL,
    err && err[0] ? err : "Tools used up.",
    body,
    sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

/* Order-gate OK chrome (human / unset-human). */
static void units_pioneer_emit_order_gate(
  const ColonizeUnit* u,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages,
  const char* section,
  const char* fallback
) {
  if (!ai_popups || !section) {
    return;
  }
  if (u && g_units_combat_human_nation >= 0 && u->nation_id != g_units_combat_human_nation) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(messages, section, NULL, fallback ? fallback : section, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

bool units_pioneer_work_tick(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !map || !u->active || !units_is_on_map(u)) {
    return false;
  }
  const bool road = (u->orders == UNITS_ORDER_BUILD_ROAD);
  const bool clear_plow = (u->orders == UNITS_ORDER_CLEAR_PLOW);
  if (!road && !clear_plow) {
    return false;
  }
  if (!units_is_pioneer(pool, unit_id) || u->tools < UNITS_PIONEER_TOOL_COST) {
    u->orders = UNITS_ORDER_NONE;
    u->turns_worked = 0;
    if (err && err_size) {
      snprintf(err, err_size, "Need tools");
    }
    return false;
  }

  if (road) {
    if (!map_tile_is_land(map, u->x, u->y) || map_tile_is_high_seas(map, u->x, u->y) ||
        map_tile_has_road(map, u->x, u->y)) {
      u->orders = UNITS_ORDER_NONE;
      u->turns_worked = 0;
      if (err && err_size) {
        snprintf(err, err_size, "Cannot build road here");
      }
      return false;
    }
  } else {
    bool clearing = false;
    if (!units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, &clearing)) {
      u->orders = UNITS_ORDER_NONE;
      u->turns_worked = 0;
      if (err && err_size) {
        snprintf(err, err_size, "Cannot plow here");
      }
      return false;
    }
    (void)clearing;
  }

  /* FUN_281f_0934 stand-in: exhaust MP for this act. */
  u->moves_left = 0;
  if (u->turns_worked < 255) {
    u->turns_worked++;
  }
  const int needed = units_pioneer_work_needed(u, map, road);
  if (u->turns_worked < needed) {
    if (err && err_size) {
      if (road) {
        snprintf(err, err_size, "Building road (%d/%d)", u->turns_worked, needed);
      } else {
        bool clearing = false;
        (void)units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, &clearing);
        snprintf(
          err,
          err_size,
          clearing ? "Clearing forest (%d/%d)" : "Plowing (%d/%d)",
          u->turns_worked,
          needed
        );
      }
    }
    return true;
  }

  u->turns_worked = 0;
  u->orders = UNITS_ORDER_NONE;
  if (road) {
    map_tile_set_road(map, u->x, u->y, true);
    const bool demoted = units_pioneer_wear_tools(pool, u);
    /*
     * FUN_479b_0526 road completion: nearest same-nation colony (no radius
     * limit — DOS FUN_281f_0614(x,y,nation,0xffff)) gets a flat
     * +10 hammers_purchased. Mirrors the already-ported case-8 clear-forest
     * lumber grant's "nearest own colony" pattern (units_pioneer_work_tick
     * clearing branch below).
     */
    if (colonies) {
      ColonizeColony* near_road = NULL;
      int best_md_road = -1;
      for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
        ColonizeColony* c = &colonies->colonies[i];
        if (!c->active || c->nation_id != u->nation_id) {
          continue;
        }
        const int md = abs(c->x - u->x) + abs(c->y - u->y);
        if (best_md_road < 0 || md < best_md_road) {
          best_md_road = md;
          near_road = c;
        }
      }
      if (near_road) {
        int next = (int)near_road->hammers_purchased + 10;
        if (next > 65535) {
          next = 65535;
        }
        near_road->hammers_purchased = (uint16_t)next;
      }
    }
    if (demoted) {
      units_pioneer_emit_useduptools(u, err, err_size, ai_popups, messages);
    } else if (err && err_size) {
      snprintf(
        err,
        err_size,
        "Road built (-%d tools, %d left)",
        UNITS_PIONEER_TOOL_COST,
        u->tools
      );
    }
  } else {
    bool clearing = false;
    (void)units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, &clearing);
    if (clearing) {
      map_tile_clear_forest(map, u->x, u->y);
      const bool demoted = units_pioneer_wear_tools(pool, u);
      /*
       * FUN_479b_01a6 clear: lumber → nearest same-nation colony + @CLEARCUT.
       * Real formula (was flat 20, PARKED, before the terrain-table byte was
       * captured 2026-08-20): scale = terrain +8 byte if colony has a Lumber
       * Mill, else 1 (a floor, not a gate) — add = scale*20, doubled for
       * Hardy Pioneer, clamped to warehouse room. Cite: viceroy_unpacked
       * FUN_479b_01a6, live capture 2026-08-20.
       */
      int lumber_add = 0;
      ColonizeColony* near = NULL;
      if (colonies) {
        int best_md = -1;
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          ColonizeColony* c = &colonies->colonies[i];
          if (!c->active || c->nation_id != u->nation_id) {
            continue;
          }
          const int md = abs(c->x - u->x) + abs(c->y - u->y);
          if (best_md < 0 || md < best_md) {
            best_md = md;
            near = c;
          }
        }
        if (near) {
          const int lumber_mill = colonies_find_building(colonies, "Lumber Mill");
          const bool has_mill = lumber_mill >= 0 && near->has_building[lumber_mill];
          int scale = map_dos_terr_lumber_reward_byte(map_dos_terr_class_at(map, u->x, u->y));
          if (!has_mill) {
            scale = 1;
          }
          int potential = scale * 20;
          if (u->profession == UNITS_JOB_PIONEER) {
            potential *= 2;
          }
          const int cap = colonies_warehouse_capacity(colonies, near, COLONIZE_CARGO_LUMBER);
          int room = cap - near->stock[COLONIZE_CARGO_LUMBER];
          if (room < 0) {
            room = 0;
          }
          lumber_add = potential;
          if (lumber_add > room) {
            lumber_add = room;
          }
          if (lumber_add > 0) {
            int next = near->stock[COLONIZE_CARGO_LUMBER] + lumber_add;
            if (next < 0) {
              next = 0;
            }
            if (next > 65535) {
              next = 65535;
            }
            near->stock[COLONIZE_CARGO_LUMBER] = next;
          }
        }
      }
      if (err && err_size) {
        if (lumber_add > 0 && near && near->name[0]) {
          snprintf(
            err,
            err_size,
            "Forest cleared (+%d lumber to %s)",
            lumber_add,
            near->name
          );
        } else if (lumber_add > 0) {
          snprintf(err, err_size, "Forest cleared (+%d lumber)", lumber_add);
        } else {
          snprintf(
            err,
            err_size,
            "Forest cleared (-%d tools, %d left)",
            UNITS_PIONEER_TOOL_COST,
            u->tools
          );
        }
      }
      if (lumber_add > 0 && near && ai_popups &&
          (g_units_combat_human_nation < 0 || u->nation_id == g_units_combat_human_nation)) {
        char body[AI_POPUP_BODY_LEN];
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = near->name[0] ? near->name : "colony";
        tok.number0 = lumber_add;
        tok.has_number0 = true;
        popup_msg_fill(
          messages,
          "CLEARCUT",
          &tok,
          err && err[0] ? err : "Forest cleared.",
          body,
          sizeof(body)
        );
        ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
      /* @DEFOREST tip when clear is near an owned colony (even if lumber=0). */
      if (near && ai_popups &&
          (g_units_combat_human_nation < 0 || u->nation_id == g_units_combat_human_nation)) {
        const char* cname = near->name[0] ? near->name : "colony";
        char body[AI_POPUP_BODY_LEN];
        char fallback[96];
        snprintf(fallback, sizeof(fallback), "Deforestation near %s.", cname);
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = cname;
        popup_msg_fill(messages, "DEFOREST", &tok, fallback, body, sizeof(body));
        ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
      }
      if (demoted) {
        units_pioneer_emit_useduptools(u, err, err_size, ai_popups, messages);
      }
    } else {
      map_tile_set_plowed(map, u->x, u->y, true);
      const bool demoted = units_pioneer_wear_tools(pool, u);
      if (demoted) {
        units_pioneer_emit_useduptools(u, err, err_size, ai_popups, messages);
      } else if (err && err_size) {
        snprintf(
          err,
          err_size,
          "Plowed (-%d tools, %d left)",
          UNITS_PIONEER_TOOL_COST,
          u->tools
        );
      }
    }
  }
  return true;
}

bool units_pioneer_plow(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !map || !units_is_pioneer(pool, unit_id)) {
    if (err && err_size) {
      snprintf(err, err_size, "Select a Pioneer");
    }
    units_pioneer_emit_order_gate(u, ai_popups, messages, "ONLYPIO", "Only pioneers can do that.");
    return false;
  }
  if (u->orders != UNITS_ORDER_CLEAR_PLOW && u->moves_left <= 0) {
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
  if (map_tile_is_plowed(map, u->x, u->y)) {
    if (err && err_size) {
      snprintf(err, err_size, "Already plowed");
    }
    units_pioneer_emit_order_gate(u, ai_popups, messages, "NOPLOW", "Already plowed.");
    return false;
  }
  if (!units_pioneer_tile_can_clear_or_plow(map, u->x, u->y, NULL)) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot plow here");
    }
    return false;
  }
  if (u->orders != UNITS_ORDER_CLEAR_PLOW) {
    u->turns_worked = 0;
    u->orders = UNITS_ORDER_CLEAR_PLOW;
  }
  return units_pioneer_work_tick(
    pool, unit_id, map, err, err_size, colonies, ai_popups, messages
  );
}

bool units_pioneer_road(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size,
  ColonizeColonyPool* colonies,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !map || !units_is_pioneer(pool, unit_id)) {
    if (err && err_size) {
      snprintf(err, err_size, "Select a Pioneer");
    }
    units_pioneer_emit_order_gate(u, ai_popups, messages, "ONLYPIO", "Only pioneers can do that.");
    return false;
  }
  if (u->orders != UNITS_ORDER_BUILD_ROAD && u->moves_left <= 0) {
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
    units_pioneer_emit_order_gate(u, ai_popups, messages, "NOROAD", "Already a road.");
    return false;
  }
  if (u->orders != UNITS_ORDER_BUILD_ROAD) {
    u->turns_worked = 0;
    u->orders = UNITS_ORDER_BUILD_ROAD;
  }
  return units_pioneer_work_tick(
    pool, unit_id, map, err, err_size, colonies, ai_popups, messages
  );
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

int units_find_boardable_ship(const ColonizeUnitPool* pool, int x, int y, int nation_id) {
  if (!pool || nation_id < 0) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* ship = &pool->units[i];
    if (!ship->active || ship->nation_id != nation_id) {
      continue;
    }
    if (!units_is_sea(pool, ship->id) || !units_is_on_map(ship)) {
      continue;
    }
    if (ship->x != x || ship->y != y) {
      continue;
    }
    const int cap = units_ship_capacity(pool, ship->id);
    if (cap > 0 && ship->cargo_count < cap) {
      return ship->id;
    }
  }
  return -1;
}

int units_board_sentries_from_tile(ColonizeUnitPool* pool, int ship_id, int x, int y) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!pool || !ship || !units_is_sea(pool, ship_id)) {
    return 0;
  }
  int boarded = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* land = &pool->units[i];
    if (!land->active || !units_is_on_map(land) || units_is_sea(pool, land->id)) {
      continue;
    }
    if (land->nation_id != ship->nation_id || land->x != x || land->y != y) {
      continue;
    }
    if (land->orders != UNITS_ORDER_SENTRY) {
      continue;
    }
    if (!units_board_stacked(pool, land->id, ship_id)) {
      break; /* full or reject — stop filling */
    }
    boarded++;
  }
  return boarded;
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
  /*
   * Shore-step MP (FUN_465b ADD). Aboard sentry often has moves_left==0 as a
   * skip-select flag while DOS spent is still 0 (full allotment) — restore
   * type movement for the charge only, then spend dest terrain cost. Never
   * leave a free full refill. Cite: 4720_015c; move_spent.c.
   */
  {
    const ColonizeUnitType* type = units_type(pool, pax->type_index);
    int remaining = pax->moves_left;
    if (remaining <= 0) {
      remaining = type && type->movement > 0 ? type->movement : 1;
    }
    int cost = map_move_cost_step(map, ship->x, ship->y, dest_x, dest_y);
    if (cost < 1) {
      cost = 1;
    }
    pax->moves_left = remaining > cost ? remaining - cost : 0;
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

/*
 * DOS FUN_4720_015c landfall pick: prefer cargo with remaining MP; else any
 * passenger. Aboard sentry uses moves_left=0 as "skip select" but DOS spent
 * is still 0 (full allotment) — they remain landfall-eligible.
 */
int units_first_landfall_cargo(const ColonizeUnitPool* pool, int ship_id) {
  const int ready = units_first_cargo_with_moves(pool, ship_id);
  if (ready >= 0) {
    return ready;
  }
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!ship || ship->cargo_count <= 0) {
    return -1;
  }
  return ship->cargo_ids[0];
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
  if (ut && strcmp(ut->name, "Colonists") == 0) {
    return "Free Colonist";
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

/*
 * FUN_48d3_0434: HS (0x1a) + empty or own nation.
 * FUN_48d3_048e: expand radius; for each ring scan horizontal then vertical
 * edges with ±radius offsets (decomp ~77836–77876).
 */
static bool units_hs_place_tile_ok(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int nation_id,
  int x,
  int y
) {
  if (!map || x < 0 || y < 0 || x >= (int)map->width || y >= (int)map->height) {
    return false;
  }
  if (!map_tile_is_high_seas(map, x, y)) {
    return false;
  }
  if (!pool) {
    return true;
  }
  const int id = units_id_at(pool, x, y);
  if (id < 0) {
    return true;
  }
  if (nation_id < 0) {
    return false;
  }
  const ColonizeUnit* u = units_get_const(pool, id);
  return u && u->nation_id == nation_id;
}

bool units_spiral_place_hs_near(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  const int max_dim = (int)map->width > (int)map->height ? (int)map->width : (int)map->height;
  for (int e = 0; e < max_dim; ++e) {
    /* Horizontal bands at y = start_y ± e, x from start_x-e .. start_x+e */
    for (int x = start_x - e; x <= start_x + e; ++x) {
      for (int si = 0; si < 2; ++si) {
        const int yoff = (si == 0) ? -e : e;
        const int y = start_y + yoff;
        if (units_hs_place_tile_ok(pool, map, nation_id, x, y)) {
          *out_x = x;
          *out_y = y;
          return true;
        }
      }
    }
    /* Vertical bands at x = start_x ± e, y from start_y-e .. start_y+e */
    for (int y = start_y - e; y <= start_y + e; ++y) {
      for (int si = 0; si < 2; ++si) {
        const int xoff = (si == 0) ? -e : e;
        const int x = start_x + xoff;
        if (units_hs_place_tile_ok(pool, map, nation_id, x, y)) {
          *out_x = x;
          *out_y = y;
          return true;
        }
      }
    }
  }
  return false;
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
