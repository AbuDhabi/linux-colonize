/* divergences.scout_autoexplore / ship_autoexplore: target choice, hostile avoidance,
 * autostop, SAUT round trip. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_runner.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/pioneer_plan.h"
#include "core/explore_auto.h"
#include "core/settings.h"
#include "core/units.h"
#include "core/units_move.h"
#include "platform/diagnostics.h"

#define CHECK(c)                                                  \
  do {                                                            \
    if (!(c)) {                                                   \
      fprintf(stderr, "explore_auto: %s:%d %s\n", __FILE__, __LINE__, #c); \
      return 1;                                                   \
    }                                                             \
  } while (0)

static ColonizeMsgCatalog s_names;
static ColonizeUnitPool s_pool;
static ColonizeWorldMap s_map;
static ColonizeColonyPool s_colonies;

static int case_scout(void) {
  ColonizeSettings s;
  settings_defaults(&s);
  s.scout_autoexplore = true;
  settings_set(&s);
  explore_auto_reset();
  assets_msg_init(&s_names);
  if (!assets_msg_load_file(&s_names, "COLONIZE/NAMES.TXT")) {
    return TR_SKIP;
  }
  memset(&s_pool, 0, sizeof(s_pool));
  CHECK(units_load_types(&s_pool, &s_names));
  memset(&s_map, 0, sizeof(s_map));
  char err[128];
  CHECK(map_alloc(&s_map, 12, 12, err, sizeof(err)));
  /* 12x12 plains, one continent; nation 0 has seen only the 5x5 corner. */
  for (int y = 0; y < 12; ++y) {
    for (int x = 0; x < 12; ++x) {
      s_map.terrain[y * 12 + x] = 2;
      s_map.seen[y * 12 + x] = (x < 5 && y < 5) ? 0x10 : 0;
      s_map.layer3[y * 12 + x] = 0xf1;
      (void)map_clear_rumour(&s_map, x, y);
    }
  }
  colonies_init(&s_colonies);
  colonies_set_occupancy_map(NULL);
  const ColonizeWorld w = {.units = &s_pool, .colonies = &s_colonies, .map = &s_map};
  const int sid = units_spawn(&s_pool, units_kind_type_index(&s_pool, UNITS_KIND_SCOUT), 1, 1);
  ColonizeUnit* u = units_get(&s_pool, sid);
  CHECK(u && explore_auto_is_explorer(&s_pool, u) && !units_is_sea(&s_pool, sid));
  u->nation_id = 0;
  u->moves = units_max_mp(&s_pool, sid);

  /* Nearest seen tile next to unexplored ground. */
  CHECK(explore_auto_automate(&w, u));
  CHECK(u->automated && u->orders == UNITS_ORDER_GOTO);
  CHECK(u->goto_x == 4 || u->goto_y == 4);
  const int gx = u->goto_x;
  const int gy = u->goto_y;

  /* A foreign unit (hostile: no treaty data) next to the target moves the target away. */
  const int fid = units_spawn(&s_pool, units_kind_type_index(&s_pool, UNITS_KIND_SOLDIER), gx, gy == 0 ? 1 : gy - 1);
  units_get(&s_pool, fid)->nation_id = 1;
  u->orders = UNITS_ORDER_NONE;
  explore_auto_step(&w, u);
  CHECK(u->automated && u->orders == UNITS_ORDER_GOTO);
  CHECK(map_dos_dist(u->goto_x - units_get(&s_pool, fid)->x, u->goto_y - units_get(&s_pool, fid)->y) > 1);

  /* Pioneer Automate leaves an automated Scout alone. */
  CHECK(!pioneer_plan_auto_step(&w, u, NULL, NULL));
  CHECK(u->automated);

  /* SAUT round trip keeps the automated Scout and the visited settlement. */
  explore_auto_note_village(0, 7, 8);
  size_t n = 0;
  uint8_t* blob = explore_auto_serialize(&s_pool, &n);
  CHECK(blob && n > 0);
  u->automated = false;
  explore_auto_deserialize(blob, n, &s_pool);
  CHECK(u->automated);
  explore_auto_reset();
  explore_auto_deserialize(blob, n, &s_pool);
  free(blob);
  size_t n2 = 0;
  uint8_t* again = explore_auto_serialize(&s_pool, &n2);
  CHECK(again && n2 == n);
  free(again);

  /* Everything seen: nothing to explore cancels. */
  for (int i = 0; i < 144; ++i) {
    s_map.seen[i] = 0x10;
  }
  u->orders = UNITS_ORDER_NONE;
  explore_auto_step(&w, u);
  CHECK(!u->automated && u->orders == UNITS_ORDER_NONE);

  /* Off: no SAUT chunk. */
  s.scout_autoexplore = false;
  settings_set(&s);
  u->automated = true;
  CHECK(!explore_auto_serialize(&s_pool, &n) && n == 0);
  map_free(&s_map);
  assets_msg_free(&s_names);
  return 0;
}

/* 12x12 ocean (region 1); nation 0 has seen the 5x5 corner. A ship explores
 * the unseen ocean, and stops once only unseen lake water is left. */
static int case_ship(void) {
  ColonizeSettings s;
  settings_defaults(&s);
  s.ship_autoexplore = true;
  settings_set(&s);
  explore_auto_reset();
  assets_msg_init(&s_names);
  if (!assets_msg_load_file(&s_names, "COLONIZE/NAMES.TXT")) {
    return TR_SKIP;
  }
  memset(&s_pool, 0, sizeof(s_pool));
  CHECK(units_load_types(&s_pool, &s_names));
  memset(&s_map, 0, sizeof(s_map));
  char err[128];
  CHECK(map_alloc(&s_map, 12, 12, err, sizeof(err)));
  for (int y = 0; y < 12; ++y) {
    for (int x = 0; x < 12; ++x) {
      s_map.terrain[y * 12 + x] = 25;
      s_map.seen[y * 12 + x] = (x < 5 && y < 5) ? 0x10 : 0;
      s_map.layer3[y * 12 + x] = 0xf1;
    }
  }
  colonies_init(&s_colonies);
  colonies_set_occupancy_map(NULL);
  const ColonizeWorld w = {.units = &s_pool, .colonies = &s_colonies, .map = &s_map};
  const int sid = units_spawn(&s_pool, units_kind_type_index(&s_pool, UNITS_KIND_CARAVEL), 1, 1);
  ColonizeUnit* u = units_get(&s_pool, sid);
  CHECK(u && units_is_sea(&s_pool, sid));
  u->nation_id = 0;
  u->moves = units_max_mp(&s_pool, sid);
  CHECK(explore_auto_automate(&w, u));
  CHECK(u->automated && u->orders == UNITS_ORDER_GOTO);
  CHECK(u->goto_x == 4 || u->goto_y == 4);

  /* SAUT keeps the ship as a ship. */
  size_t n = 0;
  uint8_t* blob = explore_auto_serialize(&s_pool, &n);
  CHECK(blob && n > 0);
  u->automated = false;
  explore_auto_deserialize(blob, n, &s_pool);
  free(blob);
  CHECK(u->automated);

  /* Only the never-revealed map rim left unseen: nothing to explore. */
  for (int y = 0; y < 12; ++y) {
    for (int x = 0; x < 12; ++x) {
      const bool rim = x == 0 || y == 0 || x == 11 || y == 11;
      s_map.seen[y * 12 + x] = rim ? 0 : 0x10;
    }
  }
  u->orders = UNITS_ORDER_NONE;
  explore_auto_step(&w, u);
  CHECK(!u->automated && u->orders == UNITS_ORDER_NONE);
  for (int i = 0; i < 144; ++i) {
    s_map.seen[i] = (i % 12 < 5 && i / 12 < 5) ? 0x10 : 0;
  }
  u->automated = true;

  /* Only lake water left unseen: nothing to explore, automation ends. */
  for (int y = 0; y < 12; ++y) {
    for (int x = 0; x < 12; ++x) {
      if (!(x < 5 && y < 5)) {
        s_map.layer3[y * 12 + x] = 0xf2;
      }
    }
  }
  u->orders = UNITS_ORDER_NONE;
  explore_auto_step(&w, u);
  CHECK(!u->automated && u->orders == UNITS_ORDER_NONE);

  /* Most fog per tile sailed: from (5,5), one unseen tile at (5,2) (frontier at
   * distance 2) loses to a 3x3 unseen block at x 8..10, y 7..9 (also distance 2). */
  for (int i = 0; i < 144; ++i) {
    const int x = i % 12;
    const int y = i / 12;
    s_map.layer3[i] = 0xf1;
    s_map.seen[i] = ((x == 5 && y == 2) || (x >= 8 && x <= 10 && y >= 7 && y <= 9)) ? 0 : 0x10;
  }
  u->x = 5;
  u->y = 5;
  u->automated = true;
  u->orders = UNITS_ORDER_NONE;
  explore_auto_step(&w, u);
  CHECK(u->automated && u->orders == UNITS_ORDER_GOTO && u->goto_y >= 6);

  /* A ship already on the sea lane: Go To refuses every lane tile further east
   * (units_enter_probe_w reason 5). Those are skipped, not counted as path misses
   * (campaign 14: 8 of them cancelled the caravel), so the ship heads for the one
   * ordinary-ocean frontier tile (7,10) even though nine lane tiles score better. */
  for (int i = 0; i < 144; ++i) {
    const int x = i % 12;
    s_map.terrain[i] = x >= 1 ? T_HIGH_SEAS : T_OCEAN;
    s_map.seen[i] = x < 8 ? 0x10 : 0;
  }
  s_map.terrain[10 * 12 + 7] = T_OCEAN;
  u->x = 2;
  u->y = 1;
  u->automated = true;
  u->orders = UNITS_ORDER_NONE;
  explore_auto_step(&w, u);
  CHECK(u->automated && u->orders == UNITS_ORDER_GOTO && u->goto_x == 7 && u->goto_y == 10);

  /* A Scout setting does not automate ships. */
  s.ship_autoexplore = false;
  s.scout_autoexplore = true;
  settings_set(&s);
  CHECK(!explore_auto_automate(&w, u));
  map_free(&s_map);
  assets_msg_free(&s_names);
  return 0;
}

static const TestCase k_cases[] = {
  {"scout", case_scout},
  {"ship", case_ship},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
