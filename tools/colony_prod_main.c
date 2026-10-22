/*
 * colony_prod: per-colony production of a SAV, one TSV row per colony, from
 * the port's colony-screen preview (colony_preview_compute_w). Gross amounts:
 * cargo 0 = food produced, 8 = horses net (breed), the rest field + craft
 * output. Port code, not a DOS readout. Consumed by tools/save_compare.py.
 *
 *   colony_prod <save.SAV>     (repo root as cwd: reads COLONIZE/NAMES.TXT)
 */
#include <stdio.h>
#include <string.h>

#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_preview.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/units.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: colony_prod <save.SAV>\n");
    return 2;
  }
  char err[256];
  static ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(argv[1], &save, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", argv[1], err);
    return 1;
  }
  static ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return 1;
  }
  static ColonizeUnitPool units;
  units_reset(&units);
  static ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!units_load_types(&units, &names) || !colonies_load_buildings(&colonies, &names)) {
    fprintf(stderr, "catalog load failed\n");
    return 1;
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");
  static ColonizeWorldMap map;
  static EuropeScreen europe;
  europe.cargo_count = 16;
  ColonizeWorld w = {
    .units = &units, .colonies = &colonies, .map = &map, .col1 = &save, .col1_ok = true, .europe = &europe
  };
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply_w(&w, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge: %s\n", err);
    return 1;
  }
  printf("#nation\tname\tbells\tcrosses\thammers");
  for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) printf("\tcargo%d", c);
  printf("\n");
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* col = &colonies.colonies[i];
    if (!col->active) continue;
    ColonizeColonyPreview p;
    colony_preview_compute_w(&w, col, &p);
    printf("%d\t%s\t%d\t%d\t%d", col->nation_id, col->name, p.bells, p.crosses, p.hammers);
    for (int c = 0; c < COLONIZE_CARGO_COUNT; ++c) {
      const int v = c == 0 ? p.food_produced : c == 8 ? p.goods[8] : p.field_gross[c] + p.craft_gross[c];
      printf("\t%d", v);
    }
    printf("\n");
  }
  return 0;
}
