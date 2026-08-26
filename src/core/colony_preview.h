#ifndef COLONIZE_COLONY_PREVIEW_H
#define COLONIZE_COLONY_PREVIEW_H

#include "core/colony.h"
#include "core/map.h"

typedef struct ColonizeCol1Save ColonizeCol1Save;

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
  /*
   * Per-tier GROSS amounts, uncollapsed by downstream consumption within
   * this same tick — for the colony screen's Production tab badges, which
   * golden-confirmed (New Amsterdam, dutch-reports.SAV) show what each
   * assigned worker actually produced this turn, not the warehouse-delta
   * `goods[]` above (e.g. Ore badge reads 28 = both ore-miner tiles' raw
   * output, even though `goods[ORE]` is only 4 once the Blacksmith's own
   * 24-ore draw nets it down; Tools badge reads 24 = the Blacksmith's gross
   * output, even though `goods[TOOLS]` is only 14 once the Armory's 10-tool
   * draw nets it down further). `field_gross` is field-tile worker output,
   * plus the town-commons secondary yield (e.g. New Amsterdam's Cotton,
   * entirely center-tile — no colonist works it, but the Production tab
   * still needs a nonzero gross to pair with the Weaver's input shortfall,
   * player-reported) — excludes only the town-commons FOOD yield (shown
   * via the People band, not this tab) and any craft output. `craft_gross` is each
   * recipe's actual (stock-clamped) production this tick, keyed by
   * out_cargo. A raw good that's also a craft out_cargo (none currently
   * are) would need both summed; every other cargo needs exactly one of
   * the two, or neither (Horses/Food: keep reading `goods[]`, see
   * colony_screen.c).
   */
  int field_gross[COLONIZE_CARGO_COUNT];
  int craft_gross[COLONIZE_CARGO_COUNT];
  /*
   * Each out_cargo's full worker capacity this tick — `craft_gross` above,
   * but *before* the stock clamp (what every staffed worker could produce
   * if the recipe's input were never short). Settlement badges want this
   * (a worker's maximum potential output, matching DOS), not the stock-
   * clamped actual — player-caught (New Amsterdam Weaver's House,
   * dutch-reports.SAV): the badge showed 5 (this tick's cotton-limited
   * actual output) instead of 10 (the staffed worker's real capacity).
   * Equal to `craft_gross` whenever there's no shortfall.
   */
  int craft_capacity[COLONIZE_CARGO_COUNT];
} ColonizeColonyPreview;

void colony_preview_compute(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeColonyPreview* out
);

/* Best field job for an unworked tile (max yield); -1 if none. */
int colony_preview_best_job(const ColonizeWorldMap* map, int x, int y);

/* Second-best job (distinct cargo from first); -1 if none. */
int colony_preview_second_job(const ColonizeWorldMap* map, int x, int y, int first_job);

#endif
