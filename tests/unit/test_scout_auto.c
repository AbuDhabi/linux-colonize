/* divergences.scout_autoexplore: target choice, hostile avoidance, SAUT round trip. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_runner.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/pioneer_plan.h"
#include "core/scout_auto.h"
#include "core/settings.h"
#include "core/units.h"
#include "core/units_move.h"
#include "platform/diagnostics.h"

#define CHECK(c)                                                  \
  do {                                                            \
    if (!(c)) {                                                   \
      fprintf(stderr, "scout_auto: %s:%d %s\n", __FILE__, __LINE__, #c); \
      return 1;                                                   \
    }                                                             \
  } while (0)

static ColonizeMsgCatalog s_names;
static ColonizeUnitPool s_pool;
static ColonizeWorldMap s_map;
static ColonizeColonyPool s_colonies;

static int case_automate(void) {
  ColonizeSettings s;
  settings_defaults(&s);
  s.scout_autoexplore = true;
  settings_set(&s);
  scout_auto_reset();
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
  CHECK(u && scout_auto_is_scout(&s_pool, u));
  u->nation_id = 0;
  u->moves = units_max_mp(&s_pool, sid);

  /* Nearest seen tile next to unexplored ground. */
  CHECK(scout_auto_automate(&w, u));
  CHECK(u->automated && u->orders == UNITS_ORDER_GOTO);
  CHECK(u->goto_x == 4 || u->goto_y == 4);
  const int gx = u->goto_x;
  const int gy = u->goto_y;

  /* A foreign unit (hostile: no treaty data) next to the target moves the target away. */
  const int fid = units_spawn(&s_pool, units_kind_type_index(&s_pool, UNITS_KIND_SOLDIER), gx, gy == 0 ? 1 : gy - 1);
  units_get(&s_pool, fid)->nation_id = 1;
  u->orders = UNITS_ORDER_NONE;
  scout_auto_step(&w, u);
  CHECK(u->automated && u->orders == UNITS_ORDER_GOTO);
  CHECK(map_dos_dist(u->goto_x - units_get(&s_pool, fid)->x, u->goto_y - units_get(&s_pool, fid)->y) > 1);

  /* Pioneer Automate leaves an automated Scout alone. */
  CHECK(!pioneer_plan_auto_step(&w, u, NULL, NULL));
  CHECK(u->automated);

  /* SAUT round trip keeps the automated Scout and the visited settlement. */
  scout_auto_note_village(0, 7, 8);
  size_t n = 0;
  uint8_t* blob = scout_auto_serialize(&s_pool, &n);
  CHECK(blob && n > 0);
  u->automated = false;
  scout_auto_deserialize(blob, n, &s_pool);
  CHECK(u->automated);
  scout_auto_reset();
  scout_auto_deserialize(blob, n, &s_pool);
  free(blob);
  size_t n2 = 0;
  uint8_t* again = scout_auto_serialize(&s_pool, &n2);
  CHECK(again && n2 == n);
  free(again);

  /* Everything seen: nothing to explore cancels. */
  for (int i = 0; i < 144; ++i) {
    s_map.seen[i] = 0x10;
  }
  u->orders = UNITS_ORDER_NONE;
  scout_auto_step(&w, u);
  CHECK(!u->automated && u->orders == UNITS_ORDER_NONE);

  /* Off: no SAUT chunk. */
  s.scout_autoexplore = false;
  settings_set(&s);
  u->automated = true;
  CHECK(!scout_auto_serialize(&s_pool, &n) && n == 0);
  map_free(&s_map);
  assets_msg_free(&s_names);
  return 0;
}

static const TestCase k_cases[] = {
  {"automate", case_automate},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
