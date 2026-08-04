#include <stdio.h>
#include <string.h>

#include "core/colony.h"
#include "core/europe.h"
#include "core/ui_drag.h"
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

  /* Food: start_lo=1, burden=7 → ask = bid + burden + 1 = 9 */
  if (strcmp(eu.cargo[0].name, "Food") != 0 || eu.cargo[0].bid != 1 || eu.cargo[0].ask != 9) {
    fprintf(
      stderr,
      "Food quote expected bid=1 ask=9 got name='%s' bid=%d ask=%d\n",
      eu.cargo[0].name,
      eu.cargo[0].bid,
      eu.cargo[0].ask
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

  /* Sell sugar at tax 0: bid*100. */
  eu.selected_harbor = 0;
  eu.tax_percent = 0;
  const int sugar_bid = eu.cargo[COLONIZE_CARGO_SUGAR].bid;
  const int gold_pre_sell = eu.gold;
  const int gained = europe_sell_hold(&eu, 0, 0);
  const int expect_gain = sugar_bid * 100;
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
  if (taxed != (sugar_bid * 40 * 50) / 100) {
    fprintf(stderr, "taxed proceeds expected %d got %d\n", (sugar_bid * 40 * 50) / 100, taxed);
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
  if (europe_voyage_turns(true, 4) != EUROPE_VOYAGE_EAST_TURNS ||
      europe_voyage_turns(false, 4) != EUROPE_VOYAGE_WEST_TURNS) {
    fprintf(stderr, "voyage turns unexpected\n");
    europe_free(&eu);
    return 1;
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
          &eu, 14, "Caravel", pax_types, pax_profs, 2, NULL, NULL, 50, 10, true, 4
        ) ||
        eu.expected_ships != 1 || eu.expected[0].turns_left != EUROPE_VOYAGE_EAST_TURNS) {
      fprintf(stderr, "enqueue_expected failed\n");
      europe_free(&eu);
      return 1;
    }
  }
  for (int t = 0; t < EUROPE_VOYAGE_EAST_TURNS; ++t) {
    europe_tick_voyages(&eu, NULL);
  }
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
  if (!europe_set_sail_from_harbor(&eu, 0, 4, NULL) || eu.bound_ships != 1 ||
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

  fprintf(
    stderr,
    "europe tests ok (cargo=%d train=%d purchase=%d gold=%d dock=%d)\n",
    eu.cargo_count,
    eu.train_count,
    eu.purchase_count,
    eu.gold,
    eu.dock_count
  );
  europe_free(&eu);
  diag_shutdown();
  return 0;
}
