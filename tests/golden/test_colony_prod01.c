#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/platform.h"

/*
 * Colony production golden: COLONY00_no-transports.SAV -> one turn_end() ->
 * compare against COLONY01_no-transports.SAV, a save produced by running
 * the *real* one turn in original DOS (not Linux-derived, unlike the
 * test-saves-ai/ series). Both fixtures had every European ship / wagon
 * train stripped first (see original_saves/colony-prod-tests/), so this
 * is Euro-unit-free and isolates colony field production math.
 *
 * First pass, human-controlled Dutch (nation_id 3) colonies only: the
 * player made no moves that turn (no pioneers/military built, no orders),
 * so any Dutch colony field drift is either engine production math or a
 * bridge apply/capture bug — not a player-action Linux can't know about.
 * AI nations (English/French/Spanish) are NOT checked here: their turn
 * involves AI unit/build decisions this suite does not attempt to
 * reproduce move-for-move against the DOS RNG stream.
 */

#define COLONY_PROD01_RNG_SEED 100u
#define COLONY_PROD01_HUMAN_NATION 3 /* Netherlands */

static const char* k_cargo_names[COLONIZE_CARGO_COUNT] = {
  "food",   "sugar",  "tobacco", "cotton", "furs",   "lumber", "ore",   "silver",
  "horses", "rum",    "cigars",  "cloth",  "coats",  "trade",  "tools", "muskets"
};

static int find_colony_by_xy(
  const ColonizeCol1Save* save,
  uint8_t x,
  uint8_t y
) {
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
      stderr,
      "%s %s population got %u expected %u\n",
      step_label, e->name, g->population, e->population
    );
    ok = false;
  }
  if (g->building_in_production != e->building_in_production) {
    fprintf(
      stderr,
      "%s %s building_in_production got %u expected %u\n",
      step_label, e->name, g->building_in_production, e->building_in_production
    );
    ok = false;
  }
  if (g->hammers != e->hammers) {
    fprintf(
      stderr,
      "%s %s hammers got %u expected %u\n",
      step_label, e->name, g->hammers, e->hammers
    );
    ok = false;
  }
  if (g->hammers_purchased != e->hammers_purchased) {
    fprintf(
      stderr,
      "%s %s hammers_purchased got %u expected %u\n",
      step_label, e->name, g->hammers_purchased, e->hammers_purchased
    );
    ok = false;
  }
  if (g->warehouse_level != e->warehouse_level) {
    fprintf(
      stderr,
      "%s %s warehouse_level got %u expected %u\n",
      step_label, e->name, g->warehouse_level, e->warehouse_level
    );
    ok = false;
  }
  if (g->capitol_level != e->capitol_level) {
    fprintf(
      stderr,
      "%s %s capitol_level got %u expected %u\n",
      step_label, e->name, g->capitol_level, e->capitol_level
    );
    ok = false;
  }
  if (g->depletion_counter != e->depletion_counter) {
    fprintf(
      stderr,
      "%s %s depletion_counter got %u expected %u\n",
      step_label, e->name, g->depletion_counter, e->depletion_counter
    );
    ok = false;
  }
  if (g->specialty_cargo != e->specialty_cargo) {
    fprintf(
      stderr,
      "%s %s specialty_cargo got %u expected %u\n",
      step_label, e->name, g->specialty_cargo, e->specialty_cargo
    );
    ok = false;
  }
  if (g->labor_shortage != e->labor_shortage) {
    fprintf(
      stderr,
      "%s %s labor_shortage got %u expected %u\n",
      step_label, e->name, g->labor_shortage, e->labor_shortage
    );
    ok = false;
  }
  if (g->cargo_idle_turns != e->cargo_idle_turns) {
    fprintf(
      stderr,
      "%s %s cargo_idle_turns got %u expected %u\n",
      step_label, e->name, g->cargo_idle_turns, e->cargo_idle_turns
    );
    ok = false;
  }
  if (g->cargo_produced_mask != e->cargo_produced_mask) {
    fprintf(
      stderr,
      "%s %s cargo_produced_mask got 0x%04x expected 0x%04x\n",
      step_label, e->name, g->cargo_produced_mask, e->cargo_produced_mask
    );
    ok = false;
  }
  if (g->improve_timer != e->improve_timer) {
    fprintf(
      stderr,
      "%s %s improve_timer got %u expected %u\n",
      step_label, e->name, g->improve_timer, e->improve_timer
    );
    ok = false;
  }
  for (unsigned c = 0; c < COLONIZE_COL1_CARGO_TYPES; ++c) {
    if (g->stock[c] != e->stock[c]) {
      fprintf(
        stderr,
        "%s %s stock[%s] got %u expected %u\n",
        step_label, e->name, k_cargo_names[c], g->stock[c], e->stock[c]
      );
      ok = false;
    }
  }
  return ok;
}

static bool compare_dutch_colonies(
  const ColonizeCol1Save* got,
  const ColonizeCol1Save* exp,
  const char* step_label
) {
  bool ok = true;
  int checked = 0;
  for (unsigned i = 0; i < exp->head.colony_count; ++i) {
    const ColonizeCol1Colony* e = &exp->colony[i];
    if (e->nation_id != COLONY_PROD01_HUMAN_NATION) {
      continue;
    }
    const int gi = find_colony_by_xy(got, e->x, e->y);
    if (gi < 0) {
      fprintf(
        stderr,
        "%s missing Dutch colony '%s' at (%u,%u)\n",
        step_label, e->name, e->x, e->y
      );
      ok = false;
      continue;
    }
    const ColonizeCol1Colony* g = &got->colony[gi];
    if (g->nation_id != e->nation_id || strncmp(g->name, e->name, sizeof(g->name)) != 0) {
      fprintf(
        stderr,
        "%s colony at (%u,%u) got n=%u '%s' expected n=%u '%s'\n",
        step_label, e->x, e->y, g->nation_id, g->name, e->nation_id, e->name
      );
      ok = false;
      continue;
    }
    ++checked;
    if (!compare_colony_production(g, e, step_label)) {
      ok = false;
    }
  }
  fprintf(stderr, "%s checked %d Dutch colonies\n", step_label, checked);
  return ok;
}

static int run_pair(const char* path_in, const char* path_exp, const char* label) {
  char err[256];

  ColonizeCol1Save start;
  ColonizeCol1Save expect;
  col1_save_init(&start);
  col1_save_init(&expect);
  if (!col1_save_read_file(path_in, &start, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    return 1;
  }
  if (!col1_save_read_file(path_exp, &expect, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_exp, err);
    col1_save_free(&start);
    return 1;
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    col1_save_free(&start);
    col1_save_free(&expect);
    return 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    return 1;
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "colonies_load_buildings failed\n");
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
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
    return 1;
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, COLONY_PROD01_RNG_SEED);

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
  ctx.rng_seed = COLONY_PROD01_RNG_SEED;

  turn_end(&ctx);

  if (!col1_bridge_capture(
        &start,
        &map,
        &units,
        &colonies,
        &europe,
        year,
        autumn,
        turn_number,
        br.human_nation,
        br.cursor_x,
        br.cursor_y,
        units.selected_id,
        err,
        sizeof(err)
      )) {
    fprintf(stderr, "bridge capture: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&start);
    col1_save_free(&expect);
    return 1;
  }

  const bool ok = compare_dutch_colonies(&start, &expect, label);

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&start);
  col1_save_free(&expect);
  if (!ok) {
    fprintf(stderr, "%s FAILED\n", label);
    return 1;
  }
  printf("%s ok\n", label);
  return 0;
}

int main(void) {
  const int rc = run_pair(
    "original_saves/colony-prod-tests/COLONY00_no-transports.SAV",
    "original_saves/colony-prod-tests/COLONY01_no-transports.SAV",
    "colony_prod01 COLONY00->01 (Dutch)"
  );
  if (rc != 0) {
    return rc;
  }
  printf("golden_colony_prod01: Dutch colony production ok\n");
  return 0;
}
