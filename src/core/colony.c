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

int colonies_settlement_icon(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  return colony_map_icon(pool, colony);
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

/* FUN_4cc6_0356 stand-in: nearest tribe index; *out_dist = DOS distance. */
static int colonies_nearest_tribe(
  const ColonizeCol1Save* col1,
  int x,
  int y,
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
 * Manual Indian Land: camps/villages home radius 1; cities (capital) radius 2.
 * Tile needs purchase when within that radius of the nearest village.
 */
static int colonies_tile_indian_homeland(
  const ColonizeCol1Save* col1,
  int x,
  int y,
  int* out_tribe,
  int* out_dist
) {
  int dist = 9999;
  const int ti = colonies_nearest_tribe(col1, x, y, &dist);
  if (out_tribe) {
    *out_tribe = ti;
  }
  if (out_dist) {
    *out_dist = dist;
  }
  if (ti < 0 || !col1 || !col1->tribe) {
    return 0;
  }
  const int radius = col1->tribe[ti].state.capital ? 2 : 1;
  return dist <= radius ? 1 : 0;
}

int colonies_indian_land_purchase_gold(
  const ColonizeCol1Save* col1,
  const ColonizeWorldMap* map,
  int x,
  int y,
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
  if (!colonies_tile_indian_homeland(col1, x, y, &tribe_i, &dist)) {
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
      *gold -= (uint32_t)cost;
      /* Mirror FUN_479b_00ca: INC indian[+5] lands-bought after spend. */
      int tribe_i = -1;
      if (colonies_tile_indian_homeland(col1, x, y, &tribe_i, NULL) && tribe_i >= 0) {
        const int indian_idx = (int)col1->tribe[tribe_i].nation_id - 4;
        if (indian_idx >= 0 && indian_idx < (int)COLONIZE_COL1_INDIAN_COUNT) {
          uint8_t* bought = &col1->indian[indian_idx].lands_bought;
          if (*bought < 0xffu) {
            (*bought)++;
          }
        }
      }
      /* FUN_281f_068c(..., 0x10, 1) — purchased tribal land on founding tile. */
      if (col1->map.mask && col1->head.map_size_x > 0) {
        const size_t idx = (size_t)y * (size_t)col1->head.map_size_x + (size_t)x;
        if (idx < col1->map.tile_count) {
          col1->map.mask[idx] = (uint8_t)(col1->map.mask[idx] | 0x10u);
        }
      }
      if (map && map->layer2 && map_coords_inset(map, x, y)) {
        const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
        if (idx < map->tile_count) {
          ((ColonizeWorldMap*)map)->layer2[idx] =
            (uint8_t)(map->layer2[idx] | MAP_LAYER2_PURCHASED);
        }
      }
    }
  }
  return colonies_found(
    pool, map, x, y, nation_id, founder_type_index, founder_profession, tools, muskets, horses
  );
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
  snprintf(slot->name, sizeof(slot->name), "%s", colonies_next_name(pool, nation_id));
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
  /* DOS FUN_364b_1ba8: cargo stock (+0x9a, 0x20 bytes) cleared — food starts 0. */

  if (founder_type_index >= 0 && slot->colonist_count < COLONIZE_COLONY_POP_MAX) {
    ColonizeColonist* c = &slot->colonists[slot->colonist_count++];
    c->active = true;
    c->unit_type_index = founder_type_index;
    c->profession =
      (founder_profession >= 0) ? founder_profession : UNITS_JOB_NONE;
    c->building_type = colonies_find_building(pool, "Town Hall");
    c->field_job = -1;
    slot->population = slot->colonist_count;
  } else {
    slot->population = 0;
  }

  /* Default first project so carpenter hammers have a target (0 accumulated). */
  {
    const int stockade = colonies_find_building(pool, "Stockade");
    if (stockade >= 0 && !slot->has_building[stockade]) {
      slot->building_in_production = stockade;
      slot->hammers = 0;
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
      profession == COLONIZE_PROF_CRIMINAL || profession == COLONIZE_PROF_CONVERT) {
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

int colonies_admit_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id
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
  c->unit_type_index = work_type;
  c->profession = profession;
  c->building_type = -1;
  c->field_job = -1;
  const int idx = col->colonist_count++;
  col->population = col->colonist_count;
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
  return idx;
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
  case COLONIZE_EJECT_PIONEER: {
    int avail = col->stock[COLONIZE_CARGO_TOOLS];
    avail = (avail / UNITS_EQUIP_TOOLS_STEP) * UNITS_EQUIP_TOOLS_STEP;
    if (avail < UNITS_EQUIP_TOOLS_STEP) {
      return -1;
    }
    if (avail > UNITS_EQUIP_TOOLS_MAX) {
      avail = UNITS_EQUIP_TOOLS_MAX;
    }
    tools_take = avail;
    type_name = "Pioneers";
    break;
  }
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

bool colonies_capture(ColonizeColonyPool* pool, int colony_id, int new_nation_id) {
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
  col->nation_id = new_nation_id;
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
  /* Player-confirmed 2026-08-17 (colony_prod02 golden, a real single DOS
   * turn): building_in_production stays pointed at the just-completed
   * project — DOS never clears it on completion, only has_building[] and
   * hammers change. The guard above stops this function from re-firing on
   * a later call against the same still-selected, now-owned project. */
  col->build_ai_flags =
    (uint8_t)(col->build_ai_flags & (uint8_t)~COLONIZE_BUILD_AI_WANTS_CONSTRUCTION);
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
  /* FUN_2f2b_5e44: accumulate remainder hammers bought with gold (+0x98). */
  if (gold_cost > 0) {
    const unsigned sum = (unsigned)col->hammers_purchased + (unsigned)gold_cost;
    col->hammers_purchased = sum > 0xffffu ? 0xffffu : (uint16_t)sum;
  }
  col->hammers = bt->hammers;
  if (!colonies_try_complete_building(pool, colony_id)) {
    *gold += gold_cost;
    col->hammers = prev_hammers;
    if (gold_cost > 0) {
      col->hammers_purchased =
        (uint16_t)(col->hammers_purchased >= (uint16_t)gold_cost
                     ? col->hammers_purchased - (uint16_t)gold_cost
                     : 0);
    }
    return false;
  }
  col->colony_flags |= COLONIZE_COLONY_FLAG_BUILD_COMPLETE;
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

  if (strcmp(n, "Capitol Expansion") == 0) {
    return colonies_has_building_named(pool, col, "Capitol") &&
           !colonies_has_building_named(pool, col, "Capitol Expansion");
  }

  /* Starter houses and other leaf buildings: available if not already owned. */
  if (strcmp(n, "Weaver's House") == 0 || strcmp(n, "Tobacconist's House") == 0 ||
      strcmp(n, "Rum Distiller's House") == 0 || strcmp(n, "Fur Trader's House") == 0 ||
      strcmp(n, "Blacksmith's House") == 0 || strcmp(n, "Stable") == 0 ||
      strcmp(n, "Capitol") == 0) {
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
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    const int cap = colonies_warehouse_capacity(pool, colony, c);
    if (cap <= 0) {
      continue;
    }
    if (colony->stock[c] > cap) {
      if (out_first_cargo && *out_first_cargo < 0) {
        *out_first_cargo = c;
      }
      types++;
      spoiled += colony->stock[c] - cap;
      colony->stock[c] = cap;
    }
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
  /* Col1 +0x8f: goods unload clears cargo_idle_turns (decomp ~90249). */
  if (move > 0) {
    col->cargo_idle_turns = 0;
  }
  if (move < got_amt) {
    /* Put remainder back into the same hold. */
    units_load_goods(units, unit_id, ctype, got_amt - move);
    if (out_warehouse_full) {
      *out_warehouse_full = true;
    }
  }
  return move;
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
  const int unload_n = (int)stop->unload_count;
  if (unload_n > 0) {
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
  } else {
    int had_goods = 0;
    for (int guard = 0; guard < COLONIZE_UNIT_CARGO_MAX; ++guard) {
      const int n = units_goods_hold_count(units, unit_id);
      int found = -1;
      for (int h = 0; h < n; ++h) {
        if (u->hold_goods_amount[h] > 0 && u->hold_goods_amount[h] < 255) {
          found = h;
          break;
        }
      }
      if (found < 0) {
        break;
      }
      had_goods = 1;
      bool full = false;
      if (colonies_transfer_from_unit(pool, colony_id, units, unit_id, found, &full) > 0) {
        moved = 1;
      } else {
        break;
      }
    }
    /* Thin fallback: unload-all then surplus only when unit arrived empty. */
    if (had_goods) {
      return moved;
    }
  }

  const int load_n = (int)stop->load_count;
  if (load_n > 0) {
    for (int i = 0; i < load_n && i < 6; ++i) {
      const int ct = col1_trade_nibble_cargo(stop->load_cargo_nibbles, i);
      if (ct < 0 || ct >= COLONIZE_CARGO_COUNT) {
        continue;
      }
      const int amt = colonies_trade_surplus_load_amount(cmut, ct);
      if (cmut->stock[ct] < amt) {
        continue;
      }
      if (colonies_transfer_to_unit(pool, colony_id, units, unit_id, ct, amt) > 0) {
        moved = 1;
      }
    }
    return moved;
  }

  /* Surplus ladder when no Col1 load list (and empty-arrival for unload-all path). */
  {
    int has_goods = 0;
    const int n = units_goods_hold_count(units, unit_id);
    for (int h = 0; h < n; ++h) {
      if (u->hold_goods_amount[h] > 0 && u->hold_goods_amount[h] < 255) {
        has_goods = 1;
        break;
      }
    }
    if (has_goods) {
      return moved;
    }
  }
  static const int k_load[] = {
    COLONIZE_CARGO_TOOLS,
    COLONIZE_CARGO_LUMBER,
    COLONIZE_CARGO_ORE,
    COLONIZE_CARGO_MUSKETS,
    COLONIZE_CARGO_HORSES,
    COLONIZE_CARGO_FOOD
  };
  for (size_t i = 0; i < sizeof(k_load) / sizeof(k_load[0]); ++i) {
    const int ct = k_load[i];
    const int amt = colonies_trade_surplus_load_amount(cmut, ct);
    if (cmut->stock[ct] < amt * 2) {
      continue;
    }
    if (colonies_transfer_to_unit(pool, colony_id, units, unit_id, ct, amt) > 0) {
      moved = 1;
      break;
    }
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
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  const ColonizeWorldMap* fog_map,
  int fog_nation
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
