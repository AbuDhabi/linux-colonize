#include <stdio.h>
#include <string.h>

#include "core/ai.h"
#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "core/units.h"
#include "data/seed100_fixture.h"
#include "platform/diagnostics.h"
#include "platform/platform.h"

static int terrain_index(uint8_t b) {
  return (int)(b & 0x1fu);
}

static int is_forest_index(int idx) {
  return idx >= 8 && idx <= 23;
}

static int find_unit(
  const ColonizeUnitPool* units,
  int x,
  int y,
  int type_index,
  int nation_id
) {
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active) {
      continue;
    }
    if (u->x == x && u->y == y && u->type_index == type_index && u->nation_id == nation_id) {
      return u->id;
    }
  }
  return -1;
}

int main(void) {
  char err[256];

  /* --- DOS RNG entry + customize axes (hypothesis 1: params then reseed) --- */
  ColonizeDosRng rng;
  dos_rng_seed(&rng, SEED100_SEED);
  const int mass = dos_rng_range(&rng, 0, 3);
  const int form = dos_rng_range(&rng, 0, 3);
  const int temp = dos_rng_range(&rng, 0, 3);
  const int clim = dos_rng_range(&rng, 0, 3);
  const int forest = dos_rng_range(&rng, 0, 3);
  if (mass != SEED100_LAND_MASS || form != SEED100_LAND_FORM || temp != SEED100_TEMPERATURE ||
      clim != SEED100_CLIMATE || forest != SEED100_FOREST_EXTRA) {
    fprintf(
      stderr,
      "seed100 params got (%d,%d,%d,%d,%d) expected (%d,%d,%d,%d,%d)\n",
      mass,
      form,
      temp,
      clim,
      forest,
      SEED100_LAND_MASS,
      SEED100_LAND_FORM,
      SEED100_TEMPERATURE,
      SEED100_CLIMATE,
      SEED100_FOREST_EXTRA
    );
    return 1;
  }
  MapGenParams randomized;
  memset(&randomized, 0, sizeof(randomized));
  map_gen_params_random(&randomized, SEED100_SEED);
  if (randomized.land_mass != SEED100_LAND_MASS || randomized.land_form != SEED100_LAND_FORM ||
      randomized.temperature != SEED100_TEMPERATURE || randomized.climate != SEED100_CLIMATE ||
      randomized.forest_extra != SEED100_FOREST_EXTRA || randomized.seed != SEED100_SEED) {
    fprintf(stderr, "map_gen_params_random(100) mismatch\n");
    return 1;
  }

  /* --- Golden save --- */
  ColonizeCol1Save golden;
  col1_save_init(&golden);
  if (!col1_save_read_file("test-saves-mapgen/SEED100.SAV", &golden, err, sizeof(err))) {
    fprintf(stderr, "failed to read SEED100.SAV: %s\n", err);
    return 1;
  }
  if (golden.head.map_size_x != SEED100_MAP_W || golden.head.map_size_y != SEED100_MAP_H) {
    fprintf(
      stderr,
      "golden map size %ux%u expected %dx%d\n",
      golden.head.map_size_x,
      golden.head.map_size_y,
      SEED100_MAP_W,
      SEED100_MAP_H
    );
    col1_save_free(&golden);
    return 1;
  }
  if (golden.head.tribe_count != SEED100_TRIBE_COUNT || golden.head.unit_count != SEED100_UNIT_COUNT) {
    fprintf(
      stderr,
      "golden tribes=%u units=%u expected %d/%d\n",
      golden.head.tribe_count,
      golden.head.unit_count,
      SEED100_TRIBE_COUNT,
      SEED100_UNIT_COUNT
    );
    col1_save_free(&golden);
    return 1;
  }

  const size_t n = (size_t)SEED100_MAP_W * (size_t)SEED100_MAP_H;
  ColonizeWorldMap golden_map;
  memset(&golden_map, 0, sizeof(golden_map));
  if (!map_alloc(&golden_map, SEED100_MAP_W, SEED100_MAP_H, err, sizeof(err))) {
    fprintf(stderr, "map_alloc golden failed: %s\n", err);
    col1_save_free(&golden);
    return 1;
  }
  for (size_t i = 0; i < n; ++i) {
    golden_map.terrain[i] = col1_tile_to_mp_terrain(golden.map.tile[i]);
  }

  /* --- Port NEW WORLD pipeline with seed 100 (shared DOS LCG, no fixture) --- */
  ColonizeDosRng campaign_rng;
  dos_rng_seed(&campaign_rng, SEED100_SEED);
  MapGenParams params;
  memset(&params, 0, sizeof(params));
  params.rng = &campaign_rng;
  map_gen_params_random(&params, SEED100_SEED);
  /* Hypothesis 1: reseed with the same BIOS seed before FUN_684c_08c0. */
  dos_rng_seed(&campaign_rng, SEED100_SEED);
  params.rng = &campaign_rng;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!map_generate(&map, &params, err, sizeof(err))) {
    fprintf(stderr, "map_generate failed: %s\n", err);
    map_free(&golden_map);
    col1_save_free(&golden);
    return 1;
  }

  /* Terrain: every MP byte must match golden conversion. */
  for (size_t i = 0; i < n; ++i) {
    if (map.terrain[i] != golden_map.terrain[i]) {
      const int x = (int)(i % (size_t)SEED100_MAP_W);
      const int y = (int)(i / (size_t)SEED100_MAP_W);
      fprintf(
        stderr,
        "terrain mismatch at (%d,%d): got 0x%02x expected 0x%02x\n",
        x,
        y,
        map.terrain[i],
        golden_map.terrain[i]
      );
      map_free(&map);
      map_free(&golden_map);
      col1_save_free(&golden);
      return 1;
    }
  }

  /* Feature bits + spot-check PHYS0 on ~20 golden land tiles. */
  int land_checked = 0;
  for (int y = 0; y < SEED100_MAP_H && land_checked < 20; ++y) {
    for (int x = 0; x < SEED100_MAP_W && land_checked < 20; ++x) {
      if (!map_tile_is_land(&golden_map, x, y)) {
        continue;
      }
      const uint8_t a = map.terrain[y * SEED100_MAP_W + x];
      const uint8_t b = golden_map.terrain[y * SEED100_MAP_W + x];
      if (((a >> 5) != (b >> 5)) || (is_forest_index(terrain_index(a)) != is_forest_index(terrain_index(b)))) {
        fprintf(stderr, "feature bits mismatch at (%d,%d)\n", x, y);
        map_free(&map);
        map_free(&golden_map);
        col1_save_free(&golden);
        return 1;
      }
      if (map_phys0_forest_sprite_at(&map, x, y) != map_phys0_forest_sprite_at(&golden_map, x, y) ||
          map_phys0_overlay_count(&map, x, y) != map_phys0_overlay_count(&golden_map, x, y)) {
        fprintf(stderr, "phys0 mismatch at (%d,%d)\n", x, y);
        map_free(&map);
        map_free(&golden_map);
        col1_save_free(&golden);
        return 1;
      }
      land_checked++;
    }
  }
  if (land_checked < 20) {
    fprintf(stderr, "expected 20 land tiles for phys0 check, got %d\n", land_checked);
    map_free(&map);
    map_free(&golden_map);
    col1_save_free(&golden);
    return 1;
  }

  const char* data_dir = "COLONIZE";
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  char names_path[512];
  if (!dos_compat_normalize_asset_path(data_dir, "NAMES.TXT", names_path, sizeof(names_path))) {
    snprintf(names_path, sizeof(names_path), "%s/NAMES.TXT", data_dir);
  }
  if (!assets_msg_load_file(&names, names_path)) {
    fprintf(stderr, "failed to load NAMES.TXT from %s\n", names_path);
    map_free(&map);
    map_free(&golden_map);
    col1_save_free(&golden);
    return 1;
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    fprintf(stderr, "units_load_types failed\n");
    assets_msg_free(&names);
    map_free(&map);
    map_free(&golden_map);
    col1_save_free(&golden);
    return 1;
  }

  units_new_world_start(&units, &map, SEED100_HUMAN_X, SEED100_HUMAN_Y, 0, 0);

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  bool col1_ok = false;
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));

  AiNewGameParams ai;
  memset(&ai, 0, sizeof(ai));
  ai.col1 = &col1;
  ai.col1_ok = &col1_ok;
  ai.map = &map;
  ai.units = &units;
  ai.europe = &europe;
  ai.names = &names;
  ai.data_dir = data_dir;
  ai.human_nation = 0;
  ai.difficulty = 0;
  ai.leader_name = "Walter Raleigh";
  ai.use_tribe_txt = false;
  ai.map_stem = NULL;
  ai.human_start_x = SEED100_HUMAN_X;
  ai.human_start_y = SEED100_HUMAN_Y;
  ai.rng_seed = SEED100_SEED;
  ai.rng = &campaign_rng;

  if (!ai_init_new_game(&ai, err, sizeof(err))) {
    fprintf(stderr, "ai_init_new_game failed: %s\n", err);
    assets_msg_free(&names);
    map_free(&map);
    map_free(&golden_map);
    col1_save_free(&golden);
    col1_save_free(&col1);
    return 1;
  }

  /* Tribes */
  if (col1.head.tribe_count != SEED100_TRIBE_COUNT) {
    fprintf(stderr, "tribe_count %u expected %d\n", col1.head.tribe_count, SEED100_TRIBE_COUNT);
    goto fail;
  }
  for (int i = 0; i < SEED100_TRIBE_COUNT; ++i) {
    const ColonizeCol1Tribe* t = &col1.tribe[i];
    const ColonizeCol1Tribe* g = &golden.tribe[i];
    if (t->x != g->x || t->y != g->y || t->nation_id != g->nation_id ||
        t->state.capital != g->state.capital || t->population != g->population) {
      fprintf(
        stderr,
        "tribe[%d] mismatch got (%u,%u,n=%u,cap=%u,pop=%u) expected (%u,%u,n=%u,cap=%u,pop=%u)\n",
        i,
        t->x,
        t->y,
        t->nation_id,
        (unsigned)t->state.capital,
        t->population,
        g->x,
        g->y,
        g->nation_id,
        (unsigned)g->state.capital,
        g->population
      );
      goto fail;
    }
  }

  /* Units vs golden (x,y,type,nation). */
  if (units.unit_count != SEED100_UNIT_COUNT) {
    fprintf(stderr, "unit_count %d expected %d\n", units.unit_count, SEED100_UNIT_COUNT);
    goto fail;
  }
  for (uint16_t i = 0; i < golden.head.unit_count; ++i) {
    const ColonizeCol1Unit* g = &golden.unit[i];
    if (find_unit(&units, g->x, g->y, (int)g->type, (int)g->nation_id) < 0) {
      fprintf(
        stderr,
        "missing unit[%u] type=%u nation=%u at (%u,%u)\n",
        (unsigned)i,
        g->type,
        g->nation_id,
        g->x,
        g->y
      );
      goto fail;
    }
  }

  if (find_unit(&units, SEED100_HUMAN_X, SEED100_HUMAN_Y, 13, 0) < 0 ||
      find_unit(&units, SEED100_HUMAN_X, SEED100_HUMAN_Y, 2, 0) < 0 ||
      find_unit(&units, SEED100_HUMAN_X, SEED100_HUMAN_Y, 1, 0) < 0) {
    fprintf(stderr, "human starter stack missing at (%d,%d)\n", SEED100_HUMAN_X, SEED100_HUMAN_Y);
    goto fail;
  }
  if (find_unit(&units, 229, 229, 13, 1) < 0 || find_unit(&units, 230, 230, 13, 2) < 0 ||
      find_unit(&units, 231, 231, 14, 3) < 0) {
    fprintf(stderr, "AI Europe sentinel fleets missing\n");
    goto fail;
  }
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id < 1 || u->nation_id > 3) {
      continue;
    }
    if (u->x < SEED100_MAP_W && u->y < SEED100_MAP_H) {
      fprintf(
        stderr,
        "AI euro unit nation=%d unexpectedly on-map at (%d,%d)\n",
        u->nation_id,
        u->x,
        u->y
      );
      goto fail;
    }
  }

  diag_info(
    "smoke_mapgen_seed100 ok params=(%d,%d,%d,%d,%d) tribes=%d units=%d",
    SEED100_LAND_MASS,
    SEED100_LAND_FORM,
    SEED100_TEMPERATURE,
    SEED100_CLIMATE,
    SEED100_FOREST_EXTRA,
    SEED100_TRIBE_COUNT,
    SEED100_UNIT_COUNT
  );

  assets_msg_free(&names);
  map_free(&map);
  map_free(&golden_map);
  col1_save_free(&golden);
  col1_save_free(&col1);
  return 0;

fail:
  assets_msg_free(&names);
  map_free(&map);
  map_free(&golden_map);
  col1_save_free(&golden);
  col1_save_free(&col1);
  return 1;
}
