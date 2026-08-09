#ifndef COLONIZE_MAP_GEN_H
#define COLONIZE_MAP_GEN_H

#include <stddef.h>
#include <stdint.h>

#include "core/dos_rng.h"
#include "core/map.h"

/* Classic Col1 NEW WORLD / CUSTOMIZE size (AMER2). */
#define MAP_GEN_WIDTH 58
#define MAP_GEN_HEIGHT 72

/*
 * Params mirror DS:0x1e7e words (0..2) used by VICEROY FUN_684c_08c0 /
 * CUSTOMIZE UI (LABELS.TXT Land Mass / Form / Temperature / Climate).
 */
typedef struct MapGenParams {
  int land_mass; /* 0 Small .. 2 Large */
  int land_form; /* 0 Archipelago .. 2 Continents */
  int temperature; /* 0 Cool .. 2 Warm */
  int climate; /* 0 Arid .. 2 Wet */
  int forest_extra; /* 0..2 DOS 5th word */
  uint32_t seed; /* 0 = treat as 1 for determinism helpers */
  /*
   * Optional shared DOS LCG (global DS:28EE in VICEROY). When non-NULL,
   * map_generate continues from *rng and leaves the post-gen state there
   * for tribe/euro placement (FUN_6a09). When NULL, seeds a local RNG from
   * `seed` (smoke / isolated generate).
   */
  ColonizeDosRng* rng;
  /*
   * DS:0x5398 focus / human nation at mapgen (LAB_684c_1b4c shuffle offset).
   * Nations placed as (i + focus_nation) % 4 into random latitude slots.
   */
  int focus_nation;
} MapGenParams;

/* NEW WORLD: each axis = seed-derived rand % 3 (DOS style). Uses *rng if set. */
void map_gen_params_random(MapGenParams* out, uint32_t seed);

/*
 * Allocate 58×72 via map_alloc and fill terrain.
 * Port of FUN_684c_08c0 pipeline (land blobs → cleanup → climate → features).
 * Ends with FUN_67bf continent IDs in layer3 (low nibble), flags cleared in
 * layer2, then western-ocean pacific (0x20) + offshore suppress (0x04)
 * (FUN_684c_08c0 / FUN_281f_068c).
 */
bool map_generate(ColonizeWorldMap* out, const MapGenParams* params, char* err, size_t err_size);

/*
 * FUN_67bf_0000: water then land connected-component IDs remapped to 1..0xf,
 * written to layer3. Clears layer2 (flags). Does not touch terrain.
 * Then FUN_684c_08c0 density: pacific strip + offshore prime suppress.
 */
void map_gen_assign_continents(ColonizeWorldMap* map);

/*
 * Pick a coastal land start for nation 0..3 on a generated map.
 * Prefers ocean-adjacent land in a latitude band suited to the nation.
 * Non-DOS heuristic fallback — prefer map_gen_euro_landfall after map_generate.
 */
bool map_gen_pick_start(
  const ColonizeWorldMap* map,
  int nation,
  int avoid_x,
  int avoid_y,
  int min_dist,
  int* out_x,
  int* out_y
);

/*
 * FUN_684c_08c0 LAB_684c_1b4c landfall for nation 0..3 (HS rim). Requires
 * map_generate to have filled map->euro_landfall_*.
 */
bool map_gen_euro_landfall(const ColonizeWorldMap* map, int nation, int* out_x, int* out_y);

#endif
