/*
 * ai_improvements mod (docs/ai_euro_improvement_spec.md): the pioneer job list
 * and its act (ai_euro_improved.c).
 */
#include "core/ai_euro_internal.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/colony_governor.h"
#include "core/map.h"
#include "core/pioneer_plan.h"
#include "core/units.h"
#include "platform/diagnostics.h"

#include <stdio.h>
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
  pu->orders = 0;
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
static const TestCase k_cases[] = {
  {"colony_focus_small_cash", case_colony_focus_small_cash},
  {"pjob_resume", case_pjob_resume},
  {"pioneer_released", case_pioneer_released},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
