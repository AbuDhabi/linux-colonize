/* Smoke: bilateral 15b3 diplo bytes, break_alliance, timers, Indian delta. */
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/turn.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_diplo: FAIL %s\n", msg);
  return 1;
}

int main(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  memset(col1.nation, 0, sizeof(col1.nation));
  memset(col1.head.nation_relation, 0, sizeof(col1.head.nation_relation));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    col1.player[i].diplomacy = 0;
  }

  /* Pair independence: war(0,1) must not force war(0,2). */
  ai_diplo_declare_war(&col1, 0, 1);
  if (!ai_diplo_at_war(&col1, 0, 1) || !ai_diplo_at_war(&col1, 1, 0)) {
    return fail("declare_war should be symmetric for pair 0-1");
  }
  if (ai_diplo_at_war(&col1, 0, 2) || ai_diplo_at_war(&col1, 2, 0)) {
    return fail("war(0,1) must not set war(0,2)");
  }
  if (ai_diplo_read(&col1, 0, 1) & AI_DIPLO_PEACE) {
    return fail("at-war pair should not keep PEACE");
  }

  /* Ally then break → peace, not war. */
  ai_diplo_form_alliance(&col1, 2, 3);
  if ((ai_diplo_read(&col1, 2, 3) & AI_DIPLO_ALLY) == 0) {
    return fail("form_alliance should set ALLY");
  }
  ai_diplo_break_alliance(&col1, 2, 3);
  if (ai_diplo_read(&col1, 2, 3) & AI_DIPLO_ALLY) {
    return fail("break_alliance should clear ALLY");
  }
  if (ai_diplo_at_war(&col1, 2, 3)) {
    return fail("break_alliance should not declare war");
  }
  if ((ai_diplo_read(&col1, 2, 3) & AI_DIPLO_PEACE) == 0) {
    return fail("break_alliance should leave PEACE");
  }

  /* Timer decrement + ally break on expiry. */
  ai_diplo_form_alliance(&col1, 0, 2);
  col1.nation[0].unknown26[2] = 1; /* timer toward peer 2 */
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 1);
  uint32_t turn = 1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.turn_number = &turn;
  ai_diplo_treaty_timers(&ctx, 0);
  if (col1.nation[0].unknown26[2] != 0) {
    return fail("timer should decrement to 0");
  }
  if (ai_diplo_read(&col1, 0, 2) & AI_DIPLO_ALLY) {
    return fail("timer expiry should break alliance");
  }

  /* Indian relation delta clamps. */
  col1.nation[0].relation_by_indian[0] = 250;
  ai_diplo_indian_relation_delta(&col1, 4, 0, 20);
  if (col1.nation[0].relation_by_indian[0] != 255) {
    return fail("indian delta should clamp at 255");
  }
  col1.nation[0].relation_by_indian[0] = 5;
  ai_diplo_indian_relation_delta(&col1, 4, 0, -20);
  if (col1.nation[0].relation_by_indian[0] != 0) {
    return fail("indian delta should clamp at 0");
  }

  fprintf(stderr, "smoke_ai_diplo: ok\n");
  return 0;
}
