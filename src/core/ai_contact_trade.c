/*
 * Indian contact — village gift exchange, reparations & the 2820/2e92 trade haggle
 *
 * Split out of ai_contact.c (2026-09-23) verbatim; shared symbols are
 * declared in ai_contact_internal.h. See ai_contact.c for the module
 * prologue and the DOS provenance notes.
 *
 * Sections:
 *   Village gift exchange (2820/2e92 helpers)
 *   Reparations pricing, ladder & presentation
 *   Village trade haggle/buy-sell mechanics (2820/2e92) & wagon trade dispatch
 */

#include "core/internal.h"
#include "core/ai_contact.h"
#include "core/ai_contact_internal.h"

#include "core/ai.h"
#include "core/ai_diplo.h"
#include "core/sound.h"
#include "core/woodcut.h"
#include "core/ai_king.h"
#include "core/assets.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/combat_strength.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/strutil.h"
#include "core/units.h"
#include "core/village_trade_intel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== Village gift exchange (2820/2e92 helpers) (ai_contact_values_name .. ai_contact_reset) ===================== */


/* NAMES.TXT @VALUES (DS:-0x6cc0 table): "low quality" / "good" / "fine" / "excellent". */
static const char* ai_contact_values_name(int idx) {
  static const char* const k_values[4] = {"", "", "", ""};
  static char live[32];
  if (idx < 0) {
    idx = 0;
  }
  if (idx > 3) {
    idx = 3;
  }
  const char* line = assets_msg_line_or(ai_contact_s_contact_names, "VALUES", idx, NULL);
  if (line) {
    snprintf(live, sizeof(live), "%s", line);
    return live;
  }
  return k_values[idx];
}

/* DOS: max_holds[type] - unit+0x3150 >= 1. */
static int ai_contact_2e92_unit_can_take(const ColonizeTurnContext* ctx, const ColonizeUnit* unit) {
  if (!ctx || !ctx->units || !unit) {
    return 0;
  }
  const ColonizeUnitType* ty = units_type(ctx->units, unit->type_index);
  if (!ty || ty->cargo <= 0) {
    return 0;
  }
  const int cap = ty->cargo > COLONIZE_UNIT_CARGO_MAX ? COLONIZE_UNIT_CARGO_MAX : ty->cargo;
  return (cap - units_holds_used(ctx->units, unit->id)) >= 1;
}

/* FUN_1000_8cdc → FUN_15eb_317c: drop hold `slot`, compact the rest. Returns qty. */
static int ai_contact_2820_remove_slot(ColonizeUnit* unit, int slot) {
  if (!unit || slot < 0 || slot >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const int qty = unit->hold_goods_amount[slot];
  for (int i = slot; i + 1 < COLONIZE_UNIT_CARGO_MAX; ++i) {
    unit->hold_goods_type[i] = unit->hold_goods_type[i + 1];
    unit->hold_goods_amount[i] = unit->hold_goods_amount[i + 1];
  }
  unit->hold_goods_type[COLONIZE_UNIT_CARGO_MAX - 1] = 0;
  unit->hold_goods_amount[COLONIZE_UNIT_CARGO_MAX - 1] = 0;
  return qty;
}

/*
 * FUN_1000_8f48 → FUN_0000_8f68 = FUN_15eb_30b8 (tools/address_mapping.csv):
 * the same loader the colony/Europe screens use — tops a matching hold up to
 * 100 and spills the rest into a free hold (bugs.md #471).
 */
static void ai_contact_2e92_give_goods(ColonizeTurnContext* ctx, ColonizeUnit* unit, int cargo, int qty) {
  (void)units_load_goods(ctx->units, unit->id, cargo, qty);
}

/*
 * The settlement a trading unit is visiting: the `nation_id` settlement
 * standing next to it (a unit never moves onto the village tile to trade).
 * NULL when none is adjacent.
 */
static ColonizeCol1Tribe* ai_contact_2e92_visited_village(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* unit
) {
  if (!ctx || !ctx->col1 || !ctx->col1->tribe || !unit) {
    return NULL;
  }
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    ColonizeCol1Tribe* t = &ctx->col1->tribe[ti];
    if ((int)t->nation_id == nation_id && map_chebyshev(t->x, t->y, unit->x, unit->y) <= 1) {
      return t;
    }
  }
  return NULL;
}

/*
 * DS:0x8d4a — the settlement record FUN_4d56_2820 trades with. It is the
 * VISITED settlement (4528 binds it from the entered tile), not the tribe's
 * first record: 2820 reads/writes its +7 sticky good, +8 last_bought and
 * +9 last_sold, and FUN_4d56_2154's cover/econ tables are built around its
 * tile. The port used col1_tribe_first_of here until 2026-09-15, so trading
 * at any other village of a tribe priced and remembered goods against the
 * first one. The first-record fallback only covers callers whose unit is
 * not beside a settlement of that tribe (the Linux meet-pulse stand-in).
 */
static ColonizeCol1Tribe* ai_contact_2e92_tribe(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* unit
) {
  ColonizeCol1Tribe* v = ai_contact_2e92_visited_village(ctx, nation_id, unit);
  if (v) {
    return v;
  }
  /* Writes t->last_sold, so the const off col1_tribe_first_of is cast away. */
  return (ColonizeCol1Tribe*)col1_tribe_first_of(ctx->col1, nation_id);
}

/* tribe+10+e*2 (int16: friction | attacks<<8) -= sub, floor 0; qty == 100 → 0. */
static void ai_contact_2820_friction_sub(ColonizeCol1Tribe* t, int e, int sub, int qty) {
  if (!t || e < 0 || e > 3) {
    return;
  }
  int w = (int)t->alarm[e].friction | ((int)t->alarm[e].attacks << 8);
  w -= sub;
  if (w < 0) {
    w = 0;
  }
  if (qty == 100) {
    w = 0;
  }
  t->alarm[e].friction = (uint8_t)(w & 0xff);
  t->alarm[e].attacks = (uint8_t)((w >> 8) & 0xff);
}

/* FUN_1000_a0c0 → FUN_1cf8_000a: stable insertion sort of ids ascending by key. */
static void ai_contact_2820_sort(const int16_t* key, int* order) {
  for (int i = 0; i < 16; ++i) {
    order[i] = i;
  }
  for (int i = 1; i < 16; ++i) {
    const int v = order[i];
    int j = i - 1;
    while (j >= 0 && key[order[j]] > key[v]) {
      order[j + 1] = order[j];
      j--;
    }
    order[j + 1] = v;
  }
}

/*
 * DOS-LITERAL: `*(byte *)(cargo + nation * 0x10 - 0x7b44)` = DS:0x84BC[nation*0x10 + cargo]
 * (FUN_5bfb_022e raw 96967/96843, FUN_4d56_2820 asm 4d56:31d8 / 4d56:3214).
 *
 * That byte is NOT a frozen table: every writer stores
 * `nation[n].trade.euro_price[cargo] - 1` clamped at 0 (viceroy_unpacked.c
 * 6316-6320 / 51962-51966 / 58996-59000 — the same writers
 * colonies_ftrade_price_byte cites), so it tracks the LIVE Europe market.
 */
static int ai_contact_2820_price_byte(const ColonizeCol1Save* col1, int nation, int cargo) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT || cargo < 0 ||
      cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  const int p = (int)col1->nation[nation].trade.euro_price[cargo] - 1;
  return p > 0 ? p : 0;
}

/*
 * FUN_5bfb_022e generous-visit ("bVar6") arm — @INDIANGIVEFOOD / @INDIANGIVESTUFF
 * (viceroy_unpacked_2.c 87455–88030). The DOS most-common peaceful visitor:
 * a Brave walks up to an already-met colony and, when the mood roll lands
 * generous, gifts goods (or food when the colony is starving).
 *
 * DOS gates, all ported literally:
 *  - nation alarm > 0x4a (74) → no peaceful visit at all;
 *  - village alarm word ≥ 0x80 or contact_state == 1 (hostile latch) → not
 *    the gift arm;
 *  - mood roll: rng(1, 0x148) must be ≥ max(0, alarm−0x19)*4 + village word;
 *  - alarm > 0x31 (49) → generous flips off, contact_state ← 2, no gift.
 * At gift-arm entry DOS also stamps contact_state ← 2 and ZEROES the village
 * alarm word toward the visited nation (`*(0x8d4a + e*2 + 10) = 0`, right
 * after the FUN_281f_09e6 colony bind) — a gift-bearing visit discharges that
 * village's friction/attacks pair.
 * Mission-owned-by-e villages roll for @INDIANSCONVERT first (`tech + 2`,
 * doubled for a Jesuit mission, vs rng(0,0xf) — a hit sends a convert instead
 * and resolves the visit, in the convert arm below); a missed roll falls
 * through to the gift arms exactly as DOS does at LAB_5bfb_096c.
 *
 * Branch pick (LAB_5bfb_096c): 2154 bid[0] > ask[0] (village food surplus)
 * AND colony food ≤ 0x19 (25) → GIVEFOOD, topping the colony to 0x4b (75).
 * Else GIVESTUFF: bid[] keys with bid[0]=0 and near-full goods forced to 1
 * (capacity−10 < stock), insertion-sorted ascending; pick from the top at
 * rank rng(1,3), skipping food and skipping silver (7) for tech < 2;
 * qty = min(100/(want+1), key+5) clamped [5,100], capped to warehouse room,
 * floor 2. Cite: FUN_281f_0d3a = warehouse capacity; FUN_291f_0ed0 sort.
 *
 * TRIGGER (bugs.md #824, 2026-09-23 — no longer reconstructed).
 * DOS reaches 022e from FUN_465b's move tail (FUN_281f_0984 →
 * FUN_5bfb_3180), once per Brave step, for the neighbour tile the encounter
 * scan found. The port now calls this arm from that same site:
 * ai_native_step_first_contact (ai_brave.c) runs the 022e mood roll
 * (ai_contact_visit_step_roll) and then this apply, for the Brave that just
 * stepped. The two reconstruction gates this function used to carry are
 * gone with the post-pulse call site that needed them — the
 * "decline entirely without a popup queue" bail is gone with the post-pulse
 * call site that needed it; ai_contact_brave_walked_up_to survives as the
 * per-Brave SELECTOR (it now picks the Brave that is mid-step, not a
 * reconstruction of one). There is no turn cooldown either; see the visit
 * pacing note at the top of this file.
 *
 * Returns 1 when a gift was actually handed over. The caller uses that to
 * skip the demand/beg arm: `bVar6` true → LAB_5bfb_096c gifts, false →
 * LAB_5bfb_0def demands.
 *
 * NOT exclusive in one direction, though (bugs.md #863): the @INDIANBEGFOOD
 * block at raw 87650-87698 runs BEFORE that fork, and a beg the colony
 * CONCEDED (`local_c == 2`) sets `bVar6 = true` as well as `bVar7`, so it
 * falls through into this gift half in the same visit. The port re-enters
 * here from ai_contact_apply_beg_food's accept arm, which publishes bVar7
 * through ai_contact_s_visit_mood; bVar7 then vetoes the @INDIANGIVEFOOD
 * branch so the village cannot hand back the food it just took.
 */
int ai_contact_try_village_gifts(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->colonies || !ctx->col1->tribe || !ctx->rng ||
      !ctx->units) {
    return 0;
  }
  if (nation_id < 4 || nation_id > 11) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  for (int e = 0; e < 4; ++e) {
    /* DOS `bVar7` (raw 87470 init / 87687 set): this visit's food-beg was
     * conceded, which vetoes gifting food back at LAB_5bfb_096c. bugs.md #863. */
    int bvar7 = 0;
    /* One encounter, one mood verdict — drop last turn's. */
    {
      const AiContactVisitMood* pm = &ai_contact_s_visit_mood[nation_id - 4][e];
      const int pturn = ctx->turn_number ? (int)*ctx->turn_number : -1;
      if (!(pm->valid && pm->turn == pturn)) {
        ai_contact_visit_mood_clear(nation_id, e);
      }
    }
    if (!ind->euro_diplo[e]) {
      continue; /* unmet — first contact runs its own arm */
    }
    const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
    if (alarm > 0x4a) {
      continue; /* DOS: no peaceful visit interaction at all */
    }
    /* A Brave of this nation that walked up to a colony of e this turn. */
    int best_ci = -1;
    const ColonizeUnit* brave = NULL;
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX && best_ci < 0; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != e) {
        continue;
      }
      /* Slot walk — `ui` is not a unit id; see ai_contact_land_combat_sum. */
      for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
        const ColonizeUnit* bu = &ctx->units->units[ui];
        if (!bu->active || bu->nation_id != nation_id || !units_is_on_map(bu)) {
          continue;
        }
        if (abs(bu->x - c->x) > 1 || abs(bu->y - c->y) > 1) {
          continue;
        }
        /*
         * bugs.md #824: still the move-tail filter, but no longer a
         * reconstruction — this arm runs from the stepping Brave's own
         * 022e now, so "walked up this turn" simply SELECTS that Brave out
         * of the nation's units and leaves a loitering one alone, which is
         * what FUN_5bfb_3180's per-step neighbour scan does.
         */
        if (!ai_contact_brave_walked_up_to(bu, c->x, c->y)) {
          continue;
        }
        best_ci = ci;
        brave = bu;
        break;
      }
    }
    if (best_ci < 0 || !brave) {
      continue;
    }
    ColonizeColony* c = &ctx->colonies->colonies[best_ci];
    /* The visiting Brave's own village record (DOS FUN_281f_0a4c bind). */
    ColonizeCol1Tribe* t = NULL;
    if (brave->home_tribe_id >= 0 && brave->home_tribe_id < (int)ctx->col1->head.tribe_count &&
        (int)ctx->col1->tribe[brave->home_tribe_id].nation_id == nation_id) {
      t = &ctx->col1->tribe[brave->home_tribe_id];
    } else {
      for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
        if ((int)ctx->col1->tribe[ti].nation_id == nation_id) {
          t = &ctx->col1->tribe[ti];
          break;
        }
      }
    }
    if (!t) {
      continue;
    }
    {
      /* The Brave's own step already rolled this encounter (022e via 3180 in
       * the 465b tail, ai_contact_visit_step_roll): reuse that verdict. */
      const AiContactVisitMood* m = &ai_contact_s_visit_mood[nation_id - 4][e];
      const int turn = ctx->turn_number ? (int)*ctx->turn_number : -1;
      /* bugs.md #863: a conceded beg re-enters this arm after the fact
       * (ai_contact_apply_beg_food), where the record carries bVar7 and may
       * predate this Brave selection -- accept it on bvar7 alone. */
      if (m->valid && m->turn == turn && (m->brave_id == brave->id || m->bvar7)) {
        bvar7 = m->bvar7;
        if (!m->bvar6) {
          continue;
        }
        goto gifts_generous;
      }
    }
    const int word = col1_tribe_attitude(t, e);
    if (word >= 0x80 || ind->contact_state[e] == 1) {
      continue; /* hostile latch (local_10) — demand arm, not gifts */
    }
    {
      const int roll = dos_rng_range(ctx->rng, 1, 0x148);
      int over = alarm - 0x19;
      if (over < 0) {
        over = 0;
      }
      if (over * 4 + word > roll) {
        /* mood roll failed (bVar5) — not generous this visit; DOS then falls
         * into LAB_5bfb_0def with bVar6 == false. Publish the verdict so the
         * demand arm does not re-roll the same encounter (see
         * ai_contact_visit_mood_publish). */
        ai_contact_visit_mood_publish(ctx, nation_id, e, brave ? brave->id : -1, 0);
        continue;
      }
    }
    ai_contact_visit_mood_publish(ctx, nation_id, e, brave ? brave->id : -1, 1);
  gifts_generous:
    if (alarm > 0x31) {
      ind->contact_state[e] = 2; /* DOS: bVar6 flips off, state stamped */
      ai_contact_mark_visit_brave(ctx, nation_id, brave->id);
      continue;
    }
    /*
     * DOS stamps state 2 and discharges this village's alarm word toward the
     * visited nation before the convert/gift split (both halves get it).
     */
    ind->contact_state[e] = 2;
    ai_contact_mark_visit_brave(ctx, nation_id, brave->id);
    col1_tribe_attitude_set(t, e, 0);
    /*
     * @INDIANSCONVERT (viceroy 96996-97010): mission owned by e → `tech + 2`,
     * ×2 for a Jesuit mission, vs rng(0,0xf); a hit sends an Indian Convert
     * INSTEAD of a gift and resolves the visit (DOS falls straight through to
     * LAB_5bfb_1000, `local_1a = 1`), a miss falls into the gift arms at
     * LAB_5bfb_096c below.
     *
     * smell #75: this used to roll and then `continue` without sending, while
     * a second, invented standing-adjacency pulse
     * (ai_contact_mission_convert_visit) rolled again and did the sending —
     * two draws per Indian nation-turn where DOS draws once, and a convert
     * factory out of a Brave parked next to a colony. The send now lives at
     * the DOS site, on the DOS draw.
     */
    if (t->mission != COL1_TRIBE_MISSION_NONE && (int)(t->mission & 0x0f) == e) {
      int need = (int)ind->tech + 2;
      if ((t->mission & COL1_TRIBE_MISSION_JESUIT_BIT) != 0) {
        need *= 2;
      }
      if (need > dos_rng_range(ctx->rng, 0, 0xf)) {
        /* DOS-LITERAL FUN_5bfb raw 96996-97012: shows the popup first
         * (FUN_281f_0416 STRING0 = colony name, then @INDIANSCONVERT / tag
         * 0x182a), then spawns unit type 0 at raw 97009-97013:
         * `puVar4 = *(0x8542)` is the COLONY record, and
         * `FUN_281f_095c(0, puVar4[0x1a], puVar4[0], puVar4[1])` = type 0,
         * nation = colony+0x1a (the colony's owner byte), x/y = the colony
         * tile — not the visiting brave's tile (bugs.md #840). Then stamps
         * profession 0x1b. The brave is not consumed. */
        ai_contact_bind_names(ctx);
        const int human_convert = ai_contact_euro_is_human(ctx, e);
        if (human_convert) {
          units_combat_watch_notify(ctx->units, brave->id, c->x, c->y);
          PopupMsgTokens tok;
          memset(&tok, 0, sizeof(tok));
          tok.string0 = c->name[0] ? c->name : "";
          char body[AI_POPUP_BODY_LEN];
          popup_msg_fill(ctx->messages, "INDIANSCONVERT", &tok, "", body, sizeof(body));
          ai_contact_human_chrome(
            ctx, e, AI_POPUP_TAG_CONTACT_CONVERT, nation_id, "", body
          );
        }
        const int convert_type = units_kind_type_index(ctx->units, UNITS_KIND_COLONIST);
        if (convert_type >= 0) {
          const int cid = units_spawn_allow_stack(ctx->units, convert_type, c->x, c->y);
          ColonizeUnit* convert = cid >= 0 ? units_get(ctx->units, cid) : NULL;
          if (convert) {
            /*
             * DOS FUN_281f_095c(0, colony_owner, x, y) stamps the owner nibble
             * through the shared spawn helper (FUN_1427_02ca restamps tile
             * occupancy) — the port's equivalent is units_set_nation, which
             * also sets col1_vis_mask = 1<<nation. A raw nation_id assignment
             * left the mask at 0 (units_spawn zeroes it) so the owner could
             * not see its own convert through the fog and the mask
             * round-tripped wrong through the save (bugs.md #879). Sibling
             * spawn site: units_spawn_subjugated_convert, units_combat.c.
             */
            units_set_nation(convert, c->nation_id);
            convert->profession = COLONIZE_PROF_CONVERT;
          }
        }
        if (ctx->status && ctx->status_size && !human_convert) {
          snprintf(
            ctx->status, ctx->status_size, "The %s send a convert to %s.",
            ai_contact_tribe_name(nation_id), c->name
          );
        }
        return 1; /* DOS: goto LAB_5bfb_1000 — the visit is resolved */
      }
    }
    AiContactMeetEcon2154 econ;
    memset(&econ, 0, sizeof(econ));
    if (!ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
      continue;
    }
    ai_contact_bind_names(ctx);
    const int human = ai_contact_euro_is_human(ctx, e);

    /*
     * DOS-LITERAL FUN_5bfb_022e raw 87903 (LAB_5bfb_096c):
     *   if ((bid[0] <= ask[0]) || bVar7 || (0x19 < colony_food)) -> @INDIANGIVESTUFF
     * i.e. @INDIANGIVEFOOD only when the village has a food surplus, the
     * colony is under 26 food, AND this visit did not already TAKE food off
     * the colony in the beg arm (bVar7 -- bugs.md #863).
     */
    if (!bvar7 && (int)econ.bid[0] > (int)econ.ask[0] &&
        c->stock[COLONIZE_CARGO_FOOD] <= 0x19) {
      /* @INDIANGIVEFOOD: village food surplus, colony starving → top to 75. */
      const int qty = 0x4b - c->stock[COLONIZE_CARGO_FOOD];
      c->stock[COLONIZE_CARGO_FOOD] += qty;
      if (human) {
        units_combat_watch_notify(ctx->units, brave->id, c->x, c->y);
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = ai_contact_tribe_name(nation_id);
        tok.number0 = qty;
        tok.has_number0 = true;
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages, "INDIANGIVEFOOD", &tok,
          "",
          body, sizeof(body)
        );
        ai_contact_human_chrome(
          ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Gift", body
        );
      }
    } else {
      /* @INDIANGIVESTUFF: gift the good the village values most that fits.
       * key[0]=0 is DOS-LITERAL FUN_5bfb_022e raw 97019: `*(int*)0x9e78 = 0`
       * clears the sort key array before the loop below repopulates it. */
      int16_t key[16];
      memcpy(key, econ.bid, sizeof(key));
      key[0] = 0;
      /*
       * DOS-LITERAL FUN_5bfb_022e raw 96992: `iVar9 = FUN_281f_0d3a(0x281f);`
       * computes the warehouse capacity ONCE before the cargo loop (raw
       * 97020-97024: `iVar9 + -10 < stock[local_32]`), not per cargo. The
       * accessor ignores cargo_type (colonies_warehouse_capacity, colony_goods.c)
       * so hoisting the call changes nothing today, but matches the DOS shape
       * (bugs.md #892).
       */
      const int cap = colonies_warehouse_capacity(ctx->colonies, c, COLONIZE_CARGO_FOOD);
      for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
        if (c->stock[i] > cap - 10) {
          key[i] = 1; /* near-full — deprioritized in the sort */
        }
      }
      int order[16];
      ai_contact_2820_sort(key, order);
      int cargo = -1;
      for (int rank = dos_rng_range(ctx->rng, 1, 3); rank <= 16; ++rank) {
        const int cand = order[16 - rank];
        if (cand == COLONIZE_CARGO_FOOD) {
          continue; /* food has its own arm above */
        }
        if (cand == COLONIZE_CARGO_SILVER && ind->tech < 2) {
          /* DOS-LITERAL FUN_5bfb_022e raw 97031: `if ((iVar16 == 7) &&
           * (byte[*0x8d4e+2] < 2)) local_4 = 0;` — only advanced tribes
           * (Aztec/Inca tier) gift silver. */
          continue;
        }
        cargo = cand;
        break;
      }
      if (cargo < 0) {
        continue;
      }
      /*
       * DOS-LITERAL FUN_5bfb_022e raw 87930:
       *   100 / (*(byte *)(cargo + colony[0x1a] * 0x10 - 0x7b44) + 1)
       * The nation index is the VISITED colony's owner byte (colony+0x1a == e),
       * and -0x7b44 is the live DS:0x84BC row = euro_price[cargo] - 1 (bugs.md
       * #797/#819) — not a start-of-game capture.
       */
      int qty = 100 / (ai_contact_2820_price_byte(ctx->col1, e, cargo) + 1);
      const int alt = (int)key[cargo] + 5;
      if (alt < qty) {
        qty = alt;
      }
      if (qty < 5) {
        qty = 5;
      }
      if (qty > 100) {
        qty = 100;
      }
      const int room = colonies_warehouse_capacity(ctx->colonies, c, cargo) - c->stock[cargo];
      if (room < qty) {
        qty = room;
      }
      if (qty < 2) {
        qty = 2; /* DOS floor even into a full warehouse */
      }
      c->stock[cargo] += qty;
      if (human) {
        units_combat_watch_notify(ctx->units, brave->id, c->x, c->y);
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = ai_contact_tribe_name(nation_id);
        tok.string1 = c->name;
        tok.string2 = ai_contact_cargo_name(cargo);
        tok.number0 = qty;
        tok.has_number0 = true;
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(
          ctx->messages, "INDIANGIVESTUFF", &tok,
          "",
          body, sizeof(body)
        );
        ai_contact_human_chrome(
          ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "Gift", body
        );
      }
    }
    if (ctx->status && ctx->status_size && !human) {
      snprintf(
        ctx->status, ctx->status_size, "The %s bring gifts to %s.",
        ai_contact_tribe_name(nation_id), c->name
      );
    }
    return 1; /* one gift-bearing visit per Indian nation per turn */
  }
  return 0;
}

/*
 * ===========================================================================
 * FUN_5bfb_022e LAB_5bfb_0def — the demand half's reparations sites.
 * viceroy_unpacked.c 96856-96979; the shared refuse limb is LAB_5bfb_0ff2
 * at raw 96973 and the block ends at LAB_5bfb_1000.
 *
 * Two flavors, picked exactly as DOS picks them (`if (-1 < local_4c)` first,
 * else `local_42`):
 *
 *   @INDIANCITY   (tag 0x1866, raw 96882) — the visit landed beside a Euro
 *       COLONY: the village names the good it wants most out of the colony
 *       stores. DOS scores every cargo `price[g] * min(stock[g],100)` off the
 *       per-nation DS:0x84BC (= −0x7b44) byte row, with two adjustments:
 *         g == 8 (Horses):  want = 10 − (horse_herds − want)
 *         g == 15 (Muskets): want += (rng(1,4) − tech) + difficulty + 4
 *       then `contact_state[e] = 1`, and `rng(0, difficulty+1) == 0` halves
 *       the quantity asked. Accept is row 2 ("Hand them over.").
 *
 *   @INDIANWAGONS (tag 0x1871, raw 96959) — no colony, but the Euro side of
 *       the encounter is a WAGON TRAIN (DOS unit type 0x0c, `local_42`): the
 *       village demands the whole of hold 0. Accept is row 1 ("Hand them
 *       over."), and accepting strips the hold outright (FUN_281f_0aec).
 *
 * ACCEPT effects (both flavors):
 *   village attitude word toward this European -> 0   (*(0x8d4a + e*2 + 10))
 *   FUN_281f_0d6c(nation, e, delta, 0) with a NEGATIVE delta:
 *     @INDIANCITY   delta = (best_score * 4) / -100, then walked down in
 *                   steps of 5 while `alarm + delta >= 0x47` (the same
 *                   "reconciled" loop @INDIANBEGFOOD's accept row uses)
 *     @INDIANWAGONS delta = (price[cargo] * qty * 4) / -100  (no walk-down)
 *   the goods leave (colony stock -= qty / the wagon's hold 0 is emptied)
 *   @INDIANCITY only, the arming tail: Muskets arm the visiting Brave
 *   (type +1) or bank `ind->muskets`; Horses mount it (type +2) or bank
 *   `ind->horse_breeding += 0x32`, and always `ind->horse_herds += 1`.
 *
 * REFUSE effect (LAB_5bfb_0ff2, both flavors — THE point of this port):
 *   village attitude word += 0x80, and nothing changes hands. That single
 *   `piVar2 = ...; *piVar2 = *piVar2 + 0x80;` was the last unported raiser
 *   of the settlement attitude word (docs/indians.md); every other writer
 *   (465b trespass, 152e encroachment, the beg-refused ×1.5, the raid /
 *   combat discharges) was already live. +0x80 lands the word in the
 *   `0x7f <` hostile band that `local_10` at the top of 022e reads, so one
 *   refusal switches this village off the gift arm and onto the demand arm
 *   for good — which is why it matters.
 *
 * AI Euro targets take DOS's `else` limb: @INDIANWAGONS auto-accepts
 * (`local_c = 1`), @INDIANCITY auto-accepts unless the good demanded is
 * Muskets (`local_40 == 0xf -> local_c = 1`). DOS also has a second AI
 * refusal roll there (`FUN_281f_07e0(10)` -> `FUN_281f_08bc`, raw 96888-96892);
 * neither thunk is resolved, and inventing a draw would move every
 * downstream RNG value, so it is deliberately NOT modelled.
 *
 * TRIGGER: the same reconstruction @INDIANBEGFOOD and the gift arm use — a
 * unit of this nation that WALKED UP to the target this turn
 * (ai_contact_brave_walked_up_to), at most one event per Indian nation per
 * turn (no turn cooldown — see the visit pacing note at the top of this
 * file).
 * Like the gift arm, the whole arm declines without a popup queue: a caller
 * with no presentation context (the DOS colony-production golden fixtures
 * drive turn_end directly) is replaying production math, and this arm both
 * draws RNG and moves colony stores.
 * ===========================================================================
 */

AiContactReparations ai_contact_s_reparations[4];

/*
 * New-game / load hook (sibling of ai_goals_reset, called from
 * ai_init_new_game). ai_contact_s_reparations holds a pending Indian reparations offer
 * with a colony_id / unit_id / tribe_index into the CURRENT game; without
 * this, an offer outstanding when the player starts or loads another game
 * resolves against col1->tribe[] of a different world (smell #58).
 */
void ai_contact_reset(void) {
  memset(ai_contact_s_reparations, 0, sizeof(ai_contact_s_reparations));
  memset(ai_contact_s_visit_mood, 0, sizeof(ai_contact_s_visit_mood));
  /* Audit AC-37: these two carry indices into the PREVIOUS game as well —
   * ai_contact_s_2820 latches active/unit_id/nation_id for a village trade in flight,
   * ai_contact_s_visit_brave_id a brave id plus its turn stamp. */
  memset(ai_contact_s_2820, 0, sizeof(ai_contact_s_2820));
  for (size_t i = 0; i < sizeof(ai_contact_s_visit_brave_id) / sizeof(ai_contact_s_visit_brave_id[0]); ++i) {
    ai_contact_s_visit_brave_id[i] = -1;
  }
  memset(ai_contact_s_visit_brave_turn, 0, sizeof(ai_contact_s_visit_brave_turn));
  /*
   * @RAID* chrome scratch. ai_contact_apply_raid_loot re-seeds all five at its
   * head, but ai_contact_last_raid_kind() is readable without a raid, so a new
   * game must not report the previous game's last raid. Initializers as
   * declared at the top of this file.
   */
  ai_contact_s_last_raid_kind = AI_RAID_NOTHING;
  ai_contact_s_last_burn_building[0] = '\0';
  ai_contact_s_last_stores_cargo[0] = '\0';
  ai_contact_s_last_ship_type[0] = '\0';
  ai_contact_s_last_gold_drained = 0;
  /* Bound NAMES.TXT catalog — a dangling pointer into the previous game's
   * assets until the next ai_contact_bind_names call. */
  ai_contact_s_contact_names = NULL;
  village_trade_intel_reset(); /* sidebar Buys/Sells knowledge is campaign-scoped */
}
/* ===================== Reparations pricing, ladder & presentation (ai_contact_reparations_price .. ai_contact_try_village_reparations) ===================== */


/*
 * DOS's demand price row: `-0x7b44 + nation*0x10 + good` = the LIVE per-nation
 * DS:0x84BC sell-price row (bugs.md #797). Was a frozen capture table
 * capture, which pinned @INDIANWAGONS alarm and the @INDIANCITY cargo scan to
 * start-of-game prices for the whole campaign.
 */
static int ai_contact_reparations_price(const ColonizeCol1Save* col1, int e, int cargo) {
  return ai_contact_2820_price_byte(col1, e, cargo);
}

/*
 * The DOS Brave type ladder (0x13 Brave, 0x14 Armed Brave, 0x15 Mtd. Brave,
 * 0x16 Mtd. Warrior): +1 arms, +2 mounts, exactly the arithmetic 022e's
 * accept tail does on the unit's `+0x3146` type byte. Same names
 * units_new_village_temp_defender picks from, so a missing entry is a safe
 * no-op rather than a wrong unit.
 */
static int ai_contact_brave_ladder_rank(const ColonizeUnitPool* pool, const ColonizeUnit* u) {
  if (!pool || !u) {
    return -1;
  }
  const ColonizeUnitType* ty = units_type((ColonizeUnitPool*)pool, u->type_index);
  if (!ty) {
    return -1;
  }
  for (int i = 0; i < 4; ++i) {
    if (strcmp(ty->name, reports_brave_ladder_name(i)) == 0) {
      return i;
    }
  }
  return -1;
}

/* FUN_281f_0902 / 08d0 stand-ins: is the visitor already armed / mounted? */
static void ai_contact_brave_ladder_add(
  ColonizeUnitPool* pool, ColonizeUnit* u, int step
) {
  const int rank = ai_contact_brave_ladder_rank(pool, u);
  if (rank < 0) {
    return;
  }
  int next = rank | step; /* +1 = muskets bit, +2 = horses bit */
  if (next > 3) {
    next = 3;
  }
  if (next == rank) {
    return;
  }
  const int ti = units_find_type(pool, reports_brave_ladder_name(next));
  if (ti >= 0) {
    u->type_index = ti;
  }
}

/*
 * LAB_5bfb_0ff2 / the two accept limbs. `accept` is already resolved against
 * the flavor's own row numbering by the caller.
 */
void ai_contact_apply_reparations(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int flavor,
  int accept
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || e < 0 || e > 3) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  AiContactReparations* s = &ai_contact_s_reparations[e];
  if (!s->active || s->nation_id != nation_id || s->flavor != flavor) {
    return; /* stale / never offered — never move goods on a guess */
  }
  s->active = 0;
  ColonizeCol1Tribe* t = NULL;
  if (ctx->col1->tribe && s->tribe_index >= 0 &&
      s->tribe_index < (int)ctx->col1->head.tribe_count) {
    t = &ctx->col1->tribe[s->tribe_index];
  }
  ai_contact_bind_names(ctx);

  if (!accept) {
    /*
     * LAB_5bfb_0ff2 — the refuse limb. No goods move; the village's own
     * attitude word toward this European gains 0x80. DOS writes the whole
     * signed word, so the carry lands in `attacks` (the high byte) once
     * `friction` is already past 0x80, exactly as col1_tribe_attitude_set
     * re-splits it.
     */
    if (t) {
      const int w = col1_tribe_attitude(t, e);
      col1_tribe_attitude_set(t, e, w + 0x80);
    }
    /* bugs.md #802: DOS writes no status line here (no such text in any
     * COLONIZE .TXT section); the invented English narration is gone. */
    return;
  }

  /* Accept: the attitude word is discharged outright (both flavors). */
  if (t) {
    col1_tribe_attitude_set(t, e, 0);
  }

  int delta = 0;
  if (flavor == AI_CONTACT_REPARATIONS_CITY) {
    delta = (s->score * 4) / -100;
    /*
     * `while (alarm + delta >= 0x47) delta -= 5;` — the same reconciliation
     * loop the @INDIANBEGFOOD accept row runs. Bounded: delta only falls and
     * alarm is a 0..100 byte.
     */
    const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e);
    for (int guard = 0; guard < 32 && alarm + delta >= 0x47; ++guard) {
      delta -= 5;
    }
  } else {
    delta = (ai_contact_reparations_price(ctx->col1, e, s->cargo) * s->qty * 4) / -100;
  }
  if (delta != 0) {
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, delta);
  }

  if (flavor == AI_CONTACT_REPARATIONS_CITY) {
    ColonizeColony* c = NULL;
    if (ctx->colonies && s->colony_id >= 0 && s->colony_id < COLONIZE_COLONIES_MAX) {
      c = &ctx->colonies->colonies[s->colony_id];
    }
    if (c && c->active && c->nation_id == e && s->cargo >= 0 &&
        s->cargo < COLONIZE_CARGO_COUNT) {
      c->stock[s->cargo] -= s->qty;
      if (c->stock[s->cargo] < 0) {
        c->stock[s->cargo] = 0;
      }
    }
    /* Arming tail (raw 96906-96928) — Muskets arm, Horses mount. */
    ColonizeUnit* brave =
      (ctx->units && s->brave_id >= 0) ? units_get(ctx->units, s->brave_id) : NULL;
    if (s->cargo == COLONIZE_CARGO_MUSKETS) {
      const int rank = ai_contact_brave_ladder_rank(ctx->units, brave);
      if (brave && rank >= 0 && (rank & 1) == 0) {
        ai_contact_brave_ladder_add(ctx->units, brave, 1);
      } else {
        /* DOS 022e raw ~96906-96928: plain signed-byte `+= 1`, no cap
         * (bugs.md #841 — the invented 0xff guard is gone). */
        ind->muskets = (uint8_t)(ind->muskets + 1);
      }
    } else if (s->cargo == COLONIZE_CARGO_HORSES) {
      const int rank = ai_contact_brave_ladder_rank(ctx->units, brave);
      if (brave && rank >= 0 && (rank & 2) == 0) {
        ai_contact_brave_ladder_add(ctx->units, brave, 2);
      } else {
        ind->horse_breeding = (uint16_t)(ind->horse_breeding + 0x32);
      }
      /* DOS 022e raw ~96906-96928: unconditional signed-byte `+= 1`
       * (bugs.md #841). */
      ind->horse_herds = (uint8_t)(ind->horse_herds + 1);
    }
    /* bugs.md #802: DOS writes no status line here (no such text in any
     * COLONIZE .TXT section); the invented English narration is gone. */
    return;
  }

  /* @INDIANWAGONS accept: FUN_281f_0aec strips the whole hold. */
  ColonizeUnit* wag =
    (ctx->units && s->unit_id >= 0) ? units_get(ctx->units, s->unit_id) : NULL;
  if (wag && wag->active && wag->nation_id == e && s->hold >= 0 &&
      s->hold < COLONIZE_UNIT_CARGO_MAX) {
    wag->hold_goods_amount[s->hold] = 0;
    wag->hold_goods_type[s->hold] = 0;
  }
  /* bugs.md #802: DOS writes no status line here (no such text in any
   * COLONIZE .TXT section); the invented English narration is gone. */
}

/* A unit of `nation_id` that walked up to (x,y) this turn, or NULL. */
static ColonizeUnit* ai_contact_reparations_visitor(
  ColonizeTurnContext* ctx, int nation_id, int x, int y
) {
  if (!ctx || !ctx->units) {
    return NULL;
  }
  /* Slot walk — `ui` is not a unit id; see ai_contact_land_combat_sum. */
  for (int ui = 0; ui < COLONIZE_UNITS_MAX; ++ui) {
    ColonizeUnit* bu = &ctx->units->units[ui];
    if (!bu->active || bu->nation_id != nation_id || !units_is_on_map(bu)) {
      continue;
    }
    if (abs(bu->x - x) > 1 || abs(bu->y - y) > 1) {
      continue;
    }
    if (!ai_contact_brave_walked_up_to(bu, x, y)) {
      continue;
    }
    return bu;
  }
  return NULL;
}

/* The visiting unit's own village record (DOS FUN_281f_0a4c bind). */
static int ai_contact_reparations_home_tribe(
  const ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* brave
) {
  if (!ctx || !ctx->col1 || !ctx->col1->tribe) {
    return -1;
  }
  if (brave && brave->home_tribe_id >= 0 &&
      brave->home_tribe_id < (int)ctx->col1->head.tribe_count &&
      (int)ctx->col1->tribe[brave->home_tribe_id].nation_id == nation_id) {
    return brave->home_tribe_id;
  }
  /*
   * No fallback: DOS aborts the WHOLE encounter when the visiting unit's
   * origin byte is unset — `if (*(char *)(param_3 * 0x1c + 0x314a) < '\0')
   * goto LAB_5bfb_1005;` (viceroy 96705), right before FUN_281f_0a4c binds
   * DS:0x8d4a to that village record. The old "first village of the nation"
   * fallback made the refusal's +0x80 (LAB_5bfb_0ff2) land on a village that
   * is not the demanding Brave's home, so units_native_village_grudge — which
   * reads the mover's own +0x314a row, exactly like FUN_521d_0906 on
   * DS:0x54f6 — never saw the grudge and the Brave never answered a refusal.
   */
  for (uint16_t ti = 0; ti < ctx->col1->head.tribe_count; ++ti) {
    if ((int)ctx->col1->tribe[ti].nation_id == nation_id) {
      return (int)ti;
    }
  }
  return -1;
}

/* Enqueue the human CHOICE, or run DOS's `else` limb for an AI Euro. */
static void ai_contact_reparations_present(
  ColonizeTurnContext* ctx,
  ColonizeCol1Indian* ind,
  int nation_id,
  int e,
  int flavor,
  const char* section,
  const PopupMsgTokens* tok,
  const char* fallback,
  int ai_accept
) {
  if (!ai_contact_euro_is_human(ctx, e)) {
    /* DOS `else` limb — no dialog, the AI's own answer applies at once. */
    ai_contact_apply_reparations(ctx, ind, nation_id, e, flavor, ai_accept);
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, section, tok, fallback, body, sizeof(body));
  char label_buf[2][POPUP_MSG_CHOICE_LEN];
  const char* labels[2];
  popup_msg_section_labels(
    ctx->messages,
    section,
    tok,
    "",
    "",
    label_buf,
    labels
  );
  /* DOS row numbers: FUN_291f_019c returns 1 for the first printed row. */
  const int ids[2] = {AI_CONTACT_REPARATIONS_ROW1, AI_CONTACT_REPARATIONS_ROW2};
  if (!ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_CONTACT_REPARATIONS, e, nation_id, flavor, NULL,
        body, labels, ids, 2
      )) {
    ai_contact_s_reparations[e].active = 0;
    return;
  }
  /* raw 96882 / 96959: `FUN_291f_019c(0x281f, 0x1866|0x1871, *(0x8d52))` —
   * @INDIANCITY / @INDIANWAGONS are chief audiences, portrait and all. */
  ai_contact_chief_flair(ctx, e, nation_id);
  /* bugs.md #802: DOS writes no status line here (no such text in any
   * COLONIZE .TXT section); the invented English narration is gone. */
}

void ai_contact_try_village_reparations(ColonizeTurnContext* ctx, int nation_id) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ctx->colonies || !ctx->col1->tribe ||
      !ctx->rng || !ctx->units) {
    return;
  }
  if (nation_id < 4 || nation_id > 11) {
    return;
  }
  if (!ctx->ai_popups) {
    return; /* no presentation context — see the header's TRIGGER note */
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[nation_id - 4];
  for (int e = 0; e < 4; ++e) {
    if (!ind->euro_diplo[e]) {
      continue; /* unmet — first contact runs its own arm */
    }
    if (ai_contact_s_reparations[e].active) {
      continue; /* an offer of ours is still on the queue */
    }
    /* 022e's own ceiling on the whole visit (`if (0x4a < iVar9) return;`). */
    if (ai_diplo_indian_alarm(ctx->col1, nation_id, e) > 0x4a) {
      continue;
    }
    /* LAB_5bfb_0def's gate: a resolved generous visit latches this off. */
    if (ind->contact_state[e] == 2) {
      continue;
    }

    /* `if (-1 < local_4c)` — a colony the visitor walked up to wins. */
    int city_ci = -1;
    ColonizeUnit* brave = NULL;
    for (int ci = 0; ci < COLONIZE_COLONIES_MAX && city_ci < 0; ++ci) {
      const ColonizeColony* c = &ctx->colonies->colonies[ci];
      if (!c->active || c->nation_id != e) {
        continue;
      }
      ColonizeUnit* v = ai_contact_reparations_visitor(ctx, nation_id, c->x, c->y);
      if (!v) {
        continue;
      }
      city_ci = ci;
      brave = v;
    }

    if (city_ci >= 0) {
      ColonizeColony* c = &ctx->colonies->colonies[city_ci];
      const int tribe_index = ai_contact_reparations_home_tribe(ctx, nation_id, brave);
      if (tribe_index < 0) {
        continue;
      }
      if (!ai_contact_visit_demand_allowed(
            ctx, ind, &ctx->col1->tribe[tribe_index], nation_id, e,
            brave ? brave->id : -1, ai_diplo_indian_alarm(ctx->col1, nation_id, e)
          )) {
        continue; /* bVar6 — this encounter is the generous half, not a demand */
      }
      /* The cargo scan (raw 96843-96874). */
      int best_cargo = -1;
      int best_qty = 0;
      int best_score = 0;
      for (int g = 0; g < COLONIZE_CARGO_COUNT; ++g) {
        int stock = c->stock[g];
        if (stock > 100) {
          stock = 100;
        }
        if (stock <= 0) {
          continue;
        }
        int want = ai_contact_reparations_price(ctx->col1, e, g);
        if (g == COLONIZE_CARGO_HORSES) {
          /* DOS reads +8 (horse_herds) as a SIGNED char here, as the
           * §6c breeding tick in ai_contact_indian_nation_tick does. */
          want = 10 - ((int)(int8_t)ind->horse_herds - want);
        } else if (g == COLONIZE_CARGO_MUSKETS) {
          want += (dos_rng_range(ctx->rng, 1, 4) - (int)ind->tech) +
                  (int)ctx->col1->head.difficulty + 4;
        }
        const int score = want * stock;
        if (score > best_score) {
          best_cargo = g;
          best_qty = stock;
          best_score = score;
        }
      }
      if (best_cargo < 0) {
        continue; /* DOS `if (local_40 < 0) goto LAB_5bfb_0dea;` */
      }
      ind->contact_state[e] = 1;
      if (dos_rng_range(ctx->rng, 0, (int)ctx->col1->head.difficulty + 1) == 0) {
        best_qty >>= 1;
      }
      if (best_qty <= 0) {
        continue;
      }
      AiContactReparations* s = &ai_contact_s_reparations[e];
      s->active = 1;
      s->nation_id = nation_id;
      s->flavor = AI_CONTACT_REPARATIONS_CITY;
      s->tribe_index = tribe_index;
      s->brave_id = brave ? brave->id : -1;
      s->colony_id = city_ci;
      s->unit_id = -1;
      s->hold = -1;
      s->cargo = best_cargo;
      s->qty = best_qty;
      s->score = best_score;
      ai_contact_bind_names(ctx);
      if (brave && ai_contact_euro_is_human(ctx, e)) {
        units_combat_watch_notify(ctx->units, brave->id, c->x, c->y);
      }
      PopupMsgTokens tok;
      memset(&tok, 0, sizeof(tok));
      tok.string0 = ai_contact_euro_name(e);
      tok.string1 = ai_contact_tribe_name(nation_id);
      tok.string2 = ai_contact_cargo_name(best_cargo);
      tok.string3 = c->name;
      tok.number0 = best_qty;
      tok.has_number0 = true;
      /*
       * AI Euro (`local_c = 2` unless the good is Muskets): auto-accept.
       * The second DOS refusal roll is not modelled — see the header.
       */
      const int ai_accept = (best_cargo != COLONIZE_CARGO_MUSKETS);
      ai_contact_reparations_present(
        ctx, ind, nation_id, e, AI_CONTACT_REPARATIONS_CITY, "INDIANCITY", &tok,
        "",
        ai_accept
      );
      return; /* one reparations demand per Indian nation per turn */
    }

    /* `local_42` — no colony, but a Wagon Train (DOS type 0x0c) with cargo. */
    ColonizeUnit* wag = NULL;
    int hold = -1;
    /* Slot walk — `ui` is not a unit id; see ai_contact_land_combat_sum. */
    for (int ui = 0; ui < COLONIZE_UNITS_MAX && !wag; ++ui) {
      ColonizeUnit* u = &ctx->units->units[ui];
      if (!u->active || u->nation_id != e || !units_is_on_map(u)) {
        continue;
      }
      const ColonizeUnitType* ty = units_type(ctx->units, u->type_index);
      if (!units_type_is_wagon(ty)) {
        continue;
      }
      /*
       * DOS reads hold 0 only (FUN_281f_0be6/0c68 with slot 0) and does NOT
       * require it to be non-empty: `local_42` is just the encounter tile's
       * unit when its type byte is 0x0c (raw 96690/96696), so a 0-qty demand
       * is legal (bugs.md #807). The port's slot walk stays, but it is
       * narrowed to the Brave's own encounter partner by the visitor test
       * below, which is DOS's `FUN_281f_07e0()` in all reachable states.
       */
      if (!ai_contact_reparations_visitor(ctx, nation_id, u->x, u->y)) {
        continue;
      }
      wag = u;
      hold = 0;
    }
    if (!wag) {
      continue;
    }
    brave = ai_contact_reparations_visitor(ctx, nation_id, wag->x, wag->y);
    const int tribe_index = ai_contact_reparations_home_tribe(ctx, nation_id, brave);
    if (tribe_index < 0) {
      continue;
    }
    if (!ai_contact_visit_demand_allowed(
          ctx, ind, &ctx->col1->tribe[tribe_index], nation_id, e,
          brave ? brave->id : -1, ai_diplo_indian_alarm(ctx->col1, nation_id, e)
        )) {
      continue; /* bVar6 — generous encounter; DOS never reaches @INDIANWAGONS */
    }
    ind->contact_state[e] = 1;
    AiContactReparations* s = &ai_contact_s_reparations[e];
    s->active = 1;
    s->nation_id = nation_id;
    s->flavor = AI_CONTACT_REPARATIONS_WAGONS;
    s->tribe_index = tribe_index;
    s->brave_id = brave ? brave->id : -1;
    s->colony_id = -1;
    s->unit_id = wag->id;
    s->hold = hold;
    s->cargo = wag->hold_goods_type[hold];
    s->qty = wag->hold_goods_amount[hold];
    s->score = 0;
    ai_contact_bind_names(ctx);
    if (brave && ai_contact_euro_is_human(ctx, e)) {
      units_combat_watch_notify(ctx->units, brave->id, wag->x, wag->y);
    }
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_euro_name(e);
    tok.string1 = ai_contact_tribe_name(nation_id);
    tok.string2 = ai_contact_cargo_name(s->cargo);
    tok.number0 = s->qty;
    tok.has_number0 = true;
    /* DOS `else { local_c = 1; }` — an AI Euro always hands the wagon over. */
    ai_contact_reparations_present(
      ctx, ind, nation_id, e, AI_CONTACT_REPARATIONS_WAGONS, "INDIANWAGONS", &tok,
      "",
      1
    );
    return; /* one reparations demand per Indian nation per turn */
  }
}
/* ===================== Village trade haggle/buy-sell mechanics (2820/2e92) & wagon trade dispatch (ai_contact_2820_prepare .. ai_contact_find_adjacent_euro) ===================== */


/*
 * Shell tables: 2154 ask/bid, then
 *   bid0 = bid[0]; bid[0] = 0; if (bid[13] < bid0) ask[0] = 0;
 *   sort by bid; top three: ask[c] = 0 (a food slot becomes cloth, 0xc).
 */
static int ai_contact_2820_prepare(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* unit, AiContact2820* s
) {
  const ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id, unit);
  if (!t) {
    return 0;
  }
  AiContactMeetEcon2154 econ;
  memset(&econ, 0, sizeof(econ));
  if (!ai_contact_meet_economics_2154(ctx, nation_id, t, &econ)) {
    return 0;
  }
  memcpy(s->ask, econ.ask, sizeof(s->ask));
  memcpy(s->bid, econ.bid, sizeof(s->bid));
  const int bid0 = s->bid[0];
  s->bid[0] = 0;
  if (s->bid[13] < bid0) {
    s->ask[0] = 0;
  }
  ai_contact_2820_sort(s->bid, s->cand);
  for (int i = 1; i < 4; ++i) {
    const int c = s->cand[16 - i];
    s->ask[c] = 0;
    if (c == 0) {
      s->cand[16 - i] = 12;
    }
  }
  return 1;
}

/*
 * LAB_002bbc price (iStack_c8 = cargo, iStack_6a = qty, aiStack_d6[0] = alarm):
 *   r = RNG(1,5); base = cargo > 8 ? 7 : 6;
 *   13: base -= RNG(0,7); 15: base -= muskets-12; 8: base -= horse_herds-10; 14: base += 1
 *   tier2 = quartile(alarm)<<1 (0 for 15/8; >>1 when ask > 19)
 *   raw = max(0, ((base-diff) - tier2 + r + 4) * 2 * ask)
 *   price = max(1, ((r*5 + raw) * qty / 100) / 2)
 *   iVar8 = ask - tier2 + 4; value_idx = min(3, iVar8/10); c4 = RNG(0,1) + (iVar8>>2)
 *   fair = (ask+1)*4 + price
 */
int ai_contact_2820_sell_price(
  const ColonizeCol1Indian* ind, int cargo, int ask, int qty, int difficulty, int alarm,
  AiContact2820* s
) {
  ColonizeDosRng* rng = &s->rng;
  const int r = dos_rng_range(rng, 1, 5);
  int base = cargo > 8 ? 7 : 6;
  if (cargo == COLONIZE_CARGO_TRADE_GOODS) {
    base -= dos_rng_range(rng, 0, 7);
  }
  /* DOS-LITERAL: `*(char *)(*0x8d4e + 7)` / `+ 8` — SIGNED byte reads
   * (bugs.md #806); a herd/armoury count past 0x7f makes the term negative. */
  if (cargo == COLONIZE_CARGO_MUSKETS) {
    base -= (int)(int8_t)ind->muskets - 12;
  }
  if (cargo == COLONIZE_CARGO_HORSES) {
    base -= (int)(int8_t)ind->horse_herds - 10;
  }
  if (cargo == COLONIZE_CARGO_TOOLS) {
    base += 1;
  }
  int tier2 = ai_relation_quartile(alarm) << 1;
  if (cargo == COLONIZE_CARGO_MUSKETS || cargo == COLONIZE_CARGO_HORSES) {
    tier2 = 0;
  }
  if (ask > 19) {
    tier2 >>= 1;
  }
  int raw = ((base - difficulty) - tier2 + r + 4) * 2 * ask;
  if (raw < 0) {
    raw = 0;
  }
  int price = (int)(((long)(r * 5 + raw) * (long)qty) / 100) / 2;
  if (price < 1) {
    price = 1;
  }
  const int iVar8 = (ask - tier2) + 4;
  int value_idx = iVar8 / 10;
  if (value_idx > 3) {
    value_idx = 3;
  }
  s->c4 = dos_rng_range(rng, 0, 1) + (iVar8 >> 2);
  s->fair = (ask + 1) * 4 + price;
  s->tier2 = tier2;
  s->value_idx = value_idx;
  s->price = price;
  return price;
}

/* @TRADE0 "fairer price" arm. Returns 1 = tribe raises its offer (*io_price, *io_fair
 * updated, c4 decremented), 0 = patience exhausted. Exposed for tests. */
int ai_contact_2820_sell_haggle(
  int difficulty, int ask, int qty, ColonizeDosRng* rng, int* io_c4, int* io_price, int* io_fair
) {
  if (*io_c4 > 0 && dos_rng_range(rng, 1, *io_c4 << 3) > difficulty) {
    (*io_c4)--;
    int inc = dos_rng_range(rng, (ask >> 1) + 1, ask * 2 + 1) * qty / 100;
    if (inc < 1) {
      inc = 1;
    }
    *io_price += inc;
    if (*io_fair <= *io_price) {
      *io_fair = *io_price + 10;
    }
    return 1;
  }
  return 0;
}

/* LAB_002bbc accept (iStack_5e == 1): slot out, gold in, tribe bookkeeping. */
static void ai_contact_2820_sell_settle(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, ColonizeCol1Tribe* t, int nation_id, int e,
  ColonizeUnit* unit, AiContact2820* s
) {
  const int cargo = s->cargo;
  const int qty = ai_contact_2820_remove_slot(unit, s->slot);
  s->qty = qty; /* DS:0x8dc4 */
  /* audit G3: single treasury — village trade is the human's main non-Europe
   * gold source, and crediting the record alone meant the sale evaporated at
   * the next europe→col1 push. */
  europe_nation_gold_add(ctx->europe, ctx->col1, e, (long)s->price);
  ind->tons[cargo & 15] = (int16_t)(ind->tons[cargo & 15] + qty);
  if (t) {
    t->sticky_trade_good = 0xff;
  }
  if (s->c4 > 0) {
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, -2 * s->c4);
    ai_contact_2820_friction_sub(t, e, qty, qty);
  }
  /*
   * bugs.md #803 — DOS-LITERAL FUN_4d56_2820 raw 82286+ (2820 doc line 551).
   * DOS tests `param_2 == 0xf || param_2 == 8`, where `param_2` is the
   * acting unit's ARRAY INDEX into the 300-slot unit pool (`param_2 * 0x1c +
   * 0x3146` is its type byte — same record-stride evidence as bugs.md
   * #878(a) in ai_euro_land.c), not the cargo id. Ported using the same
   * pool-slot-index proxy as #878(a): `unit - ctx->units->units`.
   */
  if (t) {
    const int slot = (ctx && ctx->units && unit) ? (int)(unit - ctx->units->units) : -1;
    t->last_bought = (slot == 0xf || slot == 8) ? 0xffu : (uint8_t)cargo;
  }
  if (cargo == COLONIZE_CARGO_MUSKETS) {
    if (qty > 0x18) {
      ind->muskets++;
    }
    if (qty > 0x31) {
      ind->muskets++;
    }
  }
  if (cargo == COLONIZE_CARGO_HORSES) {
    ind->horse_breeding = (uint16_t)(ind->horse_breeding + (qty >> 2));
    if (qty > 0x18) {
      ind->horse_herds++;
    }
    if (qty > 0x31) {
      ind->horse_herds++;
    }
  }
}

/* LAB_002bbc gift arm (iStack_5e == 3 && iStack_88 == 0). */
static void ai_contact_2820_gift_settle(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, ColonizeCol1Tribe* t, int nation_id, int e,
  ColonizeUnit* unit, AiContact2820* s
) {
  const int cargo = s->cargo;
  const int qty = ai_contact_2820_remove_slot(unit, s->slot);
  s->qty = qty;
  if (t) {
  /*
   * bugs.md #803 — DOS-LITERAL FUN_4d56_2820 raw 82286+ (2820 doc line 617).
   * Same `param_2 == 0xf || param_2 == 8` unit-pool-slot test as the sell
   * arm above (2820 doc line 551); see that comment for the #878(a)
   * evidence for the slot-index proxy.
   */
    t->sticky_trade_good = 0xff;
    const int slot = (ctx && ctx->units && unit) ? (int)(unit - ctx->units->units) : -1;
    t->last_bought = (slot == 0xf || slot == 8) ? 0xffu : (uint8_t)cargo;
  }
  if (s->c4 >= 0) {
    s->c4++;
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, -4 * s->c4);
    ai_contact_2820_friction_sub(t, e, 2 * qty, qty);
  }
  if (cargo == COLONIZE_CARGO_MUSKETS) {
    ind->muskets++;
  }
  if (cargo == COLONIZE_CARGO_HORSES) {
    ind->horse_breeding = (uint16_t)(ind->horse_breeding + (qty >> 2));
    ind->horse_herds++;
  }
}

/* The three highest-ask goods (acStack_7c after zeroing last_bought/last_sold). */
static void ai_contact_2820_wanted(const AiContact2820* s, const ColonizeCol1Tribe* t, int out[3]) {
  int16_t ask[16];
  memcpy(ask, s->ask, sizeof(ask));
  if (t && t->last_bought < 16) {
    ask[t->last_bought] = 0;
  }
  if (t && t->last_sold < 16) {
    ask[t->last_sold] = 0;
  }
  int order[16];
  ai_contact_2820_sort(ask, order);
  out[0] = order[15];
  out[1] = order[14];
  out[2] = order[13];
}

/*
 * LAB_002e92 candidates: walk `cand` (the bid-sorted cargo order) from the top,
 * skipping muskets/food/tools/trade goods. DOS-LITERAL asm 4d56:3144-319b —
 * `aiStack_d6[n + 1] = 0xf - i` stores the SORTED SLOT, not the cargo id, and
 * every later price term reads `bid[slot]`, not `bid[cargo]` (bugs.md #795).
 * That is a DOS indexing quirk (`DS:0x9e78` is a per-cargo table); it is
 * reproduced, not repaired. `slots` may be NULL.
 */
int ai_contact_2e92_candidates(const AiContact2820* s, int goods[3], int slots[3]) {
  int n = 0;
  for (int k = 15; k >= 0 && n < 3; --k) {
    const int c = s->cand[k];
    if (c == 15 || c == 0 || c == 14 || c == 13) {
      continue;
    }
    if (slots) {
      slots[n] = k;
    }
    goods[n++] = c;
  }
  return n;
}

/*
 * DOS-LITERAL asm 4d56:31b8-3204 — the AI's buy pick. It does NOT scan the
 * filtered candidate list: it reads three raw bytes of the bid-sorted array at
 * `BP-0x86 - i`, i.e. sorted slots 16, 15 and 14, scores each with the live
 * DS:0x84bc row and keeps the first strict maximum (seed 0xd8f1 = -9999
 * signed, so slot 16 always wins the first comparison). Slot 16 is ONE PAST
 * the 16-byte array; in DOS it aliases the low byte of `iStack_88`, the haggle
 * round counter, which LAB_002bbc zeroes before every loop the AI can reach —
 * so it is `(int8_t)s->round` here, and `cand[]` itself is never indexed out
 * of bounds. The chosen index then selects from the FILTERED list
 * (`aiStack_d6[iStack_5e]`), which is the quirk this reproduces (bugs.md #796).
 * Returns 0..2.
 */
static int ai_contact_2e92_ai_pick(
  const ColonizeTurnContext* ctx, const AiContact2820* s, int e, int n
) {
  int pick = 1; /* BP-0x5c */
  int best = -9999; /* BP+0xff36 seeded 0xd8f1, compared as a signed word */
  for (int i = 0; i < 3; ++i) {
    const int slot = 16 - i;
    const int c = (slot >= 16) ? (int)(int8_t)(s->round & 0xff) : s->cand[slot];
    const int w = ai_contact_2820_price_byte(ctx->col1, e, c);
    if (w > best) {
      best = w;
      pick = i + 1;
    }
  }
  pick -= 1;
  /* n is 3 in every reachable state (the skip list drops at most 4 of 16
   * cargos); DOS would read uninitialised stack if it were not. */
  if (pick >= n) {
    pick = n > 0 ? n - 1 : 0;
  }
  return pick;
}

/* LAB_002e92 price (uStack_62). qty = DS:0x8dc4 (ships: >> 2). */
static int ai_contact_2e92_price(
  ColonizeTurnContext* ctx, const ColonizeCol1Indian* ind, int nation_id, int e, int cargo,
  int bid, int qty, ColonizeDosRng* rng
) {
  const int diff = (int)ctx->col1->head.difficulty;
  int price = 200;
  if (cargo > 7) {
    price = ((int)ind->tech - 8) * -0x32;
  }
  if (cargo > 6) {
    /* LIVE DS:0x84bc[e*0x10 + cargo] (bugs.md #797), not the frozen capture. */
    price += ai_contact_2820_price_byte(ctx->col1, e, cargo & 15) * (diff * 2 + 15);
  }
  price += dos_rng_range(rng, 0, price);
  price += bid * -4;
  price += ai_diplo_indian_alarm(ctx->col1, nation_id, e) * 4;
  price = (int)(((long)qty * (long)price) / 100);
  price += (diff + dos_rng_range(rng, 0, 2)) * 10;
  if (price < 0x32) {
    price = 0x32;
  }
  return price;
}

/* LAB_002e92 accept (iStack_5e == 1): returns 1 on purchase, 0 when unaffordable. */
static int ai_contact_2e92_settle(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, ColonizeCol1Tribe* t, int nation_id, int e,
  ColonizeUnit* unit, int cargo, int price, int qty, ColonizeDosRng* rng
) {
  /* audit G3: single treasury — the buy gate and the debit both go through
   * the accessor, so a purse topped up in Europe can actually be spent here. */
  if (europe_nation_gold(ctx->europe, ctx->col1, e) < (uint32_t)price) {
    return 0;
  }
  europe_nation_gold_add(ctx->europe, ctx->col1, e, -(long)price);
  if (t) {
    t->last_sold = (uint8_t)(cargo == 9 ? 0xff : cargo); /* literal `if (cargo == 9) +9 = 0xff` */
  }
  ind->tons[cargo & 15] = (int16_t)(ind->tons[cargo & 15] - qty);
  ai_contact_2e92_give_goods(ctx, unit, cargo, qty);
  /* DOS-LITERAL (2820 doc 408-409): the draw happens and its result
   * (`iStack_c4`) is never read again — only the un-rolled `price/25 + 1`
   * reaches FUN_1000_8f5c. Keeping the draw keeps the LCG stream in step
   * (bugs.md #804). */
  if (rng) {
    (void)dos_rng_range(rng, 0, price / 0x19 + 1);
  }
  ai_contact_alarm_delta_00f2(ctx, nation_id, e, price / 0x19 + 1);
  return 1;
}

static int ai_contact_2820_buy_qty(const ColonizeTurnContext* ctx, const ColonizeUnit* unit, int sold_qty) {
  int qty = sold_qty > 0 ? sold_qty : 100;
  if (unit && ctx && ctx->units && units_is_sea(ctx->units, unit->id)) {
    qty >>= 2; /* `if (0xc < type < 0x13) *0x8dc4 >>= 2` */
  }
  return qty;
}

/*
 * @BUY0 {%STRING1}, the container noun in "We shall fill up your {%STRING1}".
 * DOS-LITERAL FUN_4d56_2820 (2820 doc 347-352): `type < 0xd || 0x12 < type`
 * (i.e. not a ship) picks DS:0x2e0c, else DS:0x2e0e. Label ordinals by the
 * @MISC rule (base DS:0x2dba + 2*row): 0x2e0c = row 41, 0x2e0e = row 42 =
 * LABELS.TXT lines 56/57 = "wagons" / "holds". Never typed here (bugs.md
 * #802); a catalog miss is the empty string.
 */
static const char* ai_contact_2820_vehicle_name(const ColonizeTurnContext* ctx, const ColonizeUnit* unit) {
  const int row = (unit && ctx && ctx->units && units_is_sea(ctx->units, unit->id)) ? 42 : 41;
  const char* s = reports_labels_field("MISC", row);
  return s ? s : "";
}

/*
 * DOS-LITERAL `FUN_1000_8c28(dialog, param_4, param_5) & 0x40` — the MET bit
 * of the raw peer byte (docs/ai_euro.md / euro_unit_act.md; col1_save_layout.h
 * records `relation_by_indian[t]` AS that accessor's result). 2820 gates three
 * of its dialogs on it (2820 doc 419-425 / 437-441 / 607-610); every 2820
 * caller is a village entry, so it is set in every reachable state, but the
 * gate is transcribed rather than assumed (bugs.md #805).
 */
int ai_contact_2820_met_bit(const ColonizeTurnContext* ctx, int e, int nation_id) {
  if (!ctx || !ctx->col1 || e < 0 || e >= (int)COLONIZE_COL1_NATION_COUNT || nation_id < 4 ||
      nation_id > 11) {
    return 0;
  }
  return (ctx->col1->nation[e].relation_by_indian[nation_id - 4] & 0x40) != 0;
}

/*
 * @BUY0 Haggle arm (iStack_5e == 2): roll RNG(0, bid/25+8); if price < 11 or
 * roll <= difficulty+1 the tribe loses patience (returns 0: alarm +2,
 * sticky_trade_good = 0xfe, @BADHAGGLE2); else price -= price>>2 (floor 10),
 * a 1-in-(8-difficulty) roll adds alarm +1, and the offer is re-asked with
 * the @BUY1 "grow tired" text (returns 1, *io_price updated).
 */
int ai_contact_2e92_haggle(int difficulty, int bid, ColonizeDosRng* rng, int* io_price, int* out_alarm_delta) {
  int price = *io_price;
  const int roll = dos_rng_range(rng, 0, bid / 0x19 + 8);
  *out_alarm_delta = 0;
  if (price < 0xb || roll <= difficulty + 1) {
    *out_alarm_delta = 2;
    return 0;
  }
  int quarter = price >> 2;
  if (quarter < 1) {
    quarter = 1;
  }
  price -= quarter;
  if (price < 10) {
    price = 10;
  }
  if (dos_rng_range(rng, 1, 8 - difficulty) == 1) {
    *out_alarm_delta = 1;
  }
  *io_price = price;
  return 1;
}

/*
 * Sidebar trade intel (Linux-only, village_trade_intel.h): the settlement of
 * `nation_id` the trading unit stands next to. NULL when none is adjacent —
 * never guess the tribe's first village, the knowledge is per settlement.
 */
static const ColonizeCol1Tribe* ai_contact_intel_village(
  ColonizeTurnContext* ctx, int nation_id, const ColonizeUnit* unit
) {
  return ai_contact_2e92_visited_village(ctx, nation_id, unit);
}

static void ai_contact_intel_note_buys(
  ColonizeTurnContext* ctx, int e, int nation_id, const ColonizeUnit* unit, const int wanted[3]
) {
  const ColonizeCol1Tribe* v = ai_contact_intel_village(ctx, nation_id, unit);
  if (v) {
    village_trade_intel_note_buys(e, v->x, v->y, wanted, 3);
  }
}

/*
 * Choice rows of a GAME.TXT section with the tokens applied — the haggle
 * dialogs' "We gratefully accept {N}$." style rows are catalog text like the
 * body, never typed here. Returns how many rows were filled (0 when the
 * section is missing: the popup then simply has no such row).
 */
static int ai_contact_choice_rows(
  const ColonizeMsgCatalog* catalog, const char* section_name, const PopupMsgTokens* tok,
  char out[][AI_POPUP_CHOICE_LEN], int max_rows
) {
  const ColonizeMsgSection* sec = catalog ? assets_msg_find(catalog, section_name) : NULL;
  if (!sec) {
    return 0;
  }
  char raw[AI_POPUP_CHOICE_MAX][POPUP_MSG_CHOICE_LEN];
  int n = popup_msg_choices(sec, raw, max_rows < AI_POPUP_CHOICE_MAX ? max_rows : AI_POPUP_CHOICE_MAX);
  for (int i = 0; i < n; ++i) {
    popup_msg_apply_tokens(out[i], AI_POPUP_CHOICE_LEN, raw[i], tok);
  }
  return n;
}

static void ai_contact_enqueue_buy0(
  ColonizeTurnContext* ctx, int nation_id, int e, ColonizeUnit* unit, int cargo, int price, int qty,
  int round
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_cargo_name(cargo);
  tok.string1 = ai_contact_2820_vehicle_name(ctx, unit);
  tok.number0 = price;
  tok.has_number0 = true;
  int fair = price >> 1;
  if (fair < 10) {
    fair = 10;
  }
  tok.number1 = fair;
  tok.has_number1 = true;
  tok.number2 = qty;
  tok.has_number2 = true;
  char tag[8];
  snprintf(tag, sizeof(tag), "BUY%d", round > 0 ? 1 : 0); /* FUN_0000_d9b4 + '0'+iStack_88 */
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, tag, &tok, "", body, sizeof(body));
  /* @BUY0/@BUY1 rows: pay {N0} (of {N3}) / a fairer price {N1} / never mind. */
  tok.number3 = (int)europe_nation_gold(ctx->europe, ctx->col1, e); /* audit G3 */
  tok.has_number3 = true;
  char rows[3][AI_POPUP_CHOICE_LEN] = {{0}, {0}, {0}};
  (void)ai_contact_choice_rows(ctx->messages, tag, &tok, rows, 3);
  const char* labels[3] = {rows[0], rows[1], rows[2]};
  const int ids[3] = {1, 2, 0};
  if (ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_CONTACT_BUY0, e, nation_id,
        (unit->id & 0xffff) | (cargo << 16) |
          ((price > 0x7ff ? 0x7ff : price) << 20) | (round > 0 ? (1 << 31) : 0),
        NULL, body, labels, ids, 3
      )) {
    /* 2820 +1297: `FUN_291f_019c(…, BP-0xbe, *(0x8d52))` — @BUY0/@BUY1 carry
     * the chief portrait like every other village dialog. */
    ai_contact_chief_flair(ctx, e, nation_id);
  }
}

/*
 * Human: @BUYWHICH. DOS menu 0x15a0 has four rows — the three goods
 * (`"{%STRING0}."` …) and `"Nothing right now, thank you."`; 2820 only buys
 * on a pick of 1..3 (`0 < iStack_5e < 4`), row 4 ends the visit. The
 * decline row carries id 0 like a cancelled popup. Returns 1 when a CHOICE
 * was queued.
 */
static int ai_contact_enqueue_buywhich(
  ColonizeTurnContext* ctx, int nation_id, int e, ColonizeUnit* unit, const AiContact2820* s
) {
  if (!ctx || !ctx->ai_popups || !unit) {
    return 0;
  }
  int goods[3];
  const int n = ai_contact_2e92_candidates(s, goods, NULL);
  if (n <= 0) {
    return 0;
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_cargo_name(goods[0]);
  tok.string1 = n > 1 ? ai_contact_cargo_name(goods[1]) : "";
  tok.string2 = n > 2 ? ai_contact_cargo_name(goods[2]) : "";
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, "BUYWHICH", &tok, "", body, sizeof(body));
  /* GAME.TXT rows when the section carries all four, else plain fallbacks. */
  char rows[AI_POPUP_CHOICE_MAX][POPUP_MSG_CHOICE_LEN];
  const ColonizeMsgSection* sec = ctx->messages ? assets_msg_find(ctx->messages, "BUYWHICH") : NULL;
  const int nrows = sec ? popup_msg_choices(sec, rows, AI_POPUP_CHOICE_MAX) : 0;
  char filled[4][AI_POPUP_CHOICE_LEN];
  const char* labels[4];
  int ids[4];
  for (int k = 0; k < n; ++k) {
    if (nrows >= 4) {
      popup_msg_apply_tokens(filled[k], sizeof(filled[k]), rows[k], &tok);
    } else {
      snprintf(filled[k], sizeof(filled[k]), "%s", ai_contact_cargo_name(goods[k]));
    }
    labels[k] = filled[k];
    ids[k] = goods[k] + 1; /* 1..16 */
  }
  snprintf(filled[n], sizeof(filled[n]), "%s", nrows >= 4 ? rows[3] : "");
  labels[n] = filled[n];
  ids[n] = 0; /* decline: no purchase */
  if (!ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_CONTACT_BUYWHICH, e, nation_id, unit->id, NULL, body,
        labels, ids, n + 1
      )) {
    return 0;
  }
  /* 2820 +1217: `FUN_291f_019c(…, 0x15a0, *(0x8d52))`. */
  ai_contact_chief_flair(ctx, e, nation_id);
  {
    const ColonizeCol1Tribe* v = ai_contact_intel_village(ctx, nation_id, unit);
    if (v) {
      village_trade_intel_note_sells(e, v->x, v->y, goods, n);
    }
  }
  return 1;
}

/*
 * LAB_002e92 entry, after the sell loop (or straight from the shell when the
 * unit carries nothing). `human` = iStack_8.
 */
static void ai_contact_2820_buy_phase(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit,
  AiContact2820* s, int human
) {
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id, unit);
  if (!unit || !ai_contact_2e92_unit_can_take(ctx, unit)) {
    s->sold_ok = 0;
  }
  if (!s->sold_ok) {
    s->active = 0;
    return;
  }
  int wanted[3];
  ai_contact_2820_wanted(s, t, wanted);
  if (wanted[0] != s->cargo && wanted[1] != s->cargo && human) {
    /* @BRING 0x1587 */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_cargo_name(wanted[0]);
    tok.string1 = ai_contact_cargo_name(wanted[1]);
    tok.string2 = ai_contact_cargo_name(wanted[2]);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BRING", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_MEET, nation_id, "", body);
    ai_contact_intel_note_buys(ctx, e, nation_id, unit, wanted);
  }
  if (human && t && t->sticky_trade_good == 0xfe) {
    /* DOS asm 4d56:2f7e-2f8b — `tribe[+7] == 0xfe` (this tribe already walked
     * out of a haggle) pushes @BADHAGGLE3 (DS:0x158d) and ends the visit. An
     * AI Euro falls straight through to 311e with no dialog (bugs.md #801). */
    s->active = 0;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BADHAGGLE3", NULL, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
    return;
  }
  if (s->cargo < 0) {
    /* DOS FUN_4d56_311e asm 139666-139675 — nothing was sold this visit, so
     * nothing can be bought; a human is told so with @DEFICIT (DS:0x1598),
     * an AI Euro exits silently (bugs.md #801). */
    s->active = 0;
    if (human) {
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "DEFICIT", NULL, "", body, sizeof(body));
      ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
    }
    return;
  }
  int goods[3];
  int slots[3];
  const int n = ai_contact_2e92_candidates(s, goods, slots);
  if (n <= 0) {
    s->active = 0;
    return;
  }
  s->buy_qty = ai_contact_2820_buy_qty(ctx, unit, s->qty);
  if (human) {
    if (!ai_contact_enqueue_buywhich(ctx, nation_id, e, unit, s)) {
      s->active = 0;
    }
    return;
  }
  const int best = ai_contact_2e92_ai_pick(ctx, s, e, n);
  s->buy_cargo = goods[best];
  s->buy_slot = slots[best];
  /* `uStack_62 += bid[iStack_86] * -4` — bid indexed by the SORTED SLOT. */
  const int price = ai_contact_2e92_price(
    ctx, ind, nation_id, e, goods[best], (int)s->bid[slots[best]], s->buy_qty, &s->rng
  );
  if (!ai_contact_2e92_settle(
        ctx, ind, t, nation_id, e, unit, goods[best], price, s->buy_qty, &s->rng
      )) {
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, 1); /* @NOTENOUGH arm */
  }
  s->active = 0;
}

/*
 * TEST-ONLY (bugs.md #798). An AI-controlled empty-handed unit buying the
 * tribe's own goods has NO DOS equivalent: LAB_002e92 is guarded by
 * `if (-1 < iStack_c8)` (2820 doc 284), so a purchase is impossible without a
 * preceding sale in the same visit. The production call site in
 * ai_contact_auto_trade is gone; this entry stays only so the unit tests can
 * drive the LAB_002e92 pick + price + settle arithmetic directly, with qty 100
 * (ships 25).
 */
int ai_contact_auto_buy_2e92(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !unit || e < 0 || e > 3) {
    return 0;
  }
  if (!ai_contact_2e92_unit_can_take(ctx, unit)) {
    return 0;
  }
  AiContact2820 s;
  memset(&s, 0, sizeof(s));
  if (!ai_contact_2820_prepare(ctx, nation_id, unit, &s)) {
    return 0;
  }
  ai_contact_local_rng(ctx, nation_id, &s.rng);
  int goods[3];
  int slots[3];
  const int n = ai_contact_2e92_candidates(&s, goods, slots);
  if (n <= 0) {
    return 0;
  }
  const int best = ai_contact_2e92_ai_pick(ctx, &s, e, n);
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id, unit);
  const int qty = ai_contact_2820_buy_qty(ctx, unit, 0);
  const int price =
    ai_contact_2e92_price(ctx, ind, nation_id, e, goods[best], (int)s.bid[slots[best]], qty, &s.rng);
  return ai_contact_2e92_settle(ctx, ind, t, nation_id, e, unit, goods[best], price, qty, &s.rng);
}

void ai_contact_apply_buywhich(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int unit_id, int cargo
) {
  if (!ctx || !ctx->ai_popups || !ctx->units || cargo < 0 || cargo > 15 || e < 0 || e > 3) {
    return;
  }
  ColonizeUnit* unit = units_get(ctx->units, unit_id);
  if (!unit) {
    return;
  }
  AiContact2820* s = &ai_contact_s_2820[e];
  if (!s->active || s->nation_id != nation_id) {
    /* Session lost (save/load mid-dialog): rebuild tables, qty 100 / 25. */
    memset(s, 0, sizeof(*s));
    if (!ai_contact_2820_prepare(ctx, nation_id, unit, s)) {
      return;
    }
    ai_contact_local_rng(ctx, nation_id, &s->rng);
    s->active = 1;
    s->nation_id = nation_id;
    s->unit_id = unit_id;
    s->buy_qty = ai_contact_2820_buy_qty(ctx, unit, 0);
  }
  s->buy_cargo = cargo;
  s->round = 0;
  /* `iStack_86 = aiStack_d6[iStack_5e]` — every bid read downstream of the
   * @BUYWHICH pick is bid[SORTED SLOT], not bid[cargo] (bugs.md #795).
   * The CHOICE id only carries the cargo, so recover the slot from the same
   * candidate walk that built the menu. */
  {
    int goods[3];
    int slots[3];
    const int n = ai_contact_2e92_candidates(s, goods, slots);
    s->buy_slot = cargo;
    for (int k = 0; k < n; ++k) {
      if (goods[k] == cargo) {
        s->buy_slot = slots[k];
        break;
      }
    }
  }
  const int price =
    ai_contact_2e92_price(ctx, ind, nation_id, e, cargo, (int)s->bid[s->buy_slot], s->buy_qty, &s->rng);
  s->price = price;
  ai_contact_enqueue_buy0(ctx, nation_id, e, unit, cargo, price, s->buy_qty, 0);
}

void ai_contact_apply_buy0(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int payload, int choice
) {
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return;
  }
  const int unit_id = payload & 0xffff;
  const int cargo = (payload >> 16) & 0xf;
  int price = (payload >> 20) & 0x7ff; /* 11 bits: 0..2047 */
  const int round = (payload >> 31) & 1;
  AiContact2820* s = &ai_contact_s_2820[e];
  const int have_state = s->active && s->nation_id == nation_id && s->buy_cargo == cargo;
  if (have_state) {
    price = s->price;
  }
  if (choice != 1 && choice != 2) {
    s->active = 0;
    return; /* Never mind */
  }
  ColonizeUnit* unit = units_get(ctx->units, unit_id);
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id, unit);
  if (!unit) {
    s->active = 0;
    return;
  }
  const int qty = have_state ? s->buy_qty : ai_contact_2820_buy_qty(ctx, unit, 0);
  ColonizeDosRng local;
  ColonizeDosRng* rng = have_state ? &s->rng : &local;
  if (!have_state) {
    ai_contact_local_rng(ctx, nation_id, &local);
  }
  if (choice == 2) {
    /* bid[SORTED SLOT] — `iStack_82 = RNG(0, bid[iStack_86]/25 + 8)`. */
    const int bid = have_state ? (int)s->bid[s->buy_slot & 15] : 0;
    int alarm_delta = 0;
    const int again = ai_contact_2e92_haggle((int)ctx->col1->head.difficulty, bid, rng, &price, &alarm_delta);
    if (alarm_delta) {
      ai_contact_alarm_delta_00f2(ctx, nation_id, e, alarm_delta);
    }
    const int met = ai_contact_2820_met_bit(ctx, e, nation_id);
    if (!again) {
      /* 2820 doc 419-425: the alarm +2 is UNGATED, but `tribe+7 = 0xfe` and
       * @BADHAGGLE2 both sit inside `if (8c28(...) & 0x40)` (bugs.md #805) —
       * the sticky refusal is not persistent state written outside its guard. */
      s->active = 0;
      if (met) {
        if (t) {
          t->sticky_trade_good = 0xfe; /* tribe+7 = 0xfe: refused outright */
        }
        PopupMsgTokens tok;
        memset(&tok, 0, sizeof(tok));
        tok.string0 = ai_contact_cargo_name(cargo);
        char body[AI_POPUP_BODY_LEN];
        popup_msg_fill(ctx->messages, "BADHAGGLE2", &tok, "", body, sizeof(body));
        ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
      }
      return;
    }
    /* 2820 doc 437-441: the walked-down price and its alarm roll stand, but
     * the re-ask (`iStack_88 = 1; iStack_6c = 1`) is inside the same guard —
     * without it the offer simply lapses. */
    if (!met) {
      s->active = 0;
      return;
    }
    s->price = price;
    s->round = round + 1;
    if (ctx->ai_popups) {
      ai_contact_enqueue_buy0(ctx, nation_id, e, unit, cargo, price, qty, round + 1);
    }
    return;
  }
  if (!ai_contact_2e92_settle(ctx, ind, t, nation_id, e, unit, cargo, price, qty, rng)) {
    /* @NOTENOUGH 0x15ae; FUN_1000_8f5c(…, 1, 0). */
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, 1);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.number0 = (int)europe_nation_gold(ctx->europe, ctx->col1, e); /* audit G3 */
    tok.has_number0 = true;
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "NOTENOUGH", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
  }
  s->active = 0;
}

/* @TRADE0 (round 0: accept / fairer / gift / never mind) or @TRADE1 (no gift). */
static int ai_contact_enqueue_trade_offer_round(
  ColonizeTurnContext* ctx, int nation_id, int e, const AiContact2820* s
) {
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = ai_contact_values_name(s->value_idx);
  tok.string1 = ai_contact_cargo_name(s->cargo);
  tok.number0 = s->price;
  tok.has_number0 = true;
  tok.number1 = s->fair;
  tok.has_number1 = true;
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(ctx->messages, s->round == 0 ? "TRADE0" : "TRADE1", &tok, "", body, sizeof(body));
  /* @TRADE0 rows: accept {N0} / fairer {N1} / gift / never mind; @TRADE1 has
   * no gift row. */
  const int four = s->round == 0;
  char rows[4][AI_POPUP_CHOICE_LEN] = {{0}, {0}, {0}, {0}};
  (void)ai_contact_choice_rows(
    ctx->messages, four ? "TRADE0" : "TRADE1", &tok, rows, four ? 4 : 3
  );
  const char* labels[4] = {rows[0], rows[1], rows[2], rows[3]};
  const int ids4[4] = {AI_CONTACT_TRADE_OFFER_ACCEPT, AI_CONTACT_TRADE_OFFER_HAGGLE,
                       AI_CONTACT_TRADE_OFFER_GIFT, AI_CONTACT_TRADE_OFFER_DECLINE};
  const char* labels3[3] = {rows[0], rows[1], rows[2]};
  const int ids3[3] = {AI_CONTACT_TRADE_OFFER_ACCEPT, AI_CONTACT_TRADE_OFFER_HAGGLE,
                       AI_CONTACT_TRADE_OFFER_DECLINE};
  if (!ai_popup_enqueue_choice_ctx(
        ctx->ai_popups, AI_POPUP_TAG_CONTACT_TRADE_OFFER, e, nation_id, s->price, NULL,
        body, four ? labels : labels3, four ? ids4 : ids3, four ? 4 : 3
      )) {
    return 0;
  }
  /* 2820 +606/+890/+1037: `FUN_291f_019c(…, BP-0xbe, *(0x8d52))` — the
   * @TRADE0/@TRADE1 haggle rounds are chief audiences. */
  ai_contact_chief_flair(ctx, e, nation_id);
  return 1;
}

/*
 * Post-pick dispatch (shell lines ~256-283 / 450-452 / 656-675):
 *   cargo < 0            → LAB_002e92 (@BRING for humans, nothing else)
 *   AI                   → LAB_002bbc: alarm > 0x31 ? gift : accept; then buy
 *   human, refused good  → @BADCARGO / @BADHAGGLE1
 *   human                → @TRADE0 CHOICE
 */
static void ai_contact_2820_dispatch(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit,
  AiContact2820* s
) {
  const int human = ai_contact_euro_is_human(ctx, e);
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id, unit);
  const int alarm = ai_diplo_indian_alarm(ctx->col1, nation_id, e); /* aiStack_d6[0] */
  if (s->cargo < 0) {
    ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, human);
    return;
  }
  if (!human) {
    ai_contact_2820_sell_price(
      ind, s->cargo, (int)s->ask[s->cargo], s->qty, (int)ctx->col1->head.difficulty, alarm, s
    );
    if (alarm > 0x31) {
      ai_contact_2820_gift_settle(ctx, ind, t, nation_id, e, unit, s);
    } else {
      ai_contact_2820_sell_settle(ctx, ind, t, nation_id, e, unit, s);
    }
    ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, 0);
    return;
  }
  const int lb = t ? (int)t->last_bought : 0xff;
  const int ls = t ? (int)t->last_sold : 0xff;
  if (lb == s->cargo || ls == s->cargo || s->ask[s->cargo] == 0) {
    /* @BADCARGO 0x1561: STRING0 = cargo, STRING1..3 = the three highest asks. */
    int wanted[3];
    ai_contact_2820_wanted(s, t, wanted);
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_cargo_name(s->cargo);
    tok.string1 = ai_contact_cargo_name(wanted[0]);
    tok.string2 = ai_contact_cargo_name(wanted[1]);
    tok.string3 = ai_contact_cargo_name(wanted[2]);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BADCARGO", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
    ai_contact_intel_note_buys(ctx, e, nation_id, unit, wanted);
    s->active = 0;
    return;
  }
  if (t && (int)t->sticky_trade_good == s->cargo) {
    /* @BADHAGGLE1 0x156a */
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string0 = ai_contact_cargo_name(s->cargo);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BADHAGGLE1", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
    s->active = 0;
    return;
  }
  ai_contact_2820_sell_price(
    ind, s->cargo, (int)s->ask[s->cargo], s->qty, (int)ctx->col1->head.difficulty, alarm, s
  );
  s->round = 0;
  if (!ctx->ai_popups || !ai_contact_enqueue_trade_offer_round(ctx, nation_id, e, s)) {
    s->active = 0;
  }
}

/*
 * Shell entry: tables + hold pick. Returns 1 when the trade ran or a CHOICE
 * was queued, 0 when nothing could be priced (no tribe / econ).
 */
static int ai_contact_2820_begin_slot(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit,
  int pick_slot
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !unit || e < 0 || e > 3 ||
      unit->nation_id != e) {
    return 0;
  }
  AiContact2820* s = &ai_contact_s_2820[e];
  memset(s, 0, sizeof(*s));
  if (!ai_contact_2820_prepare(ctx, nation_id, unit, s)) {
    return 0;
  }
  ai_contact_local_rng(ctx, nation_id, &s->rng);
  s->active = 1;
  s->nation_id = nation_id;
  s->unit_id = unit->id;
  s->cargo = -1;
  s->slot = 0;
  s->sold_ok = 1;
  const int holds = units_holds_used(ctx->units, unit->id);
  const int human = ai_contact_euro_is_human(ctx, e);
  if (pick_slot >= 0 && pick_slot < COLONIZE_UNIT_CARGO_MAX && unit->hold_goods_amount[pick_slot] > 0) {
    s->slot = pick_slot;
    s->cargo = unit->hold_goods_type[pick_slot];
    s->qty = unit->hold_goods_amount[pick_slot];
    ai_contact_2820_dispatch(ctx, ind, nation_id, e, unit, s);
    return 1;
  }
  if (holds > 1) {
    if (!human) {
      s->slot = dos_rng_range(&s->rng, 0, holds - 1);
    } else if (ctx->ai_popups) {
      /* DOS: one menu row per hold ("<qty> <name>"), 99 = cancel. */
      const char* labels[AI_POPUP_CHOICE_MAX];
      char text[AI_POPUP_CHOICE_MAX][AI_POPUP_CHOICE_LEN];
      int ids[AI_POPUP_CHOICE_MAX];
      int n = 0;
      for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX && n < AI_POPUP_CHOICE_MAX - 1; ++h) {
        if (unit->hold_goods_amount[h] <= 0) {
          continue;
        }
        snprintf(text[n], sizeof(text[n]), "%d %s", unit->hold_goods_amount[h],
                 ai_contact_cargo_name(unit->hold_goods_type[h]));
        labels[n] = text[n];
        ids[n] = h + 1;
        n++;
      }
      /* DOS `FUN_1000_8212(0x191f, *DS:0x2dfa, 99)` (2820 doc 242) — the
       * cancel row is LABELS @MISC[32] ((0x2dfa - 0x2dba) / 2 = 32) = the same
       * "Nothing" row game_loop_orders.c already resolves for the
       * foreign-colony @TRADEWHICH menu. Never typed here (bugs.md #800). */
      {
        const char* nothing_live = reports_labels_field("MISC", 32);
        snprintf(text[n], sizeof(text[n]), "%s", nothing_live ? nothing_live : "");
      }
      labels[n] = text[n];
      ids[n] = 99;
      n++;
      /* DOS asm 138911-138912 `LEA AX,[0x1556]` -> FUN_291f_0182: the SECOND
       * @TRADEWHICH tag (VICEROY.EXE 121248+0x1556 = "TRADEWHICH\0"), the same
       * GAME.TXT section the foreign-colony hold menu uses. It takes no
       * tokens. Was hardcoded English (bugs.md #800). */
      char body[AI_POPUP_BODY_LEN];
      popup_msg_fill(ctx->messages, "TRADEWHICH", NULL, "", body, sizeof(body));
      if (ai_popup_enqueue_choice_ctx(
            ctx->ai_popups, AI_POPUP_TAG_CONTACT_TRADE_PICK, e, nation_id, unit->id, NULL, body,
            labels, ids, n
          )) {
        /* 2820 +1436/+1439 (0x15ce/0x15da) — the hold menu is a 019c chief
         * audience too. */
        ai_contact_chief_flair(ctx, e, nation_id);
        return 1;
      }
    }
  }
  if (holds > 0) {
    /* Single hold (or AI pick): iStack_7e = first used slot / RNG slot. */
    int seen = 0;
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (unit->hold_goods_amount[h] <= 0) {
        continue;
      }
      if (seen == s->slot) {
        s->slot = h;
        s->cargo = unit->hold_goods_type[h];
        s->qty = unit->hold_goods_amount[h];
        break;
      }
      seen++;
    }
  }
  ai_contact_2820_dispatch(ctx, ind, nation_id, e, unit, s);
  return 1;
}

int ai_contact_2820_begin(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit
) {
  return ai_contact_2820_begin_slot(ctx, ind, nation_id, e, unit, -1);
}

/* Hold-pick CHOICE result (human, multi-hold unit). */
void ai_contact_apply_trade_pick(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int unit_id, int choice
) {
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return;
  }
  AiContact2820* s = &ai_contact_s_2820[e];
  if (!s->active || s->nation_id != nation_id || s->unit_id != unit_id) {
    return;
  }
  ColonizeUnit* unit = units_get(ctx->units, unit_id);
  if (!unit || choice < 1 || choice > COLONIZE_UNIT_CARGO_MAX) {
    s->active = 0; /* 99 / cancel → LAB_003582 */
    return;
  }
  const int h = choice - 1;
  if (unit->hold_goods_amount[h] <= 0) {
    s->active = 0;
    return;
  }
  s->slot = h;
  s->cargo = unit->hold_goods_type[h];
  s->qty = unit->hold_goods_amount[h];
  ai_contact_2820_dispatch(ctx, ind, nation_id, e, unit, s);
}

/*
 * AI-silent path (Brave-adjacency meet pulse — a Linux stand-in; DOS's own
 * 2820 callers are the village-enter arms). Scope kept from the earlier
 * stand-in so the pulse does not strip every AI cargo unit each turn: sells
 * only a TRADE_GOODS hold (full LAB_002bbc mechanics, whole hold, then the
 * LAB_002e92 buy), or buys when empty-handed. `forced_price` is ignored.
 * Returns 1 when a sale, gift or purchase happened.
 */
/*
 * AI wagon village-errand arrival (FUN_4d56_4528 AI arm, type 0xc → case 1
 * Trade → FUN_4d56_2820). Called from ai_euro's 20e6 village-errand walker
 * when an errand wagon reaches its village. Runs the real 2820 shell on the
 * AI-silent path (random hold pick, LAB_002bbc sell / gift by alarm) — the
 * same machine the human Trade row uses. 2820 itself clears the DOS errand
 * byte (+0x3158) for land types at entry; the caller owns the Linux latch.
 * Returns 1 when the trade shell ran (sold, gifted or priced-out), 0 when
 * the gates refused (no contact yet / bad record).
 */
int ai_contact_ai_wagon_village_trade(
  ColonizeTurnContext* ctx, int indian_nation, int euro_nation, int unit_id
) {
  if (!ctx || !ctx->units || !ctx->col1_ok || !ctx->col1 || indian_nation < 4 ||
      indian_nation > 11 || euro_nation < 0 || euro_nation > 3) {
    return 0;
  }
  ColonizeUnit* unit = units_get(ctx->units, unit_id);
  if (!unit || !unit->active || unit->nation_id != euro_nation) {
    return 0;
  }
  ColonizeCol1Indian* ind = &ctx->col1->indian[indian_nation - 4];
  /* DOS 4528 runs off a real tile entry, which implies contact; the port's
   * errand walker only reaches adjacency, so gate on the met bit rather than
   * inventing a first-contact side effect here. */
  if (!ind->euro_diplo[euro_nation]) {
    return 0;
  }
  return ai_contact_2820_begin(ctx, ind, indian_nation, euro_nation, unit);
}

int ai_contact_auto_trade(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, ColonizeUnit* unit
) {
  if (!ctx || !ctx->col1_ok || !ctx->col1 || !ind || !unit || unit->nation_id != e) {
    return 0;
  }
  if (!ind->euro_diplo[e]) {
    return 0;
  }
  if (ai_contact_euro_is_human(ctx, e)) {
    return ai_contact_2820_begin(ctx, ind, nation_id, e, unit);
  }
  int slot = -1;
  for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
    if (unit->hold_goods_type[h] == COLONIZE_CARGO_TRADE_GOODS && unit->hold_goods_amount[h] > 0) {
      slot = h;
      break;
    }
  }
  if (slot < 0) {
    /* bugs.md #798: DOS gates LAB_002e92 on `if (-1 < iStack_c8)` (2820 doc
     * 284) — an empty-handed unit cannot buy, it only ever gets @BRING. The
     * old `ai_contact_auto_buy_2e92` fallback here was invented. */
    return 0;
  }
  return ai_contact_2820_begin_slot(ctx, ind, nation_id, e, unit, slot);
}

/*
 * @TRADE0/@TRADE1 CHOICE result (LAB_002bbc human loop, iStack_5e):
 *   1 accept → sell_settle, then LAB_002e92
 *   2 fairer → haggle; raise → @TRADE1 again; exhausted → @BADHAGGLE0, no buy
 *   3 gift (round 0) → gift_settle, then LAB_002e92
 *   else → iStack_c6 = 0 (nothing)
 */
void ai_contact_apply_trade_offer(
  ColonizeTurnContext* ctx, ColonizeCol1Indian* ind, int nation_id, int e, int price, int choice
) {
  (void)price;
  if (!ctx || !ind || e < 0 || e > 3 || !ctx->units) {
    return;
  }
  AiContact2820* s = &ai_contact_s_2820[e];
  if (!s->active || s->nation_id != nation_id || s->cargo < 0) {
    return;
  }
  ColonizeUnit* unit = units_get(ctx->units, s->unit_id);
  ColonizeCol1Tribe* t = ai_contact_2e92_tribe(ctx, nation_id, unit);
  if (!unit || unit->hold_goods_amount[s->slot] <= 0 || unit->hold_goods_type[s->slot] != s->cargo) {
    s->active = 0;
    return;
  }
  if (choice == AI_CONTACT_TRADE_OFFER_HAGGLE) {
    if (ai_contact_2820_sell_haggle(
          (int)ctx->col1->head.difficulty, (int)s->ask[s->cargo], s->qty, &s->rng, &s->c4,
          &s->price, &s->fair
        )) {
      s->round++;
      (void)ai_contact_enqueue_trade_offer_round(ctx, nation_id, e, s);
      return;
    }
    /* Patience gone: tribe+7 = cargo, alarm += tier2/2 + 1, @BADHAGGLE0. */
    if (t) {
      t->sticky_trade_good = (uint8_t)s->cargo;
    }
    ai_contact_alarm_delta_00f2(ctx, nation_id, e, (s->tier2 >> 1) + 1);
    s->active = 0;
    /* 2820 doc 607-610: tribe+7 and the alarm bump are ungated; only the
     * @BADHAGGLE0 dialog sits behind `8c28(...) & 0x40` (bugs.md #805). */
    if (!ai_contact_2820_met_bit(ctx, e, nation_id)) {
      return;
    }
    PopupMsgTokens tok;
    memset(&tok, 0, sizeof(tok));
    tok.string1 = ai_contact_cargo_name(s->cargo);
    char body[AI_POPUP_BODY_LEN];
    popup_msg_fill(ctx->messages, "BADHAGGLE0", &tok, "", body, sizeof(body));
    ai_contact_human_chrome(ctx, e, AI_POPUP_TAG_CONTACT_REFUSE, nation_id, "", body);
    return;
  }
  if (choice == AI_CONTACT_TRADE_OFFER_GIFT && s->round == 0) {
    ai_contact_2820_gift_settle(ctx, ind, t, nation_id, e, unit, s);
    ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, 1);
    return;
  }
  if (choice != AI_CONTACT_TRADE_OFFER_ACCEPT) {
    s->active = 0;
    return;
  }
  ai_contact_2820_sell_settle(ctx, ind, t, nation_id, e, unit, s);
  ai_contact_2820_buy_phase(ctx, ind, nation_id, e, unit, s, 1);
}

/* Find Euro unit of nation e adjacent to a Brave of nation_id (meet/gift apply). */
ColonizeUnit* ai_contact_find_adjacent_euro(
  ColonizeTurnContext* ctx,
  int nation_id,
  int e,
  int* near_x,
  int* near_y
) {
  if (!ctx || !ctx->units || e < 0 || e > 3) {
    return NULL;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* brave = &ctx->units->units[i];
    if (!brave->active || brave->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(ctx->units, brave->id)) {
      continue;
    }
    for (int d = 0; d < 8; ++d) {
      const int oid = units_id_at(ctx->units, brave->x + MAP_DIR8_DX[d], brave->y + MAP_DIR8_DY[d]);
      if (oid < 0) {
        continue;
      }
      ColonizeUnit* other = units_get(ctx->units, oid);
      if (!other || other->nation_id != e) {
        continue;
      }
      if (near_x) {
        *near_x = brave->x;
      }
      if (near_y) {
        *near_y = brave->y;
      }
      return other;
    }
  }
  return NULL;
}
