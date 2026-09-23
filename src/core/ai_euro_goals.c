/*
 * Euro AI — FUN_521d_0a60 goal orders + producers, FUN_5952_035e labor/threat seeds, colony-goal passes, move scoring
 *
 * Split out of ai_euro.c (2026-09-23) verbatim; shared symbols are declared
 * in ai_euro_internal.h. See ai_euro.c for the dispatcher entry point.
 *
 * Sections:
 *   FUN_521d_0a60 weight seed, goal-pursuit gates, continent presence, stack counts
 *   0a60 unit housekeeping + goal-orders structural pass
 *   0a60 foreign-colony / village / settlement goal producers
 *   FUN_5952_035e labor demand, AI flags, threat seed
 *   FUN_5952_035e absorption + equip arm (raw 94231-94352) and build-pref 0306
 *   ai_euro_colony_goals_* passes and the ai_euro_colony_goals driver
 *   ai_euro_ocean_score_step + ai_euro_score_move (land/sea move scoring)
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
 * The `aiStack_1da[64]` weight seed 0a60 fills every primary slot with at
 * entry — **resolved 2026-09-06d**, was a fixed-50 placeholder.
 *
 * Ghidra's `func_0x0001854c(0xd1d, X, 3, 99)` is `FUN_1000_854c` =
 * `FUN_281f_035c` (`address_mapping.csv:809` — flat ram `1854c`), a far
 * RTLink thunk `CALLF FUN_210d_0d91; JMPF FUN_124c_000c`. `FUN_124c_000c`
 * (`viceroy_unpacked.asm`, 124c:000c) is the resident 3-arg clamp:
 *   DX=[BP+6]; AX=[BP+8]; if(AX<DX) AX=DX; if(AX>[BP+0a]) AX=[BP+0a]; ret AX
 * i.e. `min(max(a,b),c)`. The leading `0xd1d` in the decompile is a segment
 * artifact of the far-call thunk, not an argument — the raw call site
 * (521d:0ab8, `PUSH 0x63 / PUSH 0x3 / MOV BX,[BP+6] / MOV AL,[BX+0x8cfc] /
 * SHR AL,3 / PUSH AX / CALLF 281f:035c / ADD SP,6`) pushes exactly three
 * words.
 *
 * And the value fed in is NOT the difficulty byte the old comment guessed:
 * `[BX + 0x8cfc]` is `all_unit_counts[nation]` (DS:0x8cfc, the saturating
 * per-nation unit total written by the `FUN_4962_0018` census — see
 * docs/save_format_map.md "Stuff" file-off 12).
 *
 *   seed = clamp(all_unit_counts[nation] >> 3, 3, 99)
 *
 * The 99 ceiling is unreachable in practice (the source is a byte, so the
 * shift caps at 31); the live range is [3, 31]. It matters because the
 * consumption tail's claim-count `weight[slot]++` is measured against this
 * seed — with the old fixed 50 a claim moved a slot's score by 2%, with the
 * real early-game seed of 3 it moves it by 33%, which is what makes DOS
 * spread a nation's units across distinct goal slots instead of piling them
 * onto the single cheapest one.
 */
static int ai_euro_0a60_weight_seed(const ColonizeTurnContext* ctx, int nation_id) {
  int count = -1;
  if (ctx && ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4) {
    count = (int)ctx->col1->stuff.all_unit_counts[nation_id];
  }
  if (count < 0 && ctx && ctx->units) {
    /* No col1 census window (unit-test contexts): recompute DS:0x8cfc live,
     * same saturating all-active-units-of-nation tally FUN_4962_0018 does. */
    count = 0;
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (u->active && u->nation_id == nation_id && count < 255) {
        count++;
      }
    }
  }
  if (count < 0) {
    count = 0;
  }
  int seed = count >> 3;
  if (seed < 3) {
    seed = 3;
  }
  if (seed > 99) {
    seed = 99;
  }
  return seed;
}

int ai_euro_20e6_type_flags(int dos_type);
int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);

/*
 * DOS-LITERAL FUN_521d_0a60 raw 88184-88191: the goal-capability gate is a
 * single bit test, `(1 << (goal.code & 0x1f)) & DS:0x523d[type * 0xe]`, i.e.
 * the goal code IS the bit index into the unit type's NAMES.TXT @UNIT
 * capability byte (k_20e6_type_flags). Plus the two unit+0x3148 clauses on
 * the same line: code 1 (FOUND) additionally needs bit2 (0x04), code 7
 * (MIL_EXPAND) additionally needs bit3 (0x08).
 *
 * Consequences of the table (they are DOS's, not an accident of this port):
 * FOUND(1) and MIL_EXPAND(7) are TRANSPORT goals — only the ship rows
 * 0x81/0x82/0xa2 carry bits 1 and 7. Land rows carry ESCORT(2)/LABOR(3)/
 * MILITARY(4)/COLONY(5); Colonist and Pioneer (0x40) carry only bit 6
 * (EXPLORE), a code the primary table never holds (ai_goals.h). The
 * previous port keyed on unit *names* and let land settlers chase FOUND.
 * bugs.md #511.
 */
/*
 * Fallback for pools whose types did not come from NAMES.TXT (hand-built test
 * type tables — `ai_euro_20e6_dos_type` returns −1 there, see conventions.md
 * "ColonizeUnit.type_index"): the pre-#511 name-keyed equivalent of the bit
 * test below. Never reached in a real game, where every type resolves.
 */
static int ai_euro_0a60_can_pursue_goal_by_name(
  ColonizeUnitKind kind, int goal_code, int is_ship, int has_bit2, int has_bit3
) {
  switch (goal_code) {
    case AI_GOAL_FOUND:
      if (is_ship) {
        return has_bit2;
      }
      return has_bit2 && (ai_euro_name_is_pioneer(kind) || kind == UNITS_KIND_COLONIST);
    case AI_GOAL_MIL_EXPAND:
      if (is_ship) {
        return has_bit3;
      }
      return has_bit3 && (ai_euro_is_military_name(kind) || ai_euro_is_artillery_name(kind));
    case AI_GOAL_MILITARY:
      if (is_ship) {
        return has_bit3;
      }
      return ai_euro_is_military_name(kind) || ai_euro_is_artillery_name(kind);
    case AI_GOAL_ESCORT:
      if (is_ship) {
        return 0;
      }
      return ai_euro_is_military_name(kind) || kind == UNITS_KIND_SCOUT;
    default:
      return 1;
  }
}

static int ai_euro_0a60_unit_can_pursue_goal(
  int dos_type, int goal_code, int has_bit2, int has_bit3,
  ColonizeUnitKind kind, int is_ship
) {
  if (goal_code < 0 || goal_code > 0x1f) {
    return 0;
  }
  if (dos_type < 0) {
    return ai_euro_0a60_can_pursue_goal_by_name(kind, goal_code, is_ship, has_bit2, has_bit3);
  }
  if (((1u << (unsigned)(goal_code & 0x1f)) & (unsigned)ai_euro_20e6_type_flags(dos_type)) == 0) {
    return 0;
  }
  if (goal_code == AI_GOAL_FOUND && !has_bit2) {
    return 0; /* raw 88189: `code != 1 || (unit+0x3148 & 4)` */
  }
  if (goal_code == AI_GOAL_MIL_EXPAND && !has_bit3) {
    return 0; /* raw 88190-88191: `code != 7 || (unit+0x3148 & 8)` */
  }
  return 1;
}

/*
 * Soldier/Dragoon continent-defense skip gate (raw: `land_units_here =
 * table[-0x6b5a][continent + nation*0x10]`, `colonies = table[-0x6b1a][...]`
 * — the same colony-count / land-unit-count-by-continent tables
 * euro_g_table_0a60.md resolved and ai_euro_refresh_continent_stance
 * already recomputes for its own purpose). Recomputed fresh here too
 * (cheap, one pass) rather than exposing that function's private locals.
 */
int ai_euro_20e6_own_colonies_on(const ColonizeTurnContext* ctx, int nation, int cid);
int ai_euro_10ec_land_units_on(const ColonizeTurnContext* ctx, int nation, int cid);

/*
 * Audit AE-9: the two halves of this function ARE the two later standalone
 * helpers. The only spelling differences were inert here — the colony half's
 * `cid >= 0` guard (a colony always stands on land, so its continent id is
 * never negative) and the unit half's 255 byte clamp (the sole caller only
 * compares against 2 and 3).
 */
static void ai_euro_0a60_continent_presence(
  const ColonizeTurnContext* ctx, int nation_id, int continent_id, int* out_colonies,
  int* out_land_units
) {
  *out_colonies = ai_euro_20e6_own_colonies_on(ctx, nation_id, continent_id);
  *out_land_units = ai_euro_10ec_land_units_on(ctx, nation_id, continent_id);
}

/*
 * FUN_521d_0a60's own goal-consumption tail, literally: for every idle-ish
 * unit of `nation_id`, pick the closest/highest-priority matching primary
 * goal slot and write order_code/act_state/goal_x/goal_y — mirrors raw
 * decomp lines 974-1063 control flow and arithmetic 1:1 (see file header
 * comment for what's real vs. placeholder). Since 2026-09-18 (bugs.md #525)
 * it writes the REAL DOS bytes (`u->col1_ai_plan` / `u->orders` /
 * `u->goto_x` / `u->goto_y`) through `ai_euro_set_goto`, so every downstream
 * reader — the 20e6 pre-LAB_4d2e gate included — sees the same course the
 * rest of the port sets, and the old file-local mirror is gone.
 */

/* --- 0a60 unit-loop housekeeping (raw lines 1-189) ----------------------
 *
 * Literal port of FUN_521d_0a60's opening per-unit loop (raw decomp lines
 * 704-811 of euro_goal_orders_0a60_full.md's recovery): the unit+0x3148
 * scratch-bit rederive, act-state transitions, the spare-transport mark and
 * the foreign-ship CONTACT goal producer. Previously skipped behind the
 * "FUN_1000_8aac field-id wall".
 *
 * 2026-09-06b CORRECTION: the wall's "4fa8" resolution targeted the wrong
 * function — FUN_1000_8aac = FUN_281f_08bc → FUN_1427_0d38, the stack-
 * query dispatcher, ALL cases byte-decoded (table at
 * ai_euro_20e6_stack_count; the 4fa8 chain was a Ghidra reloc misresolve).
 * The modes 0a60's unit loop reads DO carry real signal:
 *     mode 3 → # Pioneers in the unit's stack;
 *     mode 4 → # military land types {1,4,6,7,8,9} (`8aac(u,4) < 2` =
 *              "fewer than two military in the stack" — a real gate);
 *     mode 6 → mobilizable count (mil + veteran-professioned).
 *   The eligibility block below reads the REAL stack counts as of the
 *   2026-09-06b rewire (ai_euro_0a60_stack_counts; ships keep the literal
 *   hold-full + fleet-coordination gate on top), and the consumption
 *   tail's FOUND/MIL_EXPAND gates consume the bits for land units too —
 *   DOS reads `0x3148 & 4` / `& 8` for every unit, not only ships (raw
 *   tail, goal codes 1 and 7).
 *
 * Field ids resolved for the rest of the loop (address_mapping.csv chain
 * FUN_1000_X → FUN_281f_(X-0x81f0) → FUNCTION_CATALOG.md):
 *   FUN_1000_84f2 → FUN_137f_000a map_tile_in_bounds (inset interior);
 *   FUN_1000_8958 → FUN_13e4_0074 ocean_or_high_seas;
 *   FUN_1000_8d18 → FUN_15eb_08e6 unit-type-has-profession (region stamp
 *     bit variant only — the whole DS:0x9faa stamp is already covered by
 *     ai_coarse_fog_euro_restamp, which consumers only test for nonzero);
 *   -0x6da6/-0x6da5/0x9259 → unit_type_counts[4][19] (DS:0x924c,
 *     save_format_map.md row 252) at types 0x0e/0x0f/0x0d — Merchantman/
 *     Galleon/Caravel counts, recomputed here like FUN_4962_0018 does;
 *   DS:0x5382 bit0 → game_options.woi (Frigate CONTACT exception);
 *   unit+0x3147 high bits (0x10<<nation "this unit spotted") → per-nation
 *     map seen bit (map_tile_seen_by), the closest live substitute;
 *   relation gate (FUN_1000_8c28 raw peer byte): (d & 0x60) == 0x20 =
 *     MET(0x20) set + PEACE(0x40) clear — "met, no peace treaty".
 *
 * Runs before the colony loop each nation turn (DOS order). The shadow
 * state is fresh-zeroed each turn, so the pure act-state *resets* (orders
 * 'A'→'G', act 1/2/3→0) are structural no-ops kept for shape; the live
 * effects are the eligibility/spare bits, act_state=1 (in transit: aboard
 * ship, in Europe, off-map) excluding units from goal consumption,
 * act_state=10 (adjacent contact claim) doing the same, and the CONTACT
 * goals at spotted hostile ships.
 */
int ai_euro_20e6_dos_type(const ColonizeUnitPool* units, const ColonizeUnit* u);
int ai_euro_20e6_type_combat(int dos_type);

/*
 * FUN_1427_0d38 stack-query counts over a tile's stack (2026-09-06b byte
 * decode; case table at ai_euro_20e6_stack_count). DOS chains a ship's
 * passengers into its tile stack, so members here = units standing on the
 * tile plus the cargo of every ship on it:
 *   pioneers    case 3   # type 2
 *   mil_types   case 4   # types {1,4,6,7,8,9}
 *   mobilizable case 6   {1,4} else profession 0x15, plus {6..9} again
 *                        (a veteran-professioned 6..9 counts twice — kept)
 *   armed       case 0xa # non-ship with @UNIT combat (0x5236) > 1
 *   hold_cap    case 0xd Σ ship hold capacity (0x5237)
 */
typedef struct Ai0a60StackCounts {
  int pioneers;
  int mil_types;
  int mobilizable;
  int armed;
  int hold_cap;
} Ai0a60StackCounts;

static void ai_euro_0a60_stack_count_member(
  const ColonizeUnitPool* units, const ColonizeUnit* m, Ai0a60StackCounts* sc
) {
  const int t = ai_euro_20e6_dos_type(units, m);
  const int is_ship = (t >= 0x0d && t <= 0x12);
  if (t == 2) {
    sc->pioneers++;
  }
  if (t == 1 || t == 4 || (t >= 6 && t <= 9)) {
    sc->mil_types++;
  }
  if (t == 1 || t == 4) {
    sc->mobilizable++;
  } else if (m->profession == UNITS_JOB_SOLDIER) {
    sc->mobilizable++;
  }
  if (t >= 6 && t <= 9) {
    sc->mobilizable++;
  }
  if (!is_ship && ai_euro_20e6_type_combat(t) > 1) {
    sc->armed++;
  }
  if (is_ship) {
    sc->hold_cap += units_ship_capacity(units, m->id);
  }
}

static void ai_euro_0a60_stack_counts(
  const ColonizeUnitPool* units, int x, int y, Ai0a60StackCounts* sc
) {
  memset(sc, 0, sizeof(*sc));
  if (!units) {
    return;
  }
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* m = &units->units[i];
    if (!m->active || m->aboard_ship_id >= 0 || m->x != x || m->y != y) {
      continue;
    }
    ai_euro_0a60_stack_count_member(units, m, sc);
    for (int ci = 0; ci < m->cargo_count && ci < COLONIZE_UNIT_CARGO_MAX; ++ci) {
      const ColonizeUnit* pax = units_get_const(units, m->cargo_ids[ci]);
      if (pax && pax->active) {
        ai_euro_0a60_stack_count_member(units, pax, sc);
      }
    }
  }
}

/*
 * DOS unit+0x3150: the packed GOODS-hold count only (bumped by FUN_15eb_30b8
 * on a load, decomp 13331); passengers never enter it — the DOS saves show it
 * 0 on every opening Caravel carrying two colonists (test-saves-ai/TURN2).
 * Scanned over the ship's capacity, never the array bound, so the 255
 * empty-hold sentinel (col1_bridge.c:2485) is not an occupant.
 */
int ai_euro_0a60_goods_holds_used(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const int cap = units_ship_capacity(units, u->id);
  int goods = 0;
  for (int i = 0; i < cap && i < COLONIZE_UNIT_CARGO_MAX; ++i) {
    if (units_hold_amount(units, u->id, i) > 0) {
      goods++;
    }
  }
  return goods;
}

/*
 * DOS-LITERAL raw 87511-87514: `0x5237[type] == unit+0x3150` — every GOODS
 * hold in use. A transport carrying only passengers is never "full", never
 * gets +0x3148 bits 2/3, and cannot take a FOUND/MIL_EXPAND goal — which is
 * why DOS's opening ships wander (plan '9', act 0x0c) instead of walking a
 * goal. Counting passengers here (the pre-2026-09-19 reading) made those
 * ships FOUND-eligible and was what kept 0a60's ship binding switched off
 * (bugs.md #527).
 */
static int ai_euro_0a60_ship_full(const ColonizeUnitPool* units, const ColonizeUnit* u) {
  const int cap = units_ship_capacity(units, u->id);
  return cap > 0 && ai_euro_0a60_goods_holds_used(units, u) >= cap;
}

static void ai_euro_0a60_unit_housekeeping(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  /* unit_type_counts[nation][0x0d/0x0e/0x0f] recompute (FUN_4962_0018). */
  int caravels = 0;
  int merchantmen = 0;
  int galleons = 0;
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    const int t = ai_euro_20e6_dos_type(ctx->units, u);
    if (t == 0x0d) {
      caravels++;
    } else if (t == 0x0e) {
      merchantmen++;
    } else if (t == 0x0f) {
      galleons++;
    }
  }

  int spare_marked = 0; /* iStack_c: at most one spare-transport mark per turn */
  const int woi = (ctx->col1_ok && ctx->col1) ? (int)ctx->col1->head.game_options.woi : 0;

  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index, not a unit id. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* u = &ctx->units->units[ui];
    if (!u->active) {
      continue;
    }
    const int dos_type = ai_euro_20e6_dos_type(ctx->units, u);
    const int is_ship_t = (dos_type >= 0x0d && dos_type <= 0x12);
    if (u->nation_id == nation_id) {
      const int ux = u->x;
      const int uy = u->y;
      if (u->col1_ai_plan == 'A') {
        u->col1_ai_plan = 'G'; /* admitted labor → garrisoned */
      }
      u->col1_flags15 &= AI_EURO_F3148_KEEP; /* rederive bits 1/2/3/5 below */
      if (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) {
        u->col1_flags15 |= AI_EURO_F3148_ROAM; /* roam_reeval_pending */
      }

      /* FOUND/MIL_EXPAND eligibility bits — REWIRED 2026-09-06b to the
       * real 0d38 stack counts (raw: iStack_1e = 8aac(u,4) >= 2 ||
       * 8aac(u,6) != 0; iStack_1c = 8aac(u,3)): military-in-stack sets
       * bits 2+3, Pioneers-in-stack sets bit 2. A lone plain Colonist gets
       * no bits — DOS-faithful (colonists found via a stacked Pioneer/
       * escort, or via the ship goal fold in 20e6's band). */
      Ai0a60StackCounts sc;
      ai_euro_0a60_stack_counts(ctx->units, ux, uy, &sc);
      const int has_military = (sc.mil_types >= 2 || sc.mobilizable != 0);
      const int has_founders = (sc.pioneers != 0);
      if (has_founders || has_military) {
        int ok = 1;
        if (is_ship_t) {
          /* Literal DOS: only a full ship qualifies, and only when no
           * earlier-indexed own ship in the same stack is still loading. */
          ok = ai_euro_0a60_ship_full(ctx->units, u);
          for (int oi = 0; ok && oi < ui; ++oi) {
            const ColonizeUnit* o = &ctx->units->units[oi];
            if (!o->active || o->nation_id != nation_id || o->x != ux ||
                o->y != uy) {
              continue;
            }
            const int ot = ai_euro_20e6_dos_type(ctx->units, o);
            if (ot >= 0x0d && ot <= 0x12 && !ai_euro_0a60_ship_full(ctx->units, o)) {
              ok = 0;
            }
          }
        }
        if (ok) {
          /* Raw: iStack_1e → |= 0xc, iStack_1c → |= 4 — bit2 from either,
           * bit3 only from military. */
          u->col1_flags15 |= AI_EURO_F3148_FOUND;
          if (has_military) {
            u->col1_flags15 |= AI_EURO_F3148_MIL;
          }
        }
      }

      /* Spare-transport mark (bit5, one ship per nation per turn): with
       * fewer than 2 Merchantman+Galleon (or no Merchantman), a 2nd+
       * Caravel is the spare; otherwise the first Merchantman is. Only for
       * ships not already FOUND/MIL_EXPAND-eligible. */
      if (!spare_marked && is_ship_t &&
          (u->col1_flags15 & (AI_EURO_F3148_FOUND | AI_EURO_F3148_MIL)) == 0) {
        if (merchantmen + galleons < 2 || merchantmen == 0) {
          if (dos_type == UNITS_KIND_CARAVEL && caravels > 1) {
            u->col1_flags15 |= AI_EURO_F3148_SPARE;
            spare_marked = 1;
          }
        } else if (dos_type == UNITS_KIND_MERCHANTMAN) {
          u->col1_flags15 |= AI_EURO_F3148_SPARE;
          spare_marked = 1;
        }
      }

      /* Tile housekeeping: act-state transitions. */
      const int inset = (ux >= 1 && uy >= 1 && ux < ctx->map->width - 1 &&
                         uy < ctx->map->height - 1);
      int in_transit = 1;
      if (inset) {
        /* DS:0x9faa region stamp (|=1 / |=5 by profession-capability) is
         * covered by ai_coarse_fog_euro_restamp — its consumers only test
         * the byte for nonzero, so the 1-vs-5 split is behaviorally inert. */
        /*
         * DOS raw 87560-87564: `if (act_state == 3 || == 2 || == 1 ||
         * (act_state > 9 && order_code != '1')) act_state = 0;` — an AI
         * course only survives the nation-turn boundary when its order code
         * is the generic goal-pursue '1'; 't'/'i'/0x45 courses and every
         * one-step 0x0c commit are dropped and re-decided. Live since
         * 2026-09-18 (bugs.md #525) now that +0x314c is the real `u->orders`
         * byte; against the retired shadow (zeroed every dispatcher turn) the
         * whole test was vacuous.
         *
         * `== 1 || == 2 || == 3` ported unconditionally (bugs.md #528,
         * 2026-09-22): those DOS values mean "aboard a ship / in transit /
         * off-map" — on a landed unit, order 1 (SENTRY) is simply the
         * leftover "aboard" value `FUN_1427_10be` wrote at boarding (raw
         * 8297/8674); the LAB_3558 unload block (raw 89440-89560) never
         * rewrites it, so a unit unloaded during nation-turn N still reads 1
         * at N's end, and this clear (N+1's own 0a60 top) wipes it to 0
         * before 20e6 re-decides. That is exactly what the TURN2→3/TURN3→4
         * DOS save pairs show. The port's own first-colony landfall goto
         * memory used to piggyback the same `orders == SENTRY` value
         * (collides with this clear); it now lives in the port-only
         * `ai_landfall_wait` flag (units.h) instead, set alongside
         * `orders = UNITS_ORDER_SENTRY` at every AI unload site and read by
         * whichever caller needs the landfall memory, so this clear no
         * longer has to dodge it. The `>= 10` arm is the one that carries the
         * AI courses.
         */
        if ((u->orders >= AI_EURO_ACT_ADJACENT && u->col1_ai_plan != 0x31) ||
            (u->orders >= 1 && u->orders <= 3)) {
          u->orders = UNITS_ORDER_NONE;
        }
        int side = 0;
        if (ai_goals_probe_adjacent_contact_claim_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map), .col1=(ColonizeCol1Save*)(ctx->col1_ok ? ctx->col1 : NULL), .col1_ok=((ctx->col1_ok ? ctx->col1 : NULL) != NULL)}, ux, uy, nation_id, 1, &side) >= 0) {
          u->orders = AI_EURO_ACT_ADJACENT; /* on-site at a contact claim: no new goal */
        }
        const int on_water = map_tile_is_water(ctx->map, ux, uy) ||
                             map_tile_is_high_seas(ctx->map, ux, uy);
        if (!on_water || is_ship_t) {
          in_transit = 0;
        }
      }
      if (in_transit) {
        u->orders = UNITS_ORDER_SENTRY; /* +0x314c = 1: aboard ship / Europe / off-map */
      }
    } else if (u->nation_id >= 0 && u->nation_id < 4) {
      /* Foreign branch: spotted hostile ship → CONTACT goal prio 3.
       * Frigates only count once independence is declared (DS:0x5382 bit0). */
      if (is_ship_t && (dos_type != UNITS_KIND_FRIGATE || woi) &&
          map_tile_seen_by(ctx->map, u->x, u->y, nation_id) && ctx->col1_ok &&
          ctx->col1) {
        const uint8_t d = ai_diplo_read(ctx->col1, nation_id, u->nation_id);
        if ((d & (AI_DIPLO_MET | AI_DIPLO_PEACE)) == AI_DIPLO_MET ||
            dos_type == UNITS_KIND_PRIVATEER) {
          ai_goals_upsert_primary(nation_id, u->x, u->y, AI_GOAL_CONTACT, 3);
        }
      }
    }
  }
}

void ai_euro_0a60_goal_orders_structural(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  const int weight_seed = ai_euro_0a60_weight_seed(ctx, nation_id);
  int weight[AI_PRIMARY_SLOTS];
  for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
    weight[i] = weight_seed;
  }

  /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index, not a unit id. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* u = &ctx->units->units[ui];
    if (!u->active || u->nation_id != nation_id || u->id < 0 ||
        u->id >= COLONIZE_UNITS_MAX) {
      continue;
    }
    /*
     * Pioneer work orders 8/9 are FUN_521d_5b66's own switch cases, i.e.
     * act states DOS's raw 88164 gate (`0/5/6 only`) already refuses — the
     * explicit skip stays because units_pioneer_work_tick, not 5b66, drives
     * the job here and must not be hijacked mid-job.
     */
    if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
      continue;
    }
    if (u->col1_ai_plan == 'A') {
      continue; /* already admitted as labor */
    }
    if (u->orders < AI_EURO_ACT_ADJACENT) {
      u->col1_ai_plan = 0x3f; /* '?' pending-decision placeholder */
    }
    if (u->orders != UNITS_ORDER_NONE && u->orders != UNITS_ORDER_FORTIFY &&
        u->orders != UNITS_ORDER_FORTIFIED) {
      continue; /* raw 88164: fresh goals only at act_state 0/5/6 */
    }
    if (u->col1_ai_plan == 't' || u->col1_ai_plan == 'i') {
      u->col1_ai_plan = 0x3f; /* clear stale goal-pursuit code */
    }

    const ColonizeUnitKind ukind = ai_euro_unit_kind(ctx->units, u);
    const int unit_is_ship = ai_euro_is_ship_type(ctx->units, u->id);
    const int unit_continent = map_continent_id_at(ctx->map, u->x, u->y);

    /* unit+0x3148 bits 2/3, written by ai_euro_0a60_unit_housekeeping from
     * the real 0d38 stack counts (2026-09-06b rewire; ships additionally
     * behind DOS's hold-full + fleet-coordination gate): bit2 =
     * FOUND-eligible, bit3 = MIL_EXPAND-eligible — read for every unit,
     * land included, as the DOS tail does. */
    const int has_bit2 = (u->col1_flags15 & AI_EURO_F3148_FOUND) != 0;
    const int has_bit3 = (u->col1_flags15 & AI_EURO_F3148_MIL) != 0;

    if (!unit_is_ship && (ukind == UNITS_KIND_SOLDIER || ukind == UNITS_KIND_DRAGOON)) {
      int colonies = 0;
      int land_units = 0;
      ai_euro_0a60_continent_presence(ctx, nation_id, unit_continent, &colonies, &land_units);
      if (land_units < 3 && (land_units < 2 || colonies == 0)) {
        /* FUN_521d_0a60 raw 88176-88181, literally: at the head of the
         * goal-binding body, `if (type == 1 || type == 4) { A =
         * [cont + nation*0x10 - 0x6b5a]; if (A < 3 && (A < 2 ||
         * [cont + nation*0x10 - 0x6b1a] == 0)) goto LAB_521d_1fdf; }`,
         * and LAB_521d_1fdf (raw 88240) is the unit-loop increment. So
         * this IS the DOS skip, not a port invention (bugs.md #652).
         * -0x6b5a = land units per continent, -0x6b1a = colonies per
         * continent. DOS has no ship guard; types 1/4 are land anyway. */
        continue;
      }
    }

    int best_score = 9999;
    int best_slot = -1;
    for (int slot = 0; slot < AI_PRIMARY_SLOTS; ++slot) {
      const AiGoalSlot* g = ai_goals_primary(nation_id, slot);
      if (!g || g->code == AI_GOAL_EMPTY) {
        continue;
      }
      if (!ai_euro_0a60_unit_can_pursue_goal(
            ai_euro_20e6_dos_type(ctx->units, u), g->code, has_bit2, has_bit3,
            ukind, unit_is_ship
          )) {
        continue;
      }
      const int goal_continent = map_continent_id_at(ctx->map, g->x, g->y);
      if (goal_continent != unit_continent && !unit_is_ship) {
        continue; /* off-continent goal, land unit can't reach it */
      }

      const int dist = map_dos_dist(g->x - u->x, g->y - u->y);
      const int score = weight[slot] * dist / (g->prio + 1);

      if ((u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED) &&
          !unit_is_ship) {
        /*
         * Real check (fixed 2026-08-18, `address_mapping.csv`:
         * FUN_1000_8886 → canonical FUN_281f_0696 → FUN_137f_0358 =
         * `euro_settlement_owner`, already resolved in `accessors.c` —
         * NOT a "is anyone standing on the goal tile" unit lookup as
         * first ported. DOS's own args are the *unit's own* x/y
         * (`uStack_36`/`uStack_3a`, set from `unit+0x3144/0x3145`
         * earlier this same block), not the goal's — this checks
         * whether the re-evaluating unit is *currently sitting in any
         * Euro colony* (any nation), not whether the goal is occupied.
         * `colonies_id_at` is the Linux equivalent (colony pool holds
         * only Euro colonies; native villages are a separate `col1`
         * table, matching DOS's own tribe-owner exclusion in
         * `euro_settlement_owner`).
         */
        const int at_colony = colonies_id_at(ctx->colonies, u->x, u->y) >= 0;
        if (at_colony ||
            (g->prio < 3 || (g->prio * weight_seed < score && weight[slot] != weight_seed))) {
          continue; /* re-evaluating unit already parked in a colony, or goal not worth it */
        }
      }

      if (score < best_score && score / weight_seed <= g->prio * 3 / 2) {
        best_score = score;
        best_slot = slot;
      }
    }

    if (best_slot >= 0) {
      const AiGoalSlot* g = ai_goals_primary(nation_id, best_slot);
      u->col1_ai_plan = 0x31; /* '1' default goal-pursue code */
      if (g->code == AI_GOAL_FOUND) {
        u->col1_ai_plan = 0x74; /* 't' */
      } else if (g->code == AI_GOAL_MIL_EXPAND) {
        u->col1_ai_plan = 0x69; /* 'i' */
      }
      /*
       * bugs.md #525: the commit writes the REAL +0x314c/+0x314d/e now
       * (act_state 0x0b + the goal tile), so `ai_euro_move_scoring_gate`'s
       * raw 90210-90219 test and `ai_euro_act_land_goal_dispatch` both read
       * the unit's own stored goal instead of a private mirror the rest of
       * the port never stamped.
       *
       * Ships bind here too, as in DOS (a 0x0b hull whose DS:0x523d bit0 is
       * clear — every transport row 0xa2/0x82 — skips FUN_521d_20e6 and
       * just walks the goal, FUN_521d_5b66 raw 90552-90560). Until
       * 2026-09-19 this was land-only: ai_euro_0a60_ship_full counted
       * passengers, so every loaded opening transport looked FOUND-eligible
       * and got pulled off its landfall (bugs.md #527).
       */
      ai_euro_set_goto(u, AI_EURO_ACT_GOAL, g->x, g->y);
      if (g->code != AI_GOAL_MILITARY) {
        weight[best_slot]++; /* claim-count so the same slot isn't over-assigned */
      }
    }
  }
}

/* --- 0a60 foreign-colony / village goal producers ----------------------
 *
 * Literal port of FUN_521d_0a60's foreign-colony branch + village loop
 * (viceroy_unpacked.c ~87795-88052; raw lines 983-1276 of
 * euro_goal_orders_0a60_full.md's recovery) — the FOUND/CONTACT producers
 * that write OCEAN tiles next to foreign colonies and villages as ship
 * goals. These pair with the DS:0x523d capability mask: FOUND(1)/
 * MIL_EXPAND(7) goals match bit1/bit7 = the transport ships
 * (Caravel/Merchantman/Galleon 0xa2/0x82/0x82), CONTACT(0) matches bit0 =
 * the warships — i.e. DOS stages loaded transports at open-sea tiles
 * adjacent to land worth settling, and lurks warships two tiles off
 * foreign harbors. Land units can't take these goals (water tile →
 * continent −1 ≠ any land continent, and the tail's ship gate).
 *
 * Resolved symbols (FUN_1000_X → FUN_281f_(X−0x81f0) → catalog):
 *   84f2 map_tile_in_bounds; 8958 ocean_or_high_seas; 88a4 layer3 low
 *   nibble (raw water-region id — region 1 = open sea, see map.c's lake
 *   note); 8912 map_continent_id_at (land only, −1 on water); 893a
 *   tile_explore_mask (bit 0x10<<nation = map_tile_seen_by); 8872
 *   tile_owner_or_presence (layer2 bit0 + layer3 owner nibble); 8bd6/8c3c
 *   colony/village binds; 84fc alarm_by_player; 8c28 raw peer byte
 *   ((d&0x48)==0x40 = PEACE set + amicable-latch 0x08 clear;
 *   (d&0x60)==0x20 = MET set + PEACE clear); DS:0x538e head.turn;
 *   DS:0x53a6 head.difficulty; DS:0x543f polarity 0 = human;
 *   −0x6ada skilled-unit counts (FUN_281f_0b78 = "type has a profession
 *   slot"); −0x7a38 continent_tally_b; DS:0x173c/0x173e per-continent
 *   MIL_EXPAND/FOUND-registered masks (zeroed at 0a60 entry — locals
 *   here, shared between the two loops exactly as DOS shares them).
 *
 * FUN_1000_8aac mode 2 — 2026-09-06b: the real 0d38 case-2 body IS the
 * plain stack count, so the plain units_count_at walk is byte-right (the old
 * "4fa8 chain splice, substituted" story targeted the wrong function).
 * Mode 0xd (every-4th CONTACT-scan skip, Σ ship hold capacity in the
 * stack) is wired for real at the lurk scan — 2026-09-06b rewire.
 * The village-population scratch (aiStack_14e) only feeds a dead
 * accumulator and the G-formula's Indian presence test, which Linux's
 * stance recompute already covers via live Brave units — not modeled.
 */

/* Raw water-region check: FUN_1000_8958 && FUN_1000_88a4(x,y) == 1. */
static int ai_euro_0a60_open_sea(const ColonizeWorldMap* map, int x, int y) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    return 0;
  }
  if (!map_tile_is_water(map, x, y) && !map_tile_is_high_seas(map, x, y)) {
    return 0;
  }
  return (map_get_layer3(map, x, y) & 0x0fu) == 1;
}

/* FUN_1000_8872 tile_owner_or_presence: layer2 bit0 + layer3 owner nibble. */
static int ai_euro_0a60_tile_owner_or_presence(const ColonizeWorldMap* map, int x, int y) {
  if (!map || !map->layer2 || x < 1 || y < 1 || x >= map->width - 1 ||
      y >= map->height - 1) {
    return -1;
  }
  if ((map->layer2[y * map->width + x] & 1u) == 0) {
    return -1;
  }
  const int hi = (int)((map_get_layer3(map, x, y) >> 4) & 0x0fu);
  return hi == 0x0f ? -1 : hi;
}

/* Foreign-colony producer loop (raw 983-1212) of
 * ai_euro_0a60_settlement_goal_producers. The two DS continent masks
 * (0x173c MIL_EXPAND / 0x173e FOUND) are threaded in and out so the loop body
 * stays a verbatim transcription. */
static void ai_euro_0a60_foreign_colony_producers(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeWorldMap* map,
  int have_col1, int turn, int difficulty, int human,
  uint8_t col_cnt[4][16], uint8_t land_cnt[4][16], uint8_t skilled_cnt[4][16],
  uint16_t* io_mask_mil_expand, uint16_t* io_mask_found
) {
  uint16_t mask_mil_expand = *io_mask_mil_expand;
  uint16_t mask_found = *io_mask_found;

  /* Foreign-colony loop (raw 983-1212). */
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id == nation_id || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    const int owner = c->nation_id;
    const int cont = map_continent_id_at(map, c->x, c->y);
    if (cont < 0 || cont >= 16) {
      continue;
    }
    const int seen = map_tile_seen_by(map, c->x, c->y, nation_id);
    const uint8_t d = have_col1 ? ai_diplo_read(ctx->col1, nation_id, owner) : 0;
    /* (d & 0x48) == 0x40 — PEACE without the amicable latch. */
    const int at_peace = ((d & (AI_DIPLO_PEACE | 0x08)) == AI_DIPLO_PEACE);
    /* Early fair-play gate #1 (difficulty×turn < 0xb5): skip the approach/
     * lurk block for an unseen human colony. DS:0x543f polarity 0=human. */
    const int block1 = !(difficulty * turn < 0xb5 && !seen && owner == human);
    if (block1) {
      /* MILITARY approach goal (defender count = 0d38 case-2 stack count
       * + population, byte-right). Prio 3 while formally at peace, 5
       * otherwise. */
      if ((int)col_cnt[nation_id][cont] + (int)land_cnt[nation_id][cont] != 0 &&
          ((i + turn) & 3) != 0) {
        const int defenders =
          units_count_at(ctx->units, c->x, c->y) + (int)c->population;
        if (defenders > 6 - turn / 50) {
          ai_goals_upsert_primary(
            nation_id, c->x, c->y, AI_GOAL_MILITARY, at_peace ? 3 : 5
          );
        }
      }
      if (at_peace) {
        continue; /* raw: goto next colony — no lurk/staging vs peace peers */
      }
      /* CONTACT lurk scan: ring-2 open-sea tiles off a coastal foreign
       * colony (raw 1012-1089; +0x1c bit 0x40 coastal, live recompute).
       * Every-4th early-out REWIRED 2026-09-06b: raw
       * `(unit_at_tile + turn) % 4 == 0 && 8aac(unit_at_tile, 0xd) == 0`
       * — 0d38 case 0xd = Σ ship hold capacity over the colony-tile
       * stack; a foreign harbor with no ship capacity present skips the
       * lurk scan on its beat (the staging scan below still runs, as in
       * DOS's goto past LAB_11b6). unit_at_tile −1 when the tile is
       * empty, matching DOS's miss value through the same arithmetic. */
      int lurk_skip = 0;
      {
        const int uid_at = units_id_at(ctx->units, c->x, c->y);
        if ((uid_at + turn) % 4 == 0) {
          Ai0a60StackCounts lsc;
          ai_euro_0a60_stack_counts(ctx->units, c->x, c->y, &lsc);
          lurk_skip = (lsc.hold_cap == 0);
        }
      }
      if (!lurk_skip && map_tile_is_coastal(map, c->x, c->y)) {
        int best = 0;
        int bx = 0;
        int by = 0;
        for (int dy = -2; dy <= 2; ++dy) {
          for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) {
              continue;
            }
            if (abs(dx) != 2 && abs(dy) != 2) {
              continue; /* Chebyshev ring 2 only */
            }
            const int tx = c->x + dx;
            const int ty = c->y + dy;
            if (!ai_euro_0a60_open_sea(map, tx, ty)) {
              continue;
            }
            int cnt = 0;
            for (int k = 0; k < 8; ++k) {
              const int nx = tx + MAP_DIR8_DX[k];
              const int ny = ty + MAP_DIR8_DY[k];
              if (!ai_euro_0a60_open_sea(map, nx, ny)) {
                continue;
              }
              if (abs(c->x - nx) < 2 && abs(c->y - ny) < 2) {
                cnt++; /* open-sea tile adjacent to both candidate and colony */
              }
            }
            if (best < cnt) {
              best = cnt;
              bx = tx;
              by = ty;
            }
          }
        }
        if (best > 0) {
          int p = ((int)c->population + 4) >> 3;
          if (p > 2) {
            p = 2;
          }
          ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_CONTACT, p + 2);
        }
      }
    }
    /* LAB_521d_11b6: FOUND/MIL_EXPAND ship-staging scan. Early fair-play
     * gate #2 (difficulty×turn < 0xc9): unseen colony → next colony. */
    if (difficulty * turn < 0xc9 && !seen) {
      continue;
    }
    int outmatched = 0; /* iStack_2e: fewer colonies here than a developed owner */
    int absent = 0;     /* bVar5: no colonies here, owner not yet developed */
    if (col_cnt[nation_id][cont] < col_cnt[owner][cont] && skilled_cnt[owner][cont] >= 8) {
      outmatched = 1;
    }
    if (col_cnt[nation_id][cont] == 0 && skilled_cnt[owner][cont] < 8) {
      absent = 1;
    }
    if (!outmatched && !absent) {
      continue;
    }
    int best = -99;
    int bx = c->x;
    int by = c->y;
    for (int dx = -3; dx <= 3; ++dx) {
      for (int dy = -3; dy <= 3; ++dy) {
        const int tx = c->x + dx;
        const int ty = c->y + dy;
        if (!ai_euro_0a60_open_sea(map, tx, ty)) {
          continue;
        }
        int cnt = 0;
        for (int k = 0; k < 8; ++k) {
          const int nx = tx + MAP_DIR8_DX[k];
          const int ny = ty + MAP_DIR8_DY[k];
          if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
            continue;
          }
          if (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) {
            continue;
          }
          if (map_continent_id_at(map, nx, ny) == cont) {
            cnt++;
          }
        }
        if (cnt != 0) {
          const int sc = (abs(dx) + abs(dy) + cnt) * 2;
          if (best <= sc) { /* raw `iStack_15a <= iStack_e`: later ties win */
            best = sc;
            bx = tx;
            by = ty;
          }
        }
      }
    }
    if (best <= 0) {
      continue;
    }
    if (ai_euro_0a60_tile_owner_or_presence(map, bx, by) >= 0) {
      continue; /* FUN_1000_8872: someone already claims that tile */
    }
    if (outmatched) {
      mask_mil_expand |= (uint16_t)(1u << cont);
    } else {
      mask_found |= (uint16_t)(1u << cont);
    }
    /* Priority ladder, transliterated (raw 1162-1196). */
    const int tally =
      have_col1 ? (int)ctx->col1->post_map.continent_tally_b[cont] : 0;
    const int total_cols = (int)col_cnt[0][cont] + (int)col_cnt[1][cont] +
                           (int)col_cnt[2][cont] + (int)col_cnt[3][cont];
    int e = outmatched ? 3 : 2;
    int v = e;
    if (owner == human) {
      v = e + 1;
      if (total_cols == (int)col_cnt[owner][cont]) {
        if (tally > 0xf) {
          v = e + 2;
        }
        e = v;
        v = e;
        if (tally > 0x3f) {
          v = e + 1;
        }
      }
    }
    e = v;
    if (tally < total_cols * 0x10) {
      e -= 1;
    }
    if ((d & (AI_DIPLO_MET | AI_DIPLO_PEACE)) == AI_DIPLO_MET) {
      e += 1; /* met, no peace treaty */
    }
    if (turn < 0x96) {
      e <<= 1;
    }
    /* Weakly-defended target cancels the staging goal (raw 1197-1203). */
    const int defenders =
      units_count_at(ctx->units, c->x, c->y) + (int)c->population;
    if (defenders <= 6 - turn / 50) {
      outmatched = 0;
      absent = 0;
    }
    if (outmatched || absent) {
      ai_goals_upsert_primary(
        nation_id, bx, by, outmatched ? AI_GOAL_MIL_EXPAND : AI_GOAL_FOUND, e
      );
    }
  }

  *io_mask_mil_expand = mask_mil_expand;
  *io_mask_found = mask_found;
}

/* Village producer loop (raw 1215-1276) of
 * ai_euro_0a60_settlement_goal_producers; same mask threading as above. */
static void ai_euro_0a60_village_producers(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeWorldMap* map,
  int have_col1, int turn,
  uint8_t col_cnt[4][16], uint8_t land_cnt[4][16],
  uint16_t* io_mask_mil_expand, uint16_t* io_mask_found
) {
  uint16_t mask_mil_expand = *io_mask_mil_expand;
  uint16_t mask_found = *io_mask_found;

  /* Village loop (raw 1215-1276). */
  if (have_col1 && ctx->col1->tribe) {
    for (uint16_t vi = 0; vi < ctx->col1->head.tribe_count; ++vi) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[vi];
      const int vx = (int)t->x;
      const int vy = (int)t->y;
      const int cont = map_continent_id_at(map, vx, vy);
      if (cont < 0 || cont >= 16) {
        continue;
      }
      const int ind = (int)t->nation_id; /* 4..11 */
      /* MILITARY at the village: own presence on the continent, and either
       * alarm >= 0x4b or the Indian-matrix WAR bit (FUN_1000_8c28 & 2).
       * Prio 2 for a mission-less village (+5 byte 0xff → sign bit), 4
       * when a mission stands there. */
      if ((int)col_cnt[nation_id][cont] + (int)land_cnt[nation_id][cont] != 0 &&
          ind >= 4 && ind < 12) {
        const int alarm = ai_diplo_indian_alarm(ctx->col1, ind, nation_id);
        int fire = 1;
        if (alarm < 0x4b) {
          fire = (ctx->col1->indian[ind - 4].euro_diplo[nation_id] &
                  COL1_INDIAN_WAR_BIT) != 0;
        }
        if (fire) {
          const int prio = (t->mission & 0x80u) ? 2 : 4;
          ai_goals_upsert_primary(nation_id, vx, vy, AI_GOAL_MILITARY, prio);
        }
      }
      /* Ship FOUND staging next to a village on a continent with no own
       * colony and no staging goal registered yet this turn: best village-
       * adjacent open-sea tile by count of same-continent land neighbours
       * (a zero-count ocean tile still qualifies — DOS init is −1). */
      if ((mask_mil_expand & (1u << cont)) == 0 && (mask_found & (1u << cont)) == 0 &&
          col_cnt[nation_id][cont] == 0) {
        int best = -1;
        int bx = -1;
        int by = -1;
        for (int k = 0; k < 8; ++k) {
          const int tx = vx + MAP_DIR8_DX[k];
          const int ty = vy + MAP_DIR8_DY[k];
          if (!ai_euro_0a60_open_sea(map, tx, ty)) {
            continue;
          }
          int cnt = 0;
          for (int m = 0; m < 8; ++m) {
            const int nx = tx + MAP_DIR8_DX[m];
            const int ny = ty + MAP_DIR8_DY[m];
            if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height) {
              continue;
            }
            if (map_tile_is_water(map, nx, ny) || map_tile_is_high_seas(map, nx, ny)) {
              continue;
            }
            if (map_continent_id_at(map, nx, ny) == cont) {
              cnt++;
            }
          }
          if (best < cnt) {
            best = cnt;
            bx = tx;
            by = ty;
          }
        }
        if (bx > 0) { /* raw `0 < (int)uStack_24` */
          ai_goals_upsert_primary(nation_id, bx, by, AI_GOAL_FOUND, 2);
          mask_found |= (uint16_t)(1u << cont);
        }
      }
    }
  }

  *io_mask_mil_expand = mask_mil_expand;
  *io_mask_found = mask_found;
}

static void ai_euro_0a60_settlement_goal_producers(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units || !ctx->colonies) {
    return;
  }
  const ColonizeWorldMap* map = ctx->map;
  const int have_col1 = (ctx->col1_ok && ctx->col1 != NULL);
  const int turn = have_col1 ? (int)ctx->col1->head.turn
                             : ((ctx->turn_number && *ctx->turn_number) ? (int)*ctx->turn_number : 0);
  const int difficulty = have_col1 ? (int)ctx->col1->head.difficulty : 2;
  const int human = have_col1 ? (int)ctx->col1->head.human_player : -1;

  /* FUN_4962_0018-style per-nation/continent tables (colonies, land units,
   * skilled units — the −0x6b1a/−0x6b5a/−0x6ada trio). */
  uint8_t col_cnt[4][16];
  uint8_t land_cnt[4][16];
  uint8_t skilled_cnt[4][16];
  memset(col_cnt, 0, sizeof(col_cnt));
  memset(land_cnt, 0, sizeof(land_cnt));
  memset(skilled_cnt, 0, sizeof(skilled_cnt));
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &ctx->colonies->colonies[i];
    if (!c->active || c->nation_id < 0 || c->nation_id > 3) {
      continue;
    }
    const int cid = map_continent_id_at(map, c->x, c->y);
    if (cid >= 0 && cid < 16 && col_cnt[c->nation_id][cid] < 0xff) {
      col_cnt[c->nation_id][cid]++;
    }
  }
  /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id < 0 || u->nation_id > 3 ||
        units_is_sea(ctx->units, u->id)) {
      continue;
    }
    const int cid = map_continent_id_at(map, u->x, u->y);
    if (cid < 0 || cid >= 16) {
      continue;
    }
    if (land_cnt[u->nation_id][cid] < 0xff) {
      land_cnt[u->nation_id][cid]++;
    }
    if (units_type_has_profession_slot(u->type_index) &&
        skilled_cnt[u->nation_id][cid] < 0xff) {
      skilled_cnt[u->nation_id][cid]++;
    }
  }


  uint16_t mask_mil_expand = 0; /* DS:0x173c */
  uint16_t mask_found = 0;      /* DS:0x173e */

  ai_euro_0a60_foreign_colony_producers(
    ctx, nation_id, map, have_col1, turn, difficulty, human, col_cnt, land_cnt,
    skilled_cnt, &mask_mil_expand, &mask_found
  );

  ai_euro_0a60_village_producers(
    ctx, nation_id, map, have_col1, turn, col_cnt, land_cnt, &mask_mil_expand,
    &mask_found
  );
}

/* --- FUN_5952_035e colony threat accumulator ---------------------------- */

/*
 * DS:0x543f stride-0x34 control byte == 0 → that Euro nation is human-driven.
 * Same table/idiom as nation_crosses_bells_1f72 / europe_nation_eot.md.
 */
static int ai_euro_nation_is_human(const ColonizeTurnContext* ctx, int nation) {
  if (!ctx || nation < 0 || nation > 3) {
    return 0;
  }
  if (ctx->human_nation >= 0 && ctx->human_nation <= 3) {
    return nation == ctx->human_nation;
  }
  if (ctx->col1_ok && ctx->col1) {
    return ctx->col1->player[nation].control == 0;
  }
  return 0;
}

/*
 * Colony threat accumulator → garrison_quota (+0x1e). Ported DOS-LITERALLY
 * 2026-09-08 from the CLEAN RECOVERY in
 * `original_sources_annotated/ai/colony_tick_5952_035e.md` (raw body lines
 * 233-324 there); the canonical Ghidra export of FUN_5952_035e is corrupted
 * (decomp_inventory.md), but `viceroy_unpacked.c:94940-94975` carries the same
 * accumulator body under argless far calls and corroborates it operand for
 * operand. This REPLACES the old "idle unfortified Soldier/Dragoon on the
 * colony tile and quota == 0 → quota = 1" thin latch.
 *
 *   threat = 0
 *   for dy in -5..5, dx in -5..5:                        (11x11 box)
 *     t = (colony.x + dx, colony.y + dy)
 *     if !map_tile_in_bounds(t): continue                 FUN_1000_84f2/281f_0302
 *     head = first unit on t                              FUN_1000_89d0/281f_07e0
 *     if head < 0 or (head.nation & 0xf) == colony.nation: continue
 *     for every unit u on t (whole stack, FUN_1000_84d4/281f_02e4):
 *       if 0x0d <= u.type <= 0x12: skip                   (ships never count)
 *       v = combat value x8, mode 1                       FUN_1000_8bb8/281f_09c8
 *       if (u.nation & 0xf) < 4:                          European owner
 *         if v < 2: v = 0
 *         if DS:0x543f[u.nation] == 0: v += v >> 1        (human units count 1.5x)
 *       else:                                             Indian owner
 *         if alarm(u.nation - 4, colony.nation) < 0x19: v = 0   FUN_281f_030c
 *         if DS:0x54f6[u.+0x314a * 9 + colony.nation] < 0x80: v = 0
 *       if settlement_owner(t) >= 0: v >>= 1              FUN_1000_88ae/281f_06be
 *       v = -(dos_dist(dx, dy) - 8) * v >> 3              FUN_1000_8560/281f_0370
 *       threat += v
 *   floor  = min(threat, 0x10)
 *   walls  = owned buildings along the Stockade→Fort→Fortress parent chain
 *                                                        FUN_1000_8ca0(0)/281f_0ab0
 *   threat = max(threat / (walls + 1), floor)
 *   colony.garrison_quota = (uint8_t)(threat >> 3)        (DOS `(char)` truncation)
 *
 * The divide only bites above the 0x10 floor, so walls never drag the quota
 * below 2 — they only cap a very large threat.
 *
 * DS:0x54f6 read site (this closes the 5952_035e half of the "two parked
 * tension readers" punch-list row, docs/indians.md): the row key is the
 * ADJACENT unit's own home settlement id — DOS unit +0x06 / DS:0x314a =
 * `ColonizeUnit.home_tribe_id` — not the tribe owning the nearest village.
 * The apparent "stride 9 words" is the settlement record stride 0x12, so
 * the cell is that record's attitude[nation] word = tribe.alarm[nation]
 * (col1_tribe_attitude, signed; raw 94967 `w < 0x80`). Repointed off the
 * phantom `indian_tension` array 2026-09-08.
 *
 * The same loop's `iStack_22` ring-1 counter and the `iStack_76`
 * labor_shortage (+0x8e) formula that consumes it were parked when this was
 * first ported; both are live since 2026-09-09 (smell audit #38/#41) and are
 * documented at their site in the tail of this function, together with the
 * +0x1b `& 7` clear and the 0x40 / 0x08 / 0x04 flag writers.
 */
/* FUN_5952_035e labor/garrison demand stage: colony+0x8e (`labor_shortage`)
 * from pop + units outside, and the homed-military tally. */
static void ai_euro_5952_labor_demand(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  const ColonizeCol1Save* col1, int quota, int ring1,
  int* out_n, int* out_want, int* out_homed_mil
) {
  /*
   * ---- labor_shortage (+0x8e) and the +0x1b flag byte -------------------
   * Ported 2026-09-09 (smell audit #38/#41) from raw 94029-94071 and
   * 94141-94199, the continuation of the same DOS body. Previously the port
   * stopped at the quota above and substituted (a) a thin
   * `labor_shortage = 1` demand latch in ai_euro_colony_goals and (b)
   * `NEEDS_GARRISON = garrison_quota > 0`, which only fires at threat >= 8
   * where DOS raises the bit on any pop>=3 town with no soldier standing in
   * it.
   *
   *   n     = colony.population(+0x1f) + DS:0x8d72 (units on the colony tile
   *           with a profession slot, DS:0x30e[type] >= 0, capped at 0x32)
   *   want  = clamp(max((n - 1) / 2, quota), <= n / 2)
   *   want += 1                      when DS:0x5382 bit0 (WoI declared)
   *   want  = 1                      when ring1 != 0 && n > 1 && want < 1
   *   colony.labor_shortage = want   (unconditional, every tick)
   *   for each non-ship unit on the colony tile, in order:
   *       if combat_byte(0x5236[type]) > 1 && want != 0: want--
   *   ai_flags |= 0x40               iff want > 0
   */
  const int pop = (int)c->population;
  int outside = 0; /* DS:0x8d72 — FUN_15eb_09c0 raw 10014-10034 */
  if (ctx->units) {
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->aboard_ship_id >= 0 || u->x != c->x || u->y != c->y) {
        continue;
      }
      if (units_type_has_profession_slot(ai_euro_20e6_dos_type(ctx->units, u))) {
        outside++;
      }
    }
  }
  if (outside > 0x32) {
    outside = 0x32; /* raw 10031-10033 */
  }
  const int n = pop + outside;
  int want = (n - 1) / 2;
  if (want < quota) {
    want = quota;
  }
  if (n / 2 < want) {
    want = n / 2;
  }
  if (col1 && col1->head.game_options.woi != 0) {
    want++; /* DS:0x5382 bit0 — War of Independence declared */
  }
  if (ring1 != 0 && n > 1 && want < 1) {
    want = 1;
  }
  c->labor_shortage = (uint8_t)(want < 0 ? 0 : (want > 255 ? 255 : want));
  const int want_pre = want;
  if (ctx->units) {
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || u->aboard_ship_id >= 0 || u->x != c->x || u->y != c->y) {
        continue;
      }
      const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
      if (dtype >= 0x0d && dtype <= 0x12) {
        continue; /* ships never garrison */
      }
      if (ai_euro_20e6_type_combat(dtype) > 1 && want != 0) {
        want--;
      }
    }
  }

  /*
   * local_82 (raw 94063-94071): this nation's LAND military homed to this
   * colony (+0x314a origin == colony), minus the ones the tile walk above
   * already counted as garrison. So it is the OFF-STATION surplus, not the
   * raw defender count — the reading in colony.h's bit-pair note is the
   * simplification; this is the literal DOS quantity.
   */
  int homed_mil = 0;
  if (ctx->units) {
    /* Slot walk (Leads 2, 2026-09-10): `i` is an array index, not a unit id. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &ctx->units->units[i];
      if (!u->active || (u->nation_id & 0xf) != nation_id) {
        continue;
      }
      if ((int)u->col1_origin != c->id) {
        continue;
      }
      const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
      if (dtype >= 0x0d && dtype <= 0x12) {
        continue;
      }
      if (ai_euro_20e6_type_combat(dtype) > 1) {
        homed_mil++;
      }
    }
  }
  homed_mil -= (want_pre - want);

  *out_n = n;
  *out_want = want;
  *out_homed_mil = homed_mil;
}

/* FUN_5952_035e ai_flags stage: the wanted-garrison formula and the +0x1b
 * bit writes. */
static void ai_euro_5952_ai_flags(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int n, int want,
  int homed_mil
) {
  /*
   * Wanted defenders, local_74 (raw 94150-94193):
   *   lt2  = @LEADERNAME column 2 (DS:0x9568, signed, stride 3 per nation)
   *   base = (n * 3 >> 1) - lt2 - (turn >> 7)
   *   div  = lt2 + 5, or lt2 + 6 when want != 0; −1 more when stance == 4
   *   base += 2 when stance == 0; += 1 when stance == 3
   *   wanted = base / div
   *   wanted = 0                 when presence[cont] == 0 && stance != 0
   *   wanted = min(wanted, 1)    when presence[cont] bit0 set AND
   *                              (presence & 6) != 0 AND nation != 2
   *     (the other half of that branch is the Indian war-declare block,
   *      already ported as ai_contact_colony_tick_war_5952)
   * Substitution: DS:0x95f2 is DOS's per-continent presence bitmask, zeroed
   * at the top of every per-nation FUN_4962_0018 call (raw 78149-78150), so
   * it always describes the nation being censused; the port has no stored
   * mirror, so bit0/1/2 are recomputed here from live state for this
   * nation's point of view (docs/save_format_map.md row 156, corrected
   * 2026-09-10 audit C7).
   */
  const int cont = map_continent_id_at(ctx->map, c->x, c->y);
  const int stance = ai_euro_continent_stance_at(nation_id, cont);
  const int lt2 = ai_diplo_leader_trait(ctx, nation_id, 2);
  const uint32_t turn = ctx->turn_number ? *ctx->turn_number : 0u;
  int base = ((n * 3) >> 1) - lt2 - (int)(turn >> 7);
  int div = lt2 + (want != 0 ? 6 : 5);
  if (stance == 4) {
    div -= 1;
  }
  if (stance == 0) {
    base += 2;
  } else if (stance == 3) {
    base += 1;
  }
  int wanted = div != 0 ? base / div : 0;
  /*
   * DS:0x95f2[cont] `continent_presence_flags` — the war-declare half of this
   * same FUN_5952_035e body reads the identical byte ~6 lines later, so the
   * computation lives once, in ai_contact.c, which carries the full
   * FUN_4962_0018 citation for all three bits and the argument that the array
   * is zeroed at the top of every per-nation call. Two copies that disagreed
   * about the aboard-ship filter and the owner-nibble mask were merged
   * 2026-09-10 (audit C7); the same audit refuted docs/save_format_map.md row
   * 156's "not cleared between nations, accumulates across the full per-turn
   * pass". Bit 8 (this nation's own dug-in field force) has no reader here; it
   * is modelled since 2026-09-10 for the DS:0xa89c tally FUN_521d_20e6's
   * war-cargo scorer consumes, and is simply ignored in this arm. The local
   * rename wrapper this used to go through went with audit AE-34.
   */
  const int presence = ai_contact_continent_presence_4962(ctx, nation_id, cont);
  if (presence == 0 && stance != 0) {
    wanted = 0;
  }
  if ((presence & 1) != 0 && ((presence & 6) != 0 && nation_id != 2) && wanted > 1) {
    wanted = 1;
  }

  /*
   * raw 94142: DOS clears the whole flag byte down to `& 7` before the
   * recompute — bits 0x01/0x02 (census, FUN_4962_0018's own disjoint mask)
   * and 0x04 survive; 0x08/0x10/0x20/0x40/0x80 are rebuilt every tick.
   * 0x04 is deliberately sticky in DOS: only its consumers clear it
   * (raw 85332 FUN_4d56_4528, 90168 FUN_521d_20e6, 94247 the join loop) —
   * those clears are NOT ported yet, see the note in colony.h.
   */
  c->ai_flags = (uint8_t)(c->ai_flags & 0x07u);
  /*
   * raw 94143-94146 (= colony_tick_5952_035e.md:487-490): the FIRST of DOS's
   * two +0x1b bit 0x10 (NEEDS_COLONISTS) writers, and it runs exactly here —
   * immediately after the `&= 7` clear above, before every other flag writer
   * of the tick:
   *   if ((+0x1c & 0x10) && +0x1f < ' ') { +0x1b |= 0x10; +0x1c &= 0xef; }
   * A one-shot hand-off: the +0x1c bit 0x10 latch (COLONIZE_COLONY_FLAG_SMALL_AI,
   * col1_save.h `small_colony_ai`) is consumed AND cleared, raising
   * NEEDS_COLONISTS once for a colony still under 0x20 population. That read
   * is the only reference to +0x1c bit 0x10 in the whole DOS image: no writer
   * of the bit exists in viceroy_unpacked.c, viceroy_overlays.c or
   * viceroy_unpacked_2.c (checked over every `(byte *)(x + 0x1c)` access and
   * every `| 0x10` / `& 0xef` store), so in DOS the bit can only arrive from
   * a loaded save. The port used to re-stamp it from an invented `pop < 10`
   * every tick in ai_euro_refresh_colony_ai_flags, which would have turned
   * this one-shot into "NEEDS_COLONISTS whenever pop < 10"; that writer is
   * gone (smell audit 2026-09-10 C3).
   * The second writer is the formula one in ai_euro_refresh_colony_ai_flags
   * (md:557-563), which only ORs — the per-tick clear for both is the
   * `&= 7` above, so neither may carry an `else`-clear of its own.
   */
  if ((c->colony_flags & COLONIZE_COLONY_FLAG_SMALL_AI) != 0 && c->population < 0x20) {
    c->ai_flags |= COLONIZE_COLONY_AI_NEEDS_COLONISTS;
    c->colony_flags =
      (uint8_t)(c->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_SMALL_AI);
  }
  if (want > 0) {
    c->ai_flags |= COLONIZE_COLONY_AI_NEEDS_GARRISON; /* raw 94147-94149 */
  }
  if (homed_mil < wanted) {
    c->ai_flags |= COLONIZE_COLONY_AI_SHORT_DEFENDERS; /* raw 94194-94196 */
  }
  if (wanted + (wanted > 1 ? 1 : 0) < homed_mil) {
    c->ai_flags |= COLONIZE_COLONY_AI_MILITARY_SURPLUS; /* raw 94197-94199 */
  }
}

/* ===== FUN_5952_035e absorption + equip arm (raw 94231-94352) ===========
 *
 * RE-HOSTED 2026-09-18. Both arms used to run from the arriving unit's act
 * (`ai_euro_act_colony_absorb` / the on-tile block of
 * `ai_euro_act_pioneer_corridor`). DOS runs them inside the COLONY tick, at
 * this exact position — after the +0x1b flag writes and the by-profession
 * census (raw 94219-94229) and before the build-preference / construction
 * cascade — as a re-scan of the units standing on the colony tile. The
 * unit-act hosting got the ordering, the turn, the multi-unit case and the
 * RNG position all wrong; this is the DOS host.
 *
 * DOS shape, raw 94231-94272:
 *   if (DS:0x8d72 != 0 && (+0x1b & 0x10)) do {
 *     iStack_32 = 0;
 *     for (ac = +0x1f; !iStack_32 && ac < +0x1f + DS:0x8d72 && +0x1f < 0x20; ac++)
 *         { one case per @UNIT type; an absorption sets iStack_32 }
 *   } while (iStack_32);
 * i.e. absorb at most one unit per pass and restart the scan from the top —
 * every later pass sees the new population, the consumed census cell, the
 * cleared MILITARY_SURPLUS bit and the raised tools latch. `+0x1f` is
 * re-read at every loop test, so the pop < 0x20 cap tracks the absorptions.
 *
 * `local_16` (uStack_16) RESOLVED: the tick seeds it at its top from
 * `0x13 < colony[+0xb6]` ("already holds more than 19 TOOLS", +0xb6 = +0x9a
 * + 2*14) and the Pioneer case then latches it to 1 so no SECOND Pioneer is
 * absorbed through that disjunct in the same tick. It is a real tick-local:
 * DOS never re-reads the stock word, so the absorbed Pioneer's own tools
 * refund does not move the gate. Carried here as `tools_latch`, which
 * retires the "residual divergence" the unit-act hosting had to document.
 *
 * `iStack_76` is the tick's running labor/garrison demand — the value
 * `ai_euro_5952_labor_demand` leaves in `want` after the on-tile military
 * walk, NOT the `+0x8e` byte (DOS writes +0x8e once, before that walk, and
 * never again in the tick). It is passed in by address and the Soldier case
 * `++`s it. In-tick readers of the post-absorption value: the dead disjunct
 * (a) below, and the second Muskets build-preference arm
 * (`FUN_OVL15_L0000__002a82(0x181f, 0xf, ...)` = FUN_5952_0306), ported
 * 2026-09-18 as ai_euro_5952_build_pref_0306 and called from the tail of
 * this function — so the increment is now observable where DOS makes it
 * observable: one extra absorbed Soldier can force the colony's +0x8d
 * preference back onto Muskets.
 */
static int ai_euro_5952_tile_stack(
  const ColonizeTurnContext* ctx, const ColonizeColony* c, int* ids, int max
) {
  int n = 0;
  if (!ctx->units) {
    return 0;
  }
  /* DS:0x8d72 (FUN_15eb_09c0 raw 10014-10034) — the same predicate
   * ai_euro_5952_labor_demand counts `outside` with. Slot walk: `i` is an
   * array index, not a unit id. */
  for (int i = 0; i < COLONIZE_UNITS_MAX && n < max; ++i) {
    const ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->aboard_ship_id >= 0 || u->x != c->x || u->y != c->y) {
      continue;
    }
    if (!units_type_has_profession_slot(ai_euro_20e6_dos_type(ctx->units, u))) {
      continue;
    }
    ids[n++] = u->id;
  }
  return n;
}

/* Defined below, in DOS's own order: the equip arm and then the five
 * FUN_5952_0306 build-preference calls that follow it. */
static void ai_euro_5952_equip_arm(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int stance
);
static void ai_euro_5952_build_pref_0306(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  int labor_running, int tools_latch
);

COLONIZE_INTERNAL void ai_euro_5952_absorb_equip(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int* labor_running
) {
  if (!ctx || !ctx->units || !ctx->colonies || !c) {
    return;
  }
  const int cont = map_continent_id_at(ctx->map, c->x, c->y);
  const int stance = ai_euro_continent_stance_at(nation_id, cont); /* local_2a */
  /* local_16, seeded at the tick's top from +0xb6 (md:298). */
  int tools_latch = (int)(c->stock[COLONIZE_CARGO_TOOLS] > 0x13);
  const int idx = (c->id >= 0 && c->id < COLONIZE_COLONIES_MAX) ? c->id : -1;

  /* ---- absorption arm, raw 94231-94272 -------------------------------- */
  int ids[COLONIZE_UNITS_MAX];
  if ((c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) != 0 &&
      ai_euro_5952_tile_stack(ctx, c, ids, COLONIZE_UNITS_MAX) != 0) {
    int restart = 1;
    int guard = 0;
    while (restart && guard++ <= COLONIZE_UNITS_MAX) {
      restart = 0;
      const int n = ai_euro_5952_tile_stack(ctx, c, ids, COLONIZE_UNITS_MAX);
      for (int k = 0; k < n && !restart; ++k) {
        if ((int)c->population >= 0x20) {
          break; /* raw 94235-94236, re-read every loop test */
        }
        ColonizeUnit* u = units_get(ctx->units, ids[k]);
        if (!u || !u->active) {
          continue;
        }
        /*
         * local_ee = FUN_1000_8dfe(slot) = FUN_15eb_0e18; for a slot past the
         * population it is FUN_15eb_0902(unit) = DS:0x30e[@UNIT type], the
         * type's DEFAULT @JOB — 0x13 Colonist / 0x14 Pioneer / 0x15 Soldier /
         * 0x16 Scout / 0x17 Dragoon (and Cont. Army → 0x15, Cont. Cav →
         * 0x17). NOT the @UNIT code. (smell audit 2026-09-10, "DEFERRED —
         * raw 94247".)
         */
        const int dtype =
          units_type_default_job(ai_euro_20e6_dos_type(ctx->units, u)); /* local_ee */
        const int prof = u->profession; /* local_1a, FUN_281f_0c54 */
        int take = 0;
        int is_mil = 0;
        int is_pioneer = 0;
        /* The four DOS cases are disjoint on local_ee, so the raw's chain of
         * bare `if`s is spelled as an else-chain here. */
        if (dtype == 0x15 || dtype == 0x17) { /* Soldier / Dragoon, raw 94239 */
          is_mil = 1;
          const int census13 = idx >= 0 ? ai_euro_s_5952_census_nonexpert[idx] : 0;
          const int census15 = idx >= 0 ? ai_euro_s_5952_census_vet_soldier[idx] : 0;
          /* (a) DOS-LITERAL dead branch: iStack_76 is clamped >= 0 by the
           * labor formula and only ever ++/--s under `!= 0`, so it is never
           * negative here. Now that the running total is real, the branch is
           * spelled against it rather than against a hard 0. */
          const int dead_shortage_arm =
            (*labor_running < 0 &&
             (c->ai_flags & COLONIZE_COLONY_AI_SHORT_DEFENDERS) == 0);
          const int specialist_arm =
            ai_euro_5952_job_is_expert(prof) && prof != UNITS_JOB_SOLDIER &&
            (census13 != 0 || census15 != 0);
          const int surplus_arm =
            (c->ai_flags & COLONIZE_COLONY_AI_MILITARY_SURPLUS) != 0;
          take = dead_shortage_arm || specialist_arm || surplus_arm;
        } else if (dtype == 0x14) { /* Pioneer, raw 94257-94263 */
          is_pioneer = 1;
          take = ((c->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) != 0 &&
                  tools_latch == 0) ||
                 stance == 0;
        } else if (dtype == 0x16) { /* Scout, raw 94264-94270 */
          take = (stance == 0 || c->stock[COLONIZE_CARGO_HORSES] < 0x34);
        } else if (dtype == 0x13) { /* Free Colonist, raw 94271-94274 */
          take = 1;
        }
        if (!take) {
          continue;
        }
        /* FUN_1000_8e26(slot, 0x12) = FUN_281f_0c36 = FUN_15eb_1068 with the
         * idle-sentinel job: the refund loop (raw 11234-11248) banks the
         * unit's gear into stock and the colonist keeps its profession. */
        ColonizeWorld w = world_from_turn_ctx(ctx);
        if (colonies_admit_unit_w(&w, c->id, u->id) < 0) {
          continue;
        }
        restart = 1; /* iStack_32 */
        if (is_mil) {
          /* raw 94247-94255 */
          (*labor_running)++;
          c->ai_flags =
            (uint8_t)(c->ai_flags & (uint8_t)~COLONIZE_COLONY_AI_MILITARY_SURPLUS);
          if (idx >= 0) {
            if (ai_euro_s_5952_census_vet_soldier[idx] != 0) {
              ai_euro_s_5952_census_vet_soldier[idx]--;
            } else if (ai_euro_s_5952_census_nonexpert[idx] != 0) {
              ai_euro_s_5952_census_nonexpert[idx]--;
            }
          }
        } else if (is_pioneer) {
          tools_latch = 1; /* raw 94261 `local_16 = 1` */
        }
      }
    }
  }

  ai_euro_5952_equip_arm(ctx, nation_id, c, stance);
  /* raw 94353-94354: FUN_1000_8ebc(0x181f) = the ledger refresh, then DOS's
   * five FUN_5952_0306 build-preference calls. `local_16` (tools_latch) is
   * the fourth one's own input, which is why the block is hosted here, in
   * the same frame that owns the latch, rather than in the caller. */
  ai_euro_5952_build_pref_0306(ctx, nation_id, c, *labor_running, tools_latch);
}

/* ---- equip arm, raw 94276-94352 ---------------------------------------
 * `if (+0x1f < 2) goto LAB_5952_0f7c` skips the whole block. Both
 * population reads are DOS's own spelling and see the post-absorption
 * population, which is why this must be hosted here.
 *
 * Completed 2026-09-18: all THREE of DOS's `local_136` targets are now
 * modelled, in DOS's order — Scout (raw 94277-94285), Pioneer (raw
 * 94300-94303), Soldier/Dragoon (raw 94304-94312). They are three plain
 * `if`s over the same `local_8e`, so a later arm silently OVERRIDES an
 * earlier one and at most ONE re-type happens per tick (`local_136` is a
 * flag, not a count, and the picker below runs once).
 *
 * `stance` is the caller's `local_2a` (the owner's continent stance); the
 * early `return`s are DOS's `goto LAB_5952_0f7c`, which lands on the ledger
 * refresh + build-preference block the caller runs next.
 */
static void ai_euro_5952_equip_arm(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c, int stance
) {
  const int equip_pop = (int)c->population;
  if (equip_pop < 2) {
    return;
  }
  int local_8e = UNITS_JOB_COLONIST; /* raw 94276: local_8e = 0x13 */
  int local_136 = 0;
  /*
   * Scout target, raw 94277-94285 (md:644-654), DOS-LITERAL:
   *   if (0x65 < +0xaa) {                       // stock[horses] > 101
   *     if (+0x1f < '\n')                       // pop < 10
   *       { cVar8 = FUN_1000_8e6c(0x181f); if (+0x1f < cVar8) goto LAB_0de5; }
   *     if ((+0x1b & 0x10) == 0) { local_8e = 0x16; local_136 = 1; }
   *   }
   * `FUN_1000_8e6c` = `FUN_281f_0c7c` -> `FUN_15eb_0484` = the AI's wanted
   * colony size (ai_euro_colony_wanted_size; 8 in practice — see its note),
   * the same callee the NEEDS_COLONISTS latch uses ten lines earlier with
   * segment literal 0x1a1f. `LAB_OVL15_L0000__000de5` is the local_90
   * computation below, so the jump skips the Scout arm ONLY; it does not
   * leave the equip block.
   * No horses test beyond the 0x65 threshold lives in the arm — the 50-horse
   * charge is inside FUN_15eb_1068 (colonies_eject_colonist,
   * COLONIZE_EJECT_SCOUT), and 0x65 guarantees it is affordable.
   * The scorer is called with local_1b4 = local_8e = 0x16 unchanged (only
   * 0x17 is remapped), so ai_euro_5952_equip_pick scores against 0x16: a
   * Seasoned Scout in the colony scores 4, any non-expert +1, and the
   * `-99` expert-strip arm does NOT apply (it is gated on target == 0x15).
   */
  if (c->stock[COLONIZE_CARGO_HORSES] > 0x65) {
    int skip_scout = 0;
    if (equip_pop < 10 && equip_pop < ai_euro_colony_wanted_size(ctx->colonies, c)) {
      skip_scout = 1; /* goto LAB_OVL15_L0000__000de5 */
    }
    if (!skip_scout && (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) == 0) {
      local_8e = UNITS_JOB_SCOUT;
      local_136 = 1;
    }
  }
  /*
   * local_90, raw 94292-94299, all four conjuncts in DOS's order. The
   * FUN_281f_04d4 = dos_rng_range(0, 3) draw is the THIRD conjunct, so the
   * stance and population tests must gate it or the shared LCG stream
   * shifts. Hosting this in the colony tick draws it once per AI colony per
   * turn, which is DOS; the unit-act hosting drew it only when a Pioneer
   * happened to stand on the tile.
   */
  int local_90 = 0;
  if (stance == 0 && equip_pop > 10 && dos_rng_range(ctx->rng, 0, 3) == 0 &&
      (c->ai_flags & COLONIZE_COLONY_AI_NEEDS_COLONISTS) == 0) {
    local_90 = 1;
  }
  /*
   * Pioneer target, raw 94300-94303 (md:664-668), DOS-LITERAL:
   *   if (local_90 != 0 && *(char *)(local_1b0 * 0x13 + -0x6db2) == 0 &&
   *       local_10 != 0 && 0x13 < +0xb6) { local_8e = 0x14; local_136 = 1; }
   * Variable resolutions, all from the raw text plus already-pinned tables:
   *  - `local_1b0` is the colony's owner nation (+0x1a); the two neighbouring
   *    reads in this same tail index the same local as `*3 + -0x6a9a` (the
   *    DS:0x9566 @LEADERNAME trait triple) and `*0x10 + -0x7b37` (the
   *    DS:0x84bc Europe price row), both per-Euro-nation.
   *  - `-0x6db2` = DS:0x924e = DS:0x924c + 2 = `unit_type_counts[nation][2]`,
   *    stride 0x13 = 19 @UNIT types (save_format_map.md row 252,
   *    FUN_4962_0018). Column 2 is the nation's Pioneer count — the identical
   *    expression FUN_521d_5d04's tools-side training arm reads (raw
   *    92745-92748, ai_euro.c). So the gate is "this nation has NO Pioneer
   *    anywhere"; the colony only mints one when the nation owns none.
   *  - `local_10` = `func_0x0001a684(0x181f, +0x1a)` = `FUN_2a1f_0494`
   *    (address_mapping.csv raw 1a684) = the far thunk for `FUN_521d_03d0`,
   *    founding_expansion_urgency(nation) — ai_goals_founding_expansion_
   *    urgency. Computed once at the top of the tick (md:608) and only read
   *    here.
   *  - `0x13 < +0xb6` is stock[tools] > 19. This is the SAME threshold the
   *    absorption arm's `local_16` was seeded from, but NOT the same
   *    quantity: `local_16` is a tick-local frozen at the tick's top and
   *    latched to 1 by the Pioneer absorb case, while this arm re-reads the
   *    live stock word. So a Pioneer absorbed earlier in this very tick
   *    refunds its tools into +0xb6 and can push this gate open — DOS takes
   *    a Pioneer in and hands one back out in one tick, and the latch does
   *    not stop it (it only guards the absorb side).
   * The tools are charged by FUN_15eb_1068 (colonies_eject_colonist,
   * COLONIZE_EJECT_PIONEER), which also refuses when the stock cannot pay.
   */
  {
    const int pioneers_afield =
      (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4)
        ? (int)ctx->col1->stuff.unit_type_counts[nation_id][2]
        : 0;
    const int total_colonies = ctx->colonies ? ctx->colonies->colony_count : 0;
    if (local_90 && pioneers_afield == 0 &&
        ai_goals_founding_expansion_urgency(nation_id, total_colonies) != 0 &&
        c->stock[COLONIZE_CARGO_TOOLS] > 0x13) {
      local_8e = UNITS_JOB_PIONEER;
      local_136 = 1;
    }
  }
  /* raw 94304-94306: `((+0x1b & 0x48) != 0 || local_90 != 0) && 0x31 < +0xb8` */
  const int equip_demand =
    (c->ai_flags &
     (COLONIZE_COLONY_AI_NEEDS_GARRISON | COLONIZE_COLONY_AI_SHORT_DEFENDERS)) != 0 ||
    local_90;
  if (equip_demand && c->stock[COLONIZE_CARGO_MUSKETS] > 0x31) {
    /* raw 94307-94312: local_8e = 0x15, upgraded to 0x17 on horses > 0x33. */
    local_8e = c->stock[COLONIZE_CARGO_HORSES] > 0x33 ? UNITS_JOB_DRAGOON : UNITS_JOB_SOLDIER;
    local_136 = 1;
  }
  if (!local_136) { /* raw 94313: `if (local_136 != 0)` guards the picker */
    return;
  }
  /* raw 94314-94317: local_1b4 = local_8e, with 0x17 mapped to 0x15. */
  const int target = (local_8e == UNITS_JOB_DRAGOON) ? UNITS_JOB_SOLDIER : local_8e;
  const int pick = ai_euro_5952_equip_pick(c, target);
  if (pick < 0) {
    return;
  }
  /*
   * raw 94347-94350, DOS-LITERAL: the expert strip reads FUN_281f_0c9a with
   * iStack_18 still holding the LAST loop iteration's profession, not the
   * picked colonist's — the scorer's loop variable leaks out of the loop.
   */
  const int last_prof = (int)c->colonists[equip_pop - 1].profession;
  if (ai_euro_5952_job_is_expert(last_prof) && target != last_prof) {
    /* FUN_1000_8e9e(colony, pick, 0x1c) = FUN_281f_0cae, clear specialty. */
    c->colonists[pick].profession = COLONIZE_PROF_FREE_COLONIST;
  }
  /* FUN_1000_8e26(colony, pick, local_8e) = FUN_15eb_1068: the colonist
   * leaves as a Scout / Pioneer / Soldier / Dragoon on the colony tile
   * keeping its profession byte, orders byte (+0x314c) zeroed, gear charged
   * off the stock (raw 11318-11329). local_8e, not local_1b4 — the Dragoon
   * remap is for the SCORER only. */
  int eject_role = COLONIZE_EJECT_COLONIST;
  if (local_8e == UNITS_JOB_PIONEER) {
    eject_role = COLONIZE_EJECT_PIONEER;
  } else if (local_8e == UNITS_JOB_SOLDIER) {
    eject_role = COLONIZE_EJECT_SOLDIER;
  } else if (local_8e == UNITS_JOB_SCOUT) {
    eject_role = COLONIZE_EJECT_SCOUT;
  } else if (local_8e == UNITS_JOB_DRAGOON) {
    eject_role = COLONIZE_EJECT_DRAGOON;
  }
  (void)colonies_eject_colonist(ctx->colonies, c->id, pick, ctx->units, eject_role);
}

/* ---- build-preference block, raw 94353-94395 (md:758-793) --------------
 *
 * DOS's five `FUN_OVL15_L0000__002a82(0x181f, cargo, want)` calls, in order,
 * immediately after the equip arm's `FUN_1000_8ebc(0x181f)` ledger refresh.
 *
 * CALLEE PINNED. `thunk_FUN_2a1f_05e4` is the RTLink dynalink stub at
 * `5952:2a82` (address_mapping.csv row `thunk_FUN_2a1f_05e4,5952:2a82,
 * OVL15_L0000,2a82`), and its target is `FUN_5952_0306(cargo, want)`
 * (viceroy_unpacked.c:93760-93780) — the real `+0x8d` `specialty_cargo`
 * writer, already ported as `colonies_specialty_cargo_update`:
 *     cap = FUN_281f_0d3a()                      // warehouse capacity
 *     if (cap <= stock[cargo])        want = 0
 *     if (DS:0x8dc8[cargo] != 0)      want = 0   // colony already MAKES it
 *     if (want) +0x8d = cargo; else if (+0x8d == cargo) +0x8d = 0xff
 * Ghidra drops the two args at every call site because they arrive in
 * registers; the `(0x181f, tag, value)` spelling in the clean overlay
 * recovery IS the literal argument list (md:760-793), so no ndisasm
 * recovery was needed beyond confirming the thunk target.
 * `DS:0x8dc8` (`-0x7238`) is the tick's gross-production scratch ledger,
 * `ai_euro_5952_ledgers`' `gross[]`.
 *
 * ARM-BY-ARM, all four cargo tags fall out of the colony record's stock
 * base `+0x9a + cargo*2`: `0xf` = +0xb8 Muskets, `0xe` = +0xb6 Tools,
 * `0xd` = +0xb4 Trade Goods, `8` = +0xaa Horses.
 *
 *  1. `(0xf, (target - muskets != 0 && muskets <= target))` where
 *     `target = (byte[nation*3 + -0x6a9a] + 2) * 0x32`. `-0x6a9a` =
 *     DS:0x9566 column 0, the @LEADERNAME **belligerence** trait
 *     (ai_diplo_leader_trait column 0, shipped 1/0/1/-1), so a warlike
 *     leader wants 150 muskets, a meek one 50. The pair of tests is just
 *     `muskets < target` spelled long-hand.
 *  2. `(0xd, trade_goods < 100 && byte[nation*0x10 + -0x7b37] < 4 &&
 *     (+0x1c & 0x20))`. `-0x7b37` = DS:0x84c9 = DS:0x84bc + 0xd, the
 *     per-nation Europe SELL row for Trade Goods (`euro_price − 1`, clamped
 *     at 0 — the cascade header above cites the same table at +0xf), and
 *     `+0x1c & 0x20` is COLONIZE_COLONY_FLAG_WAGON_TRAIN. So: stock a
 *     trading post's worth of Trade Goods only while they are cheap and the
 *     colony has a wagon to move them.
 *  3. `(8, horses < 0x32)`.
 *  4. `(0xe, local_16 == 0 && (+0x1b & 0x80))` — the tick-local tools latch
 *     (seeded `0x13 < +0xb6`, raised by the Pioneer absorb case) and
 *     WANTS_PIONEER_WORK. This is the second of the two `local_16` readers
 *     and the reason the latch has to leave the absorb frame.
 *  5. `(0xf, ...)` AGAIN, overriding arm 1 — this is the `iStack_76`
 *     reader the 2026-09-18 re-hosting left unmodelled. DOS computes
 *     `uStack_96 = (+0x8d == 0x0f)` between arms 1 and 2, i.e. "arm 1 left
 *     Muskets as the standing preference", then:
 *       want = !( (labor < 1 || muskets > 0x31)
 *              && (+0x8e != 1 || muskets > 0x31 || +0x8d == 0x0e)
 *              && ((+0x1b & 8) == 0 || muskets > 0x31 || +0x8d == 0x0e)
 *              && (uStack_96 == 0 || +0x8d != 0x0f) )
 *     i.e. the preference is FORCED back onto Muskets whenever the colony
 *     is short of hands for its garrison (`iStack_76 >= 1`, the running
 *     total the absorption arm `++`s), or +0x8e says labor_shortage 1, or
 *     SHORT_DEFENDERS is set — unless it already has more than 49 muskets
 *     or has settled on Tools. That is the whole observable consequence of
 *     the absorb-time increment: one more absorbed Soldier this tick can
 *     tip a colony into buying muskets.
 *
 * The 11-cargo "surplus haul ladder" that used to stand in for this block
 * in ai_euro_colony_inventory was a port invention (its own comment said
 * "FUN_5952_0306 thin"); it is deleted, and with it the invented `boycotted`
 * clear — DOS's second clear is `gross[cargo] != 0`.
 */
static void ai_euro_5952_build_pref_0306(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  int labor_running, int tools_latch
) {
  if (!ctx->colonies) {
    return;
  }
  int gross[AI_EURO_5952_LEDGER_SLOTS];
  int demand[AI_EURO_5952_LEDGER_SLOTS];
  {
    const ColonizeWorld w = world_from_turn_ctx(ctx);
    ai_euro_5952_ledgers(
      &w, ctx->colonies, c, (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL, gross, demand
    );
  }
  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;

  /* arm 1 — Muskets, md:759-762 */
  {
    const int target = (ai_diplo_leader_trait(ctx, nation_id, 0) + 2) * 0x32;
    const int muskets = c->stock[COLONIZE_CARGO_MUSKETS];
    colonies_specialty_cargo_update(
      ctx->colonies, c, COLONIZE_CARGO_MUSKETS,
      (target - muskets != 0 && muskets <= target),
      gross[COLONIZE_CARGO_MUSKETS] != 0
    );
  }
  /* md:764-765: uStack_96 is latched HERE, between arms 1 and 2. */
  const int local_96 = (c->specialty_cargo == (uint8_t)COLONIZE_CARGO_MUSKETS);
  /* arm 2 — Trade Goods, md:766-773 */
  {
    int price = 0;
    if (col1 && nation_id >= 0 && nation_id < 4) {
      const int p = (int)col1->nation[nation_id].trade.euro_price[COLONIZE_CARGO_TRADE_GOODS] - 1;
      price = p < 0 ? 0 : p;
    }
    const int want = c->stock[COLONIZE_CARGO_TRADE_GOODS] < 100 && price < 4 &&
                     (c->colony_flags & COLONIZE_COLONY_FLAG_WAGON_TRAIN) != 0;
    colonies_specialty_cargo_update(
      ctx->colonies, c, COLONIZE_CARGO_TRADE_GOODS, want,
      gross[COLONIZE_CARGO_TRADE_GOODS] != 0
    );
  }
  /* arm 3 — Horses, md:774 */
  colonies_specialty_cargo_update(
    ctx->colonies, c, COLONIZE_CARGO_HORSES, c->stock[COLONIZE_CARGO_HORSES] < 0x32,
    gross[COLONIZE_CARGO_HORSES] != 0
  );
  /* arm 4 — Tools, md:775-781 */
  colonies_specialty_cargo_update(
    ctx->colonies, c, COLONIZE_CARGO_TOOLS,
    tools_latch == 0 && (c->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) != 0,
    gross[COLONIZE_CARGO_TOOLS] != 0
  );
  /* arm 5 — Muskets again, md:782-793. DOS-LITERAL, including the three
   * repeats of `0x31 < +0xb8` and the `+0x8d == 0x0e` (Tools) escape. */
  {
    const int muskets = c->stock[COLONIZE_CARGO_MUSKETS];
    const int spec = (int)c->specialty_cargo;
    const int rich = muskets > 0x31;
    const int quiet =
      (labor_running < 1 || rich) &&
      ((int)c->labor_shortage != 1 || rich || spec == COLONIZE_CARGO_TOOLS) &&
      ((c->ai_flags & COLONIZE_COLONY_AI_SHORT_DEFENDERS) == 0 || rich ||
       spec == COLONIZE_CARGO_TOOLS) &&
      (local_96 == 0 || spec != COLONIZE_CARGO_MUSKETS);
    colonies_specialty_cargo_update(
      ctx->colonies, c, COLONIZE_CARGO_MUSKETS, !quiet,
      gross[COLONIZE_CARGO_MUSKETS] != 0
    );
  }
}

static void ai_euro_colony_threat_seed_5952(
  ColonizeTurnContext* ctx,
  int nation_id,
  ColonizeColony* c,
  int* out_labor_running
) {
  if (out_labor_running) {
    *out_labor_running = 0;
  }
  if (!ctx || !ctx->units || !ctx->map || !c) {
    return;
  }
  ColonizeCombatStrengthCtx sctx = combat_strength_ctx_from_turn(ctx);

  const ColonizeCol1Save* col1 = (ctx->col1_ok && ctx->col1) ? ctx->col1 : NULL;
  const int mw = (int)ctx->map->width;
  const int mh = (int)ctx->map->height;
  int threat = 0;
  int ring1 = 0; /* iStack_22 — raw 94097-94134 */

  for (int dy = -5; dy <= 5; ++dy) {
    for (int dx = -5; dx <= 5; ++dx) {
      const int tx = c->x + dx;
      const int ty = c->y + dy;
      if (tx < 0 || ty < 0 || tx >= mw || ty >= mh) {
        continue; /* FUN_281f_0302 map_tile_in_bounds */
      }
      const int head = units_id_at(ctx->units, tx, ty);
      if (head < 0) {
        continue;
      }
      {
        const ColonizeUnit* hu = units_get_const(ctx->units, head);
        if (!hu || (hu->nation_id & 0xf) == nation_id) {
          continue; /* DOS tests the STACK HEAD's nation only */
        }
      }
      /* Settlement on the scanned tile (colony or village): FUN_281f_06be is
       * the layer2 bit-2 owner, set for both. */
      const int on_settlement =
        (ctx->colonies && colonies_id_at(ctx->colonies, tx, ty) >= 0) ||
        (ai_euro_village_nation_at(col1, tx, ty) >= 0);
      const int dist = map_dos_dist(dx, dy); /* FUN_124c_0040 */

      /* DOS walks the whole tile stack once the head qualified — including
       * any own-nation unit stacked behind a foreign one. */
      /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index, not an id. */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        const ColonizeUnit* u = &ctx->units->units[ui];
        if (!u->active || !units_is_on_map(u) || u->x != tx || u->y != ty) {
          continue;
        }
        const int dtype = ai_euro_20e6_dos_type(ctx->units, u);
        if (dtype >= 0x0d && dtype <= 0x12) {
          continue; /* raw: `type < 0xd || 0x12 < type` — ships excluded */
        }
        int v = combat_unit_base_x8(&sctx, u->id, 1, NULL);
        const int owner = u->nation_id & 0xf;
        if (owner < 4) {
          if (v < 2) {
            v = 0;
          }
          if (ai_euro_nation_is_human(ctx, owner)) {
            v = v + (v >> 1);
          }
          /*
           * iStack_22 (raw 94120-94133): a EUROPEAN foe with a non-zero
           * scored value inside ring 1 (|dx| < 2 && |dy| < 2). Counted here,
           * i.e. before the settlement halving and the distance scaling, and
           * never for Indian owners. Feeds the labor_shortage floor below.
           */
          if (v != 0) {
            const int adx = dx < 0 ? -dx : dx;
            const int ady = dy < 0 ? -dy : dy;
            if (adx < 2 && ady < 2) {
              ring1++;
            }
          }
        } else {
          if (ai_diplo_indian_alarm(col1, owner, nation_id) < 0x19) {
            v = 0;
          }
          {
            int tension = 0;
            if (col1 && col1->tribe && u->home_tribe_id >= 0 &&
                u->home_tribe_id < (int)col1->head.tribe_count) {
              tension = col1_tribe_attitude(&col1->tribe[u->home_tribe_id], nation_id);
            }
            if (tension < 0x80) {
              v = 0;
            }
          }
        }
        if (on_settlement) {
          v >>= 1;
        }
        v = (-(dist - 8) * v) >> 3;
        threat += v;
      }
    }
  }

  int floor_v = threat;
  if (floor_v > 0x10) {
    floor_v = 0x10;
  }
  int walls = 0; /* FUN_281f_0ab0(0): Stockade→Fort→Fortress chain count */
  if (ctx->colonies) {
    /* Chain names come from the live @BUILDING rows 0-2 (theme D). */
    for (int i = 0; i < 3; ++i) {
      const int b = colonies_find_building(ctx->colonies, reports_fort_tier_name(i));
      if (b >= 0 && b < COLONIZE_BUILDING_TYPES_MAX && c->has_building[b]) {
        walls++;
      }
    }
  }
  threat = threat / (walls + 1);
  if (threat < floor_v) {
    threat = floor_v;
  }
  const int quota = threat >> 3;
  c->garrison_quota = (uint8_t)quota; /* DOS `(char)` truncation */

  int n = 0;
  int want = 0;
  int homed_mil = 0;
  /* iStack_22 also gates the build cascade's Wagon Train arm, which the port
   * runs from a different pass of this same DOS body — stash it. */
  if (c->id >= 0 && c->id < COLONIZE_COLONIES_MAX) {
    ai_euro_s_5952_ring1[c->id] = ring1;
  }

  ai_euro_5952_labor_demand(ctx, nation_id, c, col1, quota, ring1, &n, &want, &homed_mil);

  ai_euro_5952_ai_flags(ctx, nation_id, c, n, want, homed_mil);

  /*
   * raw 94219-94229 (md:566-577), DOS-LITERAL: right after the +0x1b flag
   * writes the tick wipes the by-profession census and rebuilds it over the
   * colony's own colonists, folding every non-expert (`FUN_281f_0c9a == 0`)
   * into cell 0x13. Only the two cells the absorption arm reads are kept —
   * see the s_5952_census_* note above for why they are stashed.
   */
  {
    int nonexpert = 0;
    int vet_soldier = 0;
    const int pop = (int)c->population;
    for (int i = 0; i < pop && i < COLONIZE_COLONY_POP_MAX; ++i) {
      const int prof = (int)c->colonists[i].profession;
      if (!ai_euro_5952_job_is_expert(prof)) {
        nonexpert++;
      } else if (prof == UNITS_JOB_SOLDIER) {
        vet_soldier++;
      }
    }
    ai_euro_5952_set_absorb_census(c->id, nonexpert, vet_soldier);
  }

  /*
   * `want` is DOS's `iStack_76` at this point in the tick (post on-tile
   * military walk, NOT the +0x8e byte — DOS writes that once, before the
   * walk). It is handed to the caller so the absorption arm at raw
   * 94231-94272 can `++` it as its own tick-local; see
   * ai_euro_5952_absorb_equip, which the caller runs once the port's SECOND
   * half of DOS's +0x1b writers (ai_euro_refresh_colony_ai_flags, raw
   * 94200-94210) has also run — DOS emits every flag write before the
   * absorption.
   */
  if (out_labor_running) {
    *out_labor_running = want;
  }
}

/* --- 0a60 colony goals: stage helpers ---------------------------------- */

COLONIZE_INTERNAL void ai_euro_colony_goals_unit_contact(
  ColonizeTurnContext* ctx, int nation_id
) {
  /* B: own units — CONTACT from adjacent foreign; work queue only for bindable. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &ctx->units->units[i];
    if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
      continue;
    }
    if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int nx = u->x + MAP_DIR8_DX[d];
      const int ny = u->y + MAP_DIR8_DY[d];
      const int foe = units_id_at(ctx->units, nx, ny);
      if (foe < 0) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id != nation_id) {
        /*
         * DOS raw `0a60` unit loop reaches `thunk_FUN_2a1f_0470`
         * (upsert_primary) only — there is no `0524` (upsert_work_queue)
         * call anywhere outside the colony loop. The extra work-queue row
         * this used to write (`ai_goals_upsert_work(u->id, 3, ...)`) was a
         * port invention: it stored a *unit* id in a queue whose `+0` field
         * is a colony index, and nothing ever read it back — the 4393
         * consumer skipped it on the old `flag_b != 1` filter, which is the
         * only reason the id-namespace collision never bit. Removed
         * 2026-09-06d with the flag_a/flag_b decode.
         */
        ai_goals_upsert_primary(nation_id, nx, ny, AI_GOAL_CONTACT, 3);
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_colony_labor(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c,
  AiEuroInventory* inv, int urgency
) {
  /*
   * FUN_5952_035e threat accumulator → garrison_quota (+0x1e). DOS order:
   * the quota write happens BEFORE the tick's ai_flags bit writes, so the
   * refresh below reads this turn's quota (it used to read last turn's).
   */
  int labor_running = 0; /* iStack_76, carried into the absorption arm */
  ai_euro_colony_threat_seed_5952(ctx, nation_id, c, &labor_running);
  /* FUN_5952_035e raw 94170-94190 — the tick's Indian war-declare block
   * (or_both(nation, tribe+4, 2)); lives in ai_contact.c. DOS runs it in
   * the same per-colony body, between the expansion-appetite math and the
   * +0x1b flag writes; it draws no RNG, so its position inside the tick
   * cannot shift a stream. */
  ai_contact_colony_tick_war_5952(ctx, nation_id, c->x, c->y);
  ai_euro_refresh_colony_ai_flags(ctx, nation_id, c);
  /*
   * raw 94231-94352: the tile re-scan absorption arm and the equip arm, at
   * DOS's position — after every +0x1b flag write and the by-profession
   * census, before the build-preference / construction cascade.
   */
  ai_euro_5952_absorb_equip(ctx, nation_id, c, &labor_running);
  /*
   * `|| c->labor_shortage > 0` used to be a third disjunct here. It was
   * calibrated against the retired thin latch (0 unless something set
   * it); since #38 +0x8e carries the real FUN_5952_035e number and is
   * >= 1 for essentially every colony of pop >= 3, so the disjunct made
   * this arm unconditional — it swallowed the DOS-gated ship-pressure
   * `else` below and pulled every idle unit into the nearest town.
   * DOS never uses +0x8e as a boolean "wants labor": its consumer is the
   * garrison-quota distribution loop further down, which registers its
   * own LABOR goal at prio `shortage − garrisoned + 2` and decrements the
   * counter per admission. Dropped 2026-09-09.
   */
  int labor = (c->population < 3) || ai_euro_colony_food_short(c);
  if (inv && inv->tools_short > 0 && c->stock[COLONIZE_CARGO_TOOLS] < 20) {
    labor = 1;
  }
  if (inv && inv->food_short > 0 && c->stock[COLONIZE_CARGO_FOOD] < c->population * 2) {
    labor = 1;
  }
  const int construction = ai_euro_colony_wants_construction_labor(ctx->colonies, c);
  if (construction) {
    labor = 1;
    /* Latch Col1 +0x1d bit7 when Linux sees named construction. */
    if (c->building_in_production >= 0) {
      c->build_ai_flags |= COLONIZE_BUILD_AI_WANTS_CONSTRUCTION;
    }
  }
  if (labor) {
    const int labor_prio = construction ? 6 : (4 + urgency / 4);
    /* The thin `labor_shortage = 1` demand latch that used to sit here is
     * retired 2026-09-09: +0x8e is now stamped unconditionally from the
     * real FUN_5952_035e formula (local_76 / DS:0x8d72) in
     * ai_euro_colony_threat_seed_5952, called at the top of this same
     * colony body, and the latch could only overwrite a legitimate 0. */
    ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, labor_prio);
  } else if (c->ai_flags & (COLONIZE_COLONY_AI_NEARBY_ARMED_SHIP |
                             COLONIZE_COLONY_AI_NEARBY_FRIGATE)) {
    /*
     * Real 0a60 write site (raw decomp, thunk_FUN_2a1f_0470 call #2 in
     * the colony loop): code is actually CONTACT(0), not a distinct
     * COLONY/COLONY_ALT type — Linux keeps its own COLONY/COLONY_ALT
     * codes (downstream ai_euro_unit_act already branches on them for
     * "go work/garrison this colony", a real behavior CONTACT's own
     * downstream handling — move-and-attack — doesn't have), but the
     * *gate* is real DOS: only fires when the colony's ai_flags bit0
     * (nearby armed ship) or bit1 (nearby Man-O-War) is set — prio 8 if
     * bit1, else 5. Was unconditional ("else always register a visit
     * goal"), which invented a goal DOS wouldn't have here and let it
     * out-compete FOUND under the real prio-weighted formula whenever a
     * colony had nothing better to report — see
     * euro_goal_orders_0a60_full.md, "blocks getting the structure
     * right" fix, 2026-08-18 (root cause of the unit_ai_euro_expand
     * regression from making the goal-consumption tail live).
     */
    const int mow = (c->ai_flags & COLONIZE_COLONY_AI_NEARBY_FRIGATE) != 0;
    ai_goals_upsert_primary(
      nation_id,
      c->x,
      c->y,
      mow ? AI_GOAL_COLONY_ALT : AI_GOAL_COLONY,
      mow ? 8 : 5
    );
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_colony_work(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
) {
  /*
   * garrison_quota (+0x1e) is now the real FUN_5952_035e threat>>3 seed —
   * see ai_euro_colony_threat_seed_5952 above, called at the top of this
   * colony body in DOS order. The thin latch that used to live here
   * ("idle unfortified Soldier/Dragoon on the colony tile and quota == 0
   * → 1, skipped while NEEDS_COLONISTS / LABOR so early towns admit the
   * beachhead soldier") is retired: it had no DOS basis, and its
   * deliberate labor-gate carve-out is not something DOS does — the real
   * seed is unconditional per colony tick and keys on nearby hostiles,
   * not on who happens to be standing in the town.
   */
  /*
   * NO expand-FOUND seed here — REFUTED 2026-09-08. The old "FOUND via
   * 06ae around colony" row (and the ring-2..4 rescan that made it
   * functional) was a Linux invention: 06ae's only DOS callers are the
   * 20e6 ship unload placement (decomp 89587) and the landing block
   * (~85045), and the full FUN_521d_016a call-site enumeration shows
   * DOS writes FOUND (code 1) primaries in exactly two producers, both
   * ported — the 0a60 per-village ocean-beachhead producer, gated on NO
   * own colony on that continent (decomp 88049, one per continent via
   * the 0x173c/0x173e masks), and the foreign-colony producer's
   * FOUND-or-MIL_EXPAND arm (decomp 87983). DOS AI never seeds a
   * second colony around an existing one; same-landmass growth comes
   * from the foreign-colony arm and the labor loop. (The 95081+
   * decompile block is a duplicate pass over the same 0a60 body —
   * positive vs negative DS spellings, same LAB_521d_0ef0.)
   */
  /*
   * 0a60 work-queue haul score, real formula (raw decomp ~lines
   * 528-604 of the colony loop, `thunk_FUN_2a1f_0524` =
   * `upsert_work_queue`; was the thin "16×6 matrix OPEN" idle*8+
   * specialty-bump stand-in). The "16×6 matrix" turned out to be:
   * per-cargo Σ `euro_price[cargo][nation] * clamp(f(stock,target),
   * 0,target)` over all 16 cargo slots except FOOD(0)/LUMBER(5)/
   * TRADE_GOODS(13) — confirmed real, both tables already live in
   * Linux (`col1->nation[n].trade.euro_price[]`, `col1_save.h`;
   * `c->stock[]`, same 16-slot order, cross-checked field-for-field
   * against `col1_save.h`'s Col1 colony struct at +0x9a). TOOLS(14)/
   * MUSKETS(15) only contribute (with a flat −100 discount) when
   * `cargo_produced_mask` has that bit set this tick — otherwise
   * skipped entirely, not just discounted (DOS `goto`s past them).
   * HORSES(8) below target gets a small floor-adjust
   * (`stock+(25−target)`, clamped ≥0) instead of the plain `f()`.
   * `f(stock,target)`: below target → stock as-is (HORSES exception
   * above); at/above target → stock doubled (still capped to target
   * right after). `target` is `FUN_1000_8f2a()` — a single scalar
   * whose callee is unresolved (three other unrelated call sites
   * across this project, never named); approximated as a fixed 100,
   * matching base Warehouse capacity — the only DOS-documented
   * "target stock level" constant already in this codebase.
   * The DOS pre-loop over the units stacked on the colony tile
   * (+800 idle Pioneer / +1500 exposed combat-capable land unit on
   * a `stance==0` continent) IS ported — see the block just below;
   * `FUN_1000_89d0`/`84d4` resolved to the unit-on-tile + transport
   * chain walkers (accessors.c), substituted with an x/y filter.
   * `flag_a`/`flag_b` DECODED 2026-09-06d and now carry their real
   * DOS meanings (`AiWorkSlot.loads` / `.military`, record bytes +4
   * and +5): `loads` is DOS's `iStack_40` accumulator (+1 per counted
   * idle Pioneer, +1 per exposed combat unit, plus
   * `(min(adjusted, target) + 25) / 100` per counted cargo slot) and
   * `military` is the boolean the +1500 exposed-unit arm sets. Both
   * are read back by `FUN_521d_4393` — `loads` as the "slot still has
   * work" gate and the quantity its tail decrements, `military` as
   * the permission for a non-civilian hull to take the slot. The old
   * Linux `flag_a = specialty_cargo` hint moved into the 4393 pick
   * itself (it reads `c->specialty_cargo` directly now), so nothing
   * downstream lost the Series R tie-break.
   * Cite: move_scoring_ship.md Series F2; col1_save.h `stock`/
   * `trade.euro_price`/`cargo_produced_mask`.
   */
  {
    /*
     * Registration gate: DOS's `bVar5` — LIVE since 2026-09-06g.
     * Raw `viceroy_unpacked.c` FUN_521d_0a60 colony loop (the
     * `thunk_FUN_2a1f_0524(0x281f,local_3e,(int)local_1a,local_40,
     * local_44)` call site, :87681): `bVar5` is set in exactly three
     * places —
     *   :87622  idle-Pioneer arm (`local_40++; bVar5 = true;` +800)
     *   :87633  exposed-combat  (`local_44 = 1; bVar5 = true;` +1500)
     *   :87663  `if (0x4a < local_2a) bVar5 = true;`
     * — and `if (bVar5) { 0x1734[nation]++; local_1a += colony[+0x8f]
     * * 8; clamp 0x7fff; upsert; }` (:87674-87682).
     *
     * The two earlier reverts (2026-08-18 `target=100` placeholder;
     * 2026-09-06d "the port consumes the queue in the *delivery*
     * direction") are both retired. The 06d objection was structural
     * and correct at the time: DOS's queue is a PICKUP queue (score
     * = Σ euro_price × stock, `loads` = hold-loads of goods sitting
     * at the colony), so a DOS-gated row aims a hauler at the colony
     * that HAS the goods. That is now the right thing to do, because
     * both pickup consumers exist: the 20e6 LOAD matrix
     * (`ai_euro_20e6_load_pick`) fires for ships on arrival at an own
     * colony (2026-09-06e) and for wagons (2026-09-06f), and the
     * delivery half afterwards is owned by the already-ported DOS
     * arms (ship: delivery-tally matrix + sell tail + Europe export;
     * wagon: own-colony dump sweep + village errand). The Linux
     * shortage ladder and `ai_euro_nearest_haul_short_colony` that
     * pulled the queue the other way are deleted with this pass.
     */
    int wbvar5 = 0;
    /*
     * FUN_1000_8f2a() RESOLVED (2026-08-18, static — no live session
     * needed): address_mapping.csv's canonical chain
     * FUN_1000_8f2a → FUN_281f_0d3a → FUN_15eb_0a50 is exactly the
     * already-known, already-documented warehouse-capacity formula
     * (`save_format_map.md`/`FUNCTION_CATALOG.md`: 100×(1+
     * warehouse_level)) — already live in Linux as
     * `colonies_warehouse_capacity`. DOS calls this once per colony
     * (no cargo_type arg), same as here — and since smell audit #25
     * removed the port's uncited FOOD-199 branch, the accessor is now
     * cargo-independent like DOS's, so the cargo passed here is only a
     * readability choice (FOOD is skipped by this loop anyway).
     */
    const int target =
      colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_TOOLS);
    const ColonizeCol1Nation* nat =
      (ctx->col1_ok && ctx->col1 && nation_id >= 0 && nation_id < 4)
        ? &ctx->col1->nation[nation_id]
        : NULL;
    long wscore = 0;
    /*
     * Idle-Pioneer / exposed-combat-unit bonus (raw lines ~536-563,
     * same colony loop, before the cargo-weight scan below) — DOS
     * walks units *stacked at this colony's own tile* via a
     * transport-chain stack walk (`unit_index_on_tile` + prev-link
     * follow, `FUN_1000_89d0`/`84d4`; both resolved this pass via
     * `address_mapping.csv`: canonical `FUN_281f_07e0`/`02e4`,
     * already-known `ai/accessors.c` unit-on-tile + transport-chain
     * helpers). Linux has no live per-tile unit stack to walk, so
     * iterate + filter x/y instead — same substitution this file
     * already uses elsewhere (e.g. the garrison_quota scan just
     * above). +800 (saturating in DOS; harmless to add plain here,
     * the shared clamp below still applies) per idle PIONEER (DOS
     * type 0x02 — the earlier "Missionary" reading was a type-id
     * mislabel, fixed 2026-09-07) when colony +0x1b bit 0x80
     * (WANTS_PIONEER_WORK, live since 2026-09-07) is clear.
     * +1500 per exposed combat-capable land unit (attack>1,
     * not a ship) when this continent has no G-table stance assigned
     * (`ai_euro_continent_stance_at()==0`) and the unit's AI plan
     * letter is neither 'G' nor 'A' (raw :87631 —
     * `+0x314b != 'G' && +0x314b != 'A'`).
     *
     * 2026-09-09: that last gate is now the REAL one. It used to be
     * argued away against this port's own 0a60 shadow state
     * (`s_0a60_pilot_state`, always fresh-zeroed here), but the DOS byte
     * is +0x314b = `ai_plan`, not the +0x314c act-state the shadow
     * mirrors — the same mislabel already corrected for the 4962 census
     * gate (col1_stuff_census.c:166-187, ai_diplo.c:1592-1631,
     * ai_contact.c:9061-9069, all reading `col1_ai_plan` ∈ {'A','G'}).
     * 'A'/'G' are the garrison/assigned plans: a unit already spoken for
     * is not "exposed", so it must not raise this colony's work-queue
     * score. `col1_ai_plan` is save-backed and survives across turns,
     * so unlike the shadow it is a live, non-vacuous test.
     *
     * ONE-TURN-STALE STANCE READ IS DOS-FAITHFUL (smell audit #34,
     * REFUTED 2026-09-09 — do not "fix" by hoisting the refresh):
     * in `FUN_521d_0a60` the colony loop that owns this arm is
     * viceroy_unpacked.c:87595-87991 (the read at :87627), while the
     * only writer of the −0x6790 G-table is the continent loop at
     * :88054-88151 — later in the *same* call, and no other DOS
     * function writes −0x6790. So DOS's arm also sees the table left
     * by the previous turn's 0a60 call for this nation, all-zero on
     * the first call. `s_euro_continent_stance` is file-static and
     * likewise persists across turns, and
     * `ai_euro_refresh_continent_stance` below (the port of the
     * :88054 loop) is the only writer — same semantics.
     */
    /* DOS iStack_40 / uStack_44 / bVar5 — see AiWorkSlot in ai_goals.h. */
    int wloads = 0;
    int wmilitary = 0;
    if (ctx->units) {
      const int cid = map_continent_id_at(ctx->map, c->x, c->y);
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        const ColonizeUnit* u = &ctx->units->units[ui];
        if (!u->active || u->x != c->x || u->y != c->y) {
          continue;
        }
        /*
         * Raw :87619-87622: `type == 0x02` — a PIONEER (not a Missionary;
         * type 0x03 is the Missionary — mislabel fixed 2026-09-07), and
         * only when colony +0x1b bit 0x80 is CLEAR (no pioneer work at
         * this colony → the idle Pioneer registers it for pickup).
         */
        if (ai_euro_20e6_dos_type(ctx->units, u) == 0x02 &&
            (c->ai_flags & COLONIZE_COLONY_AI_WANTS_PIONEER_WORK) == 0) {
          wscore += 800;
          ++wloads; /* DOS iStack_40++ in the same arm */
          wbvar5 = 1; /* raw :87622 */
        }
        /* `units_is_sea` takes a unit ID; this loop is a slot walk, so
         * `ui` was the wrong key (Leads 2, 2026-09-10). */
        if (ai_euro_continent_stance_at(nation_id, cid) == 0 &&
            !units_is_sea(ctx->units, u->id) && u->col1_ai_plan != 0x47u /* 'G' */ &&
            u->col1_ai_plan != 0x41u /* 'A' */) {
          const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
          if (ty && ty->attack > 1) {
            wscore += 1500;
            ++wloads;
            wmilitary = 1; /* DOS uStack_44 = 1 — the real flag_b */
            wbvar5 = 1;    /* raw :87633-87634 */
          }
        }
      }
    }
    for (int slot = 0; slot < COLONIZE_CARGO_COUNT; ++slot) {
      int have = c->stock[slot];
      if (have < target) {
        if (slot == COLONIZE_CARGO_HORSES) {
          have += 25 - target;
          if (have < 0) {
            have = 0;
          }
        }
      } else {
        have <<= 1;
      }
      /* DOS iStack_32, computed from the target-clamped value BEFORE the
       * TOOLS/MUSKETS −100 discount, and only banked below. */
      const int loads_here = ((have > target ? target : have) + 25) / 100;
      if (slot == COLONIZE_CARGO_FOOD || slot == COLONIZE_CARGO_LUMBER ||
          slot == COLONIZE_CARGO_TRADE_GOODS) {
        continue;
      }
      if (slot == COLONIZE_CARGO_TOOLS || slot == COLONIZE_CARGO_MUSKETS) {
        if (!(c->cargo_produced_mask & (1u << slot))) {
          continue; /* not produced this tick — DOS skips entirely */
        }
        have -= 100;
      }
      /*
       * DOS raw :87663 `if (0x4a < local_2a) bVar5 = true;` — the
       * registration gate, LIVE. Note the exact position: it reads the
       * POST-adjustment, POST-`−100`-discount value (the same `local_2a`
       * the score below multiplies by `euro_price`), it is inside the
       * FOOD/LUMBER/TRADE_GOODS skip (those three cargoes can never arm
       * it), and for TOOLS/MUSKETS it therefore needs the adjusted value
       * to clear 0x4a + 100. Because `local_2a` is doubled at/above
       * `target`, a colony at or over warehouse capacity in any counted
       * cargo arms the gate outright.
       */
      if (have > 0x4a) {
        wbvar5 = 1;
      }
      if (have >= 0) {
        const int price = nat ? (int)nat->trade.euro_price[slot] : 0;
        wscore += (long)price * have;
        wloads += loads_here;
      }
    }
    if (wbvar5) {
      /*
       * Raw :87677 `local_1a += *(char *)(colony + 0x8f) * 8;` — the
       * `cargo_idle_turns` bonus is DOS's, unconditional inside the
       * `bVar5` branch (it was previously carried only on the Linux
       * shortage arm), applied BEFORE the 0x7fff clamp. `+0x8f` is a
       * signed char in DOS, and `cargo_idle_turns` is the port's own
       * name for it.
       */
      wscore += (long)(int8_t)c->cargo_idle_turns * 8;
      if (wscore > 0x7fff) {
        wscore = 0x7fff;
      }
      /*
       * No `loads` floor any more: DOS's `local_40` is exactly what it
       * writes, and a `bVar5`-gated colony always has something to
       * collect (the 0x4a arm banks at least one `loads_here`, and the
       * idle-Pioneer / exposed-unit arms each add their own +1), so the
       * `loads == 0` "no work" state `4393` skips is unreachable here —
       * which is why the 2026-08-18 "colony never registers" regression
       * signature cannot recur. The old floor-of-1 existed only to give
       * the Linux shortage arm a fake load; that arm is gone.
       */
      if (wloads > 255) {
        wloads = 255;
      }
      if (getenv("AI_0A60_WORK_TRACE")) {
        fprintf(
          stderr, "[work] n%d colony %d (%d,%d) score %ld loads %d mil %d\n",
          nation_id, c->id, c->x, c->y, wscore, wloads, wmilitary
        );
      }
      ai_goals_upsert_work(
        c->id, (int)wscore, (uint8_t)wloads, (uint8_t)wmilitary
      );
      /* `*(int *)(nation*2 + 0x1734)` bump (:87675) — the urgency term
       * the 20e6 colony-sail / unload matrices and the berth boarding
       * scan read; only that scan ever zeroes it (:81295). */
      if (ai_euro_s_0a60_work_registered[nation_id] < 0x7fff) {
        ai_euro_s_0a60_work_registered[nation_id]++;
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_colony_garrison(
  ColonizeTurnContext* ctx, int nation_id, ColonizeColony* c
) {
  /*
   * Garrison-quota distribution (raw lines 904-980; was the last
   * unported own-colony piece). DOS: while colony+0x8e
   * (labor_shortage, "units wanted") > 0, register a LABOR goal at the
   * colony (prio = shortage − already-garrisoned + 2) and then admit
   * ('A') military units standing on the colony tile in strict
   * preference order — Artillery(0x0b), non-veteran Soldier(0x01),
   * veteran Soldier (profession 0x15), non-veteran Dragoon(0x04),
   * veteran Dragoon — decrementing +0x8e and +0x1e (garrison_quota)
   * per admission. Gated, like DOS's whole own-colony block, on the
   * colony being coastal (+0x1c bit 0x40; live map_tile_is_coastal
   * here rather than the thin-latched colony_flags bit).
   * The "already garrisoned" count is DOS `FUN_1000_8aac(unit,10)` =
   * 0d38 case 0xa: # non-ship units with @UNIT combat (0x5236) > 1 in
   * the colony-tile stack — REWIRED 2026-09-06b to that real count
   * (ai_euro_0a60_stack_counts .armed; was "own fortified units on
   * the tile"). Admitted units get
   * shadow order 'A', which excludes them from this turn's goal scan
   * (DOS-identical effect); deeper 'A' labor handling stays with the
   * existing colony-join paths.
   */
  if (c->labor_shortage > 0 && ctx->units &&
      map_tile_is_coastal(ctx->map, c->x, c->y)) {
    Ai0a60StackCounts gsc;
    ai_euro_0a60_stack_counts(ctx->units, c->x, c->y, &gsc);
    const int garrisoned = gsc.armed;
    if (garrisoned < (int)c->labor_shortage) {
      ai_goals_upsert_primary(
        nation_id, c->x, c->y, AI_GOAL_LABOR,
        (int)c->labor_shortage - garrisoned + 2
      );
    }
    /* Five admission passes in DOS preference order. */
    static const int k_adm_type[5] = {0x0b, 0x01, 0x01, 0x04, 0x04};
    static const int k_adm_vet[5] = {-1, 0, 1, 0, 1}; /* -1 any; 0/1 vs prof 0x15 */
    for (int pass = 0; pass < 5 && c->labor_shortage > 0; ++pass) {
      /* Slot walk (Leads 2, 2026-09-10): `ui` is an array index. */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX && c->labor_shortage > 0; ++ui) {
        ColonizeUnit* gu = &ctx->units->units[ui];
        if (!gu->active || gu->nation_id != nation_id || gu->x != c->x ||
            gu->y != c->y || gu->id < 0 || gu->id >= COLONIZE_UNITS_MAX) {
          continue;
        }
        if (ai_euro_20e6_dos_type(ctx->units, gu) != k_adm_type[pass]) {
          continue;
        }
        const int is_vet = (gu->profession == UNITS_JOB_SOLDIER);
        if (k_adm_vet[pass] >= 0 && is_vet != k_adm_vet[pass]) {
          continue;
        }
        if (gu->col1_ai_plan == 'A') {
          continue; /* already admitted this turn */
        }
        gu->col1_ai_plan = 'A'; /* +0x314b */
        c->labor_shortage--;
        if (c->garrison_quota != 0) {
          c->garrison_quota--;
        }
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_foreign_colonies(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv
) {
  /* E: foreign colonies MILITARY if at war; thin bind one idle Soldier/Dragoon.
   * CONTACT scout rings (peace + own≥1): idle Scout → ring MD 2–4 around tribe
   * (fog-aware when map.seen exists). Deep mid-mil scoring — PARKED. */
  if (ctx->colonies && ctx->col1_ok && ctx->col1) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      const ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id == nation_id || c->nation_id < 0 || c->nation_id > 3) {
        continue;
      }
      if (ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_MILITARY, 5);
      }
    }
    /* Thin E deepen: one idle Soldier/Dragoon → nearest foreign MILITARY. */
    if (ai_euro_at_war_any_peer(ctx->col1, nation_id)) {
      ColonizeUnit* pick = NULL;
      int pick_gx = 0;
      int pick_gy = 0;
      int pick_d = -1;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
          continue;
        }
        if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
          continue;
        }
        if (units_orders_follow_goto(u->orders)) {
          continue; /* idle only */
        }
        if (!ai_euro_is_military_name(ai_euro_unit_kind(ctx->units, u))) {
          continue;
        }
        int gx = 0;
        int gy = 0;
        if (!ai_euro_nearest_military_goal(nation_id, u->x, u->y, &gx, &gy)) {
          continue;
        }
        const int d = abs(gx - u->x) + abs(gy - u->y);
        if (pick_d < 0 || d < pick_d) {
          pick = u;
          pick_gx = gx;
          pick_gy = gy;
          pick_d = d;
        }
      }
      if (pick) {
        ai_euro_set_goto(pick, UNITS_ORDER_AI_MOVE, pick_gx, pick_gy);
      }
    }
    /*
     * The goals-phase twin of the invented act-level scout ring aim used to
     * sit here (bugs.md #493/#495) and is gone with it: DOS has no
     * goals-phase Scout aim at all. A Scout's course is decided inside
     * FUN_521d_20e6 (explorer flag / patrol 0x56 / village 0x4c / explore
     * ring), which the act now reaches on every act (see the DOS re-entry
     * gate in ai_euro_unit_act, raw 90551).
     */
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_food_emergency(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv
) {
  /*
   * Food emergency (5cf6 food_short high): inventory food_short ≥ 4 → bind
   * nearest idle food-capable colonist/Pioneer to a hungry own colony LABOR
   * (MD≤8), even when not already adjacent. Cite: manual 2 food/colonist;
   * building_production food eat; no invented production rates.
   */
  if (inv && inv->food_short >= 4 && ctx->colonies && ctx->units) {
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      if (c->stock[COLONIZE_CARGO_FOOD] >= c->population * 2) {
        continue;
      }
      ColonizeUnit* pick = NULL;
      int pick_d = -1;
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        ColonizeUnit* u = &ctx->units->units[ui];
        if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
          continue;
        }
        if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
          continue;
        }
        if (ai_euro_land_is_fortified(u)) {
          continue;
        }
        /*
         * Don't yank a Pioneer off an in-progress tile improve job for
         * emergency food LABOR — exposed once the real DS:0x2f78 threshold
         * (2026-08-20 live capture) made these jobs usually take more than
         * one turn, so there's now a real window for this scan to hit a
         * unit mid-job.
         */
        if (u->orders == UNITS_ORDER_CLEAR_PLOW || u->orders == UNITS_ORDER_BUILD_ROAD) {
          continue;
        }
        if (!ai_euro_unit_is_food_labor(ctx->units, u)) {
          continue;
        }
        /* Skip if already on this colony tile (join happens in act). */
        const int dist = abs(u->x - c->x) + abs(u->y - c->y);
        if (dist > 8) {
          continue;
        }
        if (units_orders_follow_goto(u->orders) && u->goto_x == c->x &&
            u->goto_y == c->y) {
          pick = NULL;
          pick_d = -1;
          break; /* already LABOR-bound toward this colony */
        }
        if (pick_d < 0 || dist < pick_d) {
          pick = u;
          pick_d = dist;
        }
      }
      if (pick) {
        ai_goals_upsert_primary(nation_id, c->x, c->y, AI_GOAL_LABOR, 5);
        if (!units_orders_follow_goto(pick->orders) || pick->goto_x != c->x ||
            pick->goto_y != c->y) {
          ai_euro_set_goto(pick, UNITS_ORDER_AI_MOVE, c->x, c->y);
        }
        break; /* one emergency bind per planning pass */
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_tribe_seeds(
  ColonizeTurnContext* ctx, int nation_id
) {
  /* F: tribe-adjacent FOUND prio 2; alarmed → MILITARY. Never FOUND on village. */
  if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe) {
    for (uint16_t i = 0; i < ctx->col1->head.tribe_count; ++i) {
      const ColonizeCol1Tribe* t = &ctx->col1->tribe[i];
      int fx = 0;
      int fy = 0;
      {
        if (ai_euro_pick_founding_tile(
              ctx->map,
              ctx->colonies,
              ctx->col1_ok ? ctx->col1 : NULL,
              ctx->units,
              nation_id,
              t->x,
              t->y,
              &fx,
              &fy)) {
          ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 2);
        }
      }
      if (t->alarm[nation_id].friction > 50) {
        /* Capital villages: higher MILITARY prio (Cortes rich_capital path).
         * Cite: col1 tribe.state.capital; fandom capital / Aztec treasure. */
        const int prio = t->state.capital ? 5 : 3;
        ai_goals_upsert_primary(nation_id, t->x, t->y, AI_GOAL_MILITARY, prio);
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_producers(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv,
  int urgency
) {
  /* Foreign-colony + village producers (raw 983-1276): MILITARY approach,
   * CONTACT lurk ring, and the FOUND/MIL_EXPAND ship-staging goals at
   * open-sea tiles next to foreign colonies / villages. */
  ai_euro_0a60_settlement_goal_producers(ctx, nation_id);

  /*
   * G continent stance — mid-game pressure once established (≥2 colonies).
   * Refresh thin −0x6790 stance nibbles {0,3,4,6} from live tallies, then at war
   * MILITARY primary prio: own≥2 → 6, ≥3 → 7, ≥4 → 8; stance==3 soft-caps hunt
   * and bumps FOUND; stance==4 keeps mil ladder. Cite: euro_dispatcher.c G.
   */
  {
    ai_euro_refresh_continent_stance(ctx, nation_id);
    const int own =
      inv ? inv->colony_count : colonies_count_for_nation(ctx->colonies, nation_id);
    if (!ai_euro_ship_dos_enabled() && own >= 2 && ctx->colonies) {
      const int at_war =
        ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, nation_id);
      if (at_war) {
        /* Bump founding urgency stand-in + extra MILITARY on weakest/nearest foe. */
        if (inv) {
          inv->urgency += 2;
        }
        int ref_x = 0;
        int ref_y = 0;
        int have_ref = 0;
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          const ColonizeColony* c = &ctx->colonies->colonies[i];
          if (c->active && c->nation_id == nation_id) {
            ref_x = c->x;
            ref_y = c->y;
            have_ref = 1;
            break;
          }
        }
        const ColonizeColony* target = NULL;
        int best_key = -1;
        for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
          const ColonizeColony* c = &ctx->colonies->colonies[i];
          if (!c->active || c->nation_id == nation_id || c->nation_id < 0 ||
              c->nation_id > 3) {
            continue;
          }
          if (!ai_diplo_at_war(ctx->col1, nation_id, c->nation_id)) {
            continue;
          }
          const int dist =
            have_ref ? (abs(c->x - ref_x) + abs(c->y - ref_y)) : 0;
          /* Prefer weaker (low pop), then nearer — pack into one key. */
          const int key = c->population * 10000 + dist;
          if (!target || key < best_key) {
            target = c;
            best_key = key;
          }
        }
        if (target) {
          /* Higher than E's foreign MILITARY (5); ladder own≥2/3/4 → 6/7/8. */
          int mil_prio = 6;
          if (own >= 4) {
            mil_prio = 8;
          } else if (own >= 3) {
            mil_prio = 7;
          }
          int under_cont = 0;
          if (ctx->map) {
            const int cid = map_continent_id_at(ctx->map, target->x, target->y);
            const int stance = ai_euro_continent_stance_at(nation_id, cid);
            /* stance 3 expand / bal under-target: soft-cap hunt, bump FOUND. */
            if (stance == 3) {
              under_cont = 1;
              if (mil_prio > 6) {
                mil_prio--;
              }
            }
          }
          ai_goals_upsert_primary(
            nation_id, target->x, target->y, AI_GOAL_MILITARY, mil_prio
          );
          if (under_cont) {
            int fx = 0;
            int fy = 0;
            if (ai_euro_pick_founding_tile(
                  ctx->map,
                  ctx->colonies,
                  ctx->col1,
                  ctx->units,
                  nation_id,
                  target->x,
                  target->y,
                  &fx,
                  &fy)) {
              ai_goals_upsert_secondary(nation_id, fx, fy, AI_GOAL_FOUND, 3);
            }
          }
        }
      } else {
        /* Peaceful: bump one primary FOUND +1, else idle Scout/Soldier → explore. */
        int bumped = 0;
        for (int i = 0; i < AI_PRIMARY_SLOTS; ++i) {
          const AiGoalSlot* s = ai_goals_primary(nation_id, i);
          if (!s || s->code != AI_GOAL_FOUND) {
            continue;
          }
          ai_goals_upsert_primary(
            nation_id, s->x, s->y, AI_GOAL_FOUND, (int)s->prio + 1
          );
          bumped = 1;
          break;
        }
        if (!bumped) {
          int tx = 0;
          int ty = 0;
          int have_t = 0;
          /* Prefer tribe-adjacent FOUND (never the village tile itself). */
          if (ctx->col1_ok && ctx->col1 && ctx->col1->tribe &&
              ctx->col1->head.tribe_count > 0) {
            const ColonizeCol1Tribe* t0 = &ctx->col1->tribe[0];
            if (ai_euro_pick_founding_tile(
                  ctx->map,
                  ctx->colonies,
                  ctx->col1_ok ? ctx->col1 : NULL,
                  ctx->units,
                  nation_id,
                  (int)t0->x,
                  (int)t0->y,
                  &tx,
                  &ty)) {
              have_t = 1;
            }
          } else if (ai_goals_best_found_tile(nation_id, &tx, &ty)) {
            have_t = 1;
          }
          if (have_t) {
            for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
              ColonizeUnit* u = &ctx->units->units[i];
              if (!u->active || u->nation_id != nation_id || u->aboard_ship_id >= 0) {
                continue;
              }
              if (!units_is_on_map(u) || ai_euro_is_ship_type(ctx->units, u->id)) {
                continue;
              }
              if (units_orders_follow_goto(u->orders)) {
                continue;
              }
              const ColonizeUnitKind kind = ai_euro_unit_kind(ctx->units, u);
              if (kind != UNITS_KIND_SCOUT && !ai_euro_is_military_name(kind)) {
                continue;
              }
              ai_euro_set_goto(u, UNITS_ORDER_AI_MOVE, tx, ty);
              break;
            }
          }
        }
      }
    }
  }
}

COLONIZE_INTERNAL void ai_euro_colony_goals_ship_found(
  ColonizeTurnContext* ctx, int nation_id, AiEuroInventory* inv,
  int urgency
) {
  /* Ship FOUND: first colony via 06ae/0a60 landfall seed (adj 06ae from coastal
   * ship still prefers inland high 2f77). Second-wave while < 6 uses live 06ae
   * + coastal prefer. */
  {
    const int colonies = inv ? inv->colony_count : 0;
    if (colonies < 6) {
      const int found_prio = (colonies == 0) ? (6 + urgency / 2) : 4;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &ctx->units->units[i];
        if (!u->active || u->nation_id != nation_id) {
          continue;
        }
        if (!ai_euro_is_ship_type(ctx->units, u->id) || ai_euro_in_europe(u->x, u->y)) {
          continue;
        }
        int fx = 0;
        int fy = 0;
        int have = 0;
        if (colonies == 0) {
          int lx = 0;
          int ly = 0;
          if (ai_euro_recover_landfall_from_ship(u->x, u->y, &lx, &ly) &&
              ai_euro_06ae_first_colony_from_landfall(ctx->map, ctx->colonies, ctx->units, nation_id, lx, ly, &fx, &fy)) {
            have = 1;
          }
        } else if (ai_euro_pick_founding_tile(
                     ctx->map,
                     ctx->colonies,
                     ctx->col1_ok ? ctx->col1 : NULL,
                     ctx->units,
                     nation_id,
                     u->x,
                     u->y,
                     &fx,
                     &fy)) {
          have = 1;
        }
        if (have) {
          ai_goals_upsert_primary(nation_id, fx, fy, AI_GOAL_FOUND, found_prio);
        }
      }
    }
  }
}

/*
 * bugs.md #707 — RETIRED 2026-09-23. The "H: light bind" arm that stood here
 * scanned every idle on-map land unit of kind Pioneer or Colonist and stamped
 * an AI_MOVE goto at ai_goals_best_found_tile_near. It had no FUN/raw cite and
 * no DOS counterpart:
 *   - DOS binds a unit to a goal only through FUN_521d_0a60's tail
 *     (+0x314c = 0x0b, +0x314d/e = the goal tile), never from a colony-goals
 *     pass and never as a bare goto.
 *   - FOUND eligibility there requires unit+0x3148 bit 2, which raw
 *     87511-87514 (via 8aac/0d38 case 3, `cmp type,2`) grants only when the
 *     unit's own stack holds a Pioneer or a military unit — so a lone
 *     Colonist can never take a FOUND goal in DOS at all.
 * DOS's type-0 arms are the FUN_521d_20e6 labor arm, the explore ring and the
 * become-a-Pioneer fall-through (raw 89350-89357), all of which are ported.
 * The arm, its declaration and its call site are gone.
 */

/* --- 0a60 colony goals ------------------------------------------------- */

void ai_euro_colony_goals(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->map || !ctx->units) {
    return;
  }
  /* FUN_15eb_28c8's structural port (T1.17) is reference-only, not called
   * from the live colonist-job path below (see its own header comment for
   * scope). W1.7 (2026-08-24) added a golden fixture verifying the 9-job
   * formula — see tests/unit/test_ai_euro_28c8_job_score.c — but wiring it
   * live is Tier 3 (docs/port_plan.md W3.1), a user-confirmed behavior
   * change, not attempted here. External linkage (declared in ai_euro.h)
   * so the fixture can call it directly; still address-taken by nothing
   * else in this file, same convention as
   * ai_euro_5d04_nation_planning_structural. */
  AiEuroInventory* inv = ai_goals_inventory(nation_id);
  ai_goals_clear_work_queue();
  /* Per-tick scratch, like the queue itself: DOS `clear_work_queue` sits at
   * exactly this point and every hauler claim below it is a fresh one. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ai_euro_s_4393_claim_valid[i] = 0;
  }

  /* A: urgency seed; FUN_1d1d_0dae(0x9faa,0,0x10e) coarse-plane wipe + restamp. */
  ai_coarse_fog_euro_restamp(ctx->units, ctx->colonies, nation_id);
  /* Raw lines 1-189: per-unit 0x3148 housekeeping + foreign-ship CONTACT
   * producer, DOS position (after the memsets, before the colony loop). */
  ai_euro_0a60_unit_housekeeping(ctx, nation_id);
  const int urgency = inv ? inv->urgency : 0;

  ai_euro_colony_goals_unit_contact(ctx, nation_id);

  /* D: own colonies — LABOR from tools/food shortage / underpop (5cf6 tallies)
   * or Stockade/Warehouse under construction. NOT from Col1 labor_shortage
   * (+0x8e): that disjunct was dropped 2026-09-09 and must not come back —
   * the long comment at the arm itself explains why (+0x8e is >= 1 for
   * essentially every colony of pop >= 3, so it made the arm unconditional).
   * (A "threatened Stockade deepen" LABOR-priority term stood here; deleted
   * 2026-09-18 with ai_euro_colony_threatened_by_war — the AI's building
   * choice is the FUN_5952_035e cascade alone, never war proximity.) */
  ai_euro_ship_pressure_reset(nation_id); /* FUN_4962_0018 raw 78239-78242 */
  if (ctx->colonies) {
    for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
      ColonizeColony* c = &ctx->colonies->colonies[i];
      if (!c->active || c->nation_id != nation_id) {
        continue;
      }
      ai_euro_colony_goals_colony_labor(ctx, nation_id, c, inv, urgency);
      ai_euro_colony_goals_colony_work(ctx, nation_id, c);
      ai_euro_colony_goals_colony_garrison(ctx, nation_id, c);
    }
  }

  ai_euro_colony_goals_foreign_colonies(ctx, nation_id, inv);

  ai_euro_colony_goals_food_emergency(ctx, nation_id, inv);

  const int dos_ship = ai_euro_ship_dos_enabled();
  if (!dos_ship) {
    ai_euro_colony_goals_tribe_seeds(ctx, nation_id);
  }

  ai_euro_colony_goals_producers(ctx, nation_id, inv, urgency);

  if (!dos_ship) {
    ai_euro_colony_goals_ship_found(ctx, nation_id, inv, urgency);
  }
}

/* --- 20e6 scoring (land Manhattan + ocean/ship branch) ----------------- */

int ai_euro_tile_under_enemy_fort_fire(
  ColonizeTurnContext* ctx,
  const ColonizeUnit* viewer,
  int x,
  int y
);
int ai_euro_foe_toughness(
  ColonizeTurnContext* ctx,
  const ColonizeUnitPool* units,
  const ColonizeUnit* f,
  int is_naval
);

static int ai_euro_ocean_score_step(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int goal_x,
  int goal_y,
  int* out_dx,
  int* out_dy
) {
  /*
   * Naval/ocean branch of FUN_521d_20e6 / LAB_521d_3558 (thin extract):
   * prefer water that reduces Manhattan + Chebyshev distance to goal; avoid land;
   * HS west/east bias; leave eastern HS into ocean when westbound (Atlantic
   * first leg). Full cargo/colony matrix in 3558 still OPEN.
   * Cite: move_scoring.md §ocean; euro_ocean_scoring.c; FUN_157e_004a.
   */
  const int on_hs = map_tile_is_high_seas(ctx->map, u->x, u->y);
  const int west_explore = goal_x < u->x;
  const int east_europe = goal_x > u->x;
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, u->nation_id);
  const int own_tough = ai_euro_foe_toughness(ctx, ctx->units, u, 1);
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    const int foe = units_id_at(ctx->units, nx, ny);
    if (foe >= 0) {
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (f && f->nation_id == u->nation_id) {
        /* Own ship on water: stackable, step through it. Anything else of
         * our own on a non-water tile is a land unit — skip. */
        if (!units_is_sea(ctx->units, foe)) {
          continue;
        }
      } else if (f && !units_is_sea(ctx->units, foe)) {
        continue;
      }
    } else if (!map_tile_is_water(ctx->map, nx, ny)) {
      /* Allow coastal landfall tile if it is the goal. */
      if (!(nx == goal_x && ny == goal_y)) {
        continue;
      }
    }
    const int manh = abs(goal_x - nx) + abs(goal_y - ny);
    const int cheb_dx = abs(goal_x - nx);
    const int cheb_dy = abs(goal_y - ny);
    const int cheb = cheb_dx > cheb_dy ? cheb_dx : cheb_dy;
    int score = 2000 - manh * 12 - cheb * 4;
    const int step_hs = map_tile_is_high_seas(ctx->map, nx, ny);
    if (step_hs) {
      score += 5;
    }
    if (west_explore && MAP_DIR8_DX[d] < 0) {
      score += 4; /* west bias toward New World */
    }
    if (on_hs && west_explore && step_hs && MAP_DIR8_DX[d] < 0) {
      score += 6; /* HS west-explore: prefer westward HS tiles */
    }
    /* Leave eastern HS rim into ocean when sailing west (Atlantic first leg). */
    if (on_hs && west_explore && !step_hs) {
      score += 14;
    }
    if (east_europe && MAP_DIR8_DX[d] > 0) {
      score += 4; /* east bias toward Europe / eastern HS */
    }
    if (on_hs && east_europe && step_hs && MAP_DIR8_DX[d] > 0) {
      score += 6; /* HS east-Europe: prefer eastward HS tiles */
    }
    /* Avoid enemy Fort/Fortress batteries (FUN_364b_03f6). */
    if (ai_euro_tile_under_enemy_fort_fire(ctx, u, nx, ny)) {
      score -= 800;
    }
    /* Thin combat: prefer closing on weaker adjacent foe ships. */
    if (at_war) {
      for (int ad = 0; ad < 8; ++ad) {
        const int ax = nx + MAP_DIR8_DX[ad];
        const int ay = ny + MAP_DIR8_DY[ad];
        const int fid = units_id_at(ctx->units, ax, ay);
        if (fid < 0 || !units_is_sea(ctx->units, fid)) {
          continue;
        }
        const ColonizeUnit* f = units_get_const(ctx->units, fid);
        if (!f || f->nation_id == u->nation_id || f->nation_id < 0 || f->nation_id > 3) {
          continue;
        }
        if (!ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
          continue;
        }
        const int ft = ai_euro_foe_toughness(ctx, ctx->units, f, 1);
        if (ft < own_tough) {
          score += 18;
        } else if (ft > own_tough) {
          score -= 8;
        }
      }
    }
    /* Empty-hold coastal cling (3558/457e thin): prefer coast water near goal. */
    if (u->cargo_count == 0 && map_tile_is_coast_water(ctx->map, nx, ny)) {
      score += 8;
    }
    if (ctx->rng) {
      score += dos_rng_range(ctx->rng, 0, 2);
    }
    if (score > best) {
      best = score;
      bdx = MAP_DIR8_DX[d];
      bdy = MAP_DIR8_DY[d];
    }
  }
  if (best < -999990) {
    return 0;
  }
  *out_dx = bdx;
  *out_dy = bdy;
  return 1;
}

/*
 * Native village owner on this tile (col1 tribe table), else -1. DOS
 * FUN_1000_88e0 / FUN_137f_0392; the walk itself is col1_save_village_at.
 */
int ai_euro_village_nation_at(const ColonizeCol1Save* col1, int x, int y) {
  const ColonizeCol1Tribe* t = col1_save_village_at(col1, x, y);
  return t ? (int)t->nation_id : -1;
}

/* @UNIT attack 0 (Pioneers, Colonists, Wagon Train, unarmed transports). */
static int ai_euro_unit_cannot_attack(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  const ColonizeUnitType* t = u ? units_type(pool, u->type_index) : NULL;
  return t && t->attack <= 0;
}

int ai_euro_score_move(
  ColonizeTurnContext* ctx,
  ColonizeUnit* u,
  int goal_x,
  int goal_y,
  int* out_dx,
  int* out_dy
) {
  if (!ctx || !ctx->map || !u || !out_dx || !out_dy) {
    return 0;
  }
  if (units_is_sea(ctx->units, u->id)) {
    return ai_euro_ocean_score_step(ctx, u, goal_x, goal_y, out_dx, out_dy);
  }
  const int at_war =
    ctx->col1_ok && ctx->col1 && ai_euro_at_war_any_peer(ctx->col1, u->nation_id);
  const int own_tough = at_war ? ai_euro_foe_toughness(ctx, ctx->units, u, 0) : 0;
  int best = -999999;
  int bdx = 0;
  int bdy = 0;
  int bd = 0;
  for (int d = 0; d < 8; ++d) {
    const int nx = u->x + MAP_DIR8_DX[d];
    const int ny = u->y + MAP_DIR8_DY[d];
    if (nx < 0 || ny < 0 || nx >= ctx->map->width || ny >= ctx->map->height) {
      continue;
    }
    if (!units_can_enter_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(ctx->units), .colonies=(ColonizeColonyPool*)(ctx->colonies), .map=(ColonizeWorldMap*)(ctx->map)}, u->type_index, nx, ny, u->id)) {
      /*
       * FUN_465b_0000 resolves a step onto an occupied tile against the
       * tile's BEST DEFENDER (FUN_5fef_0000), never the head of the stack.
       * Reading units_id_at here let a berthed foreign ship (which
       * units_is_sea then skipped) hide the land garrison of a defended
       * colony, so an approaching land unit scored the target tile as
       * unreachable and oscillated beside it forever — the REF stall
       * behind bugs.md #521. Fall back to the raw head for defenderless
       * tiles (lone Treasure / civilians), as units_try_move does.
       */
      int foe = units_best_defender_at(
        ctx->units, ctx->col1_ok ? ctx->col1 : NULL, nx, ny, u->id, u->id
      );
      if (foe < 0) {
        foe = units_id_at(ctx->units, nx, ny);
      }
      if (foe < 0) {
        continue;
      }
      const ColonizeUnit* f = units_get_const(ctx->units, foe);
      if (!f || f->nation_id == u->nation_id || units_is_sea(ctx->units, foe)) {
        continue;
      }
      /* Only a tile a fight could take. A settler treated a blocked tile as an
       * attack candidate and walked into it. */
      if (ai_euro_unit_cannot_attack(ctx->units, u)) {
        continue;
      }
    }
    /*
     * Entering a native village is an attack on it, resolved inside
     * units_try_move against a defender spawned from the dwelling, with no
     * unit visible on the tile beforehand. Movement scoring must never route
     * through one: settlers died on the way to their colony site, and a
     * peacetime Soldier that wandered in lost and took its whole stack with it
     * (units_sweep_stack_after_loss). Deliberate raids go through the war hunt
     * arms, which pick their target explicitly.
     */
    if (ctx->col1_ok && ctx->col1 && ai_euro_village_nation_at(ctx->col1, nx, ny) >= 4) {
      continue;
    }
    const int dist = abs(goal_x - nx) + abs(goal_y - ny);
    int score = 1000 - dist * 10;
    /*
     * Explore ring (LAB_521d_2912→2a59 thin): continent match, FoW unseen
     * nibble, skip LCR (rumour / terr class 0x1b) for non-Scouts; Scouts prefer
     * rumour tiles. Cite: move_scoring_land.md §explore ring.
     */
    {
      const int unit_cid = map_continent_id_at(ctx->map, u->x, u->y);
      const int step_cid = map_continent_id_at(ctx->map, nx, ny);
      if (unit_cid > 0 && step_cid == unit_cid) {
        score += 4;
      }
      const int unseen =
        ctx->map->seen && !map_tile_seen_by(ctx->map, nx, ny, u->nation_id) ? 1 : 0;
      if (unseen) {
        score += 6;
      }
      const int rum = map_tile_has_rumour(ctx->map, nx, ny);
      const int lcr_class = map_dos_terr_class_at(ctx->map, nx, ny) == 0x1b;
      const int is_scout = ai_euro_unit_kind(ctx->units, u) == UNITS_KIND_SCOUT;
      if (is_scout && rum) {
        score += 12;
      } else if (!is_scout && (rum || lcr_class)) {
        score -= 20; /* non-scout skip LCR / rumour */
      }
    }
    /*
     * Land combat 20e6 (structured deepen): prefer closing on weaker adjacent war
     * foes.
     *
     * Deleted 2026-09-23 (bugs.md #759): the "+16 siege approach" additive for a
     * foreign colony destination and the "+10 Artillery prefers a fortified port"
     * rider were invented (cited only as `move_scoring_land.md LAB_521d_5183 /
     * 0x46; unpark #4` — no FUN_/raw). DOS's only colony-destination term in the
     * 20e6 direction scorer is the ×3 multiplier inside the attack term
     * (raw 88898-88901, ai_euro_20e6_attack_term), and the only fortification
     * read in raw 88266-90445 is the ship-only fort-fire penalty (raw 88813-88818).
     * The +10 also inverted raw 88911, where Artillery off a settlement scores 0.
     */
    if (at_war) {
      for (int ad = 0; ad < 8; ++ad) {
        const int ax = nx + MAP_DIR8_DX[ad];
        const int ay = ny + MAP_DIR8_DY[ad];
        const int fid = units_id_at(ctx->units, ax, ay);
        if (fid < 0 || units_is_sea(ctx->units, fid)) {
          continue;
        }
        const ColonizeUnit* f = units_get_const(ctx->units, fid);
        if (!f || f->nation_id == u->nation_id) {
          continue;
        }
        if (f->nation_id >= 0 && f->nation_id <= 3 &&
            !ai_diplo_at_war(ctx->col1, u->nation_id, f->nation_id)) {
          continue;
        }
        if (f->nation_id >= 4 && f->nation_id <= 11 &&
            !ai_diplo_indian_at_war(ctx->col1, u->nation_id, f->nation_id - 4)) {
          continue;
        }
        const int ft = ai_euro_foe_toughness(ctx, ctx->units, f, 0);
        if (ft < own_tough) {
          score += 14;
        } else if (ft > own_tough) {
          score -= 6;
        }
        if (f->nation_id >= 0 && f->nation_id <= 3 && ctx->colonies &&
            colonies_id_at(ctx->colonies, f->x, f->y) >= 0) {
          score += 8; /* settlement-adjacent foe (0x46-shaped) */
        }
      }
    }
    /*
     * Facing/momentum bias (LAB_521d_54f5, unit+0x314f): same-as-last-move
     * direction preferred, exact opposite (d^4) penalized, adjacent (diff
     * 1) mildly preferred — identical shape to the already-ported Brave
     * `quiet_score_facing`.
     */
    {
      const int last_dir = ai_euro_s_euro_last_dir[u->id];
      if (d == last_dir) {
        score += 4;
      } else if (d == (last_dir ^ 4)) {
        score -= 6;
      } else {
        int diff = d - last_dir;
        if (diff < 0) {
          diff = -diff;
        }
        if (diff > 4) {
          diff = 8 - diff;
        }
        if (diff == 1) {
          score += 3;
        }
      }
    }
    if (ctx->rng) {
      score += dos_rng_range(ctx->rng, 0, 3);
    }
    if (score > best) {
      best = score;
      bdx = MAP_DIR8_DX[d];
      bdy = MAP_DIR8_DY[d];
      bd = d;
    }
  }
  if (best < -999990) {
    return 0;
  }
  ai_euro_s_euro_last_dir[u->id] = (int8_t)bd;
  *out_dx = bdx;
  *out_dy = bdy;
  return 1;
}

/*
 * ======================================================================
 * FUN_521d_20e6 — structural land port (2026-08-27)
 * ======================================================================
 * Transcribed from the clean 2215-line recovery in
 * original_sources_annotated/ai/move_scoring_20e6_full.md ("Raw recovered
 * C"). This block covers the Euro LAND path of the function, arm by arm:
 *
 *   prologue            → ai_euro_20e6_prologue      (raw lines ~1006-1090)
 *   explorer flag       → ai_euro_20e6_explorer_flag (iStack_6a, ~1095-1180)
 *   SCOUT/PATROL 0x56   → ai_euro_20e6_patrol_arm    (LAB_277a, ~1260-1275)
 *   explore ring        → ai_euro_land_explore_scan_target (LAB_2912→2a59,
 *                         ~1320-1480; replaces the 2026-08-15 thin scan)
 *   8-dir wander score  → ai_euro_20e6_wander_step   (LAB_4d2e→5183,
 *                         ~1940-2180)
 *   epilogue commit     → ai_euro_move_scoring_gate  (LAB_589e/5a78)
 *
 * 2026-09-06 deepening pass (the six thin pieces from port_plan.md's 20e6
 * row): LAB_52aa attack-odds tail (crown==2 halving + Soldier/Dragoon
 * colony mass gate, ai_euro_20e6_attack_term), 0x4c village arms
 * (ai_euro_20e6_village_arm → ai_contact AI wrappers), colonist labor loop
 * (ai_euro_20e6_labor_arm), LAB_3558 per-cargo unload mask
 * (ai_euro_20e6_unload_mask / _unload_by_mask in ai_euro_unload_settle),
 * −0x6168 rival strength (persistent 0a60 max-tracker + explore
 * fatigue → local_12), explore-plane low nibble (ai_euro_20e6_site_nibble
 * reads the real seen-plane site-score nibble).
 *
 * NOT here (own Linux mechanics already cover them, or closed as dead in
 * port_plan.md T1.2/T1.3): 0x42/0x65 found/contact writes; the LAB_3558
 * colony-sail matrix / HS spiral / work-queue haul tails beyond the unload
 * rule.
 *
 * DOS state this port models file-locally (same pattern as ai_euro_s_euro_last_dir
 * for unit+0x314f):
 *   DS:0xa13c (−0x5ec4) per-continent explorer count → ai_euro_s_20e6_explorers
 * (unit+0x3155/+0x3156, the explorer's 4-tile ring-hop wander latch, sits in
 * the ship-band tail LAB_4b2c and is not reached by the land path ported
 * here — not modelled.)
 *     (memset 0 at FUN_521d_0a60 entry every nation turn; mirrored in
 *     ai_euro_dispatcher_turn)
 *
 * Deliberate substitutions (each marked at its use site):
 *   - DS:0x9faa coarse fog plane (far-probe +8): Linux keeps that plane only
 *     for tribe placement, so the per-nation seen[] plane is used instead.
 *   - explore-plane low nibble (FUN_1000_893a & 0xf): now the real seen-plane
 *     site-score nibble on DOS-imported maps (ai_euro_20e6_site_nibble);
 *     Linux-generated maps carry no nibble, old unseen→4 stand-in kept there.
 *   - −0x6168[continent] rival-strength: live (s_euro_rival_strength,
 *     the 0a60 max-tracker recomputed per call) + ai_euro_s_20e6_explore_fatigue for
 *     unit+0x3154; radius shrink and >40 halving now fire.
 *   - FUN_1000_8aac: resolved as the FUN_1427_0d38 stack-query dispatcher;
 *     ALL its case bodies byte-decoded 2026-09-06 (table at
 *     ai_euro_20e6_stack_count) — case 2 = total stack count (the old
 *     "# military types" reading was case 4), case 0xb = stack combat sum;
 *     the 0x42/0x65 gates (case 2 < 2 = "unit is alone") stay closed per
 *     T1.2 — Linux's goal-driven found/contact impulses cover them.
 */
