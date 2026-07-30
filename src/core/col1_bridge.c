#include "core/col1_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/diagnostics.h"

#define COL1_FAIL(err, err_size, ...)           \
  do {                                          \
    if ((err) && (err_size) > 0) {              \
      snprintf((err), (err_size), __VA_ARGS__); \
    }                                           \
    return false;                               \
  } while (0)

/*
 * COL1 tile byte (LSB-first bitfield):
 *   base:3 forest:1 special:1 hills:1 river:1 major:1
 * FreeCol / .MP terrain byte:
 *   bits0-4 terrain index; bits5-7 overlay (hills|river|major as 1|2|4)
 */
uint8_t col1_tile_to_mp_terrain(uint8_t col1_tile_byte) {
  const uint8_t base = (uint8_t)(col1_tile_byte & 7u);
  const bool forest = (col1_tile_byte & 0x08u) != 0;
  const bool special = (col1_tile_byte & 0x10u) != 0;
  const bool hills = (col1_tile_byte & 0x20u) != 0;
  const bool river = (col1_tile_byte & 0x40u) != 0;
  const bool major = (col1_tile_byte & 0x80u) != 0;

  uint8_t index = 0;
  if (special && forest) {
    if (base == 0) {
      index = 24; /* arctic */
    } else if (base == 1) {
      index = 25; /* ocean */
    } else {
      index = 26; /* sea lane */
    }
  } else if (forest) {
    index = (uint8_t)(8u + (base & 7u));
  } else {
    index = (uint8_t)(base & 7u);
  }

  uint8_t overlay = 0;
  if (hills) {
    overlay |= 1u;
  }
  if (river) {
    overlay |= 2u;
  }
  if (major) {
    overlay |= 4u;
  }
  return (uint8_t)((index & 0x1fu) | (uint8_t)(overlay << 5));
}

uint8_t col1_mp_terrain_to_tile(uint8_t mp_terrain_byte) {
  const uint8_t index = (uint8_t)(mp_terrain_byte & 0x1fu);
  const uint8_t overlay = (uint8_t)(mp_terrain_byte >> 5);
  uint8_t out = 0;
  if (index >= 24) {
    out = (uint8_t)(0x08u | 0x10u | ((index - 24u) & 7u));
  } else if (index >= 8) {
    out = (uint8_t)(0x08u | (index & 7u));
  } else {
    out = (uint8_t)(index & 7u);
  }
  if (overlay & 1u) {
    out |= 0x20u;
  }
  if (overlay & 2u) {
    out |= 0x40u;
  }
  if (overlay & 4u) {
    out |= 0x80u;
  }
  return out;
}

static bool col1_coord_is_europe(uint8_t x, uint8_t y) {
  return x >= 200 || y >= 200;
}

static int col1_find_human_nation(const ColonizeCol1Save* save) {
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
    if (save->player[i].control == 0) {
      return i;
    }
  }
  return 0;
}

static void col1_copy_name24(char* dst, size_t dst_size, const char* src24) {
  if (!dst || dst_size == 0) {
    return;
  }
  size_t n = 0;
  while (n < 23 && n + 1 < dst_size && src24[n] != '\0') {
    dst[n] = src24[n];
    n++;
  }
  dst[n] = '\0';
}

static void col1_apply_building_level(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const char* const* names,
  int name_count,
  unsigned level
) {
  if (level == 0) {
    return;
  }
  for (int i = 0; i < name_count && (unsigned)i < level; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX) {
      colony->has_building[idx] = true;
    }
  }
}

static void col1_apply_colony_buildings(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const ColonizeCol1Buildings* b
) {
  static const char* k_fort[] = {"Stockade", "Fort", "Fortress"};
  static const char* k_armory[] = {"Armory", "Magazine", "Arsenal"};
  static const char* k_docks[] = {"Docks", "Drydock", "Shipyard"};
  static const char* k_school[] = {"Schoolhouse", "College", "University"};
  static const char* k_warehouse[] = {"Warehouse", "Warehouse Expansion"};
  static const char* k_press[] = {"Printing Press", "Newspaper"};
  static const char* k_weaver[] = {"Weaver's House", "Weaver's Shop", "Textile Mill"};
  static const char* k_tobacco[] = {
    "Tobacconist's House", "Tobacconist's Shop", "Cigar Factory"
  };
  static const char* k_rum[] = {
    "Rum Distiller's House", "Rum Distiller's Shop", "Rum Factory"
  };
  static const char* k_fur[] = {
    "Fur Trader's House", "Fur Trading Post", "Fur Factory"
  };
  static const char* k_carpenter[] = {"Carpenter's Shop", "Lumber Mill"};
  static const char* k_church[] = {"Church", "Cathedral"};
  static const char* k_smith[] = {
    "Blacksmith's House", "Blacksmith's Shop", "Iron Works"
  };

  col1_apply_building_level(pool, colony, k_fort, 3, b->fortification);
  col1_apply_building_level(pool, colony, k_armory, 3, b->armory);
  col1_apply_building_level(pool, colony, k_docks, 3, b->docks);
  if (b->town_hall) {
    static const char* k_hall[] = {"Town Hall"};
    col1_apply_building_level(pool, colony, k_hall, 1, 1);
  }
  col1_apply_building_level(pool, colony, k_school, 3, b->schoolhouse);
  col1_apply_building_level(pool, colony, k_warehouse, 2, b->warehouse);
  if (b->stables) {
    static const char* k_stable[] = {"Stable"};
    col1_apply_building_level(pool, colony, k_stable, 1, 1);
  }
  if (b->custom_house) {
    static const char* k_custom[] = {"Custom House"};
    col1_apply_building_level(pool, colony, k_custom, 1, 1);
  }
  col1_apply_building_level(pool, colony, k_press, 2, b->printing_press);
  col1_apply_building_level(pool, colony, k_weaver, 3, b->weavers_house);
  col1_apply_building_level(pool, colony, k_tobacco, 3, b->tobacconists_house);
  col1_apply_building_level(pool, colony, k_rum, 3, b->rum_distillers_house);
  col1_apply_building_level(pool, colony, k_fur, 3, b->fur_traders_house);
  col1_apply_building_level(pool, colony, k_carpenter, 2, b->carpenters_shop);
  col1_apply_building_level(pool, colony, k_church, 2, b->church);
  col1_apply_building_level(pool, colony, k_smith, 3, b->blacksmiths_house);
}

static int col1_unit_type_to_runtime(const ColonizeUnitPool* units, uint8_t col1_type) {
  if (!units || units->type_count <= 0) {
    return -1;
  }
  if ((int)col1_type < units->type_count) {
    return (int)col1_type;
  }
  return 0;
}

static int col1_find_ship_root(const ColonizeCol1Unit* units, int count, int start) {
  int i = start;
  for (int guard = 0; guard < count + 2; ++guard) {
    if (i < 0 || i >= count) {
      return -1;
    }
    const uint8_t t = units[i].type;
    if (t >= 13 && t <= 18) {
      return i;
    }
    const int next = units[i].transport_chain.next_unit_idx;
    if (next < 0 || next == i) {
      return -1;
    }
    i = next;
  }
  return -1;
}

bool col1_bridge_init_template(
  ColonizeCol1Save* save,
  uint16_t map_w,
  uint16_t map_h,
  char* err,
  size_t err_size
) {
  if (!save) {
    COL1_FAIL(err, err_size, "null save");
  }
  col1_save_free(save);
  col1_save_init(save);
  memset(&save->head, 0, sizeof(save->head));
  memcpy(save->head.sig_colonize, COLONIZE_COL1_SIG, 8);
  save->head.sig_colonize[8] = '\0';
  save->head.map_size_x = map_w;
  save->head.map_size_y = map_h;
  save->head.year = 1492;
  save->head.turn = 0;
  save->head.colony_count = 0;
  save->head.unit_count = 0;
  save->head.tribe_count = 0;
  save->head.difficulty = 0;
  save->player[0].control = 0;
  snprintf(save->player[0].name, sizeof(save->player[0].name), "Governor");
  snprintf(save->player[0].country_name, sizeof(save->player[0].country_name), "New England");
  for (int i = 1; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
    save->player[i].control = 1;
  }
  if (!col1_save_alloc_sections(save, err, err_size)) {
    return false;
  }
  memset(save->map.path, 0xFF, save->map.tile_count); /* unvisited */
  return true;
}

bool col1_bridge_apply(
  const ColonizeCol1Save* save,
  ColonizeWorldMap* map,
  ColonizeUnitPool* units,
  ColonizeColonyPool* colonies,
  EuropeScreen* europe,
  ColonizeCol1BridgeResult* out,
  char* err,
  size_t err_size
) {
  if (!save || !map || !units || !colonies) {
    COL1_FAIL(err, err_size, "col1_bridge_apply bad args");
  }
  if (save->head.map_size_x == 0 || save->head.map_size_y == 0 ||
      save->head.map_size_x > 255 || save->head.map_size_y > 255) {
    COL1_FAIL(err, err_size, "unsupported map size %ux%u", save->head.map_size_x, save->head.map_size_y);
  }
  if (!save->map.tile || save->map.tile_count !=
                           (size_t)save->head.map_size_x * (size_t)save->head.map_size_y) {
    COL1_FAIL(err, err_size, "save map layers incomplete");
  }

  ColonizeCol1BridgeResult local;
  memset(&local, 0, sizeof(local));
  local.year = save->head.year;
  local.autumn = save->head.autumn;
  local.turn_number = save->head.turn;
  local.human_nation = col1_find_human_nation(save);
  local.cursor_x = save->stuff.viewport_x ? (int)save->stuff.viewport_x : (int)save->stuff.x;
  local.cursor_y = save->stuff.viewport_y ? (int)save->stuff.viewport_y : (int)save->stuff.y;

  if (!map_alloc(
        map,
        (uint8_t)save->head.map_size_x,
        (uint8_t)save->head.map_size_y,
        err,
        err_size
      )) {
    return false;
  }
  for (size_t i = 0; i < save->map.tile_count; ++i) {
    map->terrain[i] = col1_tile_to_mp_terrain(save->map.tile[i]);
    if (save->map.seen) {
      /* visibility nibble → simple fog: unseen if player bit clear */
      const uint8_t seen = save->map.seen[i];
      const bool visible = (seen & (uint8_t)(0x10u << local.human_nation)) != 0;
      map->layer3[i] = visible ? 0 : 1;
    }
  }

  /* Colonies — soft-reset actives, keep name/building catalogs. */
  {
    const int name_count = colonies->name_count;
    const int name_next = colonies->name_next;
    char names_backup[COLONIZE_COLONY_NAMES_MAX][COLONIZE_COLONY_NAME_MAX];
    ColonizeBuildingType buildings_backup[COLONIZE_BUILDING_TYPES_MAX];
    const int building_type_count = colonies->building_type_count;
    memcpy(names_backup, colonies->names, sizeof(names_backup));
    memcpy(buildings_backup, colonies->building_types, sizeof(buildings_backup));
    colonies_init(colonies);
    memcpy(colonies->names, names_backup, sizeof(names_backup));
    colonies->name_count = name_count;
    colonies->name_next = name_next;
    memcpy(colonies->building_types, buildings_backup, sizeof(buildings_backup));
    colonies->building_type_count = building_type_count;
  }

  for (int i = 0; i < (int)save->head.colony_count; ++i) {
    if (colonies->colony_count >= COLONIZE_COLONIES_MAX) {
      diag_warn("col1 import: truncating colonies at %d", COLONIZE_COLONIES_MAX);
      break;
    }
    const ColonizeCol1Colony* src = &save->colony[i];
    ColonizeColony* dst = &colonies->colonies[colonies->colony_count];
    memset(dst, 0, sizeof(*dst));
    dst->id = colonies->next_id++;
    dst->active = true;
    dst->x = src->x;
    dst->y = src->y;
    col1_copy_name24(dst->name, sizeof(dst->name), src->name);
    dst->population = src->population;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      dst->stock[c] = src->stock[c];
    }
    col1_apply_colony_buildings(colonies, dst, &src->buildings);
    const int pop = src->population > COLONIZE_COLONY_POP_MAX ? COLONIZE_COLONY_POP_MAX
                                                              : (int)src->population;
    for (int p = 0; p < pop; ++p) {
      ColonizeColonist* col = &dst->colonists[p];
      col->active = true;
      col->building_type = -1;
      int t = src->profession[p];
      if (t < 0 || t >= units->type_count) {
        t = 0;
      }
      col->unit_type_index = t;
      dst->colonist_count++;
    }
    dst->population = dst->colonist_count;
    colonies->colony_count++;
    local.imported_colonies++;
  }

  /* Units */
  units_reset(units);
  int* id_by_index = NULL;
  if (save->head.unit_count > 0) {
    id_by_index = calloc((size_t)save->head.unit_count, sizeof(int));
    if (!id_by_index) {
      COL1_FAIL(err, err_size, "oom unit index map");
    }
    for (int i = 0; i < (int)save->head.unit_count; ++i) {
      id_by_index[i] = -1;
    }
  }

  for (int i = 0; i < (int)save->head.unit_count; ++i) {
    const ColonizeCol1Unit* src = &save->unit[i];
    if (col1_coord_is_europe(src->x, src->y)) {
      local.skipped_europe_units++;
      if (europe && (src->nation_id == (uint8_t)local.human_nation) &&
          src->type >= 13 && src->type <= 18) {
        const int ti = col1_unit_type_to_runtime(units, src->type);
        const ColonizeUnitType* ut = units_type(units, ti);
        europe_harbor_push(europe, ti, ut ? ut->name : "Ship", NULL, 0);
      }
      continue;
    }
    const int ti = col1_unit_type_to_runtime(units, src->type);
    if (ti < 0) {
      continue;
    }
    const int id = units_spawn_allow_stack(units, ti, src->x, src->y);
    if (id < 0) {
      diag_warn("col1 import: failed to spawn unit %d type=%u", i, src->type);
      continue;
    }
    ColonizeUnit* u = units_get(units, id);
    if (u) {
      u->nation_id = src->nation_id;
      /* COL1 moves are spent-ish; treat 0 as full refresh for playability. */
      const ColonizeUnitType* ut = units_type(units, ti);
      u->moves_left = (src->moves == 0 && ut) ? ut->movement : (int)src->moves;
    }
    id_by_index[i] = id;
    local.imported_units++;
  }

  /* Board passengers via transport chain. */
  for (int i = 0; i < (int)save->head.unit_count; ++i) {
    if (!id_by_index || id_by_index[i] < 0) {
      continue;
    }
    const int ship_idx = col1_find_ship_root(save->unit, (int)save->head.unit_count, i);
    if (ship_idx < 0 || ship_idx == i || id_by_index[ship_idx] < 0) {
      continue;
    }
    if (!units_is_sea(units, id_by_index[ship_idx])) {
      continue;
    }
    if (units_is_sea(units, id_by_index[i])) {
      continue;
    }
    units_board_stacked(units, id_by_index[i], id_by_index[ship_idx]);
  }

  /* Active unit */
  if (save->head.active_unit < save->head.unit_count && id_by_index &&
      id_by_index[save->head.active_unit] >= 0) {
    units->selected_id = id_by_index[save->head.active_unit];
  } else if (units->unit_count > 0) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (units->units[i].active && units_is_on_map(&units->units[i])) {
        units->selected_id = units->units[i].id;
        break;
      }
    }
  }
  free(id_by_index);

  /* Europe / nation */
  if (europe) {
    const ColonizeCol1Nation* nat = &save->nation[local.human_nation];
    europe->gold = (int)nat->gold;
    europe->tax_percent = nat->tax_rate;
    col1_copy_name24(europe->nation_name, sizeof(europe->nation_name),
                     save->player[local.human_nation].country_name);
    for (int i = 0; i < europe->cargo_count && i < COLONIZE_COL1_CARGO_TYPES; ++i) {
      europe->cargo[i].bid = nat->trade.euro_price[i];
      europe->cargo[i].ask = nat->trade.euro_price[i] + 1;
    }
  }

  if (local.cursor_x < 0 || local.cursor_x >= map->width) {
    local.cursor_x = map->width / 2;
  }
  if (local.cursor_y < 0 || local.cursor_y >= map->height) {
    local.cursor_y = map->height / 2;
  }

  if (out) {
    *out = local;
  }
  diag_info(
    "col1_bridge_apply: turn=%u year=%u nation=%d units=%d colonies=%d europe_skip=%d",
    local.turn_number,
    local.year,
    local.human_nation,
    local.imported_units,
    local.imported_colonies,
    local.skipped_europe_units
  );
  if (err && err_size) {
    err[0] = '\0';
  }
  return true;
}

bool col1_bridge_capture(
  ColonizeCol1Save* save,
  const ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const EuropeScreen* europe,
  uint16_t year,
  uint16_t autumn,
  uint32_t turn_number,
  int human_nation,
  int cursor_x,
  int cursor_y,
  int active_unit_id,
  char* err,
  size_t err_size
) {
  if (!save || !map || !units || !colonies) {
    COL1_FAIL(err, err_size, "col1_bridge_capture bad args");
  }
  if (!save->owned || !save->map.tile) {
    COL1_FAIL(err, err_size, "save has no allocated sections");
  }
  if (map->width != save->head.map_size_x || map->height != save->head.map_size_y) {
    COL1_FAIL(
      err,
      err_size,
      "map size mismatch live=%ux%u save=%ux%u",
      map->width,
      map->height,
      save->head.map_size_x,
      save->head.map_size_y
    );
  }
  if (human_nation < 0 || human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    human_nation = 0;
  }

  save->head.year = year;
  save->head.autumn = autumn;
  save->head.turn = (uint16_t)(turn_number > 0xffffu ? 0xffffu : turn_number);
  save->stuff.x = (uint16_t)cursor_x;
  save->stuff.y = (uint16_t)cursor_y;
  save->stuff.viewport_x = (uint16_t)cursor_x;
  save->stuff.viewport_y = (uint16_t)cursor_y;
  save->player[human_nation].control = 0;

  for (size_t i = 0; i < save->map.tile_count; ++i) {
    save->map.tile[i] = col1_mp_terrain_to_tile(map->terrain[i]);
  }

  /* Nations: update human treasury/tax/prices; leave AI blob intact. */
  if (europe) {
    ColonizeCol1Nation* nat = &save->nation[human_nation];
    nat->gold = (uint32_t)(europe->gold < 0 ? 0 : europe->gold);
    nat->tax_rate = (uint8_t)(europe->tax_percent < 0 ? 0 : (europe->tax_percent > 99 ? 99 : europe->tax_percent));
    for (int i = 0; i < europe->cargo_count && i < COLONIZE_COL1_CARGO_TYPES; ++i) {
      int bid = europe->cargo[i].bid;
      if (bid < 0) {
        bid = 0;
      }
      if (bid > 255) {
        bid = 255;
      }
      nat->trade.euro_price[i] = (uint8_t)bid;
    }
  }

  /* Rebuild colony list (human + any already in save for other nations is dropped
   * when we replace the array — keep it simple: export live colonies only). */
  {
    uint16_t n = (uint16_t)colonies->colony_count;
    if (n > COLONIZE_COLONIES_MAX) {
      n = COLONIZE_COLONIES_MAX;
    }
    ColonizeCol1Colony* neu = NULL;
    if (n > 0) {
      neu = calloc(n, sizeof(ColonizeCol1Colony));
      if (!neu) {
        COL1_FAIL(err, err_size, "oom colonies export");
      }
    }
    int written = 0;
    for (int i = 0; i < COLONIZE_COLONIES_MAX && written < (int)n; ++i) {
      const ColonizeColony* src = &colonies->colonies[i];
      if (!src->active) {
        continue;
      }
      ColonizeCol1Colony* dst = &neu[written++];
      memset(dst, 0, sizeof(*dst));
      dst->x = (uint8_t)src->x;
      dst->y = (uint8_t)src->y;
      snprintf(dst->name, sizeof(dst->name), "%s", src->name);
      dst->nation_id = (uint8_t)human_nation;
      dst->population = (uint8_t)(src->colonist_count > 32 ? 32 : src->colonist_count);
      for (int p = 0; p < dst->population; ++p) {
        int t = src->colonists[p].unit_type_index;
        if (t < 0) {
          t = 0;
        }
        dst->occupation[p] = (uint8_t)t;
        dst->profession[p] = (uint8_t)t;
      }
      for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
        int s = src->stock[c];
        if (s < 0) {
          s = 0;
        }
        if (s > 65535) {
          s = 65535;
        }
        dst->stock[c] = (uint16_t)s;
      }
      memset(dst->tiles, 0xFF, sizeof(dst->tiles));
      /* Minimal buildings: town hall if present in live flags. */
      if (colonies_find_building(colonies, "Town Hall") >= 0) {
        const int thi = colonies_find_building(colonies, "Town Hall");
        if (thi >= 0 && src->has_building[thi]) {
          dst->buildings.town_hall = 1;
        }
      }
    }
    free(save->colony);
    save->colony = neu;
    save->head.colony_count = (uint16_t)written;
  }

  /* Rebuild units from live pool (includes natives only if still in pool). */
  {
    int live = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      if (units->units[i].active) {
        live++;
      }
    }
    ColonizeCol1Unit* neu = NULL;
    if (live > 0) {
      neu = calloc((size_t)live, sizeof(ColonizeCol1Unit));
      if (!neu) {
        COL1_FAIL(err, err_size, "oom units export");
      }
    }
    int* runtime_to_col1 = calloc((size_t)(units->next_id + 1), sizeof(int));
    if (!runtime_to_col1) {
      free(neu);
      COL1_FAIL(err, err_size, "oom unit remap");
    }
    for (int i = 0; i <= units->next_id; ++i) {
      runtime_to_col1[i] = -1;
    }

    int written = 0;
    int active_col1 = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* src = &units->units[i];
      if (!src->active) {
        continue;
      }
      ColonizeCol1Unit* dst = &neu[written];
      memset(dst, 0, sizeof(*dst));
      dst->x = (uint8_t)src->x;
      dst->y = (uint8_t)src->y;
      dst->type = (uint8_t)(src->type_index < 0 ? 0 : src->type_index);
      dst->nation_id = (uint8_t)(src->nation_id & 0xF);
      dst->moves = (uint8_t)(src->moves_left < 0 ? 0 : src->moves_left);
      dst->orders = src->aboard_ship_id >= 0 ? 1 : 0; /* sentry if aboard */
      dst->transport_chain.next_unit_idx = -1;
      dst->transport_chain.prev_unit_idx = -1;
      memset(dst->cargo_hold, 0, sizeof(dst->cargo_hold));
      dst->cargo_hold[2] = 255;
      runtime_to_col1[src->id] = written;
      if (src->id == active_unit_id) {
        active_col1 = written;
      }
      written++;
    }

    /* Wire passenger → ship chains (pax0→pax1→…→ship). */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* ship = &units->units[i];
      if (!ship->active || ship->cargo_count <= 0) {
        continue;
      }
      const int ship_ci = (ship->id >= 0 && ship->id <= units->next_id) ? runtime_to_col1[ship->id]
                                                                        : -1;
      if (ship_ci < 0) {
        continue;
      }
      int last = -1;
      for (int c = 0; c < ship->cargo_count; ++c) {
        const int pid = ship->cargo_ids[c];
        if (pid < 0 || pid > units->next_id) {
          continue;
        }
        const int pci = runtime_to_col1[pid];
        if (pci < 0) {
          continue;
        }
        if (last >= 0) {
          neu[last].transport_chain.next_unit_idx = (int16_t)pci;
          neu[pci].transport_chain.prev_unit_idx = (int16_t)last;
        } else {
          neu[pci].transport_chain.prev_unit_idx = -1;
        }
        last = pci;
      }
      if (last >= 0) {
        neu[last].transport_chain.next_unit_idx = (int16_t)ship_ci;
        neu[ship_ci].transport_chain.prev_unit_idx = (int16_t)last;
        neu[ship_ci].transport_chain.next_unit_idx = -1;
      }
    }

    free(save->unit);
    save->unit = neu;
    save->head.unit_count = (uint16_t)written;
    save->head.active_unit = (uint16_t)active_col1;
    free(runtime_to_col1);
  }

  if (err && err_size) {
    err[0] = '\0';
  }
  return true;
}
