/*
 * ai_improvements mod (docs/ai_euro_improvement_spec.md): the pioneer job list
 * and its act (ai_euro_improved.c).
 */
#include "core/ai_euro_internal.h"
#include "core/assets.h"
#include "core/colony.h"
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
static const TestCase k_cases[] = {
  {"pjob_resume", case_pjob_resume},
};
TEST_MAIN_WRAPPED(k_cases, diag_init(0, NULL), diag_shutdown())
