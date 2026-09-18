#ifndef COLONIZE_CORE_AI_EURO_INTERNAL_H
#define COLONIZE_CORE_AI_EURO_INTERNAL_H

/*
 * Stage seams for ai_euro.c's ai_euro_unit_act dispatcher (ai_euro_act_*)
 * and its colony-goals helpers (ai_euro_colony_goals_*). The ctx struct and
 * status enum live here (not just in ai_euro.c) so tests/unit/test_stage_seams.c
 * can build a `struct ai_euro_act_ctx` and call one stage directly. Under a
 * normal build the stage functions stay `static` (see core/internal.h);
 * their prototypes are declared here only when COLONIZE_TESTING is defined.
 */

#include "core/ai_goals.h"
#include "core/colony.h"
#include "core/internal.h"
#include "core/turn.h"
#include "core/units.h"

typedef enum {
  AI_EURO_ACT_CONTINUE = 0, /* stage fell through — run the next one */
  AI_EURO_ACT_RETURN = 1    /* stage ended the act (was a bare `return;`) */
} AiEuroActStatus;

/* Act-local state shared between the stages of one ai_euro_unit_act call. */
struct ai_euro_act_ctx {
  ColonizeTurnContext* ctx;
  ColonizeUnit* u; /* re-read after every call that can free/replace the unit */
  int nation_id;
  int is_ship;
  /* ship band */
  int exited_europe;
  int at_war;
  int treasure_aboard;
  /* land band */
  const char* uname;
  int is_land_hunter;
  int is_scout;
  int is_treasure;
  int is_missionary;
  int at_war_land;
  int land_war_hunted;
  int scout_explored;
  int treasure_routed;
  int missionary_contacted;
  int peace_border_hunted;
  int wagon_hauled;
  int pioneer_improved;
  int lumberjack_fielded;
  int miner_fielded;
  int farmer_fielded;
  int fisherman_fielded;
  int planter_fielded;
  int workplace_assigned;
  int goal_x;
  int goal_y;
  int goal_code;
};

/*
 * FUN_5952_035e indoor-workplace want-weight scorer (raw 94784-94860, asm
 * 5952:1ef7-5952:2193). Every field is one DOS read the weight arms make, so
 * the scorer itself is a pure function a unit test can drive. Ledger slots
 * are DOS's 20-word scratch arrays (16 cargos + hammers 16 / crosses 17 /
 * bells 18); see colony_craft.c's DS:0x8dc8 / DS:0x8e0a header.
 */
#define AI_EURO_5952_LEDGER_SLOTS 20
#define AI_EURO_5952_HAMMERS 16
#define AI_EURO_5952_CROSSES 17
#define AI_EURO_5952_BELLS 18

typedef struct AiEuro5952Want {
  int gross[AI_EURO_5952_LEDGER_SLOTS]; /* DS:0x8dc8, -0x7238 */
  int owner_nation;                     /* colony +0x1a */
  int human_nation;                     /* DS:0x5398 */
  unsigned char wealth_rank[4];         /* DS:0x917c, 0 = richest */
  int year;                             /* DS:0x538a */
  int turn;                             /* DS:0x538e */
  int independence;                     /* DS:0x5382 & 1 */
  int jefferson;                        /* FUN_281f_07b4(owner, 0x0f) */
  int nation_flag_bit4;                 /* *(byte*)DS:0x84fc & 4 */
  int tories;                           /* (pop*(100-SoL%)+50)/100, 0 under WoI */
  int capitol_level;                    /* colony +0x96 */
  int population;                       /* colony +0x1f */
  int wants_construction;               /* colony +0x1d & 0x80 */
  int press_chain_count;                /* FUN_281f_0ab0(0x13) */
  unsigned char sell_price[64];         /* DS:0x84bc, nation*0x10 + cargo */
} AiEuro5952Want;

COLONIZE_INTERNAL int ai_euro_5952_want_weight(const AiEuro5952Want* w, int job, int out_cargo);
COLONIZE_INTERNAL int ai_euro_5952_fallback_job(
  int has_church, int lumber_surplus, int preacher_count
);
COLONIZE_INTERNAL int ai_euro_5952_job_score(
  const AiEuro5952Want* w, int job, int out_cargo, int qty
);

#ifdef COLONIZE_TESTING
void ai_euro_colony_goals_unit_contact(ColonizeTurnContext* ctx, int nation_id);
void ai_euro_colony_goals_colony_labor(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  AiEuroInventory* inv, int urgency
);
void ai_euro_colony_goals_colony_work(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
);
void ai_euro_colony_goals_colony_garrison(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
);
void ai_euro_colony_goals_foreign_colonies(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv
);
void ai_euro_colony_goals_food_emergency(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv
);
void ai_euro_colony_goals_tribe_seeds(ColonizeTurnContext* ctx, int nation_id);
void ai_euro_colony_goals_producers(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv, int urgency
);
void ai_euro_colony_goals_ship_found(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv, int urgency
);
void ai_euro_colony_goals_bind_founders(ColonizeTurnContext* ctx, int nation_id);

int ai_euro_5952_indoor_pass_enabled(void);

/* FUN_5952_035e construction-project cascade (bugs.md #483). */
COLONIZE_INTERNAL void ai_euro_5952_build_cascade(
  ColonizeTurnContext* ctx, ColonizeColony* col
);
COLONIZE_INTERNAL void ai_euro_5952_set_ring1_threat(int colony_id, int ring1);
/* FUN_5952_035e equip-arm candidate scorer (raw 94318-94345). */
COLONIZE_INTERNAL int ai_euro_5952_equip_pick(const ColonizeColony* c, int target);
/* FUN_5952_035e carpenter-staffing arm, per-pass election (raw 94690-94740). */
COLONIZE_INTERNAL int ai_euro_5952_carpenter_pick(
  ColonizeColony* c, const bool* placed, int n, int pass, int start, int* out_prof
);
AiEuroActStatus ai_euro_act_pioneer_corridor(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_soldier_staging(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_ship_europe_exit(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_ship_first_colony_course(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_ship_war_trade(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_ship_sail(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_ship_arrival(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_hunt_scout(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_treasure(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_roles(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_fortify(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_goal_consume(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_goal_dispatch(struct ai_euro_act_ctx* a);
void ai_euro_act_ship(struct ai_euro_act_ctx* a);
void ai_euro_act_land(struct ai_euro_act_ctx* a);
#endif /* COLONIZE_TESTING */

#endif /* COLONIZE_CORE_AI_EURO_INTERNAL_H */
