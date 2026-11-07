#include <stdio.h>
#include <string.h>

#include "core/col1_save.h"
#include "core/dos_buggy.h"
#include "core/map_gen.h"
#include "core/map_gen_internal.h"
#include "platform/diagnostics.h"

#include "../common/test_runner.h"

static int count_land(const ColonizeWorldMap* map) {
  int n = 0;
  for (int y = 0; y < map->height; ++y) {
    for (int x = 0; x < map->width; ++x) {
      if (map_tile_is_land(map, x, y)) {
        n++;
      }
    }
  }
  return n;
}

static int count_ocean(const ColonizeWorldMap* map) {
  int n = 0;
  for (int y = 0; y < map->height; ++y) {
    for (int x = 0; x < map->width; ++x) {
      if (map_tile_is_water(map, x, y)) {
        n++;
      }
    }
  }
  return n;
}

static int maps_equal(const ColonizeWorldMap* a, const ColonizeWorldMap* b) {
  if (a->width != b->width || a->height != b->height) {
    return 0;
  }
  const size_t n = (size_t)a->width * (size_t)a->height;
  return memcmp(a->terrain, b->terrain, n) == 0;
}

/* Fixed customize-style mid settings shared by every case that needs a
 * generated map. Each case calls setup() itself, which frees any
 * previously generated s_map and regenerates from scratch (deterministic,
 * fixed seed) -- so cases never depend on state a previous case left
 * behind. */
static MapGenParams s_params;
static ColonizeWorldMap s_map;
static int s_map_ready = 0;

static int setup(void) {
  char err[256];
  if (s_map_ready) {
    map_free(&s_map);
    s_map_ready = 0;
  }
  memset(&s_params, 0, sizeof(s_params));
  s_params.land_mass = 1;
  s_params.land_form = 1;
  s_params.temperature = 1;
  s_params.climate = 1;
  s_params.forest_extra = 1;
  s_params.seed = 0xC01A71Eu;

  memset(&s_map, 0, sizeof(s_map));
  if (!map_generate(&s_map, &s_params, err, sizeof(err))) {
    fprintf(stderr, "map_generate failed: %s\n", err);
    return 0;
  }
  s_map_ready = 1;
  return 1;
}

static int test_params_random_in_range(void) {
  MapGenParams params;
  memset(&params, 0, sizeof(params));
  map_gen_params_random(&params, 0xC01A71Eu);
  /* NEW WORLD axes are FUN_281f_04d4(0,3) -> 0..3 (CUSTOMIZE UI stays 0..2). */
  if (params.land_mass < 0 || params.land_mass > 3 || params.land_form < 0 || params.land_form > 3 ||
      params.temperature < 0 || params.temperature > 3 || params.climate < 0 || params.climate > 3 ||
      params.forest_extra < 0 || params.forest_extra > 3) {
    fprintf(stderr, "map_gen_params_random out of range\n");
    return 1;
  }
  return 0;
}

static int test_dims_and_land_budget(void) {
  if (!setup()) {
    return 1;
  }
  const int budget = (s_params.land_form + s_params.land_mass + 1) * 0x140;

  if (s_map.width != MAP_GEN_WIDTH || s_map.height != MAP_GEN_HEIGHT) {
    fprintf(stderr, "dims %dx%d expected %dx%d\n", s_map.width, s_map.height, MAP_GEN_WIDTH, MAP_GEN_HEIGHT);
    map_free(&s_map);
    s_map_ready = 0;
    return 1;
  }

  const int land = count_land(&s_map);
  const int ocean = count_ocean(&s_map);
  int rc = 0;
  if (ocean < 200) {
    fprintf(stderr, "expected ocean tiles, got %d\n", ocean);
    rc = 1;
  }
  /* Cleanup + extras: land near budget, not empty / not full map. */
  if (land < budget / 4 || land > budget + 600) {
    fprintf(stderr, "land %d outside expected band around budget %d\n", land, budget);
    rc = 1;
  }
  if (rc == 0) {
    diag_info("unit_map_gen dims/budget ok land=%d budget=%d", land, budget);
  }
  return rc;
}

static int test_euro_landfall_nation0_is_water(void) {
  if (!setup()) {
    return 1;
  }
  int sx = -1, sy = -1;
  if (!map_gen_euro_landfall(&s_map, 0, &sx, &sy)) {
    fprintf(stderr, "map_gen_euro_landfall failed\n");
    return 1;
  }
  if (!map_tile_is_high_seas(&s_map, sx, sy) && !map_tile_is_water(&s_map, sx, sy)) {
    fprintf(stderr, "euro landfall (%d,%d) is not water/HS\n", sx, sy);
    return 1;
  }
  return 0;
}

static int test_euro_landfall_bands_distinct(void) {
  if (!setup()) {
    return 1;
  }
  const int h = (int)s_map.height;
  const int h5 = h / 5;
  int seen_band[4] = {0, 0, 0, 0};
  const int bands[4] = {h5, h5 * 2, h5 * 3, h5 * 4};
  for (int n = 0; n < 4; ++n) {
    int lx = -1, ly = -1;
    if (!map_gen_euro_landfall(&s_map, n, &lx, &ly)) {
      fprintf(stderr, "euro landfall missing for nation %d\n", n);
      return 1;
    }
    int band = -1;
    for (int b = 0; b < 4; ++b) {
      if (ly == bands[b]) {
        band = b;
        break;
      }
    }
    if (band < 0) {
      fprintf(stderr, "nation %d landfall Y=%d not in {14,28,42,56}-style bands\n", n, ly);
      return 1;
    }
    if (seen_band[band]) {
      fprintf(stderr, "duplicate landfall band %d\n", band);
      return 1;
    }
    seen_band[band] = 1;
  }
  return 0;
}

static int test_pick_start_fallback_is_land(void) {
  if (!setup()) {
    return 1;
  }
  int sx = -1, sy = -1;
  if (!map_gen_pick_start(&s_map, 0, -1, -1, 0, &sx, &sy)) {
    fprintf(stderr, "map_gen_pick_start failed\n");
    return 1;
  }
  if (!map_tile_is_land(&s_map, sx, sy)) {
    fprintf(stderr, "pick_start (%d,%d) is not land\n", sx, sy);
    return 1;
  }
  diag_info("unit_map_gen ok start=(%d,%d)", sx, sy);
  return 0;
}

static int test_seed_reproducibility(void) {
  if (!setup()) {
    return 1;
  }
  char err[256];
  ColonizeWorldMap map2;
  memset(&map2, 0, sizeof(map2));
  if (!map_generate(&map2, &s_params, err, sizeof(err))) {
    fprintf(stderr, "second map_generate failed: %s\n", err);
    return 1;
  }
  const int equal = maps_equal(&s_map, &map2);
  map_free(&map2);
  if (!equal) {
    fprintf(stderr, "same seed did not reproduce terrain\n");
    return 1;
  }
  return 0;
}

/* TB-46 fixed (flag off): one latitude draw per tile, so the stream (and the
 * map) differs from the DOS two-draw map, and stays seed-reproducible. */
static int test_tb46_latitude_single_draw(void) {
  if (!setup()) {
    return 1;
  }
  char err[256];
  ColonizeWorldMap a;
  ColonizeWorldMap b;
  memset(&a, 0, sizeof(a));
  memset(&b, 0, sizeof(b));
  dos_buggy_behavior_set(false);
  const int ok = map_generate(&a, &s_params, err, sizeof(err)) &&
                 map_generate(&b, &s_params, err, sizeof(err));
  dos_buggy_behavior_set(true);
  int rc = 0;
  if (!ok) {
    fprintf(stderr, "map_generate failed: %s\n", err);
    rc = 1;
  } else if (!maps_equal(&a, &b)) {
    fprintf(stderr, "fixed latitude path not reproducible\n");
    rc = 1;
  } else if (maps_equal(&a, &s_map)) {
    fprintf(stderr, "fixed latitude path produced the DOS two-draw map\n");
    rc = 1;
  } else if (count_land(&a) == 0 || count_ocean(&a) == 0) {
    fprintf(stderr, "fixed latitude map degenerate\n");
    rc = 1;
  }
  map_free(&a);
  map_free(&b);
  return rc;
}

/* FUN_684c_08c0 raw 113529-113536 (asm 684c:1b3a-1b4a) gives the focus nation
 * on a generated map range(1,2): one of the middle bands (2h/5, 3h/5). */
static int test_euro_landfall_focus_middle_band(void) {
  int rc = 0;
  for (uint32_t seed = 1; seed <= 24; ++seed) {
    MapGenParams p = {1, 1, 1, 1, 1, seed, NULL, (int)(seed % 4)};
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    char err[256];
    if (!map_generate(&map, &p, err, sizeof(err))) {
      fprintf(stderr, "map_generate failed: %s\n", err);
      return 1;
    }
    int x = -1, y = -1;
    const int h5 = map.height / 5;
    if (!map_gen_euro_landfall(&map, p.focus_nation, &x, &y) || (y != h5 * 2 && y != h5 * 3)) {
      fprintf(stderr, "seed %u focus %d landfall y=%d not a middle band\n", seed, p.focus_nation, y);
      rc = 1;
    }
    map_free(&map);
  }
  return rc;
}

/* DOS runs the same band pick on AMER2 (param_1 != 0, all four slots
 * range(0,3)). DOS seed-100 save original_saves/golden_idle_america_seed-100
 * /000001.SAV: 0x190 = 365, England (47,28), idle human Spain (37,14). France
 * and the Dutch are not asserted (their AI ships departed and restamped the
 * tile). */
static int test_euro_landfall_amer2_seed100_matches_dos(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
    fprintf(stderr, "map_load_mp failed: %s\n", err);
    return 1;
  }
  map_gen_finalize_border_and_forest(map.terrain, (int)map.width, (int)map.height);
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 100);
  const int prime = dos_rng_range(&rng, 1, 0x7fff);
  map_gen_assign_euro_landfalls(&map, &rng, 2, true);
  int ex = -1, ey = -1, sx = -1, sy = -1;
  const bool ok =
    map_gen_euro_landfall(&map, 0, &ex, &ey) && map_gen_euro_landfall(&map, 2, &sx, &sy);
  map_free(&map);
  if (!ok || prime != 365 || ex != 47 || ey != 28 || sx != 37 || sy != 14) {
    fprintf(stderr, "prime=%d England (%d,%d) Spain (%d,%d); DOS 365 (47,28) (37,14)\n", prime,
            ex, ey, sx, sy);
    return 1;
  }
  return 0;
}

/* #1466: a loaded AMER2.MP runs FUN_684c_08c0's Pacific walk with the
 * param_1 != 0 cutoff W-16, the offshore suppress pass and the (21,1)/(43,68)
 * |= 0xa0 stamp (raw 107083-107130), after the full-byte HS/arctic finalize.
 * DOS seed-100 000001.SAV: 1309 Pacific tiles reaching x=41; mask 0x24 and
 * terrain must match tile for tile. */
static int test_amer2_pacific_suppress_matches_dos(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[256];
  if (!map_load_mp("COLONIZE/AMER2.MP", &map, err, sizeof(err))) {
    fprintf(stderr, "map_load_mp failed: %s\n", err);
    return 1;
  }
  ColonizeCol1Save save;
  memset(&save, 0, sizeof(save));
  if (!col1_save_read_file("original_saves/golden_idle_america_seed-100/000001.SAV", &save, err,
                           sizeof(err))) {
    fprintf(stderr, "col1_save_read_file failed: %s\n", err);
    map_free(&map);
    return 1;
  }
  map_gen_finalize_border_and_forest(map.terrain, (int)map.width, (int)map.height);
  map.prime_resource_seed = 365; /* DS:0x190 of the same save */
  map_gen_pacific_and_suppress(&map, true, false);
  int rc = 0;
  int pacific = 0;
  int bad_mask = 0;
  int bad_terrain = 0;
  for (size_t i = 0; i < map.tile_count && i < save.map.tile_count; ++i) {
    if ((map.layer2[i] & MAP_LAYER2_PACIFIC) != 0) {
      pacific++;
    }
    if ((map.layer2[i] & 0x24u) != (save.map.mask[i] & 0x24u)) {
      bad_mask++;
    }
    if (map.terrain[i] != save.map.tile[i]) {
      bad_terrain++;
    }
  }
  if (pacific != 1309 || bad_mask != 0 || bad_terrain != 0) {
    fprintf(stderr, "pacific=%d (DOS 1309) mask diffs=%d terrain diffs=%d\n", pacific, bad_mask,
            bad_terrain);
    rc = 1;
  }
  col1_save_free(&save);
  map_free(&map);
  return rc;
}

/*
 * #1559: FUN_684c_04a6's ring-20 loop and thicken walk have no bounds check.
 * DOSBox trace 2026-11-07 (VR_SEED, NEW WORLD, at 684c:04a6 entry): this
 * synthetic plane was written over the terrain plane (74f8:0) and the LCG
 * (DS:0x28ee) set to 0x15de3720. Rivers then read and wrote the slack, the
 * layer2 MCB, layer2 row 0 and the wrapped 74f8:ffxx bytes; DOS left LCG
 * 0x3d15a05d, layer2 bytes 0 and 5 = 8, and this terrain hash.
 */
static int test_rivers_out_of_plane_matches_dos(void) {
  enum { W = 58, H = 72 };
  static uint8_t terrain[W * H];
  static uint8_t layer2[W * H];
  memset(terrain, 0x19, sizeof terrain);
  memset(layer2, 0, sizeof layer2);
  for (int y = 62; y <= 71; ++y) {
    for (int x = 3; x <= 44; ++x) {
      if (!(y == 71 && x >= 16 && x <= 30)) {
        terrain[y * W + x] = 0x13;
      }
    }
  }
  for (int y = 0; y <= 9; ++y) {
    for (int x = 3; x <= 55; ++x) {
      if (!(y == 0 && x >= 40)) {
        terrain[y * W + x] = 0x13;
      }
    }
  }
  ColonizeDosRng rng = {0x15de3720u};
  map_gen_rivers_pass(terrain, layer2, W, H, &rng, 0, 0);

  uint32_t hash = 0x811c9dc5u; /* FNV-1a */
  for (int i = 0; i < W * H; ++i) {
    hash = (hash ^ terrain[i]) * 0x01000193u;
  }
  int bad_layer2 = 0;
  for (int i = 0; i < W * H; ++i) {
    bad_layer2 += layer2[i] != ((i == 0 || i == 5) ? 8 : 0);
  }
  if (rng.state != 0x3d15a05du || hash != 0x23eca8d7u || bad_layer2) {
    fprintf(stderr, "lcg %08x (DOS 3d15a05d) terrain hash %08x (DOS 23eca8d7) layer2 diffs %d\n",
            (unsigned)rng.state, (unsigned)hash, bad_layer2);
    return 1;
  }
  return 0;
}

static int read_fixture(const char* path, uint8_t* buf, size_t n) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "missing %s\n", path);
    return 0;
  }
  const size_t got = fread(buf, 1, n, f);
  fclose(f);
  if (got != n) {
    fprintf(stderr, "%s: %zu bytes, want %zu\n", path, got, n);
  }
  return got == n;
}

/*
 * Whole NEW WORLD maps vs DOS: original_saves/mapgen/DOS_PLANES_S<seed>.bin is
 * the tile|mask|path planes (58x72 each) at FUN_684c_08c0's epilogue (684c:1bb4),
 * dumped under DOSBox-X from VR_SEED with the seed imm patched (2026-11-07; the
 * same LCG and planes on Discoverer and Viceroy for seed 105). Seed 7553 hits
 * the #1559 ring cells past the tile plane; the others vary the five params.
 */
static int test_generated_maps_match_dos_dumps(void) {
  static const struct {
    uint32_t seed;
    uint32_t lcg; /* DS:0x28ee at the epilogue */
  } k_runs[] = {{105, 0xa2dffd81u}, {2544, 0x29e37785u}, {5008, 0x439a32dau}, {7553, 0x8ebf01d9u}};
  enum { N = MAP_GEN_WIDTH * MAP_GEN_HEIGHT };
  static uint8_t dos[3 * N];
  int rc = 0;
  for (size_t r = 0; r < sizeof k_runs / sizeof k_runs[0]; ++r) {
    char path[96];
    snprintf(path, sizeof path, "original_saves/mapgen/DOS_PLANES_S%u.bin", (unsigned)k_runs[r].seed);
    if (!read_fixture(path, dos, sizeof dos)) {
      rc = 1;
      continue;
    }
    ColonizeDosRng rng;
    MapGenParams params;
    memset(&params, 0, sizeof params);
    dos_rng_seed(&rng, k_runs[r].seed);
    params.rng = &rng;
    map_gen_params_random(&params, k_runs[r].seed);
    dos_rng_seed(&rng, k_runs[r].seed); /* as golden_mapgen_seed100 */
    params.rng = &rng;
    ColonizeWorldMap map;
    memset(&map, 0, sizeof map);
    char err[128];
    if (!map_generate(&map, &params, err, sizeof err)) {
      fprintf(stderr, "seed %u: %s\n", (unsigned)k_runs[r].seed, err);
      rc = 1;
      continue;
    }
    const uint8_t* planes[3] = {map.terrain, map.layer2, map.layer3};
    int diffs[3] = {0, 0, 0};
    for (int k = 0; k < 3; ++k) {
      for (int i = 0; i < N; ++i) {
        diffs[k] += planes[k][i] != dos[k * N + i];
      }
    }
    if (rng.state != k_runs[r].lcg || diffs[0] || diffs[1] || diffs[2]) {
      fprintf(stderr, "seed %u: lcg %08x (DOS %08x) tile/mask/path diffs %d/%d/%d\n",
              (unsigned)k_runs[r].seed, (unsigned)rng.state, (unsigned)k_runs[r].lcg, diffs[0],
              diffs[1], diffs[2]);
      rc = 1;
    }
    map_free(&map);
  }
  return rc;
}

/*
 * The river pass alone on seed 105's DOS planes at 684c:04a6 entry
 * (original_saves/mapgen/DOS_PRERIVER_S105.bin = tile|layer2, LCG bd634cd6):
 * DOS left LCG 3529d9f1 and these plane hashes.
 */
static int test_rivers_seed105_matches_dos(void) {
  enum { N = MAP_GEN_WIDTH * MAP_GEN_HEIGHT };
  static uint8_t pre[2 * N];
  if (!read_fixture("original_saves/mapgen/DOS_PRERIVER_S105.bin", pre, sizeof pre)) {
    return 1;
  }
  ColonizeDosRng rng = {0xbd634cd6u};
  map_gen_rivers_pass(pre, pre + N, MAP_GEN_WIDTH, MAP_GEN_HEIGHT, &rng, 0, 0);
  uint32_t hash[2] = {0x811c9dc5u, 0x811c9dc5u}; /* FNV-1a */
  for (int k = 0; k < 2; ++k) {
    for (int i = 0; i < N; ++i) {
      hash[k] = (hash[k] ^ pre[k * N + i]) * 0x01000193u;
    }
  }
  if (rng.state != 0x3529d9f1u || hash[0] != 0xf3199dcbu || hash[1] != 0x49bb1d05u) {
    fprintf(stderr, "lcg %08x (DOS 3529d9f1) tile %08x (DOS f3199dcb) layer2 %08x (DOS 49bb1d05)\n",
            (unsigned)rng.state, (unsigned)hash[0], (unsigned)hash[1]);
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"test_amer2_pacific_suppress_matches_dos", test_amer2_pacific_suppress_matches_dos},
    {"test_euro_landfall_focus_middle_band", test_euro_landfall_focus_middle_band},
    {"test_euro_landfall_amer2_seed100_matches_dos", test_euro_landfall_amer2_seed100_matches_dos},
    {"test_params_random_in_range", test_params_random_in_range},
    {"test_dims_and_land_budget", test_dims_and_land_budget},
    {"test_euro_landfall_nation0_is_water", test_euro_landfall_nation0_is_water},
    {"test_euro_landfall_bands_distinct", test_euro_landfall_bands_distinct},
    {"test_pick_start_fallback_is_land", test_pick_start_fallback_is_land},
    {"test_seed_reproducibility", test_seed_reproducibility},
    {"test_tb46_latitude_single_draw", test_tb46_latitude_single_draw},
    {"test_rivers_out_of_plane_matches_dos", test_rivers_out_of_plane_matches_dos},
    {"test_rivers_seed105_matches_dos", test_rivers_seed105_matches_dos},
    {"test_generated_maps_match_dos_dumps", test_generated_maps_match_dos_dumps},
};

TEST_MAIN(k_cases)
