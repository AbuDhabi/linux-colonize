#include "core/units.h"

/*
 * Cargo/passenger holds, boarding, landfall, rendering, start placement.
 *
 * Sections:
 *  - Cargo/passenger holds, boarding, landfall, display name/sprite & map rendering (units_ship_capacity .. units_render_on_map)
 *  - New-world start placement & colonist deployment (units_spawn_euro_starter_fleet .. units_deploy_colonist)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/combat_analysis.h"
#include "core/combat_strength.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/sound.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/village_trade_intel.h"
#include "core/woodcut.h"
#include "platform/diagnostics.h"
#include "core/units_internal.h"

/* ===================== Cargo/passenger holds, boarding, landfall, display name/sprite & map rendering (units_ship_capacity .. units_render_on_map) ===================== */


/* Sea-only view of units_goods_hold_count (UN-16): identical body behind the
 * units_is_sea gate, so a wagon reports 0 capacity but keeps its goods holds. */
int units_ship_capacity(const ColonizeUnitPool* pool, int ship_id) {
  if (!units_is_sea(pool, ship_id)) {
    return 0;
  }
  return units_goods_hold_count(pool, ship_id);
}

bool units_is_transport(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u)) {
    return false;
  }
  if (units_is_sea(pool, unit_id)) {
    return units_goods_hold_count(pool, unit_id) > 0;
  }
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  if (!type) {
    return false;
  }
  return units_type_is_wagon(type) && type->cargo > 0;
}

int units_goods_hold_count(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    return 0;
  }
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  if (!type || type->cargo <= 0) {
    return 0;
  }
  /* Commodity holds share the @UNIT cargo count with passenger slots conceptually;
   * goods use the same slot count (passengers occupy separate cargo_ids). */
  return type->cargo > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX : type->cargo;
}

int units_first_goods_hold(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    return -1;
  }
  const int n = units_goods_hold_count(pool, unit_id);
  for (int i = 0; i < n; ++i) {
    if (units_unit_hold_amount(u, i) > 0) {
      return i;
    }
  }
  return -1;
}

/*
 * DOS FUN_15eb_30b8 goods packing, on a raw (types, amounts) hold pair so the
 * Europe harbor mirror (europe.c europe_buy_cargo) and the unit pool can share
 * one implementation: top up matching partial holds to 100 first, then append
 * into free slots. The append pass is budgeted because DOS only appends while
 * `holds_occupied < cargo_cap`; pass n_holds for an unbudgeted append.
 */
int goods_pack_into_holds(
  int* hold_types,
  int* hold_amounts,
  int n_holds,
  int cargo_type,
  int amount,
  int max_new_slots
) {
  if (!hold_types || !hold_amounts || n_holds <= 0 || amount <= 0) {
    return 0;
  }
  int loaded = 0;
  /* Prefer stacking into a matching partial hold. */
  for (int i = 0; i < n_holds && amount > 0; ++i) {
    if (hold_amounts[i] <= 0 || hold_amounts[i] >= 255) {
      continue;
    }
    if (hold_types[i] != cargo_type) {
      continue;
    }
    const int room = 100 - hold_amounts[i];
    if (room <= 0) {
      continue;
    }
    const int add = amount < room ? amount : room;
    hold_amounts[i] += add;
    amount -= add;
    loaded += add;
  }
  for (int i = 0; i < n_holds && amount > 0 && max_new_slots > 0; ++i) {
    if (hold_amounts[i] > 0 && hold_amounts[i] < 255) {
      continue;
    }
    const int add = amount < 100 ? amount : 100;
    hold_types[i] = cargo_type;
    hold_amounts[i] = add;
    amount -= add;
    loaded += add;
    max_new_slots--;
  }
  return loaded;
}

int units_load_goods(ColonizeUnitPool* pool, int unit_id, int cargo_type, int amount) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !units_is_transport(pool, unit_id)) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT || amount <= 0) {
    return 0;
  }
  const int n = units_goods_hold_count(pool, unit_id);
  return goods_pack_into_holds(
    u->hold_goods_type, u->hold_goods_amount, n, cargo_type, amount, n
  );
}

int units_unload_goods_hold(
  ColonizeUnitPool* pool,
  int unit_id,
  int hold_index,
  int* out_cargo_type,
  int* out_amount
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !units_is_transport(pool, unit_id)) {
    return 0;
  }
  const int n = units_goods_hold_count(pool, unit_id);
  if (hold_index < 0 || hold_index >= n) {
    return 0;
  }
  const int amt = units_unit_hold_amount(u, hold_index);
  if (amt <= 0) {
    return 0;
  }
  const int ctype = u->hold_goods_type[hold_index];
  if (out_cargo_type) {
    *out_cargo_type = ctype;
  }
  if (out_amount) {
    *out_amount = amt;
  }
  u->hold_goods_amount[hold_index] = 0;
  u->hold_goods_type[hold_index] = 0;
  return amt;
}

/*
 * Passenger slots left in a ship's hold: total capacity minus the @UNIT
 * "size" column (DS:0x5238) of every passenger already riding, minus holds
 * occupied by GOODS — cargo shares the same slots passengers go into
 * (bugs.md; DOS's one per-hold array holds both).
 *
 * DOS-LITERAL FUN_4720_00e0 (viceroy_unpacked_2.c raw 74628-74665). The
 * "does this stack fit on these ships" pass seeds each hull's room with
 *   `*(char *)(type * 0xe + 0x5237) - *(char *)(unit * 0x1c + 0x3150)`
 * (capacity column − goods holds in use) and then, for every non-ship unit
 * on the tile whose size is under the 99 sentinel, first-fits it into a hull
 * with `size <= room` and does `room -= size`. So a passenger costs its own
 * size, not one slot: a Treasure (@UNIT size 6) fills a Galleon's whole hold
 * (cargo 6) on its own, and no Caravel/Merchantman/Privateer/Frigate (cargo
 * 2/4/2/4) can ever take one. That is where the manual's "treasure needs a
 * Galleon" rule actually comes from — there is no type-name check in DOS.
 */
int units_ship_free_passenger_slots(const ColonizeUnitPool* pool, int ship_id) {
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  const int cap = units_ship_capacity(pool, ship_id);
  if (!ship || cap <= 0) {
    return 0;
  }
  /* Bounded by the hull's own hold count, not COLONIZE_UNIT_CARGO_MAX, so this
   * is deliberately not units_holds_used (which walks all 8 slots). */
  int goods = 0;
  const int holds = units_goods_hold_count(pool, ship_id);
  for (int i = 0; i < holds && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (units_unit_hold_amount(ship, i) > 0) {
      goods++;
    }
  }
  /* Σ 0x5238[type] over the riders, not the rider count (raw 74655-74661). */
  int pax_size = 0;
  for (int i = 0; i < ship->cargo_count && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    const ColonizeUnit* pax = units_get_const(pool, ship->cargo_ids[i]);
    const ColonizeUnitType* pt = pax ? units_type(pool, pax->type_index) : NULL;
    pax_size += (pt && pt->space > 0) ? pt->space : 1;
  }
  int free_slots = cap - pax_size - goods;
  return free_slots > 0 ? free_slots : 0;
}

bool units_board_stacked(ColonizeUnitPool* pool, int land_unit_id, int ship_id) {
  ColonizeUnit* land = units_get(pool, land_unit_id);
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!land || !ship) {
    return false;
  }
  if (units_is_sea(pool, land_unit_id) || !units_is_sea(pool, ship_id)) {
    return false;
  }
  if (land->aboard_ship_id >= 0 || ship->aboard_ship_id >= 0) {
    return false;
  }
  /*
   * bugs.md #482 — DOS-LITERAL boarding size gate. FUN_4720_00e0
   * (viceroy_unpacked_2.c raw 74644-74648), the "does this tile's stack fit on
   * these ships" pass, only considers a non-ship unit when
   *   `*(byte *)(type * 0xe + 0x5238) < 99`
   * i.e. its @UNIT "size" column is under the 99 sentinel; the same `< 99`
   * test guards the move-onto-ship candidate walk in FUN_4720_015c (raw
   * 74739). Wagon Train's @UNIT size is 99 (as is every ship's), so DOS can
   * never load one aboard — the sentinel is the gate, not a type name.
   */
  {
    const ColonizeUnitType* lt = units_type(pool, land->type_index);
    if (lt && lt->space >= 99) {
      return false;
    }
    /*
     * ...and the hold charge is that same size column, not a flat 1
     * (raw 74655-74661): `if (size <= room) room -= size;`. Six Treasures
     * used to fit one Galleon.
     */
    const int need = (lt && lt->space > 0) ? lt->space : 1;
    if (units_ship_capacity(pool, ship_id) <= 0 ||
        units_ship_free_passenger_slots(pool, ship_id) < need) {
      return false;
    }
  }
  land->aboard_ship_id = ship_id;
  land->x = ship->x;
  land->y = ship->y;
  /*
   * Deliberate RAW write, not units_mp_exhaust/restore (audit A9): the aboard
   * zero is the hold's park sentinel, and the whole aboard subsystem reads it
   * back literally in Euro space — units_unload_passenger ("0 means full
   * allotment, restore for the charge") and units_first_landfall_cargo. A
   * spent-aware writer would hand a native passenger max_mp and every one of
   * those readers would then call it movable. Only Euro land units ever board
   * (the boardable-ship pick matches nations), so the sentinel and its readers
   * stay in one space.
   */
  /*
   * What rides along is the passenger's own spent byte: DOS keeps +0x3149
   * as it was (bugs.md #544). A live allotment is carried as is; an
   * overnight Sentry/Fortified zero is a park (DOS spent 0, full); any
   * other zero is a real spend. Natives never board, but a native zero
   * means "nothing spent", so they carry full.
   */
  if (land->moves > 0) {
    land->aboard_moves = land->moves;
  } else if (land->nation_id >= 4 ||
             ((land->orders == UNITS_ORDER_SENTRY || land->orders == UNITS_ORDER_FORTIFIED) &&
              land->park_nights > 0)) {
    land->aboard_moves = -1;
  } else {
    land->aboard_moves = 0;
  }
  land->moves = 0;
  land->orders = UNITS_ORDER_SENTRY; /* sentry aboard */
  ship->cargo_ids[ship->cargo_count++] = land_unit_id;
  return true;
}

/* units_board = units_board_stacked plus the adjacency gate and the selection
 * hand-off (UN-10). The adjacency test must run BEFORE anything is written. */
bool units_board(ColonizeUnitPool* pool, int land_unit_id, int ship_id) {
  const ColonizeUnit* land = units_get_const(pool, land_unit_id);
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!land || !ship || !units_adjacent(land->x, land->y, ship->x, ship->y)) {
    return false;
  }
  if (!units_board_stacked(pool, land_unit_id, ship_id)) {
    return false;
  }
  if (pool->selected_id == land_unit_id) {
    pool->selected_id = ship_id;
  }
  const ColonizeUnit* boat = units_get_const(pool, ship_id);
  diag_info(
    "Unit %d boarded ship %d (cargo %d/%d)",
    land_unit_id,
    ship_id,
    boat ? boat->cargo_count : 0,
    units_ship_capacity(pool, ship_id)
  );
  return true;
}

int units_find_boardable_ship(
  const ColonizeUnitPool* pool, int x, int y, int nation_id, int need_space
) {
  if (!pool || nation_id < 0) {
    return -1;
  }
  if (need_space < 1) {
    need_space = 1;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* ship = &pool->units[i];
    if (!ship->active || ship->nation_id != nation_id) {
      continue;
    }
    if (!units_is_sea(pool, ship->id) || !units_is_on_map(ship)) {
      continue;
    }
    if (ship->x != x || ship->y != y) {
      continue;
    }
    /*
     * Room test must be the one units_board applies, not the passenger count
     * alone: goods share the hold array with passengers, so a hull whose holds
     * are full of cargo has no berth even at cargo_count 0. DOS agrees — 10be
     * seeds its board budget with capacity minus occupied holds and 0a60's
     * hull-full test is the same arithmetic (FUN_15eb_3208 free room =
     * 0x5237[type] - unit+0x3150; see original_sources_annotated/ai/
     * move_scoring_20e6_full.md, 2026-09-08 section). Counting passengers only
     * made units_enter_probe answer BOARD where units_board then refused, and
     * the move surfaced as a bogus domain block.
     *
     * need_space is the candidate's own @UNIT size column (DS:0x5238), the
     * `param_2 <= room` test FUN_4720_00e0 ends on (raw 74637-74644): a
     * Treasure (size 6) therefore matches only a hull with 6 free holds,
     * which is the real form of the "Treasure needs a Galleon" rule. The old
     * `require_galleon` type-name check is gone — DOS has no such test.
     */
    if (units_ship_free_passenger_slots(pool, ship->id) >= need_space) {
      return ship->id;
    }
  }
  return -1;
}

static bool units_remove_from_cargo(ColonizeUnit* ship, int pax_id) {
  if (!ship) {
    return false;
  }
  for (int i = 0; i < ship->cargo_count; ++i) {
    if (ship->cargo_ids[i] != pax_id) {
      continue;
    }
    for (int j = i + 1; j < ship->cargo_count; ++j) {
      ship->cargo_ids[j - 1] = ship->cargo_ids[j];
    }
    ship->cargo_count--;
    return true;
  }
  return false;
}

/*
 * DOS quirk (bugs.md): loaded units switch ships en route. In DOS a
 * "passenger" is just a sentried land unit sharing the ship's tile in the
 * unit chain, so whichever ship moves off a shared tile first scoops up as
 * many of the tile's loaded units as it has room for, in chain order —
 * exactly like ships picking sentries off a colony square, first come,
 * first served. The rest stay with the remaining ship(s). The port keeps an
 * explicit aboard_ship_id, so the departure pass walks one ascending-id
 * sweep over BOTH groups: sentried land units standing on the departure
 * tile, and passengers riding other own ships still standing there. Each
 * pickup is charged the rider's @UNIT size column, so a Treasure only ever
 * hops to a hull with six free holds (FUN_4720_00e0).
 */
int units_ship_departure_pickup(ColonizeUnitPool* pool, int ship_id, int x, int y) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!pool || !ship || !units_is_sea(pool, ship_id)) {
    return 0;
  }
  int taken = 0;
  /* "Move to front" (bugs.md): the flagged unit is first in line. */
  int order[COLONIZE_UNITS_MAX];
  int on = 0;
  if (pool->board_first_slot >= 0 && pool->board_first_slot < COLONIZE_UNITS_MAX) {
    order[on++] = pool->board_first_slot;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (i != pool->board_first_slot) {
      order[on++] = i;
    }
  }
  for (int oi = 0; oi < on; ++oi) {
    const int i = order[oi];
    if (units_ship_free_passenger_slots(pool, ship_id) <= 0 ||
        ship->cargo_count >= COLONIZE_UNIT_CARGO_MAX) {
      break;
    }
    ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->id == ship_id || units_is_sea(pool, u->id) ||
        u->nation_id != ship->nation_id || u->aboard_ship_id == ship_id) {
      continue;
    }
    /*
     * Size charge, not a type-name rule: the pickup can only take a unit
     * whose @UNIT size column fits the room left (FUN_4720_00e0, raw
     * 74655-74661), which is what keeps a Treasure (size 6) off every hull
     * but a Galleon / Man-O-War (cargo 6). Sentinel types (99) never board.
     */
    const ColonizeUnitType* ut = units_type(pool, u->type_index);
    const int need = (ut && ut->space > 0) ? ut->space : 1;
    if (need >= 99 || units_ship_free_passenger_slots(pool, ship_id) < need) {
      continue;
    }
    if (u->aboard_ship_id >= 0) {
      /* Riding another own ship that is still on the departure tile. Only a
       * sentried passenger transfers — DOS's pickup takes sentries, and an
       * awake (player-activated) passenger stays with its own ship. */
      if (u->orders != UNITS_ORDER_SENTRY) {
        continue;
      }
      ColonizeUnit* host = units_get(pool, u->aboard_ship_id);
      if (!host || !units_is_on_map(host) || host->x != x || host->y != y) {
        continue;
      }
      if (!units_remove_from_cargo(host, u->id)) {
        continue;
      }
      u->aboard_ship_id = ship_id;
      u->x = ship->x;
      u->y = ship->y;
      ship->cargo_ids[ship->cargo_count++] = u->id;
      taken++;
    } else {
      /* Sentried land unit standing on the tile (colony / ocean stack). */
      if (!units_is_on_map(u) || u->x != x || u->y != y ||
          u->orders != UNITS_ORDER_SENTRY) {
        continue;
      }
      if (units_board_stacked(pool, u->id, ship_id)) {
        taken++;
      }
    }
  }
  return taken;
}

bool units_unload_passenger_w(
  const ColonizeWorld* w,
  int ship_id,
  int pax_id,
  int dest_x,
  int dest_y
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* ship = units_get(pool, ship_id);
  ColonizeUnit* pax = units_get(pool, pax_id);
  if (!ship || !pax || !map || !units_is_sea(pool, ship_id)) {
    return false;
  }
  if (pax->aboard_ship_id != ship_id) {
    return false;
  }
  /* Adjacent landfall, or same tile (colony dock). */
  if (!(ship->x == dest_x && ship->y == dest_y) &&
      !units_adjacent(ship->x, ship->y, dest_x, dest_y)) {
    return false;
  }
  if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, pax->type_index, dest_x, dest_y, pax_id)) {
    return false;
  }
  /*
   * bugs.md #423: a landfall (shore crossing onto bare coast) is only offered
   * to cargo whose DOS spent byte is below max — a passenger that already
   * used its allotment this turn stays aboard. Docking at a colony is not a
   * landfall: FUN_4720_015c's DOCK arm puts everyone ashore regardless.
   */
  if (units_move_crosses_shore(map, colonies, ship->x, ship->y, dest_x, dest_y) &&
      !units_cargo_can_landfall(pool, pax_id)) {
    return false;
  }
  if (!units_remove_from_cargo(ship, pax_id)) {
    return false;
  }
  pax->aboard_ship_id = -1;
  pax->x = dest_x;
  pax->y = dest_y;
  units_occupancy_refresh_tile(pool, dest_x, dest_y, -1);
  pax->orders = UNITS_ORDER_NONE;
  /*
   * Shore-step MP (FUN_465b ADD). Aboard sentry often has moves==0 as a
   * skip-select flag while DOS spent is still 0 (full allotment) — restore
   * type movement for the charge only, then spend dest terrain cost. Never
   * leave a free full refill. Cite: 4720_015c; move_spent.c.
   */
  {
    int remaining = pax->moves;
    /* The park zero stands for what the passenger carried aboard: its own
     * DOS spent byte, never a free refill (bugs.md #544). */
    if (remaining <= 0) {
      remaining = pax->aboard_moves >= 0 ? pax->aboard_moves : units_max_mp(pool, pax_id);
    }
    pax->aboard_moves = -1;
    int cost = map_move_spent_thirds(map, ship->x, ship->y, dest_x, dest_y);
    if (cost < 1) {
      cost = 1;
    }
    pax->moves = remaining > cost ? remaining - cost : 0;
    /*
     * bugs.md: a landfall onto bare coast is DOS's 465b_05ca shore crossing —
     * water tile to land tile with no colony on either end — so the whole
     * allotment goes, not just the terrain cost. Unloading at a dock (the
     * ship is standing on the colony tile) keeps the cheap charge.
     */
    if (units_move_crosses_shore(map, colonies, ship->x, ship->y, dest_x, dest_y)) {
      pax->moves = 0;
    }
  }
  diag_info("Unloaded unit %d from ship %d to (%d,%d)", pax_id, ship_id, dest_x, dest_y);
  return true;
}


bool units_unload_w(
  const ColonizeWorld* w,
  int ship_id,
  int dest_x,
  int dest_y
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship || ship->cargo_count <= 0) {
    return false;
  }
  return units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, ship_id, ship->cargo_ids[0], dest_x, dest_y);
}

/*
 * DOS FUN_4720_015c landfall pick (viceroy_unpacked.c:76010-76026): walk the
 * ship's cargo chain and take the FIRST passenger whose spent byte (+0x3149)
 * is strictly BELOW its own max MP (FUN_281f_090c). If none qualifies the
 * landfall reason (2/3) is never written and the whole move is refused — a
 * passenger that already burnt its allotment this turn stays aboard.
 *
 * Port mapping: moves holds REMAINING, and the port zeroes it when a
 * passenger boards (park flag). So "spent < max" is `moves > 0` for a
 * normal unit, and for the parked zero it is `!mp_spent_turn` — the flag the
 * board path sets when DOS would have forced spent to max (bugs.md #423).
 */
bool units_cargo_can_landfall(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* pax = units_get_const(pool, unit_id);
  if (!pax || !pax->active) {
    return false;
  }
  if (pax->mp_spent_turn) {
    return false; /* DOS spent == max_mp: no landfall this turn. */
  }
  return true;
}

int units_first_landfall_cargo(const ColonizeUnitPool* pool, int ship_id) {
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!ship || ship->cargo_count <= 0) {
    return -1;
  }
  /*
   * bugs.md #719: ONE pass, first match. DOS raw 76012-76020 is a single
   * chain walk that stops at the first unit with @UNIT size < 99 and
   * `+0x3149 < FUN_281f_090c(unit)`:
   *   while (local_12 < 0 && -1 < iVar11) {
   *     if (0x5238[type] < 99 && unit[+0x3149] < max_mp(unit)) local_12 = iVar11;
   *     iVar11 = next_in_chain;
   *   }
   * The port's extra "prefer one with live MP" first tier was its own
   * tie-break and could pick a later passenger than DOS does.
   */
  for (int i = 0; i < ship->cargo_count; ++i) {
    const ColonizeUnit* pax = units_get_const(pool, ship->cargo_ids[i]);
    if (pax && units_cargo_can_landfall(pool, pax->id)) {
      return pax->id;
    }
  }
  return -1;
}

bool units_pick_landfall_tile_w(
  const ColonizeWorld* w,
  int ship_id,
  int prefer_x,
  int prefer_y,
  int* out_x,
  int* out_y
) {
  const ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (!pool || !ship || !map || !out_x || !out_y || !units_is_sea(pool, ship_id)) {
    return false;
  }
  int pax_type = -1;
  int pax_id = -1;
  if (ship->cargo_count > 0) {
    pax_id = ship->cargo_ids[0];
    const ColonizeUnit* pax = units_get_const(pool, pax_id);
    if (pax) {
      pax_type = pax->type_index;
    }
  }
  if (pax_type < 0) {
    return false;
  }

  const bool have_prefer = prefer_x >= 0 && prefer_y >= 0;
  int best_x = -1;
  int best_y = -1;
  int best_score = -0x7fffffff;
  for (int d = 0; d < 8; ++d) {
    const int nx = ship->x + MAP_DIR8_DX[d];
    const int ny = ship->y + MAP_DIR8_DY[d];
    if (!map_tile_is_land(map, nx, ny) || map_tile_is_water(map, nx, ny)) {
      continue;
    }
    if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, pax_type, nx, ny, pax_id)) {
      continue;
    }
    /* Settle landfall: skip arctic / occupied (colonies_can_found). */
    if (colonies && !colonies_can_found(colonies, map, nx, ny)) {
      continue;
    }
    int score = 10;
    if (have_prefer) {
      const int dx = nx - prefer_x;
      const int dy = ny - prefer_y;
      score -= (dx * dx + dy * dy);
    }
    if (best_x < 0 || score > best_score) {
      best_x = nx;
      best_y = ny;
      best_score = score;
    }
  }
  if (best_x < 0) {
    return false;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}


int units_landfall_unload_all_w(
  const ColonizeWorld* w,
  int ship_id,
  int dest_x,
  int dest_y
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!pool || !ship || !map || !units_is_sea(pool, ship_id) || ship->cargo_count <= 0) {
    return 0;
  }
  const int saved_sel = pool->selected_id;
  int n = 0;
  /* Snapshot ids — cargo_ids shift as we unload. */
  int ids[COLONIZE_UNIT_CARGO_MAX];
  const int count = ship->cargo_count < COLONIZE_UNIT_CARGO_MAX ? ship->cargo_count
                                                               : COLONIZE_UNIT_CARGO_MAX;
  for (int i = 0; i < count; ++i) {
    ids[i] = ship->cargo_ids[i];
  }
  for (int i = 0; i < count; ++i) {
    ColonizeUnit* pax = units_get(pool, ids[i]);
    if (!pax || pax->aboard_ship_id != ship_id) {
      continue;
    }
    /* Wake sentry so unload does not leave orders=1 ashore. */
    if (pax->orders == UNITS_ORDER_SENTRY) {
      pax->orders = UNITS_ORDER_NONE;
    }
    if (units_unload_passenger_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, ship_id, ids[i], dest_x, dest_y)) {
      n++;
    }
  }
  pool->selected_id = saved_sel;
  return n;
}


int units_disembark_all(ColonizeUnitPool* pool, int ship_id, int x, int y) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship || !units_is_sea(pool, ship_id)) {
    return 0;
  }
  int n = 0;
  while (ship->cargo_count > 0) {
    const int pax_id = ship->cargo_ids[0];
    ColonizeUnit* pax = units_get(pool, pax_id);
    if (!units_remove_from_cargo(ship, pax_id)) {
      break;
    }
    if (pax) {
      pax->aboard_ship_id = -1;
      pax->x = x;
      pax->y = y;
      pax->orders = UNITS_ORDER_NONE;
      /*
       * Restore the allotment, don't just clear the order. Boarding parks a
       * passenger at moves 0 as a "don't offer this one" flag while
       * DOS's own spent byte is still zero — a full allotment — so a
       * passenger put ashore in a colony was landing unable to move at all
       * until the next turn (bugs.md: "possibly with moves if they had any
       * remaining that turn"). units_unload_passenger already restores it
       * the same way for its own move charge.
       */
      /*
       * The park zero is refilled with what the passenger carried aboard
       * (aboard_moves), never more: nothing on the ship's move or dock path
       * rewrites a passenger's +0x3149 (its only writers are the day-top
       * reset FUN_130d_0290, spawns and AI arms), so a pioneer that walked
       * aboard from open shore (465b_05ca spent = max, #423) lands spent and
       * one that boarded mid-turn in port lands with its remainder (#544).
       */
      if (units_remaining_mp(pool, pax_id) <= 0 && units_type(pool, pax->type_index)) {
        if (pax->aboard_moves >= 0) {
          pax->moves = pax->aboard_moves;
        } else {
          units_mp_restore(pool, pax);
        }
      }
      pax->aboard_moves = -1;
      n++;
    }
  }
  diag_info("Disembarked %d units from ship %d at (%d,%d)", n, ship_id, x, y);
  return n;
}

int units_collect_tile_stack(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int nation_id,
  int* out_ids,
  int out_max
) {
  if (!pool || !out_ids || out_max <= 0) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX && n < out_max; ++i) {
    const ColonizeUnit* u = &pool->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (units_is_on_map(u) && u->x == x && u->y == y) {
      out_ids[n++] = u->id;
    }
  }
  /* Passengers of ships on this tile (may already share x,y). */
  for (int i = 0; i < COLONIZE_UNITS_MAX && n < out_max; ++i) {
    const ColonizeUnit* ship = &pool->units[i];
    if (!ship->active || ship->nation_id != nation_id || ship->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_sea(pool, ship->id) || ship->x != x || ship->y != y) {
      continue;
    }
    for (int c = 0; c < ship->cargo_count && n < out_max; ++c) {
      const int pid = ship->cargo_ids[c];
      bool listed = false;
      for (int k = 0; k < n; ++k) {
        if (out_ids[k] == pid) {
          listed = true;
          break;
        }
      }
      if (!listed && units_get_const(pool, pid)) {
        out_ids[n++] = pid;
      }
    }
  }
  return n;
}

/*
 * One passenger walk with the one dangling-id skip rule, so the type and
 * profession arrays stay index-aligned (UN-17). Either out array may be NULL.
 */
static int units_export_cargo(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_types,
  int* out_profs,
  int out_max
) {
  const ColonizeUnit* ship = units_get_const(pool, ship_id);
  if (out_max <= 0 || (!out_types && !out_profs)) {
    return 0;
  }
  if (!ship) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < ship->cargo_count && n < out_max; ++i) {
    const ColonizeUnit* pax = units_get_const(pool, ship->cargo_ids[i]);
    if (!pax) {
      continue;
    }
    if (out_types) {
      out_types[n] = pax->type_index;
    }
    if (out_profs) {
      out_profs[n] = pax->profession;
    }
    n++;
  }
  return n;
}

static int units_export_cargo_types(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_types,
  int out_max
) {
  if (!out_types) {
    return 0;
  }
  return units_export_cargo(pool, ship_id, out_types, NULL, out_max);
}

int units_export_cargo_professions(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_profs,
  int out_max
) {
  if (!out_profs || out_max <= 0) {
    return 0;
  }
  for (int i = 0; i < out_max; ++i) {
    out_profs[i] = -1;
  }
  const int n = units_export_cargo(pool, ship_id, NULL, out_profs, out_max);
  return n;
}

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
) {
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship || !units_is_sea(pool, ship_id)) {
    return false;
  }
  if (out_type_index) {
    *out_type_index = ship->type_index;
  }
  if (out_name && out_name_size > 0) {
    const ColonizeUnitType* type = units_type(pool, ship->type_index);
    snprintf(out_name, out_name_size, "%s", type ? type->name : "Ship");
  }
  if (out_cargo_types && out_cargo_count && cargo_max > 0) {
    *out_cargo_count = units_export_cargo_types(pool, ship_id, out_cargo_types, cargo_max);
  } else if (out_cargo_count) {
    *out_cargo_count = 0;
  }
  if (out_hold_goods_type && out_hold_goods_amount && hold_max > 0) {
    const int n = hold_max > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX : hold_max;
    for (int i = 0; i < n; ++i) {
      out_hold_goods_type[i] = ship->hold_goods_type[i];
      out_hold_goods_amount[i] = ship->hold_goods_amount[i];
    }
    for (int i = n; i < hold_max; ++i) {
      out_hold_goods_type[i] = 0;
      out_hold_goods_amount[i] = 0;
    }
  }
  return units_despawn(pool, ship_id);
}

int units_spawn_aboard(ColonizeUnitPool* pool, int type_index, ColonizeUnit* ship) {
  if (!pool || !ship || type_index < 0 || type_index >= pool->type_count) {
    return -1;
  }
  if (ship->cargo_count >= COLONIZE_UNIT_CARGO_MAX) {
    return -1;
  }
  ColonizeUnit* slot = units_slot(pool);
  if (!slot) {
    return -1;
  }
  /* units_slot reuses inactive rows — clear like units_spawn_allow_stack.
   * home_tribe_id must be -1 so Col1 origin exports as 0xff (DOS cargo UI);
   * leftover 0 looks like tribe[0] and breaks passenger treatment. */
  const ColonizeUnitType* type = &pool->types[type_index];
  units_slot_reset_defaults(pool, slot, type, type_index, ship->x, ship->y);
  /*
   * FUN_1427_10be divergences from the on-map spawn (UN-2): a passenger has no
   * movement of its own this turn, rides sentried, and carries no colony job.
   * units_set_nation runs before aboard_ship_id is stamped, exactly as before
   * the extraction — the helper leaves that field alone so the ordering (and
   * with it whether set_nation stamps the tile owner nibble) is unchanged.
   */
  slot->moves = 0;
  units_set_nation(slot, ship->nation_id);
  slot->aboard_ship_id = ship->id;
  slot->orders = UNITS_ORDER_SENTRY; /* sentry aboard */
  slot->profession = UNITS_JOB_NONE;
  ship->cargo_ids[ship->cargo_count++] = slot->id;
  pool->unit_count++;
  return slot->id;
}

int units_spawn_ship_with_cargo(
  ColonizeUnitPool* pool,
  int ship_type_index,
  int x,
  int y,
  const int* cargo_types,
  int cargo_count,
  const int* hold_goods_type,
  const int* hold_goods_amount
) {
  /* Allow stacking: harbor return / Europe berth may share a water tile. */
  const int ship_id = units_spawn_allow_stack(pool, ship_type_index, x, y);
  if (ship_id < 0) {
    return -1;
  }
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship) {
    return -1;
  }
  int cap = units_ship_capacity(pool, ship_id);
  /* Test / incomplete @UNIT rows may list cargo 0; still allow boarding passengers. */
  if (cap <= 0) {
    cap = COLONIZE_UNIT_CARGO_MAX;
  }
  const int n = cargo_count < 0 ? 0 : cargo_count;
  for (int i = 0; i < n && ship->cargo_count < cap; ++i) {
    if (!cargo_types) {
      break;
    }
    if (units_spawn_aboard(pool, cargo_types[i], ship) < 0) {
      break;
    }
  }
  if (hold_goods_type && hold_goods_amount) {
    for (int i = 0; i < COLONIZE_UNIT_CARGO_MAX; ++i) {
      ship->hold_goods_type[i] = hold_goods_type[i];
      ship->hold_goods_amount[i] = hold_goods_amount[i];
    }
  }
  return ship_id;
}

/*
 * DOS-LITERAL FUN_75c2_235c raw 121637-121647.
 *   Pioneers (type 2): `if (local_8 == 1) +0x315b = 0x14`  — French only, no
 *     difficulty term.
 *   Soldiers (type 1): `if ((bVar1 && *(byte*)0x53a6 < 2) || local_8 == 2)
 *     +0x315b = 0x15`, where `bVar1 = (*(char*)(local_8*0x34 + 0x543f) == 0)`
 *     i.e. **this nation's player record is the human** (control byte 0), and
 *     0x53a6 is the difficulty. So the Discoverer/Explorer Veteran Soldier is a
 *     human-only handicap: AI Europeans never get it (bugs.md #488).
 * @JOB is 0-based: 0x14 = Pioneer (Hardy Pioneers), 0x15 = Soldier (Veteran
 * Soldiers). Nations: 0 English, 1 French, 2 Spanish, 3 Dutch.
 */
static void units_starter_skills(
  int nation_id, int difficulty, bool is_human, int* pioneer_job, int* soldier_job
) {
  const bool easy = difficulty <= 1;
  const bool hardy = nation_id == 1;
  const bool veteran = (is_human && easy) || nation_id == 2;
  if (pioneer_job) {
    *pioneer_job = hardy ? UNITS_JOB_PIONEER : UNITS_JOB_NONE;
  }
  if (soldier_job) {
    *soldier_job = veteran ? UNITS_JOB_SOLDIER : UNITS_JOB_NONE;
  }
}

/*
 * UN-6: DOS FUN_112b_0060 keys the WoI / King military rows (Cont. Army 9,
 * Cont. Cav. 7, Regulars 6, Cavalry 8) off the @UNIT type, never off carried
 * equipment — those rows carry their muskets and horses implicitly, so the
 * colonial equipment ladders in units_display_name / units_map_sprite /
 * units_display_type_index must not repaint them as plain or veteran
 * Soldiers/Dragoons. Two of the three had the guard open-coded and
 * units_display_type_index had none at all, so it handed chrome and the
 * reports @UNIT id (reports.c:2655) type 1 for a Cont. Army.
 */
static bool units_display_keeps_own_type(const ColonizeUnitType* t) {
  return units_type_is_continental(t) || units_type_is_royal(t);
}

/*
 * bugs.md #591: DOS names every unit from its @UNIT ROW string,
 * `*(0x5230 + type*0xe)` — the map sidebar header (raw 14128), the disband
 * prompt (raw 42654), @DEMOTE (raw 99460-99461), @CAPTURE (raw 99511) and the
 * combat chrome (raw 100625-100628) all read that one table. There is no
 * equipment- or profession-derived name channel: the rank words ("Veteran",
 * "Seasoned") are the SEPARATE second line, FUN_49dd_0386 (raw 78606-78650),
 * which the port keeps in units_profession_line / units_profession_label.
 * So a bare Expert Fisherman is "Colonists" (@UNIT row 0) and an armed one is
 * "Soldiers" (@UNIT row 1) — the type decides, nothing else.
 */
const char* units_display_name(const ColonizeUnitPool* pool, const ColonizeUnit* unit) {
  if (!unit) {
    return "Unit";
  }
  int ti = unit->type_index;
  if (pool && units_get_const(pool, unit->id) == unit) {
    /* DOS carries the equipment ladder in the type byte +0x3146 itself
     * (arming a colonist in a colony rewrites it); this port keeps the
     * equipment on a colonist body, so units_display_type_index is the
     * stand-in for that byte. */
    const int dt = units_display_type_index(pool, unit->id);
    if (dt >= 0) {
      ti = dt;
    }
  }
  const ColonizeUnitType* ut = pool ? units_type(pool, ti) : NULL;
  return (ut && ut->name[0]) ? ut->name : "Unit";
}

/*
 * ICONS.SS index per NAMES.TXT @JOB profession (0..28, UNITS_JOB_NONE
 * included) — the port of DOS FUN_112b_0002 (reached as the far thunk
 * FUN_281f_02c6), read straight off its asm at 112b:0002:
 *
 *   AX -= 0x13; if ((unsigned)AX > 9) return profession + 0x52;
 *   else JMP CS:[0x14 + AX*2]   ; 10-entry table, offsets
 *                               ; 30,36,3c,42,28,48,4e,54,5a,30
 *
 * so professions 0..18 and 25..26 take the linear `profession + 0x52`
 * branch, and 19..24 / 27..28 the jump table:
 *   19 → 0x65   20 → 0x3b   21 → 0x3c   22 → 0x3d
 *   23 → falls into the linear branch (0x69)
 *   24 → 0x3e   25 → 0x6b   26 → 0x6c   27 → 0x43   28 → 0x65 (= case 19)
 * Every id here is the 1-based NAMES/ICONS number, so the port sprite is
 * one less — hence the table below is `DOS id − 1` throughout.
 *
 * Pioneer/Soldier/Scout/Missionary land on the dedicated *working* poses
 * 58..61 (no musket/tools/horse/book), not the equipped on-map art; a bare
 * `type->icon_sprite` fallback would give the equipped pose, wrong for
 * anyone actually working. UNITS_JOB_NONE (28) shares the Free Colonists
 * portrait — an unspecialized @UNIT-type-0 Colonists unit *is* a Free
 * Colonist. Cross-checked against report-screen-goldens/labor.png, whose
 * "Jesuit Missionaries" cell shows the bookless black cassock (sprite 61),
 * not the book-carrying commissioned-missionary poses 77/105.
 */
static const int16_t k_units_job_icon[UNITS_JOB_NONE + 1] = {
  81,  82,  83, 84,  85,  86,  87,  88, /* 0-7   farm/forest/mine experts */
  89,  90,  91, 92,  93,  94,  95,  96, 97, /* 8-16  fisherman..preacher */
  98,
  /* 18 Expert Teachers: linear branch, 18 + 0x52 = 0x64 → sprite 99 (the
   * blue-coat figure holding a book, between Elder Statesman 98 and Free
   * Colonist 100). Was -1, which fell back to the @UNIT type icon and drew
   * teachers as plain Free Colonists. Note the Expert Teacher colonist type
   * was cut from the final DOS game (see europe_pool_remap); the sprite is
   * a leftover, reachable only via hand-edited saves. */
  99,
  100, UNITS_ICON_HARDY_PIONEER_WORK, UNITS_ICON_VETERAN_SOLDIER_WORK,
  /* 23 Veteran Dragoons: linear branch, 0x17 + 0x52 = 0x69 → sprite 104
   * (equipped veteran dragoon — there is no working portrait). Was -1,
   * which left prof-23 citizens invisible on the Score screen (bugs.md). */
  60,  UNITS_ICON_VETERAN_DRAGOON,
  /* 24 Jesuit Missionaries: jump-table case 5 → 0x3e → sprite 61, the
   * working black cassock. Was 77, which is the *commissioned* non-expert
   * missionary's map pose (FUN_112b_0060's type-3 downgrade), a different
   * sprite entirely (bugs.md #420). */
  UNITS_ICON_JESUIT_MISSIONARY_WORK,
  106, 107, 66, /* 25-27 servant, criminal, convert */
  100 /* 28 NONE: same as Free Colonists */
};

/*
 * Expert-skill label for a unit row (UN-22): the plural @JOB field, or NULL.
 * DOS FUN_49dd_0386 resolves the @JOB record, but FUN_15eb_0002 first gates
 * out the five non-expert professions (none / Colonist 19 / Ind. Servant 25 /
 * Criminal 26 / Convert 27), so a plain colonist gets no second line at all.
 * One copy for unit_stack.c's row label and map_panel.c's sidebar line, which
 * had the same body twice (the unit_stack one only to dodge a link edge).
 */
/* `col`-th comma-separated field of the @JOB row for `profession`, or NULL. */
static const char* units_job_field(
  const ColonizeMsgCatalog* names, int profession, int col
) {
  const ColonizeMsgSection* sec = names ? assets_msg_find(names, "JOB") : NULL;
  if (!sec || profession < 0 || profession >= sec->line_count) {
    return NULL;
  }
  const char* p = sec->lines[profession];
  for (int c = 0; c < col; ++c) {
    p = strchr(p, ',');
    if (!p) {
      return NULL;
    }
    ++p;
  }
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  static char buf[40];
  size_t n = 0;
  while (p[n] && p[n] != ',' && n + 1 < sizeof(buf)) {
    buf[n] = p[n];
    ++n;
  }
  while (n > 0 && (buf[n - 1] == ' ' || buf[n - 1] == '\t')) {
    --n;
  }
  buf[n] = '\0';
  return buf[0] ? buf : NULL;
}

/*
 * DOS-LITERAL FUN_49dd_0386 (raw 78606-78640) — the map-panel profession line.
 * `local_4 = +0x315b`, `0x1c → 0x13`; the string is @JOB **column 0**
 * (singular: DS word table stride 8, word 0), not the plural column the
 * stack-row label uses. Suppression (raw 78622-78628): only when the unit type
 * is non-zero AND the caller's flag is 0 AND `FUN_281f_0c9a` (= FUN_15eb_0002)
 * reports the profession unskilled. The three overrides (raw 78630-78638) are
 * applied after, so they win: type 1/4 + 0x15 → LABELS @MISC[65] "Veteran",
 * type 5 + 0x16 and type 3 + 0x18 → @MISC[4] "Expert". The `has_profession_slot`
 * test is the DOS caller's own `FUN_281f_0b78 >= 0` gate (bugs.md #507).
 */
const char* units_profession_line(
  const ColonizeMsgCatalog* names, int type_index, int profession, bool allow_unskilled
) {
  if (!units_type_has_profession_slot(type_index) || profession < 0) {
    return NULL;
  }
  int prof = profession;
  if (prof == UNITS_JOB_NONE) {
    prof = UNITS_JOB_COLONIST;
  }
  if ((type_index == 1 || type_index == 4) && prof == UNITS_JOB_SOLDIER) {
    return reports_misc_display_word(65, "");
  }
  if ((type_index == 5 && prof == UNITS_JOB_SCOUT) ||
      (type_index == 3 && prof == UNITS_JOB_MISSIONARY)) {
    return reports_misc_display_word(4, "");
  }
  /* FUN_15eb_0002: 0x1c / 0x13 / 0x19 / 0x1a / 0x1b are the unskilled set. */
  const bool skilled =
    !(prof == UNITS_JOB_NONE || prof == UNITS_JOB_COLONIST || prof == UNITS_JOB_SERVANT ||
      prof == UNITS_JOB_CRIMINAL || prof == UNITS_JOB_CONVERT);
  if (type_index != 0 && !allow_unskilled && !skilled) {
    return NULL;
  }
  return units_job_field(names, prof, 0);
}

/*
 * The @JOB COLUMN 1 name ("Expert Farmers") for a unit's profession, used
 * for the stack row / Europe dock caption. DOS cite (bugs.md #577, the
 * column choice used to carry none): FUN_38fd_3694,
 * viceroy_overlays.c:61190 reads `-0x715c + profession*8` for exactly this
 * caption, and -0x715c is the column-1 pointer table (column 0, "Farmer",
 * is -0x715e and is what the map panel's FUN_49dd_0386 uses instead). A
 * unit whose profession is one of the unskilled set (0x1c/0x13/0x19/0x1a/
 * 0x1b) gets no label at all — DOS's own `!= 0x1c` guard, widened to the
 * unskilled set the same way units_profession_line is.
 */
const char* units_profession_label(
  const ColonizeMsgCatalog* names, int type_index, int profession
) {
  if (!units_type_has_profession_slot(type_index)) {
    return NULL;
  }
  if (profession < 0 || profession == UNITS_JOB_NONE ||
      profession == UNITS_JOB_COLONIST || profession == UNITS_JOB_SERVANT ||
      profession == UNITS_JOB_CRIMINAL || profession == UNITS_JOB_CONVERT) {
    return NULL;
  }
  const ColonizeMsgSection* sec = names ? assets_msg_find(names, "JOB") : NULL;
  if (!sec || profession >= sec->line_count) {
    return NULL;
  }
  const char* p = strchr(sec->lines[profession], ',');
  if (!p) {
    return NULL;
  }
  ++p;
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
  static char buf[40];
  size_t n = 0;
  while (p[n] && p[n] != ',' && n + 1 < sizeof(buf)) {
    buf[n] = p[n];
    ++n;
  }
  while (n > 0 && (buf[n - 1] == ' ' || buf[n - 1] == '\t')) {
    --n;
  }
  buf[n] = '\0';
  return buf[0] ? buf : NULL;
}

int units_type_default_job(int type_index) {
  /* DS:0x30e, one signed byte per @UNIT type; -1 = no profession slot.
   * This is the value FUN_15eb_0902 returns, i.e. the `local_ee` that
   * FUN_15eb_0e18 (= FUN_1000_8dfe) hands back for a colony slot index past
   * the population — the on-tile stack. Value accessor added 2026-09-18 for
   * the re-hosted FUN_5952_035e absorption arm (smell audit 2026-09-10
   * "DEFERRED — raw 94247" asked for exactly this). */
  static const signed char k_default_job[] = {19, 21, 20, 24, 23, 22, -1, 23, -1, 21, -1, -1,
                                              -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0};
  if (type_index < 0 || type_index >= (int)(sizeof(k_default_job) / sizeof(k_default_job[0]))) {
    return -1;
  }
  return (int)k_default_job[type_index];
}

bool units_type_has_profession_slot(int type_index) {
  return units_type_default_job(type_index) >= 0;
}

int units_job_icon_sprite(int profession) {
  if (profession < 0 || profession >= (int)(sizeof(k_units_job_icon) / sizeof(k_units_job_icon[0]))) {
    return -1;
  }
  return k_units_job_icon[profession];
}

int units_working_colonist_sprite(
  const ColonizeUnitPool* pool,
  int unit_type_index,
  int profession
) {
  const int icon = units_job_icon_sprite(profession);
  if (icon >= 0) {
    return icon;
  }
  const ColonizeUnitType* type = units_type(pool, unit_type_index);
  return type ? type->icon_sprite : -1;
}

int units_map_sprite(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (!unit) {
    return -1;
  }
  const ColonizeUnitType* type = units_type(pool, unit->type_index);
  if (!type) {
    return -1;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(pool, unit_id, &tools, &muskets, &horses);
  /* bugs.md: the WoI military types (Cont. Army / Cont. Cav. / Regulars /
   * Cavalry) have their own @UNIT art — the colonial equipment overrides
   * below must not repaint them as plain/veteran Soldiers. */
  if (units_display_keeps_own_type(type)) {
    return type->icon_sprite;
  }
  /* bugs.md: damaged artillery has its own art — DOS FUN_112b icon pick:
   * type 0xb + damage bit7 -> icon 0x42, i.e. this port's sprite 65 (same
   * 1-based offset as the Scout/Dragoon poses in that function). */
  if (combat_type_is_artillery(type) && (unit->col1_flags15 & 0x80u) != 0) {
    return UNITS_ICON_DAMAGED_ARTILLERY;
  }
  /*
   * bugs.md #420: a commissioned missionary whose colonist is not a Jesuit
   * expert has its own, plainer art. DOS FUN_112b_0060 tail:
   *   if (type == 3 && profession != 0x18) icon = 0x4e;   // sprite 77
   * i.e. the same expert/generic split the 0x4a..0x4d poses give Pioneers,
   * Soldiers, Scouts and Dragoons just above it — the Jesuit keeps the
   * @UNIT icon (106 → sprite 105), everyone else drops to 77. The port had
   * no missionary branch at all, so every Missionaries-type unit fell
   * through to `type->icon_sprite` and wore the Jesuit art. Ahead of the
   * equipment branches because DOS keys this off the @UNIT type, not cargo:
   * a missionary with leftover horses is still drawn as a missionary.
   */
  if (units_type_kind(type) == UNITS_KIND_MISSIONARY) {
    return (unit->profession == UNITS_JOB_MISSIONARY)
             ? type->icon_sprite /* = UNITS_ICON_JESUIT_MISSIONARY, from NAMES @UNIT */
             : UNITS_ICON_MISSIONARY;
  }
  /*
   * DOS-LITERAL FUN_112b_0060: the veteran art test is profession == 0x15
   * exactly — `if ((type == 1) && (prof != 0x15)) icon = 0x4b;` and
   * `if ((type == 4) && (prof != 0x15)) icon = 0x4d;`. A mounted Veteran
   * Soldier (0x15) IS a veteran dragoon (bugs.md #263), but 0x17 is not a
   * veteran profession to DOS — and DOS never writes 0x17 to a unit at all
   * (bugs.md #503), so accepting it here only mis-drew hand-edited saves
   * (bugs.md #639).
   */
  const bool vet_prof = unit->profession == UNITS_JOB_SOLDIER;
  if (muskets > 0 && horses > 0) {
    return vet_prof ? UNITS_ICON_VETERAN_DRAGOON : UNITS_ICON_DRAGOON;
  }
  if (muskets > 0) {
    return vet_prof ? UNITS_ICON_VETERAN_SOLDIER : UNITS_ICON_SOLDIER;
  }
  if (horses > 0) {
    return (unit->profession == UNITS_JOB_SCOUT) ? UNITS_ICON_SEASONED_SCOUT : UNITS_ICON_SCOUT;
  }
  if (tools > 0) {
    return (unit->profession == UNITS_JOB_PIONEER) ? UNITS_ICON_HARDY_PIONEER
                                                  : UNITS_ICON_PIONEER;
  }
  /*
   * bugs.md: an Indian Convert drew as a plain Free Colonist out on the map
   * while the colony screen showed it properly. DOS has one icon rule for
   * both — FUN_112b_0060 routes *every* @UNIT type 0 (Colonists) unit through
   * FUN_112b_0002(profession), whose jump table sends profession 0x1b to icon
   * 0x43, i.e. this port's sprite 66, the same one units_job_icon_sprite
   * already returns. The equipment cases above stand in for DOS's own
   * type != 0 overrides (Pioneers/Soldiers/Scouts/Dragoons are distinct unit
   * types there, equipment on a colonist here), so they still come first.
   */
  if (units_type_is_colonist(type)) {
    const int by_job = units_job_icon_sprite(unit->profession);
    if (by_job >= 0) {
      return by_job;
    }
  }
  return type->icon_sprite;
}

int units_display_type_index(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  if (!unit) {
    return -1;
  }
  /* The WoI / King rows keep their own @UNIT index (see
   * units_display_keeps_own_type) — this ladder is the colonial one. */
  if (units_display_keeps_own_type(units_type(pool, unit->type_index))) {
    return unit->type_index;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(pool, unit_id, &tools, &muskets, &horses);
  /* Col1 @UNIT indices: match equipment → displayed type for chrome placement. */
  if (muskets > 0 && horses > 0) {
    const int t = units_kind_type_index(pool, UNITS_KIND_DRAGOON);
    return t >= 0 ? t : 4;
  }
  if (muskets > 0) {
    const int t = units_kind_type_index(pool, UNITS_KIND_SOLDIER);
    return t >= 0 ? t : 1;
  }
  if (horses > 0) {
    const int t = units_kind_type_index(pool, UNITS_KIND_SCOUT);
    return t >= 0 ? t : 5;
  }
  if (tools > 0) {
    const int t = units_kind_type_index(pool, UNITS_KIND_PIONEER);
    return t >= 0 ? t : 2;
  }
  return unit->type_index;
}

bool units_map_stack_chrome(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = pool ? units_get_const(pool, unit_id) : NULL;
  if (!u) {
    return false;
  }
  return units_count_at(pool, u->x, u->y) > 1 || u->cargo_count > 0;
}

/*
 * Prefer selected unit on the tile; else highest id (drawn last previously)
 * — except on a colony tile, where DOS never shows an idle garrison unit at
 * all, only the active/selected unit while it's actually visible (blinking
 * on, or mid-move). A non-selected unit sitting in a colony square is
 * otherwise invisible, same as it is inside the settlement view. */
int units_top_on_map_tile(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  bool selected_visible,
  const ColonizeWorldMap* map
) {
  const bool on_colony = map_tile_has_city(map, x, y);
  /*
   * The active unit owns its tile outright: it is drawn on the blink-on
   * phase and the tile is left empty on the blink-off one. Letting the rest
   * of the stack take its place off-blink made the tile alternate between
   * two different units, so which one was actually active became a guess
   * (bugs.md). A Go-To unit does not blink at all, so it never yields.
   *
   * A selected PASSENGER is not on the map (it rides in the hold), so the
   * on-map scan below never found it and the ship drew steadily instead —
   * "the loaded-active unit doesn't blink" (bugs.md). It owns the ship's
   * tile the same way: its own sprite on blink-on, empty off-blink.
   */
  if (pool->selected_id >= 0) {
    const ColonizeUnit* sel = units_get_const(pool, pool->selected_id);
    if (sel && sel->active && sel->aboard_ship_id >= 0 && sel->x == x && sel->y == y) {
      return selected_visible ? sel->id : -1;
    }
  }
  int slot_5 = 0;
  for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot_5); u != NULL;
       u = units_next_on_tile_const(pool, x, y, &slot_5)) {
    if (u->id != pool->selected_id) {
      continue;
    }
    if (!selected_visible && u->orders != UNITS_ORDER_GOTO) {
      return -1;
    }
    return u->id;
  }
  int top = -1;
  int top_id = -1;
  int slot_6 = 0;
  for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot_6); u != NULL;
       u = units_next_on_tile_const(pool, x, y, &slot_6)) {
    if (on_colony) {
      continue;
    }
    if (u->id > top_id) {
      top_id = u->id;
      top = u->id;
    }
  }
  return top;
}

/* ===================== New-world start placement & colonist deployment (units_spawn_euro_starter_fleet .. units_deploy_colonist) ===================== */

int units_spawn_euro_starter_fleet(
  ColonizeUnitPool* pool,
  int nation_id,
  int difficulty,
  bool is_human,
  int x,
  int y,
  int goto_x,
  int goto_y
) {
  if (!pool || nation_id < 0 || nation_id > 3) {
    return -1;
  }
  if (difficulty < 0) {
    difficulty = 0;
  }
  if (difficulty > 4) {
    difficulty = 4;
  }

  int pioneer_type = units_kind_type_index(pool, UNITS_KIND_PIONEER);
  if (pioneer_type < 0) {
    pioneer_type = units_kind_type_index(pool, UNITS_KIND_COLONIST);
  }
  const int soldier_type = units_kind_type_index(pool, UNITS_KIND_SOLDIER);
  int ship_type = units_kind_type_index(pool, UNITS_KIND_CARAVEL);
  if (nation_id == 3) {
    const int merchant = units_kind_type_index(pool, UNITS_KIND_MERCHANTMAN);
    if (merchant >= 0) {
      ship_type = merchant;
    }
  }
  if (ship_type < 0 || pioneer_type < 0) {
    return -1;
  }

  const int ship_id = units_spawn_allow_stack(pool, ship_type, x, y);
  if (ship_id < 0) {
    return -1;
  }
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (!ship) {
    return -1;
  }
  units_set_nation(ship, nation_id);
  ship->profession = 0; /* FUN_1427_06b4 transport profession */
  if (goto_x >= 0 && goto_x < 255 && goto_y >= 0 && goto_y < 255) {
    ship->orders = UNITS_ORDER_GOTO;
    ship->goto_x = goto_x;
    ship->goto_y = goto_y;
  }

  int pioneer_job = UNITS_JOB_NONE;
  int soldier_job = UNITS_JOB_NONE;
  units_starter_skills(nation_id, difficulty, is_human, &pioneer_job, &soldier_job);

  const int cargo_types[2] = {pioneer_type, soldier_type >= 0 ? soldier_type : pioneer_type};
  const int cargo_jobs[2] = {pioneer_job, soldier_type >= 0 ? soldier_job : pioneer_job};
  const int cargo_n = soldier_type >= 0 ? 2 : 1;
  for (int i = 0; i < cargo_n; ++i) {
    const int pid = units_spawn_aboard(pool, cargo_types[i], ship);
    if (pid < 0) {
      diag_warn("starter fleet: failed to board passenger %d for nation %d", i, nation_id);
      continue;
    }
    ColonizeUnit* pax = units_get(pool, pid);
    if (!pax) {
      continue;
    }
    units_set_nation(pax, nation_id);
    pax->profession = cargo_jobs[i];
    pax->orders = UNITS_ORDER_SENTRY; /* sentry aboard */
    pax->goto_x = goto_x >= 0 ? goto_x : 0xFF;
    pax->goto_y = goto_y >= 0 ? goto_y : 0xFF;
  }

  diag_info(
    "Euro starter fleet nation=%d ship=%d cargo=%d at (%d,%d) skills p=%d s=%d",
    nation_id,
    ship_id,
    ship->cargo_count,
    x,
    y,
    pioneer_job,
    soldier_job
  );
  return ship_id;
}

bool units_find_water_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int occupant_id,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  if (map_tile_is_water(map, start_x, start_y)) {
    const int other = pool ? units_id_at(pool, start_x, start_y) : -1;
    if (other < 0 || other == occupant_id) {
      *out_x = start_x;
      *out_y = start_y;
      return true;
    }
  }
  for (int radius = 1; radius < 48; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (abs(dx) != radius && abs(dy) != radius) {
          continue;
        }
        const int x = start_x + dx;
        const int y = start_y + dy;
        if (!map_tile_is_water(map, x, y)) {
          continue;
        }
        const int other = pool ? units_id_at(pool, x, y) : -1;
        if (other >= 0 && other != occupant_id) {
          continue;
        }
        *out_x = x;
        *out_y = y;
        return true;
      }
    }
  }
  return false;
}

bool units_find_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }

  int best_x = -1;
  int best_y = -1;
  int best_d = 1 << 30;
  for (int y = 0; y < (int)map->height; ++y) {
    for (int x = 0; x < (int)map->width; ++x) {
      if (!map_tile_is_high_seas(map, x, y)) {
        continue;
      }
      if (pool && units_id_at(pool, x, y) >= 0) {
        continue;
      }
      const int dx = x - start_x;
      const int dy = y - start_y;
      const int d = dx * dx + dy * dy;
      if (d < best_d) {
        best_d = d;
        best_x = x;
        best_y = y;
      }
    }
  }
  if (best_x < 0) {
    return false;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

/*
 * FUN_48d3_0434: HS (0x1a) + empty or own nation.
 * FUN_48d3_048e: expand radius; for each ring scan horizontal then vertical
 * edges with ±radius offsets (decomp ~77836–77876).
 */
static bool units_hs_place_tile_ok(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int nation_id,
  int x,
  int y
) {
  if (!map || x < 0 || y < 0 || x >= (int)map->width || y >= (int)map->height) {
    return false;
  }
  if (!map_tile_is_high_seas(map, x, y)) {
    return false;
  }
  if (!pool) {
    return true;
  }
  const int id = units_id_at(pool, x, y);
  if (id < 0) {
    return true;
  }
  if (nation_id < 0) {
    return false;
  }
  const ColonizeUnit* u = units_get_const(pool, id);
  return u && u->nation_id == nation_id;
}

bool units_spiral_place_hs_near(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }
  const int max_dim = (int)map->width > (int)map->height ? (int)map->width : (int)map->height;
  for (int e = 0; e < max_dim; ++e) {
    /* Horizontal bands at y = start_y ± e, x from start_x-e .. start_x+e */
    for (int x = start_x - e; x <= start_x + e; ++x) {
      for (int si = 0; si < 2; ++si) {
        const int yoff = (si == 0) ? -e : e;
        const int y = start_y + yoff;
        if (units_hs_place_tile_ok(pool, map, nation_id, x, y)) {
          *out_x = x;
          *out_y = y;
          return true;
        }
      }
    }
    /* Vertical bands at x = start_x ± e, y from start_y-e .. start_y+e */
    for (int y = start_y - e; y <= start_y + e; ++y) {
      for (int si = 0; si < 2; ++si) {
        const int xoff = (si == 0) ? -e : e;
        const int x = start_x + xoff;
        if (units_hs_place_tile_ok(pool, map, nation_id, x, y)) {
          *out_x = x;
          *out_y = y;
          return true;
        }
      }
    }
  }
  return false;
}

/*
 * Eastern high-seas scan (UN-18). **No DOS counterpart — port-only, and it is
 * NOT a placement rule.** Audited 2026-09-17 against the whole 48d3 Atlantic
 * module: every DOS appearance of a ship coming out of Europe is
 * FUN_48d3_048e's ring hunt (units_spiral_place_hs_near) around the unit's own
 * saved tile `+0x314d/+0x314e`, and that tile is only ever written from the
 * nation's landfall tile (nation record `-0x77c6/-0x77c5`) or from where the
 * ship itself left the map (FUN_48d3_007a, raw 77604-77607 — every departure
 * restamps the nation tile, so "no saved exit tile" only happens at new-game
 * time, where @SCENARIO / LAB_684c_1b4c seeds it). REF Man-O-Wars and Europe
 * purchases inherit the same nation tile (raw 99622, 107547, 112448, 114391).
 * So this function survives only as a *navigation target* helper — "which
 * Atlantic tile should a ship steer for to reach Europe" — where DOS has no
 * table at all because the human steers by hand and FUN_48d3_015e simply
 * accepts whatever High Seas tile the ship reaches. Remaining callers, all
 * target-picking: game_loop.c (trade-route Europe stop), ai_euro.c
 * (treasure sail target, last-resort Europe-exit fallback). Placement callers
 * were converted to the 048e ring hunt 2026-09-17.
 *
 * One loop serves both the western-rim pass and the "any eastern high seas"
 * fallback. Closer latitude wins; tie-break westward (smaller x).
 */
static bool units_scan_eastern_high_seas(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int prefer_y,
  int require_west_edge,
  int* out_x,
  int* out_y
) {
  int best_x = -1;
  int best_y = -1;
  int best_score = -1;
  const int east_min_x = map->width / 2;
  for (int y = 0; y < (int)map->height; ++y) {
    for (int x = east_min_x; x < (int)map->width; ++x) {
      if (!map_tile_is_high_seas(map, x, y)) {
        continue;
      }
      if (require_west_edge && map_tile_is_high_seas(map, x - 1, y)) {
        continue; /* interior of eastern high seas — not the western edge */
      }
      if (pool && units_id_at(pool, x, y) >= 0) {
        continue;
      }
      const int score = 100000 - abs(y - prefer_y) * 1000 - x;
      if (score > best_score) {
        best_score = score;
        best_x = x;
        best_y = y;
      }
    }
  }
  if (best_x < 0) {
    return false;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

bool units_find_eastern_high_seas_tile(
  const ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int prefer_y,
  int* out_x,
  int* out_y
) {
  if (!map || !out_x || !out_y) {
    return false;
  }

  /*
   * Western rim of the eastern high-seas band: high-seas tiles whose west
   * neighbour is not high seas (Atlantic approach), excluding the map's
   * western border strip. Prefer latitude near prefer_y.
   */
  int best_x = -1;
  int best_y = -1;

  if (!units_scan_eastern_high_seas(pool, map, prefer_y, 1, &best_x, &best_y)) {
    /* Fallback: any eastern high seas near prefer_y, then any high seas / water. */
    (void)units_scan_eastern_high_seas(pool, map, prefer_y, 0, &best_x, &best_y);
  }

  if (best_x < 0) {
    if (units_find_high_seas_tile(pool, map, map->width - 1, prefer_y, out_x, out_y)) {
      return true;
    }
    return units_find_water_tile(pool, map, map->width - 1, prefer_y, -1, out_x, out_y);
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

void units_new_world_start(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int start_x,
  int start_y,
  int nation_id,
  int difficulty
) {
  if (!pool) {
    return;
  }
  units_reset(pool);
  if (!map) {
    return;
  }
  if (nation_id < 0 || nation_id > 3) {
    nation_id = 0;
  }

  /*
   * DOS-LITERAL FUN_48d3_048e (raw 77810-77896) + FUN_48d3_0434 (raw 77779).
   * DOS never scans the eastern half of the map for a starting tile: the
   * nation's landfall tile (nation record `-0x77c6/-0x77c5`, seeded from
   * @SCENARIO at raw 121035 or from LAB_684c_1b4c's HS-rim walk on generated
   * maps) is copied into the new ship's `+0x314d/+0x314e` (raw 121627), and the
   * Atlantic arrival tick runs the 048e expanding-ring hunt from exactly that
   * tile, taking the first High Seas tile that is empty or holds our own unit.
   * The @SCENARIO tiles are themselves High Seas, so the ring hits at e=0 and
   * the fleet starts on the scenario tile. The old eastern-half scan put the
   * Spanish AMER2 fleet on (29,61) instead of (47,61). (bugs.md #487)
   */
  int sx = start_x;
  int sy = start_y;
  if (!units_spiral_place_hs_near(pool, map, start_x, start_y, nation_id, &sx, &sy)) {
    return;
  }

  const int ship_id = units_spawn_euro_starter_fleet(
    pool, nation_id, difficulty, true, sx, sy, start_x, start_y
  );
  if (ship_id < 0) {
    return;
  }
  /*
   * Human starts with ship selected and idle. Clear GOTO orders but pin goto to
   * the ship's tile (COLONY00). Keeping landfall in goto with orders=0 made DOS
   * treat the caravel as unloaded / peel transport_chain on select/move.
   */
  ColonizeUnit* ship = units_get(pool, ship_id);
  if (ship) {
    ship->orders = UNITS_ORDER_NONE;
    ship->goto_x = ship->x;
    ship->goto_y = ship->y;
    for (int c = 0; c < ship->cargo_count; ++c) {
      ColonizeUnit* pax = units_get(pool, ship->cargo_ids[c]);
      if (pax) {
        pax->goto_x = ship->x;
        pax->goto_y = ship->y;
      }
    }
  }
  pool->selected_id = ship_id;
}

bool units_deploy_colonist(
  ColonizeUnitPool* pool,
  const ColonizeWorldMap* map,
  int x,
  int y,
  const char* immigrant_name
) {
  (void)immigrant_name;
  if (!pool || !map) {
    return false;
  }
  int colonist_type = units_kind_type_index(pool, UNITS_KIND_COLONIST);
  if (colonist_type < 0) {
    return false;
  }
  if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(NULL), .map=(ColonizeWorldMap*)(map)}, colonist_type, x, y, -1)) {
    return false;
  }
  const int id = units_spawn(pool, colonist_type, x, y);
  if (id < 0) {
    return false;
  }
  pool->selected_id = id;
  return true;
}
