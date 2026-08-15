#ifndef COLONIZE_COLONY_PRODUCTION_H
#define COLONIZE_COLONY_PRODUCTION_H

#include <stdbool.h>

#include "core/colony.h"
#include "core/map.h"

typedef struct ColonizeCol1Save ColonizeCol1Save;

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
#define COLONIZE_PROF_TEACHER 18
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
 *
 * `sol_bonus` (colony_prod_sol_bonus) is folded in *before* the tier/skill math,
 * matching FUN_15eb_1d4c exactly: shop tier re-adds the class tag only (not
 * sol_bonus again), factory tier applies the ×1.5 to the whole running total
 * (tag math + sol_bonus together), and a skill match doubles the whole thing
 * too — not a flat add after, which only happens to match DOS at house/shop
 * tier with an unmatched skill. Pass 0 for callers that intentionally show
 * the un-modified base rate (e.g. settlement badges — see building_production.md
 * "UI: settlement badges vs Production tab"). Clamped to >= 0 (DOS does the
 * same at its shared epilogue).
 */
int colony_prod_manufacturing_output(
  const char* building_name,
  int profession,
  int craft_profession,
  int sol_bonus
);

/* Input is derived from the tier's un-modified base output (sol_bonus does not
 * affect raw-good consumption — DOS's input-side 6-in/9-out ratio is a
 * separate, not-yet-traced code path from FUN_15eb_1d4c's output calc). */
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

/*
 * Colony SoL % for display / production (capped 0..100).
 * Prefer Col1 rebel_dividend/divisor at colony tile; else nation
 * liberty_bells_total/4 (FUN_43f7_0004-shaped stand-in when rebel fields empty).
 * FUN_15eb_0274: +20 while Bolivar held and colony nation is human.
 */
int colony_prod_sol_percent(const ColonizeCol1Save* col1, const ColonizeColony* colony);

/*
 * DOS net production mod per unit (field/craft/hammers/bells/crosses):
 *   tories = (pop * (100 - sol%) + 50) / 100
 *   thresh = human ? (10 - difficulty) : 10
 *   mod    = -floor(tories / thresh) + max(sol latches, live SoL ≥50/≥100)
 * See sons_of_liberty.md / difficulty.md.
 */
int colony_prod_sol_bonus(const ColonizeCol1Save* col1, const ColonizeColony* colony);

/*
 * One-step latch Col1 +0x1c sol_50 / sol_100 from colony SoL % (FUN_364b_0688
 * Phase D). Majority then unanimous take separate ticks.
 */
void colony_prod_refresh_sol_flags(ColonizeColony* colony, const ColonizeCol1Save* col1);

/*
 * FUN_364b_0688 Phase C: shrink rebel pairs ≈÷64, divisor += pop*2,
 * dividend += bells (WoI + crown-occupied: bells = -(bells>>1)), clamp.
 * No-op without matching Col1 colony at (x,y). See sons_of_liberty.md.
 */
void colony_prod_tick_rebel_accumulators(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  ColonizeCol1Save* col1
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
 * FF-aware variants (fandom_col1994 / Colonization.pdf):
 *   statesmen_bonus_pct — Jefferson: +50% on Town Hall (statesmen) worker bells
 *   all_bells_bonus_pct  — Paine: +current tax rate % on colony bells (after press/newspaper)
 *   crosses_bonus_pct   — Penn: +50% on colony cross production
 */
int colony_prod_colony_bells_ff(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int statesmen_bonus_pct,
  int all_bells_bonus_pct
);
int colony_prod_colony_crosses_ff(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int crosses_bonus_pct
);

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

/*
 * Settlement-view building strip: Town Hall / Church / Cathedral free output
 * plus assigned workers. Colony-wide base +1 cross is people-meter only.
 */
int colony_prod_building_display_output(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int building_type
);

#endif
