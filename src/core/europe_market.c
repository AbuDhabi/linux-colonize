/*
 * Sections:
 *  - Labels, sell/buy commits & Custom House autosell (europe_set_labels .. europe_buy_cargo_w)
 *  - Icon flow, hit-testing, menus & cheats (europe_best_sell_hold .. europe_passenger_icon_sprite)
 */

#include "core/europe.h"
#include "core/europe_art.h"
#include "core/europe_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai_popup.h"
#include "core/assets.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/founding_fathers.h"
#include "core/popup_msg.h"
#include "core/reports.h"
#include "core/reports_names.h"
#include "platform/diagnostics.h"
#include "core/ss.h"
#include "core/strutil.h"
#include "core/ui_button.h"
#include "core/units.h"
#include "platform/platform.h"

/* Forward declarations for this file's own statics (the split moved
   section order; these keep every call site legal). */
static const char* europe_label(
  const EuropeScreen* eu,
  const char* section,
  int idx,
  const char* fallback
);
static void europe_push_sale_status(EuropeScreen* eu, int cargo_type, int amount, int net);
static int europe_sell_commit(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int* hold_type,
  int* hold_amount,
  int amt
);
static int europe_custom_house_cargo_eligible(int cargo_type);
static int europe_custom_house_bit_enabled(uint16_t bits, int cargo_type);
static bool europe_in_rect(int mx, int my, int x, int y, int w, int h);
static int europe_ship_icon_sprite(const ColonizeUnitPool* units, const EuropeHarborShip* ship);
static int europe_transit_ship_at(
  const EuropeHarborShip* ships,
  int count,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int box_x,
  int box_y,
  int box_w,
  int box_h,
  int transit_line_h,
  int mx,
  int my
);
static const char* europe_menu_name(EuropeMenu menu);
static bool europe_dock_menu_apply_selection(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id
);

/* ===================== Labels, sell/buy commits & Custom House autosell (europe_set_labels .. europe_buy_cargo_w) ===================== */
void europe_set_labels(EuropeScreen* eu, const struct ColonizeMsgCatalog* labels) {
  if (eu) {
    eu->labels = labels;
  }
}

void europe_set_messages(EuropeScreen* eu, const struct ColonizeMsgCatalog* game_txt) {
  if (eu) {
    eu->messages = game_txt;
  }
}

/* LABELS.TXT line, or the built-in fallback when no catalog is bound. */
static const char* europe_label(
  const EuropeScreen* eu,
  const char* section,
  int idx,
  const char* fallback
) {
  if (eu && eu->labels && section) {
    const ColonizeMsgSection* sec = assets_msg_find(eu->labels, section);
    if (sec && idx >= 0 && idx < sec->line_count && sec->lines[idx][0]) {
      return sec->lines[idx];
    }
  }
  return fallback;
}

static void europe_push_sale_status(EuropeScreen* eu, int cargo_type, int amount, int net) {
  if (!eu || amount <= 0 || cargo_type < 0 || cargo_type >= eu->cargo_count) {
    return;
  }
  if (eu->bar_event_count >= EUROPE_BAR_EVENT_MAX) {
    return;
  }
  /*
   * FUN_38fd_23c4 tail, field for field: amount, cargo name, @CMESSAGE 1
   * "sold for", gross, DS:0xfef ".", tax rate, @CMESSAGE 0x11 "% Tax:", tax
   * paid, @CMESSAGE 0x12 ". Net:", net. The 0088 calls between fields only
   * trim the space each append leaves, so the punctuation closes up. DOS
   * writes the tax half unconditionally here (Europe trade is shut once
   * independence is declared, so the rate is never 0 in practice).
   */
  const int gross = europe_sell_price(eu, cargo_type) * amount;
  const int tax_paid = gross - net;
  const char* cname =
    eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "";
  snprintf(
    eu->bar_event[eu->bar_event_count],
    EUROPE_BAR_EVENT_LEN,
    "%d %s %s %d. %d%s %d%s %d",
    amount,
    cname,
    europe_label(eu, "CMESSAGE", 1, ""),
    gross,
    eu->tax_percent,
    europe_label(eu, "CMESSAGE", 0x11, ""),
    tax_paid < 0 ? 0 : tax_paid,
    europe_label(eu, "CMESSAGE", 0x12, ""),
    net
  );
  eu->bar_event_count++;
}

/*
 * ---------------------------------------------------------------------
 * The one Europe sell pipeline (audit SC-16 / SC-17).
 *
 * europe_sell_hold, europe_sell_hold_partial and europe_sell_unit_hold each
 * wrote this sequence out in full, with three separately-maintained copies
 * of the same smell-audit comment block. The order below is DOS's and is
 * load-bearing:
 *
 *   1. Boycott gate on the SELLER's own nation+0x20 word, not the
 *      Europe-screen render mirror (smell audit G6): an EOT trade-route
 *      unload and the map/transport dump-sell both run with no screen in
 *      sight, and the AI borrow path swaps eu->gold for the AI nation.
 *   2. europe_sell_proceeds, then europe_purse_move — which credits the
 *      seller's own record when the purse is theirs (smell audit G3).
 *   3. europe_credit_sale_tax: DOS `nation+0x22 += tax` on every sale
 *      (smell audit #51), seller = the hold's owner.
 *   4. Clear the hold. Partial sales subtract and only reset the type word
 *      once the hold empties — the empty-hold sentinel is amount 0 / type 0
 *      everywhere else in the port, and 255 here used to be the odd one out
 *      (smell audit #64). A full sale is the amt == held case of the same
 *      rule, so all three callers share it.
 *   5. europe_push_sale_status BEFORE the volume move, so the printed gross
 *      is the bid the sale actually went through at (bugs.md #376).
 *   6. europe_apply_trade_volume: the 1dfa ledger (tons/tons2/gold) so the
 *      human's own trading feeds the long-run price pool (smell audit #50).
 *
 * `hold_type` / `hold_amount` point at the caller's own slot — an
 * EuropeHarborShip mirror hold or a ColonizeUnit hold, both int[]. Returns
 * the proceeds, or 0 when the boycott gate refused (nothing is mutated).
 * The caller keeps its own diag_info line: the three differ in what they
 * can name (ship name / unit id) and whether a partial shows "amt/held".
 * ---------------------------------------------------------------------
 */
static int europe_sell_commit(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int* hold_type,
  int* hold_amount,
  int amt
) {
  const int ctype = *hold_type;
  const int held = *hold_amount;
  if (europe_cargo_boycotted_ex(eu, col1, seller_nation, ctype)) {
    const char* bname =
      (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "";
    snprintf(
      eu->status, sizeof(eu->status), "%s is boycotted — cannot trade in Europe.", bname
    );
    return 0;
  }
  const int gained = europe_sell_proceeds(eu, ctype, amt);
  europe_purse_move(eu, col1, seller_nation, gained);
  europe_credit_sale_tax(col1, seller_nation, europe_sell_price(eu, ctype) * amt, gained);
  *hold_amount = held - amt;
  if (*hold_amount == 0) {
    *hold_type = 0;
  }
  europe_push_sale_status(eu, ctype, amt, gained);
  europe_apply_trade_volume(
    eu, col1, seller_nation, col1 ? (int)col1->head.human_player : seller_nation,
    ctype, amt, 0, 1
  );
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "";
  snprintf(eu->status, sizeof(eu->status), "Sold %d %s for %d$.", amt, cname, gained);
  return gained;
}

int europe_sell_hold(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int harbor_index,
  int hold_index
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= EUROPE_SHIP_CARGO_MAX) {
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  const int amt = ship->hold_goods_amount[hold_index];
  const int ctype = ship->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255) {
    return 0;
  }
  const int gained = europe_sell_commit(
    eu, col1, seller_nation, &ship->hold_goods_type[hold_index],
    &ship->hold_goods_amount[hold_index], amt
  );
  if (gained <= 0) {
    return 0;
  }
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "";
  diag_info(
    "EUROPE sold %d %s from %s: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, cname, ship->name[0] ? ship->name : "",
    europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

int europe_sell_hold_partial(
  EuropeScreen* eu,
  struct ColonizeCol1Save* col1,
  int seller_nation,
  int harbor_index,
  int hold_index,
  int amount
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= EUROPE_SHIP_CARGO_MAX) {
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  const int held = ship->hold_goods_amount[hold_index];
  const int ctype = ship->hold_goods_type[hold_index];
  if (held <= 0 || held >= 255 || amount <= 0) {
    return 0;
  }
  const int amt = amount < held ? amount : held;
  const int gained = europe_sell_commit(
    eu, col1, seller_nation, &ship->hold_goods_type[hold_index],
    &ship->hold_goods_amount[hold_index], amt
  );
  if (gained <= 0) {
    return 0;
  }
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "";
  diag_info(
    "EUROPE sold %d/%d %s from %s: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, held, cname, ship->name[0] ? ship->name : "",
    europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

/*
 * FUN_364b_0636: Custom House may auto-sell this cargo type.
 * Deny Food(0), Horses(8), Tools(0xe), Muskets(0xf).
 * Ore(6) extra DOS deny path not mapped — allow Ore (no invent).
 *
 * **2026-08-27: a same-day attempt to also deny Lumber(5) here (matching a
 * `param_1 != 5` term this function's raw decompile appears to have) was
 * reverted — both `golden_colony_prod01`/`02` (real DOS `.SAV` ground
 * truth) show Lumber genuinely auto-sold down to 50, contradicting that
 * reading. The `param_1==5` deny term is real in the decompile but must
 * gate something other than plain cargo-index-5 in this calling context
 * (`local_b6` in `FUN_364b_0688`'s loop may not be a raw cargo index the
 * way the other four terms' values are), or a second, different type-gate
 * function is the real one `0688` calls — not re-investigated this pass.
 * Golden evidence overrides the static read; leaving the deny list at its
 * original 4 entries, unchanged from before this attempt.
 */
static int europe_custom_house_cargo_eligible(int cargo_type) {
  /*
   * Resolved 2026-08-28: FUN_364b_0688 picks the gate by controller —
   * human colonies use FUN_281f_0cfe → FUN_15eb_0302 (colony +0x8a bit per
   * cargo == custom_house_bits), and ONLY AI colonies use FUN_364b_0636
   * (thunk_FUN_291f_09c0, confirmed via address_mapping.csv). So the
   * Lumber deny term is real, but it never applied to the human's Custom
   * House — which is why the real-DOS saves sold lumber. This function is
   * now the AI-only 0636 gate: deny Food/Lumber/Horses/Tools/Muskets; Ore
   * has an extra deny arm (building 3 present or DS:0x8de4/0x8de6 set)
   * that is not modelled here — Ore stays allowed.
   */
  if (cargo_type == COLONIZE_CARGO_FOOD || cargo_type == COLONIZE_CARGO_LUMBER ||
      cargo_type == COLONIZE_CARGO_HORSES || cargo_type == COLONIZE_CARGO_TOOLS ||
      cargo_type == COLONIZE_CARGO_MUSKETS) {
    return 0;
  }
  return cargo_type >= 0 && cargo_type < COLONIZE_CARGO_COUNT;
}

/* FUN_364b_0636 denylist — AI Custom House + AI peace Europe export sail. */
int europe_cargo_export_eligible(int cargo_type) {
  return europe_custom_house_cargo_eligible(cargo_type);
}

static int europe_custom_house_bit_enabled(uint16_t bits, int cargo_type) {
  /*
   * bits==0 → nothing configured yet, sell nothing (per-cargo UI PARKED —
   * see fandom_col1994.md Custom House: sells a "configured" cargo type,
   * not everything by default). Player-confirmed 2026-08-16 (real DOS
   * COLONY00/01_no-transports.SAV pair, colony-prod-tests): a real save
   * with Custom House built but custom_house_bits==0 (no cargo toggled)
   * sold *nothing* that turn — the old "bits==0 → all eligible" stand-in
   * auto-sold every stock>99 cargo down to 50 and was never DOS-verified.
   */
  if (bits == 0) {
    return 0;
  }
  return (bits >> cargo_type) & 1u;
}

bool europe_custom_house_cargo_enabled(uint16_t custom_house_bits, int cargo_type) {
  if (cargo_type < 0 || cargo_type >= COLONIZE_CARGO_COUNT) {
    return false;
  }
  return europe_custom_house_bit_enabled(custom_house_bits, cargo_type) != 0;
}

int europe_custom_house_autosell_ex_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation,
  EuropeCustomHouseSale* out,
  int out_max,
  int* out_count
) {
  EuropeScreen* eu = w->europe;
  ColonizeColonyPool* pool = w->colonies;
  ColonizeCol1Save* col1 = w->col1;

  if (out_count) {
    *out_count = 0;
  }
  /*
   * FUN_364b_0688 after production: Custom House + type gate + stock>99 →
   * sell stock-50. Cite: viceroy_unpacked.c FUN_364b_0688 / FUN_364b_0636;
   * docs/fandom_col1994.md Custom House (boycott bypass; WoI untaxed).
   */
  if (!eu || !pool || !colony || !colony->active) {
    return 0;
  }
  const int ch_id = colonies_building_row(pool, COLONY_BUILDING_CUSTOM_HOUSE);
  if (ch_id < 0 || ch_id >= COLONIZE_BUILDING_TYPES_MAX || !colony->has_building[ch_id]) {
    return 0;
  }
  const int nation = colony->nation_id;
  const int is_human = (nation == human_nation);
  /* FUN_364b_0688: a human colony's Custom House is shut while an enemy
   * armed ship / Man-O-War sits next to it (colony +0x1b & 3). */
  if (is_human && (colony->ai_flags & 0x03u) != 0u) {
    return 0;
  }
  const int woi = col1 && col1->head.game_options.woi != 0;
  int tax = 0;
  if (!woi) {
    if (is_human) {
      tax = eu->tax_percent;
    } else if (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) {
      tax = (int)col1->nation[nation].tax_rate;
    } else {
      tax = eu->tax_percent;
    }
  }
  if (tax < 0) {
    tax = 0;
  }
  if (tax > 100) {
    tax = 100;
  }
  ColonizeCol1Nation* nat =
    (col1 && nation >= 0 && nation < (int)COLONIZE_COL1_NATION_COUNT) ? &col1->nation[nation]
                                                                       : NULL;

  int total = 0;
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
    /* Human: per-cargo toggle bits (FUN_15eb_0302). AI: FUN_364b_0636. */
    if (is_human) {
      if (!europe_custom_house_bit_enabled(colony->custom_house_bits, c)) {
        continue;
      }
    } else if (!europe_custom_house_cargo_eligible(c)) {
      continue;
    }
    /* FUN_364b_0688: if (99 < stock) sell stock - 50 (leave 50 in warehouse). */
    if (colony->stock[c] <= 99) {
      continue;
    }
    const int amount = colony->stock[c] - 50;
    if (amount <= 0) {
      continue;
    }
    if (c >= eu->cargo_count) {
      continue;
    }
    /*
     * Sells at euro_price − 1 (FUN_291f_09ea → FUN_38fd_0040, which reads
     * `*(char *)(*(int *)0x84fc + cargo + 0x4c) - 1` clamped at 0). 0x84fc is
     * the *bound* nation record — the same one the tax byte above comes from —
     * so an AI colony's Custom House prices off that AI's own market, not the
     * human's. The port's live market (eu->cargo[].bid) is bound to the human
     * nation only (col1_bridge stamps col1 → bid on load and bid → col1 on
     * capture), so it is the human's live record; every other nation's market
     * lives in col1 trade.euro_price[]. Same substitution as the dump-sell
     * below: a game that never came from a DOS save has the AI nations' byte
     * still 0, so fall back to the one Linux market. A zero price still moves
     * the goods (DOS has no price gate here).
     */
    int price = europe_sell_price(eu, c);
    if (!is_human && nat && c < (int)COLONIZE_COL1_CARGO_TYPES &&
        nat->trade.euro_price[c] != 0) {
      const int p = (int)nat->trade.euro_price[c] - 1; /* FUN_38fd_0040 */
      price = p < 0 ? 0 : p;
    }
    const int gross = price * amount;
    const int gained = europe_net_after_tax(gross, tax);
    const int tax_paid = gross - gained;
    colony->stock[c] = 50;
    total += gained;
    if (out && out_count && *out_count < out_max) {
      EuropeCustomHouseSale* rec = &out[*out_count];
      rec->cargo = c;
      rec->amount = amount;
      rec->gross = gross;
      rec->tax_percent = tax;
      rec->tax_paid = tax_paid;
      rec->net = gained;
      (*out_count)++;
    }
    if (nat) {
      /*
       * One treasury (+0x2a), both stores: this was already the only europe.c
       * writer that bumped nat->gold AND eu->gold, which is why it looked
       * right while the harbor paths did not (smell audit G3). It now goes
       * through the shared accessor — for the human that credits the live
       * purse and re-stamps the record from it, for an AI colony's Custom
       * House only that AI's record, and the `is_human` special case below
       * is gone with it.
       */
      europe_nation_gold_add(eu, col1, nation, gained);
      /* nation +0x22 (royal_money) += tax paid; +0x26 write-only cumulative
       * net trade income (unknown24_pad, int32 LE). */
      nat->royal_money += tax_paid;
      uint32_t cum = (uint32_t)nat->unknown24_pad[0] | ((uint32_t)nat->unknown24_pad[1] << 8) |
                     ((uint32_t)nat->unknown24_pad[2] << 16) |
                     ((uint32_t)nat->unknown24_pad[3] << 24);
      cum += (uint32_t)gained;
      nat->unknown24_pad[0] = (uint8_t)(cum & 0xffu);
      nat->unknown24_pad[1] = (uint8_t)((cum >> 8) & 0xffu);
      nat->unknown24_pad[2] = (uint8_t)((cum >> 16) & 0xffu);
      nat->unknown24_pad[3] = (uint8_t)((cum >> 24) & 0xffu);
    }
    diag_info(
      "EUROPE customs %s sold %d %s: bid=%d tax=%d%% paid=%d proceeds=%d (nation=%d%s)",
      colony->name[0] ? colony->name : "",
      amount,
      (c < eu->cargo_count && eu->cargo[c].name[0]) ? eu->cargo[c].name : "",
      price,
      tax,
      tax_paid,
      gained,
      nation,
      is_human ? " human" : ""
    );
    /* FUN_291f_0a2e → FUN_38fd_1dfa; no FUN_38fd_0058 step here. */
    europe_apply_trade_volume(eu, col1, nation, human_nation, c, amount, 0, 0);
  }
  if (total > 0) {
    diag_info(
      "EUROPE customs %s total %d$ (nation=%d, gold=%d)",
      colony->name[0] ? colony->name : "", total, nation, eu->gold
    );
  }
  if (total > 0 && nation == human_nation) {
    if (colony->name[0]) {
      snprintf(
        eu->status,
        sizeof(eu->status),
        "Custom House in %s sold for %d$.",
        colony->name,
        total
      );
    } else {
      snprintf(eu->status, sizeof(eu->status), "Custom House sold goods for %d$.", total);
    }
  }
  return total;
}


int europe_custom_house_autosell_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation
) {
  EuropeScreen* eu = w->europe;
  ColonizeColonyPool* pool = w->colonies;
  ColonizeCol1Save* col1 = w->col1;

  return europe_custom_house_autosell_ex_w(&(ColonizeWorld){.colonies=(ColonizeColonyPool*)(pool), .col1=(ColonizeCol1Save*)(col1), .col1_ok=((col1) != NULL), .europe=(EuropeScreen*)(eu)}, colony, human_nation, NULL, 0, NULL);
}


int europe_ai_colony_dump_sell_w(
  const ColonizeWorld* w,
  ColonizeColony* colony,
  int human_nation
) {
  EuropeScreen* eu = w->europe;
  ColonizeColonyPool* pool = w->colonies;
  ColonizeCol1Save* col1 = w->col1;

  /*
   * FUN_364b_0688 phase O: non-human Euro (nation≤3, control≠0) sells warehouse
   * surplus for gold before spoilage. Stock is not reduced here — spoilage clamps.
   * Cite: viceroy_unpacked.c ~57806–57848; 291f_0a2e → 38fd_1dfa.
   *
   * UNTAXED. DOS (viceroy 57834-57846):
   *   local_66 = (uint)*(byte *)(cargo + nation*0x10 + -0x7b44) * amount;
   *   *(nation_rec + cargo*4 + 0x7c) += local_66;   // per-cargo ledger
   *   *(nation_rec + 0x2a)          += local_66;    // 32-bit treasury
   * No tax split: there is no FUN_1d1d_0ec6 tax call and no `+0x22`
   * royal_money write anywhere in this arm, unlike the Custom House arm 500
   * lines earlier (57277-57302) which does both.
   *
   * The price byte, though, IS the harbor sell price. `-0x7b44` and DS:0x84BC
   * are the same address (signed vs unsigned 16-bit), and that 4x16 table is
   * not the nation records' `euro_price[]` — it is a derived per-nation SELL
   * table, and every writer of it stores `euro_price − 1` clamped at 0:
   *   viceroy_unpacked.c:6316-6320 (the nation-bind rebuild, all four nations)
   *     iVar2 = *(char *)(*(int *)0x84fc + cargo + 0x4c) − 1;
   *     if (iVar2 < 0) iVar2 = 0;
   *     *(nation*0x10 + cargo + -0x7b44) = (char)iVar2;
   *   and identically at :51962-51966 and FUN_38fd_0058's tail :58996-59000,
   *   plus the two overlay copies (viceroy_overlays.c:30272, :36095).
   * `+0x4c` is exactly `offsetof(ColonizeCol1Nation, trade.euro_price)` (0x4c,
   * with sizeof == 0x13c), i.e. the record array this port keeps in
   * `nat->trade.euro_price[]` — the value DOS reads BEFORE the −1. So the
   * dump-sell price is `euro_price − 1` == europe_sell_price(), the project's
   * standing Europe price convention, and NOT the raw record byte.
   */
  if (!eu || !colony || !colony->active) {
    return 0;
  }
  const int nation = colony->nation_id;
  if (nation < 0 || nation > 3 || nation == human_nation) {
    return 0;
  }
  ColonizeCol1Nation* nat =
    (col1 && nation < (int)COLONIZE_COL1_NATION_COUNT) ? &col1->nation[nation] : NULL;

  int total = 0;
  for (int c = 1; c < COLONIZE_CARGO_COUNT; ++c) {
    const int cap = colonies_warehouse_capacity(pool, colony, c);
    if (cap <= 0) {
      continue;
    }
    const int surplus = colony->stock[c] - cap;
    if (surplus <= 0) {
      continue;
    }
    /* Horses: DOS adds surplus to Europe horses word; sell amount → 0. */
    if (c == COLONIZE_CARGO_HORSES) {
      unsigned h = (unsigned)eu->nation_horses[nation] + (unsigned)surplus;
      if (h > 65535u) {
        h = 65535u;
      }
      eu->nation_horses[nation] = (uint16_t)h;
      continue;
    }
    /*
     * Muskets: DOS while surplus>49: Europe musket counter++, amount−50; then
     * sell remainder for gold. DOS runs this batching before any price read,
     * so a zero price must not skip it.
     */
    int amount = surplus;
    if (c == COLONIZE_CARGO_MUSKETS) {
      while (amount > 49) {
        if (eu->nation_musket_batches[nation] < 65535u) {
          eu->nation_musket_batches[nation]++;
        }
        amount -= 50;
      }
      if (amount <= 0) {
        continue;
      }
    }
    if (c >= eu->cargo_count) {
      continue;
    }
    /* 291f_0a2e → 38fd_1dfa: ledgers + volume, no 0058 step. DOS runs this
     * BEFORE it reads the price byte (asm 364b:1795 vs 17d0). */
    europe_apply_trade_volume(eu, col1, nation, human_nation, c, amount, 0, 0);
    /*
     * DS:0x84BC byte for THIS colony's nation (asm 364b:17d0
     * `MOV AL,[BX+SI+0x84bc]`, SI = nation<<4, BX = cargo) — which is that
     * nation's record price minus one, see the derivation in the block above.
     * Substitution: a game that never came from a DOS save has the AI
     * nations' record byte still 0 (only the bound nation's is stamped, see
     * col1_bridge), so fall back to the one Linux market's sell price — the
     * same live-market-else-col1 pairing ai_euro_5d04_cb_sell_price uses.
     */
    int price;
    const int record_price =
      (nat && c < (int)COLONIZE_COL1_CARGO_TYPES) ? (int)nat->trade.euro_price[c] : 0;
    if (record_price > 0) {
      price = record_price - 1; /* the table's own `− 1` clamp; > 0 here, so no floor needed */
    } else {
      price = europe_sell_price(eu, c);
    }
    /* No price gate: DOS calls 291f_0a2e (volume) at 57827, before it reads
     * the price byte at 57834 — a zero price still moves the goods. */
    const int gained = price > 0 ? price * amount : 0; /* untaxed: DOS credits gross */
    total += gained;
    if (nat) {
      /*
       * Same double-book as the 20e6 delivery tail (ai_euro_20e6_delivery_
       * sell_tail): the 1dfa call just above already moved the taxed
       * proceeds into trade.gold[]/tons[]/tons2[]; DOS then adds the gross to
       * trade.gold[] and — verbatim, asm 364b:17bd `ADD [BX+0xbc],AX` with
       * AX = the loop's CARGO INDEX, not the amount — the index to
       * trade.tons[]. Both decompiles and the listing agree.
       */
      nat->trade.tons[c] += (int32_t)c;
      nat->trade.gold[c] += (int32_t)gained;
      nat->gold += (uint32_t)gained;
    }
  }
  if (total > 0) {
    diag_info(
      "EUROPE dump-sell %s total %d$ (nation=%d, untaxed)",
      colony->name[0] ? colony->name : "", total, nation
    );
    snprintf(eu->status, sizeof(eu->status), "AI warehouse dump-sold for %d$.", total);
  }
  return total;
}

int europe_sell_unit_hold_w(
  const ColonizeWorld* w,
  int unit_id,
  int hold_index
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;
  ColonizeUnitPool* units = w->units;

  /*
   * Map/transport dump-sell (no harbor chrome). Tax path = europe_sell_proceeds:
   * bid * amount * (100 - eu->tax_percent) / 100 — same Crown cut as
   * europe_sell_hold. Cite: Colonization.pdf Europe buy/sell + tax;
   * docs/manual_gap.md Europe commodity trade.
   */
  if (!eu || !units) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u || !u->active || !units_is_transport(units, unit_id)) {
    return 0;
  }
  if (hold_index < 0 || hold_index >= COLONIZE_UNIT_CARGO_MAX) {
    return 0;
  }
  const int amt = u->hold_goods_amount[hold_index];
  const int ctype = u->hold_goods_type[hold_index];
  if (amt <= 0 || amt >= 255) {
    return 0;
  }
  /* Seller = the hold's owner, so an AI-owned hold credits the AI purse. */
  const int gained = europe_sell_commit(
    eu, col1, (int)u->nation_id, &u->hold_goods_type[hold_index],
    &u->hold_goods_amount[hold_index], amt
  );
  if (gained <= 0) {
    return 0;
  }
  const char* cname =
    (ctype >= 0 && ctype < eu->cargo_count) ? eu->cargo[ctype].name : "";
  diag_info(
    "EUROPE sold %d %s from unit %d: bid=%d tax=%d%% proceeds=%d gold=%d",
    amt, cname, unit_id, europe_sell_price(eu, ctype), eu->tax_percent, gained, eu->gold
  );
  return gained;
}

int europe_buy_unit_cargo_w(
  const ColonizeWorld* w,
  int unit_id,
  int cargo_type,
  int amount
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;
  ColonizeUnitPool* units = w->units;

  /*
   * Map/transport buy (no harbor chrome) — trade-route load list at a Europe
   * stop (DOS FUN_479b_0bd0 → FUN_38fd_1fa2 via FUN_291f_0b42). Flat ask ×
   * bought, boycott gated, volume price applied, same as europe_buy_cargo.
   */
  if (!eu || !units || cargo_type < 0 || cargo_type >= eu->cargo_count || amount <= 0) {
    return 0;
  }
  ColonizeUnit* u = units_get(units, unit_id);
  if (!u || !u->active || !units_is_transport(units, unit_id)) {
    return 0;
  }
  if (europe_cargo_boycotted_ex(eu, col1, (int)u->nation_id, cargo_type)) {
    return 0;
  }
  const int ask = eu->cargo[cargo_type].ask;
  if (ask <= 0) {
    return 0;
  }
  int buy = amount > 100 ? 100 : amount;
  const int can_afford = eu->gold / ask;
  if (buy > can_afford) {
    buy = can_afford;
  }
  if (buy <= 0) {
    return 0;
  }
  const int loaded = units_load_goods(units, unit_id, cargo_type, buy);
  if (loaded <= 0) {
    return 0;
  }
  europe_purse_move(eu, col1, (int)u->nation_id, -(long)loaded * ask);
  /* Smell audit #50: ledger (1d80 gold −= ask·amt) + price move. */
  europe_apply_trade_volume(
    eu, col1, (int)u->nation_id, col1 ? (int)col1->head.human_player : (int)u->nation_id,
    cargo_type, loaded, 1, 1
  );
  diag_info(
    "EUROPE bought %d %s onto unit %d: ask=%d cost=%d gold=%d",
    loaded, eu->cargo[cargo_type].name, unit_id, ask, loaded * ask, eu->gold
  );
  return loaded;
}


int europe_harbor_cargo_room(
  const EuropeScreen* eu,
  const ColonizeUnitPool* units,
  int harbor_index,
  int cargo_type
) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  const EuropeHarborShip* ship = &eu->harbor[harbor_index];
  const int free_slots = europe_ship_free_slots(ship, units);
  int room = free_slots * 100;
  /*
   * DOS FUN_15eb_3208 only tallies the part-full matching holds when there is
   * no free slot left at all — with a free slot the room is already a whole
   * hold, and FUN_38fd_1fa2 clamps the buy to 100 either way.
   */
  if (free_slots == 0) {
    for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
      const int amt = ship->hold_goods_amount[i];
      if (amt > 0 && amt < 100 && ship->hold_goods_type[i] == cargo_type) {
        room += 100 - amt;
      }
    }
  }
  return room;
}

int europe_buy_cargo_w(
  const ColonizeWorld* w,
  int buyer_nation,
  int harbor_index,
  int cargo_type,
  int amount
) {
  EuropeScreen* eu = w->europe;
  struct ColonizeCol1Save* col1 = w->col1;
  const ColonizeUnitPool* units = w->units;

  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return 0;
  }
  if (cargo_type < 0 || cargo_type >= eu->cargo_count || amount <= 0) {
    return 0;
  }
  if (europe_cargo_boycotted_ex(eu, col1, buyer_nation, cargo_type)) {
    const char* cname = eu->cargo[cargo_type].name[0] ? eu->cargo[cargo_type].name : "";
    snprintf(
      eu->status, sizeof(eu->status), "%s is boycotted — cannot trade in Europe.", cname
    );
    return 0;
  }
  const int ask = eu->cargo[cargo_type].ask;
  if (ask <= 0) {
    europe_set_status(eu, "Cannot buy that cargo.");
    return 0;
  }
  EuropeHarborShip* ship = &eu->harbor[harbor_index];
  /*
   * Hold capacity (smell audit #83). DOS FUN_38fd_1fa2 asks FUN_281f_0b96 →
   * FUN_15eb_3208 for this ship's free room BEFORE charging anything and
   * bails to the "no room" popup when it comes back 0; the port used to walk
   * all six EUROPE_SHIP_CARGO_MAX slots unconditionally, so a Caravel (2
   * holds) loaded 600 tons. Capacity is the @UNIT cargo column, minus goods
   * slots already used, minus passengers (same slots — see
   * europe_board_sentry_dockers). A NULL pool still falls back to the
   * six-slot maximum, as europe_ship_cargo_cap always has.
   */
  const int free_slots = europe_ship_free_slots(ship, units);
  const int room_total = europe_harbor_cargo_room(eu, units, harbor_index, cargo_type);
  if (room_total <= 0) {
    europe_set_status(eu, "No empty hold.");
    return 0;
  }
  int can_afford = eu->gold / ask;
  if (can_afford <= 0) {
    europe_set_status(eu, "Need gold.");
    return 0;
  }
  int buy = amount;
  if (buy > 100) {
    buy = 100;
  }
  if (buy > room_total) {
    buy = room_total;
  }
  if (buy > can_afford) {
    buy = can_afford;
  }
  if (buy <= 0) {
    return 0;
  }

  /* Same two-pass packing as units_load_goods (top up matching partial
   * holds to 100, then append into free slots); the append pass is bounded
   * by `free_slots` because DOS FUN_15eb_30b8 only appends while
   * holds_occupied < cargo_cap. Shared body: goods_pack_into_holds. */
  const int remaining = buy - goods_pack_into_holds(
    ship->hold_goods_type, ship->hold_goods_amount, EUROPE_SHIP_CARGO_MAX, cargo_type, buy,
    free_slots
  );
  const int bought = buy - remaining;
  europe_purse_move(eu, col1, buyer_nation, -(long)bought * ask);
  if (bought > 0) {
    /* Smell audit #50: ledger (1d80 gold −= ask·amt) + price move. */
    europe_apply_trade_volume(
      eu, col1, buyer_nation, col1 ? (int)col1->head.human_player : buyer_nation,
      cargo_type, bought, 1, 1
    );
  }
  snprintf(
    eu->status,
    sizeof(eu->status),
    "Bought %d %s (-%d$).",
    bought,
    eu->cargo[cargo_type].name,
    bought * ask
  );
  diag_info(
    "EUROPE bought %d %s onto %s: ask=%d cost=%d gold=%d",
    bought, eu->cargo[cargo_type].name, ship->name[0] ? ship->name : "",
    ask, bought * ask, eu->gold
  );
  return bought;
}

/* ===================== Icon flow, hit-testing, menus & cheats (europe_best_sell_hold .. europe_passenger_icon_sprite) ===================== */
int europe_best_sell_hold(const EuropeScreen* eu, int harbor_index) {
  if (!eu || harbor_index < 0 || harbor_index >= eu->harbor_ships) {
    return -1;
  }
  const EuropeHarborShip* ship = &eu->harbor[harbor_index];
  int best = -1;
  int best_v = 0;
  int best_amt = 0;
  int first_loaded = -1;
  /*
   * Ordering heuristic only — a rough "which hold is worth the most" ranking,
   * with no DOS counterpart (DOS's own whole-cargo sweep, FUN_479b_0bd0 as
   * ported in game_europe_service_trade_harbor, just walks slots 0..5 in
   * index order). Its four zeroes (Lumber 5, Horses 8, Tools 14, Muskets 15)
   * used to make this function REFUSE such a hold outright, which broke both
   * its callers: European Status "U" loops on this picker to unload the whole
   * cargo and stopped at the first zero-value hold, and "-" fell back to it
   * and answered "Nothing to sell." on a ship full of muskets. The ranking
   * still decides the ORDER; it no longer decides whether a loaded hold is
   * sellable at all (smell audit #88).
   */
  static const int k_value[COLONIZE_CARGO_COUNT] = {
    1, 5, 4, 3, 5, 0, 4, 20, 0, 8, 8, 7, 7, 2, 0, 0
  };
  for (int i = 0; i < EUROPE_SHIP_CARGO_MAX; ++i) {
    const int amt = ship->hold_goods_amount[i];
    const int ctype = ship->hold_goods_type[i];
    if (amt <= 0 || amt >= 255) {
      continue;
    }
    if (first_loaded < 0) {
      first_loaded = i;
    }
    const int v =
      (ctype >= 0 && ctype < COLONIZE_CARGO_COUNT) ? k_value[ctype] : 0;
    if (v <= 0) {
      continue;
    }
    if (v > best_v || (v == best_v && amt > best_amt)) {
      best_v = v;
      best_amt = amt;
      best = i;
    }
  }
  return best >= 0 ? best : first_loaded;
}

static bool europe_in_rect(int mx, int my, int x, int y, int w, int h) {
  return ui_rect_hit(x, y, w, h, mx, my);
}

static int europe_ship_icon_sprite(const ColonizeUnitPool* units, const EuropeHarborShip* ship) {
  if (!units || !ship) {
    return -1;
  }
  int ti = ship->type_index;
  if (ti < 0) {
    ti = units_find_type(units, ship->name);
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  return ut ? ut->icon_sprite : -1;
}

int europe_pax_type_index(const ColonizeUnitPool* units, int tag) {
  if (tag == -2) {
    const int t = units_kind_type_index(units, UNITS_KIND_ARTILLERY);
    return t >= 0 ? t : 0;
  }
  if (!units || tag < 0 || tag >= units->type_count) {
    const int t = units_kind_type_index(units, UNITS_KIND_COLONIST);
    return t >= 0 ? t : 0;
  }
  return tag;
}

bool europe_icon_flow_begin(
  EuropeIconFlow* f, int box_x, int box_y, int box_w, int box_h, int line_h
) {
  if (!f) {
    return false;
  }
  line_h = line_h > 0 ? line_h : 8;
  const int header_h = EUROPE_TRANSIT_HEADER_LINES * line_h;
  f->box_x = box_x;
  f->box_y = box_y;
  f->box_w = box_w;
  f->box_h = box_h;
  f->x = box_x + 3;
  f->y = box_y + 2 + header_h + 10;
  f->row_h = 0;
  const int ship_area_h = box_y + box_h - f->y - 1;
  return ship_area_h >= 8;
}

bool europe_icon_flow_place(EuropeIconFlow* f, int w, int h) {
  if (f->x + w > f->box_x + f->box_w - 2) {
    f->x = f->box_x + 3;
    f->y += f->row_h + 1;
    f->row_h = 0;
    if (f->y + h > f->box_y + f->box_h - 1) {
      return false;
    }
  }
  if (h > f->row_h) {
    f->row_h = h;
  }
  return true;
}

void europe_icon_flow_advance(EuropeIconFlow* f, int w) {
  f->x += w + 2;
}

void europe_icon_flow_size(const ColonizeSpriteSheet* icons, int sprite, int* w, int* h) {
  const ColonizeSprite* sp = &icons->sprites[sprite];
  *w = sp->width > 0 ? sp->width : 14;
  *h = sp->height > 0 ? sp->height : 16;
}

static int europe_transit_ship_at(
  const EuropeHarborShip* ships,
  int count,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int box_x,
  int box_y,
  int box_w,
  int box_h,
  int transit_line_h,
  int mx,
  int my
) {
  if (!ships || count <= 0 || !units || !unit_icons || !europe_in_rect(mx, my, box_x, box_y, box_w, box_h)) {
    return -1;
  }
  EuropeIconFlow f;
  if (!europe_icon_flow_begin(&f, box_x, box_y, box_w, box_h, transit_line_h)) {
    return -1;
  }
  for (int i = 0; i < count; ++i) {
    const int sprite = europe_ship_icon_sprite(units, &ships[i]);
    if (sprite < 0 || sprite >= unit_icons->sprite_count) {
      continue;
    }
    int sw = 0;
    int sh = 0;
    europe_icon_flow_size(unit_icons, sprite, &sw, &sh);
    if (!europe_icon_flow_place(&f, sw, sh)) {
      break;
    }
    if (europe_in_rect(mx, my, f.x, f.y, sw, sh)) {
      return i;
    }
    europe_icon_flow_advance(&f, sw);
    /* Passengers ride along after their ship (see the renderer); a click on
     * one selects the ship — they are cargo, the ship is what the player
     * clicks. */
    for (int c = 0; c < ships[i].cargo_count && c < EUROPE_SHIP_CARGO_MAX; ++c) {
      const int pax_type = europe_pax_type_index(units, ships[i].cargo_types[c]);
      const int pax_sprite =
        europe_passenger_icon_sprite(units, pax_type, ships[i].cargo_professions[c]);
      if (pax_sprite < 0 || pax_sprite >= unit_icons->sprite_count) {
        continue;
      }
      int pw = 0;
      int ph = 0;
      europe_icon_flow_size(unit_icons, pax_sprite, &pw, &ph);
      if (!europe_icon_flow_place(&f, pw, ph)) {
        break;
      }
      if (europe_in_rect(mx, my, f.x, f.y, pw, ph)) {
        return i;
      }
      europe_icon_flow_advance(&f, pw);
    }
  }
  /* Clicked the box but not an icon — use first ship. */
  return count > 0 ? 0 : -1;
}

EuropeHitResult europe_hit_test(const EuropeScreen* eu, int mx, int my) {
  return europe_hit_test_ex(eu, mx, my, NULL, NULL, 8);
}

EuropeHitResult europe_hit_test_ex(
  const EuropeScreen* eu,
  int mx,
  int my,
  const ColonizeUnitPool* units,
  const ColonizeSpriteSheet* unit_icons,
  int transit_line_h
) {
  EuropeHitResult hit;
  hit.kind = EUROPE_HIT_NONE;
  hit.index = -1;
  if (!eu) {
    return hit;
  }

  if (mx >= EUROPE_EXIT_X && mx < EUROPE_SCREEN_W && my >= EUROPE_EXIT_Y && my < EUROPE_SCREEN_H) {
    hit.kind = EUROPE_HIT_EXIT;
    return hit;
  }

  if (europe_in_rect(mx, my, EUROPE_BTN_X, EUROPE_BTN_Y, EUROPE_BTN_W, EUROPE_BTN_H)) {
    hit.kind = EUROPE_HIT_BTN_RECRUIT;
    return hit;
  }
  if (europe_in_rect(
        mx, my, EUROPE_BTN_X, EUROPE_BTN_Y + EUROPE_BTN_PITCH, EUROPE_BTN_W + 8, EUROPE_BTN_H
      )) {
    hit.kind = EUROPE_HIT_BTN_PURCHASE;
    return hit;
  }
  if (europe_in_rect(
        mx,
        my,
        EUROPE_BTN_X,
        EUROPE_BTN_Y + 2 * EUROPE_BTN_PITCH,
        EUROPE_BTN_W,
        EUROPE_BTN_H
      )) {
    hit.kind = EUROPE_HIT_BTN_TRAIN;
    return hit;
  }

  {
    if (eu->harbor_ships > 0 && my >= EUROPE_HOLD_Y && my < EUROPE_HOLD_Y + EUROPE_HOLD_H &&
        mx >= EUROPE_HOLD_X && mx < EUROPE_HOLD_X + EUROPE_HOLD_MAX * EUROPE_HOLD_PITCH) {
      const int idx = (mx - EUROPE_HOLD_X) / EUROPE_HOLD_PITCH;
      if (idx >= 0 && idx < EUROPE_HOLD_MAX) {
        hit.kind = EUROPE_HIT_HOLD;
        hit.index = idx;
        return hit;
      }
    }
  }

  if (eu->harbor_ships > 0 &&
      europe_in_rect(mx, my, EUROPE_LOADING_X, EUROPE_LOADING_Y, EUROPE_LOADING_W, EUROPE_LOADING_H)) {
    hit.kind = EUROPE_HIT_HARBOR_SHIP;
    if (units && unit_icons) {
      const int si = europe_transit_ship_at(
        eu->harbor,
        eu->harbor_ships,
        units,
        unit_icons,
        EUROPE_LOADING_X,
        EUROPE_LOADING_Y,
        EUROPE_LOADING_W,
        EUROPE_LOADING_H,
        transit_line_h,
        mx,
        my
      );
      hit.index = si >= 0 ? si : (eu->selected_harbor >= 0 ? eu->selected_harbor : 0);
    } else {
      hit.index = eu->selected_harbor >= 0 ? eu->selected_harbor : 0;
    }
    return hit;
  }

  /* Transit boxes stay hittable when empty so ships can be dropped into them. */
  if (europe_in_rect(mx, my, EUROPE_EXPECTED_X, EUROPE_EXPECTED_Y, EUROPE_EXPECTED_W, EUROPE_EXPECTED_H)) {
    hit.kind = EUROPE_HIT_EXPECTED;
    if (eu->expected_ships > 0 && units && unit_icons) {
      hit.index = europe_transit_ship_at(
        eu->expected,
        eu->expected_ships,
        units,
        unit_icons,
        EUROPE_EXPECTED_X,
        EUROPE_EXPECTED_Y,
        EUROPE_EXPECTED_W,
        EUROPE_EXPECTED_H,
        transit_line_h,
        mx,
        my
      );
    } else {
      hit.index = eu->expected_ships > 0 ? 0 : -1;
    }
    return hit;
  }
  if (europe_in_rect(mx, my, EUROPE_BOUND_X, EUROPE_BOUND_Y, EUROPE_BOUND_W, EUROPE_BOUND_H)) {
    hit.kind = EUROPE_HIT_BOUND;
    if (eu->bound_ships > 0 && units && unit_icons) {
      hit.index = europe_transit_ship_at(
        eu->bound,
        eu->bound_ships,
        units,
        unit_icons,
        EUROPE_BOUND_X,
        EUROPE_BOUND_Y,
        EUROPE_BOUND_W,
        EUROPE_BOUND_H,
        transit_line_h,
        mx,
        my
      );
    } else {
      hit.index = eu->bound_ships > 0 ? 0 : -1;
    }
    return hit;
  }

  for (int i = 0; i < eu->dock_count; ++i) {
    int dx = 0;
    int dy = 0;
    if (!europe_dock_slot_pos(i, &dx, &dy)) {
      break;
    }
    if (europe_in_rect(mx, my, dx, dy, EUROPE_DOCK_UNIT_W, EUROPE_DOCK_UNIT_H)) {
      hit.kind = EUROPE_HIT_DOCK;
      hit.index = i;
      return hit;
    }
  }

  if (my >= EUROPE_MARKET_Y && my < EUROPE_MARKET_Y + EUROPE_MARKET_H && mx >= EUROPE_MARKET_X) {
    const int idx = (mx - EUROPE_MARKET_X) / EUROPE_MARKET_PITCH;
    if (idx >= 0 && idx < eu->cargo_count && idx < EUROPE_CARGO_MAX &&
        mx < EUROPE_MARKET_X + idx * EUROPE_MARKET_PITCH + EUROPE_MARKET_CELL) {
      hit.kind = EUROPE_HIT_MARKET;
      hit.index = idx;
      return hit;
    }
  }

  return hit;
}

/* Europe list-dialog name, for the debug log only. */
static const char* europe_menu_name(EuropeMenu menu) {
  switch (menu) {
    case EUROPE_MENU_RECRUIT: return "RECRUIT";
    case EUROPE_MENU_TRAIN: return "TRAIN";
    case EUROPE_MENU_PURCHASE: return "PURCHASE";
    case EUROPE_MENU_DOCK: return "DOCK";
    default: return "NONE";
  }
}

void europe_menu_open(EuropeScreen* eu, EuropeMenu menu) {
  if (!eu) {
    return;
  }
  eu->menu = menu;
  eu->menu_selection = 0;
  eu->menu_answered = false;
  eu->purchase_confirming = false;
  if (menu == EUROPE_MENU_RECRUIT) {
    europe_pool_ensure_filled(eu);
    snprintf(
      eu->status,
      sizeof(eu->status),
      "Recruit (passage %d$). Esc cancels.",
      eu->recruit_passage
    );
  } else if (menu == EUROPE_MENU_TRAIN) {
    europe_set_status(eu, "Royal University. Esc cancels.");
  } else if (menu == EUROPE_MENU_PURCHASE) {
    europe_set_status(eu, "Purchase. Esc cancels.");
  } else if (menu == EUROPE_MENU_DOCK) {
    /* FUN_38fd_3694 (raw 61180-61197): the dock-unit click builds the
     * "<nation> <unit> (<job>)" caption on the status strip. */
    char caption[96];
    if (europe_dock_caption(eu, eu->menu_dock_index, caption, sizeof caption)) {
      europe_set_status(eu, caption);
    } else {
      europe_set_status(eu, "Dock orders. Esc cancels.");
    }
  }
  if (diag_info_enabled()) {
    char rows[512];
    size_t at = 0;
    rows[0] = '\0';
    if (menu == EUROPE_MENU_RECRUIT) {
      for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s", i ? " " : "", i + 1, eu->pool[i].name
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    } else if (menu == EUROPE_MENU_TRAIN) {
      for (int i = 0; i < eu->train_count; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s %d$", i ? " " : "", i + 1,
          eu->train[i].expert_name, eu->train[i].cost
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    } else if (menu == EUROPE_MENU_PURCHASE) {
      for (int i = 0; i < eu->purchase_count; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s %d$", i ? " " : "", i + 1,
          eu->purchase[i].name, europe_purchase_cost(eu, i)
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    } else if (menu == EUROPE_MENU_DOCK) {
      for (int i = 0; i < eu->dock_menu_count; ++i) {
        const int n = snprintf(
          rows + at, sizeof(rows) - at, "%s[%d]%s", i ? " " : "",
          (int)eu->dock_menu_row[i], europe_arm_row_name((int)eu->dock_menu_row[i])
        );
        if (n <= 0 || (size_t)n >= sizeof(rows) - at) {
          break;
        }
        at += (size_t)n;
      }
    }
    diag_info(
      "POPUP show tag=EUROPE_%s kind=choice gold=%d choices=%s",
      europe_menu_name(menu), eu->gold, rows
    );
  }
}

void europe_menu_close(EuropeScreen* eu) {
  if (!eu) {
    return;
  }
  if (eu->menu != EUROPE_MENU_NONE && !eu->menu_answered) {
    diag_info("POPUP answered tag=EUROPE_%s cancelled", europe_menu_name(eu->menu));
  }
  eu->menu_answered = false;
  eu->menu = EUROPE_MENU_NONE;
  eu->menu_selection = 0;
  eu->menu_dock_index = -1;
  eu->purchase_confirming = false;
}

/*
 * Turn the highlighted row of the open dock menu into its @ARMOPTIONS row id
 * and apply it. Row ids are carried per-row because DOS omits the rows it
 * disabled, so the visible index is not the id.
 */
bool europe_dock_menu_apply_selection_ex_w(
  const ColonizeWorld* w,
  int nation_id
) {
  EuropeScreen* eu = w->europe;
  ColonizeUnitPool* units = w->units;
  ColonizeCol1Save* col1 = w->col1;

  if (!eu || eu->menu != EUROPE_MENU_DOCK) {
    return false;
  }
  const int sel = eu->menu_selection;
  if (sel < 0 || sel >= eu->dock_menu_count) {
    return false;
  }
  if (eu->dock_menu_greyed[sel]) {
    europe_set_status(eu, "The treasury cannot afford that.");
    return false;
  }
  eu->menu_answered = true;
  return europe_apply_dock_menu_row_ex(
    eu, units, col1, nation_id, eu->menu_dock_index, (int)eu->dock_menu_row[sel]
  );
}


static bool europe_dock_menu_apply_selection(
  EuropeScreen* eu,
  ColonizeUnitPool* units,
  int nation_id
) {
  return europe_dock_menu_apply_selection_ex_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(units), .col1=(ColonizeCol1Save*)(NULL), .col1_ok=((NULL) != NULL), .europe=(EuropeScreen*)(eu)}, nation_id);
}

bool europe_menu_confirm(EuropeScreen* eu) {
  return europe_menu_confirm_ex(eu, NULL);
}

bool europe_menu_confirm_ex(EuropeScreen* eu, ColonizeDosRng* rng) {
  if (!eu || eu->menu == EUROPE_MENU_NONE) {
    return false;
  }
  const EuropeMenu m = eu->menu;
  const int sel = eu->menu_selection;
  if (m != EUROPE_MENU_DOCK) {
    diag_info(
      "POPUP answered tag=EUROPE_%s %s row=%d", europe_menu_name(m),
      sel == 0 ? "cancelled" : "picked", sel
    );
    eu->menu_answered = true;
  }
  if (m == EUROPE_MENU_DOCK) {
    /* Units-less path (no pool to keep the mirror unit in step); game_loop
     * calls europe_dock_menu_apply_selection with the pool so equipment,
     * type and dock sprite follow the row that was picked. */
    const bool ok = europe_dock_menu_apply_selection(eu, NULL, -1);
    europe_menu_close(eu);
    return ok;
  }
  /*
   * @REALLYBUY answers are 1-based DOS choice rows (raw 64874 `iVar6 == 1`),
   * so row 0 is "Yes" and row 1 is "No" — the same order @HAVETREATY and
   * every other GAME.TXT 2-choice popup use. Special-cased ahead of the
   * generic sel==0 cancel below, which would otherwise treat "Yes" as a
   * cancel. bugs.md #753.
   */
  if (m == EUROPE_MENU_PURCHASE && eu->purchase_confirming) {
    bool ok = true;
    if (sel == 0) {
      ok = europe_purchase_commit(eu, eu->purchase_confirm_index, eu->purchase_confirm_cost, rng);
    } else {
      europe_set_status(eu, "Cancelled.");
    }
    eu->purchase_confirming = false;
    europe_menu_close(eu);
    return ok;
  }
  if (sel == 0) {
    europe_menu_close(eu);
    europe_set_status(eu, "Cancelled.");
    return true;
  }
  if (m == EUROPE_MENU_RECRUIT) {
    const int pool_i = sel - 1;
    if (!europe_recruit_affordable(eu)) {
      return false; /* greyed row: inert, dialog stays up (bugs.md #588) */
    }
    const bool ok = europe_recruit_from_pool_ex(eu, pool_i, rng);
    europe_menu_close(eu);
    return ok;
  }
  if (m == EUROPE_MENU_TRAIN) {
    if (!europe_train_affordable(eu, sel - 1)) {
      return false; /* greyed row: inert, dialog stays up (bugs.md #893) */
    }
    const bool ok = europe_train_ex(eu, sel - 1, rng);
    europe_menu_close(eu);
    return ok;
  }
  if (m == EUROPE_MENU_PURCHASE) {
    /* purchase_confirming is handled above, ahead of the generic sel==0
     * cancel. */
    if (!europe_purchase_affordable(eu, sel - 1)) {
      return false; /* greyed row: inert, dialog stays up (bugs.md #754) */
    }
    /* Open @REALLYBUY; dialog stays up until Yes/No answers it. */
    return europe_purchase_open_confirm(eu, sel - 1);
  }
  europe_menu_close(eu);
  return false;
}

void europe_cheat_add_gold(EuropeScreen* eu, int amount) {
  if (!eu) {
    return;
  }
  eu->gold += amount;
  if (eu->gold < 0) {
    eu->gold = 0;
  }
  snprintf(eu->status, sizeof(eu->status), "Treasury now %d$.", eu->gold);
}

void europe_cheat_adjust_tax(EuropeScreen* eu, int delta) {
  if (!eu) {
    return;
  }
  eu->tax_percent += delta;
  if (eu->tax_percent < 0) {
    eu->tax_percent = 0;
  }
  if (eu->tax_percent > 100) {
    eu->tax_percent = 100;
  }
  snprintf(eu->status, sizeof(eu->status), "Tax rate %d%%.", eu->tax_percent);
}

int europe_dock_icon_sprite(const ColonizeUnitPool* units, const EuropeDockImmigrant* d) {
  if (!units || !d || !d->name[0]) {
    return -1;
  }
  if (europe_dock_name_is_artillery(d->name)) {
    const int ti = units_kind_type_index(units, UNITS_KIND_ARTILLERY);
    const ColonizeUnitType* ut = units_type(units, ti);
    return ut ? ut->icon_sprite : -1;
  }
  /* Armed / equipped / blessed on the dock: show what the immigrant now is,
   * not the profession portrait it arrived with (bugs.md @ARMOPTIONS).
   * bugs.md #158/177: the @UNIT icon column carries the EXPERT poses (Hardy
   * Pioneer / Veteran Soldier …) — DOS's map rule overrides those to the
   * base pose unless the unit's own profession matches; the dock follows
   * the same rule, so a Master Blacksmith with tools reads as a plain
   * Pioneer, not a Hardy one. */
  /* bugs.md #263 (units_map_sprite) superseded by #656: units.c's veteran
   * gates (units_profession_line, units_map_sprite/FUN_112b_0060) are
   * 0x15 (UNITS_JOB_SOLDIER) only — DOS never writes 0x17
   * (UNITS_JOB_DRAGOON) to a unit (#503/#639) — so this sibling display
   * now matches them instead of tolerating 0x17. */
  const bool dock_vet_prof = d->profession == UNITS_JOB_SOLDIER;
  switch (d->dos_type) {
    case EUROPE_DOCK_TYPE_PIONEERS:
      return d->profession == UNITS_JOB_PIONEER ? UNITS_ICON_HARDY_PIONEER
                                                : UNITS_ICON_PIONEER;
    case EUROPE_DOCK_TYPE_SOLDIERS:
      return dock_vet_prof ? UNITS_ICON_VETERAN_SOLDIER : UNITS_ICON_SOLDIER;
    case EUROPE_DOCK_TYPE_DRAGOONS:
      return dock_vet_prof ? UNITS_ICON_VETERAN_DRAGOON : UNITS_ICON_DRAGOON;
    case EUROPE_DOCK_TYPE_SCOUTS:
      return d->profession == UNITS_JOB_SCOUT ? UNITS_ICON_SEASONED_SCOUT
                                              : UNITS_ICON_SCOUT;
    /* bugs.md #420: same split for the fifth kit — FUN_112b_0060's
     * `type == 3 && profession != 0x18 → 0x4e`. Without this case a
     * shipped-home missionary took the @UNIT icon (the Jesuit) whatever
     * his colonist was. */
    case EUROPE_DOCK_TYPE_MISSIONARIES:
      return d->profession == UNITS_JOB_MISSIONARY ? UNITS_ICON_JESUIT_MISSIONARY
                                                   : UNITS_ICON_MISSIONARY;
    default:
      break;
  }
  if (d->dos_type != EUROPE_DOCK_TYPE_COLONISTS) {
    const int eti = europe_dock_unit_type_index(units, d->dos_type);
    const ColonizeUnitType* eut = units_type(units, eti);
    if (eut && eut->icon_sprite >= 0) {
      return eut->icon_sprite;
    }
  }
  /* bugs.md #603: the dock entry's @UNIT row, not a display-name lookup.
   * Only EUROPE_DOCK_TYPE_COLONISTS (or a row whose icon column is unset)
   * reaches here, so the singular fallback covers hand-built test pools. */
  int ti = europe_dock_unit_type_index_ex(units, d->dos_type, true);
  if (ti < 0) {
    ti = units_kind_type_index(units, UNITS_KIND_COLONIST);
  }
  if (ti < 0) {
    return -1;
  }
  const ColonizeUnitType* ut = units_type(units, ti);
  if (units_type_is_colonist(ut)) {
    return units_working_colonist_sprite(units, ti, d->profession);
  }
  return ut ? ut->icon_sprite : -1;
}

int europe_passenger_icon_sprite(const ColonizeUnitPool* units, int type_index, int profession) {
  const ColonizeUnitType* ut = units_type(units, type_index);
  if (!ut) {
    return -1;
  }
  if (units_type_is_colonist(ut)) {
    return units_working_colonist_sprite(units, type_index, profession);
  }
  /*
   * bugs.md #452: the @UNIT icon of the five kit types is the EXPERT pose
   * (Hardy Pioneer, Veteran Soldier, ...), so a Master Carpenter given tools
   * sailed out as a Hardy Pioneer. A passenger is the same unit the dock
   * showed a moment earlier: route it through the dock's expert/generic
   * split (DOS FUN_112b_0060), keyed by its own profession.
   */
  for (int k = EUROPE_DOCK_TYPE_SOLDIERS; k < EUROPE_DOCK_TYPE_COUNT; ++k) {
    if (strcmp(ut->name, reports_dock_type_name(k)) == 0) {
      EuropeDockImmigrant d;
      memset(&d, 0, sizeof(d));
      snprintf(d.name, sizeof(d.name), "%s", ut->name);
      d.profession = profession;
      d.present = true;
      d.dos_type = k;
      return europe_dock_icon_sprite(units, &d);
    }
  }
  return ut->icon_sprite;
}
