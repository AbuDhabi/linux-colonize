/*
 * Euro AI — ai_euro_unit_act stage functions (ai_euro_act_*) and the DOS ship-act path
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Sections:
 *   FUN_5952_035e equip pick (shared with the goals equip arm)
 *   Ship Europe exit (europe_exit)
 *   Land band stages: hunt_scout, treasure, roles, goal_consume, goal_dispatch
 *   ai_euro_act_land band driver
 *   DOS ship-act path: 09dc gate, 20e6 ship dos, 479b goal walk
 */

#include "core/internal.h"
#include "core/ai_euro.h"

#include "core/ai.h"
#include "core/ai_internal.h"
#include "core/ai_contact.h"
#include "core/ai_diplo.h"
#include "core/ai_king.h"
#include "core/ai_goals.h"
#include "core/assets.h"
#include "core/ai_euro_internal.h"
#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_yield.h"
#include "core/colony_production.h"
#include "core/col1_save.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/units.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * FUN_5952_035e equip-arm candidate scorer (raw 94318-94345; annotated
 * colony_tick_5952_035e.md:667-691). DOS-LITERAL. `target` is local_1b4,
 * i.e. local_8e with 0x17 (Dragoon) mapped to 0x15 (Soldier), so it is
 * always 0x15 at the one live call site.
 *
 *   score = 0
 *   prof == target                       -> score = 4
 *   else if !FUN_281f_0c9a(prof)         -> score += 1      (not an expert)
 *   else if target == 0x15 && !(+0x1b & 0x40) -> score = 0xff9d  (= -99)
 *   prof == 0x19 (Indentured Servant)    -> score += 1
 *   prof == 0x1a (Petty Criminal)        -> score += 2
 *   prof != 0x1b (Indian Convert) && best <= score -> take it
 *
 * `local_16e` starts at 0xffff and is compared as a signed 16-bit int, so
 * best starts at -1; `<=` means later ties win. Returns the colonist slot,
 * or -1 when every slot is an Indian Convert (or the colony is empty).
 */
int ai_euro_5952_equip_pick(const ColonizeColony* c, int target) {
  if (!c) {
    return -1;
  }
  int best = -1;
  int pick = -1;
  const int pop = (int)c->population;
  for (int i = 0; i < pop && i < COLONIZE_COLONY_POP_MAX; ++i) {
    const int prof = (int)c->colonists[i].profession;
    int score = 0;
    if (prof == target) {
      score = 4;
    } else if (!ai_euro_5952_job_is_expert(prof)) {
      score += 1;
    } else if (target == UNITS_JOB_SOLDIER &&
               (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_GARRISON) == 0) {
      score = -99;
    }
    if (prof == UNITS_JOB_SERVANT) {
      score += 1;
    }
    if (prof == UNITS_JOB_CRIMINAL) {
      score += 2;
    }
    if (prof != UNITS_JOB_CONVERT && best <= score) {
      best = score;
      pick = i;
    }
  }
  return pick;
}

/*
 * Ship stage 1: Europe treasure cash-in / dump-sell, then FUN_48d3_048e
 * Europe->map exit and its first scored ocean leg.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_ship_europe_exit(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;

  /* (The Europe/HS treasure cash-in that stood here was deleted 2026-09-23,
   * bugs.md #746 — DOS's AI never puts a treasure on a ship.) */
  /*
   * No Europe-dock dump-sell in the per-unit act. The "sell every hold at the
   * Europe dock" arm (ai_euro_try_transport_europe_sell) carried no DOS
   * citation — only the manual and the wiki Boycott page — and was deleted
   * 2026-09-18. It is not needed: DOS's Europe dock seller is the nation-level
   * FUN_521d_5d04 pass (viceroy_overlays.c:83168-83192), ported as the
   * `ai_euro_5d04_cb_sell_hold0` / `_cb_reward_case` loop, which empties the
   * holds of every ship of type 0x0d..0x12 sitting in Europe — Privateers and
   * Frigates included. The other two ported sellers are FUN_521d_20e6's
   * delivery sell tail (raw 2140-2163, ai_euro_20e6_delivery_sell_tail) when
   * no colony will take the delivery cargo, and FUN_364b_0688 phase O
   * (viceroy_unpacked.c 57806-57848, europe_ai_colony_dump_sell_w) which
   * sells the warehouse surplus a ship dumped into the colony.
   */

  /* Arrivals (lane 224+n) are placed by the FUN_48d3_06ba tail;
   * departures run inside 5d04 before the unit waves. A hull still at
   * the dock here failed that pass's departure gate (e.g. damage). */
  const int exited_europe = 0;
  if (ai_euro_in_europe(u->x, u->y)) {
    u->moves = 0;
    return AI_EURO_ACT_RETURN;
  }

  a->exited_europe = exited_europe;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 1: LCR on entry. (The land-war engage/hunt and scout arms that
 * stood here went with the #530 fit layer: only Treasure / Missionary /
 * Wagon reach this band; military and Scouts take the DOS 20e6 gate + 479b
 * walk in ai_euro_unit_act.)
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_hunt_scout(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  int scout_explored = a->scout_explored;

  /*
   * LCR (FUN_65dd_0004 thin transcription): any land unit standing on a
   * rumour clears it and rolls a manual outcome — Scouts get a better-
   * weighted table (units_resolve_lcr_rumour), de Soto keeps outcomes
   * positive. AI nations have no modeled EuropeScreen recruit pool, so
   * Fountain of Youth is a no-op for them (see units_resolve_lcr_rumour).
   * Not gated on is_scout: that was this port's own over-restriction
   * (player-caught) — any AI land unit walking onto an LCR triggers it in
   * DOS, Scout is just better at it.
   * Cite: units_resolve_lcr_rumour; Colonization.pdf Lost City Rumours.
   */
  if (ctx->map && map_tile_has_rumour(ctx->map, u->x, u->y)) {
    if (units_resolve_lcr_rumour_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL), .rng=(ColonizeDosRng*)(ctx->rng), .europe=(EuropeScreen*)(NULL)}, u->id, -1)) {
      scout_explored = 1;
    }
    /* Vanish / hostile-burial outcomes may have despawned the scout. */
    if (!u->active) {
      return AI_EURO_ACT_RETURN;
    }
  }



  a->scout_explored = scout_explored;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 2: Treasure Train routing (coast / board / cash) and the
 * 20e6 47b9 dead-end bail.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_treasure(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  const int is_treasure = a->is_treasure;
  int treasure_routed = a->treasure_routed;

  /*
   * DOS-LITERAL FUN_521d_20e6 raw 89997-90040 — the whole (and only) AI
   * treasure band, in DOS order:
   *   arm 1  iStack_2e == 0 (standing on ANY own colony): cash
   *          `+0x315b * 100` into the nation purse untaxed, @LOOTFOREIGN
   *          popup while (DS:0x5382 & 1) == 0, then LAB_47b9 destroy.
   *   arm 2  else nearest own colony on this landmass
   *          (iStack_2c == iStack_38) → FUN_281f_09e6 bind + LAB_4701/4567
   *          walk to it.
   *   arm 3  else nearest own unit on this landmass (FUN_281f_08a8) →
   *          LAB_27f5 walk to it; else, when the adjacent-claim probe names
   *          the human (DS:0x5398), LAB_47b9 destroy.
   * Arms 2/3 live in ai_euro_20e6_47b9_dead_end below.
   *
   * The coast-target / board-and-sail / Europe-cash chain that stood between
   * arm 1 and the rest was deleted 2026-09-23 (bugs.md #745/#746): no DOS
   * site boards an AI treasure, prefers a coastal colony, or gives the AI
   * Cortes' free King galleon (FUN_465b_0000 raw 75800 is human-control
   * gated, bugs.md #478).
   */
  if (is_treasure) {
    if (ai_euro_20e6_treasure_cash_in(ctx, u, nation_id)) {
      return AI_EURO_ACT_RETURN;
    }
  }
  /*
   * LAB_521d_457e/47b9 treasure dead end: nothing above bound a course and the
   * treasure is cut off from every own colony and own unit on its landmass —
   * DOS destroys it. Wagons and Treasure defer the 20e6 gate (see the
   * defer_gate list), so this band runs act-level here instead.
   */
  if (is_treasure && !treasure_routed) {
    const int r = ai_euro_20e6_47b9_dead_end(ctx, u, nation_id);
    if (r == 2) {
      return AI_EURO_ACT_RETURN; /* destroyed */
    }
    if (r == 1) {
      treasure_routed = 1; /* LAB_4701 / LAB_27f5 course bound */
    }
  }

  /* The band's next arm in DOS order: the Missionary village pick. */
  if (!is_treasure && ai_euro_20e6_missionary_arm(ctx, u, nation_id)) {
    treasure_routed = 1; /* LAB_27f5 course bound: the walker steps it this act */
  }
  /* 20e6 returned with a bound course: FUN_521d_5b66 cases 0x0b/0x0c walk it
   * with FUN_479b_0972 (arrival on a 0x0b goal exhausts MP, raw 77098). */
  if (treasure_routed && (u->orders == AI_EURO_ACT_GOAL || u->orders == AI_EURO_ACT_STEP)) {
    ai_euro_goal_walk_479b(ctx, u);
    return AI_EURO_ACT_RETURN;
  }

  a->treasure_routed = treasure_routed;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 3: wagon haul, Pioneer improve job, the six field-expert
 * admit/assign arms, and the indoor expert workplace assign.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_roles(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  int scout_explored = a->scout_explored;
  int treasure_routed = a->treasure_routed;
  const ColonizeUnitKind ukind = a->ukind;

  /*
   * Wagon Train haul (act-level): idle Wagon with hold capacity or TOOLS /
   * LUMBER / ORE / MUSKETS / HORSES / FOOD → AI_MOVE toward matching short
   * colony (unload via existing delivery). Cite: euro_unit_act §2d;
   * Colonization.pdf Wagon Train; 5cf6 food_short.
   */
  int wagon_hauled = 0;
  if (!treasure_routed && ai_euro_type_is_wagon_name(ukind) &&
      !ai_euro_land_is_fortified(u)) {
    if (ai_euro_try_wagon_haul(ctx, nation_id, u)) {
      wagon_hauled = 1;
    }
    /* (The wagon Europe-export feeder — a thin OpenCol-only arm — was retired
     * 2026-09-07b: DOS's 457e origin walk owns every off-errand wagon beat,
     * so wagons never fed the coastal export leg in DOS. Surplus reaches
     * Europe through the ships-only 4393 pickup queue.) */
    /* The wagon 47b9 destroy lives inside the origin walk now (the haul beat
     * above owns the whole 457e wagon arm) — stop touching a despawned unit. */
    if (!u->active) {
      return AI_EURO_ACT_RETURN;
    }
  }

  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->wagon_hauled = wagon_hauled;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 4 (ai_euro_act_land_fortify) is gone: every arm it held was
 * invented and has been deleted. Tombstones:
 *
 * (Deleted 2026-10-02, bugs.md #1032.) A "peace fortify" arm stood here:
 * any idle armed land unit on its own colony tile was FORTIFY-ed and spent
 * one garrison_quota. It cited FUN_521d_20e6 raw 89011-89013, but that
 * `+0x314b = 0x46` write also needs local_ea != 0, the unit alone on its
 * tile and a foreign neighbour -- it is the border park, ported as
 * ai_euro_20e6_border_park_arm. DOS's own-colony garrison is the LAB_5899
 * arm (raw 88584-88612, ai_euro_20e6_land_arms: 'G', spends colony +0x8e,
 * not +0x1e), and the fortify order itself comes from the shared stay tail
 * (raw 90378-90386, ai_euro_20e6_stay_tail_589e). Gated off it moved
 * nothing in golden_ai_turns / golden_ai_joint; the unit tests that pinned
 * it (manual-cited) went with it. The "peace fortify fallback" (military
 * name + ai_euro_fortify_with_quota) in the goto-advance else branch went
 * too, same reasons.
 *
 * No Artillery-specific siege hunt. DOS has no artillery target band: in
 * FUN_521d_20e6 type 0x0b appears exactly once in the shared tile scorer
 * (raw 88911-88913) as a score *modifier* — `type == 0x0b && no colony and
 * no village on the tile -> score = 0` — and FUN_465b_0000 (raw 75417) is
 * type-agnostic. The "Artillery siege hunt" arm that used to sit here cited
 * only Colonization.pdf / king_ref and was deleted (bugs.md #513).
 *
 * (Retired 2026-09-23, bugs.md #760.) An "Artillery fortify" arm sat here:
 * idle Artillery on its own colony was FORTIFY-ed through the garrison quota.
 * Manual-only citation (euro_unit_act §2d3 / Colonization.pdf / king_ref) and
 * FUN_521d_20e6 has no `+0x3146 == 0x0b` branch in the act path at all
 * (raw 88266-90445). DOS's own-colony arm is the type-agnostic LAB_5899
 * (raw 88584-88612, ported in ai_euro_20e6_land_arms): attack > 1, type
 * outside [0xd,0x12], not 4, not 8 → labor_shortage--, order_code = 0x47,
 * stay. Artillery satisfies every one of those tests, so the DOS path already
 * handles exactly the case this arm was covering. Same invention class as the
 * already-deleted "Artillery siege hunt" above.
 *
 * (Retired 2026-09-22, bugs.md #557.) A missionary CONTACT goal arm sat
 * here (prio 3 goto of the nearest mission-less tribe). The claim that
 * FUN_521d_20e6 has no `+0x3146 == 3` arm was wrong: the DOS arm is
 * ai_euro_20e6_missionary_arm (alarm-weighted village pick, plan 'J'),
 * called from the treasure stage; arrival is FUN_4d56_4528's non-human
 * switch (ai_contact_ai_missionary_village).
 */

/*
 * Land stage 5: FUN_521d_0a60 goal-consumption tail — read the committed
 * primary-goal pick back, else run the founder/labor fallback scan.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_goal_consume(struct ai_euro_act_ctx* a) {
  ColonizeUnit* u = a->u;

  /*
   * FUN_521d_0a60 goal-consumption tail, structurally ported (see
   * ai_euro_0a60_goal_orders_structural above): runs once per nation per turn
   * and commits its pick into the unit's own +0x314c/+0x314d/e (bugs.md
   * #525). `ai_euro_unit_act` captures it before the 20e6 gate can overwrite
   * the byte with a 0x0c wander commit; the concrete AI_GOAL_* code has no
   * DOS byte at all and is re-read from the goal table at the goal tile.
   */
  if (a->goal_code < 0) {
    a->goal_x = u->goto_x;
    a->goal_y = u->goto_y;
  }
  /*
   * A "threatened-Stockade LABOR override" stood here until 2026-09-18: a
   * war-threat proximity scan that re-aimed a Free Colonist at any own
   * colony building a Stockade. It had no DOS counterpart — the goal table
   * is written only by FUN_521d_0a60 (its 'A' mark block spends colony
   * +0x1e garrison_quota / +0x8e labor_shortage, neither of which is a
   * building-choice term) and the one AI construction picker is the
   * FUN_5952_035e cascade, which selects a Stockade from the colony's own
   * +0x1b flags, never from an adjacent enemy. Deleted with its helpers.
   */

  /*
   * (Deleted 2026-10-02, bugs.md #1034(b).) A "LABOR bind" stood here: idle
   * colonist-capable land units were re-aimed at an own colony (MD 1, or 8 on
   * food_short/tools_short) whose food < pop*2 / tools < 20 / pop < 3 or that
   * was building a Stockade/Warehouse/Lumber Mill, with an AI_GOAL_LABOR
   * primary upsert. Its cites were the manual / Skills Chart only; DOS writes
   * goals only in FUN_521d_0a60 and does its labor routing in FUN_521d_20e6's
   * type-0 labor arm. Gated off it moved no golden.
   */

  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land stage 6: act on the bound goal — FOUND on arrival, wagon Europe
 * sell, LABOR/COLONY admit, MILITARY/CONTACT engage, goto commit + move
 * drain, and the adjacent seize/attack tail.
 */
COLONIZE_INTERNAL AiEuroActStatus ai_euro_act_land_goal_dispatch(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;
  const int nation_id = a->nation_id;
  int goal_code = a->goal_code;
  int goal_x = a->goal_x;
  int goal_y = a->goal_y;
  int scout_explored = a->scout_explored;
  int treasure_routed = a->treasure_routed;
  int wagon_hauled = a->wagon_hauled;

  if (goal_code == AI_GOAL_FOUND && u->x == goal_x && u->y == goal_y) {
    ai_euro_found_with_unit(ctx, u, nation_id);
    return AI_EURO_ACT_RETURN;
  }

  /*
   * No tools-delivery / Europe-sell arms here. The "wagon or Pioneer on an own
   * colony tile unloads what the colony is short of" pair
   * (ai_euro_try_wagon_tools_delivery + ai_euro_try_pioneer_tools_delivery)
   * and the wagon Europe dump-sell were manual-cited inventions, deleted
   * 2026-09-18. DOS wagon delivery is the FUN_521d_20e6 arrival block
   * (raw 3007-3012: dump EVERY hold into the bound colony, then the load
   * matrix) in ai_euro_try_wagon_haul — never a per-cargo shortage filter —
   * and a wagon never reaches Europe at all.
   */

  /*
   * No LABOR/COLONY "arrive on the colony tile → join as a colonist" arm.
   * DOS has no such unit-act outcome: FUN_521d_20e6's complete +0x314b
   * vocabulary over raw 88266-89800 is 0x39/0x3d/0x40/0x42/0x46/0x47/0x4c/
   * 0x56/0x65 (no join), and FUN_521d_0a60's own-colony block only marks
   * standing military 'A' (0x41) — raw 87683-87756, ported as
   * ai_euro_colony_goals_colony_garrison. That mark is bookkeeping: it
   * decrements labor_shortage (+0x8e) and garrison_quota (+0x1e) and hides
   * the unit from this turn's goal scan; it never moves it into a colonist
   * slot. Note the DOS polarity — garrison_quota is a COUNTER the admission
   * spends, never a `== 0` gate. The actual absorption of a unit standing
   * on its own colony tile is FUN_5952_035e's colony-tick tile re-scan
   * (raw 94231-94274, ported as ai_euro_5952_absorb_equip and called from
   * ai_euro_colony_goals_colony_labor), which owns both the per-type gates
   * and the NEEDS_COLONISTS (+0x1b bit 0x10) precondition. The
   * `garrison_quota == 0` admit that used to sit here cited only
   * test-saves-ai/TURN4-5 and was deleted (sibling of bugs.md #512/#515).
   */
  if (goal_code == AI_GOAL_MILITARY || goal_code == AI_GOAL_CONTACT) {
    if (abs(u->x - goal_x) <= 1 && abs(u->y - goal_y) <= 1) {
      /* Exclude self: a stale goal can point at a tile the unit itself now
       * occupies (e.g. just captured it opportunistically) — nothing to
       * attack there. */
      const int foe = units_id_at(ctx->units, goal_x, goal_y);
      if (foe >= 0 && foe != u->id) {
        ai_euro_try_attack(ctx, u, goal_x, goal_y);
        return AI_EURO_ACT_RETURN;
      }
    }
    if (ctx->colonies && u->x == goal_x && u->y == goal_y) {
      const int cid = colonies_id_at(ctx->colonies, goal_x, goal_y);
      if (cid >= 0) {
        ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
        if (c && c->nation_id != nation_id &&
            units_foreign_unit_at(ctx->units, u->x, u->y, u->id, nation_id) < 0) {
          int plunder = 0;
          for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
            if (c->stock[i] > 0) {
              plunder += c->stock[i];
            }
          }
          ColonizeColony snap = *c;
          if (colonies_capture(ctx->colonies, cid, nation_id)) {
            units_combat_notify_colony_captured(
              ctx->col1_ok ? ctx->col1 : NULL, &snap, nation_id, plunder
            );
          }
          return AI_EURO_ACT_RETURN;
        }
      }
    }
  }

  /* Preserve land-war / peace-border / scout / treasure / missionary / wagon /
   * pioneer-improve / LABOR. */
  if (goal_code >= 0 && !scout_explored &&
      !treasure_routed && !wagon_hauled) {
    ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, goal_x, goal_y);
  }

  /*
   * Land goto advance (thin 20e6 multi-step): scored steps while moves
   * remain for FOUND / MILITARY / CONTACT, or act-level land war hunt /
   * peace-border / scout explore. Structural only — not full combat scoring.
   * Cite: euro_unit_act §2c3; FUN_521d_20e6. Deep combat×8 / −0x6790 PARKED.
   */
  if (units_orders_follow_goto(u->orders)) {
    const int drain =
      (goal_code == AI_GOAL_FOUND || goal_code == AI_GOAL_MILITARY ||
       goal_code == AI_GOAL_CONTACT || goal_code == AI_GOAL_ESCORT || scout_explored);
    /* drain: while MP left; else one scored step (prior non-multi path). */
    for (;;) {
      if (!u->active || u->moves <= 0 || !units_orders_follow_goto(u->orders)) {
        break;
      }
      if (u->x == u->goto_x && u->y == u->goto_y) {
        break;
      }
      int dx = 0;
      int dy = 0;
      /*
       * An adjacent goto holding a foreign unit is the LAB_4d2e attack pick
       * (the scorer commits the neighbour tile itself as +0x3153/+0x3154 and
       * FUN_479b_0972 → FUN_465b_0000 steps straight into it). Re-scoring
       * that step with ai_euro_score_move sidestepped diagonally around the
       * foe and the fight never happened (bugs.md #521, 2026-09-22).
       */
      const int adj_foe = abs(u->goto_x - u->x) <= 1 && abs(u->goto_y - u->y) <= 1 &&
                          ai_euro_foreign_unit_at(ctx, u, u->goto_x, u->goto_y);
      if (adj_foe) {
        dx = u->goto_x - u->x;
        dy = u->goto_y - u->y;
      } else if (!ai_euro_score_move(ctx, u, u->goto_x, u->goto_y, &dx, &dy)) {
        break;
      }
      const int tx = u->x + dx;
      const int ty = u->y + dy;
      /* Same best-defender rule as the step scorer above: a berthed hull on
       * the tile must not read as "empty" (FUN_5fef_0000 / FUN_465b_0000). */
      int foe = units_best_defender_at(
        ctx->units, ctx->col1_ok ? ctx->col1 : NULL, tx, ty, u->id, u->id
      );
      if (foe < 0) {
        foe = units_id_at(ctx->units, tx, ty);
      }
      /*
       * Only a FOREIGN occupant makes the step a fight. FUN_465b_0000 reaches
       * its combat tail only when the destination's owner nibble differs from
       * the mover's nation (raw 75417ff); a tile holding one of our own units
       * is an ordinary stacking move. The raw `units_id_at` fallback above has
       * no nation filter, so a second REF column standing on the step tile was
       * handed to ai_euro_try_attack — which returns without acting on an
       * own-nation target — and this loop broke every turn: the whole assault
       * column froze two tiles short of the colony (bugs.md #521). The step
       * scorer's own copy of this fallback (ai_euro_score_move) already
       * filtered on nation; this one did not.
       */
      if (foe >= 0) {
        const ColonizeUnit* fu = units_get_const(ctx->units, foe);
        if (!fu || fu->nation_id == u->nation_id) {
          foe = -1;
        }
      }
      if (foe >= 0) {
        ai_euro_try_attack(ctx, u, tx, ty);
        break;
      }
      ColonizeWorld w_ = world_make(ctx->units, ctx->colonies, ctx->map, NULL, false, ctx->rng, NULL);
      if (!units_try_move_w(&w_, u->id, tx, ty)) {
        break;
      }
      if (!drain) {
        break; /* single step for non-FOUND/MILITARY/CONTACT/hunt/scout */
      }
    }
  }


  /* The "sticky CONTACT re-hunt" tail that used to sit here is folded into the
   * block above (smell audit sweep-3 area C #1): zero hits when instrumented
   * over the whole ctest suite. The two seizes are the real DOS `0x46` / `0x4c`
   * arms; the adjacent-attack stand-in that also ran here was deleted 2026-09-22
   * (bugs.md #521, LAB_521d_4d2e attack term is DOS-live). The distant land war hunt that
   * also ran here was deleted 2026-09-18: DOS has no distant hunt for land
   * units any more than it has one for ships. */

  a->goal_code = goal_code;
  a->goal_x = goal_x;
  a->goal_y = goal_y;
  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->wagon_hauled = wagon_hauled;
  a->u = u;
  return AI_EURO_ACT_CONTINUE;
}

/*
 * Land band driver (case 0x0b). Binds the per-unit role flags the stages
 * share, then runs the land stages in DOS order.
 */
void ai_euro_act_land(struct ai_euro_act_ctx* a) {
  ColonizeTurnContext* const ctx = a->ctx;
  ColonizeUnit* u = a->u;

  /* Treasure / Missionary / Wagon only (see ai_euro_unit_act). */
  const char* uname = units_display_name(ctx->units, u);
  const ColonizeUnitKind ukind = ai_euro_unit_kind(ctx->units, u);
  const int is_treasure = ai_euro_is_treasure_name(ukind);
  int scout_explored = 0;
  int treasure_routed = 0;

  a->uname = uname;
  a->ukind = ukind;
  a->is_treasure = is_treasure;
  a->scout_explored = scout_explored;
  a->treasure_routed = treasure_routed;
  a->u = u;

  if (ai_euro_act_land_hunt_scout(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_treasure(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_roles(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_goal_consume(a) == AI_EURO_ACT_RETURN) {
    return;
  }
  if (ai_euro_act_land_goal_dispatch(a) == AI_EURO_ACT_RETURN) {
    return;
  }
}

/*
 * FUN_521d_5b66 — scoring gate + case 0x0b arms; case 7 hire economy thin
 * (Pioneer tools-delivery here; wagon/tools dock hire lives in 5d04 planning).
 *
 * Correction (2026-08-13, see euro_unit_act.md): `FUN_521d_5b66` itself is a
 * tiny 198-byte `switch(unit.orders_or_state)` dispatcher (cases
 * 7/8/9/0xb/0xc/default calling out to `FUN_1000_93ea` / `func_0x000193b2` /
 * `FUN_1000_9406` / `FUN_1000_8b24` / `FUN_1000_96aa`), not the ~1815-line
 * body this file's "5b66 case N" comments were written against — that
 * estimate came from a Ghidra disassembly-fault-corrupted read of the
 * canonical export (real root cause: false adjacency to the next RTLink
 * overlay segment in the flattened file, see `docs/rtlink_decode_v2_gap.md`).
 * The case *numbers* below are still meaningful — they match the real
 * dispatcher's cases 1:1 — but the elaborate bodies live in the callees
 * above, not literally inside `5b66`. Re-attributing each "5b66 case N"
 * comment throughout this function to its real DOS home is not done (large,
 * mostly cosmetic given the case-shape framing still holds); treat "5b66
 * case N" comments here as "the game behavior DOS dispatches via 5b66's
 * case N", not "literally transcribed from 5b66's own bytes".
 */
/* ======================================================================
 * DOS ship act: FUN_521d_5b66 + the FUN_521d_20e6 hull path (bugs.md #530)
 * ======================================================================
 *
 * FUN_521d_5b66 (viceroy_overlays.asm OVL14 0x5b66-0x5c37, byte-read):
 *   if (+0x3149 != 0 && +0x314c == 0x0b) {
 *     if (!(DS:0x523d[type] & 1)) goto walk;          // transports never re-score
 *     if (!FUN_1000_8b74(x, y, nation)) goto walk;    // warship, no foreigner adjacent
 *     if (+0x314b == 'E') DS:0x9456[nation]--;
 *   }
 *   if (FUN_521d_20e6(unit)) return;
 *   walk: switch (+0x314c) { 7 found; 8 plow; 9 road;
 *                            0x0b, 0x0c: FUN_479b_0972 (one pathfinder step);
 *                            default: FUN_1000_8b24 exhaust MP }
 *
 * The 20e6 hull path, in DOS order (raw 88400-90404; ships take the
 * LAB_277a -> LAB_2912 -> LAB_304c chain straight to LAB_3558 at sea, or the
 * berth block at an own colony):
 *   entry bail (raw 88402-88405) -> LAB_3558 unload mask + unload loop
 *   (raw 89440-89612) -> colony-sail pick (raw 89614-89711) -> goods
 *   delivery / sell / Europe (raw 89717-89872) -> LAB_4393 work-queue haul
 *   -> LAB_457e (tasked hull bails; High Seas cadence) -> pre-LAB_4d2e gate
 *   (raw 90210-90219) -> LAB_4d2e ship far roam (raw 88632-88664) and the
 *   8-direction scorer -> LAB_589e commit (0x0c one step, or 5/6 stay)
 *   -> LAB_5a78 tail.
 */

/* LAB_521d_5a78 tail (raw 90399-90436), hull subset. */
static void ai_euro_20e6_ship_tail_5a78(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  if (u->orders == AI_EURO_ACT_ADJACENT || u->orders == UNITS_ORDER_NONE) {
    u->col1_ai_plan = 0x30;          /* '0' */
    u->orders = UNITS_ORDER_FORTIFY; /* 5 */
  }
  if (u->orders == UNITS_ORDER_FORTIFY && ctx->col1_ok && ctx->col1) {
    /* raw 90406-90420: an idle unit next to a settlement of a nation it is
     * at peace with (FUN_281f_0a38 & 0x40) drops back to act state 0. */
    for (int d = 0; d < 8; ++d) {
      const int owner = ai_euro_20e6_colony_owner_at(ctx, u->x + MAP_DIR8_DX[d], u->y + MAP_DIR8_DY[d]);
      if (owner >= 0 && owner != nation_id && owner < 4 &&
          (ai_diplo_read(ctx->col1, nation_id, owner) & AI_DIPLO_PEACE)) {
        u->orders = UNITS_ORDER_NONE;
        break;
      }
    }
  }
  if (u->orders == AI_EURO_ACT_GOAL && u->goto_x == u->x && u->goto_y == u->y) {
    if (u->col1_ai_plan == '1') {
      u->col1_ai_plan = 0x42; /* 'B' — raw 90428-90431, hulls only */
    }
    u->moves = 0; /* FUN_281f_0934 */
  }
}

/*
 * FUN_521d_20e6 for a hull. Returns 1 when 20e6 itself ends the act
 * (non-zero return in DOS, or the unit is gone).
 */
static int ai_euro_20e6_ship_dos(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  const int id = u->id;
  /* raw 88402-88405: courses other than 0/5/6/>=10 skip the body. */
  if (u->orders != UNITS_ORDER_NONE && u->orders != UNITS_ORDER_FORTIFY &&
      u->orders != UNITS_ORDER_FORTIFIED && u->orders < AI_EURO_ACT_ADJACENT) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  if (!map_coords_inset(ctx->map, u->x, u->y)) {
    u->col1_ai_plan = 0x40; /* '@', raw 88410-88414 */
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  const int tasked = u->col1_ai_plan == 't' || u->col1_ai_plan == 'i'; /* local_6 */

  /* FUN_1427_0d38 mode 2 counts the whole tile stack (including passengers
   * linked through ships), then 20e6 subtracts the acting hull. Snapshot it
   * before LAB_3558 unloads anyone: the dock-demand gate and 457e cadence
   * still see the entry stack for the rest of this act. */
  int tile_stack_count = 0;
  for (int i = 0; i < units_slot_end(ctx->units); ++i) {
    const ColonizeUnit* member = &ctx->units->units[i];
    if (member->active && member->x == u->x && member->y == u->y) {
      ++tile_stack_count;
    }
  }
  const int a8_at_entry = tile_stack_count > 0 ? tile_stack_count - 1 : 0;
  const int at_own_colony = colonies_id_at(ctx->colonies, u->x, u->y) >= 0 &&
                            ctx->colonies->colonies[colonies_id_at(ctx->colonies, u->x, u->y)]
                                .nation_id == nation_id;
  if (!at_own_colony) {
    /* LAB_3558 unload mask + loop. The loop exhausts every unloaded
     * passenger and then the hull (FUN_281f_0934), but 20e6 runs on. */
    const int mask9c = ai_euro_20e6_unload_mask(ctx, u, nation_id);
    if (getenv("AI_SHIP_TRACE") && mask9c) {
      fprintf(stderr, "[shipdos] unit %d n%d (%d,%d) unload mask 0x%x\n", id, nation_id, u->x, u->y, mask9c);
    }
    if (mask9c != 0 && ai_euro_20e6_unload_by_mask(ctx, u, nation_id, mask9c) > 0) {
      u = units_get(ctx->units, id);
      if (!u || !u->active) {
        return 1;
      }
      u->moves = 0;
    }
    /* Colony-sail pick (raw 89614-89711). */
    if (!tasked) {
      int pioneers = 0;
      int mil = 0;
      int scouts = 0;
      int milvet = 0;
      int civ = 0;
      ai_euro_20e6_ship_cargo_counts(ctx, u, &pioneers, &mil, &scouts, &milvet, &civ);
      int a8 = 0;
      for (int s = 0; s < u->cargo_count && s < COLONIZE_UNIT_CARGO_MAX; ++s) {
        const ColonizeUnit* p = units_get_const(ctx->units, u->cargo_ids[s]);
        if (p && p->active) {
          a8++;
        }
      }
      const int pioneers_b4 = pioneers;
      const int found_probe = ai_goals_max_primary_prio(nation_id, u->x, u->y, AI_GOAL_FOUND);
      ai_euro_20e6_goal_fold(ctx, u, nation_id, found_probe, &pioneers, &civ, NULL);
      const int urgency = ai_euro_0a60_work_registered(nation_id);
      int cx = 0;
      int cy = 0;
      if (a8 > 0 && (civ != 0 || ((pioneers != a8 || urgency > 0x18) && mask9c == 0)) &&
          ai_euro_20e6_colony_sail_pick(ctx, u, nation_id, mil, pioneers_b4, urgency, &cx, &cy)) {
        u->col1_ai_plan = 0x34; /* -> 27f5 -> 20c6 with DX = '4' (OVL14 0x3f47) */
        ai_euro_set_goto(u, AI_EURO_ACT_GOAL, cx, cy);
        ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
        return 0;
      }
    }
    /* FUN_521d_20e6 raw 89725-89728: dock demand precedes goods delivery
     * when the hull has not entered the own-colony berth block. */
    if (a8_at_entry == 0 && ai_euro_20e6_europe_dock_demand(ctx, u, nation_id)) {
      ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
      return 0;
    }
  }
  /* Berth block / goods delivery / sell / LAB_4393 haul. */
  if (!tasked) {
    const int ox = u->goto_x;
    const int oy = u->goto_y;
    const int oo = u->orders;
    const int haul = ai_euro_try_ship_trade_haul(ctx, nation_id, u);
    if (haul) {
      u = units_get(ctx->units, id);
      if (!u || !u->active) {
        return 1;
      }
      if (haul == 2 || u->orders != oo || u->goto_x != ox || u->goto_y != oy || u->moves <= 0) {
        ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
        return 0;
      }
    }
  }
  /* LAB_457e: a tasked hull leaves 20e6 here. */
  if (tasked) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  /* LAB_457e reads the entry snapshot local_a8. A hull that disembarked its
   * last passenger in LAB_3558 is still nonempty for this call. */
  if (a8_at_entry == 0 && ai_euro_20e6_hs_cadence_enabled() &&
      ai_euro_20e6_457e_hs_cadence(ctx, u, nation_id)) {
    u = units_get(ctx->units, id);
    if (!u || !u->active) {
      return 1;
    }
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  /* No Europe-export arm here: DOS sails a hull home only through LAB_3fa6
   * (full or 2+ holds, inside ai_euro_try_ship_trade_haul). The port's
   * ">= 50 export goods" arm sent half-loaded hulls to Europe (DOSBox 1505:
   * the Spanish caravel with 78 ore took the far-roam arm instead). */
  /* Pre-LAB_4d2e gate, raw 90210-90219. */
  const int idle = u->orders == UNITS_ORDER_NONE || u->orders == AI_EURO_ACT_ADJACENT ||
                   u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED ||
                   (u->orders == AI_EURO_ACT_GOAL && u->goto_x == u->x && u->goto_y == u->y);
  if (!idle && !ai_euro_20e6_adjacent_foreign_09dc(ctx, u->x, u->y, nation_id)) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  /* LAB_4d2e. */
  Ai20e6Unit s;
  ai_euro_20e6_prologue(ctx, u, nation_id, &s);
  if (ai_euro_20e6_ship_far_roam(ctx, u, &s)) {
    ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
    return 0;
  }
  int attack = 0;
  const int dir = ai_euro_20e6_wander_step(ctx, u, &s, &attack, NULL);
  if (getenv("AI_SHIP_TRACE")) {
    fprintf(stderr, "[shipdos] unit %d n%d (%d,%d) mp %d 4d2e dir %d attack %d\n", id, nation_id,
            u->x, u->y, u->moves, dir, attack);
  }
  if (dir >= 0 && dir < 8 && attack) {
    u->last_dir = dir;
    ai_euro_try_attack(ctx, u, u->x + MAP_DIR8_DX[dir], u->y + MAP_DIR8_DY[dir]);
    u = units_get(ctx->units, id);
    return !u || !u->active;
  }
  u->col1_ai_plan = 0x39; /* '9' — raw 89040 fallthrough */
  /* LAB_589e commit. */
  if (dir < 0 || dir > 7) {
    ai_euro_20e6_stay_tail_589e(u);
  } else {
    u->last_dir = dir;
    const int nx = u->x + MAP_DIR8_DX[dir];
    const int ny = u->y + MAP_DIR8_DY[dir];
    if (map_coords_inset(ctx->map, nx, ny)) {
      u->orders = AI_EURO_ACT_STEP;
      u->goto_x = nx;
      u->goto_y = ny;
    }
  }
  ai_euro_20e6_ship_tail_5a78(ctx, u, nation_id);
  return 0;
}

/* FUN_479b_0972: one pathfinder step toward +0x314d/e. */
/*
 * FUN_465b_0000 raw 75483-75490 -> FUN_4d56_4528 non-human arm (OVL13
 * 0x463c-0x4764 type switch, 0x4bdb tail, 0x4bf0 return): a computer Euro
 * land unit stepping onto a village tile. Soldier / Dragoon / Artillery take
 * code 9 (attack bit, units_try_move_w) and 4528 returns 0, so the move goes
 * on into the attack. Every other type returns [bp-0x58] = 1: FUN_1000_8b24
 * exhausts its MP and 465b abandons the step. Scout = code 6 (speak with
 * chief), Missionary = codes 3/4/7, a colonist-class unit with profession
 * 0x1c/0x19 under alarm 0x4b = code 5 (live among). Wagon Train (code 1,
 * trade) is left to the existing move path. Returns 1 when the step was
 * consumed.
 */
static int ai_euro_4528_ai_village_entry(ColonizeTurnContext* ctx, ColonizeUnit* u, int tx, int ty) {
  if (!ctx->col1_ok || !ctx->col1 || !ctx->col1->tribe || u->nation_id < 0 || u->nation_id > 3 ||
      ctx->col1->player[u->nation_id].control == 0 || units_is_sea(ctx->units, u->id)) {
    return 0;
  }
  int tribe_index = -1;
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    const ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->x == tx && (int)t->y == ty && t->nation_id >= 4 && t->nation_id <= 11) {
      tribe_index = (int)ti;
      break;
    }
  }
  const int type = u->type_index;
  if (tribe_index < 0 || type == 1 || type == 4 || type == 0x0b || type == 0x0c) {
    return 0;
  }
  const int e = u->nation_id;
  const int id = u->id;
  if (type == 5) {
    (void)ai_contact_ai_scout_visit_village(ctx, e, tribe_index, id);
  } else if (type == 3) {
    (void)ai_contact_ai_missionary_village(ctx, e, tribe_index, id);
  } else if ((type == 0 || type == 2 || type == 7 || type == 9) &&
             (u->profession == UNITS_JOB_NONE || u->profession == UNITS_JOB_SERVANT) &&
             ai_diplo_indian_alarm(ctx->col1, (int)ctx->col1->tribe[tribe_index].nation_id, e) <
               0x4b) {
    (void)ai_contact_ai_live_among_village(ctx, e, tribe_index, id);
  }
  ColonizeUnit* after = units_get(ctx->units, id);
  if (after && after->active) {
    after->moves = 0; /* FUN_1000_8b24 */
  }
  return 1;
}

void ai_euro_goal_walk_479b(ColonizeTurnContext* ctx, ColonizeUnit* u) {
  const int id = u->id;
  const int state = u->orders;
  int px = 0;
  int py = 0;
  int ok = 0;
  ColonizeWorld w = world_from_turn_ctx(ctx);
  /* FUN_465b_0000 raw 75649: partial-MP moves reseed before rolling. */
  w.rng_reseed = ai_turn_seed(ctx);
  w.rng_reseed_set = true;
  if (u->x != u->goto_x || u->y != u->goto_y) {
    ok = units_next_goto_step_w(&w, id, &px, &py);
  }
  /* FUN_6662_0f74 LAB_1599, raw 104758: the pathfinder writes +0x314f
   * on EVERY return, including -1 when already at the goal. Keeping the
   * previous step here fed a stale facing penalty into 20e6's next score
   * (bugs.md #530: Spain's second step became W instead of SW). */
  int dir = -1;
  if (ok) {
    for (int d = 0; d < 8; ++d) {
      if (px == u->x + MAP_DIR8_DX[d] && py == u->y + MAP_DIR8_DY[d]) {
        dir = d;
        break;
      }
    }
  }
  u->last_dir = dir;
  if (!ok) {
    u->orders = UNITS_ORDER_NONE; /* FUN_2a1f_0210 found no direction */
    return;
  }
  if (ai_euro_4528_ai_village_entry(ctx, u, px, py)) {
    return;
  }
  const int units_before = units_active_count(ctx->units);
  const bool moved = units_try_move_w(&w, id, px, py);
  if (!g_units_465b_settled) {
    units_465b_0bd1_tail(ctx->units, id, units_before, false);
  }
  if (!moved) {
    u = units_get(ctx->units, id);
    if (u) {
      u->moves = 0; /* blocked step: the port has no DOS retry; end the act */
    }
    return;
  }
  u = units_get(ctx->units, id);
  if (!u || !u->active) {
    return;
  }
  ai_euro_sync_aboard_cargo_xy(ctx->units, u);
  if (u->x != u->goto_x || u->y != u->goto_y) {
    return;
  }
  /* FUN_479b_0972 raw 77091-77098: an AI goto that ends on High Seas
   * (class 0x1a) under plan 'E' crosses at once (FUN_291f_0208 -> 007a);
   * after the declaration only the crown's Man-O-War on its own slot. */
  if (state != AI_EURO_ACT_STEP && u->col1_ai_plan == AI_EURO_PLAN_EUROPE_BOUND &&
      ctx->col1_ok && ctx->col1) {
    const ColonizeCol1Save* col1 = ctx->col1;
    const int mow = units_kind_type_index(ctx->units, UNITS_KIND_MAN_O_WAR);
    const bool open = !col1->head.game_options.woi ||
      (u->nation_id == ai_king_crown_nation_col1(col1, (int)col1->head.human_player) &&
       mow >= 0 && u->type_index == mow);
    if (open && ai_euro_ship_enter_europe(ctx, u)) {
      return;
    }
  }
  /* DOS-LITERAL FUN_479b_0972 raw 77099-77103: arriving pioneers
   * discard their explorer hop countdown and slot (+0x3155/+0x3156). */
  if (ai_euro_unit_kind(ctx->units, u) == UNITS_KIND_PIONEER) {
    uint8_t* sc = ai_euro_20e6_hold_scratch(u);
    sc[1] = 0;
    sc[2] = 0xff;
  }
  if (state == AI_EURO_ACT_GOAL) {
    u->moves = 0; /* FUN_281f_0934 on arrival */
  }
  if (state != AI_EURO_ACT_STEP) {
    u->orders = UNITS_ORDER_NONE;
  }
}

/* FUN_521d_5b66 for a hull. */
void ai_euro_act_ship_dos(ColonizeTurnContext* ctx, ColonizeUnit* u, int nation_id) {
  const int id = u->id;
  if (ai_euro_in_europe(u->x, u->y)) {
    struct ai_euro_act_ctx a;
    memset(&a, 0, sizeof(a));
    a.ctx = ctx;
    a.u = u;
    a.nation_id = nation_id;
    a.is_ship = 1;
    (void)ai_euro_act_ship_europe_exit(&a);
    return;
  }
  const int fresh = u->moves >= units_max_mp(ctx->units, id);
  int run_20e6 = 1;
  if (!fresh && u->orders == AI_EURO_ACT_GOAL) {
    Ai20e6Unit s;
    ai_euro_20e6_prologue(ctx, u, nation_id, &s);
    run_20e6 = (s.flags & 1) && ai_euro_20e6_adjacent_foreign_09dc(ctx, u->x, u->y, nation_id); /* FUN_1000_8b74 */
  }
  if (run_20e6 && ai_euro_20e6_ship_dos(ctx, u, nation_id)) {
    return;
  }
  u = units_get(ctx->units, id);
  /* FUN_521d_5b66 5bda..5c0e (overlay asm 139925): dispatch the
   * resulting order even when unloading exhausted MP. An arrived step
   * still clears its order and facing through FUN_479b_0972 (#530). */
  if (!u || !u->active) {
    return;
  }
  if (u->orders == AI_EURO_ACT_GOAL || u->orders == AI_EURO_ACT_STEP) {
    ai_euro_goal_walk_479b(ctx, u);
  } else {
    u->moves = 0; /* FUN_1000_8b24 */
  }
}
