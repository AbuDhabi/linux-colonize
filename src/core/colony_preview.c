#include "core/colony_preview.h"

#include <string.h>

#include "core/colony.h"
#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/founding_fathers.h"

int colony_preview_best_job(const ColonizeWorldMap* map, int x, int y) {
  int best_job = -1;
  int best_yld = 0;
  for (int j = 0; j < COLONIZE_FIELD_JOB_COUNT; ++j) {
    const int yld = colony_yield_for_tile(map, x, y, j);
    if (yld > best_yld) {
      best_yld = yld;
      best_job = j;
    }
  }
  return best_job;
}

int colony_preview_second_job(const ColonizeWorldMap* map, int x, int y, int first_job) {
  const int first_cargo = colony_yield_job_cargo(first_job);
  int best_job = -1;
  int best_yld = 0;
  for (int j = 0; j < COLONIZE_FIELD_JOB_COUNT; ++j) {
    if (j == first_job) {
      continue;
    }
    const int cargo = colony_yield_job_cargo(j);
    if (cargo == first_cargo) {
      continue;
    }
    const int yld = colony_yield_for_tile(map, x, y, j);
    if (yld > best_yld) {
      best_yld = yld;
      best_job = j;
    }
  }
  return best_job;
}

void colony_preview_compute(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeWorldMap* map,
  const ColonizeCol1Save* col1,
  ColonizeColonyPreview* out
) {
  if (!out) {
    return;
  }
  memset(out, 0, sizeof(*out));
  if (!pool || !colony || !colony->active) {
    return;
  }

  const int pop = colony->colonist_count > 0 ? colony->colonist_count : colony->population;
  const int sol_b = colony_prod_sol_bonus(col1, colony);
  /* Field yields zero this outright for AI colonies — see
   * colony_prod_sol_bonus_field / turn.c's field loop. Buildings
   * (craft/bells/crosses/hammers below) keep the shared sol_b. */
  const int sol_b_field = colony_prod_sol_bonus_field(col1, colony);

  if (map) {
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(map, colony->x, colony->y, sol_b_field, colony->colony_flags, &tc);
    if (tc.food > 0) {
      out->goods[COLONIZE_CARGO_FOOD] += tc.food;
    }
    if (tc.secondary_amount > 0 && tc.secondary_cargo >= 0 &&
        tc.secondary_cargo < COLONIZE_CARGO_COUNT) {
      out->goods[tc.secondary_cargo] += tc.secondary_amount;
      /* Also into field_gross: New Amsterdam's Cotton is entirely a
       * town-commons secondary yield (no colonist works a cotton tile), yet
       * the Production tab must still pair it with the Weaver's shortfall
       * (5 produced / 5 shortfall, player-reported) — the tab's craft-input
       * side needs a nonzero gross figure to show alongside the shortfall
       * or the pairing looks like a shortfall out of nowhere. */
      out->field_gross[tc.secondary_cargo] += tc.secondary_amount;
    }

    /* Docks (or an upgrade: Drydock/Shipyard) gates Fisherman yield to 0 —
     * FUN_15eb_18ec ~11925-11939. Must match turn.c's check. */
    bool has_docks = false;
    for (int bi = 0; bi < pool->building_type_count && bi < COLONIZE_BUILDING_TYPES_MAX; ++bi) {
      if (!colony->has_building[bi]) {
        continue;
      }
      const char* dn = pool->building_types[bi].name;
      if (dn && (strstr(dn, "Docks") != NULL || strstr(dn, "Drydock") != NULL ||
                 strstr(dn, "Shipyard") != NULL)) {
        has_docks = true;
        break;
      }
    }

    bool worked_colonist[32];
    memset(worked_colonist, 0, sizeof(worked_colonist));
    for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
      const int who = (int)colony->tiles[ti];
      if (who < 0 || who >= colony->colonist_count || (who < 32 && worked_colonist[who])) {
        continue;
      }
      if (who < 32) {
        worked_colonist[who] = true;
      }
      const ColonizeColonist* c = &colony->colonists[who];
      if (!c->active || c->field_job < 0) {
        continue;
      }
      int dx = 0;
      int dy = 0;
      if (!colonies_field_tile_delta(ti, &dx, &dy)) {
        continue;
      }
      /* sol_b_field folds into colony_yield_for_worker directly now
       * (2026-08-15, player-confirmed order) — must match turn.c's
       * turn_produce_one_colony exactly, including the same Hudson-after-
       * SoL-fold ordering note there. */
      int yld = colony_yield_for_worker(
        map,
        colony->x + dx,
        colony->y + dy,
        c->field_job,
        c->profession,
        has_docks,
        sol_b_field,
        colony->colony_flags
      );
      /* Henry Hudson: fur trapper output +100% (turn.c turn_produce_one_colony). */
      if (yld > 0 && c->field_job == COLONIZE_JOB_FUR_TRAPPER && col1 &&
          founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)) {
        yld *= 2;
      }
      const int cargo = colony_yield_job_cargo(c->field_job);
      if (yld > 0 && cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) {
        out->goods[cargo] += yld;
        /* Field-worker-only, deliberately excluding the town-commons
         * auto-yield added above (before this loop runs) — see the header
         * comment on `field_gross` in colony_preview.h. A plain memcpy of
         * `goods[]` here would double-count that secondary good. */
        out->field_gross[cargo] += yld;
        if (c->field_job == COLONIZE_JOB_FISHERMAN) {
          out->food_fish += yld;
        }
      }
    }
  }

  out->food_produced = out->goods[COLONIZE_CARGO_FOOD];
  if (out->food_fish > out->food_produced) {
    out->food_fish = out->food_produced;
  }
  out->food_consumed = pop > 0 ? pop * 2 : 0;
  out->food_net = out->food_produced - out->food_consumed;

  /* Horse breeding (turn.c turn_produce_one_colony) — must show up in the
   * Production tab's Horses row and reduce the Food row by the same amount,
   * or the preview misses a real stock change that happens every EOT tick. */
  int horse_shortfall = 0; /* folded into out->shortfall[HORSES] after
                             * colony_craft_preview below, which memsets
                             * out->shortfall at its own start. */
  if (colony->stock[COLONIZE_CARGO_HORSES] >= 2 && out->food_net > 0) {
    bool has_stable = false;
    for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
      if (colony->has_building[i] && pool->building_types[i].name &&
          strstr(pool->building_types[i].name, "Stable") != NULL) {
        has_stable = true;
        break;
      }
    }
    const int cap = has_stable ? 8 : 6;
    int breed = (out->food_net + 1) / 2;
    if (breed > cap) {
      breed = cap;
    }
    int food_avail = colony->stock[COLONIZE_CARGO_FOOD] + out->food_net;
    if (breed > food_avail) {
      breed = food_avail > 0 ? food_avail : 0;
    }
    bool has_warehouse = false;
    bool has_warehouse_expansion = false;
    for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
      if (!colony->has_building[i] || !pool->building_types[i].name) {
        continue;
      }
      if (strstr(pool->building_types[i].name, "Warehouse Expansion") != NULL) {
        has_warehouse_expansion = true;
      } else if (strstr(pool->building_types[i].name, "Warehouse") != NULL) {
        has_warehouse = true;
      }
    }
    const int max_horses = has_warehouse_expansion ? 300 : (has_warehouse ? 200 : 100);
    if (colony->stock[COLONIZE_CARGO_HORSES] + breed > max_horses) {
      breed = max_horses - colony->stock[COLONIZE_CARGO_HORSES];
      if (breed < 0) {
        breed = 0;
      }
    }
    if (breed > 0) {
      out->goods[COLONIZE_CARGO_HORSES] += breed;
      out->goods[COLONIZE_CARGO_FOOD] -= breed;
      out->food_net -= breed;
      /*
       * Player-reported (approximate — this whole breed formula is already
       * flagged "manual/fandom" in turn.c, not DOS-disassembly-confirmed):
       * DOS pairs the Horses row with a shortfall-style second number, same
       * visual as a manufacturing shortfall (Cloth/Cotton) — "N more could
       * have bred but for the food surplus." The half-rounded breed rate
       * only ever spends about half of `food_net` (this turn's un-bred
       * remainder stays in `food_net` right above), so approximate the
       * shortfall as that same leftover — the surplus that, spent instead
       * of banked, could have bred roughly that many more. */
      if (out->food_net > 0) {
        horse_shortfall = out->food_net;
      }
    }
  }

  {
    ColonizeColony scratch = *colony;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      scratch.stock[i] += out->goods[i];
    }
    ColonizeColonyProdDelta craft_delta;
    colony_craft_preview(
      pool, &scratch, out->shortfall, &craft_delta, sol_b, out->craft_gross, out->craft_capacity
    );
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      out->goods[i] += craft_delta.goods[i];
    }
    /* colony_craft_preview memsets out->shortfall at its own start, so the
     * horse-breeding shortfall computed above has to be folded in after. */
    out->shortfall[COLONIZE_CARGO_HORSES] += horse_shortfall;
  }

  /* Jefferson / Paine / Penn — must match turn.c's EOT tick (colony_prod_colony_bells_ff /
   * colony_prod_colony_crosses_ff call sites) or the Production tab preview undercounts
   * bells/crosses for colonies with these Founding Fathers active. */
  const int nation_id = colony->nation_id;
  const int statesmen_pct =
    (col1 && founding_fathers_nation_has(col1, nation_id, FF_THOMAS_JEFFERSON)) ? 50 : 0;
  const int paine_tax_pct =
    (col1 && founding_fathers_nation_has(col1, nation_id, FF_THOMAS_PAINE) &&
     nation_id >= 0 && nation_id < (int)COLONIZE_COL1_NATION_COUNT)
      ? (int)col1->nation[nation_id].tax_rate
      : 0;
  const bool nation_has_penn =
    col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_PENN);
  const bool nation_is_ai =
    col1 && nation_id >= 0 && nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
    col1->player[nation_id].control != 0;
  /* Bells / crosses: sol_b folds into each Statesman/Preacher worker
   * individually, inside colony_prod_colony_bells_ff/_crosses_ff (matches
   * FUN_15eb_1d4c's Statesman/Preacher bodies — see
   * manufacturing_worker_calc_1d4c.md). Must match turn.c's
   * turn_count_bells_and_crosses_for_nation call exactly. */
  out->crosses = colony_prod_colony_crosses_ff(pool, colony, nation_has_penn, sol_b);
  out->bells =
    colony_prod_colony_bells_ff(pool, colony, statesmen_pct, paine_tax_pct, nation_is_ai, sol_b);

  {
    /* Hammers bank even with no project queued (turn.c "TURN5→6" comment) —
     * preview must show that too, not just while a Construction item is
     * selected, or the player never sees lumber about to be consumed.
     * sol_b folds into each Carpenter worker individually, inside
     * colony_prod_colony_hammers (matches FUN_15eb_1d4c's Carpenter body).
     * Capped by lumber on hand *before* this turn's production (mirrors
     * turn.c's real Carpenter hammers block, 2026-08-16 real-DOS fix): a
     * carpenter can't spend lumber this same turn's Lumberjack hasn't
     * delivered yet, and 0 lumber on hand means 0 hammers, not a free
     * hammers_add. */
    int hammers_add = colony_prod_colony_hammers(pool, colony, sol_b, NULL);
    if (hammers_add > 0) {
      int hammers = hammers_add;
      if (hammers > colony->stock[COLONIZE_CARGO_LUMBER]) {
        hammers = colony->stock[COLONIZE_CARGO_LUMBER];
      }
      out->hammers = hammers;
    }
  }
}
