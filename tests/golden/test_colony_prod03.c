#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * Colony production golden #3: farmer skill/river arithmetic, from two
 * purpose-made DOS captures (2026-09-03). Each pair is a real DOS save plus
 * the save after one real DOS turn with no player moves; the colony under
 * test is New Amsterdam (Dutch), population 3: one expert Farmer, one Petty
 * Criminal (both on field tiles), one Town Hall worker.
 *
 * - case1: criminal on Broadleaf (base 2, no river) = 3 food; expert on
 *   Broadleaf with a MAJOR river = 6 food. 6 only reconciles if the
 *   Farmer's river bonus is flat +2 with no major-river doubling (the bug
 *   this pair caught: the port doubled major to +4, giving 8) — see
 *   colony_yield_river_bonus's Farmer case and docs/terrain_yields.md
 *   "Plow / road / river stacking" for the matching FUN_15eb_18ec asm
 *   reading (job<4 river tiles fire two +u signals, so the term==u
 *   major-river add never triggers for a Farmer).
 * - case2: same colony, workers moved; expert on Rain forest (base 1,
 *   minor river) = 5 food, criminal on riverless Rain forest = 2. Pins the
 *   expert flat +2 (not x2) and the non-expert unconditional +1 again.
 *
 * Only Dutch (nation_id 3) colonies are compared, same scope rules as
 * golden_colony_prod02.
 */

#define COLONY_PROD03_RNG_SEED 100u
#define COLONY_PROD03_HUMAN_NATION 3 /* Netherlands */

static const char* k_cargo_names[COLONIZE_CARGO_COUNT] = {
  "food",   "sugar",  "tobacco", "cotton", "furs",   "lumber", "ore",   "silver",
  "horses", "rum",    "cigars",  "cloth",  "coats",  "trade",  "tools", "muskets"
};

static int find_colony_by_xy(const ColonizeCol1Save* save, uint8_t x, uint8_t y) {
  for (unsigned i = 0; i < save->head.colony_count; ++i) {
    if (save->colony[i].x == x && save->colony[i].y == y) {
      return (int)i;
    }
  }
  return -1;
}

static bool compare_colony_production(
  const ColonizeCol1Colony* g,
  const ColonizeCol1Colony* e,
  const char* step_label
) {
  bool ok = true;
  if (g->population != e->population) {
    fprintf(
      stderr, "%s %s population got %u expected %u\n", step_label, e->name, g->population,
      e->population
    );
    ok = false;
  }
  if (g->hammers != e->hammers) {
    fprintf(
      stderr, "%s %s hammers got %u expected %u\n", step_label, e->name, g->hammers, e->hammers
    );
    ok = false;
  }
  for (unsigned c = 0; c < COLONIZE_COL1_CARGO_TYPES; ++c) {
    if (g->stock[c] != e->stock[c]) {
      fprintf(
        stderr, "%s %s stock[%s] got %u expected %u\n", step_label, e->name, k_cargo_names[c],
        g->stock[c], e->stock[c]
      );
      ok = false;
    }
  }
  return ok;
}

static bool compare_dutch_colonies(
  const ColonizeCol1Save* orig,
  const ColonizeCol1Save* got,
  const ColonizeCol1Save* exp,
  const char* step_label
) {
  bool ok = true;
  int checked = 0;
  for (unsigned i = 0; i < exp->head.colony_count; ++i) {
    const ColonizeCol1Colony* e = &exp->colony[i];
    if (e->nation_id != COLONY_PROD03_HUMAN_NATION) {
      continue;
    }
    const int oi = find_colony_by_xy(orig, e->x, e->y);
    if (oi < 0 || orig->colony[oi].nation_id != COLONY_PROD03_HUMAN_NATION) {
      continue;
    }
    const int gi = find_colony_by_xy(got, e->x, e->y);
    if (gi < 0 || got->colony[gi].nation_id != COLONY_PROD03_HUMAN_NATION) {
      fprintf(
        stderr, "%s excluded '%s' at (%u,%u): ownership changed in sim\n", step_label, e->name,
        e->x, e->y
      );
      continue;
    }
    ++checked;
    if (!compare_colony_production(&got->colony[gi], e, step_label)) {
      ok = false;
    }
  }
  if (checked == 0) {
    fprintf(stderr, "%s checked no Dutch colonies\n", step_label);
    return false;
  }
  fprintf(stderr, "%s checked %d Dutch colonies\n", step_label, checked);
  return ok;
}

static int run_pair(const char* path_in, const char* path_exp, const char* label) {
  char err[256];

  ColonizeCol1Save start;
  ColonizeCol1Save expect;
  ColonizeCol1Save orig;
  col1_save_init(&start);
  col1_save_init(&expect);
  col1_save_init(&orig);
  if (!col1_save_read_file(path_in, &start, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    return 1;
  }
  if (!col1_save_read_file(path_exp, &expect, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_exp, err);
    col1_save_free(&start);
    return 1;
  }
  if (!col1_save_read_file(path_in, &orig, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    col1_save_free(&start);
    col1_save_free(&expect);
    return 1;
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "colonies_load_buildings failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&start, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge apply %s: %s\n", path_in, err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, COLONY_PROD03_RNG_SEED);

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn_number;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.human_nation = br.human_nation;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.map = &map;
  ctx.col1 = &start;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.rng_seed = COLONY_PROD03_RNG_SEED;

  turn_end(&ctx);

  if (!col1_bridge_capture(
        &start, &map, &units, &colonies, &europe, year, autumn, turn_number, br.human_nation,
        br.cursor_x, br.cursor_y, units.selected_id, err, sizeof(err)
      )) {
    fprintf(stderr, "bridge capture: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    col1_save_free(&orig);
    return 1;
  }

  const bool ok = compare_dutch_colonies(&orig, &start, &expect, label);

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&start);
  col1_save_free(&expect);
  col1_save_free(&orig);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

int main(void) {
  int rc = run_pair(
    "original_saves/colony-prod-tests/farming/case1-turn1.SAV",
    "original_saves/colony-prod-tests/farming/case1-turn2.SAV",
    "colony_prod03 farming case1 (expert Farmer, major river)"
  );
  if (rc != 0) {
    return rc;
  }
  rc = run_pair(
    "original_saves/colony-prod-tests/farming/case2-turn1.SAV",
    "original_saves/colony-prod-tests/farming/case2-turn2.SAV",
    "colony_prod03 farming case2 (expert Farmer, minor river)"
  );
  if (rc != 0) {
    return rc;
  }
  printf("golden_colony_prod03: farming production ok\n");
  return 0;
}
