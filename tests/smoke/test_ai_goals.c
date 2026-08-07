/* Smoke: ai_goals upsert/promote/work-16 match annotated euro_goals semantics. */
#include "core/ai_goals.h"

#include <stdio.h>
#include <stdlib.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_goals: FAIL %s\n", msg);
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

  printf("smoke_ai_goals: ok\n");
  return 0;
}
