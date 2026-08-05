#ifndef COLONIZE_COLONY_YIELD_H
#define COLONIZE_COLONY_YIELD_H

#include "core/colony.h"
#include "core/map.h"

/*
 * Terrain yield stubs from NAMES.TXT @UNFORESTED / @FORESTED / @OTHER
 * (Farmer … Fisherman columns) plus light @RESOURCE bonuses.
 */

/* Cargo produced by a field @JOB (Farmer/Fisherman → food). Returns -1 if invalid. */
int colony_yield_job_cargo(int field_job);

/* Base + resource yield for working (x,y) as field_job. 0 if impossible. */
int colony_yield_for_tile(const ColonizeWorldMap* map, int x, int y, int field_job);

/* Display name for field @JOB (static string). */
const char* colony_yield_job_name(int field_job);

/*
 * Town commons (colony center): always food + one other commodity.
 * Food base = cleared Farmer+2 (forests use parent land); secondary = NAMES+1.
 * Plow → food; river → food + secondary; Game/Oasis/Wheat → +2 food;
 * matching specials → +2 secondary (Prime Timber excluded).
 * See docs/terrain_yields.md.
 */
typedef struct ColonizeTownCommonsYield {
  int food;
  int secondary_job;   /* COLONIZE_JOB_* or -1 */
  int secondary_cargo; /* COLONIZE_CARGO_* or -1 */
  int secondary_amount;
} ColonizeTownCommonsYield;

void colony_yield_town_commons(
  const ColonizeWorldMap* map,
  int x,
  int y,
  ColonizeTownCommonsYield* out
);

#endif
