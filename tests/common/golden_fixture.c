#include "tests/common/golden_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/ai_euro.h"
#include "core/ai_goals.h"
#include "core/founding_fathers.h"
#include "core/europe_art.h"
#include "core/turn.h"
#include "core/units.h"

bool sav_load_into_pools(const SavLoad* sl, char* err, size_t err_size) {
  /* Module statics that index the outgoing world go first, in
   * game_apply_col1_save's order (game_loop_saveload.c). */
  ai_goals_reset();
  founding_fathers_reset();
  ai_contact_reset();
  ai_euro_reset();
  ai_native_reset();
  ai_coarse_fog_reset();
  turn_reset();
  units_reset_hooks();
  units_reset_state();
  units_set_occupancy_map(NULL);
  colonies_set_occupancy_map(NULL);

  memset(sl->units, 0, sizeof(*sl->units));
  units_reset(sl->units);
  colonies_init(sl->colonies);
  memset(sl->map, 0, sizeof(*sl->map));
  memset(sl->europe, 0, sizeof(*sl->europe));
  sl->europe->cargo_count = 16;

  if (!units_load_types(sl->units, sl->names)) {
    snprintf(err, err_size, "units_load_types failed");
    return false;
  }
  if (!colonies_load_buildings(sl->colonies, sl->names)) {
    snprintf(err, err_size, "colonies_load_buildings failed");
    return false;
  }
  if (sl->data_dir) {
    if (!europe_load(sl->europe, sl->data_dir, err, err_size)) {
      return false;
    }
  } else if (!europe_load_tables(sl->europe, sl->names)) {
    snprintf(err, err_size, "europe_load_tables failed");
    return false;
  }
  (void)colonies_load_names(sl->colonies, "COLONIZE/COLONY.TXT");
  sl->europe->harbor_ships = 0;
  sl->europe->dock_count = 0;

  if (!col1_bridge_apply_w(&(ColonizeWorld){.units = sl->units, .colonies = sl->colonies, .map = sl->map, .col1 = sl->save, .col1_ok = true, .europe = sl->europe}, sl->br, err, err_size)) {
    return false;
  }
  ai_goals_repair_site_scores(sl->map, sl->names);
  ai_repair_nation_landfalls(&(ColonizeWorld){.units = sl->units, .colonies = sl->colonies, .map = sl->map, .col1 = sl->save, .col1_ok = true});
  return true;
}

bool golden_open(const char* path_in, const char* path_exp, uint32_t rng_seed, GoldenFixture* fx) {
  char err[256];

  memset(fx, 0, sizeof(*fx));
  fx->seed = rng_seed;
  col1_save_init(&fx->start);
  col1_save_init(&fx->expect);
  col1_save_init(&fx->orig);
  assets_msg_init(&fx->names);

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

  const SavLoad sl = {
    .save = &fx->start, .names = &fx->names, .data_dir = NULL, .units = &fx->units,
    .colonies = &fx->colonies, .map = &fx->map, .europe = &fx->europe, .br = &fx->br,
  };
  if (!sav_load_into_pools(&sl, err, sizeof(err))) {
    fprintf(stderr, "load %s: %s\n", path_in, err);
    return false;
  }
  fx->bridged = true;
  /* Live presence-bit maintenance (DOS FUN_1427_02ca/023a) — game_loop wires
   * this at load; without it every mover leaves a stale UNITFLAG behind. */
  units_set_occupancy_map(&fx->map);
  units_occupancy_rebuild(&fx->units);
  /* The sim hooks game_loop wires at load (game_try_unit_move /
   * turn_refresh_moves_for_nation_w set them again per slot). Without them
   * the first day top after a load refills ships without Magellan
   * (units_max_mp reads g_units_ff_col1) and the 0824 census decrement is
   * skipped: independence 000001 -> 000002 showed both as from-load-only
   * diffs. */
  units_set_ff_col1(&fx->start);
  units_set_native_fallout_context(&fx->start, &fx->map, -1);
  units_set_combat_colonies(&fx->colonies);

  fx->turn_number = fx->br.turn_number;
  fx->year = fx->br.year;
  fx->autumn = fx->br.autumn;
  dos_rng_seed(&fx->rng, fx->seed);
  return true;
}

void golden_ctx(GoldenFixture* fx, ColonizeTurnContext* out) {
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
  ctx.names = fx->names_loaded ? &fx->names : NULL;
  ctx.col1 = &fx->start;
  ctx.col1_ok = true;
  ctx.rng = &fx->rng;
  ctx.rng_seed = fx->seed;
  *out = ctx;
}

bool golden_turn(GoldenFixture* fx) {
  char err[256];

  ColonizeTurnContext ctx;
  golden_ctx(fx, &ctx);
  turn_end(&ctx);
  /* Headless: no UI touches View/Move Pieces, so DS:0x5390 rides through
   * from the loaded save (capture below). */

  if (!col1_bridge_capture_w(&(ColonizeWorld){.units=(ColonizeUnitPool*)(&fx->units), .colonies=(ColonizeColonyPool*)(&fx->colonies), .map=(ColonizeWorldMap*)(&fx->map), .col1=(ColonizeCol1Save*)(&fx->start), .col1_ok=true, .europe=(EuropeScreen*)(&fx->europe)}, fx->year, fx->autumn, fx->turn_number, fx->br.human_nation, fx->br.cursor_x, fx->br.cursor_y, fx->br.view_x, fx->br.view_y, fx->units.selected_id, fx->start.head.map_mode != 0, err, sizeof(err))) {
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
