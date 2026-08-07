#include "core/ai_diplo.h"

#include "core/colony.h"
#include "core/units.h"

#include <stdlib.h>
#include <string.h>

/*
 * FUN_15b3_* bilateral Euro×Euro bytes + 6d8e timers + 5bfb war/ally.
 * Thin map: original_sources_annotated/ai/euro_diplo.md
 *
 * nation[a].unknown26[0..3] = treaty timers toward peer
 * nation[a].unknown26[4..7] = diplo flag byte toward peer (15b3 stand-in)
 * Exact DS −0x77c4 offset PARKED. Indian×Euro full 15b3 matrix PORT DEBT
 * (thin stand-ins: peaceful drift + war relation hit on Euro×Euro declare).
 */

#define AI_DIPLO_FLAG_BASE 4

/* Thin FUN_5bfb_153e stand-in: treasury + tax friction on war declare.
 * Full 153e trade/military score body, dialogs PARKED.
 * FA 3f41 full body PARKED - thin ally-aid + break trust only.
 * War trade embargo: OR Furs into nation.boycott_bitmap (cargo idx 4);
 * distinct from king refuse Sugar bit1. Full per-rival 153e trade PARKED. */
#define AI_DIPLO_WAR_GOLD_STING 100u
#define AI_DIPLO_WAR_TAX_BUMP 1u
#define AI_DIPLO_WAR_TAX_CAP 75u
#define AI_DIPLO_WAR_UPKEEP_GOLD 5u
#define AI_DIPLO_WAR_EMBARGO_CARGO_BIT (1u << COLONIZE_CARGO_FURS)
#define AI_DIPLO_ALLY_GOLD_COST 25u
#define AI_DIPLO_ALLY_TREATY_MIN 8u
#define AI_DIPLO_ALLY_AID_GOLD 10u
#define AI_DIPLO_ALLY_AID_MIN_TREASURY 50u
#define AI_DIPLO_BREAK_GOLD_PENALTY 20u
#define AI_DIPLO_INDIAN_DRIFT_CAP 160u
#define AI_DIPLO_WAR_INDIAN_HIT 5

static uint8_t* ai_diplo_timer_byte(ColonizeCol1Save* col1, int nation, int peer);

static void ai_diplo_war_treasury_sting(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->gold > AI_DIPLO_WAR_GOLD_STING) {
      nat->gold -= AI_DIPLO_WAR_GOLD_STING;
    } else {
      nat->gold = 0;
    }
  }
}

/* Thin 153e side effect: +1 tax_rate both sides, cap 75 (king tax path). */
static void ai_diplo_war_tax_bump(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->tax_rate >= AI_DIPLO_WAR_TAX_CAP) {
      continue;
    }
    uint8_t next = (uint8_t)(nat->tax_rate + AI_DIPLO_WAR_TAX_BUMP);
    if (next > AI_DIPLO_WAR_TAX_CAP) {
      next = (uint8_t)AI_DIPLO_WAR_TAX_CAP;
    }
    nat->tax_rate = next;
  }
}

/* Thin wartime trade embargo: OR Furs boycott bit on both nations. */
static void ai_diplo_war_embargo_set(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    nat->boycott_bitmap =
      (uint16_t)(nat->boycott_bitmap | AI_DIPLO_WAR_EMBARGO_CARGO_BIT);
  }
}

static int ai_diplo_at_war_with_any_euro(const ColonizeCol1Save* col1, int nation) {
  if (!col1 || nation < 0 || nation >= 4) {
    return 0;
  }
  for (int other = 0; other < 4; ++other) {
    if (other == nation) {
      continue;
    }
    if (ai_diplo_at_war(col1, nation, other)) {
      return 1;
    }
  }
  return 0;
}

/*
 * Lift Furs embargo when a nation has no remaining Euro×Euro wars.
 * Call sites: make_peace, form_alliance (clears WAR). Other PEACE
 * writes / Fugger FF may still clear bits — full 153e trade PARKED.
 */
static void ai_diplo_war_embargo_lift_if_peace(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    if (ai_diplo_at_war_with_any_euro(col1, n)) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    nat->boycott_bitmap =
      (uint16_t)(nat->boycott_bitmap & (uint16_t)~AI_DIPLO_WAR_EMBARGO_CARGO_BIT);
  }
}

/* Per-turn light upkeep while at war (euro_balance); floor 0. */
static void ai_diplo_war_upkeep_drain(ColonizeCol1Nation* nat) {
  if (!nat || nat->gold == 0) {
    return;
  }
  if (nat->gold > AI_DIPLO_WAR_UPKEEP_GOLD) {
    nat->gold -= AI_DIPLO_WAR_UPKEEP_GOLD;
  } else {
    nat->gold = 0;
  }
}

/* Thin alliance treasury cost: each side pays 25 if able (floor 0). */
static void ai_diplo_ally_treasury_cost(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->gold > AI_DIPLO_ALLY_GOLD_COST) {
      nat->gold -= AI_DIPLO_ALLY_GOLD_COST;
    } else {
      nat->gold = 0;
    }
  }
}

/* Ensure treaty timer toward peer is at least 8 if currently 0 (both dirs). */
static void ai_diplo_ally_treaty_timer_bump(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  uint8_t* ta = ai_diplo_timer_byte(col1, nation_a, nation_b);
  uint8_t* tb = ai_diplo_timer_byte(col1, nation_b, nation_a);
  if (ta && *ta == 0) {
    *ta = (uint8_t)AI_DIPLO_ALLY_TREATY_MIN;
  }
  if (tb && *tb == 0) {
    *tb = (uint8_t)AI_DIPLO_ALLY_TREATY_MIN;
  }
}

/* Break-alliance trust loss: −20 gold each side (floor 0). Tax path unused. */
static void ai_diplo_break_trust_penalty(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    ColonizeCol1Nation* nat = &col1->nation[n];
    if (nat->gold > AI_DIPLO_BREAK_GOLD_PENALTY) {
      nat->gold -= AI_DIPLO_BREAK_GOLD_PENALTY;
    } else {
      nat->gold = 0;
    }
  }
}

/*
 * Thin FA / ally-aid stand-in (full 3f41 PARKED): once per euro_balance peer visit,
 * if allied and peer gold < self gold/2 and self gold >= 50, transfer 10 gold to ally.
 */
static void ai_diplo_ally_foreign_aid(ColonizeCol1Save* col1, int nation_id, int peer) {
  if (!col1 || nation_id < 0 || nation_id >= 4 || peer < 0 || peer >= 4 || nation_id == peer) {
    return;
  }
  ColonizeCol1Nation* self = &col1->nation[nation_id];
  ColonizeCol1Nation* other = &col1->nation[peer];
  if (self->gold < AI_DIPLO_ALLY_AID_MIN_TREASURY) {
    return;
  }
  if (other->gold >= self->gold / 2u) {
    return;
  }
  if (self->gold < AI_DIPLO_ALLY_AID_GOLD) {
    return;
  }
  self->gold -= AI_DIPLO_ALLY_AID_GOLD;
  other->gold += AI_DIPLO_ALLY_AID_GOLD;
}

/* True if Euro nation is at war with any other Euro (thin hostility gate). */
static int ai_diplo_euro_at_war_any(const ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation_id) {
      continue;
    }
    if (ai_diplo_at_war(col1, nation_id, peer)) {
      return 1;
    }
  }
  return 0;
}

/*
 * Peaceful Indian×Euro relation drift (not full 15b3 matrix).
 * Per tick: for each of 8 Indian slots, if < 160 and Euro not at war → +1 (cap 160).
 */
static void ai_diplo_indian_peaceful_drift(ColonizeCol1Save* col1, int nation_id) {
  if (!col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  if (ai_diplo_euro_at_war_any(col1, nation_id)) {
    return;
  }
  ColonizeCol1Nation* nat = &col1->nation[nation_id];
  for (int i = 0; i < 8; ++i) {
    uint8_t r = nat->relation_by_indian[i];
    if (r < AI_DIPLO_INDIAN_DRIFT_CAP) {
      nat->relation_by_indian[i] = (uint8_t)(r + 1u);
    }
  }
}

/* Indians dislike Euro×Euro war: −5 on all 8 Indian relation slots (both sides). */
static void ai_diplo_war_indian_relation_hit(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1) {
    return;
  }
  for (int i = 0; i < 2; ++i) {
    const int n = (i == 0) ? nation_a : nation_b;
    if (n < 0 || n >= 4) {
      continue;
    }
    for (int idx = 0; idx < 8; ++idx) {
      ai_diplo_indian_relation_delta(col1, 4 + idx, n, -AI_DIPLO_WAR_INDIAN_HIT);
    }
  }
}

static uint8_t* ai_diplo_timer_byte(ColonizeCol1Save* col1, int nation, int peer) {
  if (!col1 || nation < 0 || nation >= 4 || peer < 0 || peer >= 4 || nation == peer) {
    return NULL;
  }
  return &col1->nation[nation].unknown26[peer];
}

static uint8_t* ai_diplo_flag_byte(ColonizeCol1Save* col1, int nation, int peer) {
  if (!col1 || nation < 0 || nation >= 4 || peer < 0 || peer >= 4 || nation == peer) {
    return NULL;
  }
  return &col1->nation[nation].unknown26[AI_DIPLO_FLAG_BASE + peer];
}

static const uint8_t* ai_diplo_flag_byte_const(const ColonizeCol1Save* col1, int nation, int peer) {
  if (!col1 || nation < 0 || nation >= 4 || peer < 0 || peer >= 4 || nation == peer) {
    return NULL;
  }
  return &col1->nation[nation].unknown26[AI_DIPLO_FLAG_BASE + peer];
}

/* Mirror WAR/ALLY into nation_relation for legacy readers (derived only). */
static void ai_diplo_mirror_relation_summary(ColonizeCol1Save* col1, int nation) {
  if (!col1 || nation < 0 || nation >= 4) {
    return;
  }
  int war = 0;
  int ally = 0;
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation) {
      continue;
    }
    const uint8_t* f = ai_diplo_flag_byte_const(col1, nation, peer);
    if (!f) {
      continue;
    }
    if (*f & AI_DIPLO_WAR) {
      war = 1;
    }
    if (*f & AI_DIPLO_ALLY) {
      ally = 1;
    }
  }
  if (war) {
    col1->head.nation_relation[nation] = -50;
  } else if (ally) {
    col1->head.nation_relation[nation] = 40;
  } else {
    col1->head.nation_relation[nation] = 0;
  }
  /* Keep player.diplomacy as a coarse OR of peer flags (UI crumb). */
  uint8_t agg = 0;
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation) {
      continue;
    }
    const uint8_t* f = ai_diplo_flag_byte_const(col1, nation, peer);
    if (f) {
      agg = (uint8_t)(agg | *f);
    }
  }
  col1->player[nation].diplomacy = agg;
}

uint8_t ai_diplo_read(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4) {
    return 0;
  }
  if (nation_a == nation_b) {
    return AI_DIPLO_PEACE | AI_DIPLO_ALLY;
  }
  const uint8_t* f = ai_diplo_flag_byte_const(col1, nation_a, nation_b);
  if (!f) {
    return 0;
  }
  /* Unmet / never written: treat as peaceful known (PEACE|MET). */
  if (*f == 0) {
    return (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET);
  }
  return *f;
}

void ai_diplo_write(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t value) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4) {
    return;
  }
  if (nation_a == nation_b) {
    return;
  }
  uint8_t* f = ai_diplo_flag_byte(col1, nation_a, nation_b);
  if (!f) {
    return;
  }
  *f = value;
  ai_diplo_mirror_relation_summary(col1, nation_a);
}

void ai_diplo_or_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits) {
  if (!col1 || nation_a == nation_b) {
    return;
  }
  uint8_t a = (uint8_t)(ai_diplo_read(col1, nation_a, nation_b) | bits);
  uint8_t b = (uint8_t)(ai_diplo_read(col1, nation_b, nation_a) | bits);
  ai_diplo_write(col1, nation_a, nation_b, a);
  ai_diplo_write(col1, nation_b, nation_a, b);
}

void ai_diplo_clear_both(ColonizeCol1Save* col1, int nation_a, int nation_b, uint8_t bits) {
  if (!col1 || nation_a == nation_b) {
    return;
  }
  uint8_t a = (uint8_t)(ai_diplo_read(col1, nation_a, nation_b) & (uint8_t)~bits);
  uint8_t b = (uint8_t)(ai_diplo_read(col1, nation_b, nation_a) & (uint8_t)~bits);
  /* Store raw result (0 = unread default on next read). */
  uint8_t* fa = ai_diplo_flag_byte(col1, nation_a, nation_b);
  uint8_t* fb = ai_diplo_flag_byte(col1, nation_b, nation_a);
  if (fa) {
    *fa = a;
  }
  if (fb) {
    *fb = b;
  }
  ai_diplo_mirror_relation_summary(col1, nation_a);
  ai_diplo_mirror_relation_summary(col1, nation_b);
}

int ai_diplo_at_war(const ColonizeCol1Save* col1, int nation_a, int nation_b) {
  return (ai_diplo_read(col1, nation_a, nation_b) & AI_DIPLO_WAR) != 0;
}

void ai_diplo_declare_war(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  const int already = ai_diplo_at_war(col1, nation_a, nation_b);
  ai_diplo_clear_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_ALLY));
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_WAR | AI_DIPLO_MET));
  /* Thin 153e-shaped sting: gold drain + tax bump both sides (relation via mirror). */
  if (!already) {
    ai_diplo_war_treasury_sting(col1, nation_a, nation_b);
    ai_diplo_war_tax_bump(col1, nation_a, nation_b);
    /* Indians dislike Euro×Euro war (scalar stand-in; full 15b3 PARKED). */
    ai_diplo_war_indian_relation_hit(col1, nation_a, nation_b);
    /* Wartime trade embargo stand-in: Furs boycott bit both sides. */
    ai_diplo_war_embargo_set(col1, nation_a, nation_b);
  }
}

/*
 * Thin make-peace (not full 153e peace dialog / 102a/1092):
 * clear WAR both ways, set PEACE|MET, lift Furs embargo if no Euro wars remain.
 * No gold cost (war sting/upkeep already drained treasury). Full 153e PARKED.
 */
void ai_diplo_make_peace(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  if (!col1 || nation_a < 0 || nation_a >= 4 || nation_b < 0 || nation_b >= 4 ||
      nation_a == nation_b) {
    return;
  }
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_WAR);
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET));
  ai_diplo_war_embargo_lift_if_peace(col1, nation_a, nation_b);
}

void ai_diplo_form_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_WAR);
  ai_diplo_or_both(col1, nation_a, nation_b, (uint8_t)(AI_DIPLO_ALLY | AI_DIPLO_PEACE | AI_DIPLO_MET));
  /* Lift Furs embargo if neither side remains at Euro war. */
  ai_diplo_war_embargo_lift_if_peace(col1, nation_a, nation_b);
  /* Thin alliance treasury cost: 25 gold each side (floor 0). */
  ai_diplo_ally_treasury_cost(col1, nation_a, nation_b);
  /* Treaty timer: if peer slot is 0, set to 8 so alliance persists a few ticks. */
  ai_diplo_ally_treaty_timer_bump(col1, nation_a, nation_b);
}

void ai_diplo_break_alliance(ColonizeCol1Save* col1, int nation_a, int nation_b) {
  const int was_ally = (ai_diplo_read(col1, nation_a, nation_b) & AI_DIPLO_ALLY) != 0;
  ai_diplo_clear_both(col1, nation_a, nation_b, AI_DIPLO_ALLY);
  ai_diplo_or_both(col1, nation_a, nation_b, AI_DIPLO_PEACE);
  /* Trust loss stand-in (FA 3f41 PARKED): −20 gold each side if they were allied. */
  if (was_ally) {
    ai_diplo_break_trust_penalty(col1, nation_a, nation_b);
  }
}

void ai_diplo_treaty_timers(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* 6d8e step 4: decrement per-rival treaty timer bytes before planning. */
  for (int other = 0; other < 4; ++other) {
    if (other == nation_id) {
      continue;
    }
    uint8_t* t = ai_diplo_timer_byte(ctx->col1, nation_id, other);
    if (!t) {
      continue;
    }
    if (*t > 0) {
      (*t)--;
    }
    if (*t != 0) {
      continue;
    }
    /* Expiry: break alliance if allied; else thin peace/met tweak. */
    const uint8_t bits = ai_diplo_read(ctx->col1, nation_id, other);
    if (bits & AI_DIPLO_ALLY) {
      ai_diplo_break_alliance(ctx->col1, nation_id, other);
      continue;
    }
    if (ctx->rng && dos_rng_range(ctx->rng, 1, 8) == 1) {
      if (bits & AI_DIPLO_MET) {
        uint8_t* f = ai_diplo_flag_byte(ctx->col1, nation_id, other);
        if (f) {
          *f = (uint8_t)(AI_DIPLO_PEACE | AI_DIPLO_MET);
          ai_diplo_mirror_relation_summary(ctx->col1, nation_id);
        }
      }
    }
  }
  /* Peaceful Indian relation drift (thin; full Indian×Euro 15b3 PORT DEBT). */
  ai_diplo_indian_peaceful_drift(ctx->col1, nation_id);
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

void ai_diplo_euro_balance(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /*
   * FUN_5bfb_10ec / 13b0 checklist:
   *  1 skip human; at-war → light upkeep; near-parity → thin make_peace
   *  2 military score (0000/00f8/312e stand-in)
   *  3 10ec eligibility: war if self ≫ other; ally if near-parity
   *  4 13b0 form/break + thin ally aid (FA 3f41 PARKED)
   *  5 declare_war → thin 153e gold+tax (full body / dialogs / 12d0 PARKED)
   */
  const int self = ai_diplo_military_score(ctx, nation_id);
  for (int peer = 0; peer < 4; ++peer) {
    if (peer == nation_id || ctx->col1->player[peer].control == 2) {
      continue;
    }
    const int other = ai_diplo_military_score(ctx, peer);
    const uint8_t bits = ai_diplo_read(ctx->col1, nation_id, peer);

    if (bits & AI_DIPLO_WAR) {
      /* Thin ongoing 153e friction: 5 gold/turn while gold>0 (per war peer). */
      ai_diplo_war_upkeep_drain(&ctx->col1->nation[nation_id]);
      /*
       * Thin peace heuristic: near-parity (ally-eligible band) while at war →
       * make_peace. Full 153e peace dialog PARKED; no gold cost.
       */
      if (self > 10 && other > 10 && abs(self - other) < 15) {
        if (ctx->rng && dos_rng_range(ctx->rng, 1, 30) == 1) {
          ai_diplo_make_peace(ctx->col1, nation_id, peer);
        }
      }
      continue;
    }

    /* Thin FA ally-aid: richer ally may gift 10g once per peer visit. */
    if (bits & AI_DIPLO_ALLY) {
      ai_diplo_ally_foreign_aid(ctx->col1, nation_id, peer);
    }

    /* 13b0 break: imbalance while allied. */
    if ((bits & AI_DIPLO_ALLY) && self > other * 2 + 25 && self > 40) {
      if (ctx->rng && dos_rng_range(ctx->rng, 1, 25) == 1) {
        ai_diplo_break_alliance(ctx->col1, nation_id, peer);
      }
      continue;
    }

    /* 10ec war eligibility. */
    if (self > other * 2 + 20 && self > 30) {
      if (ctx->rng && dos_rng_range(ctx->rng, 1, 20) == 1) {
        ai_diplo_declare_war(ctx->col1, nation_id, peer);
        /* thin 153e sting inside declare_war; full body / 12d0 / dialogs PARKED */
      }
      continue;
    }

    /* 10ec/13b0 ally eligibility. */
    if (self > 10 && other > 10 && abs(self - other) < 15) {
      if ((bits & AI_DIPLO_ALLY) == 0) {
        if (ctx->rng && dos_rng_range(ctx->rng, 1, 40) == 1) {
          ai_diplo_form_alliance(ctx->col1, nation_id, peer);
        }
      }
    }
  }
}

void ai_diplo_euro_timers(ColonizeTurnContext* ctx, int nation_id) {
  ai_diplo_treaty_timers(ctx, nation_id);
}

void ai_diplo_indian_relation_delta(
  ColonizeCol1Save* col1,
  int indian_nation,
  int euro_nation,
  int delta
) {
  /*
   * FUN_4cc6_00f2 / FUN_15dc_00e0-shaped scalar store on Euro nation record.
   * Full Indian×Euro 15b3 bilateral matrix is PORT DEBT (see euro_diplo.md).
   */
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
