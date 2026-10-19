#include "core/colony_preview.h"

#include <string.h>

#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/turn.h"
#include "core/turn_internal.h"

void colony_preview_compute_w(
  const ColonizeWorld* w,
  const ColonizeColony* colony,
  ColonizeColonyPreview* out
) {
  const ColonizeColonyPool* pool = w->colonies;
  const ColonizeWorldMap* map = w->map;
  const ColonizeCol1Save* col1 = w->col1;

  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  if (!pool || !colony || !colony->active) {
    return;
  }

  const int pop = colonies_headcount(colony);
  const int sol_b = colony_prod_sol_bonus(col1, colony);

  /* Town commons + worked plots: the tick's own Phase A field composition
   * (turn_compose_colony_fields), not a copy of it (port_bugs.md #1111).
   * field_gross is field-worker + town-commons secondary only (no commons
   * food) — see its header comment in colony_preview.h. New Amsterdam's
   * Cotton is entirely a town-commons secondary yield, yet the Production
   * tab must still pair it with the Weaver's shortfall. */
  {
    TurnColonyFieldYield fy;
    turn_compose_colony_fields(pool, map, col1, colony, &fy);
    out->goods[COLONIZE_CARGO_FOOD] += fy.commons_food;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      out->goods[i] += fy.gross[i];
      out->field_gross[i] += fy.gross[i];
    }
    out->food_fish = fy.fish;
  }

  /* FUN_364b_0688 Phase B: AI Euro food += difficulty>>1 — same subsidy
   * turn.c applies before consumption (golden_colony_preview01 caught every
   * AI colony's preview one food short of the actual tick without it). */
  int ai_food_subsidy = 0;
  if (col1 && colony->nation_id >= 0 && colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
      col1->player[colony->nation_id].control != 0) {
    int diff = (int)col1->head.difficulty;
    diff = diff < 0 ? 0 : (diff > 4 ? 4 : diff);
    ai_food_subsidy = diff >> 1;
    out->goods[COLONIZE_CARGO_FOOD] += ai_food_subsidy;
  }

  out->food_produced = out->goods[COLONIZE_CARGO_FOOD];
  if (out->food_fish > out->food_produced) {
    out->food_fish = out->food_produced;
  }
  out->food_consumed = pop > 0 ? pop * 2 : 0;
  out->food_net = out->food_produced - out->food_consumed;

  /* Horse breeding (turn.c turn_produce_one_colony) — must show up in the
   * Production tab's Horses row and reduce the Food row by the same amount,
   * or the preview misses a real stock change that happens every EOT tick.
   * DOS-confirmed 2026-08-26 against real DOS ground truth (see
   * colony_prod_horse_breed's header comment in colony_production.h). */
  int horse_shortfall = 0; /* folded into out->shortfall[HORSES] after
                             * colony_craft_preview below, which memsets
                             * out->shortfall at its own start. */
  {
    const bool has_stable = colonies_has_building_row(pool, colony, COLONY_BUILDING_STABLE);
    const int warehouse_cap =
      colonies_warehouse_capacity(pool, colony, COLONIZE_CARGO_HORSES);
    const ColonyProdHorseBreed breed = colony_prod_horse_breed(
      colony->stock[COLONIZE_CARGO_HORSES],
      pop,
      out->food_produced - ai_food_subsidy, /* 1f72 reads the Phase A gross */
      warehouse_cap,
      has_stable
    );
    if (breed.bred > 0) {
      out->goods[COLONIZE_CARGO_HORSES] += breed.bred;
      out->goods[COLONIZE_CARGO_FOOD] -= breed.bred;
      out->food_net -= breed.bred;
    }
    /* DOS scratch 0x8e6a, report-only. Outside the bred>0 gate: a herd with
     * no food surplus breeds 0 but must still show its red potential. */
    horse_shortfall = breed.shortfall;
  }

  {
    ColonizeColony scratch = *colony;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      scratch.stock[i] += out->goods[i];
    }
    ColonizeColonyProdDelta craft_delta;
    /* port_bugs.md #898: an AI-controlled colony's craft pass ignores the input
     * shortfall (FUN_364b_0688 raw 57241-57243) — same control gate as the
     * AI food subsidy above, or the preview lands short of the tick. */
    const bool ai_controlled =
      col1 != NULL && colony->nation_id >= 0 &&
      (colony->nation_id >= (int)COLONIZE_COL1_NATION_COUNT ||
       col1->player[colony->nation_id].control != 0);
    colony_craft_preview(
      pool, &scratch, out->shortfall, &craft_delta, sol_b, out->craft_gross,
      out->craft_capacity, ai_controlled
    );
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      out->goods[i] += craft_delta.goods[i];
    }
    /* colony_craft_preview memsets out->shortfall at its own start, so the
     * horse-breeding shortfall computed above has to be folded in after. */
    out->shortfall[COLONIZE_CARGO_HORSES] += horse_shortfall;
  }

  /* Bells / crosses: the Jefferson/Paine/Penn/is-AI derivation and the
   * sol_b fold live in turn.h's turn_compose_colony_bells_crosses, the same
   * entry point turn.c's EOT tick uses (audit CO-4) — a private copy here
   * silently undercounted the Production tab whenever one of those Founding
   * Fathers was active on only one side. */
  turn_compose_colony_bells_crosses(pool, colony, col1, sol_b, &out->bells, &out->crosses);

  {
    /* Hammers bank even with no project queued (turn.c "TURN5→6" comment) —
     * preview must show that too, not just while a Construction item is
     * selected, or the player never sees lumber about to be consumed.
     * sol_b folds into each Carpenter worker individually, inside
     * colony_prod_colony_hammers (matches FUN_15eb_1d4c's Carpenter body).
     * Capped by stored lumber plus this turn's field harvest, as in the
     * tick and FUN_15eb_0b52's gross-lumber shortfall calculation. */
    int hammers_add = colony_prod_colony_hammers(pool, colony, sol_b, NULL);
    if (hammers_add > 0) {
      out->hammers_capacity = hammers_add;
      {
        int hammers = hammers_add;
        /*
         * port_bugs.md (hammers_lumber.SAV): the REAL tick spends this same turn's
         * Lumberjack output too (its clamp runs after field production lands
         * in stock — turn.c "same-turn production IS spendable"), so the cap
         * here is stock + this tick's field lumber. The old stock-only cap
         * made the hammers row vanish from the Production tab whenever
         * storage ran low even though the tick was banking hammers fine.
         */
        const int lumber_avail =
          colony->stock[COLONIZE_CARGO_LUMBER] + out->field_gross[COLONIZE_CARGO_LUMBER];
        if (hammers > lumber_avail) {
          hammers = lumber_avail;
        }
        out->hammers = hammers;
        /* The tick's carpenter spend (turn.c debits stock 1:1 after field
         * production lands) — goods[] is the net warehouse delta, so it must
         * carry the debit too (can go negative: spending stored lumber).
         * The old backwards gate hid this: preview hammers and the tick's
         * spend were zero together, so the missing debit never showed. */
        out->goods[COLONIZE_CARGO_LUMBER] -= hammers;
      }
    }
  }
}

