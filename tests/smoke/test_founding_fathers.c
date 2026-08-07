/* Smoke: liberty-bell threshold elects an unclaimed founding father. */
#include "core/col1_save.h"
#include "core/founding_fathers.h"
#include "core/turn.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_founding_fathers: FAIL %s\n", msg);
  return 1;
}

static void seed_unclaimed(ColonizeCol1Save* col1) {
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1->head.founding_father[i] = -1;
  }
}

int main(void) {
  if (founding_fathers_bells_needed(0) != 40u ||
      founding_fathers_bells_needed(1) != 80u ||
      founding_fathers_bells_needed(2) != 120u) {
    return fail("bells_needed curve");
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  seed_unclaimed(&col1);

  ColonizeCol1Nation* nat = &col1.nation[0];
  memset(nat, 0, sizeof(*nat));
  nat->liberty_bells_total = 40;
  nat->next_founding_father = 0; /* Adam Smith */
  nat->founding_father_count = 0;
  nat->gold = 100;
  nat->current_crosses = 0;

  char status[128];
  status[0] = '\0';

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.status = status;
  ctx.status_size = sizeof(status);

  /* Below threshold: no elect. */
  nat->liberty_bells_total = 39;
  founding_fathers_tick(&ctx);
  if (nat->founding_father_count != 0 || col1.head.founding_father[0] != -1) {
    return fail("no elect below threshold");
  }

  /* At threshold: elect next_founding_father (Adam Smith = 0). */
  nat->liberty_bells_total = 40;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[0] != 0) {
    return fail("head.founding_father[0] not human");
  }
  if (nat->founding_father_count != 1) {
    return fail("founding_father_count not 1");
  }
  if ((nat->founding_fathers[0] & 1u) == 0) {
    return fail("bitmask bit 0 unset");
  }
  if (nat->next_founding_father != 1) {
    return fail("next_founding_father not advanced to 1");
  }
  if (strstr(status, "Founding Father elected") == NULL) {
    return fail("status line missing");
  }
  /* Bells gated, not spent. */
  if (nat->liberty_bells_total != 40) {
    return fail("bells were spent (expected gate-only)");
  }

  /* Second elect needs 80; Jakob Fugger (1) tiny gold effect. */
  nat->liberty_bells_total = 80;
  const uint32_t gold_before = nat->gold;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[1] != 0 || nat->founding_father_count != 2) {
    return fail("second FF not Jakob Fugger");
  }
  if (nat->gold != gold_before + 50u) {
    return fail("Jakob Fugger gold +50 missing");
  }
  if (nat->next_founding_father != 2) {
    return fail("next after Fugger not 2");
  }

  /* Prefer next_founding_father when still unclaimed. */
  nat->liberty_bells_total = 120;
  nat->next_founding_father = 20; /* William Brewster */
  const uint16_t crosses_before = nat->current_crosses;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[20] != 0 || nat->founding_father_count != 3) {
    return fail("Brewster not elected via next");
  }
  if (nat->current_crosses != (uint16_t)(crosses_before + 8u)) {
    return fail("Brewster crosses +8 missing");
  }

  /* Force Jefferson (15): liberty bells +15. */
  nat->liberty_bells_total = 160;
  nat->next_founding_father = 15;
  const uint16_t bells_before = nat->liberty_bells_total;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[15] != 0 || nat->founding_father_count != 4) {
    return fail("Jefferson not elected via next");
  }
  if (nat->liberty_bells_total != (uint16_t)(bells_before + 15u)) {
    return fail("Jefferson bells +15 missing");
  }

  /* Force Jan de Witt (4): tax_rate -1. */
  nat->tax_rate = 12;
  nat->liberty_bells_total = 200;
  nat->next_founding_father = 4;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[4] != 0 || nat->founding_father_count != 5) {
    return fail("de Witt not elected via next");
  }
  if (nat->tax_rate != 11) {
    return fail("de Witt tax -1 missing");
  }

  /* Force Washington (11): REF regulars -1. */
  col1.head.expeditionary_force[0] = 5;
  nat->liberty_bells_total = 240;
  nat->next_founding_father = 11;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[11] != 0 || nat->founding_father_count != 6) {
    return fail("Washington not elected via next");
  }
  if (col1.head.expeditionary_force[0] != 4) {
    return fail("Washington REF regulars -1 missing");
  }

  printf("smoke_founding_fathers: OK\n");
  return 0;
}
