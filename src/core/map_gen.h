#ifndef COLONIZE_MAP_GEN_H
#define COLONIZE_MAP_GEN_H

#include <stddef.h>
#include <stdint.h>

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
} MapGenParams;

/* NEW WORLD: each axis = seed-derived rand % 3 (DOS style). */
void map_gen_params_random(MapGenParams* out, uint32_t seed);

/*
 * Allocate 58×72 via map_alloc and fill terrain (layer2/3 left 0).
 * Faithful-ish port of FUN_684c_08c0 pipeline (land blobs → cleanup →
 * climate bands → forests/rivers/hills). Documented approximations where
 * the Ghidra export is unreadable.
 */
bool map_generate(ColonizeWorldMap* out, const MapGenParams* params, char* err, size_t err_size);

/*
 * Pick a coastal land start for nation 0..3 on a generated map.
 * Prefers ocean-adjacent land in a latitude band suited to the nation.
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

#endif
