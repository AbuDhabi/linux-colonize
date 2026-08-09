#ifndef COLONIZE_UNITS_H
#define COLONIZE_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/map.h"
#include "core/ss.h"

typedef struct EuropeScreen EuropeScreen;

/*
 * Set Col1 save used by units_try_move for FF combat hooks (Washington promote,
 * Drake naval, Paul Revere auto-arm). turn_refresh_moves_for_nation sets this.
 * Pass NULL to clear. Pointer is not owned.
 */
void units_set_ff_col1(const ColonizeCol1Save* col1);

/* Optional live map for has_unit occupancy maintenance (spawn/move/despawn). */
void units_set_occupancy_map(ColonizeWorldMap* map);

/*
 * Optional post-win native settlement fallout context for
 * units_resolve_land_combat_ff (FUN_5fef_31ea-shaped). When col1/map are non-NULL
 * and attacker beats defender nation>=4, units_try_native_settlement_fallout runs.
 * conquest_gold: caller-known treasure amount, or -1 → Cortes peels FUN_5fef_31ea
 * amount via combat rng (non-Cortes still skips). Pass NULL map to disable.
 */
void units_set_native_fallout_context(
  ColonizeCol1Save* col1,
  ColonizeWorldMap* map,
  int conquest_gold
);

/*
 * Optional colony pool for land combat fortification defense
 * (Stockade/Fort/Fortress). Pass NULL to clear. Pointer is not owned.
 * units_try_move sets this from its colonies arg before combat.
 */
void units_set_combat_colonies(const ColonizeColonyPool* colonies);

/*
 * Destroy native village Col1 record at (x,y) if present. Clears map owner
 * nibble to 0xf and remaps unit home_tribe_id. Returns tribe nation_id (>=4)
 * or -1. Does not invent treasure gold.
 */
int col1_destroy_tribe_at(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int x,
  int y
);

/*
 * FUN_5fef_31ea conquest treasure gold (×100 from DOS amount byte).
 * rich_capital: stack-local -0xcc ← ColonizeCol1TribeState.capital.
 * Returns 0 if no treasure / no rng. Cite: viceroy_unpacked.c ~101407–101495.
 */
int units_cortes_conquest_treasure_gold(
  const ColonizeCol1Save* col1,
  int attacker_nation_id,
  ColonizeDosRng* rng,
  int rich_capital
);

/*
 * Post-win fallout stand-in for FUN_5fef_31ea (structural):
 * If defender was native on a tribe tile and no other same-nation Braves remain
 * on that tile after win, destroy tribe. Cortes treasure when gold_amount>0
 * (caller-known) or gold_amount<=0 with Cortes + rng peel. May adjust Indian
 * relation via ai_diplo helpers when col1 is set.
 */
bool units_try_native_settlement_fallout(
  ColonizeCol1Save* col1,
  ColonizeUnitPool* units,
  ColonizeWorldMap* map,
  int attacker_nation_id,
  int defender_nation_id,
  int tile_x,
  int tile_y,
  int gold_amount,
  ColonizeDosRng* rng
);

/*
 * Thin FUN_65dd_0004 scaffold: Scout on rumour tile clears it via map_clear_rumour.
 * With de Soto: reveal radius (positive-only, no invented gold). Without de Soto:
 * clear only; full RNG outcome table PARKED.
 */
bool units_resolve_lcr_rumour(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng
);

/* Original COLONY.SAV can hold well over 64 map units (natives + Europeans). */
#define COLONIZE_UNITS_MAX 256
#define COLONIZE_UNIT_TYPES_MAX 32
#define COLONIZE_UNIT_CARGO_MAX 6 /* Man-O-War hold size */

typedef enum ColonizeUnitDomain {
  COLONIZE_UNIT_DOMAIN_LAND = 0,
  COLONIZE_UNIT_DOMAIN_SEA = 1
} ColonizeUnitDomain;

typedef struct ColonizeUnitType {
  char name[32];
  int icon_sprite; /* ICONS.SS 0-based blit index (@UNIT icon is 1-based in NAMES.TXT) */
  int movement;
  int attack;
  int defense;
  int cargo;
  int cost; /* NAMES.TXT @UNIT cost field (Europe purchase uses screenshot gold table) */
  ColonizeUnitDomain domain;
} ColonizeUnitType;

typedef struct ColonizeUnit {
  int id;
  int type_index;
  int x;
  int y;
  int moves_left;
  bool active;
  int nation_id; /* 0..3 European, 4..11 native tribes (COL1) */
  int aboard_ship_id; /* -1 = on map; else id of carrying ship */
  int cargo_ids[COLONIZE_UNIT_CARGO_MAX]; /* passenger unit ids (ships only) */
  int cargo_count;
  /* Commodity holds (ships/wagons): type is @CARGO index; amount 0 = empty. */
  int hold_goods_type[COLONIZE_UNIT_CARGO_MAX];
  int hold_goods_amount[COLONIZE_UNIT_CARGO_MAX];
  int orders; /* @ORDERS: 0=none, 1=sentry, 3=goto, … */
  int goto_x; /* UNITS_GOTO_NONE (0xFF) = none */
  int goto_y;
  int follow_unit_id; /* -1 none; target when orders==UNITS_ORDER_FOLLOW */
  int profession; /* NAMES.TXT @JOB index; 28 = none (COL1 plain colonist) */
  int tools; /* carried tools (Pioneers); 0–100 in steps of 20 */
  int muskets; /* 0 or 50 when armed */
  int horses; /* 0 or 50 when mounted */
  int home_tribe_id; /* DOS unit+0x06 / DS:314a; -1 = none */
  int turns_worked; /* COL1 unit+0x16; Brave pulse / labor counter */
  int last_dir; /* DOS unit facing / Col1 facing; 0..7 for AI scoring */
  uint8_t col1_unknown15; /* round-trip; bit7 = ship damaged */
  /*
   * DOS unit+0x07 / Col1 ai_plan. Starter saves use 0x58 ('X') on essentially
   * every unit; spawn defaults to COL1_UNIT_UNKNOWN16_HI_DEFAULT.
   */
  uint8_t col1_ai_plan;
  uint8_t col1_vis_mask; /* DOS nation high nibble (unit byte+3 >> 4); 0x10<<euro */
} ColonizeUnit;

typedef struct ColonizeUnitPool {
  ColonizeUnitType types[COLONIZE_UNIT_TYPES_MAX];
  int type_count;
  ColonizeUnit units[COLONIZE_UNITS_MAX];
  int unit_count;
  int selected_id;
  int next_id;
} ColonizeUnitPool;

bool units_load_types(ColonizeUnitPool* pool, const ColonizeMsgCatalog* names);
void units_reset(ColonizeUnitPool* pool);

int units_find_type(const ColonizeUnitPool* pool, const char* name);
int units_spawn(ColonizeUnitPool* pool, int type_index, int x, int y);
/* Spawn even if the tile already has a unit (COL1 stacks / passengers). */
int units_spawn_allow_stack(ColonizeUnitPool* pool, int type_index, int x, int y);
/* Set nation_id and OR owner euro visibility bit (FUN_1427_0992). */
void units_set_nation(ColonizeUnit* unit, int nation_id);
/*
 * Spawn a Treasure Train at (x,y) for nation_id with COL1 LE16 gold in
 * hold_goods_amount[0]=lo / [1]=hi (same bridge as game_loop / ai_euro cash).
 * Cite: Colonization.pdf Treasure Trains; NAMES "Treasure"; decomp
 * FUN_5fef_31ea post-win native fallout (callers supply gold — no invented
 * rate here). Uses allow_stack (conquest tile may hold the winner).
 * Returns unit id, or -1.
 */
int units_spawn_treasure_train(
  ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int gold
);
/*
 * FUN_3844_0004 EOT treasure tick: Treasure on map not on own Euro colony
 * increments turns_worked (COL1 unit+0x16); after >8 turns despawn. On own
 * colony tile resets counter to 0. Returns number of Treasures removed.
 * Optional status receives a short line when any despawn. Cite:
 * FUNCTION_CATALOG FUN_3844_0004; Colonization.pdf Treasure Trains.
 */
int units_tick_treasure_outside_colony(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  char* status,
  size_t status_size
);
/*
 * Cortes free king galleon stand-in: each Treasure of nation on an own coastal
 * colony → europe_cash_treasure (tax = Crown share) + despawn. Cite: fandom
 * Hernan Cortes; GAME.TXT @KINGGALLEON3. Syncs col1 nation gold. Returns
 * number cashed. PARK: KINGGALLEON2 / voyage chrome.
 */
int units_cortes_cash_coastal_treasures(
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  ColonizeWorldMap* map,
  EuropeScreen* europe,
  ColonizeCol1Save* col1,
  int nation_id
);
bool units_despawn(ColonizeUnitPool* pool, int unit_id);
int units_id_at(const ColonizeUnitPool* pool, int x, int y);
ColonizeUnit* units_get(ColonizeUnitPool* pool, int unit_id);
const ColonizeUnit* units_get_const(const ColonizeUnitPool* pool, int unit_id);
const ColonizeUnitType* units_type(const ColonizeUnitPool* pool, int type_index);
bool units_is_sea(const ColonizeUnitPool* pool, int unit_id);
bool units_is_on_map(const ColonizeUnit* unit);

/* Equipment the unit carries into a new colony warehouse when founding. */
void units_founder_loot(
  const ColonizeUnitPool* pool,
  int unit_id,
  int* out_tools,
  int* out_muskets,
  int* out_horses
);

bool units_can_enter(
  const ColonizeUnitPool* pool,
  int type_index,
  const ColonizeWorldMap* map,
  int x,
  int y,
  int mover_id,
  const ColonizeColonyPool* colonies
);
/* Destination MP cost (terrain + road/river); sea units always 1. */
int units_move_cost(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y
);
/*
 * DOS FUN_465b gate (non-combat, deterministic half): afford if cost <= moves_left,
 * OR the unit still has its full allotment (spent MP == 0). Partial overspend is
 * decided only in units_try_move via dos_rng_range(1, cost) <= remaining.
 */
bool units_can_afford_move_cost(
  const ColonizeUnitPool* pool,
  int unit_id,
  int cost
);
bool units_try_move(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng /* nullable; required for partial overspend / combat rolls */
);

/*
 * T0 land combat: attack vs defense (+ fortified ×2, or colony fortification
 * +100%/+150%/+200% when units_set_combat_colonies / try_move colonies set —
 * fortification replaces fortify ×2 on that tile). Probability =
 * attack/(attack+defense). Winner stays; loser despawned. Naval / mixed: no fight.
 * When col1 is non-NULL and winner nation owns Washington (PEDIA/wiki George
 * Washington; docs/fandom_col1994.md: non-veteran soldiers/dragoons who win
 * always upgrade), promote winner name/type like 1eca. col1 may be NULL (no FF
 * promote). Returns true if attacker wins.
 */
bool units_resolve_land_combat_ff(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
);

/*
 * AI/contact wrapper: same as units_resolve_land_combat_ff with col1 from
 * units_set_ff_col1 (g_units_ff_col1). Callers that always passed NULL missed
 * Washington promote; turn_refresh_moves_for_nation sets the global.
 */
bool units_resolve_land_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
);

/*
 * Transfer commodity holds from loser ship into winner before naval despawn
 * (FUN_5fef_016c-shaped hold plunder). Passengers are not transferred.
 * Returns total goods amount moved into winner holds (0 if none/full).
 */
int units_plunder_ship_holds(ColonizeUnitPool* pool, int winner_id, int loser_id);

/*
 * T0 naval combat: same attack/defense roll as land; ships only.
 * Winner keeps the tile; loser despawned after hold plunder into winner.
 * When col1 is non-NULL and a side is Privateer whose nation owns Drake
 * (PEDIA/wiki: privateer combat strength +50%), that side's attack or defense
 * is multiplied by 3/2. col1 may be NULL (no Drake bonus).
 */
bool units_resolve_naval_combat_ff(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng,
  const ColonizeCol1Save* col1
);

/*
 * AI/king wrapper: same as units_resolve_naval_combat_ff with col1 from
 * units_set_ff_col1 (g_units_ff_col1). Callers that always passed NULL missed
 * Drake privateer *3/2; turn_refresh_moves_for_nation sets the global.
 * Cite: PEDIA/wiki Francis Drake; founding_fathers.c FF_FRANCIS_DRAKE.
 */
bool units_resolve_naval_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
);

/*
 * Coastal Fort/Fortress naval fire strength (FUN_364b_03f6).
 * Fort: 4*(1+arty); Fortress: 8*(1+arty). Artillery/Cannon on colony tile
 * (owner nation). Stockade alone → 0. Cite: decomp local_12*local_c*4;
 * fandom Fort/Fortress. PARK: ship-slow formula.
 */
int units_coastal_fort_attack_strength(
  const ColonizeColonyPool* colonies,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units
);

/*
 * EOT pulse: each Fort/Fortress colony fires on adjacent ocean ships that are
 * at war with the colony owner, or Privateers (peace ignored). Fort win →
 * sink ship (no hold plunder). Fort loss → no effect (no temp attacker).
 * Returns ships sunk. Cite: FUN_364b_03f6.
 */
int units_coastal_fort_fire_pulse(
  ColonizeUnitPool* units,
  const ColonizeColonyPool* colonies,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeDosRng* rng
);

/* After units_try_move: 0 none, 1 attacker won, -1 attacker lost. */
int units_last_combat_outcome(void);

/* @ORDERS indices (NAMES.TXT) + Col1 AI order bytes seen in saves. */
#define UNITS_ORDER_NONE 0
#define UNITS_ORDER_SENTRY 1
#define UNITS_ORDER_GOTO 3
#define UNITS_ORDER_FORTIFY 5
#define UNITS_ORDER_FORTIFIED 6
#define UNITS_ORDER_AI_SAIL 11 /* Euro AI ship course (TURN fixtures) */
#define UNITS_ORDER_AI_MOVE 12 /* Euro AI coastal / land course */
#define UNITS_ORDER_FOLLOW 13 /* Stick to another unit (Brave escort / AI) */
#define UNITS_GOTO_NONE 0xFF

/* True if orders byte means "follow goto_x/y". */
static inline bool units_orders_follow_goto(int orders) {
  return orders == UNITS_ORDER_GOTO || orders == UNITS_ORDER_AI_SAIL ||
         orders == UNITS_ORDER_AI_MOVE;
}

/* True if unit is ordered to stick to another unit id. */
static inline bool units_orders_is_follow(int orders) {
  return orders == UNITS_ORDER_FOLLOW;
}

void units_clear_orders(ColonizeUnitPool* pool, int unit_id);
/*
 * Set sentry / fortify / fortified. Clears goto. Land units only for fortify.
 * Sentry/fortify spend remaining MP (moves_left = 0). Returns false if invalid.
 */
bool units_set_orders(ColonizeUnitPool* pool, int unit_id, int orders);
/* Fortify: orders=FORTIFY, spend MP; next nation refresh → FORTIFIED. */
bool units_order_fortify(ColonizeUnitPool* pool, int unit_id);
/* Sentry on map (or already-aboard). Spends MP. */
bool units_order_sentry(ColonizeUnitPool* pool, int unit_id);
/* Despawn unit (map disband). False if missing. */
bool units_disband(ColonizeUnitPool* pool, int unit_id);
/* Wake sentry/fortified/fortify-in-progress and restore full MP. */
bool units_wake(ColonizeUnitPool* pool, int unit_id);
/* True if unit skips selection until woken (sentry or fortified). */
bool units_orders_skip_turn(const ColonizeUnit* unit);

/* Set Go-To order (does not move); returns false if unit/dest invalid. */
bool units_set_goto(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/*
 * Order unit to stick to target_unit_id (UNITS_ORDER_FOLLOW).
 * Clears tile goto. Both units must be active and on-map; same domain preferred
 * (sea follows sea, land follows land). Returns false if invalid.
 * Cite: Brave escort / FUN_4d56_14fe needs follow-unit orders (tile goto alone is not enough).
 */
bool units_follow_unit(ColonizeUnitPool* pool, int unit_id, int target_unit_id);
/*
 * One step toward the follow target's current tile (retarget each call).
 * Clears FOLLOW if target missing/inactive. rng may be NULL.
 */
bool units_advance_follow_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
);
/*
 * Next adjacent step toward goto (DOS FUN_6662 tiers: sign-step / cost flood / BFS).
 * Writes (out_x,out_y); returns false if stuck or already there.
 */
bool units_next_goto_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int* out_x,
  int* out_y
);
/* One adjacent step toward goto (or clear orders if arrived). rng may be NULL. */
bool units_advance_goto_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
);
/* Walk until MP exhausted, arrived, or blocked. Clears orders on arrival. */
bool units_advance_goto(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng
);
/* One step for every Go-To unit that can move; returns how many stepped. */
int units_advance_all_goto_one_step(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
);
/* Advance every unit with Go-To until stuck; returns how many took at least one step. */
int units_advance_all_goto(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
);

bool units_is_pioneer(const ColonizeUnitPool* pool, int unit_id);
/* Plow (clear forest if needed) / road on unit tile. Spends 20 tools + remaining moves. */
bool units_pioneer_plow(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size
);
bool units_pioneer_road(
  ColonizeUnitPool* pool,
  int unit_id,
  ColonizeWorldMap* map,
  char* err,
  size_t err_size
);

/* True for high-seas / sea-lane tiles (terrain index 26). */
bool units_on_high_seas(const ColonizeWorldMap* map, int x, int y);
bool units_find_water_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int occupant_id,
  int* out_x,
  int* out_y
);
bool units_find_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int* out_x,
  int* out_y
);
/* Prefer western rim of eastern high seas near prefer_y — Atlantic approach. */
bool units_find_eastern_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int prefer_y,
  int* out_x,
  int* out_y
);

/* Board a land unit onto an adjacent ship. Returns false if capacity/adjacency fails. */
bool units_board(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/* Board without adjacency check (COL1 import; passenger already stacked on ship tile). */
bool units_board_stacked(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/* Unload oldest passenger from ship onto dest (must be enterable land). */
bool units_unload(
  ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/* Unload a specific passenger onto dest. */
bool units_unload_passenger(
  ColonizeUnitPool* pool,
  int ship_id,
  int pax_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/* First cargo with moves_left > 0, or -1. */
int units_first_cargo_with_moves(const ColonizeUnitPool* pool, int ship_id);
/*
 * Pick an adjacent land tile the ship's passengers can enter (8-neighbour).
 * Prefers tiles nearer to (prefer_x, prefer_y) when prefer coords are valid;
 * pass prefer_x < 0 to ignore. Returns false if none.
 */
bool units_pick_landfall_tile(
  const ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int prefer_x,
  int prefer_y,
  int* out_x,
  int* out_y
);
/*
 * Unload every passenger onto dest (must be enterable land adjacent/same).
 * Wakes sentry cargo. Does not change pool->selected_id. Returns count unloaded.
 */
int units_landfall_unload_all(
  ColonizeUnitPool* pool,
  int ship_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y,
  const ColonizeColonyPool* colonies
);
/*
 * Colony dock: remove all passengers from the ship onto (x,y), clear sentry.
 * Does not change ship position. Returns number disembarked.
 */
int units_disembark_all(
  ColonizeUnitPool* pool,
  int ship_id,
  int x,
  int y
);

/* Collect on-map units at tile plus cargo of ships there (for stack popup). */
#define UNITS_TILE_STACK_MAX 32
int units_collect_tile_stack(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int* out_ids,
  int out_max
);

int units_ship_capacity(const ColonizeUnitPool* pool, int ship_id);

/* Ships and wagon trains that can carry commodity holds. */
bool units_is_transport(const ColonizeUnitPool* pool, int unit_id);
/* Number of commodity hold slots (from @UNIT cargo field). */
int units_goods_hold_count(const ColonizeUnitPool* pool, int unit_id);
/*
 * Add goods into an empty hold (or stack into a matching partial hold).
 * amount is clamped to remaining room (max 100 per hold). Returns amount loaded.
 */
int units_load_goods(ColonizeUnitPool* pool, int unit_id, int cargo_type, int amount);
/*
 * Remove goods from one hold index. Writes type/amount unloaded (optional outs).
 * Returns amount unloaded (0 if empty/invalid).
 */
int units_unload_goods_hold(
  ColonizeUnitPool* pool,
  int unit_id,
  int hold_index,
  int* out_cargo_type,
  int* out_amount
);
/* First non-empty goods hold index, or -1. */
int units_first_goods_hold(const ColonizeUnitPool* pool, int unit_id);
/* Snapshot passenger type indices (for Europe harbor transfer). */
int units_export_cargo_types(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_types,
  int out_max
);
/*
 * Despawn ship and all passengers; fills passenger type list and optional
 * commodity hold arrays for Europe harbor transfer.
 */
bool units_despawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_id,
  int* out_type_index,
  char* out_name,
  size_t out_name_size,
  int* out_cargo_types,
  int* out_cargo_count,
  int cargo_max,
  int* out_hold_goods_type,
  int* out_hold_goods_amount,
  int hold_max
);
/* Spawn ship at (x,y); recreate passengers and optional commodity holds. */
int units_spawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_type_index,
  int x,
  int y,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
);

void units_end_turn(ColonizeUnitPool* pool);

/* NAMES.TXT @JOB indices used for unit skills (COL1 profession byte). */
#define UNITS_JOB_COLONIST 19 /* Free Colonists */
#define UNITS_JOB_PIONEER 20  /* Hardy Pioneers */
#define UNITS_JOB_SOLDIER 21  /* Veteran Soldiers */
#define UNITS_JOB_SCOUT 22    /* Seasoned Scouts */
#define UNITS_JOB_DRAGOON 23  /* Veteran Dragoons */
#define UNITS_JOB_NONE 28     /* no expert skill (plain Pioneer/Soldier) */

/* Equipped map/fence icons (ICONS.SS); expert variants when profession matches. */
#define UNITS_ICON_PIONEER 73
#define UNITS_ICON_SOLDIER 74
#define UNITS_ICON_SCOUT 75
#define UNITS_ICON_DRAGOON 76
#define UNITS_ICON_HARDY_PIONEER 101
#define UNITS_ICON_VETERAN_SOLDIER 102
#define UNITS_ICON_SEASONED_SCOUT 103
#define UNITS_ICON_VETERAN_DRAGOON 104
/* Working inside a colony (unequipped citizen sprites). */
#define UNITS_ICON_HARDY_PIONEER_WORK 58
#define UNITS_ICON_VETERAN_SOLDIER_WORK 59

#define UNITS_EQUIP_MUSKETS 50
#define UNITS_EQUIP_HORSES 50
#define UNITS_EQUIP_TOOLS_MAX 100
#define UNITS_EQUIP_TOOLS_STEP 20

/* Human starter: Caravel (Dutch Merchantman) on eastern high seas with Pioneer+Soldier. */
void units_new_world_start(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
  int difficulty
);

/*
 * Spawn European starter fleet (ship + Pioneer + Soldier) at (x,y).
 * Skills from difficulty/nation (Discoverer/Explorer / French Hardy / Spanish Veteran).
 * Returns ship unit id or -1.
 */
int units_spawn_euro_starter_fleet(
  ColonizeUnitPool* pool,
  int nation_id,
  int difficulty,
  int x,
  int y,
  int goto_x,
  int goto_y
);

/* Panel label: "Hardy Pioneer", "Veteran Soldier", unit type name, … */
const char* units_display_name(const ColonizeUnitPool* pool, const ColonizeUnit* unit);

/*
 * ICONS.SS index for a colonist working inside a colony (no field equipment).
 * Hardy Pioneer → #58, Veteran Soldier → #59; else @UNIT icon for unit_type_index.
 */
int units_working_colonist_sprite(
  const ColonizeUnitPool* pool,
  int unit_type_index,
  int profession
);

bool units_deploy_colonist(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  const char* immigrant_name
);

int units_map_sprite(const ColonizeUnitPool* pool, int unit_id);
/* Col1 @UNIT index after equipment remap (Scout/Soldier/Dragoon/Pioneer). */
int units_display_type_index(const ColonizeUnitPool* pool, int unit_id);
/* selected_visible: when false, hide the selected unit (blink off frame). */
void units_render_on_map(
  const ColonizeUnitPool* pool,
  const ColonizeSpriteSheet* nation_sheet,
  const ColonizeFont* font,
  ColonizeFramebuffer8* framebuffer,
  int view_x,
  int view_y,
  int view_cols,
  int view_rows,
  int tile_w,
  int tile_h,
  int origin_x,
  int origin_y,
  bool selected_visible,
  const ColonizeWorldMap* fog_map, /* nullable — skip tiles unseen by fog_nation */
  int fog_nation
);

#endif
