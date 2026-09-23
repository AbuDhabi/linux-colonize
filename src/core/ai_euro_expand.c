/*
 * Euro AI — goto plumbing, wagon haul (4393 work queue, 20e6 errands), colony founding + inventory
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Sections:
 *   Food-labor predicates, ai_euro_set_goto and goto/hold helpers
 *   FUN_521d_4393 work-queue haul pick + claim latches
 *   20e6 wagon errand latches (ai_euro_wagon_errand_*) and ai_euro_try_wagon_haul
 *   ai_euro_found_with_unit (colony founding) + ai_euro_colony_inventory
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

/*
 * Free Colonist / Colonist / Pioneer / Hardy — can join LABOR for food. Kind
 * comes from the unit's @UNIT type row; the residual "Farmer" case folds
 * into the profession == 0 (@JOB Farmer) check in the caller, so this stays
 * kind-only.
 */
static int ai_euro_is_food_labor_name(ColonizeUnitKind kind) {
  if (kind == UNITS_KIND_WAGON) {
    return 0;
  }
  if (kind == UNITS_KIND_SOLDIER || kind == UNITS_KIND_DRAGOON || kind == UNITS_KIND_SCOUT) {
    return 0;
  }
  return kind == UNITS_KIND_PIONEER || kind == UNITS_KIND_COLONIST;
}

/*
 * Food-LABOR capable unit: kind-based food labor OR @JOB Farmer (profession 0)
 * Expert Farmer on a Free Colonist / Colonist. Cite: docs/building_production.md
 * @JOB Farmer→Expert Farmer / Food; Colonization.pdf Skills Chart. No invented
 * food rates — LABOR join only.
 */
int ai_euro_unit_is_food_labor(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!u) {
    return 0;
  }
  const ColonizeUnitKind kind = ai_euro_unit_kind(units, u);
  if (ai_euro_is_food_labor_name(kind)) {
    return 1;
  }
  /* profession 0 == @JOB Farmer (Expert Farmer skill). */
  if (u->profession == 0 && kind == UNITS_KIND_COLONIST) {
    return 1;
  }
  return 0;
}

int ai_euro_type_is_wagon_name(ColonizeUnitKind kind);
int ai_euro_has_useful_goto(const ColonizeUnit* u, const ColonizeWorldMap* map);

int ai_euro_ship_enter_europe(ColonizeTurnContext* ctx, ColonizeUnit* ship);

void ai_euro_set_goto(ColonizeUnit* u, int orders, int gx, int gy) {
  if (!u) {
    return;
  }
  u->orders = orders;
  u->goto_x = gx;
  u->goto_y = gy;
  /*
   * Every non-SENTRY goto write clears the port-only landfall-wait flag
   * (bugs.md #528) — a real order (goal-directed goto, found move, wander,
   * hunt, wagon, ship stage) means the unit is no longer just parked ashore
   * waiting for its next landfall act. Call sites that DO mean "still
   * waiting ashore" pass SENTRY and then set the flag themselves right
   * after this call (ai_euro_unload_pax_at and its three direct siblings).
   */
  if (orders != UNITS_ORDER_SENTRY) {
    u->ai_landfall_wait = false;
  }
  if (getenv("AI_SET_GOTO_TRACE")) {
    fprintf(stderr, "[goto] unit %d (%d,%d) orders %d -> (%d,%d)\n", u->id, u->x, u->y, orders,
            gx, gy);
  }
  /*
   * Every goto write defaults to "not idle-roam" (DOS unit+0x314c != 5) —
   * ai_euro_move_scoring_gate re-marks its own two roam branches right
   * after calling this, so a stale roam flag from an earlier turn never
   * survives onto a goal-directed goto (found-tile, hunt, wagon, ship
   * staging) set by any other call site. See ai_euro_s_euro_roam_wander.
   */
  if (u->id >= 0 && u->id < COLONIZE_UNITS_MAX) {
    ai_euro_s_euro_roam_wander[u->id] = 0;
    ai_euro_s_euro_ship_route_latch[u->id] = 0;
  }
}

int ai_euro_is_ship_type(const ColonizeUnitPool* units, int unit_id) {
  /* Dispatcher ship wave: sea domain (SHIP_A..C stand-in). */
  return units_is_sea(units, unit_id);
}

/* Chebyshev adjacency (incl. same tile) for coastal embark checks. */
int ai_euro_tiles_near(int ax, int ay, int bx, int by) {
  const int dx = abs(ax - bx);
  const int dy = abs(ay - by);
  return dx <= 1 && dy <= 1;
}

/*
 * Deleted 2026-09-23 (bugs.md #746): ai_euro_find_boardable_ship,
 * ai_euro_try_treasure_board_sail, ai_euro_treasure_gold_from_unit,
 * ai_euro_cash_one_treasure, ai_euro_try_cash_treasure_europe and
 * ai_euro_try_expected_treasure_harbor — the whole invented AI
 * "board a treasure / sail it to Europe / cash it with the Crown's cut"
 * transport economy. Every one carried a Colonization.pdf / europe.h
 * citation only.
 *
 * DOS has no such site: FUN_521d_20e6's treasure band (raw 89997-90040) is
 * cash-in-any-own-colony / walk to the nearest own colony / rendezvous /
 * destroy, and FUN_4720_049e (the AI ship cargo pick, raw 76067-76513) has no
 * `+0x3146 == '\n'` term at all, so an AI treasure is never loaded onto a
 * hull. The King-galleon/Cortes transport offer (FUN_465b_0000 raw 75800) is
 * human-control-gated (bugs.md #478). The invented chain also ran *ahead* of
 * the DOS walk-home arm and paid the AI the Europe `min(tax,50)` cut instead
 * of the band's untaxed face value.
 *
 * europe_cash_treasure itself is kept — it is the human FUN_48d3_06ba path.
 */

/* True when wagon still has free goods-hold capacity (cargo field). */
int ai_euro_wagon_has_hold_capacity(const ColonizeUnitPool* units, const ColonizeUnit* w) {
  if (!units || !w) {
    return 0;
  }
  const int n = units_goods_hold_count(units, w->id);
  if (n <= 0) {
    return 0;
  }
  for (int h = 0; h < n; ++h) {
    const int amt = units_hold_amount(units, w->id, h);
    if (amt <= 0) {
      return 1; /* empty slot (units_hold_amount folds the 255 sentinel to 0) */
    }
    if (amt < 100) {
      return 1; /* partial room */
    }
  }
  return 0;
}

/* Wagon carries cargo_type in any hold. */
int ai_euro_wagon_has_cargo_type(
  const ColonizeUnitPool* units,
  const ColonizeUnit* w,
  int cargo_type
) {
  if (!units || !w || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  const int n = units_goods_hold_count(units, w->id);
  for (int h = 0; h < n; ++h) {
    if (units_hold_amount(units, w->id, h) > 0 && w->hold_goods_type[h] == cargo_type) {
      return 1;
    }
  }
  return 0;
}

/*
 * (`ai_euro_colony_haul_threshold` / `ai_euro_colony_haul_cargo_short` — the
 * shared "stock < 20 / 10 / pop*2" ladder — were deleted 2026-09-18 with the
 * last caller, the port-only short-colony ship unload arm in
 * `ai_euro_try_ship_trade_haul`. The surplus (mult 2) arm had already lost its
 * caller on the wagon side. FUN_521d_20e6 has no such test: the arrival block
 * dumps every hold unconditionally (raw 3002-3007) and the delivery matrix
 * (raw 2047-2139) scores destinations off the colony's own
 * cargo_produced_mask / warehouse capacity instead.)
 */

/*
 * (`ai_euro_haul_load_amount` — the Linux 20/10/pop*2 load chunk — was deleted
 * on 2026-09-06g together with the wagon load ladder it fed. DOS's own load
 * quantity is `min(stock[g], 100)` inside the 20e6 LOAD matrix, raw 3126-3129,
 * live in `ai_euro_20e6_load_pick`'s call sites.)
 */

int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);

/*
 * Free commodity holds on a hauler — DOS `unit_type_hold_capacity(0x5237) −
 * unit->holds_occupied(+0x3150)`, the quantity the 4393 tail subtracts from a
 * work-queue slot's load count.
 */
int ai_euro_hauler_free_holds(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  if (!units || !u) {
    return 0;
  }
  const int cap = units_goods_hold_count(units, u->id);
  int used = 0;
  for (int i = 0; i < cap && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    /* units_hold_amount folds the COL1 255 empty-hold sentinel to 0, so an
     * imported DOS save's Euro hull does not read as fully loaded. */
    if (units_hold_amount(units, u->id, i) > 0) {
      ++used;
    }
  }
  return cap > used ? cap - used : 0;
}

/*
 * Hull budget for one berth act — DOS `iStack_d2 = 0x5237[type] − +0x3150`
 * (raw 3012-3016), corrected for this port's passenger substitution.
 *
 * DOS's `+0x3150` counts GOODS holds only: it is the length of the packed
 * hold arrays (`+0x3151` type nibbles, `+0x3154` amounts), bumped by
 * `FUN_15eb_30b8` on a load (decomp 13325-13333, gated on
 * `+0x3150 < 0x5237[type]`) and decremented by `FUN_15eb_317c` on an unload
 * (13352-13360). A passenger is not a hold entry at all — it is a unit parked
 * at (−2,−2) on the shared tile lists (see
 * `ai_euro_20e6_transport_assemble`).
 *
 * Passengers nonetheless cost hull in DOS, because DOS re-derives the whole
 * transport chain on every berth act: the arrival block's stale-mark clear
 * (raw 2991-2997) strips `act_state == 1` from everything standing on the
 * berth tile — the previous act's passengers included, since `FUN_1427_040c`
 * flushed the (−2,−2) chain back onto that tile — and the raw 3024-3051 scan
 * then RE-marks them and RE-debits `iStack_d2` by their `0x5238` size. The
 * goods matrix therefore only ever sees `capacity − goods − passengers`, and
 * `0a60`'s own "hull full" test is literally `0x5237[type] == +0x3150` after
 * that round trip (decomp 87511-87514).
 *
 * This port keeps passengers in `cargo_ids`/`aboard_ship_id` instead, so the
 * mark scan's `aboard_ship_id >= 0` skip never re-marks them and the
 * reservation DOS renews each act has to be charged here. Without it a 2-slot
 * Caravel ends its turn carrying 2 passengers AND a goods hold.
 */
int ai_euro_20e6_ship_hold_budget(
  const ColonizeUnitPool* units, const ColonizeUnit* ship
) {
  if (!units || !ship) {
    return 0;
  }
  int free_holds = ai_euro_hauler_free_holds(units, ship);
  for (int i = 0; i < ship->cargo_count && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    const ColonizeUnit* p = units_get_const(units, ship->cargo_ids[i]);
    const ColonizeUnitType* pt = p ? units_type(units, p->type_index) : NULL;
    free_holds -= pt ? pt->space : 1; /* 0x5238[type] */
  }
  return free_holds > 0 ? free_holds : 0;
}

/*
 * FUN_521d_4393 — pick a work-queue haul target, then run DOS's own
 * queue-decrement tail on the slot that won.
 *
 * Raw decomp (move_scoring_20e6_full.md "Raw recovered C", the
 * `LAB_OVL14_L0000__004393` block):
 *   for slot in 0..15:
 *     rec[+0] >= 0 && rec[+4] > 0                      -- live slot with work
 *     unit->origin(+0x06) != current colony index      -- never its home colony
 *     (colony ai_flags & 2) == 0 || unit type > 0x0f   -- MoW-threatened colony
 *                                                        is warship-only work
 *     score = rec[+2] / ((dist >> 2) + 1)
 *     if (score >= best && (bVar17 || rec[+5] != 0)) best = score, pick = slot
 * then the tail (ai_goals_work_consume): loads -= free holds, score scaled,
 * slot freed at zero.
 *
 * `bVar17` (raw ~1284-1306) is the "this hull may take civilian work" flag:
 * `type != 0x12` (Man-O-War) for everything, narrowed further for Privateer
 * (0x10) and Frigate (0x11) by globals this project has not resolved
 * (`0xa89b`, `0x9e52`, `0x9414`, the per-nation stride-0x13 tables at
 * `−0x6db4`/`−0x6da4`, and `FUN_1000_8b74`) — DEAD END, see the doc note.
 * The base `!= 0x12` term is what ships here; the two narrowings would only
 * *remove* warships from the queue, and in this port only wagons and
 * `ai_euro_is_cargo_ship_name` hulls ever reach this function, so the
 * unresolved half is unreachable rather than approximated.
 *
 * Linux-only, kept from the pre-decode version and now sourced from the
 * colony record instead of squatting on the DOS `+4` byte: the Series R
 * specialty tie-break (+32 when the hauler already carries the target
 * colony's specialty cargo).
 *
 * Also Linux-only, and required by the tail: DOS runs `20e6` once per unit
 * per turn, so a hauler claims at most one slot per tick. This port's
 * dispatcher re-enters `ai_euro_unit_act` for a unit that still has moves,
 * which without a latch let the *second* entry claim a *second* slot and
 * re-aim the wagon at a worse colony (caught by
 * `unit_specialty_flag_a_haul_match`). `s_4393_claim_*` replays the first
 * claim of the tick instead of re-scoring — one claim per unit per turn,
 * exactly DOS's cadence.
 */
/*
 * DOS unit byte +0x314a = `ColonizeCol1Unit.origin` (record +0x06) — the
 * colony this hauler is bound to. One real, save-round-tripped byte
 * (2026-09-07; previously split across two session-local latches): the 20e6
 * LOAD matrix's ship arm writes it (raw 3134-3138), the ship arrival dump
 * clears it to 0xff (raw 3008), the 457e wagon walk binds it, the colony
 * tick (FUN_5952_035e) binds unbound land units standing in a colony, and
 * the delivery matrix (raw 2055), the 4393 pick (:89887) and the arrival
 * gate (raw 1691) read it. 0xff (any value >= 0x80 — DOS reads it as a
 * signed char) = unbound; spawn inits it so, col1_bridge round-trips it.
 */
int ai_euro_20e6_origin_get(const ColonizeUnit* u) {
  return (!u || u->col1_origin >= 0x80) ? -1 : (int)u->col1_origin;
}

void ai_euro_20e6_origin_set(ColonizeUnit* u, int colony_id) {
  if (u) {
    u->col1_origin = (colony_id < 0 || colony_id > 0x7f) ? 0xff : (uint8_t)colony_id;
  }
}

uint32_t ai_euro_s_4393_claim_turn[COLONIZE_UNITS_MAX];
int ai_euro_s_4393_claim_colony[COLONIZE_UNITS_MAX];
int ai_euro_s_4393_claim_valid[COLONIZE_UNITS_MAX];

int ai_euro_4393_work_queue_haul_pick(
  ColonizeTurnContext* ctx,
  int nation_id,
  int from_x,
  int from_y,
  const ColonizeUnit* hauler,
  int* out_x,
  int* out_y
) {
  if (!ctx || !ctx->colonies || !out_x || !out_y || nation_id < 0 || nation_id >= 4) {
    return 0;
  }
  const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
  const int hid =
    (hauler && hauler->id >= 0 && hauler->id < COLONIZE_UNITS_MAX) ? hauler->id : -1;
  /*
   * DOS `unit+0x314a` — the colony this hauler is bound to, now the real
   * persistent `col1_origin` byte (2026-09-07): the ship arrival dump clears
   * it and the colony tick refreshes land units, so the DOS lifecycle holds
   * without the old turn-scoping (which existed only because the port had no
   * writer keeping the byte fresh).
   */
  const int bound_colony = ai_euro_20e6_origin_get(hauler);
  /* Replay this tick's already-made claim instead of consuming a second slot. */
  if (hid >= 0 && ai_euro_s_4393_claim_valid[hid] && ai_euro_s_4393_claim_turn[hid] == turn) {
    const ColonizeColony* held = colonies_get(ctx->colonies, ai_euro_s_4393_claim_colony[hid]);
    /*
     * The replay is subject to the same `+0x314a` rule as the scan: a hauler
     * that has LOADED at the claimed colony since the claim was made is now
     * bound to it, and DOS would have skipped that slot. Without this the
     * replay re-aims a wagon at the colony it just filled up from.
     */
    if (held && held->active && held->nation_id == nation_id &&
        held->id != bound_colony) {
      *out_x = held->x;
      *out_y = held->y;
      return 1;
    }
    return 0;
  }
  const int hauler_type = hauler ? ai_euro_20e6_dos_type(ctx->units, hauler) : -1;
  const int civilian_hull = (hauler_type != 0x12); /* bVar17 base term */
  int best_score = -1; /* DOS iStack_e2 = -1; `>=` so later ties win */
  int best_slot = -1;
  int best_colony = -1;
  int bx = 0;
  int by = 0;
  for (int i = 0; i < AI_WORK_SLOTS; ++i) {
    const AiWorkSlot* w = ai_goals_work(i);
    if (!w || w->id < 0 || w->loads == 0) {
      continue; /* DOS: rec[+0] >= 0 && rec[+4] > 0 */
    }
    const ColonizeColony* c = colonies_get(ctx->colonies, (int)w->id);
    if (!c || !c->active || c->nation_id != nation_id) {
      continue;
    }
    /* DOS `unit+0x314a != DS:0x8dc6` (raw viceroy_unpacked.c:89887, evaluated
     * per slot right after `FUN_281f_09e6` binds THAT slot's colony) — a
     * hauler never serves the colony it is bound to. See `bound_colony`. */
    if (bound_colony >= 0 && bound_colony == (int)w->id) {
      continue;
    }
    /* DOS `(colony+0x1b & 2) == 0 || type > 0x0f`: a colony with a Man-O-War
     * nearby is only worked by warship hulls. */
    if ((c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0 && hauler_type <= 0x0f) {
      continue;
    }
    const int d = abs(c->x - from_x) + abs(c->y - from_y);
    /* −0x5f24 score, DOS distance normalization (raw 2214 / 2134:
     * score / ((dist >> 2) + 1) — replaced the thin `score − d*4`). */
    int score = (int)w->score / ((d >> 2) + 1);
    /* Series R specialty tie-break (Linux-only heuristic, not DOS). */
    if (hauler && c->specialty_cargo != 0xff &&
        (int)c->specialty_cargo < COLONIZE_CARGO_COUNT &&
        ai_euro_wagon_has_cargo_type(ctx->units, hauler, (int)c->specialty_cargo)) {
      score += 32;
    }
    /* DOS raw ~2216: a non-civilian hull may only take slots the colony
     * flagged military (rec[+5]). */
    if (score >= best_score && (civilian_hull || w->military != 0)) {
      best_score = score;
      best_slot = i;
      best_colony = (int)w->id;
      bx = c->x;
      by = c->y;
    }
  }
  if (best_slot < 0) {
    return 0;
  }
  /* DOS queue-decrement tail (raw ~2226-2242). */
  ai_goals_work_consume(best_slot, ai_euro_hauler_free_holds(ctx->units, hauler));
  if (hid >= 0) {
    ai_euro_s_4393_claim_turn[hid] = turn;
    ai_euro_s_4393_claim_colony[hid] = best_colony;
    ai_euro_s_4393_claim_valid[hid] = 1;
  }
  *out_x = bx;
  *out_y = by;
  return 1;
}

/*
 * (`ai_euro_nearest_haul_short_colony` — the Linux "drive to the nearest own
 * colony that is short of something" fallback — was deleted on 2026-09-06g.
 * It was the delivery-direction consumer that forced the 0a60 registration
 * gate to stay on the Linux `haul_short` boolean; with the gate flipped to
 * DOS's `bVar5` the queue is a PICKUP queue and this function pulled haulers
 * the wrong way. DOS has no such scan: when LAB_521d_4393 finds no slot a
 * wagon falls to LAB_521d_457e's origin-colony walk and a ship to the 457e
 * ship arms — see the header of `ai_euro_try_wagon_haul`.)
 */

/*
 * DOS unit byte +0x3158 (col1 record +0x14 — a cargo_hold slot DOS reuses as
 * land-unit AI scratch): the wagon village-errand latch. Set by the 20e6
 * load matrix's wagon arm (raw 3134-3138, iStack_34 == 0), cleared by
 * FUN_4d56_2820 at trade entry for land types (viceroy_unpacked.c:82122).
 *
 * 2026-09-06: no longer session-local. The unit array base is DS:0x3144 with
 * stride 0x1c, so +0x3158 is COL1 unit record +0x14 = cargo_hold[4]
 * (col1_save.h ColonizeCol1Unit; same arithmetic that makes +0x315b the
 * `profession` Treasure byte and +0x314a the `origin` byte), and
 * col1_bridge_capture / col1_bridge_apply now round-trip it for type 0x0c
 * units — see ai_euro_wagon_errand_* below. The byte is kept verbatim (DOS
 * only ever stores 0 or 1) so an original DOS save's value survives a
 * load→save with no rewrite.
 */
uint8_t ai_euro_s_20e6_wagon_errand[COLONIZE_UNITS_MAX];

/*
 * DOS unit byte +0x315a — the "cower in port" counter. +0x315a = COL1 unit
 * record offset 0x16 = `col1_counter16` (col1_save.h), which the port already
 * round-trips (u->col1_counter16) — so the counter is save-persistent now, no
 * session array. The multiplexing with Europe-lane voyage turns and
 * trade-route stop packing is DOS's own (same byte), not a port shortcut.
 */

/*
 * DS:0x1734[nation] — count of colonies that registered work-queue work,
 * bumped in FUN_521d_0a60's bVar5 registration branch (:87675) and zeroed
 * only by the 20e6 berth boarding scan (:81295). Real counter since
 * 2026-09-07; the 20e6 colony-sail / unload matrices used to substitute a
 * NEEDS_GARRISON/MILITARY colony count for it.
 */
int16_t ai_euro_s_0a60_work_registered[4];

/*
 * DS:0x1734[nation] — colonies that registered work-queue work (0a60 bVar5
 * branch bumps it; only the 20e6 berth boarding scan zeroes it).
 * Session-local, not save data; file-local since audit AE-31.
 */
int ai_euro_0a60_work_registered(int nation_id) {
  return (nation_id >= 0 && nation_id < 4) ? (int)ai_euro_s_0a60_work_registered[nation_id] : 0;
}

unsigned char ai_euro_wagon_errand_get(int unit_id) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX) {
    return 0;
  }
  return (unsigned char)ai_euro_s_20e6_wagon_errand[unit_id];
}

void ai_euro_wagon_errand_set(int unit_id, unsigned char value) {
  if (unit_id < 0 || unit_id >= COLONIZE_UNITS_MAX) {
    return;
  }
  ai_euro_s_20e6_wagon_errand[unit_id] = (uint8_t)value;
}

void ai_euro_wagon_errand_clear_all(void) {
  memset(ai_euro_s_20e6_wagon_errand, 0, sizeof(ai_euro_s_20e6_wagon_errand));
}

/* Defined later in this file (after their 20e6 dependencies). */
int ai_euro_20e6_load_pick(
  ColonizeTurnContext* ctx, const ColonizeColony* c, int nation, int is_ship
);
int ai_euro_20e6_wagon_village_errand(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* wagon
);
int ai_euro_20e6_wagon_origin_walk(
  ColonizeTurnContext* ctx, int nation_id, ColonizeUnit* u
);

/*
 * Wagon Train beat, DOS shape (FUN_521d_20e6 land arm):
 *
 *   at an own colony  -> dump every hold into it (raw 3007-3012 — this IS
 *                        DOS wagon delivery), then the LOAD matrix wagon arm
 *                        (`ai_euro_20e6_load_pick` is_ship=0, at most ONE
 *                        hold: `if (type==0xc && d2>1) d2 = 1`, raw ~3025),
 *                        which on a load latches the village errand
 *                        (+0x3158 = 1, raw 3136);
 *   on errand         -> the errand walker owns the wagon (raw 2284-2307);
 *   otherwise         -> the work-queue tip (LAB_521d_4393), which now aims
 *                        the wagon at a colony that HAS goods — the queue is
 *                        a PICKUP queue.
 *
 * 2026-09-06g: the Linux specialty/produced/food-first load ladder and the
 * `ai_euro_nearest_haul_short_colony` delivery-direction fallback are DELETED
 * with the 0a60 `haul_short` → `bVar5` gate flip. The ladder existed only to
 * feed that fallback; both are replaced by the DOS load matrix above plus the
 * pickup tip below. Nothing in DOS scores "nearest colony short of X" for a
 * hauler at this point.
 *
 * 2026-09-07: the 4393 pick is ships-only here too (DOS gate `0xc < type <
 * 0x13`, raw :89877) — the wagon substitute is retired. Off-errand wagons run
 * the LAB_521d_457e origin walk (`ai_euro_20e6_wagon_origin_walk`): bind to
 * the nearest own colony, park there (`+0x314b = 0x55`), walk home, or
 * destroy when unbound off-landmass. The +0x314a binding is the real
 * `col1_origin` byte now (save-round-tripped, colony-tick refreshed).
 * Cite: move_scoring_20e6_full.md 2026-09-06d/f + 2026-09-07.
 */
int ai_euro_try_wagon_haul(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeUnit* wagon
) {
  if (!ctx || !ctx->units || !ctx->colonies || !wagon || !wagon->active) {
    return 0;
  }
  if (!ai_euro_type_is_wagon_name(ai_euro_unit_kind(ctx->units, wagon))) {
    return 0;
  }
  /* No cargo-flag entry gate any more: the DOS band runs for every wagon —
   * the arrival dump takes every cargo type and the 457e walk is
   * unconditional (the old has-cargo/has-capacity early-out was a
   * 4393-substitute artifact retired with it, 2026-09-07). */
  /*
   * DOS own-colony arrival block (FUN_521d_20e6 raw 2996-3138): dump every
   * hold into the colony (raw 3007-3012 — this IS DOS wagon delivery), then
   * the LOAD matrix wagon arm, capped at ONE hold (`if (type == 0xc && d2 >
   * 1) d2 = 1`, raw ~3025). A matrix load latches the village errand
   * (+0x3158 = 1, raw 3136) and the errand walker below owns the wagon from
   * there.
   */
  if (wagon->id >= 0 && wagon->id < COLONIZE_UNITS_MAX) {
    const int cid = colonies_id_at(ctx->colonies, wagon->x, wagon->y);
    if (cid >= 0) {
      ColonizeColony* c = colonies_get_mut(ctx->colonies, cid);
      /*
       * Raw 1691 gate: for a land hauler the arrival block also needs
       * `+0x314a == uStack_62` — an UNBOUND wagon does not dump; the 457e
       * walk binds it first (one-beat delay, as in DOS) and the next entry
       * dumps. Ships have no such bind requirement.
       */
      if (c && c->active && c->nation_id == nation_id &&
          ai_euro_20e6_origin_get(wagon) == cid) {
        for (;;) {
          int hold = -1;
          const int n = units_goods_hold_count(ctx->units, wagon->id);
          for (int h = 0; h < n; ++h) {
            if (units_hold_amount(ctx->units, wagon->id, h) > 0) {
              hold = h;
              break;
            }
          }
          if (hold < 0) {
            break;
          }
          if (colonies_transfer_from_unit(
                ctx->colonies, cid, ctx->units, wagon->id, hold, NULL) <= 0) {
            break;
          }
        }
        const int g = ai_euro_20e6_load_pick(ctx, c, nation_id, 0);
        if (g >= 0) {
          int qty = (int)c->stock[g];
          if (qty > 100) {
            qty = 100; /* raw 3126-3129 */
          }
          if (qty > 0 &&
              colonies_transfer_to_unit(ctx->colonies, cid, ctx->units, wagon->id, g, qty) >
                0) {
            ai_euro_s_20e6_wagon_errand[wagon->id] = 1;
            /* DOS's land arm writes only +0x3158 here (raw 3136) — the
             * wagon's +0x314a stays bound to this, its home colony. */
            if (getenv("AI_20E6_LOAD_TRACE")) {
              fprintf(
                stderr, "[load] wagon %d n%d colony %d cargo %d qty %d errand\n",
                wagon->id, nation_id, cid, g, qty
              );
            }
          }
        }
      }
    }
    /*
     * Village-errand walker (raw 2284-2307 + 4528 AI arm case 1): an errand
     * wagon ignores the haul chain — goto the nearest same-landmass village
     * (capital distance halved, min 1), trade via the 2820 shell on arrival,
     * destroy when no village shares the landmass (LAB_47b9).
     */
    if (ai_euro_s_20e6_wagon_errand[wagon->id] &&
        ai_euro_20e6_wagon_village_errand(ctx, nation_id, wagon)) {
      return 1;
    }
  }
  /*
   * LAB_521d_457e origin walk (raw 2256-2289) — the DOS home of an off-errand
   * wagon. Replaces the retired "wagons peel the ships-only 4393 queue"
   * substitute (divergence closed 2026-09-07): bind to the nearest own colony
   * when unbound, park at the bound colony (orders 0x55), walk home
   * otherwise, destroy when unbound with no own colony on this landmass.
   */
  return ai_euro_20e6_wagon_origin_walk(ctx, nation_id, wagon);
}


/*
 * bugs.md #610: the invented AI pioneer improvement planner
 * (`ai_euro_try_pioneer_improve` / `_improve_target` / `_tile_can_plow` /
 * `_tile_can_road` / `AI_EURO_IMPROVE_TIMER_MIN`) is gone. It cited only
 * Colonization.pdf: DOS's AI never writes `+0x314c = 8` (the sole writer of
 * order 8 in the image is the human ORDERS UI, raw 42511), never scans a
 * colony ring for an improvable tile, has no plow-before-road rule, no MD<=3
 * limit and no prefer-Hardy rule. The two real DOS arms replace it:
 * `ai_euro_20e6_build_road_arm` (FUN_521d_20e6 raw 90183-90206, #611) and
 * `ai_euro_5952_improve_best_plot` (FUN_5952_035e raw 94470-94551, #612).
 */

void ai_euro_found_with_unit(ColonizeTurnContext* ctx, ColonizeUnit* founder, int nation_id) {
  if (!ctx || !ctx->colonies || !ctx->map || !founder || !founder->active) {
    return;
  }
  if (!colonies_can_found(ctx->colonies, ctx->map, founder->x, founder->y)) {
    return;
  }
  int tools = 0;
  int muskets = 0;
  int horses = 0;
  units_founder_loot(ctx->units, founder->id, &tools, &muskets, &horses);
  /*
   * FUN_4cc6_07c2 Indian homeland purchase when founding on tribe land.
   * Cite: Colonization.pdf / wiki Peter Minuit (FF 2) → free; else charge
   * via colonies_found_with_indian_land. Short gold → PARK (no despawn).
   */
  int cid = -1;
  if (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
    uint32_t* gold = &ctx->col1->nation[nation_id].gold;
    const int cost = colonies_indian_land_purchase_gold(
      ctx->col1, ctx->map, founder->x, founder->y, nation_id
    );
    /*
     * DOS-LITERAL FUN_479b_00ca affordability gate (bugs.md #709), read
     * byte-exact off viceroy_unpacked.asm 121034-121094:
     *   if (nation < 4 && DS:[nation*0x34 + 0x543f] == 0) return 0;  // human
     *   cost = FUN_281f_0d78(...);          // = the 4cc6_07c2 price
     *   gold = FUN_281f_0a92(nation);       // 32-bit DX:AX
     *   SUB CX,AX / SBB BX,DX  ;  SAR (cost),1        →
     *   if ((long)gold - cost < cost / 2) return 0;   // JL/JC LAB_479b_0150
     *   FUN_281f_0af6(nation, cost);        // pay
     *   INC byte [ [0x8d4e] + 5 ];          // tribe lands_bought
     *   FUN_281f_068c(x, y, 0x10, 1);       // stamp the tile bought
     * i.e. the AI does not spend down to zero: it buys only when at least half
     * the price is still left afterwards. The gate is AI-only — for a human
     * (control byte 0x543f == 0) 00ca returns 0 without touching gold, which is
     * why the human branch below keeps its own plain `gold < cost` message.
     * Lead still open: 00ca's caller is the thunk 2a1f:01dd and is unresolved,
     * so whether a failed gate aborts the founding or founds the colony unpaid
     * is unknown; the port keeps its existing "found anyway" outcome.
     */
    int short_gold;
    if (cost <= 0) {
      short_gold = 0;
    } else if (nation_id == ctx->human_nation) {
      short_gold = *gold < (uint32_t)cost;
    } else {
      short_gold = ((long)*gold - (long)cost) < (long)(cost / 2);
    }
    if (short_gold) {
      /*
       * FUN_4cc6_07c2 short-gold gate — no despawn. Thin human status only.
       * Cite: colonies_indian_land_purchase_gold; Colonization.pdf Minuit /
       * indian land purchase.
       */
      if (nation_id == ctx->human_nation && ctx->status && ctx->status_size > 0) {
        snprintf(
          ctx->status,
          ctx->status_size,
          "Not enough gold to buy Indian land (%d$ needed).",
          cost
        );
        return;
      }
      /*
       * AI nations start with a treasury of 0 (ai_starting_gold: AI always 0),
       * and the FOUND sites the planner produces are tribe-adjacent by
       * construction, so this gate used to block every AI first colony
       * outright — settlers reached their site and stood there for the rest of
       * the game. The @INDIANLAND dialog's third option is "take it", which
       * proceeds unpaid with no immediate consequence (game_loop.c
       * GAME_INDIAN_LAND_TAKE); that is what an AI with no gold does.
       */
      cid = colonies_found(
        ctx->colonies,
        ctx->map,
        founder->x,
        founder->y,
        nation_id,
        founder->type_index,
        founder->profession,
        tools,
        muskets,
        horses
      );
    } else {
      cid = colonies_found_with_indian_land_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1), .col1_ok=((ctx->col1) != NULL)}, gold, founder->x, founder->y, nation_id, founder->type_index, founder->profession, tools, muskets, horses);
    }
  } else {
    cid = colonies_found(
      ctx->colonies,
      ctx->map,
      founder->x,
      founder->y,
      nation_id,
      founder->type_index,
      founder->profession,
      tools,
      muskets,
      horses
    );
  }
  if (cid >= 0) {
    colonies_reveal_founded_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, cid); /* FUN_364b_1dd6 Coronado */
    const int founded_x = founder->x;
    const int founded_y = founder->y;
    if (cid >= 0 && cid < COLONIZE_COLONIES_MAX) {
      ai_euro_s_founded_colony_turn[cid] = 1;
    }
    /* FUN_479b_076e -0x77b2 stamp — see ai_goals.h AiNationPlanScratch. */
    ai_goals_note_colony_founded(
      nation_id, ctx->turn_number ? (int)*ctx->turn_number : 0
    );
    units_despawn(ctx->units, founder->id);
    if (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
      ctx->col1->player[nation_id].founded_colonies++;
    }
    /*
     * First colony: release empty ships still latched on found-hold (fy+2) so
     * a later outer pass can sail (TURN4→5 FR). Cite: test-saves-ai/TURN5.
     */
    if (ctx->units && ctx->map && colonies_count_for_nation(ctx->colonies, nation_id) == 1) {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* sh = &ctx->units->units[i];
        if (!sh->active || sh->nation_id != nation_id || !units_is_sea(ctx->units, sh->id)) {
          continue;
        }
        if ((sh->goto_x == founded_x && sh->goto_y == founded_y + 2) ||
            (sh->x == founded_x && sh->y == founded_y + 2)) {
          int tx = founded_x + 2;
          int ty = founded_y + 6;
          if (tx >= (int)ctx->map->width) {
            tx = (int)ctx->map->width - 1;
          }
          if (ty >= (int)ctx->map->height) {
            ty = (int)ctx->map->height - 1;
          }
          if (tx < 0) {
            tx = 0;
          }
          if (ty < 0) {
            ty = 0;
          }
          ai_euro_set_goto(sh, UNITS_ORDER_AI_SAIL, tx, ty);
          if (sh->moves <= 0) {
            sh->moves = units_max_mp(ctx->units, sh->id);
          }
        }
      }
      /* Pioneer on found+1: SW coast course after first town. Cite: TURN5 FR. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* p = &ctx->units->units[i];
        if (!p->active || p->nation_id != nation_id || p->aboard_ship_id >= 0) {
          continue;
        }
        if (!ai_euro_name_is_pioneer(ai_euro_unit_kind(ctx->units, p))) {
          continue;
        }
        if (p->x == founded_x && p->y == founded_y + 1) {
          ai_euro_set_goto(p, UNITS_ORDER_AI_SAIL, founded_x - 3, founded_y + 3);
          /* (50,38)→(48,39) is three cardinal minor-river steps at 1 third
           * each (TURN4→5): a fresh 3-third allotment covers it. */
          p->moves = units_max_mp(ctx->units, p->id);
        }
      }
      /*
       * SP: ship one west of cruise tip → NE berth (TURN5→6 45,50→46,49);
       * soldier SE+1 → SE+2 (46,55→46,56). Cite: test-saves-ai/TURN6.
       */
      {
        int wx = 0;
        int wy = 0;
        if (ai_euro_ocean_3558_empty_cruise_tip(
              ctx->map, founded_x, founded_y, &wx, &wy
            )) {
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            ColonizeUnit* sh = &ctx->units->units[i];
            if (!sh->active || sh->nation_id != nation_id ||
                !units_is_sea(ctx->units, sh->id)) {
              continue;
            }
            if (sh->x == wx - 1 && sh->y == wy) {
              const int tx = wx;
              const int ty = wy - 1;
              if (map_tile_is_water(ctx->map, tx, ty) ||
                  map_tile_is_high_seas(ctx->map, tx, ty)) {
                ai_euro_set_goto(sh, UNITS_ORDER_AI_MOVE, tx, ty);
              } else if (map_tile_is_water(ctx->map, wx, wy + 1) ||
                         map_tile_is_high_seas(ctx->map, wx, wy + 1)) {
                /* Fallback berth south of tip if north is land. */
                ai_euro_set_goto(sh, UNITS_ORDER_AI_MOVE, wx, wy + 1);
              } else {
                ai_euro_set_goto(sh, UNITS_ORDER_AI_MOVE, wx, wy);
              }
              if (sh->moves <= 0) {
                sh->moves = units_max_mp(ctx->units, sh->id);
              }
            }
          }
          for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
            ColonizeUnit* su = &ctx->units->units[i];
            if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
              continue;
            }
            if (!ai_euro_name_is_soldier(ai_euro_unit_kind(ctx->units, su))) {
              continue;
            }
            if (su->x == founded_x + 1 && su->y == founded_y + 3) {
              ai_euro_set_goto(su, UNITS_ORDER_AI_MOVE, founded_x + 1, founded_y + 4);
              su->moves = UNITS_MP_PER_TILE;
            }
          }
        }
      }
    }
  }
  /*
   * DOS: a new AI town already carries its first project in the same turn
   * (seed-100 TURN4-6 saves: Docks). Idle-queue-only pick, so re-running it
   * here after the planning-phase call is harmless for existing towns.
   * This is the LAST surviving `ai_euro_prefer_*` pass (bugs.md #483 deleted
   * the other twenty): the FUN_5952_035e cascade runs in the nation's
   * planning phase, which a colony founded mid-act has already missed, and
   * the golden's founding-turn Docks is what this stands in for. Retiring it
   * means finding DOS's own founding-turn project writer, not moving the
   * cascade.
   */
  ai_euro_prefer_peace_construction(ctx, nation_id);
}

/*
 * `ai_euro_join_colony` (the bare colonies_admit_unit_w wrapper) is gone with
 * its last caller, the invented LABOR/COLONY arrival join. Every surviving
 * admission goes through the arm that owns it: FUN_5952_035e's colony-tick
 * tile re-scan (ai_euro_5952_absorb_equip, raw 94231-94274) and the expert
 * field/workplace assign arms, which admit *and* seat the colonist.
 */

/* --- inventory (6d8e steps 1–3) ---------------------------------------- */

void ai_euro_colony_inventory(ColonizeTurnContext* ctx, int nation_id) {
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  if (!inv || !ctx) {
    return;
  }
  ai_goals_inventory_clear(nation_id);
  inv->colony_count = colonies_count_for_nation(ctx->colonies, nation_id);
  /* founding_expansion_urgency stand-in: early game → 8. */
  inv->urgency = (inv->colony_count < 3) ? 8 : (inv->colony_count < 6 ? 4 : 0);

  if (!ctx->colonies) {
    return;
  }
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id != nation_id) {
      continue;
    }
    /* 5cf6-shaped shortage tallies. */
    if (c->stock[COLONIZE_CARGO_TOOLS] < 20) {
      inv->tools_short += 20 - c->stock[COLONIZE_CARGO_TOOLS];
    }
    /*
     * Lumber shortage tally (5cf6-shaped): mirror tools_short<20 for lumber when
     * colony wants lumberjack LABOR (Warehouse/Lumber Mill) or any construction
     * is in progress. Cite: docs/building_production.md Lumberjack→Lumber;
     * ai_euro_colony_wants_lumberjack_labor; euro_unit_act §2e.
     */
    if ((ai_euro_colony_wants_lumberjack_labor(ctx->colonies, c) ||
         c->building_in_production >= 0) &&
        c->stock[COLONIZE_CARGO_LUMBER] < 20) {
      inv->lumber_short += 20 - c->stock[COLONIZE_CARGO_LUMBER];
    }
    if (c->stock[COLONIZE_CARGO_MUSKETS] < 10) {
      inv->muskets_short += 10 - c->stock[COLONIZE_CARGO_MUSKETS];
    }
    if (c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
      inv->food_short += (c->population * 2) - c->stock[COLONIZE_CARGO_FOOD];
    }
    /* Ore shortage (5cf6-shaped): feed Blacksmith / Expert Ore Miner dock hire. */
    if (c->stock[COLONIZE_CARGO_ORE] < 20) {
      inv->ore_short += 20 - c->stock[COLONIZE_CARGO_ORE];
    }
    /* FUN_5952_035e thin: INC cargo_idle_turns (+0x8f) + improve_timer (+0x8c)
     * cap 0x7f. */
    if (c->cargo_idle_turns < 0x7f) {
      c->cargo_idle_turns++;
    }
    if (c->improve_timer < 0x7f) {
      c->improve_timer++;
    }
    /*
     * FUN_5952_035e origin refresh (colony_tick_5952_035e.md raw ~348): every
     * LAND unit in this colony's tile stack whose +0x314a origin is unbound
     * (< 0) is bound to this colony (`unit->origin = DS:0x8dc6`). This is the
     * DOS writer that keeps the persistent origin byte from drifting — the
     * 4393 pick, the 20e6 arrival gate and the 457e wagon walk all read it.
     */
    if (ctx->units) {
      /* Slot walk (audit second-wave Leads 2, 2026-09-10): the loop variable
       * is an ARRAY index, not a unit id — see the ai_euro_refresh_continent_
       * stance note. */
      for (int uid = 0; uid < COLONIZE_UNITS_MAX; ++uid) {
        ColonizeUnit* su = &ctx->units->units[uid];
        if (!su->active || su->nation_id != nation_id || su->aboard_ship_id >= 0) {
          continue;
        }
        if (su->x != c->x || su->y != c->y) {
          continue;
        }
        const int t = ai_euro_20e6_dos_type(ctx->units, su);
        if (t >= 0x0d && t <= 0x12) {
          continue; /* DOS gate: type < 0xd || type > 0x12 — land only */
        }
        if (su->col1_origin >= 0x80) {
          su->col1_origin = (uint8_t)c->id;
        }
      }
    }
    /*
     * The 11-cargo "surplus haul ladder" that used to refresh +0x8d here was
     * a port invention (its own comment read "FUN_5952_0306 thin"), and it
     * ran in the wrong body besides. DOS's only +0x8d writers in the AI turn
     * are FUN_5952_035e's five FUN_5952_0306 calls, now ported literally as
     * ai_euro_5952_build_pref_0306 and hosted at DOS's position inside the
     * colony tick (2026-09-18).
     */
  }
}

/*
 * The "unit inventory" step (FUN_521d_6d8e's own per-unit prelude, raw
 * 93147-93172) used to live here as a per-Pioneer `inv->muskets_short--`.
 * That was a mis-aimed double-port: the DOS prelude's per-unit arm decrements
 * the TOOLS demand counter DS:0xa0da (raw 93168-93170, "the Pioneers already
 * in the field cancel the tools demand"), never the Muskets counter 0xa0db,
 * and that arm is already ported faithfully in `ai_euro_5d04_cb_colony_needs`
 * (the 0xa0da/0xa0db writer the 5d04 hire matrix actually reads). Deleted
 * 2026-09-09 (smell audit #35): nothing here is unported, so no replacement
 * step is needed and `inv->muskets_short` now means exactly what
 * `ai_euro_colony_inventory` tallies.
 */

/* --- 5d04 planning / hire ---------------------------------------------- */

int ai_euro_type_is_wagon_name(ColonizeUnitKind kind) {
  /* @UNIT row 12 is "Wagon Train"; no roster spells it "Supply Train". */
  return kind == UNITS_KIND_WAGON;
}

/*
 * The AI_EURO_*_PURCHASE_GOLD / AI_EURO_VETERAN_SOLDIER_TRAIN_GOLD constants
 * that used to sit here were the Linux-shaped hire matrix's private price
 * copies; they went unreferenced when that matrix was retired 2026-09-07e and
 * are removed 2026-09-07. The real prices are the DS:0x978d stride-6 purchase
 * table europe.c now owns (europe_purchase_price, audit AE-17), and DOS
 * 5d04 never trains a Veteran in Europe for gold at all (its only Veteran path
 * is the College bVar5 profession promote).
 */

/* ========================================================================
 * FUN_521d_5d04 — euro_nation_planning, structural port (2026-08-18)
 *
 * Ports the function's own control flow + arithmetic 1:1 from the raw
 * decompile (original_sources_decompiled/viceroy_unpacked_2.c:85822-86564)
 * for the part that's genuinely resolved (raw lines 85872-86064 — the
 * difficulty-scaled treasury bump and the Frigate/Man-O-War-threat /
 * no-ships / cargo-short gate cascade); callees stay stubbed throughout,
 * per request.
 *
 * Resolved this pass (cross-referenced against save_format_map.md /
 * col1_save.h / euro_g_table_0a60.md — see those for the trace): year/
 * turn/difficulty (col1->head), nation.gold (32-bit, direct — the DOS body
 * does manual 16-bit lo/hi carry arithmetic on nation+0x2a/+0x2c that the
 * already-32-bit Linux `gold` field doesn't need), the per-nation census
 * block (colony_counts/ship_counts/colony_pop_totals/armed_ship_counts/
 * ship_cargo_totals/census_pop_proxy — col1_save.h `ColonizeCol1Stuff`),
 * unit_type_counts[nation][16]/[17], DS:0x5382 bit0 — **confirmed** (live
 * DOSBox-X capture, 2026-08-18, `BPM 237D:5382`: `or byte [5382],01` fires
 * 00→01 exactly on Declare Independence) as `game_options.woi`, settling
 * the earlier conflict with an older, looser "NEW WORLD path" guess in
 * euro_dispatcher.c / SYMBOL_MAP.md for the same bit — that guess was
 * wrong, both docs corrected. FUN_281f_09fc(building_index)
 * confirmed elsewhere (euro_unit_act.md) as `has_building` indexed by
 * NAMES.TXT `@BUILDING` file order — index 0xd in that order is "College"
 * (cross-checked against that same doc's independently-confirmed index
 * 0x24 = "Lumber Mill"), FUN_281f_0808 confirmed as `destroy_unit`
 * (move_scoring_ship.md).
 *
 * Deliberately NOT re-ported this pass: raw lines 86065-86564 (the Europe
 * hire ladder + profession/reward loop tail, ~500 raw lines). Traced
 * enough to see it's the same mechanic `ai_euro.c`'s existing extensive
 * "5d04" thin-hire coverage (peace tools/wagon matrix, war Soldier/
 * Dragoon/Artillery hire, buy ladder, `AiEuroInventory.profession_demand`
 * already wired) already approximates — re-transcribing it blind risks
 * duplicating/conflicting with that tested behavior rather than adding
 * value, the same "scope down after inspection" call the 0a60 structural
 * pilot made. Its own callees (FUN_281f_07e0/02e4/0b78/0c9a/095c/0be6/
 * 0c68/0aec, FUN_291f_0b26/0afc/0c3e/09ea/0a2e/0ec2/0d8e/0dc6/0c14,
 * FUN_1d1d_0ec6, FUN_281f_0aba) stay unresolved.
 *
 * Live now: only the treasury bump (`ai_euro_5d04_treasury_bump`) replaces
 * the old function's simplified formula, called from both this function
 * and `ai_euro_nation_planning` below. The gate cascade
 * (`ai_euro_5d04_compute_flags`) is a faithful reference implementation —
 * computed, logged nowhere, not yet gating any mutation — since its two
 * real consumers (bVar5 feeds the deferred hire ladder; the destroy-unit
 * branch is a live-side-effecting op this first pass deliberately keeps
 * stubbed per "be safe").
 * ======================================================================== */
