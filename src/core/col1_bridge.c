#include "core/col1_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/col1_post_map.h"
#include "core/col1_stuff_census.h"
#include "core/founding_fathers.h"
#include "core/turn.h"
#include "core/strutil.h"
#include "platform/diagnostics.h"

/* DS:0xc8 / 0xde — same 20-ring as map_gen offshore suppress. */
static const int k_mask_nbr20_dx[20] = {
  0, 1, 0, -1, -1, 1, 1, -1, 0, 2, 0, -2, -1, 1, -1, 1, -2, -2, 2, 2
};
static const int k_mask_nbr20_dy[20] = {
  -1, 0, 1, 0, -1, -1, 1, 1, -2, 0, 2, 0, -2, -2, 2, 2, -1, 1, -1, 1
};

static int col1_mask_is_water_mp(uint8_t mp_terrain) {
  const uint8_t t = (uint8_t)(mp_terrain & 0x1fu);
  return t == 25u || t == 26u; /* ocean / high seas */
}

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

/* Display name for a save-loaded Europe-dock colonist: eu->train[]'s @JOB
 * expert_name for that profession if trainable, else the generic name (a
 * plain Free Colonist / unspecialized unit has no @JOB training entry). */
static const char* col1_bridge_europe_dock_job_name(const EuropeScreen* eu, int profession) {
  if (eu) {
    for (int i = 0; i < eu->train_count; ++i) {
      if (eu->train[i].job_index == profession) {
        return eu->train[i].expert_name;
      }
    }
  }
  return "Colonists";
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
  return col1_save_human_nation(save);
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

/*
 * Player-confirmed 2026-08-18 (colony_prod02 golden): a chain's stored
 * level is NOT the tier count N directly — it's `(1 << N) - 1` (N
 * contiguous low bits), so a 2-tier chain reads 0/1/3 and a 3-tier chain
 * reads 0/1/3/7, never 2/5/6. Confirmed both by cross-checking against
 * founding-father gating (no nation in colony_prod02 owns Adam Smith, and
 * no blacksmiths_house/carpenters_shop/etc. field ever reads a value
 * requiring the top, Adam-Smith-gated tier) and by the value 2 simply
 * never appearing across either save's ~30 colonies for any 2-or-3-tier
 * chain field, which a plain tier-count encoding would produce constantly.
 * `popcount` recovers N from that pattern exactly.
 */
static unsigned col1_building_level_to_tier_count(unsigned level) {
  unsigned n = 0;
  while (level) {
    n += level & 1u;
    level >>= 1;
  }
  return n;
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
  const unsigned tier_count = col1_building_level_to_tier_count(level);
  for (int i = 0; i < name_count && (unsigned)i < tier_count; ++i) {
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
  unsigned tier_count = 0;
  if (!pool || !colony || !names || name_count <= 0) {
    return 0;
  }
  for (int i = 0; i < name_count; ++i) {
    const int idx = colonies_find_building(pool, names[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[idx]) {
      tier_count = (unsigned)(i + 1);
    }
  }
  return tier_count == 0 ? 0 : (1u << tier_count) - 1u;
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
    "Rum Distiller's House", "Rum Distillery", "Rum Factory"
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
    "Rum Distiller's House", "Rum Distillery", "Rum Factory"
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
  col1_save_reset_nation_slots(&save->head);
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
      /*
       * FUN_1427_02ca / FUN_137f_0228: euro unit tiles must carry owner nibble.
       * Human fleets that sailed without claiming left ocean as path=fx; DOS then
       * peels the transport stack on select/move (patch F: path=01 fixes it).
       * Natives: AI already stamps owners; village tiles keep tribe high nibble.
       */
      if (u->nation_id >= 0 && u->nation_id < 4 && u->x >= 0 && u->y >= 0 && u->x < width &&
          u->y < height) {
        const size_t ti = (size_t)u->y * (size_t)width + (size_t)u->x;
        const uint8_t hi = (uint8_t)(((unsigned)u->nation_id & 0x0fu) << 4);
        if (save && save->map.path && ti < tile_count) {
          save->map.path[ti] = (uint8_t)((save->map.path[ti] & 0x0fu) | hi);
        }
        if (map && map->layer3 && ti < map->tile_count) {
          map->layer3[ti] = (uint8_t)((map->layer3[ti] & 0x0fu) | hi);
        }
      }
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

void col1_bridge_sync_map_density(ColonizeCol1Save* save, const ColonizeWorldMap* map) {
  if (!save || !save->map.mask || !map || !map->terrain) {
    return;
  }
  if (save->head.map_size_x != map->width || save->head.map_size_y != map->height) {
    return;
  }
  const int w = (int)map->width;
  const int h = (int)map->height;
  const int pacific_xmax = w / 2;
  const size_t n = save->map.tile_count < map->tile_count ? save->map.tile_count : map->tile_count;

  for (size_t i = 0; i < n; ++i) {
    const int x = (int)(i % (size_t)w);
    const int y = (int)(i / (size_t)w);
    uint8_t m = save->map.mask[i];
    const uint8_t purchased = (uint8_t)(m & 0x10u);
    /* Recompute suppress + pacific; keep purchased unless layer2 knows better. */
    m = (uint8_t)(m & (uint8_t)~(0x04u | 0x20u));

    const uint8_t l2 = (map->layer2 && i < map->tile_count) ? map->layer2[i] : 0u;
    if ((l2 & MAP_LAYER2_SUPPRESS) != 0) {
      m = (uint8_t)(m | 0x04u);
    }
    if ((l2 & MAP_LAYER2_PACIFIC) != 0) {
      m = (uint8_t)(m | 0x20u);
    }
    if ((l2 & MAP_LAYER2_PURCHASED) != 0) {
      m = (uint8_t)(m | 0x10u);
    } else {
      m = (uint8_t)(m | purchased);
    }

    /* FUN_684c_08c0 western pacific walk (param_1==0 → width/2). */
    if (x >= 1 && x < pacific_xmax && y >= 1 && y < h - 1) {
      int western_water = 1;
      for (int wx = 1; wx <= x; ++wx) {
        if (!col1_mask_is_water_mp(map->terrain[(size_t)y * (size_t)w + (size_t)wx])) {
          western_water = 0;
          break;
        }
      }
      if (western_water) {
        m = (uint8_t)(m | 0x20u);
      }
    }

    /* Offshore suppress: water with no inset land neighbour in 20-ring. */
    if (col1_mask_is_water_mp(map->terrain[i])) {
      int has_land = 0;
      for (int k = 0; k < 20; ++k) {
        const int nx = x + k_mask_nbr20_dx[k];
        const int ny = y + k_mask_nbr20_dy[k];
        if (nx < 1 || ny < 1 || nx >= w - 1 || ny >= h - 1) {
          continue;
        }
        if (!col1_mask_is_water_mp(map->terrain[(size_t)ny * (size_t)w + (size_t)nx])) {
          has_land = 1;
          break;
        }
      }
      if (!has_land) {
        m = (uint8_t)(m | 0x04u);
      }
    }

    save->map.mask[i] = m;
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
  /*
   * post_map.prime_resource_seed (FUN_684c_08c0 mapgen) drives special-
   * resource + rumour placement (map_resource_type_at_ex /
   * map_procedural_rumour_at) — without this, those hash off a hardcoded
   * MAP_RESOURCE_SEED_DEFAULT (100, this project's own mapgen-fixture
   * seed) instead of the save's real one, so every loaded campaign save
   * whose map wasn't generated with seed 100 gets resource/rumour
   * placement for the *wrong* map. Player-confirmed 2026-08-16
   * (colony-prod-tests, seed 541): tiles the real DOS map special-cased
   * (Prime Tobacco etc.) read as plain terrain here, undercounting yield.
   */
  map->prime_resource_seed = save->post_map.prime_resource_seed;
  for (size_t i = 0; i < save->map.tile_count; ++i) {
    map->terrain[i] = col1_tile_to_mp_terrain(save->map.tile[i]);
    if (map->improve && save->map.mask) {
      const uint8_t m = save->map.mask[i];
      uint8_t flags = 0;
      /* Road in mask is 0x08 only — 0x04 is village/capital occupancy (see
       * the layer2 derivation right below, which already gets this right).
       * Checking 0x0c (0x04|0x08) here falsely flagged any tile with the
       * occupancy bit set — including plain ocean tiles — as having a
       * road. Player-reported: (47,31)/(53,32)/(55,34)/(55,37) in
       * dutch-reports.SAV, all mask=0x04, all ocean, all wrongly roaded. */
      if ((m & 0x08u) != 0) {
        flags = (uint8_t)(flags | MAP_IMPROVE_ROAD);
      }
      if ((m & 0x40u) != 0) {
        flags = (uint8_t)(flags | MAP_IMPROVE_PLOWED);
      }
      map->improve[i] = flags;
      /*
       * Col1 mask low bits carry village/capital occupancy (same as runtime
       * layer2 & 3). Road is mask 0x08 (kept in improve[], since layer2
       * 0x08 is the runtime rumour-cleared stand-in); plowed is mask 0x40
       * and stays at its real bit.
       */
      if (map->layer2) {
        map->layer2[i] = (uint8_t)(m & (uint8_t)(0x03u | 0x04u | MAP_LAYER2_PURCHASED |
                                                  MAP_LAYER2_PACIFIC | MAP_LAYER2_PLOWED));
      }
    }
    /* Col1 path = continent (lo) | owner/visitor (hi); runtime layer3 same. */
    if (map->layer3 && save->map.path) {
      map->layer3[i] = save->map.path[i];
    }
    /*
     * Lost City Rumours are purely procedural (map_procedural_rumour_at:
     * a hash of position + prime_resource_seed) with no dedicated
     * "already explored" bit anywhere in the Col1 tile/mask format —
     * map_clear_rumour instead sets our own runtime-only layer2 bit
     * (MAP_LAYER2_RUMOUR_CLEARED) the moment a unit resolves one live.
     * That bit starts zero on every fresh import, so a save carrying a
     * rumour some unit already stood on and resolved in DOS shows it as
     * freshly unexplored again. Player-reported: dutch-reports.SAV — every
     * already-explored rumour mound still stood on load.
     * Fix: reuse path's own visitor-history nibble (0xf = nobody has ever
     * occupied this tile) as the "already explored" signal, same source
     * units_resolve_lcr_rumour's own live occupancy tracking (units_map_
     * set_owner_nibble) writes to — resolving a rumour always means a
     * unit stood right on top of it, so "has anyone ever visited" implies
     * "any rumour here was already triggered". Harmless to set on a
     * non-rumour tile too: map_has_rumour_at's hash check still gates
     * everything downstream of this bit.
     */
    if (map->layer2 && save->map.path && (save->map.path[i] >> 4) != 0x0fu) {
      map->layer2[i] = (uint8_t)(map->layer2[i] | MAP_LAYER2_RUMOUR_CLEARED);
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
    /*
     * bugs.md 274: the name counter belongs to the SAVE, not the session —
     * loading must reset it, or names keep advancing across reloads. Base
     * it on player.founded_colonies (what the found paths bump); for legacy
     * saves that never tracked the human's counter, fall back to the
     * highest COLONY.TXT name a loaded colony of that nation still carries.
     */
    for (int n = 0; n < 4; ++n) {
      int next = (int)save->player[n].founded_colonies;
      for (uint16_t ci = 0; ci < save->head.colony_count; ++ci) {
        const ColonizeCol1Colony* sc = &save->colony[ci];
        if ((int)sc->nation_id != n) {
          continue;
        }
        char cname[COLONIZE_COLONY_NAME_MAX];
        col1_copy_name24(cname, sizeof(cname), sc->name);
        for (int k = 0; k < colonies->name_count[n]; ++k) {
          if (strcmp(colonies->names[n][k], cname) == 0 && k + 1 > next) {
            next = k + 1;
          }
        }
      }
      colonies->name_next[n] = next;
    }
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
    /* Raw COL1 code == NAMES.TXT @BUILDING file-order index directly — the
     * pool's own load order (colonies_load_buildings iterates the file
     * top-to-bottom), confirmed against dutch-reports.SAV/its Construction-
     * tab goldens: Recife's raw code 6 golden-shows "Docks" (@BUILDING's
     * 7th, 0-indexed 6th, entry), not "Stockade" (index 0) — an earlier
     * "COL1 Stockade id is 6" special case here was wrong (unverified
     * guess predating any Construction-tab golden) and silently
     * mis-tracked any colony actually building Docks as building a
     * Stockade instead. A code past the table's own range (41, the last
     * entry) encodes a buildable *unit* instead (New Amsterdam's golden:
     * 42 = Artillery) — colonies_building_type returns NULL for those;
     * colony_screen.c special-cases the one golden-confirmed unit code for
     * display only, see its comment. */
    if (src->building_in_production == 0xFF) {
      dst->building_in_production = -1;
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
    dst->labor_shortage = src->labor_shortage;
    dst->garrison_quota = src->garrison_quota;
    memcpy(dst->pop_on_map, src->visible_to_euro, sizeof(dst->pop_on_map));
    memcpy(dst->fort_on_map, src->fortification_on_map, sizeof(dst->fort_on_map));
    dst->specialty_cargo = src->specialty_cargo;
    dst->cargo_idle_turns = src->cargo_idle_turns;
    dst->improve_timer = src->improve_timer;
    dst->build_ai_flags = src->build_ai_flags;
    {
      uint8_t af = 0;
      memcpy(&af, &src->ai_flags, sizeof(af));
      dst->ai_flags = af;
    }
    {
      uint8_t cf = 0;
      memcpy(&cf, &src->flags, sizeof(cf));
      dst->colony_flags = cf;
    }
    dst->cargo_produced_mask = src->cargo_produced_mask;
    dst->hammers_purchased = src->hammers_purchased;
    dst->depletion_counter = src->depletion_counter;
    dst->warehouse_level = src->warehouse_level;
    dst->capitol_level = src->capitol_level;
    col1_apply_colony_buildings(colonies, dst, &src->buildings);
    const int pop = src->population > COLONIZE_COLONY_POP_MAX ? COLONIZE_COLONY_POP_MAX
                                                              : (int)src->population;
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
      dst->tiles[t] = -1;
    }
    /* Slots 8..19 are unused by DOS; preserve raw for byte-exact write-back. */
    for (int t = (int)COLONIZE_COL1_COLONY_TILE_RING; t < (int)COLONIZE_COL1_COLONY_TILES; ++t) {
      dst->col1_outer_tiles[t - (int)COLONIZE_COL1_COLONY_TILE_RING] = src->tiles[t];
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
      const int occ = (int)src->occupation[p];
      if (occ >= 0 && occ < COLONIZE_FIELD_JOB_COUNT && dst->colonists[p].field_job < 0) {
        dst->colonists[p].field_job = occ;
        dst->colonists[p].building_type = -1;
      }
    }
    for (int p = 0; p < dst->colonist_count; ++p) {
      if (dst->colonists[p].field_job >= 0) {
        continue;
      }
      const int occ = (int)src->occupation[p];
      static const char* const k_chain_carpenter[] = {"Lumber Mill", "Carpenter's Shop"};
      static const char* const k_chain_distiller[] = {"Rum Factory", "Rum Distillery", "Rum Distiller's House"};
      static const char* const k_chain_tobacconist[] = {"Cigar Factory", "Tobacconist's Shop", "Tobacconist's House"};
      static const char* const k_chain_weaver[] = {"Textile Mill", "Weaver's Shop", "Weaver's House"};
      static const char* const k_chain_fur[] = {"Fur Factory", "Fur Trading Post", "Fur Trader's House"};
      static const char* const k_chain_smith[] = {"Iron Works", "Blacksmith's Shop", "Blacksmith's House"};
      static const char* const k_chain_gunsmith[] = {"Arsenal", "Magazine", "Armory"};
      static const char* const k_chain_church[] = {"Cathedral", "Church"};
      static const char* const k_chain_school[] = {"University", "College", "Schoolhouse"};
      static const char* const k_chain_hall[] = {"Town Hall"};

      const char* const* chain = NULL;
      size_t chain_len = 0;

      if (occ == 13) {
        chain = k_chain_carpenter;
        chain_len = sizeof(k_chain_carpenter) / sizeof(k_chain_carpenter[0]);
      } else if (occ == 9 || occ == 27 || occ == 28 || occ == 29) {
        chain = k_chain_distiller;
        chain_len = sizeof(k_chain_distiller) / sizeof(k_chain_distiller[0]);
      } else if (occ == 10) {
        chain = k_chain_tobacconist;
        chain_len = sizeof(k_chain_tobacconist) / sizeof(k_chain_tobacconist[0]);
      } else if (occ == 11) {
        chain = k_chain_weaver;
        chain_len = sizeof(k_chain_weaver) / sizeof(k_chain_weaver[0]);
      } else if (occ == 12) {
        chain = k_chain_fur;
        chain_len = sizeof(k_chain_fur) / sizeof(k_chain_fur[0]);
      } else if (occ == 14) {
        chain = k_chain_smith;
        chain_len = sizeof(k_chain_smith) / sizeof(k_chain_smith[0]);
      } else if (occ == 15) {
        chain = k_chain_gunsmith;
        chain_len = sizeof(k_chain_gunsmith) / sizeof(k_chain_gunsmith[0]);
      } else if (occ == 16) {
        chain = k_chain_church;
        chain_len = sizeof(k_chain_church) / sizeof(k_chain_church[0]);
      } else if (occ == 17) {
        chain = k_chain_hall;
        chain_len = sizeof(k_chain_hall) / sizeof(k_chain_hall[0]);
      } else if (occ == 18) {
        chain = k_chain_school;
        chain_len = sizeof(k_chain_school) / sizeof(k_chain_school[0]);
      }

      if (chain) {
        for (size_t ci = 0; ci < chain_len; ++ci) {
          const int bi = colonies_find_building(colonies, chain[ci]);
          if (bi >= 0 && bi < COLONIZE_BUILDING_TYPES_MAX && dst->has_building[bi]) {
            dst->colonists[p].building_type = bi;
            break;
          }
        }
      }
      /*
       * bugs.md (starvation_bug.SAV): salvage — a building occupation that
       * could not be resolved (chain building missing, or a garbage byte
       * from the pre-fix exporter that wrote port building indexes) must
       * not leave the colonist silently idle; put them to work in the Town
       * Hall rather than let the colony starve unnoticed. occ 19 (plain
       * colonist) stays idle on purpose.
       */
      if (dst->colonists[p].building_type < 0 && dst->colonists[p].field_job < 0 &&
          occ != (int)UNITS_JOB_COLONIST) {
        const int th = colonies_find_building(colonies, "Town Hall");
        if (th >= 0 && th < COLONIZE_BUILDING_TYPES_MAX && dst->has_building[th]) {
          dst->colonists[p].building_type = th;
        }
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

  /*
   * Human ships in transit (DOS sentinel lanes, see capture): passengers
   * chained to a Bound/Expected ship stay aboard as EuropeHarborShip cargo,
   * not dock immigrants. Mark them consumed before the main walk.
   */
  bool* consumed = NULL;
  if (save->head.unit_count > 0) {
    consumed = calloc((size_t)save->head.unit_count, sizeof(bool));
    if (!consumed) {
      free(id_by_index);
      COL1_FAIL(err, err_size, "oom unit consumed map");
    }
  }
  if (europe && local.human_nation >= 0) {
    const uint8_t n = (uint8_t)local.human_nation;
    const uint8_t xy_bound = (uint8_t)(232 + n);
    const uint8_t xy_expected = (uint8_t)(244 + n);
    for (int i = 0; i < (int)save->head.unit_count; ++i) {
      const ColonizeCol1Unit* src = &save->unit[i];
      if (src->nation_id != n || src->type < 13 || src->type > 18 ||
          (src->x != xy_bound && src->x != xy_expected)) {
        continue;
      }
      const bool bound = src->x == xy_bound;
      const int ti = col1_unit_type_to_runtime(units, src->type);
      const ColonizeUnitType* ut = units_type(units, ti);
      int pax_types[EUROPE_SHIP_CARGO_MAX];
      int pax_profs[EUROPE_SHIP_CARGO_MAX];
      int pax_gold[EUROPE_SHIP_CARGO_MAX];
      int pax_n = 0;
      memset(pax_gold, 0, sizeof(pax_gold));
      /* Chain is pax0→…→ship: walk prev from the ship, then reverse. */
      {
        int tmp_t[EUROPE_SHIP_CARGO_MAX];
        int tmp_p[EUROPE_SHIP_CARGO_MAX];
        int tmp_g[EUROPE_SHIP_CARGO_MAX];
        int k = 0;
        int p = src->transport_chain.prev_unit_idx;
        for (int guard = 0; guard < (int)save->head.unit_count && p >= 0 &&
                            p < (int)save->head.unit_count && k < EUROPE_SHIP_CARGO_MAX;
             ++guard) {
          const ColonizeCol1Unit* pu = &save->unit[p];
          if (pu->type >= 13 && pu->type <= 18) {
            break;
          }
          tmp_t[k] = col1_unit_type_to_runtime(units, pu->type);
          tmp_p[k] = (int)pu->profession;
          tmp_g[k] = pu->type == 0x0a ? (int)pu->profession * 100 : 0;
          consumed[p] = true;
          k++;
          p = pu->transport_chain.prev_unit_idx;
        }
        for (int j = k - 1; j >= 0; --j) {
          pax_types[pax_n] = tmp_t[j];
          pax_profs[pax_n] = tmp_p[j];
          pax_gold[pax_n] = tmp_g[j];
          pax_n++;
        }
      }
      int hold_types[EUROPE_SHIP_CARGO_MAX];
      int hold_amts[EUROPE_SHIP_CARGO_MAX];
      memset(hold_types, 0, sizeof(hold_types));
      memset(hold_amts, 0, sizeof(hold_amts));
      {
        const uint8_t items[6] = {src->cargo_item_0, src->cargo_item_1, src->cargo_item_2,
                                  src->cargo_item_3, src->cargo_item_4, src->cargo_item_5};
        for (int h = 0; h < EUROPE_SHIP_CARGO_MAX && h < 6 && h < (int)src->holds_occupied; ++h) {
          const int amt = src->cargo_hold[h];
          if (amt > 0 && amt < 255) {
            hold_types[h] = (int)items[h];
            hold_amts[h] = amt;
          }
        }
      }
      const int turns = src->turns_worked > 0 ? (int)src->turns_worked : 1;
      const int exit_x = (int)src->goto_x;
      const int exit_y = (int)src->goto_y;
      const bool exit_east = map->width > 0 ? exit_x >= map->width / 2 : true;
      EuropeHarborShip* slot = NULL;
      if (bound) {
        if (europe->bound_ships < EUROPE_HARBOR_MAX) {
          slot = &europe->bound[europe->bound_ships++];
        }
      } else if (europe_enqueue_expected(
                   europe, ti, ut ? ut->name : "Ship", pax_types, pax_profs, pax_n,
                   hold_types, hold_amts, exit_x, exit_y, exit_east, turns
                 )) {
        slot = &europe->expected[europe->expected_ships - 1];
        slot->turns_left = turns;
        /* Restoring a save is not a departure — the ship is already at sea,
         * so it must not collect the departure turn europe_enqueue_expected
         * hands to a ship that sails this turn. */
        slot->departed_this_turn = false;
      }
      if (!slot) {
        continue;
      }
      if (bound) {
        memset(slot, 0, sizeof(*slot));
        slot->type_index = ti;
        snprintf(slot->name, sizeof(slot->name), "%s", ut ? ut->name : "Ship");
        for (int c = 0; c < pax_n; ++c) {
          slot->cargo_types[c] = pax_types[c];
          slot->cargo_professions[c] = pax_profs[c];
        }
        slot->cargo_count = pax_n;
        for (int h = 0; h < EUROPE_SHIP_CARGO_MAX; ++h) {
          slot->hold_goods_type[h] = hold_types[h];
          slot->hold_goods_amount[h] = hold_amts[h];
        }
        slot->turns_left = turns;
        slot->exit_x = exit_x;
        slot->exit_y = exit_y;
        slot->exit_east = exit_east;
      }
      for (int c = 0; c < pax_n; ++c) {
        slot->cargo_treasure_gold[c] = pax_gold[c];
      }
      consumed[i] = true;
      local.skipped_europe_units++;
    }
  }

  for (int i = 0; i < (int)save->head.unit_count; ++i) {
    const ColonizeCol1Unit* src = &save->unit[i];
    if (consumed && consumed[i]) {
      continue;
    }
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
      /* Human Europe land units are waiting-immigrant colonists — push them
       * onto the dock (what the Europe screen actually renders), not a
       * spawned off-map unit (invisible there; the old behavior). AI Europe
       * land units/fleets still spawn as live off-map units, unchanged. */
      if (europe && src->nation_id == (uint8_t)local.human_nation) {
        const char* name = col1_bridge_europe_dock_job_name(europe, (int)src->profession);
        europe_dock_push_load(europe, name, (int)src->profession);
        /*
         * The dock entry's job name says nothing about whether this
         * immigrant was armed, equipped or blessed on the dock, but the
         * save's @UNIT type does — carry it over so an @ARMOPTIONS change
         * survives the round trip (bugs.md). Types outside the six the dock
         * menu deals in fall back to what the profession implies.
         */
        if (europe->dock_count > 0 && (int)src->type <= 5) {
          europe->dock[europe->dock_count - 1].dos_type = (int)src->type;
        }
        /*
         * Keep the runtime shape turn.c's immigrant path creates: dock entry
         * plus a mirror unit at Europe (236,236) — capture only walks the
         * unit pool, so without the mirror a loaded dock colonist vanished
         * from the next save (seed-100 TURN5→6 lost the human's immigrant).
         */
        {
          const int dos_type =
            (europe && europe->dock_count > 0) ? europe->dock[europe->dock_count - 1].dos_type : 0;
          int tid = europe_dock_unit_type_index(units, dos_type);
          if (tid < 0) {
            tid = units_find_type(units, "Colonists");
          }
          const int id = units_spawn_allow_stack(units, tid >= 0 ? tid : 0, 236, 236);
          ColonizeUnit* mu = units_get(units, id);
          if (mu) {
            units_set_nation(mu, (int)src->nation_id);
            mu->orders = UNITS_ORDER_SENTRY;
            mu->profession = (int)src->profession;
            mu->goto_x = 0;
            mu->goto_y = 0;
            mu->moves_left = 0;
            europe_apply_dock_unit_kit(mu, dos_type);
            if (id_by_index) {
              id_by_index[i] = id;
            }
          }
        }
        continue;
      }
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
      /*
       * Col1 +0x05 = DOS moves_spent in thirds (FUN_465b_0000). Euro units:
       * moves_left = max_mp - spent (0 = full refresh). Natives keep the
       * literal byte — ai.c's Brave engine tracks DOS spent in moves_left
       * itself (max 3), matching the TURN goldens.
       */
      const ColonizeUnitType* ut = units_type(units, ti);
      if (src->nation_id <= 3) {
        int total = units_type_max_mp(ut);
        if (ut && ut->domain == COLONIZE_UNIT_DOMAIN_SEA &&
            founding_fathers_nation_has(save, src->nation_id, FF_FERDINAND_MAGELLAN)) {
          total += UNITS_MP_PER_TILE;
        }
        const int spent = (int)src->moves;
        u->moves_left = total > spent ? total - spent : 0;
      } else {
        u->moves_left = (int)src->moves;
      }
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
      /* DOS unit+0x06 / origin: home tribe index for Braves only; 0xff = none.
       * Euros always -1 (starter bugs exported origin=0 which is tribe[0]). */
      if ((src->nation_id & 0xF) < 4 || src->origin == 0xff) {
        u->home_tribe_id = -1;
      } else {
        u->home_tribe_id = (int)src->origin;
      }
      /* Repack the 8 named single-bit fields (was one unknown15_lo:7 blob;
       * see col1_save.h) back into the raw byte col1_unknown15 expects. */
      u->col1_unknown15 =
        (uint8_t)((src->unknown15_bit0 ? 0x01u : 0u) |
                  (src->roam_reeval_pending ? 0x02u : 0u) |
                  (src->stack_has_founders_or_military ? 0x04u : 0u) |
                  (src->stack_has_military ? 0x08u : 0u) |
                  (src->wander_dest_chosen ? 0x10u : 0u) |
                  (src->garrison_request_pending ? 0x20u : 0u) |
                  (src->bound_in_transit ? 0x40u : 0u) |
                  (src->ship_damaged ? 0x80u : 0u));
      u->col1_ai_plan = src->ai_plan;
      u->col1_vis_mask = src->vis_mask;
      u->last_dir = (int)src->facing;
      /*
       * Commodity holds: ships/wagons only. Land pioneers store tools in
       * cargo_hold[5] (DOS unit+0x15) — not a goods slot.
       */
      if (units_is_sea(units, id) || units_is_transport(units, id)) {
        const uint8_t items[6] = {
          src->cargo_item_0,
          src->cargo_item_1,
          src->cargo_item_2,
          src->cargo_item_3,
          src->cargo_item_4,
          src->cargo_item_5
        };
        /*
         * Only the first `holds_occupied` slots are real (savegame.md:
         * "Ship holds_occupied = goods only"). The rest of cargo_hold[]/
         * cargo_item_*[] can carry stale bytes left over from goods a ship
         * unloaded earlier in the game — DOS clears the slot count but not
         * the array contents. Without this gate a ship with holds_occupied
         * == 0 (nothing aboard) still had its stale array contents read as
         * real cargo. Player-reported (Naval report, dutch-reports.SAV): a
         * Merchantman and Privateer both showed phantom cargo icons (Furs /
         * Tools) that don't exist in the golden capture; both have
         * holds_occupied == 0 in the raw save.
         */
        const int holds = src->holds_occupied < COLONIZE_UNIT_CARGO_MAX
          ? src->holds_occupied
          : COLONIZE_UNIT_CARGO_MAX;
        for (int h = 0; h < holds; ++h) {
          const int amt = src->cargo_hold[h];
          if (amt > 0 && amt < 255) {
            u->hold_goods_type[h] = (int)items[h];
            u->hold_goods_amount[h] = amt;
          }
        }
      } else if (src->cargo_hold[5] > 0 && src->cargo_hold[5] <= 100) {
        u->tools = (int)src->cargo_hold[5];
      }
    }
    id_by_index[i] = id;
    local.imported_units++;
  }

  /* Board passengers via transport chain.
   *
   * col1_find_ship_root walks transport_chain prev/next looking for any sea
   * unit — but Col1 reuses that same chain for plain same-tile stacking
   * order too, not just genuine Europe-dock/hold manifests. A land unit
   * merely garrisoned on a colony tile that also has a ship docked shares
   * the tile's stacking chain with that ship, so without a discriminator
   * every such garrison unit gets wrongly "boarded" (aboard_ship_id set,
   * orders forced to Sentry — see units_board_stacked). Player-reported
   * (Naval report, dutch-reports.SAV New Amsterdam dock): a Fortified
   * Dragoon and a Fortified Artillery both showed up as passengers of the
   * Privateer docked there. Fortified/Fortify is a strict discriminator —
   * a unit mid-fortify or already fortified is definitionally not aboard a
   * ship (confirmed against the same save's one genuine passenger, a
   * Sentry-orders Colonist on the Caravel: never Fortified). This doesn't
   * catch every possible false positive (a Sentried land unit simply
   * standing at a dock, not boarded, would still slip through), but it's a
   * real, low-risk, well-evidenced fix for the concrete case seen — see
   * docs/report_screens.md's Naval report section. */
  for (int i = 0; i < (int)save->head.unit_count; ++i) {
    if (!id_by_index || id_by_index[i] < 0) {
      continue;
    }
    /* Only a SENTRY unit can be a genuine passenger (bugs.md: ships take on
     * passengers from the sentried units only; DOS stamps orders 1 on every
     * boarded unit and the dutch-reports.SAV manifest confirms it). The
     * earlier skip-list (Fortify/Fortified only) still boarded no-orders or
     * plowing pioneers merely garrisoned beside a docked ship. */
    if (save->unit[i].orders != UNITS_ORDER_SENTRY) {
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
    /*
     * bugs.md interop: the capture now writes FULL per-tile stacking chains
     * (DOS FUN_1427_02ca semantics), so a sentried land unit standing in a
     * colony shares a chain with any docked ship. A land unit is genuinely
     * aboard only on a WATER tile (the port's docking model puts everyone
     * ashore in a colony; a land unit standing on sea has no other way to
     * be there).
     */
    if (map && save->unit[i].x < 200 && save->unit[i].y < 200 &&
        map_tile_is_land(map, (int)save->unit[i].x, (int)save->unit[i].y)) {
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
  free(consumed);

  /* Europe / nation */
  if (europe) {
    const ColonizeCol1Nation* nat = &save->nation[local.human_nation];
    /* FUN_48d3_007a landfall stamp → Bound ships without an exit tile. */
    if (!europe->last_exit_valid && (nat->return_from_europe_x || nat->return_from_europe_y) &&
        nat->return_from_europe_x < 200 && nat->return_from_europe_y < 200) {
      europe->last_exit_x = nat->return_from_europe_x;
      europe->last_exit_y = nat->return_from_europe_y;
      europe->last_exit_east = map->width > 0 ? europe->last_exit_x >= map->width / 2 : true;
      europe->last_exit_valid = true;
    }
    europe->gold = (int)nat->gold;
    europe->tax_percent = nat->tax_rate;
    europe->difficulty = save->head.difficulty > 8 ? 8 : save->head.difficulty;
    /* 0xFFFF is the fingerprint of the removed Euro-war all-cargo embargo
     * stand-in (bugs.md all_boycotted.SAV). DOS boycotts are king tea-party,
     * one cargo at a time — never all 16 in one event. */
    europe->boycott_bitmap = (nat->boycott_bitmap == 0xFFFFu) ? 0u : nat->boycott_bitmap;
    europe->current_crosses = nat->current_crosses;
    europe->needed_crosses =
      nat->needed_crosses > 0 ? nat->needed_crosses : TURN_DEFAULT_NEEDED_CROSSES;
    europe->liberty_bells_total = nat->liberty_bells_total;
    europe->liberty_bells_last_turn = nat->liberty_bells_last_turn;
    /* Restore immigrant-crosses FSM from save (dock unit / spent crosses). */
    europe->crosses_immigrant_seen = false;
    for (int ui = 0; ui < (int)save->head.unit_count; ++ui) {
      const ColonizeCol1Unit* uu = &save->unit[ui];
      if (uu->nation_id == (uint8_t)local.human_nation && col1_coord_is_europe(uu->x, uu->y) &&
          uu->type < 13) {
        europe->crosses_immigrant_seen = true;
        break;
      }
    }
    europe_set_nation(europe, local.human_nation, NULL);
    for (int i = 0; i < europe->cargo_count && i < (int)COLONIZE_COL1_CARGO_TYPES; ++i) {
      europe->cargo[i].bid = nat->trade.euro_price[i];
      europe->trade_nr[i] = nat->trade.nr[i];
      {
        int burden = europe->cargo[i].burden;
        if (burden < 0) {
          burden = 0;
        }
        europe->cargo[i].ask = europe->cargo[i].bid + burden; /* FUN_38fd_0016 */
      }
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

  founding_fathers_sync_from_col1_after_load(save);

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

static int col1_bridge_tribe_at(const ColonizeCol1Save* save, int x, int y) {
  if (!save || !save->tribe) {
    return -1;
  }
  for (uint16_t i = 0; i < save->head.tribe_count; ++i) {
    if ((int)save->tribe[i].x == x && (int)save->tribe[i].y == y) {
      return (int)i;
    }
  }
  return -1;
}

static int col1_bridge_unit_is_missionary(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const ColonizeUnitType* t = units_type(units, u->type_index);
  return t && strstr(t->name, "Missionary") != NULL;
}

/*
 * Last-chance DOS hygiene before export: board co-located land onto own ships,
 * and nudge settler-ish euros off village tiles (Danger Will Robinson).
 */
static void col1_bridge_sanitize_units_for_dos(
  ColonizeUnitPool* units,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* save
) {
  if (!units || !map) {
    return;
  }
  static const int k_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int k_dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* land = &units->units[i];
    if (!land->active || land->nation_id < 0 || land->nation_id > 3) {
      continue;
    }
    if (land->aboard_ship_id >= 0 || units_is_sea(units, land->id)) {
      continue;
    }
    /*
     * Same-tile own ship → board — but ONLY when the land unit stands on a
     * water tile (an "ocean sentry orphan": a land unit on water that is not
     * aboard anything is already a broken state DOS cannot represent).
     *
     * bugs.md ("ships take away any non-fortified unit as they even enter a
     * colony"): this loop used to board every co-located land unit on ANY
     * tile — and it mutates the LIVE pool during save/export
     * (units_board_stacked stamps Sentry, parks MP, sets aboard_ship_id).
     * Save with a ship docked at a colony and the whole garrison silently
     * became passengers, sailing off with the next departure. Land-tile
     * stacks (colony docks, coastal stacks) are left alone; genuine
     * departures pick up Sentry units via units_board_sentries_from_tile.
     */
    if (map_tile_is_land(map, land->x, land->y)) {
      continue;
    }
    for (int s = 0; s < COLONIZE_UNITS_MAX; ++s) {
      ColonizeUnit* ship = &units->units[s];
      if (!ship->active || ship->nation_id != land->nation_id) {
        continue;
      }
      if (!units_is_sea(units, ship->id) || ship->x != land->x || ship->y != land->y) {
        continue;
      }
      if (units_board_stacked(units, land->id, ship->id)) {
        break;
      }
    }
  }

  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* land = &units->units[i];
    if (!land->active || land->nation_id < 0 || land->nation_id > 3) {
      continue;
    }
    if (land->aboard_ship_id >= 0 || units_is_sea(units, land->id)) {
      continue;
    }
    if (col1_bridge_unit_is_missionary(units, land)) {
      continue;
    }
    if (land->muskets > 0 || land->horses > 0) {
      continue;
    }
    if (col1_bridge_tribe_at(save, land->x, land->y) < 0) {
      continue;
    }
    int dest_x = -1;
    int dest_y = -1;
    for (int rad = 1; rad <= 3 && dest_x < 0; ++rad) {
      for (int dy = -rad; dy <= rad; ++dy) {
        for (int dx = -rad; dx <= rad; ++dx) {
          if (abs(dx) != rad && abs(dy) != rad) {
            continue;
          }
          const int nx = land->x + dx;
          const int ny = land->y + dy;
          if (!map_tile_is_land(map, nx, ny)) {
            continue;
          }
          if (col1_bridge_tribe_at(save, nx, ny) >= 0) {
            continue;
          }
          if (colonies && colonies_id_at(colonies, nx, ny) >= 0) {
            continue;
          }
          if (!units_can_enter(units, land->type_index, map, nx, ny, land->id, colonies)) {
            continue;
          }
          dest_x = nx;
          dest_y = ny;
          break;
        }
        if (dest_x >= 0) {
          break;
        }
      }
    }
    if (dest_x < 0) {
      /* Fallback: any adjacent land, even if crowded. */
      for (int d = 0; d < 8; ++d) {
        const int nx = land->x + k_dx[d];
        const int ny = land->y + k_dy[d];
        if (map_tile_is_land(map, nx, ny) && col1_bridge_tribe_at(save, nx, ny) < 0) {
          dest_x = nx;
          dest_y = ny;
          break;
        }
      }
    }
    if (dest_x >= 0) {
      diag_info(
        "DOS sanitize: moved euro unit %d off village (%d,%d)->(%d,%d)",
        land->id,
        land->x,
        land->y,
        dest_x,
        dest_y
      );
      land->x = dest_x;
      land->y = dest_y;
    }
  }
}

bool col1_bridge_capture(
  ColonizeCol1Save* save,
  const ColonizeWorldMap* map,
  ColonizeUnitPool* units,
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

  col1_bridge_sanitize_units_for_dos(units, map, colonies, save);

  save->head.year = year;
  save->head.autumn = autumn;
  save->head.turn = (uint16_t)(turn_number > 0xffffu ? 0xffffu : turn_number);
  save->stuff.x = (uint16_t)cursor_x;
  save->stuff.y = (uint16_t)cursor_y;
  save->stuff.viewport_x = (uint16_t)cursor_x;
  save->stuff.viewport_y = (uint16_t)cursor_y;
  save->player[human_nation].control = 0;
  /* DOS 0x543f polarity: 0 = human. A stale save (pre-fix template) can carry
   * a second control==0 on nation 0 — DOS then runs England's turn as a HUMAN
   * (input stop, fog view flips to England; bugs.md 288). Heal: any other
   * Euro slot still at 0 becomes AI (1); withdrawn (2) is preserved. */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (n != human_nation && save->player[n].control == 0) {
      save->player[n].control = 1;
    }
  }
  /* DOS DS:0x5398 — the new-game path never stamped it, so a non-English
   * campaign saved human_player=0 forever (misleads every head.human_player
   * reader on later loads). */
  save->head.human_player = (uint16_t)human_nation;
  /* DOS DS:0x5394/0x5396 both equal the human slot in every DOS in-game save
   * (Dutch goldens carry 3/3/3) — saves only happen mid human Move Pieces.
   * Left at the template's 0, DOS resumes the nation-0 EOT as "human"
   * (immediate @PRICEUP dialog) and draws fog for the wrong viewer. */
  save->head.nation_turn = (uint16_t)human_nation;
  save->head.curr_nation_map_view = (uint16_t)human_nation;
  /* DS:0x53a4 SETVIEW override: DOS's turn loop pins the map view to it when
   * >= 0 (`view = 0x5398; if (0x53a4 >= 0) view = 0x53a4`). The port's old
   * zero-filled head locked DOS onto England's fog after the first EOT
   * (bugs.md 288, still_foggy.SAV). 0xffff = no override, as every DOS save
   * carries; heals stale campaigns on re-save. */
  save->head.fixed_nation_map_view = 0xffffu;
  save->head.show_entire_map = 0;
  /*
   * bugs.md interop (port_saves/interop pair): DOS's own in-game saves carry
   * DS:0x53c2 turn_loop_running = 1 and DS:0x53c4 map_modal_active = 1. The
   * port wrote 0/0, and DOS's IN-GAME load restores 0x53c2 into its running
   * main loop — the `while` gate reads 0 and DOS falls straight out to the
   * command line right after the "loaded COLONY##.SAV" popup ("crashes
   * VICEROY.EXE"; a main-menu load reinitializes the flag itself, which is
   * why that path worked). Mirror DOS's live values.
   */
  save->head.turn_loop_running = 1;
  save->head.map_modal_active = 1;
  /* bugs.md 234: a WoI save must carry the crown slot (DS:0x53d2) — with -1
   * DOS picks its own crown at load and can collide with the cached
   * intervention ally (0x53d4), mislabeling the intervention force "Tory".
   * Belt for campaigns declared before ai_king_do_declare stamped it. */
  if (save->head.game_options.woi && save->head.crown_nation_id < 0) {
    save->head.crown_nation_id = (int16_t)(human_nation == 0 ? 1 : 0);
  }

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

  /*
   * FUN_67f4_0088: new-game / blank templates need sea/land connectivity +
   * continent tallies. Preserve nonzero DOS post_map (and the 10 B tail).
   */
  if (col1_post_map_is_blank(&save->post_map)) {
    col1_post_map_rebuild_connectivity(&save->post_map, map);
  }

  /*
   * Nations / indian: live Col1 snapshot already holds AI diplo/contact blobs.
   * Refresh human economy from Europe UI; leave other nations' bytes intact.
   */
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
      nat->trade.nr[i] = europe->trade_nr[i];
    }
  }

  /* Rebuild colony list from live colonies; preserve Col1-only fields by xy. */
  {
    const ColonizeCol1Colony* old_colony = save->colony;
    const uint16_t old_count = save->head.colony_count;
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
      bool matched_old = false;
      if (old_colony) {
        for (uint16_t oi = 0; oi < old_count; ++oi) {
          if (old_colony[oi].x == (uint8_t)src->x && old_colony[oi].y == (uint8_t)src->y) {
            *dst = old_colony[oi];
            matched_old = true;
            break;
          }
        }
      }
      if (!matched_old) {
        /* DOS FUN_364b_1ba8 founding: divisor seeded to 100 (dividend 0) so a
         * brand-new colony's SoL% doesn't instantly saturate off one turn of
         * bells vs a tiny pop*2 divisor. */
        dst->rebel_divisor = 100;
      }
      dst->x = (uint8_t)src->x;
      dst->y = (uint8_t)src->y;
      str_copy_trunc(dst->name, sizeof(dst->name), src->name);
      dst->nation_id = (uint8_t)src->nation_id;
      dst->population = (uint8_t)(src->colonist_count > 32 ? 32 : src->colonist_count);
      dst->hammers = (uint16_t)(src->hammers < 0 ? 0 : (src->hammers > 65535 ? 65535 : src->hammers));
      /* Mirror of the import-side fix above: raw COL1 code == @BUILDING
       * file-order index directly, none is 0xFF. See col1_bridge_apply's
       * comment on this same field for the golden evidence. */
      if (src->building_in_production < 0) {
        dst->building_in_production = 0xFF;
      } else {
        dst->building_in_production = (uint8_t)src->building_in_production;
      }
      for (int p = 0; p < (int)COLONIZE_COL1_COLONY_POP_MAX; ++p) {
        dst->profession[p] = 0;
        dst->occupation[p] = 0;
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
        } else if (c->building_type >= 0 &&
                   c->building_type < COLONIZE_BUILDING_TYPES_MAX) {
          /*
           * bugs.md (starvation_bug.SAV): the occupation byte is the DOS
           * @JOB id, but this wrote the PORT's building-type index — the
           * import side then failed to map it, so every building worker
           * came back unassigned after a save round-trip and the colony
           * quietly stopped producing until it starved. Map the building
           * to its @JOB (mirror of the import chains above).
           */
          const char* bn = colonies->building_types[c->building_type].name;
          int occ = UNITS_JOB_COLONIST;
          if (bn && bn[0]) {
            if (strstr(bn, "Lumber") != NULL || strstr(bn, "Carpenter") != NULL) {
              occ = 13;
            } else if (strstr(bn, "Rum") != NULL) {
              occ = 9;
            } else if (strstr(bn, "Cigar") != NULL || strstr(bn, "Tobacconist") != NULL) {
              occ = 10;
            } else if (strstr(bn, "Textile") != NULL || strstr(bn, "Weaver") != NULL) {
              occ = 11;
            } else if (strstr(bn, "Fur") != NULL) {
              occ = 12;
            } else if (strstr(bn, "Iron") != NULL || strstr(bn, "Blacksmith") != NULL) {
              occ = 14;
            } else if (strstr(bn, "Arsenal") != NULL || strstr(bn, "Magazine") != NULL ||
                       strstr(bn, "Armory") != NULL) {
              occ = 15;
            } else if (strstr(bn, "Cathedral") != NULL || strstr(bn, "Church") != NULL) {
              occ = 16;
            } else if (strstr(bn, "Town Hall") != NULL) {
              occ = 17;
            } else if (strstr(bn, "University") != NULL || strstr(bn, "College") != NULL ||
                       strstr(bn, "School") != NULL) {
              occ = 18;
            }
          }
          dst->occupation[p] = (uint8_t)occ;
        } else {
          dst->occupation[p] = (uint8_t)UNITS_JOB_COLONIST;
        }
      }
      /*
       * bugs.md interop: DOS keeps each colony's colonist arrays SORTED by
       * occupation. The interop pair (generated_by_port.SAV vs DOS's own
       * resave of it) shows exactly an insert-before-first->= ordering:
       * ascending occupation with ties in REVERSE arrival order. Emit that
       * canonical order — occupation[], profession[], the specialty nibbles
       * below, and the tiles[] worker indices all permute together, so DOS
       * reads the same colony without reshuffling anything.
       */
      int col1_new_index[COLONIZE_COL1_COLONY_POP_MAX];
      {
        int order[COLONIZE_COL1_COLONY_POP_MAX]; /* new position -> old index */
        int cnt = 0;
        for (int p = 0; p < dst->population && p < (int)COLONIZE_COL1_COLONY_POP_MAX; ++p) {
          int pos = 0;
          while (pos < cnt && dst->occupation[order[pos]] < dst->occupation[p]) {
            pos++;
          }
          for (int m = cnt; m > pos; --m) {
            order[m] = order[m - 1];
          }
          order[pos] = p;
          cnt++;
        }
        uint8_t occ2[COLONIZE_COL1_COLONY_POP_MAX];
        uint8_t pro2[COLONIZE_COL1_COLONY_POP_MAX];
        for (int k = 0; k < (int)COLONIZE_COL1_COLONY_POP_MAX; ++k) {
          col1_new_index[k] = k;
        }
        for (int k = 0; k < cnt; ++k) {
          occ2[k] = dst->occupation[order[k]];
          pro2[k] = dst->profession[order[k]];
          col1_new_index[order[k]] = k;
        }
        for (int k = 0; k < cnt; ++k) {
          dst->occupation[k] = occ2[k];
          dst->profession[k] = pro2[k];
        }
      }
      /* Specialty nibbles: pack profession low nibble pairs (FUN_15eb_0c7a). */
      for (int s = 0; s < 16; ++s) {
        const int e = s * 2;
        const int o = e + 1;
        dst->specialty[s].even =
          (uint8_t)(e < dst->population ? (dst->profession[e] & 0x0fu) : 0u);
        dst->specialty[s].odd =
          (uint8_t)(o < dst->population ? (dst->profession[o] & 0x0fu) : 0u);
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
      for (int ti = 0; ti < (int)COLONIZE_COL1_COLONY_TILE_RING; ++ti) {
        dst->tiles[ti] = (int8_t)-1; /* DOS empty = 0xff */
      }
      for (int ti = (int)COLONIZE_COL1_COLONY_TILE_RING; ti < (int)COLONIZE_COL1_COLONY_TILES; ++ti) {
        dst->tiles[ti] = src->col1_outer_tiles[ti - (int)COLONIZE_COL1_COLONY_TILE_RING];
      }
      for (int rti = 0; rti < COLONIZE_COLONY_FIELD_TILES; ++rti) {
        const int who = (int)src->tiles[rti];
        if (who < 0 || who >= dst->population) {
          continue;
        }
        const int cti = col1_tile_index_from_runtime(rti);
        /* Remap through the DOS-canonical colonist ordering above. */
        dst->tiles[cti] = (int8_t)col1_new_index[who];
      }
      col1_encode_colony_buildings(colonies, src, &dst->buildings);
      dst->warehouse_level = src->warehouse_level > (uint8_t)dst->buildings.warehouse
                               ? src->warehouse_level
                               : (uint8_t)dst->buildings.warehouse;
      dst->capitol_level = src->capitol_level > (uint8_t)dst->buildings.capitol
                             ? src->capitol_level
                             : (uint8_t)dst->buildings.capitol;
      memcpy(dst->visible_to_euro, src->pop_on_map, sizeof(dst->visible_to_euro));
      memcpy(dst->fortification_on_map, src->fort_on_map, sizeof(dst->fortification_on_map));
      if (src->nation_id >= 0 && src->nation_id < 4 && dst->visible_to_euro[src->nation_id] == 0) {
        dst->visible_to_euro[src->nation_id] = 1; /* FUN_13f1_00a6 seed for the owner */
      }
      {
        uint16_t bits = src->custom_house_bits;
        memcpy(&dst->custom_house, &bits, sizeof(bits));
      }
      dst->labor_shortage = src->labor_shortage;
      dst->garrison_quota = src->garrison_quota;
      dst->specialty_cargo = src->specialty_cargo;
      dst->cargo_idle_turns = src->cargo_idle_turns;
      dst->improve_timer = src->improve_timer;
      dst->build_ai_flags = src->build_ai_flags;
      {
        uint8_t af = src->ai_flags;
        memcpy(&dst->ai_flags, &af, sizeof(af));
      }
      {
        uint8_t cf = src->colony_flags;
        memcpy(&dst->flags, &cf, sizeof(cf));
      }
      dst->cargo_produced_mask = src->cargo_produced_mask;
      dst->hammers_purchased = src->hammers_purchased;
      dst->depletion_counter = src->depletion_counter;
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
    /* Human Europe-screen ships (harbor / Expected / Bound) live only in the
     * EuropeScreen lists, not the pool — reserve room to write them back. */
    const int europe_ships =
      europe ? (europe->harbor_ships + europe->expected_ships + europe->bound_ships) : 0;
    const int capacity = live + europe_ships * (1 + EUROPE_SHIP_CARGO_MAX);
    ColonizeCol1Unit* neu = NULL;
    if (capacity > 0) {
      neu = calloc((size_t)capacity, sizeof(ColonizeCol1Unit));
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
      {
        /* DOS fog draw: (0x10<<viewer) & vis_mask. Live mask (reveal marks,
         * move resets); Euro owner bit always present, natives 0 until observed. */
        const int nat = src->nation_id & 0xF;
        uint8_t vis = (uint8_t)(src->col1_vis_mask & 0x0Fu);
        if (nat >= 0 && nat < 4) {
          vis = (uint8_t)(vis | (1u << (nat & 3)));
        }
        dst->vis_mask = vis;
      }
      {
        /*
         * Col1 +0x05 = moves_spent in thirds. Euro units export
         * max_mp - moves_left (idle transports / aboard units always 0, like
         * COLONY00); natives export the literal byte (Brave engine keeps DOS
         * spent in moves_left).
         */
        const ColonizeUnitType* ut = units_type(units, src->type_index);
        const bool transport = ut && ut->cargo > 0;
        if (src->nation_id >= 0 && src->nation_id <= 3) {
          const int max_mp = units_max_mp(units, src->id);
          int spent = 0;
          if (src->aboard_ship_id >= 0 ||
              (transport && (src->orders == UNITS_ORDER_SENTRY ||
                             src->orders == UNITS_ORDER_NONE ||
                             !units_orders_follow_goto(src->orders)))) {
            /* Idle transports (no goto/sail): always export full MP like COLONY00. */
            spent = 0;
          } else if (
            transport && src->orders == UNITS_ORDER_AI_MOVE && src->goto_x == src->x &&
            src->goto_y == src->y
          ) {
            /* Station-keep tip (TURN5 FR 52,43): COL1 moves spent = 0. */
            spent = 0;
          } else if (src->moves_left <= 0) {
            /* DOS clears a nation's spent bytes when its day ends (the TURN
             * goldens show 0 on every exhausted land unit); ships on a goto
             * keep their spent byte (savegame.md). Exhausted land units
             * therefore export 0 (= full on reload, as before). */
            spent = transport ? max_mp : 0;
          } else if (src->moves_left < max_mp) {
            spent = max_mp - src->moves_left;
          }
          dst->moves = (uint8_t)(spent < 0 ? 0 : (spent > 255 ? 255 : spent));
        } else {
          dst->moves = (uint8_t)(src->moves_left < 0 ? 0 : src->moves_left);
        }
      }
      if (src->aboard_ship_id >= 0) {
        dst->orders = 1; /* sentry if aboard */
      } else if (src->orders != 0) {
        dst->orders = (uint8_t)src->orders;
      } else {
        dst->orders = 0;
      }
      /*
       * Goto: idle on-map ships must use goto==xy (COLONY00). Human starter
       * cleared ORDER_GOTO but left landfall in goto — DOS peeled the caravel
       * out of transport_chain on select/move. Do not rewrite land/Brave goto
       * (TURN AI goldens keep landfall after unload). Europe keeps landfall.
       */
      {
        const ColonizeUnitType* ut_goto = units_type(units, src->type_index);
        const bool transport = ut_goto && ut_goto->cargo > 0;
        const bool follow = units_orders_follow_goto(src->orders);
        const bool in_europe = col1_coord_is_europe((uint8_t)src->x, (uint8_t)src->y);
        const bool native = (src->nation_id & 0xF) >= 4;
        const bool goto_none = src->goto_x == UNITS_GOTO_NONE || src->goto_y == UNITS_GOTO_NONE ||
                               src->goto_x < 0 || src->goto_y < 0;
        if (transport && !follow && !in_europe) {
          dst->goto_x = dst->x;
          dst->goto_y = dst->y;
        } else if (goto_none || (native && !follow)) {
          dst->goto_x = 0;
          dst->goto_y = 0;
        } else {
          dst->goto_x = (uint8_t)src->goto_x;
          dst->goto_y = (uint8_t)src->goto_y;
        }
      }
      {
        /* FUN_1427_06b4: transports (cargo>0) always profession 0. */
        const ColonizeUnitType* ut = units_type(units, src->type_index);
        if (ut && ut->cargo > 0) {
          dst->profession = 0;
        } else {
          dst->profession = (uint8_t)(src->profession < 0 ? UNITS_JOB_NONE : src->profession);
        }
      }
      dst->turns_worked =
        (uint8_t)(src->turns_worked < 0 ? 0 : (src->turns_worked > 255 ? 255 : src->turns_worked));
      /* Unpack col1_unknown15's raw byte into the 8 named single-bit fields
       * (was one unknown15_lo:7 blob; see col1_save.h). */
      dst->unknown15_bit0 = (src->col1_unknown15 & 0x01u) != 0 ? 1u : 0u;
      dst->roam_reeval_pending = (src->col1_unknown15 & 0x02u) != 0 ? 1u : 0u;
      dst->stack_has_founders_or_military = (src->col1_unknown15 & 0x04u) != 0 ? 1u : 0u;
      dst->stack_has_military = (src->col1_unknown15 & 0x08u) != 0 ? 1u : 0u;
      dst->wander_dest_chosen = (src->col1_unknown15 & 0x10u) != 0 ? 1u : 0u;
      dst->garrison_request_pending = (src->col1_unknown15 & 0x20u) != 0 ? 1u : 0u;
      dst->bound_in_transit = (src->col1_unknown15 & 0x40u) != 0 ? 1u : 0u;
      dst->ship_damaged = (src->col1_unknown15 & 0x80u) != 0 ? 1u : 0u;
      /* Euros never carry a home-tribe origin; natives use tribe index / 0xff. */
      if ((src->nation_id & 0xF) < 4) {
        dst->origin = 0xff;
      } else {
        dst->origin =
          (uint8_t)(src->home_tribe_id < 0 || src->home_tribe_id > 255 ? 0xff
                                                                      : (src->home_tribe_id & 0xff));
      }
      dst->ai_plan =
        src->col1_ai_plan != 0 ? src->col1_ai_plan : COL1_UNIT_UNKNOWN16_HI_DEFAULT;
      dst->facing = (uint8_t)(src->last_dir & 7);
      dst->facing_pad = 0;
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
        /* Goods only — passengers live in transport_chain (not hold slots). */
        dst->holds_occupied = (uint8_t)gi;
      }
      /* DOS pioneer tools = cargo_hold[5] (FUN_479b_0158). Keep actual count. */
      if (src->tools > 0 && src->tools <= 100 && !units_is_sea(units, src->id)) {
        dst->cargo_hold[5] = (uint8_t)src->tools;
      } else if (src->tools > 0 && src->aboard_ship_id >= 0 && dst->cargo_hold[5] == 0) {
        dst->cargo_hold[5] = (uint8_t)(src->tools <= 100 ? src->tools : 100);
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

    /*
     * bugs.md interop: DOS tile stacks ARE this chain. FUN_1427_02ca (place
     * unit) appends every unit to its tile's doubly-linked list
     * (+0x315c next / +0x315e prev) and the whole engine walks it — the
     * "there are multiple units here" tab, the stack popup, colony troop
     * lists, ship-passenger pooling. Port-created units carried -1/-1, so
     * in DOS every one of them looked like a loose single (moveable via the
     * control queue, invisible as a stack). Rebuild the full per-tile chain
     * for every on-map unit: land units first in array order, ships last —
     * which also reproduces the pax0→…→ship shape the passenger wiring
     * above produced (a land unit on a sea tile is DOS's "aboard", pooled
     * per tile first-come, exactly the ship-switch quirk). Europe-sentinel
     * records (x ≥ 200) keep their dedicated lane chains.
     */
    {
      bool* chained = calloc((size_t)(written > 0 ? written : 1), sizeof(bool));
      if (chained) {
        for (int i = 0; i < written; ++i) {
          if (chained[i] || neu[i].x >= 200 || neu[i].y >= 200) {
            continue;
          }
          int group[COLONIZE_UNITS_MAX];
          int gn = 0;
          /* Land units first, ships after (both in array order). */
          for (int pass = 0; pass < 2; ++pass) {
            for (int j = i; j < written; ++j) {
              if (chained[j] || neu[j].x != neu[i].x || neu[j].y != neu[i].y) {
                continue;
              }
              const bool is_ship =
                neu[j].type >= 0x0d && neu[j].type <= 0x12; /* Caravel..Man-O-War */
              if ((pass == 0) == is_ship) {
                continue;
              }
              if (gn < COLONIZE_UNITS_MAX) {
                group[gn++] = j;
              }
            }
          }
          for (int g = 0; g < gn; ++g) {
            chained[group[g]] = true;
            neu[group[g]].transport_chain.prev_unit_idx =
              (int16_t)(g > 0 ? group[g - 1] : -1);
            neu[group[g]].transport_chain.next_unit_idx =
              (int16_t)(g + 1 < gn ? group[g + 1] : -1);
          }
        }
        free(chained);
      }
    }

    /*
     * Human Europe-screen ships. DOS keeps them as unit records at the
     * nation's Europe sentinel diagonal (FUN_48d3_007a / 0346 / 03d0):
     *   228+n  in port (harbor)
     *   232+n  sailing to the New World (Bound), goto = landfall
     *   244+n  sailing to Europe (Expected), goto = the exit tile
     * with `turns_worked` (+0x16) = voyage turns left and passengers chained
     * pax0→pax1→…→ship, sharing x/y/goto/turns. Harbor passengers were
     * already disembarked to the docks on arrival (their (236,236) mirror
     * units are in the pool), so only Expected/Bound carry cargo here.
     */
    if (europe && human_nation >= 0 && human_nation < 4) {
      const uint8_t n = (uint8_t)human_nation;
      const struct {
        const EuropeHarborShip* list;
        int count;
        uint8_t xy;
      } lanes[3] = {
        {europe->harbor, europe->harbor_ships, (uint8_t)(228 + n)},
        {europe->bound, europe->bound_ships, (uint8_t)(232 + n)},
        {europe->expected, europe->expected_ships, (uint8_t)(244 + n)},
      };
      for (int li = 0; li < 3; ++li) {
        for (int si = 0; si < lanes[li].count; ++si) {
          const EuropeHarborShip* ship = &lanes[li].list[si];
          int ship_ti = ship->type_index;
          if (ship_ti < 0) {
            ship_ti = units_find_type(units, ship->name);
          }
          if (ship_ti < 0 || written + 1 + ship->cargo_count > capacity) {
            continue;
          }
          const bool in_port = lanes[li].xy == (uint8_t)(228 + n);
          const uint8_t gx = (uint8_t)(in_port ? 0 : ship->exit_x);
          const uint8_t gy = (uint8_t)(in_port ? 0 : ship->exit_y);
          const uint8_t turns = (uint8_t)(in_port ? 0 : (ship->turns_left < 0 ? 0 : ship->turns_left));
          int last = -1;
          for (int c = 0; c < ship->cargo_count && c < EUROPE_SHIP_CARGO_MAX; ++c) {
            int pti = ship->cargo_types[c];
            if (pti == -2) {
              pti = units_find_type(units, "Artillery");
            }
            if (pti < 0) {
              continue;
            }
            ColonizeCol1Unit* px = &neu[written];
            memset(px, 0, sizeof(*px));
            px->x = lanes[li].xy;
            px->y = lanes[li].xy;
            px->type = (uint8_t)pti;
            px->nation_id = n;
            px->vis_mask = (uint8_t)(1u << n);
            px->ai_plan = COL1_UNIT_UNKNOWN16_HI_DEFAULT;
            px->origin = 0xff;
            px->orders = 1; /* sentry aboard */
            px->goto_x = gx;
            px->goto_y = gy;
            px->turns_worked = turns;
            {
              const ColonizeUnitType* put = units_type(units, pti);
              const bool treasure = put && strcmp(put->name, "Treasure") == 0;
              const int prof = ship->cargo_professions[c];
              if (treasure) {
                const int gold = ship->cargo_treasure_gold[c];
                px->profession = (uint8_t)(gold > 0 ? (gold / 100 > 255 ? 255 : gold / 100) : 0);
              } else {
                px->profession = (uint8_t)(prof < 0 ? 0 : prof);
              }
            }
            px->transport_chain.prev_unit_idx = (int16_t)last;
            px->transport_chain.next_unit_idx = -1;
            if (last >= 0) {
              neu[last].transport_chain.next_unit_idx = (int16_t)written;
            }
            last = written;
            written++;
          }
          ColonizeCol1Unit* dst = &neu[written];
          memset(dst, 0, sizeof(*dst));
          dst->x = lanes[li].xy;
          dst->y = lanes[li].xy;
          dst->type = (uint8_t)ship_ti;
          dst->nation_id = n;
          dst->vis_mask = (uint8_t)(1u << n);
          dst->ai_plan = COL1_UNIT_UNKNOWN16_HI_DEFAULT;
          dst->orders = 0;
          dst->goto_x = gx;
          dst->goto_y = gy;
          dst->turns_worked = turns;
          {
            int gi = 0;
            for (int h = 0; h < EUROPE_SHIP_CARGO_MAX && gi < 6; ++h) {
              const int amt = ship->hold_goods_amount[h];
              int t = ship->hold_goods_type[h];
              if (amt <= 0) {
                continue;
              }
              if (t < 0) {
                t = 0;
              }
              if (t > 15) {
                t = 15;
              }
              dst->cargo_hold[gi] = (uint8_t)(amt > 255 ? 255 : amt);
              switch (gi) {
                case 0: dst->cargo_item_0 = (uint8_t)t; break;
                case 1: dst->cargo_item_1 = (uint8_t)t; break;
                case 2: dst->cargo_item_2 = (uint8_t)t; break;
                case 3: dst->cargo_item_3 = (uint8_t)t; break;
                case 4: dst->cargo_item_4 = (uint8_t)t; break;
                default: dst->cargo_item_5 = (uint8_t)t; break;
              }
              gi++;
            }
            dst->holds_occupied = (uint8_t)gi;
          }
          dst->transport_chain.prev_unit_idx = (int16_t)last;
          dst->transport_chain.next_unit_idx = -1;
          if (last >= 0) {
            neu[last].transport_chain.next_unit_idx = (int16_t)written;
          }
          written++;
        }
      }
      /* FUN_48d3_007a stamps the sail-to-Europe exit as the return landfall. */
      if (europe->last_exit_valid) {
        save->nation[n].return_from_europe_x = (uint8_t)europe->last_exit_x;
        save->nation[n].return_from_europe_y = (uint8_t)europe->last_exit_y;
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
  /* Density: suppress / purchased / pacific (FUN_684c_08c0 / FUN_137f_015e). */
  col1_bridge_sync_map_density(save, map);

  /* Blank-template census only — never freshen mid-campaign lag. */
  if (col1_stuff_census_window_is_blank(&save->stuff)) {
    col1_stuff_census_fill_blank(&save->stuff, units, colonies);
  }

  /* Mid-campaign: do not leave discovery unset for DOS woodcut re-fire. */
  col1_bridge_sync_new_world_discovery(save, map, human_nation);

  if (err && err_size) {
    err[0] = '\0';
  }
  return true;
}

void col1_bridge_mark_new_world_discovered(ColonizeCol1Save* save, int human_nation) {
  if (!save || human_nation < 0 || human_nation > 3) {
    return;
  }
  save->head.event.discovery_of_the_new_world = 1;
  save->player[human_nation].named_new_world = 1;
}

bool col1_bridge_human_has_seen_land(const ColonizeWorldMap* map, int human_nation) {
  if (!map || !map->terrain || !map->seen || human_nation < 0 || human_nation > 3) {
    return false;
  }
  const int w = (int)map->width;
  const int h = (int)map->height;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      if (!map_tile_seen_by(map, x, y, human_nation)) {
        continue;
      }
      if (map_tile_is_land(map, x, y)) {
        return true;
      }
    }
  }
  return false;
}

void col1_bridge_sync_new_world_discovery(
  ColonizeCol1Save* save,
  const ColonizeWorldMap* map,
  int human_nation
) {
  if (!save || human_nation < 0 || human_nation > 3) {
    return;
  }
  if (save->head.event.discovery_of_the_new_world &&
      save->player[human_nation].named_new_world) {
    return;
  }
  if (!col1_bridge_human_has_seen_land(map, human_nation)) {
    return;
  }
  col1_bridge_mark_new_world_discovered(save, human_nation);
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
      if (save->indian[indian].euro_diplo[european_nation] == 0) {
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
