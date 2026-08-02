#include "core/colony_craft.h"

#include <string.h>

typedef struct ColonyCraftRecipe {
  const char* needle; /* match workplace / building name */
  int in_cargo;
  int out_cargo;
} ColonyCraftRecipe;

/* Ore→Tools before Tools→Muskets. */
static const ColonyCraftRecipe k_recipes[] = {
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

static int colony_craft_clamp(int v) {
  if (v < 0) {
    return 0;
  }
  if (v > 65535) {
    return 65535;
  }
  return v;
}

static int colony_craft_rate_for_name(const char* name) {
  if (!name) {
    return 3;
  }
  if (strstr(name, "Factory") != NULL || strstr(name, "Iron Works") != NULL ||
      strstr(name, "Arsenal") != NULL) {
    return 8;
  }
  if (strstr(name, "Shop") != NULL || strstr(name, "Distillery") != NULL ||
      strstr(name, "Trading Post") != NULL || strstr(name, "Magazine") != NULL) {
    return 5;
  }
  return 3; /* House, Armory, etc. */
}

static bool colony_craft_name_matches(const char* name, const char* needle) {
  return name && needle && strstr(name, needle) != NULL;
}

static int colony_craft_workers_capacity(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* needle
) {
  if (!pool || !colony || !needle) {
    return 0;
  }
  int capacity = 0;
  for (int i = 0; i < colony->colonist_count; ++i) {
    const ColonizeColonist* c = &colony->colonists[i];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    const char* bname = pool->building_types[c->building_type].name;
    if (!colony_craft_name_matches(bname, needle)) {
      continue;
    }
    capacity += colony_craft_rate_for_name(bname);
  }
  return capacity;
}

void colony_craft_one_colony(
  ColonizeColonyPool* pool,
  ColonizeColony* colony,
  ColonizeColonyProdDelta* delta
) {
  if (!pool || !colony || !colony->active) {
    return;
  }

  /* Track which (in,out) pairs already ran so Weaver+Textile don't double. */
  bool done_pair[COLONIZE_CARGO_COUNT][COLONIZE_CARGO_COUNT];
  memset(done_pair, 0, sizeof(done_pair));

  for (size_t r = 0; r < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r) {
    const ColonyCraftRecipe* rec = &k_recipes[r];
    if (rec->in_cargo < 0 || rec->in_cargo >= COLONIZE_CARGO_COUNT || rec->out_cargo < 0 ||
        rec->out_cargo >= COLONIZE_CARGO_COUNT) {
      continue;
    }
    if (done_pair[rec->in_cargo][rec->out_cargo]) {
      continue;
    }

    /* Sum capacity across all needles that share this in/out pair. */
    int capacity = 0;
    for (size_t r2 = 0; r2 < sizeof(k_recipes) / sizeof(k_recipes[0]); ++r2) {
      const ColonyCraftRecipe* rec2 = &k_recipes[r2];
      if (rec2->in_cargo != rec->in_cargo || rec2->out_cargo != rec->out_cargo) {
        continue;
      }
      capacity += colony_craft_workers_capacity(pool, colony, rec2->needle);
    }
    done_pair[rec->in_cargo][rec->out_cargo] = true;

    if (capacity <= 0) {
      continue;
    }
    int amount = capacity;
    if (colony->stock[rec->in_cargo] < amount) {
      amount = colony->stock[rec->in_cargo];
    }
    if (amount <= 0) {
      continue;
    }
    colony->stock[rec->in_cargo] -= amount;
    colony->stock[rec->out_cargo] =
      colony_craft_clamp(colony->stock[rec->out_cargo] + amount);
    if (delta) {
      delta->goods[rec->in_cargo] -= amount;
      delta->goods[rec->out_cargo] += amount;
    }
  }
}
