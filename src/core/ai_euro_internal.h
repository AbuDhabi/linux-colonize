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
