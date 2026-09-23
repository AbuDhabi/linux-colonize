/* Smoke: ai_goals upsert/promote/work-16 match annotated euro_goals semantics. */
#include "core/ai_goals.h"

#include "core/col1_save.h"
#include "core/units.h"

#include "../common/ai_fixture.h"
#include "../common/test_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_goals: FAIL %s\n", msg);
  return 1;
}

static int case_upsert_priority_promote(void) {
  ai_goals_reset();

  /* Upsert priority-ordered: higher prio inserts before lower. */
  ai_goals_upsert_primary(1, 10, 20, AI_GOAL_FOUND, 2);
  ai_goals_upsert_primary(1, 11, 21, AI_GOAL_FOUND, 5);
  const AiGoalSlot* s0 = ai_goals_primary(1, 0);
  const AiGoalSlot* s1 = ai_goals_primary(1, 1);
  if (!s0 || !s1 || s0->prio != 5 || s1->prio != 2) {
    return fail("upsert priority order");
  }
  if (s0->x != 11 || s1->x != 10) {
    return fail("upsert shift contents");
  }

  /* Reject equal-or-worse duplicate. */
  ai_goals_upsert_primary(1, 11, 21, AI_GOAL_FOUND, 5);
  ai_goals_upsert_primary(1, 11, 21, AI_GOAL_FOUND, 4);
  if (ai_goals_primary(1, 2) && ai_goals_primary(1, 2)->code != AI_GOAL_EMPTY) {
    /* may have only 2 live — slot 2 should still be empty from reset+2 inserts */
  }
  int live = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    if (ai_goals_primary(1, i)->code != AI_GOAL_EMPTY) {
      live++;
    }
  }
  if (live != 2) {
    return fail("duplicate upsert should no-op");
  }

  /* Promote clears primaries then upserts secondaries. */
  ai_goals_upsert_secondary(1, 30, 40, AI_GOAL_LABOR, 3);
  ai_goals_promote_secondary_to_primary(1);
  live = 0;
  int saw_labor = 0;
  int saw_old_found = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = ai_goals_primary(1, i);
    if (s->code == AI_GOAL_EMPTY) {
      continue;
    }
    live++;
    if (s->code == AI_GOAL_LABOR && s->x == 30) {
      saw_labor = 1;
    }
    if (s->code == AI_GOAL_FOUND) {
      saw_old_found = 1;
    }
  }
  if (!saw_labor) {
    return fail("promote should upsert secondary LABOR");
  }
  if (saw_old_found) {
    return fail("promote should clear old primaries first");
  }
  return 0;
}

static int case_work_queue_score_order(void) {
  ai_goals_reset();

  /* Work queue 16 slots, score-ordered. */
  ai_goals_clear_work_queue();
  ai_goals_upsert_work(1, 10, 0, 0);
  ai_goals_upsert_work(2, 50, 0, 0);
  const AiWorkSlot* w0 = ai_goals_work(0);
  const AiWorkSlot* w1 = ai_goals_work(1);
  if (!w0 || !w1 || w0->id != 2 || w0->score != 50 || w1->id != 1) {
    return fail("work queue score order");
  }
  if (AI_WORK_SLOTS != 16) {
    return fail("work slots must be 16");
  }
  return 0;
}

static int case_work_consume_4393_tail(void) {
  ai_goals_reset();

  /*
   * FUN_521d_4393 queue-decrement tail (ai_goals_work_consume), decoded
   * 2026-09-06d from move_scoring_20e6_full.md's LAB_..._004393 block:
   *   remaining = max(0, loads − free holds)
   *   score     = remaining × score / loads   (capacity-scaled write-back)
   *   slot freed (id = −1) once nothing is left.
   */
  ai_goals_clear_work_queue();
  ai_goals_upsert_work(7, 400, /*loads=*/4, /*military=*/1);
  ai_goals_work_consume(0, /*free_holds=*/1);
  const AiWorkSlot* wc = ai_goals_work(0);
  if (!wc || wc->id != 7 || wc->loads != 3 || wc->score != 300 || wc->military != 1) {
    return fail("4393 tail: partial claim must scale score by surviving loads");
  }
  ai_goals_work_consume(0, /*free_holds=*/2);
  wc = ai_goals_work(0);
  if (!wc || wc->id != 7 || wc->loads != 1 || wc->score != 100) {
    return fail("4393 tail: second claim must decrement again");
  }
  ai_goals_work_consume(0, /*free_holds=*/6);
  wc = ai_goals_work(0);
  if (!wc || wc->id >= 0 || wc->loads != 0) {
    return fail("4393 tail: fully served slot must be freed");
  }
  /* A freed slot is inert: consuming again must not underflow or resurrect. */
  ai_goals_work_consume(0, 3);
  wc = ai_goals_work(0);
  if (!wc || wc->id >= 0 || wc->loads != 0) {
    return fail("4393 tail: freed slot must stay freed");
  }
  return 0;
}

static int case_work_consume_zero_load_gate(void) {
  ai_goals_reset();

  /* loads == 0 is DOS's "no work here" gate — a live id with zero loads is
   * left untouched rather than decremented past zero. */
  ai_goals_clear_work_queue();
  ai_goals_upsert_work(3, 90, /*loads=*/0, /*military=*/0);
  ai_goals_work_consume(0, 2);
  const AiWorkSlot* wc = ai_goals_work(0);
  if (!wc || wc->id != 3 || wc->score != 90 || wc->loads != 0) {
    return fail("4393 tail: zero-load slot must be left alone");
  }
  return 0;
}

static int case_stack_settler_pick(void) {
  /*
   * FUN_521d_0656 (the `a654` thunk 20e6's cargo goal fold calls), decoded
   * byte-exact from OVL14_L0000:0656 on 2026-09-06e: walk the chain and keep
   * the HIGHEST unit type carrying DS:0x523d bit 0x40 (0 Colonist, 2 Pioneer,
   * 5 Scout); ships (0x81/0x82/0xa2) never qualify, so an empty transport
   * returns -1 — the gate the fold reads as "no settler aboard".
   */
  ColonizeCol1Unit chain[4];
  memset(chain, 0, sizeof(chain));
  chain[0].type = 0x0d; /* Caravel — no 0x40 bit */
  chain[0].transport_chain.next_unit_idx = 1;
  chain[1].type = 0x00; /* Colonist */
  chain[1].transport_chain.next_unit_idx = 2;
  chain[2].type = 0x02; /* Pioneer — highest 0x40 type in the chain */
  chain[2].transport_chain.next_unit_idx = 3;
  chain[3].type = 0x01; /* Soldier — 0x1c, not settler-capable */
  chain[3].transport_chain.next_unit_idx = -1;
  if (ai_goals_stack_settler_pick(chain, 4, 0) != 2) {
    return fail("0656: highest settler-capable member must win");
  }
  chain[2].type = 0x05; /* Scout outranks the Pioneer */
  if (ai_goals_stack_settler_pick(chain, 4, 0) != 2) {
    return fail("0656: Scout is settler-capable too");
  }
  chain[1].type = 0x01;
  chain[2].type = 0x01;
  if (ai_goals_stack_settler_pick(chain, 4, 0) != -1) {
    return fail("0656: a ship carrying no settler must return -1");
  }
  if (ai_goals_stack_settler_pick(chain, 4, -1) != -1) {
    return fail("0656: negative start index must return -1");
  }
  return 0;
}

static int case_goal_fold_urgency(void) {
  /*
   * The predicate 20e6's cargo goal fold branches on (raw 1750):
   *   urgency = FUN_521d_052c(rep unit) + FUN_521d_03d0(nation)
   *   >= 1 -> promote plain civilians into the founder count
   *   <  1 -> (no FOUND probe) demote Pioneers into civilians
   * 052c is clamped to <= 0, so the sign is decided by 03d0: 8 with the
   * default all-zero plan scratch (no colonies yet / none asking for
   * colonists — DOS's own early-out), 0 once colony_count and
   * colonies_wanting_colonists are both set (the formula lands on -8 and the
   * negative result returns 0). Pinning both halves keeps the demote arm
   * reachable-by-construction rather than dead.
   */
  ai_goals_reset();
  const int colonist_type = 0;
  const int pioneer_type = 2;
  if (ai_goals_founding_expansion_urgency(1, 0) != 8) {
    return fail("03d0: default plan scratch must give urgency 8");
  }
  const int promote_pio =
    ai_goals_unit_desirability_score(NULL, NULL, 1, 5, 5, pioneer_type, 0, 0, 5, 0) +
    ai_goals_founding_expansion_urgency(1, 0);
  const int promote_col =
    ai_goals_unit_desirability_score(NULL, NULL, 1, 5, 5, colonist_type, 0, 0, 5, 0) +
    ai_goals_founding_expansion_urgency(1, 0);
  if (promote_pio < 1 || promote_col < 1) {
    return fail("goal fold: default scratch must take the promote arm");
  }
  /*
   * FUN_281f_0c9a → FUN_15eb_0002: profession 0x13 / 0x19..0x1c score -2,
   * everything else -4. Profession 0 (Free Colonist) must take the -4 arm.
   */
  if (ai_goals_unit_desirability_score(NULL, NULL, 1, 5, 5, colonist_type, 0, 0, 5, 0) !=
      ai_goals_unit_desirability_score(NULL, NULL, 1, 5, 5, colonist_type, 0x1a, 0, 5, 0) - 2) {
    return fail("052c: 0c9a profession gate must split -4 (generic) vs -2 (0x19..0x1c)");
  }
  AiNationPlanScratch* p = ai_goals_plan_scratch(1);
  if (!p) {
    return fail("plan scratch");
  }
  p->colony_count = 1;
  p->colonies_wanting_colonists = 1;
  if (ai_goals_founding_expansion_urgency(1, 0) != 0) {
    return fail("03d0: colony+wanting scratch must give urgency 0");
  }
  const int stale =
    ai_goals_unit_desirability_score(NULL, NULL, 1, 5, 5, pioneer_type, 0, 0, 5, 0) +
    ai_goals_founding_expansion_urgency(1, 0);
  if (stale >= 1) {
    return fail("goal fold: zero expansion urgency must take the demote arm");
  }
  ai_goals_reset();
  return 0;
}

static int case_indian_hostility_gate(void) {
  /*
   * FUN_521d_0896 — the Indian hostility gate (viceroy_unpacked.c
   * 87319-87340), wired 2026-09-08. Euro owners pass straight through;
   * natives need `has_context`, then either alarm > 0x4a (FUN_281f_030c =
   * DS:0x5b1c) or the tile unit's DS:0x54f6 grudge slot > 0x7f. Both reads
   * were parked at 0 before, so no native could ever raise a claim.
   */
  ColonizeCol1Save gt;
  col1_save_init(&gt);
  gt.head.tribe_count = 2;
  /* DS:0x54f6 = the settlement record's own attitude[euro] word
   * (tribe.alarm[euro] = {friction, attacks}) — no parallel array. */
  ColonizeCol1Tribe tribes[2];
  memset(tribes, 0, sizeof(tribes));
  tribes[0].nation_id = 5;
  tribes[1].nation_id = 5;
  gt.tribe = tribes;
  gt.owned = false; /* stack fixture; nothing to free */

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.units[0].active = true;
  pool.units[0].id = 0;
  pool.units[0].home_tribe_id = 1; /* village 1 of the tribe array */

  const int me = 2;    /* acting Euro nation */
  const int them = 5;  /* Indian nation id 5 -> col1->indian[1] */

  /* Euro owner id (<= 3): the >3 gate never fires, value passes through. */
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, 3, 0, -1) != 3) {
    return fail("0896: Euro owner id must pass through untouched");
  }
  /* has_context 0 (the 20e6 explorer probe) rejects every native. */
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 0, 0) != -1) {
    return fail("0896: has_context 0 must reject a native outright");
  }
  /* Cool nation, zero tension -> no claim. */
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 1, 0) != -1) {
    return fail("0896: cool nation with zero tension must not claim");
  }
  /* Alarm arm: DOS compares `0x4a < alarm`, so 74 is still cool. */
  gt.indian[1].alarm_by_player[me] = 74;
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 1, 0) != -1) {
    return fail("0896: alarm 0x4a must not open the gate (strict >)");
  }
  gt.indian[1].alarm_by_player[me] = 75;
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 1, -1) != them) {
    return fail("0896: alarm 75 must open the gate with no unit on the tile");
  }

  /* Tension arm, alarm back to cool: DOS compares `0x7f < tension`. */
  gt.indian[1].alarm_by_player[me] = 0;
  col1_tribe_attitude_set(&tribes[1], me, 0x7f); /* friction 0x7f, 0 attacks */
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 1, 0) != -1) {
    return fail("0896: tension 0x7f must not open the gate (strict >)");
  }
  col1_tribe_attitude_set(&tribes[1], me, 0x80);
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 1, 0) != them) {
    return fail("0896: tension 0x80 must open the gate");
  }
  /* One recorded trespass (attacks = 1, friction 0) is the same word 0x100
   * and opens the gate on its own — the high byte counts. */
  tribes[1].alarm[me].friction = 0;
  tribes[1].alarm[me].attacks = 1;
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 1, 0) != them) {
    return fail("0896: one attack (word 0x100) must open the gate");
  }
  col1_tribe_attitude_set(&tribes[1], me, 0x80);
  /* Keyed by the tile unit's home village — no unit index, no read. */
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, me, them, 1, -1) != -1) {
    return fail("0896: tension is only read through the tile unit (unit_index >= 0)");
  }
  /* ...and by the acting nation: another Euro's column stays shut. */
  if (ai_goals_filter_profession_by_distance_wealth(&gt, &pool, 1, them, 1, 0) != -1) {
    return fail("0896: tension slot is per acting Euro nation");
  }
  /* No col1 = the pre-2026-09-08 parked behaviour (both reads answer 0). */
  if (ai_goals_filter_profession_by_distance_wealth(NULL, &pool, me, them, 1, 0) != -1) {
    return fail("0896: NULL col1 must fall back to the parked identity");
  }
  return 0;
}

/*
 * FUN_521d_06ae raw 87277-87278: the occupant gate accepts a neighbour that
 * holds a single own unit only when that unit's ARTILLERY-ness differs from
 * the placing unit's (`(*(char *)(iVar9 * 0x1c + 0x3146) == '\v') != param_5`,
 * '\v' = 0x0b = Artillery). The port tested WAGON (0x0c) here until
 * bugs.md #752, so the two roles were swapped.
 */
static int case_06ae_occupant_artillery_filter(void) {
  ColonizeWorldMap map;
  if (!fx_map_alloc(&map, 8, 8, 3, false)) {
    return fail("06ae map alloc");
  }
  for (int i = 0; i < 8 * 8; ++i) {
    map.layer3[i] = 0xf0;
  }
  ColonizeUnitPool units;
  fx_units_init(&units);
  units.type_count = 3;
  units.types[0].kind_plus1 = (uint8_t)(UNITS_KIND_ARTILLERY + 1);
  units.types[0].movement = 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[1].kind_plus1 = (uint8_t)(UNITS_KIND_WAGON + 1);
  units.types[1].movement = 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[2].kind_plus1 = (uint8_t)(UNITS_KIND_SOLDIER + 1);
  units.types[2].movement = 1;
  units.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;

  const int nation = 1;
  const int from_x = 4;
  const int from_y = 4;
  /* Lone own Artillery west, lone own Wagon east. */
  const int aid = units_spawn(&units, 0, from_x - 1, from_y);
  const int wid = units_spawn(&units, 1, from_x + 1, from_y);
  ColonizeUnit* au = units_get(&units, aid);
  ColonizeUnit* wu = units_get(&units, wid);
  if (!au || !wu) {
    fx_map_free(&map);
    return fail("06ae spawn");
  }
  au->nation_id = nation;
  wu->nation_id = nation;
  /* Block the other six neighbours with FOREIGN units (raw 87274: a tile whose
   * presence owner is not ours is skipped outright), so the pick is exactly
   * "which of the two own-occupied tiles does the XOR admit". */
  static const int k_block[6][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 1}, {0, 1}, {1, 1}};
  for (int b = 0; b < 6; ++b) {
    const int bid = units_spawn(&units, 2, from_x + k_block[b][0], from_y + k_block[b][1]);
    ColonizeUnit* bu = units_get(&units, bid);
    if (!bu) {
      fx_map_free(&map);
      return fail("06ae blocker spawn");
    }
    bu->nation_id = 0;
  }

  const ColonizeWorld w = {.units = &units, .map = &map};
  int x1 = -1;
  int y1 = -1;
  if (!ai_goals_pick_founding_tile_ex_w(&w, nation, from_x, from_y, 0, 1, &x1, &y1)) {
    fx_map_free(&map);
    return fail("06ae pick (artillery_filter=1)");
  }
  if (x1 == from_x - 1 && y1 == from_y) {
    fx_map_free(&map);
    return fail("06ae: artillery_filter=1 must reject the Artillery-held tile");
  }
  int x0 = -1;
  int y0 = -1;
  if (!ai_goals_pick_founding_tile_ex_w(&w, nation, from_x, from_y, 0, 0, &x0, &y0)) {
    fx_map_free(&map);
    return fail("06ae pick (artillery_filter=0)");
  }
  if (x0 == from_x + 1 && y0 == from_y) {
    fx_map_free(&map);
    return fail("06ae: artillery_filter=0 must reject the Wagon-held tile");
  }
  fx_map_free(&map);
  return 0;
}

static const TestCase k_cases[] = {
    {"case_upsert_priority_promote", case_upsert_priority_promote},
    {"case_work_queue_score_order", case_work_queue_score_order},
    {"case_work_consume_4393_tail", case_work_consume_4393_tail},
    {"case_work_consume_zero_load_gate", case_work_consume_zero_load_gate},
    {"case_stack_settler_pick", case_stack_settler_pick},
    {"case_goal_fold_urgency", case_goal_fold_urgency},
    {"case_indian_hostility_gate", case_indian_hostility_gate},
    {"case_06ae_occupant_artillery_filter", case_06ae_occupant_artillery_filter},
};

TEST_MAIN(k_cases)
