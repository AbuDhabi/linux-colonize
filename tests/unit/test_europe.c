#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/ui_drag.h"
#include "core/units.h"
#include "platform/diagnostics.h"

int main(void) {
  diag_init(0, NULL);

  EuropeScreen eu;
  char err[256];
  if (!europe_load(&eu, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "europe_load failed: %s\n", err);
    return 1;
  }

  if (!eu.background_ok || eu.background.width != 320 || eu.background.height != 200) {
    fprintf(
      stderr,
      "EUROPE.PIK expected 320x200, got %dx%d ok=%d\n",
      eu.background.width,
      eu.background.height,
      eu.background_ok ? 1 : 0
    );
    europe_free(&eu);
    return 1;
  }

  if (eu.cargo_count < 16) {
    fprintf(stderr, "expected >=16 cargo quotes, got %d\n", eu.cargo_count);
    europe_free(&eu);
    return 1;
  }
  if (eu.class_count < 1) {
    fprintf(stderr, "expected recruit classes from @CLASS\n");
    europe_free(&eu);
    return 1;
  }

  /* Food: start_lo=1, burden=7 → ask = euro_price + burden = 8, sell = 0
   * (DOS 1494 Europe screen shows Food 0/8). */
  if (strcmp(eu.cargo[0].name, "Food") != 0 || eu.cargo[0].bid != 1 || eu.cargo[0].ask != 8 ||
      europe_sell_price(&eu, 0) != 0 || europe_buy_price(&eu, 0) != 8) {
    fprintf(
      stderr,
      "Food quote expected bid=1 ask=8 got name='%s' bid=%d ask=%d\n",
      eu.cargo[0].name,
      eu.cargo[0].bid,
      eu.cargo[0].ask
    );
    europe_free(&eu);
    return 1;
  }

  /*
   * Real DOS `FUN_38fd_4884` passage formula (was a linear placeholder —
   * see manual_gap.md). Pure-function checks against hand-traced bytes:
   * viceroy_unpacked.c 64682-64694.
   */
  if (europe_compute_recruit_passage(0, 0, 0, 9) != 140) {
    fprintf(
      stderr, "passage(0,0,0,9) want 140 got %d\n", europe_compute_recruit_passage(0, 0, 0, 9)
    );
    europe_free(&eu);
    return 1;
  }
  if (europe_compute_recruit_passage(1, 0, 0, 9) != 160) {
    fprintf(
      stderr, "passage(1,0,0,9) want 160 got %d\n", europe_compute_recruit_passage(1, 0, 0, 9)
    );
    europe_free(&eu);
    return 1;
  }
  /* Crosses-pressure discount: base=320 floor=100 discount=(220*50)/-100=-110. */
  if (europe_compute_recruit_passage(5, 4, 50, 99) != 210) {
    fprintf(
      stderr,
      "passage(5,4,50,99) want 210 got %d\n",
      europe_compute_recruit_passage(5, 4, 50, 99)
    );
    europe_free(&eu);
    return 1;
  }
  /* Difficulty clamps to 0..8; out-of-range 20 must match the 8 case. */
  if (europe_compute_recruit_passage(0, 20, 0, 9) != europe_compute_recruit_passage(0, 8, 0, 9)) {
    fprintf(stderr, "passage difficulty clamp mismatch\n");
    europe_free(&eu);
    return 1;
  }
  /* Floor is 10 even when the discount would drive it lower/negative. */
  if (europe_compute_recruit_passage(0, 0, 60000, 1) < 10) {
    fprintf(
      stderr,
      "passage floor want >=10 got %d\n",
      europe_compute_recruit_passage(0, 0, 60000, 1)
    );
    europe_free(&eu);
    return 1;
  }
  /* Reset seeds needed_crosses=9/current=0/count=0/difficulty=0 → 140. */
  if (eu.recruit_count != 0 || eu.recruit_passage != 140) {
    fprintf(
      stderr,
      "initial recruit state want count=0 passage=140 got count=%d passage=%d\n",
      eu.recruit_count,
      eu.recruit_passage
    );
    europe_free(&eu);
    return 1;
  }

  const int gold_before = eu.gold;
  const int dock_before = eu.dock_count;
  if (!europe_recruit(&eu) || eu.menu != EUROPE_MENU_RECRUIT) {
    fprintf(stderr, "recruit menu open failed: %s\n", eu.status);
    europe_free(&eu);
    return 1;
  }
  eu.menu_selection = 1; /* first pool slot */
  if (!europe_menu_confirm(&eu)) {
    fprintf(stderr, "recruit confirm failed: %s\n", eu.status);
    europe_free(&eu);
    return 1;
  }
  if (eu.dock_count != dock_before + 1 || eu.gold >= gold_before) {
    fprintf(stderr, "recruit did not update dock/gold (dock %d→%d gold %d→%d)\n",
      dock_before, eu.dock_count, gold_before, eu.gold);
    europe_free(&eu);
    return 1;
  }
  /* Real Recruit bumps Europe+6 (recruit_count) and recomputes passage. */
  {
    const int want_passage =
      europe_compute_recruit_passage(1, eu.difficulty, eu.current_crosses, eu.needed_crosses);
    if (eu.recruit_count != 1 || eu.recruit_passage != want_passage) {
      fprintf(
        stderr,
        "post-recruit want count=1 passage=%d got count=%d passage=%d\n",
        want_passage,
        eu.recruit_count,
        eu.recruit_passage
      );
      europe_free(&eu);
      return 1;
    }
  }
  /* Crosses-driven free immigrant (0718 harbor spawn) must NOT bump
   * Europe+6 — only the real interactive Recruit path (4884) does. */
  {
    const int count_before = eu.recruit_count;
    const int dock2_before = eu.dock_count;
    if (!europe_immigrant_from_pool(&eu, NULL) || eu.dock_count != dock2_before + 1 ||
        eu.recruit_count != count_before) {
      fprintf(
        stderr,
        "free immigrant should not bump recruit_count (before=%d after=%d)\n",
        count_before,
        eu.recruit_count
      );
      europe_free(&eu);
      return 1;
    }
  }

  const int gold_after_recruit = eu.gold;
  europe_cheat_add_gold(&eu, 1000);
  if (eu.gold != gold_after_recruit + 1000) {
    fprintf(stderr, "gold cheat expected %d got %d\n", gold_after_recruit + 1000, eu.gold);
    europe_free(&eu);
    return 1;
  }

  europe_cheat_adjust_tax(&eu, -1);
  if (eu.tax_percent != 0) {
    fprintf(stderr, "tax should clamp at 0, got %d\n", eu.tax_percent);
    europe_free(&eu);
    return 1;
  }

  if (eu.harbor_ships != 0) {
    fprintf(stderr, "starter harbor should be empty, got %d\n", eu.harbor_ships);
    europe_free(&eu);
    return 1;
  }

  int hold_types[EUROPE_SHIP_CARGO_MAX];
  int hold_amts[EUROPE_SHIP_CARGO_MAX];
  memset(hold_types, 0, sizeof(hold_types));
  memset(hold_amts, 0, sizeof(hold_amts));
  hold_types[0] = COLONIZE_CARGO_SUGAR;
  hold_amts[0] = 100;

  if (!europe_harbor_push(&eu, 14, "Caravel", NULL, 0, hold_types, hold_amts) ||
      eu.harbor_ships != 1) {
    fprintf(stderr, "harbor_push failed\n");
    europe_free(&eu);
    return 1;
  }
  if (eu.selected_harbor != 0 || eu.harbor[0].hold_goods_amount[0] != 100 ||
      eu.harbor[0].hold_goods_type[0] != COLONIZE_CARGO_SUGAR) {
    fprintf(stderr, "harbor goods / selection not stored\n");
    europe_free(&eu);
    return 1;
  }

  int pax_types[2] = {3, 1};
  if (!europe_harbor_push(&eu, 15, "Merchantman", pax_types, 2, NULL, NULL) ||
      eu.harbor_ships != 2) {
    fprintf(stderr, "second harbor_push failed\n");
    europe_free(&eu);
    return 1;
  }
  if (eu.harbor[1].cargo_count != 2 || eu.harbor[1].cargo_types[0] != 3) {
    fprintf(stderr, "harbor cargo not stored\n");
    europe_free(&eu);
    return 1;
  }

  /* Sell sugar at tax 0: (euro_price − 1)*100 (FUN_38fd_0040). */
  eu.selected_harbor = 0;
  eu.tax_percent = 0;
  const int sugar_bid = eu.cargo[COLONIZE_CARGO_SUGAR].bid;
  const int gold_pre_sell = eu.gold;
  const int gained = europe_sell_hold(&eu, 0, 0);
  const int expect_gain = (sugar_bid - 1) * 100;
  if (gained != expect_gain || eu.gold != gold_pre_sell + expect_gain ||
      eu.harbor[0].hold_goods_amount[0] != 0) {
    fprintf(
      stderr,
      "sell failed gained=%d expect=%d gold %d→%d hold=%d\n",
      gained,
      expect_gain,
      gold_pre_sell,
      eu.gold,
      eu.harbor[0].hold_goods_amount[0]
    );
    europe_free(&eu);
    return 1;
  }

  /* Taxed sell: push sugar again on a fresh hold via buy then re-sell with tax. */
  eu.tax_percent = 50;
  const int trade_ask = eu.cargo[COLONIZE_CARGO_TRADE_GOODS].ask;
  const int gold_pre_buy = eu.gold;
  const int bought = europe_buy_cargo(&eu, 0, COLONIZE_CARGO_TRADE_GOODS, 100);
  if (bought != 100 || eu.gold != gold_pre_buy - 100 * trade_ask ||
      eu.harbor[0].hold_goods_amount[0] != 100 ||
      eu.harbor[0].hold_goods_type[0] != COLONIZE_CARGO_TRADE_GOODS) {
    fprintf(
      stderr,
      "buy trade goods failed bought=%d gold %d→%d hold=%d type=%d ask=%d\n",
      bought,
      gold_pre_buy,
      eu.gold,
      eu.harbor[0].hold_goods_amount[0],
      eu.harbor[0].hold_goods_type[0],
      trade_ask
    );
    europe_free(&eu);
    return 1;
  }

  /* Reload sugar for taxed sell check. */
  eu.harbor[0].hold_goods_type[1] = COLONIZE_CARGO_SUGAR;
  eu.harbor[0].hold_goods_amount[1] = 40;
  const int taxed = europe_sell_proceeds(&eu, COLONIZE_CARGO_SUGAR, 40);
  const int want_taxed = europe_net_after_tax((sugar_bid - 1) * 40, 50);
  if (taxed != want_taxed) {
    fprintf(stderr, "taxed proceeds expected %d got %d\n", want_taxed, taxed);
    europe_free(&eu);
    return 1;
  }
  const int gold_pre_tax_sell = eu.gold;
  const int sold_tax = europe_sell_hold(&eu, 0, 1);
  if (sold_tax != taxed || eu.gold != gold_pre_tax_sell + taxed) {
    fprintf(stderr, "taxed sell failed sold=%d expect=%d\n", sold_tax, taxed);
    europe_free(&eu);
    return 1;
  }

  const int best = europe_best_sell_hold(&eu, 0);
  if (best < 0 || eu.harbor[0].hold_goods_type[best] != COLONIZE_CARGO_TRADE_GOODS) {
    fprintf(stderr, "best sell hold expected trade goods got %d\n", best);
    europe_free(&eu);
    return 1;
  }

  /*
   * Boycott gating (fandom Boycott (Col): "goods blocked in Europe until
   * penalty paid or Fugger"). Boycott Furs, confirm buy/sell both refuse it
   * and leave state untouched, then lift and confirm trade works again.
   * Uses a scratch hold slot (2) so it doesn't disturb hold[0]/[1], which
   * later harbor_pop checks below depend on.
   * Cite: europe_cargo_boycotted / EuropeScreen.boycott_bitmap.
   */
  {
    if (europe_cargo_boycotted(&eu, COLONIZE_CARGO_FURS)) {
      fprintf(stderr, "furs should not start boycotted\n");
      europe_free(&eu);
      return 1;
    }
    eu.harbor[0].hold_goods_type[2] = COLONIZE_CARGO_FURS;
    eu.harbor[0].hold_goods_amount[2] = 30;
    eu.boycott_bitmap = (uint16_t)(1u << COLONIZE_CARGO_FURS);
    if (!europe_cargo_boycotted(&eu, COLONIZE_CARGO_FURS)) {
      fprintf(stderr, "furs should read as boycotted\n");
      europe_free(&eu);
      return 1;
    }
    const int gold_before = eu.gold;
    const int blocked_sell = europe_sell_hold(&eu, 0, 2);
    if (blocked_sell != 0 || eu.gold != gold_before ||
        eu.harbor[0].hold_goods_amount[2] != 30) {
      fprintf(stderr, "boycotted sell should be refused, got %d\n", blocked_sell);
      europe_free(&eu);
      return 1;
    }
    const int blocked_buy = europe_buy_cargo(&eu, 0, COLONIZE_CARGO_FURS, 50);
    if (blocked_buy != 0 || eu.gold != gold_before) {
      fprintf(stderr, "boycotted buy should be refused, got %d\n", blocked_buy);
      europe_free(&eu);
      return 1;
    }
    /* Lift the boycott; the same trade must now succeed. */
    eu.boycott_bitmap = 0;
    const int unblocked_sell = europe_sell_hold(&eu, 0, 2);
    if (unblocked_sell <= 0 || eu.harbor[0].hold_goods_amount[2] != 0) {
      fprintf(stderr, "sell should succeed once boycott lifted, got %d\n", unblocked_sell);
      europe_free(&eu);
      return 1;
    }
  }

  /*
   * Boycott buy-back (FUN_38fd_2dfe): pay ask*500 back taxes to lift a
   * boycott -- gold debited, nation.royal_money credited the same amount
   * (Crown REF budget, DOS write lands on that exact field), boycott bit
   * cleared. Insufficient funds leaves everything untouched.
   * Cite: GAME.TXT @SOMEBOYCOTT / @KISSUP / @KISSSORRY; europe.h
   * europe_buyback_boycott.
   */
  {
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.nation[0].boycott_bitmap = (uint16_t)(1u << COLONIZE_CARGO_FURS);
    col1.nation[0].royal_money = 1000;
    col1.nation[0].gold = 500;
    eu.boycott_bitmap = col1.nation[0].boycott_bitmap;
    eu.cargo[COLONIZE_CARGO_FURS].ask = 6;
    eu.gold = 500; /* not enough for 6*500 = 3000 */

    const int fail = europe_buyback_boycott(&eu, &col1, 0, COLONIZE_CARGO_FURS);
    if (fail != 0 || eu.gold != 500 || col1.nation[0].royal_money != 1000 ||
        !europe_cargo_boycotted(&eu, COLONIZE_CARGO_FURS)) {
      fprintf(stderr, "buyback should refuse on insufficient funds, got %d\n", fail);
      europe_free(&eu);
      return 1;
    }

    eu.gold = 5000;
    col1.nation[0].gold = 5000;
    const int paid = europe_buyback_boycott(&eu, &col1, 0, COLONIZE_CARGO_FURS);
    if (paid != 3000 || eu.gold != 2000 || col1.nation[0].gold != 2000 ||
        col1.nation[0].royal_money != 4000 ||
        europe_cargo_boycotted(&eu, COLONIZE_CARGO_FURS) ||
        (col1.nation[0].boycott_bitmap & (uint16_t)(1u << COLONIZE_CARGO_FURS)) != 0) {
      fprintf(
        stderr,
        "buyback expected paid=3000 gold=2000 royal_money=4000, got paid=%d gold=%d "
        "royal_money=%d\n",
        paid,
        eu.gold,
        (int)col1.nation[0].royal_money
      );
      europe_free(&eu);
      return 1;
    }

    /* Not boycotted: no-op. */
    const int noop = europe_buyback_boycott(&eu, &col1, 0, COLONIZE_CARGO_FURS);
    if (noop != 0) {
      fprintf(stderr, "buyback on non-boycotted cargo should no-op, got %d\n", noop);
      europe_free(&eu);
      return 1;
    }
    fprintf(stderr, "europe boycott buyback ok\n");
  }

  /* Drag session helpers. */
  {
    UiDragSession drag;
    memset(&drag, 0, sizeof(drag));
    ui_drag_begin(&drag, UI_DRAG_EUROPE_MARKET, 3, -1, 100);
    if (!ui_drag_active(&drag) || drag.index != 3 || drag.amount != 100) {
      fprintf(stderr, "ui_drag_begin failed\n");
      europe_free(&eu);
      return 1;
    }
    ui_drag_clear(&drag);
    if (ui_drag_active(&drag)) {
      fprintf(stderr, "ui_drag_clear failed\n");
      europe_free(&eu);
      return 1;
    }
  }

  /* Hit-tests. */
  {
    EuropeHitResult hit = europe_hit_test(&eu, EUROPE_LOADING_X + 4, EUROPE_LOADING_Y + 2);
    if (hit.kind != EUROPE_HIT_HARBOR_SHIP || hit.index != 0) {
      fprintf(stderr, "harbor hit expected ship0 got kind=%d idx=%d\n", (int)hit.kind, hit.index);
      europe_free(&eu);
      return 1;
    }
    hit = europe_hit_test(
      &eu, EUROPE_MARKET_X + COLONIZE_CARGO_SUGAR * EUROPE_MARKET_PITCH + 4, EUROPE_MARKET_Y + 2
    );
    if (hit.kind != EUROPE_HIT_MARKET || hit.index != COLONIZE_CARGO_SUGAR) {
      fprintf(stderr, "market hit expected sugar got kind=%d idx=%d\n", (int)hit.kind, hit.index);
      europe_free(&eu);
      return 1;
    }
    eu.selected_harbor = 0;
    hit = europe_hit_test(&eu, EUROPE_HOLD_X + 2, EUROPE_HOLD_Y + 2);
    if (hit.kind != EUROPE_HIT_HOLD || hit.index != 0) {
      fprintf(stderr, "hold hit expected 0 got kind=%d idx=%d\n", (int)hit.kind, hit.index);
      europe_free(&eu);
      return 1;
    }
    hit = europe_hit_test(&eu, EUROPE_EXIT_X + 2, EUROPE_EXIT_Y + 2);
    if (hit.kind != EUROPE_HIT_EXIT) {
      fprintf(stderr, "exit hit expected EXIT got kind=%d\n", (int)hit.kind);
      europe_free(&eu);
      return 1;
    }
    /* Empty Bound box must still be hittable (drag drop target). */
    {
      const int saved_bound = eu.bound_ships;
      eu.bound_ships = 0;
      hit = europe_hit_test(
        &eu, EUROPE_BOUND_X + EUROPE_BOUND_W / 2, EUROPE_BOUND_Y + EUROPE_BOUND_H / 2
      );
      eu.bound_ships = saved_bound;
      if (hit.kind != EUROPE_HIT_BOUND) {
        fprintf(stderr, "empty Bound hit expected BOUND got kind=%d\n", (int)hit.kind);
        europe_free(&eu);
        return 1;
      }
    }
  }

  /* Round-trip pop preserves remaining trade-goods hold. */
  int type_index = -1;
  char ship_name[32];
  int out_cargo[EUROPE_SHIP_CARGO_MAX];
  int out_count = -1;
  int out_hold_t[EUROPE_SHIP_CARGO_MAX];
  int out_hold_a[EUROPE_SHIP_CARGO_MAX];
  memset(out_hold_t, 0, sizeof(out_hold_t));
  memset(out_hold_a, 0, sizeof(out_hold_a));
  if (!europe_harbor_pop(
        &eu,
        &type_index,
        ship_name,
        sizeof(ship_name),
        out_cargo,
        &out_count,
        EUROPE_SHIP_CARGO_MAX,
        out_hold_t,
        out_hold_a,
        EUROPE_SHIP_CARGO_MAX
      ) ||
      type_index != 14 || strcmp(ship_name, "Caravel") != 0 || eu.harbor_ships != 1 ||
      out_count != 0 || out_hold_t[0] != COLONIZE_CARGO_TRADE_GOODS || out_hold_a[0] != 100) {
    fprintf(
      stderr,
      "harbor_pop goods failed (type=%d name='%s' ships=%d cargo=%d hold_t=%d hold_a=%d)\n",
      type_index,
      ship_name,
      eu.harbor_ships,
      out_count,
      out_hold_t[0],
      out_hold_a[0]
    );
    europe_free(&eu);
    return 1;
  }

  /* Push back and verify. */
  if (!europe_harbor_push(
        &eu, type_index, ship_name, out_cargo, out_count, out_hold_t, out_hold_a
      ) ||
      eu.harbor[eu.harbor_ships - 1].hold_goods_amount[0] != 100) {
    fprintf(stderr, "re-push goods failed\n");
    europe_free(&eu);
    return 1;
  }

  out_count = -1;
  if (!europe_harbor_pop(
        &eu,
        &type_index,
        ship_name,
        sizeof(ship_name),
        out_cargo,
        &out_count,
        EUROPE_SHIP_CARGO_MAX,
        NULL,
        NULL,
        0
      ) ||
      type_index != 15 || strcmp(ship_name, "Merchantman") != 0 ||
      out_count != 2 || out_cargo[0] != 3 || out_cargo[1] != 1) {
    fprintf(stderr, "second harbor_pop failed (cargo=%d)\n", out_count);
    europe_free(&eu);
    return 1;
  }

  /* Purchase prices match original_screenshots/europe/purchase.png */
  if (eu.purchase_count < 6 || eu.purchase[0].gold != 500 ||
      strcmp(eu.purchase[0].name, "Artillery") != 0 || eu.purchase[1].gold != 1000 ||
      eu.purchase[5].gold != 5000) {
    fprintf(stderr, "purchase table mismatch count=%d\n", eu.purchase_count);
    europe_free(&eu);
    return 1;
  }
  europe_cheat_add_gold(&eu, 10000);
  const int gold_pre_buy_ship = eu.gold;
  const int harbor_before = eu.harbor_ships;
  if (!europe_purchase(&eu, 1) || eu.harbor_ships != harbor_before + 1 ||
      strcmp(eu.harbor[eu.harbor_ships - 1].name, "Caravel") != 0 ||
      eu.gold != gold_pre_buy_ship - 1000) {
    fprintf(stderr, "purchase Caravel failed: %s\n", eu.status);
    europe_free(&eu);
    return 1;
  }

  /* Train: cheapest @JOB hire (Expert Ore Miners 600). */
  /*
   * DOS FUN_38fd_41ce sorts the collected @JOB costs (FUN_291f_0ed0 ->
   * FUN_1cf8_000a) before drawing a row, so the Train list is cheapest-first
   * and its first row is Expert Ore Miners at 600, not @JOB 0 (bugs.md).
   */
  for (int i = 1; i < eu.train_count; ++i) {
    if (eu.train[i].cost < eu.train[i - 1].cost) {
      fprintf(
        stderr, "train list not sorted by cost: [%d]=%s %d after [%d]=%s %d\n",
        i, eu.train[i].expert_name, eu.train[i].cost,
        i - 1, eu.train[i - 1].expert_name, eu.train[i - 1].cost
      );
      europe_free(&eu);
      return 1;
    }
  }
  if (eu.train_count > 0 && strstr(eu.train[0].expert_name, "Ore Miner") == NULL) {
    fprintf(stderr, "train list should open on Expert Ore Miners, got '%s'\n",
            eu.train[0].expert_name);
    europe_free(&eu);
    return 1;
  }
  if (eu.train_count < 1) {
    fprintf(stderr, "expected train options from @JOB\n");
    europe_free(&eu);
    return 1;
  }
  int train_i = 0;
  for (int i = 0; i < eu.train_count; ++i) {
    if (eu.train[i].cost > 0 &&
        (eu.train[train_i].cost <= 0 || eu.train[i].cost < eu.train[train_i].cost)) {
      train_i = i;
    }
  }
  const int dock_pre_train = eu.dock_count;
  const int gold_pre_train = eu.gold;
  if (!europe_train(&eu, train_i) || eu.dock_count != dock_pre_train + 1 ||
      eu.gold != gold_pre_train - eu.train[train_i].cost) {
    fprintf(stderr, "train failed: %s\n", eu.status);
    europe_free(&eu);
    return 1;
  }

  /* Voyage: enqueue Expected → tick → harbor; set_sail → Bound. */
  /* FUN_48d3_0002: 1 turn; 2 only on RNG>89 with >2 ships and no Magellan. */
  if (europe_voyage_turns_roll(NULL, false, 9) != 1) {
    fprintf(stderr, "voyage turns unexpected\n");
    europe_free(&eu);
    return 1;
  }
  {
    ColonizeDosRng vr;
    ColonizeDosRng vm;
    ColonizeDosRng vf;
    dos_rng_seed(&vr, 7u);
    dos_rng_seed(&vm, 7u);
    dos_rng_seed(&vf, 7u);
    int two = 0;
    int two_magellan = 0;
    int two_few_ships = 0;
    for (int i = 0; i < 400; ++i) {
      two += europe_voyage_turns_roll(&vr, false, 3) == 2;
      two_magellan += europe_voyage_turns_roll(&vm, true, 3) == 2;
      two_few_ships += europe_voyage_turns_roll(&vf, false, 2) == 2;
    }
    if (two == 0 || two > 100 || two_magellan != 0 || two_few_ships != 0) {
      fprintf(stderr, "voyage roll gate wrong: %d/%d/%d\n", two, two_magellan, two_few_ships);
      europe_free(&eu);
      return 1;
    }
  }
  eu.harbor_ships = 0;
  eu.expected_ships = 0;
  eu.bound_ships = 0;
  eu.dock_count = 0;
  memset(eu.dock, 0, sizeof(eu.dock));
  /* Arrival dumps passengers to dock front with board-next; preserves order. */
  {
    const int pax_types[2] = {0, 0};
    const int pax_profs[2] = {21, 20}; /* soldier then pioneer in cargo */
    if (!europe_enqueue_expected(
          &eu, 14, "Caravel", pax_types, pax_profs, 2, NULL, NULL, 50, 10, true, 1
        ) ||
        eu.expected_ships != 1 || eu.expected[0].turns_left != 1) {
      fprintf(stderr, "enqueue_expected failed\n");
      europe_free(&eu);
      return 1;
    }
  }
  /*
   * The turn a ship sails is a turn at sea: the first tick only burns the
   * departure turn, so an ordinary 1-turn crossing needs two End Turns to
   * dock, which is what DOS does (player-verified, bugs.md).
   */
  europe_tick_voyages(&eu, NULL);
  if (eu.expected_ships != 1 || eu.harbor_ships != 0 || eu.expected[0].turns_left != 1) {
    fprintf(
      stderr, "tick 1 should keep the ship at sea: expected=%d harbor=%d turns=%d\n",
      eu.expected_ships, eu.harbor_ships,
      eu.expected_ships > 0 ? eu.expected[0].turns_left : -1
    );
    europe_free(&eu);
    return 1;
  }
  europe_tick_voyages(&eu, NULL);
  if (eu.expected_ships != 0 || eu.harbor_ships != 1 || !eu.open_on_dock) {
    fprintf(
      stderr,
      "tick dock failed expected=%d harbor=%d open=%d\n",
      eu.expected_ships,
      eu.harbor_ships,
      eu.open_on_dock ? 1 : 0
    );
    europe_free(&eu);
    return 1;
  }
  if (eu.harbor[0].cargo_count != 0) {
    fprintf(stderr, "passengers should leave ship on dock, cargo=%d\n", eu.harbor[0].cargo_count);
    europe_free(&eu);
    return 1;
  }
  if (eu.dock_count != 2 || eu.dock[0].profession != 21 || eu.dock[1].profession != 20 ||
      !eu.dock[0].sentry || !eu.dock[1].sentry) {
    fprintf(
      stderr,
      "disembark dock front failed count=%d p0=%d s0=%d p1=%d s1=%d\n",
      eu.dock_count,
      eu.dock[0].profession,
      eu.dock[0].sentry ? 1 : 0,
      eu.dock[1].profession,
      eu.dock[1].sentry ? 1 : 0
    );
    europe_free(&eu);
    return 1;
  }
  /* Boarding: only sentry from front; skip non-sentry ahead of queue. */
  eu.dock_count = 3;
  snprintf(eu.dock[0].name, sizeof(eu.dock[0].name), "%s", "Indentured Servants");
  eu.dock[0].present = true;
  eu.dock[0].sentry = false;
  eu.dock[0].profession = 25;
  snprintf(eu.dock[1].name, sizeof(eu.dock[1].name), "%s", "Free Colonists");
  eu.dock[1].present = true;
  eu.dock[1].sentry = true;
  eu.dock[1].profession = 19;
  snprintf(eu.dock[2].name, sizeof(eu.dock[2].name), "%s", "Petty Criminals");
  eu.dock[2].present = true;
  eu.dock[2].sentry = true;
  eu.dock[2].profession = 26;
  if (!europe_set_sail_from_harbor(&eu, 0, 1, NULL, 0) || eu.bound_ships != 1 ||
      eu.harbor_ships != 0) {
    fprintf(stderr, "set_sail failed: %s\n", eu.status);
    europe_free(&eu);
    return 1;
  }
  if (eu.bound[0].cargo_count != 2 || eu.bound[0].cargo_professions[0] != 19 ||
      eu.bound[0].cargo_professions[1] != 26) {
    fprintf(
      stderr,
      "sentry board order failed cargo=%d p0=%d p1=%d\n",
      eu.bound[0].cargo_count,
      eu.bound[0].cargo_professions[0],
      eu.bound[0].cargo_professions[1]
    );
    europe_free(&eu);
    return 1;
  }
  if (eu.dock_count != 1 || eu.dock[0].profession != 25 || eu.dock[0].sentry) {
    fprintf(stderr, "non-sentry should remain on dock\n");
    europe_free(&eu);
    return 1;
  }
  for (int t = 0; t < eu.bound[0].turns_left + 1; ++t) {
    europe_tick_voyages(&eu, NULL);
  }
  int bx = -1, by = -1;
  bool beast = false;
  type_index = -1;
  out_count = 0;
  if (!europe_bound_pop_arrived(
        &eu,
        &type_index,
        ship_name,
        sizeof(ship_name),
        out_cargo,
        &out_count,
        EUROPE_SHIP_CARGO_MAX,
        out_hold_t,
        out_hold_a,
        EUROPE_SHIP_CARGO_MAX,
        &bx,
        &by,
        &beast
      ) ||
      strcmp(ship_name, "Caravel") != 0) {
    fprintf(stderr, "bound_pop_arrived failed\n");
    europe_free(&eu);
    return 1;
  }

  /* Recruit pool always 3; passage bumped after recruit. */
  int filled = 0;
  for (int i = 0; i < EUROPE_POOL_SIZE; ++i) {
    if (eu.pool[i].filled) {
      filled++;
    }
  }
  if (filled != EUROPE_POOL_SIZE) {
    fprintf(stderr, "pool should stay full, filled=%d\n", filled);
    europe_free(&eu);
    return 1;
  }

  /*
   * Treasure cash-in: GAME.TXT @LOOTCASH / @KINGGALLEON3 — Crown share =
   * tax_percent (same as sell). Cite: Colonization.pdf Treasure Trains.
   */
  {
    const int gold0 = eu.gold;
    eu.tax_percent = 0;
    const int full = europe_cash_treasure(&eu, 1000);
    if (full != 1000 || eu.gold != gold0 + 1000) {
      fprintf(
        stderr,
        "cash_treasure untaxed failed credited=%d gold %d→%d\n",
        full,
        gold0,
        eu.gold
      );
      europe_free(&eu);
      return 1;
    }
    /* @LOOTCASH wording, not an invented "Treasure cash-in" stub. */
    if (strstr(eu.status, "treasure fleet laden with 1000$") == NULL ||
        strstr(eu.status, "arrives safely in") == NULL ||
        strstr(eu.status, "1000$ added to") == NULL) {
      fprintf(stderr, "cash_treasure untaxed status '%s'\n", eu.status);
      europe_free(&eu);
      return 1;
    }
    eu.tax_percent = 50;
    const int gold1 = eu.gold;
    const int half = europe_cash_treasure(&eu, 1000);
    if (half != 500 || eu.gold != gold1 + 500) {
      fprintf(
        stderr,
        "cash_treasure 50%% fee failed credited=%d gold %d→%d\n",
        half,
        gold1,
        eu.gold
      );
      europe_free(&eu);
      return 1;
    }
    if (europe_cash_treasure(&eu, 0) != 0 || europe_cash_treasure(NULL, 500) != 0) {
      fprintf(stderr, "cash_treasure should no-op on bad args\n");
      europe_free(&eu);
      return 1;
    }
    /* FUN_48d3_06ba: Crown cut caps at 50% even if tax_rate is higher. */
    eu.tax_percent = 75;
    const int gold2 = eu.gold;
    const int capped = europe_cash_treasure(&eu, 1000);
    if (capped != 500 || eu.gold != gold2 + 500) {
      fprintf(
        stderr,
        "cash_treasure 75%% tax must cap at 50%% credited=%d gold %d→%d\n",
        capped,
        gold2,
        eu.gold
      );
      europe_free(&eu);
      return 1;
    }
  }

  /* Disembark Treasure passenger: cash-in + do not land as dock immigrant. */
  {
    ColonizeMsgCatalog names;
    ColonizeUnitPool units;
    memset(&names, 0, sizeof(names));
    units_reset(&units);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !units_load_types(&units, &names)) {
      fprintf(stderr, "treasure disembark: load NAMES/units failed\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    const int treasure_ti = units_find_type(&units, "Treasure");
    if (treasure_ti < 0) {
      fprintf(stderr, "Treasure type missing from NAMES\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    eu.harbor_ships = 0;
    eu.expected_ships = 0;
    eu.bound_ships = 0;
    eu.dock_count = 0;
    memset(eu.dock, 0, sizeof(eu.dock));
    eu.tax_percent = 25;
    const int gold_pre = eu.gold;
    const int pax_types[1] = {treasure_ti};
    const int pax_profs[1] = {-1};
    if (!europe_enqueue_expected(
          &eu, 16, "Galleon", pax_types, pax_profs, 1, NULL, NULL, 50, 10, true, 1
        )) {
      fprintf(stderr, "treasure enqueue_expected failed\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    eu.expected[0].cargo_treasure_gold[0] = 800;
    eu.expected[0].turns_left = 0;
    europe_tick_voyages(&eu, &units);
    const int expect_credit = (800 * 75) / 100;
    if (eu.gold != gold_pre + expect_credit) {
      fprintf(
        stderr,
        "treasure disembark gold %d→%d expect +%d (tax 25%%)\n",
        gold_pre,
        eu.gold,
        expect_credit
      );
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    if (eu.dock_count != 0) {
      fprintf(stderr, "Treasure must not land on dock, dock_count=%d\n", eu.dock_count);
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    if (eu.harbor_ships != 1 || eu.harbor[0].cargo_count != 0) {
      fprintf(stderr, "Treasure should leave empty holds after cash-in\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    assets_msg_free(&names);
  }

  /*
   * Map/transport sell (no harbor): europe_sell_unit_hold → europe_sell_proceeds
   * tax path (bid × amt × (100−tax)/100). Cite: Colonization.pdf Europe sell.
   */
  {
    ColonizeMsgCatalog names;
    ColonizeUnitPool units;
    memset(&names, 0, sizeof(names));
    units_reset(&units);
    if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT") ||
        !units_load_types(&units, &names)) {
      fprintf(stderr, "sell_unit_hold: load NAMES/units failed\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    const int caravel = units_find_type(&units, "Caravel");
    if (caravel < 0) {
      fprintf(stderr, "sell_unit_hold: Caravel type missing\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    const int sid = units_spawn_allow_stack(&units, caravel, 2, 2);
    ColonizeUnit* ship = units_get(&units, sid);
    if (!ship) {
      fprintf(stderr, "sell_unit_hold: spawn failed\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    ship->nation_id = 0;
    ship->hold_goods_type[0] = COLONIZE_CARGO_SUGAR;
    ship->hold_goods_amount[0] = 50;
    eu.tax_percent = 50;
    const int sugar_bid = eu.cargo[COLONIZE_CARGO_SUGAR].bid;
    const int expect = europe_net_after_tax((sugar_bid - 1) * 50, 50);
    const int gold0 = eu.gold;
    const int gained = europe_sell_unit_hold(&eu, &units, sid, 0);
    if (gained != expect || eu.gold != gold0 + expect ||
        ship->hold_goods_amount[0] != 0 || ship->hold_goods_type[0] != 0) {
      fprintf(
        stderr,
        "sell_unit_hold failed gained=%d expect=%d gold %d→%d hold=%d type=%d\n",
        gained,
        expect,
        gold0,
        eu.gold,
        ship->hold_goods_amount[0],
        ship->hold_goods_type[0]
      );
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    if (europe_sell_unit_hold(&eu, &units, sid, 0) != 0 ||
        europe_sell_unit_hold(NULL, &units, sid, 0) != 0) {
      fprintf(stderr, "sell_unit_hold should no-op on empty/null\n");
      assets_msg_free(&names);
      europe_free(&eu);
      return 1;
    }
    assets_msg_free(&names);
  }

  fprintf(
    stderr,
    "europe tests ok (cargo=%d train=%d purchase=%d gold=%d dock=%d)\n",
    eu.cargo_count,
    eu.train_count,
    eu.purchase_count,
    eu.gold,
    eu.dock_count
  );

  /*
   * GAME.TXT @ARMOPTIONS dock menu (bugs.md): DOS FUN_38fd_37xx omits the
   * rows it disabled, so a plain colonist gets the three queue rows plus buy
   * Muskets / buy Tools / buy Horses / Bless / No changes, and arming him
   * turns him into Soldiers, charges 50 x the Muskets ask, and swaps the
   * buy row for the matching sell row.
   */
  {
    EuropeScreen arm;
    char aerr[128];
    if (!europe_load(&arm, "COLONIZE", aerr, sizeof(aerr))) {
      fprintf(stderr, "armoptions: reload failed: %s\n", aerr);
      europe_free(&eu);
      return 1;
    }
    europe_cheat_add_gold(&arm, 50000);
    arm.dock_count = 0;
    memset(arm.dock, 0, sizeof(arm.dock));
    if (!europe_dock_push_load(&arm, "Free Colonists", UNITS_JOB_COLONIST)) {
      fprintf(stderr, "armoptions: dock push failed\n");
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    if (arm.dock[0].dos_type != EUROPE_DOCK_TYPE_COLONISTS) {
      fprintf(stderr, "armoptions: fresh immigrant should be Colonists, got %d\n",
              arm.dock[0].dos_type);
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    arm.menu_dock_index = 0;
    europe_build_dock_menu(&arm, NULL, 0);
    int has_buy_muskets = 0, has_sell_muskets = 0, has_bless = 0, has_unbless = 0;
    int has_to_front = 0, has_no_changes = 0;
    for (int i = 0; i < arm.dock_menu_count; ++i) {
      switch (arm.dock_menu_row[i]) {
        case EUROPE_ARM_ROW_BUY_MUSKETS: has_buy_muskets = 1; break;
        case EUROPE_ARM_ROW_SELL_MUSKETS: has_sell_muskets = 1; break;
        case EUROPE_ARM_ROW_BLESS: has_bless = 1; break;
        case EUROPE_ARM_ROW_UNBLESS: has_unbless = 1; break;
        case EUROPE_ARM_ROW_TO_FRONT: has_to_front = 1; break;
        case EUROPE_ARM_ROW_NO_CHANGES: has_no_changes = 1; break;
        default: break;
      }
    }
    if (!has_buy_muskets || has_sell_muskets || !has_bless || has_unbless ||
        has_to_front /* index 0 is already the front */ || !has_no_changes) {
      fprintf(
        stderr,
        "armoptions rows wrong: buy=%d sell=%d bless=%d unbless=%d front=%d none=%d (count=%d)\n",
        has_buy_muskets, has_sell_muskets, has_bless, has_unbless, has_to_front,
        has_no_changes, arm.dock_menu_count
      );
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    const int musket_cost =
      europe_buy_price(&arm, COLONIZE_CARGO_MUSKETS) * EUROPE_ARM_MUSKETS;
    const int gold_before = arm.gold;
    if (!europe_apply_dock_menu_row(&arm, NULL, -1, 0, EUROPE_ARM_ROW_BUY_MUSKETS)) {
      fprintf(stderr, "armoptions: arm with muskets failed\n");
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    if (arm.dock[0].dos_type != EUROPE_DOCK_TYPE_SOLDIERS ||
        arm.gold != gold_before - musket_cost) {
      fprintf(stderr, "armoptions: arm result type=%d gold=%d expected type=1 gold=%d\n",
              arm.dock[0].dos_type, arm.gold, gold_before - musket_cost);
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    /* Now the Muskets row flips to Sell, and Horses upgrades to Dragoons. */
    europe_build_dock_menu(&arm, NULL, 0);
    has_buy_muskets = 0;
    has_sell_muskets = 0;
    for (int i = 0; i < arm.dock_menu_count; ++i) {
      if (arm.dock_menu_row[i] == EUROPE_ARM_ROW_BUY_MUSKETS) has_buy_muskets = 1;
      if (arm.dock_menu_row[i] == EUROPE_ARM_ROW_SELL_MUSKETS) has_sell_muskets = 1;
    }
    if (has_buy_muskets || !has_sell_muskets) {
      fprintf(stderr, "armoptions: after arming buy=%d sell=%d\n",
              has_buy_muskets, has_sell_muskets);
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    if (!europe_apply_dock_menu_row(&arm, NULL, -1, 0, EUROPE_ARM_ROW_BUY_HORSES) ||
        arm.dock[0].dos_type != EUROPE_DOCK_TYPE_DRAGOONS) {
      fprintf(stderr, "armoptions: soldier + horses should be Dragoons, got %d\n",
              arm.dock[0].dos_type);
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    if (!europe_apply_dock_menu_row(&arm, NULL, -1, 0, EUROPE_ARM_ROW_SELL_MUSKETS) ||
        arm.dock[0].dos_type != EUROPE_DOCK_TYPE_SCOUTS) {
      fprintf(stderr, "armoptions: dragoon minus muskets should be Scouts, got %d\n",
              arm.dock[0].dos_type);
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    /*
     * UNBLESS polarity (DOS 38fd:39ec..3a09): only a *blessed* ordinary
     * colonist (type Missionaries, profession != 0x18) gets "Cancel
     * Missionary Status"; a born Jesuit Missionary (profession 0x18) does
     * not. Was inverted (bugs.md).
     */
    if (!europe_apply_dock_menu_row(&arm, NULL, -1, 0, EUROPE_ARM_ROW_SELL_HORSES) ||
        !europe_apply_dock_menu_row(&arm, NULL, -1, 0, EUROPE_ARM_ROW_BLESS) ||
        arm.dock[0].dos_type != EUROPE_DOCK_TYPE_MISSIONARIES) {
      fprintf(stderr, "armoptions: bless colonist failed, type=%d\n", arm.dock[0].dos_type);
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    europe_build_dock_menu(&arm, NULL, 0);
    has_unbless = 0;
    for (int i = 0; i < arm.dock_menu_count; ++i) {
      if (arm.dock_menu_row[i] == EUROPE_ARM_ROW_UNBLESS) has_unbless = 1;
    }
    if (!has_unbless) {
      fprintf(stderr, "armoptions: blessed colonist should offer Cancel Missionary Status\n");
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    arm.dock[0].profession = 0x18; /* born Jesuit Missionary */
    europe_build_dock_menu(&arm, NULL, 0);
    has_unbless = 0;
    for (int i = 0; i < arm.dock_menu_count; ++i) {
      if (arm.dock_menu_row[i] == EUROPE_ARM_ROW_UNBLESS) has_unbless = 1;
    }
    if (has_unbless) {
      fprintf(stderr, "armoptions: Jesuit Missionary must not offer Cancel Missionary Status\n");
      europe_free(&arm);
      europe_free(&eu);
      return 1;
    }
    europe_free(&arm);
    fprintf(stderr, "europe @ARMOPTIONS dock menu ok\n");
  }

  /*
   * Volume prices: FUN_38fd_1dfa/0058 — sell pushes nr toward fall*100 → bid−1.
   * Sugar: fall=6 vol=1 → sell 300 tons (delta 600) drops bid 4→3; ask=bid+1+1=5.
   * Cite: NAMES.TXT @CARGO; viceroy_unpacked.c FUN_38fd_0058.
   */
  {
    EuropeScreen vol;
    char verr[128];
    if (!europe_load(&vol, "COLONIZE", verr, sizeof(verr))) {
      fprintf(stderr, "volume: reload failed: %s\n", verr);
      europe_free(&eu);
      return 1;
    }
    europe_cheat_add_gold(&vol, 50000);
    /* Need a harbor ship to buy/sell holds. */
    if (vol.harbor_ships < 1) {
      EuropeHarborShip* s = &vol.harbor[0];
      memset(s, 0, sizeof(*s));
      snprintf(s->name, sizeof(s->name), "Caravel");
      s->type_index = 0;
      vol.harbor_ships = 1;
      vol.selected_harbor = 0;
    }
    const int sugar = COLONIZE_CARGO_SUGAR;
    const int bid0 = vol.cargo[sugar].bid;
    if (bid0 != 4) {
      fprintf(stderr, "volume: Sugar start bid want 4 got %d\n", bid0);
      europe_free(&vol);
      europe_free(&eu);
      return 1;
    }
    /* Direct volume apply: sell 400 (<<1 = 800) drops bid past fall*100. */
    europe_apply_volume_price(&vol, sugar, 400, 0);
    if (vol.cargo[sugar].bid != 3 || vol.cargo[sugar].ask != 4) {
      fprintf(
        stderr,
        "volume: after sell-400 Sugar bid/ask=%d/%d want 3/4 nr=%d\n",
        vol.cargo[sugar].bid,
        vol.cargo[sugar].ask,
        (int)vol.trade_nr[sugar]
      );
      europe_free(&vol);
      europe_free(&eu);
      return 1;
    }
    /* Buy 300 (<<1=600) raises bid back via rise*100. */
    europe_apply_volume_price(&vol, sugar, 300, 1);
    if (vol.cargo[sugar].bid != 4) {
      fprintf(
        stderr,
        "volume: after buy-300 Sugar bid=%d want 4 nr=%d\n",
        vol.cargo[sugar].bid,
        (int)vol.trade_nr[sugar]
      );
      europe_free(&vol);
      europe_free(&eu);
      return 1;
    }
    fprintf(stderr, "europe volume price rise/fall ok\n");
    europe_free(&vol);
  }

  /*
   * FUN_38fd_1dfa exact ledger, replaying the real-DOS dutch2 t169→t170
   * lumber sales (original_saves/colony-prod-tests, Viceroy, human = Dutch
   * slot 3): 54 by the human, 12 + 18 by AI nations. DOS moved every
   * non-Dutch nr[Lumber] by +93 and the Dutch one by +61; lumber has
   * volatility 0 and attrition 0, so the sale terms are the whole delta.
   */
  {
    EuropeScreen led;
    if (!europe_load(&led, "COLONIZE", err, sizeof(err))) {
      fprintf(stderr, "europe_load (ledger): %s\n", err);
      europe_free(&eu);
      return 1;
    }
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.difficulty = 4;
    col1.nation[3].tax_rate = 35;
    const int lumber = COLONIZE_CARGO_LUMBER;
    led.cargo[lumber].bid = 2; /* sells at 1 */
    /* Non-Dutch human view first: human = slot 0 sells 54 at Viceroy. */
    led.trade_nr[lumber] = 0;
    europe_apply_trade_volume(&led, &col1, 0, 0, lumber, 54, 0, 0);
    europe_apply_trade_volume(&led, &col1, 1, 0, lumber, 12, 0, 0);
    europe_apply_trade_volume(&led, &col1, 2, 0, lumber, 18, 0, 0);
    if (led.trade_nr[lumber] != 93) {
      fprintf(stderr, "1dfa non-Dutch lumber nr want 93 got %d\n", (int)led.trade_nr[lumber]);
      europe_free(&led);
      europe_free(&eu);
      return 1;
    }
    /* Dutch human (slot 3): every contribution ×2/3, per seller. */
    led.trade_nr[lumber] = 0;
    europe_apply_trade_volume(&led, &col1, 3, 3, lumber, 54, 0, 0);
    europe_apply_trade_volume(&led, &col1, 0, 3, lumber, 12, 0, 0);
    europe_apply_trade_volume(&led, &col1, 1, 3, lumber, 18, 0, 0);
    if (led.trade_nr[lumber] != 61 || col1.nation[3].trade.tons[lumber] != 54 ||
        col1.nation[3].trade.tons2[lumber] != 54 || col1.nation[3].trade.gold[lumber] != 35) {
      fprintf(
        stderr,
        "1dfa Dutch lumber nr want 61 got %d (tons %d tons2 %d gold %d want 54/54/35)\n",
        (int)led.trade_nr[lumber],
        (int)col1.nation[3].trade.tons[lumber],
        (int)col1.nation[3].trade.tons2[lumber],
        (int)col1.nation[3].trade.gold[lumber]
      );
      europe_free(&led);
      europe_free(&eu);
      return 1;
    }
    /* Treasury side of the same sale: 54 gross, 35% → 54 − 18 = 36. */
    if (europe_net_after_tax(54, 35) != 36) {
      fprintf(stderr, "net_after_tax(54,35) want 36 got %d\n", europe_net_after_tax(54, 35));
      europe_free(&led);
      europe_free(&eu);
      return 1;
    }
    fprintf(stderr, "europe 1dfa sale ledger (dutch2 pair) ok\n");
    europe_free(&led);
  }

  /* EOT attrition tick: Trade Goods attrition=+4 kept on nr (0058 all-cargo). */
  {
    EuropeScreen tick;
    char terr[128];
    if (!europe_load(&tick, "COLONIZE", terr, sizeof(terr))) {
      fprintf(stderr, "tick: reload failed: %s\n", terr);
      europe_free(&eu);
      return 1;
    }
    const int tg = COLONIZE_CARGO_TRADE_GOODS;
    europe_apply_volume_price(&tick, tg, 50, 0);
    const int nr1 = tick.trade_nr[tg];
    const int attr = tick.cargo[tg].attrition;
    europe_tick_market_prices(&tick, NULL, NULL, 0, 0u);
    if (tick.trade_nr[tg] != (int16_t)(nr1 + attr)) {
      fprintf(
        stderr,
        "tick: Trade Goods nr %d→%d want %+d attrition\n",
        nr1,
        (int)tick.trade_nr[tg],
        attr
      );
      europe_free(&tick);
      europe_free(&eu);
      return 1;
    }
    if (tick.cargo[tg].ask != tick.cargo[tg].bid + tick.cargo[tg].burden) {
      fprintf(stderr, "tick: ask/burden after EOT\n");
      europe_free(&tick);
      europe_free(&eu);
      return 1;
    }
    fprintf(stderr, "europe EOT market attrition tick ok\n");

    /* Phase 4 rise/fall status crumb. */
    tick.cargo[tg].bid = 10;
    tick.cargo[tg].low = 1;
    tick.cargo[tg].high = 20;
    tick.cargo[tg].fall = 1;
    tick.cargo[tg].rise = 1;
    tick.cargo[tg].attrition = 0;
    tick.trade_nr[tg] = 100; /* ≥ fall*100 → bid−1 */
    tick.status[0] = '\0';
    europe_tick_market_prices(&tick, NULL, NULL, 0, 0u);
    /* Real DOS wording, GAME.TXT @PRICEDOWN (COLONIZE/GAME.TXT:1687-1689):
       "The price of {cargo} in {port} has fallen to {bid}." */
    if (tick.cargo[tg].bid != 9 || strstr(tick.status, "has fallen to 9") == NULL ||
        strstr(tick.status, "Trade Goods") == NULL) {
      fprintf(
        stderr,
        "market fall status want bid=9+'has fallen to 9'+'Trade Goods' got bid=%d '%s'\n",
        tick.cargo[tg].bid,
        tick.status
      );
      europe_free(&tick);
      europe_free(&eu);
      return 1;
    }
    fprintf(stderr, "europe market rise/fall status ok\n");
    europe_free(&tick);
  }

  /* Colony → price_group_state half peel (0058 / DS:0x53ea). */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* col = &pool.colonies[0];
    memset(col, 0, sizeof(*col));
    col->active = true;
    col->id = 1;
    col->nation_id = 0;
    col->building_in_production = -1;
    col->stock[COLONIZE_CARGO_FOOD] = 256; /* >>7 = 2 */
    pool.colony_count = 1;

    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.price_group_state[COLONIZE_CARGO_FOOD] = 10;
    /* DOS 0058 phase 1 (2026-08-28, golden_market_prices01): ledger = pool +
     * Σ max(0, nation tons2); decay = ledger >> 7 in nation 0's pass. Colony
     * stock is not part of it. 10 + 246 = 256 >> 7 = 2. */
    col1.nation[0].trade.tons2[COLONIZE_CARGO_FOOD] = 246;
    col1.nation[1].trade.tons2[COLONIZE_CARGO_FOOD] = -500; /* negative ledgers clamp to 0 */

    EuropeScreen tick;
    memset(&tick, 0, sizeof(tick));
    tick.cargo_count = COLONIZE_CARGO_COUNT;
    europe_tick_market_prices(&tick, &col1, &pool, 0, 0u);
    if (col1.head.price_group_state[COLONIZE_CARGO_FOOD] != 8) {
      fprintf(
        stderr,
        "price_group decay want 8 got %u\n",
        (unsigned)col1.head.price_group_state[COLONIZE_CARGO_FOOD]
      );
      return 1;
    }
    fprintf(stderr, "europe price_group colony half ok\n");
  }

  /*
   * 0058 phases 2–3: cargos 9..12 pressure *100; cargos 1..4 no *100.
   * bid > ratio → +mid; bid < ratio → −mid.
   */
  {
    ColonizeCol1Save col1;
    memset(&col1, 0, sizeof(col1));
    col1.head.year = 1492;
    /* Ledgers: rum-group all 10 → sum=40, ratio=(40*3)/10=12. */
    for (int c = 9; c <= 12; ++c) {
      col1.head.price_group_state[c] = 10;
    }
    /* Sugar ledger 10; half food 0 + sugar..tobacco → sum for phase3. */
    col1.head.price_group_state[COLONIZE_CARGO_SUGAR] = 10;
    col1.head.price_group_state[COLONIZE_CARGO_TOBACCO] = 10;
    col1.head.price_group_state[COLONIZE_CARGO_COTTON] = 10;
    col1.head.price_group_state[COLONIZE_CARGO_FURS] = 20; /* halved → 10 */

    EuropeScreen tick;
    memset(&tick, 0, sizeof(tick));
    tick.cargo_count = COLONIZE_CARGO_COUNT;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      tick.cargo[c].rise = 4;
      tick.cargo[c].fall = 4;
      tick.cargo[c].low = 1;
      tick.cargo[c].high = 20;
      tick.cargo[c].attrition = 0;
    }
    /* Rum bid 20 > ratio 12 → sign +1 → nr += 4*100 = 400. */
    tick.cargo[COLONIZE_CARGO_RUM].bid = 20;
    /* Cigars bid 1 < 12 → nr -= 400. */
    tick.cargo[COLONIZE_CARGO_CIGARS].bid = 1;
    /* Sugar bid 50 >> ratio → +mid (4, no *100). */
    tick.cargo[COLONIZE_CARGO_SUGAR].bid = 50;

    europe_tick_market_prices(&tick, &col1, NULL, 0, 0u);
    /* Phase2 ±400 then phase4 rise/fall absorbs ±(mid*100) into bid. */
    if (tick.cargo[COLONIZE_CARGO_RUM].bid != 19 || tick.trade_nr[COLONIZE_CARGO_RUM] != 0) {
      fprintf(
        stderr,
        "phase2 rum bid/nr want 19/0 got %d/%d\n",
        tick.cargo[COLONIZE_CARGO_RUM].bid,
        (int)tick.trade_nr[COLONIZE_CARGO_RUM]
      );
      return 1;
    }
    if (tick.cargo[COLONIZE_CARGO_CIGARS].bid != 2 || tick.trade_nr[COLONIZE_CARGO_CIGARS] != 0) {
      fprintf(
        stderr,
        "phase2 cigars bid/nr want 2/0 got %d/%d\n",
        tick.cargo[COLONIZE_CARGO_CIGARS].bid,
        (int)tick.trade_nr[COLONIZE_CARGO_CIGARS]
      );
      return 1;
    }
    /* Phase3 +4 mid stays in nr (below fall*100 threshold). */
    if (tick.trade_nr[COLONIZE_CARGO_SUGAR] != 4) {
      fprintf(stderr, "phase3 sugar nr want 4 got %d\n", (int)tick.trade_nr[COLONIZE_CARGO_SUGAR]);
      return 1;
    }
    fprintf(stderr, "europe market phase2/3 pressure ok\n");
  }

  europe_free(&eu);
  diag_shutdown();
  return 0;
}
