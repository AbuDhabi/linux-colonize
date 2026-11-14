/* MOD_COLONY_GOVERNOR (src/core/colony_governor.c, docs/colony_governor.md). */
#include "test_turn_common.h"
#include "core/colony_governor.h"
#include "core/mods.h"
#include "core/runtime_ext.h"

static int g_diag_ready;

typedef struct GovFx {
  ColonizeWorldMap map;
  ColonizeColonyPool pool;
  ColonizeMsgCatalog names;
  ColonizeWorld w;
  ColonizeColony* col;
} GovFx;

/* A 4-colonist nation-0 colony on AMER2 with a food plot, everyone in the
 * Town Hall: no food but the town commons, so it starves without a fix. */
static bool gov_fx_open(GovFx* f) {
  if (!g_diag_ready) {
    diag_init(0, NULL);
    atexit(diag_shutdown);
    g_diag_ready = 1;
  }
  units_set_occupancy_map(NULL);
  colonies_set_occupancy_map(NULL);
  colonies_set_col1_context(NULL);
  memset(f, 0, sizeof(*f));
  char err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &f->map, err, sizeof(err))) {
    fprintf(stderr, "map: %s\n", err);
    return false;
  }
  colonies_init(&f->pool);
  assets_msg_init(&f->names);
  if (!assets_msg_load_file(&f->names, "COLONIZE/NAMES.TXT") ||
      !colonies_load_buildings(&f->pool, &f->names) ||
      !colonies_load_names(&f->pool, "COLONIZE/COLONY.TXT")) {
    return false;
  }
  for (int y = 2; y < (int)f->map.height - 2 && !f->col; ++y) {
    for (int x = 2; x < (int)f->map.width - 2 && !f->col; ++x) {
      if (!map_tile_is_land(&f->map, x, y) || !colonies_can_found(&f->pool, &f->map, x, y) ||
          colony_yield_for_tile(&f->map, x + 1, y, COLONIZE_JOB_FARMER) < 4) {
        continue;
      }
      const int cid = colonies_found(&f->pool, &f->map, x, y, 0, 0, UNITS_JOB_NONE, 0, 0, 0);
      f->col = colonies_get_mut(&f->pool, cid);
    }
  }
  if (!f->col) {
    return false;
  }
  map_reveal_radius(&f->map, f->col->x, f->col->y, 0, 2);
  f->w = (ColonizeWorld){.colonies = &f->pool, .map = &f->map, .human_nation = 0};
  ColonizeColony* c = f->col;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    c->tiles[t] = -1;
  }
  c->colonist_count = 4;
  c->population = 4;
  const int hall = colonies_building_row(&f->pool, COLONY_BUILDING_TOWN_HALL);
  for (int i = 0; i < 4; ++i) {
    c->colonists[i] = c->colonists[0];
    c->colonists[i].field_job = -1;
    c->colonists[i].building_type = i < 3 ? hall : -1;
  }
  c->colonists[3].building_type = colonies_building_row(&f->pool, COLONY_BUILDING_CARPENTERS_SHOP);
  c->stock[COLONIZE_CARGO_LUMBER] = 50;
  return hall >= 0;
}

static void gov_fx_close(GovFx* f) {
  assets_msg_free(&f->names);
  map_free(&f->map);
}

static ColonizeColonyPreview gov_preview(GovFx* f) {
  ColonizeColonyPreview pv;
  colony_preview_compute_w(&f->w, f->col, &pv);
  return pv;
}

/* Every governor's first job: no food deficit when the plots can feed them. */
static int case_feeds_colonists(void) {
  for (int gov = COLONY_GOVERNOR_FARMING; gov < COLONY_GOVERNOR_COUNT; ++gov) {
    GovFx f;
    if (!gov_fx_open(&f)) {
      fprintf(stderr, "fixture\n");
      return 1;
    }
    if (gov_preview(&f).food_net >= 0) {
      fprintf(stderr, "fixture is not starving\n");
      return 1;
    }
    f.col->governor = (uint8_t)gov;
    colony_governor_run_w(&f.w, f.col->id);
    const ColonizeColonyPreview pv = gov_preview(&f);
    int seated = 0;
    for (int i = 0; i < f.col->colonist_count; ++i) {
      seated += f.col->colonists[i].field_job >= 0 || f.col->colonists[i].building_type >= 0;
    }
    gov_fx_close(&f);
    if (pv.food_net < 0 || seated != 4) {
      fprintf(stderr, "governor %d: food_net %d seated %d\n", gov, pv.food_net, seated);
      return 1;
    }
  }
  return 0;
}

/* Politics keeps more bells than Farming; Farming grows more food. */
static int case_priorities_differ(void) {
  ColonizeColonyPreview pv[COLONY_GOVERNOR_COUNT];
  for (int gov = COLONY_GOVERNOR_FARMING; gov < COLONY_GOVERNOR_COUNT; ++gov) {
    GovFx f;
    if (!gov_fx_open(&f)) {
      return 1;
    }
    f.col->governor = (uint8_t)gov;
    colony_governor_run_w(&f.w, f.col->id);
    pv[gov] = gov_preview(&f);
    gov_fx_close(&f);
  }
  if (pv[COLONY_GOVERNOR_POLITICS].bells <= pv[COLONY_GOVERNOR_FARMING].bells ||
      pv[COLONY_GOVERNOR_FARMING].food_net <= pv[COLONY_GOVERNOR_POLITICS].food_net) {
    fprintf(stderr, "politics bells %d food %d, farming bells %d food %d\n",
            pv[COLONY_GOVERNOR_POLITICS].bells, pv[COLONY_GOVERNOR_POLITICS].food_net,
            pv[COLONY_GOVERNOR_FARMING].bells, pv[COLONY_GOVERNOR_FARMING].food_net);
    return 1;
  }
  return 0;
}

/* The mod gate; a buildable project is the player's to keep, "(No
 * Production)" is the governor's to fill. */
static int case_project_only_when_unbuildable(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  int rc = 0;
  ColonizeColony* c = f.col;
  c->governor = COLONY_GOVERNOR_FARMING;
  const int project = c->building_in_production;
  mods_campaign_set(0);
  colony_governor_run_nation_w(&f.w, 0);
  if (c->colonists[0].building_type < 0) {
    fprintf(stderr, "ran with the mod off\n");
    rc = 1;
  }
  mods_campaign_set(1u << MOD_COLONY_GOVERNOR);
  colony_governor_run_nation_w(&f.w, 0);
  if (c->building_in_production != project) {
    fprintf(stderr, "replaced a buildable project %d with %d\n", project, c->building_in_production);
    rc = 1;
  }
  c->building_in_production = -1;
  colony_governor_run_nation_w(&f.w, 0);
  if (c->building_in_production < 0) {
    fprintf(stderr, "left (No Production)\n");
    rc = 1;
  }
  /* A finished building left in the slot: the governor picks a new one. */
  const int hall = colonies_building_row(&f.pool, COLONY_BUILDING_TOWN_HALL);
  c->building_in_production = hall;
  colony_governor_run_nation_w(&f.w, 0);
  if (c->building_in_production == hall || c->building_in_production < 0) {
    fprintf(stderr, "no new project: %d\n", c->building_in_production);
    rc = 1;
  }
  mods_campaign_set(0);
  gov_fx_close(&f);
  return rc;
}

/* Every good but food full: nobody works a plot for anything but food, and
 * the project is the Warehouse. */
static int case_warehouse_room(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  int rc = 0;
  ColonizeColony* c = f.col;
  for (int k = 1; k < COLONIZE_CARGO_COUNT; ++k) {
    c->stock[k] = colonies_warehouse_capacity(&f.pool, c, k);
  }
  c->governor = COLONY_GOVERNOR_CASH;
  c->building_in_production = -1;
  colony_governor_run_w(&f.w, c->id);
  for (int i = 0; i < c->colonist_count; ++i) {
    const int job = c->colonists[i].field_job;
    if (job >= 0 && job != COLONIZE_JOB_FARMER && job != COLONIZE_JOB_FISHERMAN) {
      fprintf(stderr, "colonist %d works field job %d into a full warehouse\n", i, job);
      rc = 1;
    }
  }
  if (c->building_in_production != colonies_building_row(&f.pool, COLONY_BUILDING_WAREHOUSE)) {
    fprintf(stderr, "project %d, not the Warehouse\n", c->building_in_production);
    rc = 1;
  }
  gov_fx_close(&f);
  return rc;
}

/* RTST record 18 carries the governor by tile; a save without it clears. */
static int case_rtst_round_trip(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  f.col->governor = COLONY_GOVERNOR_CASH;
  size_t n = 0;
  uint8_t* p = runtime_ext_serialize(NULL, &f.pool, &n);
  f.col->governor = 0;
  runtime_ext_deserialize(NULL, &f.pool, &f.map, p, n);
  const int back = f.col->governor;
  free(p);
  runtime_ext_deserialize(NULL, &f.pool, &f.map, NULL, 0);
  const int cleared = f.col->governor;
  gov_fx_close(&f);
  if (back != COLONY_GOVERNOR_CASH || cleared != 0) {
    fprintf(stderr, "round trip %d, cleared %d\n", back, cleared);
    return 1;
  }
  return 0;
}

/* A capture hands the colony over with no governor. */
static int case_capture_clears(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  f.col->governor = COLONY_GOVERNOR_INDUSTRY;
  const bool ok = colonies_capture_show_ex(&f.pool, f.col->id, 1, NULL, false);
  const int gov = f.col->governor;
  gov_fx_close(&f);
  if (!ok || gov != COLONY_GOVERNOR_NONE) {
    fprintf(stderr, "capture %d, governor %d\n", ok, gov);
    return 1;
  }
  return 0;
}

static bool gov_has_carpenter(const GovFx* f) {
  const int shop = colonies_building_row(&f->pool, COLONY_BUILDING_CARPENTERS_SHOP);
  for (int i = 0; i < f->col->colonist_count; ++i) {
    if (f->col->colonists[i].building_type == shop) {
      return true;
    }
  }
  return false;
}

/* Hammers are worth working for only while the project still lacks them. */
static int case_hammers_only_toward_project(void) {
  int rc = 0;
  for (int banked = 0; banked < 2; ++banked) {
    GovFx f;
    if (!gov_fx_open(&f)) {
      return 1;
    }
    ColonizeColony* c = f.col;
    c->stock[COLONIZE_CARGO_FOOD] = 100;
    int hammers = 0;
    if (!colonies_construction_cost(&f.pool, c, &hammers, NULL)) {
      fprintf(stderr, "fixture has no project\n");
      gov_fx_close(&f);
      return 1;
    }
    c->hammers = banked ? hammers : 0;
    c->governor = COLONY_GOVERNOR_INDUSTRY;
    colony_governor_run_w(&f.w, c->id);
    if (gov_has_carpenter(&f) == (banked != 0)) {
      fprintf(stderr, "banked %d: carpenter %d\n", banked, gov_has_carpenter(&f));
      rc = 1;
    }
    gov_fx_close(&f);
  }
  return rc;
}

/* A food stock lets Politics run a deficit nobody starves of... */
static int case_temporary_food_shortfall(void) {
  ColonizeColonyPreview pv[2];
  for (int stocked = 0; stocked < 2; ++stocked) {
    GovFx f;
    if (!gov_fx_open(&f)) {
      return 1;
    }
    f.col->stock[COLONIZE_CARGO_FOOD] = stocked ? 100 : 0;
    f.col->governor = COLONY_GOVERNOR_POLITICS;
    colony_governor_run_w(&f.w, f.col->id);
    pv[stocked] = gov_preview(&f);
    gov_fx_close(&f);
  }
  /* ...but never one low enough for @FOODLOW (stock under 4 turns of it). */
  if (pv[1].bells <= pv[0].bells || pv[1].food_net >= 0 || 100 + 7 * pv[1].food_net < 0) {
    fprintf(stderr, "bells %d -> %d, food_net %d -> %d\n", pv[0].bells, pv[1].bells,
            pv[0].food_net, pv[1].food_net);
    return 1;
  }
  return 0;
}

/* A teacher keeps his class while there is a student; with none he works. */
static int case_teacher_sticky(void) {
  int rc = 0;
  for (int students = 1; students >= 0; --students) {
    GovFx f;
    if (!gov_fx_open(&f)) {
      return 1;
    }
    ColonizeColony* c = f.col;
    const int school = colonies_building_row(&f.pool, COLONY_BUILDING_SCHOOLHOUSE);
    c->has_building[school] = true;
    c->colonists[3].profession = COLONIZE_JOB_LUMBERJACK; /* Expert Lumberjack, tier 1 */
    c->colonists[3].building_type = school;
    for (int i = 0; i < 3 && !students; ++i) {
      c->colonists[i].profession = COLONIZE_JOB_FARMER; /* Expert Farmers */
    }
    c->governor = COLONY_GOVERNOR_FARMING;
    colony_governor_run_w(&f.w, c->id);
    if ((c->colonists[3].building_type == school) != (students != 0)) {
      fprintf(stderr, "students %d: teacher seat %d\n", students, c->colonists[3].building_type);
      rc = 1;
    }
    gov_fx_close(&f);
  }
  return rc;
}

/* Industry with ore and an Armory makes tools and muskets both. */
static int case_industry_tools_and_muskets(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  ColonizeColony* c = f.col;
  c->has_building[colonies_building_row(&f.pool, COLONY_BUILDING_ARMORY)] = true;
  c->stock[COLONIZE_CARGO_FOOD] = 100;
  c->stock[COLONIZE_CARGO_ORE] = 90;
  c->hammers = 1000; /* nothing left to build: no carpenter competes */
  c->governor = COLONY_GOVERNOR_INDUSTRY;
  colony_governor_run_w(&f.w, c->id);
  const ColonizeColonyPreview pv = gov_preview(&f);
  gov_fx_close(&f);
  if (pv.goods[COLONIZE_CARGO_TOOLS] <= 0 || pv.goods[COLONIZE_CARGO_MUSKETS] <= 0) {
    fprintf(stderr, "tools %d muskets %d\n", pv.goods[COLONIZE_CARGO_TOOLS],
            pv.goods[COLONIZE_CARGO_MUSKETS]);    return 1;
  }
  return 0;
}

/* A small stock: no deficit that would raise @FOODLOW on any planned turn. */
static int case_no_food_low_popup(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  f.col->stock[COLONIZE_CARGO_FOOD] = 12;
  f.col->governor = COLONY_GOVERNOR_POLITICS;
  colony_governor_run_w(&f.w, f.col->id);
  const int net = gov_preview(&f).food_net;
  gov_fx_close(&f);
  for (int turn = 1; turn <= 3 && net < 0; ++turn) {
    if (12 + turn * net < -4 * net) {
      fprintf(stderr, "food_net %d raises @FOODLOW on turn %d\n", net, turn);
      return 1;
    }
  }
  return 0;
}

/* Converts work the fields while there are plots; free colonists go inside. */
static int case_converts_outdoors(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  ColonizeColony* c = f.col;
  c->colonists[0].profession = COLONIZE_PROF_CONVERT;
  c->colonists[1].profession = COLONIZE_PROF_CONVERT;
  c->stock[COLONIZE_CARGO_FOOD] = 100;
  c->governor = COLONY_GOVERNOR_POLITICS;
  colony_governor_run_w(&f.w, c->id);
  const int b0 = c->colonists[0].building_type;
  const int b1 = c->colonists[1].building_type;
  gov_fx_close(&f);
  if (b0 >= 0 || b1 >= 0) {
    fprintf(stderr, "converts indoors: %d %d\n", b0, b1);
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"no_food_low_popup", case_no_food_low_popup},
  {"converts_outdoors", case_converts_outdoors},
  {"industry_tools_and_muskets", case_industry_tools_and_muskets},
  {"capture_clears", case_capture_clears},
  {"hammers_only_toward_project", case_hammers_only_toward_project},
  {"temporary_food_shortfall", case_temporary_food_shortfall},
  {"teacher_sticky", case_teacher_sticky},
  {"feeds_colonists", case_feeds_colonists},
  {"priorities_differ", case_priorities_differ},
  {"project_only_when_unbuildable", case_project_only_when_unbuildable},
  {"warehouse_room", case_warehouse_room},
  {"rtst_round_trip", case_rtst_round_trip},
};
TEST_MAIN(k_cases)
