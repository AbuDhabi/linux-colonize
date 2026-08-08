#include "core/col1_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/turn.h"
#include "core/strutil.h"
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

/*
 * COL1 surround-tile order (N,E,S,W,NW,NE,SE,SW) vs runtime (N,NE,E,SE,S,SW,W,NW).
 * See classic SAV notes (dledgard / CivFanatics).
 */
static const int k_col1_tile_to_runtime[8] = {0, 2, 4, 6, 7, 1, 3, 5};
static const int k_runtime_tile_to_col1[8] = {0, 5, 1, 6, 2, 7, 3, 4};

static int col1_tile_index_to_runtime(int col1_ti) {
  if (col1_ti < 0 || col1_ti >= 8) {
    return col1_ti;
  }
  return k_col1_tile_to_runtime[col1_ti];
}

static int col1_tile_index_from_runtime(int runtime_ti) {
  if (runtime_ti < 0 || runtime_ti >= 8) {
    return runtime_ti;
  }
  return k_runtime_tile_to_col1[runtime_ti];
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

static unsigned col1_encode_building_level(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* const* names,
  int name_count
) {
  unsigned level = 0;
  if (!pool || !colony || !names || name_count <= 0) {
    return 0;
  }
  for (int i = 0; i < name_count; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[idx]) {
      level = (unsigned)(i + 1);
    }
  }
  return level;
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
  static const char* k_capitol[] = {"Capitol", "Capitol Expansion"};

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
  col1_apply_building_level(pool, colony, k_capitol, 2, b->capitol);
}

static void col1_encode_colony_buildings(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  ColonizeCol1Buildings* out
) {
  static const char* k_fort[] = {"Stockade", "Fort", "Fortress"};
  static const char* k_armory[] = {"Armory", "Magazine", "Arsenal"};
  static const char* k_docks[] = {"Docks", "Drydock", "Shipyard"};
  static const char* k_hall[] = {"Town Hall"};
  static const char* k_school[] = {"Schoolhouse", "College", "University"};
  static const char* k_warehouse[] = {"Warehouse", "Warehouse Expansion"};
  static const char* k_stable[] = {"Stable"};
  static const char* k_custom[] = {"Custom House"};
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
  static const char* k_capitol[] = {"Capitol", "Capitol Expansion"};

  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  if (!pool || !colony) {
    return;
  }
  out->fortification = col1_encode_building_level(pool, colony, k_fort, 3);
  out->armory = col1_encode_building_level(pool, colony, k_armory, 3);
  out->docks = col1_encode_building_level(pool, colony, k_docks, 3);
  out->town_hall = col1_encode_building_level(pool, colony, k_hall, 1);
  out->schoolhouse = col1_encode_building_level(pool, colony, k_school, 3);
  out->warehouse = col1_encode_building_level(pool, colony, k_warehouse, 2);
  out->stables = col1_encode_building_level(pool, colony, k_stable, 1) ? 1u : 0u;
  out->custom_house = col1_encode_building_level(pool, colony, k_custom, 1) ? 1u : 0u;
  out->printing_press = col1_encode_building_level(pool, colony, k_press, 2);
  out->weavers_house = col1_encode_building_level(pool, colony, k_weaver, 3);
  out->tobacconists_house = col1_encode_building_level(pool, colony, k_tobacco, 3);
  out->rum_distillers_house = col1_encode_building_level(pool, colony, k_rum, 3);
  out->fur_traders_house = col1_encode_building_level(pool, colony, k_fur, 3);
  out->carpenters_shop = col1_encode_building_level(pool, colony, k_carpenter, 2);
  out->church = col1_encode_building_level(pool, colony, k_church, 2);
  out->blacksmiths_house = col1_encode_building_level(pool, colony, k_smith, 3);
  out->capitol = col1_encode_building_level(pool, colony, k_capitol, 2);
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
  /* Walk prev: DOS Europe fleets often chain ship→pax→pax (ship at head). */
  for (int guard = 0; guard < count + 2; ++guard) {
    if (i < 0 || i >= count) {
      break;
    }
    const uint8_t t = units[i].type;
    if (t >= 13 && t <= 18) {
      return i;
    }
    const int prev = units[i].transport_chain.prev_unit_idx;
    if (prev < 0 || prev == i) {
      break;
    }
    i = prev;
  }
  /* Walk next: pax→…→ship chains from capture / some saves. */
  i = start;
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
  col1_save_stamp_head(&save->head);
  /* Unrecruited founding fathers are -1 in DOS saves (not nation 0). */
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    save->head.founding_father[i] = -1;
  }
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

static void col1_occupancy_or_xy(
  ColonizeCol1Save* save,
  ColonizeWorldMap* map,
  int width,
  int height,
  int x,
  int y,
  uint8_t bit
) {
  if (bit == 0 || x < 0 || y < 0 || x >= width || y >= height) {
    return;
  }
  const size_t idx = (size_t)y * (size_t)width + (size_t)x;
  if (save && save->map.mask && idx < save->map.tile_count) {
    save->map.mask[idx] = (uint8_t)(save->map.mask[idx] | bit);
  }
  if (map) {
    map_occupancy_set_layer2(map, x, y, bit, true);
  }
}

void col1_bridge_sync_map_occupancy(
  ColonizeCol1Save* save,
  ColonizeWorldMap* map,
  const ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* tribe_save
) {
  int width = 0;
  int height = 0;
  size_t tile_count = 0;
  if (save && save->map.mask && save->head.map_size_x > 0 && save->head.map_size_y > 0) {
    width = (int)save->head.map_size_x;
    height = (int)save->head.map_size_y;
    tile_count = save->map.tile_count;
  } else if (map && map->layer2) {
    width = (int)map->width;
    height = (int)map->height;
    tile_count = map->tile_count;
  } else {
    return;
  }

  /* Clear occupancy bits; keep road/plow/suppress/purchased/pacific/etc. */
  if (save && save->map.mask) {
    for (size_t i = 0; i < tile_count; ++i) {
      save->map.mask[i] = (uint8_t)(save->map.mask[i] & (uint8_t)~0x03u);
    }
  }
  if (map && map->layer2) {
    for (size_t i = 0; i < tile_count && i < map->tile_count; ++i) {
      map->layer2[i] = (uint8_t)(map->layer2[i] & (uint8_t)~0x03u);
    }
  }

  if (units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units->units[i];
      if (!units_is_on_map(u)) {
        continue;
      }
      /* Europe dock sentinels (x/y >= 200) are off the playable map grid. */
      if (u->x >= 200 || u->y >= 200) {
        continue;
      }
      col1_occupancy_or_xy(save, map, width, height, u->x, u->y, MAP_OCCUPANCY_HAS_UNIT);
    }
  }

  if (colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active) {
        continue;
      }
      col1_occupancy_or_xy(save, map, width, height, c->x, c->y, MAP_OCCUPANCY_HAS_CITY);
    }
  }

  if (!tribe_save) {
    tribe_save = save;
  }
  if (tribe_save && tribe_save->tribe) {
    for (uint16_t i = 0; i < tribe_save->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* tr = &tribe_save->tribe[i];
      col1_occupancy_or_xy(
        save, map, width, height, (int)tr->x, (int)tr->y, MAP_OCCUPANCY_HAS_CITY
      );
    }
  }
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
    if (map->improve && save->map.mask) {
      const uint8_t m = save->map.mask[i];
      uint8_t flags = 0;
      if ((m & 0x08u) != 0) {
        flags = (uint8_t)(flags | MAP_IMPROVE_ROAD);
      }
      if ((m & 0x40u) != 0) {
        flags = (uint8_t)(flags | MAP_IMPROVE_PLOWED);
      }
      map->improve[i] = flags;
      /*
       * Col1 mask low bits carry village/capital occupancy (same as runtime
       * layer2 & 3). Road in mask is 0x08; DOS AI scoring also checks layer2
       * bit 0x40 for roads — mirror improve road into that bit.
       */
      if (map->layer2) {
        uint8_t l2 = (uint8_t)(m & 0x03u);
        if ((m & 0x08u) != 0) {
          l2 = (uint8_t)(l2 | 0x40u);
        }
        map->layer2[i] = l2;
      }
    }
    /* Col1 path = continent (lo) | owner/visitor (hi); runtime layer3 same. */
    if (map->layer3 && save->map.path) {
      map->layer3[i] = save->map.path[i];
    }
  }
  if (save->map.seen) {
    map_seen_from_col1(map, save->map.seen, save->map.tile_count);
  } else {
    map_reveal_all(map, -1);
  }

  /* Colonies — soft-reset actives, keep name/building catalogs. */
  {
    int name_count[4];
    int name_next[4];
    char names_backup[4][COLONIZE_COLONY_NAMES_MAX][COLONIZE_COLONY_NAME_MAX];
    ColonizeBuildingType buildings_backup[COLONIZE_BUILDING_TYPES_MAX];
    const int building_type_count = colonies->building_type_count;
    memcpy(name_count, colonies->name_count, sizeof(name_count));
    memcpy(name_next, colonies->name_next, sizeof(name_next));
    memcpy(names_backup, colonies->names, sizeof(names_backup));
    memcpy(buildings_backup, colonies->building_types, sizeof(buildings_backup));
    colonies_init(colonies);
    memcpy(colonies->names, names_backup, sizeof(names_backup));
    memcpy(colonies->name_count, name_count, sizeof(name_count));
    memcpy(colonies->name_next, name_next, sizeof(name_next));
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
    dst->nation_id = src->nation_id;
    dst->population = src->population;
    dst->hammers = src->hammers;
    /* COL1 Stockade construction id is 6; runtime uses @BUILDING index (0). */
    if (src->building_in_production == 0xFF) {
      dst->building_in_production = -1;
    } else if (src->building_in_production == 6) {
      dst->building_in_production = colonies_find_building(colonies, "Stockade");
    } else {
      dst->building_in_production = (int)src->building_in_production;
    }
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      dst->stock[c] = src->stock[c];
    }
    {
      /* ColonizeCol1CustomHouse is 2 bytes packed — same bit layout as runtime mask. */
      uint16_t bits = 0;
      memcpy(&bits, &src->custom_house, sizeof(bits));
      dst->custom_house_bits = bits;
    }
    col1_apply_colony_buildings(colonies, dst, &src->buildings);
    const int pop = src->population > COLONIZE_COLONY_POP_MAX ? COLONIZE_COLONY_POP_MAX
                                                              : (int)src->population;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      dst->tiles[t] = -1;
    }
    for (int p = 0; p < pop; ++p) {
      ColonizeColonist* col = &dst->colonists[p];
      col->active = true;
      col->building_type = -1;
      col->field_job = -1;
      /* COL1 profession[] is NAMES.TXT @JOB skill, not unit type. */
      col->profession = (int)src->profession[p];
      int work_type = units_find_type(units, "Colonists");
      if (work_type < 0) {
        work_type = 0;
      }
      col->unit_type_index = work_type;
      dst->colonist_count++;
    }
    for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
      const int who = (int)src->tiles[ti];
      if (who < 0 || who >= dst->colonist_count) {
        continue;
      }
      const int rti = col1_tile_index_to_runtime(ti);
      dst->tiles[rti] = (int8_t)who;
      const int occ = (int)src->occupation[who];
      if (occ >= 0 && occ < COLONIZE_FIELD_JOB_COUNT) {
        dst->colonists[who].field_job = occ;
        dst->colonists[who].building_type = -1;
      }
    }
    for (int p = 0; p < dst->colonist_count; ++p) {
      if (dst->colonists[p].field_job >= 0) {
        continue;
      }
      const int occ = (int)src->occupation[p];
      /* Indoor: COL1 occupation is @JOB (craftsman), not @BUILDING index. */
      const char* bname = NULL;
      switch (occ) {
        case 13: /* Carpenter */
          bname = "Carpenter's Shop";
          break;
        case 9: /* Distiller */
          bname = "Rum Distiller's House";
          break;
        case 10:
          bname = "Tobacconist's House";
          break;
        case 11:
          bname = "Weaver's House";
          break;
        case 12:
          bname = "Fur Trader's House";
          break;
        case 14: /* Blacksmith */
          bname = "Blacksmith's House";
          break;
        case 15: /* Gunsmith */
          bname = "Armory";
          break;
        case 17: /* Statesman */
        case 18: /* Preacher uses Church — Preacher is 16? */
          bname = "Town Hall";
          break;
        case 16: /* Preacher */
          bname = "Church";
          break;
        default:
          break;
      }
      if (bname) {
        const int bi = colonies_find_building(colonies, bname);
        if (bi >= 0 && dst->has_building[bi]) {
          dst->colonists[p].building_type = bi;
          continue;
        }
      }
      if (occ >= 0 && occ < colonies->building_type_count && dst->has_building[occ]) {
        dst->colonists[p].building_type = occ;
      }
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
      /* Human ships stay in Europe harbor UI; AI fleets stay as live Europe units. */
      if (europe && (src->nation_id == (uint8_t)local.human_nation) &&
          src->type >= 13 && src->type <= 18) {
        const int ti = col1_unit_type_to_runtime(units, src->type);
        const ColonizeUnitType* ut = units_type(units, ti);
        {
          int hold_types[EUROPE_SHIP_CARGO_MAX];
          int hold_amts[EUROPE_SHIP_CARGO_MAX];
          memset(hold_types, 0, sizeof(hold_types));
          memset(hold_amts, 0, sizeof(hold_amts));
          const uint8_t items[6] = {
            src->cargo_item_0,
            src->cargo_item_1,
            src->cargo_item_2,
            src->cargo_item_3,
            src->cargo_item_4,
            src->cargo_item_5
          };
          for (int h = 0; h < EUROPE_SHIP_CARGO_MAX; ++h) {
            const int amt = src->cargo_hold[h];
            if (amt > 0 && amt < 255) {
              hold_types[h] = (int)items[h];
              hold_amts[h] = amt;
            }
          }
          europe_harbor_push(
            europe, ti, ut ? ut->name : "Ship", NULL, 0, hold_types, hold_amts
          );
        }
        continue;
      }
      /* Human Europe land units (dock immigrants) and AI Europe fleets: spawn. */
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
      u->orders = (int)src->orders;
      /*
       * Col1 Braves store goto (0,0) with orders=0 meaning "no goto". Runtime
       * uses UNITS_GOTO_NONE (0xFF); leave (0,0) alone only when it equals xy.
       */
      if (!units_orders_follow_goto(u->orders) && src->goto_x == 0 && src->goto_y == 0 &&
          !(src->x == 0 && src->y == 0)) {
        u->goto_x = UNITS_GOTO_NONE;
        u->goto_y = UNITS_GOTO_NONE;
      } else {
        u->goto_x = (int)src->goto_x;
        u->goto_y = (int)src->goto_y;
      }
      u->profession = (int)src->profession;
      u->turns_worked = (int)src->turns_worked;
      /* DOS unit+0x06 / origin: home tribe index for Braves. */
      u->home_tribe_id = (int)src->origin;
      u->col1_unknown15 = src->unknown15;
      u->col1_ai_plan = src->ai_plan;
      u->col1_vis_mask = src->vis_mask;
      u->last_dir = (int)(src->unknown18 & 7u);
      /* Commodity hold slots (passengers board separately via transport chain). */
      {
        const uint8_t items[6] = {
          src->cargo_item_0,
          src->cargo_item_1,
          src->cargo_item_2,
          src->cargo_item_3,
          src->cargo_item_4,
          src->cargo_item_5
        };
        for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
          const int amt = src->cargo_hold[h];
          if (amt > 0 && amt < 255) {
            u->hold_goods_type[h] = (int)items[h];
            u->hold_goods_amount[h] = amt;
          }
        }
      }
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
    europe->current_crosses = nat->current_crosses;
    europe->needed_crosses =
      nat->needed_crosses > 0 ? nat->needed_crosses : TURN_DEFAULT_NEEDED_CROSSES;
    europe->liberty_bells_total = nat->liberty_bells_total;
    europe->liberty_bells_last_turn = nat->liberty_bells_last_turn;
    /* Restore immigrant-crosses FSM from save (dock unit / spent crosses). */
    europe->crosses_immigrant_seen = false;
    europe->crosses_pending_needed_bump = false;
    for (int ui = 0; ui < (int)save->head.unit_count; ++ui) {
      const ColonizeCol1Unit* uu = &save->unit[ui];
      if (uu->nation_id == (uint8_t)local.human_nation && col1_coord_is_europe(uu->x, uu->y) &&
          uu->type < 13) {
        europe->crosses_immigrant_seen = true;
        break;
      }
    }
    if (europe->crosses_immigrant_seen && europe->needed_crosses == 9 &&
        europe->current_crosses == 0) {
      europe->crosses_pending_needed_bump = true;
    }
    col1_copy_name24(europe->nation_name, sizeof(europe->nation_name),
                     save->player[local.human_nation].country_name);
    for (int i = 0; i < europe->cargo_count && i < (int)COLONIZE_COL1_CARGO_TYPES; ++i) {
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

  /* Align live layer2 occupancy with imported pools (tribes from save). */
  col1_bridge_sync_map_occupancy(NULL, map, units, colonies, save);

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
    if (save->map.mask && map->improve) {
      uint8_t m = save->map.mask[i];
      if ((map->improve[i] & MAP_IMPROVE_ROAD) != 0) {
        m = (uint8_t)(m | 0x08u);
      } else {
        m = (uint8_t)(m & (uint8_t)~0x08u);
      }
      if ((map->improve[i] & MAP_IMPROVE_PLOWED) != 0) {
        m = (uint8_t)(m | 0x40u);
      } else {
        m = (uint8_t)(m & (uint8_t)~0x40u);
      }
      /* Occupancy bits (has_unit / has_city) are rebuilt after unit export. */
      save->map.mask[i] = m;
    }
    if (save->map.path && map->layer3) {
      save->map.path[i] = map->layer3[i];
    }
  }
  if (save->map.seen) {
    map_seen_to_col1(map, save->map.seen, save->map.tile_count);
  }

  /* Nations: update human treasury/tax/prices; leave AI blob intact. */
  if (europe) {
    ColonizeCol1Nation* nat = &save->nation[human_nation];
    nat->gold = (uint32_t)(europe->gold < 0 ? 0 : europe->gold);
    nat->tax_rate = (uint8_t)(europe->tax_percent < 0 ? 0 : (europe->tax_percent > 99 ? 99 : europe->tax_percent));
    nat->current_crosses = europe->current_crosses;
    nat->needed_crosses = europe->needed_crosses > 0 ? europe->needed_crosses : TURN_DEFAULT_NEEDED_CROSSES;
    nat->liberty_bells_total = europe->liberty_bells_total;
    nat->liberty_bells_last_turn = europe->liberty_bells_last_turn;
    for (int i = 0; i < europe->cargo_count && i < (int)COLONIZE_COL1_CARGO_TYPES; ++i) {
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
      str_copy_trunc(dst->name, sizeof(dst->name), src->name);
      dst->nation_id = (uint8_t)src->nation_id;
      dst->population = (uint8_t)(src->colonist_count > 32 ? 32 : src->colonist_count);
      dst->hammers = (uint16_t)(src->hammers < 0 ? 0 : (src->hammers > 65535 ? 65535 : src->hammers));
      /* COL1 Stockade construction id is 6; none is 0xFF. */
      if (src->building_in_production < 0) {
        dst->building_in_production = 0xFF;
      } else {
        const int stockade = colonies_find_building(colonies, "Stockade");
        if (stockade >= 0 && src->building_in_production == stockade) {
          dst->building_in_production = 6;
        } else {
          dst->building_in_production = (uint8_t)src->building_in_production;
        }
      }
      for (int p = 0; p < dst->population; ++p) {
        const ColonizeColonist* c = &src->colonists[p];
        int prof = c->profession;
        if (prof < 0) {
          prof = UNITS_JOB_NONE;
        }
        dst->profession[p] = (uint8_t)prof;
        if (c->field_job >= 0 && c->field_job < COLONIZE_FIELD_JOB_COUNT) {
          dst->occupation[p] = (uint8_t)c->field_job;
        } else if (c->building_type >= 0 && c->building_type < 256) {
          dst->occupation[p] = (uint8_t)c->building_type;
        } else {
          dst->occupation[p] = (uint8_t)UNITS_JOB_COLONIST;
        }
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
      for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
        dst->tiles[ti] = (int8_t)-1;
      }
      for (int rti = 0; rti < COLONIZE_COLONY_FIELD_TILES; ++rti) {
        const int who = (int)src->tiles[rti];
        if (who < 0 || who >= dst->population) {
          continue;
        }
        const int cti = col1_tile_index_from_runtime(rti);
        dst->tiles[cti] = (int8_t)who;
      }
      col1_encode_colony_buildings(colonies, src, &dst->buildings);
      {
        uint16_t bits = src->custom_house_bits;
        memcpy(&dst->custom_house, &bits, sizeof(bits));
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
      dst->vis_mask = (uint8_t)(src->col1_vis_mask & 0xF);
      dst->moves = (uint8_t)(src->moves_left < 0 ? 0 : src->moves_left);
      if (src->aboard_ship_id >= 0) {
        dst->orders = 1; /* sentry if aboard */
      } else if (src->orders != 0) {
        dst->orders = (uint8_t)src->orders;
      } else {
        dst->orders = 0;
      }
      /* Col1 Braves use (0,0) for no-goto; keep that convention on export. */
      if (src->goto_x == UNITS_GOTO_NONE || src->goto_y == UNITS_GOTO_NONE || src->goto_x < 0 ||
          src->goto_y < 0) {
        dst->goto_x = 0;
        dst->goto_y = 0;
      } else {
        dst->goto_x = (uint8_t)src->goto_x;
        dst->goto_y = (uint8_t)src->goto_y;
      }
      dst->profession = (uint8_t)(src->profession < 0 ? UNITS_JOB_NONE : src->profession);
      dst->turns_worked =
        (uint8_t)(src->turns_worked < 0 ? 0 : (src->turns_worked > 255 ? 255 : src->turns_worked));
      dst->unknown15 = src->col1_unknown15;
      dst->origin =
        (uint8_t)(src->home_tribe_id < 0 || src->home_tribe_id > 255 ? 0xff
                                                                    : (src->home_tribe_id & 0xff));
      dst->ai_plan =
        src->col1_ai_plan != 0 ? src->col1_ai_plan : COL1_UNIT_UNKNOWN16_HI_DEFAULT;
      dst->unknown18 = (uint8_t)(src->last_dir & 7);
      memset(dst->cargo_hold, 0, sizeof(dst->cargo_hold));
      {
        /* Pack goods into nibble fields + amounts. Passengers are not goods. */
        int gi = 0;
        for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX && gi < 6; ++h) {
          const int amt = src->hold_goods_amount[h];
          if (amt <= 0 || amt >= 255) {
            continue;
          }
          int t = src->hold_goods_type[h];
          if (t < 0) {
            t = 0;
          }
          if (t > 15) {
            t = 15;
          }
          dst->cargo_hold[gi] = (uint8_t)amt;
          switch (gi) {
            case 0:
              dst->cargo_item_0 = (uint8_t)t;
              break;
            case 1:
              dst->cargo_item_1 = (uint8_t)t;
              break;
            case 2:
              dst->cargo_item_2 = (uint8_t)t;
              break;
            case 3:
              dst->cargo_item_3 = (uint8_t)t;
              break;
            case 4:
              dst->cargo_item_4 = (uint8_t)t;
              break;
            case 5:
              dst->cargo_item_5 = (uint8_t)t;
              break;
            default:
              break;
          }
          gi++;
        }
        if (gi == 0) {
          dst->cargo_hold[2] = 255; /* COL1 empty-hold sentinel seen in starters */
        }
        dst->holds_occupied = (uint8_t)(src->cargo_count + gi);
      }
      dst->transport_chain.next_unit_idx = -1;
      dst->transport_chain.prev_unit_idx = -1;
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

  /*
   * DOS UNITFLAG/COLONYFLAG: mask has_unit/has_city must match pools. Rebuild
   * from live units/colonies + tribe villages (not stale layer2 spawn bits).
   */
  col1_bridge_sync_map_occupancy(save, (ColonizeWorldMap*)map, units, colonies, save);

  if (err && err_size) {
    err[0] = '\0';
  }
  return true;
}

bool col1_contact_adjacent_tribe(
  ColonizeCol1Save* save,
  int x,
  int y,
  int european_nation,
  char* status_out,
  size_t status_size,
  int* out_first_indian_nation
) {
  static const char* k_tribe_names[8] = {
    "Inca", "Aztec", "Arawak", "Iroquois", "Cherokee", "Apache", "Sioux", "Tupi"
  };
  if (out_first_indian_nation) {
    *out_first_indian_nation = -1;
  }
  if (!save || !save->tribe || european_nation < 0 || european_nation > 3) {
    return false;
  }
  bool any = false;
  const char* first_name = NULL;
  for (uint16_t i = 0; i < save->head.tribe_count; ++i) {
    ColonizeCol1Tribe* tr = &save->tribe[i];
    const int dx = (int)tr->x - x;
    const int dy = (int)tr->y - y;
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1) {
      continue;
    }
    if (tr->alarm[european_nation].friction < 11) {
      tr->alarm[european_nation].friction++;
    }
    const int indian = (int)tr->nation_id - 4;
    if (indian >= 0 && indian < 8) {
      if (save->indian[indian].met_by_player[european_nation] == 0) {
        if (!first_name) {
          first_name = k_tribe_names[indian];
          if (out_first_indian_nation) {
            *out_first_indian_nation = 4 + indian;
          }
        }
      }
      if (save->indian[indian].alarm_by_player[european_nation] < 11) {
        save->indian[indian].alarm_by_player[european_nation]++;
      }
    }
    any = true;
  }
  if (any && first_name && status_out && status_size > 0) {
    snprintf(status_out, status_size, "You encounter the %s", first_name);
  }
  return any;
}
