#ifndef COLONIZE_UNITS_CARGO_H
#define COLONIZE_UNITS_CARGO_H

#include "core/units.h"
#include "core/world.h"

/* Holds, passengers, treasure, boarding: split out of units.h for navigability. */

/*
 * FUN_65dd_0004 thin transcription: Scout on rumour tile clears it via
 * map_clear_rumour, then rolls one of the manual-documented outcomes
 * (nothing / small treasure / chief's gift / burial mounds / trespass anger
 * / survivors join / Fountain of Youth / vanish / Cibola). de Soto (FF 7)
 * restricts the draw to the non-hostile subset (always positive) plus its
 * own reveal-radius bonus. europe/human_nation are optional (NULL/-1 to
 * skip the Fountain-of-Youth Europe-dock sync — AI nations have none).
 */
bool units_resolve_lcr_rumour_w(
  const ColonizeWorld* w,
  int unit_id,
  int human_nation
);


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
 * FUN_3844_00f2 ship-build ready: Col1 types 0x0d..0x12 with +0x3148 bit7
 * (Linux col1_flags15 bit7; same bit as ship_damaged for Frigate 0x0b —
 * construction gate excludes 0x0b). +1 turns_worked (+2 on any colony tile);
 * threshold = type.defense (DOS 0x5235 = NAMES @UNIT combat).
 * Clears bit7 on complete; human status line; *want_europe_open=1 if finished
 * off-colony. Returns ships completed. Cite: nation_eot_ship_spawn.md §A.
 */
int units_tick_ship_build_ready(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int human_nation,
  char* status,
  size_t status_size,
  int* want_europe_open
);

/*
 * Drydock repair: clear combat-damage bit7 for finished ships on own Drydock
 * colony. Construction (turns_worked < defense thresh) stays on ship-build tick.
 * Returns ships repaired. ai_popups/messages optional — human repair emits @REFIT.
 * Cite: building_production.md; combat.md fort bit7.
 */
int units_tick_drydock_repair(
  ColonizeUnitPool* pool,
  const ColonizeColonyPool* colonies,
  int nation_id,
  int human_nation,
  char* status,
  size_t status_size,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
);

/*
 * Cortes free king galleon stand-in: each Treasure of nation on an own coastal
 * colony → europe_cash_treasure (tax = Crown share) + despawn. Cite: fandom
 * Hernan Cortes; GAME.TXT @KINGGALLEON3. Syncs col1 nation gold. Returns
 * number cashed. Non-Cortes KINGGALLEON2 is
 * `units_king_galleon_offer_coastal_treasures` (FUN_5fef_1908, Done).
 */
int units_cortes_cash_coastal_treasures_w(
  const ColonizeWorld* w,
  int nation_id
);

/*
 * FUN_5fef_1908 Crown share % for the King's Galleon transport offer:
 * Cortes (FF 10) → current tax rate; otherwise max((difficulty+10)*5, 2*tax);
 * both capped at 90. DS strings "KINGGALLEON"+"3"/"2" at 0x1bed/0x1bf9/0x1bfb.
 */
int units_king_galleon_share_pct(const ColonizeCol1Save* col1, int nation_id);

/*
 * Gold a Treasure unit is worth. Two representations coexist and this is the
 * single reader for both:
 *   - port-spawned (units_spawn_treasure_train): full gold as an LE16 mirror
 *     in hold_goods_amount[0..1]; wins when set;
 *   - bridged from a COL1 save: DOS unit +0x315b = record +0x17 (`profession`)
 *     = gold/100 — FUN_48d3_06ba (viceroy_unpacked.c:77985) and
 *     FUN_521d_20e6's treasure band both read it that way, and col1_bridge
 *     fills hold_goods_amount only for sea/wagon hulls, so this byte is all a
 *     save-loaded Treasure carries.
 * 0 when neither is set. See the definition for the one 2800-gold blind spot.
 */
int units_treasure_value_gold(const ColonizeUnit* treasure);

/*
 * FUN_521d_20e6 treasure act band, first arm: an AI Treasure standing in an
 * own colony (DOS iStack_2e == 0 — the caller owns that gate) credits its full
 * face value to the nation treasury (nation +0x2a, no Crown cut, no Cortes or
 * galleon term), fires GAME.TXT @LOOTFOREIGN (DOS popup id 0x1786) while
 * DS:0x5382 bit0 (WoI) is clear, and is then destroyed (LAB_0047b9).
 * Returns the gold credited; the unit is despawned either way, as in DOS.
 */
int units_ai_treasure_cash_in_colony(
  ColonizeUnitPool* pool,
  ColonizeCol1Save* col1,
  int nation_id,
  int treasure_id,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
);

/*
 * FUN_465b_0000 trigger + FUN_5fef_1908 body, human nation only (DOS gates on
 * DS:0x543f == 0): each own Treasure standing on an own coastal colony tile.
 * WoI declared → cash full value at once (@CASHTREASURE). Else, if the nation
 * owns a Galleon and lacks Cortes → no offer (it can ship it itself). Else
 * enqueue the @KINGGALLEON3 (Cortes) / @KINGGALLEON2 CHOICE with payload =
 * treasure unit id; the apply step below does the cash. Returns the number
 * of treasures cashed or offered. DOS runs this on the move onto the tile;
 * Linux runs it at human turn end (same place the Cortes auto-cash lived).
 */
int units_king_galleon_offer_coastal_treasures_w(
  const ColonizeWorld* w,
  int nation_id,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
);

/*
 * Apply the pending AI_POPUP_TAG_KING_GALLEON result: choice 1 → Crown share
 * to nation.royal_money (DOS nation+0x22), remainder to gold, @LOOTCASH
 * notify, Treasure despawned. Refuse/cancel → Treasure stays. Returns true
 * when the result was for this tag (consumed).
 */
/*
 * Fountain of Youth (FUN_65dd_0004 case 1): DOS runs the Recruit picker
 * FUN_38fd_4884(1,0) eight times — free passage, player picks among the 3
 * pool slots each time. Enqueue one @RECRUIT CHOICE (payload = picks left);
 * the apply hook recruits the chosen slot and chains the next pick.
 */
void units_fountain_youth_enqueue_pick(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt, int human,
  int remaining
);
/*
 * 5e52 Brewster branch → FUN_38fd_4884(0,1): @RECRUITCHOOSE with the 3 pool
 * names (%COUNTRY = nation, %STRING0 = Europe), passage 0. Apply moves the
 * chosen pool entry to the docks, zeroes crosses and mirrors it as the
 * Europe-map unit; cancel leaves everything (DOS re-asks next turn).
 */
void units_brewster_enqueue_pick(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt, int human
);
/* The `_ex` forms carry the game rng into the 4884 tail's pool refill, whose
 * `46d4` tier rolls DOS takes off the shared stream; the plain forms pass
 * NULL (fixture shorthand). Smell audit 2026-09-10 G5. */
bool units_brewster_apply_popup(
  EuropeScreen* europe, AiPopupState* popups, ColonizeUnitPool* units
);
bool units_brewster_apply_popup_ex_w(
  const ColonizeWorld* w,
  AiPopupState* popups
);
bool units_fountain_youth_apply_popup(
  EuropeScreen* europe, AiPopupState* popups, const ColonizeMsgCatalog* game_txt
);
bool units_fountain_youth_apply_popup_ex(
  EuropeScreen* europe,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt,
  ColonizeDosRng* rng
);
bool units_king_galleon_apply_popup_w(
  const ColonizeWorld* w,
  AiPopupState* popups,
  const ColonizeMsgCatalog* game_txt
);

/*
 * COL1 empty-hold sentinel (DOS FUN_15eb_30b8 / 317c): a hold counts as loaded
 * only when 0 < amount < 255. Returns the amount, or 0 for an empty hold.
 */
int units_hold_amount(const ColonizeUnitPool* pool, int unit_id, int hold);
/*
 * Goods holds in use = DOS unit +0x3150. GOODS only: boarding parks passengers
 * off-map (FUN_1427_10be) and never bumps the byte, so a troop-laden ship
 * counts as empty here (combat_strength.c FUN_157e_004a peel, viceroy
 * 8957-8959; units.c FUN_5bfb_312e evasion peel, viceroy 98448).
 */
int units_holds_used(const ColonizeUnitPool* pool, int unit_id);
/*
 * Two-pass DOS FUN_15eb_30b8 goods packing on a raw hold pair: top up matching
 * partial holds to 100, then append into free slots (at most max_new_slots of
 * them; pass n_holds for "unbounded"). Returns the amount actually packed.
 */
int goods_pack_into_holds(
  int* hold_types,
  int* hold_amounts,
  int n_holds,
  int cargo_type,
  int amount,
  int max_new_slots
);

/*
 * Dump first non-empty commodity hold overboard (ORDERS Dump Cargo Overboard).
 * Transport only. Returns amount dumped (0 if none/invalid).
 */
int units_dump_cargo_overboard(
  ColonizeUnitPool* pool,
  int unit_id,
  int* out_cargo_type,
  int* out_amount
);

/* Board a land unit onto an adjacent ship. Returns false if capacity/adjacency fails. */
bool units_board(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/* Board without adjacency check (COL1 import; passenger already stacked on ship tile). */
bool units_board_stacked(ColonizeUnitPool* pool, int land_unit_id, int ship_id);
/*
 * Own ship on (x,y) with free passenger capacity, or -1.
 * Cite: FUN_4720_0006 / 015c land→ocean embark probe.
 * require_galleon: Treasure Trains may only board a Galleon (Colonization.pdf
 * / P7.3) — pass true when the boarding unit's type name contains "Treasure".
 */
int units_find_boardable_ship(
  const ColonizeUnitPool* pool, int x, int y, int nation_id, bool require_galleon
);
/*
 * Departure pickup (DOS ship-switch quirk): one ascending-id first-come-
 * first-served sweep boarding sentried land units on (x,y) AND passengers
 * riding other own ships still on (x,y), until the departing ship is full.
 * Treasure Trains never transfer to a non-Galleon. Returns units taken.
 */
int units_ship_departure_pickup(ColonizeUnitPool* pool, int ship_id, int x, int y);
/* Unload oldest passenger from ship onto dest (must be enterable land). */
bool units_unload_w(
  const ColonizeWorld* w,
  int ship_id,
  int dest_x,
  int dest_y
);
/* Unload a specific passenger onto dest; charges dest terrain MP (no gift). */
bool units_unload_passenger_w(
  const ColonizeWorld* w,
  int ship_id,
  int pax_id,
  int dest_x,
  int dest_y
);
/*
 * DOS FUN_4720_015c landfall eligibility: the passenger's spent byte must be
 * BELOW its max MP. An aboard sentry parked at moves_left 0 still qualifies
 * (DOS spent == 0); one that burnt its allotment this turn (mp_spent_turn)
 * does not — it stays on the ship (bugs.md #423).
 */
bool units_cargo_can_landfall(const ColonizeUnitPool* pool, int unit_id);
/*
 * Landfall passenger pick (FUN_4720_015c): first eligible cargo, preferring
 * one with live moves_left. −1 = nobody may land this turn (DOS then refuses
 * the move outright instead of raising @LANDFALL).
 */
int units_first_landfall_cargo(const ColonizeUnitPool* pool, int ship_id);
/*
 * Pick an adjacent land tile the ship's passengers can enter (8-neighbour).
 * Prefers tiles nearer to (prefer_x, prefer_y) when prefer coords are valid;
 * pass prefer_x < 0 to ignore. Returns false if none.
 */
bool units_pick_landfall_tile_w(
  const ColonizeWorld* w,
  int ship_id,
  int prefer_x,
  int prefer_y,
  int* out_x,
  int* out_y
);
/*
 * Unload every passenger onto dest (must be enterable land adjacent/same).
 * Wakes sentry cargo. Does not change pool->selected_id. Returns count unloaded.
 * AI beachhead helper — human @LANDFALL Make Landfall unloads one unit only.
 */
int units_landfall_unload_all_w(
  const ColonizeWorld* w,
  int ship_id,
  int dest_x,
  int dest_y
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

/* Passenger slots left: capacity − passengers − holds occupied by goods
 * (goods share the slots passengers ride in). */
int units_ship_free_passenger_slots(const ColonizeUnitPool* pool, int ship_id);

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
/* Same walk, the passengers' @JOB professions (unset entries = -1) — the
 * companion array europe_harbor_push_ex / europe_enqueue_expected want so a
 * transferred passenger keeps its profession label. */
int units_export_cargo_professions(
  const ColonizeUnitPool* pool,
  int ship_id,
  int* out_profs,
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

#endif
