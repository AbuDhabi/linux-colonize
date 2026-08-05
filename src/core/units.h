#ifndef COLONIZE_UNITS_H
#define COLONIZE_UNITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/map.h"
#include "core/ss.h"

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
  int profession; /* NAMES.TXT @JOB index; 28 = none (COL1 plain colonist) */
  int tools; /* carried tools (Pioneers); 0–100 in steps of 20 */
  int muskets; /* 0 or 50 when armed */
  int horses; /* 0 or 50 when mounted */
  int home_tribe_id; /* DOS unit+0x06 / DS:314a; -1 = none */
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
 * T0 land combat: attack vs defense (+ fortified ×2). Probability =
 * attack/(attack+defense). Winner stays; loser despawned. Naval / mixed: no fight.
 * Returns true if attacker wins (defender removed).
 */
bool units_resolve_land_combat(
  ColonizeUnitPool* pool,
  int attacker_id,
  int defender_id,
  ColonizeDosRng* rng
);

/* After units_try_move: 0 none, 1 attacker won, -1 attacker lost. */
int units_last_combat_outcome(void);

/* @ORDERS indices (NAMES.TXT). */
#define UNITS_ORDER_NONE 0
#define UNITS_ORDER_SENTRY 1
#define UNITS_ORDER_GOTO 3
#define UNITS_ORDER_FORTIFY 5
#define UNITS_ORDER_FORTIFIED 6
#define UNITS_GOTO_NONE 0xFF

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
/* One adjacent step toward goto (or clear orders if arrived). */
bool units_advance_goto_one_step(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
);
/* Walk until MP exhausted, arrived, or blocked. Clears orders on arrival. */
bool units_advance_goto(
  ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies
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
