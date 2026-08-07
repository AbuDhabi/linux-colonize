#include "core/founding_fathers.h"

#include <stdint.h>
#include <stdio.h>

/*
 * Rough structural FF election (FUN_4345_0a22 / 0982 / 0342 stand-ins).
 * head.founding_father[i]: -1 unclaimed; 0..3 = owning European nation.
 * nation.founding_fathers[4]: bit i set when nation elected FF i.
 *
 * Jakob Fugger (1) boycott forgive stand-in: gold+50 plus clear Sugar
 * boycott bit (1<<1) and Furs embargo bit (1<<4) on boycott_bitmap;
 * for the human nation also clear head.unknown46[2] (king tax-refuse).
 */

/* King tax-refuse stand-in byte (ai_king unknown46[2]). */
#define FF_KING_BOYCOTT_BYTE 2
/* Sugar cargo idx 1; Furs cargo idx 4 — same bits as king refuse / diplo embargo. */
#define FF_BOYCOTT_SUGAR_BIT (1u << 1)
#define FF_BOYCOTT_FURS_BIT (1u << 4)
#define FF_FUGGER_BOYCOTT_MASK (FF_BOYCOTT_SUGAR_BIT | FF_BOYCOTT_FURS_BIT)

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

static void bump_gold(ColonizeCol1Nation* nat, EuropeScreen* europe, uint32_t amount) {
  if (nat->gold < 0xffffffffu - amount) {
    nat->gold += amount;
  } else {
    nat->gold = 0xffffffffu;
  }
  if (europe) {
    europe->gold = (int)nat->gold;
  }
}

static void bump_crosses(ColonizeCol1Nation* nat, EuropeScreen* europe, uint16_t amount) {
  if (nat->current_crosses < 65535u - amount) {
    nat->current_crosses = (uint16_t)(nat->current_crosses + amount);
  } else {
    nat->current_crosses = 65535u;
  }
  if (europe) {
    europe->current_crosses = nat->current_crosses;
  }
}

static void bump_bells(ColonizeCol1Nation* nat, EuropeScreen* europe, uint16_t amount) {
  if (nat->liberty_bells_total < 65535u - amount) {
    nat->liberty_bells_total = (uint16_t)(nat->liberty_bells_total + amount);
  } else {
    nat->liberty_bells_total = 65535u;
  }
  if (europe) {
    europe->liberty_bells_total = nat->liberty_bells_total;
  }
}

static void cut_tax(ColonizeCol1Nation* nat, EuropeScreen* europe, uint8_t amount) {
  if (nat->tax_rate > amount) {
    nat->tax_rate = (uint8_t)(nat->tax_rate - amount);
  } else {
    nat->tax_rate = 0;
  }
  if (europe) {
    europe->tax_percent = nat->tax_rate;
  }
}

/* Tiny stand-ins only — full wiki/decomp effect table PARKED. */
static void apply_tiny_effect(
  ColonizeCol1Save* col1,
  ColonizeCol1Nation* nat,
  EuropeScreen* europe,
  int nation_id,
  int human_nation,
  int ff_index
) {
  switch (ff_index) {
    case 0: /* Adam Smith — factory / industry stand-in */
      bump_gold(nat, europe, 25u);
      break;
    case 1: /* Jakob Fugger — boycott forgive stand-in */
      bump_gold(nat, europe, 50u);
      nat->boycott_bitmap =
        (uint16_t)(nat->boycott_bitmap & (uint16_t)~FF_FUGGER_BOYCOTT_MASK);
      if (nation_id == human_nation && col1) {
        col1->head.unknown46[FF_KING_BOYCOTT_BYTE] = 0;
      }
      break;
    case 2: /* Peter Minuit — cheap land purchase stand-in */
      bump_gold(nat, europe, 30u);
      break;
    case 4: /* Jan de Witt — trade / finance stand-in */
      cut_tax(nat, europe, 1u);
      break;
    case 10: /* Hernan Cortes — conquest plunder stand-in */
      bump_gold(nat, europe, 100u);
      break;
    case 11: /* George Washington — veteran / REF pressure stand-in */
      if (col1 && col1->head.expeditionary_force[0] > 0) {
        col1->head.expeditionary_force[0]--;
      }
      break;
    case 15: /* Thomas Jefferson — liberty bells stand-in */
      bump_bells(nat, europe, 15u);
      break;
    case 17: /* Thomas Paine — tax-weighted bells stand-in */
      bump_bells(nat, europe, (uint16_t)nat->tax_rate);
      break;
    case 18: /* Simon Bolivar — SoL / bells stand-in */
      bump_bells(nat, europe, 20u);
      break;
    case 19: /* Benjamin Franklin — diplomacy / tax ease stand-in */
      cut_tax(nat, europe, 2u);
      break;
    case 20: /* William Brewster — immigration help stand-in */
      bump_crosses(nat, europe, 8u);
      break;
    case 21: /* William Penn — crosses / goodwill stand-in */
      bump_crosses(nat, europe, 5u);
      break;
    default:
      break;
  }
}

/* Returns true if a founding father was elected for this nation. */
static bool try_elect_nation(
  ColonizeCol1Save* col1,
  int nation_id,
  int human_nation,
  EuropeScreen* europe,
  char* status,
  size_t status_size
) {
  if (!col1 || nation_id < 0 || nation_id >= (int)COLONIZE_COL1_NATION_COUNT) {
    return false;
  }

  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  const unsigned needed = founding_fathers_bells_needed(nat->founding_father_count);
  if ((unsigned)nat->liberty_bells_total < needed) {
    return false;
  }

  const int idx = pick_candidate(col1, nat);
  if (idx < 0) {
    return false;
  }

  col1->head.founding_father[idx] = (int8_t)nation_id;
  nat->founding_fathers[idx / 8] |= (uint8_t)(1u << (idx % 8));
  if (nat->founding_father_count < 65535u) {
    nat->founding_father_count++;
  }
  nat->next_founding_father = advance_next_candidate(col1, idx);

  apply_tiny_effect(col1, nat, europe, nation_id, human_nation, idx);

  if (status && status_size > 0 && nation_id == human_nation) {
    snprintf(status, status_size, "Founding Father elected (#%d)", idx);
  }
  return true;
}

void founding_fathers_tick(ColonizeTurnContext* ctx) {
  if (!ctx || !ctx->col1_ok || !ctx->col1) {
    return;
  }
  if (ctx->human_nation < 0 || ctx->human_nation >= (int)COLONIZE_COL1_NATION_COUNT) {
    return;
  }

  ColonizeCol1Save* col1 = ctx->col1;

  /* Human first (one elect max). */
  try_elect_nation(
    col1,
    ctx->human_nation,
    ctx->human_nation,
    ctx->europe,
    ctx->status,
    ctx->status_size
  );

  /* Then each AI Euro nation (control==1), one elect each max. */
  for (int n = 0; n < (int)COLONIZE_COL1_NATION_COUNT; ++n) {
    if (n == ctx->human_nation) {
      continue;
    }
    if (col1->player[n].control != 1) {
      continue;
    }
    try_elect_nation(col1, n, ctx->human_nation, NULL, NULL, 0);
  }
}
