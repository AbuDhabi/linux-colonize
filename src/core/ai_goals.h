#ifndef COLONIZE_AI_GOALS_H
#define COLONIZE_AI_GOALS_H

#include <stdint.h>

#include "core/colony.h"
#include "core/map.h"

/* Linux port of FUN_521d_0000…0906 goal tables (annotated euro_goals.c). */

#define AI_GOAL_EMPTY 0xff
#define AI_GOAL_CONTACT 0
#define AI_GOAL_FOUND 1
#define AI_GOAL_LABOR 3
#define AI_GOAL_MILITARY 4
#define AI_GOAL_COLONY 5
#define AI_GOAL_MIL_EXPAND 7
#define AI_GOAL_COLONY_ALT 8

#define AI_PRIMARY_SLOTS 64
#define AI_SECONDARY_SLOTS 16
#define AI_WORK_SLOTS 16 /* VICEROY_AI_WORK_QUEUE_SLOTS 0x10 */

typedef struct AiGoalSlot {
  int8_t x;
  int8_t y;
  uint8_t code; /* AI_GOAL_EMPTY or goal code */
  uint8_t prio;
} AiGoalSlot;

typedef struct AiWorkSlot {
  int16_t id;
  int16_t score;
  uint8_t flag_a;
  uint8_t flag_b;
} AiWorkSlot;

typedef struct AiNationGoals {
  AiGoalSlot primary[AI_PRIMARY_SLOTS];
  AiGoalSlot secondary[AI_SECONDARY_SLOTS];
} AiNationGoals;

/* Per-nation shortage / demand scratch filled by dispatcher inventory. */
typedef struct AiEuroInventory {
  int tools_short;
  int lumber_short;
  int muskets_short;
  int food_short;
  int ore_short; /* 5cf6-shaped Ore tally for Expert Ore Miner dock hire */
  int found_flags; /* dock/construction tally stand-in for −0x5f48 */
  int profession_demand[16]; /* decremented by passenger types */
  int colony_count;
  int urgency; /* founding_expansion_urgency stand-in */
} AiEuroInventory;

void ai_goals_reset(void);
void ai_goals_clear_primary_slot(int nation_id, int slot);
void ai_goals_clear_secondary_slots(int nation_id);
void ai_goals_promote_secondary_to_primary(int nation_id);
void ai_goals_upsert_primary(int nation_id, int x, int y, int code, int prio);
void ai_goals_upsert_secondary(int nation_id, int x, int y, int code, int prio);
void ai_goals_clear_work_queue(void);
void ai_goals_upsert_work(int id, int score, uint8_t flag_a, uint8_t flag_b);
int ai_goals_max_primary_prio(int nation_id, int x, int y, int code);
const AiGoalSlot* ai_goals_primary(int nation_id, int slot);
const AiWorkSlot* ai_goals_work(int slot);
int ai_goals_best_found_tile(int nation_id, int* out_x, int* out_y);

AiEuroInventory* ai_goals_inventory(int nation_id);
void ai_goals_inventory_clear(int nation_id);

/*
 * FUN_521d_06ae — pick best adjacent founding tile.
 * Returns 1 and writes out_x/out_y, or 0 if none.
 */
int ai_goals_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int x,
  int y,
  int* out_x,
  int* out_y
);

#endif
