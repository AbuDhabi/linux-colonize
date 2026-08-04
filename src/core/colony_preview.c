#include "core/colony_preview.h"

#include <string.h>

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

static int preview_workplace_workers(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* needle
) {
  if (!pool || !colony || !needle) {
    return 0;
  }
  int n = 0;
  for (int i = 0; i < colony->colonist_count; ++i) {
    if (!colony->colonists[i].active) {
      continue;
    }
    const int bt = colony->colonists[i].building_type;
    if (bt >= 0 && bt < pool->building_type_count &&
        strstr(pool->building_types[bt].name, needle) != NULL) {
      n++;
    }
  }
  return n;
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

  /* Center tile auto-yield (town commons: food + one other). */
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
      const int yld = colony_yield_for_tile(map, colony->x + dx, colony->y + dy, c->field_job);
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

  /* Craft shortfalls + net craft (preview without mutating). */
  {
    ColonizeColony scratch = *colony;
    for (int i = 0; i < COLONIZE_CARGO_COUNT; ++i) {
      scratch.stock[i] += out->goods[i];
    }
    /* Capacity wanted vs available raw — use craft on a stock snapshot. */
    typedef struct {
      const char* needle;
      int in_cargo;
      int out_cargo;
    } Rec;
    static const Rec k_rec[] = {
      {"Rum Distill", COLONIZE_CARGO_SUGAR, COLONIZE_CARGO_RUM},
      {"Tobacconist", COLONIZE_CARGO_TOBACCO, COLONIZE_CARGO_CIGARS},
      {"Weaver", COLONIZE_CARGO_COTTON, COLONIZE_CARGO_CLOTH},
      {"Textile", COLONIZE_CARGO_COTTON, COLONIZE_CARGO_CLOTH},
      {"Fur Trad", COLONIZE_CARGO_FURS, COLONIZE_CARGO_COATS},
      {"Fur Fact", COLONIZE_CARGO_FURS, COLONIZE_CARGO_COATS},
      {"Blacksmith", COLONIZE_CARGO_ORE, COLONIZE_CARGO_TOOLS},
      {"Iron Works", COLONIZE_CARGO_ORE, COLONIZE_CARGO_TOOLS},
      {"Armory", COLONIZE_CARGO_TOOLS, COLONIZE_CARGO_MUSKETS},
      {"Magazine", COLONIZE_CARGO_TOOLS, COLONIZE_CARGO_MUSKETS},
      {"Arsenal", COLONIZE_CARGO_TOOLS, COLONIZE_CARGO_MUSKETS},
    };
    bool done[COLONIZE_CARGO_COUNT][COLONIZE_CARGO_COUNT];
    memset(done, 0, sizeof(done));
    for (size_t r = 0; r < sizeof(k_rec) / sizeof(k_rec[0]); ++r) {
      const Rec* rec = &k_rec[r];
      if (done[rec->in_cargo][rec->out_cargo]) {
        continue;
      }
      int capacity = 0;
      for (size_t r2 = 0; r2 < sizeof(k_rec) / sizeof(k_rec[0]); ++r2) {
        if (k_rec[r2].in_cargo != rec->in_cargo || k_rec[r2].out_cargo != rec->out_cargo) {
          continue;
        }
        for (int i = 0; i < scratch.colonist_count; ++i) {
          const ColonizeColonist* c = &scratch.colonists[i];
          if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
            continue;
          }
          const char* bname = pool->building_types[c->building_type].name;
          if (!bname || strstr(bname, k_rec[r2].needle) == NULL) {
            continue;
          }
          int rate = 3;
          if (strstr(bname, "Factory") || strstr(bname, "Iron Works") || strstr(bname, "Arsenal")) {
            rate = 8;
          } else if (
            strstr(bname, "Shop") || strstr(bname, "Distillery") || strstr(bname, "Trading Post") ||
            strstr(bname, "Magazine")
          ) {
            rate = 5;
          }
          capacity += rate;
        }
      }
      done[rec->in_cargo][rec->out_cargo] = true;
      if (capacity <= 0) {
        continue;
      }
      int amount = capacity;
      if (scratch.stock[rec->in_cargo] < amount) {
        out->shortfall[rec->out_cargo] += capacity - scratch.stock[rec->in_cargo];
        amount = scratch.stock[rec->in_cargo];
      }
      if (amount <= 0) {
        continue;
      }
      scratch.stock[rec->in_cargo] -= amount;
      scratch.stock[rec->out_cargo] += amount;
      out->goods[rec->in_cargo] -= amount;
      out->goods[rec->out_cargo] += amount;
    }
  }

  /* Crosses / bells (match turn_count_bells_and_crosses style). */
  if (preview_building_has(pool, colony, "Church") || preview_building_has(pool, colony, "Cathedral")) {
    out->crosses += 1 + pop / 4;
  }
  out->crosses += preview_workplace_workers(pool, colony, "Church");
  out->crosses += preview_workplace_workers(pool, colony, "Cathedral");
  if (preview_building_has(pool, colony, "Town Hall")) {
    out->bells += 1 + pop / 4;
  }
  out->bells += preview_workplace_workers(pool, colony, "Town Hall");
  out->bells += preview_workplace_workers(pool, colony, "Printing");
  out->bells += preview_workplace_workers(pool, colony, "Newspaper");

  /* Carpenter hammers. */
  {
    int hammer_workers = preview_workplace_workers(pool, colony, "Carpenter");
    if (hammer_workers == 0 && preview_building_has(pool, colony, "Carpenter")) {
      hammer_workers = 1;
    }
    if (colony->building_in_production >= 0 && hammer_workers > 0) {
      int lumber =
        colony->stock[COLONIZE_CARGO_LUMBER] + out->goods[COLONIZE_CARGO_LUMBER];
      int lumber_use = hammer_workers;
      if (lumber < lumber_use) {
        lumber_use = lumber > 0 ? lumber : 0;
      }
      out->hammers = lumber_use > 0 ? lumber_use : hammer_workers;
    }
  }
}
