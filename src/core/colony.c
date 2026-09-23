#include "core/colony.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/ai_euro.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/units_cargo.h"
#include "platform/diagnostics.h"

/* ICONS.SS #0–3: European colonies by fortification (none / stockade / fort / fortress). */
#define COLONY_MAP_ICON_STOCKADE 0
#define COLONY_MAP_ICON_FORT 1
#define COLONY_MAP_ICON_FORTRESS 2
#define COLONY_MAP_ICON_NONE 3

int colonies_settlement_icon(const ColonizeColonyPool* pool, const ColonizeColony* c) {
  if (!pool || !c) {
    return COLONY_MAP_ICON_NONE;
  }
  const int fortress = colonies_building_row(pool, COLONY_BUILDING_FORTRESS);
  if (fortress >= 0 && c->has_building[fortress]) {
    return COLONY_MAP_ICON_FORTRESS;
  }
  const int fort = colonies_building_row(pool, COLONY_BUILDING_FORT);
  if (fort >= 0 && c->has_building[fort]) {
    return COLONY_MAP_ICON_FORT;
  }
  const int stockade = colonies_building_row(pool, COLONY_BUILDING_STOCKADE);
  if (stockade >= 0 && c->has_building[stockade]) {
    return COLONY_MAP_ICON_STOCKADE;
  }
  return COLONY_MAP_ICON_NONE;
}

/* File-local since the 2026-09-14 duplication pass (audit CO-29): the only
 * caller is colonies_fog_snapshot below. */
/* Names as loaded, by row — the only place a building name can be matched. */
static char g_building_row_names[COLONIZE_BUILDING_TYPES_MAX][40];
static int g_building_row_name_count = 0;

static int colonies_fortification_tier(const ColonizeColonyPool* pool, const ColonizeColony* c) {
  switch (colonies_settlement_icon(pool, c)) {
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

int colonies_count_for_nation(const ColonizeColonyPool* pool, int nation) {
  if (!pool) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (c->active && c->nation_id == nation) {
      n++;
    }
  }
  return n;
}

/*
 * Pool-bound walk, not `colony_count`-bound: colonies_abandon zeroes a slot
 * in place, so the array has holes and the live colonies are not a prefix.
 */
const ColonizeColony* colonies_find_at_xy(const ColonizeColonyPool* pool, int x, int y) {
  if (!pool) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (c->active && c->x == x && c->y == y) {
      return c;
    }
  }
  return NULL;
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

void colonies_reveal_all_for_nation(
  ColonizeWorldMap* map,
  ColonizeColonyPool* pool,
  int nation_id
) {
  if (!map || !pool || nation_id < 0 || nation_id > 3) {
    return;
  }
  /*
   * DOS FUN_4345_0342 (viceroy_unpacked.c 73155-73159), the FF-apply case
   * `param_2 == 6` (Coronado): `for i in 0..colony_count: FUN_281f_09e6(i);
   * FUN_281f_07aa(i, nation)` — the loop has NO owner test, so every colony
   * on the board (any nation's) gets the FUN_13f1_00a6 ±5 sweep.
   */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &pool->colonies[i];
    if (!c->active) {
      continue;
    }
    colonies_reveal_ring5(map, pool, c, nation_id);
  }
}

void colonies_reveal_founded_w(
  const ColonizeWorld* w,
  int colony_id
) {
  ColonizeWorldMap* map = w->map;
  ColonizeColonyPool* pool = w->colonies;
  const ColonizeCol1Save* col1 = w->col1;

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


void colonies_init(ColonizeColonyPool* pool) {
  if (!pool) {
    return;
  }
  memset(pool, 0, sizeof(*pool));
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
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
    str_trim(line);
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
    str_trim(line);
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
    const char* p = str_split_name_row(line);
    if (!p || line[0] == '\0') {
      continue;
    }
    int hammers = 0;
    int tools_cost = 0;
    int size = 0;
    int min_pop = 0;
    int upkeep = 0;
    /* NAMES.TXT: name, cost, tools(*10), size, min_colony, upkeep */
    sscanf(p, " %d , %d , %d , %d , %d", &hammers, &tools_cost, &size, &min_pop, &upkeep);
    (void)upkeep;

    ColonizeBuildingType* t = &pool->building_types[pool->building_type_count++];
    t->row_plus1 = pool->building_type_count; /* @BUILDING row = identity */
    str_copy_trunc(t->name, sizeof(t->name), line);
    /* Remember the catalog's own spelling for colonies_building_name_row. */
    str_copy_trunc(
      g_building_row_names[pool->building_type_count - 1],
      sizeof(g_building_row_names[0]),
      t->name
    );
    g_building_row_name_count = pool->building_type_count;
    t->hammers = hammers;
    /* NAMES.TXT tools(*10): file stores tens of tools (2 → 20 tools). */
    t->tools_cost = tools_cost * 10;
    t->min_population = min_pop;
    t->size_class = size;
  }

  diag_info("Loaded %d building types from NAMES.TXT @BUILDING", pool->building_type_count);
  return pool->building_type_count > 0;
}

static ColoniesBuildingNameRowResolver g_building_name_row_resolver = NULL;
static ColoniesBuildingRowNameResolver g_building_row_name_resolver = NULL;

void colonies_set_building_row_name_resolver(ColoniesBuildingRowNameResolver fn) {
  g_building_row_name_resolver = fn;
}

void colonies_set_building_name_row_resolver(ColoniesBuildingNameRowResolver fn) {
  g_building_name_row_resolver = fn;
}

int colonies_building_name_row(const char* name) {
  if (!name || !name[0]) {
    return -1;
  }
  for (int i = 0; i < g_building_row_name_count; ++i) {
    if (strcmp(g_building_row_names[i], name) == 0) {
      return i;
    }
  }
  return g_building_name_row_resolver ? g_building_name_row_resolver(name) : -1;
}

bool colonies_has_building_row(
  const ColonizeColonyPool* pool, const ColonizeColony* col, ColonizeBuildingRow row
) {
  if (!pool || !col) {
    return false;
  }
  const int idx = colonies_building_row(pool, row);
  return idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && col->has_building[idx];
}

int colonies_building_type_row(const ColonizeColonyPool* pool, int type_index) {
  if (!pool || type_index < 0 || type_index >= pool->building_type_count) {
    return -1;
  }
  const ColonizeBuildingType* t = &pool->building_types[type_index];
  if (t->row_plus1 > 0) {
    return t->row_plus1 - 1;
  }
  return g_building_name_row_resolver ? g_building_name_row_resolver(t->name) : -1;
}

int colonies_building_row(const ColonizeColonyPool* pool, ColonizeBuildingRow row) {
  if (!pool || (int)row < 0) {
    return -1;
  }
  /* A loaded pool has row == slot; scan anyway so a sparse pool resolves. */
  if ((int)row < pool->building_type_count &&
      colonies_building_type_row(pool, (int)row) == (int)row) {
    return (int)row;
  }
  for (int i = 0; i < pool->building_type_count; ++i) {
    if (colonies_building_type_row(pool, i) == (int)row) {
      return i;
    }
  }
  return -1;
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
   * DOS-LITERAL FUN_2b5a Build handler, asm 0x22620-0x22632 (bugs.md #685):
   * the found path has exactly ONE terrain-class test,
   * `FUN_281f_078c(x,y) == 0x1b` -> @TOOMOUNTAIN. Water is caught earlier by
   * FUN_281f_0768 -> @SEACOLONY. There is no Arctic gate anywhere on the
   * path, so the old pedia-24 rejection (cited only to Colonization.pdf) was
   * invented and is gone; Arctic is a legal, miserable colony site.
   */
  {
    const int pedia = map_pedia_terrain_index_at(map, x, y);
    if (pedia == 27) {
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
   * EMS-mapping garbage, same false-alarm class port_plan.md's Method
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
  /* Pool bound, not colony_count: colonies_abandon zeroes a slot in place and
   * recounts, so live colonies can sit past colony_count. */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
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

/*
 * DOS keeps a per-nation settlement count in the byte `nation + 0x9298`
 * (written raw 58030, decremented raw 58154) and the Build handler refuses
 * at 0x26 == 38 (asm 0x22595). The port has no such byte, so it recounts the
 * pool — the same quantity by construction. bugs.md #681.
 */
int colonies_nation_settlement_count(const ColonizeColonyPool* pool, int nation_id) {
  if (!pool) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    if (pool->colonies[i].active && pool->colonies[i].nation_id == nation_id) {
      ++n;
    }
  }
  return n;
}

/*
 * DOS thunk_FUN_2a1f_01f4 (raw 76995) builds the nation's next default colony
 * name into a local BEFORE the name prompt runs, without consuming it — the
 * prompt may still be cancelled (bugs.md #682). Non-advancing peek.
 */
const char* colonies_peek_next_name(const ColonizeColonyPool* pool, int nation_id) {
  if (!pool) {
    return "New Colony";
  }
  if (nation_id < 0 || nation_id > 3) {
    nation_id = 0;
  }
  if (pool->name_count[nation_id] == 0) {
    return "New Colony";
  }
  return pool->names[nation_id][pool->name_next[nation_id] % pool->name_count[nation_id]];
}

static const char* colonies_next_name(ColonizeColonyPool* pool, int nation_id) {
  const char* n = colonies_peek_next_name(pool, nation_id);
  if (pool) {
    const int nid = (nation_id < 0 || nation_id > 3) ? 0 : nation_id;
    if (pool->name_count[nid] != 0) {
      pool->name_next[nid]++;
    }
  }
  return n;
}

static void colonies_grant_building(
  ColonizeColonyPool* pool, ColonizeColony* slot, ColonizeBuildingRow row
) {
  const int idx = colonies_building_row(pool, row);
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
  static const ColonizeBuildingRow k_starters[] = {
    COLONY_BUILDING_TOWN_HALL,
    COLONY_BUILDING_CARPENTERS_SHOP,
    COLONY_BUILDING_BLACKSMITHS_HOUSE,
    COLONY_BUILDING_WEAVERS_HOUSE,
    COLONY_BUILDING_TOBACCONISTS_HOUSE,
    COLONY_BUILDING_RUM_DISTILLERS_HOUSE,
    COLONY_BUILDING_FUR_TRADERS_HOUSE,
  };
  for (size_t i = 0; i < sizeof(k_starters) / sizeof(k_starters[0]); ++i) {
    colonies_grant_building(pool, slot, k_starters[i]);
  }
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
    const int d = map_dos_dist(x - (int)t->x, y - (int)t->y);
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
  /*
   * DOS-LITERAL FUN_4cc6_07c2 raw 81213-81219 (bugs.md #710), both terms in
   * this order and both before the <1 clamp:
   *   iVar1 = (int)-(*(byte *)(param_2 + -0x6bf0) - 10) >> 1;
   *   if (iVar1 < 0) iVar1 = 0;
   *   local_4 = local_4 - iVar1;
   *   iVar1 = FUN_281f_0718(0x281f, param_3, param_4);
   *   if (iVar1 != -1) local_4 = local_4 * 2;
   * DS:nation−0x6bf0 is DS:0x9410 = census_pop_proxy[nation] (resolved
   * 2026-09-06, see ai_goals.h / original_sources_annotated/ai/king_ref.md),
   * so a small nation pays less. FUN_281f_0718(x, y) is the special-resource
   * probe (== the human Build handler's raw-45631 `!= -1` count), i.e.
   * map_resource_type_at != -1 → the tile is worth double.
   */
  {
    const unsigned census =
      (nation_id >= 0 && nation_id < 4) ? (unsigned)col1->stuff.census_pop_proxy[nation_id] : 0u;
    int adj = -((int)census - 10) >> 1;
    if (adj < 0) {
      adj = 0;
    }
    score -= adj;
  }
  if (map && map_resource_type_at(map, x, y) != -1) {
    score *= 2;
  }
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
 * bugs.md #284 — DOS FUN_15eb_26e4 (colony screen 5x5 Indian-land table):
 * a tile shows the totem / triggers a work complaint when a village's
 * tech-tier radius covers it, the tribe has been MET (bit 0x20), the land
 * has not been bought (purchase price > 0 also folds in Peter Minuit — the
 * whole table empties with FF 2), and no colony sits on it. Returns the
 * claiming tribe index, or -1.
 *
 * bugs.md #366 — two rules the first port of the table dropped, both visible as
 * totems in the wrong place:
 *  - `FUN_13e4_0074(tile)` clears the slot on terrain index 25/26 (Ocean and
 *    Sea Lane), so a coastal colony never shows a totem out on the water;
 *  - the village search takes the continent of the COLONY tile, not of the
 *    cell being tested (`uVar2 = FUN_137f_02a0(colony.x, colony.y)` hoisted
 *    above the 5x5 loop), so a village on a neighbouring island can never
 *    claim a cell of this colony's ring.
 * `origin_x`/`origin_y` are that colony tile.
 */
int colonies_indian_claim_tribe_from_w(
  const ColonizeWorld* w,
  int viewer_nation,
  int origin_x,
  int origin_y,
  int x,
  int y
) {
  const ColonizeCol1Save* col1 = w->col1;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* pool = w->colonies;

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

int colonies_found_with_indian_land_w(
  const ColonizeWorld* w,
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
  ColonizeColonyPool* pool = w->colonies;
  const ColonizeWorldMap* map = w->map;
  ColonizeCol1Save* col1 = w->col1;

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

static ColonizeCol1Save* g_colonies_col1 = NULL;

/*
 * Founding on a tile whose previous colony is gone: DOS deletes the colony
 * record when a colony is destroyed/abandoned, so FUN_364b_1ba8 always mints
 * a fresh one. The port keeps the COL1 array as a mirror that is only rebuilt
 * on the next export, and every reader pairs runtime colony <-> record by
 * tile alone (colony_prod_sol_percent, reports_pool_colony_for, the
 * col1_bridge capture pairing). A colony re-founded on the same tile before
 * that export therefore inherited the dead colony's SoL history (and, through
 * the capture pairing, its buildings/stock). Wipe the orphan record in place
 * instead: keep the slot (COL1 colony indices are referenced elsewhere), keep
 * the tile, and reseed it like a founding — rebel_divisor = 100, dividend 0
 * (FUN_364b_1ba8, mirrored in col1_bridge's unmatched-colony branch).
 */
static void colonies_col1_forget_record_at(ColonizeCol1Save* col1, int x, int y, int nation_id) {
  if (!col1 || !col1->colony) {
    return;
  }
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    ColonizeCol1Colony* c = &col1->colony[i];
    if ((int)c->x != x || (int)c->y != y) {
      continue;
    }
    memset(c, 0, sizeof(*c));
    c->x = (uint8_t)x;
    c->y = (uint8_t)y;
    c->nation_id = (uint8_t)(nation_id >= 0 && nation_id <= 3 ? nation_id : 0);
    c->rebel_divisor = 100;
    c->building_in_production = 0xFF;
    break;
  }
}

/*
 * DOS colony-screen colonist add (viceroy_unpacked.c:11301) does
 * `rebel_divisor += 100` and remove (:10279) `rebel_divisor -= 100`, so the
 * FUN_15eb_0274 SoL% (dividend*100/divisor) moves the moment population
 * changes instead of waiting for the EOT accumulator (bugs.md: "adding a
 * colonist doesn't recalculate SoL instantly"). Record absent (colony
 * founded this turn, mirror not yet exported) → nothing to touch.
 */
static void colonies_col1_rebel_divisor_adjust(ColonizeCol1Save* col1, int x, int y, int delta) {
  if (!col1 || !col1->colony) {
    return;
  }
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    ColonizeCol1Colony* c = &col1->colony[i];
    if ((int)c->x != x || (int)c->y != y) {
      continue;
    }
    if (delta < 0 && c->rebel_divisor < (uint32_t)(-delta)) {
      c->rebel_divisor = 0;
    } else {
      c->rebel_divisor = (uint32_t)((int64_t)c->rebel_divisor + delta);
    }
    if (c->rebel_dividend > c->rebel_divisor) {
      c->rebel_dividend = c->rebel_divisor;
    }
    break;
  }
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
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    slot->tiles[t] = -1;
  }
  /* Drop any orphan COL1 record left on this tile by a destroyed colony —
   * every runtime<->COL1 pairing is by tile, so its SoL/buildings would be
   * inherited by this brand-new colony (see colonies_col1_forget_record_at). */
  colonies_col1_forget_record_at(g_colonies_col1, x, y, nation_id);
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
    c->unit_type_index = founder_type_index;
    c->profession =
      (founder_profession >= 0) ? founder_profession : UNITS_JOB_NONE;
    c->building_type = colonies_building_row(pool, COLONY_BUILDING_TOWN_HALL);
    c->field_job = -1;
    slot->population = slot->colonist_count;
  } else {
    slot->population = 0;
  }

  /*
   * DOS colony +0x1c bit 0x40, the coastal bit — set here and nowhere else,
   * mirroring FUN_364b_1ba8 (viceroy_unpacked.c 58105-58110), which zeroes the
   * flag byte and ORs 0x40 in from the site test alone. It used to be a side
   * effect of the Docks branch below, so a coastal colony founded at pop >= 3
   * (Stockade branch) or one whose "Docks" row was missing/already built never
   * got the bit. Nothing recomputes or clears it afterwards — the image has no
   * other writer — so from here it is save-carried state. Predicate lives in
   * map.c (map_tile_is_open_sea_adjacent) and is shared with the AI refresh.
   * Smell audit 2026-09-10 D4.
   */
  if (map_tile_is_open_sea_adjacent(map, x, y)) {
    slot->colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
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
    const int stockade = colonies_building_row(pool, COLONY_BUILDING_STOCKADE);
    if (stockade >= 0 && !slot->has_building[stockade] &&
        (pool->building_types[stockade].min_population <= 0 ||
         slot->population >= pool->building_types[stockade].min_population)) {
      slot->building_in_production = stockade;
      slot->hammers = 0;
    } else if ((slot->colony_flags & COLONIZE_COLONY_FLAG_COASTAL) != 0) {
      /*
       * Gate on the coastal FLAG stamped above, not the loose live probe:
       * DOS's own Docks buildability filter IS that flag (raw 13688:
       * `local_e == 7 && (colony+0x1c & 0x40) == 0 -> reject`, building id 7),
       * so a site the flag rejects could never carry Docks as a project. The
       * first-project default itself stays a port heuristic (DOS's found-colony
       * writes +0x8d = 0xff, i.e. no project at all), but its Docks/Warehouse
       * fork now uses DOS's own predicate. Practical delta vs the old
       * map_tile_is_coastal probe: a lake-only or map-edge-only "coastal" town
       * starts on Warehouse. Smell audit D4 / 2026-09-10 lead 8.
       */
      const int docks = colonies_building_row(pool, COLONY_BUILDING_DOCKS);
      if (docks >= 0 && !slot->has_building[docks]) {
        slot->building_in_production = docks;
        slot->hammers = 0;
      }
    } else {
      const int warehouse = colonies_building_row(pool, COLONY_BUILDING_WAREHOUSE);
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
  return colonies_get_mut((ColonizeColonyPool*)pool, colony_id);
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

/*
 * Runtime plot slots. 0..7 are the port's own clockwise ring
 * N, NE, E, SE, S, SW, W, NW; 8..19 are the DOS outer plots in the DS:0xc8 /
 * DS:0xde order (slots 8..19 of those tables), so the runtime array and the
 * save's colony+0x70 array agree above 7 and col1_bridge only has to remap
 * 0..7.
 *
 * How many of these a colony actually works is per colony, not 8: DOS reads
 * `DS:0x329[FUN_15eb_0470()]` over {0,4,8,12,20} with
 * `FUN_15eb_0470 = min(FUN_15eb_039e(10),2)+2` (raw 9636-9645 / 9561-9578).
 * See colonies_work_plot_count(). bugs.md #593.
 */
static const int k_field_dx[COLONIZE_COLONY_FIELD_TILES_MAX] = {
  0, 1, 1, 1, 0, -1, -1, -1,
  0, 2, 0, -2, -1, 1, -1, 1, -2, -2, 2, 2
};
static const int k_field_dy[COLONIZE_COLONY_FIELD_TILES_MAX] = {
  -1, -1, 0, 1, 1, 1, 0, -1,
  -2, 0, 2, 0, -2, -2, 2, 2, -1, 1, -1, 1
};

bool colonies_field_tile_delta(int tile_index, int* out_dx, int* out_dy) {
  if (tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES_MAX) {
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
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    if (k_field_dx[i] == dx && k_field_dy[i] == dy) {
      return i;
    }
  }
  return -1;
}

/*
 * DS:0xc8 / DS:0xde (VICEROY.EXE file offset 121248 + addr), 20 entries each:
 * the DOS work-plot delta tables. Index order is
 *   0..7  N, E, S, W, NW, NE, SE, SW      (the ring the port stores in tiles[])
 *   8..19 the outer ring (tiles[8..19], worked only at ring tier 3/4)
 * `FUN_15eb_05e2` (raw ~9838) searches them for a (dx,dy) pair, and
 * `FUN_15eb_28c8` (raw 12993) walks them in this order, so the earliest index
 * wins an equal-score tie. The port's own slot order (k_field_dx/dy above) is
 * the MAP_DIR8 clockwise one and is kept — col1_bridge.c maps between the two.
 * bugs.md #584.
 */
static const int8_t k_dos_plot_dx[20] = {
  0, 1, 0, -1, -1, 1, 1, -1, 0, 2, 0, -2, -1, 1, -1, 1, -2, -2, 2, 2
};
static const int8_t k_dos_plot_dy[20] = {
  -1, 0, 1, 0, -1, -1, 1, 1, -2, 0, 2, 0, -2, -2, 2, 2, -1, 1, -1, 1
};

int colonies_field_scan_order(int step) {
  if (step < 0 || step >= COLONIZE_COLONY_FIELD_TILES_MAX) {
    return -1;
  }
  return colonies_field_tile_index((int)k_dos_plot_dx[step], (int)k_dos_plot_dy[step]);
}

/* FUN_15eb_05e2 (raw ~9838): the DOS plot slot (0..19) for a delta, or -1. */
static int colonies_dos_plot_index(int dx, int dy) {
  for (int i = 0; i < 20; ++i) {
    if ((int)k_dos_plot_dx[i] == dx && (int)k_dos_plot_dy[i] == dy) {
      return i;
    }
  }
  return -1;
}

/* Is plot slot `dos_index` of `oc` worked? DOS reads colony+0x70+index. */
static bool colonies_dos_plot_worked(const ColonizeColony* oc, int dos_index) {
  if (dos_index < 0) {
    return false;
  }
  if (dos_index >= COLONIZE_COLONY_FIELD_TILES_MAX) {
    return false;
  }
  const int rti = colonies_field_tile_index(
    (int)k_dos_plot_dx[dos_index], (int)k_dos_plot_dy[dos_index]
  );
  return rti >= 0 && (int)oc->tiles[rti] >= 0;
}

/*
 * DOS-LITERAL FUN_15eb_0470 raw 9636-9645 + the DS:0x329 table (VICEROY.EXE
 * file offset 121248+0x329 = {0,4,8,12,20}).
 *
 * `0470` is `min(FUN_15eb_039e(10),2)+2`, and `039e(10)` (raw 9561-9578)
 * counts the owned rows of the @BUILDING chain that starts at row 10, walking
 * the next-row byte at `row*0xc - 0x707a`. That chain is rows 0x0a then 0x0b,
 * the two Town Hall upgrades of NAMES.TXT:177-178 — the port's Town Hall chain
 * above its first row. DOS's buildability gate FUN_15eb_3650 (raw ~13674)
 * hard-zeroes both rows (`if (local_e == 10) local_14 = 0;` and the same for
 * 0xb), so its own construction menu can never offer them and every colony
 * stock DOS produces stays at tier 2 / ring 8. A save can still carry the bits
 * (colony buildings mask bits 9-11 = @BUILDING rows 9/10/11), and then DOS
 * really does work 12 or 20 plots — hence the per-colony count.
 */
int colonies_work_plot_count(const ColonizeColonyPool* pool, const ColonizeColony* col) {
  static const int k_ring_by_tier[5] = {0, 4, 8, 12, 20};
  int owned = 0;
  if (pool && col) {
    if (colonies_has_building_row(pool, col, COLONY_BUILDING_TOWN_HALL_2)) {
      owned++;
    }
    if (colonies_has_building_row(pool, col, COLONY_BUILDING_TOWN_HALL_3)) {
      owned++;
    }
  }
  if (owned > 2) {
    owned = 2;
  }
  return k_ring_by_tier[owned + 2];
}

/*
 * DOS-LITERAL FUN_15eb_23f2 raw 12695-12800 — the work-plot "blocked" byte.
 * `FUN_15eb_268e` (raw 12806-12820) caches the whole 5x5 into DS:0x8df0; this
 * port computes one plot on demand (the cache is a DOS speed trick, not a
 * rule). Both consumers — the AI plot scan `FUN_15eb_28c8` (raw 12993) and the
 * human area-view click `FUN_2f2b_3fa6` (raw 50917) — require byte == 0.
 *
 * Bits, in DOS order:
 *   0x10  off the playable map, outside the colony's work radius, or
 *         unexplored by the colony's nation (an early `return 0x10`, so no
 *         other bit can be set with it);
 *   0x80  the tile is owned by another European nation (layer2 unit flag +
 *         layer3 owner nibble), is not water, and carries a fortified unit
 *         that is either armed (type attack > 1) or a Pioneer (DOS type 2)
 *         of a human-controlled nation. DOS also reveals that unit to the
 *         colony's nation here (`FUN_1427_0992`); the port's query is const
 *         and skips that side effect;
 *   0x02  Lost City Rumour tile (`FUN_137f_0598`);
 *   0x04  an Indian village stands on it;
 *   0x20  another colony's centre tile;
 *   0x40  the plot is already worked by another colony;
 *   0x08  the colony's own centre tile (unreachable through this entry point,
 *         which only takes the 8 ring slots).
 */
uint8_t colonies_plot_blocked_mask(
  const ColonizeWorld* w,
  const ColonizeColony* col,
  int tile_index
) {
  if (!w || !col || !col->active) {
    return 0x10u;
  }
  int dx = 0;
  int dy = 0;
  if (!colonies_field_tile_delta(tile_index, &dx, &dy)) {
    return 0x10u;
  }
  const ColonizeWorldMap* map = w->map;
  const int x = col->x + dx;
  const int y = col->y + dy;
  /* FUN_137f_000a: the playable interior. */
  if (!map || !map_coords_inset(map, x, y)) {
    return 0x10u;
  }
  /*
   * DOS-LITERAL FUN_137f_003c raw 6535-6557, called as
   * `FUN_137f_003c(|dx|,|dy|,FUN_15eb_0470())` (raw 12727): the work-radius
   * test. Tier 1 keeps |dx|+|dy| < 2, tier 2 adds the diagonals (the 3x3
   * block), tier 3 adds |dx|+|dy| < 3, tier 4 takes everything in the 5x5 but
   * the four corners. The tier is per colony (bugs.md #593), recovered here
   * from the plot count rather than assumed to be 2.
   */
  const int ring = colonies_work_plot_count(w->colonies, col);
  int tier = 2;
  if (ring <= 4) {
    tier = 1;
  } else if (ring <= 8) {
    tier = 2;
  } else if (ring <= 12) {
    tier = 3;
  } else {
    tier = 4;
  }
  const int adx = dx < 0 ? -dx : dx;
  const int ady = dy < 0 ? -dy : dy;
  bool in_radius = (adx + ady) < 2;
  if (tier != 1) {
    if (adx < 2 && ady < 2) {
      in_radius = true;
    }
    if (tier != 2) {
      in_radius = in_radius || (adx + ady) < 3;
      if (tier != 3 && (adx < 2 || ady < 2)) {
        in_radius = true;
      }
    }
  }
  if (!in_radius) {
    return 0x10u;
  }
  /* `3 < colony_nation || (FUN_137f_02f8(x,y) & (0x10 << nation))` — a native
   * settlement skips the fog test, a European colony needs the tile seen. */
  if (col->nation_id < 4 && !map_tile_seen_by(map, x, y, col->nation_id)) {
    return 0x10u;
  }
  uint8_t mask = 0;
  const ColonizeCol1Save* col1 = w->col1_ok ? w->col1 : NULL;
  /* 0x80: foreign-owned tile guarded by a fortified unit. */
  const size_t mi = (size_t)y * (size_t)map->width + (size_t)x;
  const int occ = map->layer2 ? (int)map->layer2[mi] : 0;
  const int owner =
    ((occ & (int)MAP_OCCUPANCY_HAS_UNIT) != 0 && map->layer3)
      ? (int)((map->layer3[mi] >> MAP_L3_OWNER_SHIFT) & MAP_L3_OWNER_MASK)
      : -1;
  if (owner >= 0 && owner != col->nation_id && owner < 4 &&
      !map_tile_is_water((ColonizeWorldMap*)map, x, y) && w->units) {
    const int pioneer_type = units_kind_type_index(w->units, UNITS_KIND_PIONEER);
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &w->units->units[i];
      if (!u->active || u->x != x || u->y != y) {
        continue;
      }
      if (u->orders != UNITS_ORDER_FORTIFY && u->orders != UNITS_ORDER_FORTIFIED) {
        continue; /* raw 12747: `+0x314c == 5 || == 6` */
      }
      const ColonizeUnitType* ut = units_type(w->units, u->type_index);
      bool blocks = (ut && ut->attack > 1); /* type table +0x5236 */
      if (!blocks && pioneer_type >= 0 && u->type_index == pioneer_type &&
          u->nation_id >= 0 && u->nation_id < 4 && col1 && col1->player &&
          col1->player[u->nation_id].control == 0) {
        blocks = true; /* raw 12745: DOS type 2, nation < 4, 0x543f == 0 */
      }
      if (blocks) {
        mask |= 0x80u;
      }
    }
  }
  /* 0x02: FUN_137f_0598 rumour tile. */
  if (map_dos_0598_rumour_tile(map, x, y)) {
    mask |= 0x02u;
  }
  /* 0x04: an Indian settlement record on the tile (DOS walks all of them). */
  if (col1 && col1_save_tribe_at(col1, x, y)) {
    mask |= 0x04u;
  }
  /* 0x20 / 0x40: every OTHER colony's centre and worked plots. */
  if (w->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* oc = &w->colonies->colonies[i];
      if (!oc->active || oc->id == col->id) {
        continue;
      }
      if (oc->x == x && oc->y == y) {
        mask |= 0x20u;
      }
      const int ddx = oc->x - x < 0 ? x - oc->x : oc->x - x;
      const int ddy = oc->y - y < 0 ? y - oc->y : oc->y - y;
      if (ddx < 3 && ddy < 3 &&
          colonies_dos_plot_worked(oc, colonies_dos_plot_index(x - oc->x, y - oc->y))) {
        mask |= 0x40u;
      }
    }
  }
  return mask;
}

int colonies_colonist_tile(const ColonizeColony* colony, int colonist_index) {
  if (!colony || colonist_index < 0) {
    return -1;
  }
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
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
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    if ((int)col->tiles[i] == colonist_index) {
      col->tiles[i] = -1;
    }
  }
}

/*
 * NAMES.TXT @JOB **column 2** — the school tier (1..4) a profession needs.
 * DOS `FUN_364b_0688` (raw 57518) reads the loaded record at `-0x715a +
 * prof*8`, i.e. the third word of the stride-8 @JOB row, so the port reads
 * the same catalog column instead of compiling a copy of it (bugs.md #596,
 * data_vs_hardcoded Part D). 0 when the catalog or the row is missing.
 */
int colonies_job_school_tier(int profession) {
  if (profession < 0 || profession >= 28) {
    return 0;
  }
  const char* field = reports_names_field("JOB", profession, 2);
  if (!field) {
    return 0;
  }
  const int tier = atoi(field);
  return (tier >= 1 && tier <= 4) ? tier : 0;
}

int colonies_school_building_tier(
  const ColonizeColonyPool* pool,
  int building_type
) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return 0;
  }
  switch (colonies_building_type_row(pool, building_type)) {
  case COLONY_BUILDING_UNIVERSITY:
    return 3;
  case COLONY_BUILDING_COLLEGE:
    return 2;
  case COLONY_BUILDING_SCHOOLHOUSE:
    return 1;
  default:
    return 0;
  }
}

/*
 * The school tier this colony actually OWNS: University 3, College 2,
 * Schoolhouse 1, none 0. DOS's work-assign validator
 * (thunk_FUN_1000_9808, overlays.c:60459-60484) never looks at the school row
 * the player clicked — it asks `FUN_1000_8bec(0xe/0xd/0xc)`, "does the colony
 * own this @BUILDING", both for the faculty cap (@UNIV3 3 / @COLLEGE2 2 /
 * @SCHOOL1 1 teachers) and for the teacher's own level requirement
 * (@NEEDUNIVERSITY / @NEEDCOLLEGE). DOS never clears the lower tiers, so a
 * colony with a University owns all three rows. bugs.md #580, #589.
 */
int colonies_school_owned_tier(const ColonizeColonyPool* pool, const ColonizeColony* col) {
  if (!pool || !col) {
    return 0;
  }
  static const int k_rows[3] = {
    COLONY_BUILDING_UNIVERSITY, COLONY_BUILDING_COLLEGE, COLONY_BUILDING_SCHOOLHOUSE
  };
  for (int i = 0; i < 3; ++i) {
    const int bi = colonies_building_row(pool, k_rows[i]);
    if (bi >= 0 && bi < pool->building_type_count && col->has_building[bi]) {
      return 3 - i;
    }
  }
  return 0;
}

/*
 * The DOS @JOB occupation a workable building employs, or -1. The mirror of
 * col1_bridge.c's building→occupation switch (the save byte DOS stores per
 * colonist): DOS has no per-building membership at all, so both the school
 * faculty cap and @MORETHANTHREE tally COLONISTS BY OCCUPATION
 * (`FUN_1000_8dfe` / `FUN_15eb_1376`), which spans a chain's tiers —
 * Church and Cathedral are one Preacher pool.
 */
int colonies_building_occupation(const ColonizeColonyPool* pool, int building_type) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return -1;
  }
  switch (colonies_building_row_chain(colonies_building_type_row(pool, building_type))) {
    case COLONIES_CHAIN_RUM:         return 9;
    case COLONIES_CHAIN_TOBACCONIST: return 10;
    case COLONIES_CHAIN_WEAVER:      return 11;
    case COLONIES_CHAIN_FUR:         return 12;
    case COLONIES_CHAIN_CARPENTER:   return 13;
    case COLONIES_CHAIN_BLACKSMITH:  return 14;
    case COLONIES_CHAIN_ARMORY:      return 15;
    case COLONIES_CHAIN_CHURCH:      return 16;
    case COLONIES_CHAIN_TOWN_HALL:   return 17;
    case COLONIES_CHAIN_SCHOOL:      return COLONIES_JOB_TEACHER;
    default:                         return -1;
  }
}

/* Colonists working that occupation, skipping `except_index` (-1 = none). */
int colonies_occupation_worker_count(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int occupation,
  int except_index
) {
  if (!pool || !col || occupation < 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < col->colonist_count; ++i) {
    if (i == except_index) {
      continue;
    }
    const ColonizeColonist* c = &col->colonists[i];
    if (!c->active || c->building_type < 0) {
      continue;
    }
    if (colonies_building_occupation(pool, c->building_type) == occupation) {
      n++;
    }
  }
  return n;
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

/*
 * DOS FUN_364b_0688 (viceroy_unpacked.c ~57519): a colonist may teach when the
 * @JOB school level of his specialty is below 4 — `local_8c = jobtable[prof].
 * level; if (local_8c < 4)`. Level 4 covers @JOB 18 "Teacher"/19 "Colonist"
 * (the two placeholder rows) plus Indentured Servant, Petty Criminal and
 * Indian Convert, so the old hand-written exclusion list was right about
 * everything except @JOB 18, which it wrongly admitted.
 */
bool colonies_profession_may_teach(int profession) {
  const int level = colonies_job_school_tier(profession);
  return level >= 1 && level <= 3;
}

/*
 * NAMES.TXT @JOB column 0, with the table below as the no-catalog fallback
 * (audit CO-33). The fallback also decides WHICH professions get a label at
 * all: 0x13 Colonist / 0x19 Ind. Servant / 0x1a Criminal / 0x1b Convert are
 * deliberately absent and answer "profession", matching DOS
 * FUN_15eb_0002's non-expert gate, so the live lookup is only consulted for
 * professions the fallback already names.
 */
/* Professions DOS labels at all (FUN_15eb_0002's non-expert gate): the field
 * jobs, the ten indoor trades and the five equipped roles. 0x13 Colonist /
 * 0x19 Ind. Servant / 0x1a Criminal / 0x1b Convert answer the port's neutral
 * "profession". The wording itself is NAMES.TXT @JOB column 0. */
static bool colonies_profession_is_labelled(int profession) {
  if (profession >= 0 && profession < COLONIZE_FIELD_JOB_COUNT) {
    return true;
  }
  switch (profession) {
  case COLONIZE_PROF_DISTILLER:
  case COLONIZE_PROF_TOBACCONIST:
  case COLONIZE_PROF_WEAVER:
  case COLONIZE_PROF_FUR_TRADER:
  case COLONIZE_PROF_CARPENTER:
  case COLONIZE_PROF_BLACKSMITH:
  case COLONIZE_PROF_GUNSMITH:
  case COLONIZE_PROF_PREACHER:
  case COLONIZE_PROF_STATESMAN:
  case COLONIZE_PROF_TEACHER:
  case UNITS_JOB_PIONEER:
  case UNITS_JOB_SOLDIER:
  case UNITS_JOB_SCOUT:
  /* bugs.md #656: 0x17 (UNITS_JOB_DRAGOON) dropped — DOS never writes it
   * to a unit (#503/#639), matching units.c's 0x15-only veteran gates. */
  case UNITS_JOB_MISSIONARY:
    return true;
  default:
    return false;
  }
}

const char* colonies_profession_name(int profession) {
  if (!colonies_profession_is_labelled(profession)) {
    return ""; /* no catalog row = empty string, never a typed fallback */
  }
  return reports_job_short_name(profession);
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
    "",
    body,
    sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

/*
 * @SCHOOL1 / @COLLEGE2 / @UNIV3 (GAME.TXT:474-487) — the faculty cap refusal,
 * overlays.c:60459-60472 cases 9 / 8 / 7 (tag ids 0xc29 / 0xc20 / 0xc1a).
 * `owned_tier` is colonies_school_owned_tier's 1/2/3.
 */
void colonies_emit_school_faculty_chrome(
  int owned_tier,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || owned_tier < 1 || owned_tier > 3) {
    return;
  }
  static const char* const k_sections[3] = {"SCHOOL1", "COLLEGE2", "UNIV3"};
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(messages, k_sections[owned_tier - 1], NULL, "", body, sizeof(body));
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
  const char* pname = colonies_profession_name(profession);
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = pname;
  popup_msg_fill(messages, section, &tok, "", body, sizeof(body));
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
  /* Chains with no crew: fortifications, the docks line, Warehouse (+
   * Expansion), Stable, Custom House, Printing Press/Newspaper, Capitol. */
  switch (colonies_building_row_chain(colonies_building_type_row(pool, building_type))) {
    case COLONIES_CHAIN_FORTIFICATION:
    case COLONIES_CHAIN_DOCKS:
    case COLONIES_CHAIN_WAREHOUSE:
    case COLONIES_CHAIN_STABLE:
    case COLONIES_CHAIN_CUSTOM_HOUSE:
    case COLONIES_CHAIN_PRESS:
    case COLONIES_CHAIN_CAPITOL:
      return false;
    default:
      break;
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
  /*
   * DOS work-assign validator thunk_FUN_1000_9808 (overlays.c:60412-60498),
   * in its own order. What it validates is an @JOB OCCUPATION, not a building:
   * `iStack_10 = FUN_1000_8dfe(colonist); if (iStack_10 == param_2) return 0`
   * lets a no-op reassignment through before any cap runs.
   * bugs.md #580 / #589 / #590.
   */
  const int occupation = colonies_building_occupation(pool, building_type);
  const int cur_occupation =
    (c->building_type >= 0) ? colonies_building_occupation(pool, c->building_type) : -1;
  if (occupation >= 0 && occupation != cur_occupation) {
    if (occupation == COLONIES_JOB_TEACHER) {
      /* overlays.c:60459-60472: faculty cap = the best school OWNED —
       * University 3 (@UNIV3), College 2 (@COLLEGE2), Schoolhouse 1
       * (@SCHOOL1) teachers, counted over the whole colony. */
      const int cap = colonies_school_owned_tier(pool, col);
      if (cap > 0 &&
          colonies_occupation_worker_count(pool, col, COLONIES_JOB_TEACHER, -1) >= cap) {
        return false;
      }
      /* overlays.c:60474-60484: @JOB school level > 3 → @NOTEACHER; level 3
       * without a University (@BUILDING 0xe) → @NEEDUNIVERSITY; level 2
       * without a College (0xd) → @NEEDCOLLEGE. Again the test is what the
       * colony OWNS, never the tier of the clicked school row. */
      if (!colonies_profession_may_teach(c->profession)) {
        return false;
      }
      if (colonies_school_tier_shortfall(c->profession, cap) != 0) {
        return false;
      }
    }
    /*
     * DOS-LITERAL overlays.c:60486-60496 @MORETHANTHREE: DOS tallies every
     * OTHER colonist by occupation and refuses a fourth with
     * `if ((2 < aiStack_42[param_2]) && (9 < param_2)) return 0x16;` — the
     * `9 < param_2` half means @JOB 9 (Rum Distiller) has no cap at all.
     * Verbatim, oddity included.
     */
    if (occupation > 9 &&
        colonies_occupation_worker_count(pool, col, occupation, colonist_index) > 2) {
      return false;
    }
  }
  colonies_clear_colonist_tile(col, colonist_index);
  /* FUN_15eb_1068 (raw 11256-11258): `if (param_2 != current_job)
   * FUN_15eb_0cbc(colonist, 0)` — a real job change zeroes the +0x60
   * education nibble; a no-op reassignment keeps it. */
  if (c->building_type != building_type || c->field_job >= 0) {
    c->turns_in_job = 0;
  }
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
  const int ch = colonies_building_row(pool, COLONY_BUILDING_CUSTOM_HOUSE);
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
  /* Only the colony's own ring can be seated (DS:0x329[FUN_15eb_0470()],
   * bugs.md #593) — slots beyond it are outside the work radius. */
  if (tile_index < 0 || tile_index >= colonies_work_plot_count(pool, col)) {
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
  if (c->building_type >= 0 || c->field_job != field_job) {
    c->turns_in_job = 0; /* FUN_15eb_1068 raw 11256-11258, as assign_workplace */
  }
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
  if (tile_index < 0 || tile_index >= COLONIZE_COLONY_FIELD_TILES_MAX) {
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

int colonies_admit_unit_w(
  const ColonizeWorld* w,
  int colony_id,
  int unit_id
) {
  ColonizeColonyPool* pool = w->colonies;
  ColonizeUnitPool* units = w->units;
  const ColonizeCol1Save* col1 = w->col1;

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
  int work_type = units_kind_type_index(units, UNITS_KIND_COLONIST);
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
  colonies_col1_rebel_divisor_adjust(g_colonies_col1, col->x, col->y, 100);
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
    const int stockade = colonies_building_row(pool, COLONY_BUILDING_STOCKADE);
    if (stockade >= 0 && col->building_in_production == stockade) {
      col->building_in_production = -1;
    }
  }
  /* bugs.md #256: every admit path (AI joins, capture, save import) puts the
   * newcomer to work immediately — DOS has no idle colonists, and an idle
   * one made the head count disagree with the visible workers. */
  colonies_seat_new_colonist(pool, colony_id, idx);
  return idx;
}

/*
 * DOS `FUN_15eb_1068(slot, 0xd)` — the auto-assign fallback both
 * FUN_15eb_2ea0 (raw 13189-13192) and FUN_15eb_28c8 (raw 13152) use when no
 * work plot scores: @JOB row 13, Carpenter. DOS sets the job unconditionally;
 * the port needs a workplace to put the colonist in, so it seats him in the
 * Carpenter chain when the colony owns one and only then falls back to any
 * other non-school building (bugs.md #6/#256/#408 keep their intent: no idle
 * colonist, and never a silent re-seat at a school).
 */
void colonies_assign_carpenter_fallback(ColonizeColonyPool* pool, int colony_id, int colonist_index) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!pool || !col) {
    return;
  }
  const char* const* chain = colonies_building_chain(COLONIES_CHAIN_CARPENTER);
  for (int i = 0; chain && chain[i]; ++i) {
    const int bi = colonies_find_building(pool, chain[i]);
    if (bi >= 0 && bi < COLONIZE_BUILDING_TYPES_MAX && col->has_building[bi] &&
        colonies_assign_workplace(pool, colony_id, colonist_index, bi)) {
      return;
    }
  }
  for (int bi = 0; bi < pool->building_type_count; ++bi) {
    if (!col->has_building[bi] || colonies_school_building_tier(pool, bi) > 0) {
      continue;
    }
    if (colonies_assign_workplace(pool, colony_id, colonist_index, bi)) {
      return;
    }
  }
}

/*
 * bugs.md #562: DOS's join path is FUN_15eb_3930 -> FUN_15eb_2ea0 ->
 * FUN_15eb_28c8 — the newcomer takes the best-scoring WORK PLOT, and only when
 * nothing scores does he become a Carpenter (`1068(slot, 0xd)`). The port used
 * to seat every joiner in the Town Hall, which has no DOS counterpart at all,
 * so an Expert Farmer joining a colony with a free Plains plot made 0 food.
 */
void colonies_seat_new_colonist(ColonizeColonyPool* pool, int colony_id, int colonist_index) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!pool || !col || colonist_index < 0 || colonist_index >= col->colonist_count) {
    return;
  }
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.colonies = pool;
  ctx.map = g_colonies_occupancy_map;
  ctx.col1 = g_colonies_col1;
  ctx.col1_ok = g_colonies_col1 != NULL;
  /* bVar1 (raw 12952-12958) is `colony+0x1a < 4 && player[nation].control
   * == 0` — a per-colony test, so feeding the scorer this colony's own nation
   * when it is human-controlled answers it exactly (and keeps colony.c free of
   * a save-layer call the slim test targets do not link). */
  ctx.human_nation = -1;
  if (g_colonies_col1 && col->nation_id >= 0 && col->nation_id < 4 &&
      g_colonies_col1->player[col->nation_id].control == 0) {
    ctx.human_nation = col->nation_id;
  }
  if (!ctx.map) {
    /* No map bound (headless import / unit tests): DOS's no-plot outcome. */
    colonies_assign_carpenter_fallback(pool, colony_id, colonist_index);
    return;
  }
  ai_euro_28c8_auto_assign_plots(&ctx, colony_id, colonist_index);
}

/*
 * Stale-save sweep, NOT a DOS routine: a colonist imported with DOS
 * occupation 0x13 (@JOB row 19, plain "Colonist") is genuinely idle in DOS —
 * FUN_15eb_0e18 returns 19, so FUN_15eb_2ea0's `< 9` gate skips him, and the
 * colony_prod01 DOS capture shows he stays unproductive. The port still needs
 * him visible on the settlement grid (bugs.md #6/#256), so he is parked in a
 * building; new joiners go through colonies_seat_new_colonist instead.
 */
void colonies_auto_assign_idle(ColonizeColonyPool* pool, int colony_id) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!pool || !col) {
    return;
  }
  const int town_hall = colonies_building_row(pool, COLONY_BUILDING_TOWN_HALL);
  for (int i = 0; i < col->colonist_count; ++i) {
    ColonizeColonist* c = &col->colonists[i];
    if (!c->active || c->field_job >= 0 || c->building_type >= 0) {
      continue;
    }
    if (town_hall >= 0 && colonies_assign_workplace(pool, colony_id, i, town_hall)) {
      continue;
    }
    /* bugs.md #408: never quietly seat a specialist back at a school. */
    colonies_assign_carpenter_fallback(pool, colony_id, i);
  }
}

const char* colonies_eject_role_name(int role) {
  /* NAMES.TXT @JOB column 0: rows 20..24 are the equipped roles, 19 Colonist. */
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    return reports_job_short_name(UNITS_JOB_PIONEER);
  case COLONIZE_EJECT_SOLDIER:
    return reports_job_short_name(UNITS_JOB_SOLDIER);
  case COLONIZE_EJECT_SCOUT:
    return reports_job_short_name(UNITS_JOB_SCOUT);
  case COLONIZE_EJECT_DRAGOON:
    return reports_job_short_name(UNITS_JOB_DRAGOON);
  case COLONIZE_EJECT_MISSIONARY:
    return reports_job_short_name(UNITS_JOB_MISSIONARY);
  case COLONIZE_EJECT_COLONIST:
  default:
    return reports_job_short_name(19);
  }
}

int colonies_has_church_or_cathedral(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col
) {
  if (!pool || !col) {
    return 0;
  }
  const int church = colonies_building_row(pool, COLONY_BUILDING_CHURCH);
  const int cath = colonies_building_row(pool, COLONY_BUILDING_CATHEDRAL);
  if (church >= 0 && church < COLONIZE_BUILDING_TYPES_MAX && col->has_building[church]) {
    return 1;
  }
  if (cath >= 0 && cath < COLONIZE_BUILDING_TYPES_MAX && col->has_building[cath]) {
    return 1;
  }
  return 0;
}

/*
 * Tools handed to a body being equipped as a Pioneer: whole 20-tool steps,
 * capped at 100. DOS-LITERAL, not a port convenience — verified 2026-09-10
 * (seventh wave, second-wave lead 7, which suspected a deviation):
 *   FUN_15eb_1068, the equip/leave-as applier (viceroy_unpacked.c 11250-11253):
 *     local_8 = colony stock[TOOLS] (+0xb6) / 0x14;
 *     iVar6   = local_8 * 0x14;  if (100 < iVar6) iVar6 = 100;
 *   then `param_2 == 0x14` (profession Pioneer) writes that byte to the unit's
 *   tools field +0x3159 (raw 11274 and 11288, the re-type and the new-unit
 *   arms), and the tail at raw 11322-11331 charges the colony the same amount.
 *   The four dialog builders recompute it the same way for their row text
 *   (raw 50570-50571, 53455, 62541, 67774), clamped to [0x14, 100].
 * FUN_15eb_35d0's `min(stock, 100, req)` is a different path — the cargo-hold
 * loader, whose `req` is FUN_15eb_3208's free-hold count * 100 (raw 13375) —
 * i.e. loading 100-lots into a ship/wagon, never equipping a colonist.
 * bugs.md #356 (a Pioneer legitimately walks with 20/40/60/80/100) is the
 * behaviour this reproduces.
 */
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

/*
 * The "Leave as" row list, DOS's three states.
 *
 * This list is FUN_2f2b_348c's leave-as mode, rows = professions 0x13..0x18
 * (`local_fa = 0x13, local_146 = 6`), each asked through FUN_281f_0bb4 →
 * FUN_15eb_3454 (viceroy_unpacked.c 13518-13590), whose answer has THREE
 * values, not two:
 *   0       — row not offered at all; the dialog loop skips it outright
 *             (raw 50753 `if (local_e != 0)`, and the row counter at 50575).
 *   0xffff  — row offered but DISABLED: raw 50805 `if (local_e == -1)` calls
 *             the greyed-row draw FUN_291f_01b6. This is the short-stock
 *             answer: for each cargo in the row's FUN_15eb_0d8e list, colony
 *             stock < required (tools 0x14 = 20, muskets/horses 0x32 = 50)
 *             sets local_4 = 0xffff (raw 13580-13585).
 *   0xfffe  — ordinary enabled row.
 * Return 0 comes from exactly three places for these rows: an Indian Convert
 * (@JOB 0x1b) gets nothing but Colonist — raw 13557-13560,
 * `if (0x13 < param_1 && cur_prof == 0x1b) return 0` — the Missionary row
 * 0x18 needs the Church bit UNLESS the body is already a Jesuit
 * (FUN_15eb_038e(0x25) && cur_prof != 0x18, raw 13567-13569), and the
 * Colonist row 0x13 disappears for a Jesuit body under the DS:0x8dc6 test
 * (raw 13561-13565, see colonies_eject_row_offered below).
 *
 * The port used to OMIT short-stock gear rows instead of greying them, and
 * offered all six to a Convert. Both fixed 2026-09-10 (seventh wave,
 * second-wave lead 4). out_enabled (optional) carries the 0xfffe/0xffff
 * distinction; callers that pass NULL get the row list only.
 *
 * The same DOS function serves a unit standing on the fence (a band index at
 * or past the colonist count forces leave-as mode), so game_loop.c's
 * game_colony_list_outside_roles is a twin of this list and must stay
 * row-for-row identical, greying included.
 *
 * Earlier cites for the bless row: Colonization.pdf Establishing a Mission /
 * Church; building_production Missionary.
 */
/* 0-based pool slot of a colony record, DOS's colony index (DS:0x8dc6 is set
 * from the same kind of index by FUN_15eb_002c raw 9318). -1 if unknown. */
static int colonies_pool_slot_index(const ColonizeColonyPool* pool, const ColonizeColony* col) {
  if (!pool || !col) {
    return -1;
  }
  const ptrdiff_t slot = col - &pool->colonies[0];
  if (slot < 0 || slot >= (ptrdiff_t)COLONIZE_COLONIES_MAX) {
    return -1;
  }
  return (int)slot;
}

/*
 * DOS-LITERAL FUN_15eb_3454 raw 13556-13570 — the "is this leave-as row
 * offered at all" test (return 0 vs 0xfffe/0xffff), shared by the two row
 * builders (here and game_loop's outside twin) and by both appliers, so a
 * click cannot take a row the list never drew.
 *
 *   raw 13557-13560: rows above 0x13 return 0 for an Indian Convert (0x1b).
 *   raw 13561-13565: row 0x13 (Colonist) returns 0 when the body is already a
 *     Jesuit Missionary (0x18) AND `*(int*)0x8dc6 < 4` AND
 *     `*(char*)(*(int*)0x8dc6 * 0x34 + 0x543f) == 0`. DS:0x8dc6 is the ACTIVE
 *     COLONY INDEX (FUN_15eb_002c raw 9318 writes it from the colony record
 *     index), while 0x543f + n*0x34 is the NATION table whose byte 0 is the
 *     control flag (0 = human). DOS indexes the nation table with a colony
 *     index: a genuine DOS bug, ported literally per docs/project_goals.md —
 *     whether a Jesuit may be un-blessed back to a plain Colonist depends on
 *     which pool slot his colony happens to occupy.
 *   raw 13567-13569: row 0x18 (Missionary) returns 0 only when the colony has
 *     no Church AND the body is not already 0x18 — a Jesuit is always offered
 *     the Missionary row, churchless colony or not.
 */
bool colonies_eject_row_offered(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int profession,
  int role
) {
  if (!col) {
    return false;
  }
  if (role != COLONIZE_EJECT_COLONIST && profession == COLONIZE_PROF_CONVERT) {
    return false;
  }
  if (role == COLONIZE_EJECT_COLONIST) {
    if (profession == UNITS_JOB_MISSIONARY) {
      const int slot = colonies_pool_slot_index(pool, col);
      if (slot >= 0 && slot < 4 && g_colonies_col1 &&
          g_colonies_col1->player[slot].control == 0) {
        return false;
      }
    }
    return true;
  }
  if (role == COLONIZE_EJECT_MISSIONARY) {
    return colonies_has_church_or_cathedral(pool, col) || profession == UNITS_JOB_MISSIONARY;
  }
  return true;
}

int colonies_list_eject_roles_gear(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int add_tools,
  int add_muskets,
  int add_horses,
  int profession,
  int* out_roles,
  bool* out_enabled,
  int out_max
) {
  if (!col || !out_roles || out_max <= 0) {
    return 0;
  }
  const bool convert = (profession == COLONIZE_PROF_CONVERT);
  const int tools = col->stock[COLONIZE_CARGO_TOOLS] + add_tools;
  const int muskets = col->stock[COLONIZE_CARGO_MUSKETS] + add_muskets;
  const int horses = col->stock[COLONIZE_CARGO_HORSES] + add_horses;

  int n = 0;
  if (colonies_eject_row_offered(pool, col, profession, COLONIZE_EJECT_COLONIST)) {
    out_roles[n] = COLONIZE_EJECT_COLONIST;
    if (out_enabled) {
      out_enabled[n] = true;
    }
    ++n;
  }
  if (!convert) {
    const struct {
      int role;
      bool enabled;
    } k_gear[] = {
      {COLONIZE_EJECT_PIONEER, tools >= UNITS_EQUIP_TOOLS_STEP},
      {COLONIZE_EJECT_SOLDIER, muskets >= UNITS_EQUIP_MUSKETS},
      {COLONIZE_EJECT_SCOUT, horses >= UNITS_EQUIP_HORSES},
      {COLONIZE_EJECT_DRAGOON, muskets >= UNITS_EQUIP_MUSKETS && horses >= UNITS_EQUIP_HORSES}
    };
    for (size_t i = 0; i < sizeof(k_gear) / sizeof(k_gear[0]) && n < out_max; ++i) {
      out_roles[n] = k_gear[i].role;
      if (out_enabled) {
        out_enabled[n] = k_gear[i].enabled;
      }
      ++n;
    }
    /* Bless costs no cargo, so the row is never the greyed kind
     * (FUN_15eb_3454 row 0x18). */
    if (n < out_max &&
        colonies_eject_row_offered(pool, col, profession, COLONIZE_EJECT_MISSIONARY)) {
      out_roles[n] = COLONIZE_EJECT_MISSIONARY;
      if (out_enabled) {
        out_enabled[n] = true;
      }
      ++n;
    }
  }
  return n;
}

int colonies_list_eject_roles_ex(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  bool* out_enabled,
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
  /* raw 13556: every >= 0x13 row gate reads the body's own @JOB. */
  const int profession = col->colonists[colonist_index].profession;
  return colonies_list_eject_roles_gear(
    pool, col, 0, 0, 0, profession, out_roles, out_enabled, out_max
  );
}

bool colonies_eject_role_gear(
  int role,
  int stock_tools,
  int* out_tools,
  int* out_muskets,
  int* out_horses
) {
  int tools_take = 0;
  int muskets_take = 0;
  int horses_take = 0;
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    tools_take = colonies_equip_tools_take(stock_tools);
    break;
  case COLONIZE_EJECT_SOLDIER:
    muskets_take = UNITS_EQUIP_MUSKETS;
    break;
  case COLONIZE_EJECT_SCOUT:
    horses_take = UNITS_EQUIP_HORSES;
    break;
  case COLONIZE_EJECT_DRAGOON:
    muskets_take = UNITS_EQUIP_MUSKETS;
    horses_take = UNITS_EQUIP_HORSES;
    break;
  case COLONIZE_EJECT_COLONIST:
  case COLONIZE_EJECT_MISSIONARY:
    break;
  default:
    return false;
  }
  if (out_tools) {
    *out_tools = tools_take;
  }
  if (out_muskets) {
    *out_muskets = muskets_take;
  }
  if (out_horses) {
    *out_horses = horses_take;
  }
  return true;
}

int colonies_list_eject_roles(
  const ColonizeColonyPool* pool,
  int colony_id,
  int colonist_index,
  int* out_roles,
  int out_max
) {
  return colonies_list_eject_roles_ex(pool, colony_id, colonist_index, out_roles, NULL, out_max);
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
  ColonizeUnitKind type_kind = UNITS_KIND_COLONIST;
  /* Unknown rows fall through as a plain Colonist (the old `default:` arm). */
  (void)colonies_eject_role_gear(
    role, col->stock[COLONIZE_CARGO_TOOLS], &tools_take, &muskets_take, &horses_take
  );
  switch (role) {
  case COLONIZE_EJECT_PIONEER:
    if (tools_take <= 0) {
      return -1;
    }
    type_kind = UNITS_KIND_PIONEER;
    break;
  case COLONIZE_EJECT_SOLDIER:
    type_kind = UNITS_KIND_SOLDIER;
    break;
  case COLONIZE_EJECT_SCOUT:
    type_kind = UNITS_KIND_SCOUT;
    break;
  case COLONIZE_EJECT_DRAGOON:
    type_kind = UNITS_KIND_DRAGOON;
    break;
  case COLONIZE_EJECT_MISSIONARY:
    /* Row gate re-tested (Church bit, or a body that is already a Jesuit —
     * FUN_15eb_3454 raw 13567-13569). */
    if (!colonies_eject_row_offered(pool, col, c->profession, role)) {
      return -1;
    }
    type_kind = UNITS_KIND_MISSIONARY;
    break;
  case COLONIZE_EJECT_COLONIST:
    /* raw 13561-13565: the Colonist row can be missing for a Jesuit. */
    if (!colonies_eject_row_offered(pool, col, c->profession, role)) {
      return -1;
    }
    break;
  default:
    break;
  }
  if (col->stock[COLONIZE_CARGO_MUSKETS] < muskets_take ||
      col->stock[COLONIZE_CARGO_HORSES] < horses_take) {
    return -1;
  }

  int type_index = units_kind_type_index(units, type_kind);
  if (type_index < 0) {
    type_index = c->unit_type_index;
  }
  /* bugs.md #556: DOS's colony eject applier (overlays.c:10225-10245) writes
   * only the TYPE byte from table 0x2f5 and copies the profession byte
   * verbatim — `*(0x315b) = FUN_0000_6d02(param_1)` — for every row, bless
   * included. A blessed Expert Farmer stays an Expert Farmer under type 3.
   * The port used to wipe it to NONE here; the outside twin
   * (game_loop game_colony_apply_outside_role) never did. */
  const int profession = c->profession;

  colonies_clear_colonist_tile(col, colonist_index);
  for (int i = colonist_index; i < col->colonist_count - 1; ++i) {
    col->colonists[i] = col->colonists[i + 1];
  }
  col->colonist_count--;
  col->population = col->colonist_count;
  colonies_col1_rebel_divisor_adjust(g_colonies_col1, col->x, col->y, -100);
  if (col->colonist_count >= 0 && col->colonist_count < COLONIZE_COLONY_POP_MAX) {
    memset(&col->colonists[col->colonist_count], 0, sizeof(col->colonists[0]));
  }
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
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
    u->moves = 0;
  }
  return uid;
}

bool colonies_has_fortification(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  if (!pool || !colony) {
    return false;
  }
  const int* k_forts = colonies_building_chain_rows(COLONIES_CHAIN_FORTIFICATION);
  for (size_t i = 0; k_forts && k_forts[i] >= 0; ++i) {
    const int idx = colonies_building_row(pool, (ColonizeBuildingRow)k_forts[i]);
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
  const int fortress = colonies_building_row(pool, COLONY_BUILDING_FORTRESS);
  if (fortress >= 0 && fortress < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fortress]) {
    return 200;
  }
  const int fort = colonies_building_row(pool, COLONY_BUILDING_FORT);
  if (fort >= 0 && fort < COLONIZE_BUILDING_TYPES_MAX && colony->has_building[fort]) {
    return 150;
  }
  const int stockade = colonies_building_row(pool, COLONY_BUILDING_STOCKADE);
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
    /* FUN_5fef_1b0e moves the share between the two records' +0x2a — one word
     * per nation. The port's human treasury is live in EuropeScreen.gold, so
     * route both halves through the accessor or the human's plunder lands in a
     * copy the next export overwrites (audit G3). */
    const uint32_t gold = europe_nation_gold(NULL, col1, old_nation);
    plunder = (int)(((uint64_t)gold * (uint64_t)pop) / (uint64_t)total);
    europe_nation_gold_add(NULL, col1, old_nation, -(long)plunder);
    europe_nation_gold_add(NULL, col1, new_nation, (long)plunder);
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

bool colonies_unit_build_info(int raw_code, const char** name, int* hammers, int* tools_cost) {
  /*
   * DOS FUN_15eb_33aa kind-2 arm (raw 13489-13504) reads the @UNIT cost/tools
   * columns; units_build_project_info owns that arithmetic. The golden
   * Artillery pair (192/40, New Amsterdam, dutch-reports.SAV) and the
   * long-standing Wagon Train 40/0 both fall out of it, which is what pinned
   * the ×32 / ×10 scale factors.
   */
  return units_build_project_info(raw_code, name, hammers, tools_cost);
}

int colonies_nation_colony_count_census(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  return (int)col1->stuff.colony_counts[nation_id];
}

bool colonies_wagon_cap_reached(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return false;
  }
  /* DOS-LITERAL FUN_15eb_3650 raw 13736-13740:
   *   colony_counts[n] <= unit_type_counts[n][12]  ->  unavailable.
   * `<=` makes the cap exactly "wagons < colonies". */
  return (int)col1->stuff.colony_counts[nation_id] <=
         (int)col1->stuff.unit_type_counts[nation_id][12];
}

/*
 * Unit-project availability — DOS-LITERAL FUN_15eb_3650 kind-2 arm
 * (viceroy_unpacked.c raw 13714-13735):
 *
 *   avail = 1;
 *   if (idx < 0xd || idx > 0x12) {             // not a ship row
 *     if (idx != 0xc) {                        // not Wagon Train
 *       if (idx != 0xb) { gate = 0x24; goto check; }
 *       avail = has_building(3);               // Artillery <- Armory
 *     }
 *   } else { gate = 8; check: if (!has_building(gate)) avail = 0; }
 *
 * So every ship row (Caravel..Frigate) needs @BUILDING index 8 = Shipyard —
 * not Docks, not Drydock, and there is no population or coastal test of its
 * own (the Shipyard's own @BUILDING row carries the coastal requirement).
 *
 * The Artillery gate is has_building(3) = the Armory bit **literally**, not an
 * Armory/Magazine/Arsenal fold: FUN_15eb_035e tests one bit
 * (`bits[n>>3] & 1<<(n&7)` at colony +0x84), the completion writer
 * FUN_15eb_1030(idx, 1) only ORs the new bit in and never clears the tier
 * below, and FUN_15eb_3650's building arm refuses an upgrade whose
 * predecessor byte (DS:0x8f85 + idx*0xc) is not owned. So a normally-grown
 * Arsenal colony still has the Armory bit set and the two readings agree —
 * confirmed over 977 colonies in `original_saves` (armory mask is only ever
 * 0/1/3). They differ only where a lone upper bit is real, i.e. after a
 * pillage clears one tier (`docs/save_format_map.md` records real lone-upper
 * carpenters_shop / printing_press / church masks), and there DOS says no.
 *
 * Wagon Train's only gate is the per-nation cap (raw 13736-13740).
 */
static bool colonies_unit_project_available(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  int raw_code,
  const ColoniesBuildableOpts* opts
) {
  const int idx = units_build_code_to_index(raw_code);
  if (idx < 0 || !col) {
    return false;
  }
  if (idx >= COLONIZE_UNIT_INDEX_SHIP_FIRST && idx <= COLONIZE_UNIT_INDEX_SHIP_LAST) {
    return colonies_has_building_row(pool, col, COLONY_BUILDING_SHIPYARD);
  }
  if (idx == COLONIZE_UNIT_INDEX_ARTILLERY) {
    return colonies_has_building_row(pool, col, COLONY_BUILDING_ARMORY);
  }
  if (idx == COLONIZE_UNIT_INDEX_WAGON_TRAIN) {
    return !colonies_wagon_cap_reached(opts ? opts->col1 : NULL, col->nation_id);
  }
  return false;
}

bool colonies_set_construction(ColonizeColonyPool* pool, int colony_id, int building_type) {
  return colonies_set_construction_ex(pool, colony_id, building_type, NULL);
}

bool colonies_set_construction_ex(
  ColonizeColonyPool* pool,
  int colony_id,
  int building_type,
  const ColoniesBuildableOpts* opts
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (units_build_code_to_index(building_type) >= 0) {
    const char* uname = NULL;
    if (!col || !colonies_unit_project_available(pool, col, building_type, opts)) {
      return false;
    }
    colonies_unit_build_info(building_type, &uname, NULL, NULL);
    col->building_in_production = building_type;
    col->colony_flags =
      (uint8_t)(col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
    diag_info(
      "COLONY %s: building %s", col->name[0] ? col->name : "colony", uname ? uname : "unit"
    );
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
  /* DOS FUN_5952_0214 (~93714) clears +0x1c bit 0x80 whenever a project is
   * successfully assigned — the "finished, pick something new" latch is spent. */
  col->colony_flags =
    (uint8_t)(col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
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
  /* DOS FUN_5952_02f4 (~93754) clears the build-complete latch here too. */
  col->colony_flags =
    (uint8_t)(col->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
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
  if (bt && colonies_building_name_row(bt->name) == COLONY_BUILDING_TOWN_HALL) {
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
    if (colonies_building_name_row(bt->name) == COLONY_BUILDING_WAREHOUSE && col->warehouse_level < 1u) {
      col->warehouse_level = 1;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_WAREHOUSE_EXPANSION) {
      col->warehouse_level = 2;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_CAPITOL && col->capitol_level < 1u) {
      col->capitol_level = 1;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_CAPITOL_EXPANSION) {
      col->capitol_level = 2;
    } else if (colonies_building_name_row(bt->name) == COLONY_BUILDING_CUSTOM_HOUSE && col->custom_house_bits == 0) {
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
    /* A worked chain = every chain with a crew (see colonies_building_workable):
     * the crew of any lower tier follows the upgrade. */
    const int fam = colonies_building_row_chain(colonies_building_type_row(pool, bid));
    if (fam >= 0 && colonies_building_workable(pool, bid)) {
      for (int p = 0; p < col->colonist_count; ++p) {
        ColonizeColonist* c = &col->colonists[p];
        if (!c->active || c->building_type < 0 || c->building_type == bid ||
            c->building_type >= pool->building_type_count) {
          continue;
        }
        if (colonies_building_row_chain(colonies_building_type_row(pool, c->building_type)) == fam) {
          c->building_type = bid;
        }
      }
    }
  }
  /*
   * Col1 colony +0x1c bit 0x80 (COLONIZE_COLONY_FLAG_BUILD_COMPLETE):
   * FUN_364b_0114 ORs it in on every construction-completed arm — the
   * Warehouse-Expansion special case (build id 0x10, ~56925) and the generic
   * `FUN_281f_0bbe(item, 1)` grant right below it (~56935). It means "this
   * colony finished its project and has not been given a new one"; DOS clears
   * it again the moment a project is assigned (FUN_5952_0214 / 5952_02f4,
   * ~93714 / ~93754), and reads it as the colony-screen construction-pane
   * chrome (FUN_2f2b_2d1c ~49333, gated on pane DS:0x337 == 2). The port never
   * wrote it, so an export lost the state DOS would have carried (smell audit
   * #70). Set here; cleared in colonies_set_construction /
   * colonies_clear_construction, exactly like DOS's two clear sites.
   */
  col->colony_flags = (uint8_t)(col->colony_flags | COLONIZE_COLONY_FLAG_BUILD_COMPLETE);
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
    /* Player request: freshly built Artillery heads the colony's Units-
     * Present / Military row (same "front" slot as the Move-to-front order). */
    if (units_type_is_artillery(units_type(units, type_index))) {
      units->board_first_slot = (int)(u - units->units);
    }
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

bool colonies_has_building_named(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* name
) {
  if (!pool || !col || !name) {
    return false;
  }
  const int idx = colonies_find_building(pool, name);
  return idx >= 0 && idx < COLONIZE_BUILDING_TYPES_MAX && col->has_building[idx];
}

bool colonies_has_building_name_contains(
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const char* needle
) {
  if (!pool || !col || !needle) {
    return false;
  }
  for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    if (!col->has_building[i]) {
      continue;
    }
    const char* bn = pool->building_types[i].name;
    if (bn && strstr(bn, needle) != NULL) {
      return true;
    }
  }
  return false;
}

/*
 * ---------------------------------------------------------------------
 * The building upgrade chains (audit CO-13 / IN-24).
 *
 * DOS groups the 42 NAMES.TXT @BUILDING rows into 15 CATEGORIES (the static
 * setup table FUN_75c2_144c). Each category is one upgrade chain, low tier
 * first, and each owns exactly one colony-screen slot. This table has 17
 * entries: the 15 screen categories, plus Capitol and Stable broken out of
 * the Town Hall and Warehouse categories they share a slot with, because
 * the save format gives each of those its own bit group.
 *
 * ***THE ORDER OF THIS TABLE IS A SAVE-FORMAT CONTRACT.*** col1_bridge.c's
 * col1_apply_colony_buildings / col1_encode_colony_buildings walk the chains
 * in this order and map chain position i to bit i of the matching
 * ColonizeCol1Buildings group word. Reordering the table, or inserting a
 * tier in the middle of a chain, silently rewrites every colony's building
 * mask on the next export. Appending a whole new chain at the end is safe;
 * nothing else is.
 *
 * Two chains carry DOS quirks the bridge handles itself and that are
 * deliberately NOT folded in here:
 *   - Warehouse: "Warehouse Expansion" has no bit of its own; DOS counts it
 *     on the colony's warehouse_level byte (+0x95). The chain still lists
 *     it because the colony screen draws it as a tier.
 *   - Capitol / Capitol Expansion: the tail of the Town Hall category in
 *     DOS's own table, but unbuildable (see colonies_building_is_buildable)
 *     and tracked on capitol_level (+0x96), so they are a separate chain
 *     here rather than three Town Hall tiers.
 * The colony screen also draws the Stable in the warehouse slot; that is a
 * rendering rule (colony_screen_category_sprite), not a chain tier, so the
 * Stable keeps its own one-entry chain.
 * ---------------------------------------------------------------------
 */
#define R(x) COLONY_BUILDING_##x
static const int k_chain_rows[COLONIES_BUILDING_CHAIN_COUNT][4] = {
  {R(STOCKADE), R(FORT), R(FORTRESS), -1},                      /* FORTIFICATION */
  {R(ARMORY), R(MAGAZINE), R(ARSENAL), -1},                     /* ARMORY */
  {R(DOCKS), R(DRYDOCK), R(SHIPYARD), -1},                      /* DOCKS */
  {R(TOWN_HALL), -1, -1, -1},                                   /* TOWN_HALL */
  {R(SCHOOLHOUSE), R(COLLEGE), R(UNIVERSITY), -1},              /* SCHOOL */
  {R(WAREHOUSE), R(WAREHOUSE_EXPANSION), -1, -1},               /* WAREHOUSE */
  {R(CAPITOL), R(CAPITOL_EXPANSION), -1, -1},                   /* CAPITOL */
  {R(STABLE), -1, -1, -1},                                      /* STABLE */
  {R(CUSTOM_HOUSE), -1, -1, -1},                                /* CUSTOM_HOUSE */
  {R(PRINTING_PRESS), R(NEWSPAPER), -1, -1},                    /* PRESS */
  {R(WEAVERS_HOUSE), R(WEAVERS_SHOP), R(TEXTILE_MILL), -1},     /* WEAVER */
  {R(TOBACCONISTS_HOUSE), R(TOBACCONISTS_SHOP), R(CIGAR_FACTORY), -1}, /* TOBACCONIST */
  {R(RUM_DISTILLERS_HOUSE), R(RUM_DISTILLERY), R(RUM_FACTORY), -1},    /* RUM */
  {R(FUR_TRADERS_HOUSE), R(FUR_TRADING_POST), R(FUR_FACTORY), -1},     /* FUR */
  {R(CARPENTERS_SHOP), R(LUMBER_MILL), -1, -1},                 /* CARPENTER */
  {R(CHURCH), R(CATHEDRAL), -1, -1},                            /* CHURCH */
  {R(BLACKSMITHS_HOUSE), R(BLACKSMITHS_SHOP), R(IRON_WORKS), -1},      /* BLACKSMITH */
};
#undef R

const int* colonies_building_chain_rows(int chain) {
  if (chain < 0 || chain >= COLONIES_BUILDING_CHAIN_COUNT) {
    return NULL;
  }
  return k_chain_rows[chain];
}

/* Chain a @BUILDING row belongs to, or -1. */
int colonies_building_row_chain(int row) {
  if (row < 0) {
    return -1;
  }
  for (int c = 0; c < COLONIES_BUILDING_CHAIN_COUNT; ++c) {
    for (int t = 0; t < 4 && k_chain_rows[c][t] >= 0; ++t) {
      if (k_chain_rows[c][t] == row) {
        return c;
      }
    }
  }
  return -1;
}

/* The catalog's own spelling of @BUILDING `row` ("" when none is known). */
const char* colonies_building_row_name(int row) {
  if (row >= 0 && row < g_building_row_name_count && g_building_row_names[row][0]) {
    return g_building_row_names[row];
  }
  if (g_building_row_name_resolver) {
    const char* n = g_building_row_name_resolver(row);
    if (n) {
      return n;
    }
  }
  return "";
}

/*
 * Name view of a chain, for the callers that still walk names: built from
 * the row table and the names the catalog supplied — the port carries no
 * building names of its own.
 */
const char* const* colonies_building_chain(int chain) {
  static const char* names[COLONIES_BUILDING_CHAIN_COUNT][4];
  const int* rows = colonies_building_chain_rows(chain);
  if (!rows) {
    return NULL;
  }
  for (int i = 0; i < 4; ++i) {
    names[chain][i] = rows[i] >= 0 ? colonies_building_row_name(rows[i]) : NULL;
  }
  return names[chain];
}

int colonies_building_chain_length(int chain) {
  const char* const* c = colonies_building_chain(chain);
  int n = 0;
  while (c && c[n]) {
    ++n;
  }
  return n;
}

/*
 * Standard tier gate for a chain position: the tier below it is owned (or
 * this is the first tier) and no tier from this one up is. Every chain arm
 * of colonies_building_is_buildable below is this rule plus at most a
 * Founding-Father / coastal side condition.
 */
static bool colonies_chain_tier_open(
  const ColonizeColonyPool* pool, const ColonizeColony* col, int chain, int tier
) {
  const char* const* names = colonies_building_chain(chain);
  if (!names || tier < 0 || !names[tier]) {
    return false;
  }
  if (tier > 0 && !colonies_has_building_named(pool, col, names[tier - 1])) {
    return false;
  }
  for (int i = tier; names[i]; ++i) {
    if (colonies_has_building_named(pool, col, names[i])) {
      return false;
    }
  }
  return true;
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
  if (colonies_building_name_row(n) == COLONY_BUILDING_TOWN_HALL) {
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
  if (colonies_building_name_row(n) == COLONY_BUILDING_CAPITOL || colonies_building_name_row(n) == COLONY_BUILDING_CAPITOL_EXPANSION) {
    return false;
  }

  /*
   * Chain tiers (audit CO-13): "the tier below is owned and nothing from
   * this tier up is" is colonies_chain_tier_open; the only per-row extras
   * are Adam Smith on every factory tier and coastal on the port chain.
   * Spelled out arm-by-arm below this block are the rows that are NOT that
   * rule (the Warehouse and the six starter houses, which DOS lets you
   * build regardless of what sits above them in the chain).
   */
  if (colonies_building_name_row(n) == COLONY_BUILDING_STOCKADE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FORTIFICATION, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_FORT) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FORTIFICATION, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_FORTRESS) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FORTIFICATION, 2);
  }

  /* Military production chain. */
  if (colonies_building_name_row(n) == COLONY_BUILDING_ARMORY) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_ARMORY, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_MAGAZINE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_ARMORY, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_ARSENAL) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_ARMORY, 2);
  }

  /* Port chain (coastal only). */
  if (colonies_building_name_row(n) == COLONY_BUILDING_DOCKS) {
    return coastal && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_DOCKS, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_DRYDOCK) {
    return coastal && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_DOCKS, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_SHIPYARD) {
    return coastal && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_DOCKS, 2);
  }

  /* Education chain. */
  if (colonies_building_name_row(n) == COLONY_BUILDING_SCHOOLHOUSE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_SCHOOL, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_COLLEGE) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_SCHOOL, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_UNIVERSITY) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_SCHOOL, 2);
  }

  /* Warehouse tier 0 is deliberately NOT colonies_chain_tier_open: DOS gates
   * it on the Warehouse alone and ignores the Expansion above it (an
   * Expansion without its Warehouse is unreachable anyway — col1_bridge
   * derives tier 1 from warehouse_level, which implies tier 0). */
  if (colonies_building_name_row(n) == COLONY_BUILDING_WAREHOUSE) {
    return !colonies_has_building_row(pool, col, COLONY_BUILDING_WAREHOUSE);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_WAREHOUSE_EXPANSION) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_WAREHOUSE, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_CUSTOM_HOUSE) {
    return stuy && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CUSTOM_HOUSE, 0);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_PRINTING_PRESS) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_PRESS, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_NEWSPAPER) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_PRESS, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_WEAVERS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_WEAVER, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_TEXTILE_MILL) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_WEAVER, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_TOBACCONISTS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_TOBACCONIST, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_CIGAR_FACTORY) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_TOBACCONIST, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_RUM_DISTILLERY) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_RUM, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_RUM_FACTORY) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_RUM, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_FUR_TRADING_POST) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FUR, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_FUR_FACTORY) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_FUR, 2);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_CARPENTERS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CARPENTER, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_LUMBER_MILL) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CARPENTER, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_CHURCH) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CHURCH, 0);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_CATHEDRAL) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_CHURCH, 1);
  }

  if (colonies_building_name_row(n) == COLONY_BUILDING_BLACKSMITHS_SHOP) {
    return colonies_chain_tier_open(pool, col, COLONIES_CHAIN_BLACKSMITH, 1);
  }
  if (colonies_building_name_row(n) == COLONY_BUILDING_IRON_WORKS) {
    return adam && colonies_chain_tier_open(pool, col, COLONIES_CHAIN_BLACKSMITH, 2);
  }

  /* Starter houses and other leaf buildings: available if not already owned. */
  if (colonies_building_name_row(n) == COLONY_BUILDING_WEAVERS_HOUSE || colonies_building_name_row(n) == COLONY_BUILDING_TOBACCONISTS_HOUSE ||
      colonies_building_name_row(n) == COLONY_BUILDING_RUM_DISTILLERS_HOUSE || colonies_building_name_row(n) == COLONY_BUILDING_FUR_TRADERS_HOUSE ||
      colonies_building_name_row(n) == COLONY_BUILDING_BLACKSMITHS_HOUSE || colonies_building_name_row(n) == COLONY_BUILDING_STABLE) {
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
  /*
   * The seven unit projects, in DOS's own code order — FUN_15eb_38e8
   * (viceroy_unpacked.c raw 13773) walks codes -1..0x30 and keeps whatever
   * FUN_15eb_3650 calls available, so Artillery, Wagon Train and the five
   * buildable ships follow the @BUILDING rows in that order. None of them is
   * a real @BUILDING row, so none is ever "owned": no has_building[] dedup,
   * and the colony can queue another one the moment the last one spawns.
   */
  for (int code = COLONIZE_UNIT_BUILD_CODE_FIRST;
       code < COLONIZE_UNIT_BUILD_CODE_FIRST + COLONIZE_UNIT_BUILD_CODE_COUNT && n < out_max;
       ++code) {
    if (colonies_unit_project_available(pool, col, code, opts)) {
      out_ids[n++] = code;
    }
  }
  return n;
}

int colonies_warehouse_capacity(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type
) {
  /* Audit CO-28: DOS takes no cargo argument (see below) and neither does
   * this body; the parameter stays only to keep the ~10 call sites legible. */
  (void)cargo_type;
  if (!colony) {
    return 0;
  }
  /*
   * FUN_15eb_0a50 (viceroy_unpacked.c 10043-10053): 100*(1+warehouse_level);
   * buildings raise the level. The DOS routine takes NO cargo argument — one
   * capacity for all sixteen goods, Food included.
   *
   * Smell audit #25: this used to answer 199 for Food, uncited and blind to the
   * warehouse level. DOS's Food exemption is not a *different* cap, it is the
   * absence of the three per-cargo rules, each of which skips index 0 outright:
   *   - EOT overflow/spoilage: FUN_364b_0688's `if (local_b6 != 0)` guard and
   *     the paired `if (local_b6 == 0) aiStack_e4[0] = 0` (viceroy 57806-57872,
   *     57332-57336) — food is never clamped to capacity, which is what lets a
   *     colony bank past 200 toward the new-colonist threshold;
   *   - the unload @WAREHOUSEFULL confirm: FUN_479b_0f60's
   *     `(cap < stock + amount) && (local_18 != 0)` (viceroy 77396-77404);
   *   - the colony-screen alert colour: FUN_2f2b_28d6's `if (local_80 != 0)`
   *     (viceroy 49118-49123).
   * Those three sites carry the exemption; this accessor must not.
   */
  int level = (int)colony->warehouse_level;
  if (pool) {
    int derived = 0;
    const int wh = colonies_building_row(pool, COLONY_BUILDING_WAREHOUSE);
    const int whe = colonies_building_row(pool, COLONY_BUILDING_WAREHOUSE_EXPANSION);
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
  int deposited,
  int already_included,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  /* FUN_479b_0f60 (viceroy_unpacked.c 77396): the @WAREHOUSEFULL confirm is
   * gated `(cap < stock + amount) && (local_18 != 0)` — Food never warns,
   * because food over capacity is the new-colonist rule, not spoilage. */
  if (cargo_type == COLONIZE_CARGO_FOOD) {
    return;
  }
  const int cap = colonies_warehouse_capacity(pool, colony, cargo_type);
  /* bugs.md #433: NUMBER0 is the pre-deposit stock; back out whatever part of
   * this deposit has already landed in stock[]. */
  int stock = colony->stock[cargo_type] - (already_included > 0 ? already_included : 0);
  if (stock < 0) {
    stock = 0;
  }
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
  tok.number2 = deposited > 0 ? deposited : 0;
  tok.has_number2 = true;
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
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  popup_msg_fill(messages, "FULL", &tok, "", body, sizeof(body));
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
  const bool warehouse_exp = (colonies_building_name_row(bname) == COLONY_BUILDING_WAREHOUSE_EXPANSION);
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
    "", body, sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_specialty_cargo_update(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  int cargo_type,
  int want_set,
  int already_produced
) {
  if (!colony || !colony->active || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  const int cap = colonies_warehouse_capacity(pool, colony, cargo_type);
  /* FUN_5952_0306: stock >= warehouse cap → do not set / clear match. */
  if (cap > 0 && cap <= colony->stock[cargo_type]) {
    want_set = 0;
  }
  /* FUN_5952_0306's second clear, raw: `if (*(int *)(cargo * 2 + -0x7238) !=
   * 0) want = 0` — DS:0x8dc8, the tick's GROSS PRODUCTION scratch ledger. A
   * colony that already makes the cargo never asks to be sent it. This
   * parameter used to be an invented `boycotted`, which no DOS reader of
   * +0x8d has; corrected 2026-09-18 with the real callee. */
  if (already_produced) {
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
     * Overflow caused purely by production is a silent clamp to capacity with
     * no @SPOIL message. A reported loss below 2 tons is also dropped
     * (`if (local_74 < 2) local_74 = 0`).
     *
     * DOS-LITERAL FUN_364b_0688 raw 57847-57868. The DOS branch test is
     * `aiStack_e4[c] < overflow`, i.e. (stock_now - before) < (stock_now -
     * cap), i.e. `before > cap`. In that branch DOS first backs this turn's
     * production off the stock (`stock -= aiStack_e4[c]`, leaving `before`)
     * and only then subtracts local_74 — so when local_74 is zeroed by the
     * `< 2` rule the stock is left at `before`, NOT at cap. A single ton of
     * pre-existing overflow therefore sits in the warehouse forever: DOS
     * discards only above capacity, and only 2 tons or more.
     */
    const int before = stock_before ? stock_before[c] : colony->stock[c];
    if (before <= cap) {
      /* Overflow is entirely this turn's production: silent clamp. */
      colony->stock[c] = cap;
      continue;
    }
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
    colony->stock[c] = before - lost;
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
  /* FUN_479b_0f60 (viceroy 77396) gates the full-warehouse confirm on
   * `local_18 != 0` — cargo 0 (Food) never raises it. */
  if (out_warehouse_full && ctype != COLONIZE_CARGO_FOOD && cap > 0 &&
      col->stock[ctype] > cap) {
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
  /* FUN_479b_0f60 (viceroy 77396) gates the full-warehouse confirm on
   * `local_18 != 0` — cargo 0 (Food) never raises it. */
  if (out_warehouse_full && ctype != COLONIZE_CARGO_FOOD && cap > 0 &&
      col->stock[ctype] > cap) {
    *out_warehouse_full = true;
  }
  return amount;
}

/* ===== Foreign-colony trade (FUN_5f7a_020e) — docs/foreign_colony_trade.md ===== */

/*
 * DS:0x84bc[nation*0x10 + cargo]. Every writer of that byte stores
 * `nation[n].trade.euro_price[cargo] - 1` clamped at 0 (viceroy_unpacked.c
 * 6316-6320 / 51962-51966 / 58996-59000) — the same derivation europe.c's
 * dump-sell arm cites.
 */
static int colonies_ftrade_price_byte(const ColonizeCol1Save* col1, int nation, int cargo) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT || cargo < 0 ||
      cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  const int p = (int)col1->nation[nation].trade.euro_price[cargo] - 1;
  return p > 0 ? p : 0;
}

/* The two units the trade dialog needs, plus the DOS nation ids. */
static int colonies_ftrade_bind(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id,
  const ColonizeColony** out_col,
  const ColonizeUnit** out_unit
) {
  if (!w || !w->colonies || !w->units || !w->col1_ok || !w->col1) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(w->colonies, foreign_colony_id);
  const ColonizeUnit* u = units_get_const(w->units, unit_id);
  if (!col || !col->active || !u || !u->active) {
    return 0;
  }
  if (col->nation_id < 0 || col->nation_id > 3) {
    return 0;
  }
  const int actor = u->nation_id;
  /* raw 98915-98921: nibble > 3 (natives) leaves without a word. */
  if (actor < 0 || actor > 3 || actor == col->nation_id) {
    return 0;
  }
  *out_col = col;
  *out_unit = u;
  return 1;
}

ColonizeForeignTradeGate colonies_foreign_trade_gate(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id
) {
  const ColonizeColony* col = NULL;
  const ColonizeUnit* u = NULL;
  if (!colonies_ftrade_bind(w, foreign_colony_id, unit_id, &col, &u)) {
    return COLONIZE_FTRADE_NONE;
  }
  const ColonizeCol1Save* col1 = w->col1;
  const int actor = u->nation_id;
  /*
   * raw 98921-98923: `player[actor].control != 0` returns before any dialog.
   * The AI never trades with a foreign colony in DOS — FF 4 is read in exactly
   * two places in VICEROY, here and the Foreign Affairs report.
   */
  if (col1->player[actor].control != 0) {
    return COLONIZE_FTRADE_NONE;
  }
  /* raw 98924-98927: FUN_281f_0a38(actor, owner) & 0x40 = peace treaty. */
  if ((ai_diplo_read(col1, actor, col->nation_id) & AI_DIPLO_PEACE) == 0) {
    return COLONIZE_FTRADE_ATWAR;
  }
  /* raw 98928-98934: FUN_281f_07b4(actor, 4) = Jan de Witt. */
  if (!founding_fathers_de_witt_allows_foreign_colony_trade(col1, actor)) {
    return COLONIZE_FTRADE_MERCANTILISM;
  }
  /* raw 98935-98936: unit +0x3150 (goods holds occupied). */
  if (units_holds_used(w->units, unit_id) <= 0) {
    return COLONIZE_FTRADE_NOCARGO;
  }
  return COLONIZE_FTRADE_OK;
}

int colonies_foreign_trade_prepare(
  const ColonizeWorld* w,
  ColonizeDosRng* rng,
  int foreign_colony_id,
  int unit_id,
  int hold_index,
  ColonizeForeignTradeDeal* out
) {
  if (!out) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  out->offer_cargo = -1;
  if (colonies_foreign_trade_gate(w, foreign_colony_id, unit_id) != COLONIZE_FTRADE_OK) {
    return 0;
  }
  const ColonizeColony* col = NULL;
  const ColonizeUnit* u = NULL;
  if (!colonies_ftrade_bind(w, foreign_colony_id, unit_id, &col, &u)) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const ColonizeCol1Save* col1 = w->col1;
  const int actor = u->nation_id;
  const int owner = col->nation_id;
  const int sold = u->hold_goods_type[hold_index];
  const int qty = units_hold_amount(w->units, unit_id, hold_index);
  if (qty <= 0 || sold < 0 || sold >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  const bool woi = col1->head.game_options.woi;
  const int ally = (int)col1->head.rival_nation_slot_1; /* DS:0x53d4 */
  const int difficulty = (int)col1->head.difficulty;   /* DS:0x53a6 */

  /* raw 98963-98968: gross, then the owner's tax withheld. */
  const int gross = colonies_ftrade_price_byte(col1, owner, sold) * qty;
  int gold = europe_net_after_tax(gross, (int)col1->nation[owner].tax_rate);
  /* raw 98969-98972: at war (rel & 2) outside the WoI — half. */
  if (!woi && (ai_diplo_read(col1, actor, owner) & AI_DIPLO_WAR) != 0) {
    gold >>= 1;
  }
  /* raw 98973-98983: haggle. The WoI intervention ally shaves only 5+d %. */
  if (!woi || owner != ally) {
    const int pct = rng ? dos_rng_range(rng, 10, (difficulty + 1) * 12) : 10;
    gold += (pct * gold) / -100;
  } else {
    gold += ((-5 - difficulty) * gold) / 100;
  }
  if (gold < 1) {
    gold = 1; /* raw 98984-98986 */
  }

  /* raw 98987-99012: best counter-offer out of the colony's warehouse. */
  int best_val = -1;
  int best_cargo = -1;
  int best_qty = 0;
  for (int c = 0; c < 0x10 && c < COLONIZE_CARGO_COUNT; ++c) {
    if (!woi && (c == 0x0f || c == 0x0e || c == 0 || c == 5)) {
      continue; /* Muskets / Tools / Food / Lumber are off the table at peace */
    }
    if (c == sold) {
      continue;
    }
    int avail = (int)col->stock[c];
    if (avail > 100) {
      avail = 100;
    }
    if (woi && (sold == 0x0f || sold == 8)) {
      if (owner == ally) {
        avail = 100;
      } else if (avail < 0x32) {
        avail = 0x32;
      }
    }
    const int unit_price = colonies_ftrade_price_byte(col1, owner, c);
    int val = unit_price * avail;
    while (val > gross) {
      avail--;
      val -= unit_price;
    }
    if (avail > 0 && val > best_val) {
      best_val = val;
      best_qty = avail;
      best_cargo = c;
    }
  }

  out->sold_cargo = sold;
  out->sold_qty = qty;
  out->gold = gold;
  out->offer_cargo = best_val >= 0 ? best_cargo : -1;
  out->offer_qty = best_val >= 0 ? best_qty : 0;
  return 1;
}

int colonies_foreign_trade_apply(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id,
  int hold_index,
  const ColonizeForeignTradeDeal* deal,
  int take_goods
) {
  if (!deal || !w || !w->colonies || !w->units || !w->col1_ok || !w->col1) {
    return 0;
  }
  ColonizeColony* col = colonies_get_mut(w->colonies, foreign_colony_id);
  ColonizeUnit* u = units_get(w->units, unit_id);
  if (!col || !col->active || !u || !u->active) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  if (deal->sold_cargo < 0 || deal->sold_cargo >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  if (take_goods) {
    if (deal->offer_cargo < 0 || deal->offer_cargo >= COLONIZE_CARGO_COUNT) {
      return 0;
    }
    /* raw 99019-99021: FUN_281f_0cea/0ca4 overwrite the hold in place. */
    u->hold_goods_type[hold_index] = deal->offer_cargo;
    u->hold_goods_amount[hold_index] = deal->offer_qty;
  } else {
    /* raw 99022-99030: FUN_281f_0aec empties the hold, then gold to the purse. */
    (void)units_unload_goods_hold(w->units, unit_id, hold_index, NULL, NULL);
    europe_nation_gold_add(NULL, w->col1, u->nation_id, (long)deal->gold);
  }
  /*
   * raw 99031-99033: the colony gains what it bought. DOS does NOT debit the
   * stock of the cargo it hands over in the barter arm — transcribed, not fixed.
   */
  col->stock[deal->sold_cargo] += deal->sold_qty;
  col->cargo_idle_turns = 0;
  return 1;
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
