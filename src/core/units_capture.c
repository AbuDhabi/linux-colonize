#include "core/units.h"

/*
 * Colony capture & movement-point accounting.
 *
 * Sections:
 *  - Colony capture & movement-point accounting (units_try_capture_foreign_colony .. units_revere_defend_colony_tile)
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

/* ===================== Colony capture & movement-point accounting (units_try_capture_foreign_colony .. units_revere_defend_colony_tile) ===================== */


void units_try_capture_foreign_colony(
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  int unit_id
) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !colonies || units_is_sea(pool, unit_id)) {
    return;
  }
  const int cid = colonies_id_at(colonies, u->x, u->y);
  ColonizeColony* col = colonies_get_mut(colonies, cid);
  if (!col || !col->active) {
    return;
  }
  if (col->nation_id < 0 || col->nation_id > 3 || col->nation_id == u->nation_id) {
    return;
  }
  /*
   * Still contested if a foreign unit of the CAPTOR'S OWN DOMAIN remains on
   * the tile (units_domain_blocker_at — DOS's FUN_5fef_0000 gate). A foreign
   * ship berthed in the port is not a blocker and is not touched by the
   * capture: raw 100905-101034 flips the colony, moves the counts and the
   * treasury share, and never looks at the unit array except for the WoI
   * neighbour re-home below.
   */
  if (units_domain_blocker_at(pool, u->x, u->y, unit_id, u->nation_id) >= 0) {
    return;
  }
  /*
   * bugs.md #281: Indians never conquer — no "march into" @CAPTURED, no
   * ownership flip. DOS (FUN_5fef land-combat colony arm, ~91576): an
   * Indian winner kills ONE colonist while population > 1; only when the
   * LAST colonist falls is the colony burned to the ground
   * (@INDIANBURNCOLONY — destroy, not capture).
   */
  if (u->nation_id > 3) {
    ColonizeColony snap = *col;
    if (col->population > 1) {
      col->population--;
      if (col->colonist_count > 1) {
        col->colonist_count--;
      }
      /*
       * DOS FUN_5fef_1b0e colony arm (viceroy_unpacked_2.c ~91930): the
       * native winner's colonist kill fires the massacre dialog — human
       * victim @INDIANWINCOLONY (0x1c88), human bystander @INDIANWINCOLONY2
       * (0x1c98, "Spies report…"). Was silent (user-reported).
       */
      {
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = units_combat_nation_label(g_units_ff_col1, u->nation_id);
        tok.string1 = units_combat_nation_label(g_units_ff_col1, snap.nation_id);
        tok.string2 = reports_job_short_name(UNITS_JOB_NONE);
        tok.string3 = snap.name;
        const int victim_human =
          (g_units_ff_col1 && snap.nation_id >= 0 && snap.nation_id <= 3 &&
           g_units_ff_col1->player[snap.nation_id].control == 0) ||
          (g_units_combat_human_nation >= 0 &&
           snap.nation_id == g_units_combat_human_nation);
        if (victim_human) {
          units_combat_enqueue_tok(
            AI_POPUP_TAG_COMBAT_COLONY,
            "INDIANWINCOLONY",
            snap.nation_id,
            u->nation_id,
            0,
            &tok,
            "");
        } else if (g_units_combat_human_nation >= 0) {
          units_combat_enqueue_tok(
            AI_POPUP_TAG_INFO,
            "INDIANWINCOLONY2",
            g_units_combat_human_nation,
            u->nation_id,
            0,
            &tok,
            "");
        }
      }
      /*
       * DOS 1b0e reaches this as its `bVar28` limb — the colony had no
       * defender, so 1b0e spawned one, the native won and `local_c == 0`
       * (colony survived). Raw 101040 clears the raider's DS:0x54f6 row and
       * raw 101059-101064 vents `difficulty − 10` of alarm.
       */
      units_indian_tension_clear(g_units_ff_col1, u->home_tribe_id, snap.nation_id);
      if (!g_units_indian_combat_vent_done) {
        units_indian_attack_alarm_vent(
          g_units_ff_col1,
          u->nation_id,
          snap.nation_id,
          units_indian_attack_alarm_vent_amount(g_units_ff_col1, snap.nation_id, 10, 0)
        );
      }
      return;
    }
    (void)colonies_abandon(colonies, cid);
    /*
     * DOS native colony arm: the human victim also gets woodcut 11 (COLONY
     * BURNING) via FUN_281f_0524(0xb) before the @INDIANBURNCOLONY dialog;
     * a human bystander instead gets @INDIANBURNCOLONY2 ("Spies report…").
     */
    {
      const char* burner = units_combat_nation_label(g_units_ff_col1, u->nation_id);
      const int victim_human =
        (g_units_ff_col1 && snap.nation_id >= 0 && snap.nation_id <= 3 &&
         g_units_ff_col1->player[snap.nation_id].control == 0) ||
        (g_units_combat_human_nation >= 0 && snap.nation_id == g_units_combat_human_nation);
      if (victim_human && g_units_ff_col1) {
        (void)woodcut_fire((ColonizeCol1Save*)g_units_ff_col1, WOODCUT_COLONY_BURNING);
      }
      units_combat_notify_colony_burned(g_units_ff_col1, snap.name, snap.nation_id, burner);
      units_combat_notify_colony_burned_foreign(
        g_units_ff_col1, snap.name, snap.nation_id, burner
      );
    }
    /* Same limb with `local_c != 0` (the colony is gone): raw 101086 sets
     * `local_a6 = 0xffce` — a flat −50, with no difficulty term. */
    units_indian_tension_clear(g_units_ff_col1, u->home_tribe_id, snap.nation_id);
    if (!g_units_indian_combat_vent_done) {
      units_indian_attack_alarm_vent(g_units_ff_col1, u->nation_id, snap.nation_id, -50);
    }
    return;
  }
  int plunder = units_colony_plunder_stock_sum(col);
  const int old_nat = col->nation_id;
  ColonizeColony snap = *col;
  int share = 0;
  if (!colonies_capture_ex(colonies, cid, u->nation_id, &share)) {
    return;
  }
  (void)old_nat;
  /* DOS @CAPTURED %NUMBER0 is the treasury share (FUN_5fef_1b0e tail), not
   * a warehouse sum — the sum stays only as the no-save fallback. */
  if (g_units_ff_col1) {
    plunder = share;
  }
  units_combat_notify_colony_captured(g_units_ff_col1, &snap, u->nation_id, plunder);
  /* Raw 100937-100948: the prize claims the ring around it for its new owner. */
  units_capture_claim_ring(units_occupancy_map, u->x, u->y, u->nation_id);
  /*
   * Raw 100949-100963 (FUN_5fef_1b0e capture tail): WoI + crown winner —
   * every crown unit standing on the 8 neighbouring tiles is re-homed to
   * the captured colony (`+0x314a := *(0x8dc6)`, the port's col1_origin).
   * This is DOS's whole post-capture "garrison" step; it writes no fortify
   * order — the units simply belong to the prize and the 0a60 garrison
   * quotas keep them there.
   */
  if (g_units_ff_col1 && g_units_ff_col1->head.game_options.woi &&
      u->nation_id == (int)g_units_ff_col1->head.crown_nation_id && cid >= 0 &&
      cid <= 0x7f) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* n = &pool->units[i];
      if (!units_is_on_map(n) || n->nation_id != u->nation_id || n->id == u->id) {
        continue;
      }
      const int ddx = n->x - u->x;
      const int ddy = n->y - u->y;
      if (ddx >= -1 && ddx <= 1 && ddy >= -1 && ddy <= 1 && (ddx != 0 || ddy != 0)) {
        n->col1_origin = (uint8_t)cid;
      }
    }
  }
  /* DOS 5fef capture tail (raw 100913-100920): @HOWTOWIN "glorious victory
   * on the road to freedom" fires ONCE, on the first colony the human
   * recaptures after declaring — gate is `0x5382 & 1` (woi) only, not
   * ref_present (bugs.md #662) — latch DS:0x5386 bit0 (tut2.howtowin).
   * bugs.md #236: it does NOT fire at the declaration. */
  if (g_units_ff_col1 && g_units_ff_col1->head.game_options.woi &&
      u->nation_id == g_units_combat_human_nation &&
      !g_units_ff_col1->head.tut2.howtowin) {
    ColonizeCol1Save* mut = (ColonizeCol1Save*)g_units_ff_col1;
    mut->head.tut2.howtowin = 1;
    units_combat_enqueue_tok(
      AI_POPUP_TAG_INFO, "HOWTOWIN", u->nation_id, -1, 0, NULL,
      "");
  }
  /*
   * Raw 101032-101034, the last thing 1b0e's capture arm does: DOS drops a
   * HUMAN captor straight into the town it just took —
   * `if (attacker < 4 && player[attacker].control == 0)
   *      FUN_281f_0608(colony)` (far thunk → FUN_2f2b_6cd4, the colony
   * screen). It sits AFTER the blocking @CAPTURED dialog (raw 101030), so
   * the popup is answered first and the screen comes up behind it.
   *
   * The port already owns that beat: an elected colony zoom is drained by
   * game_loop's `ai_popup_take_colony_zoom` → `game_enter_colony`, which is
   * how DOS's other FUN_281f_0608 caller (FUN_364b_0688's colony-event tail)
   * is wired. Pump the queue first so @CAPTURED / @HOWTOWIN keep DOS's
   * order — headless callers install no pump and no popup state, so both
   * calls are inert there.
   */
  if (g_units_combat_popups && u->nation_id >= 0 && u->nation_id <= 3 && cid >= 0 &&
      ((g_units_ff_col1 && g_units_ff_col1->player[u->nation_id].control == 0) ||
       (g_units_combat_human_nation >= 0 && u->nation_id == g_units_combat_human_nation))) {
    units_combat_pump_popups();
    ai_popup_colony_zoom_elect(g_units_combat_popups, cid);
  }
}

/*
 * DOS entry seizure ("0512 seizes whatever stands there"): once a colony's
 * defense is beaten, the non-combat foreigners still standing on the tile are
 * taken with it — Colonists/Wagons flip to the captor, the rest are destroyed
 * (units_apply_land_loss_outcome per unit). Without this, a bystanding
 * civilian keeps the tile "contested" and blocks the capture forever.
 */
void units_seize_noncombat_at(
  ColonizeUnitPool* pool, int winner_id, int x, int y, const ColonizeCol1Save* col1
) {
  const ColonizeUnit* win = units_get_const(pool, winner_id);
  if (!win) {
    return;
  }
  const int win_nat = win->nation_id;
  int slot_4 = 0;
  for (ColonizeUnit* u = units_next_on_tile(pool, x, y, &slot_4); u != NULL;
       u = units_next_on_tile(pool, x, y, &slot_4)) {
    if (u->id == winner_id || u->nation_id == win_nat) {
      continue;
    }
    /*
     * Smell audit 2026-09-09 #4. The gate used to be a bare `type->attack ==
     * 0`, which split the tile the wrong way twice over:
     *
     *  - Berthed HULLS. Caravel / Merchantman / Galleon carry attack 0, so
     *    they were swept into units_apply_land_loss_outcome — which has no
     *    capture or demote row for a ship and therefore silently DESPAWNED
     *    them — while Privateer / Frigate / Man-O-War (attack > 0) were left
     *    alone. That accidental split contradicts this file's own domain rule
     *    (units_domain_blocker_at, FUN_5fef_0000 raw 99186-99195) and the
     *    ai_euro caller's comment: a land walk-in neither sees nor touches a
     *    hull in the harbour. Skip every ship, not just the armed ones.
     *    (DOS's own 0ec0 does hand hulls to 0352's damage/repair-port arm,
     *    raw 99518-99649 — ported 2026-09-09 in units_sweep_stack_after_loss.
     *    It is NOT wired here: this helper stands in for the colony walk-in,
     *    and DOS's own colony-fall purge FUN_43f7_0512 destroys hulls outright
     *    with @SEIZURESEA (ai_king.c ai_king_0982_purge_tile) rather than
     *    routing them to a repair port. Neither DOS path says "damage them
     *    here", so the skip stands.)
     *
     *  - Armed colonist BODIES. `attack == 0` is true for a Colonists-type
     *    unit carrying muskets (bugs.md: how a colony-armed soldier is stored
     *    here), which is DOS type 1 "Soldiers", attack 2. Ask the body
     *    question instead. In practice this is belt-and-braces: all three
     *    callers reach this only after units_best_defender_at returned < 0,
     *    and that picker ranks muskets/horses carriers in its ARMED tier —
     *    an armed body on the tile would have been fought, not seized.
     */
    if (units_is_sea(pool, u->id) || units_is_combat_role(pool, u)) {
      continue;
    }
    (void)units_apply_land_loss_outcome(pool, u->id, winner_id, col1, 1, NULL);
  }
}

/* Boarding helpers are defined later; enter_probe needs the embark probe. */
int units_find_boardable_ship(
  const ColonizeUnitPool* pool, int x, int y, int nation_id, int need_space
);

ColonizeEnterReason units_enter_probe_w(
  const ColonizeWorld* w,
  int type_index,
  int x,
  int y,
  int mover_id
) {
  const ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
  if (!pool || type_index < 0 || type_index >= pool->type_count || !map) {
    return g_units_last_enter_reason;
  }
  /* DOS `0x0d <= type <= 0x12` (the @UNIT ship band) — needed by the rim
   * test below, before the rest of the type reads (bugs.md #717). */
  const bool sea_probe =
    pool->types[type_index].domain == COLONIZE_UNIT_DOMAIN_SEA;
  /*
   * bugs.md #429: DOS's in-bounds predicate is FUN_137f_000a
   * (viceroy_unpacked.c:6519-6531) — `x < 1 || y < 1 || map_w-1 <= x ||
   * map_h-1 <= y` is OUT. The playable board is the interior 1..w-2 / 1..h-2;
   * the 1-tile rim exists only as map data. This probe tested the raw array
   * bounds instead, so a unit could step onto the rim — and the rim is a tile
   * the viewport can never scroll to (FUN_6ba1_000c clamps the view origin to
   * >= 1, and so does map_panel_clamp_view_origin) and no map click can
   * address (the click handler gates on map_coords_inset). On the Americas map
   * that bites at the west sea lane, which IS column 1: one more step west
   * dropped the ship onto column 0, off the drawable map — "the western edge
   * is cut off one tile too soon".
   *
   * Degenerate fixture maps (< 3 tiles on a side) have no interior at all;
   * keep the old raw-bounds behaviour there rather than blocking every move.
   */
  const bool rim_meaningful = map->width >= 3 && map->height >= 3;
  if (x < 0 || y < 0 || x >= map->width || y >= map->height ||
      (rim_meaningful && !map_coords_inset(map, x, y))) {
    /*
     * DOS-LITERAL FUN_4720_015c head (raw 75960-75977, bugs.md #717). The two
     * axes are NOT the same refusal:
     *   if (y < 1) return 0;                       // silent, no reason word
     *   if (map_h - 1 <= y) return 0;              // silent
     *   if (map_w - 1 <= x || x < 1) {             // the EAST/WEST rim
     *       if (type < 0x0d || 0x12 < type) return 0;   // land unit: silent
     *       *(int *)0x9e4e = 4; return 0;               // ship: reason 4
     *   }
     * Reason 4's UI arm is the jump table at 4720:060a
     * (viceroy_ndisasm.asm 0x3FF4A, index = reason-1 after the `dec ax` at
     * 0x3FF38): entries 3 AND 4 both point at 0x0566 = 0x3FEA6, the very body
     * reason 5 uses — @EUROPENOTLEAVE when DS:0x5382 bit0 is set, otherwise
     * the @SAILHOME (DS:0x140c) Yes/No. Yes sails for Europe. The two reasons
     * part only in the tail at 0x3FEEA: `cmp word [0x9e4e],4` jumps to the
     * abort at 0xfff2 for reason 4, where reason 5 falls into the commit at
     * 0xff5c. So a ship pushed off the east/west rim is offered the voyage
     * home and a "No" simply leaves it where it stands.
     */
    const bool y_out = y < 0 || y >= map->height ||
                       (rim_meaningful && (y < 1 || y >= map->height - 1));
    if (!y_out && sea_probe) {
      g_units_last_enter_reason = COLONIZE_ENTER_EDGE_SAIL;
      return g_units_last_enter_reason;
    }
    g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_EDGE;
    return g_units_last_enter_reason;
  }

  int mover_nation = -1;
  const ColonizeUnit* mover = (mover_id >= 0) ? units_get_const(pool, mover_id) : NULL;
  if (mover) {
    mover_nation = mover->nation_id;
  }

  const ColonizeUnitType* type = &pool->types[type_index];
  const bool sea = type->domain == COLONIZE_UNIT_DOMAIN_SEA;
  const bool water = map_tile_is_water(map, x, y);
  const bool land = map_tile_is_land(map, x, y);

  /*
   * FUN_5fef_0000 domain gate (raw 99186-99195): a candidate defender's
   * ship-ness must match the destination tile's water test, so a berthed
   * foreign hull neither defends a port nor blocks the assault. Prefer a
   * domain-matching foe; when only mismatched foreigners stand on a colony
   * tile, ignore them and fall through to the colony entry rules below
   * (the walk-in capture gate re-applies the same test). Open ground keeps
   * the old first-found semantics.
   */
  /*
   * DOS-LITERAL FUN_4720_015c reason 9 = @LANDFIRST (DS tag 0x1429; the UI
   * dispatcher FUN_4720_049e's jump table at 4720:060a maps reason 9 to
   * `push 0x1429`, viceroy_unpacked_2.asm 4720:05f0). Decomp raw
   * 74720-74728:
   *   if (dest not ocean/HS && mover type < 0xd|| > 0x12       // land unit
   *       && (origin terrain class == 0x19 || 0x1a)            // stands on water
   *       && tile_tribe_or_presence(dest) >= 0                 // occupied
   *       && (mover nation & 0xf) != that owner)               // by a foreigner
   *       { reason = 9; move invalid; }
   * GAME.TXT says it in words: "Land units cannot enter an enemy occupied
   * square from on board a ship. You must first unload them into an empty or
   * friendly-occupied square." So there is no amphibious assault in DOS —
   * neither on a unit stack, nor on a village, nor a walk-in capture of an
   * empty enemy colony. The port used to route a passenger's step onto an
   * occupied tile into COMBAT_LAND and let it fight from the deck.
   *
   * DOS reads the layer2 presence/tribe bit plus the layer3 owner nibble;
   * the port asks the live pools instead, because many fixtures never run the
   * occupancy sync (col1_bridge_sync_map_occupancy) and would silently lose
   * the gate.
   */
  if (mover && !sea && land && map_tile_is_water(map, mover->x, mover->y)) {
    int occupant_owner = -1;
    int slot_a = 0;
    for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot_a); u != NULL;
         u = units_next_on_tile_const(pool, x, y, &slot_a)) {
      if (u->id == mover_id || u->aboard_ship_id >= 0) {
        continue;
      }
      occupant_owner = u->nation_id;
      break;
    }
    if (occupant_owner < 0 && colonies) {
      const ColonizeColony* col = colonies_get(colonies, colonies_id_at(colonies, x, y));
      if (col && col->active) {
        occupant_owner = col->nation_id;
      }
    }
    if (occupant_owner < 0 && map->layer2) {
      /* Native village = HAS_CITY with no colony record; its owner is the
       * layer3 high nibble (COLONYFLAG invariant, docs/conventions.md). */
      const size_t vidx = (size_t)y * (size_t)map->width + (size_t)x;
      if (vidx < (size_t)map->width * (size_t)map->height &&
          (map->layer2[vidx] & MAP_OCCUPANCY_HAS_CITY) != 0) {
        occupant_owner = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
        if (occupant_owner > 11) {
          occupant_owner = -1; /* 0xf = unowned */
        }
      }
    }
    if (occupant_owner >= 0 && occupant_owner != mover_nation) {
      g_units_last_enter_reason = COLONIZE_ENTER_LANDFIRST;
      return g_units_last_enter_reason;
    }
  }

  /*
   * bugs.md #553: a ship stepping into its OWN colony docks. DOS
   * FUN_465b_0000 reads the destination's settlement owner (FUN_281f_06be,
   * raw 75467) and lets only the stack head's nation (FUN_281f_07e0) override
   * it; every DOS path that brings a foreign unit onto a colony tile is an
   * attack or capture, so the stack of an own colony is always own. Foreign
   * units found there are port leftovers (a Brave that walked in before the
   * 465b foreign-destination arm was ported in ai_native_brave_step — live
   * saves carry one), and must not bar the owner's ships.
   */
  bool own_colony_dock = false;
  if (sea && land && colonies && mover_nation >= 0) {
    const ColonizeColony* oc = colonies_get(colonies, colonies_id_at(colonies, x, y));
    own_colony_dock = oc && oc->active && oc->nation_id == mover_nation;
  }
  /*
   * A ship stepping onto a native village tile is dispatched to
   * FUN_4d56_4528 at 465b raw 75484-75492 — BEFORE 465b ever looks at who
   * stands on the tile — so the Brave that normally sits inside a dwelling
   * must not turn the step into a silent domain refusal (bugs.md: galleon
   * cannot trade with an occupied village).
   */
  bool village_ship = false;
  if (sea && land && map->layer2) {
    const size_t vidx2 = (size_t)y * (size_t)map->width + (size_t)x;
    if (vidx2 < (size_t)map->width * (size_t)map->height &&
        (map->layer2[vidx2] & MAP_OCCUPANCY_HAS_CITY) != 0 &&
        (!colonies || colonies_id_at(colonies, x, y) < 0)) {
      village_ship = true;
    }
  }
  int foe = -1;
  if (!own_colony_dock && !village_ship) {
    int foe_mismatch = -1;
    int slot_d = 0;
    for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot_d); u != NULL;
         u = units_next_on_tile_const(pool, x, y, &slot_d)) {
      if (u->id == mover_id) {
        continue;
      }
      if (mover_nation >= 0 && u->nation_id == mover_nation) {
        continue;
      }
      if (units_is_sea(pool, u->id) == water) {
        foe = u->id;
        break;
      }
      if (foe_mismatch < 0) {
        foe_mismatch = u->id;
      }
    }
    if (foe < 0 && foe_mismatch >= 0) {
      const int dom_cid = colonies ? colonies_id_at(colonies, x, y) : -1;
      if (dom_cid < 0) {
        foe = foe_mismatch;
      }
    }
  }
  if (foe >= 0) {
    const bool foe_sea = units_is_sea(pool, foe);
    if (sea && foe_sea) {
      const ColonizeUnit* fu = units_get_const(pool, foe);
      const int foe_nation = fu ? fu->nation_id : -1;
      if (mover && !units_at_war_for_move(mover_nation, foe_nation)) {
        g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_PEACE;
      } else if (mover && !units_is_combat_role(pool, mover)) {
        /*
         * bugs.md: unarmed transports (Caravel/Merchantman/Galleon, attack=0
         * in @UNIT) must not be able to initiate ship combat — GAME.TXT:
         * "Only Privateers and Frigates can attack enemy ships." Bounce
         * instead of fighting; the foreign ship's own attack stat is
         * irrelevant here, only the mover's.
         */
        g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_FOREIGN;
      } else {
        g_units_last_enter_reason = COLONIZE_ENTER_COMBAT_NAVAL;
      }
      return g_units_last_enter_reason;
    }
    if (sea != foe_sea) {
      /*
       * FUN_4720_015c raw 75989-76032: reason 9 (@LANDFIRST, 0x1429) is only
       * reachable for a land-type mover standing on water (handled above).
       * A ship (type 0xd..0x12) bumping foreign-held land falls through with
       * no reason word: silent refusal (bugs.md #486).
       */
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
      return g_units_last_enter_reason;
    }
    /* Land × land foreign. */
    if (!mover) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
      return g_units_last_enter_reason;
    }
    const ColonizeUnit* fu = units_get_const(pool, foe);
    const int foe_nation = fu ? fu->nation_id : -1;
    if (!units_is_combat_role(pool, mover)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_FOREIGN;
      return g_units_last_enter_reason;
    }
    /* No Treasure exception here: there is no `type == 0x0a` term anywhere in
     * the DOS move/attack chain (FUN_465b_0000 raw 75417-75843) beyond the
     * King's-Galleon arm. DOS's "braves find treasures hard to resist" is a
     * WEIGHT, not a war bypass: the FUN_4d56_021a direction scorer adds +0x10
     * for a Treasure/Artillery/Wagon on the candidate tile, already ported at
     * ai.c:3520. */
    const bool grudge = mover_nation >= 4 && foe_nation >= 0 && foe_nation <= 3 &&
      units_native_village_grudge(mover, foe_nation);
    if (!grudge && !units_at_war_for_move(mover_nation, foe_nation)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_PEACE;
      return g_units_last_enter_reason;
    }
    g_units_last_enter_reason = COLONIZE_ENTER_COMBAT_LAND;
    return g_units_last_enter_reason;
  }

  if (sea) {
    /*
     * 4720 reason 5 (FUN_4720_015c ~76048): deny only when the ship is
     * ALREADY on a high-seas tile (`uVar9 == 0x1a`) and steps further east
     * off the map without a Go To / Trade Route order. Entering the sea
     * lane from ordinary ocean is always legal — the port used to test the
     * destination alone, which blocked every first step onto the lane and
     * also made units_set_goto refuse a sea-lane destination outright
     * (bugs.md: "I can't move a ship onto a sea lane either way").
     */
    if (mover && map_tile_is_high_seas(map, x, y) &&
        map_tile_is_high_seas(map, mover->x, mover->y) && x > mover->x &&
        !units_orders_follow_goto(mover->orders)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_HS_SAIL;
      return g_units_last_enter_reason;
    }
    if (water) {
      /*
       * GAME.TXT @SHIPLAKE: "Ship units cannot enter inland lake squares."
       * map_tile_is_lake's own DOS-faithful test (region nibble != 1) treats
       * an unpopulated/zeroed layer3 (nibble 0 — many synthetic unit-test and
       * AI fixtures never run the water connected-component pass) as a lake
       * too, which would spuriously bounce every ship on those boards. A
       * real generated map's main sea body is always exactly region 1 and a
       * real enclosed lake is >= 2, so gate on that stricter reading here
       * instead of reusing the shared predicate.
       */
      if (map_pedia_terrain_index_at(map, x, y) == 25 &&
          (int)(map_get_layer3(map, x, y) & 0x0fu) > 1) {
        g_units_last_enter_reason = COLONIZE_ENTER_LAKE_BLOCKED;
        return g_units_last_enter_reason;
      }
      g_units_last_enter_reason = COLONIZE_ENTER_OK;
      return g_units_last_enter_reason;
    }
    if (land && colonies && mover_nation >= 0) {
      const int cid = colonies_id_at(colonies, x, y);
      const ColonizeColony* col = colonies_get(colonies, cid);
      if (col && col->active && col->nation_id == mover_nation) {
        g_units_last_enter_reason = COLONIZE_ENTER_DOCK;
        return g_units_last_enter_reason;
      }
      /*
       * A foreign colony is NEVER enterable, Jan de Witt or not: DOS
       * FUN_465b_0000 (raw 75491-75500) hands the step to FUN_5f7a_0662,
       * which trades from outside and aborts the move. The "de Witt docks in
       * a foreign port" arm that used to sit here was a fandom invention.
       * See docs/foreign_colony_trade.md.
       */
      if (col && col->active) {
        g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
        return g_units_last_enter_reason;
      }
    }
    if (land) {
      /*
       * Native village (HAS_CITY, no Euro colony): ship abort path in
       * FUN_4d56_4528 — not landfall. Cite: indian_settlement_4528.md.
       */
      if (map->layer2) {
        const size_t idx = (size_t)y * (size_t)map->width + (size_t)x;
        if (idx < (size_t)map->width * (size_t)map->height &&
            (map->layer2[idx] & MAP_OCCUPANCY_HAS_CITY) != 0) {
          const int cid = colonies ? colonies_id_at(colonies, x, y) : -1;
          if (cid < 0) {
            g_units_last_enter_reason = COLONIZE_ENTER_VILLAGE_SHIP;
            return g_units_last_enter_reason;
          }
        }
      }
      g_units_last_enter_reason = COLONIZE_ENTER_LANDFALL;
      return g_units_last_enter_reason;
    }
    g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
    return g_units_last_enter_reason;
  }

  if (!land) {
    /*
     * Land → ocean/HS: embark if own ship on dest has room (FUN_4720_015c /
     * 0006). Otherwise domain deny.
     */
    /* FUN_4720_00e0's param_2 = the boarder's own @UNIT size (DS:0x5238). */
    const int board_need = type->space > 0 ? type->space : 1;
    if (mover_nation >= 0 &&
        units_find_boardable_ship(pool, x, y, mover_nation, board_need) >= 0) {
      g_units_last_enter_reason = COLONIZE_ENTER_BOARD;
      return g_units_last_enter_reason;
    }
    g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
    return g_units_last_enter_reason;
  }
  if (units_village_squat_illegal(pool, type, mover, map, x, y, mover_nation, colonies)) {
    g_units_last_enter_reason = COLONIZE_ENTER_VILLAGE_ILLEGAL;
    return g_units_last_enter_reason;
  }
  /*
   * DOS-LITERAL FUN_465b_0000 raw 75510-75516 (bugs.md #720). The
   * destination's owner (`local_4`) is read from the SETTLEMENT first —
   * FUN_281f_06be, raw 75467 — and only overwritten by an occupying unit's
   * nation when one stands there (FUN_281f_07e0), so an UNDEFENDED foreign
   * colony still sets bVar4. The Euro trade hand-off FUN_2a1f_015e ->
   * FUN_5f7a_0662 (raw 75499) declines for a unit whose @UNIT cargo column
   * DS:0x5237 is 0, and the step then reaches
   *   if (0x5236[type] == 0) {            // @UNIT attack column
   *       if (land type && nation < 4 && control == 0) FUN_281f_03fe(0x13a0);
   *       goto LAB_465b_0bd1;             // no entry, no capture, no MP
   *   }
   * (ASM viceroy_overlays.asm:112490-112492). So a plain colonist bumping an
   * empty foreign colony bounces on @CANNOTATTACK; it never walks in and
   * flips the owner. Wagons/ships take the trade arm above this one.
   */
  if (colonies && mover && mover_nation >= 0 && !units_is_combat_role(pool, mover)) {
    const int fcid = colonies_id_at(colonies, x, y);
    const ColonizeColony* fcol = colonies_get(colonies, fcid);
    if (fcol && fcol->active && fcol->nation_id >= 0 && fcol->nation_id != mover_nation) {
      g_units_last_enter_reason = COLONIZE_ENTER_BOUNCE_FOREIGN;
      return g_units_last_enter_reason;
    }
  }
  g_units_last_enter_reason = COLONIZE_ENTER_OK;
  return g_units_last_enter_reason;
}


bool units_can_enter_w(
  const ColonizeWorld* w,
  int type_index,
  int x,
  int y,
  int mover_id
) {
  const ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  const ColonizeEnterReason r =
    units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, type_index, x, y, mover_id);
  return r == COLONIZE_ENTER_OK || r == COLONIZE_ENTER_DOCK;
}


int units_type_max_mp(const ColonizeUnitType* type) {
  const int tiles = type && type->movement > 0 ? type->movement : 1;
  return tiles * UNITS_MP_PER_TILE;
}

/*
 * DOS `uVar15 = unit_max_mp(u) - u->moves_spent` (FUN_5fef_1b0e ~100340) in
 * the port's own units: Euro units keep REMAINING thirds in moves, while
 * native units keep the DOS SPENT byte there (turn.c's refresh and the COL1
 * bridge both say so), so the two have to be read differently.
 */
int units_remaining_mp(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active) {
    return 0;
  }
  if (u->nation_id >= 4) {
    const int max_mp = units_max_mp(pool, unit_id);
    const int rem = max_mp - u->moves;
    return rem < 0 ? 0 : rem;
  }
  return u->moves < 0 ? 0 : u->moves;
}

/*
 * Writer counterparts of units_remaining_mp — every MP charge/exhaust on a
 * unit that may be native has to go through these, since natives grow the
 * SPENT byte toward max while Euros shrink REMAINING toward 0 (smell audit
 * 2026-09-08 cross-cutting item: raw moves writes inverted the charge
 * for natives — a drain became a refresh).
 */
void units_mp_charge(const ColonizeUnitPool* pool, ColonizeUnit* u, int cost) {
  if (!u || cost <= 0) {
    return;
  }
  const int max_mp = units_max_mp(pool, u->id);
  if (u->nation_id >= 4) {
    u->moves += cost;
    if (u->moves > max_mp) {
      u->moves = max_mp;
    }
  } else {
    u->moves -= cost;
    if (u->moves < 0) {
      u->moves = 0;
    }
  }
}

void units_mp_exhaust(const ColonizeUnitPool* pool, ColonizeUnit* u) {
  if (!u) {
    return;
  }
  u->moves = (u->nation_id >= 4) ? units_max_mp(pool, u->id) : 0;
}

/*
 * Restore side of the same inversion (smell audit 2026-09-09 #6): "this unit
 * has its whole allotment again" is moves = max for a Euro unit but
 * moves = 0 (nothing SPENT) for a native one. A raw `= units_max_mp()`
 * therefore reads as *fully spent* on a Brave. Every park/refund site goes
 * through this so the trap cannot come back by copy-paste.
 */
/*
 * Public spelling of the same "FUN_281f_0934: spend the whole allotment" act,
 * for callers outside units.c (game_dialogs.c's respect-their-wishes arm,
 * bugs.md #616/#626) that used to re-derive the nation>=4 inversion inline.
 */
void units_mp_exhaust_unit(ColonizeUnitPool* pool, int unit_id) {
  units_mp_exhaust(pool, units_get(pool, unit_id));
}

void units_mp_restore(const ColonizeUnitPool* pool, ColonizeUnit* u) {
  if (!u) {
    return;
  }
  u->moves = (u->nation_id >= 4) ? 0 : units_max_mp(pool, u->id);
}

int units_max_mp(const ColonizeUnitPool* pool, int unit_id) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  const ColonizeUnitType* type = u ? units_type(pool, u->type_index) : NULL;
  int mp = units_type_max_mp(type);
  /* FUN_1427_065a: +3 for ship types when the nation's capability bit 5 is
   * set — Magellan (the only naval +1 in the game). */
  if (u && type && type->domain == COLONIZE_UNIT_DOMAIN_SEA && g_units_ff_col1 &&
      u->nation_id >= 0 && u->nation_id <= 3 &&
      founding_fathers_nation_has(g_units_ff_col1, u->nation_id, FF_FERDINAND_MAGELLAN)) {
    mp += UNITS_MP_PER_TILE;
  }
  return mp;
}

void units_format_mp(int thirds, char* out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (thirds < 0) {
    thirds = 0;
  }
  const int whole = thirds / UNITS_MP_PER_TILE;
  const int rem = thirds % UNITS_MP_PER_TILE;
  if (rem == 0) {
    snprintf(out, out_size, "%d", whole);
  } else if (whole == 0) {
    snprintf(out, out_size, "%d/3", rem);
  } else {
    snprintf(out, out_size, "%d %d/3", whole, rem);
  }
}

int units_move_cost(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  int dest_x,
  int dest_y
) {
  if (!map) {
    return UNITS_MP_PER_TILE;
  }
  if (units_is_sea(pool, unit_id)) {
    /* DOS: terr_cost[ocean] = 1 -> 3 thirds per sea tile. */
    return UNITS_MP_PER_TILE;
  }
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    return map_move_cost_at(map, dest_x, dest_y) * UNITS_MP_PER_TILE;
  }
  return map_move_spent_thirds(map, u->x, u->y, dest_x, dest_y);
}

bool units_can_afford_move_cost(const ColonizeUnitPool* pool, int unit_id, int cost) {
  const ColonizeUnit* unit = units_get_const(pool, unit_id);
  const int remaining = units_remaining_mp(pool, unit_id);
  if (!unit || !unit->active || remaining <= 0) {
    return false;
  }
  if (cost <= remaining) {
    return true;
  }
  /* Full allotment remaining (DOS: spent MP byte == 0) → always allow. */
  if (remaining >= units_max_mp(pool, unit_id)) {
    return true;
  }
  /* Partial overspend needs an RNG roll in units_try_move — not guaranteed. */
  return false;
}

/* Standing military defender on a colony tile (PEDIA Revere "standing soldiers"). */
static bool units_is_standing_soldier(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!pool || !u || !u->active) {
    return false;
  }
  if (u->muskets > 0) {
    return true;
  }
  const ColonizeUnitType* t = units_type(pool, u->type_index);
  /* bugs.md #581: the @UNIT row class alone. The muskets test above already
   * covers an armed Colonists-type body; the old display-name arm resolved to
   * UNKNOWN in production (no name resolver is installed outside tests). */
  return units_kind_is_military(units_type_kind(t));
}

static bool units_colony_has_soldier_on_tile(
  const ColonizeUnitPool* pool,
  int x,
  int y,
  int colony_nation
) {
  if (!pool || colony_nation < 0) {
    return false;
  }
  int slot_c = 0;
  for (const ColonizeUnit* u = units_next_on_tile_const(pool, x, y, &slot_c); u != NULL;
       u = units_next_on_tile_const(pool, x, y, &slot_c)) {
    if (u->nation_id != colony_nation) {
      continue;
    }
    if (units_is_standing_soldier(pool, u)) {
      return true;
    }
  }
  return false;
}

/*
 * W1.8 / P5.4: undefended-Euro-colony token militia. DOS FUN_5fef_1b0e's
 * "no live defender found" branch, colony_at_xy>=0 half (viceroy_unpacked.c
 * 100417-100432): picks a random colonist (FUN_281f_04d4 RNG(0,+0x1f-1),
 * +0x1f already named colonist_count elsewhere), reads their job
 * (FUN_281f_0c54 → FUN_15eb_0e52, colony +0x40+idx) and maps
 * profession→ICONS.SS index (FUN_281f_02c6) for a weak civilian stand-in — a
 * phantom, not a real colonist, so it never touches the colony's actual
 * population. Mirrors the already-ported village empty-dwelling Brave arm
 * (units_spawn_village_temp_defender).
 *
 * The phantom is a scratch @UNIT row in DOS, not a real type: FUN_291f_0a20
 * (= FUN_478c_002c, raw 76545-76564) stamps unit type 0x17 on the new unit
 * and FUN_478c_0002 (raw 76530-76543) writes that type's table row 0x17
 * (0x5230 + 0x17*0xe = 0x5372) from the two arguments — graphic at +2, and
 * BOTH combat columns (+5 defense, +6 attack) from the passed base combat.
 * Undoing it is FUN_291f_0a06 = FUN_478c_00d0 (raw 76594-76601): "if the last
 * unit's type byte is 0x17, delete it", i.e. the phantom always evaporates.
 *
 *   local_de = *(byte*)0x5235      → @UNIT row 0 (Colonists) DEFENSE = 1
 *   Revere arm (raw 100424-100429):
 *     if (FUN_281f_07b4(owner, 0xc) && colony.Muskets(+0xb8) > 0x31) {
 *       local_94 = 0x4b; local_de += 1; *(byte*)0x8d03 |= 4;
 *     }
 *
 * So the Revere phantom is graphic 0x4b (= this port's sprite 74,
 * UNITS_ICON_SOLDIER — the plain armed-colonist map pose) with attack 2 /
 * defense 2. That is byte-for-byte the port's "Soldiers" @UNIT row (NAMES.TXT
 * `Soldiers, 103, 1, 2, 2, …`), so the Revere phantom simply spawns as that
 * type; the plain militia phantom keeps Colonists (defense 1). The defense
 * value also feeds DOS's halving peel `*(byte*)(def_type*0xe+0x5235) < 2`
 * (combat_strength.c `dt->defense < 2`), which the Revere phantom escapes and
 * the plain one does not — same as here.
 *
 * NOT ported because DOS does not do it: no muskets are spent. 1b0e reads
 * colony +0xb8 exactly twice (raw 100425 as this gate, raw 100708 as the
 * "tribe gains muskets" test when a burned colony had any) and never writes
 * it, on any outcome.
 */
int units_spawn_colony_temp_defender(
  ColonizeUnitPool* pool,
  const ColonizeColony* col,
  bool revere_armed
) {
  if (!pool || !col || !col->active || col->colonist_count <= 0) {
    return -1;
  }
  int ti = -1;
  if (revere_armed) {
    ti = units_kind_type_index(pool, UNITS_KIND_SOLDIER);
  }
  if (ti < 0) {
    ti = units_kind_type_index(pool, UNITS_KIND_COLONIST);
  }
  if (ti < 0) {
    ti = units_kind_type_index(pool, UNITS_KIND_COLONIST);
  }
  if (ti < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(pool, ti, col->x, col->y);
  ColonizeUnit* u = units_get(pool, id);
  if (!u) {
    return -1;
  }
  u->nation_id = col->nation_id;
  return id;
}

/*
 * PEDIA Paul Revere: when stepping onto a foreign colony with no map unit and
 * no standing soldiers, the town's militia turns out and fights. Revere is
 * NOT a separate mechanism in DOS — 1b0e always builds the same phantom
 * defender (units_spawn_colony_temp_defender) and Revere only *overrides* its
 * graphic (0x4b) and base combat (+1), exactly the `FUN_281f_07b4`/`+0xb8`
 * gate at raw 100424-100429. The colonist never leaves the colony, the
 * warehouse muskets are never spent, and the phantom always evaporates after
 * the roll (FUN_291f_0a06).
 *
 * Loss consequence lives where DOS puts it — the walk-in that follows, raw
 * 100680-100713 inside `if (bVar8) { … if (bVar28) { … } }`:
 *   • Euro attacker  → colony is CAPTURED (`bVar12`), no colonist dies.
 *   • Native attacker, colony pop > 1 → FUN_281f_0a9c(local_b0)
 *     (= FUN_15eb_0d04: shift the colonist arrays down, colony +0x1f -= 1).
 *   • Native attacker, colony pop == 1 → colony destroyed (FUN_291f_0254),
 *     tribe gains horses/muskets if the town held any.
 * All three are already ported in units_try_capture_foreign_colony, which
 * runs on the attacker's entry step right after this returns true.
 *
 * Returns true if the move may continue (no fight, or attacker won). False if
 * the attacker lost / despawned. Requires g_units_ff_col1.
 */
bool units_revere_defend_colony_tile(
  ColonizeUnitPool* pool,
  ColonizeColonyPool* colonies,
  int attacker_id,
  int dest_x,
  int dest_y,
  ColonizeDosRng* rng
) {
  if (!pool || !colonies || !g_units_ff_col1) {
    return true;
  }
  ColonizeUnit* atk = units_get(pool, attacker_id);
  if (!atk || !atk->active || units_is_sea(pool, attacker_id)) {
    return true;
  }
  const int cid = colonies_id_at(colonies, dest_x, dest_y);
  ColonizeColony* col = colonies_get_mut(colonies, cid);
  if (!col || !col->active) {
    return true;
  }
  if (col->nation_id < 0 || col->nation_id > 3 || col->nation_id == atk->nation_id) {
    return true;
  }
  const bool has_soldier =
    units_colony_has_soldier_on_tile(pool, dest_x, dest_y, col->nation_id);
  const bool revere_armed = founding_fathers_revere_should_auto_arm(
    g_units_ff_col1, col->nation_id, has_soldier, col->stock[COLONIZE_CARGO_MUSKETS]
  );
  const int def_id = units_spawn_colony_temp_defender(pool, col, revere_armed);
  if (def_id < 0) {
    return true; /* nobody home at all (pop 0) — nothing to defend with */
  }
  if (revere_armed) {
    /* DOS `OR byte ptr [0x8d03],4` (asm 5fef:1d5b) — Combat Analysis
     * Muskets row on the defender side. Gate is identical: FF 12 owned +
     * colony Muskets word > 0x31 (UNITS_EQUIP_MUSKETS). */
    g_units_revere_muskets_latch = 1;
  }
  /* DOS bVar28: this defender did not exist before the attack — 1b0e picks a
   * different `local_a6` row for it (see units_indian_attack_alarm_vent), and
   * the strength peels read it for the Discoverer beginner shield (raw
   * 100544: `if (bVar28 && difficulty == 0) local_92 = 0`). */
  g_units_colony_autodefender = true;
  combat_set_auto_defender(true);
  const bool won = units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .col1=(ColonizeCol1Save*)(g_units_ff_col1), .col1_ok=((g_units_ff_col1) != NULL), .rng=(ColonizeDosRng*)(rng)}, attacker_id, def_id);
  combat_set_auto_defender(false);
  g_units_colony_autodefender = false;
  if (won && units_combat_is_visible(pool, attacker_id, def_id)) {
    units_play_event_sound(UNITS_SFX_COMBAT_WON);
  }
  /* FUN_291f_0a06 / FUN_478c_00d0: the phantom always vanishes after the
   * roll, win or lose (it may have been demoted or flipped by the shared
   * outcome path first — DOS's own 0x17-typed row never survives either). */
  {
    ColonizeUnit* d = units_get(pool, def_id);
    if (d && d->active) {
      units_despawn(pool, def_id);
    }
  }
  if (!won) {
    return false;
  }
  return units_get(pool, attacker_id) != NULL;
}
