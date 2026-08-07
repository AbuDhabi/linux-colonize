#include "core/founding_fathers.h"

#include <stdint.h>
#include <stdio.h>

/*
 * Rough structural FF election (FUN_4345_0a22 / 0982 / 0342 stand-ins).
 * head.founding_father[i]: -1 unclaimed; 0..3 = owning European nation.
 * nation.founding_fathers[4]: bit i set when nation elected FF i.
 */

unsigned founding_fathers_bells_needed(unsigned elected_count) {
  /* First at 40, second at 80, … — linear stand-in for FUN_4345_0982. */
  return 40u * (elected_count + 1u);
}

static bool ff_unclaimed(const ColonizeCol1Save* col1, int idx) {
  return col1->head.founding_father[idx] < 0;
}

static int pick_candidate(const ColonizeCol1Save* col1, const ColonizeCol1Nation* nat) {
  const int next = (int)nat->next_founding_father;
  if (next >= 0 && next < (int)COLONIZE_COL1_FF_COUNT && ff_unclaimed(col1, next)) {
    return next;
  }
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (ff_unclaimed(col1, i)) {
      return i;
    }
  }
  return -1;
}

static int16_t advance_next_candidate(const ColonizeCol1Save* col1, int elected_idx) {
  for (int i = elected_idx + 1; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    if (ff_unclaimed(col1, i)) {
      return (int16_t)i;
    }
  }
  for (int i = 0; i < elected_idx; ++i) {
    if (ff_unclaimed(col1, i)) {
      return (int16_t)i;
    }
  }
  return -1;
}

/* Tiny stand-ins only — full wiki/decomp effect table PARKED. */
static void apply_tiny_effect(ColonizeCol1Nation* nat, EuropeScreen* europe, int ff_index) {
  switch (ff_index) {
    case 1: /* Jakob Fugger — boycott forgive stand-in */
      if (nat->gold < 0xffffffffu - 50u) {
        nat->gold += 50u;
      }
      break;
    case 20: /* William Brewster — immigration help stand-in */
      if (nat->current_crosses < 65535u - 8u) {
        nat->current_crosses = (uint16_t)(nat->current_crosses + 8u);
      } else {
        nat->current_crosses = 65535u;
      }
      if (europe) {
        europe->current_crosses = nat->current_crosses;
      }
      break;
    default:
      break;
  }
}

void founding_fathers_tick(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (ctx->human_nation < 0 || ctx->human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }

  ColonizeCol1Save* col1 = ctx->col1;
  ColonizeCol1Nation* nat = &col1->nation[ctx->human_nation];
  const unsigned needed = founding_fathers_bells_needed(nat->founding_father_count);
  if ((unsigned)nat->liberty_bells_total < needed) {
    return;
  }

  const int idx = pick_candidate(col1, nat);
  if (idx < 0) {
    return;
  }

  col1->head.founding_father[idx] = (int8_t)ctx->human_nation;
  nat->founding_fathers[idx / 8] |= (uint8_t)(1u << (idx % 8));
  if (nat->founding_father_count < 65535u) {
    nat->founding_father_count++;
  }
  nat->next_founding_father = advance_next_candidate(col1, idx);

  apply_tiny_effect(nat, ctx->europe, idx);

  if (ctx->status && ctx->status_size > 0) {
    snprintf(
      ctx->status,
      ctx->status_size,
      "Founding Father elected (#%d)",
      idx
    );
  }
}
