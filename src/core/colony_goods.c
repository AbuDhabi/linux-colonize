#include "core/colony.h"

/*
 * Sections:
 *  - Warehouse capacity, spoilage & goods-full chrome popups (colonies_warehouse_capacity .. colonies_apply_warehouse_spoilage)
 *  - Cargo transfer to/from carrier units (colonies_transfer_to_unit .. colonies_transfer_from_unit_amount)
 *  - Foreign/Custom-House trade gate & trade-route stop servicing (colonies_ftrade_price_byte .. colonies_best_load_cargo)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/ai_euro.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/font.h"
#include "core/founding_fathers.h"
#include "core/ai_diplo.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/europe.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/unit_chrome.h"
#include "core/units_cargo.h"
#include "platform/diagnostics.h"

#include "core/colony_internal.h"

/* ===================== Warehouse capacity, spoilage & goods-full chrome popups (colonies_warehouse_capacity .. colonies_apply_warehouse_spoilage) ===================== */
int colonies_warehouse_capacity(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type
) {
  /* Audit CO-28: DOS takes no cargo argument (see below) and neither does
   * this body; the parameter stays only to keep the ~10 call sites legible. */
  (void)cargo_type;
  if (!colony) {
    return 0;
  }
  /*
   * FUN_15eb_0a50 (viceroy_unpacked.c 10043-10053): 100*(1+warehouse_level);
   * buildings raise the level. The DOS routine takes NO cargo argument — one
   * capacity for all sixteen goods, Food included.
   *
   * Smell audit #25: this used to answer 199 for Food, uncited and blind to the
   * warehouse level. DOS's Food exemption is not a *different* cap, it is the
   * absence of the three per-cargo rules, each of which skips index 0 outright:
   *   - EOT overflow/spoilage: FUN_364b_0688's `if (local_b6 != 0)` guard and
   *     the paired `if (local_b6 == 0) aiStack_e4[0] = 0` (viceroy 57806-57872,
   *     57332-57336) — food is never clamped to capacity, which is what lets a
   *     colony bank past 200 toward the new-colonist threshold;
   *   - the unload @WAREHOUSEFULL confirm: FUN_479b_0f60's
   *     `(cap < stock + amount) && (local_18 != 0)` (viceroy 77396-77404);
   *   - the colony-screen alert colour: FUN_2f2b_28d6's `if (local_80 != 0)`
   *     (viceroy 49118-49123).
   * Those three sites carry the exemption; this accessor must not.
   */
  int level = (int)colony->warehouse_level;
  if (pool) {
    int derived = 0;
    const int wh = colonies_building_row(pool, COLONY_BUILDING_WAREHOUSE);
    const int whe = colonies_building_row(pool, COLONY_BUILDING_WAREHOUSE_EXPANSION);
    if (wh >= 0 && colony->has_building[wh]) {
      derived = 1;
    }
    if (whe >= 0 && colony->has_building[whe]) {
      derived = 2;
    }
    if (derived > level) {
      level = derived;
    }
  }
  if (level < 0) {
    level = 0;
  }
  if (level > 2) {
    level = 2;
  }
  return 100 * (1 + level);
}

void colonies_emit_warehouse_full_chrome(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int cargo_type,
  const char* cargo_name,
  int deposited,
  int already_included,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  /* FUN_479b_0f60 (viceroy_unpacked.c 77396): the @WAREHOUSEFULL confirm is
   * gated `(cap < stock + amount) && (local_18 != 0)` — Food never warns,
   * because food over capacity is the new-colonist rule, not spoilage. */
  if (cargo_type == COLONIZE_CARGO_FOOD) {
    return;
  }
  const int cap = colonies_warehouse_capacity(pool, colony, cargo_type);
  /* bugs.md #433: NUMBER0 is the pre-deposit stock; back out whatever part of
   * this deposit has already landed in stock[]. */
  int stock = colony->stock[cargo_type] - (already_included > 0 ? already_included : 0);
  if (stock < 0) {
    stock = 0;
  }
  const char* cname = colony->name[0] ? colony->name : "colony";
  const char* gname = (cargo_name && cargo_name[0]) ? cargo_name : "cargo";
  char body[AI_POPUP_BODY_LEN];
  char fallback[160];
  snprintf(
    fallback,
    sizeof(fallback),
    "Warehouse full at %s (%d/%d %s).",
    cname,
    stock,
    cap,
    gname
  );
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  tok.string1 = gname;
  tok.number0 = stock;
  tok.has_number0 = true;
  tok.number1 = cap;
  tok.has_number1 = true;
  tok.number2 = deposited > 0 ? deposited : 0;
  tok.has_number2 = true;
  popup_msg_fill(messages, "WAREHOUSEFULL", &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_full_chrome(
  const ColonizeColony* colony,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  const char* cname = colony->name[0] ? colony->name : "colony";
  char body[AI_POPUP_BODY_LEN];
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  popup_msg_fill(messages, "FULL", &tok, "", body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_already_have_chrome(
  const ColonizeColony* colony,
  const char* building_name,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  const char* cname = colony->name[0] ? colony->name : "colony";
  const char* bname =
    (building_name && building_name[0]) ? building_name : "building";
  const bool warehouse_exp = (colonies_building_name_row(bname) == COLONY_BUILDING_WAREHOUSE_EXPANSION);
  const char* section = warehouse_exp ? "NOMOREWAREHOUSE" : "ALREADYHAVE";
  char body[AI_POPUP_BODY_LEN];
  char fallback[192];
  if (warehouse_exp) {
    snprintf(
      fallback,
      sizeof(fallback),
      "%s cannot build another Warehouse Expansion.",
      cname
    );
  } else {
    snprintf(
      fallback,
      sizeof(fallback),
      "%s already built a %s.",
      cname,
      bname
    );
  }
  PopupMsgTokens tok;
  memset(&tok, 0, sizeof(tok));
  tok.string0 = cname;
  tok.string1 = bname;
  popup_msg_fill(messages, section, &tok, fallback, body, sizeof(body));
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_emit_more_than_three_chrome(
  const ColonizeColony* colony,
  AiPopupState* ai_popups,
  const ColonizeMsgCatalog* messages
) {
  if (!ai_popups || !colony || !colony->active) {
    return;
  }
  char body[AI_POPUP_BODY_LEN];
  popup_msg_fill(
    messages, "MORETHANTHREE", NULL,
    "", body, sizeof(body)
  );
  ai_popup_enqueue_ok(ai_popups, AI_POPUP_TAG_INFO, NULL, body);
}

void colonies_specialty_cargo_update(
  const ColonizeColonyPool* pool,
  ColonizeColony* colony,
  int cargo_type,
  int want_set,
  int already_produced
) {
  if (!colony || !colony->active || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return;
  }
  const int cap = colonies_warehouse_capacity(pool, colony, cargo_type);
  /* FUN_5952_0306: stock >= warehouse cap → do not set / clear match. */
  if (cap > 0 && cap <= colony->stock[cargo_type]) {
    want_set = 0;
  }
  /* FUN_5952_0306's second clear, raw: `if (*(int *)(cargo * 2 + -0x7238) !=
   * 0) want = 0` — DS:0x8dc8, the tick's GROSS PRODUCTION scratch ledger. A
   * colony that already makes the cargo never asks to be sent it. This
   * parameter used to be an invented `boycotted`, which no DOS reader of
   * +0x8d has; corrected 2026-09-18 with the real callee. */
  if (already_produced) {
    want_set = 0;
  }
  if (want_set) {
    colony->specialty_cargo = (uint8_t)cargo_type;
    return;
  }
  if (colony->specialty_cargo == (uint8_t)cargo_type) {
    colony->specialty_cargo = 0xff;
  }
}

int colonies_apply_warehouse_spoilage(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  const int* stock_before,
  int* out_first_cargo,
  int* out_type_count
) {
  if (out_first_cargo) {
    *out_first_cargo = -1;
  }
  if (out_type_count) {
    *out_type_count = 0;
  }
  if (!colony || !colony->active) {
    return 0;
  }
  int spoiled = 0;
  int types = 0;
  /* c starts at 1: FUN_364b_0688's loop skips cargo 0 (Food) entirely — food
   * over capacity is the new-colonist rule, never warehouse spoilage. */
  for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
    const int cap = colonies_warehouse_capacity(pool, colony, c);
    if (cap <= 0) {
      continue;
    }
    if (colony->stock[c] <= cap) {
      continue;
    }
    /*
     * DOS reports a loss only for the part of the overflow that was already
     * there *before* this turn's production: `if (production < overflow)`.
     * Overflow caused purely by production is a silent clamp to capacity with
     * no @SPOIL message. A reported loss below 2 tons is also dropped
     * (`if (local_74 < 2) local_74 = 0`).
     *
     * DOS-LITERAL FUN_364b_0688 raw 57847-57868. The DOS branch test is
     * `aiStack_e4[c] < overflow`, i.e. (stock_now - before) < (stock_now -
     * cap), i.e. `before > cap`. In that branch DOS first backs this turn's
     * production off the stock (`stock -= aiStack_e4[c]`, leaving `before`)
     * and only then subtracts local_74 — so when local_74 is zeroed by the
     * `< 2` rule the stock is left at `before`, NOT at cap. A single ton of
     * pre-existing overflow therefore sits in the warehouse forever: DOS
     * discards only above capacity, and only 2 tons or more.
     */
    const int before = stock_before ? stock_before[c] : colony->stock[c];
    if (before <= cap) {
      /* Overflow is entirely this turn's production: silent clamp. */
      colony->stock[c] = cap;
      continue;
    }
    int lost = before - cap;
    if (lost < 2) {
      lost = 0;
    }
    if (lost > 0) {
      if (out_first_cargo && *out_first_cargo < 0) {
        *out_first_cargo = c;
      }
      types++;
      spoiled += lost;
    }
    colony->stock[c] = before - lost;
  }
  if (out_type_count) {
    *out_type_count = types;
  }
  return spoiled;
}

/* ===================== Cargo transfer to/from carrier units (colonies_transfer_to_unit .. colonies_transfer_from_unit_amount) ===================== */
int colonies_transfer_to_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int cargo_type,
  int amount
) {
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units || cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT || amount <= 0) {
    return 0;
  }
  if (col->stock[cargo_type] < amount) {
    amount = col->stock[cargo_type];
  }
  if (amount <= 0) {
    return 0;
  }
  const int loaded = units_load_goods(units, unit_id, cargo_type, amount);
  if (loaded > 0) {
    col->stock[cargo_type] -= loaded;
    diag_info(
      "CARGO %s -> unit %d: cargo %d x%d (colony stock %d)",
      col->name[0] ? col->name : "colony", unit_id, cargo_type, loaded,
      col->stock[cargo_type]
    );
  }
  return loaded;
}

int colonies_transfer_from_unit(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  bool* out_warehouse_full
) {
  if (out_warehouse_full) {
    *out_warehouse_full = false;
  }
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units) {
    return 0;
  }
  int ctype = -1;
  int amt = 0;
  /* Peek without clearing — unload helper clears; reload remainder if capped. */
  const ColonizeUnit* u = units_get_const(units, unit_id);
  if (!u) {
    return 0;
  }
  const int n = units_goods_hold_count(units, unit_id);
  if (hold_index < 0 || hold_index >= n) {
    return 0;
  }
  amt = u->hold_goods_amount[hold_index];
  ctype = u->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255 || ctype < 0 || ctype >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  /*
   * bugs.md: unloading past warehouse capacity is ALLOWED — the whole hold
   * goes ashore, the excess spoils at next turn's warehouse pass, and the
   * over-capacity stock number is drawn in the alert colour. The flag only
   * informs the caller (status/popup), it no longer blocks or splits the
   * unload.
   */
  const int cap = colonies_warehouse_capacity(pool, col, ctype);
  const int move = amt;
  int got_type = 0;
  int got_amt = 0;
  if (units_unload_goods_hold(units, unit_id, hold_index, &got_type, &got_amt) <= 0) {
    return 0;
  }
  col->stock[ctype] += move;
  /* Col1 +0x8f: goods unload clears cargo_idle_turns (decomp ~90249). */
  if (move > 0) {
    col->cargo_idle_turns = 0;
    diag_info(
      "CARGO unit %d -> %s: cargo %d x%d (colony stock %d, cap %d)",
      unit_id, col->name[0] ? col->name : "colony", ctype, move, col->stock[ctype], cap
    );
  }
  /* FUN_479b_0f60 (viceroy 77396) gates the full-warehouse confirm on
   * `local_18 != 0` — cargo 0 (Food) never raises it. */
  if (out_warehouse_full && ctype != COLONIZE_CARGO_FOOD && cap > 0 &&
      col->stock[ctype] > cap) {
    *out_warehouse_full = true;
  }
  return move;
}

int colonies_transfer_from_unit_amount(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  int hold_index,
  int amount,
  bool* out_warehouse_full
) {
  if (out_warehouse_full) {
    *out_warehouse_full = false;
  }
  if (amount <= 0) {
    return 0;
  }
  ColonizeColony* col = colonies_get_mut(pool, colony_id);
  if (!col || !units) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u) {
    return 0;
  }
  const int n = units_goods_hold_count(units, unit_id);
  if (hold_index < 0 || hold_index >= n) {
    return 0;
  }
  const int held = u->hold_goods_amount[hold_index];
  const int ctype = u->hold_goods_type[hold_index];
  if (held <= 0 || held >= 255 || ctype < 0 || ctype >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  if (amount >= held) {
    /* Whole hold: reuse the full-unload path (hold clear + bookkeeping). */
    return colonies_transfer_from_unit(pool, colony_id, units, unit_id, hold_index, out_warehouse_full);
  }
  u->hold_goods_amount[hold_index] = (uint8_t)(held - amount);
  const int cap = colonies_warehouse_capacity(pool, col, ctype);
  col->stock[ctype] += amount;
  col->cargo_idle_turns = 0;
  /* FUN_479b_0f60 (viceroy 77396) gates the full-warehouse confirm on
   * `local_18 != 0` — cargo 0 (Food) never raises it. */
  if (out_warehouse_full && ctype != COLONIZE_CARGO_FOOD && cap > 0 &&
      col->stock[ctype] > cap) {
    *out_warehouse_full = true;
  }
  return amount;
}

/* ===== Foreign-colony trade (FUN_5f7a_020e) — docs/foreign_colony_trade.md ===== */

/*
 * DS:0x84bc[nation*0x10 + cargo]. Every writer of that byte stores
 * `nation[n].trade.euro_price[cargo] - 1` clamped at 0 (viceroy_unpacked.c
 * 6316-6320 / 51962-51966 / 58996-59000) — the same derivation europe.c's
 * dump-sell arm cites.
 */
/* ===================== Foreign/Custom-House trade gate & trade-route stop servicing (colonies_ftrade_price_byte .. colonies_best_load_cargo) ===================== */
static int colonies_ftrade_price_byte(const ColonizeCol1Save* col1, int nation, int cargo) {
  if (!col1 || nation < 0 || nation >= (int)COLONIZE_COL1_NATION_COUNT || cargo < 0 ||
      cargo >= (int)COLONIZE_COL1_CARGO_TYPES) {
    return 0;
  }
  const int p = (int)col1->nation[nation].trade.euro_price[cargo] - 1;
  return p > 0 ? p : 0;
}

/* The two units the trade dialog needs, plus the DOS nation ids. */
static int colonies_ftrade_bind(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id,
  const ColonizeColony** out_col,
  const ColonizeUnit** out_unit
) {
  if (!w || !w->colonies || !w->units || !w->col1_ok || !w->col1) {
    return 0;
  }
  const ColonizeColony* col = colonies_get(w->colonies, foreign_colony_id);
  const ColonizeUnit* u = units_get_const(w->units, unit_id);
  if (!col || !col->active || !u || !u->active) {
    return 0;
  }
  if (col->nation_id < 0 || col->nation_id > 3) {
    return 0;
  }
  const int actor = u->nation_id;
  /* raw 98915-98921: nibble > 3 (natives) leaves without a word. */
  if (actor < 0 || actor > 3 || actor == col->nation_id) {
    return 0;
  }
  *out_col = col;
  *out_unit = u;
  return 1;
}

ColonizeForeignTradeGate colonies_foreign_trade_gate(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id
) {
  const ColonizeColony* col = NULL;
  const ColonizeUnit* u = NULL;
  if (!colonies_ftrade_bind(w, foreign_colony_id, unit_id, &col, &u)) {
    return COLONIZE_FTRADE_NONE;
  }
  const ColonizeCol1Save* col1 = w->col1;
  const int actor = u->nation_id;
  /*
   * raw 98921-98923: `player[actor].control != 0` returns before any dialog.
   * The AI never trades with a foreign colony in DOS — FF 4 is read in exactly
   * two places in VICEROY, here and the Foreign Affairs report.
   */
  if (col1->player[actor].control != 0) {
    return COLONIZE_FTRADE_NONE;
  }
  /* raw 98924-98927: FUN_281f_0a38(actor, owner) & 0x40 = peace treaty. */
  if ((ai_diplo_read(col1, actor, col->nation_id) & AI_DIPLO_PEACE) == 0) {
    return COLONIZE_FTRADE_ATWAR;
  }
  /* raw 98928-98934: FUN_281f_07b4(actor, 4) = Jan de Witt. */
  if (!founding_fathers_de_witt_allows_foreign_colony_trade(col1, actor)) {
    return COLONIZE_FTRADE_MERCANTILISM;
  }
  /* raw 98935-98936: unit +0x3150 (goods holds occupied). */
  if (units_holds_used(w->units, unit_id) <= 0) {
    return COLONIZE_FTRADE_NOCARGO;
  }
  return COLONIZE_FTRADE_OK;
}

int colonies_foreign_trade_prepare(
  const ColonizeWorld* w,
  ColonizeDosRng* rng,
  int foreign_colony_id,
  int unit_id,
  int hold_index,
  ColonizeForeignTradeDeal* out
) {
  if (!out) {
    return 0;
  }
  memset(out, 0, sizeof(*out));
  out->offer_cargo = -1;
  if (colonies_foreign_trade_gate(w, foreign_colony_id, unit_id) != COLONIZE_FTRADE_OK) {
    return 0;
  }
  const ColonizeColony* col = NULL;
  const ColonizeUnit* u = NULL;
  if (!colonies_ftrade_bind(w, foreign_colony_id, unit_id, &col, &u)) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const ColonizeCol1Save* col1 = w->col1;
  const int actor = u->nation_id;
  const int owner = col->nation_id;
  const int sold = u->hold_goods_type[hold_index];
  const int qty = units_hold_amount(w->units, unit_id, hold_index);
  if (qty <= 0 || sold < 0 || sold >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  const bool woi = col1->head.game_options.woi;
  const int ally = (int)col1->head.rival_nation_slot_1; /* DS:0x53d4 */
  const int difficulty = (int)col1->head.difficulty;   /* DS:0x53a6 */

  /* raw 98963-98968: gross, then the owner's tax withheld. */
  const int gross = colonies_ftrade_price_byte(col1, owner, sold) * qty;
  int gold = europe_net_after_tax(gross, (int)col1->nation[owner].tax_rate);
  /* raw 98969-98972: at war (rel & 2) outside the WoI — half. */
  if (!woi && (ai_diplo_read(col1, actor, owner) & AI_DIPLO_WAR) != 0) {
    gold >>= 1;
  }
  /* raw 98973-98983: haggle. The WoI intervention ally shaves only 5+d %. */
  if (!woi || owner != ally) {
    const int pct = rng ? dos_rng_range(rng, 10, (difficulty + 1) * 12) : 10;
    gold += (pct * gold) / -100;
  } else {
    gold += ((-5 - difficulty) * gold) / 100;
  }
  if (gold < 1) {
    gold = 1; /* raw 98984-98986 */
  }

  /* raw 98987-99012: best counter-offer out of the colony's warehouse. */
  int best_val = -1;
  int best_cargo = -1;
  int best_qty = 0;
  for (int c = 0; c < 0x10 && c < COLONIZE_CARGO_COUNT; ++c) {
    if (!woi && (c == 0x0f || c == 0x0e || c == 0 || c == 5)) {
      continue; /* Muskets / Tools / Food / Lumber are off the table at peace */
    }
    if (c == sold) {
      continue;
    }
    int avail = (int)col->stock[c];
    if (avail > 100) {
      avail = 100;
    }
    if (woi && (sold == 0x0f || sold == 8)) {
      if (owner == ally) {
        avail = 100;
      } else if (avail < 0x32) {
        avail = 0x32;
      }
    }
    const int unit_price = colonies_ftrade_price_byte(col1, owner, c);
    int val = unit_price * avail;
    while (val > gross) {
      avail--;
      val -= unit_price;
    }
    if (avail > 0 && val > best_val) {
      best_val = val;
      best_qty = avail;
      best_cargo = c;
    }
  }

  out->sold_cargo = sold;
  out->sold_qty = qty;
  out->gold = gold;
  out->offer_cargo = best_val >= 0 ? best_cargo : -1;
  out->offer_qty = best_val >= 0 ? best_qty : 0;
  return 1;
}

int colonies_foreign_trade_apply(
  const ColonizeWorld* w,
  int foreign_colony_id,
  int unit_id,
  int hold_index,
  const ColonizeForeignTradeDeal* deal,
  int take_goods
) {
  if (!deal || !w || !w->colonies || !w->units || !w->col1_ok || !w->col1) {
    return 0;
  }
  ColonizeColony* col = colonies_get_mut(w->colonies, foreign_colony_id);
  ColonizeUnit* u = units_get(w->units, unit_id);
  if (!col || !col->active || !u || !u->active) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  if (deal->sold_cargo < 0 || deal->sold_cargo >= COLONIZE_CARGO_COUNT) {
    return 0;
  }
  if (take_goods) {
    if (deal->offer_cargo < 0 || deal->offer_cargo >= COLONIZE_CARGO_COUNT) {
      return 0;
    }
    /* raw 99019-99021: FUN_281f_0cea/0ca4 overwrite the hold in place. */
    u->hold_goods_type[hold_index] = deal->offer_cargo;
    u->hold_goods_amount[hold_index] = deal->offer_qty;
  } else {
    /* raw 99022-99030: FUN_281f_0aec empties the hold, then gold to the purse. */
    (void)units_unload_goods_hold(w->units, unit_id, hold_index, NULL, NULL);
    europe_nation_gold_add(NULL, w->col1, u->nation_id, (long)deal->gold);
  }
  /*
   * raw 99031-99033: the colony gains what it bought. DOS does NOT debit the
   * stock of the cargo it hands over in the barter arm — transcribed, not fixed.
   */
  col->stock[deal->sold_cargo] += deal->sold_qty;
  col->cargo_idle_turns = 0;
  return 1;
}


static int colonies_trade_surplus_load_amount(const ColonizeColony* c, int ct) {
  int amt = 20;
  if (ct == COLONIZE_CARGO_MUSKETS || ct == COLONIZE_CARGO_HORSES) {
    amt = 10;
  }
  if (ct == COLONIZE_CARGO_FOOD) {
    amt = c->population > 0 ? c->population * 2 : 10;
  }
  return amt;
}

void colonies_trade_stop_set_cargos(
  ColonizeCol1TradeStop* stop,
  const int* unload_types,
  int unload_n,
  const int* load_types,
  int load_n
) {
  if (!stop) {
    return;
  }
  stop->unload_count = 0;
  stop->load_count = 0;
  memset(stop->unload_cargo_nibbles, 0, sizeof(stop->unload_cargo_nibbles));
  memset(stop->load_cargo_nibbles, 0, sizeof(stop->load_cargo_nibbles));
  int uc = 0;
  if (unload_types && unload_n > 0) {
    int seen[COLONIZE_CARGO_COUNT];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < unload_n && uc < 6; ++i) {
      const int ct = unload_types[i];
      if (ct < 0 || ct >= COLONIZE_CARGO_COUNT || seen[ct]) {
        continue;
      }
      seen[ct] = 1;
      col1_trade_nibble_set(stop->unload_cargo_nibbles, uc, ct);
      uc++;
    }
  }
  stop->unload_count = (uint8_t)uc;
  int lc = 0;
  if (load_types && load_n > 0) {
    int seen[COLONIZE_CARGO_COUNT];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < load_n && lc < 6; ++i) {
      const int ct = load_types[i];
      if (ct < 0 || ct >= COLONIZE_CARGO_COUNT || seen[ct]) {
        continue;
      }
      seen[ct] = 1;
      col1_trade_nibble_set(stop->load_cargo_nibbles, lc, ct);
      lc++;
    }
  }
  stop->load_count = (uint8_t)lc;
}

void colonies_trade_stop_autofill(
  ColonizeCol1TradeStop* stop,
  const ColonizeColony* colony,
  const ColonizeUnitPool* units,
  int unit_id
) {
  if (!stop) {
    return;
  }
  stop->unload_count = 0;
  stop->load_count = 0;
  memset(stop->unload_cargo_nibbles, 0, sizeof(stop->unload_cargo_nibbles));
  memset(stop->load_cargo_nibbles, 0, sizeof(stop->load_cargo_nibbles));

  if (units && unit_id >= 0) {
    const ColonizeUnit* u = units_get_const(units, unit_id);
    if (u) {
      int seen[COLONIZE_CARGO_COUNT];
      memset(seen, 0, sizeof(seen));
      int uc = 0;
      const int n = units_goods_hold_count(units, unit_id);
      for (int h = 0; h < n && uc < 6; ++h) {
        const int ct = u->hold_goods_type[h];
        const int amt = u->hold_goods_amount[h];
        if (amt <= 0 || amt >= 255 || ct < 0 || ct >= COLONIZE_CARGO_COUNT || seen[ct]) {
          continue;
        }
        seen[ct] = 1;
        col1_trade_nibble_set(stop->unload_cargo_nibbles, uc, ct);
        uc++;
      }
      stop->unload_count = (uint8_t)uc;
    }
  }

  if (!colony) {
    return; /* Europe: sell path; no load list */
  }
  static const int k_load[] = {
    COLONIZE_CARGO_TOOLS,
    COLONIZE_CARGO_LUMBER,
    COLONIZE_CARGO_ORE,
    COLONIZE_CARGO_MUSKETS,
    COLONIZE_CARGO_HORSES,
    COLONIZE_CARGO_FOOD
  };
  int lc = 0;
  for (size_t i = 0; i < sizeof(k_load) / sizeof(k_load[0]) && lc < 6; ++i) {
    const int ct = k_load[i];
    const int amt = colonies_trade_surplus_load_amount(colony, ct);
    if (colony->stock[ct] < amt * 2) {
      continue;
    }
    col1_trade_nibble_set(stop->load_cargo_nibbles, lc, ct);
    lc++;
  }
  stop->load_count = (uint8_t)lc;
}

int colonies_trade_route_service_stop(
  ColonizeColonyPool* pool,
  int colony_id,
  ColonizeUnitPool* units,
  int unit_id,
  const ColonizeCol1TradeStop* stop
) {
  if (!pool || !units || !stop) {
    return 0;
  }
  ColonizeColony* cmut = colonies_get_mut(pool, colony_id);
  ColonizeUnit* u = units_get(units, unit_id);
  if (!cmut || !u) {
    return 0;
  }

  int moved = 0;
  /*
   * Unload phase (DOS FUN_479b_0bd0): exactly the stop's unload-list cargos,
   * every matching hold, into the warehouse. No list → unload nothing.
   */
  const int unload_n = (int)stop->unload_count;
  for (int i = 0; i < unload_n && i < 6; ++i) {
    const int want = col1_trade_nibble_cargo(stop->unload_cargo_nibbles, i);
    if (want < 0 || want >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    /* Re-scan holds each pass — unload may compact. */
    for (int guard = 0; guard < COLONIZE_UNIT_CARGO_MAX; ++guard) {
      const int n = units_goods_hold_count(units, unit_id);
      int found = -1;
      for (int h = 0; h < n; ++h) {
        if (u->hold_goods_amount[h] > 0 && u->hold_goods_amount[h] < 255 &&
            u->hold_goods_type[h] == want) {
          found = h;
          break;
        }
      }
      if (found < 0) {
        break;
      }
      bool full = false;
      if (colonies_transfer_from_unit(pool, colony_id, units, unit_id, found, &full) > 0) {
        moved = 1;
      } else {
        break;
      }
    }
  }

  /*
   * Load phase (DOS: sort load-list cargos by weight×stock, take the best,
   * load one hold, repeat until the transport is full or nothing is left).
   * Port keeps the greedy shape with uniform weights: highest stock first.
   */
  const int load_n = (int)stop->load_count;
  for (int guard = 0; guard < COLONIZE_UNIT_CARGO_MAX + 2; ++guard) {
    int best = -1;
    int best_stock = 0;
    for (int i = 0; i < load_n && i < 6; ++i) {
      const int ct = col1_trade_nibble_cargo(stop->load_cargo_nibbles, i);
      if (ct < 0 || ct >= COLONIZE_CARGO_COUNT) {
        continue;
      }
      if (cmut->stock[ct] > best_stock) {
        best_stock = cmut->stock[ct];
        best = ct;
      }
    }
    if (best < 0) {
      break;
    }
    const int amt = best_stock > 100 ? 100 : best_stock;
    if (colonies_transfer_to_unit(pool, colony_id, units, unit_id, best, amt) <= 0) {
      break;
    }
    moved = 1;
  }
  return moved;
}

int colonies_best_load_cargo(const ColonizeColony* colony) {
  if (!colony) {
    return -1;
  }
  /* Rough Europe bid ranking; exclude horses/tools/muskets (manual L-key). */
  static const int k_value[COLONIZE_CARGO_COUNT] = {
    1,  /* food */
    5,  /* sugar */
    4,  /* tobacco */
    3,  /* cotton */
    5,  /* furs */
    0,  /* lumber — rarely sold */
    4,  /* ore */
    20, /* silver */
    0,  /* horses — excluded */
    8,  /* rum */
    8,  /* cigars */
    7,  /* cloth */
    7,  /* coats */
    2,  /* trade goods */
    0,  /* tools — excluded */
    0   /* muskets — excluded */
  };
  int best = -1;
  int best_v = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    if (k_value[c] <= 0 || colony->stock[c] <= 0) {
      continue;
    }
    if (k_value[c] > best_v || (k_value[c] == best_v && colony->stock[c] > colony->stock[best])) {
      best_v = k_value[c];
      best = c;
    }
  }
  return best;
}

