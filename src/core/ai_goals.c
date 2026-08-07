#include "core/ai_goals.h"

#include <string.h>

static AiNationGoals s_goals[4];
static AiWorkSlot s_work[AI_WORK_SLOTS];

void ai_goals_reset(void) {
  memset(s_goals, 0, sizeof(s_goals));
  memset(s_work, 0, sizeof(s_work));
  for (int n = 0; n < 4; ++n) {
    for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
      s_goals[n].primary[i].code = AI_GOAL_EMPTY;
    }
    for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
      s_goals[n].secondary[i].code = AI_GOAL_EMPTY;
    }
  }
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    s_work[i].id = -1;
  }
}

void ai_goals_clear_primary_slot(int nation_id, int slot) {
  if (nation_id < 0 || nation_id >= 4 || slot < 0 || slot >= AI_PRIMARY_SLOTS) {
    return;
  }
  s_goals[nation_id].primary[slot].code = AI_GOAL_EMPTY;
  s_goals[nation_id].primary[slot].prio = 0;
}

void ai_goals_clear_secondary_slots(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    s_goals[nation_id].secondary[i].code = AI_GOAL_EMPTY;
    s_goals[nation_id].secondary[i].prio = 0;
  }
}

void ai_goals_promote_secondary_to_primary(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* Clear emptied primaries, then copy secondary → first free primary. */
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &s_goals[nation_id].secondary[i];
    if (s->code == AI_GOAL_EMPTY) {
      continue;
    }
    ai_goals_upsert_primary(nation_id, s->x, s->y, (int)s->code, (int)s->prio);
    s->code = AI_GOAL_EMPTY;
    s->prio = 0;
  }
}

void ai_goals_upsert_primary(int nation_id, int x, int y, int code, int prio) {
  if (nation_id < 0 || nation_id >= 4 || code == (int)AI_GOAL_EMPTY) {
    return;
  }
  AiNationGoals* g = &s_goals[nation_id];
  int empty = -1;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    if (g->primary[i].code == (uint8_t)code && g->primary[i].x == (int8_t)x &&
        g->primary[i].y == (int8_t)y) {
      if ((int)g->primary[i].prio < prio) {
        g->primary[i].prio = (uint8_t)prio;
      }
      return;
    }
    if (empty < 0 && g->primary[i].code == AI_GOAL_EMPTY) {
      empty = i;
    }
  }
  if (empty >= 0) {
    g->primary[empty].x = (int8_t)x;
    g->primary[empty].y = (int8_t)y;
    g->primary[empty].code = (uint8_t)code;
    g->primary[empty].prio = (uint8_t)(prio > 255 ? 255 : prio);
  }
}

void ai_goals_upsert_secondary(int nation_id, int x, int y, int code, int prio) {
  if (nation_id < 0 || nation_id >= 4 || code == (int)AI_GOAL_EMPTY) {
    return;
  }
  AiNationGoals* g = &s_goals[nation_id];
  int empty = -1;
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    if (g->secondary[i].code == (uint8_t)code && g->secondary[i].x == (int8_t)x &&
        g->secondary[i].y == (int8_t)y) {
      if ((int)g->secondary[i].prio < prio) {
        g->secondary[i].prio = (uint8_t)prio;
      }
      return;
    }
    if (empty < 0 && g->secondary[i].code == AI_GOAL_EMPTY) {
      empty = i;
    }
  }
  if (empty >= 0) {
    g->secondary[empty].x = (int8_t)x;
    g->secondary[empty].y = (int8_t)y;
    g->secondary[empty].code = (uint8_t)code;
    g->secondary[empty].prio = (uint8_t)(prio > 255 ? 255 : prio);
  }
}

void ai_goals_clear_work_queue(void) {
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    s_work[i].id = -1;
    s_work[i].score = 0;
    s_work[i].flag_a = 0;
    s_work[i].flag_b = 0;
  }
}

void ai_goals_upsert_work(int id, int score, uint8_t flag_a, uint8_t flag_b) {
  int empty = -1;
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    if (s_work[i].id == id) {
      if (score > s_work[i].score) {
        s_work[i].score = (int16_t)score;
        s_work[i].flag_a = flag_a;
        s_work[i].flag_b = flag_b;
      }
      return;
    }
    if (empty < 0 && s_work[i].id < 0) {
      empty = i;
    }
  }
  if (empty >= 0) {
    s_work[empty].id = (int16_t)id;
    s_work[empty].score = (int16_t)score;
    s_work[empty].flag_a = flag_a;
    s_work[empty].flag_b = flag_b;
  }
}

int ai_goals_max_primary_prio(int nation_id, int x, int y, int code) {
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int best = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = &s_goals[nation_id].primary[i];
    if (s->code != (uint8_t)code) {
      continue;
    }
    if (x >= 0 && (s->x != (int8_t)x || s->y != (int8_t)y)) {
      continue;
    }
    if ((int)s->prio > best) {
      best = (int)s->prio;
    }
  }
  return best;
}

const AiGoalSlot* ai_goals_primary(int nation_id, int slot) {
  if (nation_id < 0 || nation_id >= 4 || slot < 0 || slot >= AI_PRIMARY_SLOTS) {
    return NULL;
  }
  return &s_goals[nation_id].primary[slot];
}

int ai_goals_best_found_tile(int nation_id, int* out_x, int* out_y) {
  if (nation_id < 0 || nation_id >= 4 || !out_x || !out_y) {
    return 0;
  }
  int best_prio = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = &s_goals[nation_id].primary[i];
    if (s->code != AI_GOAL_FOUND && s->code != AI_GOAL_MIL_EXPAND) {
      continue;
    }
    if ((int)s->prio > best_prio) {
      best_prio = (int)s->prio;
      bx = s->x;
      by = s->y;
    }
  }
  if (best_prio < 0) {
    return 0;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}
