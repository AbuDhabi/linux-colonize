#include "tests/common/golden_fixture.h"

#include <stdio.h>
#include <string.h>

#include "core/ai.h"
#include "core/ai_euro.h"
#include "core/turn.h"
#include "core/units.h"

bool golden_open(const char* path_in, const char* path_exp, uint32_t rng_seed, GoldenFixture* fx) {
  char err[256];

  memset(fx, 0, sizeof(*fx));
  fx->seed = rng_seed;
  col1_save_init(&fx->start);
  col1_save_init(&fx->expect);
  col1_save_init(&fx->orig);
  colonies_init(&fx->colonies);
  units_reset(&fx->units);
  units_reset_hooks();
  ai_euro_reset();
  ai_native_reset();
  turn_reset();
  assets_msg_init(&fx->names);
  fx->europe.cargo_count = 16;

  if (!col1_save_read_file(path_in, &fx->start, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    return false;
  }
  if (!col1_save_read_file(path_exp, &fx->expect, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_exp, err);
    return false;
  }
  if (!col1_save_read_file(path_in, &fx->orig, err, sizeof(err))) {
    fprintf(stderr, "read %s: %s\n", path_in, err);
    return false;
  }

  if (!assets_msg_load_file(&fx->names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return false;
  }
  fx->names_loaded = true;

  if (!units_load_types(&fx->units, &fx->names)) {
    fprintf(stderr, "units_load_types failed\n");
    return false;
  }
  if (!colonies_load_buildings(&fx->colonies, &fx->names)) {
    fprintf(stderr, "colonies_load_buildings failed\n");
    return false;
  }
  (void)colonies_load_names(&fx->colonies, "COLONIZE/COLONY.TXT");

  if (!col1_bridge_apply(
        &fx->start, &fx->map, &fx->units, &fx->colonies, &fx->europe, &fx->br, err, sizeof(err)
      )) {
    fprintf(stderr, "bridge apply %s: %s\n", path_in, err);
    return false;
  }
  fx->bridged = true;
  /* Live presence-bit maintenance (DOS FUN_1427_02ca/023a) — game_loop wires
   * this at load; without it every mover leaves a stale UNITFLAG behind. */
  units_set_occupancy_map(&fx->map);
  units_occupancy_rebuild(&fx->units);

  fx->turn_number = fx->br.turn_number;
  fx->year = fx->br.year;
  fx->autumn = fx->br.autumn;
  dos_rng_seed(&fx->rng, fx->seed);
  return true;
}

bool golden_turn(GoldenFixture* fx) {
  char err[256];

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &fx->turn_number;
  ctx.game_year = &fx->year;
  ctx.game_autumn = &fx->autumn;
  ctx.human_nation = fx->br.human_nation;
  ctx.units = &fx->units;
  ctx.colonies = &fx->colonies;
  ctx.europe = &fx->europe;
  ctx.map = &fx->map;
  ctx.col1 = &fx->start;
  ctx.col1_ok = true;
  ctx.rng = &fx->rng;
  ctx.rng_seed = fx->seed;

  turn_end(&ctx);

  if (!col1_bridge_capture(
        &fx->start,
        &fx->map,
        &fx->units,
        &fx->colonies,
        &fx->europe,
        fx->year,
        fx->autumn,
        fx->turn_number,
        fx->br.human_nation,
        fx->br.cursor_x,
        fx->br.cursor_y,
        fx->br.view_x,
        fx->br.view_y,
        fx->units.selected_id,
        fx->units.selected_id < 0,
        err,
        sizeof(err)
      )) {
    fprintf(stderr, "bridge capture: %s\n", err);
    return false;
  }
  return true;
}

void golden_close(GoldenFixture* fx) {
  units_set_occupancy_map(NULL);
  map_free(&fx->map);
  if (fx->names_loaded) {
    assets_msg_free(&fx->names);
  }
  col1_save_free(&fx->start);
  col1_save_free(&fx->expect);
  col1_save_free(&fx->orig);
  memset(fx, 0, sizeof(*fx));
}
