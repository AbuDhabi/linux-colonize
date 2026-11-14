/* mods.pioneer_plan: plan editing, reconcile, Automate, PPLN round trip, off. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_runner.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/pioneer_plan.h"
#include "core/mods.h"
#include "core/units.h"
#include "core/units_move.h"
#include "platform/diagnostics.h"

#define CHECK(c)                                                    \
  do {                                                              \
    if (!(c)) {                                                     \
      fprintf(stderr, "pioneer_plan: %s:%d %s\n", __FILE__, __LINE__, #c); \
      return 1;                                                     \
    }                                                               \
  } while (0)

static ColonizeMsgCatalog s_names;
static ColonizeUnitPool s_pool;
static ColonizeWorldMap s_map;
static ColonizeColonyPool s_colonies;

/* 8x8 plains, one continent, all seen by nation 0; forest at (3,3); pioneer at (1,1). */
static int setup(int* out_pid) {
  mods_campaign_set(1u << MOD_PIONEER_PLAN);
  pioneer_plan_reset();

  assets_msg_init(&s_names);
  if (!assets_msg_load_file(&s_names, "COLONIZE/NAMES.TXT")) {
    return TR_SKIP;
  }
  memset(&s_pool, 0, sizeof(s_pool));
  CHECK(units_load_types(&s_pool, &s_names));
  memset(&s_map, 0, sizeof(s_map));
  char err[128];
  CHECK(map_alloc(&s_map, 8, 8, err, sizeof(err)));
  for (int i = 0; i < 64; ++i) {
    s_map.terrain[i] = 2; /* plains */
    s_map.seen[i] = 0x10;
    s_map.layer3[i] = 0xf1; /* unowned, continent 1 */
  }
  s_map.terrain[3 * 8 + 3] = 10; /* mixed forest */
  colonies_init(&s_colonies);
  colonies_set_occupancy_map(NULL);
  const int pid = units_spawn(&s_pool, units_find_type(&s_pool, "Pioneers"), 1, 1);
  ColonizeUnit* u = units_get(&s_pool, pid);
  CHECK(u && units_is_pioneer(&s_pool, pid));
  u->nation_id = 0;
  u->tools = 100;
  u->moves = UNITS_MP_PER_TILE;
  *out_pid = pid;
  return 0;
}

static void teardown(void) {
  mods_campaign_set(0);
  map_free(&s_map);
  assets_msg_free(&s_names);
}

static ColonizeWorld world(void) {
  return (ColonizeWorld){.units = &s_pool, .colonies = &s_colonies, .map = &s_map};
}

static int case_toggle_and_reconcile(void) {
  int pid = 0;
  const int r = setup(&pid);
  if (r) {
    return r;
  }
  const ColonizeWorld w = world();
  /* Plains: Plow toggles; Clear is not offered. */
  CHECK(!pioneer_plan_toggle(&w, 0, 5, 5, PIONEER_PLAN_CLEAR));
  CHECK(pioneer_plan_toggle(&w, 0, 5, 5, PIONEER_PLAN_PLOW));
  CHECK(pioneer_plan_at(5, 5) == PIONEER_PLAN_PLOW);
  CHECK(pioneer_plan_toggle(&w, 0, 5, 5, PIONEER_PLAN_PLOW));
  CHECK(pioneer_plan_at(5, 5) == 0);
  /* Forest: Clear -> Clear+Plow -> nothing; Road rides alongside. */
  CHECK(pioneer_plan_toggle(&w, 0, 3, 3, PIONEER_PLAN_ROAD));
  CHECK(pioneer_plan_toggle(&w, 0, 3, 3, PIONEER_PLAN_CLEAR));
  CHECK(pioneer_plan_at(3, 3) == (PIONEER_PLAN_ROAD | PIONEER_PLAN_CLEAR));
  CHECK(pioneer_plan_toggle(&w, 0, 3, 3, PIONEER_PLAN_CLEAR));
  CHECK(pioneer_plan_at(3, 3) == (PIONEER_PLAN_ROAD | PIONEER_PLAN_CLEAR | PIONEER_PLAN_PLOW));
  /* No duplicate: a road tile takes no road plan; unexplored tiles take nothing. */
  map_tile_set_road(&s_map, 6, 6, true);
  CHECK(!pioneer_plan_toggle(&w, 0, 6, 6, PIONEER_PLAN_ROAD));
  s_map.seen[7 * 8 + 7] = 0;
  CHECK(!pioneer_plan_toggle(&w, 0, 7, 7, PIONEER_PLAN_PLOW));
  /* Out of sight the plan stays; in sight the done Clear drops, Plow stays. */
  map_tile_clear_forest(&s_map, 3, 3);
  pioneer_plan_reconcile_seen(&w, 0);
  CHECK(pioneer_plan_at(3, 3) == (PIONEER_PLAN_ROAD | PIONEER_PLAN_CLEAR | PIONEER_PLAN_PLOW));
  units_get(&s_pool, pid)->x = 2;
  units_get(&s_pool, pid)->y = 2;
  pioneer_plan_reconcile_seen(&w, 0);
  CHECK(pioneer_plan_at(3, 3) == (PIONEER_PLAN_ROAD | PIONEER_PLAN_PLOW));
  teardown();
  return 0;
}

static int case_automate(void) {
  int pid = 0;
  const int r = setup(&pid);
  if (r) {
    return r;
  }
  const ColonizeWorld w = world();
  ColonizeUnit* u = units_get(&s_pool, pid);
  CHECK(!pioneer_plan_automate(&w, u, NULL, NULL)); /* nothing planned */
  CHECK(!u->automated);
  CHECK(pioneer_plan_toggle(&w, 0, 4, 1, PIONEER_PLAN_ROAD));
  CHECK(pioneer_plan_toggle(&w, 0, 4, 1, PIONEER_PLAN_PLOW));
  CHECK(pioneer_plan_automate(&w, u, NULL, NULL));
  CHECK(u->orders == UNITS_ORDER_GOTO && u->goto_x == 4 && u->goto_y == 1);
  /* Arrived: Road comes first, through the ordinary road body. */
  u->x = 4;
  u->orders = UNITS_ORDER_NONE;
  CHECK(pioneer_plan_auto_step(&w, u, NULL, NULL));
  CHECK(u->orders == UNITS_ORDER_BUILD_ROAD && u->automated);
  /* PPLN round trip keeps the plan and the automated unit. */
  size_t n = 0;
  uint8_t* blob = pioneer_plan_serialize(&s_map, &s_pool, &n);
  CHECK(blob && n > 0);
  u->automated = false;
  pioneer_plan_deserialize(blob, n, &s_pool);
  free(blob);
  CHECK(u->automated);
  CHECK(pioneer_plan_at(4, 1) == (PIONEER_PLAN_ROAD | PIONEER_PLAN_PLOW));
  /* The job that spends the last tools ends Automate right then, not at the
   * next auto step. */
  u->tools = 20;
  for (int t = 0; t < 16 && u->orders == UNITS_ORDER_BUILD_ROAD; ++t) {
    u->moves = units_max_mp(&s_pool, u->id);
    (void)units_pioneer_work_tick_w(&w, u->id, NULL, NULL, NULL);
  }
  CHECK(u->tools == 0 && !u->automated);
  CHECK(u->orders == UNITS_ORDER_NONE); /* no colony with tools: just stops */
  /* Resupply: min(tools, 100) / (distance + 1). From (4,1): A (2,1) d2 60
   * tools scores 20, B (6,6) d6 200 tools scores 100/7, C (5,1) under 20.
   * (Edge tiles refuse a Go To, hence 6,6.) */
  ColonizeColony* ca = colonies_get_mut(&s_colonies, colonies_found(&s_colonies, &s_map, 2, 1, 0, 0, 0, 60, 0, 0));
  ColonizeColony* cb = colonies_get_mut(&s_colonies, colonies_found(&s_colonies, &s_map, 6, 6, 0, 0, 0, 200, 0, 0));
  ColonizeColony* cc = colonies_get_mut(&s_colonies, colonies_found(&s_colonies, &s_map, 5, 1, 0, 0, 0, 10, 0, 0));
  CHECK(ca && cb && cc);
  ca->stock[COLONIZE_CARGO_TOOLS] = 60;
  cb->stock[COLONIZE_CARGO_TOOLS] = 200;
  cc->stock[COLONIZE_CARGO_TOOLS] = 10;
  units_pioneer_resupply(&w, u);
  CHECK(u->orders == UNITS_ORDER_GOTO && u->goto_x == 2 && u->goto_y == 1 && !u->automated);
  ca->stock[COLONIZE_CARGO_TOOLS] = 30; /* 30/3 now loses to 100/7 */
  units_pioneer_resupply(&w, u);
  CHECK(u->orders == UNITS_ORDER_GOTO && u->goto_x == 6 && u->goto_y == 6);
  units_clear_orders(&s_pool, u->id);
  /* Through the work tick: the last-tools job sends the Colonist off. */
  ca->stock[COLONIZE_CARGO_TOOLS] = 60;
  const int pid2 = units_spawn(&s_pool, units_find_type(&s_pool, "Pioneers"), 4, 2);
  ColonizeUnit* u2 = units_get(&s_pool, pid2);
  CHECK(u2);
  u2->nation_id = 0;
  u2->tools = 20;
  u2->automated = true;
  u2->orders = UNITS_ORDER_BUILD_ROAD;
  for (int t = 0; t < 16 && u2->orders == UNITS_ORDER_BUILD_ROAD; ++t) {
    u2->moves = units_max_mp(&s_pool, u2->id);
    (void)units_pioneer_work_tick_w(&w, u2->id, NULL, NULL, NULL);
  }
  CHECK(u2->tools == 0 && !u2->automated);
  CHECK(u2->orders == UNITS_ORDER_GOTO && u2->goto_x == 2 && u2->goto_y == 1);
  /* Out of tools cancels. */
  u->automated = true;
  u->orders = UNITS_ORDER_NONE;
  u->tools = 0;
  CHECK(!pioneer_plan_auto_step(&w, u, NULL, NULL));
  CHECK(!u->automated);
  /* Mod off (a campaign without it): no Automate, an automated Pioneer drops
   * it on its next step, no PPLN chunk. */
  mods_campaign_set(0);
  u->tools = 100;
  CHECK(!pioneer_plan_enabled());
  CHECK(!pioneer_plan_automate(&w, u, NULL, NULL) && !u->automated);
  u->automated = true;
  CHECK(!pioneer_plan_auto_step(&w, u, NULL, NULL));
  CHECK(!u->automated && u->orders == UNITS_ORDER_NONE);
  n = 0;
  CHECK(!pioneer_plan_serialize(&s_map, &s_pool, &n) && n == 0);
  teardown();
  return 0;
}

static const TestCase k_cases[] = {
  {"toggle_and_reconcile", case_toggle_and_reconcile},
  {"automate", case_automate},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
