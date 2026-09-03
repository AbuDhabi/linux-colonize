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
/* bugs.md 294 family: these are the DOS @JOB bytes (0x19/0x1a/0x1b/0x1c) —
 * the port briefly had 29/30 for servant/criminal, which no real colonist
 * ever carries (unit joins, DOS saves, education all use @JOB), so every
 * class check against them was dead. Note the port also uses 19 (@JOB
 * "Colonist" row) as a free-colonist alias in a few spawn paths. */
#define COLONIZE_PROF_FREE_COLONIST 28
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

/*
 * Raw-good input for one worker's manufacturing output. Factory tier
 * discounts 6-in for 9-out (`colony_prod_tier_input_for_output`); house/shop
 * are 1:1. `sol_bonus` folds into the *output* used to derive this the same
 * way colony_prod_manufacturing_output does — player-confirmed 2026-08-15
 * (Viceroy): factory tier, +2 sentiment, output 12 → input 8, matching
 * `(12*6+8)/9`, not the un-modified-base-output reading this function used
 * to force (`(9*6+8)/9=6`, wrong). Pass 0 for callers that intentionally
 * show the un-modified base rate (settlement badges).
 */
int colony_prod_manufacturing_input(
  const char* building_name,
  int profession,
  int craft_profession,
  int sol_bonus
);

bool colony_prod_field_skill_matches(int profession, int field_job);

/* colony_yield_for_worker moved to colony_yield.h/.c 2026-08-15 — it needs
 * static helpers private to that file (road/river/resource pipeline
 * internals) now that SoL folding and expert road/river doubling are wired
 * in there directly. See colony_yield.h. */

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
 * `nation_is_ai` — DOS AI bells subsidy (FUN_15eb_1f72: `bells += (pop+3)/5`
 * on the Town Hall passive, gated on the colony's nation being AI-controlled
 * — same primitive as colony_prod_sol_bonus_field's AI gate). Player-
 * confirmed 2026-08-15 (Viceroy difficulty): AI free-colonist Statesman
 * nets 5 colony bells vs 3 for human, same setup. Pass
 * `col1->player[nation_id].control != 0` (false when col1 is unavailable).
 * See manufacturing_worker_calc_1d4c.md / nation_crosses_bells_1f72.md.
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
  bool nation_is_ai,
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

/*
 * Horse breeding — FUN_15eb_1f72 tail (viceroy_unpacked.c ~12649-12690,
 * raw .asm at 15eb:2300-2392). The *shape* of the formula (herd-size-based
 * growth potential, gated on a Stable, then capped by this turn's food
 * surplus and by warehouse headroom) is read directly from the decompile;
 * the exact quantity DOS actually applies to the horses stock was pinned
 * down against real DOS ground truth (`golden_colony_prod01`/`02`, Col1
 * `.SAV` fixtures produced by running one turn in original DOS) — the
 * static asm reading alone pointed at the *uncapped* potential feeding the
 * colony's shared production gross/reserve pipeline (`FUN_364b_0688` Phase
 * B), which turned out to overshoot every one of 13 checked Dutch colonies
 * across both goldens by exactly `potential - capped`; the *capped* figure
 * matches all 13 exactly. (The pipeline detail responsible for that
 * capping — which overlay-swapped callee actually consumes `local_22`
 * versus `local_e` — is not fully resolved statically; the applied
 * *result* is, byte-for-byte, against real DOS saves, which is the
 * stronger form of confirmation here.) Colony offset +0xaa (cargo id 8) is
 * the horses stock word.
 *
 *   potential = horses_stock < 2 ? 0
 *             : ceil(horses_stock / divisor) * 2
 *   divisor   = colony_has_stable ? 25 : 50
 *               (FUN_15eb_038e building-catalog index 0x11 = 17 = Stable
 *               in NAMES.TXT @BUILDING order — confirmed against this same
 *               function's already-ported Church(0x25)/Cathedral(0x26)/
 *               Printing Press(0x13)/Newspaper(0x14) FUN_15eb_038e checks,
 *               same indexing convention, see building_production.md)
 *   food_avail = max(0, food_gross_this_turn - population*2)
 *   food_cap   = ceil(food_avail / 2)
 *   capped     = min(potential, food_cap)
 *   headroom   = max(0, warehouse_cap - horses_stock)
 *   bred       = min(capped, headroom)                    -- DOS local_22
 *
 * `bred` is applied to BOTH the horses stock (+bred) and the food stock
 * (-bred, one horse "costs" one food, in addition to ordinary population
 * consumption) — golden-confirmed 2026-08-26, see building_production.md's
 * dated horse-breeding entry for the full derivation and the 13-colony
 * verification table.
 *
 * `shortfall` (potential - bred) is DOS's own report-only figure (scratch
 * 0x8e6a) — "N more could have bred but for the food/warehouse limit" —
 * feeds the Production tab's shortfall pairing only, never applied to any
 * stock.
 */
typedef struct ColonyProdHorseBreed {
  int bred;       /* apply to both horses stock (+) and food stock (-) */
  int shortfall;   /* potential - bred; UI/report only */
} ColonyProdHorseBreed;

ColonyProdHorseBreed colony_prod_horse_breed(
  int horses_stock,
  int population,
  int food_gross_this_turn,
  int warehouse_cap,
  bool colony_has_stable
);

/* Primary goods output for a colonist in a workplace (for settlement badges). */
const char* colony_prod_highest_manufacturing_tier_name(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* base_name
);

int colony_prod_worker_building_output_sol(
  const ColonizeColonyPool* pool,
  int building_type,
  int profession,
  int sol_bonus
);
/* Full-context worker output: folds in the colony's upgrade multipliers
 * (Lumber Mill x2, Cathedral x2) and Penn (x1.5 crosses) — what the turn
 * tick actually pays (bugs.md: badge read 7 where DOS shows 14). */
int colony_prod_worker_building_output_ctx(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  int building_type,
  int profession,
  int sol_bonus
);
int colony_prod_worker_building_output(
  const ColonizeColonyPool* pool,
  int building_type,
  int profession
);

/*
 * Settlement-view building strip: Town Hall / Church / Cathedral free output
 * plus assigned workers. Colony-wide base +1 cross is people-meter only.
 */
/* bugs.md: the settlement badge shows POTENTIAL (unclamped) output and must
 * include the colony's SoL bonus — the plain variant passes bonus 0. */
int colony_prod_building_display_output_sol(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const ColonizeCol1Save* col1,
  int building_type,
  int sol_bonus
);
int colony_prod_building_display_output(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int building_type
);

#endif
