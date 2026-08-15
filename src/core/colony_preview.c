#include "core/colony_preview.h"

#include <string.h>

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

  if (map) {
    ColonizeTownCommonsYield tc;
    colony_yield_town_commons(map, colony->x, colony->y, &tc);
    if (tc.food > 0) {
      out->goods[COLONIZE_CARGO_FOOD] += tc.food;
    }
    if (tc.secondary_amount > 0 && tc.secondary_cargo >= 0 &&
        tc.secondary_cargo < COLONIZE_CARGO_COUNT) {
      out->goods[tc.secondary_cargo] += tc.secondary_amount;
    }

    for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
      const int who = (int)colony->tiles[ti];
      if (who < 0 || who >= colony->colonist_count) {
        continue;
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
      int yld = colony_yield_for_worker(
        map, colony->x + dx, colony->y + dy, c->field_job, c->profession
      );
      /* Henry Hudson: fur trapper output +100% (turn.c turn_produce_one_colony). */
      if (yld > 0 && c->field_job == COLONIZE_JOB_FUR_TRAPPER && col1 &&
          founding_fathers_nation_has(col1, colony->nation_id, FF_HENRY_HUDSON)) {
        yld *= 2;
      }
      if (yld > 0 && sol_b > 0) {
        yld += sol_b;
      }
      const int cargo = colony_yield_job_cargo(c->field_job);
      if (yld > 0 && cargo >= 0 && cargo < COLONIZE_CARGO_COUNT) {
        out->goods[cargo] += yld;
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
  if (colony->stock[COLONIZE_CARGO_HORSES] >= 2 && out->food_net > 0) {
    bool has_stable = false;
    for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
      if (colony->has_building[i] && pool->building_types[i].name &&
          strstr(pool->building_types[i].name, "Stable") != NULL) {
        has_stable = true;
        break;
      }
    }
    const int cap = has_stable ? 4 : 2;
    int breed = out->food_net / 2;
    if (breed > cap) {
      breed = cap;
    }
    int food_avail = colony->stock[COLONIZE_CARGO_FOOD] + out->food_net;
    if (breed > food_avail) {
      breed = food_avail > 0 ? food_avail : 0;
    }
    if (breed > 0) {
      out->goods[COLONIZE_CARGO_HORSES] += breed;
      out->goods[COLONIZE_CARGO_FOOD] -= breed;
      out->food_net -= breed;
    }
  }

  {
    ColonizeColony scratch = *colony;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      scratch.stock[i] += out->goods[i];
    }
    ColonizeColonyProdDelta craft_delta;
    colony_craft_preview(pool, &scratch, out->shortfall, &craft_delta, sol_b);
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      out->goods[i] += craft_delta.goods[i];
    }
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
  const int penn_crosses_pct =
    (col1 && founding_fathers_nation_has(col1, nation_id, FF_WILLIAM_PENN)) ? 50 : 0;
  out->crosses = colony_prod_colony_crosses_ff(pool, colony, penn_crosses_pct);
  out->bells = colony_prod_colony_bells_ff(pool, colony, statesmen_pct, paine_tax_pct);
  if (sol_b > 0) {
    /* SoL +1/+2 per production unit — per worker, not flat; must match
     * turn_count_bells_and_crosses_for_nation's per-colony body (turn.c). */
    int bell_workers = 0;
    int cross_workers = 0;
    for (int p = 0; p < colony->colonist_count; ++p) {
      const ColonizeColonist* cc = &colony->colonists[p];
      if (!cc->active || cc->building_type < 0 || cc->building_type >= pool->building_type_count) {
        continue;
      }
      const char* bn = pool->building_types[cc->building_type].name;
      if (colony_prod_bells_worker(bn, cc->profession) > 0) {
        bell_workers++;
      }
      if (colony_prod_crosses_worker(bn, cc->profession) > 0) {
        cross_workers++;
      }
    }
    if (bell_workers > 0) {
      out->bells += sol_b * bell_workers;
    } else if (out->bells > 0) {
      out->bells += sol_b; /* Town Hall / press passive unit */
    }
    if (cross_workers > 0) {
      out->crosses += sol_b * cross_workers;
    } else if (out->crosses > 0) {
      out->crosses += sol_b; /* church passive / colony base */
    }
  }

  {
    /* Hammers bank even with no project queued (turn.c "TURN5→6" comment) —
     * preview must show that too, not just while a Construction item is
     * selected, or the player never sees lumber about to be consumed. */
    int lumber_use = 0;
    int hammers_add = colony_prod_colony_hammers(pool, colony, &lumber_use);
    if (hammers_add > 0 && sol_b > 0) {
      int carpenters = 0;
      for (int ci = 0; ci < colony->colonist_count; ++ci) {
        const ColonizeColonist* cc = &colony->colonists[ci];
        if (!cc->active || cc->building_type < 0) {
          continue;
        }
        const char* bn = pool->building_types[cc->building_type].name;
        if (bn && (strstr(bn, "Carpenter") != NULL || strstr(bn, "Lumber Mill") != NULL)) {
          carpenters++;
        }
      }
      hammers_add += sol_b * carpenters;
    }
    if (hammers_add > 0) {
      int lumber = colony->stock[COLONIZE_CARGO_LUMBER] + out->goods[COLONIZE_CARGO_LUMBER];
      if (lumber_use > lumber) {
        lumber_use = lumber > 0 ? lumber : 0;
      }
      if (colony->building_in_production >= 0) {
        out->hammers = lumber_use > 0 ? lumber_use : hammers_add;
      } else if (lumber_use > 0) {
        out->hammers = lumber_use;
      } else {
        out->hammers = hammers_add;
      }
    }
  }
}
