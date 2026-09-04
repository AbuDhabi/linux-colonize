#include "core/colony.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/popup_msg.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
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

int colonies_fortification_tier(const ColonizeColonyPool* pool, const ColonizeColony* c) {
  switch (colony_map_icon(pool, c)) {
    case COLONY_MAP_ICON_FORTRESS:
      return 3;
    case COLONY_MAP_ICON_FORT:
      return 2;
    case COLONY_MAP_ICON_STOCKADE:
      return 1;
    default:
      return 0;
  }
}

void colonies_fog_snapshot(ColonizeColonyPool* pool, int colony_id, int nation_id) {
  ColonizeColony* c = colonies_get_mut(pool, colony_id);
  if (!c || !c->active || nation_id < 0 || nation_id > 3) {
    return;
  }
  c->pop_on_map[nation_id] = (uint8_t)(c->population > 255 ? 255 : (c->population < 0 ? 0 : c->population));
  c->fort_on_map[nation_id] = (uint8_t)colonies_fortification_tier(pool, c);
}

bool colonies_known_to(const ColonizeColony* c, int nation_id, bool show_entire_map) {
  if (!c || nation_id < 0 || nation_id > 3) {
    return true;
  }
  return c->nation_id == nation_id || show_entire_map || c->pop_on_map[nation_id] != 0;
}

/* FUN_13f1_00a6 proper: the ±5 sweep for one nation around one colony. */
static void colonies_reveal_ring5(
  ColonizeWorldMap* map,
  ColonizeColonyPool* pool,
  const ColonizeColony* c,
  int nation
) {
  for (int y = c->y - 5; y <= c->y + 5; ++y) {
    for (int x = c->x - 5; x <= c->x + 5; ++x) {
      if (!map_coords_inset(map, x, y)) {
        continue;
      }
      map_reveal_tile(map, x, y, nation);
      const int other = colonies_id_at(pool, x, y);
      ColonizeColony* oc = colonies_get_mut(pool, other);
      if (oc && oc->active && oc->pop_on_map[nation] == 0) {
        oc->pop_on_map[nation] = 1;
        oc->fort_on_map[nation] = 0;
      }
    }
  }
}

void colonies_reveal_founded(
  ColonizeWorldMap* map,
  ColonizeColonyPool* pool,
  const ColonizeCol1Save* col1,
  int colony_id
) {
  const ColonizeColony* c = colonies_get(pool, colony_id);
  if (!map || !col1 || !c || !c->active || c->nation_id < 0 || c->nation_id > 3) {
    return;
  }
  /*
   * FUN_364b_1dd6: `for n in 0..3: if FUN_15eb_3960(n, 6) then
   * FUN_13f1_00a6(new_colony, n)`. Gated on Coronado per nation — every
   * Coronado owner sees around the new colony, not just its founder.
   */
  for (int nation = 0; nation < 4; ++nation) {
    const int byte_i = FF_FRANCISCO_CORONADO / 8;
    const int bit_i = FF_FRANCISCO_CORONADO % 8;
    if (((col1->nation[nation].founding_fathers[byte_i] >> bit_i) & 1) == 0) {
      continue;
    }
    colonies_reveal_ring5(map, pool, c, nation);
  }
}

int colonies_settlement_icon(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  return colony_map_icon(pool, colony);
}

/*
 * ICONS.SS #0-3's stored flag: 15 fixed (dx,dy) pixels, identical across
 * all four fortification tiers (dumped directly from ICONS.SS — see
 * unit_chrome_nation_flag_shades_for_palette's comment for why this needs
 * recoloring at all). is_dark selects which of the two nation shades that
 * pixel gets.
 */
typedef struct ColonyIconFlagPixel {
  int8_t dx;
  int8_t dy;
  bool is_dark;
} ColonyIconFlagPixel;

static const ColonyIconFlagPixel k_colony_icon_flag_pixels[15] = {
  {6, 0, true}, {7, 0, false}, {8, 0, false},
  {6, 1, true}, {7, 1, false}, {8, 1, false}, {9, 1, false},
  {6, 2, true}, {7, 2, false}, {8, 2, false}, {9, 2, false}, {10, 2, false},
  {8, 3, true}, {9, 3, false}, {10, 3, false}
};

void colonies_blit_settlement_icon(
  const ColonizeSpriteSheet* icons,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int px,
  int py,
  int nation_id,
  const ColonizePalette* active_palette
) {
  if (!icons || sprite < 0 || sprite >= icons->sprite_count || !framebuffer || !framebuffer->pixels) {
    return;
  }
  const ColonizeSprite* sp = &icons->sprites[sprite];
  if (!sp->pixels || sp->width <= 0 || sp->height <= 0) {
    return;
  }
  ss_blit_sprite(icons, sprite, framebuffer, px, py);

  int light = -1;
  int dark = -1;
  unit_chrome_nation_flag_shades_for_palette(nation_id, active_palette, &light, &dark);
  if (light < 0 && dark < 0) {
    return;
  }
  /* bugs.md: the rebel nation flies an actual striped American flag — navy
   * hoist edge, alternating red/white stripe rows — not a plain white flag
   * ("we are not surrendering just yet"). */
  int us_navy = -1;
  int us_red = -1;
  int us_white = -1;
  const bool rebel = nation_id >= 0 && nation_id == unit_chrome_rebel_nation();
  if (rebel) {
    unit_chrome_rebel_flag_colors_for_palette(active_palette, &us_navy, &us_red, &us_white);
  }
  for (int i = 0; i < 15; ++i) {
    const ColonyIconFlagPixel* fp = &k_colony_icon_flag_pixels[i];
    int color = fp->is_dark ? dark : light;
    if (rebel && us_navy >= 0) {
      color = fp->is_dark ? us_navy : ((fp->dy & 1) ? us_white : us_red);
    }
    if (color < 0) {
      continue;
    }
    const int fx = px + fp->dx;
    const int fy = py + fp->dy;
    if (fx < 0 || fy < 0 || fx >= framebuffer->width || fy >= framebuffer->height) {
      continue;
    }
    framebuffer->pixels[fy * framebuffer->width + fx] = (uint8_t)color;
  }
}

static void colony_blit_map_icon(
  const ColonizeSpriteSheet* icons,
  int sprite,
  ColonizeFramebuffer8* framebuffer,
  int tile_px,
  int tile_py,
  int tile_w,
  int tile_h,
  int nation_id,
  const ColonizePalette* active_palette
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
  colonies_blit_settlement_icon(icons, sprite, framebuffer, px, py, nation_id, active_palette);
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
    memset(pool->colonies[i].col1_outer_tiles, 0xff, sizeof(pool->colonies[i].col1_outer_tiles));
    pool->colonies[i].specialty_cargo = 0xff; /* Col1 +0x8d none */
  }
}

bool colonies_load_names(ColonizeColonyPool* pool, const char* colony_txt_path) {
  if (!pool || !colony_txt_path) {
    return false;
  }
  for (int n = 0; n < 4; ++n) {
    pool->name_count[n] = 0;
    pool->name_next[n] = 0;
  }

  FILE* f = fopen(colony_txt_path, "r");
  if (!f) {
    diag_warn("Cannot open %s for colony names", colony_txt_path);
    return false;
  }

  char line[128];
  int section = -1; /* 0 English, 1 French, 2 Spanish, 3 Dutch */
  while (fgets(line, sizeof(line), f)) {
    colony_trim(line);
    if (line[0] == '@') {
      if (strncmp(line + 1, "ENGLISH", 7) == 0) {
        section = 0;
      } else if (strncmp(line + 1, "FRENCH", 6) == 0) {
        section = 1;
      } else if (strncmp(line + 1, "SPANISH", 7) == 0) {
        section = 2;
      } else if (strncmp(line + 1, "DUTCH", 5) == 0) {
        section = 3;
      } else {
        section = -1; /* @STOP or unknown */
      }
      continue;
    }
    if (section < 0 || section > 3) {
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
    if (pool->name_count[section] >= COLONIZE_COLONY_NAMES_MAX) {
      continue;
    }
    str_copy_trunc(
      pool->names[section][pool->name_count[section]], COLONIZE_COLONY_NAME_MAX, line
    );
    pool->name_count[section]++;
  }
  fclose(f);
  diag_info(
    "Loaded colony names EN=%d FR=%d SP=%d DU=%d from %s",
    pool->name_count[0],
    pool->name_count[1],
    pool->name_count[2],
    pool->name_count[3],
    colony_txt_path
  );
  return pool->name_count[0] > 0 || pool->name_count[1] > 0 || pool->name_count[2] > 0 ||
         pool->name_count[3] > 0;
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
    int size = 0;
    int min_pop = 0;
    int upkeep = 0;
    /* NAMES.TXT: name, cost, tools(*10), size, min_colony, upkeep */
    sscanf(p, " %d , %d , %d , %d , %d", &hammers, &tools_cost, &size, &min_pop, &upkeep);
    (void)size;
    (void)upkeep;

    ColonizeBuildingType* t = &pool->building_types[pool->building_type_count++];
    str_copy_trunc(t->name, sizeof(t->name), line);
    t->hammers = hammers;
    /* NAMES.TXT tools(*10): file stores tens of tools (2 → 20 tools). */
    t->tools_cost = tools_cost * 10;
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
  /*
   * Arctic (pedia 24) and mountains (pedia 27) are not colonizable.
   * Hills (28) are valid. Cite: Colonization.pdf; GAME.TXT @TOOMOUNTAIN;
   * docs/terrain_yields.md.
   */
  {
    const int pedia = map_pedia_terrain_index_at(map, x, y);
    if (pedia == 24 || pedia == 27) {
      return false;
    }
  }
  /*
   * Distance gate: A colony cannot be founded on a square adjacent to
   * (Chebyshev distance <= 1, i.e. dx <= 1 && dy <= 1) ANY existing active
   * colony (own or foreign).
   *
   * Cite (corrected 2026-08-24 — the previous citation of FUN_2b5a_3252 was
   * wrong; that function is the numpad/arrow-key movement dispatcher, not
   * Build Colony, confirmed by a clean overlay-project decompile that
   * contains no call anywhere near this logic). Real DOS chain, traced via
   * tools/GhidraDecompileAt.java + tools/GhidraListXRefs.java against the
   * OvlWork/Ovl overlay Ghidra project (canonical viceroy_unpacked.c's
   * export of this address range is corrupted -- WARNING: jumptable/
   * EMS-mapping garbage, same false-alarm class ai_port_plan.md's Method
   * notes warn about): the Build Colony order handler (OVL02_L0000
   * offset 0x16ce, canonical FUN_2b5a_16ce, inside the larger
   * ENTER-prologue function at FUN_2b5a_1662, an undocumented gap in
   * FUNCTION_CATALOG.md between FUN_2b5a_1454 and FUN_2b5a_199e) calls
   * FUN_1000_8804 (thin resident thunk) -> FUN_15eb_0142 / FUN_0000_5ff2
   * ("nearest colony" utility, called with type=-1/nation=-1 i.e. any
   * nation, any colony type) whose returned distance (DS:0x8db8, via the
   * FUN_0000_2500 metric: max(|dx|,|dy|) + min(|dx|,|dy|)/2, which
   * evaluates to exactly 1 for all 8 Chebyshev-adjacent neighbor tiles and
   * to 0 only for the same tile) is compared == 1; on match the winning
   * colony's name is formatted into a dialog string-substitution slot
   * (FUN_1000_8606) and a bounce message (opaque numeric GAME.TXT id
   * 0x9a5 -- popup_string_resolver.md documents these ids don't resolve to
   * a @TAG statically, needs a live capture) aborts the order. This id is
   * mid-cluster among 3 sibling hard-reject "bounce" ids in the same
   * function that line up structurally with @TOOMOUNTAIN (terrain==0x1b
   * gate, 2 ids later) and @TOONEARBUILD (a 9-tile neighbor scan for a
   * stacked unit with order==7 pending, 1 id later) -- @TOONEAR is the
   * only one of the three whose gate condition (nearest-colony distance)
   * and dialog substitution (colony name) match GAME.TXT's own @TOONEAR
   * text ("too near to {colony}") exactly, which is why the id ordering
   * plus semantics together (not just the id) pin it down without needing
   * the live capture. Distance==1 is exactly the Chebyshev dx<=1&&dy<=1
   * ring below (same tile, dx=dy=0, yields metric 0 and is instead caught
   * by the separate occupied-tile case already folded into this same
   * loop) -- this confirms the already-shipped dx<=1&&dy<=1 formula, it
   * was not an invented threshold.
   */
  for (int i = 0; i < pool->colony_count; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (c->active) {
      const int dx = abs(c->x - x);
      const int dy = abs(c->y - y);
      if (dx <= 1 && dy <= 1) {
        return false;
      }
    }
  }
  /* Indian village tiles carry layer2 has_city without a colony row. */
  if (map->layer2) {
    const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
    if (idx < (size_t)map->width * (size_t)map->height &&
        (map->layer2[idx] & MAP_OCCUPANCY_HAS_CITY) != 0) {
      return false;
    }
  }
  return true;
}

static const char* colonies_next_name(ColonizeColonyPool* pool, int nation_id) {
  if (!pool) {
    return "New Colony";
  }
  if (nation_id < 0 || nation_id > 3) {
    nation_id = 0;
  }
  if (pool->name_count[nation_id] == 0) {
    return "New Colony";
  }
  const char* n =
    pool->names[nation_id][pool->name_next[nation_id] % pool->name_count[nation_id]];
  pool->name_next[nation_id]++;
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

/* DOS FUN_124c_0040 / ai_dos_dist — diagonal-aware tile distance. */
static int colonies_dos_dist(int dx, int dy) {
  if (dx < 0) {
    dx = -dx;
  }
  if (dy < 0) {
    dy = -dy;
  }
  if (dy < dx) {
    return (dy >> 1) + dx;
  }
  return (dx >> 1) + dy;
}

/*
 * FUN_4cc6_0356: nearest village index; *out_dist = DOS distance (DS:0x8db8).
 * DOS filters by continent (param_4): villages on another landmass never claim
 * a tile. Which tile supplies that continent differs per call site — the unit
 * gates (FUN_479b_043b / _0687) pass the unit's own tile, the colony-screen
 * claim table (FUN_15eb_26e4) passes the COLONY's tile for all 25 cells — so
 * `continent` is the caller's to choose. -1 (or a NULL map) = no filter, same
 * as DOS's param_4 < 0 arm.
 */
static int colonies_nearest_tribe_on(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int continent,
  int* out_dist
) {
  int best = -1;
  int best_d = 9999;
  if (!col1 || !col1->tribe) {
    if (out_dist) {
      *out_dist = best_d;
    }
    return -1;
  }
  for (uint16_t i = 0; i < col1->head.tribe_count; ++i) {
    const ColonizeCol1Tribe* t = &col1->tribe[i];
    if (continent >= 0 && map) {
      const int tc = map_continent_id_at(map, (int)t->x, (int)t->y);
      if (tc >= 0 && tc != continent) {
        continue;
      }
    }
    const int d = colonies_dos_dist(x - (int)t->x, y - (int)t->y);
    if (d <= best_d) {
      best_d = d;
      best = (int)i;
    }
  }
  if (out_dist) {
    *out_dist = best_d;
  }
  return best;
}

/*
 * FUN_15eb_26e4 5x5 native-contact cache rule (viceroy_unpacked.c ~12850):
 * a tile is tribal land when the nearest village (same continent) lies within
 * FUN_15dc_006a(tribe) = tech tier: tech 0/1 → 1, tech 2 → 2, tech 3 → 3.
 * (Was "capital ? 2 : 1" from the manual; DOS keys the radius on the
 * tribe's civilization level, not on the capital flag.)
 */
static int colonies_indian_land_radius(const ColonizeCol1Save* col1, const ColonizeCol1Tribe* t) {
  if (!col1 || !t) {
    return 1;
  }
  const int idx = (int)t->nation_id - 4;
  if (idx < 0 || idx >= (int)COLONIZE_COL1_INDIAN_COUNT) {
    return 1;
  }
  const unsigned tech = (unsigned)col1->indian[idx].tech;
  if (tech <= 1u) {
    return 1;
  }
  return tech == 2u ? 2 : 3;
}

static int colonies_tile_indian_homeland_on(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int continent,
  int* out_tribe,
  int* out_dist
) {
  int dist = 9999;
  const int ti = colonies_nearest_tribe_on(col1, map, x, y, continent, &dist);
  if (out_tribe) {
    *out_tribe = ti;
  }
  if (out_dist) {
    *out_dist = dist;
  }
  if (ti < 0 || !col1 || !col1->tribe) {
    return 0;
  }
  const int radius = colonies_indian_land_radius(col1, &col1->tribe[ti]);
  return dist <= radius ? 1 : 0;
}

/* DOS 479b arm: the queried tile supplies its own continent. */
static int colonies_tile_indian_homeland(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int* out_tribe,
  int* out_dist
) {
  return colonies_tile_indian_homeland_on(
    col1, map, x, y, map ? map_continent_id_at(map, x, y) : -1, out_tribe, out_dist
  );
}

static int colonies_indian_land_purchase_gold_on(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int continent,
  int nation_id
) {
  if (!col1 || nation_id < 0 || nation_id > 3) {
    return 0;
  }
  /*
   * Already gifted / bought tribal land (layer2 MAP_LAYER2_PURCHASED, or Col1
   * mask bit 0x10). First-contact WELCOME grant stamps this; founding must not
   * charge again. Cite: GAME.TXT @INDIANWELCOME; FUN_281f_068c.
   */
  if (map && map->layer2 && map_coords_inset(map, x, y)) {
    const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
    if (idx < map->tile_count && (map->layer2[idx] & MAP_LAYER2_PURCHASED) != 0) {
      return 0;
    }
  } else if (col1->map.mask && col1->head.map_size_x > 0 && x >= 0 && y >= 0) {
    const size_t idx = (size_t)y * (size_t)col1->head.map_size_x + (size_t)x;
    if (idx < col1->map.tile_count && (col1->map.mask[idx] & 0x10u) != 0) {
      return 0;
    }
  }
  int tribe_i = -1;
  int dist = 9999;
  if (!colonies_tile_indian_homeland_on(col1, map, x, y, continent, &tribe_i, &dist)) {
    return 0;
  }
  /* Colonization.pdf / FUN_4cc6_07c2: Peter Minuit (FF 2) → cost 0. */
  if (founding_fathers_nation_has(col1, nation_id, FF_PETER_MINUIT)) {
    return 0;
  }

  const ColonizeCol1Tribe* tribe = &col1->tribe[tribe_i];
  const int indian_idx = (int)tribe->nation_id - 4;
  const ColonizeCol1Indian* ind =
    (indian_idx >= 0 && indian_idx < (int)COLONIZE_COL1_INDIAN_COUNT) ? &col1->indian[indian_idx]
                                                                     : NULL;
  /* indian+2 tech; indian+5 lands-bought counter (decomp INC on purchase). */
  const unsigned tech = ind ? (unsigned)ind->tech : 0u;
  const unsigned bought = ind ? (unsigned)ind->lands_bought : 0u;
  const unsigned diff = (unsigned)col1->head.difficulty;
  const int is_human =
    (nation_id < 4 && col1->player[nation_id].control == 0);

  int score;
  int scale;
  if (is_human) {
    /* ((difficulty+3)*2 + tech + bought) - dist; scale 0x41. */
    score = (int)((diff + 3u) * 2u + tech + bought) - dist;
    scale = 0x41;
  } else {
    /* (tech + bought - difficulty) - dist + 0xc; scale 0x32. */
    score = (int)(tech + bought) - (int)diff - dist + 0xc;
    scale = 0x32;
  }
  /* PARKED: full −0x6bf0 / 0x9410 per-nation table adjust (decomp SAR). */
  if (score < 1) {
    score = 1;
  }
  int cost = scale * score;
  /* Human: *(tension+1); tension stand-in 0 until 0a60 wired. */
  if (is_human) {
    cost = (0 + 1) * cost;
  }
  if (tribe->state.capital) {
    cost = cost + (cost >> 1);
  }
  return cost >> 1;
}

int colonies_indian_land_purchase_gold(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id
) {
  return colonies_indian_land_purchase_gold_on(
    col1, map, x, y, map ? map_continent_id_at(map, x, y) : -1, nation_id
  );
}

int colonies_indian_land_owner_tribe(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y
) {
  int tribe_i = -1;
  if (!colonies_tile_indian_homeland(col1, map, x, y, &tribe_i, NULL)) {
    return -1;
  }
  return tribe_i;
}

/*
 * bugs.md 290 — DOS FUN_15eb_26e4 (colony screen 5x5 Indian-land table):
 * a tile shows the totem / triggers a work complaint when a village's
 * tech-tier radius covers it, the tribe has been MET (bit 0x20), the land
 * has not been bought (purchase price > 0 also folds in Peter Minuit — the
 * whole table empties with FF 2), and no colony sits on it. Returns the
 * claiming tribe index, or -1.
 *
 * bugs.md 372 — two rules the first port of the table dropped, both visible as
 * totems in the wrong place:
 *  - `FUN_13e4_0074(tile)` clears the slot on terrain index 25/26 (Ocean and
 *    Sea Lane), so a coastal colony never shows a totem out on the water;
 *  - the village search takes the continent of the COLONY tile, not of the
 *    cell being tested (`uVar2 = FUN_137f_02a0(colony.x, colony.y)` hoisted
 *    above the 5x5 loop), so a village on a neighbouring island can never
 *    claim a cell of this colony's ring.
 * `origin_x`/`origin_y` are that colony tile.
 */
int colonies_indian_claim_tribe_from(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* pool,
  int viewer_nation,
  int origin_x,
  int origin_y,
  int x,
  int y
) {
  if (!col1 || viewer_nation < 0 || viewer_nation > 3) {
    return -1;
  }
  /* FUN_13e4_0074: Ocean / Sea Lane are never claimed. */
  if (map && map_tile_is_water(map, x, y)) {
    return -1;
  }
  const int continent = map ? map_continent_id_at(map, origin_x, origin_y) : -1;
  int ti = -1;
  if (!colonies_tile_indian_homeland_on(col1, map, x, y, continent, &ti, NULL) || ti < 0 ||
      !col1->tribe) {
    return -1;
  }
  const int tn = (int)col1->tribe[ti].nation_id;
  if (tn < 4 || tn > 11) {
    return -1;
  }
  if ((col1->indian[tn - 4].euro_diplo[viewer_nation] & COL1_INDIAN_MET_BIT) == 0) {
    return -1;
  }
  if (colonies_indian_land_purchase_gold_on(col1, map, x, y, continent, viewer_nation) <= 0) {
    return -1; /* bought / Minuit / outside radius */
  }
  if (pool && colonies_id_at(pool, x, y) >= 0) {
    return -1;
  }
  return ti;
}

int colonies_indian_claim_tribe(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* pool,
  int viewer_nation,
  int x,
  int y
) {
  return colonies_indian_claim_tribe_from(
    col1, map, pool, viewer_nation, x, y, x, y
  );
}

void colonies_indian_land_pay(
  ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  uint32_t* gold,
  int cost
) {
  if (!col1) {
    return;
  }
  (void)nation_id;
  if (gold && cost > 0) {
    *gold = (*gold >= (uint32_t)cost) ? (*gold - (uint32_t)cost) : 0u;
  }
  /* Mirror FUN_479b_00ca: INC indian[+5] lands-bought after spend. */
  int tribe_i = -1;
  if (colonies_tile_indian_homeland(col1, map, x, y, &tribe_i, NULL) && tribe_i >= 0) {
    const int indian_idx = (int)col1->tribe[tribe_i].nation_id - 4;
    if (indian_idx >= 0 && indian_idx < (int)COLONIZE_COL1_INDIAN_COUNT) {
      uint8_t* bought = &col1->indian[indian_idx].lands_bought;
      if (*bought < 0xffu) {
        (*bought)++;
      }
    }
  }
  /* FUN_281f_068c(..., 0x10, 1) — purchased tribal land on the tile. */
  if (col1->map.mask && col1->head.map_size_x > 0) {
    const size_t idx = (size_t)y * (size_t)col1->head.map_size_x + (size_t)x;
    if (idx < col1->map.tile_count) {
      col1->map.mask[idx] = (uint8_t)(col1->map.mask[idx] | 0x10u);
    }
  }
  if (map && map->layer2 && map_coords_inset(map, x, y)) {
    const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
    if (idx < map->tile_count) {
      ((ColonizeWorldMap*)map)->layer2[idx] = (uint8_t)(map->layer2[idx] | MAP_LAYER2_PURCHASED);
    }
  }
}

int colonies_found_with_indian_land(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  uint32_t* gold,
  int x,
  int y,
  int nation_id,
  int founder_type_index,
  int founder_profession,
  int tools,
  int muskets,
  int horses
) {
  if (col1 && gold) {
    const int cost = colonies_indian_land_purchase_gold(col1, map, x, y, nation_id);
    if (cost > 0) {
      if (*gold < (uint32_t)cost) {
        return -1;
      }
      colonies_indian_land_pay(col1, map, x, y, nation_id, gold, cost);
    }
  }
  return colonies_found(
    pool, map, x, y, nation_id, founder_type_index, founder_profession, tools, muskets, horses
  );
}

/*
 * Live occupancy map for the settlement bit. layer2's MAP_OCCUPANCY_HAS_CITY
 * is what the map renderer uses to decide a tile shows its settlement instead
 * of the stack standing on it (units_top_on_map_tile), and what the DOS road
 * art keys off — but it was only ever (re)built by the col1 bridge at load and
 * at the end-of-turn capture. So a colony founded mid-turn kept drawing its
 * garrison, and an abandoned one kept HIDING units that were still there:
 * bugs.md, "after abandoning a colony … units on its square vanish, they are
 * on the sidebar". Bound wherever units_set_occupancy_map is; NULL = the old
 * capture-time-only behaviour.
 */
static ColonizeWorldMap* g_colonies_occupancy_map = NULL;

void colonies_set_occupancy_map(ColonizeWorldMap* map) {
  g_colonies_occupancy_map = map;
}

static void colonies_mark_settlement_tile(int x, int y, bool on) {
  if (!g_colonies_occupancy_map) {
    return;
  }
  map_occupancy_set_layer2(g_colonies_occupancy_map, x, y, MAP_OCCUPANCY_HAS_CITY, on);
}

int colonies_found(
  ColonizeColonyPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int nation_id,
  int founder_type_index,
  int founder_profession,
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
  slot->nation_id = nation_id;
  slot->building_in_production = -1;
  slot->specialty_cargo = 0xff;
  slot->active = true;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
    slot->tiles[t] = -1;
  }
  memset(slot->col1_outer_tiles, 0xff, sizeof(slot->col1_outer_tiles));
  snprintf(slot->name, sizeof(slot->name), "%s", colonies_next_name(pool, nation_id));
  colonies_grant_starters(pool, slot);
  colonies_mark_settlement_tile(x, y, true);

  if (tools > 0) {
    slot->stock[COLONIZE_CARGO_TOOLS] += tools;
  }
  if (muskets > 0) {
    slot->stock[COLONIZE_CARGO_MUSKETS] += muskets;
  }
  if (horses > 0) {
    slot->stock[COLONIZE_CARGO_HORSES] += horses;
  }
  /* DOS FUN_364b_1ba8: cargo stock (+0x9a, 0x20 bytes) cleared — food starts 0. */

  if (founder_type_index >= 0 && slot->colonist_count < COLONIZE_COLONY_POP_MAX) {
    ColonizeColonist* c = &slot->colonists[slot->colonist_count++];
    c->active = true;
    c->col1_specialty = 0xff;
    c->unit_type_index = founder_type_index;
    c->profession =
      (founder_profession >= 0) ? founder_profession : UNITS_JOB_NONE;
    c->building_type = colonies_find_building(pool, "Town Hall");
    c->field_job = -1;
    slot->population = slot->colonist_count;
  } else {
    slot->population = 0;
  }

  /*
   * Default first project so carpenter hammers have a target (0 accumulated).
   * Only when the colony can actually build it: Stockade needs 3 colonists
   * (@BUILDING min_colony), and DOS never shows a size-1 town building one —
   * seed-100 TURN4–6 AI towns (New Amsterdam / Quebec / Isabella) all start
   * on Docks instead (bugs.md: a new colony's project should be Docks, not
   * "none"). Fallbacks: Stockade when the population already qualifies,
   * else Docks for a coastal site, else Warehouse for a landlocked one
   * (player-confirmed DOS behaviour for a landlocked 1-pop colony).
   */
  {
    const int stockade = colonies_find_building(pool, "Stockade");
    if (stockade >= 0 && !slot->has_building[stockade] &&
        (pool->building_types[stockade].min_population <= 0 ||
         slot->population >= pool->building_types[stockade].min_population)) {
      slot->building_in_production = stockade;
      slot->hammers = 0;
    } else if (map && map_tile_is_coastal((ColonizeWorldMap*)map, x, y)) {
      const int docks = colonies_find_building(pool, "Docks");
      if (docks >= 0 && !slot->has_building[docks]) {
        slot->building_in_production = docks;
        slot->hammers = 0;
        slot->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
      }
    } else {
      const int warehouse = colonies_find_building(pool, "Warehouse");
      if (warehouse >= 0 && !slot->has_building[warehouse]) {
        slot->building_in_production = warehouse;
        slot->hammers = 0;
      }
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

bool colonies_is_school_building(const ColonizeColonyPool* pool, int building_type) {
  return colonies_school_building_tier(pool, building_type) > 0;
}

/* NAMES.TXT @JOB school field (1..4), index-aligned. */
static const uint8_t k_job_school_tier[28] = {
  1, 2, 2, 2, 1, 1, 1, 1, 1, /* 0..8 field */
  2, 2, 2, 2, 1, 2, 2,       /* 9..15 craft */
  3, 3, 4, 4, 1, 2, 1, 2, 3, /* 16..24 civic/military */
  4, 4, 4                    /* 25..27 servant/criminal/convert */
};

int colonies_job_school_tier(int profession) {
  if (profession < 0 || profession >= (int)(sizeof(k_job_school_tier) / sizeof(k_job_school_tier[0]))) {
    return 0;
  }
  return (int)k_job_school_tier[profession];
}

int colonies_school_building_tier(
  const ColonizeColonyPool* pool,
  int building_type
) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return 0;
  }
  const char* bn = pool->building_types[building_type].name;
  if (!bn || !bn[0]) {
    return 0;
  }
  if (strstr(bn, "University") != NULL) {
    return 3;
  }
  if (strstr(bn, "College") != NULL) {
    return 2;
  }
  if (strstr(bn, "Schoolhouse") != NULL) {
    return 1;
  }
  return 0;
}

int colonies_school_tier_shortfall(int profession, int building_tier) {
  if (building_tier <= 0) {
    return 0;
  }
  const int need = colonies_job_school_tier(profession);
  if (need != 2 && need != 3) {
    return 0;
  }
  if (need > building_tier) {
    return need;
  }
  return 0;
}

bool colonies_profession_may_teach(int profession) {
  if (profession < 0) {
    return false;
  }
  if (profession == COLONIZE_PROF_FREE_COLONIST || profession == COLONIZE_PROF_INDENTURED ||
      profession == COLONIZE_PROF_CRIMINAL || profession == COLONIZE_PROF_CONVERT ||
      profession == UNITS_JOB_COLONIST /* @JOB 19 free-colonist alias */) {
    return false;
  }
  return true;
}

static const char* colonies_profession_label(int profession) {
  if (profession >= 0 && profession < COLONIZE_FIELD_JOB_COUNT) {
    const char* n = colony_yield_job_name(profession);
    if (n && n[0]) {
      return n;
    }
  }
  switch (profession) {
  case COLONIZE_PROF_DISTILLER:
    return "Distiller";
  case COLONIZE_PROF_TOBACCONIST:
    return "Tobacconist";
  case COLONIZE_PROF_WEAVER:
    return "Weaver";
  case COLONIZE_PROF_FUR_TRADER:
    return "Fur Trader";
  case COLONIZE_PROF_CARPENTER:
    return "Carpenter";
  case COLONIZE_PROF_BLACKSMITH:
    return "Blacksmith";
  case COLONIZE_PROF_GUNSMITH:
    return "Gunsmith";
  case COLONIZE_PROF_PREACHER:
    return "Preacher";
  case COLONIZE_PROF_STATESMAN:
    return "Statesman";
  case COLONIZE_PROF_TEACHER:
    return "Teacher";
  case 20:
    return "Pioneer";
  case 21:
    return "Soldier";
  case 22:
    return "Scout";
  case 23:
    return "Dragoon";
  case 24:
    return "Missionary";
  default:
    return "profession";
  }
}

void colonies_emit_noteacher_chrome(
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    messages,
    "NOTEACHER",
    NULL,
    "Only colonists who have mastered a profession may teach.",
    body,
    sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_need_school_chrome(
  int profession,
  int building_tier,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups) {
    return;
  }
  const int shortfall = colonies_school_tier_shortfall(profession, building_tier);
  if (shortfall != 2 && shortfall != 3) {
    return;
  }
  const char* section = (shortfall == 3) ? "NEEDUNIVERSITY" : "NEEDCOLLEGE";
  const char* pname = colonies_profession_label(profession);
  char body[AI_POPUP_BODY_LEN];
  char fallback[160];
  snprintf(
    fallback,
    sizeof(fallback),
    shortfall == 3 ? "Need a university to teach %s." : "Need a college to teach %s.",
    pname
  );
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = pname;
  popup_msg_fill(messages, section, &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

int colonies_building_worker_count(const ColonizeColony* colony, int building_type) {
  if (!colony) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < colony->colonist_count; ++i) {
    const ColonizeColonist* c = &colony->colonists[i];
    if (c->active && c->building_type == building_type) {
      n++;
    }
  }
  return n;
}

/*
 * bugs.md: only buildings with a real @JOB worker slot accept colonists.
 * Passive/structural buildings (fortifications, docks chain, Warehouse,
 * Stable, Custom House, Printing Press/Newspaper, Capitol) have no crew —
 * assigning there must be refused BEFORE any admit side effect.
 */
bool colonies_building_workable(const ColonizeColonyPool* pool, int building_type) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  const char* bn = pool->building_types[building_type].name;
  if (!bn || !bn[0]) {
    return false;
  }
  static const char* const k_no_slot[] = {
    "Stockade", "Fort", "Fortress", "Docks", "Drydock", "Shipyard",
    "Warehouse", "Stable", "Custom House", "Printing Press", "Newspaper",
    "Capitol"
  };
  for (size_t i = 0; i < sizeof(k_no_slot) / sizeof(k_no_slot[0]); ++i) {
    if (strstr(bn, k_no_slot[i]) != NULL) {
      return false;
    }
  }
  return true;
}

/* debug.logs: colonist "#2 Master Carpenter" style tag for assign lines. */
static void colony_log_colonist(
  const ColonizeColony* col,
  int colonist_index,
  char* out,
  size_t out_size
) {
  if (!col || colonist_index < 0 || colonist_index >= col->colonist_count) {
    snprintf(out, out_size, "#%d", colonist_index);
    return;
  }
  snprintf(out, out_size, "#%d job=%d", colonist_index, col->colonists[colonist_index].profession);
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
  if (!colonies_building_workable(pool, building_type)) {
    return false;
  }
  /* @MORETHANTHREE: at most 3 colonists per building (manual ch. 6 / schools
   * teacher+students). No-op reassignment (already working there) is fine. */
  if (c->building_type != building_type &&
      colonies_building_worker_count(col, building_type) >= COLONIZE_BUILDING_MAX_WORKERS) {
    return false;
  }
  const int school_tier = colonies_school_building_tier(pool, building_type);
  if (school_tier > 0) {
    if (!colonies_profession_may_teach(c->profession)) {
      return false;
    }
    if (colonies_school_tier_shortfall(c->profession, school_tier) != 0) {
      return false;
    }
  }
  colonies_clear_colonist_tile(col, colonist_index);
  c->field_job = -1;
  c->building_type = building_type;
  if (diag_info_enabled()) {
    char who[48];
    colony_log_colonist(col, colonist_index, who, sizeof(who));
    diag_info(
      "COLONY %s: colonist %s -> %s",
      col->name[0] ? col->name : "colony", who, pool->building_types[building_type].name
    );
  }
  return true;
}

bool colonies_toggle_custom_house_cargo(ColonizeColonyPool* pool, int colony_id, int cargo_type) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return false;
  }
  const int ch = colonies_find_building(pool, "Custom House");
  if (ch < 0 || !col->has_building[ch]) {
    return false;
  }
  col->custom_house_bits = (uint16_t)(col->custom_house_bits ^ (1u << cargo_type));
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
  if (diag_info_enabled()) {
    char who[48];
    colony_log_colonist(col, colonist_index, who, sizeof(who));
    diag_info(
      "COLONY %s: colonist %s -> field tile %d as %s",
      col->name[0] ? col->name : "colony", who, tile_index,
      colony_yield_job_name(field_job)
    );
  }
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
    diag_info(
      "COLONY %s: colonist #%d off field tile %d",
      col->name[0] ? col->name : "colony", who, tile_index
    );
  }
  return true;
}

int colonies_admit_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  const ColonizeCol1Save* col1
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  const ColonizeUnit* unit = units_get_const(units, unit_id);
  if (!col || !units || !unit || !unit->active) {
    return -1;
  }
  if (!units_is_on_map(unit) || unit->x != col->x || unit->y != col->y) {
    return -1;
  }
  if (unit->nation_id != col->nation_id) {
    return -1;
  }
  if (units_is_sea(units, unit_id) || units_is_transport(units, unit_id)) {
    return -1;
  }
  if (col->colonist_count >= COLONIZE_COLONY_POP_MAX) {
    return -1;
  }
  const int profession = unit->profession;
  int work_type = units_find_type(units, "Colonists");
  if (work_type < 0) {
    work_type = unit->type_index;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(units, unit_id, &tools, &muskets, &horses);
  if (!units_despawn(units, unit_id)) {
    return -1;
  }
  if (tools > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] += tools;
  }
  if (muskets > 0) {
    col->stock[COLONIZE_CARGO_MUSKETS] += muskets;
  }
  if (horses > 0) {
    col->stock[COLONIZE_CARGO_HORSES] += horses;
  }
  ColonizeColonist* c = &col->colonists[col->colonist_count];
  memset(c, 0, sizeof(*c));
  c->active = true;
  c->col1_specialty = 0xff;
  c->unit_type_index = work_type;
  c->profession = profession;
  c->building_type = -1;
  c->field_job = -1;
  const int idx = col->colonist_count++;
  col->population = col->colonist_count;
  /* La Salle: this join may have just crossed pop 3 — grant the free
   * Stockade the same moment, not next turn (see founding_fathers.h). */
  (void)founding_fathers_la_salle_check(pool, col1, col->nation_id);
  /* Col1 +0x8e / +0x1e: LABOR join co-decrements demand counters (~87701). */
  if (col->labor_shortage > 0) {
    col->labor_shortage--;
  }
  if (col->garrison_quota > 0) {
    col->garrison_quota--;
  }
  /*
   * Early Isabella TURN4→5: beachhead soldier join cancels unused Stockade
   * auto-start (hammers still 0) → COL1 bip 0xFF. Cite: test-saves-ai/TURN5.
   */
  if (col->hammers == 0 && col->building_in_production >= 0) {
    const int stockade = colonies_find_building(pool, "Stockade");
    if (stockade >= 0 && col->building_in_production == stockade) {
      col->building_in_production = -1;
    }
  }
  /* bugs.md 262: every admit path (AI joins, capture, save import) puts the
   * newcomer to work immediately — DOS has no idle colonists, and an idle
   * one made the head count disagree with the visible workers. */
  colonies_auto_assign_idle(pool, colony_id);
  return idx;
}

void colonies_auto_assign_idle(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!pool || !col) {
    return;
  }
  const int town_hall = colonies_find_building(pool, "Town Hall");
  for (int i = 0; i < col->colonist_count; ++i) {
    ColonizeColonist* c = &col->colonists[i];
    if (!c->active || c->field_job >= 0 || c->building_type >= 0) {
      continue;
    }
    if (town_hall >= 0 && colonies_assign_workplace(pool, colony_id, i, town_hall)) {
      continue;
    }
    for (int bi = 0; bi < pool->building_type_count; ++bi) {
      if (bi == town_hall || !col->has_building[bi]) {
        continue;
      }
      if (colonies_assign_workplace(pool, colony_id, i, bi)) {
        break;
      }
    }
  }
}

const char* colonies_eject_role_name(int role) {
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    return "Pioneer";
  case COLONIZE_EJECT_SOLDIER:
    return "Soldier";
  case COLONIZE_EJECT_SCOUT:
    return "Scout";
  case COLONIZE_EJECT_DRAGOON:
    return "Dragoon";
  case COLONIZE_EJECT_MISSIONARY:
    return "Missionary";
  case COLONIZE_EJECT_COLONIST:
  default:
    return "Colonist";
  }
}

static int colonies_has_church_or_cathedral(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col
) {
  if (!pool || !col) {
    return 0;
  }
  const int church = colonies_find_building(pool, "Church");
  const int cath = colonies_find_building(pool, "Cathedral");
  if (church >= 0 && church < COLONIZE_BUILDING_TYPES_MAX && col->has_building[church]) {
    return 1;
  }
  if (cath >= 0 && cath < COLONIZE_BUILDING_TYPES_MAX && col->has_building[cath]) {
    return 1;
  }
  return 0;
}

int colonies_equip_tools_take(int available) {
  if (available < UNITS_EQUIP_TOOLS_STEP) {
    return 0;
  }
  int take = (available / UNITS_EQUIP_TOOLS_STEP) * UNITS_EQUIP_TOOLS_STEP;
  if (take > UNITS_EQUIP_TOOLS_MAX) {
    take = UNITS_EQUIP_TOOLS_MAX;
  }
  return take;
}

int colonies_list_eject_roles(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  int out_max
) {
  const ColonizeColony* col = colonies_get(pool, colony_id);
  if (!col || !out_roles || out_max <= 0) {
    return 0;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count ||
      !col->colonists[colonist_index].active) {
    return 0;
  }
  int n = 0;
  out_roles[n++] = COLONIZE_EJECT_COLONIST;
  if (n < out_max && col->stock[COLONIZE_CARGO_TOOLS] >= UNITS_EQUIP_TOOLS_STEP) {
    out_roles[n++] = COLONIZE_EJECT_PIONEER;
  }
  if (n < out_max && col->stock[COLONIZE_CARGO_MUSKETS] >= UNITS_EQUIP_MUSKETS) {
    out_roles[n++] = COLONIZE_EJECT_SOLDIER;
  }
  if (n < out_max && col->stock[COLONIZE_CARGO_HORSES] >= UNITS_EQUIP_HORSES) {
    out_roles[n++] = COLONIZE_EJECT_SCOUT;
  }
  if (n < out_max && col->stock[COLONIZE_CARGO_MUSKETS] >= UNITS_EQUIP_MUSKETS &&
      col->stock[COLONIZE_CARGO_HORSES] >= UNITS_EQUIP_HORSES) {
    out_roles[n++] = COLONIZE_EJECT_DRAGOON;
  }
  /* Church bless: leave as Missionary (no cargo cost). Cite: Colonization.pdf
   * Establishing a Mission / Church; building_production Missionary; fandom bless. */
  if (n < out_max && colonies_has_church_or_cathedral(pool, col)) {
    out_roles[n++] = COLONIZE_EJECT_MISSIONARY;
  }
  return n;
}

int colonies_eject_colonist(
  ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  ColonizeUnitPool* units,
  int role
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units) {
    return -1;
  }
  if (colonist_index < 0 || colonist_index >= col->colonist_count) {
    return -1;
  }
  ColonizeColonist* c = &col->colonists[colonist_index];
  if (!c->active) {
    return -1;
  }

  int tools_take = 0;
  int muskets_take = 0;
  int horses_take = 0;
  const char* type_name = "Colonists";
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    tools_take = colonies_equip_tools_take(col->stock[COLONIZE_CARGO_TOOLS]);
    if (tools_take <= 0) {
      return -1;
    }
    type_name = "Pioneers";
    break;
  case COLONIZE_EJECT_SOLDIER:
    if (col->stock[COLONIZE_CARGO_MUSKETS] < UNITS_EQUIP_MUSKETS) {
      return -1;
    }
    muskets_take = UNITS_EQUIP_MUSKETS;
    type_name = "Soldiers";
    break;
  case COLONIZE_EJECT_SCOUT:
    if (col->stock[COLONIZE_CARGO_HORSES] < UNITS_EQUIP_HORSES) {
      return -1;
    }
    horses_take = UNITS_EQUIP_HORSES;
    type_name = "Scouts";
    break;
  case COLONIZE_EJECT_DRAGOON:
    if (col->stock[COLONIZE_CARGO_MUSKETS] < UNITS_EQUIP_MUSKETS ||
        col->stock[COLONIZE_CARGO_HORSES] < UNITS_EQUIP_HORSES) {
      return -1;
    }
    muskets_take = UNITS_EQUIP_MUSKETS;
    horses_take = UNITS_EQUIP_HORSES;
    type_name = "Dragoons";
    break;
  case COLONIZE_EJECT_MISSIONARY:
    if (!colonies_has_church_or_cathedral(pool, col)) {
      return -1;
    }
    type_name = "Missionaries";
    break;
  case COLONIZE_EJECT_COLONIST:
  default:
    type_name = "Colonists";
    break;
  }

  int type_index = units_find_type(units, type_name);
  if (type_index < 0) {
    type_index = c->unit_type_index;
  }
  int profession = c->profession;
  if (role == COLONIZE_EJECT_MISSIONARY) {
    /* Church bless → ordinary Missionary; keep Jesuit (job 24) if already skilled. */
    if (profession != UNITS_JOB_MISSIONARY) {
      profession = UNITS_JOB_NONE;
    }
  }

  colonies_clear_colonist_tile(col, colonist_index);
  for (int i = colonist_index; i < col->colonist_count - 1; ++i) {
    col->colonists[i] = col->colonists[i + 1];
  }
  col->colonist_count--;
  col->population = col->colonist_count;
  if (col->colonist_count >= 0 && col->colonist_count < COLONIZE_COLONY_POP_MAX) {
    memset(&col->colonists[col->colonist_count], 0, sizeof(col->colonists[0]));
  }
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES; ++t) {
    const int who = (int)col->tiles[t];
    if (who == colonist_index) {
      col->tiles[t] = -1;
    } else if (who > colonist_index) {
      col->tiles[t] = (int8_t)(who - 1);
    }
  }

  col->stock[COLONIZE_CARGO_TOOLS] -= tools_take;
  col->stock[COLONIZE_CARGO_MUSKETS] -= muskets_take;
  col->stock[COLONIZE_CARGO_HORSES] -= horses_take;

  const int uid = units_spawn_allow_stack(units, type_index, col->x, col->y);
  if (uid < 0) {
    /* Refund gear if spawn fails (colonist already removed — best-effort). */
    col->stock[COLONIZE_CARGO_TOOLS] += tools_take;
    col->stock[COLONIZE_CARGO_MUSKETS] += muskets_take;
    col->stock[COLONIZE_CARGO_HORSES] += horses_take;
    return -1;
  }
  ColonizeUnit* u = units_get(units, uid);
  if (u) {
    units_set_nation(u, col->nation_id);
    u->profession = profession;
    u->tools = tools_take;
    u->muskets = muskets_take;
    u->horses = horses_take;
    /* bugs.md: a freshly ejected/armed/horsed unit starts with no moves —
     * it acts from next turn's refresh. */
    u->moves_left = 0;
  }
  return uid;
}

bool colonies_has_fortification(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  if (!pool || !colony) {
    return false;
  }
  static const char* k_forts[] = {"Stockade", "Fort", "Fortress"};
  for (size_t i = 0; i < sizeof(k_forts) / sizeof(k_forts[0]); ++i) {
    const int idx = colonies_find_building(pool, k_forts[i]);
    if (idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[idx]) {
      return true;
    }
  }
  return false;
}

int colonies_fortification_defense_bonus_percent(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
) {
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  /* Highest tier wins (Fortress upgrades Fort upgrades Stockade). */
  const int fortress = colonies_find_building(pool, "Fortress");
  if (fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fortress]) {
    return 200;
  }
  const int fort = colonies_find_building(pool, "Fort");
  if (fort >= 0 && fort < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fort]) {
    return 150;
  }
  const int stockade = colonies_find_building(pool, "Stockade");
  if (stockade >= 0 && stockade < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[stockade]) {
    return 100;
  }
  return 0;
}

bool colonies_abandon(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !col->active) {
    return false;
  }
  colonies_mark_settlement_tile(col->x, col->y, false);
  memset(col, 0, sizeof(*col));
  int active = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active) {
      active++;
    }
  }
  pool->colony_count = active;
  return true;
}

static ColonizeCol1Save* g_colonies_col1 = NULL;

void colonies_set_col1_context(ColonizeCol1Save* col1) {
  g_colonies_col1 = col1;
}

/*
 * FUN_5fef_1b0e colony-capture tail (viceroy_unpacked.c ~100905-101030),
 * Euro→Euro only. In DOS order: crown capture during WoI sets 0x5382|0x40
 * (REF unit threshold); colony_counts/colony_pop_totals move with the
 * colony; colony nation byte swaps; rebel dividend (+0xc2) = old*2/3;
 * peacetime: loser's treasury share gold*pop/(pop + Σ pop of the loser's
 * remaining colonies) moves to the captor (the @CAPTURED %NUMBER0);
 * nation_relation words of both zeroed; WAR bit set between them when not
 * already at war (DOS toggles the single byte; mirrored both ways here).
 * @HOWTOWIN once-latch (0x5386 bit0) is left to ai_king's declare path.
 */
static int colonies_capture_col1_effects(
  ColonizeCol1Save* col1, const ColonizeColony* col, int old_nation, int new_nation
) {
  if (!col1 || !col1->colony || old_nation < 0 || old_nation > 3 || new_nation < 0 ||
      new_nation > 3) {
    return 0;
  }
  const int pop = col->colonist_count > 0 ? col->colonist_count : 0;
  const bool woi = col1->head.game_options.woi != 0;
  if (woi && new_nation == (int)col1->head.crown_nation_id) {
    col1->head.game_options.ref_unit_threshold = 1;
  }
  if (col1->stuff.colony_counts[old_nation] > 0) {
    col1->stuff.colony_counts[old_nation]--;
  }
  col1->stuff.colony_counts[new_nation]++;
  col1->stuff.colony_pop_totals[old_nation] =
    (uint8_t)(col1->stuff.colony_pop_totals[old_nation] > pop
                ? col1->stuff.colony_pop_totals[old_nation] - pop
                : 0);
  col1->stuff.colony_pop_totals[new_nation] =
    (uint8_t)(col1->stuff.colony_pop_totals[new_nation] + pop > 255
                ? 255
                : col1->stuff.colony_pop_totals[new_nation] + pop);
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    ColonizeCol1Colony* c = &col1->colony[i];
    if ((int)c->x == col->x && (int)c->y == col->y) {
      c->nation_id = (uint8_t)new_nation;
      c->rebel_dividend = (uint32_t)(((uint64_t)c->rebel_dividend * 2u) / 3u);
      break;
    }
  }
  int plunder = 0;
  if (!woi) {
    int total = pop;
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if ((int)c->nation_id == old_nation) {
        total += c->population;
      }
    }
    if (total < 1) {
      total = 1;
    }
    const uint32_t gold = col1->nation[old_nation].gold;
    plunder = (int)(((uint64_t)gold * (uint64_t)pop) / (uint64_t)total);
    col1->nation[old_nation].gold -= (uint32_t)plunder;
    col1->nation[new_nation].gold += (uint32_t)plunder;
  }
  col1->head.nation_relation[old_nation] = 0;
  col1->head.nation_relation[new_nation] = 0;
  if ((ai_diplo_read(col1, old_nation, new_nation) & AI_DIPLO_WAR) == 0) {
    ai_diplo_or_both(col1, old_nation, new_nation, AI_DIPLO_WAR);
  }
  return plunder;
}

bool colonies_capture_ex(
  ColonizeColonyPool* pool, int colony_id, int new_nation_id, int* plunder_gold
) {
  if (plunder_gold) {
    *plunder_gold = 0;
  }
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !col->active) {
    return false;
  }
  if (new_nation_id < 0 || new_nation_id > 11) {
    return false;
  }
  /* Indian capture: abandon (natives don't run Euro colonies T0). */
  if (new_nation_id >= 4) {
    return colonies_abandon(pool, colony_id);
  }
  const int old_nation = col->nation_id;
  col->nation_id = new_nation_id;
  if (old_nation != new_nation_id) {
    const int plunder = colonies_capture_col1_effects(g_colonies_col1, col, old_nation, new_nation_id);
    if (plunder_gold) {
      *plunder_gold = plunder;
    }
  }
  return true;
}

bool colonies_capture(ColonizeColonyPool* pool, int colony_id, int new_nation_id) {
  return colonies_capture_ex(pool, colony_id, new_nation_id, NULL);
}

static bool colonies_has_building_named(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* name
);

bool colonies_unit_build_info(int raw_code, const char** name, int* hammers, int* tools_cost) {
  if (raw_code == COLONIZE_UNIT_BUILD_ARTILLERY) {
    /* Golden-confirmed (New Amsterdam, dutch-reports.SAV): hammers=192 the
     * one time this port shows an Artillery project — @UNIT's own row
     * (NAMES.TXT) doesn't carry a hammers/tools construction cost field at
     * all (that's a purchase-price row, Europe money not colony hammers),
     * so this pair isn't independently cross-checked against NAMES.TXT. */
    if (name) {
      *name = "Artillery";
    }
    if (hammers) {
      *hammers = 192;
    }
    if (tools_cost) {
      *tools_cost = 40;
    }
    return true;
  }
  if (raw_code == COLONIZE_UNIT_BUILD_WAGON_TRAIN) {
    if (name) {
      *name = "Wagon Train";
    }
    if (hammers) {
      *hammers = 40;
    }
    if (tools_cost) {
      *tools_cost = 0;
    }
    return true;
  }
  return false;
}

/* Armory or an upgrade (Magazine/Arsenal) — player-requested Artillery
 * construction gate. */
static bool colonies_has_armory_chain(const ColonizeColonyPool* pool, const ColonizeColony* col) {
  return colonies_has_building_named(pool, col, "Armory") ||
         colonies_has_building_named(pool, col, "Magazine") ||
         colonies_has_building_named(pool, col, "Arsenal");
}

bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (building_type == COLONIZE_UNIT_BUILD_ARTILLERY) {
    if (!col || !colonies_has_armory_chain(pool, col)) {
      return false;
    }
    col->building_in_production = building_type;
    diag_info("COLONY %s: building Artillery", col->name[0] ? col->name : "colony");
    return true;
  }
  if (building_type == COLONIZE_UNIT_BUILD_WAGON_TRAIN) {
    /* Buildable anywhere, no building gate. */
    if (!col) {
      return false;
    }
    col->building_in_production = building_type;
    diag_info("COLONY %s: building Wagon Train", col->name[0] ? col->name : "colony");
    return true;
  }
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
  diag_info(
    "COLONY %s: building %s (hammers %d, pop %d)",
    col->name[0] ? col->name : "colony", bt->name, col->hammers, col->population
  );
  return true;
}

bool colonies_clear_construction(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col) {
    return false;
  }
  col->building_in_production = -1;
  /* FUN_5952 ~95710: drop wants_construction with queue clear. */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags & (uint8_t)~COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
  return true;
}

bool colonies_destroy_building(ColonizeColonyPool* pool, int colony_id, int building_type) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool) {
    return false;
  }
  if (building_type < 0 || building_type >= pool->building_type_count) {
    return false;
  }
  if (!col->has_building[building_type]) {
    return false;
  }
  const ColonizeBuildingType* bt = &pool->building_types[building_type];
  /* Town Hall is the colony core — never burn/remove via raid destroy. */
  if (bt && strcmp(bt->name, "Town Hall") == 0) {
    return false;
  }
  col->has_building[building_type] = false;
  if (col->building_in_production == building_type) {
    col->building_in_production = -1;
  }
  for (int i = 0; i < col->colonist_count; ++i) {
    ColonizeColonist* c = &col->colonists[i];
    if (!c->active) {
      continue;
    }
    if (c->building_type == building_type) {
      c->building_type = -1;
    }
  }
  return true;
}

/* Shared by colonies_construction_gold_cost/_tools_needed/_buy_construction:
 * the current project's hammers/tools_cost, whether it's a real building or
 * a unit-type project (colonies_unit_build_info). False if there's no
 * project or its cost can't be resolved either way. */
static bool colonies_construction_cost(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int* hammers,
  int* tools_cost
) {
  if (!pool || !colony || colony->building_in_production < 0) {
    return false;
  }
  const char* uname = NULL;
  if (colonies_unit_build_info(colony->building_in_production, &uname, hammers, tools_cost)) {
    return true;
  }
  const ColonizeBuildingType* bt = colonies_building_type(pool, colony->building_in_production);
  if (!bt || bt->hammers <= 0) {
    return false;
  }
  if (hammers) {
    *hammers = bt->hammers;
  }
  if (tools_cost) {
    *tools_cost = bt->tools_cost;
  }
  return true;
}

int colonies_construction_gold_cost(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int difficulty
) {
  int hammers_need = 0;
  int tools_cost = 0;
  if (!colonies_construction_cost(pool, colony, &hammers_need, &tools_cost)) {
    return 0;
  }
  int hammers_deficit = hammers_need - colony->hammers;
  if (hammers_deficit < 0) {
    hammers_deficit = 0;
  }
  int tools_deficit = tools_cost - colony->stock[COLONIZE_CARGO_TOOLS];
  if (tools_deficit < 0) {
    tools_deficit = 0;
  }
  /* See the doc comment in colony.h for the FUN_2f2b_5e44 citation and what
   * is/isn't independently verified here. */
  int cost = hammers_deficit * 13;
  if (tools_deficit > 0) {
    cost += tools_deficit * (difficulty + 4);
  }
  if (colony->hammers == 0) {
    cost *= 2;
  }
  return cost;
}

int colonies_construction_tools_needed(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony
) {
  int tools_cost = 0;
  if (!colonies_construction_cost(pool, colony, NULL, &tools_cost) || tools_cost <= 0) {
    return 0;
  }
  const int have = colony->stock[COLONIZE_CARGO_TOOLS];
  const int need = tools_cost - have;
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
  /* Already own the selected project (a later call the same turn it
   * completed would otherwise re-fire; the selection itself is never
   * cleared below, matching DOS — see that comment). Treat as nothing to
   * complete, same as the player revisiting Construction on an
   * already-owned item (@ALREADYHAVE). */
  if (bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX && col->has_building[bid]) {
    return false;
  }
  if (bt->tools_cost > 0 && col->stock[COLONIZE_CARGO_TOOLS] < bt->tools_cost) {
    return false;
  }
  col->pending_build_reveal = bid + 1; /* DS:0x34a — colony-screen reveal + 0x54 */
  if (bt->tools_cost > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] -= bt->tools_cost;
  }
  if (bid >= 0 && bid < COLONIZE_BUILDING_TYPES_MAX) {
    col->has_building[bid] = true;
  }
  /* Col1 +0x95/+0x96: INC warehouse / capitol levels on matching completes. */
  if (bt->name[0] != '\0') {
    if (strcmp(bt->name, "Warehouse") == 0 && col->warehouse_level < 1u) {
      col->warehouse_level = 1;
    } else if (strcmp(bt->name, "Warehouse Expansion") == 0) {
      col->warehouse_level = 2;
    } else if (strcmp(bt->name, "Capitol") == 0 && col->capitol_level < 1u) {
      col->capitol_level = 1;
    } else if (strcmp(bt->name, "Capitol Expansion") == 0) {
      col->capitol_level = 2;
    } else if (strcmp(bt->name, "Custom House") == 0 && col->custom_house_bits == 0) {
      col->custom_house_bits = COLONIZE_CUSTOM_HOUSE_DEFAULT_MASK;
    }
  }
  col->hammers = 0;
  /*
   * bugs.md: an upgrade takes its workers with it — DOS has no per-building
   * membership at all (occupation is the @JOB; the worker always works the
   * best tier owned), so completing e.g. a Lumber Mill must move the
   * Carpenter's Shop crew over. Same chain families the save bridge maps.
   */
  {
    static const char* const k_fam[][4] = {
      {"Lumber Mill", "Carpenter", NULL},
      {"Rum", NULL},
      {"Cigar", "Tobacconist", NULL},
      {"Textile", "Weaver", NULL},
      {"Fur", NULL},
      {"Iron", "Blacksmith", NULL},
      {"Arsenal", "Magazine", "Armory", NULL},
      {"Cathedral", "Church", NULL},
      {"University", "College", "School", NULL},
    };
    int fam = -1;
    for (int f = 0; fam < 0 && f < (int)(sizeof(k_fam) / sizeof(k_fam[0])); ++f) {
      for (int s = 0; k_fam[f][s]; ++s) {
        if (strstr(bt->name, k_fam[f][s]) != NULL) {
          fam = f;
          break;
        }
      }
    }
    if (fam >= 0) {
      for (int p = 0; p < col->colonist_count; ++p) {
        ColonizeColonist* c = &col->colonists[p];
        if (!c->active || c->building_type < 0 || c->building_type == bid ||
            c->building_type >= pool->building_type_count) {
          continue;
        }
        const char* on = pool->building_types[c->building_type].name;
        for (int s = 0; k_fam[fam][s]; ++s) {
          if (on && strstr(on, k_fam[fam][s]) != NULL) {
            c->building_type = bid;
            break;
          }
        }
      }
    }
  }
  /* Player-confirmed 2026-08-17 (colony_prod02 golden, a real single DOS
   * turn): building_in_production stays pointed at the just-completed
   * project — DOS never clears it on completion, only has_building[] and
   * hammers change. The guard above stops this function from re-firing on
   * a later call against the same still-selected, now-owned project. */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags & (uint8_t)~COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
  return true;
}

int colonies_try_complete_unit_construction(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units || col->building_in_production < 0) {
    return -1;
  }
  const char* name = NULL;
  int hammers_need = 0;
  int tools_cost = 0;
  if (!colonies_unit_build_info(col->building_in_production, &name, &hammers_need, &tools_cost)) {
    return -1;
  }
  if (hammers_need <= 0 || col->hammers < hammers_need) {
    return -1;
  }
  if (tools_cost > 0 && col->stock[COLONIZE_CARGO_TOOLS] < tools_cost) {
    return -1;
  }
  const int type_index = units_find_type(units, name);
  if (type_index < 0) {
    return -1;
  }
  const int uid = units_spawn_allow_stack(units, type_index, col->x, col->y);
  if (uid < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(units, uid);
  if (u) {
    units_set_nation(u, col->nation_id);
  }
  if (tools_cost > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] -= tools_cost;
  }
  /* Unlike colonies_try_complete_building, no has_building[]/re-fire guard
   * needed: a unit is never "owned" by the colony, and resetting hammers to
   * 0 here is itself the guard (next call reads hammers_need > 0 again). */
  col->hammers = 0;
  return uid;
}

bool colonies_buy_construction(ColonizeColonyPool* pool, int colony_id, int difficulty, int* gold) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !pool || !gold || col->building_in_production < 0) {
    return false;
  }
  int hammers_need = 0;
  int tools_cost = 0;
  if (!colonies_construction_cost(pool, col, &hammers_need, &tools_cost)) {
    return false;
  }
  int hammers_deficit = hammers_need - col->hammers;
  if (hammers_deficit < 0) {
    hammers_deficit = 0;
  }
  int tools_deficit = tools_cost - col->stock[COLONIZE_CARGO_TOOLS];
  if (tools_deficit < 0) {
    tools_deficit = 0;
  }
  const int gold_cost = colonies_construction_gold_cost(pool, col, difficulty);
  if (*gold < gold_cost) {
    return false;
  }
  *gold -= gold_cost;
  /* FUN_2f2b_5e44: accumulate hammers *deficit* (not gold spent — the two
   * only used to be numerically equal back when gold_cost was 1:1 with
   * hammers; not any more) into +0x98. */
  if (hammers_deficit > 0) {
    const unsigned sum = (unsigned)col->hammers_purchased + (unsigned)hammers_deficit;
    col->hammers_purchased = sum > 0xffffu ? 0xffffu : (uint16_t)sum;
  }
  /* Tops hammers/tools only — does NOT complete the project (matches
   * FUN_2f2b_5e44, which never touches has_building[]/spawns a unit
   * itself). Completion happens next turn via turn_run_colony_building_
   * completion / turn_run_colony_unit_construction. */
  col->hammers = hammers_need;
  if (tools_deficit > 0) {
    col->stock[COLONIZE_CARGO_TOOLS] += tools_deficit;
  }
  diag_info(
    "COLONY %s: bought construction for %d$ (hammers +%d, tools +%d, gold=%d)",
    col->name[0] ? col->name : "colony", gold_cost, hammers_deficit, tools_deficit, *gold
  );
  return true;
}

static bool colonies_has_building_named(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* name
) {
  const int idx = colonies_find_building(pool, name);
  return idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && col->has_building[idx];
}

/* True if this type is currently a legal construction project for the colony. */
static bool colonies_building_is_buildable(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int type_index,
  const ColoniesBuildableOpts* opts
) {
  if (!pool || !col || type_index < 0 || type_index >= pool->building_type_count) {
    return false;
  }
  if (col->has_building[type_index]) {
    return false;
  }
  const ColonizeBuildingType* bt = &pool->building_types[type_index];
  if (bt->name[0] == '\0' || bt->hammers <= 0) {
    return false;
  }
  if (bt->min_population > 0 && col->population < bt->min_population) {
    return false;
  }

  const char* n = bt->name;
  const bool adam = opts && opts->has_adam_smith;
  const bool stuy = opts && opts->has_peter_stuyvesant;
  const bool coastal =
    opts && opts->map && map_tile_is_coastal(opts->map, col->x, col->y);

  /* Duplicate Town Hall rows in NAMES.TXT — never list once any Town Hall exists. */
  if (strcmp(n, "Town Hall") == 0) {
    return false;
  }

  /*
   * Cut construction rows. DOS's own "can this colony build it" gate
   * (FUN_15eb_3650) hard-zeroes three @BUILDING file indices no matter what
   * the colony has: 0x0a and 0x0b (the two unfinished Town Hall upgrades —
   * PEDIA.TXT calls 0x0b "COLONIAL ASSEMBLY") and 0x1e (Capitol). 0x1f
   * (Capitol Expansion) is not zeroed there but is unreachable anyway: its
   * prerequisite is the Capitol, which can never be owned. NAMES.TXT still
   * carries all four rows and PEDIA.TXT keeps title-only stubs for them, but
   * no DOS colony can ever start one — the port used to offer Capitol /
   * Capitol Expansion in the construction picker.
   */
  if (strcmp(n, "Capitol") == 0 || strcmp(n, "Capitol Expansion") == 0) {
    return false;
  }

  /* Fortification chain. */
  if (strcmp(n, "Stockade") == 0) {
    return !colonies_has_building_named(pool, col, "Stockade") &&
           !colonies_has_building_named(pool, col, "Fort") &&
           !colonies_has_building_named(pool, col, "Fortress");
  }
  if (strcmp(n, "Fort") == 0) {
    return colonies_has_building_named(pool, col, "Stockade") &&
           !colonies_has_building_named(pool, col, "Fort") &&
           !colonies_has_building_named(pool, col, "Fortress");
  }
  if (strcmp(n, "Fortress") == 0) {
    return colonies_has_building_named(pool, col, "Fort") &&
           !colonies_has_building_named(pool, col, "Fortress");
  }

  /* Military production chain. */
  if (strcmp(n, "Armory") == 0) {
    return !colonies_has_building_named(pool, col, "Armory") &&
           !colonies_has_building_named(pool, col, "Magazine") &&
           !colonies_has_building_named(pool, col, "Arsenal");
  }
  if (strcmp(n, "Magazine") == 0) {
    return colonies_has_building_named(pool, col, "Armory") &&
           !colonies_has_building_named(pool, col, "Magazine") &&
           !colonies_has_building_named(pool, col, "Arsenal");
  }
  if (strcmp(n, "Arsenal") == 0) {
    return adam && colonies_has_building_named(pool, col, "Magazine") &&
           !colonies_has_building_named(pool, col, "Arsenal");
  }

  /* Port chain (coastal only). */
  if (strcmp(n, "Docks") == 0) {
    return coastal && !colonies_has_building_named(pool, col, "Docks") &&
           !colonies_has_building_named(pool, col, "Drydock") &&
           !colonies_has_building_named(pool, col, "Shipyard");
  }
  if (strcmp(n, "Drydock") == 0) {
    return coastal && colonies_has_building_named(pool, col, "Docks") &&
           !colonies_has_building_named(pool, col, "Drydock") &&
           !colonies_has_building_named(pool, col, "Shipyard");
  }
  if (strcmp(n, "Shipyard") == 0) {
    return coastal && colonies_has_building_named(pool, col, "Drydock") &&
           !colonies_has_building_named(pool, col, "Shipyard");
  }

  /* Education chain. */
  if (strcmp(n, "Schoolhouse") == 0) {
    return !colonies_has_building_named(pool, col, "Schoolhouse") &&
           !colonies_has_building_named(pool, col, "College") &&
           !colonies_has_building_named(pool, col, "University");
  }
  if (strcmp(n, "College") == 0) {
    return colonies_has_building_named(pool, col, "Schoolhouse") &&
           !colonies_has_building_named(pool, col, "College") &&
           !colonies_has_building_named(pool, col, "University");
  }
  if (strcmp(n, "University") == 0) {
    return colonies_has_building_named(pool, col, "College") &&
           !colonies_has_building_named(pool, col, "University");
  }

  if (strcmp(n, "Warehouse") == 0) {
    return !colonies_has_building_named(pool, col, "Warehouse");
  }
  if (strcmp(n, "Warehouse Expansion") == 0) {
    return colonies_has_building_named(pool, col, "Warehouse") &&
           !colonies_has_building_named(pool, col, "Warehouse Expansion");
  }

  if (strcmp(n, "Custom House") == 0) {
    return stuy && !colonies_has_building_named(pool, col, "Custom House");
  }

  if (strcmp(n, "Printing Press") == 0) {
    return !colonies_has_building_named(pool, col, "Printing Press") &&
           !colonies_has_building_named(pool, col, "Newspaper");
  }
  if (strcmp(n, "Newspaper") == 0) {
    return colonies_has_building_named(pool, col, "Printing Press") &&
           !colonies_has_building_named(pool, col, "Newspaper");
  }

  if (strcmp(n, "Weaver's Shop") == 0) {
    return colonies_has_building_named(pool, col, "Weaver's House") &&
           !colonies_has_building_named(pool, col, "Weaver's Shop") &&
           !colonies_has_building_named(pool, col, "Textile Mill");
  }
  if (strcmp(n, "Textile Mill") == 0) {
    return adam && colonies_has_building_named(pool, col, "Weaver's Shop") &&
           !colonies_has_building_named(pool, col, "Textile Mill");
  }

  if (strcmp(n, "Tobacconist's Shop") == 0) {
    return colonies_has_building_named(pool, col, "Tobacconist's House") &&
           !colonies_has_building_named(pool, col, "Tobacconist's Shop") &&
           !colonies_has_building_named(pool, col, "Cigar Factory");
  }
  if (strcmp(n, "Cigar Factory") == 0) {
    return adam && colonies_has_building_named(pool, col, "Tobacconist's Shop") &&
           !colonies_has_building_named(pool, col, "Cigar Factory");
  }

  if (strcmp(n, "Rum Distillery") == 0) {
    return colonies_has_building_named(pool, col, "Rum Distiller's House") &&
           !colonies_has_building_named(pool, col, "Rum Distillery") &&
           !colonies_has_building_named(pool, col, "Rum Factory");
  }
  if (strcmp(n, "Rum Factory") == 0) {
    return adam && colonies_has_building_named(pool, col, "Rum Distillery") &&
           !colonies_has_building_named(pool, col, "Rum Factory");
  }

  if (strcmp(n, "Fur Trading Post") == 0) {
    return colonies_has_building_named(pool, col, "Fur Trader's House") &&
           !colonies_has_building_named(pool, col, "Fur Trading Post") &&
           !colonies_has_building_named(pool, col, "Fur Factory");
  }
  if (strcmp(n, "Fur Factory") == 0) {
    return adam && colonies_has_building_named(pool, col, "Fur Trading Post") &&
           !colonies_has_building_named(pool, col, "Fur Factory");
  }

  if (strcmp(n, "Carpenter's Shop") == 0) {
    return !colonies_has_building_named(pool, col, "Carpenter's Shop") &&
           !colonies_has_building_named(pool, col, "Lumber Mill");
  }
  if (strcmp(n, "Lumber Mill") == 0) {
    return colonies_has_building_named(pool, col, "Carpenter's Shop") &&
           !colonies_has_building_named(pool, col, "Lumber Mill");
  }

  if (strcmp(n, "Church") == 0) {
    return !colonies_has_building_named(pool, col, "Church") &&
           !colonies_has_building_named(pool, col, "Cathedral");
  }
  if (strcmp(n, "Cathedral") == 0) {
    return colonies_has_building_named(pool, col, "Church") &&
           !colonies_has_building_named(pool, col, "Cathedral");
  }

  if (strcmp(n, "Blacksmith's Shop") == 0) {
    return colonies_has_building_named(pool, col, "Blacksmith's House") &&
           !colonies_has_building_named(pool, col, "Blacksmith's Shop") &&
           !colonies_has_building_named(pool, col, "Iron Works");
  }
  if (strcmp(n, "Iron Works") == 0) {
    return adam && colonies_has_building_named(pool, col, "Blacksmith's Shop") &&
           !colonies_has_building_named(pool, col, "Iron Works");
  }

  /* Starter houses and other leaf buildings: available if not already owned. */
  if (strcmp(n, "Weaver's House") == 0 || strcmp(n, "Tobacconist's House") == 0 ||
      strcmp(n, "Rum Distiller's House") == 0 || strcmp(n, "Fur Trader's House") == 0 ||
      strcmp(n, "Blacksmith's House") == 0 || strcmp(n, "Stable") == 0) {
    return !colonies_has_building_named(pool, col, n);
  }

  /* Unknown name: allow if unmet and not owned (forward-compatible). */
  return true;
}

int colonies_list_buildable(
  const ColonizeColonyPool* pool,
  int colony_id,
  int* out_ids,
  int out_max,
  const ColoniesBuildableOpts* opts
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
    if (colonies_building_is_buildable(pool, col, i, opts)) {
      out_ids[n++] = i;
    }
  }
  /* Artillery (colonies_unit_build_info) — player-requested: buildable with
   * an Armory or an upgrade. Not a real @BUILDING row so it's never "owned"
   * (no has_building[] dedup — the colony can queue another one right after
   * the last one spawns, same as any other repeatable unit purchase). */
  if (n < out_max && colonies_has_armory_chain(pool, col)) {
    out_ids[n++] = COLONIZE_UNIT_BUILD_ARTILLERY;
  }
  /* Wagon Train (colonies_unit_build_info) — player-requested: buildable in
   * any colony, no gate, same no-dedup reasoning as Artillery above. */
  if (n < out_max) {
    out_ids[n++] = COLONIZE_UNIT_BUILD_WAGON_TRAIN;
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
  /* FUN_15eb_0a50: 100*(1+warehouse_level); buildings raise the level. */
  int level = (int)colony->warehouse_level;
  if (pool) {
    int derived = 0;
    const int wh = colonies_find_building(pool, "Warehouse");
    const int whe = colonies_find_building(pool, "Warehouse Expansion");
    if (wh >= 0 && colony->has_building[wh]) {
      derived = 1;
    }
    if (whe >= 0 && colony->has_building[whe]) {
      derived = 2;
    }
    if (derived > level) {
      level = derived;
    }
  }
  if (level < 0) {
    level = 0;
  }
  if (level > 2) {
    level = 2;
  }
  return 100 * (1 + level);
}

void colonies_emit_warehouse_full_chrome(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type,
  const char* cargo_name,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  const int cap = colonies_warehouse_capacity(pool, colony, cargo_type);
  const int stock = colony->stock[cargo_type];
  const char* cname = colony->name[0] ? colony->name : "colony";
  const char* gname = (cargo_name && cargo_name[0]) ? cargo_name : "cargo";
  char body[AI_POPUP_BODY_LEN];
  char fallback[160];
  snprintf(
    fallback,
    sizeof(fallback),
    "Warehouse full at %s (%d/%d %s).",
    cname,
    stock,
    cap,
    gname
  );
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  tok.string1 = gname;
  tok.number0 = stock;
  tok.has_number0 = true;
  tok.number1 = cap;
  tok.has_number1 = true;
  popup_msg_fill(messages, "WAREHOUSEFULL", &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_full_chrome(
  const ColonizeColony* colony,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  const char* cname = colony->name[0] ? colony->name : "colony";
  char body[AI_POPUP_BODY_LEN];
  char fallback[160];
  snprintf(fallback, sizeof(fallback), "The colony of %s is far too crowded.", cname);
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  popup_msg_fill(messages, "FULL", &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_already_have_chrome(
  const ColonizeColony* colony,
  const char* building_name,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  const char* cname = colony->name[0] ? colony->name : "colony";
  const char* bname =
    (building_name && building_name[0]) ? building_name : "building";
  const bool warehouse_exp = (strcmp(bname, "Warehouse Expansion") == 0);
  const char* section = warehouse_exp ? "NOMOREWAREHOUSE" : "ALREADYHAVE";
  char body[AI_POPUP_BODY_LEN];
  char fallback[192];
  if (warehouse_exp) {
    snprintf(
      fallback,
      sizeof(fallback),
      "%s cannot build another Warehouse Expansion.",
      cname
    );
  } else {
    snprintf(
      fallback,
      sizeof(fallback),
      "%s already built a %s.",
      cname,
      bname
    );
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  tok.string1 = bname;
  popup_msg_fill(messages, section, &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_more_than_three_chrome(
  const ColonizeColony* colony,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    messages, "MORETHANTHREE", NULL,
    "We cannot put more than three colonists in any one building.", body, sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_specialty_cargo_update(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  int cargo_type,
  int want_set,
  int boycotted
) {
  if (!colony || !colony->active || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  const int cap = colonies_warehouse_capacity(pool, colony, cargo_type);
  /* FUN_5952_0306: stock >= warehouse cap → do not set / clear match. */
  if (cap > 0 && cap <= colony->stock[cargo_type]) {
    want_set = 0;
  }
  if (boycotted) {
    want_set = 0;
  }
  if (want_set) {
    colony->specialty_cargo = (uint8_t)cargo_type;
    return;
  }
  if (colony->specialty_cargo == (uint8_t)cargo_type) {
    colony->specialty_cargo = 0xff;
  }
}

int colonies_apply_warehouse_spoilage(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const int* stock_before,
  int* out_first_cargo,
  int* out_type_count
) {
  if (out_first_cargo) {
    *out_first_cargo = -1;
  }
  if (out_type_count) {
    *out_type_count = 0;
  }
  if (!colony || !colony->active) {
    return 0;
  }
  int spoiled = 0;
  int types = 0;
  /* c starts at 1: FUN_364b_0688's loop skips cargo 0 (Food) entirely — food
   * over capacity is the new-colonist rule, never warehouse spoilage. */
  for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
    const int cap = colonies_warehouse_capacity(pool, colony, c);
    if (cap <= 0) {
      continue;
    }
    if (colony->stock[c] <= cap) {
      continue;
    }
    /*
     * DOS reports a loss only for the part of the overflow that was already
     * there *before* this turn's production: `if (production < overflow)`.
     * Either way the stock ends clamped at capacity, so overflow caused purely
     * by production is a silent clamp with no @SPOIL message. A reported loss
     * below 2 tons is also dropped (`if (local_74 < 2) local_74 = 0`).
     */
    const int before = stock_before ? stock_before[c] : colony->stock[c];
    int lost = before - cap;
    if (lost < 2) {
      lost = 0;
    }
    if (lost > 0) {
      if (out_first_cargo && *out_first_cargo < 0) {
        *out_first_cargo = c;
      }
      types++;
      spoiled += lost;
    }
    colony->stock[c] = cap;
  }
  if (out_type_count) {
    *out_type_count = types;
  }
  return spoiled;
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
    diag_info(
      "CARGO %s -> unit %d: cargo %d x%d (colony stock %d)",
      col->name[0] ? col->name : "colony", unit_id, cargo_type, loaded,
      col->stock[cargo_type]
    );
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
  /*
   * bugs.md: unloading past warehouse capacity is ALLOWED — the whole hold
   * goes ashore, the excess spoils at next turn's warehouse pass, and the
   * over-capacity stock number is drawn in the alert colour. The flag only
   * informs the caller (status/popup), it no longer blocks or splits the
   * unload.
   */
  const int cap = colonies_warehouse_capacity(pool, col, ctype);
  const int move = amt;
  int got_type = 0;
  int got_amt = 0;
  if (units_unload_goods_hold(units, unit_id, hold_index, &got_type, &got_amt) <= 0) {
    return 0;
  }
  col->stock[ctype] += move;
  /* Col1 +0x8f: goods unload clears cargo_idle_turns (decomp ~90249). */
  if (move > 0) {
    col->cargo_idle_turns = 0;
    diag_info(
      "CARGO unit %d -> %s: cargo %d x%d (colony stock %d, cap %d)",
      unit_id, col->name[0] ? col->name : "colony", ctype, move, col->stock[ctype], cap
    );
  }
  if (out_warehouse_full && cap > 0 && col->stock[ctype] > cap) {
    *out_warehouse_full = true;
  }
  return move;
}

int colonies_transfer_from_unit_amount(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  int amount,
  bool* out_warehouse_full
) {
  if (out_warehouse_full) {
    *out_warehouse_full = false;
  }
  if (amount <= 0) {
    return 0;
  }
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u) {
    return 0;
  }
  const int n = units_goods_hold_count(units, unit_id);
  if (hold_index < 0 || hold_index >= n) {
    return 0;
  }
  const int held = u->hold_goods_amount[hold_index];
  const int ctype = u->hold_goods_type[hold_index];
  if (held <= 0 || held >= 255 || ctype < 0 || ctype >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  if (amount >= held) {
    /* Whole hold: reuse the full-unload path (hold clear + bookkeeping). */
    return colonies_transfer_from_unit(pool, colony_id, units, unit_id, hold_index, out_warehouse_full);
  }
  u->hold_goods_amount[hold_index] = (uint8_t)(held - amount);
  const int cap = colonies_warehouse_capacity(pool, col, ctype);
  col->stock[ctype] += amount;
  col->cargo_idle_turns = 0;
  if (out_warehouse_full && cap > 0 && col->stock[ctype] > cap) {
    *out_warehouse_full = true;
  }
  return amount;
}

static int colonies_de_witt_trade_ok(
  const ColonizeColonyPool* pool,
  int foreign_colony_id,
  const ColonizeUnitPool* units,
  int unit_id,
  const ColonizeCol1Save* col1
) {
  /*
   * Jan de Witt foreign-colony trade gate. Cite: fandom Jan de Witt;
   * founding_fathers_de_witt_allows_foreign_colony_trade.
   */
  if (!pool || !units || !col1) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(pool, foreign_colony_id);
  const ColonizeUnit* u = units_get_const(units, unit_id);
  if (!col || !col->active || !u || !u->active || !units_is_on_map(u)) {
    return 0;
  }
  if (u->nation_id < 0 || u->nation_id > 3 || col->nation_id < 0 || col->nation_id > 3) {
    return 0;
  }
  if (col->nation_id == u->nation_id) {
    return 0; /* own colony uses normal transfer APIs */
  }
  if (u->x != col->x || u->y != col->y) {
    return 0;
  }
  if (!founding_fathers_de_witt_allows_foreign_colony_trade(col1, u->nation_id)) {
    return 0;
  }
  if (ai_diplo_at_war(col1, u->nation_id, col->nation_id)) {
    return 0;
  }
  return 1;
}

int colonies_de_witt_transfer_from_colony(
  ColonizeColonyPool* pool,
  int foreign_colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int cargo_type,
  int amount,
  const ColonizeCol1Save* col1
) {
  if (!colonies_de_witt_trade_ok(pool, foreign_colony_id, units, unit_id, col1)) {
    return 0;
  }
  return colonies_transfer_to_unit(pool, foreign_colony_id, units, unit_id, cargo_type, amount);
}

int colonies_de_witt_transfer_to_colony(
  ColonizeColonyPool* pool,
  int foreign_colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  const ColonizeCol1Save* col1,
  bool* out_warehouse_full
) {
  if (!colonies_de_witt_trade_ok(pool, foreign_colony_id, units, unit_id, col1)) {
    if (out_warehouse_full) {
      *out_warehouse_full = false;
    }
    return 0;
  }
  return colonies_transfer_from_unit(
    pool, foreign_colony_id, units, unit_id, hold_index, out_warehouse_full
  );
}

static int colonies_trade_surplus_load_amount(const ColonizeColony* c, int ct) {
  int amt = 20;
  if (ct == COLONIZE_CARGO_MUSKETS || ct == COLONIZE_CARGO_HORSES) {
    amt = 10;
  }
  if (ct == COLONIZE_CARGO_FOOD) {
    amt = c->population > 0 ? c->population * 2 : 10;
  }
  return amt;
}

void colonies_trade_stop_set_cargos(
  ColonizeCol1TradeStop* stop,
  const int* unload_types,
  int unload_n,
  const int* load_types,
  int load_n
) {
  if (!stop) {
    return;
  }
  stop->unload_count = 0;
  stop->load_count = 0;
  memset(stop->unload_cargo_nibbles, 0, sizeof(stop->unload_cargo_nibbles));
  memset(stop->load_cargo_nibbles, 0, sizeof(stop->load_cargo_nibbles));
  int uc = 0;
  if (unload_types && unload_n > 0) {
    int seen[COLONIZE_CARGO_COUNT];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < unload_n && uc < 6; ++i) {
      const int ct = unload_types[i];
      if (ct < 0 || ct >= COLONIZE_CARGO_COUNT || seen[ct]) {
        continue;
      }
      seen[ct] = 1;
      col1_trade_nibble_set(stop->unload_cargo_nibbles, uc, ct);
      uc++;
    }
  }
  stop->unload_count = (uint8_t)uc;
  int lc = 0;
  if (load_types && load_n > 0) {
    int seen[COLONIZE_CARGO_COUNT];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < load_n && lc < 6; ++i) {
      const int ct = load_types[i];
      if (ct < 0 || ct >= COLONIZE_CARGO_COUNT || seen[ct]) {
        continue;
      }
      seen[ct] = 1;
      col1_trade_nibble_set(stop->load_cargo_nibbles, lc, ct);
      lc++;
    }
  }
  stop->load_count = (uint8_t)lc;
}

void colonies_trade_stop_autofill(
  ColonizeCol1TradeStop* stop,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int unit_id
) {
  if (!stop) {
    return;
  }
  stop->unload_count = 0;
  stop->load_count = 0;
  memset(stop->unload_cargo_nibbles, 0, sizeof(stop->unload_cargo_nibbles));
  memset(stop->load_cargo_nibbles, 0, sizeof(stop->load_cargo_nibbles));

  if (units && unit_id >= 0) {
    const ColonizeUnit* u = units_get_const(units, unit_id);
    if (u) {
      int seen[COLONIZE_CARGO_COUNT];
      memset(seen, 0, sizeof(seen));
      int uc = 0;
      const int n = units_goods_hold_count(units, unit_id);
      for (int h = 0; h < n && uc < 6; ++h) {
        const int ct = u->hold_goods_type[h];
        const int amt = u->hold_goods_amount[h];
        if (amt <= 0 || amt >= 255 || ct < 0 || ct >= COLONIZE_CARGO_COUNT || seen[ct]) {
          continue;
        }
        seen[ct] = 1;
        col1_trade_nibble_set(stop->unload_cargo_nibbles, uc, ct);
        uc++;
      }
      stop->unload_count = (uint8_t)uc;
    }
  }

  if (!colony) {
    return; /* Europe: sell path; no load list */
  }
  static const int k_load[] = {
    COLONIZE_CARGO_TOOLS,
    COLONIZE_CARGO_LUMBER,
    COLONIZE_CARGO_ORE,
    COLONIZE_CARGO_MUSKETS,
    COLONIZE_CARGO_HORSES,
    COLONIZE_CARGO_FOOD
  };
  int lc = 0;
  for (size_t i = 0; i < sizeof(k_load) / sizeof(k_load[0]) && lc < 6; ++i) {
    const int ct = k_load[i];
    const int amt = colonies_trade_surplus_load_amount(colony, ct);
    if (colony->stock[ct] < amt * 2) {
      continue;
    }
    col1_trade_nibble_set(stop->load_cargo_nibbles, lc, ct);
    lc++;
  }
  stop->load_count = (uint8_t)lc;
}

int colonies_trade_route_service_stop(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  const ColonizeCol1TradeStop* stop
) {
  if (!pool || !units || !stop) {
    return 0;
  }
  ColonizeColony* cmut = colonies_get_mut(pool, colony_id);
  ColonizeUnit* u = units_get(units, unit_id);
  if (!cmut || !u) {
    return 0;
  }

  int moved = 0;
  /*
   * Unload phase (DOS FUN_479b_0bd0): exactly the stop's unload-list cargos,
   * every matching hold, into the warehouse. No list → unload nothing.
   */
  const int unload_n = (int)stop->unload_count;
  for (int i = 0; i < unload_n && i < 6; ++i) {
    const int want = col1_trade_nibble_cargo(stop->unload_cargo_nibbles, i);
    if (want < 0 || want >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    /* Re-scan holds each pass — unload may compact. */
    for (int guard = 0; guard < COLONIZE_UNIT_CARGO_MAX; ++guard) {
      const int n = units_goods_hold_count(units, unit_id);
      int found = -1;
      for (int h = 0; h < n; ++h) {
        if (u->hold_goods_amount[h] > 0 && u->hold_goods_amount[h] < 255 &&
            u->hold_goods_type[h] == want) {
          found = h;
          break;
        }
      }
      if (found < 0) {
        break;
      }
      bool full = false;
      if (colonies_transfer_from_unit(pool, colony_id, units, unit_id, found, &full) > 0) {
        moved = 1;
      } else {
        break;
      }
    }
  }

  /*
   * Load phase (DOS: sort load-list cargos by weight×stock, take the best,
   * load one hold, repeat until the transport is full or nothing is left).
   * Port keeps the greedy shape with uniform weights: highest stock first.
   */
  const int load_n = (int)stop->load_count;
  for (int guard = 0; guard < COLONIZE_UNIT_CARGO_MAX + 2; ++guard) {
    int best = -1;
    int best_stock = 0;
    for (int i = 0; i < load_n && i < 6; ++i) {
      const int ct = col1_trade_nibble_cargo(stop->load_cargo_nibbles, i);
      if (ct < 0 || ct >= COLONIZE_CARGO_COUNT) {
        continue;
      }
      if (cmut->stock[ct] > best_stock) {
        best_stock = cmut->stock[ct];
        best = ct;
      }
    }
    if (best < 0) {
      break;
    }
    const int amt = best_stock > 100 ? 100 : best_stock;
    if (colonies_transfer_to_unit(pool, colony_id, units, unit_id, best, amt) <= 0) {
      break;
    }
    moved = 1;
  }
  return moved;
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
  const ColonizeFont* pop_font,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  const ColonizeWorldMap* fog_map,
  int fog_nation,
  const ColonizePalette* active_palette
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
    if (fog_map && !map_tile_seen_by(fog_map, c->x, c->y, fog_nation)) {
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
      colony_blit_map_icon(
        icons, colony_map_icon(pool, c), framebuffer, px, py, tile_w, tile_h, c->nation_id,
        active_palette
      );
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

    /*
     * Population badge + name label — DOS FUN_112b_0c64, and only in the
     * full-size (16px) tile set: the smaller zoom levels draw the icon
     * alone. Geometry read off CODE_5:112b:0dd b..0e79 (Ghidra drops the
     * register arguments of FUN_1c11_000c, so the coordinates come from
     * the instruction stream): digits at tile+(7,7), name at tile+(2,16).
     */
    if (font && tile_w >= 16 && tile_h >= 16) {
      /*
       * What is counted: your own colonies show their live population; a
       * foreign one shows what you last saw (colony +0xba per viewer,
       * floored at 1), so the badge never leaks a rival's growth. DOS
       * writes that floor back into the record; the reveal writer
       * (colonies_note_seen_by) owns it here, so the draw stays const.
       */
      int shown = c->population;
      if (fog_nation >= 0 && fog_nation < 4 && c->nation_id != fog_nation) {
        shown = c->pop_on_map[fog_nation];
        if (shown <= 0) {
          shown = 1;
        }
      }
      /* Colour by Sons of Liberty latch: white, bright green at ≥50%,
       * bright cyan once 100% is latched on top of it (+0x1c bits 4/2). */
      uint8_t ink = 15;
      if ((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0) {
        ink = 10;
        if ((c->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0) {
          ink = 11;
        }
      }
      char pop_text[8];
      snprintf(pop_text, sizeof(pop_text), "%d", shown);
      const uint8_t pop_shade[4] = {0, ink, ink, ink};
      /* FONTTINY (DS:0x89e), not the name label's FONTINTR (DS:0x268a). */
      font_draw_text_shaded(
        pop_font ? pop_font : font, framebuffer, px + 7, py + 7, pop_text, pop_shade
      );

      /* Colony name below the tile: white ink, black shadow. FONTINTR
       * already bakes a soft AA shadow into shade 2/3 of every glyph
       * (font_draw_text's color==15 path, FF_COLOR_MAP) — that shadow just
       * renders grey/brown, not black. Recolor it in place via
       * font_draw_text_shaded rather than layering a second, separate
       * manual shadow on top (player-caught: an earlier pass added one,
       * doubling up). */
      static const uint8_t kShade[4] = {0, 15, 0, 0};
      font_draw_text_shaded(font, framebuffer, px + 2, py + 16, c->name, kShade);
    }
  }
}
