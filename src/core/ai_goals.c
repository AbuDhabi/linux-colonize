#include "core/ai_goals.h"

#include "core/colony.h"
#include "core/col1_save.h"
#include "core/map.h"

#include <limits.h>
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

/*
 * FUN_521d_0492 — colony_count_balance_flags(nation, continent).
 * Live Euro colony × layer3 continent tallies; target = continent_tally_b/12.
 * +2 when no Euro colonies on continent (live sum==0; decomp 947e==summed).
 * +4 when this nation has 0 on continent. Cite: viceroy_unpacked.c ~87098.
 */
int ai_goals_colony_balance_flags(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int continent_id
) {
  if (!col1 || continent_id < 0 || continent_id > 15) {
    return 0;
  }
  if (nation_id < 0 || nation_id > 3) {
    return 0;
  }

  int nation_cont[4][16];
  memset(nation_cont, 0, sizeof(nation_cont));
  if (map && colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &colonies->colonies[i];
      if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      const int cid = map_continent_id_at(map, c->x, c->y);
      if (cid < 0 || cid > 15) {
        continue;
      }
      nation_cont[c->nation_id][cid]++;
    }
  }

  int sum_on_cont = 0;
  for (int n = 0; n < 4; ++n) {
    sum_on_cont += nation_cont[n][continent_id];
  }

  const unsigned target = (unsigned)col1->post_map.continent_tally_b[continent_id] / 12u;
  int flags = 0;
  if ((int)target > sum_on_cont) {
    flags = 1;
  } else if ((int)target < sum_on_cont) {
    flags = -1;
  }
  /* Decomp: 947e == (947e + Σ94e6) ⇒ Σ nation colonies == 0. Prefer live sum. */
  if (sum_on_cont == 0) {
    flags += 2;
  }
  if (nation_cont[nation_id][continent_id] == 0) {
    flags += 4;
  }
  return flags;
}

/*
 * FUN_521d_06ae — pick_best_adjacent_founding_tile.
 * Decomp viceroy_unpacked.c ~87237. Base score = DS:0x2f77[class]; when
 * score_extras, add 0492(candidate continent)*0x10 + (explore & 0xf) per empty
 * land neighbor. Cite: euro_goals.c; move_scoring.md.
 */
int ai_goals_pick_founding_tile_ex(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int x,
  int y,
  int score_extras,
  int wagon_filter,
  int coastal_bonus,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return 0;
  }
  (void)wagon_filter; /* DOS own-tile wagon type filter — thin: colonies_can_found */
  int best_dir = -1;
  int best_score = INT_MIN;
  int any = 0;
  for (int dir = 0; dir <= 8; ++dir) {
    const int nx = x + k_dir8_dx[dir];
    const int ny = y + k_dir8_dy[dir];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    /* Land, not ocean/HS (FUN_281f_0302 + !0768). */
    if (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) {
      continue;
    }
    /* Arctic never foundable. */
    if (map_pedia_terrain_index_at(map, nx, ny) == 24) {
      continue;
    }
    /*
     * Tribe/owner gate (06d2/06be): empty or foundable. Stay (dir 8) may keep
     * a non-foundable tile in DOS; Linux still requires can_found when pool set.
     */
    int ok = 1;
    if (colonies) {
      if (dir == 8) {
        ok = colonies_can_found(colonies, map, nx, ny) ? 1 : 0;
      } else if (!colonies_can_found(colonies, map, nx, ny)) {
        ok = 0;
      }
    }
    if (!(dir == 8 || ok)) {
      continue;
    }
    if (!ok && dir == 8) {
      continue;
    }

    /* Base: terrain-class founding byte @ DS:0x2f77. */
    int score = map_dos_terr_found_score_byte(map_dos_terr_class_at(map, nx, ny));
    if (coastal_bonus > 0 && map_tile_is_coastal(map, nx, ny)) {
      score += coastal_bonus;
    }

    if (score_extras) {
      /* Decomp: 0492(nation, continent_of_candidate) once per empty neighbor. */
      const int cand_cid = map_continent_id_at(map, nx, ny);
      const int bal = ai_goals_colony_balance_flags(map, colonies, col1, nation_id, cand_cid);
      for (int nd = 0; nd < 8; ++nd) {
        const int hx = nx + k_dir8_dx[nd];
        const int hy = ny + k_dir8_dy[nd];
        if (!map_coords_inset(map, hx, hy)) {
          continue;
        }
        if (map_tile_is_water(map, hx, hy) || map_tile_is_high_seas(map, hx, hy)) {
          continue;
        }
        /* Neighbor empty of colony (0682 owner < 0 stand-in). */
        if (colonies && colonies_id_at(colonies, hx, hy) >= 0) {
          continue;
        }
        /*
         * DOS: 0492(nation, continent_id)*0x10 + (explore_mask & 0xf).
         * Explore: thin seen→1 (074a plane low nibble PARKED).
         * Score must stay signed — bal can be −1.
         */
        int explore = 0;
        if (map_tile_seen_by(map, hx, hy, nation_id)) {
          explore = 1;
        }
        score += bal * 0x10 + (explore & 0xf);
      }
    }

    if (score > best_score) {
      best_score = score;
      best_dir = dir;
      any = 1;
    }
  }
  if (!any) {
    return 0;
  }
  *out_x = x + k_dir8_dx[best_dir];
  *out_y = y + k_dir8_dy[best_dir];
  return 1;
}

int ai_goals_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int x,
  int y,
  int* out_x,
  int* out_y
) {
  return ai_goals_pick_founding_tile_ex(
    map,
    colonies,
    col1,
    nation_id,
    x,
    y,
    /*score_extras=*/1,
    /*wagon_filter=*/0,
    /*coastal_bonus=*/0,
    out_x,
    out_y
  );
}
