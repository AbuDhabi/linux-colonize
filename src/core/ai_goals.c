#include "core/ai_goals.h"

#include "core/colony.h"
#include "core/col1_save.h"
#include "core/map.h"
#include "core/units.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static AiNationGoals s_goals[4];
static AiWorkSlot s_work[AI_WORK_SLOTS];
static AiEuroInventory s_inv[4];
static AiNationPlanScratch s_plan[4];

static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

void ai_goals_reset(void) {
  memset(s_goals, 0, sizeof(s_goals));
  memset(s_work, 0, sizeof(s_work));
  memset(s_inv, 0, sizeof(s_inv));
  memset(s_plan, 0, sizeof(s_plan));
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

/*
 * FUN_521d_001c — invalidate_nearby_secondary_goals. Distance callee (DOS
 * FUN_281f_037a) is corrupted in the decomp (no visible body/params); reuses
 * the FUN_124c_0040 / ai_dos_dist formula as the closest known "generic DOS
 * tile distance" analog.
 */
void ai_goals_invalidate_nearby_secondary(int nation_id, int code, int x, int y, int radius) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  for (int i = 0; i < AI_SECONDARY_SLOTS; ++i) {
    AiGoalSlot* s = &s_goals[nation_id].secondary[i];
    if ((int)s->code != code) {
      continue;
    }
    int dx = x - (int)s->x;
    int dy = y - (int)s->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    const int d = (dy < dx) ? (dy >> 1) + dx : (dx >> 1) + dy;
    if (d <= radius) {
      s->code = AI_GOAL_EMPTY;
      s->prio = 0;
    }
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
  const struct ColonizeUnitPool* units,
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
    /*
     * DOS 06ae occupant gate (FUN_281f_06d2 tribe-or-presence, then
     * FUN_281f_08bc(unit) == 1 singleton check): a neighbour holding a
     * foreign presence is skipped; an own unit there only passes when it is
     * a lone unit whose wagon-ness differs from the unit being placed
     * (type 0x0b == wagon XOR wagon_filter). Stay (dir 8) is never gated.
     * Seed-100 TURN2→3: the Dutch Soldier lands at (48,14) because the
     * Pioneer already dropped on (49,14) blocks that tile.
     */
    if (dir != 8 && units) {
      const int oid = units_id_at(units, nx, ny);
      if (oid >= 0) {
        const ColonizeUnit* ou = units_get_const(units, oid);
        if (!ou || ou->nation_id != nation_id) {
          continue;
        }
        int on_tile = 0;
        for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
          const ColonizeUnit* tu = &units->units[ui];
          if (tu->active && tu->aboard_ship_id < 0 && tu->x == nx && tu->y == ny) {
            on_tile++;
          }
        }
        const ColonizeUnitType* ot = units_type(units, ou->type_index);
        const int ou_is_wagon = ot && strstr(ot->name, "Wagon") != NULL;
        if (on_tile != 1 || (ou_is_wagon != 0) == (wagon_filter != 0)) {
          continue;
        }
      }
    }

    /* Base: terrain-class founding byte @ DS:0x2f77. */
    int score = map_dos_terr_found_score_byte(map_dos_terr_class_at(map, nx, ny));
    if (coastal_bonus > 0 && map_tile_is_coastal(map, nx, ny)) {
      score += coastal_bonus;
    }
    /* First-colony (coastal≥40): west-of-origin bias toward Atlantic towns. */
    if (coastal_bonus >= 40) {
      score += (x - nx);
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
    int best_rx = -1, best_ry = -1;
    for (int r = 2; r <= 4 && !any; ++r) {
      for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
          if (abs(dx) != r && abs(dy) != r) {
            continue;
          }
          const int nx = x + dx;
          const int ny = y + dy;
          if (!map_coords_inset(map, nx, ny)) {
            continue;
          }
          if (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) {
            continue;
          }
          if (map_pedia_terrain_index_at(map, nx, ny) == 24) {
            continue;
          }
          if (colonies && !colonies_can_found(colonies, map, nx, ny)) {
            continue;
          }
          int score = map_dos_terr_found_score_byte(map_dos_terr_class_at(map, nx, ny));
          if (coastal_bonus > 0 && map_tile_is_coastal(map, nx, ny)) {
            score += coastal_bonus;
          }
          if (score > best_score) {
            best_score = score;
            best_rx = nx;
            best_ry = ny;
            any = 1;
          }
        }
      }
    }
    if (any) {
      *out_x = best_rx;
      *out_y = best_ry;
      return 1;
    }
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
    /*units=*/NULL,
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

AiNationPlanScratch* ai_goals_plan_scratch(int nation_id) {
  if (nation_id < 0 || nation_id >= 4) {
    return NULL;
  }
  return &s_plan[nation_id];
}

/*
 * FUN_479b_076e's `nation*0x13c-0x77b2 = DS:0x538e` stamp (case-7 FOUND
 * COLONY handler, viceroy_unpacked.c:77006): record the current turn as
 * this nation's last colony-founding turn, feeding 052c's decay term.
 */
void ai_goals_note_colony_founded(int nation_id, int turn) {
  if (nation_id < 0 || nation_id >= 4) {
    return;
  }
  s_plan[nation_id].last_colony_founded_turn = turn;
}

/*
 * FUN_521d_03d0 — founding_expansion_urgency. Cite: viceroy_unpacked.c ~87058.
 * All-zero scratch (no writer wired yet) takes the hire_flag==0 early return,
 * reproducing today's existing "early game -> 8" stand-in in ai_euro.c.
 */
int ai_goals_founding_expansion_urgency(int nation_id, int total_colony_count) {
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  if (total_colony_count < 0x30) {
    const AiNationPlanScratch* p = &s_plan[nation_id];
    if (p->hire_flag == 0 || p->found_flag == 0) {
      return 8;
    }
    int local_10 = ((int)p->c_val - (int)p->hire_flag) / (4 - (int)p->e_val);
    const int uv4 = (int)p->d_val >> 1;
    if (uv4 < local_10) {
      local_10 = -(((local_10 - uv4 + 1) >> 1) - local_10);
    } else if (local_10 < uv4) {
      local_10 = local_10 + ((1 - (local_10 - uv4)) >> 1);
    }
    const int iv2 = (int)p->e_val * 3 - 7;
    const int iv3 = -iv2;
    const int iv1 = p->f_val;
    if (-iv1 != iv2 && iv1 <= iv3) {
      local_10 = local_10 + (-1 - (iv3 - iv1)) * (int)p->hire_flag;
    }
    if (local_10 >= 0) {
      return local_10;
    }
  }
  return 0;
}

/*
 * FUN_521d_052c — unit_desirability_score. Cite: viceroy_unpacked.c ~87139.
 * dist_to_bound_colony is DOS DS:0x8db8 (caller-supplied snapshot, see
 * move_scoring_land.md). FUN_281f_0c9a "needs training" gate is PARKED (reads
 * as false). Tail term identity confirmed 2026-08-19 (see
 * AiNationPlanScratch's own header comment): thunk_2a1f_0494(nation) is
 * founding_expansion_urgency, called here a second time purely as a
 * nonzero gate (not a war flag) for a "(turns since last colony founded)
 * >> 4" decay bonus.
 */
int ai_goals_unit_desirability_score(
  const ColonizeColonyPool* colonies,
  int nation_id,
  int unit_x,
  int unit_y,
  int unit_type,
  int unit_profession,
  int continent_id,
  int dist_to_bound_colony,
  int turn,
  int total_colony_count
) {
  (void)continent_id; /* DOS 0614 continent-gate not replicated — thin exact-tile check only */
  if (nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  const AiNationPlanScratch* p = &s_plan[nation_id];
  int score = 0;
  if (p->hire_flag != 0) {
    int owns = -1;
    if (colonies) {
      const int idx = colonies_id_at(colonies, unit_x, unit_y);
      if (idx >= 0 && colonies->colonies[idx].nation_id == nation_id) {
        owns = idx;
      }
    }
    if (owns < 0) {
      score = 2;
    } else {
      score = dist_to_bound_colony / 5 - 1;
    }
  }
  if (unit_type == 2) score += 2;
  if (unit_type == 1) score += -2;
  if (unit_type == 4) score += -3;
  if (unit_type == 0) {
    /* FUN_281f_0c9a(profession) — PARKED identity, thin false. */
    const int needs_training = 0;
    score += needs_training ? -4 : -2;
    if (unit_profession == 0x1b) {
      score += -0x14;
    }
  }
  if (ai_goals_founding_expansion_urgency(nation_id, total_colony_count) != 0) {
    score += (turn - p->last_colony_founded_turn) >> 4;
  }
  if (score > 0) {
    score = 0;
  }
  return score;
}

/*
 * FUN_521d_0600 — composite_unit_priority. Cite: viceroy_unpacked.c ~87196.
 * Thunk identities resolved by arg-shape match against 052c/0492/03d0's own
 * signatures (all three take a leading nation id and share this call site's
 * exact arg counts).
 */
int ai_goals_composite_unit_priority(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  int nation_id,
  int unit_x,
  int unit_y,
  int unit_type,
  int unit_profession,
  int dist_to_bound_colony,
  int turn,
  int total_colony_count
) {
  if (!map) {
    return 0;
  }
  const int continent = map_continent_id_at(map, unit_x, unit_y);
  const int desirability = ai_goals_unit_desirability_score(
    colonies, nation_id, unit_x, unit_y, unit_type, unit_profession,
    continent, dist_to_bound_colony, turn, total_colony_count
  );
  const int balance = ai_goals_colony_balance_flags(map, colonies, col1, nation_id, continent);
  const int urgency = ai_goals_founding_expansion_urgency(nation_id, total_colony_count);
  int total = desirability + balance + urgency;
  if (total < 0) {
    total = 0;
  }
  return total;
}

/*
 * FUN_521d_0656 — walk_unit_stack_to_end. Cite: viceroy_unpacked.c ~87219.
 * DOS body is corrupted (drops the FUN_281f_02e4 call's argument), but the
 * shape is unambiguous: follow the transport chain until -1.
 */
int ai_goals_walk_unit_stack_to_end(
  const ColonizeCol1Unit* units,
  int unit_count,
  int unit_index
) {
  int last = -1;
  while (units && unit_index >= 0 && unit_index < unit_count) {
    last = unit_index;
    unit_index = units[unit_index].transport_chain.next_unit_idx;
  }
  return last;
}

/*
 * FUN_521d_0896 — filter_profession_by_distance_wealth. Cite:
 * viceroy_unpacked.c ~87319. `profession` is DOS's overloaded owner id
 * (0..3 = nation, >=4 = Indian tribe index) reused as a profession code by
 * the caller (see FUN_521d_0906) — the >3 gate is a no-op for real nation
 * ids, matching by construction. FUN_281f_030c relation/alarm lookup and the
 * DS:0x54f6 wealth table are PARKED (no Linux accessor).
 */
int ai_goals_filter_profession_by_distance_wealth(
  const ColonizeCol1Unit* units,
  int unit_count,
  int nation_id,
  int profession,
  int has_context,
  int unit_index
) {
  (void)nation_id;
  if (profession > 3) {
    if (!has_context) {
      return -1;
    }
    /* FUN_281f_030c(nation, profession-4) — PARKED identity. */
    const int relation_or_dist = 0;
    int gate = relation_or_dist > 0x4a;
    if (!gate && unit_index >= 0 && units && unit_index < unit_count) {
      /* DS:0x54f6 wealth/tribute table [origin*9+nation], int16 — PARKED. */
      const int wealth = 0;
      if (wealth > 0x7f) {
        gate = 1;
      }
    }
    if (!gate) {
      return -1;
    }
  }
  return profession;
}

/*
 * FUN_521d_0906 — probe_adjacent_contact_claim. Cite: viceroy_unpacked.c
 * ~87345. Second (DOS DS:0x9ea8) probe and the tribe-owner / armed-cargo
 * defender walk are PARKED (no tile->tribe or tile->unit-stack accessor
 * wired here) — those branches always take their "not found" arm; see
 * ai_goals.h.
 */
int ai_goals_probe_adjacent_contact_claim(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int x,
  int y,
  int nation_id,
  int profession,
  int* out_side_claim
) {
  int side = -1;
  if (!map) {
    if (out_side_claim) *out_side_claim = side;
    return -1;
  }
  const int origin_water =
    (map_tile_is_water(map, x, y) || map_tile_is_high_seas(map, x, y)) ? 1 : 0;
  int claim = -1;
  for (int dir = 0; dir < 8; ++dir) {
    const int nx = x + k_dir8_dx[dir];
    const int ny = y + k_dir8_dy[dir];
    if (!map_coords_inset(map, nx, ny)) {
      continue;
    }
    const int n_water =
      (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) ? 1 : 0;
    if (n_water != origin_water) {
      continue;
    }
    /* FUN_281f_0682: owner id (0..3 nation, >=4 tribe) — tribe half PARKED. */
    int owner = -1;
    if (colonies) {
      const int idx = colonies_id_at(colonies, nx, ny);
      if (idx >= 0) {
        owner = colonies->colonies[idx].nation_id;
      }
    }
    if (claim < 0 && owner >= 0 && owner != nation_id) {
      /* Tribe (>=4) armed-cargo top-of-stack filter unit index — PARKED. */
      const int unit_filter = -1;
      claim = ai_goals_filter_profession_by_distance_wealth(
        NULL, 0, nation_id, owner, profession, unit_filter
      );
      if (claim >= 0 && origin_water != 0) {
        /* Armed-defender walk over the tile's unit stack — PARKED. */
        const int has_valid_defender = 0;
        if (!has_valid_defender) {
          claim = -1;
        }
      }
    }
    /* Independent second probe (DOS FUN_281f_06be) — PARKED ownership table. */
    const int owner_alt = -1;
    if (owner_alt >= 0 && owner_alt != nation_id) {
      const int side_claim = ai_goals_filter_profession_by_distance_wealth(
        NULL, 0, nation_id, owner_alt, profession, -1
      );
      if (side_claim < 4 && side < 0) {
        side = side_claim;
      }
    }
  }
  if (out_side_claim) {
    *out_side_claim = side;
  }
  return claim;
}
