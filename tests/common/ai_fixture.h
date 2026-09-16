#ifndef COLONIZE_TESTS_COMMON_AI_FIXTURE_H
#define COLONIZE_TESTS_COMMON_AI_FIXTURE_H

/*
 * The small-board fixture the Euro-AI unit tests build by hand in every
 * test function (duplication audit TT-1..5): a w×h map with three planes
 * (plus an optional fog plane) filled with one terrain byte, a reset unit
 * pool with no occupancy map, a colony pool, and the matching frees.
 * Bodies are byte-for-byte what the per-test copies did.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/ai_euro.h"
#include "core/colony.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "core/world.h"

static inline bool fx_map_alloc(
  ColonizeWorldMap* map, int w, int h, int terrain_fill, bool with_seen
) {
  memset(map, 0, sizeof(*map));
  map->width = w;
  map->height = h;
  map->tile_count = (size_t)(w * h);
  map->terrain = calloc((size_t)(w * h), 1);
  map->layer2 = calloc((size_t)(w * h), 1);
  map->layer3 = calloc((size_t)(w * h), 1);
  if (with_seen) {
    map->seen = calloc((size_t)(w * h), 1);
  }
  if (!map->terrain || !map->layer2 || !map->layer3 || (with_seen && !map->seen)) {
    return false;
  }
  for (int i = 0; i < w * h; ++i) {
    map->terrain[i] = (uint8_t)terrain_fill;
  }
  return true;
}

static inline void fx_map_free(ColonizeWorldMap* map) {
  free(map->terrain);
  free(map->layer2);
  free(map->layer3);
  free(map->seen);
  free(map->improve);
  map->terrain = NULL;
  map->layer2 = NULL;
  map->layer3 = NULL;
  map->seen = NULL;
  map->improve = NULL;
}

static inline void fx_units_init(ColonizeUnitPool* units) {
  memset(units, 0, sizeof(*units));
  units_reset(units);
  units_set_occupancy_map(NULL);
  units_reset_hooks();
  units_reset_state();
  ai_euro_reset();
  ai_native_reset();
  turn_reset();
}

static inline void fx_colonies_init(ColonizeColonyPool* colonies) {
  colonies_init(colonies);
  colonies_set_occupancy_map(NULL);
}

/* Colony `colony_count` for `nation` at (x, y) with pop working colonists. */
static inline ColonizeColony* fx_colony_add(
  ColonizeColonyPool* colonies, int nation, int x, int y, int pop
) {
  ColonizeColony* c = &colonies->colonies[colonies->colony_count];
  c->id = colonies->colony_count;
  c->active = true;
  c->nation_id = nation;
  c->x = x;
  c->y = y;
  c->population = pop;
  c->colonist_count = pop;
  c->building_in_production = -1;
  colonies->colony_count++;
  colonies->next_id = colonies->colony_count;
  return c;
}

/*
 * Stack view over the fixture's pools (see src/core/world.h). Pass whichever
 * pieces the call under test needs and leave the rest NULL — ColonizeWorld
 * owns nothing, so there is no matching free.
 */
static inline ColonizeWorld fx_world(
  ColonizeUnitPool* units,
  ColonizeColonyPool* colonies,
  ColonizeWorldMap* map,
  ColonizeCol1Save* col1,
  ColonizeDosRng* rng,
  EuropeScreen* europe
) {
  return world_make(units, colonies, map, col1, col1 != NULL, rng, europe);
}

#endif /* COLONIZE_TESTS_COMMON_AI_FIXTURE_H */
