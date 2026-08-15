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

/*
 * Tile yield for colonist `profession` on `field_job` (convert +1, expert ×2
 * when matched). `has_docks`: pass whether the colony owns Docks (or an
 * upgrade: Drydock/Shipyard) — Fisherman yields 0 without it, matching DOS
 * (FUN_15eb_18ec). Irrelevant for other jobs.
 */
int colony_yield_for_worker(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  int profession,
  bool has_docks
);

/*
 * Colony SoL % for display / production (capped 0..100).
 * Prefer Col1 rebel_dividend/divisor at colony tile; else nation
 * liberty_bells_total/4 (FUN_43f7_0004-shaped stand-in when rebel fields empty).
 * FUN_15eb_0274: +20 while Bolivar held and colony nation is human.
 */
int colony_prod_sol_percent(const ColonizeCol1Save* col1, const ColonizeColony* colony);

/*
 * DOS net production mod per unit (craft/hammers/bells/crosses — NOT field
 * yields, which use colony_prod_sol_bonus_field instead, see below):
 *   tories = (pop * (100 - sol%) + 50) / 100
 *   thresh = human ? (10 - difficulty) : 10
 *   mod    = -floor(tories / thresh) + max(sol latches, live SoL ≥50/≥100)
 * See sons_of_liberty.md / difficulty.md.
 */
int colony_prod_sol_bonus(const ColonizeCol1Save* col1, const ColonizeColony* colony);

/*
 * Field-yield variant of colony_prod_sol_bonus: returns 0 outright for
 * AI-controlled colonies instead of computing a reduced-but-nonzero value —
 * FUN_15eb_18ec zeroes the whole SoL/Tory term for AI, unlike
 * FUN_15eb_1d4c (manufacturing/bells/crosses/hammers), which only changes
 * the threshold. Use this for field/area yields; use colony_prod_sol_bonus
 * for everything else. See manufacturing_worker_calc_1d4c.md.
 */
int colony_prod_sol_bonus_field(const ColonizeCol1Save* col1, const ColonizeColony* colony);

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

/*
 * sol_bonus (colony_prod_sol_bonus) folds in before the skill-driven
 * doubling — matches FUN_15eb_1d4c's Preacher body: skill match picks a flat
 * top-rate baseline (not a class-scale ×2), sol_bonus adds next,
 * `colony_has_cathedral` (a *colony-wide* flag, not this worker's own
 * building) doubles next, and `nation_has_penn` (William Penn, "+50% cross
 * production") multiplies by another ×1.5 *after* that — DOS's Preacher
 * body falls through into the Penn check unconditionally, so Cathedral+Penn
 * stack to ×3 for that worker, not a flat ×1.5 of the colony total (the
 * port used to apply Penn that way; wrong — see
 * colony_prod_colony_crosses_ff). Pass sol_bonus=0,
 * colony_has_cathedral=false, nation_has_penn=false for callers that
 * intentionally show the un-modified base rate (settlement badges). See
 * manufacturing_worker_calc_1d4c.md.
 */
int colony_prod_crosses_worker(
  const char* building_name,
  int profession,
  int sol_bonus,
  bool colony_has_cathedral,
  bool nation_has_penn
);

/*
 * sol_bonus (colony_prod_sol_bonus) is folded in *before* the skill-match
 * doubling, matching FUN_15eb_1d4c's Statesman body exactly. Pass 0 for
 * callers that intentionally show the un-modified base rate (settlement
 * badges).
 */
int colony_prod_bells_worker(const char* building_name, int profession, int sol_bonus);

/* Same shape as colony_prod_crosses_worker (Carpenter body) —
 * `colony_has_lumber_mill` is colony-wide, not this worker's own building. */
int colony_prod_hammers_worker(
  const char* building_name,
  int profession,
  int sol_bonus,
  bool colony_has_lumber_mill
);

/* Passive crosses when Church (+1) or Cathedral (+1) is built — same
 * passive either way, confirmed via FUN_15eb_1f72 (not manual-sourced
 * +2/+3). See manufacturing_worker_calc_1d4c.md. */
int colony_prod_church_passive_crosses(const char* building_name);

#define COLONY_PROD_COLONY_BASE_CROSSES 1

/* Crosses / bells for one colony (assigned workers + building passives + colony base). */
int colony_prod_colony_crosses(const ColonizeColonyPool* pool, const ColonizeColony* colony);
int colony_prod_colony_bells(const ColonizeColonyPool* pool, const ColonizeColony* colony);

/*
 * FF-aware variants (fandom_col1994 / Colonization.pdf):
 *   statesmen_bonus_pct — Jefferson: +50% on Town Hall (statesmen) worker bells
 *   all_bells_bonus_pct  — Paine: +current tax rate % on colony bells (after press/newspaper)
 *
 * `sol_bonus` (colony_prod_bells_ff only) folds the SoL/Tory term into each
 * bell worker individually (colony_prod_bells_worker) instead of a flat
 * post-hoc colony-wide add — pass 0 to get the pre-2026-08-15 un-modified
 * behavior (e.g. the rebel-accumulator tick, which must not feed sol back
 * into itself). When there are no bell workers but Town Hall exists, the
 * passive bell still gets sol_bonus added directly (nothing to fold it into).
 */
int colony_prod_colony_bells_ff(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int statesmen_bonus_pct,
  int all_bells_bonus_pct,
  int sol_bonus
);
/*
 * nation_has_penn — William Penn, "+50% cross production": folds into each
 * Preacher worker individually (colony_prod_crosses_worker), stacking with
 * that worker's own Cathedral ×2, not a flat colony-total multiply — see
 * colony_prod_crosses_worker's comment and manufacturing_worker_calc_1d4c.md.
 * sol_bonus: same per-worker-fold pattern as colony_prod_colony_bells_ff.
 */
int colony_prod_colony_crosses_ff(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  bool nation_has_penn,
  int sol_bonus
);

/*
 * Sum carpenter hammer output and lumber consumed this turn.
 * Only assigned workers at Carpenter's Shop / Lumber Mill produce hammers.
 * sol_bonus folds into each hammer worker individually
 * (colony_prod_hammers_worker) and only affects the returned hammer count —
 * *out_lumber_use always reflects the un-modified base rate (lumber
 * consumption doesn't scale with SoL/Tory).
 */
int colony_prod_colony_hammers(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int sol_bonus,
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
