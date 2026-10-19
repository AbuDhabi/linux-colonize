/*
 * port_plan P6.1: the Europe market after one real DOS turn. Each pair is a
 * DOS save plus the save after one DOS turn with no player moves. The port
 * plays the whole turn (golden_turn: production, Custom Houses, AI Europe
 * trade, FUN_38fd_0058 per nation) and then every nation's trade ledger
 * (nr / euro_price / tons / tons2) and head.market_demand_pool[16] must
 * match the after-save.
 *
 * Until 2026-10-18 this test ran the market tick alone and skipped 4 + 7
 * human cargos "with residue"; the residue was the colony-side and AI-side
 * ledger writes the tick-only run never made. Played in full, the
 * no-transports pair matches on every slot. The dutch2 pair keeps two
 * exclusions (port_bugs.md #1085):
 *   - Horses, all nations: the DOS French AI bought 50 Horses in Europe
 *     that turn (FUN_4d56_4528 equip arm, 1d80 lands -34 on all four nr
 *     rows); this fixture has no VR seed, so the port's AI stream does not
 *     reproduce that decision.
 *   - Muskets, human row only: DOS -5, port 7. DOSBox BPM trace
 *     (2026-10-18): the Dutch word is written only by 1dfa (+45, English
 *     100-musket sale), 1d80 (an AI Europe buy) and 0058 attrition (+6);
 *     the archive's -12 is an AI buy of ~17 muskets from the player's
 *     unsaved session. A DOS from-load replay has the French buy 50
 *     muskets instead of the archive's 50 horses, so neither residue is
 *     reproducible even in DOS.
 */
#include <stdio.h>
#include <string.h>

#include "tests/common/golden_fixture.h"
#include "../common/test_runner.h"

typedef struct PairSpec {
  const char* before;
  const char* after;
  uint16_t skip_all_mask;   /* cargos excluded on every nation */
  uint16_t skip_human_mask; /* cargos excluded on the human row only */
} PairSpec;

static int run_pair(const PairSpec* ps) {
  GoldenFixture fx;
  if (!golden_open(ps->before, ps->after, 100u, &fx)) {
    golden_close(&fx);
    return 1;
  }
  if (!golden_turn(&fx)) {
    golden_close(&fx);
    return 1;
  }
  int rc = 0;
  for (int c = 0; c < 16; ++c) {
    if (fx.start.head.market_demand_pool[c] != fx.expect.head.market_demand_pool[c]) {
      fprintf(stderr, "%s: market_demand_pool[%d] want %u got %u\n", ps->before, c,
              fx.expect.head.market_demand_pool[c], fx.start.head.market_demand_pool[c]);
      rc = 1;
    }
  }
  for (int n = 0; n < 4; ++n) {
    const ColonizeCol1NationTrade* got = &fx.start.nation[n].trade;
    const ColonizeCol1NationTrade* want = &fx.expect.nation[n].trade;
    for (int c = 0; c < 16; ++c) {
      if (ps->skip_all_mask & (1u << c)) continue;
      if (n == (int)fx.br.human_nation && (ps->skip_human_mask & (1u << c))) continue;
      if (got->nr[c] != want->nr[c] || got->euro_price[c] != want->euro_price[c] ||
          got->tons[c] != want->tons[c] || got->tons2[c] != want->tons2[c]) {
        fprintf(stderr,
                "%s: nation %d cargo %d: nr %d/%d price %d/%d tons %d/%d tons2 %d/%d (got/want)\n",
                ps->before, n, c, (int)got->nr[c], (int)want->nr[c], (int)got->euro_price[c],
                (int)want->euro_price[c], (int)got->tons[c], (int)want->tons[c],
                (int)got->tons2[c], (int)want->tons2[c]);
        rc = 1;
      }
    }
  }
  golden_close(&fx);
  if (rc == 0) {
    printf("golden_market_prices01: %s ok\n", ps->before);
  }
  return rc;
}

static int case_dutch2(void) {
  static const PairSpec ps = {
    "original_saves/colony-prod-tests/COLONY00-dutch2-t0.SAV",
    "original_saves/colony-prod-tests/COLONY01-dutch2-t1.SAV",
    1u << 8, 1u << 15,
  };
  return run_pair(&ps);
}

static int case_no_transports(void) {
  static const PairSpec ps = {
    "original_saves/colony-prod-tests/COLONY00_no-transports.SAV",
    "original_saves/colony-prod-tests/COLONY01_no-transports.SAV",
    0, 0,
  };
  return run_pair(&ps);
}

static const TestCase k_cases[] = {
  {"dutch2_t169_t170", case_dutch2},
  {"no_transports_t268_t269", case_no_transports},
};

TEST_MAIN(k_cases)
