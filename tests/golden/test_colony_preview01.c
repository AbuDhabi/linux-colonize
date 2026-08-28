/*
 * port_plan P4.10: the colony screen's production preview must equal what
 * the end-of-turn tick actually does to the warehouse. Loads the real-DOS
 * fixture COLONY00_no-transports.SAV (same one golden_colony_prod01 uses),
 * snapshots colony_preview_compute for every active colony, runs one
 * turn_run_colony_production, and compares preview.goods[] / food_net /
 * hammers against the observed stock + hammer deltas.
 *
 * Deliberately excluded (the preview can't know these, DOS's own preview
 * doesn't either): warehouse-cap clamping (stock ended exactly at cap and
 * the preview would have overshot it), a colonist birth/starvation tick
 * (pop changed), a building completion (hammers reset), and Horses (turn.c
 * breed formula spends food out of band; preview's own fold is flagged
 * approximate — compared separately below with a loose bound).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

static const char* k_cargo_names[COLONIZE_CARGO_COUNT] = {
  "food",   "sugar",  "tobacco", "cotton", "furs",   "lumber", "ore",   "silver",
  "horses", "rum",    "cigars",  "cloth",  "coats",  "trade",  "tools", "muskets"
};

typedef struct Snapshot {
  ColonizeColonyPreview preview;
  int stock[COLONIZE_CARGO_COUNT];
  int cap[COLONIZE_CARGO_COUNT];
  int hammers;
  int colonist_count;
  int building;
} Snapshot;

static int run_fixture(const char* path) {
  char err[256];

  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(path, &save, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path, err);
    return 1;
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    return 1;
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "colonies_load_buildings failed\n");
    return 1;
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&save, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge: %s\n", err);
    return 1;
  }

  static Snapshot snap[COLONIZE_COLONIES_MAX];
  int checked = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* col = &colonies.colonies[i];
    if (!col->active) {
      continue;
    }
    Snapshot* s = &snap[i];
    colony_preview_compute(&colonies, col, &map, &save, &s->preview);
    memcpy(s->stock, col->stock, sizeof(s->stock));
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      s->cap[c] = colonies_warehouse_capacity(&colonies, col, c);
    }
    s->hammers = col->hammers;
    s->colonist_count = col->colonist_count;
    s->building = col->building_in_production;
  }

  ColonizeDosRng rng;
  dos_rng_seed(&rng, 100u);
  ColonizeTurnResult out;
  memset(&out, 0, sizeof(out));
  /* europe=NULL: no Custom House auto-sell, no status popups. */
  turn_run_colony_production(&colonies, &map, &save, NULL, 3, &out, NULL, NULL, &rng);

  int mismatches = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* col = &colonies.colonies[i];
    if (!col->active) {
      continue;
    }
    const Snapshot* s = &snap[i];
    const bool pop_changed = col->colonist_count != s->colonist_count;
    const bool built = col->building_in_production != s->building || col->hammers < s->hammers;
    checked++;
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      const int actual = col->stock[c] - s->stock[c];
      const int want = (c == COLONIZE_CARGO_FOOD) ? s->preview.food_net : s->preview.goods[c];
      if (c == COLONIZE_CARGO_FOOD && pop_changed) {
        continue;
      }
      if (col->stock[c] == s->cap[c] && s->stock[c] + want >= s->cap[c]) {
        continue; /* clamped at warehouse cap (also over-cap stock clamped down) */
      }
      if (c == COLONIZE_CARGO_HORSES) {
        /* Both sides approximate; only flag a sign/gross disagreement. */
        if ((want > 0) != (actual > 0) || abs(want - actual) > 2) {
          fprintf(
            stderr,
            "%s (nation %d) horses: preview %+d actual %+d (loose bound)\n",
            col->name, col->nation_id, want, actual
          );
          mismatches++;
        }
        continue;
      }
      if (s->stock[c] == 0 && want < 0 && actual == 0) {
        continue; /* nothing to draw down — spoiled-to-zero input */
      }
      if (actual != want) {
        fprintf(
          stderr,
          "%s (nation %d) %s: preview %+d actual %+d (stock %d -> %d, cap %d)\n",
          col->name, col->nation_id, k_cargo_names[c], want, actual, s->stock[c], col->stock[c],
          s->cap[c]
        );
        mismatches++;
      }
    }
    if (!built) {
      const int actual = col->hammers - s->hammers;
      if (actual != s->preview.hammers) {
        fprintf(
          stderr,
          "%s (nation %d) hammers: preview %+d actual %+d\n",
          col->name, col->nation_id, s->preview.hammers, actual
        );
        mismatches++;
      }
    }
  }

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&save);
  if (checked == 0) {
    fprintf(stderr, "no active colonies in fixture\n");
    return 1;
  }
  if (mismatches) {
    fprintf(stderr, "golden_colony_preview01: %d preview/EOT mismatches\n", mismatches);
    return 1;
  }
  printf("golden_colony_preview01: %s preview == EOT delta for %d colonies\n", path, checked);
  return 0;
}

int main(void) {
  static const char* k_fixtures[] = {
    "original_saves/colony-prod-tests/COLONY00_no-transports.SAV",
    "original_saves/colony-prod-tests/COLONY00-dutch2-t0.SAV",
  };
  for (unsigned i = 0; i < sizeof(k_fixtures) / sizeof(k_fixtures[0]); ++i) {
    const int rc = run_fixture(k_fixtures[i]);
    if (rc != 0) {
      return rc;
    }
  }
  return 0;
}
