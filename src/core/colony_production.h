#ifndef COLONIZE_COLONY_PRODUCTION_H
#define COLONIZE_COLONY_PRODUCTION_H

#include <stdbool.h>

#include "core/colony.h"
#include "core/map.h"

/* NAMES.TXT @JOB indices (settlement skills and colonist classes). */
#define COLONIZE_PROF_DISTILLER 9
#define COLONIZE_PROF_TOBACCONIST 10
#define COLONIZE_PROF_WEAVER 11
#define COLONIZE_PROF_FUR_TRADER 12
#define COLONIZE_PROF_CARPENTER 13
#define COLONIZE_PROF_BLACKSMITH 14
#define COLONIZE_PROF_GUNSMITH 15
#define COLONIZE_PROF_PREACHER 16
#define COLONIZE_PROF_STATESMAN 17
#define COLONIZE_PROF_FREE_COLONIST 19
#define COLONIZE_PROF_INDENTURED 25
#define COLONIZE_PROF_CRIMINAL 26
#define COLONIZE_PROF_CONVERT 27

typedef enum ColonyProdTier {
  COLONY_PROD_TIER_HOUSE = 0,
  COLONY_PROD_TIER_SHOP,
  COLONY_PROD_TIER_FACTORY,
} ColonyProdTier;

ColonyProdTier colony_prod_building_tier(const char* building_name);

/* Free-colonist manufacturing output at tier (3 / 6 / 9). */
int colony_prod_tier_free_output(ColonyProdTier tier);

/* Cargo input consumed to produce `output` units at tier (factory: 6 in per 9 out). */
int colony_prod_tier_input_for_output(ColonyProdTier tier, int output);

/*
 * Manufacturing output for one worker. `craft_profession` is the @JOB index for the
 * recipe (e.g. COLONIZE_PROF_BLACKSMITH). Wrong skill → free-colonist rate only.
 */
int colony_prod_manufacturing_output(
  const char* building_name,
  int profession,
  int craft_profession
);

int colony_prod_manufacturing_input(
  const char* building_name,
  int profession,
  int craft_profession
);

bool colony_prod_field_skill_matches(int profession, int field_job);

/* Tile yield for colonist `profession` on `field_job` (convert +1, expert ×2 when matched). */
int colony_yield_for_worker(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  int profession
);

int colony_prod_crosses_worker(const char* building_name, int profession);
int colony_prod_bells_worker(const char* building_name, int profession);
int colony_prod_hammers_worker(const char* building_name, int profession);

/* Passive crosses when Church (+2) or Cathedral (+3) is built. */
int colony_prod_church_passive_crosses(const char* building_name);

#define COLONY_PROD_COLONY_BASE_CROSSES 1

/* Crosses / bells for one colony (assigned workers + building passives + colony base). */
int colony_prod_colony_crosses(const ColonizeColonyPool* pool, const ColonizeColony* colony);
int colony_prod_colony_bells(const ColonizeColonyPool* pool, const ColonizeColony* colony);

/*
 * Sum carpenter hammer output and lumber consumed this turn.
 * Only assigned workers at Carpenter's Shop / Lumber Mill produce hammers.
 */
int colony_prod_colony_hammers(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int* out_lumber_use
);

/* Primary goods output for a colonist in a workplace (for settlement badges). */
int colony_prod_worker_building_output(
  const ColonizeColonyPool* pool,
  int building_type,
  int profession
);

#endif
