#include "core/colony_preview.h"

#include <string.h>

#include "core/colony_craft.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"

static int preview_building_has(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* needle
) {
  if (!pool || !colony || !needle) {
    return 0;
  }
  for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    if (!colony->has_building[i]) {
      continue;
    }
    if (strstr(pool->building_types[i].name, needle) != NULL) {
      return 1;
    }
  }
  return 0;
}

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
      const int yld = colony_yield_for_worker(
        map, colony->x + dx, colony->y + dy, c->field_job, c->profession
      );
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

  {
    ColonizeColony scratch = *colony;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      scratch.stock[i] += out->goods[i];
    }
    ColonizeColonyProdDelta craft_delta;
    colony_craft_preview(pool, &scratch, out->shortfall, &craft_delta);
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      out->goods[i] += craft_delta.goods[i];
    }
  }

  out->crosses = colony_prod_colony_crosses(pool, colony);
  out->bells = colony_prod_colony_bells(pool, colony);

  if (colony->building_in_production >= 0) {
    int lumber_use = 0;
    const int hammers = colony_prod_colony_hammers(pool, colony, &lumber_use);
    if (hammers > 0) {
      int lumber = colony->stock[COLONIZE_CARGO_LUMBER] + out->goods[COLONIZE_CARGO_LUMBER];
      if (lumber_use > lumber) {
        lumber_use = lumber > 0 ? lumber : 0;
      }
      out->hammers = lumber_use > 0 ? lumber_use : hammers;
    }
  }
}
