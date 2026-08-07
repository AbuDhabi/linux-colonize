/* Smoke: bilateral 15b3 diplo bytes, war gold/tax sting, upkeep, ally cost,
 * Indian drift, break, timers. */
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

  /* Pair independence: war(0,1) must not force war(0,2).
   * Thin 153e: first declare drains 100 gold + bumps tax_rate both sides.
   * Indians dislike war: −5 on relation_by_indian[0..7] both sides. */
  col1.nation[0].gold = 250;
  col1.nation[1].gold = 80;
  col1.nation[2].gold = 500;
  col1.nation[0].tax_rate = 10;
  col1.nation[1].tax_rate = 74;
  col1.nation[2].tax_rate = 20;
  col1.nation[3].tax_rate = 75; /* cap probe via separate declare later */
  for (int i = 0; i < 8; ++i) {
    col1.nation[0].relation_by_indian[i] = 50;
    col1.nation[1].relation_by_indian[i] = 40;
    col1.nation[2].relation_by_indian[i] = 100; /* untouched by war(0,1) */
  }
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
  if (col1.nation[0].gold != 150) {
    return fail("declare_war should drain 100 gold from nation 0");
  }
  if (col1.nation[1].gold != 0) {
    return fail("declare_war gold sting should floor at 0");
  }
  if (col1.nation[2].gold != 500) {
    return fail("war(0,1) must not drain gold of nation 2");
  }
  if (col1.nation[0].tax_rate != 11) {
    return fail("declare_war should bump tax_rate +1 on nation 0");
  }
  if (col1.nation[1].tax_rate != 75) {
    return fail("declare_war tax bump should cap at 75");
  }
  if (col1.nation[2].tax_rate != 20) {
    return fail("war(0,1) must not bump tax of nation 2");
  }
  for (int i = 0; i < 8; ++i) {
    if (col1.nation[0].relation_by_indian[i] != 45) {
      return fail("declare_war should −5 Indian relations for nation 0");
    }
    if (col1.nation[1].relation_by_indian[i] != 35) {
      return fail("declare_war should −5 Indian relations for nation 1");
    }
    if (col1.nation[2].relation_by_indian[i] != 100) {
      return fail("war(0,1) must not change Indian relations of nation 2");
    }
  }
  /* Re-declare: no second sting / tax bump / Indian hit. */
  ai_diplo_declare_war(&col1, 0, 1);
  if (col1.nation[0].gold != 150) {
    return fail("re-declare_war should not re-sting gold");
  }
  if (col1.nation[0].tax_rate != 11) {
    return fail("re-declare_war should not re-bump tax");
  }
  if (col1.nation[0].relation_by_indian[0] != 45) {
    return fail("re-declare_war should not re-hit Indian relations");
  }

  /* euro_balance at-war upkeep (before timer pass can PEACE-tweak zero timers). */
  {
    ColonizeDosRng rng_up;
    dos_rng_seed(&rng_up, 1);
    uint32_t turn_up = 1;
    ColonizeTurnContext ctx_up;
    memset(&ctx_up, 0, sizeof(ctx_up));
    ctx_up.col1 = &col1;
    ctx_up.col1_ok = true;
    ctx_up.rng = &rng_up;
    ctx_up.turn_number = &turn_up;
    col1.nation[0].gold = 40;
    ai_diplo_euro_balance(&ctx_up, 0);
    if (col1.nation[0].gold != 35) {
      return fail("euro_balance at-war should drain 5 gold upkeep");
    }
    col1.nation[0].gold = 3;
    ai_diplo_euro_balance(&ctx_up, 0);
    if (col1.nation[0].gold != 0) {
      return fail("euro_balance upkeep should floor gold at 0");
    }
    ai_diplo_euro_balance(&ctx_up, 0);
    if (col1.nation[0].gold != 0) {
      return fail("euro_balance upkeep should no-op when gold already 0");
    }
    if (col1.nation[2].gold != 500) {
      return fail("euro_balance upkeep must not drain peaceful peer treasury");
    }
  }

  /* Ally treasury cost: 25 gold each side (floor 0). */
  col1.nation[2].gold = 100;
  col1.nation[3].gold = 10;
  ai_diplo_form_alliance(&col1, 2, 3);
  if ((ai_diplo_read(&col1, 2, 3) & AI_DIPLO_ALLY) == 0) {
    return fail("form_alliance should set ALLY");
  }
  if (col1.nation[2].gold != 75) {
    return fail("form_alliance should drain 25 gold from nation 2");
  }
  if (col1.nation[3].gold != 0) {
    return fail("form_alliance gold cost should floor at 0");
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
  col1.nation[0].gold = 50;
  col1.nation[2].gold = 50;
  ai_diplo_form_alliance(&col1, 0, 2);
  if (col1.nation[0].gold != 25 || col1.nation[2].gold != 25) {
    return fail("form_alliance(0,2) should drain 25 gold each");
  }
  col1.nation[0].unknown26[2] = 1; /* timer toward peer 2 */
  /* Keep other peer timers non-zero so expiry does not PEACE-tweak war(0,1). */
  col1.nation[0].unknown26[1] = 5;
  col1.nation[0].unknown26[3] = 5;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 1);
  uint32_t turn = 1;
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.turn_number = &turn;
  const uint8_t rel0_at_war = col1.nation[0].relation_by_indian[0];
  if (!ai_diplo_at_war(&col1, 0, 1)) {
    return fail("precondition: nation 0 should still be at war with 1");
  }
  ai_diplo_treaty_timers(&ctx, 0);
  if (col1.nation[0].unknown26[2] != 0) {
    return fail("timer should decrement to 0");
  }
  if (ai_diplo_read(&col1, 0, 2) & AI_DIPLO_ALLY) {
    return fail("timer expiry should break alliance");
  }
  if (!ai_diplo_at_war(&col1, 0, 1)) {
    return fail("timer pass must not clear war(0,1) when peer-1 timer live");
  }
  if (col1.nation[0].relation_by_indian[0] != rel0_at_war) {
    return fail("treaty_timers must not drift Indian relations while at war");
  }

  /* Peaceful Indian drift: +1 per slot under 160, cap 160; skip if at war. */
  {
    ColonizeDosRng rng_d;
    dos_rng_seed(&rng_d, 2);
    uint32_t turn_d = 2;
    ColonizeTurnContext ctx_d;
    memset(&ctx_d, 0, sizeof(ctx_d));
    ctx_d.col1 = &col1;
    ctx_d.col1_ok = true;
    ctx_d.rng = &rng_d;
    ctx_d.turn_number = &turn_d;
    /* Nation 3 is peaceful (no war with anyone after break of 2-3 ally). */
    for (int i = 0; i < 8; ++i) {
      col1.nation[3].relation_by_indian[i] = (uint8_t)(100 + i);
    }
    col1.nation[3].relation_by_indian[7] = 160; /* already at cap */
    ai_diplo_treaty_timers(&ctx_d, 3);
    for (int i = 0; i < 7; ++i) {
      if (col1.nation[3].relation_by_indian[i] != (uint8_t)(101 + i)) {
        return fail("treaty_timers should +1 peaceful Indian relations under 160");
      }
    }
    if (col1.nation[3].relation_by_indian[7] != 160) {
      return fail("Indian drift should not raise relations above 160");
    }
    /* Second tick: still under cap for slots 0..6. */
    ai_diplo_treaty_timers(&ctx_d, 3);
    if (col1.nation[3].relation_by_indian[0] != 102) {
      return fail("Indian drift should apply +1 each treaty_timers tick");
    }
  }

  /* tax_rate already at cap stays put on first declare. */
  col1.nation[2].gold = 200;
  col1.nation[3].gold = 200;
  col1.nation[2].tax_rate = 75;
  col1.nation[3].tax_rate = 75;
  ai_diplo_declare_war(&col1, 2, 3);
  if (col1.nation[2].tax_rate != 75 || col1.nation[3].tax_rate != 75) {
    return fail("declare_war must not raise tax_rate above 75");
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
