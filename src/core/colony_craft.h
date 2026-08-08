#ifndef COLONIZE_COLONY_CRAFT_H
#define COLONIZE_COLONY_CRAFT_H

#include "core/colony.h"
#include "core/turn.h"

/*
 * Settlement manufacturing: workplace colonists convert warehouse raw → goods.
 * Called from turn production after field harvest, before carpenter hammers.
 * sol_bonus: SoL ≥50% → +1 / =100% → +2 per craft worker on output
 * (building_production.md); 0 skips. PARK: Tory −1.
 */
void colony_craft_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeColonyProdDelta* delta,
  int sol_bonus
);

/* Non-mutating craft pass on scratch stock; fills shortfall[] and optional delta. */
void colony_craft_preview(
  const ColonizeColonyPool* pool,
  ColonizeColony* scratch,
  int shortfall[COLONIZE_CARGO_COUNT],
  ColonizeColonyProdDelta* delta,
  int sol_bonus
);

#endif
