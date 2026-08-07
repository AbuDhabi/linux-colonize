/*
 * European AI goal-table helpers — FUN_521d_0000 … FUN_521d_0906.
 *
 * Source: original_sources_decompiled/viceroy_unpacked.c ~86772–87403
 * Callers: FUN_521d_0a60 (upserts), FUN_521d_6d8e plan pass (0342),
 *          FUN_521d_20e6 founding (06ae via 2a1f_04ac).
 * Linux:   PORT DEBT — ai_euro_early_turn peels substitute for real goals.
 *
 * Reference only — not compiled into the Linux binary.
 */

#include <stdint.h>

#include "../include/viceroy_types.h"
#include "../include/viceroy_globals.h"

extern int rng_range(int lo, int hi_inclusive); /* unused here; kept for LCG peers */
extern int map_tile_in_bounds(int x, int y);
extern int ocean_or_high_seas(int x, int y);
extern int tile_owner_or_presence(int x, int y);
extern int tile_tribe_or_presence(int x, int y);
extern int continent_id(int x, int y);
extern int unit_index_on_tile(int x, int y);
extern int terrain_class_at(int x, int y);
extern uint8_t tile_explore_mask(int x, int y);
extern int dos_dist(int x0, int y0, int x1, int y1);

/* Dir8 neighbor tables at DS:0xb4 (dx) / DS:0xbe (dy) — same as Indian act. */
static const int k_dir8_dx[9] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
static const int k_dir8_dy[9] = {-1, -1, 0, 1, 1, 1, 0, -1, 0};

/* ====================================================================== */
/* Goal-table addressing                                                  */
/* ====================================================================== */

/*
 * Primary slot base for (nation, slot):
 *   DS:0x98b0 + (nation * 0x40 + slot) * 4
 * Annotated as pointer math; not live-linked.
 */
static uint8_t *primary_slot(int nation_id, int slot) {
  return (uint8_t *)(uintptr_t)(VICEROY_DS_AI_PRIMARY_GOALS +
                                (nation_id * VICEROY_AI_PRIMARY_SLOTS + slot) *
                                    VICEROY_AI_GOAL_STRIDE);
}

static uint8_t *secondary_slot(int nation_id, int slot) {
  return (uint8_t *)(uintptr_t)(VICEROY_DS_AI_SECONDARY_GOALS +
                                (nation_id * VICEROY_AI_SECONDARY_SLOTS + slot) *
                                    VICEROY_AI_GOAL_STRIDE);
}

static uint8_t *work_slot(int slot) {
  return (uint8_t *)(uintptr_t)(VICEROY_DS_AI_WORK_QUEUE +
                                slot * VICEROY_AI_WORK_STRIDE);
}

/* ====================================================================== */
/* Slot bookkeeping                                                       */
/* ====================================================================== */

/*
 * Ghidra: FUN_521d_0000 | clear_primary_goal_slot
 * Clear one primary slot: code=0xff, priority=0. Leaves x/y untouched in
 * decomp (only writes −0x674e/−0x674d).
 */
void clear_primary_goal_slot(int nation_id, int slot) {
  uint8_t *s = primary_slot(nation_id, slot);
  s[2] = VICEROY_AI_GOAL_EMPTY;
  s[3] = 0;
}

/*
 * Ghidra: FUN_521d_001c | invalidate_nearby_secondary_goals
 * Walk nation×16 secondary slots; if code matches and DOS dist(xy,goal)≤radius,
 * clear that secondary slot.
 */
void invalidate_nearby_secondary_goals(int nation_id, int code, int x, int y,
                                       int radius) {
  for (int i = 0; i < VICEROY_AI_SECONDARY_SLOTS; ++i) {
    uint8_t *s = secondary_slot(nation_id, i);
    if ((int8_t)s[2] == code) {
      int d = dos_dist(x, y, (int8_t)s[0], (int8_t)s[1]);
      if (d <= radius) {
        s[2] = VICEROY_AI_GOAL_EMPTY;
        s[3] = 0;
      }
    }
  }
}

/*
 * Ghidra: FUN_521d_0072 | primary_goal_shift_down
 * Shift primary slots [slot..0x3e] down by one (open a hole at `slot`).
 */
void primary_goal_shift_down(int nation_id, int slot) {
  for (int i = 0x3e; i >= slot; --i) {
    uint8_t *dst = primary_slot(nation_id, i + 1);
    uint8_t *src = primary_slot(nation_id, i);
    /* decomp copies two words: (x,y) then (code,prio) */
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
  }
}

/*
 * Ghidra: FUN_521d_00a8 | secondary_goal_shift_down
 * Same for secondary slots [slot..0xe].
 */
void secondary_goal_shift_down(int nation_id, int slot) {
  for (int i = 0xe; i >= slot; --i) {
    uint8_t *dst = secondary_slot(nation_id, i + 1);
    uint8_t *src = secondary_slot(nation_id, i);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
  }
}

/*
 * Ghidra: FUN_521d_00de | work_queue_shift_down
 * Shift global work-queue entries [slot..0xe] down (6-byte records).
 */
void work_queue_shift_down(int slot) {
  for (int i = 0xe; i >= slot; --i) {
    int16_t *dst = (int16_t *)(void *)work_slot(i + 1);
    int16_t *src = (int16_t *)(void *)work_slot(i);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
}

/*
 * Ghidra: FUN_521d_0116 | max_primary_goal_priority
 * Max priority among primary slots matching (x,y,code).
 */
int max_primary_goal_priority(int nation_id, int x, int y, int code) {
  int best = 0;
  for (int i = 0; i < VICEROY_AI_PRIMARY_SLOTS; ++i) {
    uint8_t *s = primary_slot(nation_id, i);
    if ((int8_t)s[0] == x && (int8_t)s[1] == y && (int8_t)s[2] == code &&
        (int8_t)s[3] >= best) {
      best = (int8_t)s[3];
    }
  }
  return best;
}

/*
 * Ghidra: FUN_521d_016a | upsert_primary_goal
 * If matching (x,y,code) already has priority ≥ prio, no-op.
 * Else find first slot with lower prio or empty code; shift_down; write.
 * Args: nation, x, y, code, priority.
 */
void upsert_primary_goal(int nation_id, int x, int y, int code, int prio) {
  for (int i = 0; i < VICEROY_AI_PRIMARY_SLOTS; ++i) {
    uint8_t *s = primary_slot(nation_id, i);
    if ((int8_t)s[0] == x && (int8_t)s[1] == y && (int8_t)s[2] == code &&
        prio <= (int8_t)s[3]) {
      return; /* already have equal-or-better */
    }
  }
  for (int i = 0; i < VICEROY_AI_PRIMARY_SLOTS; ++i) {
    uint8_t *s = primary_slot(nation_id, i);
    if ((int8_t)s[3] < prio || (int8_t)s[2] == -1) {
      primary_goal_shift_down(nation_id, i); /* via 2a1f_04e8 → 0072 */
      s = primary_slot(nation_id, i);
      s[0] = (uint8_t)x;
      s[1] = (uint8_t)y;
      s[2] = (uint8_t)code;
      s[3] = (uint8_t)prio;
      return;
    }
  }
}

/*
 * Ghidra: FUN_521d_0214 | upsert_secondary_goal
 * Same pattern on nation×16 secondary table (−0x6156).
 */
void upsert_secondary_goal(int nation_id, int x, int y, int code, int prio) {
  for (int i = 0; i < VICEROY_AI_SECONDARY_SLOTS; ++i) {
    uint8_t *s = secondary_slot(nation_id, i);
    if ((int8_t)s[0] == x && (int8_t)s[1] == y && (int8_t)s[2] == code &&
        prio <= (int8_t)s[3]) {
      return;
    }
  }
  for (int i = 0; i < VICEROY_AI_SECONDARY_SLOTS; ++i) {
    uint8_t *s = secondary_slot(nation_id, i);
    if ((int8_t)s[3] < prio || (int8_t)s[2] == -1) {
      secondary_goal_shift_down(nation_id, i);
      s = secondary_slot(nation_id, i);
      s[0] = (uint8_t)x;
      s[1] = (uint8_t)y;
      s[2] = (uint8_t)code;
      s[3] = (uint8_t)prio;
      return;
    }
  }
}

/*
 * Ghidra: FUN_521d_02be | upsert_work_queue
 * Insert into global 16-slot work queue when score higher than occupant or
 * slot id < 0. Records: id (int16), score (int16), flag_a, flag_b.
 */
void upsert_work_queue(int id, int score, uint8_t flag_a, uint8_t flag_b) {
  for (int i = 0; i < VICEROY_AI_WORK_QUEUE_SLOTS; ++i) {
    int16_t *w = (int16_t *)(void *)work_slot(i);
    if (w[1] < score || w[0] < 0) {
      work_queue_shift_down(i);
      w = (int16_t *)(void *)work_slot(i);
      w[0] = (int16_t)id;
      w[1] = (int16_t)score;
      ((uint8_t *)w)[4] = flag_a;
      ((uint8_t *)w)[5] = flag_b;
      return;
    }
  }
}

/*
 * Ghidra: FUN_521d_031c | clear_work_queue
 * Set all 16 work-queue ids to −1.
 */
void clear_work_queue(void) {
  for (int i = 0; i < VICEROY_AI_WORK_QUEUE_SLOTS; ++i) {
    *(int16_t *)(void *)work_slot(i) = -1;
  }
}

/*
 * Ghidra: FUN_521d_0342 | promote_secondary_to_primary
 * 6d8e plan pass (2a1f_0578): clear all 64 primary slots, then upsert each
 * live secondary goal into primary (2a1f_0470 → 016a).
 */
void promote_secondary_to_primary(int nation_id) {
  for (int i = 0; i < VICEROY_AI_PRIMARY_SLOTS; ++i) {
    clear_primary_goal_slot(nation_id, i); /* 2a1f_04a0 → 0000 */
  }
  for (int i = 0; i < VICEROY_AI_SECONDARY_SLOTS; ++i) {
    uint8_t *s = secondary_slot(nation_id, i);
    if ((int8_t)s[2] >= 0) {
      upsert_primary_goal(nation_id, (int8_t)s[0], (int8_t)s[1], (int8_t)s[2],
                          (int8_t)s[3]);
    }
  }
}

/*
 * Ghidra: FUN_521d_03a6 | clear_secondary_goal_slots
 * Clear all 16 secondary slots for nation (code=0xff, prio=0).
 * Catalog once said “primary”; table offsets are secondary (−0x6154).
 */
void clear_secondary_goal_slots(int nation_id) {
  for (int i = 0; i < VICEROY_AI_SECONDARY_SLOTS; ++i) {
    uint8_t *s = secondary_slot(nation_id, i);
    s[2] = VICEROY_AI_GOAL_EMPTY;
    s[3] = 0;
  }
}

/* ====================================================================== */
/* Scoring helpers                                                        */
/* ====================================================================== */

/*
 * Ghidra: FUN_521d_03d0 | founding_expansion_urgency
 * Returns 0..N urgency from colony-count targets, nation hire flags
 * (DS −0x6d68), and related bookkeeping. Early game with few colonies
 * often returns 8. Full formula PARKED detail — see decomp ~87058.
 */
int founding_expansion_urgency(int nation_id) {
  (void)nation_id;
  /* if colony_count < 0x30 and hire/found flags… return computed; else 0 */
  return 0; /* skeleton — structure only */
}

/*
 * Ghidra: FUN_521d_0492 | colony_count_balance_flags
 * Compare continent colony target vs counted colonies; return bitfield
 * (−1/0/1 +2/+4 flags). Used by unit priority.
 */
int colony_count_balance_flags(int unk_goal, int continent_or_nation) {
  (void)unk_goal;
  (void)continent_or_nation;
  return 0;
}

/*
 * Ghidra: FUN_521d_052c | unit_desirability_score
 * Type-based adjustments (pioneer +2, soldier −2, scout −3, colonist −2/−4)
 * plus diplo/founding terms. Clamped so positive results become 0.
 */
int unit_desirability_score(int nation_id, int unit_index, int unk) {
  (void)nation_id;
  (void)unit_index;
  (void)unk;
  return 0;
}

/*
 * Ghidra: FUN_521d_0600 | composite_unit_priority
 * desirability (052c) + balance (0492) + founding urgency (03d0); floor 0.
 */
int composite_unit_priority(int nation_id, int unit_index) {
  (void)nation_id;
  (void)unit_index;
  return 0;
}

/*
 * Ghidra: FUN_521d_0656 | walk_unit_stack_to_end
 * Follow transport chain (281f_02e4) until −1; return last index.
 */
int walk_unit_stack_to_end(int unit_index) {
  int last = -1;
  while (unit_index >= 0) {
    last = unit_index;
    /* unit_index = FUN_281f_02e4(...); */
    break; /* annotated structure only */
  }
  return last;
}

/* ====================================================================== */
/* Founding / contact probes                                              */
/* ====================================================================== */

/*
 * Ghidra: FUN_521d_06ae | pick_best_adjacent_founding_tile
 *
 * For dirs 0..8 (8 = stay on param tile): score land, in-bounds, non-ocean
 * neighbors. Prefer empty / own-nation presence; add terrain-class yield and
 * explore-mask terms when param_4≠0. Returns best dir index (0..8).
 *
 * Called from Euro FUN_521d_20e6 via 2a1f_04ac when scoring founding moves.
 * Linux: ai_euro_found_tile_from_landfall is PORT DEBT vs this scorer.
 *
 * param_1 = nation, param_2/3 = x/y, param_4 = score-extras gate,
 * param_5 = wagon-vs-not filter bit.
 */
int pick_best_adjacent_founding_tile(int nation_id, int x, int y,
                                     int score_extras, unsigned wagon_filter) {
  int best_dir = 8;
  int best_score = -1;
  for (int dir = 0; dir <= 8; ++dir) {
    int nx = x + k_dir8_dx[dir];
    int ny = y + k_dir8_dy[dir];
    int ok = 0;
    int tribe = tile_tribe_or_presence(nx, ny);
    if (tribe < 0) {
      ok = 1;
    } else {
      /* own tribe tile + unit filters — see decomp LAB_521d_0713 */
      (void)nation_id;
      (void)wagon_filter;
    }
    if (map_tile_in_bounds(nx, ny) && !ocean_or_high_seas(nx, ny) &&
        (dir == 8 || ok)) {
      int score = 0; /* terrain_class yield byte @ 0x2f77 + extras */
      if (score_extras) {
        /* neighbor explore/owner walk — PARKED detail */
        (void)tile_explore_mask;
        (void)continent_id;
      }
      (void)terrain_class_at;
      if (score > best_score) {
        best_score = score;
        best_dir = dir;
      }
    }
  }
  return best_dir;
}

/*
 * Ghidra: FUN_521d_0896 | filter_profession_by_distance_wealth
 * For Indian-range professions (param_2>3): reject unless relation/distance
 * and optional unit-wealth gates pass. Else return profession unchanged.
 */
int filter_profession_by_distance_wealth(int nation_id, int profession,
                                         int has_context, int unit_index) {
  (void)nation_id;
  (void)has_context;
  (void)unit_index;
  if (profession > 3) {
    /* distance / wealth gates — return −1 to reject */
  }
  return profession;
}

/*
 * Ghidra: FUN_521d_0906 | probe_adjacent_contact_claim
 * Scan 8 neighbors of (x,y). Same ocean-class as origin: if foreign owner
 * or tribe, ask claim helper (2a1f_056c). May require armed cargo on stack.
 * Writes side result to DS:0x9ea8. Returns claim profession or −1.
 */
int probe_adjacent_contact_claim(int x, int y, int nation_id, int unk) {
  (void)x;
  (void)y;
  (void)nation_id;
  (void)unk;
  (void)unit_index_on_tile;
  (void)tile_owner_or_presence;
  /* *(int *)0x9ea8 = −1; … */
  return -1;
}
