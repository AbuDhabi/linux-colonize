#include "core/units.h"

/*
 * Movement, orders, pathfinding, goto/follow & route caching.
 *
 * Sections:
 *  - Unit movement, orders & basic order commands (units_try_move .. units_order_trade_route)
 *  - Pillage/disband/wake, goto/follow flood-fill & greedy pathfinding (units_dump_cargo_overboard .. units_greedy_next_step)
 *  - /* ======================================================================
 *  - Coarse-grid route caching & goto step advancement (units_coarse_domain_tile .. units_advance_goto)
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

/* ===================== Unit movement, orders & basic order commands (units_try_move .. units_order_trade_route) ===================== */


bool units_try_move_w(
  const ColonizeWorld* w,
  int unit_id,
  int dest_x,
  int dest_y
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;
  ColonizeDosRng* rng = w->rng;

  g_units_last_combat = 0;
  g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
  ColonizeUnit* unit = units_get(pool, unit_id);
  if (!unit || !map) {
    return false;
  }
  if (unit->aboard_ship_id >= 0) {
    return false;
  }
  if (units_remaining_mp(pool, unit_id) <= 0) {
    g_units_last_enter_reason = COLONIZE_ENTER_NO_MP;
    return false;
  }
  if (unit->x == dest_x && unit->y == dest_y) {
    return false;
  }
  const int dx = dest_x - unit->x;
  const int dy = dest_y - unit->y;
  if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || (dx == 0 && dy == 0)) {
    return false;
  }

  /* Fortification defense uses defender's colony tile (set before combat). */
  units_set_combat_colonies(colonies);
  g_units_indian_combat_vent_done = false;

  /*
   * Village Attack empty tile (FUN_5fef_1b0e): spawn a temporary Brave from
   * dwelling stocks — do not drag nearby map Braves. Cite: 1b0e local_8a arm.
   */
  int village_temp = -1;
  int village_nation = -1;
  /*
   * DOS attack MP model (re-derived 2026-09-10, seventh-wave pass; the old
   * "(step cost + 3) surcharge, ship-slow survives it" model was wrong):
   *   - FUN_5fef_1b0e entry does `spent += 3` for an attack
   *     (viceroy_unpacked.c:100341-100343) but then unconditionally calls
   *     FUN_281f_0934 under the same attack flag (100381-100383), and 0934 →
   *     FUN_1427_155e writes `spent = FUN_1427_065a(unit)` — the FULL max
   *     allotment (viceroy_unpacked.c:8880-8888). The +3 is a dead store;
   *     the real charge is a full exhaust, before the roll, win or lose,
   *     ships included. There is no ship-slow: a Frigate that attacks is
   *     done for the turn exactly like a Soldier.
   *   - FUN_465b charges its per-tile step cost only on NON-attack moves:
   *     `if (!bVar4) spent += local_40` (viceroy_unpacked.c:75639-75640),
   *     and the shore-crossing exhaust sits inside the same `!bVar4` block —
   *     an attacker pays neither (it is already exhausted by 1b0e).
   * The fatigue penalty and its Combat Analysis rows read the ENTRY
   * remaining (uVar15 is captured at 100339-100340, before the mutations),
   * which is why the port keeps charging after the resolve: the strength
   * calc reads live remaining, so charging first would erase the fatigue.
   */
  bool combat_attack_entry = false;
  if (g_units_ff_col1 && unit->nation_id >= 0 && unit->nation_id <= 3) {
    village_nation = units_tribe_nation_at(g_units_ff_col1, dest_x, dest_y);
    if (village_nation >= 4) {
      village_temp = units_spawn_village_temp_defender(
        pool, g_units_ff_col1, dest_x, dest_y, village_nation, unit_id
      );
    }
  }

  const ColonizeEnterReason reason =
    units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, unit->type_index, dest_x, dest_y, unit_id);
  g_units_last_enter_reason = reason;

  if (reason == COLONIZE_ENTER_BOARD) {
    if (village_temp >= 0) {
      units_despawn(pool, village_temp);
      village_temp = -1;
    }
    const ColonizeUnitType* mover_ty = units_type(pool, unit->type_index);
    const int board_need = (mover_ty && mover_ty->space > 0) ? mover_ty->space : 1;
    const int ship_id =
      units_find_boardable_ship(pool, dest_x, dest_y, unit->nation_id, board_need);
    if (ship_id < 0) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED_DOMAIN;
      return false;
    }
    /*
     * DOS-LITERAL FUN_465b_0000 (bugs.md #714): DOS has no boarding branch.
     * Walking onto an ocean tile that carries an own ship is the ordinary
     * step, so it runs LAB_465b_05ca's cost/allow gate exactly like a land
     * step (raw 75638-75651):
     *   cVar7 = spent; spent += local_40;   // local_40 = terr*3, ocean = 3
     *   ... shore-cross exhaust (also inside `if (!bVar4)`) ...
     *   if (local_40 <= iVar13 || cVar7 == '\0' || (04ca(0x83a6), bVar4)) commit;
     *   else roll = range(1, local_40) and commit only if roll <= iVar13.
     * The port used to require nothing but `remaining > 0`, so a unit with a
     * sliver of MP always got aboard for free. The charge is applied ahead of
     * the gate in DOS, so a refused board still burns the step — and the
     * whole allotment when the step crossed the waterline.
     */
    {
      const int board_cost = units_move_cost(pool, unit_id, map, dest_x, dest_y);
      const int board_left = units_remaining_mp(pool, unit_id);
      const bool board_full = board_left >= units_max_mp(pool, unit_id);
      bool board_allow = board_cost <= board_left || board_full;
      if (!board_allow && rng) {
        board_allow = dos_rng_range(rng, 1, board_cost > 0 ? board_cost : 1) <= board_left;
      }
      if (!board_allow) {
        units_mp_charge(pool, unit, board_cost);
        if (units_move_crosses_shore(map, colonies, unit->x, unit->y, dest_x, dest_y)) {
          units_mp_exhaust(pool, unit);
        }
        g_units_last_enter_reason = COLONIZE_ENTER_NO_MP;
        return false;
      }
    }
    if (unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
        unit->orders == UNITS_ORDER_FORTIFIED) {
      unit->orders = UNITS_ORDER_NONE;
    }
    const int ox = unit->x;
    const int oy = unit->y;
    if (!units_board(pool, unit_id, ship_id)) {
      g_units_last_enter_reason = COLONIZE_ENTER_BLOCKED;
      return false;
    }
    /*
     * DOS 465b_05ca: walking aboard from open shore CROSSES the waterline
     * with no colony on either end, so moves_spent is forced to max_mp — the
     * passenger is done for the turn and FUN_4720_015c will not offer it
     * landfall (spent < max_mp test, viceroy_unpacked.c:76014-76017).
     * units_board zeroes moves as a park flag, which cannot tell that
     * spend apart from a fresh in-port load; mark it explicitly.
     * bugs.md #423.
     *
     * Shore-crossing is the ONLY reachable spend here: this function bails out
     * with COLONIZE_ENTER_NO_MP above (`units_remaining_mp(...) <= 0`) before
     * any boarding can happen, so a unit that had already burnt its allotment
     * never reaches the board branch at all. The old "was the pre-boarding
     * zero a park or a real spend?" discriminator was therefore dead and is
     * gone (smell audit 2026-09-10 A4); if the MP gate above is ever relaxed
     * to let a spent unit board, that half of the rule has to come back here.
     */
    {
      ColonizeUnit* boarded = units_get(pool, unit_id);
      if (boarded && units_move_crosses_shore(map, colonies, ox, oy, dest_x, dest_y)) {
        boarded->mp_spent_turn = 1;
        boarded->aboard_moves = 0;
      } else if (boarded && boarded->aboard_moves > 0) {
        /* Boarding from a colony tile is an ordinary step: 465b's ADD charges
         * the water tile's cost to the spent byte the passenger then carries
         * (bugs.md #544). Spent at or past max = no landfall this turn. */
        int cost = map_move_spent_thirds(map, ox, oy, dest_x, dest_y);
        if (cost < 1) {
          cost = 1;
        }
        boarded->aboard_moves = boarded->aboard_moves > cost ? boarded->aboard_moves - cost : 0;
        if (boarded->aboard_moves == 0) {
          boarded->mp_spent_turn = 1;
        }
      }
    }
    units_occupancy_refresh_tile(pool, ox, oy, unit_id);
    units_occupancy_refresh_tile(pool, dest_x, dest_y, -1);
    g_units_last_enter_reason = COLONIZE_ENTER_BOARD;
    return true;
  }

  if (reason == COLONIZE_ENTER_BOUNCE_FOREIGN || reason == COLONIZE_ENTER_BOUNCE_PEACE ||
      reason == COLONIZE_ENTER_BLOCKED_DOMAIN || reason == COLONIZE_ENTER_BLOCKED_EDGE ||
      reason == COLONIZE_ENTER_EDGE_SAIL || /* bugs.md #717: reason 4 aborts */
      reason == COLONIZE_ENTER_BLOCKED_HS_SAIL || reason == COLONIZE_ENTER_VILLAGE_ILLEGAL ||
      reason == COLONIZE_ENTER_LANDFALL || reason == COLONIZE_ENTER_VILLAGE_SHIP ||
      reason == COLONIZE_ENTER_NO_MP || reason == COLONIZE_ENTER_BLOCKED ||
      reason == COLONIZE_ENTER_LAKE_BLOCKED || reason == COLONIZE_ENTER_LANDFIRST) {
    if (village_temp >= 0) {
      units_despawn(pool, village_temp);
    }
    return false;
  }

  if (reason == COLONIZE_ENTER_COMBAT_LAND || reason == COLONIZE_ENTER_COMBAT_NAVAL) {
    const int foe = units_best_defender_at(
      pool, g_units_ff_col1, dest_x, dest_y, unit_id, unit_id
    );
    if (foe < 0) {
      if (village_temp >= 0) {
        units_despawn(pool, village_temp);
        village_temp = -1;
      }
      /*
       * Foreign colony tile with no armed map defender: only non-combat
       * bystanders stand there (the picker's soft tier is off on colony
       * tiles). DOS fights the colony itself — Paul Revere auto-arm or the
       * civilian militia stand-in — and then seizes whoever remains on
       * entry, rather than dueling a bystanding colonist one by one.
       */
      const int nc_cid = colonies ? colonies_id_at(colonies, dest_x, dest_y) : -1;
      const ColonizeColony* nc = nc_cid >= 0 ? colonies_get(colonies, nc_cid) : NULL;
      if (reason != COLONIZE_ENTER_COMBAT_LAND || !nc || !nc->active ||
          units_is_sea(pool, unit_id)) {
        return false;
      }
      if (nc->nation_id == unit->nation_id) {
        /* Own colony squatted by foreign civilians (an old capture left them
         * put): seize them on entry, no militia fight against your own town. */
        units_seize_noncombat_at(pool, unit_id, dest_x, dest_y, g_units_ff_col1);
        combat_attack_entry = true;
        goto combat_entry_resolved;
      }
      if (!units_revere_defend_colony_tile(
            pool, (ColonizeColonyPool*)colonies, unit_id, dest_x, dest_y, rng
          )) {
        return false;
      }
      unit = units_get(pool, unit_id);
      if (!unit) {
        return false;
      }
      units_seize_noncombat_at(pool, unit_id, dest_x, dest_y, g_units_ff_col1);
      combat_attack_entry = true;
      goto combat_entry_resolved;
    }
    /* FUN_465b_0000 raw 75527-75545: Privateer-sighting relation bit,
     * written before the attack resolves (same 465b entry head as the
     * Indian alarm slam below). */
    ai_euro_465b_privateer_sighting(
      (ColonizeCol1Save*)g_units_ff_col1, pool, rng, unit_id, foe
    );
    /*
     * FUN_465b_0000 (viceroy_unpacked.c:75600-75626): a Euro unit moving onto
     * a tile held by an Indian occupant slams tribe alarm by (difficulty+5),
     * doubled on a village tile — where the village record's
     * alarm[nation].attacks byte is also bumped — and sextupled on a capital
     * (the *6 replaces the *2). Written before the combat resolves; the only
     * writer of that attacks byte (move_scoring_20e6_full.md 2026-09-06e §3
     * refuted the "visit counter" reading). Peaceful village entries never
     * reach this arm — FUN_4d56_4528 consumes them first.
     */
    if (reason == COLONIZE_ENTER_COMBAT_LAND && g_units_ff_col1 &&
        unit->nation_id >= 0 && unit->nation_id <= 3) {
      const ColonizeUnit* fu = units_get(pool, foe);
      const int foe_nation = fu ? fu->nation_id : -1;
      if (foe_nation >= 4 && foe_nation <= 11) {
        const int base = (int)g_units_ff_col1->head.difficulty + 5;
        int delta = base;
        if (g_units_ff_col1->tribe) {
          for (uint16_t ti = 0; ti < g_units_ff_col1->head.tribe_count; ++ti) {
            ColonizeCol1Tribe* vt = &g_units_ff_col1->tribe[ti];
            if ((int)vt->x != dest_x || (int)vt->y != dest_y) {
              continue;
            }
            delta = base * 2;
            vt->alarm[unit->nation_id].attacks++;
            if (vt->state.capital) {
              delta = base * 6;
            }
            break;
          }
        }
        ai_diplo_indian_alarm_delta(
          (ColonizeCol1Save*)g_units_ff_col1, foe_nation, unit->nation_id, delta
        );
      }
    }
    /*
     * DOS FUN_5fef_1b0e entry gate (viceroy_unpacked.c:100359-100372): with
     * fewer than 3 thirds remaining an attack is refused outright for every
     * non-interactive nation — natives/crown (`3 < uVar16` return) and any
     * Euro slot whose control byte is set (`0x543f[nation] != 0` return).
     * Only the interactive human reaches the @HALF tired-attack CHOICE
     * (game_loop asks it before calling here, so a confirmed human attack
     * arrives with the same low MP and proceeds). The 465b alarm slam above
     * already ran — DOS orders it the same way (75600-75626 precede the
     * 0a14 → 1b0e call at 75692). Gated on col1 being wired so bare unit
     * fixtures keep their attacks.
     */
    if (g_units_ff_col1 && units_remaining_mp(pool, unit_id) < UNITS_MP_PER_TILE) {
      const bool ai_controlled =
        unit->nation_id > 3 ||
        (unit->nation_id >= 0 && unit->nation_id <= 3 &&
         g_units_ff_col1->player[unit->nation_id].control != 0);
      if (ai_controlled) {
        if (village_temp >= 0) {
          units_despawn(pool, village_temp);
          village_temp = -1;
        }
        g_units_last_enter_reason = COLONIZE_ENTER_NO_MP;
        return false;
      }
    }
    bool won = false;
    /* DOS bVar28 is set by BOTH auto-spawn arms of 1b0e — the colony militia
     * (units_revere_defend_colony_tile) and this empty-dwelling Brave. The
     * beginner-shield peel that reads it also demands a colony and a European
     * defender, so a village fight can never trip it; the latch is raised here
     * anyway because that is what the byte means. */
    const bool phantom_defender = (village_temp >= 0 && foe == village_temp);
    if (phantom_defender) {
      combat_set_auto_defender(true);
    }
    if (reason == COLONIZE_ENTER_COMBAT_NAVAL) {
      won = units_resolve_naval_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .col1=(ColonizeCol1Save*)(g_units_ff_col1), .col1_ok=((g_units_ff_col1) != NULL), .rng=(ColonizeDosRng*)(rng)}, unit_id, foe);
    } else {
      won = units_resolve_land_combat_ff_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .col1=(ColonizeCol1Save*)(g_units_ff_col1), .col1_ok=((g_units_ff_col1) != NULL), .rng=(ColonizeDosRng*)(rng)}, unit_id, foe);
    }
    if (phantom_defender) {
      combat_set_auto_defender(false);
    }
    if (won && units_combat_is_visible(pool, unit_id, foe)) {
      units_play_event_sound(village_temp >= 0 ? 0x4b : UNITS_SFX_COMBAT_WON);
      /* FUN_5fef_1b0e 5fef:2546 + 28b0: an Indian attacker (nation ≥ 4)
       * beating a colony's defender (colony at the defender tile, pop > 1)
       * sets local_6 and the tail then pushes 0x45 (COLDIG 17 glancing
       * shot); the 0x44 arm needs a ship attacker, unreachable here. */
      if (unit->nation_id >= 4 && colonies) {
        const int cid = colonies_id_at(colonies, dest_x, dest_y);
        const ColonizeColony* cc = cid >= 0 ? colonies_get(colonies, cid) : NULL;
        if (cc && cc->population > 1) {
          units_play_event_sound(0x45);
        }
      }
    }
    if (village_temp >= 0 && foe == village_temp) {
      ColonizeCol1Save* mut = (ColonizeCol1Save*)g_units_ff_col1;
      units_finish_village_temp_defender(
        pool,
        mut,
        g_units_fallout_map ? g_units_fallout_map : (ColonizeWorldMap*)map,
        village_temp,
        won ? 1 : 0,
        unit->nation_id,
        dest_x,
        dest_y,
        rng
      );
      village_temp = -1;
    } else if (village_temp >= 0) {
      units_despawn(pool, village_temp);
      village_temp = -1;
    }
    /*
     * DOS: an attacker is FULLY exhausted for the turn, win or lose, ships
     * included — FUN_281f_0934 (spent = max allotment via FUN_1427_155e) runs
     * at 1b0e entry under the attack flag (viceroy_unpacked.c:100381-100383),
     * and 465b's step cost is skipped on attacks (75639-75640). See the model
     * comment above `combat_attack_entry`. Every outcome of the same attack
     * pays this same full exhaust: the loss return just below, the ordinary
     * land-win stay-put branch, the native raid-stay-put branch, and the
     * walk-in/advance path via `combat_attack_entry`.
     */
    if (!won) {
      ColonizeUnit* atk_mp = units_get(pool, unit_id);
      if (atk_mp && atk_mp->active) {
        units_mp_exhaust(pool, atk_mp);
      }
      return false;
    }
    unit = units_get(pool, unit_id);
    if (!unit) {
      return false;
    }
    /*
     * Native village raid: fight from the adjacent tile and stay there (DOS
     * FUN_4d56_4528 contact). The attack exhausts the full allotment (1b0e's
     * FUN_281f_0934, viceroy_unpacked.c:100381-100383 — the same charge as
     * every other outcome of the same attack; 465b's step cost is attack-
     * skipped, 75639-75640). Do not enter the dwelling tile.
     */
    if (village_nation >= 4 && unit->nation_id >= 0 && unit->nation_id <= 3) {
      /*
       * No overspend gamble here (smell audit 2026-09-09 #5). The DOS gate is
       * FUN_465b ~75643: `(cost <= left) || (spent == 0) || (04ca(seed),
       * bVar4)` — the third clause IS the attack flag, so an attack is never
       * denied by the MP roll; the sibling site below (the shared step-cost
       * gate) already spells that out and cites it. Rolling it here was doubly
       * wrong: the raid had already resolved and drained the dwelling, so the
       * denial refused an entry whose effects were permanent, and the draw
       * itself shifted the RNG stream for every later native raid.
       */
      units_mp_exhaust(pool, unit);
      if (unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
          unit->orders == UNITS_ORDER_FORTIFIED) {
        unit->orders = UNITS_ORDER_NONE;
      }
      g_units_last_enter_reason = COLONIZE_ENTER_OK;
      return true;
    }
    /* After win, dest must be clear of foreigners for enter — of the mover's
     * own domain only (units_domain_blocker_at). DOS's walk-in is gated on
     * FUN_5fef_0000 having run out of defenders, and that picker never sees
     * the other domain, so a hull in a fallen port does not bar the town. */
    if (units_domain_blocker_at(pool, dest_x, dest_y, unit_id, unit->nation_id) >= 0) {
      return false;
    }
    /*
     * bugs.md #243: a land attacker does NOT advance into the vacated tile.
     * Only ships (naval combat) and the attack that captures a colony enter;
     * everywhere else the winner stays put — the full attack exhaust is
     * spent either way (DOS 1b0e FUN_281f_0934, same charge as the loss
     * branch).
     */
    if (reason == COLONIZE_ENTER_COMBAT_LAND &&
        (!colonies || colonies_id_at(colonies, dest_x, dest_y) < 0)) {
      units_mp_exhaust(pool, unit);
      if (unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
          unit->orders == UNITS_ORDER_FORTIFIED) {
        unit->orders = UNITS_ORDER_NONE;
      }
      g_units_last_enter_reason = COLONIZE_ENTER_OK;
      return true;
    }
    /* Advancing attacker (naval win, colony capture): the full exhaust is
     * applied at the shared charge site below via `combat_attack_entry` —
     * DOS charged it at 1b0e entry and 465b then skips both the step cost
     * and the shore exhaust on the attack flag (75639-75640). */
    combat_attack_entry = true;
  } else {
    if (village_temp >= 0) {
      units_despawn(pool, village_temp);
      village_temp = -1;
    }
    if (colonies) {
      /* Paul Revere: empty foreign colony tile → auto-arm from muskets + fight.
       * Cite: PEDIA / docs/fandom_col1994.md Paul Revere. */
      if (!units_revere_defend_colony_tile(
            pool, (ColonizeColonyPool*)colonies, unit_id, dest_x, dest_y, rng
          )) {
        return false;
      }
      unit = units_get(pool, unit_id);
      if (!unit) {
        return false;
      }
    }
  }

combat_entry_resolved:
  /* Dock / OK: domain enterability already confirmed by probe. */
  if (reason != COLONIZE_ENTER_OK && reason != COLONIZE_ENTER_DOCK &&
      reason != COLONIZE_ENTER_COMBAT_LAND && reason != COLONIZE_ENTER_COMBAT_NAVAL) {
    return false;
  }
  if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, unit->type_index, dest_x, dest_y, unit_id)) {
    /* Combat cleared foe — re-probe should be OK/DOCK now. */
    const ColonizeEnterReason after =
      units_enter_probe_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, unit->type_index, dest_x, dest_y, unit_id);
    g_units_last_enter_reason = after;
    if (after != COLONIZE_ENTER_OK && after != COLONIZE_ENTER_DOCK) {
      return false;
    }
  }

  const int cost = units_move_cost(pool, unit_id, map, dest_x, dest_y);
  const int remaining = units_remaining_mp(pool, unit_id);
  const int max_mp = units_max_mp(pool, unit_id);
  const bool full_mp = remaining >= max_mp;

  bool allow = false;
  if (reason == COLONIZE_ENTER_COMBAT_LAND || reason == COLONIZE_ENTER_COMBAT_NAVAL) {
    /*
     * DOS FUN_465b gate (~75643): `(cost <= left) || (spent == 0) ||
     * (04ca(seed), bVar4)` — the third clause is the attack flag itself, so
     * an attack is never denied by the MP overspend roll (moves > 0 was
     * already required to act). Rolling here after the fight had already
     * resolved could capture a colony's defender yet refuse the entry that
     * seizes the colony (bugs: scout beat a Spanish colony's colonist,
     * captured them, and stayed outside).
     */
    allow = true;
  } else if (cost <= remaining || full_mp) {
    allow = true;
  } else if (rng) {
    /* DOS FUN_465b raw 75649: `FUN_281f_04ca(DS:0x83a6)` reseeds the LCG
     * from the timer word before the `else` roll at raw 75825-75827
     * (`FUN_281f_04d4(1, cost) <= remaining`). Same chokepoint the Brave
     * path uses (ai_brave.c, ai_turn_seed). bugs.md #842. */
    if (w->rng_reseed_set) {
      dos_rng_seed(rng, w->rng_reseed);
    }
    const int roll = dos_rng_range(rng, 1, cost > 0 ? cost : 1);
    allow = roll <= remaining;
  } else {
    return false;
  }

  /*
   * DOS charges MP here in two disjoint regimes (465b_05ca, `if (!bVar4)`
   * with bVar4 = the attack flag, viceroy_unpacked.c:75639-75648):
   *   - attack: the full-allotment exhaust 1b0e already applied via
   *     FUN_281f_0934 — no step cost, no shore exhaust (both sit inside the
   *     `!bVar4` block);
   *   - plain move: the full terrain cost is added to spent before the
   *     allow/deny gate — including failed partial-overspend rolls — and
   *     crossing the shoreline outside a colony spends the lot.
   */
  if (combat_attack_entry) {
    units_mp_exhaust(pool, unit);
  } else {
    units_mp_charge(pool, unit, cost);
    if (units_move_crosses_shore(map, colonies, unit->x, unit->y, dest_x, dest_y)) {
      units_mp_exhaust(pool, unit);
    }
  }
  if (!allow) {
    return false;
  }

  /* Moving cancels sentry / fortify; Go-To cleared only on arrival elsewhere. */
  if (unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
      unit->orders == UNITS_ORDER_FORTIFIED) {
    unit->orders = UNITS_ORDER_NONE;
  }

  const int ox = unit->x;
  const int oy = unit->y;
  unit->x = dest_x;
  unit->y = dest_y;
  units_tile_stack_arrive(pool, unit_id);
  /* Keep passengers' coordinates mirrored to the ship for debugging / unload. */
  for (int i = 0; i < unit->cargo_count; ++i) {
    ColonizeUnit* pax = units_get(pool, unit->cargo_ids[i]);
    if (pax) {
      pax->x = dest_x;
      pax->y = dest_y;
    }
  }
  units_occupancy_refresh_tile(pool, ox, oy, unit_id);
  units_occupancy_refresh_tile(pool, dest_x, dest_y, -1);
  units_vis_mask_after_move(pool, map, unit_id, dest_x, dest_y);

  /* Departure pickup: sentried units on the tile AND passengers of other own
   * ships still there, first come first served (DOS ship-switch quirk). */
  if (units_is_sea(pool, unit_id)) {
    (void)units_ship_departure_pickup(pool, unit_id, ox, oy);
  }

  /*
   * DOS-LITERAL FUN_465b_0bd1 raw 75721: `FUN_281f_088a(unit) != 0 ||
   * type == 0x0c` gates the whole colony-enter arm (MP exhaust, the
   * col1_counter16 reset and the destination-stack sentry wake below).
   * FUN_281f_088a (== FUN_1427_1284, raw 8684-8702) walks the mover's tile
   * stack — evaluated before FUN_281f_0934 relinks it into the destination
   * stack, i.e. it still reads the ORIGIN tile's stack at this point even
   * though x/y already hold the destination — and returns true if ANY unit
   * there (including the mover itself) is a hull (type 0xd..0x12). bugs.md
   * #791: was gated on `units_is_transport(mover)` (self-only, cargo-gated),
   * so a plain land unit leaving a tile that also held a ship kept its MP
   * when it walked into a colony, and a wagon/ship with an empty hold was
   * wrongly exempted.
   */
  bool hull_in_origin_stack = units_is_sea(pool, unit_id);
  if (!hull_in_origin_stack) {
    int slot = 0;
    for (ColonizeUnit* other = units_next_on_tile(pool, ox, oy, &slot); other != NULL;
         other = units_next_on_tile(pool, ox, oy, &slot)) {
      if (units_is_sea(pool, other->id)) {
        hull_in_origin_stack = true;
        break;
      }
    }
  }
  const ColonizeUnitType* mover_type = units_type(pool, unit->type_index);
  const bool mover_is_wagon = mover_type && units_type_is_wagon(mover_type);
  const bool colony_enter_gate = hull_in_origin_stack || mover_is_wagon;

  if (colonies && colonies_id_at(colonies, dest_x, dest_y) >= 0) {
    if (colony_enter_gate) {
      units_mp_exhaust(pool, unit);
      /* DOS raw 75731-75740: bugs.md #792 — col1_counter16 = 0 on arrival,
       * then wake (clear Sentry orders on) every unit on the destination
       * stack; docking a wagon/ship wakes the colony's sentried units. */
      unit->col1_counter16 = 0;
      int dslot = 0;
      for (ColonizeUnit* stacked = units_next_on_tile(pool, dest_x, dest_y, &dslot);
           stacked != NULL;
           stacked = units_next_on_tile(pool, dest_x, dest_y, &dslot)) {
        if (stacked->orders == UNITS_ORDER_SENTRY) {
          stacked->orders = UNITS_ORDER_NONE;
        }
      }
    }
    /*
     * bugs.md: docking puts everyone ashore. A unit inside a colony is in the
     * colony, not in a hold — whether it sails again is decided when a ship
     * next leaves, and the sentried units on the tile board it then (the
     * auto-board above). They land with no orders and their allotment intact,
     * so an arrival can still walk on the turn it lands.
     *
     * Deliberately NOT gated on units_is_transport any more: that predicate
     * exists for the wagon/ship MP-forfeit rule above and answers "does this
     * hull have goods holds", which is a different question from "is anyone
     * riding in it". A ship carrying passengers puts them ashore on docking
     * whatever its hold count says (bugs.md: "does not always happen").
     */
    if (units_is_sea(pool, unit_id) && unit->cargo_count > 0) {
      /* Own colonies only: a De Witt trade dock at a *foreign* colony
       * (COLONIZE_ENTER_DOCK via the foreign-colony-trade branch) must not
       * put passengers ashore in someone else's town. */
      const ColonizeColony* dock_col =
        colonies_get(colonies, colonies_id_at(colonies, dest_x, dest_y));
      if (dock_col && dock_col->active && dock_col->nation_id == unit->nation_id) {
        (void)units_disembark_all(pool, unit_id, dest_x, dest_y);
        units_occupancy_refresh_tile(pool, dest_x, dest_y, -1);
      }
    }
  }

  if (colonies) {
    units_try_capture_foreign_colony(pool, (ColonizeColonyPool*)colonies, unit_id);
  }

  g_units_last_enter_reason =
    (reason == COLONIZE_ENTER_DOCK) ? COLONIZE_ENTER_DOCK : COLONIZE_ENTER_OK;
  if (diag_info_enabled()) {
    char who[96];
    units_log_ident(pool, unit_id, who, sizeof(who));
    diag_info("MOVE %s: (%d,%d) -> (%d,%d)", who, ox, oy, unit->x, unit->y);
  }
  /* FUN_5bfb_3180 naval half: adjacent foreign warship / Fort / Fortress
   * may eat the mover's remaining MP (see units_ship_slow_scan). */
  units_ship_slow_scan_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map), .rng=(ColonizeDosRng*)(rng)}, unit_id);
  /* FUN_5bfb_3180 sentry half: a foreign unit now adjacent wakes that stack's
   * Sentry units (raw 98628-98646). Runs for every mover, land or sea. */
  units_sentry_wake_scan(pool, map, colonies, unit_id);
  unit = units_get(pool, unit_id);
  if (g_units_move_watch && units_is_on_map(unit)) {
    g_units_move_watch(
      g_units_move_watch_user,
      pool,
      map,
      colonies,
      unit_id,
      ox,
      oy,
      unit->x,
      unit->y
    );
  }
  return true;
}


static int units_sign_i(int v) {
  if (v < 0) {
    return -1;
  }
  if (v > 0) {
    return 1;
  }
  return 0;
}

static int units_chebyshev(int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0);
  const int dy = abs(y1 - y0);
  return dx > dy ? dx : dy;
}

/* Octile-style distance (FUN_124c_0040): max + min/2. */
static int units_octile(int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0);
  const int dy = abs(y1 - y0);
  const int mx = dx > dy ? dx : dy;
  const int mn = dx < dy ? dx : dy;
  return mx + mn / 2;
}

void units_clear_orders(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u) {
    return;
  }
  u->orders = UNITS_ORDER_NONE;
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
  u->follow_unit_id = -1;
}

/*
 * DOS FUN_465b_0000 LAB_465b_05ca "ocean force-to-max" (annotated
 * ai/move_spent.c section 4): after the step cost is added, a move that
 * CROSSES the water/land boundary with no European settlement on either tile
 * sets moves_spent to the unit's maximum — the whole allotment is gone.
 * That is the rule behind bugs.md's "dragoons should have their entire
 * movement spent from stepping off a ship onto land": a landfall onto bare
 * coast is exactly this case, and so is boarding a ship from open shore. A
 * colony on either end (a dock) exempts the step, which is why loading and
 * unloading in port stays cheap. Indian villages do not exempt it — DOS's
 * FUN_281f_0696 clamps any owner above 3 to −1.
 */
bool units_move_crosses_shore(
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int from_x,
  int from_y,
  int to_x,
  int to_y
) {
  if (!map) {
    return false;
  }
  if (map_tile_is_water(map, from_x, from_y) == map_tile_is_water(map, to_x, to_y)) {
    return false;
  }
  if (colonies && (colonies_id_at(colonies, from_x, from_y) >= 0 ||
                   colonies_id_at(colonies, to_x, to_y) >= 0)) {
    return false;
  }
  return true;
}

bool units_orders_skip_turn(const ColonizeUnit* unit) {
  if (!unit || !unit->active) {
    return false;
  }
  /*
   * bugs.md: FORTIFY (5) belongs here too. It used to be left out on the
   * argument that digging in already zeroes the allotment — but the COL1
   * export writes spent 0 for an exhausted land unit (DOS clears the spent
   * bytes at day end), so a unit that fortified this turn reloads with
   * orders 5 AND a full allotment and walked straight back into the control
   * queue. DOS's cycle reads the order byte, not the MP.
   */
  return unit->orders == UNITS_ORDER_SENTRY || unit->orders == UNITS_ORDER_FORTIFY ||
         unit->orders == UNITS_ORDER_FORTIFIED || unit->orders == UNITS_ORDER_CLEAR_PLOW ||
         unit->orders == UNITS_ORDER_BUILD_ROAD;
}

/* @ORDERS byte → debug-log name. */
const char* units_order_name(int orders) {
  switch (orders) {
    case UNITS_ORDER_NONE:
      return "none";
    case UNITS_ORDER_SENTRY:
      return "sentry";
    case UNITS_ORDER_TRADE_ROUTE:
      return "trade route";
    case UNITS_ORDER_GOTO:
      return "goto";
    case UNITS_ORDER_FORTIFY:
      return "fortify";
    case UNITS_ORDER_FORTIFIED:
      return "fortified";
    case UNITS_ORDER_BUILD_COLONY:
      return "build colony";
    case UNITS_ORDER_CLEAR_PLOW:
      return "clear/plow";
    case UNITS_ORDER_BUILD_ROAD:
      return "build road";
    case UNITS_ORDER_AI_SAIL:
      return "ai sail";
    case UNITS_ORDER_AI_MOVE:
      return "ai move";
    case UNITS_ORDER_FOLLOW:
      return "follow";
    default:
      break;
  }
  return "?";
}

/* "Veteran Dragoon id=12 nation=0 at (34,21) mp=3" for order/combat lines. */
void units_log_ident(
  const ColonizeUnitPool* pool,
  int unit_id,
  char* out,
  size_t out_size
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u) {
    snprintf(out, out_size, "unit %d (gone)", unit_id);
    return;
  }
  snprintf(
    out, out_size, "%s id=%d nation=%d at (%d,%d) mp=%d",
    units_display_name(pool, u), unit_id, u->nation_id, u->x, u->y, u->moves
  );
}

bool units_set_orders(ColonizeUnitPool* pool, int unit_id, int orders) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  if (orders == UNITS_ORDER_FORTIFY || orders == UNITS_ORDER_FORTIFIED) {
    /*
     * Ships fortify too. DOS's Fortify command (FUN_2b5a_1112) writes
     * orders 5 unconditionally; its only type test is `type < 0x0d ||
     * type > 0x12` — the six ship types — and that gate guards the
     * *adjacent-foreign-power war prompt* the command runs first, not the
     * order itself. There is no water or harbour condition anywhere in it
     * (bugs.md). A unit still has to be on the map, not riding in a hold.
     */
    if (!units_is_on_map(u)) {
      return false;
    }
  }
  if (orders == UNITS_ORDER_SENTRY) {
    /* Map sentry or already aboard (Europe/cargo path uses raw orders=1). */
    if (!units_is_on_map(u) && u->aboard_ship_id < 0) {
      return false;
    }
  }
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
  u->follow_unit_id = -1;
  const int prev = u->orders;
  u->orders = orders;
  if (orders == UNITS_ORDER_SENTRY || orders == UNITS_ORDER_FORTIFY ||
      orders == UNITS_ORDER_FORTIFIED) {
    /* Park = "no moves left this turn": spent-aware, since moves holds
     * REMAINING for Euros and SPENT for natives (audit #6). */
    units_mp_exhaust(pool, u);
    /* Fresh park: no overnight yet, so a same-turn wake refunds nothing. */
    u->park_nights = 0;
  }
  if (diag_info_enabled() && prev != orders) {
    char who[96];
    units_log_ident(pool, unit_id, who, sizeof(who));
    diag_info("ORDER %s: %s (was %s)", who, units_order_name(orders), units_order_name(prev));
  }
  return true;
}

bool units_order_fortify(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  /*
   * bugs.md #695: no "already fortified" early-out. FUN_2b5a_1112's tail
   * (viceroy_unpacked.c raw 42418-42421) runs unconditionally — it writes
   * order 5, zeroes +0x315a and calls FUN_281f_0934 (full MP exhaust) even
   * when the unit was already Fortified. Re-issuing Fortify on a dug-in unit
   * therefore costs it the turn in DOS; the port used to make it a no-op.
   */
  const bool ok = units_set_orders(pool, unit_id, UNITS_ORDER_FORTIFY);
  if (ok) {
    units_play_event_sound(UNITS_SFX_ORDER_FORTIFY);
    /* FUN_2b5a_1112's own tail also zeroes the unit's +0x315a counter. */
    u->col1_counter16 = 0;
  }
  return ok;
}

bool units_order_sentry(ColonizeUnitPool* pool, int unit_id) {
  const bool ok = units_set_orders(pool, unit_id, UNITS_ORDER_SENTRY);
  if (ok) {
    units_play_event_sound(UNITS_SFX_ORDER_FORTIFY);
  }
  return ok;
}

bool units_order_trade_route(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u)) {
    return false;
  }
  /* Wagons and ships run trade routes; refuse pure foot units without holds. */
  if (!units_is_transport(pool, unit_id)) {
    return false;
  }
  u->goto_x = UNITS_GOTO_NONE;
  u->goto_y = UNITS_GOTO_NONE;
  u->follow_unit_id = -1;
  u->orders = UNITS_ORDER_TRADE_ROUTE;
  /* Same park-as-spent rule units_set_orders uses (:7782): spent-aware, since
   * moves is REMAINING for Euros and SPENT for natives (audit A9). */
  units_mp_exhaust(pool, u);
  return true;
}
/* ===================== Pillage/disband/wake, goto/follow flood-fill & greedy pathfinding (units_dump_cargo_overboard .. units_greedy_next_step) ===================== */


int units_dump_cargo_overboard(
  ColonizeUnitPool* pool,
  int unit_id,
  int* out_cargo_type,
  int* out_amount
) {
  if (!units_is_transport(pool, unit_id)) {
    return 0;
  }
  const int hold = units_first_goods_hold(pool, unit_id);
  if (hold < 0) {
    return 0;
  }
  return units_unload_goods_hold(pool, unit_id, hold, out_cargo_type, out_amount);
}

bool units_pillage_w(
  const ColonizeWorld* w,
  int unit_id,
  char* err,
  size_t err_size
) {
  ColonizeUnitPool* pool = w->units;
  ColonizeWorldMap* map = w->map;
  ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !units_is_on_map(u) || !map) {
    if (err && err_size) {
      snprintf(err, err_size, "Select a unit");
    }
    return false;
  }
  if (units_is_sea(pool, unit_id)) {
    if (err && err_size) {
      snprintf(err, err_size, "Cannot pillage at sea");
    }
    return false;
  }
  const ColonizeUnitType* type = units_type(pool, u->type_index);
  if (!type || type->attack <= 0) {
    if (err && err_size) {
      snprintf(err, err_size, "Need a military unit");
    }
    return false;
  }
  /* Spent-aware read (audit A9): moves is REMAINING for Euros but the
   * DOS SPENT byte for natives, so ask the accessor, not the field. */
  if (units_remaining_mp(pool, unit_id) <= 0) {
    if (err && err_size) {
      snprintf(err, err_size, "No moves left");
    }
    return false;
  }

  const int cid = colonies ? colonies_id_at(colonies, u->x, u->y) : -1;
  ColonizeColony* col = (cid >= 0) ? colonies_get_mut(colonies, cid) : NULL;
  if (col && col->nation_id != u->nation_id && col->nation_id >= 0 && col->nation_id < 4) {
    /* Loot richest non-food warehouse cargo (thin ORDERS Pillage). */
    int best = -1;
    int best_amt = 0;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      if (c == COLONIZE_CARGO_FOOD) {
        continue;
      }
      if (col->stock[c] > best_amt) {
        best_amt = col->stock[c];
        best = c;
      }
    }
    if (best < 0 || best_amt <= 0) {
      if (err && err_size) {
        snprintf(err, err_size, "Nothing to pillage");
      }
      return false;
    }
    const int take = best_amt < 100 ? best_amt : 100;
    col->stock[best] -= take;
    units_mp_exhaust(pool, u); /* pillaging ends the turn (audit A9) */
    if (err && err_size) {
      snprintf(err, err_size, "Pillaged %d cargo", take);
    }
    return true;
  }

  /*
   * bugs.md #631: there is no improvement-destroying pillage in DOS. Every
   * FUN_281f_068c(x,y,bit,on) call in both exports was enumerated: only
   * `0x10,1` (land purchased, raw 42499 / 42592 / 76688), `4,1` (raw 57001)
   * and `1,0` / `2,0` (occupancy, raw 58157 / 101569-101570). Nothing ever
   * clears the plow bit 0x40 or the road bit 0x08 — not raids (4cc6), not
   * 5fef. The port's "clear plow / road on the tile" arm was invented; it is
   * gone. ORDERS id 0x317 Pillage is hidden unconditionally by
   * FUN_2b5a_0b34 anyway, so only the colony-loot arm above is reachable.
   */
  if (err && err_size) {
    snprintf(err, err_size, "Nothing to pillage");
  }
  return false;
}


bool units_disband(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  /* Unboard passengers first if disbanding a ship — despawn handles cargo in
   * units_despawn paths; keep simple: refuse sea with cargo for T0. */
  if (units_is_sea(pool, unit_id) && u->cargo_count > 0) {
    return false;
  }
  if (u->aboard_ship_id >= 0) {
    /* Leave ship hold then despawn. */
    ColonizeUnit* ship = units_get(pool, u->aboard_ship_id);
    if (ship) {
      for (int i = 0; i < ship->cargo_count; ++i) {
        if (ship->cargo_ids[i] == unit_id) {
          for (int j = i; j < ship->cargo_count - 1; ++j) {
            ship->cargo_ids[j] = ship->cargo_ids[j + 1];
          }
          ship->cargo_count--;
          break;
        }
      }
    }
    u->aboard_ship_id = -1;
  }
  if (diag_info_enabled()) {
    char who[96];
    units_log_ident(pool, unit_id, who, sizeof(who));
    diag_info("ORDER %s: disband", who);
  }
  return units_despawn(pool, unit_id);
}

bool units_wake(ColonizeUnitPool* pool, int unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  const int prev = u->orders;
  units_clear_orders(pool, unit_id);
  /*
   * Restore the allotment only where the zero was a PARK, not a spend:
   * boarding parks a passenger at moves 0, and Sentry/Fortify(ed)
   * zero it as their "skip this unit" flag while DOS's own spent byte
   * stays 0. A unit with no standing order (None, or mid-Go-To) genuinely
   * spent its moves — Activate Unit picks who is controlled, it does not
   * hand out free movement (bugs.md).
   */
  /*
   * bugs.md (player-clarified): the order byte itself separates the cases.
   * FORTIFY (5) was given THIS turn — those moves are spent, no refund.
   * SENTRY/FORTIFIED use park_nights (bumped by the turn refresh):
   * overnight parks wake with full moves, same-turn ones keep what they
   * had. FORTIFIED with park_nights 0 is the overnight-promotion turn —
   * DOS FUN_479b_0b6c spends the allotment on promotion (viceroy
   * 77146-77153), so no refund that turn either. Hold passengers restore
   * as before. DOS's activate handler writes the order byte only (viceroy
   * 42717/42781), so col1_counter16 (the shared +0x16 repair-timer /
   * treasure-clock / route-stop counter) is left alone here.
   */
  /* mp_spent_turn: the aboard zero is a real DOS spend — walked aboard from
   * open shore, the one case units_try_move can mark (a unit that is already
   * out of MP never gets to board, audit A4) — so waking must not refund it
   * (bugs.md #423; same discriminator the landfall pick uses). */
  /* Aboard, the zero stands for the carried allotment (bugs.md #544). */
  if (u->aboard_ship_id >= 0 && u->moves <= 0 && u->aboard_moves >= 0) {
    u->moves = u->aboard_moves;
  } else {
    const bool parked =
      (u->aboard_ship_id >= 0 && !u->mp_spent_turn) ||
      ((prev == UNITS_ORDER_FORTIFIED || prev == UNITS_ORDER_SENTRY ||
        /* bugs.md #528: FUN_521d_0a60's turn-top clear (ai_euro.c) may already
         * have zeroed a landfall-waiting unit's `orders` back to NONE before
         * this wake — this port-only flag (units.h) is the same "was parked
         * ashore, not really idle" signal, cross-checked against the
         * DOS-real park_nights counter below so only an overnight park (not
         * a same-turn 0a60 clear) refunds the allotment. */
        (prev == UNITS_ORDER_NONE && u->ai_landfall_wait)) &&
       u->park_nights > 0);
    if (parked && units_type(pool, u->type_index)) {
      units_mp_restore(pool, u);
    }
  }
  if (prev == UNITS_ORDER_SENTRY || prev == UNITS_ORDER_FORTIFY ||
      prev == UNITS_ORDER_FORTIFIED) {
    u->park_nights = 0;
  }
  if (prev == UNITS_ORDER_SENTRY || prev == UNITS_ORDER_NONE) {
    u->ai_landfall_wait = false; /* woken: no longer just parked ashore (bugs.md #528) */
  }
  if (diag_info_enabled() && prev != UNITS_ORDER_NONE) {
    char who[96];
    units_log_ident(pool, unit_id, who, sizeof(who));
    diag_info("ORDER %s: wake (was %s)", who, units_order_name(prev));
  }
  return prev == UNITS_ORDER_SENTRY || prev == UNITS_ORDER_FORTIFY ||
         prev == UNITS_ORDER_FORTIFIED || prev == UNITS_ORDER_GOTO;
}

bool units_set_goto_w(
  const ColonizeWorld* w,
  int unit_id,
  int dest_x,
  int dest_y
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* u = units_get(pool, unit_id);
  /* A passenger aboard a ship never carries a lasting Go To: its only legal
   * step is one tile ashore, and a one-tile Go To is an ordinary move (the
   * UI issues it through the same path an arrow key takes), so it is a
   * landfall, not an order. */
  if (!u || !u->active || !map || !units_is_on_map(u)) {
    return false;
  }
  if (dest_x < 0 || dest_y < 0 || dest_x >= (int)map->width || dest_y >= (int)map->height) {
    return false;
  }
  if (u->x == dest_x && u->y == dest_y) {
    units_clear_orders(pool, unit_id);
    return true;
  }
  /*
   * bugs.md: a Go To onto a fogged square is always legal — the player does
   * not know what is under the fog, so the destination check must not peek.
   * Only a tile the mover's nation has actually seen gets the enterability
   * test; an unseen one takes the order and the unit finds out en route
   * (the per-step move logic stops or bounces it once the truth is in
   * sight, same as DOS).
   */
  if (map_tile_seen_by(map, dest_x, dest_y, u->nation_id) &&
      !units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, u->type_index, dest_x, dest_y, unit_id)) {
    /*
     * bugs.md: a Go To aimed at an Indian village is a legal order — the
     * unit travels there and the village-enter handling fires on arrival
     * (bugs.md #418: the final step is dispatched as a real move into the
     * village, see units_goto_dest_is_village_entry).
     * Everything else that fails the enterability check still refuses.
     */
    const bool village_dest = map_tile_has_city(map, dest_x, dest_y) &&
      (!colonies || colonies_id_at(colonies, dest_x, dest_y) < 0);
    if (!village_dest) {
      return false;
    }
  }
  u->follow_unit_id = -1;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = dest_x;
  u->goto_y = dest_y;
  if (diag_info_enabled()) {
    char who[96];
    units_log_ident(pool, unit_id, who, sizeof(who));
    diag_info("ORDER %s: goto (%d,%d)", who, dest_x, dest_y);
  }
  return true;
}


/*
 * bugs.md #418: is this unit's NEXT goto step the final one, onto an Indian
 * settlement it was explicitly ordered to? DOS runs every goto step through
 * FUN_465b_0000, so that step is a move command into the village and must
 * raise FUN_4d56_4528's entry flow (woodcut 7 + the @ACTIONS menu / unmet
 * warn) exactly as an arrow-key step does. The raw pacer
 * (units_advance_goto_one_step) has no popup channel, so game_loop's
 * activation pacer asks this and hands the step to game_try_unit_move.
 *
 * True only for a village TILE (HAS_CITY with no Euro colony on it) that is
 * the ordered destination and is adjacent — never for a village the path
 * merely brushes past.
 */
bool units_goto_dest_is_village_entry_w(
  const ColonizeWorld* w,
  int unit_id
) {
  const ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !u->active || !map || !units_is_on_map(u)) {
    return false;
  }
  if (u->orders != UNITS_ORDER_GOTO) {
    return false;
  }
  const int gx = u->goto_x;
  const int gy = u->goto_y;
  if (gx >= UNITS_GOTO_NONE || gy >= UNITS_GOTO_NONE) {
    return false;
  }
  if (u->x == gx && u->y == gy) {
    return false; /* already there — nothing left to enter */
  }
  if (units_chebyshev(u->x, u->y, gx, gy) > 1) {
    return false; /* still en route */
  }
  if (!map_tile_has_city(map, gx, gy)) {
    return false;
  }
  return !colonies || colonies_id_at(colonies, gx, gy) < 0;
}


bool units_follow_unit(ColonizeUnitPool* pool, int unit_id, int target_unit_id) {
  ColonizeUnit* u = units_get(pool, unit_id);
  const ColonizeUnit* t = units_get_const(pool, target_unit_id);
  if (!u || !t || !u->active || !t->active) {
    return false;
  }
  if (unit_id == target_unit_id) {
    return false;
  }
  if (!units_is_on_map(u) || !units_is_on_map(t)) {
    return false;
  }
  /* Sea follows sea; land follows land — mixed escort is not a map path. */
  if (units_is_sea(pool, unit_id) != units_is_sea(pool, target_unit_id)) {
    return false;
  }
  u->orders = UNITS_ORDER_FOLLOW;
  u->follow_unit_id = target_unit_id;
  u->goto_x = t->x;
  u->goto_y = t->y;
  if (diag_info_enabled()) {
    char who[96];
    units_log_ident(pool, unit_id, who, sizeof(who));
    diag_info("ORDER %s: follow unit %d at (%d,%d)", who, target_unit_id, t->x, t->y);
  }
  return true;
}

bool units_advance_follow_one_step_w(
  const ColonizeWorld* w,
  int unit_id
) {
  ColonizeUnitPool* pool = w->units;

  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || u->orders != UNITS_ORDER_FOLLOW) {
    return false;
  }
  const ColonizeUnit* t = units_get_const(pool, u->follow_unit_id);
  if (!t || !t->active || !units_is_on_map(t)) {
    units_clear_orders(pool, unit_id);
    return false;
  }
  /* Already adjacent or stacked — hold FOLLOW, no MP spend. */
  if (u->x == t->x && u->y == t->y) {
    return true;
  }
  const int dx = abs(u->x - t->x);
  const int dy = abs(u->y - t->y);
  if (dx <= 1 && dy <= 1) {
    return true;
  }
  /* Retarget tile goto toward target, one step, restore FOLLOW order. */
  const int tid = u->follow_unit_id;
  u->orders = UNITS_ORDER_GOTO;
  u->goto_x = t->x;
  u->goto_y = t->y;
  const bool stepped = units_advance_goto_one_step_w(w, unit_id);
  u = units_get(pool, unit_id);
  if (!u || !u->active) {
    return false;
  }
  /* Re-arm FOLLOW unless the unit was cleared (arrived / blocked clears goto). */
  u->orders = UNITS_ORDER_FOLLOW;
  u->follow_unit_id = tid;
  u->goto_x = t->x;
  u->goto_y = t->y;
  return stepped;
}


#define UNITS_FLOOD_W 16
#define UNITS_FLOOD_INF 0x3fff
#define UNITS_FLOOD_QMAX 256

/*
 * FUN_OVL20_L0000__0015bc per-edge cost for stepping (cx,cy) -> (nx,ny),
 * viceroy_overlays.c:86680-86700 (2026-08-29 re-read). DOS:
 *   +1  when FA(cur)&0xa && FA(cand)&0xa   (layer2 road 0x08 / city 0x02
 *                                            on both tiles: road move)
 *       or DS:0x1dd4 (the populator's build-mode flag — uniform BFS)
 *       or river(cur)&0x40 && river(cand)&0x40 && cardinal step
 *   else `movement < 4` ? 3 : terr_cost[cand]*3.
 * Earlier notes called the 0x1dd4 arm a "cached/favored route" override;
 * it's the road/river pair rule, ×3-scaled like the rest of the formula
 * (map_move_cost_step is the same rule at NAMES scale).
 */
static int units_flood_edge(
  const ColonizeWorldMap* map, bool low_move, int cx, int cy, int nx, int ny
) {
  const bool road_cur = map_tile_has_road(map, cx, cy) || map_tile_has_city(map, cx, cy);
  const bool road_cand = map_tile_has_road(map, nx, ny) || map_tile_has_city(map, nx, ny);
  if (road_cur && road_cand) {
    return 1;
  }
  if (map_tile_has_river(map, cx, cy) && map_tile_has_river(map, nx, ny) &&
      (cx == nx || cy == ny)) {
    return 1;
  }
  int edge = low_move ? 3 : map_dos_terr_cost_byte(map_dos_terr_class_at(map, nx, ny)) * 3;
  /*
   * Out-of-table guard, not DOS: the terrain-class records only run 0..28, so
   * DOS never indexes the 255 bytes sitting at 29..31 and its flood applies no
   * clamp. The port's class comes from terrain_byte & 0x1f, which can reach
   * 30/31 on a corrupt or hand-edited map, and both map.c cost helpers already
   * fold that sentinel to 1 (map_move_spent_thirds, map_move_cost_step). Same
   * fold here so the flood cannot price a tile at 765 thirds that the real move
   * charges 1 for. No effect on any class a valid map produces.
   */
  if (edge > 100) {
    edge = 1;
  }
  return edge > 0 ? edge : 1;
}

/*
 * 0015bc's ownership terms on a candidate tile (viceroy_overlays.c:86716-
 * 86733, re-applied verbatim in its neighbour-pick tail :86803-86812).
 * Returns -1 = reject, else the additive penalty (0 or 8).
 */
static int units_flood_owner_term(
  const ColonizeUnit* u, const ColonizeWorldMap* map, int nx, int ny
) {
  if (u->nation_id < 0) {
    return 0;
  }
  const int occupant = map_tile_tribe_or_presence(map, nx, ny);
  if (occupant >= 0 && occupant != u->nation_id) {
    return -1;
  }
  if (u->nation_id <= 3 && g_units_ff_col1 && map->improve &&
      map->improve[(size_t)ny * (size_t)map->width + (size_t)nx] != 0) {
    const int owner = (int)((map_get_layer3(map, nx, ny) >> 4) & 0x0fu);
    /* DOS 88d6: euro_relation & 0x40 = PEACE (bit map re-derived 2026-08-27). */
    if (owner >= 0 && owner <= 3 && owner != u->nation_id &&
        (g_units_ff_col1->nation[u->nation_id].euro_relation[owner] & AI_DIPLO_PEACE) != 0) {
      if (u->nation_id != g_units_combat_human_nation) {
        return -1;
      }
      return 8;
    }
  }
  return 0;
}

/*
 * The single retained destination cost grid of FUN_6662_00f2 (DS:-0x5d90)
 * and its goal key (DS:0x2d1a / DS:0x2d1c). See units_flood_next_step.
 * bugs.md #706.
 */
static int s_flood_cost[UNITS_FLOOD_W][UNITS_FLOOD_W];
static int s_flood_gx = -1;
static int s_flood_gy = -1;
static const ColonizeWorldMap* s_flood_map = NULL;

/* Not a DOS act: DOS never reloads a different world into the same DS. A
 * New Game / Load can reuse a goal coordinate on a different map, so drop
 * the grid with the rest of the goto statics. */
static void units_flood_cache_invalidate(void) {
  s_flood_gx = -1;
  s_flood_gy = -1;
  s_flood_map = NULL;
}

static bool units_flood_next_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int gx,
  int gy,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !out_x || !out_y) {
    return false;
  }
  /*
   * FUN_OVL20_L0000__0015bc (viceroy_overlays.c:86572-86900), structural
   * port re-aligned 2026-08-29 against the full decompile:
   *  - window: candidates need |cand-goal| < 8 on both axes (goal-7..goal+7,
   *    not the goal-8 edge the old 16-box admitted); 225 expansions max.
   *  - `movement < 4` (raw DS:0x5234 column) picks flat 3 vs terr_cost*3;
   *    the road/river pair rule gives +1 (units_flood_edge).
   *  - cross-domain candidate (land tile for a ship) only when it carries
   *    a settlement (FUN_1000_8886 >= 0) AND is the unit's own tile or the
   *    goal — DOS never routes *through* a colony; units_can_enter keeps
   *    the legality half.
   *  - ownership terms (units_flood_owner_term): tribe/foreign-presence
   *    tile hard-rejects; improved tile of a MET Euro nation rejects AI
   *    movers, +8 for the human nation.
   *  - popped cells with cost above the unit's own (once known) are not
   *    expanded; the unit's tile itself is never expanded (DS:0xa370 latch).
   *  - neighbour pick (:86760-86840): cost[cand] != 0 && < cost[unit];
   *    score = cost[cand] + owner term + edge(unit->cand); lower score
   *    wins, equal score only if octile(goal,cand) is strictly lower.
   * Not ported: the sea "continent id == 1" main-ocean gate (Linux water
   * tiles carry no continent id), the type >= 0x13 `FUN_1000_894e` tile
   * gate (accessor unidentified), the DS:0xa370 cost cap register (BX at
   * entry — convention unresolved; Linux caps at "reached").
   */
  /* DS:0x5234 is in thirds (NAMES movement * 3): `< 4` is true only for
   * 1-tile units (Colonist/Soldier/Pioneer/Brave...), not the 2-tile Wagon. */
  const ColonizeUnitType* flood_type = units_type(pool, u->type_index);
  const bool flood_low_move = units_type_max_mp(flood_type) < 4;
  const bool unit_sea = units_unit_is_sea(pool, u);
  const int origin_x = gx - UNITS_FLOOD_W / 2;
  const int origin_y = gy - UNITS_FLOOD_W / 2;

  const int dx0 = gx - origin_x;
  const int dy0 = gy - origin_y;
  if (dx0 < 0 || dy0 < 0 || dx0 >= UNITS_FLOOD_W || dy0 >= UNITS_FLOOD_W) {
    return false;
  }
  if (gx < 0 || gy < 0 || gx >= (int)map->width || gy >= (int)map->height) {
    return false;
  }

  /*
   * DOS-LITERAL FUN_6662_00f2 raw 103880-103886 (bugs.md #706). The cost
   * grid is a DESTINATION flood (seeded at the goal, expanded outwards), and
   * DOS keeps exactly one of them in the fixed DS:-0x5d90 block. It is
   * rebuilt only when
   *     DS:0x2d1a != goal_x || DS:0x2d1c != goal_y ||
   *     grid[unit tile] == 0
   * i.e. when the goal moved, or when the retained grid never reached the
   * unit asking now. Nothing else is part of the key — not the mover's type,
   * domain or nation, even though the expansion reads all three (DS:0x1dd2 /
   * DS:0x1dd6) and stops at the asking unit's own cost. That is why the
   * "did it reach me" half of the test exists, and it is what makes the
   * reuse safe enough for DOS: a grid built for another unit either already
   * covers this one's tile or is thrown away. Ported literally, key and all.
   */
  /* bugs.md #700 removed the pathfinder's last MP pre-filters; the helper
   * stays for the move-commit paths that legitimately ask. */
  (void)units_can_afford_move_cost;

  int (*cost)[UNITS_FLOOD_W] = s_flood_cost;

  const int ulx = u->x - origin_x;
  const int uly = u->y - origin_y;
  const bool unit_in_window =
    ulx >= 0 && uly >= 0 && ulx < UNITS_FLOOD_W && uly < UNITS_FLOOD_W;
  /* s_flood_map is not part of the DOS key (DOS has exactly one world in
   * DS for the whole process); it only stops a grid leaking across a
   * New Game / Load / unit-test map swap that happens to reuse a goal. */
  const bool reuse_grid = unit_in_window && s_flood_map == map && s_flood_gx == gx &&
                          s_flood_gy == gy &&
                          cost[uly][ulx] < UNITS_FLOOD_INF;

  int unit_cost = UNITS_FLOOD_INF;
  if (reuse_grid) {
    unit_cost = cost[uly][ulx];
  } else {
  s_flood_map = map;
  s_flood_gx = gx;
  s_flood_gy = gy;
  for (int y = 0; y < UNITS_FLOOD_W; ++y) {
    for (int x = 0; x < UNITS_FLOOD_W; ++x) {
      cost[y][x] = UNITS_FLOOD_INF;
    }
  }

  int qx[UNITS_FLOOD_QMAX];
  int qy[UNITS_FLOOD_QMAX];
  int qh = 0;
  int qt = 0;

  cost[dy0][dx0] = 1;
  qx[qt] = gx;
  qy[qt] = gy;
  qt = (qt + 1) % UNITS_FLOOD_QMAX;

  int expansions = 0;
  while (qh != qt && expansions < 225) {
    const int cx = qx[qh];
    const int cy = qy[qh];
    qh = (qh + 1) % UNITS_FLOOD_QMAX;
    ++expansions;
    const int lx = cx - origin_x;
    const int ly = cy - origin_y;
    const int base = cost[ly][lx];
    if (base > unit_cost) {
      continue;
    }
    if (cx == u->x && cy == u->y) {
      unit_cost = base;
      continue;
    }
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int nx = cx + dx;
        const int ny = cy + dy;
        if (abs(nx - gx) >= 8 || abs(ny - gy) >= 8) {
          continue;
        }
        if (nx < 0 || ny < 0 || nx >= (int)map->width || ny >= (int)map->height) {
          continue;
        }
        const int nlx = nx - origin_x;
        const int nly = ny - origin_y;
        const bool cand_water = map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny);
        if (cand_water != unit_sea) {
          if (!map_tile_has_city(map, nx, ny)) {
            continue;
          }
          if (!((nx == u->x && ny == u->y) || (nx == gx && ny == gy))) {
            continue;
          }
        }
        if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, u->type_index, nx, ny, unit_id)) {
          continue;
        }
        const int owner_term = units_flood_owner_term(u, map, nx, ny);
        if (owner_term < 0) {
          continue;
        }
        const int nc = base + owner_term + units_flood_edge(map, flood_low_move, cx, cy, nx, ny);
        if (nc < cost[nly][nlx]) {
          cost[nly][nlx] = nc;
          const int next_t = (qt + 1) % UNITS_FLOOD_QMAX;
          if (next_t != qh) {
            qx[qt] = nx;
            qy[qt] = ny;
            qt = next_t;
          }
        }
      }
    }
  }

  } /* !reuse_grid */

  if (unit_cost >= UNITS_FLOOD_INF) {
    return false;
  }

  int best_x = -1;
  int best_y = -1;
  /*
   * Both bounds are the DOS-LITERALS, not a port cap: the neighbour-pick tail
   * opens with `uStack_30 = 99; uStack_e = 99;` (viceroy_overlays.c:86761-86762,
   * FUN_OVL20_L0000__0015bc). Costs are thirds, so a path priced above 99
   * inside the 15x15 window genuinely takes no candidate and the flood reports
   * failure — DOS behaves the same and the caller falls back to the greedy tier.
   */
  int best_score = 99;
  int best_tie = 99;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      const int nx = u->x + dx;
      const int ny = u->y + dy;
      if (abs(nx - gx) >= 8 || abs(ny - gy) >= 8) {
        continue;
      }
      if (nx < 0 || ny < 0 || nx >= (int)map->width || ny >= (int)map->height) {
        continue;
      }
      const int nlx = nx - origin_x;
      const int nly = ny - origin_y;
      int c = cost[nly][nlx];
      if (c >= unit_cost) {
        continue;
      }
      const int owner_term = units_flood_owner_term(u, map, nx, ny);
      if (owner_term < 0) {
        continue;
      }
      c += owner_term;
      if (c >= unit_cost) {
        continue;
      }
      if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, u->type_index, nx, ny, unit_id)) {
        continue;
      }
      /*
       * bugs.md #700: no MP pre-filter here. FUN_OVL20_L0000__0015bc's
       * neighbour pick (viceroy_overlays.c:86760-86840) never reads the
       * mover's remaining MP — the partial-MP overspend is decided later by
       * FUN_465b_0000 (viceroy_unpacked.c raw 75643), exactly as for an
       * arrow-key step. Dropping unaffordable candidates here made a
       * part-spent unit path differently from a fresh one.
       */
      const int score = c + units_flood_edge(map, flood_low_move, u->x, u->y, nx, ny);
      if (score > best_score) {
        continue;
      }
      const int tie = units_octile(gx, gy, nx, ny);
      if (score == best_score && tie >= best_tie) {
        continue;
      }
      best_score = score;
      best_tie = tie;
      best_x = nx;
      best_y = ny;
    }
  }
  if (best_x < 0) {
    return false;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

#define UNITS_BFS_MAX 2048

static bool units_bfs_next_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int gx,
  int gy,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !map || !out_x || !out_y) {
    return false;
  }
  const int w = (int)map->width;
  const int h = (int)map->height;
  if (w <= 0 || h <= 0 || w * h > 128 * 128) {
    /* Huge maps: fall back without allocating a full visit grid. */
    return false;
  }

  const int cells = w * h;
  uint8_t* visited = (uint8_t*)calloc((size_t)cells, 1);
  int* parent = (int*)malloc((size_t)cells * sizeof(int));
  if (!visited || !parent) {
    free(visited);
    free(parent);
    return false;
  }
  for (int i = 0; i < cells; ++i) {
    parent[i] = -1;
  }

  int* qx = (int*)malloc((size_t)UNITS_BFS_MAX * sizeof(int));
  int* qy = (int*)malloc((size_t)UNITS_BFS_MAX * sizeof(int));
  if (!qx || !qy) {
    free(visited);
    free(parent);
    free(qx);
    free(qy);
    return false;
  }

  int qh = 0;
  int qt = 0;
  const int start = u->y * w + u->x;
  visited[start] = 1;
  qx[qt] = u->x;
  qy[qt] = u->y;
  qt++;

  bool found = false;
  while (qh < qt && qt < UNITS_BFS_MAX) {
    const int cx = qx[qh];
    const int cy = qy[qh];
    ++qh;
    if (cx == gx && cy == gy) {
      found = true;
      break;
    }
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int nx = cx + dx;
        const int ny = cy + dy;
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
          continue;
        }
        const int ni = ny * w + nx;
        if (visited[ni]) {
          continue;
        }
        if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, u->type_index, nx, ny, unit_id)) {
          continue;
        }
        visited[ni] = 1;
        parent[ni] = cy * w + cx;
        if (qt < UNITS_BFS_MAX) {
          qx[qt] = nx;
          qy[qt] = ny;
          qt++;
        }
      }
    }
  }

  bool ok = false;
  if (found) {
    int cur = gy * w + gx;
    int prev = parent[cur];
    while (prev >= 0 && prev != start) {
      cur = prev;
      prev = parent[cur];
    }
    if (prev == start) {
      *out_x = cur % w;
      *out_y = cur / w;
      /* bugs.md #700: no MP pre-filter (the DOS pathfinder never reads MP;
       * FUN_465b_0000 raw 75643 owns the overspend). */
      ok = true;
    }
  }

  free(visited);
  free(parent);
  free(qx);
  free(qy);
  return ok;
}

/* DOS dir8 table (DS:0xb4/0xbe), also used project-wide; index^4 = reverse. */
static int units_dir8_index(int dx, int dy) {
  for (int i = 0; i < 8; ++i) {
    if (MAP_DIR8_DX[i] == dx && MAP_DIR8_DY[i] == dy) {
      return i;
    }
  }
  return -1;
}

/*
 * FUN_6662_0f74's own last-taken-step tracker (unit+0x314f), used only for
 * the anti-backtrack wiggle-retry below. Deliberately NOT `ColonizeUnit`'s
 * `last_dir` field — that one is already live-owned by ai.c's Indian native
 * Brave engine (`ai_native_pick_dir`); writing it here would silently
 * corrupt that engine's own bookkeeping for any unit that also takes a
 * goto step. Same shadow-array pattern ai_euro.c already uses for its own
 * Euro `last_dir` equivalent (`s_euro_last_dir`), for the same reason.
 * Stored as dir+1 so the zero-initialised slot means "no history" (the old
 * "0 == North, harmless bias" reading was not harmless: a ship whose first
 * pathfinder step was South saw it as the reverse of a step it never took,
 * dropped the flood hit and fell back to the greedy tier — campaign3
 * Spanish Caravel ping-pong at (32,51)). Any goto stepper that commits a
 * move outside units_advance_goto_one_step (the AI ship sail loop) must
 * record its step through units_note_goto_step so the anti-backtrack check
 * compares against the unit's real last step.
 */
static int8_t s_units_goto_last_dir[COLONIZE_UNITS_MAX];

static int units_goto_last_dir_reverse(int unit_id) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX || s_units_goto_last_dir[unit_id] <= 0) {
    return -1;
  }
  return (s_units_goto_last_dir[unit_id] - 1) ^ 4;
}

void units_note_goto_step(int unit_id, int dx, int dy) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX) {
    return;
  }
  const int d = units_dir8_index(dx, dy);
  if (d >= 0) {
    s_units_goto_last_dir[unit_id] = (int8_t)(d + 1);
  }
}

/*
 * Statics-reset sweep 2026-09-16: s_units_goto_last_dir is indexed by
 * unit_id, not owned by the pool, so units_reset(pool) never touches it — a
 * New Game / Load reuses low unit ids for brand-new units that inherit a
 * stale shadow direction from the previous campaign. units_coarse (below,
 * s_units_coarse) is deliberately NOT reset here: it caches walkability by
 * (map pointer, width, height), and the world map's land/water layout never
 * changes across a New Game / Load in the same process (always the same
 * AMER2.MP), so the cache is content-correct without ever rebuilding.
 */
void units_reset_state(void) {
  memset(s_units_goto_last_dir, 0, sizeof(s_units_goto_last_dir));
  units_flood_cache_invalidate();
}

/*
 * The greedy tier's own ownership gate — FUN_6662_0f74 carries one, exactly
 * like 0015bc's flood tier (units_flood_owner_term), it is just spelled as a
 * hard skip instead of an additive penalty (viceroy_unpacked.c:104678-104690):
 *
 *   uVar19 = FUN_281f_06d2(cand);            // = FUN_1000_88c2, the same
 *   if ((int)uVar19 < 0 || uVar19 == uVar8)  // tile_tribe_or_presence the
 *     ... domain test; on mismatch fall through to LAB_6662_13a5
 *   else
 *   LAB_6662_13a5:
 *     uVar19 = FUN_281f_0696(cand);          // Euro settlement owner
 *     if (uVar19 != uVar8 || cand != (uVar21,uVar5)) goto LAB_6662_12f6;
 *
 * (uVar8 = the mover's nation nibble, unit+0x3147 & 0xf, :104553;
 *  uVar21/uVar5 = the goto goal, unit+0x314d/+0x314e, :104537-104538.)
 * So a tile another nation or tribe physically occupies — a foreign colony
 * or village counts even with no unit on it, since 06d2 reads the settlement
 * bit first — is never stepped through; the sole escape is this nation's OWN
 * colony when that colony is the goal itself. DOS's domain half of the same
 * `if` is already units_can_enter's job here (and its own-colony DOCK arm is
 * the same escape), so only the ownership half is expressed.
 *
 * NOT units_flood_owner_term: the flood's second arm (FUN_1000_88d6, the
 * improved-tile +8 on a peer's land) has no counterpart in 0f74, which knows
 * only "occupied by someone else" — porting the +8 here would be invention.
 */
static bool units_greedy_owner_ok(
  const ColonizeUnit* u,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  int nx,
  int ny,
  int gx,
  int gy
) {
  if (!u || u->nation_id < 0) {
    return true;
  }
  const int occupant = map_tile_tribe_or_presence(map, nx, ny);
  if (occupant < 0 || occupant == u->nation_id) {
    return true;
  }
  /* FUN_281f_0696: Euro colony owner (clamps any owner above 3 to -1). */
  const ColonizeColony* col =
    colonies ? colonies_get(colonies, colonies_id_at(colonies, nx, ny)) : NULL;
  const int colony_owner =
    (col && col->active && col->nation_id >= 0 && col->nation_id <= 3) ? col->nation_id : -1;
  return colony_owner == u->nation_id && nx == gx && ny == gy;
}

static bool units_greedy_next_step(
  const ColonizeUnitPool* pool,
  int unit_id,
  const ColonizeWorldMap* map,
  const ColonizeColonyPool* colonies,
  ColonizeDosRng* rng,
  /* gx/gy = DS:0xa14e/0xa14c, the tile the last flood aimed at (goal on the
   * near tier, coarse waypoint on the far one) — every distance term uses
   * it. goal_x/goal_y = the unit's real +0x314d/e goal, which only the
   * 06d2/0696 own-colony escape reads (raw 104690). bugs.md #698. */
  int gx,
  int gy,
  int goal_x,
  int goal_y,
  int* out_x,
  int* out_y
) {
  const ColonizeUnit* u = units_get_const(pool, unit_id);
  if (!u || !out_x || !out_y) {
    return false;
  }
  int best_x = -1;
  int best_y = -1;
  int best_score = 1 << 30;
  /*
   * bugs.md #732 (2026-09-23): the invented 5-candidate pre-tier
   * (`octile*10 + step_cost` over the goal-ward directions) that used to
   * run ahead of this loop is gone — it had no DOS counterpart and shadowed
   * the real FUN_6662_0f74 loop below almost every time. golden_ai_turns
   * 6/6 and the full suite are unchanged without it.
   */
  {
    /*
     * The 8-neighbour scored loop (FUN_6662_0f74's own scored tail,
     * viceroy_unpacked.c:104652-104720 — transcribed byte-exact, not the
     * `move_scoring_20e6_full.md` prose summary, which undersold the
     * distance term). Per-candidate:
     *   score = penalty + chebyshev(cand,goal)*4 + manhattan(cand,goal)*5
     * where penalty = 3 if the unit's max MP < 2 (`FUN_281f_090c`),
     * else terrain_cost[candidate_terrain]*3 (DS:0x2f76 offset +0 —
     * `map_dos_terr_cost_byte`, unblocked by the 2026-08-21 terrain-table
     * capture, `T4.1`/`terrain_yields.md`). AI-owned units (not the human
     * nation) additionally skip any candidate that doesn't improve or hold
     * (chebyshev+manhattan to goal) vs. the unit's current position —
     * `bVar27`'s `DS:0x543f` per-nation human-flag gate; reused via the
     * existing `g_units_combat_human_nation` module cache rather than
     * threading a new parameter through 16+ call sites.
     *
     * `FUN_281f_090c` itself re-resolved 2026-08-21 (its `address_mapping.csv`
     * row is stale/wrong — points at `FUN_1000_8afc`, an unrelated
     * village/nation-throttle function; the real 2-call thunk body's own
     * second call lands at `FUN_1427_065a` -> `FUN_0000_48ca`, confirmed a
     * clean, plausible max-MP accessor by direct disassembly, see
     * `euro_unit_act.md`'s T1.8 update). Real DOS formula there is
     * `type_table[type][DS:0x5234] + (3 if a per-nation capability bit is
     * set AND type is in the ship range 0xd..0x12, else 0)` — `DS:0x5234`
     * is this project's already-known `0x5236`/`0x5239`/`0x523d` unit-type
     * table (stride 0xe), offset +0, i.e. exactly `type->movement`, so the
     * `type->movement` read below is the right field. The ship-only `+3`
     * bonus is a real gap (not wired: the gating nation flag isn't
     * independently identified yet, and ship base movement is never <2 in
     * practice, so this bonus can't actually change the `<2` branch —
     * negligible real-world effect either way).
     */
    /* FUN_281f_090c returns thirds (>= 3 for every type), so `max_mp < 2`
     * never holds in DOS — the penalty is always terr_cost*3. Kept literal. */
    const int max_mp = units_max_mp(pool, unit_id);
    const bool is_human_unit =
      g_units_combat_human_nation >= 0 && u->nation_id == g_units_combat_human_nation;
    const int cheb_cur = units_chebyshev(u->x, u->y, gx, gy);
    const int manh_cur = abs(gx - u->x) + abs(gy - u->y);
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        const int nx = u->x + dx;
        const int ny = u->y + dy;
        const int cheb_cand = units_chebyshev(nx, ny, gx, gy);
        const int manh_cand = abs(gx - nx) + abs(gy - ny);
        if (!is_human_unit && cheb_cand + manh_cand > cheb_cur + manh_cur) {
          continue;
        }
        /* 0f74's own 06d2/0696 gate, :104678-104690 — see
         * units_greedy_owner_ok. This is the loop DOS spells it in. */
        if (!units_greedy_owner_ok(u, map, colonies, nx, ny, goal_x, goal_y)) {
          continue;
        }
        if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, u->type_index, nx, ny, unit_id)) {
          continue;
        }
        /* bugs.md #700: no MP pre-filter (see the head of this loop's tier). */
        /*
         * DOS-LITERAL FUN_6662_0f74 raw 104676-104690 (bugs.md #699).
         * ndisasm re-read of 6662:13d1-1420 (viceroy_unpacked_2.asm), because
         * the Ghidra parenthesisation of this test is misleading:
         *   13d1: (cur & 0x0a) && (FUN_281f_0754(cand) & 0x0a) -> 1
         *   13f0: (cur & 0x40) && (FUN_281f_072c(cand) & 0x40)
         *         && cur_x == cand_x                            -> 1
         *   1410: cur_y == cand_y                               -> 1
         *   else: FUN_281f_090c(unit) < 2 ? 3 : terr_cost*3
         * The 1410 arm is a bare `CMP local_40,local_e / JZ LAB_6662_13e9`
         * with no river operand in scope, i.e. DOS gives EVERY due-East /
         * due-West step the discounted penalty 1 whether or not a river or
         * road is involved. Almost certainly a MicroProse slip (the river
         * arm wanted `cur_x == cand_x || cur_y == cand_y`), but it is what
         * VICEROY.EXE executes, so it is what the port executes. Note the
         * flood tier (FUN_OVL20_L0000__0015bc, units_flood_edge) spells the
         * sane both-axes river rule — the asymmetry is DOS-real.
         */
        const bool road_cur =
          map_tile_has_road(map, u->x, u->y) || map_tile_has_city(map, u->x, u->y);
        const bool road_cand = map_tile_has_road(map, nx, ny) || map_tile_has_city(map, nx, ny);
        int penalty;
        if (road_cur && road_cand) {
          penalty = 1;
        } else if (map_tile_has_river(map, u->x, u->y) && map_tile_has_river(map, nx, ny) &&
                   u->x == nx) {
          penalty = 1;
        } else if (u->y == ny) {
          penalty = 1;
        } else {
          penalty = max_mp < 2 ? 3 : map_dos_terr_cost_byte(map_dos_terr_class_at(map, nx, ny)) * 3;
        }
        const int score = penalty + cheb_cand * 4 + manh_cand * 5;
        if (score < best_score) {
          best_score = score;
          best_x = nx;
          best_y = ny;
        }
      }
    }
  }
  if (best_x < 0) {
    return false;
  }
  /*
   * FUN_6662_0f74 tail (anti-backtrack wiggle): when the scored fallback's
   * best pick is the exact reverse of the unit's last-taken step, DOS
   * doesn't take it — it rerolls up to 8 random directions instead,
   * accepting the first legal/affordable one (0f74 gates this on
   * unit+0x314c=='\v'/goto-pending; Linux has no live copy of that cache
   * per move_scoring_20e6_full.md, so gate on the live equivalent —
   * already following a goto here). Avoids visible ping-pong between two
   * tiles. Cite: euro_unit_act.md T1.8.
   */
  if (rng != NULL && unit_id >= 0 && unit_id < COLONIZE_UNITS_MAX &&
      units_dir8_index(best_x - u->x, best_y - u->y) ==
        units_goto_last_dir_reverse(unit_id) &&
      units_orders_follow_goto(u->orders)) {
    int wig_x = -1;
    int wig_y = -1;
    for (int tries = 0; tries < 8 && wig_x < 0; ++tries) {
      const int d = dos_rng_range(rng, 0, 7);
      const int nx = u->x + MAP_DIR8_DX[d];
      const int ny = u->y + MAP_DIR8_DY[d];
      /*
       * The reroll re-applies 06d2 as well (viceroy_unpacked.c:104724-104726:
       * `if ((-1 < uVar21) && (uVar21 != uVar8)) ... local_1c = 0xffff`), and
       * here with no own-colony escape at all — a random sidestep never lands
       * on someone else's square.
       */
      const int wig_occ = map_tile_tribe_or_presence(map, nx, ny);
      if (wig_occ >= 0 && u->nation_id >= 0 && wig_occ != u->nation_id) {
        continue;
      }
      if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(pool), .colonies=(ColonizeColonyPool*)(colonies), .map=(ColonizeWorldMap*)(map)}, u->type_index, nx, ny, unit_id)) {
        continue;
      }
      /* bugs.md #700: the reroll (raw 104724-104731) tests 06d2 / 0768 /
       * 06b4 / 0302 only — never the mover's MP. */
      wig_x = nx;
      wig_y = ny;
    }
    if (wig_x < 0) {
      /* All 8 rerolls rejected: DOS falls through to total failure here. */
      return false;
    }
    best_x = wig_x;
    best_y = wig_y;
  }
  *out_x = best_x;
  *out_y = best_y;
  return true;
}

/* ======================================================================
 * FUN_6662_0f74 far tier — coarse-grid waypoint pathing (2026-08-27 port).
 *
 * DOS keeps two quarter-resolution walkability bitmaps (DS:0x85e8 land,
 * DS:0x86f6 sea; 15 rows x 18 cols, stride 0x12, cell = 4x4 tiles, sample
 * point = (4cx+1, 4cy+1)) built once per game by FUN_OVL21_L0040__0007d8:
 * each cell byte is an 8-bit mask of the directions (DS:0xb4/0xbe order)
 * whose neighbour cell is reachable in that domain. Linux rebuilds the
 * same tables lazily per map (terrain is static): a cell holds the domain
 * when any tile of its 2x2 sample block does (the FUN_OVL20_L0000__000000
 * probe's own test), and bit d is set when a windowed flood between the two
 * sample points succeeds (the FUN_6662_0906 Chebyshev<8 / 0015bc check the
 * populator runs per direction — approximated with a domain-only BFS in a
 * 16x16 window). The populator's own body is not decompiled; this is the
 * consumer-side reconstruction, flagged as such (euro_unit_act.md).
 *
 *   FUN_6662_09ae  snap (x,y) to a walkable cell: its own cell when the
 *                  probe passes, else the nearest walkable neighbour cell
 *                  by DOS distance (FUN_124c_0040) from cell centre to (x,y).
 *   0015c1         U = snap(unit), G = snap(goal); BFS over the coarse grid
 *                  from G (cost 1) until U pops; among U's neighbours pick
 *                  the lowest cost, ties by octile distance to the goal
 *                  (FUN_1000_856a); the 000000 probe turns that cell into a
 *                  real tile -> waypoint (DS:0xa14e/0xa14c).
 *   0f74 far arm   flood (0015bc) toward the waypoint; if that fails, toward
 *                  the probed centre of U (DS:0xa572/0xa574 * 4 + 1); then
 *                  the scored 8-neighbour fallback.
 * The DOS sea probe also requires continent id 1; Linux water tiles carry no
 * continent id (map_continent_id_at -> -1), so that term is dropped.
 * ====================================================================== */
#define UNITS_COARSE_ROWS 15
#define UNITS_COARSE_COLS 18
typedef struct UnitsCoarseGrid {
  const ColonizeWorldMap* map;
  int width;
  int height;
  uint8_t mask[2][UNITS_COARSE_ROWS][UNITS_COARSE_COLS];
  uint8_t walk[2][UNITS_COARSE_ROWS][UNITS_COARSE_COLS];
} UnitsCoarseGrid;
static UnitsCoarseGrid s_units_coarse;
/* ===================== Coarse-grid route caching & goto step advancement (units_coarse_domain_tile .. units_advance_goto) ===================== */


static int units_coarse_domain_tile(const ColonizeWorldMap* map, int x, int y, int sea) {
  if (x < 0 || y < 0 || x >= (int)map->width || y >= (int)map->height) {
    return 0;
  }
  const int water = map_tile_is_water(map, x, y) || map_tile_is_high_seas(map, x, y);
  return sea ? water : !water;
}

/* Body id a coarse cell probe compares (FUN_1000_88a4 on the sample tile):
 * land -> continent id; sea -> the DOS ocean-body nibble (1 = the main
 * ocean, lakes differ) — Linux keeps that nibble in layer3 for water too
 * when it came from a save; a zero nibble is read as the main ocean. */
static int units_coarse_body_id(const ColonizeWorldMap* map, int x, int y, int sea) {
  if (!sea) {
    return map_continent_id_at(map, x, y);
  }
  const int nib = (int)(map_get_layer3(map, x, y) & 0x0fu);
  return nib == 0 ? 1 : nib;
}

/* FUN_OVL21_L0040__000758 / FUN_OVL20_L0000__000000: 2x2 probe at
 * (4cx+1.., 4cy+1..); returns the body id of the first domain tile (sea
 * tiles must be body 1, the main ocean) or -1; writes the tile. */
static int units_coarse_probe_id(
  const ColonizeWorldMap* map, int cx, int cy, int sea, int* out_x, int* out_y
) {
  for (int r = cx * 4 + 1; r <= cx * 4 + 2; ++r) {
    for (int c = cy * 4 + 1; c <= cy * 4 + 2; ++c) {
      if (!units_coarse_domain_tile(map, r, c, sea)) {
        continue;
      }
      const int id = units_coarse_body_id(map, r, c, sea);
      if (sea && id != 1) {
        continue;
      }
      if (out_x) {
        *out_x = r;
      }
      if (out_y) {
        *out_y = c;
      }
      return id;
    }
  }
  return -1;
}
static int units_coarse_probe(
  const ColonizeWorldMap* map, int cx, int cy, int sea, int* out_x, int* out_y
) {
  return units_coarse_probe_id(map, cx, cy, sea, out_x, out_y) >= 0;
}

/*
 * FUN_6662_0906 (viceroy_unpacked.c:104150): with (ax,ay) the mover's tile
 * and (bx,by) the flood goal, returns -1 unless |a-b| < 8 on both axes, 0
 * when a == b, else runs 0015bc from b (DS:0x1dd6 = -1, no ownership terms)
 * and returns the flood cost at a (DS:0xa370), -1 when a was not reached.
 * Two callers: the populator (DS:0x1dd4 = 1 -> every edge costs 1, gate
 * 0 < cost < 8) and 0009ae's neighbour validation (gate >= 0). Here the
 * flood is the domain-only walk over 0015bc's own window (|cand-b| < 8,
 * 225 expansions); with `steps` = 1 each edge, `cost` is the step count.
 */
static int units_coarse_reach(const ColonizeWorldMap* map, int ax, int ay, int bx, int by, int sea) {
  if (abs(ax - bx) >= 8 || abs(ay - by) >= 8) {
    return -1;
  }
  if (ax == bx && ay == by) {
    return 0;
  }
  uint8_t dist[16][16];
  memset(dist, 0, sizeof(dist));
  const int ox = bx - 8;
  const int oy = by - 8;
  int qx[256];
  int qy[256];
  int head = 0;
  int tail = 0;
  qx[tail] = bx;
  qy[tail] = by;
  tail++;
  dist[bx - ox][by - oy] = 1;
  int expansions = 0;
  while (head < tail && expansions < 225) {
    const int x = qx[head];
    const int y = qy[head];
    head++;
    ++expansions;
    if (x == ax && y == ay) {
      return dist[x - ox][y - oy];
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = x + MAP_DIR8_DX[d];
      const int ny = y + MAP_DIR8_DY[d];
      if (abs(nx - bx) >= 8 || abs(ny - by) >= 8) {
        continue;
      }
      const int lx = nx - ox;
      const int ly = ny - oy;
      if (dist[lx][ly] != 0) {
        continue;
      }
      if (!units_coarse_domain_tile(map, nx, ny, sea)) {
        continue;
      }
      dist[lx][ly] = (uint8_t)(dist[x - ox][y - oy] + 1);
      if (tail < 256) {
        qx[tail] = nx;
        qy[tail] = ny;
        tail++;
      }
    }
  }
  return -1;
}

int units_short_sea_route_cost(
  const ColonizeWorldMap* map, int ax, int ay, int bx, int by
) {
  return units_coarse_reach(map, ax, ay, bx, by, 1);
}

/* Populator gate (asm OVL21_L0040:8b3-8bb): 0 < cost < 8. */
static int units_coarse_connected(const ColonizeWorldMap* map, int ax, int ay, int bx, int by, int sea) {
  const int c = units_coarse_reach(map, ax, ay, bx, by, sea);
  return c > 0 && c < 8;
}

static void units_coarse_build(const ColonizeWorldMap* map) {
  UnitsCoarseGrid* g = &s_units_coarse;
  if (g->map == map && g->width == (int)map->width && g->height == (int)map->height) {
    return;
  }
  memset(g, 0, sizeof(*g));
  g->map = map;
  g->width = (int)map->width;
  g->height = (int)map->height;
  for (int sea = 0; sea < 2; ++sea) {
    for (int cx = 0; cx < UNITS_COARSE_ROWS; ++cx) {
      for (int cy = 0; cy < UNITS_COARSE_COLS; ++cy) {
        g->walk[sea][cx][cy] = (uint8_t)units_coarse_probe(map, cx, cy, sea, NULL, NULL);
      }
    }
    for (int cx = 0; cx < UNITS_COARSE_ROWS; ++cx) {
      for (int cy = 0; cy < UNITS_COARSE_COLS; ++cy) {
        if (!g->walk[sea][cx][cy]) {
          continue;
        }
        int ax = 0;
        int ay = 0;
        const int id_a = units_coarse_probe_id(map, cx, cy, sea, &ax, &ay);
        /* asm OVL21_L0040:853-915: directions 0..3 only, the reverse bit
         * ((d+4)&7) is written on the neighbour; both cells must probe to
         * the same body id. */
        for (int d = 0; d < 4; ++d) {
          const int nx = cx + MAP_DIR8_DX[d];
          const int ny = cy + MAP_DIR8_DY[d];
          if (nx < 0 || ny < 0 || nx >= UNITS_COARSE_ROWS || ny >= UNITS_COARSE_COLS ||
              !g->walk[sea][nx][ny]) {
            continue;
          }
          int bx = 0;
          int by = 0;
          if (units_coarse_probe_id(map, nx, ny, sea, &bx, &by) != id_a) {
            continue;
          }
          if (units_coarse_connected(map, ax, ay, bx, by, sea)) {
            g->mask[sea][cx][cy] |= (uint8_t)(1u << d);
            g->mask[sea][nx][ny] |= (uint8_t)(1u << ((d + 4) & 7));
          }
        }
      }
    }
  }
}

/* FUN_124c_0040 (ai_dos_dist): max(|dx|,|dy|) + min(|dx|,|dy|)/2. */
/* FUN_6662_09ae */
static int units_coarse_snap(
  const ColonizeWorldMap* map, int x, int y, int sea, int* out_cx, int* out_cy
) {
  const UnitsCoarseGrid* g = &s_units_coarse;
  const int cx = x >> 2;
  const int cy = y >> 2;
  if (cx < 0 || cy < 0 || cx >= UNITS_COARSE_ROWS || cy >= UNITS_COARSE_COLS) {
    return 0;
  }
  int pick = -1;
  if (g->mask[sea][cx][cy] != 0 && units_coarse_probe(map, cx, cy, sea, NULL, NULL)) {
    pick = 8; /* stay */
  }
  if (pick < 0) {
    int best = 99;
    for (int d = 0; d < 8; ++d) {
      const int nx = cx + MAP_DIR8_DX[d];
      const int ny = cy + MAP_DIR8_DY[d];
      if (nx < 0 || ny < 0 || nx >= UNITS_COARSE_ROWS || ny >= UNITS_COARSE_COLS ||
          g->mask[sea][nx][ny] == 0) {
        continue;
      }
      const int dist = map_dos_dist(x - (nx * 4 + 1), y - (ny * 4 + 1));
      if (dist >= best) {
        continue;
      }
      /* 0009ae :104254-104256: the 000000 probe must hit, then the 0906
       * relay (flood from the mover's tile to the probed sample) must
       * answer >= 0. */
      int px = nx * 4 + 1;
      int py = ny * 4 + 1;
      if (!units_coarse_probe(map, nx, ny, sea, &px, &py)) {
        continue;
      }
      if (units_coarse_reach(map, px, py, x, y, sea) < 0) {
        continue;
      }
      best = dist;
      pick = d;
    }
  }
  if (pick < 0) {
    return 0;
  }
  *out_cx = pick == 8 ? cx : cx + MAP_DIR8_DX[pick];
  *out_cy = pick == 8 ? cy : cy + MAP_DIR8_DY[pick];
  return 1;
}

/* FUN_OVL20_L0000__0015c1: coarse waypoint toward (gx,gy). Also reports the
 * snapped unit cell (DS:0xa572/0xa574) for 0f74's fallback. */
static int units_coarse_waypoint(
  const ColonizeWorldMap* map, int ux, int uy, int gx, int gy, int sea,
  int* out_x, int* out_y, int* out_ucx, int* out_ucy
) {
  units_coarse_build(map);
  const UnitsCoarseGrid* g = &s_units_coarse;
  int ucx = 0;
  int ucy = 0;
  int gcx = 0;
  int gcy = 0;
  if (!units_coarse_snap(map, ux, uy, sea, &ucx, &ucy)) {
    return 0;
  }
  *out_ucx = ucx;
  *out_ucy = ucy;
  if (!units_coarse_snap(map, gx, gy, sea, &gcx, &gcy)) {
    return 0;
  }
  uint8_t cost[UNITS_COARSE_ROWS][UNITS_COARSE_COLS];
  memset(cost, 0, sizeof(cost));
  uint8_t qr[256];
  uint8_t qc[256];
  int head = 0;
  int tail = 0;
  cost[gcx][gcy] = 1;
  qr[tail] = (uint8_t)gcx;
  qc[tail] = (uint8_t)gcy;
  tail++;
  int found = 0;
  while (head < tail) {
    const int cx = qr[head];
    const int cy = qc[head];
    head++;
    if (cx == ucx && cy == ucy) {
      found = 1;
      break;
    }
    const uint8_t m = g->mask[sea][cx][cy];
    for (int d = 0; d < 8; ++d) {
      if ((m & (1u << d)) == 0) {
        continue;
      }
      const int nx = cx + MAP_DIR8_DX[d];
      const int ny = cy + MAP_DIR8_DY[d];
      if (nx < 0 || ny < 0 || nx >= UNITS_COARSE_ROWS || ny >= UNITS_COARSE_COLS ||
          cost[nx][ny] != 0) {
        continue;
      }
      cost[nx][ny] = (uint8_t)(cost[cx][cy] + 1);
      if (tail < 256) {
        qr[tail] = (uint8_t)nx;
        qc[tail] = (uint8_t)ny;
        tail++;
      }
    }
  }
  if (!found) {
    return 0;
  }
  int best_d = -1;
  int best_cost = 99;
  int best_dist = 0;
  const uint8_t um = g->mask[sea][ucx][ucy];
  for (int d = 0; d < 8; ++d) {
    if ((um & (1u << d)) == 0) {
      continue;
    }
    const int nx = ucx + MAP_DIR8_DX[d];
    const int ny = ucy + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= UNITS_COARSE_ROWS || ny >= UNITS_COARSE_COLS ||
        cost[nx][ny] == 0) {
      continue;
    }
    const int dist = units_octile(nx * 4 + 1, ny * 4 + 1, gx, gy);
    if (cost[nx][ny] < best_cost || (cost[nx][ny] == best_cost && dist < best_dist)) {
      best_cost = cost[nx][ny];
      best_dist = dist;
      best_d = d;
    }
  }
  if (best_d < 0) {
    return 0;
  }
  const int wcx = ucx + MAP_DIR8_DX[best_d];
  const int wcy = ucy + MAP_DIR8_DY[best_d];
  int wx = wcx * 4 + 1;
  int wy = wcy * 4 + 1;
  (void)units_coarse_probe(map, wcx, wcy, sea, &wx, &wy);
  *out_x = wx;
  *out_y = wy;
  return 1;
}

bool units_next_goto_step_w(
  const ColonizeWorld* w,
  int unit_id,
  int* out_x,
  int* out_y
) {
  const ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;
  ColonizeDosRng* rng = w->rng;

  const ColonizeUnit* u = units_get_const(pool, unit_id);
  /* Aboard passengers path from their ship's tile (x/y stay synced). */
  if (!u || !u->active || !map || !out_x || !out_y ||
      (!units_is_on_map(u) && u->aboard_ship_id < 0)) {
    return false;
  }
  if (!units_orders_follow_goto(u->orders)) {
    return false;
  }
  const int gx = u->goto_x;
  const int gy = u->goto_y;
  if (gx < 0 || gy < 0 || gx >= (int)map->width || gy >= (int)map->height ||
      gx >= UNITS_GOTO_NONE || gy >= UNITS_GOTO_NONE) {
    return false;
  }
  if (u->x == gx && u->y == gy) {
    return false;
  }

  const int adx = abs(gx - u->x);
  const int ady = abs(gy - u->y);

  /* Adjacent: sign-step (FUN_6662_0086). No afford pre-check here — a
   * partial-MP overspend is decided by units_try_move's own DOS roll, the
   * same as an arrow-key step; the earlier hard afford gate made a one-tile
   * Go To (e.g. onto a lost-city rumour) fail where the arrow key
   * succeeded (bugs.md). */
  if (units_chebyshev(u->x, u->y, gx, gy) < 2) {
    /*
     * DOS-LITERAL FUN_6662_0f74 raw 104556-104565 (bugs.md #701): when both
     * |dx| and |dy| are below 2 the pathfinder returns
     * thunk_FUN_2a1f_059c -> FUN_6662_0086 (raw 103795-103840) and nothing
     * else. 0086 is a pure sign -> dir8 table lookup: it tests no
     * enterability, consults no terrain and has no fallback tier. The
     * caller (FUN_479b_0972) hands the direction straight to
     * FUN_465b_0000, which is where a refusal is decided. The old
     * units_can_enter/greedy fallback made an adjacent Go To sidestep to a
     * different tile where DOS simply fails the move and keeps the order.
     */
    *out_x = u->x + units_sign_i(gx - u->x);
    *out_y = u->y + units_sign_i(gy - u->y);
    return true;
  }

  /*
   * FUN_6662_0f74 (viceroy_unpacked.c:104535-104660), the tier dispatch:
   *   near (|dx|<7 && |dy|<7): 0015bc flood toward the goal; success returns.
   *   LAB_10e1 (far, or near-flood miss): 0b4e (`0015c1`) coarse waypoint;
   *     when it fails after a near miss the flood is skipped, otherwise
   *     0015bc runs toward DS:0xa14e/0xa14c — the waypoint, or the goal
   *     itself when 0b4e left it alone; on a miss, once more toward the
   *     probed centre of the unit's own coarse cell (DS:0xa572/0xa574).
   *   a far-flood hit is dropped again when the unit already moved this
   *     turn (unit+0x3149) and the step is the exact reverse of its last
   *     one (unit+0x314f ^ 4) — for Indian nations (> 3) the step is still
   *     returned; Euro nations fall through.
   *   nation > 3 (Indian): return the result as is (-1 on failure);
   *   Euro nations: the scored 8-neighbour fallback (units_greedy_next_step).
   * Not ported: the `unit+0x314b != '9'` letter gate on the reversal check
   * (orders letter unidentified); DOS reads DS:0xa572/0xa574 stale when the
   * unit snap failed — Linux skips that flood instead.
   */
  const bool near_tier = adx <= 6 && ady <= 6;
  bool near_missed = false;
  if (near_tier) {
    if (units_flood_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y)) {
      return true;
    }
    near_missed = true;
  }

  /*
   * DS:0xa14e / DS:0xa14c — the tile the tiers last aimed the flood at. The
   * scored 8-neighbour fallback takes its deltas from THIS pair, not from
   * the goal (FUN_6662_0f74 raw 104623-104627: `uVar10 = *(int *)0xa14c -
   * uVar7; uVar11 = *(int *)0xa14e - uVar6;`), so on a long trip the greedy
   * tier steers to the coarse waypoint. bugs.md #698. The near tier writes
   * the goal into the pair (raw 104581-104582), which is the default here.
   */
  int scored_x = gx;
  int scored_y = gy;

  {
    const int sea = units_is_sea(pool, unit_id) ? 1 : 0;
    int wx = 0;
    int wy = 0;
    int ucx = -1;
    int ucy = -1;
    /* UNITS_FAR_BFS=1: diagnostic fallback to the pre-2026-08-27 whole-map BFS tier. */
    static int s_far_bfs = -1;
    if (s_far_bfs < 0) {
      const char* e = getenv("UNITS_FAR_BFS");
      s_far_bfs = (e && e[0] == '1') ? 1 : 0;
    }
    if (s_far_bfs && units_bfs_next_step(pool, unit_id, map, colonies, gx, gy, out_x, out_y)) {
      return true;
    }
    (void)units_bfs_next_step;
    const int have_wp = units_coarse_waypoint(map, u->x, u->y, gx, gy, sea, &wx, &wy, &ucx, &ucy);
    if (have_wp || !near_missed) {
      const int tx = have_wp ? wx : gx;
      const int ty = have_wp ? wy : gy;
      scored_x = tx;
      scored_y = ty;
      bool hit = units_flood_next_step(pool, unit_id, map, colonies, tx, ty, out_x, out_y);
      if (!hit && ucx >= 0) {
        int fx = ucx * 4 + 1;
        int fy = ucy * 4 + 1;
        (void)units_coarse_probe(map, ucx, ucy, sea, &fx, &fy);
        /* raw 104596-104598 overwrites DS:0xa14c/0xa14e with the probed
         * cell centre before the retry flood. bugs.md #698. */
        scored_x = fx;
        scored_y = fy;
        if (fx != u->x || fy != u->y) {
          hit = units_flood_next_step(pool, unit_id, map, colonies, fx, fy, out_x, out_y);
        }
      }
      if (hit) {
        const bool moved_this_turn =
          units_remaining_mp(pool, unit_id) < units_max_mp(pool, unit_id);
        const bool reversal =
          moved_this_turn &&
          units_dir8_index(*out_x - u->x, *out_y - u->y) == units_goto_last_dir_reverse(unit_id);
        if (!reversal || u->nation_id > 3) {
          return true;
        }
      }
    }
  }
  if (u->nation_id > 3) {
    return false;
  }
  return units_greedy_next_step(
    pool, unit_id, map, colonies, rng, scored_x, scored_y, gx, gy, out_x, out_y
  );
}


bool units_advance_goto_one_step_w(
  const ColonizeWorld* w,
  int unit_id
) {
  ColonizeUnitPool* pool = w->units;
  const ColonizeWorldMap* map = w->map;
  const ColonizeColonyPool* colonies = w->colonies;

  ColonizeUnit* u = units_get(pool, unit_id);
  if (!u || !u->active || !map) {
    return false;
  }
  if (!units_orders_follow_goto(u->orders)) {
    return false;
  }
  if (!units_is_on_map(u)) {
    return false;
  }
  const int gx = u->goto_x;
  const int gy = u->goto_y;
  if (gx < 0 || gy < 0 || gx >= UNITS_GOTO_NONE || gy >= UNITS_GOTO_NONE) {
    if (u->orders != UNITS_ORDER_TRADE_ROUTE) {
      units_clear_orders(pool, unit_id);
    }
    return false;
  }
  if (u->x == gx && u->y == gy) {
    /* TRADE_ROUTE: stay ordered at stop so caller can advance to next dest. */
    if (u->orders != UNITS_ORDER_TRADE_ROUTE) {
      units_clear_orders(pool, unit_id);
    }
    return false;
  }
  if (units_remaining_mp(pool, unit_id) <= 0) {
    return false;
  }
  const int ox = u->x;
  const int oy = u->y;
  int nx = -1;
  int ny = -1;
  if (!units_next_goto_step_w(w, unit_id, &nx, &ny)) {
    /*
     * DOS-LITERAL FUN_479b_0972 raw 77066-77072 + FUN_6662_0f74 raw
     * 104703/104738 (bugs.md #696). A pathfinder miss is terminal in DOS,
     * not a silent no-op: 0f74 itself calls FUN_281f_0934 (spend the whole
     * allotment) on both its failure exits — the "no candidate scored"
     * one (`if (local_1c != 8) goto LAB_6662_1599; FUN_281f_0934(...)`) and
     * the reversal-reroll one — and the caller then clears +0x314c because
     * the returned direction is < 0 or == 8. Only order 0x02 (TRADE_ROUTE)
     * survives, and then only on the ==8 flavour. Without this the order
     * stayed on the unit forever and the pacer re-polled it every frame.
     */
    ColonizeUnit* miss = units_get(pool, unit_id);
    if (miss) {
      units_mp_exhaust(pool, miss);
      if (miss->orders != UNITS_ORDER_TRADE_ROUTE) {
        units_clear_orders(pool, unit_id);
      }
    }
    return false;
  }
  /*
   * A goto must never turn into an attack by a unit that cannot fight
   * (@UNIT attack 0: Pioneers, Colonists, Wagon Trains, unarmed transports).
   * Stepping onto a foreign unit, or onto a native village -- which
   * units_try_move resolves as an attack even with no unit visible on the tile
   * -- killed AI settlers walking to their colony site, and would do the same
   * to a human unit under goto orders. Stop the order instead.
   */
  {
    const ColonizeUnitType* mt = units_type(pool, u->type_index);
    const int missionary = units_type_is_missionary(mt);
    const int occ = units_id_at(pool, nx, ny);
    const ColonizeUnit* of = occ >= 0 ? units_get_const(pool, occ) : NULL;
    const int village = units_tribe_nation_at(g_units_ff_col1, nx, ny);
    const int step_is_dest = (nx == gx && ny == gy);
    /* Smell #105: an EMPTY foreign colony tile is an attack too (DOS 465b
     * sets the attack flag from the settlement owner alone, viceroy
     * 75467-75482) — without this the pacer walked through/onto one and
     * units_try_move captured it silently, skipping every confirm. Treat
     * it exactly like a foreign occupant below. */
    int foreign_colony = 0;
    if (!of && colonies) {
      const int ccid = colonies_id_at(colonies, nx, ny);
      const ColonizeColony* ccol = ccid >= 0 ? colonies_get(colonies, ccid) : NULL;
      foreign_colony = ccol && ccol->active && ccol->nation_id != u->nation_id;
    }
    /*
     * bugs.md: a goto must NEVER open combat en route — a scout pathing
     * past a Brave was auto-attacking it. Any foreign occupant (or native
     * village) on an intermediate step tile halts the step instead; only
     * the DESTINATION tile may be attacked, and then only by a unit that
     * can fight (@UNIT attack 0: Pioneers, Colonists, Wagon Trains,
     * unarmed transports stop even at the destination).
     */
    if ((of && of->nation_id != u->nation_id) || foreign_colony ||
        (village >= 4 && u->nation_id >= 0 && u->nation_id <= 3)) {
      if (!step_is_dest) {
        return false;
      }
      /*
       * A go-to AIMED at an Indian settlement ends here: this raw pacer has
       * no popup channel, and units_try_move alone would resolve the village
       * tile as a silent attack. Clear the order and stop.
       *
       * The HUMAN side no longer sees this: game_loop's activation pacer
       * intercepts the adjacent-to-destination frame and hands the final
       * step to game_try_unit_move, which raises the real FUN_4d56_4528
       * entry flow (woodcut 7 + @ACTIONS menu) — bugs.md #418, superseding
       * 293's "stop one tile short". This arm is the AI/headless backstop.
       */
      if (village >= 4) {
        units_clear_orders(pool, unit_id);
        return false;
      }
      if (mt && mt->attack <= 0 && !missionary) {
        return false;
      }
    }
  }
  if (!units_try_move_w(w, unit_id, nx, ny)) {
    return false;
  }
  u = units_get(pool, unit_id);
  if (u) {
    /* FUN_6662_0f74's own tail writes unit+0x314f (last_dir) on every step
     * it commits — feeds the anti-backtrack wiggle check above. Tracked in
     * s_units_goto_last_dir, not ColonizeUnit.last_dir (see that array's
     * own header comment for why). */
    units_note_goto_step(unit_id, nx - ox, ny - oy);
    /*
     * DOS-LITERAL FUN_479b_0972 raw 77136-77142 (bugs.md #697), the arrival
     * tail after a committed step:
     *   if (+0x314c == '\v')  FUN_281f_0934(unit);   // AI_SAIL: MP exhaust
     *   if (+0x314c == '\x02' || +0x314c == '\f') keep the order;
     *   else +0x314c = 0;
     * i.e. AI_MOVE (12) survives arrival exactly like TRADE_ROUTE (2) — its
     * holder is re-aimed by the 20e6/0a60 goal machinery, not by this walk —
     * and an arriving AI ship ends its turn on the spot.
     */
    if (u->x == gx && u->y == gy) {
      if (u->orders == UNITS_ORDER_AI_SAIL) {
        units_mp_exhaust(pool, u);
      }
      if (u->orders != UNITS_ORDER_TRADE_ROUTE && u->orders != UNITS_ORDER_AI_MOVE) {
        units_clear_orders(pool, unit_id);
      }
    }
  }
  return true;
}


bool units_advance_goto_w(
  const ColonizeWorld* w,
  int unit_id
) {

  bool moved = false;
  while (units_advance_goto_one_step_w(w, unit_id)) {
    moved = true;
  }
  return moved;
}
