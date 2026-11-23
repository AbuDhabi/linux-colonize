/*
 * ai_improvements mod (docs/ai_euro_improvement_spec.md): the pioneer job list
 * and its act (ai_euro_improved.c).
 */
#include "core/ai_euro_internal.h"
#include "core/ai_diplo.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_governor.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/mods.h"
#include "core/pioneer_plan.h"
#include "core/units.h"
#include "platform/diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_runner.h"

/* ai_improvements mod (pt2 P10): 0a60's contact-claim probe rewrites a
 * working pioneer's order to 10 at the nation-turn top; the job act must
 * resume the road instead of restarting it at 0 every turn. */
static int case_pjob_resume(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") || !units_load_types(&pool, &names) ||
      !map_alloc(&map, 16, 16, err, sizeof(err))) {
    fprintf(stderr, "pjob_resume: setup failed\n");
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.colonies = &colonies;
  ctx.map = &map;
  const int pid = units_spawn(&pool, units_find_type(&pool, "Pioneers"), 4, 4);
  ColonizeUnit* pu = units_get(&pool, pid);
  pu->nation_id = 0;
  pu->tools = 100;
  pu->orders = AI_EURO_ACT_ADJACENT;
  pu->col1_counter16 = 1;
  ai_euro_pjob_reset();
  ai_euro_pjob_post(0, 4, 4, PIONEER_PLAN_ROAD);
  int rc = 0;
  if (!ai_euro_pioneer_act(&ctx, pu, 0) || pu->col1_counter16 != 2 ||
      pu->orders != UNITS_ORDER_BUILD_ROAD) {
    fprintf(stderr, "pjob_resume: counter %d orders %d, want 2 / road\n", pu->col1_counter16, pu->orders);
    rc = 1;
  }
  ai_euro_pjob_reset();
  map_free(&map);
  assets_msg_free(&names);
  return rc;
}
/* ai_improvements mod (2026-11-16 expansion gap): a Pioneer between jobs on a
 * continent with fewer than 3 own colonies takes the DOS founder path; one
 * mid-job, on a colony tile, or past the expansion phase stays on the list. */
static int case_pioneer_released(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") || !units_load_types(&pool, &names) ||
      !map_alloc(&map, 16, 16, err, sizeof(err))) {
    fprintf(stderr, "pioneer_released: setup failed\n");
    assets_msg_free(&names);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.colonies = &colonies;
  ctx.map = &map;
  ColonizeUnit* pu = units_get(&pool, units_spawn(&pool, units_find_type(&pool, "Pioneers"), 4, 4));
  pu->nation_id = 0;
  pu->tools = 100;
  ai_euro_pjob_reset();
  ai_euro_pjob_post(0, 8, 8, PIONEER_PLAN_ROAD);
  int rc = 0;
  for (int i = 0; i < 2; ++i) {
    colonies.colonies[i].active = true;
    colonies.colonies[i].nation_id = 0;
    colonies.colonies[i].x = 10 + i;
    colonies.colonies[i].y = 10;
  }
  if (!ai_euro_pioneer_released(&ctx, pu)) {
    fprintf(stderr, "pioneer_released: idle with 2 colonies not released\n");
    rc = 1;
  }
  pu->orders = AI_EURO_ACT_GOAL;
  pu->goto_x = 8;
  pu->goto_y = 8;
  if (ai_euro_pioneer_released(&ctx, pu)) {
    fprintf(stderr, "pioneer_released: walking to a job released\n");
    rc = 1;
  }
  /* #1888: arrived on its job tile, work starts next turn. */
  pu->orders = 0;
  pu->x = 8;
  pu->y = 8;
  if (ai_euro_pioneer_released(&ctx, pu)) {
    fprintf(stderr, "pioneer_released: standing on its job released\n");
    rc = 1;
  }
  pu->x = 10;
  pu->y = 10;
  if (ai_euro_pioneer_released(&ctx, pu)) {
    fprintf(stderr, "pioneer_released: on a colony tile released\n");
    rc = 1;
  }
  pu->x = 4;
  pu->y = 4;
  colonies.colonies[2] = colonies.colonies[1];
  colonies.colonies[2].x = 12;
  if (ai_euro_pioneer_released(&ctx, pu)) {
    fprintf(stderr, "pioneer_released: released with 3 colonies\n");
    rc = 1;
  }
  ai_euro_pjob_reset();
  map_free(&map);
  assets_msg_free(&names);
  return rc;
}
/* ai_improvements mod (2026-11-17 Dragoon founder gap): a colony under 4
 * colonists works for export; at 4 with no SoL it turns to Politics. */
static int case_colony_focus_small_cash(void) {
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ColonizeColony col;
  memset(&col, 0, sizeof(col));
  col.population = 2;
  int rc = 0;
  if (ai_euro_colony_focus(&ctx, &col) != COLONY_GOVERNOR_CASH) {
    fprintf(stderr, "colony_focus: pop 2 focus %d, want Cash\n", ai_euro_colony_focus(&ctx, &col));
    rc = 1;
  }
  col.population = 4;
  if (ai_euro_colony_focus(&ctx, &col) != COLONY_GOVERNOR_POLITICS) {
    fprintf(stderr, "colony_focus: pop 4 focus %d, want Politics\n", ai_euro_colony_focus(&ctx, &col));
    rc = 1;
  }
  return rc;
}
/* ai_improvements mod (pt7b): a one-tile islet is no colony site; a mass of
 * AI_EURO_MIN_SITE_LAND tiles is. */
static int case_land_mass_islet(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = T_OCEAN;
  }
  map.terrain[3 * 16 + 3] = 2; /* islet */
  for (int x = 8; x < 8 + AI_EURO_MIN_SITE_LAND; ++x) {
    map.terrain[8 * 16 + x] = 2; /* strip */
  }
  int rc = 0;
  if (!ai_euro_land_mass_below(&map, 3, 3, AI_EURO_MIN_SITE_LAND) ||
      ai_euro_land_mass_below(&map, 8, 8, AI_EURO_MIN_SITE_LAND) ||
      !ai_euro_land_mass_below(&map, 8, 8, AI_EURO_MIN_SITE_LAND + 1)) {
    fprintf(stderr, "land_mass_islet: wrong mass test\n");
    rc = 1;
  }
  map_free(&map);
  return rc;
}

/* #1910: a site needs a lumber plot in its ring (the French desert
 * fill-ins had none). */
static int case_poor_site(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 1; /* desert */
  }
  int rc = 0;
  if (!ai_euro_land_poor_site(&map, 5, 5)) {
    fprintf(stderr, "poor_site: bare desert accepted\n");
    rc = 1;
  }
  map.terrain[5 * 16 + 4] = 1 + T_FOREST_BIT; /* scrub forest */
  if (ai_euro_land_poor_site(&map, 5, 5)) {
    fprintf(stderr, "poor_site: a lumber plot rejected\n");
    rc = 1;
  }
  map_free(&map);
  return rc;
}

/* #1905: an islet is a site only for a wide nation (3+ colonies, 20+
 * colonists) and only with a forest in its ring. */
static int case_wide_islet_sites(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = T_OCEAN;
  }
  map.terrain[3 * 16 + 3] = 2;                  /* bare islet */
  map.terrain[9 * 16 + 9] = 2;                  /* islet ... */
  map.terrain[9 * 16 + 10] = 2 + T_FOREST_BIT;  /* ... with a forest */
  int rc = 0;
  if (!ai_euro_land_islet_blocked(&map, 3, 3, true) || !ai_euro_land_islet_blocked(&map, 9, 9, false) ||
      ai_euro_land_islet_blocked(&map, 9, 9, true)) {
    fprintf(stderr, "wide_islet_sites: islet gate wrong\n");
    rc = 1;
  }
  map_free(&map);

  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[1].control = 1;
  static ColonizeColonyPool colonies;
  memset(&colonies, 0, sizeof(colonies));
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.human_nation = 0;
  ctx.colonies = &colonies;
  /* {colonies, population each, mod, want} */
  static const int rows[][4] = {{3, 7, 1, 1}, {2, 10, 1, 0}, {3, 6, 1, 0}, {3, 7, 0, 0}};
  for (size_t i = 0; i < sizeof rows / sizeof rows[0]; ++i) {
    memset(&colonies, 0, sizeof(colonies));
    for (int c = 0; c < rows[i][0]; ++c) {
      colonies.colonies[c].active = true;
      colonies.colonies[c].nation_id = 1;
      colonies.colonies[c].population = rows[i][1];
    }
    mods_campaign_set(rows[i][2] ? 1u << MOD_AI_IMPROVEMENTS : 0u);
    if (ai_euro_land_wide_sites(&ctx, 1) != (rows[i][3] != 0)) {
      fprintf(stderr, "wide_islet_sites: wide row %zu wrong\n", i);
      rc = 1;
    }
  }
  mods_campaign_set(0);
  return rc;
}

/* ai_improvements mod (#1880): a Colonist waits for a ship only when no own
 * colony on its continent would admit it and one elsewhere wants colonists. */
static int case_colonist_ferry_wanted(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = 2;
    map.layer3[i] = (uint8_t)(0xf0 | (i % 16 < 8 ? 1 : 2)); /* continents 1 | 2 */
  }
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  units_reset(&pool);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* home = &colonies.colonies[0];
  home->active = true;
  home->nation_id = 1;
  home->x = 3;
  home->y = 3;
  home->population = 20;
  home->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS; /* flagged, but full */
  ColonizeColony* away = &colonies.colonies[1];
  away->active = true;
  away->nation_id = 1;
  away->x = 12;
  away->y = 3;
  away->population = 3;
  away->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.map = &map;
  ctx.colonies = &colonies;
  ctx.units = &pool;
  ctx.human_nation = 0;
  int rc = 0;
  mods_campaign_set(1u << MOD_AI_IMPROVEMENTS);
  if (!ai_euro_colonist_ferry_wanted(&ctx, 1, 1)) {
    fprintf(stderr, "colonist_ferry_wanted: full home colony, wanted elsewhere: want a ship\n");
    rc = 1;
  }
  home->population = 3;
  if (rc == 0 && ai_euro_colonist_ferry_wanted(&ctx, 1, 1)) {
    fprintf(stderr, "colonist_ferry_wanted: the home colony admits it\n");
    rc = 1;
  }
  home->population = 20;
  away->ai_flags = 0;
  if (rc == 0 && ai_euro_colonist_ferry_wanted(&ctx, 1, 1)) {
    fprintf(stderr, "colonist_ferry_wanted: no colony elsewhere wants colonists\n");
    rc = 1;
  }
  away->ai_flags = COLONIZE_COLONY_AI_NEEDS_COLONISTS;
  mods_campaign_set(0);
  if (rc == 0 && ai_euro_colonist_ferry_wanted(&ctx, 1, 1)) {
    fprintf(stderr, "colonist_ferry_wanted: DOS mode never ferries\n");
    rc = 1;
  }
  map_free(&map);
  return rc;
}

/* ai_improvements mod (pt7d): two seas split by a land wall; a hull only
 * "has" the colony that borders its own sea. */
static int case_sea_has_own_colony(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.terrain[i] = (i % 16 == 8) ? 2 : T_OCEAN; /* land column x = 8 */
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  const int cid = 0;
  colonies.colonies[cid].active = true;
  colonies.colonies[cid].nation_id = 3;
  colonies.colonies[cid].x = 8;
  colonies.colonies[cid].y = 4;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.map = &map;
  ctx.colonies = &colonies;
  int rc = 0;
  if (!ai_euro_sea_has_own_colony(&ctx, 3, 3, 3) || !ai_euro_sea_has_own_colony(&ctx, 12, 3, 3) ||
      ai_euro_sea_has_own_colony(&ctx, 3, 3, 2)) {
    fprintf(stderr, "sea_has_own_colony: colony on the wall borders both seas, not nation 2\n");
    rc = 1;
  }
  /* Move the colony to (8,15) and wall off its east side. */
  colonies.colonies[cid].x = 8;
  colonies.colonies[cid].y = 15;
  map.terrain[14 * 16 + 9] = 2;
  map.terrain[15 * 16 + 9] = 2;
  if (rc == 0 && ai_euro_sea_has_own_colony(&ctx, 12, 3, 3)) {
    fprintf(stderr, "sea_has_own_colony: east sea reaches a colony walled off from it\n");
    rc = 1;
  }
  map_free(&map);
  return rc;
}

/* ai_improvements mod (#1891): a Scout's target is the nearest seen village
 * no Scout has visited that a land walk reaches; a village across the water
 * on a land mass with the same continent nibble is not. */
static int case_scout_target(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[128];
  if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    const int x = i % 16;
    map.terrain[i] = (x >= 1 && x <= 5) || (x >= 9 && x <= 14) ? 2 : T_OCEAN; /* two islands */
    map.layer3[i] = 0xf1; /* one continent nibble for both */
    map.layer2[i] = MAP_LAYER2_LCR_CONSUMED; /* no rumours */
    map.seen[i] = 0x10 << 1;               /* seen by nation 1 */
  }
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.tribe_count = 3;
  col1.tribe = calloc(3, sizeof(ColonizeCol1Tribe));
  if (!col1.tribe) {
    map_free(&map);
    return 1;
  }
  col1.owned = true;
  const int vx[3] = {10, 4, 3};
  const int vy[3] = {2, 12, 4};
  for (int i = 0; i < 3; ++i) {
    col1.tribe[i].x = (uint8_t)vx[i];
    col1.tribe[i].y = (uint8_t)vy[i];
    col1.tribe[i].nation_id = 5;
    col1.tribe[i].mission = 0xff;
  }
  col1.tribe[2].state.scouted = true;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.map = &map;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  int rc = 0;
  int tx = -1;
  int ty = -1;
  /* Scout at (3,2): the village at (10,2) is nearer but across the water,
   * (3,4) is scouted, so (4,12). */
  if (!ai_euro_scout_target(&ctx, 1, 3, 2, -1, -1, NULL, 0, &tx, &ty, NULL, NULL) || tx != 4 || ty != 12) {
    fprintf(stderr, "scout_target: want (4,12), got (%d,%d)\n", tx, ty);
    rc = 1;
  }
  /* With (3,4) unscouted it is the nearest, but the last target, (4,12),
   * stays while it still is one (the AI goto lasts a turn). */
  col1.tribe[2].state.scouted = false;
  if (rc == 0 && (!ai_euro_scout_target(&ctx, 1, 3, 2, -1, -1, NULL, 0, &tx, &ty, NULL, NULL) || tx != 3 || ty != 4)) {
    fprintf(stderr, "scout_target: want (3,4), got (%d,%d)\n", tx, ty);
    rc = 1;
  }
  if (rc == 0 && (!ai_euro_scout_target(&ctx, 1, 3, 2, 4, 12, NULL, 0, &tx, &ty, NULL, NULL) || tx != 4 || ty != 12)) {
    fprintf(stderr, "scout_target: last target dropped, got (%d,%d)\n", tx, ty);
    rc = 1;
  }
  /* The mover found no path to it: (3,4) after all. */
  const int no_path[1] = {12 * 16 + 4};
  if (rc == 0 && (!ai_euro_scout_target(&ctx, 1, 3, 2, 4, 12, no_path, 1, &tx, &ty, NULL, NULL) || tx != 3 || ty != 4)) {
    fprintf(stderr, "scout_target: blocked target kept, got (%d,%d)\n", tx, ty);
    rc = 1;
  }
  col1.tribe[2].state.scouted = true;
  col1.tribe[1].state.scouted = true;
  int fx = 0;
  int fy = 0;
  if (rc == 0 && (ai_euro_scout_target(&ctx, 1, 3, 2, -1, -1, NULL, 0, &tx, &ty, &fx, &fy) || fx >= 0)) {
    fprintf(stderr, "scout_target: nothing left on the island, got (%d,%d) fog (%d,%d)\n", tx, ty, fx, fy);
    rc = 1;
  }
  /* #1892: an unseen tile on its own island keeps the Scout exploring; one
   * across the water does not. */
  map.seen[2 * 16 + 12] = 0;
  if (rc == 0 && (ai_euro_scout_target(&ctx, 1, 3, 2, -1, -1, NULL, 0, &tx, &ty, &fx, &fy) || fx >= 0)) {
    fprintf(stderr, "scout_target: fog across the water counted\n");
    rc = 1;
  }
  map.seen[14 * 16 + 2] = 0;
  if (rc == 0 && (ai_euro_scout_target(&ctx, 1, 3, 2, -1, -1, NULL, 0, &tx, &ty, &fx, &fy) || fx != 2 || fy != 14)) {
    fprintf(stderr, "scout_target: unseen land on its own island missed, got (%d,%d)\n", fx, fy);
    rc = 1;
  }
  col1_save_free(&col1);
  map_free(&map);
  return rc;
}

/* #1893: a sale-good backlog with under AI_EURO_FLEET_HOLDS_PER_COLONY
 * cargo holds per colony makes an improved nation short of hulls. */
static int case_fleet_short(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") || !units_load_types(&pool, &names)) {
    fprintf(stderr, "fleet_short: setup failed\n");
    assets_msg_free(&names);
    return 1;
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  for (int i = 0; i < 2; ++i) {
    ColonizeColony* c = &colonies.colonies[i];
    c->active = true;
    c->nation_id = 1;
    c->x = 3 + 6 * i;
    c->y = 3;
    c->population = 4;
  }
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.colonies = &colonies;
  ctx.human_nation = 0;
  ColonizeUnit* caravel = units_get(&pool, units_spawn(&pool, units_find_type(&pool, "Caravel"), 5, 5));
  caravel->nation_id = 1;
  int rc = 0;
  mods_campaign_set(1u << MOD_AI_IMPROVEMENTS);
  if (ai_euro_improved_fleet_short(&ctx, 1)) {
    fprintf(stderr, "fleet_short: no backlog, no hull wanted\n");
    rc = 1;
  }
  colonies.colonies[1].stock[COLONIZE_CARGO_FURS] =
    colonies_warehouse_capacity(&colonies, &colonies.colonies[1], COLONIZE_CARGO_FURS);
  if (rc == 0 && !ai_euro_improved_fleet_short(&ctx, 1)) {
    fprintf(stderr, "fleet_short: furs at the limit, 2 holds for 2 colonies: want a hull\n");
    rc = 1;
  }
  ColonizeUnit* galleon = units_get(&pool, units_spawn(&pool, units_find_type(&pool, "Galleon"), 6, 6));
  galleon->nation_id = 1;
  if (rc == 0 && ai_euro_improved_fleet_short(&ctx, 1)) {
    fprintf(stderr, "fleet_short: 8 holds for 2 colonies is enough\n");
    rc = 1;
  }
  galleon->active = false;
  mods_campaign_set(0);
  if (rc == 0 && ai_euro_improved_fleet_short(&ctx, 1)) {
    fprintf(stderr, "fleet_short: DOS mode never\n");
    rc = 1;
  }
  assets_msg_free(&names);
  return rc;
}

/* #1900: an improved AI's hold sale pays its tax rate into royal_money;
 * mod off, DOS pays the AI the untaxed gross. */
static int case_sale_tax(void) {
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[1].control = 1;
  col1.nation[1].tax_rate = 30;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.human_nation = 0;
  int rc = 0;
  if (ai_euro_improved_sale_net(&ctx, 1, 1000) != 1000 || col1.nation[1].royal_money != 0) {
    fprintf(stderr, "sale_tax: DOS mode pays the gross\n");
    rc = 1;
  }
  mods_campaign_set(1u << MOD_AI_IMPROVEMENTS);
  if (rc == 0 && (ai_euro_improved_sale_net(&ctx, 1, 1000) != 700 || col1.nation[1].royal_money != 300)) {
    fprintf(stderr, "sale_tax: 1000 at 30%% nets 700, 300 to the Crown\n");
    rc = 1;
  }
  mods_campaign_set(0);
  return rc;
}

/* #1904: an improved AI's colony tile takes the handout at 4 - difficulty. */
static int case_commons_mirror(void) {
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[1].control = 1;
  /* {mod, difficulty, nation, want}: nation 0 is the human (control 0). */
  static const int rows[][4] = {
    {0, 0, 1, 0}, {1, 0, 1, 4}, {1, 1, 1, 3}, {1, 4, 1, 0}, {1, 0, 0, 0},
  };
  int rc = 0;
  for (size_t i = 0; i < sizeof rows / sizeof rows[0]; ++i) {
    mods_campaign_set(rows[i][0] ? 1u << MOD_AI_IMPROVEMENTS : 0u);
    col1.head.difficulty = (uint8_t)rows[i][1];
    const int got = colony_yield_commons_difficulty(&col1, rows[i][2]);
    if (got != rows[i][3]) {
      fprintf(stderr, "commons_mirror row %zu: %d want %d\n", i, got, rows[i][3]);
      rc = 1;
    }
  }
  mods_campaign_set(0);
  return rc;
}

/* #1894: an improved nation takes the slate row worth most to it. */
static int case_father_pick(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->active = true;
  c->nation_id = 1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.colonies = &colonies;
  ctx.human_nation = 0;
  int rc = 0;
  /* Trade, Exploration, Military, Political, Religious rows. */
  int slate[5] = {FF_PETER_MINUIT, FF_HENRY_HUDSON, FF_PAUL_REVERE, FF_THOMAS_JEFFERSON, FF_WILLIAM_PENN};
  int pick = founding_fathers_improved_pick(&ctx, 1, slate);
  if (pick != FF_THOMAS_JEFFERSON) {
    fprintf(stderr, "father_pick: %d, want Jefferson\n", pick);
    rc = 1;
  }
  slate[3] = FF_JAN_DE_WITT; /* human-only effect */
  pick = founding_fathers_improved_pick(&ctx, 1, slate);
  if (rc == 0 && pick != FF_WILLIAM_PENN) {
    fprintf(stderr, "father_pick: %d, want Penn over de Witt\n", pick);
    rc = 1;
  }
  for (int m = 0; m < 6; ++m) {
    c->colonists[m].active = true;
    c->colonists[m].field_job = COLONIZE_JOB_FUR_TRAPPER;
  }
  pick = founding_fathers_improved_pick(&ctx, 1, slate);
  if (rc == 0 && pick != FF_HENRY_HUDSON) {
    fprintf(stderr, "father_pick: six Fur Trappers: %d, want Hudson\n", pick);
    rc = 1;
  }
  /* Spain: Cortes 60, and 80 with two villages burned. */
  const int war[5] = {FF_PETER_MINUIT, FF_FERDINAND_MAGELLAN, FF_HERNAN_CORTES, FF_THOMAS_PAINE, FF_JEAN_DE_BREBEUF};
  if (rc == 0 && founding_fathers_improved_pick(&ctx, 1, war) != FF_FERDINAND_MAGELLAN) {
    fprintf(stderr, "father_pick: France takes Magellan over Cortes\n");
    rc = 1;
  }
  col1.nation[2].villages_burned = 2;
  if (rc == 0 && founding_fathers_improved_pick(&ctx, 2, war) != FF_HERNAN_CORTES) {
    fprintf(stderr, "father_pick: Spain with 2 villages burned takes Cortes\n");
    rc = 1;
  }
  const int empty[5] = {-1, -1, -1, -1, -1};
  if (rc == 0 && founding_fathers_improved_pick(&ctx, 1, empty) != -1) {
    fprintf(stderr, "father_pick: empty slate\n");
    rc = 1;
  }
  return rc;
}

/* #1913: the war ledger starts with WAR, counts fights and colonies, tires
 * with time, clears with peace and round-trips through RTST. */
static int case_war_momentum(void) {
  mods_campaign_set(1u << MOD_AI_IMPROVEMENTS);
  ai_diplo_war_ledger_reset();
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof col1);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.col1 = &col1; /* no units/map: 10ec answers 0, so only the ledger counts */
  col1.head.turn = 10;
  int fail = 0;
  fail |= ai_diplo_war_momentum(&ctx, 0, 1) != 0;
  ai_diplo_or_both(&col1, 0, 1, AI_DIPLO_WAR);
  fail |= ai_diplo_war_momentum(&ctx, 0, 1) != 3 || ai_diplo_war_momentum(&ctx, 1, 0) != 3;
  for (int i = 0; i < 4; ++i) {
    ai_diplo_war_note_fight(0, 1);
  }
  ai_diplo_war_note_capture(1, 0);
  ai_diplo_war_note_fight(2, 3); /* not at war: ignored */
  fail |= ai_diplo_war_momentum(&ctx, 0, 1) != 3 + 2 - 2 || ai_diplo_war_momentum(&ctx, 1, 0) != 3 - 2 + 2;
  uint8_t blob[AI_DIPLO_WAR_LEDGER_SIZE];
  fail |= !ai_diplo_war_ledger_export(blob);
  ai_diplo_war_ledger_reset();
  ai_diplo_war_ledger_import(blob);
  col1.head.turn = 50; /* 40 turns on: fully tired */
  fail |= ai_diplo_war_momentum(&ctx, 0, 1) != -4 + 2 - 2;
  ai_diplo_war_note_capture(0, 1);
  ai_diplo_war_note_capture(0, 1);
  fail |= ai_diplo_war_momentum(&ctx, 0, 1) != -4 + 2 + 2;
  /* #1914: no encounter for 30 turns, so the treaty tick runs the peace
   * test itself; both sides are tired and sign. */
  for (int n = 0; n < 4; ++n) {
    col1.player[n].control = 1;
  }
  ai_diplo_war_note_capture(1, 0);
  ai_diplo_war_note_capture(1, 0);
  uint32_t turn = 50;
  ctx.turn_number = &turn;
  ctx.col1_ok = true;
  ctx.human_nation = -1;
  col1.head.turn = 39; /* 29 turns since the war began: not yet */
  ai_diplo_treaty_timers(&ctx, 0);
  fail |= (col1.nation[0].euro_relation[1] & AI_DIPLO_WAR) == 0;
  col1.head.turn = 40;
  ai_diplo_treaty_timers(&ctx, 0);
  fail |= (col1.nation[0].euro_relation[1] & AI_DIPLO_WAR) != 0;
  fail |= ai_diplo_war_momentum(&ctx, 0, 1) != 0 || ai_diplo_war_ledger_export(blob);
  mods_campaign_set(0);
  ai_diplo_or_both(&col1, 2, 3, AI_DIPLO_WAR); /* mod off: no ledger */
  fail |= ai_diplo_war_ledger_export(blob);
  if (fail) {
    fprintf(stderr, "war momentum ledger mismatch\n");
  }
  return fail;
}

/* ai_improvements mod (#1915): an improved AI's craft buildings need their
 * inputs; DOS's AI-only shortfall exemption (#898) stays mod-off. */
static int case_craft_needs_inputs(void) {
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[1].control = 1;
  ColonizeColony colony;
  memset(&colony, 0, sizeof(colony));
  colony.active = true;
  colony.nation_id = 1;
  int rc = 0;
  mods_campaign_set(0);
  if (!colony_prod_craft_ignores_inputs(&col1, 0, &colony)) {
    fprintf(stderr, "craft_needs_inputs: mod off, AI keeps DOS exemption\n");
    rc = 1;
  }
  mods_campaign_set(1u << MOD_AI_IMPROVEMENTS);
  if (colony_prod_craft_ignores_inputs(&col1, 0, &colony)) {
    fprintf(stderr, "craft_needs_inputs: improved AI must need inputs\n");
    rc = 1;
  }
  colony.nation_id = 0;
  if (colony_prod_craft_ignores_inputs(&col1, 0, &colony)) {
    fprintf(stderr, "craft_needs_inputs: human always needs inputs\n");
    rc = 1;
  }
  mods_campaign_set(0);
  return rc;
}

/* #1917: with building upkeep on, an improved nation keeps next turn's upkeep
 * out of its spending; the human, a DOS AI and either mod off keep nothing. */
static int case_upkeep_reserve(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  static ColonizeColonyPool pool;
  colonies_init(&pool);
  int rc = 0;
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") || !colonies_load_buildings(&pool, &names)) {
    fprintf(stderr, "upkeep_reserve: fixture load failed\n");
    return 1;
  }
  ColonizeColony* col = &pool.colonies[0];
  col->active = true;
  col->nation_id = 1;
  col->has_building[colonies_building_row(&pool, COLONY_BUILDING_DOCKS)] = true;
  const int total = colonies_upkeep_total(&pool, 1);
  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.player[1].control = 1;
  const unsigned both = (1u << MOD_BUILDING_UPKEEP) | (1u << MOD_AI_IMPROVEMENTS);
  mods_campaign_set(both);
  if (total <= 0 || ai_euro_upkeep_reserve(&col1, &pool, 0, 1) != total) {
    fprintf(stderr, "upkeep_reserve: improved AI keeps %ld, want %d\n",
            ai_euro_upkeep_reserve(&col1, &pool, 0, 1), total);
    rc = 1;
  }
  if (ai_euro_upkeep_reserve(&col1, &pool, 1, 1) != 0) {
    fprintf(stderr, "upkeep_reserve: the human keeps nothing\n");
    rc = 1;
  }
  mods_campaign_set(1u << MOD_AI_IMPROVEMENTS);
  if (ai_euro_upkeep_reserve(&col1, &pool, 0, 1) != 0) {
    fprintf(stderr, "upkeep_reserve: upkeep off keeps nothing\n");
    rc = 1;
  }
  mods_campaign_set(1u << MOD_BUILDING_UPKEEP);
  if (ai_euro_upkeep_reserve(&col1, &pool, 0, 1) != 0) {
    fprintf(stderr, "upkeep_reserve: DOS AI keeps nothing\n");
    rc = 1;
  }
  /* #1918: a Fort over a Stockade adds only the difference; a second Docks
   * adds nothing. */
  const int stockade = colonies_building_row(&pool, COLONY_BUILDING_STOCKADE);
  const int fort = colonies_building_row(&pool, COLONY_BUILDING_FORT);
  const int docks = colonies_building_row(&pool, COLONY_BUILDING_DOCKS);
  const int fort_alone = ai_euro_upkeep_added(&pool, col, fort);
  col->has_building[stockade] = true;
  const int stockade_cost = colonies_upkeep_colony(&pool, col) - total;
  if (fort_alone <= 0 || ai_euro_upkeep_added(&pool, col, fort) != fort_alone - stockade_cost ||
      ai_euro_upkeep_added(&pool, col, docks) != 0) {
    fprintf(stderr, "upkeep_reserve: added fort %d (alone %d, stockade %d), docks %d\n",
            ai_euro_upkeep_added(&pool, col, fort), fort_alone, stockade_cost,
            ai_euro_upkeep_added(&pool, col, docks));
    rc = 1;
  }
  mods_campaign_set(0);
  assets_msg_free(&names);
  return rc;
}

static const TestCase k_cases[] = {
  {"colony_focus_small_cash", case_colony_focus_small_cash},
  {"pjob_resume", case_pjob_resume},
  {"pioneer_released", case_pioneer_released},
  {"land_mass_islet", case_land_mass_islet},
  {"sea_has_own_colony", case_sea_has_own_colony},
  {"colonist_ferry_wanted", case_colonist_ferry_wanted},
  {"scout_target", case_scout_target},
  {"fleet_short", case_fleet_short},
  {"father_pick", case_father_pick},
  {"sale_tax", case_sale_tax},
  {"commons_mirror", case_commons_mirror},
  {"wide_islet_sites", case_wide_islet_sites},
  {"poor_site", case_poor_site},
  {"war_momentum", case_war_momentum},
  {"craft_needs_inputs", case_craft_needs_inputs},
  {"upkeep_reserve", case_upkeep_reserve},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
