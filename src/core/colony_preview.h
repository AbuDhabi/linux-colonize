#ifndef COLONIZE_COLONY_PREVIEW_H
#define COLONIZE_COLONY_PREVIEW_H

#include "core/colony.h"
#include "core/map.h"

/*
 * Non-mutating this-turn production preview for the colony screen
 * (area badges, people meters, multifunction Production).
 */
typedef struct ColonizeColonyPreview {
  int food_produced; /* field + center before consumption */
  int food_fish;     /* fisherman portion of food_produced (display icon only) */
  int food_consumed; /* pop * 2 */
  int food_net;      /* produced - consumed */
  int crosses;
  int bells;
  int hammers; /* carpenter hammers that would be added */
  int goods[COLONIZE_CARGO_COUNT];     /* net field+craft before food eat */
  int shortfall[COLONIZE_CARGO_COUNT]; /* craft wanted beyond available raw */
} ColonizeColonyPreview;

void colony_preview_compute(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  ColonizeColonyPreview* out
);

/* Best field job for an unworked tile (max yield); -1 if none. */
int colony_preview_best_job(const ColonizeWorldMap* map, int x, int y);

/* Second-best job (distinct cargo from first); -1 if none. */
int colony_preview_second_job(const ColonizeWorldMap* map, int x, int y, int first_job);

#endif
