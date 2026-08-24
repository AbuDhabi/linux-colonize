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

/*
 * Per-raw-cargo craft demand this tick: demand[in_cargo] is true iff some
 * staffed worker's tier-scaled recipe input (colony_prod_manufacturing_input,
 * same sol_bonus fold as colony_craft_one_colony) is > 0 for a recipe whose
 * in_cargo is that good — i.e. someone is actually working a building that
 * wants to consume it, not just "the building exists" (DOS FUN_364b_0688
 * Phase K gates its "ran out of X" chrome on the FUN_15eb_0bd4/0b96 demand
 * scratch word, derived from this same tier-scaled worker output, not on
 * building presence — see colony_eot_production.md Deep K). Stock is not
 * read; safe to call before or after colony_craft_one_colony consumes it.
 */
void colony_craft_demand_mask(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int sol_bonus,
  bool demand[COLONIZE_CARGO_COUNT]
);

#endif
