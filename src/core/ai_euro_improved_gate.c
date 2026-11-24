/*
 * MOD_AI_IMPROVEMENTS nation gate (docs/ai_euro_improvement_spec.md), kept
 * apart from ai_euro_improved.c so the slim test targets, which link the
 * production code but not the AI, can link it.
 */
#include "core/ai_euro.h"
#include "core/ai_king.h"
#include "core/col1_save.h"
#include "core/mods.h"

#include <stdint.h>
#include <string.h>

bool ai_euro_improved_nation(const ColonizeCol1Save* col1, int human_nation, int nation) {
  if (!mods_on(MOD_AI_IMPROVEMENTS) || nation < 0 || nation > 3 || nation == human_nation) {
    return false;
  }
  if (col1) {
    if (col1->player[nation].control == 0) {
      return false;
    }
    if (col1->head.game_options.woi != 0 && nation == ai_king_crown_nation_col1(col1, human_nation)) {
      return false;
    }
  }
  return true;
}

/* MOD_BUILDING_UPKEEP + MOD_AI_IMPROVEMENTS #1917 (not DOS): an improved
 * nation keeps AI_EURO_UPKEEP_RESERVE_TURNS turns of building upkeep out of
 * every purchase, so it is not short at the production pass (#1916) between
 * its lumpy hold sales (#1919: one turn's left it short 40% of the time). */
long ai_euro_upkeep_reserve(
  const ColonizeCol1Save* col1, const ColonizeColonyPool* pool, int human_nation, int nation
) {
  if (!mods_on(MOD_BUILDING_UPKEEP) || !pool || !ai_euro_improved_nation(col1, human_nation, nation)) {
    return 0;
  }
  return (long)colonies_upkeep_total(pool, nation) * AI_EURO_UPKEEP_RESERVE_TURNS;
}

/* MOD_BUILDING_UPKEEP + MOD_AI_IMPROVEMENTS #1919 (not DOS): each nation's
 * trade income per turn (Europe hold sales, Custom House, village sales;
 * #1920: not treasure or gifts, whose spikes passed builds the trade could
 * not carry), smoothed over about sixteen turns, so an improved colony
 * builds only what its trade keeps up. */
enum { AI_EURO_INCOME_WINDOW = 16 };
static int32_t s_income_turn[4];
static int32_t s_income_avg8[4]; /* AI_EURO_INCOME_WINDOW x the running average */

static bool ai_euro_income_on(int nation) {
  return nation >= 0 && nation < 4 && mods_on(MOD_BUILDING_UPKEEP) && mods_on(MOD_AI_IMPROVEMENTS);
}

void ai_euro_income_note(int nation, int64_t delta) {
  if (delta > 0 && ai_euro_income_on(nation)) {
    const int64_t v = (int64_t)s_income_turn[nation] + delta;
    s_income_turn[nation] = v > INT32_MAX / 16 ? INT32_MAX / 16 : (int32_t)v;
  }
}

void ai_euro_income_roll(int nation) {
  if (!ai_euro_income_on(nation)) {
    return;
  }
  const int32_t got = s_income_turn[nation];
  s_income_turn[nation] = 0;
  int32_t* avg8 = &s_income_avg8[nation];
  *avg8 = *avg8 == 0 ? got * AI_EURO_INCOME_WINDOW : *avg8 - *avg8 / AI_EURO_INCOME_WINDOW + got;
}

int ai_euro_income_avg(int nation) {
  return ai_euro_income_on(nation) ? s_income_avg8[nation] / AI_EURO_INCOME_WINDOW : 0;
}

void ai_euro_income_reset(void) {
  memset(s_income_turn, 0, sizeof s_income_turn);
  memset(s_income_avg8, 0, sizeof s_income_avg8);
}

bool ai_euro_income_export(uint8_t out[AI_EURO_INCOME_SIZE]) {
  bool any = false;
  for (int n = 0; n < 4; ++n) {
    const uint32_t v[2] = {(uint32_t)s_income_turn[n], (uint32_t)s_income_avg8[n]};
    for (int k = 0; k < 2; ++k) {
      for (int b = 0; b < 4; ++b) {
        out[n * 8 + k * 4 + b] = (uint8_t)(v[k] >> (8 * b));
      }
      any = any || v[k] != 0;
    }
  }
  return any;
}

void ai_euro_income_import(const uint8_t in[AI_EURO_INCOME_SIZE]) {
  for (int n = 0; n < 4; ++n) {
    uint32_t v[2] = {0, 0};
    for (int k = 0; k < 2; ++k) {
      for (int b = 0; b < 4; ++b) {
        v[k] |= (uint32_t)in[n * 8 + k * 4 + b] << (8 * b);
      }
    }
    s_income_turn[n] = (int32_t)v[0];
    s_income_avg8[n] = (int32_t)v[1];
  }
}
