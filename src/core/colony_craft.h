#ifndef COLONIZE_COLONY_CRAFT_H
#define COLONIZE_COLONY_CRAFT_H

#include "core/colony.h"
#include "core/turn.h"

/*
 * One row of the craft table (audit CO-12: colony_production.c's
 * colony_prod_worker_building_output_ctx and colony_screen.c's
 * colony_screen_building_production_badge each carried the same six chains
 * as a private if-ladder, with a "must stay in lockstep with k_recipes"
 * comment admitting it).
 *
 *   chain             @BUILDING chain id (colonies_building_row_chain) —
 *                      identity is the chain, not the English name; every
 *                      tier of the chain (house/shop/factory spelling) uses
 *                      the same recipe
 *   in_cargo          raw good consumed
 *   out_cargo         manufactured good produced
 *   craft_profession  the @JOB id that is "expert" at this building
 *
 * The table order is the DOS conversion-ledger emission order and is
 * load-bearing (Ore→Tools first, Tools→Muskets last) — see colony_craft.c.
 */
typedef struct ColonizeCraftRecipe {
  int chain;
  int in_cargo;
  int out_cargo;
  int craft_profession;
} ColonizeCraftRecipe;

/* Recipe whose chain owns @BUILDING row `building_row`, or NULL. */
const ColonizeCraftRecipe* colony_craft_recipe_for_building_row(int building_row);

/* Name-taking wrapper (colonies_building_name_row lookup) — kept for
 * callers outside colony_craft.c/.h that only have a catalog name in hand
 * (e.g. ai_euro.c's per-worker want pass). Prefer the _row form. */
const ColonizeCraftRecipe* colony_craft_recipe_for_building(const char* building_name);

/* Raw table access, for callers that need to walk every chain. */
int colony_craft_recipe_count(void);
const ColonizeCraftRecipe* colony_craft_recipe_at(int index);

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

/*
 * Non-mutating craft pass on scratch stock; fills shortfall[] and optional
 * delta. `gross_out` (optional, NULL to skip) receives each recipe's actual
 * (stock-clamped) production this tick keyed by out_cargo — the colony
 * screen's Production tab badges want this uncollapsed-by-further-
 * consumption figure, not `delta->goods[]`'s net (see
 * ColonizeColonyPreview.craft_gross's header comment in colony_preview.h).
 *
 * `shortfall[]` now carries entries on *both* sides of a recipe, not just
 * the output: `shortfall[out_cargo]` is the usual "output not made for lack
 * of input" figure; `shortfall[in_cargo]` (added alongside it, same array)
 * is the new symmetric "input the staffed worker(s) wanted but the
 * warehouse didn't have" figure — player-caught (New Amsterdam Weaver's
 * House, dutch-reports.SAV): DOS shows a shortfall indicator on the raw
 * good's own row too (Cotton), using the exact same visual the output
 * shortfall (Cloth) uses, not just on the manufactured good.
 *
 * `capacity_out` (optional, NULL to skip) receives each out_cargo's full
 * *uncapped* worker capacity this tick — `total_out` before the stock
 * clamp, i.e. what every staffed worker could produce if input were never
 * short. Settlement badges want this (a worker's maximum potential output,
 * matching DOS), not `gross_out`'s stock-clamped actual — player-caught:
 * the Weaver's House badge showed 5 (this tick's actual, cotton-limited
 * output) instead of 10 (both staffed workers' real capacity).
 */
void colony_craft_preview(
  const ColonizeColonyPool* pool,
  ColonizeColony* scratch,
  int shortfall[COLONIZE_CARGO_COUNT],
  ColonizeColonyProdDelta* delta,
  int sol_bonus,
  int gross_out[COLONIZE_CARGO_COUNT],
  int capacity_out[COLONIZE_CARGO_COUNT]
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
