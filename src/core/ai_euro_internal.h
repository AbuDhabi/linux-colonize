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

#include "core/ai_euro.h"
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
  /* land band */
  const char* uname;
  ColonizeUnitKind ukind; /* unit's @UNIT type-row kind (never string-derived) */
  int is_treasure;
  int scout_explored;
  int treasure_routed;
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
  unsigned char owner_wealth_rank;         /* byte[local_1b0 + 0x917c]: the owner's
                                           rank unless the improvement scan
                                           overwrote local_1b0 */
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

COLONIZE_INTERNAL unsigned char ai_euro_5952_rank_byte(
  const ColonizeCol1Save* col1, const unsigned char rank[4], int owner_index);
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
 * Per-nation-turn per-cargo demand table DS:0xa0cc (0xa0da/0xa0db included).
 * The hire arms' nation bytes +0x48/+0x49/+0x4a are the col1 record's own
 * king_grace_counter / musket_bank_lots / col1_nation_bank_4a() (FUN_521d_5d04
 * raw 84230-84468 reads and writes them through DS:0x84fc).
 */
typedef struct Ai5d04HireScratch {
  /* DS:0xa0cc[16], rebuilt by the FUN_521d_6d8e prelude each nation-turn;
   * cells 15 / 14 are the hire arms' 0xa0db / 0xa0da. */
  int8_t a0cc[16];
} Ai5d04HireScratch;

/* ===== Cross-file seams of the ai_euro.c split (2026-09-23) =====
 * ai_euro.c was 21k lines; it is now split into ai_euro{,_colony_jobs,_expand,
 * _europe,_goals,_land,_ship,_act}.c. The declarations below are the symbols
 * used across those files; everything else stayed `static` in its own file.
 * Code moved verbatim — these are the only de-static'd names.
 * ===================================================================== */

int ai_euro_env_flag(const char* name, int dflt);
extern int ai_euro_s_5952_census_vet_soldier[COLONIZE_COLONIES_MAX];
void ai_euro_refresh_continent_stance(ColonizeTurnContext* ctx, int nation_id);
int ai_euro_rival_strength_at(int nation_id, int continent_id);
int ai_euro_continent_stance_at(int nation_id, int continent_id);
/* Both DOS-unsaved per-continent tables (stance DS:0x9870, rival strength
 * DS:0x9e98), 4x16 bytes each, for the RTST chunk (runtime_ext.c). */
#define AI_EURO_STANCE_TABLES_SIZE 128
void ai_euro_stance_tables_export(uint8_t out[AI_EURO_STANCE_TABLES_SIZE]);
void ai_euro_stance_tables_import(const uint8_t in[AI_EURO_STANCE_TABLES_SIZE]);
/* DS:0x2d12 / DS:0x2d14 sticky anti-spin pair (RTST chunk). */
void ai_euro_sticky_get(int* unit, int* count);
void ai_euro_sticky_set(int unit, int count);
int ai_euro_in_europe(int x, int y);
int ai_euro_europe_lane(int nation_id, int x, int y);
int ai_euro_at_europe_dock(int nation_id, int x, int y);
void ai_euro_sync_aboard_cargo_xy(ColonizeUnitPool* units, ColonizeUnit* ship);
void ai_euro_resolve_landfall_goto(
  ColonizeTurnContext* ctx,
  ColonizeUnit* ship,
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
int ai_euro_foreign_unit_at(const ColonizeTurnContext* ctx, const ColonizeUnit* u, int x, int y);
typedef struct AiEuroShipPressure {
  uint8_t frigate_colonies; /* DS:0xa89b */
  uint8_t other_colonies;   /* DS:0xa89a */
  int frigate_pop;          /* DS:0x9e52 */
  int other_pop;            /* DS:0x9e54 */
} AiEuroShipPressure;
extern AiEuroShipPressure ai_euro_s_ship_pressure[4];
void ai_euro_ship_pressure_reset(int nation_id);
int ai_euro_colony_wanted_size(
  const ColonizeColonyPool* pool,
  const ColonizeColony* c
);
void ai_euro_refresh_colony_ai_flags(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c
);
int ai_euro_at_war_any_peer(const ColonizeCol1Save* col1, int nation_id);
int ai_euro_is_military_name(ColonizeUnitKind k);
int ai_euro_is_artillery_name(ColonizeUnitKind kind);
int ai_euro_land_is_fortified(const ColonizeUnit* u);
int ai_euro_is_treasure_name(ColonizeUnitKind kind);

bool ai_euro_5952_job_is_expert(int job);
/* local_80/local_1e/local_7e placement-section gate (raw 94570-94584,
 * port_bugs.md #1028): local_7e is the return value; out_under_populated and
 * out_horses_have_room hand back the two sub-gates the food pass and pass-2 loops
 * also need. */
bool ai_euro_5952_placement_gate(
  const ColonizeColonyPool* pool, const ColonizeColony* col,
  bool* out_under_populated, bool* out_horses_have_room
);
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
extern int ai_euro_s_5952_arty_plot[COLONIZE_COLONIES_MAX];
extern int ai_euro_s_5952_census_nonexpert[COLONIZE_COLONIES_MAX]; /* aiStack_68[0x13] */
/* local_1b0 = the tick's improvement-arm owner (DS index of the per-nation reads). */
void ai_euro_5952_build_cascade_for_owner(
  ColonizeTurnContext* ctx, ColonizeColony* col, int improvement_owner
);
void ai_euro_colony_tick_5952(ColonizeTurnContext* ctx, int nation_id);
void ai_euro_5952_colony_counters(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
);
void ai_euro_5952_colony_prelude(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
);
void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy);
int ai_euro_is_ship_type(const ColonizeUnitPool* units, int unit_id);
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
extern uint16_t ai_euro_s_0a60_mask_mil[4];
extern uint16_t ai_euro_s_0a60_mask_found[4];
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
int ai_euro_5d04_port_type_for(const ColonizeUnitPool* pool, int dos_code);
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
void ai_euro_5952_improve_best_plot(
  ColonizeTurnContext* ctx, ColonizeColony* col, int* out_improvement_owner, int* out_best_plot
);
ColonizeColony* ai_euro_5952_tools_supply_and_connect(
  ColonizeTurnContext* ctx, ColonizeColony* col
);
extern uint8_t ai_euro_s_20e6_explorers[16];
/* DOS unit+0x3154..+0x3156 (fatigue, hop countdown, hop slot) in the raw hold bytes. */
uint8_t* ai_euro_20e6_hold_scratch(ColonizeUnit* u);
/* The one DOS unit+0x3146 @UNIT code resolver (NAMES.TXT @UNIT row; ships
 * 0x0d..0x12). Miss (no type row, unresolved kind) = -1, which falls outside
 * every code range its callers test. */
int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);
/* @UNIT DEFENSE column (port_bugs.md #651) — exposed for test_regress_ai_tables.c. */
int ai_euro_20e6_unit_col5(const ColonizeUnitPool* pool, int dos_type);
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
  int prio14;     /* iStack_14 = FUN_521d_0600 */
  int turn;
  int year;
  int woi;
} Ai20e6Unit;
int ai_euro_20e6_park_arms_4d2e(ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s);
void ai_euro_20e6_prologue(ColonizeTurnContext* ctx, const ColonizeUnit* u, int nation, Ai20e6Unit* s);
int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid);
int ai_euro_10ec_land_units_on(const ColonizeTurnContext* ctx, int nation, int cid);
int ai_euro_20e6_foreign_colony_on(const ColonizeTurnContext* ctx, int nation, int cid);
int ai_euro_20e6_open_continents(const ColonizeTurnContext* ctx, int nation);
extern uint8_t ai_euro_s_20e6_village_visited[AI_20E6_VILLAGE_MAX];
int ai_euro_20e6_wander_step(
  ColonizeTurnContext* ctx, ColonizeUnit* u, Ai20e6Unit* s, int* out_attack, int* out_saw_foe
);
int ai_euro_20e6_ship_far_roam(
  ColonizeTurnContext* ctx, ColonizeUnit* u, const Ai20e6Unit* s,
  int military_founders, int unload_mask
);
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
int ai_euro_20e6_type_gate_no_manowar(
  const ColonizeTurnContext* ctx, const ColonizeUnit* u, int dos_type
);
int ai_euro_20e6_457e_hs_cadence_snapshot(
  ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id, int stack_other, int delivery_type
);
int ai_euro_20e6_delivery_tallies(
  const ColonizeTurnContext* ctx, const ColonizeUnit* ship, int tally[COLONIZE_CARGO_COUNT]
);

/* `+0x314b = 0x45` ('E'), the Europe-course stamp FUN_48d3_015e writes
 * (raw 77712-77727). The port keeps DOS's byte in `ColonizeUnit.col1_ai_plan`,
 * and the crossing gate in the shared mover (FUN_479b_076e raw 77095-77107)
 * reads it. */
#define AI_EURO_PLAN_EUROPE_BOUND 0x45

/* DS:0x945a[n] / DS:0x9456[n] census tallies and the LAB_521d_3fa6 sail-home
 * course setter — bodies carry the prose (ai_euro_land.c). */
int ai_euro_europe_dock_land_units(const ColonizeUnitPool* units, int nation_id);
int ai_euro_europe_lane_ships(const ColonizeUnitPool* units, int nation_id);
int ai_euro_20e6_3fa6_sail_home(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
int ai_euro_20e6_missionary_arm(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
int ai_euro_20e6_europe_dock_demand(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
int ai_euro_move_scoring_gate(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
/* FUN_5952_035e indoor-expert pass (OVL15 0x1cb2-0x1e65), ahead of the scored
 * indoor pass: seats unplaced indoor experts in their own job. */
void ai_euro_5952_indoor_expert_pass(
  ColonizeTurnContext* ctx, ColonizeColony* col, bool* placed, int n
);
/* FUN_5952_035e LAB_5952_178f: real 28c8 seat of the bad winner, then
 * 169c + 1068(slot, 0x12). */
void ai_euro_5952_178f_unseat(
  const ColonizeWorld* world, ColonizeColony* col, int s, const AiEuro28c8JobCandidate* best
);
void ai_euro_try_attack(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty);
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
int ai_euro_try_ship_trade_haul_snapshot(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* ship, int stack_other
);
int ai_euro_20e6_tasked_ship_berth_arrival(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* ship
);
int ai_euro_ship_enter_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship);
void ai_euro_ship_leave_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship);

int ai_euro_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f,
  int is_naval
);
int ai_euro_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map);
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
  int* promoted_civilian_carry
);
typedef struct AiEuroShipBandCounts {
  int stack_other;
  int pioneers;
  int pioneers_before_fold;
  int military;
  int civilians;
} AiEuroShipBandCounts;
int ai_euro_20e6_unload_mask_snapshot(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation, AiEuroShipBandCounts* counts
);
int ai_euro_20e6_unload_mask(ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation);
int ai_euro_20e6_unload_by_mask(
  ColonizeTurnContext* ctx, ColonizeUnit* ship, int nation, int mask
);
int ai_euro_20e6_colony_sail_pick(
  ColonizeTurnContext* ctx, const ColonizeUnit* ship, int nation, int mil, int pioneers_b4,
  int urgency, int* out_x, int* out_y
);
void ai_euro_act_land(struct ai_euro_act_ctx* a);
void ai_euro_act_ship_dos(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
void ai_euro_goal_walk_479b(ColonizeTurnContext* ctx, ColonizeUnit* u);

/*
 * FUN_5952_035e build-decision cascade ctx (asm 5952:21d4-5952:274b), split
 * into ai_euro_5952_cascade_* stages (port_bugs.md #1107). Fields below
 * `improvement_owner` are the DOS locals the stages share.
 */
typedef struct AiEuro5952Cascade {
  ColonizeTurnContext* ctx;
  ColonizeColonyPool* pool;
  ColonizeColony* col;
  const ColonizeCol1Save* col1;
  int nation;
  int buildable[COLONIZE_BUILDING_TYPES_MAX];
  int n_buildable;
  int improvement_owner; /* DOS local_1b0 */
  int gross[AI_EURO_5952_LEDGER_SLOTS];  /* DS:0x8dc8 */
  int demand[AI_EURO_5952_LEDGER_SLOTS]; /* DS:0x8e0a */
  int pop;
  int human;
  int turn;
  int year;
  int difficulty;
  int musket_price;           /* DS:0x84cb + local_1b0*0x10 */
  int armed_count_for_nation; /* DS:0x9424 + local_1b0 */
  int ring1;
  int food_workers;           /* iVar12 */
  int prof_count[28];
  int experts;
  int tier_max;
  int arty_on_tile;           /* iStack_92 */
  int hungry;                 /* iStack_a0 */
  int ring;                   /* uStack_a2 */
  int ring_nonland;           /* iStack_142 */
  int factory;                /* iStack_2c */
  bool has_shipyard;
} AiEuro5952Cascade;

/* NEXT = fall into the next stage; WANTS = DOS's jump to 2747 (set the
 * wants-construction latch); RETURN = a project was picked. */
typedef enum AiEuro5952CascadeStatus {
  AI_EURO_5952_CASCADE_NEXT,
  AI_EURO_5952_CASCADE_RETURN,
  AI_EURO_5952_CASCADE_WANTS
} AiEuro5952CascadeStatus;

/*
 * FUN_5952_035e colony-tick ctx for one colony, split into
 * ai_euro_colony_tick_* stages (port_bugs.md #1107). `col` is *(DS:0x8542): the
 * road-connect arm may move it to another colony record.
 */
typedef struct AiEuroColonyTick {
  ColonizeTurnContext* ctx;
  ColonizeWorld world;
  int nation_id;
  bool whole_tick;
  ColonizeColony* col;
  int tory_weight;       /* local_7c */
  int improvement_owner; /* local_1b0 */
  int n;
  int prev_job[COLONIZE_COLONY_POP_MAX];
  bool placed[COLONIZE_COLONY_POP_MAX];
  bool fishable;
  int food_have;
  int food_need;
  int food_stock;        /* colony+0x9a */
  bool section_done;     /* goto LAB_5952_178f */
  bool under_populated;  /* local_80 */
  bool horses_have_room; /* local_1e */
  bool placement_open;   /* local_7e */
} AiEuroColonyTick;

#ifdef COLONIZE_TESTING
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
void ai_euro_colony_goals_producers(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv, int urgency
);
/* ai_euro_colony_goals_bind_founders retired 2026-09-23 — port_bugs.md #707. */

int ai_euro_28c8_probe(
  const ColonizeTurnContext* ctx, const ColonizeColony* col, int slot,
  AiEuro28c8JobCandidate* best
);

/* FUN_5952_035e construction-project cascade (port_bugs.md #483). */
COLONIZE_INTERNAL void ai_euro_5952_build_cascade(
  ColonizeTurnContext* ctx, ColonizeColony* col
);
COLONIZE_INTERNAL void ai_euro_5952_set_ring1_threat(int colony_id, int ring1);
/*
 * FUN_5952_035e's two closing specialist arms (port_bugs.md #571/#572), and the
 * cross-arm latch the Docks branch of the cascade writes into DOS's
 * [BP+0xff62] (port_bugs.md #586): non-zero means the cascade started Docks for
 * that colony this tick, which suppresses ARM 2's expert purchase.
 */
/* DS:0x8d72 list (FUN_15eb_09c0): non-ship units on the colony tile, latest
 * arrival first. Returns the count; ids gets unit ids. */
COLONIZE_INTERNAL int ai_euro_5952_tile_stack(
  const ColonizeTurnContext* ctx, const ColonizeColony* c, int* ids, int max
);
COLONIZE_INTERNAL void ai_euro_5952_specialist_arms(
  ColonizeTurnContext* ctx, ColonizeColony* col, int n
);
/* FUN_5952_035e raw 95959-95972: buy the two-Horse starter herd. */
COLONIZE_INTERNAL void ai_euro_5952_seed_horses(
  ColonizeTurnContext* ctx, ColonizeColony* col
);

COLONIZE_INTERNAL int ai_euro_5952_docks_started(int colony_id);
/* FUN_5952_035e's field-placement section (raw 94551-94620), port_bugs.md #585. */
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
 * chosen colonist slot. port_bugs.md #571. */
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
AiEuroActStatus ai_euro_act_ship_europe_exit(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_hunt_scout(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_treasure(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_roles(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_goal_consume(struct ai_euro_act_ctx* a);
AiEuroActStatus ai_euro_act_land_goal_dispatch(struct ai_euro_act_ctx* a);
/* Test-only externs (port_bugs.md #1106), no production caller. */
int ai_euro_5d04_dos_type_code(const ColonizeUnitPool* pool, int type_index);
/* 5d04's Europe dock walk (bound to ai_euro_s_5d04_ctx/_nation): slot indices
 * in DOS tile-chain order, newest arrival first; -1 at the end. */
int ai_euro_5d04_cb_list_iter_first(int list_id);
int ai_euro_5d04_cb_list_iter_next(int prev);
/* Live-tally wrapper of ai_euro_20e6_457e_hs_cadence_snapshot. */
int ai_euro_20e6_457e_hs_cadence(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id);
/* Test-only ai_goals.c externs (port_bugs.md #1106). */
void ai_goals_upsert_secondary(int nation_id, int x, int y, int code, int prio);
AiNationPlanScratch* ai_goals_plan_scratch(int nation_id);
int ai_goals_stack_settler_pick(const ColonizeCol1Unit* units, int unit_count, int unit_index);
/* port_bugs.md #1106: test-only entry points (no production caller). */
int ai_euro_28c8_colonist_job_score_structural(
  const ColonizeTurnContext* ctx, int colony_id, int colonist_slot,
  AiEuro28c8JobCandidate* out_best
);
/* port_bugs.md #1107 stage seams: FUN_5952_035e build cascade + colony tick. */
void ai_euro_5952_cascade_setup(AiEuro5952Cascade* s);
void ai_euro_5952_cascade_census(AiEuro5952Cascade* s);
void ai_euro_5952_cascade_pressure(AiEuro5952Cascade* s);
AiEuro5952CascadeStatus ai_euro_5952_cascade_head(AiEuro5952Cascade* s);
AiEuro5952CascadeStatus ai_euro_5952_cascade_town(AiEuro5952Cascade* s);
AiEuro5952CascadeStatus ai_euro_5952_cascade_military(AiEuro5952Cascade* s);
AiEuro5952CascadeStatus ai_euro_5952_cascade_chain_upgrades(AiEuro5952Cascade* s);
void ai_euro_colony_tick_open(AiEuroColonyTick* t);
void ai_euro_colony_tick_unseat_all(AiEuroColonyTick* t);
void ai_euro_colony_tick_food_pass(AiEuroColonyTick* t);
void ai_euro_colony_tick_pass2(AiEuroColonyTick* t);
void ai_euro_colony_tick_specialist_restore(AiEuroColonyTick* t);
#endif /* COLONIZE_TESTING */

#endif /* COLONIZE_CORE_AI_EURO_INTERNAL_H */
