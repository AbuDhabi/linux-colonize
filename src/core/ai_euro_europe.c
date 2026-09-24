/*
 * Euro AI — FUN_521d_5d04 Europe planning: gold floors, ship-buy ladder, dock-hire callbacks + hire tail
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Sections:
 *   FUN_521d_5d04 gold floors, WoI Man-o-War seizure, treasury bump
 *   5d04 planning flag cascade + naval gold floors
 *   5d04 ship-buy proposal and purchase ladder
 *   5d04 dock/market callback surface (s_5d04_ctx binding)
 *   5d04 hire tail (candidates / colony demand / departing ships) + ai_euro_nation_planning
 */

#include "core/internal.h"
#include "core/ai_euro.h"

#include "core/ai.h"
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

/* DS:0x9796/0x97a8/0x97ae — gold-floor candidates 5d04 uses to clamp a
 * nation's treasury up to a minimum ("catch-up" gold). Values confirmed
 * 2026-08-18 via live DOSBox-X data dump (`D 237D:9790`): 1000/2000/5000.
 * Identity resolved 2026-09-07d: these are the *price cells* of the
 * FUN_521d_5c3c Europe purchase table (DS:0x978d stride 6) — Caravel /
 * Privateer / Frigate — so each floor guarantees the flagged nation can
 * afford exactly the ship its flag buys. */
static uint32_t ai_euro_5d04_ph_gold_floor(int which) {
  const EuropePurchaseOption* opt;
  switch (which) {
    case 0x9796: /* no_ships: Caravel floor (table index 1) */
      opt = europe_purchase_option_at(1);
      return opt ? (uint32_t)opt->gold : 0;
    case 0x97a8: /* privateer threat: Privateer floor (table index 4) */
      opt = europe_purchase_option_at(4);
      return opt ? (uint32_t)opt->gold : 0;
    case 0x97ae: /* frigate threat: Frigate floor (table index 5) */
      opt = europe_purchase_option_at(5);
      return opt ? (uint32_t)opt->gold : 0;
    default: return 0;
  }
}

/* DS:0xa89a/0xa89b/0x9e52/0x9e54 — writer mechanism fully traced and
 * live-confirmed 2026-08-19 (raw viceroy_unpacked.c:78243-78304 +
 * DOSBox-X captures, see census_tally.md phase 3 for the full write-up).
 * NOT a "rival" stat — it's this nation's own **(count, colony-level-sum)
 * of its own colonies currently within 5 tiles of a foreign warship**:
 * `0xa89b`/`0x9e52` = threatened by a **Frigate** specifically (the source
 * code literal-checks `unit+0x3146 == 0x11`, confirmed = Frigate against
 * NAMES.TXT `@UNIT` order); `0xa89a`/`0x9e54` = threatened by any other
 * qualifying armed ship. Per-nation-call-reset (not turn-accumulated)
 * confirmed both by the raw zero-out at the top of `FUN_4962_0018` and
 * live capture.
 *
 * The type-range (`0x0d..0x12`) and the `type==0x11`-vs-else bit split are
 * solid (literal code). The sub-gate (`FUN_2a1f_027e`) that has to pass
 * before any qualifying ship counts at all is now identified by reading
 * the source (not guessed from behavior): it thunks to `FUN_6662_0906`, a
 * **movement/pathfinding cost check** ("is there a short-enough navigable
 * route between ship and colony"), not a diplomacy gate — see
 * census_tally.md for the trace. That retroactively explains three live
 * captures that all fired regardless of diplomatic state (peace, alliance,
 * war all confirmed) — diplomacy was never the variable being tested.
 * Genuinely still open: whether Privateer specifically can pass the
 * pathfinding gate (one confirmed instrumented positive at peace, zero
 * confirmed instrumented negatives — an earlier "excluded" read came from
 * an unverified report and was retracted), and the underlying cost
 * function (`thunk_FUN_2a1f_05f0`)'s own formula.
 *
 * LIVE 2026-09-07: the source detector (the 11×11 ship probe with the
 * FUN_6662_0906 cost 0..5 sub-gate) now runs in
 * ai_euro_refresh_colony_ai_flags, accumulating ai_euro_s_ship_pressure per nation
 * — this crumb reads those tallies. */
static int ai_euro_5d04_ph_naval_threat_crumb_n(int which, int nation_id) {
  if (nation_id < 0 || nation_id > 3) {
    return 0;
  }
  const AiEuroShipPressure* sp = &ai_euro_s_ship_pressure[nation_id];
  switch (which) {
    case 0xa89b: return (int)sp->frigate_colonies;
    case 0xa89a: return (int)sp->other_colonies;
    case 0x9e52: return sp->frigate_pop;
    case 0x9e54: return sp->other_pop;
    default: return 0;
  }
}

int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);

/* FUN_281f_09fc(0xd) on a scanned colony — building index 0xd = College
 * (NAMES.TXT @BUILDING file order, cross-checked against index 0x24 =
 * Lumber Mill in euro_unit_act.md). Real since 2026-09-07d — the bVar5
 * consumer (hire tail Veteran Soldier promote) is live. */
static int ai_euro_5d04_colony_has_college(
  const ColonizeTurnContext* ctx, const ColonizeColony* colony
) {
  const int id = colonies_building_row(ctx->colonies, COLONY_BUILDING_COLLEGE);
  return id >= 0 && colony->has_building[id];
}

/* Raw 92412-92423 (WoI arm): destroy the nation's first Man-O-War and
 * `*(int*)0x53de += 1`. Real since 2026-09-07d — type 0x12 = Man-O-War by
 * the confirmed @UNIT ship roster (0x0d..0x12; the old "NEW WORLD wagon"
 * name predates that confirmation), and 0x53de = head.expeditionary_force
 * [2] (man-o-wars): on Declare Independence the crown seizes the AI
 * nation's MoW into the REF pool — the increment is genuine reuse, not a
 * Ghidra misattribution.
 *
 * 2026-09-07g (asm OVL14:005dd9): the scan is NOT map-wide. FUN_281f_07e0
 * is unit_index_on_tile(236+n, 236+n) — the nation's Europe-dock
 * pseudo-tile — so the King seizes only a Man-O-War standing in a European
 * port. The wave MoW (0982) spawns on real map water and is never
 * touchable, which is why DOS can run 5d04 on the crown slot itself
 * during WoI without eating the REF fleet. */
static void ai_euro_5d04_woi_seize_manowar(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx->units) {
    return;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || !ai_euro_in_europe(u->x, u->y)) {
      continue;
    }
    if (ai_euro_20e6_dos_type(ctx->units, u) == 0x12) {
      (void)units_despawn(ctx->units, u->id);
      ctx->col1->head.expeditionary_force[2]++;
      return;
    }
  }
}

/*
 * Raw decomp 85878-85899 (verbatim control flow): difficulty-scaled
 * per-turn treasury bump. local_12 = colony_counts[n] + (year-1500)/50,
 * zeroed before turn 20, doubled past year 1699; local_2e = difficulty *
 * local_12, scaled *1.5 at difficulty 3, *2 at difficulty 4; gold +=
 * local_2e*4.
 */
static void ai_euro_5d04_treasury_bump(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  const ColonizeCol1Head* head = &ctx->col1->head;
  const int colony_count = ctx->col1->stuff.colony_counts[nation_id];

  int local_12 = colony_count + ((int)head->year - 1500) / 50;
  if ((int)head->turn < 20) {
    local_12 = 0;
  }
  if ((int)head->year > 0x6a3) { /* 1699 */
    local_12 <<= 1;
  }
  int local_2e = (int)head->difficulty * local_12;
  if (head->difficulty == 3) {
    local_2e = (local_2e >> 1) + local_2e; /* *1.5 */
  } else if (head->difficulty == 4) {
    local_2e <<= 1; /* *2 */
  }
  nat->gold += (uint32_t)(local_2e * 4);
}

static int nat_gold_ge(const ColonizeTurnContext* ctx, int nation_id, uint32_t threshold) {
  return ctx->col1->nation[nation_id].gold >= threshold;
}

/* Raw decomp 85973-86002: raise gold to `floor` if it's currently lower
 * (a per-flag "catch-up" clamp). Real mechanism, inert while
 * `ai_euro_5d04_ph_gold_floor` returns 0. */
static void ai_euro_5d04_apply_gold_floor(ColonizeTurnContext* ctx, int nation_id, uint32_t floor) {
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  if (nat->gold < floor) {
    nat->gold = floor;
  }
}

/* Raw 92404-92532 gate cascade — live since 2026-09-07d (flags feed the
 * real ship-buy ladder, the gold floors, and the hire tail's Veteran
 * promote). bVar5/no_ships/frigate_threatened/privateer_threatened/
 * no_clear_navy/cargo_short name the raw bVar5/21/22/23/24/7 booleans in
 * call order. Naming history: weak_vs_euro/weak_vs_indian until
 * 2026-08-19 (the 0xa89a crumbs are "own colonies near an armed rival
 * ship", not rival-strength); manowar_threatened until 2026-09-07d (the
 * bVar23 response is a Privateer purchase). */
typedef struct Ai5d04PlanningFlags {
  int has_college;       /* bVar5 */
  int no_ships;           /* bVar21 */
  int frigate_threatened;  /* bVar22 */
  int privateer_threatened; /* bVar23 — buys purchase-table type 4 =
     Privateer (0x10) at the Privateer gold floor (0x97a8 = its price
     cell); the type-count gates read unit_type_counts[.][0x10]. Was
     misnamed manowar_threatened until 2026-09-07d — the 0xa89a crumb is
     "non-Frigate armed ship nearby" (MoW included), but what the flag
     *buys* is a Privateer. */
  int no_clear_navy;         /* bVar24 */
  int cargo_short;             /* bVar7 */
} Ai5d04PlanningFlags;

static Ai5d04PlanningFlags ai_euro_5d04_compute_flags(
  ColonizeTurnContext* ctx, int nation_id
) {
  Ai5d04PlanningFlags f;
  memset(&f, 0, sizeof(f));
  if (!ctx || !ctx->col1 || !ctx->colonies || nation_id < 0 || nation_id >= 4) {
    return f;
  }
  const ColonizeCol1Head* head = &ctx->col1->head;
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int turn = (int)head->turn;
  const int woi = head->game_options.woi != 0;

  /* bVar5: any own colony has building index 0xd (College). */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (c->active && c->nation_id == nation_id &&
        ai_euro_5d04_colony_has_college(ctx, c)) {
      f.has_college = 1;
      break;
    }
  }

  /* WoI arm (raw 92412-92423): crown seizes the nation's Man-O-War into
   * the REF pool — see ai_euro_5d04_woi_seize_manowar's header. */
  if (woi) {
    ai_euro_5d04_woi_seize_manowar(ctx, nation_id);
  }

  f.no_ships = stuff->ship_counts[nation_id] == 0;

  /* local_3c: avg (over the other 3 nations) of unit_type_counts[.][16] +
   * unit_type_counts[.][17]*4, >>2 (raw divides by 4 regardless of the
   * 3-term sum — kept literal). Zeroed under WoI. */
  int local_3c = 0;
  for (int n = 0; n < 4; ++n) {
    if (n == nation_id) {
      continue;
    }
    local_3c += stuff->unit_type_counts[n][16] + stuff->unit_type_counts[n][17] * 4;
  }
  local_3c >>= 2;
  if (woi) {
    local_3c = 0;
  }

  const int col_half = stuff->colony_counts[nation_id] >> 1;
  const int pop_half = stuff->colony_pop_totals[nation_id] >> 1;
  const int focus_nation = head->human_player;
  const int focus_ok = focus_nation >= 0 && focus_nation < 4;

  /* bVar22: gated on Frigate-threatened own colonies (raw 85934-85952).
   * `0xa89b`/`0x9e52` = Frigate-threat (count, level-sum) — see
   * `ai_euro_5d04_ph_naval_threat_crumb` header for how that's confirmed. */
  f.frigate_threatened = 0;
  if (!((ai_euro_5d04_ph_naval_threat_crumb_n(0xa89b, nation_id) == 0 &&
         ai_euro_5d04_ph_naval_threat_crumb_n(0xa89a, nation_id) == 0) ||
        local_3c == 0)) {
    int reach_weak_check = 0;
    if (ai_euro_5d04_ph_naval_threat_crumb_n(0xa89b, nation_id) < col_half &&
        ai_euro_5d04_ph_naval_threat_crumb_n(0x9e52, nation_id) < pop_half) {
      if (turn > 200 && nat_gold_ge(ctx, nation_id, 2000)) {
        reach_weak_check = 1;
      }
    } else {
      reach_weak_check = 1;
    }
    if (reach_weak_check && focus_ok) {
      f.frigate_threatened = stuff->unit_type_counts[nation_id][17] == 0 &&
                              stuff->unit_type_counts[focus_nation][17] != 0;
    }
  }

  /* bVar23: gated on "other-armed-ship"-threatened own colonies (raw
   * 92456-92474). `0xa89a`/`0x9e54` = count/level-sum for any qualifying
   * ship type other than Frigate. The response is a Privateer buy: own
   * Privateer count (unit_type_counts[.][0x10]) < 2 and the human owns
   * one — see the flag's own comment in Ai5d04PlanningFlags. */
  f.privateer_threatened = 0;
  if (((ai_euro_5d04_ph_naval_threat_crumb_n(0xa89a, nation_id) != 0 ||
        ai_euro_5d04_ph_naval_threat_crumb_n(0xa89b, nation_id) != 0)) &&
      local_3c != 0 && !f.frigate_threatened) {
    int reach = 0;
    if (ai_euro_5d04_ph_naval_threat_crumb_n(0xa89a, nation_id) < col_half &&
        ai_euro_5d04_ph_naval_threat_crumb_n(0x9e54, nation_id) < pop_half) {
      if (turn > 100 && nat_gold_ge(ctx, nation_id, 1000)) {
        reach = 1;
      }
    } else {
      reach = 1;
    }
    if (reach && focus_ok) {
      f.privateer_threatened = stuff->unit_type_counts[nation_id][16] < 2 &&
                              stuff->unit_type_counts[focus_nation][16] != 0;
    }
  }

  /* bVar24: no clear strongest navy (raw 86003-86024). */
  uint8_t max_armed = 0;
  for (int n = 0; n < 4; ++n) {
    if (stuff->armed_ship_counts[n] > max_armed) {
      max_armed = stuff->armed_ship_counts[n];
    }
  }
  int tied = 0;
  for (int n = 0; n < 4; ++n) {
    if (stuff->armed_ship_counts[n] == max_armed) {
      tied++;
    }
  }
  f.no_clear_navy =
    f.frigate_threatened || stuff->armed_ship_counts[nation_id] < max_armed || tied > 1;

  /* bVar7: cargo/passenger space short (raw 92528-92532) —
   * `ship_cargo_totals <= (census_pop_proxy/2 + colony_counts*2)/2`.
   * −0x6bf0 = DS:0x9410 = census_pop_proxy (misread as colony_pop_totals
   * until 2026-09-07d; colony_pop_totals is −0x6bf4/0x940c and only feeds
   * the threat weak-checks above). */
  f.cargo_short =
    stuff->ship_cargo_totals[nation_id] <=
      (((stuff->census_pop_proxy[nation_id] >> 1) +
        stuff->colony_counts[nation_id] * 2) >> 1) &&
    !woi;

  return f;
}

/* Gold-floor-max: raise gold to a per-scenario candidate under each threat
 * flag (raw 85973-86002, was inline inside `ai_euro_5d04_compute_flags`
 * until 2026-08-19 — split out once the floor values went from stubbed-0
 * to real (1000/2000/5000): baking a genuine mutation into a function
 * whose result callers were discarding as "reference-only" was exactly
 * the bug that regressed `unit_ai_euro_war` the first time (see memory).
 * `compute_flags` is pure again; this is the deliberate, explicit
 * mutation step — live since 2026-09-07d (the orchestrator calls it right
 * after `compute_flags`, matching raw 92476-92505 order). */
static void ai_euro_5d04_apply_naval_gold_floors(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f
) {
  if (f->no_ships) {
    ai_euro_5d04_apply_gold_floor(ctx, nation_id, ai_euro_5d04_ph_gold_floor(0x9796));
  }
  if (f->privateer_threatened) {
    ai_euro_5d04_apply_gold_floor(ctx, nation_id, ai_euro_5d04_ph_gold_floor(0x97a8));
  }
  if (f->frigate_threatened) {
    ai_euro_5d04_apply_gold_floor(ctx, nation_id, ai_euro_5d04_ph_gold_floor(0x97ae));
  }
}

/* ---- FUN_521d_5d04 tail: Europe hire ladder (raw 86030-86564) --------
 * Finishing the structural port (2026-08-19) — the part scoped out
 * earlier as "overlaps existing thin coverage, callees genuinely
 * unresolved." Ported anyway per request: control flow and arithmetic
 * 1:1 where resolved, every callee stubbed (inert defaults) per the
 * original brief. NOT wired into the live path — a complete reference
 * implementation alongside `ai_euro_nation_planning`, same posture as
 * the gate-cascade section above. Most of this body is naturally inert
 * at runtime (unit-iteration stubs return "none found"), which is safe
 * by construction, not a workaround — see each stub's own comment. */

/* thunk_FUN_2a1f_0500 = FUN_521d_5c3c — Europe unit purchase (real port,
 * 2026-09-07d). Table at DS:0x978d stride 6 ({dos_type, ?, 0xff, price16}),
 * pinned byte-identical across 3 original_memory_dumps: 0=Artillery/500,
 * 1=Caravel/1000, 2=Merchantman/2000, 3=Galleon/3000, 4=Privateer/2000,
 * 5=Frigate/5000 (the "gold floors" 0x9796/0x97a8/0x97ae are this table's
 * Caravel/Privateer/Frigate price cells). The raw weight_pct arg feeds
 * FUN_521d_5c38 which is `return 1;` in the retail build — dead, dropped.
 * Body: gold >= price → spawn table type in Europe (FUN_281f_095c at the
 * nation's Europe tile), then raw 92291-92300: goal target (+0x314d/e) =
 * nation+0x32/+0x33 (return_from_europe x/y) and the orders byte (+0x314c) = 1
 * for land types (< 0xd || > 0x12), 0 for hulls; gold -= price, return 1. */
static int ai_euro_5d04_propose_ship_buy(
  ColonizeTurnContext* ctx, int nation_id, int type_id
) {
  /* The DS:0x978d stride-6 table itself lives in europe.c (audit AE-17);
   * `type_id` is that table's own row index, unchanged. */
  const EuropePurchaseOption* opt =
    (ctx && ctx->units && ctx->col1) ? europe_purchase_option_at(type_id) : NULL;
  if (!opt) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  const uint32_t price = (uint32_t)opt->gold;
  if (nat->gold < price) {
    return 0;
  }
  /* The purchase slot's identity is its @UNIT kind; its name is display text. */
  int lt = units_kind_type_index(ctx->units, opt->kind);
  if (lt < 0) {
    return 0;
  }
  const int sid = units_spawn_allow_stack(ctx->units, lt, 200, 100);
  if (sid < 0) {
    return 0;
  }
  ColonizeUnit* u = units_get(ctx->units, sid);
  if (!u) {
    return 0;
  }
  units_set_nation(u, nation_id);
  u->moves = 0; /* docked in Europe this turn, as in DOS */
  /*
   * FUN_521d_5c3c raw 92291-92300, verbatim after FUN_281f_095c:
   *   +0x314d = nation+0x32;  +0x314e = nation+0x33;      (return-from-Europe)
   *   +0x314c = (type < 0xd || type > 0x12) ? 1 : 0;      (orders byte)
   * i.e. a land purchase leaves Europe SENTRY (act_state 1) with the nation's
   * last Europe-exit tile as its goal target; a hull gets orders 0. The port's
   * nation record carries +0x32/+0x33 as return_from_europe_x/y (col1_bridge.c
   * :1721 / :3138), so the target is written straight from there.
   */
  {
    const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
    const int is_hull = dtype >= 0xd && dtype <= 0x12;
    u->orders = is_hull ? UNITS_ORDER_NONE : UNITS_ORDER_SENTRY;
    u->goto_x = (int)nat->return_from_europe_x;
    u->goto_y = (int)nat->return_from_europe_y;
  }
  nat->gold -= price;
  if (ctx->europe && nation_id == ctx->human_nation) {
    ctx->europe->gold = (int)nat->gold;
  }
  return 1;
}

/*
 * Raw 92533-92567: ship-buy candidate ladder, gated on `!woi &&
 * ship_cargo_totals[n] <= census_pop_proxy[n]/2 + colony_counts[n]`
 * (−0x6bf0 = DS:0x9410 = census_pop_proxy — an earlier pass misread it as
 * colony_pop_totals, fixed 2026-09-07d). Returns the raw body's `local_3e`
 * (0 = no candidate bought). `*out_abort` is the raw early `return;` —
 * frigate/privateer threatened and the purchase failed (couldn't afford).
 */
static int ai_euro_5d04_ship_buy_ladder(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f, int* out_abort
) {
  *out_abort = 0;
  const ColonizeCol1Head* head = &ctx->col1->head;
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int woi = head->game_options.woi != 0;
  const int proxy_half = stuff->census_pop_proxy[nation_id] >> 1;
  if (woi || stuff->ship_cargo_totals[nation_id] >
             (uint32_t)(proxy_half + stuff->colony_counts[nation_id])) {
    return 0;
  }
  int candidate = 0;
  if (f->frigate_threatened) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 5);
  }
  if (candidate == 0 && f->frigate_threatened) {
    *out_abort = 1;
    return 0;
  }
  if (f->privateer_threatened) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 4);
  }
  if (candidate == 0 && f->privateer_threatened) {
    *out_abort = 1;
    return 0;
  }
  if (candidate == 0 && stuff->armed_ship_counts[nation_id] < 8 &&
      dos_rng_range(ctx->rng, 0, 1) != 0 && f->no_clear_navy) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 5);
  }
  if (candidate == 0 && dos_rng_range(ctx->rng, 0, 3) != 0) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 3);
  }
  if (candidate == 0 && dos_rng_range(ctx->rng, 0, 1) == 0 &&
      stuff->ship_cargo_totals[nation_id] < 0xc) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 2);
  }
  if (candidate == 0 && stuff->ship_cargo_totals[nation_id] < 3) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 1);
  }
  if (candidate == 0 && stuff->armed_ship_counts[nation_id] < 4 &&
      dos_rng_range(ctx->rng, 0, 3) == 0 && f->no_clear_navy && !f->cargo_short) {
    candidate = ai_euro_5d04_propose_ship_buy(ctx, nation_id, 4);
  }
  return candidate;
}

/* --- The hire-ladder tail's own callees (real as of 2026-08-27) ---------
 * Identity table (address_mapping.csv chain, FUNCTION_CATALOG, 38fd_0718 /
 * 521d_6d8e / 5bfb_00f8 decompiles):
 *   FUN_281f_07e0 / 02e4   head unit on the nation's Europe tile (x = y =
 *                          nation-0x14, the sentinel FUN_281f_095c spawns
 *                          at) / next unit in that stack -> "units of this
 *                          nation in Europe", iterated by pool index.
 *   unit+0x3146            unit type (0xd..0x12 = ships; 1 Soldier, 2
 *                          Pioneer, 3 Missionary, 4 Dragoon) — the tail
 *                          "dispatch byte" writes are Europe equip changes.
 *   unit+0x315b            profession (@JOB index, 0x1c = none).
 *   FUN_281f_0c9a          expert gate: 0 for {0x13,0x19,0x1a,0x1b,0x1c}.
 *   FUN_281f_0b78          DS:0x30e[type] >= 0 — type has a profession slot
 *                          (colonist-class types 0..9).
 *   thunk_FUN_2a1f_0494    FUN_521d_03d0 founding_expansion_urgency.
 *   FUN_291f_0b26 / 0afc   FUN_38fd_0718 recruit spawn (profession-coded,
 *                          Vet. Soldier 1-in-(diff+5) becomes a Dragoon,
 *                          Pioneer gets 100 tools) / FUN_38fd_46d4 next
 *                          recruit profession (the +0x44/+0x45 remap table
 *                          is not ported — a plain RNG pick stands in).
 *   FUN_291f_0c3e / 09ea   Europe buy price of a cargo (nation market).
 *   FUN_291f_0c14 / 0a2e   market buy / sell volume bookkeeping.
 *   FUN_281f_08bc(head,4/0xc/0xe)  FUN_1427_0d38 stack queries: 4 = land
 *                          units waiting, 0xc = ships, 0xe = Σ passenger
 *                          capacity; the register-arg call = Artillery on
 *                          the dock.
 *   FUN_281f_095c(0xb,..)  spawn an Artillery in Europe.
 *   FUN_281f_0be6/0c68/0aec hold-0 cargo type / amount / remove.
 *   FUN_291f_0dc6 + 0aba   sell 100 of hold 0, credit the nation's gold.
 *   FUN_291f_0d8e          buy+load 100 of a cargo onto the ship.
 *   FUN_291f_0ec2          ship departs Europe: the dock stack boards, the
 *                          ship leaves the Europe list (the dispatcher's
 *                          own FUN_48d3_048e teleport places it on the
 *                          high seas next act).
 *   0x5238[type]           ColonizeUnitType.space (ship slots taken).
 *   DS:0xa0db / 0xa0da     per-turn counts from FUN_521d_6d8e's prelude:
 *                          own colonies with specialty muskets or an empty
 *                          muskets stock / with an empty tools stock.
 */
ColonizeTurnContext* ai_euro_s_5d04_ctx = NULL;
int ai_euro_s_5d04_nation = -1;

static int ai_euro_5d04_cb_in_europe_list(int idx) {
  if (!ai_euro_s_5d04_ctx || !ai_euro_s_5d04_ctx->units || idx < 0 || idx >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  const ColonizeUnit* u = &ai_euro_s_5d04_ctx->units->units[idx];
  return u->active && u->nation_id == ai_euro_s_5d04_nation && ai_euro_in_europe(u->x, u->y);
}
static int ai_euro_5d04_cb_list_iter_next(int prev) {
  for (int i = prev + 1; i < COLONIZE_UNITS_MAX; ++i) {
    if (ai_euro_5d04_cb_in_europe_list(i)) {
      return i;
    }
  }
  return -1;
}
static int ai_euro_5d04_cb_list_iter_first(int list_id) {
  (void)list_id;
  return ai_euro_5d04_cb_list_iter_next(-1);
}
static ColonizeUnit* ai_euro_5d04_cb_unit(int idx) {
  if (!ai_euro_s_5d04_ctx || !ai_euro_s_5d04_ctx->units || idx < 0 || idx >= COLONIZE_UNITS_MAX) {
    return NULL;
  }
  return &ai_euro_s_5d04_ctx->units->units[idx];
}
/*
 * DOS unit+0x3146 @UNIT code for a Linux type, by name (fixtures use small
 * synthetic pools, so pool indices are not DOS indices). 0xff = unknown.
 *
 * Audit AE-4: this file carried two name→@UNIT tables. This one used to
 * collapse every sea hull to Caravel 0xd / Privateer 0x10; DOS does no such
 * thing — +0x3146 always holds the real @UNIT row (COLONIZE/NAMES.TXT rows
 * 13 Caravel .. 18 Man-O-War), and the collapse was a port shortcut. Neither
 * live consumer can see the difference (the Pioneer test at the recruit loop
 * compares against 2, and the DS:0x5238 hull-space read resolves ship codes
 * to no pool row either way), so the full table is adopted. Both tables are
 * now units_type_dos_code, whose kind ids ARE the @UNIT row numbers.
 */
static int ai_euro_5d04_dos_type_of(const ColonizeUnitPool* pool, int type_index) {
  const int code = units_type_dos_code(units_type(pool, type_index));
  return code >= 0 ? code : 0xff;
}
/*
 * DOS @UNIT code → Linux pool type. Codes 0..5 are the Europe dock table
 * (europe_dock_unit_type_index_ex with the singular fallbacks this path
 * needs, audit AE-18); 0xb = Artillery, which the dock table does not
 * carry, so it keeps its own arm.
 */
int ai_euro_5d04_linux_type_for(const ColonizeUnitPool* pool, int dos_code) {
  if (dos_code == UNITS_KIND_ARTILLERY) {
    const int t = units_kind_type_index(pool, UNITS_KIND_ARTILLERY);
    return t >= 0 ? t : units_kind_type_index(pool, UNITS_KIND_ARTILLERY);
  }
  return europe_dock_unit_type_index_ex(pool, dos_code, true);
}
/*
 * DS:0x5238[type] — the @UNIT hull-space column for a DOS @UNIT code. The
 * table is DOS-indexed, so the code has to be translated back to a Linux pool
 * type first; 1 when the pool has no such row (DOS's own land-unit default).
 */
int ai_euro_5d04_dos_type_space(const ColonizeUnitPool* pool, int dos_code) {
  const int lt = ai_euro_5d04_linux_type_for(pool, dos_code);
  const ColonizeUnitType* t = lt >= 0 ? units_type(pool, lt) : NULL;
  return t ? t->space : 1;
}
int ai_euro_5d04_dos_type_code(const ColonizeUnitPool* pool, int type_index) {
  return ai_euro_5d04_dos_type_of(pool, type_index);
}
static int ai_euro_5d04_cb_unit_dispatch_byte(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  return u ? ai_euro_5d04_dos_type_of(ai_euro_s_5d04_ctx->units, u->type_index) : -1;
}
static void ai_euro_5d04_cb_set_unit_dispatch_byte(int idx, int value) {
  ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  const int lt = u ? ai_euro_5d04_linux_type_for(ai_euro_s_5d04_ctx->units, value) : -1;
  if (!u || lt < 0) {
    return;
  }
  u->type_index = lt;
  if (value == 1) {
    u->muskets = 50;
    u->horses = 0;
  } else if (value == 4) {
    u->muskets = 50;
    u->horses = 50;
  } else if (value == 2) {
    /*
     * DELIBERATE PORT SUBSTITUTION (bugs.md #636), not a transcription.
     * DOS FUN_521d_5d04 raw 92770-92783 (dup raw 86688-86697) writes only
     * `unit[+0x3146] = 2` plus the FUN_291f_0c14(0xe, 100) market
     * bookkeeping for the 100 tools it just bought; it never touches
     * `+0x3159`, so the fresh unit keeps whatever slot-reuse garbage is
     * there — often 0, which is how DOS feeds the FUN_479b_0158 tools
     * underflow family. The port stocks the 100 tools it paid for instead,
     * because a 0-tools AI Pioneer is inert here (`units_is_pioneer`
     * requires tools > 0). Keep as is; do not "fix" to DOS without a live
     * trace — reverting would disarm every Europe-bought AI Pioneer.
     */
    if (u->tools < 100) {
      u->tools = 100;
    }
  }
}
static int ai_euro_5d04_cb_unit_profession(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  return u ? u->profession : 0x1c;
}
static void ai_euro_5d04_cb_set_unit_profession(int idx, int value) {
  ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (u) {
    u->profession = value;
  }
}
static int ai_euro_5d04_cb_artillery_on_dock(int head) {
  (void)head; /* register-arg stack query: counts Artillery (dispatch byte 0x0b) on the Europe dock */
  int n = 0;
  for (int i = ai_euro_5d04_cb_list_iter_first(0); i >= 0; i = ai_euro_5d04_cb_list_iter_next(i)) {
    if (ai_euro_5d04_cb_unit_dispatch_byte(i) == 0xb) {
      n++;
    }
  }
  return n;
}
static int ai_euro_5d04_cb_nation_hire_mask(int nation_id) {
  const int total = ai_euro_s_5d04_ctx && ai_euro_s_5d04_ctx->colonies ? ai_euro_s_5d04_ctx->colonies->colony_count : 0;
  return ai_goals_founding_expansion_urgency(nation_id, total);
}
static int ai_euro_5d04_cb_unit_is_skilled(int idx) {
  const int t = ai_euro_5d04_cb_unit_dispatch_byte(idx);
  return (t >= 0 && t <= 9) ? 0 : -1;
}
static int ai_euro_5d04_cb_profession_gate(int profession) {
  return (profession == UNITS_JOB_COLONIST ||
          (profession >= 0x19 && profession <= 0x1c))
           ? 0
           : 1;
}
/* FUN_38fd_0718: spawn the recruit in Europe (gold already paid by the caller). */
static int ai_euro_5d04_cb_dock_pop_candidate(int profession) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  if (!ctx || !ctx->units || !ctx->col1) {
    return -1;
  }
  int type = 0;
  if (profession == UNITS_JOB_PIONEER) {
    type = 2;
  } else if (profession == UNITS_JOB_MISSIONARY) {
    type = 3;
  } else if (profession == UNITS_JOB_SCOUT) {
    type = 5;
  } else if (profession == UNITS_JOB_SOLDIER) {
    type = 1;
    const int span = (ai_euro_s_5d04_nation == ctx->human_nation) ? (int)ctx->col1->head.difficulty : 1;
    if (dos_rng_range(ctx->rng, 0, span + 4) == 0) {
      type = 4;
    }
  }
  const int lt = ai_euro_5d04_linux_type_for(ctx->units, type);
  if (lt < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(ctx->units, lt, 200, 100);
  if (id < 0) {
    return -1;
  }
  ColonizeUnit* u = units_get(ctx->units, id);
  if (!u) {
    return -1;
  }
  units_set_nation(u, ai_euro_s_5d04_nation);
  u->moves = 0;
  /* raw 59133 `*(undefined1 *)(iVar2 + 0x314c) = 1`: FUN_38fd_0718 parks the
   * fresh recruit SENTRY in the harbour (+0x314c = the orders byte; europe.c
   * does the same for the human harbour spawn). bugs.md #509. */
  u->orders = UNITS_ORDER_SENTRY;
  u->profession = profession;
  if (type == 2) {
    u->tools = 100; /* 59140: local_4 == 2 -> +0x3159 = 100 */
  } else if (type == 1) {
    u->muskets = 50;
  } else if (type == 4) {
    u->muskets = 50;
    u->horses = 50;
  } else if (type == 5) {
    /*
     * DOS FUN_38fd_0718 (raw 59133-59142) writes no equipment for any type
     * but the Pioneer's tools: mounted/armed state IS the type byte
     * (+0x3146 = 1 Soldier / 4 Dragoon / 5 Scout). This port models the kit
     * as per-unit goods instead (units_sync_equip_after_type_change,
     * units.c: SCOUT -> horses = UNITS_EQUIP_HORSES, the port's own
     * canonical Scout shape), so the Soldier/Dragoon musket/horse writes
     * above are the same representation choice. A hired Scout with 0 horses
     * was the odd one out: it dismounted to nothing on demote and returned
     * no horses when it joined a colony.
     */
    u->horses = UNITS_EQUIP_HORSES;
  }
  return (int)(u - ctx->units->units);
}
/*
 * Refill the recruit-pool slot the AI hire just emptied. DOS `FUN_38fd_46d4`
 * (raw 64554-64694) is a difficulty-scaled tier roll — Petty Criminal /
 * Indentured Servant / Free Colonist are common, experts rare and never
 * three at once — not a flat draw over the @JOB range, which is what this
 * callback used to be. The faithful port already lives in europe.c and
 * writes `nation.recruit[slot]` itself, including the DOS draw order
 * (FUN_281f_04d4(1,15) / (1,10) / (1,8) off the shared game stream,
 * raw 64632/64636/64640) and the AI difficulty substitution of 1
 * (raw 64627-64633). bugs.md #510.
 */
static int ai_euro_5d04_refill_pool_slot(int slot) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  if (!ctx || !ctx->col1) {
    return UNITS_JOB_NONE;
  }
  return europe_nation_refill_pool_slot(ctx->col1, ai_euro_s_5d04_nation, slot, false, ctx->rng);
}
/* Europe SELL quote: FUN_291f_09ea → FUN_38fd_0040 = euro_price − 1 (the same
 * value europe_sell_price returns), from the EuropeScreen (the one Linux
 * market) when present, else from the nation's col1 euro_price byte. */
static int ai_euro_5d04_cb_sell_price(int cargo) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  if (!ctx || !ctx->col1 || cargo < 0 || cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  if (ctx->europe && cargo < ctx->europe->cargo_count && ctx->europe->cargo[cargo].bid > 0) {
    return europe_sell_price(ctx->europe, cargo);
  }
  const int p = (int)ctx->col1->nation[ai_euro_s_5d04_nation].trade.euro_price[cargo] - 1;
  return p < 0 ? 0 : p;
}
static int ai_euro_5d04_cb_price(int cargo) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  if (!ctx || !ctx->col1 || cargo < 0 || cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 1;
  }
  if (ctx->europe && cargo < ctx->europe->cargo_count && ctx->europe->cargo[cargo].ask > 0) {
    return ctx->europe->cargo[cargo].ask;
  }
  return (int)ctx->col1->nation[ai_euro_s_5d04_nation].trade.euro_price[cargo] + 1;
}
static void ai_euro_5d04_cb_sync_gold(void) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  if (ctx && ctx->europe && ctx->col1 && ai_euro_s_5d04_nation == ctx->human_nation) {
    ctx->europe->gold = (int)ctx->col1->nation[ai_euro_s_5d04_nation].gold;
  }
}
static void ai_euro_5d04_cb_market_volume(int cargo, int qty, int is_buy) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  if (ctx && ctx->europe && ai_euro_s_5d04_nation == ctx->human_nation) {
    europe_apply_volume_price(ctx->europe, cargo, qty, is_buy);
  }
}
static void ai_euro_5d04_cb_set_pool_counter(int cargo, int qty) {
  ai_euro_5d04_cb_market_volume(cargo, qty, 1);
}
static int ai_euro_5d04_cb_colony_demand_query(int head, int mode) {
  (void)head;
  int n = 0;
  for (int i = ai_euro_5d04_cb_list_iter_first(0); i >= 0; i = ai_euro_5d04_cb_list_iter_next(i)) {
    const ColonizeUnit* u = ai_euro_5d04_cb_unit(i);
    const int is_ship = units_is_sea(ai_euro_s_5d04_ctx->units, u->id);
    if (mode == 4) {
      n += (!is_ship && u->aboard_ship_id < 0) ? 1 : 0;
    } else if (mode == 0xc) {
      n += is_ship ? 1 : 0;
    } else if (mode == 0xe) {
      n += is_ship ? units_ship_capacity(ai_euro_s_5d04_ctx->units, u->id) : 0;
    }
  }
  return n;
}
/* DOS unit+0x3150 holds_occupied (defined below with the 0a60 block). */
int ai_euro_0a60_goods_holds_used(const ColonizeUnitPool* units, const ColonizeUnit* u);

static int ai_euro_5d04_cb_reward_case(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (!u || units_hold_amount(ai_euro_s_5d04_ctx->units, u->id, 0) <= 0) {
    return -1;
  }
  return u->hold_goods_type[0];
}
static int ai_euro_5d04_cb_reward_value(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  return units_hold_amount(ai_euro_s_5d04_ctx->units, u->id, 0);
}
static void ai_euro_5d04_cb_reward_ack(int idx) {
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (u) {
    (void)units_unload_goods_hold(ai_euro_s_5d04_ctx->units, u->id, 0, NULL, NULL);
  }
}
/*
 * FUN_291f_0dc6(unit, 0, 100) + FUN_281f_0aba: sell hold 0, credit the nation.
 *
 * What the DOS leg actually writes (viceroy_unpacked.c:91089-91090, twin at
 * 93015-93016):
 *   iVar16 = FUN_291f_0dc6(unit, 0, 100);            // → FUN_38fd_1f0c
 *   FUN_281f_0aba(DS:0x9e12, iVar16, iVar16 >> 15);  // → FUN_15eb_0556
 *
 * FUN_38fd_1f0c (viceroy 60320-60337): FUN_281f_0aec empties hold 0 (qty into
 * DS:0x8dc4, excess above 100 handed back by FUN_281f_0d58), price =
 * thunk_FUN_291f_09ea = FUN_38fd_0040 = euro_price−1, then
 * thunk_FUN_291f_0a2e → FUN_38fd_1dfa (viceroy 60247-60293) = the whole ledger:
 * the four market pools (record 3 damped ·2/3), nation+0xbc trade.tons += qty,
 * nation+0xfc trade.tons2 += qty, nation+0x7c trade.gold += (price·qty·
 * (100−tax))/100.  1f0c then RETURNS price·qty — the UNTAXED gross — and 0aba
 * adds exactly that to the treasury.
 *
 * So, against smell audit #61: tons2 was genuinely missing (fixed — 1dfa is
 * already ported as europe_apply_trade_volume(..., is_buy=0,
 * immediate_threshold=0); threshold 0 because 1f0c never calls FUN_38fd_0058),
 * the trade.gold ledger is tax-adjusted but the TREASURY credit is not, and
 * royal_money is NOT written here — the nation+0x22/+0x26 pair belongs to the
 * HUMAN harbor sale (viceroy 60557-60575), which applies the tax itself outside
 * 1f0c. The AI leg pays no Crown cut, like the 20e6 dump-sell tail; #61's
 * royal_money half is refuted, not fixed.
 */
static int ai_euro_5d04_cb_sell_hold0(int idx) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  /* Sentinel-aware: a 255 hold 0 is empty, not 255 units to sell at
   * euro_price−1 (smell audit sweep-3 area C #5). */
  if (!ctx || !u || units_hold_amount(ai_euro_s_5d04_ctx->units, u->id, 0) <= 0) {
    return 0;
  }
  /* Boycotted cargo stays aboard (europe_cargo_boycotted / boycott_bitmap). */
  {
    const int c0 = u->hold_goods_type[0];
    /* One accessor, one word (nation+0x20 — audit G6): the human branch used
     * to read the render mirror, which is stale outside the Europe screen. */
    const int boycotted =
      europe_cargo_boycotted_ex(ctx->europe, ctx->col1, ai_euro_s_5d04_nation, c0);
    if (boycotted) {
      return 0;
    }
  }
  int cargo = -1;
  int amount = 0;
  if (units_unload_goods_hold(ctx->units, u->id, 0, &cargo, &amount) <= 0 || cargo < 0) {
    return 0;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[ai_euro_s_5d04_nation];
  /* 0a2e → 1dfa: pools + trade.tons + trade.tons2 + tax-adjusted trade.gold.
   * Needs the shared EuropeScreen; headless (no eu) the ledger is skipped, the
   * treasury half below is col1-only and always applies (same shape as
   * ai_euro_20e6_delivery_sell_tail). */
  if (ctx->europe) {
    europe_apply_trade_volume(
      ctx->europe, ctx->col1, ai_euro_s_5d04_nation, ctx->human_nation, cargo, amount, 0, 0
    );
  }
  /* 1f0c's return → 0aba: the UNTAXED gross, price = euro_price−1. */
  const int gained = ai_euro_5d04_cb_sell_price(cargo) * amount;
  if (gained > 0) {
    nat->gold += (uint32_t)gained;
    ai_euro_5d04_cb_sync_gold();
  }
  return 1;
}
/* FUN_291f_0ec2: the ship departs — waiting dock units board first. */
static void ai_euro_5d04_cb_unit_exhaust(int idx) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  ColonizeUnit* ship = ai_euro_5d04_cb_unit(idx);
  if (!ctx || !ship || !units_is_sea(ctx->units, ship->id)) {
    return;
  }
  const int ship_id = ship->id;
  for (int i = ai_euro_5d04_cb_list_iter_first(0); i >= 0; i = ai_euro_5d04_cb_list_iter_next(i)) {
    ColonizeUnit* u = ai_euro_5d04_cb_unit(i);
    if (u->aboard_ship_id >= 0 || units_is_sea(ctx->units, u->id)) {
      continue;
    }
    const ColonizeUnitType* t = units_type(ctx->units, u->type_index);
    if (!t || t->space >= 99) {
      continue;
    }
    ColonizeUnit* sh = units_get(ctx->units, ship_id);
    if (!sh || sh->cargo_count >= units_ship_capacity(ctx->units, ship_id)) {
      break;
    }
    (void)units_board_stacked(ctx->units, u->id, ship_id);
  }
  /* The dispatcher's Europe act (FUN_48d3_048e teleport) takes it from here. */
}
/* FUN_291f_0d8e(unit, cargo, 100): buy 100 of cargo onto the ship. */
static void ai_euro_5d04_cb_apply_bump(int idx, int cargo, int qty) {
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  const ColonizeUnit* u = ai_euro_5d04_cb_unit(idx);
  if (!ctx || !u) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[ai_euro_s_5d04_nation];
  const uint32_t cost = (uint32_t)(ai_euro_5d04_cb_price(cargo) * qty);
  if (nat->gold < cost) {
    return;
  }
  const int loaded = units_load_goods(ctx->units, u->id, cargo, qty);
  if (loaded <= 0) {
    return;
  }
  nat->gold -= (uint32_t)(ai_euro_5d04_cb_price(cargo) * loaded);
  ai_euro_5d04_cb_sync_gold();
  ai_euro_5d04_cb_market_volume(cargo, loaded, 1);
}
/* FUN_281f_095c(0xb, nation, nation-0x14, nation-0x14): Artillery in Europe. */
static int ai_euro_5d04_cb_goal_trigger(int code, int a, int b, int c) {
  (void)a;
  (void)b;
  (void)c;
  ColonizeTurnContext* ctx = ai_euro_s_5d04_ctx;
  const int lt = ctx && ctx->units ? ai_euro_5d04_linux_type_for(ctx->units, code) : -1;
  if (lt < 0) {
    return -1;
  }
  const int id = units_spawn_allow_stack(ctx->units, lt, 200, 100);
  ColonizeUnit* u = id >= 0 ? units_get(ctx->units, id) : NULL;
  if (!u) {
    return -1;
  }
  units_set_nation(u, ai_euro_s_5d04_nation);
  u->moves = 0;
  return (int)(u - ctx->units->units);
}
/*
 * FUN_521d_6d8e prelude (raw 93107-93172, the writer of every DS byte the
 * 5d04 hire matrix reads) — decoded 2026-09-07e:
 *   DS:0xa0db += 1 per own colony whose `+0x8d` specialty_cargo == 0x0f
 *                (Muskets) and again per own colony whose `+0xb8`
 *                stock[15] (Muskets) is 0.
 *   DS:0xa0da += 1 per own colony whose `+0xb6` stock[14] (Tools) is 0,
 *                then −= 1 per own unit of type 0x02 (Pioneer) — the
 *                Pioneers already in the field cancel the tools demand.
 *   (DS:0xa0d4 is the same tally for `+0xaa` stock[8] Horses; 5d04 never
 *   reads it, so it is not modelled.)
 * Colony stock offsets pinned from col1_save.h (`stock[16]` u16 @ +0x9a):
 * +0xaa = 8 Horses, +0xb6 = 14 Tools, +0xb8 = 15 Muskets.
 */
static void ai_euro_5d04_cb_colony_needs(int nation_id, int* out_muskets, int* out_tools) {
  int m = 0;
  int t = 0;
  if (ai_euro_s_5d04_ctx && ai_euro_s_5d04_ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ai_euro_s_5d04_ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (c->specialty_cargo == COLONIZE_CARGO_MUSKETS) {
        m++;
      }
      if (c->stock[COLONIZE_CARGO_MUSKETS] == 0) {
        m++;
      }
      if (c->stock[COLONIZE_CARGO_TOOLS] == 0) {
        t++;
      }
    }
  }
  /* raw 93168-93170: every own Pioneer (@UNIT type 0x02) decrements 0xa0da. */
  if (ai_euro_s_5d04_ctx && ai_euro_s_5d04_ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ai_euro_s_5d04_ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (ai_euro_5d04_dos_type_of(ai_euro_s_5d04_ctx->units, u->type_index) == 2) {
        t--;
      }
    }
  }
  *out_muskets = m;
  *out_tools = t;
}

/*
 * DS:0xa0b8[nation] (`param_1 + -0x5f48`) — resolved 2026-09-07e from its
 * sole writer, the FUN_521d_6d8e prelude (raw 93109 zeroes it, raw
 * 93139-93141 bumps it): the number of this nation's colonies whose AI
 * byte `+0x1b` has bit 0x10 set — COLONIZE_COLONY_AI_NEEDS_COLONISTS.
 * (The only other reader, raw 87069 in 0a60, gates on the same "no
 * colonies OR nobody wants colonists" pair, which corroborates it.)
 * Replaces the old `inv->found_flags` construction-count stand-in.
 */
static int ai_euro_5d04_cb_colonies_wanting_colonists(int nation_id) {
  int n = 0;
  if (!ai_euro_s_5d04_ctx || !ai_euro_s_5d04_ctx->colonies) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ai_euro_s_5d04_ctx->colonies->colonies[i];
    if (c->active && c->nation_id == nation_id &&
        (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) != 0) {
      n++;
    }
  }
  return n > 127 ? 127 : n;
}

/*
 * DS:0x945a[nation] (`param_1 + -0x6ba6`) — resolved 2026-09-07e from the
 * census writer FUN_4962_0018 (raw 78147 zeroes it, raw 78167-78170 bumps
 * it): a NON-ship unit (`+0x3146` outside 0x0d..0x12) whose map x byte
 * `+0x3144` satisfies `x - nation == -0x14` is a unit standing on that
 * nation's Europe dock, so the byte is simply **how many land units this
 * nation currently has waiting in Europe**. It seeds `local_46`, the
 * "how badly does a departing ship need cargo" threshold, in the tail's
 * final loop. (Raw 77748 is the same tally under a different base.)
 */
static int ai_euro_5d04_cb_europe_land_units(int nation_id) {
  int n = 0;
  if (!ai_euro_s_5d04_ctx || !ai_euro_s_5d04_ctx->units) {
    return 0;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ai_euro_s_5d04_ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ai_euro_s_5d04_ctx->units, u->id)) {
      continue;
    }
    if (ai_euro_in_europe(u->x, u->y)) {
      n++;
    }
  }
  return n > 127 ? 127 : n;
}

/*
 * DS:0xa0cc[16] (`local_40 + -0x5f34`) — resolved 2026-09-07e. Built by
 * the FUN_521d_6d8e prelude, once per nation-turn, immediately before it
 * calls 5d04 through `thunk_FUN_2a1f_0554`:
 *   raw 93107  memset(0xa0cc, 0, 0x10)
 *   raw 93122  per own colony with `+0x8d` >= 0: demand[specialty]++
 *   raw 93146  memcpy(0xa0bc, 0xa0cc, 0x10)  (untouched snapshot)
 *   raw 93163  per own SHIP (type 0x0d..0x12), per occupied hold slot
 *              0..`+0x3150`: demand[hold cargo]--
 * so it is a **per-cargo demand table**: how many of this nation's
 * colonies specialise in that cargo, minus how much of it is already
 * afloat. 5d04's departing-ship loop walks cargo 15..0 and buys 100 of
 * the first cargo whose demand clears `local_46`, decrementing the cell.
 * This replaces the old `inv->profession_demand[]` stand-in, which was
 * profession-indexed but was being read here with a cargo index.
 */
static void ai_euro_5d04_cb_cargo_demand(int nation_id, int8_t out[16]) {
  memset(out, 0, 16 * sizeof(out[0]));
  if (!ai_euro_s_5d04_ctx) {
    return;
  }
  if (ai_euro_s_5d04_ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ai_euro_s_5d04_ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (c->specialty_cargo < 16) {
        out[c->specialty_cargo]++;
      }
    }
  }
  if (ai_euro_s_5d04_ctx->units) {
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ai_euro_s_5d04_ctx->units->units[i];
      if (!u->active || u->nation_id != nation_id) {
        continue;
      }
      if (!units_is_sea(ai_euro_s_5d04_ctx->units, u->id)) {
        continue;
      }
      const int holds = units_goods_hold_count(ai_euro_s_5d04_ctx->units, u->id);
      for (int h = 0; h < holds && h < COLONIZE_UNIT_CARGO_MAX; ++h) {
        /* Sentinel-aware, hold-count bound: a 255 hold is empty, not cargo
         * already in transit (smell audit sweep-3 area C #5). */
        if (units_hold_amount(ai_euro_s_5d04_ctx->units, u->id, h) > 0 && u->hold_goods_type[h] >= 0 &&
            u->hold_goods_type[h] < 16) {
          out[u->hold_goods_type[h]]--;
        }
      }
    }
  }
}

Ai5d04HireScratch ai_euro_s_5d04_hire_scratch[4];

/* --- 5d04 hire-ladder tail: shared frame + stages ---------------------- */

/* Shared locals of the 86065-86561 hire ladder tail. DOS keeps them in one
 * stack frame across all three phases; the port threads the same frame
 * through this struct so the phases below are verbatim transcriptions with
 * no renaming (each stage aliases the fields back to their raw local names
 * on entry and writes the mutated ones back on exit). */
typedef struct Ai5d04HireTail {
  ColonizeTurnContext* ctx;
  int nation_id;
  const Ai5d04PlanningFlags* f;
  ColonizeCol1Nation* nat;
  const ColonizeCol1Stuff* stuff;
  Ai5d04HireScratch* hs;
  int turn;
  int difficulty;
  int woi;
  int local_16;
  int local_34;
  int local_8;
  int has_any_colony;
  int unit_flag_bit5;
  int every_third_turn;
  int expand_signal;
  int local_28;
  int bVar9;
  int bVar10;
  int local_24;
} Ai5d04HireTail;

static void ai_euro_5d04_hire_tail_candidates(Ai5d04HireTail* t) {
  ColonizeTurnContext* ctx = t->ctx;
  const int nation_id = t->nation_id;
  const Ai5d04PlanningFlags* f = t->f;
  ColonizeCol1Nation* nat = t->nat;
  const ColonizeCol1Stuff* stuff = t->stuff;
  Ai5d04HireScratch* hs = t->hs;
  const int turn = t->turn;
  const int woi = t->woi;
  const int has_any_colony = t->has_any_colony;
  const int unit_flag_bit5 = t->unit_flag_bit5;
  const int every_third_turn = t->every_third_turn;
  int local_8 = t->local_8;
  int local_28 = t->local_28;
  int bVar10 = t->bVar10;

  /* raw 86122-86306: two-pass candidate loop (local_3a = 0, 1). */
  for (int local_3a = 0; local_3a < 2; ++local_3a) {
    int idx = ai_euro_5d04_cb_list_iter_first(-1);
    while (idx >= 0) {
      int next_idx = idx;
      if (!woi) {
        const int skilled = ai_euro_5d04_cb_unit_is_skilled(idx);
        if (skilled >= 0) {
          const int gate = ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx));
          int run_body = 0;
          if (gate == 0) {
            if (local_3a == 0) {
              run_body = 1;
            }
          } else if (local_3a != 0) {
            run_body = 1;
          }
          if (run_body) {
            if (ai_euro_5d04_cb_unit_dispatch_byte(idx) == 2) {
              local_28 |= local_8;
            }
            int handled = 0;
            if (ai_euro_5d04_cb_unit_dispatch_byte(idx) == 0 &&
                (!has_any_colony || (!unit_flag_bit5 && !every_third_turn))) {
              const int gate2 = ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx));
              const int local_c = gate2 != 0;
              /* raw 92658-92671. Two ways into the muskets training arm
               * (LAB_521d_6454):
               *   0xa0db <= 0  → only the LAB_521d_642a body;
               *   0xa0db >= 1  → RNG(0, local_c+1) == 0 trains outright,
               *                  and a MISS falls THROUGH into 642a (the
               *                  raw `goto LAB_521d_642a`) for a second
               *                  chance. That fall-through was missing
               *                  before 2026-09-07e. */
              int try_train = 0;
              int try_642a = 0;
              if (hs->colonies_need_muskets <= 0) {
                try_642a = 1;
              } else if (dos_rng_range(ctx->rng, 0, local_c + 1) == 0) {
                try_train = 1;
              } else {
                try_642a = 1;
              }
              if (try_642a && local_8 != 0 &&
                  dos_rng_range(ctx->rng, 0, local_c + 2) == 0 && turn > 99) {
                try_train = 1;
              }
              if (try_train) {
                /* LAB_521d_6454: tools-side training. */
                uint32_t local_1a = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_MUSKETS) * 50);
                if (hs->musket_bank_lots != 0) {
                  local_1a = 0;
                }
                if (nat->gold >= local_1a && !f->cargo_short) {
                  if (hs->musket_bank_lots == 0) {
                    ai_euro_5d04_cb_set_pool_counter(0xf, 0x32);
                  } else {
                    hs->musket_bank_lots--;
                  }
                  nat->gold -= local_1a;
                  ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 1);
                  if (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx)) != 0) {
                    int swapped = 0x1c;
                    for (int j = 0; j < 3; ++j) {
                      if (ai_euro_5d04_cb_profession_gate(nat->recruit[j]) == 0) {
                        swapped = nat->recruit[j];
                        nat->recruit[j] = (uint8_t)ai_euro_5d04_cb_unit_profession(idx);
                        break;
                      }
                    }
                    ai_euro_5d04_cb_set_unit_profession(idx, swapped);
                  }
                  local_8 = 0;
                  bVar10 = 1;
                  hs->colonies_need_muskets--;
                  if (f->has_college &&
                      dos_rng_range(
                        ctx->rng, 0,
                        stuff->unit_type_counts[nation_id][4] + stuff->unit_type_counts[nation_id][1]
                      ) <= stuff->veteran_teach_threshold[nation_id]) {
                    ai_euro_5d04_cb_set_unit_profession(idx, 0x15); /* Veteran Soldier */
                  }
                  uint32_t local_1a2 = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_HORSES) * 50);
                  if ((uint32_t)hs->musket_bank_raw > 0x31) {
                    local_1a2 = 0;
                  }
                  if (nat->gold >= local_1a2) {
                    nat->gold -= local_1a2;
                    ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 4);
                    if (hs->musket_bank_raw < 0x32) {
                      /* raw 92729-92733: DOS only calls FUN_291f_0c14(8,0x32)
                       * here — the old `= 0x32` write-back was invented. */
                      ai_euro_5d04_cb_set_pool_counter(8, 0x32);
                    } else {
                      hs->musket_bank_raw -= 0x32;
                    }
                  }
                  handled = 1;
                }
              }
              if (!handled && ai_euro_5d04_cb_unit_dispatch_byte(idx) == 0 &&
                  local_8 - (int)stuff->unit_type_counts[nation_id][2] > 0 &&
                  dos_rng_range(ctx->rng, 0, 2) == 0 &&
                  local_28 == 0) {
                /* Tools-side (Pioneer) training, raw 92742-92783.
                 * raw 92745-92748: past turn 99 the arm is skipped whenever
                 * RNG(0,2) <= the nation's own Pioneer count
                 * (`param_1*0x13 + -0x6db2` = unit_type_counts[n][2]) — the
                 * more Pioneers already in the field, the less likely a new
                 * one. Fixed 2026-09-07e: the port read free_colonist_counts
                 * and inverted the sense. */
                int proceed = 1;
                if (turn > 99) {
                  proceed = !(dos_rng_range(ctx->rng, 0, 2) <=
                              (int)stuff->unit_type_counts[nation_id][2]);
                }
                if (proceed &&
                    (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx)) == 0 ||
                     dos_rng_range(ctx->rng, 0, 4) == 0)) {
                  const uint32_t cost = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_TOOLS) * 100);
                  if (nat->gold >= cost) {
                    nat->gold -= cost;
                    ai_euro_5d04_cb_set_pool_counter(0xe, 100);
                    ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 2);
                    local_28 = local_8;
                    local_8 = 0;
                    bVar10 = 1;
                    hs->colonies_need_tools--;
                    handled = 1;
                  }
                }
              }
            }
            if (!handled && ai_euro_5d04_cb_unit_dispatch_byte(idx) == 0 &&
                stuff->unit_type_counts[nation_id][3] == 0 && turn > 0x32) {
              int proceed = 1;
              if (turn > 199) {
                /* DOS-LITERAL FUN_521d_5d04 raw 92785-92788: `iVar14 =
                 * FUN_281f_04d4(iVar19,0,3); if (iVar14 != 0) goto
                 * LAB_521d_638a;` — skip the arm unless the roll lands on 0. */
                proceed = dos_rng_range(ctx->rng, 0, 3) == 0;
              }
              if (proceed && (turn % 7) == 0) {
                if (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(idx)) == 0 ||
                    dos_rng_range(ctx->rng, 0, 7) == 0) {
                  ai_euro_5d04_cb_set_unit_dispatch_byte(idx, 3);
                  /* raw 92798-92799: DOS bumps `param_1*0x13 + -0x6db1`
                   * (= unit_type_counts[n][3], the Missionary row) right
                   * here, which is what stops a second Missionary in the
                   * same pass. Mirrored 2026-09-07e; the census pass
                   * recomputes the row from scratch next turn. */
                  if (ctx->col1->stuff.unit_type_counts[nation_id][3] < 0xff) {
                    ctx->col1->stuff.unit_type_counts[nation_id][3]++;
                  }
                }
              }
            }
          }
        }
      }
      next_idx = ai_euro_5d04_cb_list_iter_next(idx);
      idx = next_idx;
    }
  }

  t->local_8 = local_8;
  t->local_28 = local_28;
  t->bVar10 = bVar10;
}

static void ai_euro_5d04_hire_tail_colony_demand(Ai5d04HireTail* t) {
  ColonizeTurnContext* ctx = t->ctx;
  const int nation_id = t->nation_id;
  const Ai5d04PlanningFlags* f = t->f;
  ColonizeCol1Nation* nat = t->nat;
  const ColonizeCol1Stuff* stuff = t->stuff;
  Ai5d04HireScratch* hs = t->hs;
  const int turn = t->turn;
  const int difficulty = t->difficulty;
  const int woi = t->woi;
  const int local_16 = t->local_16;
  const int local_34 = t->local_34;
  const int has_any_colony = t->has_any_colony;
  const int unit_flag_bit5 = t->unit_flag_bit5;
  const int every_third_turn = t->every_third_turn;
  int bVar9 = t->bVar9;

  /* raw 86307-86479: colony demand vs. purchase loop. `local_24` is read
   * by the final loop below regardless of whether this block runs (the
   * raw decompile shows the same cross-block read — DOS quirk, mirrored
   * here with an explicit 0 default rather than leaving it
   * uninitialized). */
  int local_24 = 0;
  int local_22 = ai_euro_5d04_cb_colony_demand_query(local_16, 4);
  if (woi) {
    local_22 += ai_euro_5d04_cb_colony_demand_query(local_16, 0xc);
  }
  if (local_22 != 0 && !f->cargo_short &&
      (!has_any_colony || (!unit_flag_bit5 && !every_third_turn && !woi))) {
    local_24 = ai_euro_5d04_cb_colony_demand_query(local_16, 0xe);
    int local_42 = local_24 - local_22;
    if (turn > 0x50) {
      while ((uint32_t)(hs->delay_48 + 1) < (uint32_t)hs->musket_bank_raw / 50) {
        hs->musket_bank_raw -= 50;
        hs->delay_48++;
      }
      while ((uint32_t)hs->musket_bank_raw / 50 + 1 < (uint32_t)hs->delay_48) {
        hs->delay_48--;
        hs->musket_bank_raw += 50;
      }
    }
    if (local_34 == 0 && local_24 > 5) {
      uint32_t adj = 0;
      if (turn > 0x27) {
        adj = (uint32_t)((difficulty - 10) * -100);
      }
      if (hs->delay_48 != 0) {
        hs->delay_48--;
        adj = 0;
      }
      if (nat->gold >= adj && ai_euro_5d04_cb_goal_trigger(0xb, nation_id, nation_id - 0x14, nation_id - 0x14) >= 0) {
        local_42--;
        nat->gold -= adj;
      }
    }
    int bVar8_2 = 0;
    int buy_guard = 0;
    do {
      if (++buy_guard > 64) {
        break;
      }
      bVar8_2 = 0;
      const int base2 = (((int)nat->recruit_count + 7) * 2 - (difficulty & 0xfe)) * 10;
      const long extra =
        ((long)base2 * (long)nat->current_crosses) / (-1L - (long)nat->needed_crosses);
      uint32_t local_38b = (uint32_t)(base2 + extra);
      if (hs->musket_bank_lots == 0) {
        local_38b += (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_MUSKETS) * 50);
      }
      if (turn > 99) {
        local_38b += (uint32_t)((int)(difficulty * (int)local_38b * 10) / -100);
      }
      if (nat->gold >= local_38b) {
        const int slot = dos_rng_range(ctx->rng, 0, 2);
        const int cand = ai_euro_5d04_cb_dock_pop_candidate(nat->recruit[slot]);
        if (cand < 0) {
          break;
        }
        const int cdisp = ai_euro_5d04_cb_unit_dispatch_byte(cand);
        int extra_cost = 0;
        if (cdisp == 1 || cdisp == 4) {
          if (hs->musket_bank_lots == 0) {
            extra_cost = ai_euro_5d04_cb_price(COLONIZE_CARGO_MUSKETS) * -50;
            ai_euro_5d04_cb_market_volume(COLONIZE_CARGO_MUSKETS, 50, 0); /* FUN_291f_0a2e */
          } else {
            hs->musket_bank_lots++;
          }
        } else if (cdisp == 2) {
          extra_cost = ai_euro_5d04_cb_price(COLONIZE_CARGO_TOOLS) * -100;
          ai_euro_5d04_cb_market_volume(COLONIZE_CARGO_TOOLS, 100, 0); /* FUN_291f_0a2e */
        } else if (cdisp == 5) {
          ai_euro_5d04_cb_set_unit_dispatch_byte(cand, 4);
        }
        local_38b += (uint32_t)extra_cost;
        if (ai_euro_5d04_cb_unit_dispatch_byte(cand) != 4) {
          ai_euro_5d04_cb_set_unit_dispatch_byte(cand, 1);
          if (ai_euro_5d04_cb_profession_gate(ai_euro_5d04_cb_unit_profession(cand)) != 0) {
            int swapped = 0x1c;
            for (int j = 0; j < 3; ++j) {
              if (ai_euro_5d04_cb_profession_gate(nat->recruit[j]) == 0) {
                swapped = nat->recruit[j];
                nat->recruit[j] = (uint8_t)ai_euro_5d04_cb_unit_profession(cand);
                break;
              }
            }
            ai_euro_5d04_cb_set_unit_profession(cand, swapped);
          }
        }
        nat->gold -= local_38b;
        if (hs->musket_bank_lots == 0) {
          ai_euro_5d04_cb_set_pool_counter(0xf, 0x32);
        } else {
          hs->musket_bank_lots--;
        }
        if (f->has_college && ai_euro_5d04_cb_unit_profession(cand) != 0x15) {
          const int roll4 = dos_rng_range(
            ctx->rng, 0,
            stuff->unit_type_counts[nation_id][4] + stuff->unit_type_counts[nation_id][1]
          );
          if (roll4 <= (int)stuff->veteran_teach_threshold[nation_id]) {
            ai_euro_5d04_cb_set_unit_profession(cand, 0x15);
          }
        }
        uint32_t local_1a3 = (uint32_t)(ai_euro_5d04_cb_price(COLONIZE_CARGO_HORSES) * 50);
        if (turn > 99) {
          local_1a3 += (uint32_t)((int)(difficulty * (int)local_1a3 * 10) / -100);
        }
        if ((uint32_t)hs->musket_bank_raw > 0x31) {
          local_1a3 = 0;
        }
        if (nat->gold >= local_1a3) {
          nat->gold -= local_1a3;
        }
        ai_euro_5d04_cb_set_unit_dispatch_byte(cand, 4);
        if (hs->musket_bank_raw < 0x32) {
          ai_euro_5d04_cb_set_pool_counter(8, 0x32);
        } else {
          hs->musket_bank_raw -= 0x32;
        }
        (void)ai_euro_5d04_refill_pool_slot(slot); /* FUN_38fd_46d4 */
        bVar9 = 1;
        bVar8_2 = 1;
        /* DS:0x5238[type] is indexed by the DOS @UNIT code, not by a Linux
         * pool index — translate as every other consumer of the dispatch
         * byte does (fixed 2026-09-09; the raw index only happened to agree
         * on a NAMES-ordered pool). */
        local_42 -= ai_euro_5d04_dos_type_space(ctx->units,
                                                ai_euro_5d04_cb_unit_dispatch_byte(cand));
      }
      if (!bVar8_2 || local_42 < 1) {
        break;
      }
    } while (1);
  }

  t->bVar9 = bVar9;
  t->local_24 = local_24;
}

static void ai_euro_5d04_hire_tail_departing_ships(Ai5d04HireTail* t) {
  ColonizeTurnContext* ctx = t->ctx;
  const int nation_id = t->nation_id;
  const Ai5d04PlanningFlags* f = t->f;
  ColonizeCol1Nation* nat = t->nat;
  Ai5d04HireScratch* hs = t->hs;
  const int turn = t->turn;
  const int has_any_colony = t->has_any_colony;
  const int expand_signal = t->expand_signal;
  const int local_24 = t->local_24;
  const int local_28 = t->local_28;
  int bVar9 = t->bVar9;
  const int bVar10 = t->bVar10;

  /* raw 92983-93070: the departing-ship loop — sell/bank the hold, then
   * top the hull up with the cargo its colonies most want.
   * raw 92983-92988: `local_46` = DS:0x945a[nation] (land units this
   * nation has waiting in Europe) + the turn parity bit — real since
   * 2026-09-07e, was a hard 0. It is the bar a cargo's demand cell has to
   * clear before the ship buys 100 of it, so the more colonists are
   * queued on the dock, the pickier the ship gets about goods. */
  const int seed46 = ai_euro_5d04_cb_europe_land_units(nation_id);
  int local_46 = seed46 + ((turn & 1) != 0);
  /* raw 93036/93050 `local_40 + -0x5f34` = DS:0xa0cc[16], the per-cargo
   * demand table 6d8e rebuilds just before calling 5d04. Real since
   * 2026-09-07e (was `inv->profession_demand[]`, a profession-indexed
   * array being read with a cargo index). */
  int8_t cargo_demand[16];
  ai_euro_5d04_cb_cargo_demand(nation_id, cargo_demand);
  int matched;
  /* DOS drops a departed ship from the Europe stack (FUN_291f_0ec2); Linux
   * leaves it at the Europe coords until the dispatcher's own act teleports
   * it, so remember which ships this pass already handled. */
  uint8_t departed[COLONIZE_UNITS_MAX];
  memset(departed, 0, sizeof(departed));
  int restarts = 0;
  do {
    matched = 0;
    if (++restarts > COLONIZE_UNITS_MAX) {
      break;
    }
    int idx2 = ai_euro_5d04_cb_list_iter_first(-1);
    while (!matched && idx2 >= 0) {
      int next2 = idx2;
      const ColonizeUnit* ship2 = ai_euro_5d04_cb_unit(idx2);
      const int flags3148 = (ship2 && ship2->col1_flags15 & 0x80) ? 0x80 : 0; /* damaged */
      const int dispatch2 = ai_euro_5d04_cb_unit_dispatch_byte(idx2);
      if (!departed[idx2] && ((flags3148 & 0x80) == 0 || dispatch2 == 0x0b) &&
          dispatch2 > 0xc && dispatch2 < 0x13) {
        departed[idx2] = 1;
        /* raw 92996: a ship leaving Europe drops its colony binding
         * (`+0x314a` = col1_origin, 0xff = unbound). Ported 2026-09-07e —
         * the 4393 pick and the 20e6 arrival gate both read this byte. */
        {
          ColonizeUnit* wship = ai_euro_5d04_cb_unit(idx2);
          if (wship) {
            wship->col1_origin = 0xff;
          }
        }
        /* unload/sell loop over hold 0 (raw `while (unit+0x3150 != 0)`). */
        int last_lots = 0; /* DS:0x8dc4 scratch */
        for (int guard = 0; guard < COLONIZE_UNIT_CARGO_MAX + 1; ++guard) {
          const int kind = ai_euro_5d04_cb_reward_case(idx2);
          if (kind < 0) {
            break;
          }
          if (kind == COLONIZE_CARGO_MUSKETS) {
            const int v = ai_euro_5d04_cb_reward_value(idx2);
            last_lots = (v + 0x31) / 0x32;
            hs->musket_bank_lots = (int8_t)(hs->musket_bank_lots + last_lots);
            ai_euro_5d04_cb_reward_ack(idx2);
          } else if (kind == COLONIZE_CARGO_HORSES) {
            ai_euro_5d04_cb_reward_ack(idx2);
            hs->musket_bank_raw += last_lots; /* DOS reuses the stale 0x8dc4 lots value */
          } else if (!ai_euro_5d04_cb_sell_hold0(idx2)) {
            break; /* boycotted hold stays aboard */
          }
        }
        if (bVar9 && units_ship_capacity(ctx->units, ship2->id) == local_24) {
          bVar9 = 0;
          matched = 1;
          ai_euro_5d04_cb_unit_exhaust(idx2);
          next2 = idx2;
        } else {
          for (int p = 15; p >= 0; --p) {
            /* raw: break when 0x5237[type] (capacity) == unit+0x3150 (cargo). */
            const ColonizeUnit* sh = ai_euro_5d04_cb_unit(idx2);
            const int cap = units_ship_capacity(ctx->units, sh->id);
            /* Goods-only, as every +0x3150 read at raw 93020-93039 is
             * (bugs.md #530 side lead; same finding as #527): a passenger
             * never occupies a DOS hold, so a transport carrying colonists
             * still buys cargo here. */
            const int used = ai_euro_0a60_goods_holds_used(ctx->units, sh);
            if (cap == used || f->cargo_short || !has_any_colony) {
              break;
            }
            if ((local_46 <= (int)cargo_demand[p] || expand_signal) &&
                ((!bVar10 && local_28 == 0) || (cap - used > 2 || expand_signal))) {
              if (nat->gold >= (uint32_t)(ai_euro_5d04_cb_price(p) * 100)) {
                ai_euro_5d04_cb_apply_bump(idx2, p, 100);
                cargo_demand[p]--; /* raw 93050 */
              }
            }
          }
          ai_euro_5d04_cb_unit_exhaust(idx2);
          matched = 1;
          next2 = idx2;
        }
      }
      next2 = ai_euro_5d04_cb_list_iter_next(idx2);
      idx2 = next2;
    }
  } while (matched);

  t->bVar9 = bVar9;
}

/*
 * Raw 86065-86561 — Europe hire ladder + profession/reward tail. DOS is one
 * function body whose locals thread across all three phases in one scope;
 * the port keeps that single frame in `Ai5d04HireTail` and hands it to the
 * three stage functions above, each of which aliases the fields back to
 * their raw local names so the transcriptions stay verbatim. `goto` labels
 * below are abbreviated from the raw `LAB_521d_XXXX` names so this can be
 * cross-checked against the decompile directly. First-draft quality —
 * this is a large, dense transcription; expect bugs in the deep nested
 * arithmetic even where the shape is right, same standard the rest of
 * this project's large first-pass ports were held to.
 */
static void ai_euro_5d04_hire_ladder_tail(
  ColonizeTurnContext* ctx, int nation_id, const Ai5d04PlanningFlags* f
) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  ColonizeCol1Nation* nat = &ctx->col1->nation[nation_id];
  const ColonizeCol1Head* head = &ctx->col1->head;
  const ColonizeCol1Stuff* stuff = &ctx->col1->stuff;
  const int turn = (int)head->turn;
  const int difficulty = (int)head->difficulty;
  const int woi = head->game_options.woi != 0;
  Ai5d04HireScratch* hs = &ai_euro_s_5d04_hire_scratch[nation_id];
  ai_euro_s_5d04_ctx = ctx;
  ai_euro_s_5d04_nation = nation_id;
  {
    int need_m = 0;
    int need_t = 0;
    ai_euro_5d04_cb_colony_needs(nation_id, &need_m, &need_t);
    if (need_m > 127) { need_m = 127; }
    if (need_m < -128) { need_m = -128; }
    if (need_t > 127) { need_t = 127; }
    if (need_t < -128) { need_t = -128; } /* the Pioneer subtraction can go negative */
    hs->colonies_need_muskets = (int8_t)need_m;
    hs->colonies_need_tools = (int8_t)need_t;
  }

  /* Raw 92569-92578: no Artillery on the Europe dock + colonies needing
   * muskets → buy one (purchase table entry 0, 500 gold), re-query. */
  int local_16 = ai_euro_5d04_cb_list_iter_first(0x0c);
  int local_34 = ai_euro_5d04_cb_artillery_on_dock(local_16);
  if (local_34 == 0 && !woi && hs->colonies_need_muskets > 0 &&
      dos_rng_range(ctx->rng, 0, 3) == 0 && !f->cargo_short &&
      stuff->ship_cargo_totals[nation_id] > 4) {
    (void)ai_euro_5d04_propose_ship_buy(ctx, nation_id, 0);
    local_16 = ai_euro_5d04_cb_list_iter_first(0x0c);
    local_34 = ai_euro_5d04_cb_artillery_on_dock(local_16);
  }

  /* raw 86076-86084: local_8 = per-nation hire-mask; bVar8 = any unit in
   * the 0xc list whose dispatch byte falls outside the ship range. */
  int local_8 = ai_euro_5d04_cb_nation_hire_mask(nation_id);
  int bVar8 = 0;
  {
    int idx = local_16;
    while (idx >= 0) {
      const int dispatch = ai_euro_5d04_cb_unit_dispatch_byte(idx);
      if (dispatch < 0xd || dispatch > 0x12) {
        bVar8 = 1;
      }
      idx = ai_euro_5d04_cb_list_iter_next(idx);
    }
  }

  /* raw 86085-86092: fresh local booleans — DOS reuses the same stack
   * slots `bVar21`/`bVar22`/`bVar23`/`bVar24` for a NEW meaning here,
   * unrelated to the gate-cascade flags of the same raw names earlier in
   * the function; fresh C names to avoid confusion with `f->*`. */
  const int every_third_turn = (turn % 3) == 0;
  /* unit+0x3148 bit 0x20 of the last list-walk cursor, which is -1 (past
   * end) by the time this reads it in the raw body — an artifact of
   * DOS's register reuse, not a meaningful read. Structural placeholder. */
  const int unit_flag_bit5 = 0;
  const int has_any_colony = stuff->colony_counts[nation_id] != 0;
  /* raw 92595 `-0x5f48` = DS:0xa0b8[nation] — own colonies flagged
   * NEEDS_COLONISTS. Real since 2026-09-07e (was `inv->found_flags`). */
  const int colonies_want_colonists = ai_euro_5d04_cb_colonies_wanting_colonists(nation_id);
  const int expand_signal = has_any_colony && (unit_flag_bit5 || every_third_turn);

  /* raw 92592-92625: gold-spend recruit-slot swap (Europe recruit price
   * falls with accumulated crosses: base + base*crosses/(-1-needed)). */
  if (!woi && !bVar8 && !f->cargo_short &&
      (!has_any_colony ||
       (!unit_flag_bit5 && !every_third_turn &&
        (stuff->colony_counts[nation_id] >> 1) <=
          colonies_want_colonists - stuff->free_colonist_counts[nation_id]))) {
    const int base = ((int)nat->recruit_count - difficulty + 7) * 20;
    const long scaled =
      ((long)base * (long)nat->current_crosses) / (-1L - (long)nat->needed_crosses);
    /* raw 92601 reads `-0x6bf0` = census_pop_proxy, not free_colonist_counts
     * (fixed 2026-09-07e — the same −0x6bf0/−0x6bf8 mix-up the ladder gate
     * had before 2026-09-07d). */
    int reserve = ((int)stuff->census_pop_proxy[nation_id] * 30 - turn) * 2;
    if (reserve < 0) {
      reserve = 0;
    }
    const long local_38 = scaled + base;
    if (nat->gold >= (uint32_t)(local_38 + reserve)) {
      nat->gold -= (uint32_t)local_38;
      const int slot = dos_rng_range(ctx->rng, 0, 2); /* nat->recruit[3] */
      const int candidate = ai_euro_5d04_cb_dock_pop_candidate(nat->recruit[slot]);
      if (candidate >= 0) {
        (void)ai_euro_5d04_refill_pool_slot(slot); /* FUN_38fd_46d4 */
        local_16 = candidate;
      }
    }
  }

  int local_28 = 0;
  int bVar9 = 0;   /* "a hire/train happened this pass" */
  int bVar10 = 0;  /* "tools-side training happened" */

  Ai5d04HireTail tail = {
    .ctx = ctx,
    .nation_id = nation_id,
    .f = f,
    .nat = nat,
    .stuff = stuff,
    .hs = hs,
    .turn = turn,
    .difficulty = difficulty,
    .woi = woi,
    .local_16 = local_16,
    .local_34 = local_34,
    .local_8 = local_8,
    .has_any_colony = has_any_colony,
    .unit_flag_bit5 = unit_flag_bit5,
    .every_third_turn = every_third_turn,
    .expand_signal = expand_signal,
    .local_28 = local_28,
    .bVar9 = bVar9,
    .bVar10 = bVar10,
    .local_24 = 0
  };
  ai_euro_5d04_hire_tail_candidates(&tail);
  ai_euro_5d04_hire_tail_colony_demand(&tail);
  ai_euro_5d04_hire_tail_departing_ships(&tail);
}

/*
 * FUN_521d_5d04 — full port, orchestrator (structural 2026-08-19, fully
 * live 2026-09-07d). Mirrors the raw function's own top-level order:
 * treasury bump → gate cascade (with the WoI Man-O-War seizure) → naval
 * gold floors (raw 92476-92505 — applied in DOS, so applied here) →
 * ship-buy candidate ladder with real FUN_521d_5c3c purchases (raw
 * 92533-92567) → hire-ladder tail, itself gated on raw 92568
 * `bought-a-ship || has-ships`.
 */
/* Returns 1 when the raw body's early `return;` fired (ship-buy ladder abort
 * — reachable since the naval-threat crumbs went live 2026-09-07); the live
 * caller must then skip its thin hire matrix too, as DOS skips the whole
 * hire ladder. */
static int ai_euro_5d04_nation_planning_structural(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  ai_euro_5d04_treasury_bump(ctx, nation_id);
  const Ai5d04PlanningFlags f = ai_euro_5d04_compute_flags(ctx, nation_id);
  ai_euro_5d04_apply_naval_gold_floors(ctx, nation_id, &f);
  int abort_early = 0;
  const int candidate = ai_euro_5d04_ship_buy_ladder(ctx, nation_id, &f, &abort_early);
  if (abort_early) {
    return 1;
  }
  /* Raw 92568 `if (local_3e != 0 || !bVar21)`: a shipless nation that
   * bought nothing skips the whole hire tail. */
  if (candidate != 0 || !f.no_ships) {
    ai_euro_5d04_hire_ladder_tail(ctx, nation_id, &f);
  }
  return 0;
}

void ai_euro_nation_planning(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || nation_id < 0 || nation_id >= 4) {
    return;
  }
  /*
   * T3.1 (2026-08-27): the structural 5d04 orchestrator is the live entry.
   * 2026-09-07d: fully real — naval gold floors applied, FUN_521d_5c3c
   * Europe purchases live (the thin ship-buy ladder that used to sit below
   * is retired; the DOS ladder + Artillery dock buy cover it).
   *
   * 2026-09-07e: the Linux-shaped hire matrix that used to run *after* the
   * orchestrator is RETIRED (~765 lines). It was an invention, not a port:
   * a `hire_cost = 200 + 25*difficulty` treasury gate, a Europe-dock expert
   * ladder keyed on NAMES display strings (tools/blacksmith/food/fisherman/
   * carpenter/lumberjack/ore/gunsmith/missionary/scout/elder/preacher/
   * teacher/craft), `units_find_type("Dragoon"/"Veteran Soldier"/...)`
   * war preferences, a wagon goods-load ladder and `inv->*_short` cargo
   * stand-ins — none of which exists in FUN_521d_5d04.
   *
   * DOS's own hire matrix is the raw 92568-93070 tail, ported in
   * `ai_euro_5d04_hire_ladder_tail` and called from the orchestrator:
   *   - Artillery dock buy when none is in Europe and colonies want muskets;
   *   - the recruit-slot swap priced off accumulated crosses;
   *   - the two-pass (unskilled-first, skilled-second) Europe-dock loop that
   *     turns Colonists into Soldiers (50 muskets) → Dragoons (50 horses),
   *     Pioneers (100 tools) or a Missionary, each with its own DS gate;
   *   - the recruit-buy loop that fills the departing hull;
   *   - the departure loop that sells/banks the inbound hold and tops the
   *     hull up from the DS:0xa0cc per-cargo demand table.
   * The orchestrator's return value still carries the raw 92541/92547 early
   * `return;` (ship-buy abort), which now simply ends the pass.
   */
  (void)ai_euro_5d04_nation_planning_structural(ctx, nation_id);
}

/* --- 0a60 goal-consumption engine (structural port, live) --------------
 *
 * Literal, section-scoped structural port of FUN_521d_0a60's own final
 * goal-table consumption engine — raw decomp lines 974-1063 of the
 * ~845-line function (see
 * original_sources_annotated/ai/euro_goal_orders_0a60_full.md, "New
 * section: goal -> orders wiring"). Per explicit instruction: port what
 * 0a60 does IN ITSELF faithfully; the functions/data it reaches out to can
 * stay at whatever level of development they're already at in this file
 * (dos_dist / map_continent_id_at / units_id_at / ai_goals_primary are all
 * real, already-ported equivalents of their DOS callees) or a documented
 * placeholder where DOS's own callee/data semantics are still unresolved
 * (DS:0x523d unit-type capability bitmask, unit+0x3148's FOUND/MIL_EXPAND
 * eligibility bits — `func_0x0001854c`'s weight seed was in this list until
 * 2026-09-06d, when it resolved to a plain clamp; see
 * `ai_euro_0a60_weight_seed`).
 *
 * **Live as of 2026-08-18**, replacing the old approximate soldier/
 * founder/generic-fallback three-loop scan inside `ai_euro_unit_act`
 * (which never covered LABOR/COLONY assignment for founders without a
 * matching FOUND/MIL_EXPAND slot, and used a two-phase "soldier goals
 * first, then anything" priority hack that DOS's real single-pass 64-slot
 * scan doesn't have — see euro_goal_orders_0a60_full.md's "Structural
 * pilot port" section for the before/after). Runs once per nation per
 * turn from `ai_euro_nation_planning`-equivalent, alongside
 * `ai_euro_colony_goals`; `ai_euro_unit_act` reads its committed pick back
 * per unit (act_state==0xb) instead of recomputing its own scan. Not a
 * `golden_ai_turns` fidelity claim — expect no immediate change there
 * (pre-existing TURN4→5 failure, unrelated); this is a structural quality
 * improvement over the old approximation, not a new golden-alignment pass.
 *
 * Deliberately out of scope this pass: the unit-loop threat-flag section
 * (raw lines 1-189) and the deep G-table / colony-loop section (raw lines
 * 190-973). Both lean on DOS accessor semantics (FUN_1000_8aac's field-id
 * meaning, thunk_FUN_2a1f_0470/047c/0524/0560's real effects, unit+0x3148's
 * individual bit *writers*) that no prior mapping pass in this project has
 * pinned down — a literal transliteration there would be unverifiable
 * guesswork, which this project's own method notes explicitly warn
 * against. ai_euro_refresh_continent_stance already covers the G-table's
 * *effect* (nation x continent stance) via a from-scratch recompute, just
 * not FUN_521d_0a60's literal write path.
 *
 * DOS unit AI bytes +0x3148/+0x314b/c/d/e — RESOLVED 2026-09-18, bugs.md
 * #525. They are not port-only scratch and need no shadow array: the Col1
 * unit record is 0x1c bytes based at DS:0x3144, and its field order (see
 * col1_save_layout.h `ColonizeCol1Unit`) is
 *   +0x3144/5 x,y   +0x3146 type   +0x3147 nation|vis   +0x3148 flags
 *   +0x3149 moves   +0x314a origin +0x314b ai_plan      +0x314c orders
 *   +0x314d goto_x  +0x314e goto_y +0x314f facing
 * so the whole DOS AI scratch block maps onto real `ColonizeUnit` fields:
 *   +0x3148 flag byte  -> u->col1_flags15
 *   +0x3149 MP SPENT   -> units_max_mp() - u->moves  (raw 6357 zeroes it at
 *                         the day top, raw 100342 adds 3 per step)
 *   +0x314b order code -> u->col1_ai_plan
 *   +0x314c act state  -> u->orders
 *   +0x314d/e goal x/y -> u->goto_x / u->goto_y
 * The port's own @ORDERS constants ARE those DOS act-state bytes:
 * FUN_521d_5b66's switch cases 7/8/9 are UNITS_ORDER_BUILD_COLONY /
 * CLEAR_PLOW / BUILD_ROAD, case 0x0b is UNITS_ORDER_AI_SAIL ("pursuing the
 * goal stored at +0x314d/e") and case 0x0c is UNITS_ORDER_AI_MOVE ("one
 * committed step"). The file-local `s_0a60_pilot_state` mirror of those
 * bytes was retired with this pass; only the AI_GOAL_* code of the committed
 * slot has no DOS byte at all, and it is re-read from the goal table by
 * `ai_goals_primary_code_at` instead of being mirrored.
 */

/* unit+0x314c literals, spelled as the port's own @ORDERS constants. */

/* unit+0x3148 — DOS `&= 0xd1` scratch bits (col1_save_layout.h names). */
