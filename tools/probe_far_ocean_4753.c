/*
 * Phase 8: compare far tiles for (47,53) fog +8 flip — Linux mapgen vs SEED100.SAV.
 *
 * Usage (from repo root, after cmake build):
 *   ./build/probe_far_ocean_4753
 */
#include <stdio.h>
#include <string.h>

#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/map.h"
#include "core/map_gen.h"
#include "data/seed100_fixture.h"

static int terr_class(uint8_t terr) {
  if ((terr & 0x20) != 0) {
    return (terr & 0x80) ? 27 : 28;
  }
  return (int)(terr & 0x1f);
}

static int is_ocean(uint8_t terr) {
  const int c = terr_class(terr);
  return c == 0x19 || c == 0x1a;
}

static void dump_one(const char* label, const ColonizeWorldMap* map, int x, int y, int show_l2) {
  if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height) {
    printf("  %s (%d,%d) OOB\n", label, x, y);
    return;
  }
  const size_t i = (size_t)y * (size_t)map->width + (size_t)x;
  const uint8_t terr = map->terrain[i];
  if (show_l2 && map->layer2) {
    printf(
      "  %s (%d,%d) terr=%02x class=%d ocean=%d river=%d l2=%02x\n",
      label,
      x,
      y,
      terr,
      terr_class(terr),
      is_ocean(terr),
      (terr & 0x40) != 0,
      map->layer2[i]
    );
  } else {
    printf(
      "  %s (%d,%d) terr=%02x class=%d ocean=%d river=%d\n",
      label,
      x,
      y,
      terr,
      terr_class(terr),
      is_ocean(terr),
      (terr & 0x40) != 0
    );
  }
}

static int cmp_tile(const ColonizeWorldMap* a, const ColonizeWorldMap* b, int x, int y) {
  const size_t i = (size_t)y * (size_t)a->width + (size_t)x;
  if (a->terrain[i] != b->terrain[i]) {
    printf(
      "  MISMATCH (%d,%d) linux=%02x sav=%02x ocean_linux=%d ocean_sav=%d\n",
      x,
      y,
      a->terrain[i],
      b->terrain[i],
      is_ocean(a->terrain[i]),
      is_ocean(b->terrain[i])
    );
    return 0;
  }
  return 1;
}

int main(void) {
  char err[256];
  ColonizeCol1Save golden;
  col1_save_init(&golden);
  if (!col1_save_read_file("test-saves-mapgen/SEED100.SAV", &golden, err, sizeof err)) {
    fprintf(stderr, "SEED100.SAV: %s\n", err);
    return 1;
  }

  ColonizeWorldMap sav_map;
  memset(&sav_map, 0, sizeof sav_map);
  if (!map_alloc(&sav_map, SEED100_MAP_W, SEED100_MAP_H, err, sizeof err)) {
    fprintf(stderr, "map_alloc sav: %s\n", err);
    col1_save_free(&golden);
    return 1;
  }
  const size_t n = (size_t)SEED100_MAP_W * (size_t)SEED100_MAP_H;
  for (size_t i = 0; i < n; ++i) {
    sav_map.terrain[i] = col1_tile_to_mp_terrain(golden.map.tile[i]);
  }

  ColonizeDosRng rng;
  dos_rng_seed(&rng, SEED100_SEED);
  MapGenParams params;
  memset(&params, 0, sizeof params);
  params.rng = &rng;
  map_gen_params_random(&params, SEED100_SEED);
  dos_rng_seed(&rng, SEED100_SEED);
  params.rng = &rng;

  ColonizeWorldMap linux_map;
  memset(&linux_map, 0, sizeof linux_map);
  if (!map_generate(&linux_map, &params, err, sizeof err)) {
    fprintf(stderr, "map_generate: %s\n", err);
    map_free(&sav_map);
    col1_save_free(&golden);
    return 1;
  }

  static const int pts[][2] = {
    {47, 53}, /* unit start */
    {46, 52}, /* golden NW dest */
    {46, 53}, /* ASM W dest */
    {43, 49}, /* far NW — phase 7 ocean */
    {43, 53}, /* far W — phase 7 land */
    {51, 57},
    {47, 57},
    {43, 57},
  };
  const int npt = (int)(sizeof pts / sizeof pts[0]);

  printf("SEED100.SAV (col1 tile→mp terrain; fog ocean uses class only):\n");
  for (int i = 0; i < npt; ++i) {
    dump_one("sav", &sav_map, pts[i][0], pts[i][1], 0);
  }
  printf("Linux map_generate(seed=100):\n");
  for (int i = 0; i < npt; ++i) {
    dump_one("linux", &linux_map, pts[i][0], pts[i][1], 1);
  }

  int ok = 1;
  printf("Compare terrain bytes:\n");
  for (int i = 0; i < npt; ++i) {
    if (!cmp_tile(&linux_map, &sav_map, pts[i][0], pts[i][1])) {
      ok = 0;
    }
  }
  if (ok) {
    printf("AGREE: all probed tiles match Linux ↔ SEED100.SAV\n");
  } else {
    printf("DISAGREE: see MISMATCH lines above\n");
  }

  printf(
    "\nCoarse fog (AI plane, not map.seen): run\n"
    "  AI_QUIET_ASM=1 AI_LCG_AUDIT=1 AI_ASM_STAY_SYNC=1 ./build/golden_mapgen_seed100\n"
    "and look for AI_SCORE_DUMP coarse farW/farNW explore bytes.\n"
  );

  map_free(&linux_map);
  map_free(&sav_map);
  col1_save_free(&golden);
  return ok ? 0 : 2;
}
