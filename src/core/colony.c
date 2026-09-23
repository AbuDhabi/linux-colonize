#include "core/colony.h"

/*
 * Sections:
 *  - Map icons, fortification tier & colony lookup/fog queries (colonies_settlement_icon .. colonies_reveal_founded_w)
 *  - Pool init & building/name catalog loading (colonies_init .. colonies_building_type)
 *  - Founding site checks & Indian land purchase/claim (colonies_can_found .. colonies_found_with_indian_land_w)
 *  - Occupancy map, colony founding & col1 tile bookkeeping (colonies_set_occupancy_map .. colonies_found)
 */

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

#include "core/colony_internal.h"

/* ICONS.SS #0–3: European colonies by fortification (none / stockade / fort / fortress). */
#define COLONY_MAP_ICON_STOCKADE 0
#define COLONY_MAP_ICON_FORT 1
#define COLONY_MAP_ICON_FORTRESS 2
#define COLONY_MAP_ICON_NONE 3

/* ===================== Map icons, fortification tier & colony lookup/fog queries (colonies_settlement_icon .. colonies_reveal_founded_w) ===================== */
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
char colony_building_row_names[COLONIZE_BUILDING_TYPES_MAX][40];
int colony_building_row_name_count = 0;

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


/* ===================== Pool init & building/name catalog loading (colonies_init .. colonies_building_type) ===================== */
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
      colony_building_row_names[pool->building_type_count - 1],
      sizeof(colony_building_row_names[0]),
      t->name
    );
    colony_building_row_name_count = pool->building_type_count;
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
ColoniesBuildingRowNameResolver colony_building_row_name_resolver = NULL;

void colonies_set_building_row_name_resolver(ColoniesBuildingRowNameResolver fn) {
  colony_building_row_name_resolver = fn;
}

void colonies_set_building_name_row_resolver(ColoniesBuildingNameRowResolver fn) {
  g_building_name_row_resolver = fn;
}

int colonies_building_name_row(const char* name) {
  if (!name || !name[0]) {
    return -1;
  }
  for (int i = 0; i < colony_building_row_name_count; ++i) {
    if (strcmp(colony_building_row_names[i], name) == 0) {
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

/* ===================== Founding site checks & Indian land purchase/claim (colonies_can_found .. colonies_found_with_indian_land_w) ===================== */
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
  /*
   * DOS-LITERAL FUN_4cc6_07c2 raw 81226-81228 (bugs.md #735): human only,
   *   uVar2 = FUN_281f_030c(tribe, DS:0x5394 nation_turn);   // alarm word
   *   iVar1 = FUN_281f_0a60(uVar2);                          // quartile 0..3
   *   local_4 = (iVar1 + 1) * local_4;
   * i.e. the tribe's alarm quartile toward the buying nation scales the price
   * x1..x4. nation_turn == nation_id on the human's own turn.
   */
  if (is_human) {
    const int alarm = ai_diplo_indian_alarm(col1, (int)tribe->nation_id, nation_id);
    cost = (ai_relation_quartile(alarm) + 1) * cost;
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
ColonizeWorldMap* g_colonies_occupancy_map = NULL;

/* ===================== Occupancy map, colony founding & col1 tile bookkeeping (colonies_set_occupancy_map .. colonies_found) ===================== */
void colonies_set_occupancy_map(ColonizeWorldMap* map) {
  g_colonies_occupancy_map = map;
}

void colonies_mark_settlement_tile(int x, int y, bool on) {
  if (!g_colonies_occupancy_map) {
    return;
  }
  map_occupancy_set_layer2(g_colonies_occupancy_map, x, y, MAP_OCCUPANCY_HAS_CITY, on);
}

ColonizeCol1Save* g_colonies_col1 = NULL;

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
void colonies_col1_rebel_divisor_adjust(ColonizeCol1Save* col1, int x, int y, int delta) {
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

