#include "core/colony_production.h"

#include <string.h>

#include "core/colony_yield.h"

static bool colony_prod_name_has(const char* name, const char* needle) {
  return name && needle && strstr(name, needle) != NULL;
}

ColonyProdTier colony_prod_building_tier(const char* building_name) {
  if (!building_name) {
    return COLONY_PROD_TIER_HOUSE;
  }
  if (colony_prod_name_has(building_name, "Factory") ||
      colony_prod_name_has(building_name, "Iron Works") ||
      colony_prod_name_has(building_name, "Arsenal") ||
      colony_prod_name_has(building_name, "Textile Mill")) {
    return COLONY_PROD_TIER_FACTORY;
  }
  /* Carpenter's Shop is house-tier (3); Lumber Mill is shop-tier (6). Match
   * before the generic "Shop" needle so "Carpenter's Shop" is not mis-tiered. */
  if (colony_prod_name_has(building_name, "Lumber Mill")) {
    return COLONY_PROD_TIER_SHOP;
  }
  if (colony_prod_name_has(building_name, "Carpenter")) {
    return COLONY_PROD_TIER_HOUSE;
  }
  if (colony_prod_name_has(building_name, "Shop") ||
      colony_prod_name_has(building_name, "Distillery") ||
      colony_prod_name_has(building_name, "Trading Post") ||
      colony_prod_name_has(building_name, "Magazine")) {
    return COLONY_PROD_TIER_SHOP;
  }
  return COLONY_PROD_TIER_HOUSE;
}

int colony_prod_tier_free_output(ColonyProdTier tier) {
  switch (tier) {
  case COLONY_PROD_TIER_SHOP:
    return 6;
  case COLONY_PROD_TIER_FACTORY:
    return 9;
  case COLONY_PROD_TIER_HOUSE:
  default:
    return 3;
  }
}

int colony_prod_tier_input_for_output(ColonyProdTier tier, int output) {
  if (output <= 0) {
    return 0;
  }
  if (tier == COLONY_PROD_TIER_FACTORY) {
    return (output * 6 + 8) / 9;
  }
  return output;
}

static int colony_prod_scale_by_class(int profession, int free_tier_output) {
  if (profession == COLONIZE_PROF_CRIMINAL || profession == COLONIZE_PROF_CONVERT) {
    return free_tier_output / 3;
  }
  if (profession == COLONIZE_PROF_INDENTURED) {
    return (free_tier_output * 2) / 3;
  }
  return free_tier_output;
}

static bool colony_prod_craft_skill_matches(int profession, int craft_profession) {
  return profession >= 0 && profession == craft_profession;
}

static int colony_prod_religious_worker_rate(const char* building_name, int profession, int church_rate, int cathedral_rate) {
  int base = colony_prod_name_has(building_name, "Cathedral") ? cathedral_rate
                                                              : church_rate;
  base = colony_prod_scale_by_class(profession, base);
  if (colony_prod_craft_skill_matches(profession, COLONIZE_PROF_PREACHER)) {
    base *= 2;
  }
  return base;
}

int colony_prod_manufacturing_output(
  const char* building_name,
  int profession,
  int craft_profession
) {
  if (!building_name) {
    return 0;
  }
  const ColonyProdTier tier = colony_prod_building_tier(building_name);
  int out = colony_prod_scale_by_class(profession, colony_prod_tier_free_output(tier));
  if (colony_prod_craft_skill_matches(profession, craft_profession)) {
    out *= 2;
  }
  return out;
}

int colony_prod_manufacturing_input(
  const char* building_name,
  int profession,
  int craft_profession
) {
  const int out = colony_prod_manufacturing_output(building_name, profession, craft_profession);
  if (out <= 0) {
    return 0;
  }
  const ColonyProdTier tier = colony_prod_building_tier(building_name);
  return colony_prod_tier_input_for_output(tier, out);
}

bool colony_prod_field_skill_matches(int profession, int field_job) {
  return profession >= 0 && profession == field_job;
}

int colony_yield_for_worker(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  int profession
) {
  int yld = colony_yield_for_tile(map, x, y, field_job);
  if (yld <= 0) {
    return 0;
  }
  if (profession == COLONIZE_PROF_CONVERT) {
    yld += 1;
  }
  if (colony_prod_field_skill_matches(profession, field_job)) {
    yld *= 2;
  }
  return yld;
}

int colony_prod_crosses_worker(const char* building_name, int profession) {
  if (!building_name ||
      (!colony_prod_name_has(building_name, "Church") &&
       !colony_prod_name_has(building_name, "Cathedral"))) {
    return 0;
  }
  return colony_prod_religious_worker_rate(building_name, profession, 3, 6);
}

int colony_prod_bells_worker(const char* building_name, int profession) {
  if (!building_name || !colony_prod_name_has(building_name, "Town Hall")) {
    return 0;
  }
  int base = colony_prod_scale_by_class(profession, 3);
  if (colony_prod_craft_skill_matches(profession, COLONIZE_PROF_STATESMAN)) {
    base *= 2;
  }
  return base;
}

int colony_prod_hammers_worker(const char* building_name, int profession) {
  if (!building_name ||
      (!colony_prod_name_has(building_name, "Carpenter") &&
       !colony_prod_name_has(building_name, "Lumber Mill"))) {
    return 0;
  }
  const ColonyProdTier tier = colony_prod_building_tier(building_name);
  int out = colony_prod_scale_by_class(profession, colony_prod_tier_free_output(tier));
  if (colony_prod_craft_skill_matches(profession, COLONIZE_PROF_CARPENTER)) {
    out *= 2;
  }
  return out;
}

int colony_prod_church_passive_crosses(const char* building_name) {
  if (!building_name) {
    return 0;
  }
  if (colony_prod_name_has(building_name, "Cathedral")) {
    return 3;
  }
  if (colony_prod_name_has(building_name, "Church")) {
    return 2;
  }
  return 0;
}

static bool colony_prod_building_built(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* needle
) {
  if (!pool || !colony || !needle) {
    return false;
  }
  for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    if (!colony->has_building[i]) {
      continue;
    }
    if (colony_prod_name_has(pool->building_types[i].name, needle)) {
      return true;
    }
  }
  return false;
}

int colony_prod_colony_crosses(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  int crosses = COLONY_PROD_COLONY_BASE_CROSSES;
  for (int i = 0; i < pool->building_type_count && i < COLONIZE_BUILDING_TYPES_MAX; ++i) {
    if (!colony->has_building[i]) {
      continue;
    }
    crosses += colony_prod_church_passive_crosses(pool->building_types[i].name);
  }
  for (int p = 0; p < colony->colonist_count; ++p) {
    const ColonizeColonist* c = &colony->colonists[p];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    crosses += colony_prod_crosses_worker(pool->building_types[c->building_type].name, c->profession);
  }
  return crosses;
}

int colony_prod_colony_bells(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  int bells = 0;
  if (colony_prod_building_built(pool, colony, "Town Hall")) {
    bells += 1;
  }
  for (int p = 0; p < colony->colonist_count; ++p) {
    const ColonizeColonist* c = &colony->colonists[p];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    bells += colony_prod_bells_worker(pool->building_types[c->building_type].name, c->profession);
  }
  int bonus_pct = 0;
  if (colony_prod_building_built(pool, colony, "Printing Press")) {
    bonus_pct += 50;
  }
  if (colony_prod_building_built(pool, colony, "Newspaper")) {
    bonus_pct += 100;
  }
  if (bonus_pct > 0) {
    bells = bells * (100 + bonus_pct) / 100;
  }
  return bells;
}

int colony_prod_colony_hammers(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int* out_lumber_use
) {
  if (out_lumber_use) {
    *out_lumber_use = 0;
  }
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  int total = 0;
  for (int p = 0; p < colony->colonist_count; ++p) {
    const ColonizeColonist* c = &colony->colonists[p];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    const char* bname = pool->building_types[c->building_type].name;
    total += colony_prod_hammers_worker(bname, c->profession);
  }
  if (out_lumber_use && total > 0) {
    *out_lumber_use = total;
  }
  return total;
}

int colony_prod_worker_building_output(
  const ColonizeColonyPool* pool,
  int building_type,
  int profession
) {
  if (!pool || building_type < 0 || building_type >= pool->building_type_count) {
    return 0;
  }
  const char* name = pool->building_types[building_type].name;
  if (!name) {
    return 0;
  }
  if (colony_prod_name_has(name, "Town Hall")) {
    return colony_prod_bells_worker(name, profession);
  }
  if (colony_prod_name_has(name, "Church") || colony_prod_name_has(name, "Cathedral")) {
    return colony_prod_crosses_worker(name, profession);
  }
  if (colony_prod_name_has(name, "Carpenter") || colony_prod_name_has(name, "Lumber Mill")) {
    return colony_prod_hammers_worker(name, profession);
  }
  if (colony_prod_name_has(name, "Rum Distill")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_DISTILLER);
  }
  if (colony_prod_name_has(name, "Tobacconist")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_TOBACCONIST);
  }
  if (colony_prod_name_has(name, "Weaver") || colony_prod_name_has(name, "Textile")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_WEAVER);
  }
  if (colony_prod_name_has(name, "Fur Trad") || colony_prod_name_has(name, "Fur Fact")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_FUR_TRADER);
  }
  if (colony_prod_name_has(name, "Blacksmith") || colony_prod_name_has(name, "Iron Works")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_BLACKSMITH);
  }
  if (colony_prod_name_has(name, "Armory") || colony_prod_name_has(name, "Magazine") ||
      colony_prod_name_has(name, "Arsenal")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_GUNSMITH);
  }
  return 0;
}
