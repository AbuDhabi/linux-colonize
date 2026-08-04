#ifndef COLONIZE_COLONY_CRAFT_H
#define COLONIZE_COLONY_CRAFT_H

#include "core/colony.h"
#include "core/turn.h"

/*
 * Settlement manufacturing: workplace colonists convert warehouse raw → goods.
 * Called from turn production after field harvest, before carpenter hammers.
 */
void colony_craft_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeColonyProdDelta* delta
);

/* Non-mutating craft pass on scratch stock; fills shortfall[] and optional delta. */
void colony_craft_preview(
  const ColonizeColonyPool* pool,
  ColonizeColony* scratch,
  int shortfall[COLONIZE_CARGO_COUNT],
  ColonizeColonyProdDelta* delta
);

#endif
