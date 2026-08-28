/*
 * port_plan P6.1: FUN_38fd_0058 EOT market tick vs two real-DOS turn pairs.
 * Loads the "before" save, runs europe_tick_market_prices once (the human's
 * 5e52 phase-3 call, with nation 0's pool decay folded in), and compares:
 *   - head.price_group_state[16] against the "after" save, all 16 slots
 *     (decay = (pool + Σ max(0, nation tons2)) >> 7, only while nation 0 is
 *     not withdrawn — the no-transports pair has nation 0 at control 2 and
 *     its pool is byte-identical across the turn, the dutch2 pair decays);
 *   - the human's trade.nr / euro_price for every cargo that had no sale,
 *     purchase or colony-side feedback that turn (the excluded slots are
 *     listed per pair; their residuals are the still-open 1dfa / 364b_0688
 *     volume + production terms — see docs/port_plan.md P6.1).
 * Derived 2026-08-28 from a python replica of the decompile iterated until it
 * matched both pairs; column roles rise=@CARGO c6, fall=c7, attrition=c8.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"

typedef struct PairSpec {
  const char* before;
  const char* after;
  uint16_t skip_mask; /* human cargos with volume/feedback residue that turn */
} PairSpec;

static int run_pair(const PairSpec* ps) {
  char err[256];
  ColonizeCol1Save a;
  ColonizeCol1Save b;
  col1_save_init(&a);
  col1_save_init(&b);
  if (!col1_save_read_file(ps->before, &a, err, sizeof(err)) ||
      !col1_save_read_file(ps->after, &b, err, sizeof(err))) {
    fprintf(stderr, "read: %s\n", err);
    return 1;
  }
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    return 1;
  }
  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    return 1;
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    return 1;
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  if (!europe_load(&europe, "COLONIZE", err, sizeof(err))) {
    fprintf(stderr, "europe_load: %s\n", err);
    return 1;
  }
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&a, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge: %s\n", err);
    return 1;
  }
  const int human = br.human_nation;
  /* DOS increments the turn before the nation passes run. */
  europe_tick_market_prices(&europe, &a, &colonies, human, (uint32_t)a.head.turn + 1u);

  int rc = 0;
  for (int c = 0; c < 16; ++c) {
    if (a.head.price_group_state[c] != b.head.price_group_state[c]) {
      fprintf(
        stderr, "%s: price_group_state[%d] want %u got %u\n", ps->before, c,
        b.head.price_group_state[c], a.head.price_group_state[c]
      );
      rc = 1;
    }
  }
  const ColonizeCol1NationTrade* tb = &b.nation[human].trade;
  for (int c = 0; c < 16; ++c) {
    if (ps->skip_mask & (1u << c)) {
      continue;
    }
    if (europe.trade_nr[c] != tb->nr[c]) {
      fprintf(
        stderr, "%s: human nr[%d] want %d got %d\n", ps->before, c, (int)tb->nr[c],
        (int)europe.trade_nr[c]
      );
      rc = 1;
    }
    if (europe.cargo[c].bid != (int)tb->euro_price[c]) {
      fprintf(
        stderr, "%s: human bid[%d] want %d got %d\n", ps->before, c, (int)tb->euro_price[c],
        europe.cargo[c].bid
      );
      rc = 1;
    }
  }
  europe_free(&europe);
  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&a);
  col1_save_free(&b);
  if (rc == 0) {
    printf("golden_market_prices01: %s ok\n", ps->before);
  }
  return rc;
}

int main(void) {
  static const PairSpec pairs[] = {
    /* dutch2 t169→t170: lumber sold (Custom House), furs/horses/muskets colony-side residue. */
    {"original_saves/colony-prod-tests/COLONY00-dutch2-t0.SAV",
     "original_saves/colony-prod-tests/COLONY01-dutch2-t1.SAV",
     (1u << 4) | (1u << 5) | (1u << 8) | (1u << 15)},
    /* no-transports t268→t269: cotton + lumber sold (Custom House), sugar/furs/silver/tools/muskets residue. */
    {"original_saves/colony-prod-tests/COLONY00_no-transports.SAV",
     "original_saves/colony-prod-tests/COLONY01_no-transports.SAV",
     (1u << 1) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 14) | (1u << 15)},
  };
  for (unsigned i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
    if (run_pair(&pairs[i]) != 0) {
      return 1;
    }
  }
  return 0;
}
