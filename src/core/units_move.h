#ifndef COLONIZE_UNITS_MOVE_H
#define COLONIZE_UNITS_MOVE_H

#include "core/units.h"
#include "core/world.h"

/* Movement, pathing, goto, orders, MP: split out of units.h for navigability. */

/*
 * FUN_6662_0906 with param_2 = 1 (sea domain): flood cost from (bx,by) to
 * (ax,ay) inside the |a−b| < 8 window, −1 when unreachable/out of window.
 * FUN_4962_0018's colony ship-pressure probe gates on cost 0..5.
 */
int units_short_sea_route_cost(
  const ColonizeWorldMap* map, int ax, int ay, int bx, int by
);

/*
 * Enter-probe outcomes (DOS FUN_4720 / FUN_465b shaped). See docs/move_enter.md.
 * units_can_enter is true only for OK / DOCK (pathfinding / Go-To).
 */
typedef enum ColonizeEnterReason {
  COLONIZE_ENTER_OK = 0,
  COLONIZE_ENTER_DOCK = 1,
  COLONIZE_ENTER_LANDFALL = 2,
  COLONIZE_ENTER_COMBAT_LAND = 3,
  COLONIZE_ENTER_COMBAT_NAVAL = 4,
  COLONIZE_ENTER_BOUNCE_FOREIGN = 5,
  COLONIZE_ENTER_BOUNCE_PEACE = 6,
  COLONIZE_ENTER_BLOCKED_DOMAIN = 7,
  COLONIZE_ENTER_BLOCKED_EDGE = 8,
  COLONIZE_ENTER_BLOCKED_HS_SAIL = 9, /* 4720 reason 5: HS east without sail */
  COLONIZE_ENTER_VILLAGE_ILLEGAL = 10,
  COLONIZE_ENTER_NO_MP = 11,
  COLONIZE_ENTER_BLOCKED = 12,
  COLONIZE_ENTER_BOARD = 13, /* land → ocean tile with own ship that has room */
  COLONIZE_ENTER_VILLAGE_SHIP = 14, /* ship → native village (not landfall); 4528 abort */
  COLONIZE_ENTER_LAKE_BLOCKED = 15, /* ship → inland lake square; GAME.TXT @SHIPLAKE */
  COLONIZE_ENTER_LANDFIRST = 16 /* ship → enemy-occupied land, must unload first; @LANDFIRST */
} ColonizeEnterReason;

ColonizeEnterReason units_enter_probe_w(
  const ColonizeWorld* w,
  int type_index,
  int x,
  int y,
  int mover_id
);
/* Last reason from units_enter_probe / units_try_move (0 if none). */
ColonizeEnterReason units_last_enter_reason(void);
/* Short player status for a probe reason (never NULL). */
const char* units_enter_reason_status(ColonizeEnterReason reason);

bool units_can_enter_w(
  const ColonizeWorld* w,
  int type_index,
  int x,
  int y,
  int mover_id
);

/*
 * Movement points are DOS thirds (FUN_465b_0000 cost head; DS:0x5234 unit
 * table = @UNIT movement * 3, ships +3 with Magellan): a plains step costs
 * 3, a road/colony pair or cardinal minor-river pair costs 1, an ocean tile
 * costs 3. `moves` holds thirds remaining. Native Braves (ai.c) keep
 * their own bookkeeping (moves = DOS spent thirds, max 3).
 */
#define UNITS_MP_PER_TILE 3
/* @UNIT movement * 3 (DOS FUN_1427_065a base). */
int units_type_max_mp(const ColonizeUnitType* type);
/* Per-unit max MP in thirds: type base + 3 for ships when the nation has Magellan. */
int units_max_mp(const ColonizeUnitPool* pool, int unit_id);
/*
 * Movement thirds the unit still has. Euro units store that directly in
 * moves; native units store DOS's spent byte there, so this is the only
 * safe way to ask "how much is left" across both.
 */
int units_remaining_mp(const ColonizeUnitPool* pool, int unit_id);
/* "1", "2/3", "1 1/3" — DOS panel style. */
void units_format_mp(int thirds, char* out, size_t out_size);
/* Destination MP cost in thirds (DOS 465b cost head); sea units always 3. */
int units_move_cost(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y
);
bool units_try_move_w(
  const ColonizeWorld* w,
  int unit_id,
  int dest_x,
  int dest_y
);

/*
 * Observe successful on-map tile moves. The callback is process-global, like
 * the combat/FF context hooks, and is normally installed only by the
 * interactive game loop while AI turns are being shown.
 */
typedef void (*ColonizeUnitsMoveWatchFn)(
  void* user,
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int unit_id,
  int from_x,
  int from_y,
  int to_x,
  int to_y
);
void units_set_move_watch(ColonizeUnitsMoveWatchFn fn, void* user);

/* @ORDERS indices (NAMES.TXT) + Col1 AI order bytes seen in saves. */
#define UNITS_ORDER_NONE 0
#define UNITS_ORDER_SENTRY 1
#define UNITS_ORDER_TRADE_ROUTE 2
#define UNITS_ORDER_GOTO 3
#define UNITS_ORDER_FORTIFY 5
#define UNITS_ORDER_FORTIFIED 6
#define UNITS_ORDER_BUILD_COLONY 7
#define UNITS_ORDER_CLEAR_PLOW 8
#define UNITS_ORDER_BUILD_ROAD 9
#define UNITS_ORDER_AI_SAIL 11 /* Euro AI ship course (TURN fixtures) */
#define UNITS_ORDER_AI_MOVE 12 /* Euro AI coastal / land course */
#define UNITS_ORDER_FOLLOW 13 /* Stick to another unit (Brave escort / AI) */
#define UNITS_GOTO_NONE 0xFF

/* True if orders byte means "follow goto_x/y". */
static inline bool units_orders_follow_goto(int orders) {
  return orders == UNITS_ORDER_GOTO || orders == UNITS_ORDER_AI_SAIL ||
         orders == UNITS_ORDER_AI_MOVE || orders == UNITS_ORDER_TRADE_ROUTE;
}

/* True if unit is ordered to stick to another unit id. */
static inline bool units_orders_is_follow(int orders) {
  return orders == UNITS_ORDER_FOLLOW;
}

void units_clear_orders(ColonizeUnitPool* pool, int unit_id);
/*
 * Set sentry / fortify / fortified. Clears goto. Land units only for fortify.
 * Sentry/fortify spend remaining MP (moves = 0). Returns false if invalid.
 */
bool units_set_orders(ColonizeUnitPool* pool, int unit_id, int orders);
/* Fortify: orders=FORTIFY, spend MP; next nation refresh → FORTIFIED. */
bool units_order_fortify(ColonizeUnitPool* pool, int unit_id);
/* Sentry on map (or already-aboard). Spends MP. */
bool units_order_sentry(ColonizeUnitPool* pool, int unit_id);
/* Begin Trade Route (@ORDERS index 2). Clears goto; spends remaining MP. */
bool units_order_trade_route(ColonizeUnitPool* pool, int unit_id);
/* Despawn unit (map disband). False if missing. */
bool units_disband(ColonizeUnitPool* pool, int unit_id);

/*
 * Thin Pillage (ORDERS): land military (attack>0) on foreign Euro colony →
 * destroy up to 100 of richest warehouse stock (not Food); on non-colony tile
 * with plow/road → clear those improvements. Spends remaining MP.
 * Cite: MENU.TXT @ORDERS Pillage; full 2b5a mega-dispatch body still thin.
 */
bool units_pillage_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size
);
/* Wake sentry/fortified/fortify-in-progress and restore full MP. */
bool units_wake(ColonizeUnitPool* pool, int unit_id);
/* True if unit skips selection until woken (sentry or fortified). */
bool units_orders_skip_turn(const ColonizeUnit* unit);

/* Set Go-To order (does not move); returns false if unit/dest invalid. */
bool units_set_goto_w(
  const ColonizeWorld* w,
  int unit_id,
  int dest_x,
  int dest_y
);
/*
 * True when the unit stands adjacent to a Go To destination that is an Indian
 * settlement tile — its final step is a move INTO the village and must be
 * dispatched through the normal entry flow (FUN_4d56_4528: woodcut 7 +
 * @ACTIONS menu), not silently paced or dropped. bugs.md #418.
 */
bool units_goto_dest_is_village_entry_w(
  const ColonizeWorld* w,
  int unit_id
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
bool units_advance_follow_one_step_w(
  const ColonizeWorld* w,
  int unit_id
);
/*
 * Record a committed goto step (dx,dy in -1..1) for the FUN_6662 anti-backtrack
 * check. units_advance_goto_one_step does this itself; any other stepper that
 * moves a goto-following unit via units_try_move (the AI ship sail loop) must
 * call it after each successful step, or the check compares against stale
 * history and drops correct pathfinder hits.
 */
void units_note_goto_step(int unit_id, int dx, int dy);
/*
 * Next adjacent step toward goto (DOS FUN_6662 tiers: sign-step / cost flood / BFS).
 * Writes (out_x,out_y); returns false if stuck or already there. rng may be
 * NULL (disables the anti-backtrack wiggle reroll; deterministic geometry
 * still runs).
 */
bool units_next_goto_step_w(
  const ColonizeWorld* w,
  int unit_id,
  int* out_x,
  int* out_y
);
/* One adjacent step toward goto (or clear orders if arrived). rng may be NULL. */
bool units_advance_goto_one_step_w(
  const ColonizeWorld* w,
  int unit_id
);
/* Walk until MP exhausted, arrived, or blocked. Clears orders on arrival. */
bool units_advance_goto_w(
  const ColonizeWorld* w,
  int unit_id
);

bool units_is_pioneer(const ColonizeUnitPool* pool, int unit_id);
/*
 * Start/continue pioneer terrain work (DOS FUN_479b_01a6 / 0526).
 * Clear Forest and Plow Fields are separate jobs (P on forest clears only;
 * P on open land plows). Road is R. Each job takes terr_cost[+2 for
 * clear/plow] turns (Hardy Pioneer halves); tools −20 on completion.
 * First call sets orders + one work-tick; further ticks via
 * units_pioneer_work_tick / turn_refresh.
 */
bool units_pioneer_plow_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);
bool units_pioneer_road_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);
/* One work-tick for CLEAR_PLOW / BUILD_ROAD orders. Returns true if unit still
 * has that order (in progress or just started). colonies/ai_popups/messages
 * optional — clear-forest completion grants lumber + @CLEARCUT when set. */
bool units_pioneer_work_tick_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
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
/*
 * FUN_48d3_048e + 0434 — expanding spiral from (start_x,start_y) for a High Seas
 * tile (terrain 0x1a) that is empty or owned by nation_id (-1 = empty only).
 * Cite: viceroy_unpacked.c ~77810; move_scoring.md §ocean.
 */
bool units_spiral_place_hs_near(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
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

#endif
