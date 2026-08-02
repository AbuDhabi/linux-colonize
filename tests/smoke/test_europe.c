#include <stdio.h>
#include <string.h>

#include "core/colony.h"
#include "core/europe.h"
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
  if (!europe_recruit(&eu)) {
    fprintf(stderr, "recruit failed: %s\n", eu.status);
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

  /* Hit-tests. */
  {
    EuropeHitResult hit = europe_hit_test(&eu, EUROPE_HARBOR_LIST_X + 4, EUROPE_HARBOR_LIST_Y + 2);
    if (hit.kind != EUROPE_HIT_HARBOR_SHIP || hit.index != 0) {
      fprintf(stderr, "harbor hit expected ship0 got kind=%d idx=%d\n", (int)hit.kind, hit.index);
      europe_free(&eu);
      return 1;
    }
    hit = europe_hit_test(
      &eu, EUROPE_MARKET_X + 4, EUROPE_MARKET_Y + COLONIZE_CARGO_SUGAR * EUROPE_MARKET_ROW_H + 2
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

  fprintf(
    stderr,
    "europe tests ok (cargo=%d classes=%d gold=%d dock=%d harbor=%d)\n",
    eu.cargo_count,
    eu.class_count,
    eu.gold,
    eu.dock_count,
    eu.harbor_ships
  );
  europe_free(&eu);
  diag_shutdown();
  return 0;
}
