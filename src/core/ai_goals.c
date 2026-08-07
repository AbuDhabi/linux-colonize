#include "core/ai_goals.h"

#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/map.h"

#include <stdlib.h>
#include <string.h>

static AiNationGoals s_goals[4];
static AiWorkSlot s_work[AI_WORK_SLOTS];
static AiEuroInventory s_inv[4];

static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

void ai_goals_reset(void) {
  memset(s_goals, 0, sizeof(s_goals));
  memset(s_work, 0, sizeof(s_work));
  memset(s_inv, 0, sizeof(s_inv));
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

AiEuroInventory* ai_goals_inventory(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return NULL;
  }
  return &s_inv[nation_id];
}

void ai_goals_inventory_clear(int nation_id) {
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  if (inv) {
    memset(inv, 0, sizeof(*inv));
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

/* FUN_521d_0072 — open hole at slot by shifting [slot..62] down. */
static void primary_shift_down(int nation_id, int slot) {
  for (int i = AI_PRIMARY_SLOTS - 2; i >= slot; --i) {
    s_goals[nation_id].primary[i + 1] = s_goals[nation_id].primary[i];
  }
}

static void secondary_shift_down(int nation_id, int slot) {
  for (int i = AI_SECONDARY_SLOTS - 2; i >= slot; --i) {
    s_goals[nation_id].secondary[i + 1] = s_goals[nation_id].secondary[i];
  }
}

static void work_shift_down(int slot) {
  for (int i = AI_WORK_SLOTS - 2; i >= slot; --i) {
    s_work[i + 1] = s_work[i];
  }
}

void ai_goals_upsert_primary(int nation_id, int x, int y, int code, int prio) {
  if (nation_id < 0 || nation_id >= 4 || code < 0 || code == (int)AI_GOAL_EMPTY) {
    return;
  }
  AiNationGoals* g = &s_goals[nation_id];
  /* Reject if matching (x,y,code) already has prio ≥ new. */
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->primary[i];
    if (s->x == (int8_t)x && s->y == (int8_t)y && s->code == (uint8_t)code &&
        prio <= (int)s->prio) {
      return;
    }
  }
  /* Insert at first slot with lower prio or empty code (priority-ordered). */
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->primary[i];
    if ((int)s->prio < prio || s->code == AI_GOAL_EMPTY) {
      primary_shift_down(nation_id, i);
      s = &g->primary[i];
      s->x = (int8_t)x;
      s->y = (int8_t)y;
      s->code = (uint8_t)code;
      s->prio = (uint8_t)(prio > 255 ? 255 : prio);
      return;
    }
  }
}

void ai_goals_upsert_secondary(int nation_id, int x, int y, int code, int prio) {
  if (nation_id < 0 || nation_id >= 4 || code < 0 || code == (int)AI_GOAL_EMPTY) {
    return;
  }
  AiNationGoals* g = &s_goals[nation_id];
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->secondary[i];
    if (s->x == (int8_t)x && s->y == (int8_t)y && s->code == (uint8_t)code &&
        prio <= (int)s->prio) {
      return;
    }
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &g->secondary[i];
    if ((int)s->prio < prio || s->code == AI_GOAL_EMPTY) {
      secondary_shift_down(nation_id, i);
      s = &g->secondary[i];
      s->x = (int8_t)x;
      s->y = (int8_t)y;
      s->code = (uint8_t)code;
      s->prio = (uint8_t)(prio > 255 ? 255 : prio);
      return;
    }
  }
}

void ai_goals_promote_secondary_to_primary(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  /* FUN_521d_0342: clear all 64 primaries, then upsert each live secondary. */
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    ai_goals_clear_primary_slot(nation_id, i);
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &s_goals[nation_id].secondary[i];
    if ((int8_t)s->code >= 0) {
      ai_goals_upsert_primary(nation_id, s->x, s->y, (int)s->code, (int)s->prio);
    }
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
  /* FUN_521d_02be: score-ordered insert when score > occupant or id < 0. */
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    if (s_work[i].score < score || s_work[i].id < 0) {
      work_shift_down(i);
      s_work[i].id = (int16_t)id;
      s_work[i].score = (int16_t)score;
      s_work[i].flag_a = flag_a;
      s_work[i].flag_b = flag_b;
      return;
    }
  }
}

int ai_goals_max_primary_prio(int nation_id, int x, int y, int code) {
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  int best = 0;
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = &s_goals[nation_id].primary[i];
    if (s->x == (int8_t)x && s->y == (int8_t)y && s->code == (uint8_t)code &&
        (int)s->prio >= best) {
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

const AiWorkSlot* ai_goals_work(int slot) {
  if (slot < 0 || slot >= AI_WORK_SLOTS) {
    return NULL;
  }
  return &s_work[slot];
}

int ai_goals_best_found_tile(int nation_id, int* out_x, int* out_y) {
  if (nation_id < 0 || nation_id >= 4 || !out_x || !out_y) {
    return 0;
  }
  /* Primaries are priority-ordered; first FOUND/MIL_EXPAND wins. */
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    const AiGoalSlot* s = &s_goals[nation_id].primary[i];
    if (s->code == AI_GOAL_FOUND || s->code == AI_GOAL_MIL_EXPAND) {
      *out_x = s->x;
      *out_y = s->y;
      return 1;
    }
  }
  return 0;
}

int ai_goals_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int x,
  int y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return 0;
  }
  (void)nation_id;
  int best_dir = -1;
  int best_score = -1;
  for (int dir = 0; dir <= 8; ++dir) {
    const int nx = x + k_dir8_dx[dir];
    const int ny = y + k_dir8_dy[dir];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    if (map_tile_is_water(map, nx, ny)) {
      continue;
    }
    /* Arctic never foundable — skip even if colonies pool is NULL. */
    if (map_pedia_terrain_index_at(map, nx, ny) == 24) {
      continue;
    }
    if (colonies && !colonies_can_found(colonies, map, nx, ny) && dir != 8) {
      continue;
    }
    if (colonies && dir == 8 && !colonies_can_found(colonies, map, nx, ny)) {
      continue;
    }
    /* Food yield stand-in (not raw terrain index — arctic 24 was wrongly preferred). */
    int score = 10;
    score += colony_yield_for_tile(map, nx, ny, COLONIZE_JOB_FARMER) * 3;
    score += colony_yield_for_tile(map, nx, ny, COLONIZE_JOB_FISHERMAN);
    if (dir == 8) {
      score += 2;
    }
    if (map_tile_has_river(map, nx, ny)) {
      score += 3;
    }
    if (score > best_score) {
      best_score = score;
      best_dir = dir;
    }
  }
  if (best_dir < 0) {
    return 0;
  }
  *out_x = x + k_dir8_dx[best_dir];
  *out_y = y + k_dir8_dy[best_dir];
  return 1;
}
