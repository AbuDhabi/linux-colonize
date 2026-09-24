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
#include "core/col1_save.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/world.h"
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
  /* land band */
  const char* uname;
  ColonizeUnitKind ukind; /* unit's @UNIT type-row kind (never string-derived) */
  int is_land_hunter;
  int is_scout;
  int is_treasure;
  int at_war_land;
  int land_war_hunted;
  int scout_explored;
  int treasure_routed;
  int peace_border_hunted;
  int wagon_hauled;
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

/* FUN_521d_20e6 shared exit tail LAB_521d_589e, local_76 == 8 (raw 90378-90386). */
COLONIZE_INTERNAL void ai_euro_20e6_stay_tail_589e(ColonizeUnit* u);


#define AI_20E6_VILLAGE_MAX 128 /* ai_euro_land.c village-visit latch size */

/*
 * Opaque per-nation scratch for the raw body's `nation+0x48/0x49/0x4a`
 * (+0x49/+0x4a = the nation's banked MUSKET lots / raw muskets, identified
 * 2026-09-24 from FUN_5952_035e raw 94362-94367 crediting a lot out of an
 * AI colony's musket surplus; genuine arithmetic, see the normalization
 * loop below) and the two Europe-dock "training slot"
 * counters at `DS:0xa0da`/`0xa0db`. NOT reused from the real
 * `ColonizeCol1Nation` struct, for two separate reasons (both re-checked
 * 2026-09-09, after smell audit #52 moved the Indian-hostility sticky
 * stand-in off `+0x48` and onto the one dead byte `+0x4b`/`unknown26[11]`):
 *   - `+0x48` is `col1_save.h`'s `king_grace_counter`, a REAL DOS quantity
 *     (FUN_4d56_4528's decrementing grace/waiver counter) that the port may
 *     read but must never write — so it cannot host scratch either;
 *   - `+0x49` is the one remaining Linux stand-in collision here
 *     (`privateer_spawn_mask`), and `+0x4a` (`unknown26_pad`) is DOS's raw
 *     banked musket total.
 * Same reasoning as the `0x53de` correction: don't reuse a live field on an
 * unconfirmed reading.
 */
typedef struct Ai5d04HireScratch {
  int8_t delay_48;              /* DS nation+0x48 */
  /* DS nation+0x49: the musket-LOT bank, one lot = 50 muskets. Credited by
   * FUN_5952_035e raw 94362-94367 (an AI colony with >199 muskets and no
   * garrison shortfall banks a lot); spent by FUN_521d_5d04's Europe-dock
   * hire arm to arm a recruit for free instead of buying 50 muskets. */
  int8_t musket_bank_lots;
  /* DS nation+0x4a: the same bank's RAW musket total, kept in step with
   * +0x49 by the +-0x32 normalize loop at FUN_521d_5d04 raw 84272-84278
   * (lots ~= raw/50). FUN_521d_5d04 raw 84334-84343 spends it 50 at a time
   * on the hire arm's horse charge. */
  int32_t musket_bank_raw;
  int8_t colonies_need_muskets;  /* DS 0xa0db (per nation-turn) */
  int8_t colonies_need_tools;    /* DS 0xa0da (per nation-turn, minus own Pioneers) */
} Ai5d04HireScratch;

/* ===== Cross-file seams of the ai_euro.c split (2026-09-23) =====
 * ai_euro.c was 21k lines; it is now split into ai_euro{,_colony_jobs,_expand,
 * _europe,_goals,_land,_ship,_act}.c. The declarations below are the symbols
 * used across those files; everything else stayed `static` in its own file.
 * Code moved verbatim — these are the only de-static'd names.
 * ===================================================================== */

int ai_euro_env_flag(const char* name, int dflt);
extern int ai_euro_s_5952_census_vet_soldier[COLONIZE_COLONIES_MAX];
extern uint8_t ai_euro_s_deferred_found[COLONIZE_UNITS_MAX];
extern uint8_t ai_euro_s_unloaded_this_turn[COLONIZE_UNITS_MAX];
extern int8_t ai_euro_s_euro_last_dir[COLONIZE_UNITS_MAX];
extern uint8_t ai_euro_s_founded_colony_turn[COLONIZE_COLONIES_MAX];
extern uint32_t ai_euro_s_violate_last_turn[COLONIZE_UNITS_MAX];
extern uint8_t ai_euro_s_euro_roam_wander[COLONIZE_UNITS_MAX];
extern uint8_t ai_euro_s_euro_ship_route_latch[COLONIZE_UNITS_MAX];
void ai_euro_refresh_continent_stance(ColonizeTurnContext* ctx, int nation_id);
int ai_euro_rival_strength_at(int nation_id, int continent_id);
int ai_euro_continent_stance_at(int nation_id, int continent_id);
int ai_euro_in_europe(int x, int y);
void ai_euro_sync_aboard_cargo_xy(ColonizeUnitPool* units, ColonizeUnit* ship);
void ai_euro_resolve_landfall_goto(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  int* out_x,
  int* out_y
);
int ai_euro_ocean_3558_first_leg_tip(
  const ColonizeWorldMap* map,
  int from_x,
  int from_y,
  int landfall_x,
  int landfall_y,
  int goal_x,
  int goal_y,
  int max_steps,
  int* out_x,
  int* out_y
);
int ai_euro_ocean_3558_empty_cruise_tip(
  const ColonizeWorldMap* map,
  int found_x,
  int found_y,
  int* out_x,
  int* out_y
);
int ai_euro_06ae_first_colony_from_landfall(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeUnitPool* units,
  int nation_id,
  int landfall_x,
  int landfall_y,
  int* out_x,
  int* out_y
);
int ai_euro_recover_landfall_from_ship(
  int ship_x,
  int ship_y,
  int* out_x,
  int* out_y
);
int ai_euro_land_adjacent_to(
  const ColonizeWorldMap* map,
  int wx,
  int wy,
  int* out_x,
  int* out_y
);
int ai_euro_ship_has_land_adjacent(const ColonizeWorldMap* map, int sx, int sy);
int ai_euro_pick_unload_land(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  int pax_id,
  int prefer_x,
  int prefer_y,
  int avoid_x,
  int avoid_y,
  int* out_x,
  int* out_y
);
int ai_euro_unload_pax_at(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
  ColonizeUnit* pax,
  int dest_x,
  int dest_y,
  int orders,
  int goto_x,
  int goto_y
);
ColonizeUnitKind ai_euro_unit_kind(const ColonizeUnitPool* pool, const ColonizeUnit* u);
int ai_euro_name_is_pioneer(ColonizeUnitKind kind);
int ai_euro_name_is_soldier(ColonizeUnitKind kind);
int ai_euro_coastal_staging_from_landfall(
  const ColonizeWorldMap* map,
  int landfall_x,
  int landfall_y,
  int* out_x,
  int* out_y
);
int ai_euro_foreign_unit_at(const ColonizeTurnContext* ctx, const ColonizeUnit* u, int x, int y);
int ai_euro_try_post_found_coast_cruise(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
);
typedef struct AiEuroShipPressure {
  uint8_t frigate_colonies; /* DS:0xa89b */
  uint8_t other_colonies;   /* DS:0xa89a */
  int frigate_pop;          /* DS:0x9e52 */
  int other_pop;            /* DS:0x9e54 */
} AiEuroShipPressure;
extern AiEuroShipPressure ai_euro_s_ship_pressure[4];
void ai_euro_ship_pressure_reset(int nation_id);
int ai_euro_colony_food_short(const ColonizeColony* c);
int ai_euro_colony_wanted_size(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
);
void ai_euro_refresh_colony_ai_flags(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c
);
int ai_euro_colony_wants_construction_labor(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
);
void ai_euro_prefer_peace_construction(ColonizeTurnContext* ctx, int nation_id);
int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id);
int ai_euro_is_military_name(ColonizeUnitKind k);
int ai_euro_is_land_war_hunter(ColonizeUnitKind kind);
int ai_euro_is_artillery_name(ColonizeUnitKind kind);
int ai_euro_fortify_with_quota(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u,
  int colony_id
);
int ai_euro_land_is_fortified(const ColonizeUnit* u);
int ai_euro_land_is_passive_orders(const ColonizeUnit* u);
int ai_euro_pick_founding_tile(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  const ColonizeCol1Save* col1,
  const ColonizeUnitPool* units,
  int nation_id,
  int x,
  int y,
  int* out_x,
  int* out_y
);
int ai_euro_nearest_military_goal(
  int nation_id,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
);
int ai_euro_is_treasure_name(ColonizeUnitKind kind);
int ai_euro_europe_sail_target(
  ColonizeTurnContext* ctx,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
);
bool ai_euro_5952_job_is_expert(int job);
void ai_euro_5952_ledgers(
  const ColonizeWorld* world,
  const ColonizeColonyPool* pool,
  const ColonizeColony* col,
  const ColonizeCol1Save* col1,
  int gross[AI_EURO_5952_LEDGER_SLOTS],
  int demand[AI_EURO_5952_LEDGER_SLOTS]
);
extern int ai_euro_s_5952_ring1[COLONIZE_COLONIES_MAX];
extern int ai_euro_s_5952_docks_started[COLONIZE_COLONIES_MAX];
extern int ai_euro_s_5952_census_nonexpert[COLONIZE_COLONIES_MAX]; /* aiStack_68[0x13] */
void ai_euro_5952_build_cascade(
  ColonizeTurnContext* ctx, ColonizeColony* col
);
void ai_euro_colony_tick_28c8_reassign(
  ColonizeTurnContext* ctx, int nation_id
);
int ai_euro_unit_is_food_labor(const ColonizeUnitPool* units, const ColonizeUnit* u);
void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);
int ai_euro_is_ship_type(const ColonizeUnitPool* units, int unit_id);
int ai_euro_tiles_near(int ax, int ay, int bx, int by);
int ai_euro_unit_hold_has_capacity(const ColonizeUnitPool* units, const ColonizeUnit* w);
int ai_euro_unit_hold_has_cargo_type(
  const ColonizeUnitPool* units,
  const ColonizeUnit* w,
  int cargo_type
);
int ai_euro_hauler_free_holds(const ColonizeUnitPool* units, const ColonizeUnit* u);
int ai_euro_20e6_ship_hold_budget(
  const ColonizeUnitPool* units, const ColonizeUnit* ship
);
int ai_euro_20e6_origin_get(const ColonizeUnit* u);
void ai_euro_20e6_origin_set(ColonizeUnit* u, int colony_id);
extern uint32_t ai_euro_s_4393_claim_turn[COLONIZE_UNITS_MAX];
extern int ai_euro_s_4393_claim_colony[COLONIZE_UNITS_MAX];
extern int ai_euro_s_4393_claim_valid[COLONIZE_UNITS_MAX];
int ai_euro_4393_work_queue_haul_pick(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  const ColonizeUnit* hauler,
  int* out_x,
  int* out_y
);
extern uint8_t ai_euro_s_20e6_wagon_errand[COLONIZE_UNITS_MAX];
extern int16_t ai_euro_s_0a60_work_registered[4];
int ai_euro_0a60_work_registered(int nation_id);
void ai_euro_wagon_errand_clear_all(void);
int ai_euro_try_wagon_haul(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* wagon
);
void ai_euro_found_with_unit(ColonizeTurnContext* ctx, ColonizeUnit* founder, int nation_id);
void ai_euro_colony_inventory(ColonizeTurnContext* ctx, int nation_id);
int ai_euro_type_is_wagon_name(ColonizeUnitKind kind);
extern ColonizeTurnContext* ai_euro_s_5d04_ctx;
extern int ai_euro_s_5d04_nation;
int ai_euro_5d04_linux_type_for(const ColonizeUnitPool* pool, int dos_code);
extern Ai5d04HireScratch ai_euro_s_5d04_hire_scratch[4];
void ai_euro_nation_planning(ColonizeTurnContext* ctx, int nation_id);
#define AI_EURO_ACT_GOAL UNITS_ORDER_AI_SAIL /* 0x0b — pursue +0x314d/e */
#define AI_EURO_ACT_STEP UNITS_ORDER_AI_MOVE /* 0x0c — one committed step */
#define AI_EURO_ACT_ADJACENT 10              /* FUN_521d_0a60 raw 87567 */
#define AI_EURO_F3148_KEEP 0xd1u
#define AI_EURO_F3148_ROAM 0x02u  /* roam_reeval_pending (act_state 5/6) */
#define AI_EURO_F3148_FOUND 0x04u /* stack_has_founders_or_military */
#define AI_EURO_F3148_MIL 0x08u   /* stack_has_military */
#define AI_EURO_F3148_SPARE 0x20u /* spare-transport mark */
int ai_euro_0a60_goods_holds_used(const ColonizeUnitPool* units, const ColonizeUnit* u);
void ai_euro_0a60_goal_orders_structural(ColonizeTurnContext* ctx, int nation_id);
void ai_euro_colony_goals(ColonizeTurnContext* ctx, int nation_id);
int ai_euro_village_nation_at(const ColonizeCol1Save* col1, int x, int y);
int ai_euro_score_move(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int goal_x,
  int goal_y,
  int* out_dx,
  int* out_dy
);
#define AI_20E6_TYPE_COUNT 23
extern uint32_t ai_euro_s_20e6_type_cache_valid;
void ai_euro_20e6_refresh_type_cache(const ColonizeUnitPool* units);
extern const int8_t ai_euro_k_20e6_ring20_dx[20];
extern const int8_t ai_euro_k_20e6_ring20_dy[20];
void ai_euro_5952_improve_best_plot(ColonizeTurnContext* ctx, ColonizeColony* col);
void ai_euro_5952_tools_supply_and_connect(
  ColonizeTurnContext* ctx, ColonizeColony* col
);
extern uint8_t ai_euro_s_20e6_explorers[16];
extern uint8_t ai_euro_s_20e6_explore_fatigue[COLONIZE_UNITS_MAX];
extern int8_t ai_euro_s_20e6_hop_steps[COLONIZE_UNITS_MAX];
extern int16_t ai_euro_s_20e6_hop_slot[COLONIZE_UNITS_MAX];
int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);
int ai_euro_20e6_type_combat(int dos_type);
int ai_euro_20e6_type_flags(int dos_type);
int ai_euro_20e6_nearest_colony(
  const ColonizeTurnContext* ctx, int x, int y, int nation, int cid, int* out_dist
);
int ai_euro_20e6_nearest_village(const ColonizeTurnContext* ctx, int x, int y, int* out_dist);
int ai_euro_20e6_colony_owner_at(const ColonizeTurnContext* ctx, int x, int y);
int ai_euro_20e6_tribe_or_presence(const ColonizeTurnContext* ctx, int x, int y);
typedef struct Ai20e6Unit {
  int nation;
  int x;
  int y;
  int dos_type;   /* unit+0x3146 */
  int flags;      /* DS:0x523d row */
  int combat;     /* DS:0x5236 row */
  int is_ship;    /* iStack_34: type ∈ [0xd,0x12] */
  int cid;        /* iStack_38: continent at unit tile (−1 water) */
  int home_colony; /* uStack_62: nearest own colony id, −1 none */
  int home_dist;  /* iStack_2e: DS:0x8db8 after that search */
  int home_cid;   /* iStack_2c: −2 when no own colony */
  int any_colony_dist; /* iStack_74: nearest colony of any nation */
  int stance;     /* uStack_2a: G-table DS:0x9870[nation][cid], 5 off-land */
  int village_idx; /* uStack_ac */
  int village_dist; /* iStack_a0 */
  int unit_river; /* uStack_84 */
  int unit_road;  /* uStack_5a */
  int act_state;  /* unit+0x314c == ColonizeUnit.orders */
  int order_code; /* unit+0x314b */
  int explorer;   /* iStack_6a */
  int turn;
  int year;
  int woi;
} Ai20e6Unit;
void ai_euro_20e6_prologue(ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation, Ai20e6Unit* s);
int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid);
int ai_euro_10ec_land_units_on(const ColonizeTurnContext* ctx, int nation, int cid);
int ai_euro_20e6_foreign_colony_on(const ColonizeTurnContext* ctx, int nation, int cid);
int ai_euro_20e6_open_continents(const ColonizeTurnContext* ctx, int nation);
extern uint8_t ai_euro_s_20e6_village_visited[AI_20E6_VILLAGE_MAX];
int ai_euro_20e6_wander_step(
  ColonizeTurnContext* ctx, ColonizeUnit* u, Ai20e6Unit* s, int* out_attack, int* out_saw_foe
);
int ai_euro_20e6_ship_far_roam(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s);
int ai_euro_20e6_ship_wander_act(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id, int busy);
int ai_euro_20e6_treasure_cash_in(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int nation_id
);
int ai_euro_20e6_47b9_dead_end(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
int ai_euro_20e6_wagon_origin_walk(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* u
);
int ai_euro_20e6_hs_cadence_enabled(void);
int ai_euro_20e6_adjacent_foreign_09dc(
  const ColonizeTurnContext* ctx, int x, int y, int nation_id
);
int ai_euro_20e6_457e_type_gate(
  const ColonizeTurnContext* ctx, const ColonizeUnit* u, int dos_type
);
/* bVar20 seed (raw 88556-88573); see ai_euro_land.c. */
int ai_euro_20e6_bvar20_seed(
  const ColonizeTurnContext* ctx, const ColonizeUnit* u, int dos_type
);
int ai_euro_20e6_457e_hs_cadence(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
void ai_euro_20e6_stay_tail_589e(ColonizeUnit* u);
int ai_euro_move_scoring_gate(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
void ai_euro_try_violate_notify(ColonizeTurnContext* ctx, ColonizeUnit* u);
void ai_euro_try_attack(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty);
int ai_euro_coastal_water_near(
  const ColonizeWorldMap* map,
  int cx,
  int cy,
  int from_x,
  int from_y,
  int* out_x,
  int* out_y
);
int ai_euro_is_cargo_ship_name(ColonizeUnitKind kind);
int ai_euro_20e6_load_pick(
  ColonizeTurnContext* ctx,
  const ColonizeColony* c,
  int nation,
  int is_ship
);
int ai_euro_20e6_wagon_village_errand(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* wagon
);
int ai_euro_try_ship_trade_haul(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
);
int ai_euro_ship_enter_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship);
int ai_euro_try_ship_europe_export(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* ship
);
int ai_euro_tile_under_enemy_fort_fire(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* viewer,
  int x,
  int y
);
int ai_euro_naval_try_flee_fort_fire(ColonizeTurnContext* ctx, ColonizeUnit* u);
int ai_euro_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f,
  int is_naval
);
int ai_euro_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map);
int ai_euro_land_try_adjacent_colony_seize(ColonizeTurnContext* ctx, ColonizeUnit* u);
int ai_euro_land_try_adjacent_village_seize(ColonizeTurnContext* ctx, ColonizeUnit* u);
void ai_euro_20e6_ship_cargo_counts(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship,
  int* pioneers, int* mil, int* scouts, int* milvet, int* civ
);
void ai_euro_20e6_goal_fold(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* ship,
  int nation,
  int found_probe,
  int* pioneers,
  int* civ,
  int* carry80
);
int ai_euro_20e6_unload_mask(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation);
int ai_euro_20e6_unload_by_mask(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation, int mask
);
int ai_euro_20e6_colony_sail_pick(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship, int nation, int mil, int pioneers_b4,
  int urgency, int* out_x, int* out_y
);
void ai_euro_unload_settle(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation_id);
int ai_euro_nation_settler_aboard(ColonizeTurnContext* ctx, int nation_id);
int ai_euro_nation_pioneer_aboard(ColonizeTurnContext* ctx, int nation_id);
int ai_euro_recover_nation_landfall(
  ColonizeTurnContext* ctx, int nation_id, int* lf_x, int* lf_y
);
int ai_euro_try_first_colony_land(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
int ai_euro_land_engage_adjacent(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int* hunted
);
void ai_euro_settlers_ashore(
  const ColonizeTurnContext* ctx,
  int nation_id,
  int* out_pioneer,
  int* out_soldier,
  int* lf_x,
  int* lf_y
);
void ai_euro_first_colony_ship_course(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int nation_id,
  int fx,
  int fy,
  int pioneer_aboard,
  int any_cargo,
  int pioneer_ashore,
  int soldier_ashore
);
int ai_euro_5952_equip_pick(const ColonizeColony* c, int target);
AiEuroActStatus ai_euro_act_pioneer_corridor(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_soldier_staging(struct ai_euro_act_ctx* a);
void ai_euro_act_ship(struct ai_euro_act_ctx* a);
void ai_euro_act_land(struct ai_euro_act_ctx* a);
int ai_euro_ship_dos_enabled(void);
void ai_euro_act_ship_dos(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);

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
/* ai_euro_colony_goals_bind_founders retired 2026-09-23 — bugs.md #707. */

int ai_euro_5952_indoor_pass_enabled(void);

/* FUN_5952_035e construction-project cascade (bugs.md #483). */
COLONIZE_INTERNAL void ai_euro_5952_build_cascade(
  ColonizeTurnContext* ctx, ColonizeColony* col
);
COLONIZE_INTERNAL void ai_euro_5952_set_ring1_threat(int colony_id, int ring1);
/*
 * FUN_5952_035e's two closing specialist arms (bugs.md #571/#572), and the
 * cross-arm latch the Docks branch of the cascade writes into DOS's
 * [BP+0xff62] (bugs.md #586): non-zero means the cascade started Docks for
 * that colony this tick, which suppresses ARM 2's expert purchase.
 */
COLONIZE_INTERNAL void ai_euro_5952_specialist_arms(
  ColonizeTurnContext* ctx, ColonizeColony* col, int n
);
COLONIZE_INTERNAL int ai_euro_5952_docks_started(int colony_id);
/* FUN_5952_035e's field-placement section (raw 94551-94620), bugs.md #585. */
COLONIZE_INTERNAL void ai_euro_colony_tick_28c8_reassign(
  ColonizeTurnContext* ctx, int nation_id
);
/* FUN_5952_035e by-profession census cells aiStack_68[0x13] / [0x15], the two
 * the absorption arm's Soldier/Dragoon case consumes (raw 94242, 94248-94255). */
COLONIZE_INTERNAL void ai_euro_5952_set_absorb_census(
  int colony_id, int nonexpert, int vet_soldier
);
/* FUN_5952_035e absorption + equip arms (raw 94231-94352), hosted in the
 * colony tick as a tile re-scan. `labor_running` is the tick-local iStack_76
 * the Soldier case increments. */
COLONIZE_INTERNAL void ai_euro_5952_absorb_equip(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int* labor_running
);
/* FUN_5952_035e equip-arm candidate scorer (raw 94318-94345). */
COLONIZE_INTERNAL int ai_euro_5952_equip_pick(const ColonizeColony* c, int target);

/* FUN_5952_035e raw 95926-95944: the "train an expert here" pick. Returns the
 * @JOB to buy (COLONIZE_PROF_FREE_COLONIST / 0x1c = buy nothing) and the
 * chosen colonist slot. bugs.md #571. */
COLONIZE_INTERNAL int ai_euro_5952_train_pick(
  const ColonizeColony* c, int n, bool has_docks, int* out_slot
);
/* FUN_5952_035e carpenter-staffing arm, per-pass election (raw 94690-94740). */
COLONIZE_INTERNAL int ai_euro_5952_carpenter_pick(
  ColonizeColony* c, const bool* placed, int n, int pass, int start, int* out_prof
);
/* FUN_5952_035e forced-lumberjack pass, per-pass election (raw 94659-94679). */
COLONIZE_INTERNAL int ai_euro_5952_lumberjack_pick(
  const ColonizeColony* c, const bool* placed, int n, int pass, int start
);
/* FUN_5952_035e AI emergency lumber purchase (raw 94680-94689). */
COLONIZE_INTERNAL void ai_euro_5952_lumber_purchase(
  struct EuropeScreen* eu, struct ColonizeCol1Save* col1, ColonizeColony* col, int turn,
  bool lumber_producer_placed
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
