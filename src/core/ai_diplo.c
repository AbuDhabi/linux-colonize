#include "core/ai_diplo.h"

#include "core/colony.h"
#include "core/units.h"

#include <stdlib.h>

/*
 * T0 storage: pack bilateral Euro bits into player[n].diplomacy nibble pairs and
 * head.nation_relation signed scores. Full DOS 0x13c/0x4e matrices are larger;
 * this is enough for planner war/ally gates.
 */

static uint8_t* ai_diplo_byte(ColonizeCol1Save* col1, int nation) {
  if (!col1 || nation < 0 || nation >= 4) {
    return NULL;
  }
  return &col1->player[nation].diplomacy;
}

uint8_t ai_diplo_read(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4) {
    return 0;
  }
  if (nation_a == nation_b) {
    return AI_DIPLO_PEACE | AI_DIPLO_ALLY;
  }
  /* Encode peer bits in diplomacy: low nibble = vs nation0.. use relation sign. */
  int16_t rel = col1->head.nation_relation[nation_a];
  (void)nation_b;
  if (rel < -20) {
    return AI_DIPLO_WAR | AI_DIPLO_MET;
  }
  if (rel > 20) {
    return AI_DIPLO_ALLY | AI_DIPLO_PEACE | AI_DIPLO_MET;
  }
  return (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET | (col1->player[nation_a].diplomacy & 0xf0));
}

void ai_diplo_write(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t value) {
  uint8_t* b = ai_diplo_byte(col1, nation_a);
  if (!b) {
    return;
  }
  *b = value;
  if (value & AI_DIPLO_WAR) {
    col1->head.nation_relation[nation_a] = -50;
    if (nation_b >= 0 && nation_b < 4) {
      col1->head.nation_relation[nation_b] = -50;
    }
  } else if (value & AI_DIPLO_ALLY) {
    col1->head.nation_relation[nation_a] = 40;
  } else {
    col1->head.nation_relation[nation_a] = 0;
  }
  (void)nation_b;
}

void ai_diplo_or_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits) {
  if (!col1) {
    return;
  }
  uint8_t a = ai_diplo_read(col1, nation_a, nation_b) | bits;
  uint8_t b = ai_diplo_read(col1, nation_b, nation_a) | bits;
  ai_diplo_write(col1, nation_a, nation_b, a);
  ai_diplo_write(col1, nation_b, nation_a, b);
}

void ai_diplo_clear_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits) {
  if (!col1) {
    return;
  }
  uint8_t a = (uint8_t)(ai_diplo_read(col1, nation_a, nation_b) & (uint8_t)~bits);
  uint8_t b = (uint8_t)(ai_diplo_read(col1, nation_b, nation_a) & (uint8_t)~bits);
  ai_diplo_write(col1, nation_a, nation_b, a);
  ai_diplo_write(col1, nation_b, nation_a, b);
}

int ai_diplo_at_war(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  return (ai_diplo_read(col1, nation_a, nation_b) & AI_DIPLO_WAR) != 0;
}

void ai_diplo_declare_war(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  ai_diplo_clear_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_ALLY));
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
}

void ai_diplo_form_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_WAR);
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_ALLY | AI_DIPLO_PEACE | AI_DIPLO_MET));
}

void ai_diplo_break_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_ALLY);
  ai_diplo_or_both(col1, nation_a, nation_b, AI_DIPLO_PEACE);
}

static int ai_diplo_military_score(const ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->units) {
    return 0;
  }
  int score = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
    if (t) {
      score += t->attack + t->defense;
    }
  }
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (c->active && c->nation_id == nation_id) {
        score += c->population * 2;
      }
    }
  }
  return score;
}

void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* FUN_5bfb_10ec-shaped: if much stronger than peer and not allied, may war. */
  const int self = ai_diplo_military_score(ctx, nation_id);
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation_id) {
      continue;
    }
    if (ctx->col1->player[peer].control == 2) {
      continue;
    }
    const int other = ai_diplo_military_score(ctx, peer);
    if (ai_diplo_at_war(ctx->col1, nation_id, peer)) {
      continue;
    }
    if (self > other * 2 + 20 && self > 30) {
      /* Occasional war declaration (T0; not LCG-faithful). */
      if (ctx->rng && dos_rng_range(ctx->rng, 1, 20) == 1) {
        ai_diplo_declare_war(ctx->col1, nation_id, peer);
      }
    } else if (self > 10 && other > 10 && abs(self - other) < 15) {
      if (!ai_diplo_at_war(ctx->col1, nation_id, peer) &&
          (ai_diplo_read(ctx->col1, nation_id, peer) & AI_DIPLO_ALLY) == 0) {
        if (ctx->rng && dos_rng_range(ctx->rng, 1, 40) == 1) {
          ai_diplo_form_alliance(ctx->col1, nation_id, peer);
        }
      }
    }
  }
}

void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
) {
  if (!col1 || euro_nation < 0 || euro_nation >= 4) {
    return;
  }
  int idx = indian_nation - 4;
  if (idx < 0 || idx >= 8) {
    return;
  }
  int v = (int)col1->nation[euro_nation].relation_by_indian[idx] + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > 255) {
    v = 255;
  }
  col1->nation[euro_nation].relation_by_indian[idx] = (uint8_t)v;
}
