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

/* AI seat (rolling project): a full TB-57 bank takes no more hammers, so no
 * carpenter stays seated (pt3 O3); an empty bank still wants one. */
static int case_ai_seat_bank_full(void) {
  int rc = 0;
  dos_buggy_behavior_set(false);
  for (int full = 0; full < 2; ++full) {
    GovFx f;
    if (!gov_fx_open(&f)) {
      dos_buggy_behavior_set(true);
      return 1;
    }
    ColonizeColony* c = f.col;
    c->stock[COLONIZE_CARGO_FOOD] = 100;
    c->building_in_production = -1;
    c->hammers = full ? COLONIZE_HAMMER_BANK_MAX : 0;
    colony_governor_seat_w(&f.w, c->id, COLONY_GOVERNOR_INDUSTRY);
    if (gov_has_carpenter(&f) == (full != 0)) {
      fprintf(stderr, "bank full %d: carpenter %d\n", full, gov_has_carpenter(&f));
      rc = 1;
    }
    gov_fx_close(&f);
  }
  dos_buggy_behavior_set(true);
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

/* One student, two experts of no use here: the dearer one on the Europe
 * Train list (Veteran Soldier 2000 over Master Gunsmith 850) teaches,
 * whichever colonist slot he is in. */
static int case_teacher_dearest(void) {
  int rc = 0;
  for (int order = 0; order < 2; ++order) {
    GovFx f;
    if (!gov_fx_open(&f)) {
      return 1;
    }
    ColonizeColony* c = f.col;
    const int college = colonies_building_row(&f.pool, COLONY_BUILDING_COLLEGE);
    c->has_building[colonies_building_row(&f.pool, COLONY_BUILDING_SCHOOLHOUSE)] = true;
    c->has_building[college] = true;
    const int soldier = order ? 1 : 2;
    c->colonists[3 - soldier].profession = COLONIZE_PROF_GUNSMITH;
    c->colonists[soldier].profession = UNITS_JOB_SOLDIER;
    /* An indoor expert: a field expert would teach for his future surplus. */
    c->colonists[3].profession = COLONIZE_PROF_CARPENTER;
    c->stock[COLONIZE_CARGO_FOOD] = 150; /* covers the deficit, so teaching pays */
    c->governor = COLONY_GOVERNOR_CASH;
    colony_governor_run_w(&f.w, c->id);
    if (c->colonists[soldier].building_type != college ||
        c->colonists[3 - soldier].building_type == college) {
      fprintf(stderr, "order %d: soldier seat %d gunsmith seat %d\n", order,
              c->colonists[soldier].building_type, c->colonists[3 - soldier].building_type);
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

/* Military: a colony without walls starts on the Stockade; with a food
 * stock it still grows rather than runs a deficit. */
static int case_military_walls_and_growth(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  ColonizeColony* c = f.col;
  c->building_in_production = -1;
  c->governor = COLONY_GOVERNOR_MILITARY;
  colony_governor_run_w(&f.w, c->id);
  const int project = c->building_in_production;
  const int stockade = colonies_building_row(&f.pool, COLONY_BUILDING_STOCKADE);
  const int food = gov_preview(&f).food_net;
  gov_fx_close(&f);
  if (project != stockade || food < 0) {
    fprintf(stderr, "project %d (stockade %d), food_net %d\n", project, stockade, food);
    return 1;
  }
  return 0;
}

/* At 100% SoL with every Father in (the pool buys nothing), the governor
 * still keeps a statesman for the 2 * pop bells a turn that hold it there. */
static int case_bells_keep_full_sol(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  ColonizeColony* c = f.col;
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeCol1Colony rec;
  memset(&rec, 0, sizeof(rec));
  rec.x = (uint8_t)c->x;
  rec.y = (uint8_t)c->y;
  rec.population = (uint8_t)c->colonist_count;
  rec.rebel_dividend = 1000;
  rec.rebel_divisor = 1000;
  col1.head.colony_count = 1;
  col1.colony = &rec;
  memset(col1.nation[0].founding_fathers, 0xff, sizeof(col1.nation[0].founding_fathers));
  f.w.col1 = &col1;
  f.w.col1_ok = true;
  c->stock[COLONIZE_CARGO_FOOD] = 100;
  c->stock[COLONIZE_CARGO_LUMBER] = 0; /* no hammers to outbid the Town Hall */
  c->governor = COLONY_GOVERNOR_INDUSTRY;
  colony_governor_run_w(&f.w, c->id);
  const int hall = colonies_building_row(&f.pool, COLONY_BUILDING_TOWN_HALL);
  int statesmen = 0;
  for (int i = 0; i < c->colonist_count; ++i) {
    statesmen += c->colonists[i].building_type == hall;
  }
  const int bells = gov_preview(&f).bells;
  gov_fx_close(&f);
  if (statesmen == 0) {
    fprintf(stderr, "Town Hall empty, bells %d\n", bells);
    return 1;
  }
  return 0;
}

/* Cash builds crop chains for every crop the colony can grow, worked or not
 * (nobody here works a plot): the site's forests grow furs, and one plot made
 * grassland grows tobacco. Two picks, two chains: with one built through
 * what it can start (its factory needs Adam Smith), the other follows. */
static int case_cash_crop_chains(void) {
  static const int k_tier2[] = {
    COLONY_BUILDING_RUM_DISTILLERY, COLONY_BUILDING_TOBACCONISTS_SHOP,
    COLONY_BUILDING_WEAVERS_SHOP, COLONY_BUILDING_FUR_TRADING_POST,
  };
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  int rc = 0;
  ColonizeColony* c = f.col;
  int fur_plots = 0;
  int grass = -1;
  for (int t = 0; t < 8; ++t) {
    int dx = 0;
    int dy = 0;
    colonies_field_tile_delta(t, &dx, &dy);
    const int x = c->x + dx;
    const int y = c->y + dy;
    if (colony_yield_for_tile_in_colony(&f.pool, c, &f.map, x, y, COLONIZE_JOB_FUR_TRAPPER) > 0 &&
        ++fur_plots == 2) {
      grass = y * (int)f.map.width + x;
    }
  }
  if (grass < 0) {
    fprintf(stderr, "fixture: under two fur plots\n");
    gov_fx_close(&f);
    return 1;
  }
  f.map.terrain[grass] = T_GRASSLAND;
  c->governor = COLONY_GOVERNOR_CASH;
  int picked[2] = {-1, -1};
  for (int round = 0; round < 2 && rc == 0; ++round) {
    c->building_in_production = -1;
    colony_governor_run_w(&f.w, c->id);
    for (int i = 0; i < 4; ++i) {
      if (c->building_in_production == colonies_building_row(&f.pool, k_tier2[i])) {
        picked[round] = i;
      }
    }
    if (picked[round] < 0) {
      fprintf(stderr, "pick %d: project %d is no crop chain's second tier\n", round,
              c->building_in_production);
      rc = 1;
    } else {
      c->has_building[c->building_in_production] = true;
    }
  }
  if (rc == 0 && picked[0] + picked[1] != 1 + 3) {
    fprintf(stderr, "chains %d and %d, not tobacco and furs\n", picked[0], picked[1]);
    rc = 1;
  }
  gov_fx_close(&f);
  return rc;
}

/* Lumberjack and carpenter only pay as a pair (no lumber in store): the
 * chain move seats both where single moves see each alone as a loss. A hold
 * of tools in store keeps the blacksmith pair from outbidding them. */
static int case_chain_lumber_to_hammers(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  ColonizeColony* c = f.col;
  const int hall = colonies_building_row(&f.pool, COLONY_BUILDING_TOWN_HALL);
  c->colonists[3].building_type = hall;
  c->stock[COLONIZE_CARGO_FOOD] = 100;
  c->stock[COLONIZE_CARGO_LUMBER] = 0;
  c->stock[COLONIZE_CARGO_TOOLS] = 100;
  c->governor = COLONY_GOVERNOR_INDUSTRY;
  colony_governor_run_w(&f.w, c->id);
  int lumberjacks = 0;
  for (int i = 0; i < c->colonist_count; ++i) {
    lumberjacks += c->colonists[i].field_job == COLONIZE_JOB_LUMBERJACK;
  }
  const bool carpenter = gov_has_carpenter(&f);
  gov_fx_close(&f);
  if (lumberjacks == 0 || !carpenter) {
    fprintf(stderr, "lumberjacks %d carpenter %d\n", lumberjacks, carpenter);
    return 1;
  }
  return 0;
}

/* Tribal land is the player's to clear (or Peter Minuit's), met tribe or
 * not: a village two tiles off keeps every plot out of the plan. */
static int case_no_tribal_land(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  ColonizeColony* c = f.col;
  int vx = -1;
  int vy = -1;
  for (int dy = -1; dy <= 1 && vx < 0; ++dy) {
    for (int dx = -2; dx <= 2 && vx < 0; dx += 4) {
      if (map_tile_is_land(&f.map, c->x + dx, c->y + dy) &&
          map_continent_id_at(&f.map, c->x + dx, c->y + dy) == map_continent_id_at(&f.map, c->x, c->y)) {
        vx = c->x + dx;
        vy = c->y + dy;
      }
    }
  }
  if (vx < 0) {
    fprintf(stderr, "fixture: no village site\n");
    gov_fx_close(&f);
    return 1;
  }
  ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  ColonizeCol1Tribe tribe;
  memset(&tribe, 0, sizeof(tribe));
  tribe.x = (uint8_t)vx;
  tribe.y = (uint8_t)vy;
  tribe.nation_id = 4;
  col1.indian[0].tech = 3; /* radius 3: every plot of the colony */
  col1.head.tribe_count = 1;
  col1.tribe = &tribe;
  f.w.col1 = &col1;
  f.w.col1_ok = true;
  /* Priced before the run: a seat stamps its plot bought. */
  bool tribal[COLONIZE_COLONY_FIELD_TILES_MAX] = {false};
  int claimed = 0;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    int dx = 0;
    int dy = 0;
    tribal[t] = colonies_field_tile_delta(t, &dx, &dy) &&
                colonies_indian_land_purchase_gold(&col1, &f.map, c->x + dx, c->y + dy, 0) > 0;
    claimed += tribal[t];
  }
  c->governor = COLONY_GOVERNOR_FARMING;
  colony_governor_run_w(&f.w, c->id);
  int rc = claimed == 0;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    if (c->tiles[t] >= 0 && tribal[t]) {
      fprintf(stderr, "plot %d is tribal land\n", t);
      rc = 1;
    }
  }
  gov_fx_close(&f);
  return rc;
}

/* "Autobuy tools": a project with its hammers in and tools short buys them at
 * the BUY price instead of raising @NEEDTOOLS, says so on the status strip,
 * and completes the next turn; an empty treasury gets the notice as before. */
static int case_autobuy_tools(void) {
  colonies_set_occupancy_map(NULL);
  colonies_set_col1_context(NULL);
  static ColonizeColonyPool pool;
  colonies_init(&pool);
  snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Press");
  pool.building_types[0].hammers = 52;
  pool.building_types[0].tools_cost = 20;
  pool.building_types[0].row_plus1 = COLONY_BUILDING_PRINTING_PRESS + 1;
  pool.building_type_count = 1;
  ColonizeColony* col = &pool.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 1;
  snprintf(col->name, sizeof(col->name), "Salem");
  col->building_in_production = 0;
  col->governor_autobuy = true;
  col->stock[COLONIZE_CARGO_FOOD] = 100;
  col->colonists[0].active = true;
  col->colonists[0].building_type = -1;
  col->colonists[0].field_job = -1;
  col->colonist_count = 1;
  col->population = 1;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    col->tiles[t] = -1;
  }
  pool.colony_count = 1;
  static EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  ColonizeMsgCatalog game_txt;
  assets_msg_init(&game_txt);
  (void)assets_msg_load_file(&game_txt, "COLONIZE/GAME.TXT");
  static AiPopupState pops;
  mods_campaign_set(1u << MOD_COLONY_GOVERNOR);
  int fail = 0;
  for (int rich = 0; rich < 2 && !fail; ++rich) {
    col->hammers = 52;
    col->stock[COLONIZE_CARGO_TOOLS] = 5;
    eu.gold = rich ? 5000 : 0;
    ai_popup_init(&pops);
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production_w(&(ColonizeWorld){.colonies = &pool, .europe = &eu}, 0, &prod,
                                 &pops, &game_txt);
    const int tools = col->stock[COLONIZE_CARGO_TOOLS];
    if (rich ? (tools != 20 || eu.gold >= 5000 || pops.queue_count != 0 ||
                pops.bar_msg_count != 1 || strncmp(pops.bar_msg[0], "Salem ", 6) != 0 ||
                col->has_building[0])
             : (tools != 5 || eu.gold != 0 || pops.queue_count != 1 || pops.bar_msg_count != 0)) {
      fprintf(stderr, "autobuy rich=%d: tools %d gold %d q=%d bar=%d built=%d\n", rich, tools,
              eu.gold, pops.queue_count, pops.bar_msg_count, (int)col->has_building[0]);
      fail = 1;
    }
  }
  if (!fail) {
    ai_popup_init(&pops);
    ColonizeTurnResult prod;
    memset(&prod, 0, sizeof(prod));
    turn_run_colony_production_w(&(ColonizeWorld){.colonies = &pool, .europe = &eu}, 0, &prod,
                                 &pops, &game_txt);
    if (!col->has_building[0]) {
      fprintf(stderr, "autobuy: not built the turn after\n");
      fail = 1;
    }
  }
  mods_campaign_set(0);
  assets_msg_free(&game_txt);
  return fail;
}

/* A worker idle in a house with no input leaves it for a job that makes
 * something, even when the weights value that something at 0: here no
 * prices (an unbound market), no project (hammers 0), water all round. */
/* A worker idle in a house with no input leaves it for a job that makes
 * something, even when the weights value that something at 0: a full-SoL
 * colony with every Father in (its upkeep bells already made, the rest worth
 * nothing), no prices (an unbound market), water all round. */
static int case_idle_worker_leaves(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  static EuropeScreen eu;
  memset(&eu, 0, sizeof(eu));
  f.w.europe = &eu;
  ColonizeColony* c = f.col;
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  static ColonizeCol1Colony rec;
  memset(&rec, 0, sizeof(rec));
  rec.x = (uint8_t)c->x;
  rec.y = (uint8_t)c->y;
  rec.population = (uint8_t)c->colonist_count;
  rec.rebel_dividend = 1000;
  rec.rebel_divisor = 1000;
  col1.head.colony_count = 1;
  col1.colony = &rec;
  memset(col1.nation[0].founding_fathers, 0xff, sizeof(col1.nation[0].founding_fathers));
  f.w.col1 = &col1;
  f.w.col1_ok = true;
  for (int t = 0; t < COLONIZE_COLONY_FIELD_TILES_MAX; ++t) {
    int dx = 0;
    int dy = 0;
    colonies_field_tile_delta(t, &dx, &dy);
    if (dx || dy) {
      f.map.terrain[(c->y + dy) * (int)f.map.width + c->x + dx] = T_OCEAN;
    }
  }
  const int furs = colonies_building_row(&f.pool, COLONY_BUILDING_FUR_TRADERS_HOUSE);
  c->colonists[2].building_type = furs;
  c->colonists[3].building_type = furs;
  c->stock[COLONIZE_CARGO_FOOD] = 100;
  c->stock[COLONIZE_CARGO_LUMBER] = 0;
  c->building_in_production = -1;
  colony_governor_seat_w(&f.w, c->id, COLONY_GOVERNOR_CASH);
  int idle = 0;
  for (int i = 0; i < c->colonist_count; ++i) {
    idle += c->colonists[i].building_type == furs;
  }
  const int bells = gov_preview(&f).bells;
  gov_fx_close(&f);
  if (furs < 0 || idle != 1) {
    fprintf(stderr, "%d idle in the fur house, bells %d\n", idle, bells);
    return 1;
  }
  return 0;
}

/* An expert farmer with free colonists to teach and more food plots than
 * farmers: the governor builds a Schoolhouse for him, then seats him there,
 * for the food his graduates will make. */
static int case_field_expert_teaches(void) {
  GovFx f;
  if (!gov_fx_open(&f)) {
    return 1;
  }
  ColonizeColony* c = f.col;
  const int school = colonies_building_row(&f.pool, COLONY_BUILDING_SCHOOLHOUSE);
  c->colonists[3].profession = COLONIZE_PROF_FARMER;
  c->stock[COLONIZE_CARGO_FOOD] = 150;
  c->building_in_production = -1;
  c->governor = COLONY_GOVERNOR_CASH;
  colony_governor_run_w(&f.w, c->id);
  const int project = c->building_in_production;
  c->has_building[school] = true;
  colony_governor_run_w(&f.w, c->id);
  const int seat = c->colonists[3].building_type;
  gov_fx_close(&f);
  if (project != school || seat != school) {
    fprintf(stderr, "project %d seat %d (school %d)\n", project, seat, school);
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"field_expert_teaches", case_field_expert_teaches},
  {"idle_worker_leaves", case_idle_worker_leaves},
  {"autobuy_tools", case_autobuy_tools},
  {"cash_crop_chains", case_cash_crop_chains},
  {"chain_lumber_to_hammers", case_chain_lumber_to_hammers},
  {"no_tribal_land", case_no_tribal_land},
  {"bells_keep_full_sol", case_bells_keep_full_sol},
  {"military_walls_and_growth", case_military_walls_and_growth},
  {"no_food_low_popup", case_no_food_low_popup},
  {"converts_outdoors", case_converts_outdoors},
  {"industry_tools_and_muskets", case_industry_tools_and_muskets},
  {"capture_clears", case_capture_clears},
  {"hammers_only_toward_project", case_hammers_only_toward_project},
  {"ai_seat_bank_full", case_ai_seat_bank_full},
  {"temporary_food_shortfall", case_temporary_food_shortfall},
  {"teacher_sticky", case_teacher_sticky},
  {"teacher_dearest", case_teacher_dearest},
  {"feeds_colonists", case_feeds_colonists},
  {"priorities_differ", case_priorities_differ},
  {"project_only_when_unbuildable", case_project_only_when_unbuildable},
  {"warehouse_room", case_warehouse_room},
  {"rtst_round_trip", case_rtst_round_trip},
};
TEST_MAIN(k_cases)
