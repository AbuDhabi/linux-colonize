/* Smoke: ai_goals upsert/promote/work-16 match annotated euro_goals semantics. */
#include "core/ai_goals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_goals: FAIL %s\n", msg);
  return 1;
}

int main(void) {
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
  /* loads == 0 is DOS's "no work here" gate — a live id with zero loads is
   * left untouched rather than decremented past zero. */
  ai_goals_clear_work_queue();
  ai_goals_upsert_work(3, 90, /*loads=*/0, /*military=*/0);
  ai_goals_work_consume(0, 2);
  wc = ai_goals_work(0);
  if (!wc || wc->id != 3 || wc->score != 90 || wc->loads != 0) {
    return fail("4393 tail: zero-load slot must be left alone");
  }

  /*
   * FUN_521d_0656 (the `a654` thunk 20e6's cargo goal fold calls), decoded
   * byte-exact from OVL14_L0000:0656 on 2026-09-06e: walk the chain and keep
   * the HIGHEST unit type carrying DS:0x523d bit 0x40 (0 Colonist, 2 Pioneer,
   * 5 Scout); ships (0x81/0x82/0xa2) never qualify, so an empty transport
   * returns -1 — the gate the fold reads as "no settler aboard".
   */
  {
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
  }

  /*
   * The predicate 20e6's cargo goal fold branches on (raw 1750):
   *   urgency = FUN_521d_052c(rep unit) + FUN_521d_03d0(nation)
   *   >= 1 -> promote plain civilians into the founder count
   *   <  1 -> (no FOUND probe) demote Pioneers into civilians
   * 052c is clamped to <= 0, so the sign is decided by 03d0: 8 with the
   * default all-zero plan scratch, 0 once hire_flag/found_flag are set (the
   * formula lands on -8 and the negative result returns 0). Pinning both
   * halves keeps the demote arm reachable-by-construction rather than dead.
   */
  {
    ai_goals_reset();
    const int colonist_type = 0;
    const int pioneer_type = 2;
    if (ai_goals_founding_expansion_urgency(1, 0) != 8) {
      return fail("03d0: default plan scratch must give urgency 8");
    }
    const int promote_pio =
      ai_goals_unit_desirability_score(NULL, 1, 5, 5, pioneer_type, 0, 0, 0, 5, 0) +
      ai_goals_founding_expansion_urgency(1, 0);
    const int promote_col =
      ai_goals_unit_desirability_score(NULL, 1, 5, 5, colonist_type, 0, 0, 0, 5, 0) +
      ai_goals_founding_expansion_urgency(1, 0);
    if (promote_pio < 1 || promote_col < 1) {
      return fail("goal fold: default scratch must take the promote arm");
    }
    AiNationPlanScratch* p = ai_goals_plan_scratch(1);
    if (!p) {
      return fail("plan scratch");
    }
    p->hire_flag = 1;
    p->found_flag = 1;
    if (ai_goals_founding_expansion_urgency(1, 0) != 0) {
      return fail("03d0: hire+found scratch must give urgency 0");
    }
    const int stale =
      ai_goals_unit_desirability_score(NULL, 1, 5, 5, pioneer_type, 0, 0, 0, 5, 0) +
      ai_goals_founding_expansion_urgency(1, 0);
    if (stale >= 1) {
      return fail("goal fold: zero expansion urgency must take the demote arm");
    }
    ai_goals_reset();
  }

  printf("unit_ai_goals: ok\n");
  return 0;
}
