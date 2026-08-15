#include "core/colony_production.h"

#include <string.h>

#include "core/col1_save.h"
#include "core/colony_yield.h"
#include "core/founding_fathers.h"

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

/*
 * Shared shape for Carpenter/Preacher (FUN_15eb_1d4c bodies at 15eb:1e50 /
 * 15eb:1e82 — see manufacturing_worker_calc_1d4c.md): skill match picks a
 * flat top-rate baseline instead of the class tag (not a ×2 of the class
 * scale like Statesman/the shared craft body), sol_bonus adds next, and a
 * *colony-wide* "owns the upgraded building" flag (Lumber Mill / Cathedral —
 * not this worker's own assigned building) doubles the result last. Clamped
 * to >= 0, matching FUN_15eb_1d4c's shared epilogue.
 */
static int colony_prod_carpenter_preacher_shape(
  int profession,
  int craft_profession,
  int sol_bonus,
  bool colony_has_upgrade
) {
  const bool skilled = colony_prod_craft_skill_matches(profession, craft_profession);
  int v = (skilled ? 6 : colony_prod_scale_by_class(profession, 3)) + sol_bonus;
  if (colony_has_upgrade) {
    v *= 2;
  }
  return v > 0 ? v : 0;
}

int colony_prod_manufacturing_output(
  const char* building_name,
  int profession,
  int craft_profession,
  int sol_bonus
) {
  if (!building_name) {
    return 0;
  }
  const ColonyProdTier tier = colony_prod_building_tier(building_name);
  /* DOS FUN_15eb_1d4c: class tag (1/2/3, i.e. colony_prod_scale_by_class at a
   * fixed house-tier "3") plus sol_bonus first; shop re-adds the tag alone;
   * factory applies ×1.5 (floor, matching x86 SAR) to the running total;
   * skill match doubles whatever's left. See
   * original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md. */
  const int tag = colony_prod_scale_by_class(profession, 3);
  int out = tag + sol_bonus;
  if (tier == COLONY_PROD_TIER_SHOP || tier == COLONY_PROD_TIER_FACTORY) {
    out += tag;
  }
  if (tier == COLONY_PROD_TIER_FACTORY) {
    out += out >> 1;
  }
  if (colony_prod_craft_skill_matches(profession, craft_profession)) {
    out *= 2;
  }
  return out > 0 ? out : 0;
}

int colony_prod_manufacturing_input(
  const char* building_name,
  int profession,
  int craft_profession
) {
  /* sol_bonus 0: input consumption tracks the un-modified base rate, not the
   * SoL-adjusted output — see header comment. */
  const int out = colony_prod_manufacturing_output(building_name, profession, craft_profession, 0);
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
  int profession,
  bool has_docks
) {
  int yld = colony_yield_for_tile(map, x, y, field_job);
  /* Fisherman needs Docks (or an upgrade: Drydock/Shipyard) to work ocean/sea
   * surrounds at all — FUN_15eb_18ec ~11925-11939 zeroes the whole yield,
   * not just a modifier. */
  if (field_job == COLONIZE_JOB_FISHERMAN && !has_docks) {
    return 0;
  }
  if (yld <= 0) {
    return 0;
  }
  /* Convert +1 only on the DOS whitelist (FUN_15eb_18ec ~11974-11979): food/
   * cash crops + fur trapper + fisherman — not lumber/ore/silver. */
  if (profession == COLONIZE_PROF_CONVERT && field_job != COLONIZE_JOB_LUMBERJACK &&
      field_job != COLONIZE_JOB_ORE_MINER && field_job != COLONIZE_JOB_SILVER_MINER) {
    yld += 1;
  }
  if (colony_prod_field_skill_matches(profession, field_job)) {
    /* Food/fish expert gets flat +2, not ×2 like every other field job
     * (FUN_15eb_18ec ~11890-11899: `if (food/fish) yld+=2; else yld<<=1;`).
     * The "re-add the positive SoL mod a second time" refinement mentioned
     * alongside this in DOS is not applied here — needs sol_bonus threaded
     * into this function first (deferred, see terrain_yields.md). */
    if (field_job == COLONIZE_JOB_FARMER || field_job == COLONIZE_JOB_FISHERMAN) {
      yld += 2;
    } else {
      yld *= 2;
    }
  }
  return yld;
}

int colony_prod_sol_percent(const ColonizeCol1Save* col1, const ColonizeColony* colony) {
  if (!colony) {
    return 0;
  }
  int sol = 0;
  bool have = false;
  if (col1 && col1->colony) {
    for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
      const ColonizeCol1Colony* c = &col1->colony[i];
      if ((int)c->x != colony->x || (int)c->y != colony->y) {
        continue;
      }
      if (c->rebel_divisor == 0) {
        break; /* fall through to nation bells */
      }
      sol = (int)((c->rebel_dividend * 100u) / c->rebel_divisor);
      have = true;
      break;
    }
  }
  /* FUN_43f7_0004-shaped: liberty_bells_total/4 when rebel fields unavailable. */
  if (!have && col1 && colony->nation_id >= 0 && colony->nation_id < 4) {
    sol = (int)col1->nation[colony->nation_id].liberty_bells_total / 4;
    have = true;
  }
  if (!have) {
    return 0;
  }
  if (sol < 0) {
    sol = 0;
  }
  /* FUN_15eb_0274: Bolivar +20 for human nation (display-time, not storage). */
  sol += founding_fathers_bolivar_sol_bonus(col1, colony->nation_id);
  if (sol > 100) {
    sol = 100;
  }
  return sol;
}

int colony_prod_sol_bonus(const ColonizeCol1Save* col1, const ColonizeColony* colony) {
  if (!colony) {
    return 0;
  }
  const int sol = colony_prod_sol_percent(col1, colony);
  int pop = colony->population > 0 ? colony->population : colony->colonist_count;
  if (pop < 0) {
    pop = 0;
  }
  /* Round half-up Tory share (decomp ~11880). */
  const int tories = (pop * (100 - sol) + 50) / 100;

  int thresh = 10;
  if (col1 && colony->nation_id >= 0 &&
      colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT) {
    /* control 0 = human; AI / withdrawn use fixed thresh 10. */
    if (col1->player[colony->nation_id].control == 0) {
      int diff = (int)col1->head.difficulty;
      if (diff < 0) {
        diff = 0;
      }
      if (diff > 4) {
        diff = 4;
      }
      thresh = 10 - diff;
    }
  }
  if (thresh < 1) {
    thresh = 1;
  }

  int mod = -(tories / thresh);

  /* Latch bits (hysteresis) or live SoL stand-in; take the larger so a
   * stale sol_50-only flag cannot under-count after SoL rises to 100, while
   * sol_100 hysteresis (95..99) still beats live. */
  int from_latch = 0;
  if ((colony->colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0) {
    from_latch += 1;
  }
  if ((colony->colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0) {
    from_latch += 1;
  }
  int from_live = 0;
  if (sol >= 100) {
    from_live = 2;
  } else if (sol >= 50) {
    from_live = 1;
  }
  mod += (from_latch > from_live) ? from_latch : from_live;
  return mod;
}

/*
 * FUN_364b_0688 Phase D: one-step latch +0x1c sol_50 (0x04) / sol_100 (0x02).
 * Crossing 50 then 100 takes two ticks (majority then unanimous). Clears
 * sol_100 below ~95 and sol_50 below 50 (hysteresis). Cite: decomp ~57415.
 */
void colony_prod_refresh_sol_flags(ColonizeColony* colony, const ColonizeCol1Save* col1) {
  if (!colony || !colony->active) {
    return;
  }
  const int sol = colony_prod_sol_percent(col1, colony);
  const uint8_t f = colony->colony_flags;
  if (sol >= 50 && (f & COLONIZE_COLONY_FLAG_SOL_50) == 0) {
    colony->colony_flags |= COLONIZE_COLONY_FLAG_SOL_50;
  } else if (sol >= 100 && (f & COLONIZE_COLONY_FLAG_SOL_100) == 0) {
    colony->colony_flags |=
      (uint8_t)(COLONIZE_COLONY_FLAG_SOL_100 | COLONIZE_COLONY_FLAG_SOL_50);
  } else if (sol < 95 && (f & COLONIZE_COLONY_FLAG_SOL_100) != 0) {
    colony->colony_flags =
      (uint8_t)(colony->colony_flags & (uint8_t)~COLONIZE_COLONY_FLAG_SOL_100);
  } else if (sol < 50 && (f & COLONIZE_COLONY_FLAG_SOL_50) != 0) {
    colony->colony_flags = (uint8_t)(colony->colony_flags &
                                     (uint8_t)~(COLONIZE_COLONY_FLAG_SOL_50 |
                                                COLONIZE_COLONY_FLAG_SOL_100));
  }
}

/*
 * Crown / REF peer of human Euro slot (0↔1). Match ai_king / combat_strength.
 */
static int colony_prod_crown_nation(const ColonizeCol1Save* col1) {
  if (!col1) {
    return 1;
  }
  for (int i = 0; i < (int)COLONIZE_COL1_NATION_COUNT; ++i) {
    if (col1->player[i].control == 0) {
      return (i == 0) ? 1 : 0;
    }
  }
  return 1;
}

void colony_prod_tick_rebel_accumulators(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  ColonizeCol1Save* col1
) {
  if (!pool || !colony || !colony->active || !col1 || !col1->colony) {
    return;
  }
  ColonizeCol1Colony* cc = NULL;
  for (uint16_t i = 0; i < col1->head.colony_count; ++i) {
    ColonizeCol1Colony* c = &col1->colony[i];
    if ((int)c->x == colony->x && (int)c->y == colony->y) {
      cc = c;
      break;
    }
  }
  if (!cc) {
    return;
  }

  int pop = colony->population > 0 ? colony->population : colony->colonist_count;
  if (pop < 0) {
    pop = 0;
  }

  const int nation_id = colony->nation_id;
  const int statesmen_pct =
    (nation_id >= 0 && founding_fathers_nation_has(col1, nation_id, FF_THOMAS_JEFFERSON))
      ? 50
      : 0;
  const int paine_tax_pct =
    (nation_id >= 0 && nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
     founding_fathers_nation_has(col1, nation_id, FF_THOMAS_PAINE))
      ? (int)col1->nation[nation_id].tax_rate
      : 0;
  /* sol_bonus=0: the rebel-accumulator tick must not feed SoL back into itself. */
  int bells = colony_prod_colony_bells_ff(pool, colony, statesmen_pct, paine_tax_pct, 0);

  /* WoI + crown-occupied: bells feed Tory (negative half). */
  const int woi = col1->head.unknown46[0] != 0;
  if (woi && nation_id == colony_prod_crown_nation(col1)) {
    bells = -(bells >> 1);
  }

  cc->rebel_dividend >>= 6;
  cc->rebel_divisor >>= 6;
  cc->rebel_divisor += (uint32_t)(pop * 2);

  if (bells >= 0) {
    if (cc->rebel_dividend < 0xffffffffu - (uint32_t)bells) {
      cc->rebel_dividend += (uint32_t)bells;
    } else {
      cc->rebel_dividend = 0xffffffffu;
    }
  } else {
    const uint32_t sub = (uint32_t)(-bells);
    if (cc->rebel_dividend > sub) {
      cc->rebel_dividend -= sub;
    } else {
      cc->rebel_dividend = 0;
    }
  }

  if (cc->rebel_dividend > cc->rebel_divisor) {
    cc->rebel_dividend = cc->rebel_divisor;
  }
}

int colony_prod_crosses_worker(
  const char* building_name,
  int profession,
  int sol_bonus,
  bool colony_has_cathedral
) {
  if (!building_name ||
      (!colony_prod_name_has(building_name, "Church") &&
       !colony_prod_name_has(building_name, "Cathedral"))) {
    return 0;
  }
  return colony_prod_carpenter_preacher_shape(
    profession, COLONIZE_PROF_PREACHER, sol_bonus, colony_has_cathedral
  );
}

int colony_prod_bells_worker(const char* building_name, int profession, int sol_bonus) {
  if (!building_name || !colony_prod_name_has(building_name, "Town Hall")) {
    return 0;
  }
  /* DOS FUN_15eb_1d4c Statesman body: v = class_tag + local_e (sol_bonus),
   * *then* doubled on skill match — sol_bonus must be inside the doubling,
   * not added after (manufacturing_worker_calc_1d4c.md). Clamped to >= 0,
   * matching FUN_15eb_1d4c's shared epilogue. */
  int base = colony_prod_scale_by_class(profession, 3) + sol_bonus;
  if (colony_prod_craft_skill_matches(profession, COLONIZE_PROF_STATESMAN)) {
    base *= 2;
  }
  return base > 0 ? base : 0;
}

int colony_prod_hammers_worker(
  const char* building_name,
  int profession,
  int sol_bonus,
  bool colony_has_lumber_mill
) {
  if (!building_name ||
      (!colony_prod_name_has(building_name, "Carpenter") &&
       !colony_prod_name_has(building_name, "Lumber Mill"))) {
    return 0;
  }
  return colony_prod_carpenter_preacher_shape(
    profession, COLONIZE_PROF_CARPENTER, sol_bonus, colony_has_lumber_mill
  );
}

int colony_prod_church_passive_crosses(const char* building_name) {
  /*
   * DOS FUN_15eb_1f72 (nation bells/crosses composer, viceroy_unpacked_2.c
   * ~11306-11314): colony crosses = 1 (unconditional) + 1 if Church built +
   * 1 if Cathedral built — Church and Cathedral are worth the *same* passive
   * (+1), not the manual/wiki-sourced +2/+3 this used to return. Confirmed
   * by the same read that pinned down the Printing Press/Newspaper bell
   * multipliers and the Jefferson/Paine FF indices (15/17) exactly matching
   * founding_fathers.h — see manufacturing_worker_calc_1d4c.md.
   */
  if (!building_name) {
    return 0;
  }
  if (colony_prod_name_has(building_name, "Cathedral")) {
    return 1;
  }
  if (colony_prod_name_has(building_name, "Church")) {
    return 1;
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

int colony_prod_colony_crosses_ff(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int crosses_bonus_pct,
  int sol_bonus
) {
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
  const bool colony_has_cathedral = colony_prod_building_built(pool, colony, "Cathedral");
  int cross_workers = 0;
  for (int p = 0; p < colony->colonist_count; ++p) {
    const ColonizeColonist* c = &colony->colonists[p];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    const char* bn = pool->building_types[c->building_type].name;
    if (!colony_prod_name_has(bn, "Church") && !colony_prod_name_has(bn, "Cathedral")) {
      continue;
    }
    cross_workers++;
    crosses += colony_prod_crosses_worker(bn, c->profession, sol_bonus, colony_has_cathedral);
  }
  /* No cross workers to fold sol_bonus into individually — apply it to the
   * base/passive crosses directly instead (nothing else it could attach to;
   * matches the pre-2026-08-15 external "church passive / colony base"
   * fallback this replaces). */
  if (cross_workers == 0 && sol_bonus != 0 && crosses > 0) {
    crosses += sol_bonus;
    if (crosses < 0) {
      crosses = 0;
    }
  }
  /* William Penn: cross production in all colonies +50% (fandom_col1994). */
  if (crosses_bonus_pct > 0) {
    crosses = crosses * (100 + crosses_bonus_pct) / 100;
  }
  return crosses;
}

int colony_prod_colony_crosses(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  return colony_prod_colony_crosses_ff(pool, colony, 0, 0);
}

int colony_prod_colony_bells_ff(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int statesmen_bonus_pct,
  int all_bells_bonus_pct,
  int sol_bonus
) {
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  int bells = 0;
  const bool has_town_hall = colony_prod_building_built(pool, colony, "Town Hall");
  if (has_town_hall) {
    bells += 1;
  }
  int bell_workers = 0;
  for (int p = 0; p < colony->colonist_count; ++p) {
    const ColonizeColonist* c = &colony->colonists[p];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    const char* bn = pool->building_types[c->building_type].name;
    if (!colony_prod_name_has(bn, "Town Hall")) {
      continue;
    }
    bell_workers++;
    int w = colony_prod_bells_worker(bn, c->profession, sol_bonus);
    /* Thomas Jefferson: liberty bell production of statesmen +50% (wiki). */
    if (w > 0 && statesmen_bonus_pct > 0) {
      w = w * (100 + statesmen_bonus_pct) / 100;
    }
    bells += w;
  }
  /* No bell workers to fold sol_bonus into individually — apply it to the
   * Town Hall passive directly instead (nothing else it could attach to). */
  if (bell_workers == 0 && has_town_hall && sol_bonus != 0) {
    bells += sol_bonus;
    if (bells < 0) {
      bells = 0;
    }
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
  /* Thomas Paine: bells increased by current tax rate % (multiplicative w/ media). */
  if (all_bells_bonus_pct > 0) {
    bells = bells * (100 + all_bells_bonus_pct) / 100;
  }
  return bells;
}

int colony_prod_colony_bells(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  return colony_prod_colony_bells_ff(pool, colony, 0, 0, 0);
}

int colony_prod_colony_hammers(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int sol_bonus,
  int* out_lumber_use
) {
  if (out_lumber_use) {
    *out_lumber_use = 0;
  }
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  const bool colony_has_lumber_mill = colony_prod_building_built(pool, colony, "Lumber Mill");
  int lumber_total = 0; /* sol_bonus=0: lumber consumption tracks the un-modified base rate. */
  int hammers_total = 0;
  for (int p = 0; p < colony->colonist_count; ++p) {
    const ColonizeColonist* c = &colony->colonists[p];
    if (!c->active || c->building_type < 0 || c->building_type >= pool->building_type_count) {
      continue;
    }
    const char* bname = pool->building_types[c->building_type].name;
    lumber_total += colony_prod_hammers_worker(bname, c->profession, 0, colony_has_lumber_mill);
    hammers_total +=
      colony_prod_hammers_worker(bname, c->profession, sol_bonus, colony_has_lumber_mill);
  }
  if (out_lumber_use && lumber_total > 0) {
    *out_lumber_use = lumber_total;
  }
  return hammers_total;
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
    return colony_prod_bells_worker(name, profession, 0);
  }
  if (colony_prod_name_has(name, "Church") || colony_prod_name_has(name, "Cathedral")) {
    return colony_prod_crosses_worker(name, profession, 0, false);
  }
  if (colony_prod_name_has(name, "Carpenter") || colony_prod_name_has(name, "Lumber Mill")) {
    return colony_prod_hammers_worker(name, profession, 0, false);
  }
  if (colony_prod_name_has(name, "Rum Distill")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_DISTILLER, 0);
  }
  if (colony_prod_name_has(name, "Tobacconist")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_TOBACCONIST, 0);
  }
  if (colony_prod_name_has(name, "Weaver") || colony_prod_name_has(name, "Textile")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_WEAVER, 0);
  }
  if (colony_prod_name_has(name, "Fur Trad") || colony_prod_name_has(name, "Fur Fact")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_FUR_TRADER, 0);
  }
  if (colony_prod_name_has(name, "Blacksmith") || colony_prod_name_has(name, "Iron Works")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_BLACKSMITH, 0);
  }
  if (colony_prod_name_has(name, "Armory") || colony_prod_name_has(name, "Magazine") ||
      colony_prod_name_has(name, "Arsenal")) {
    return colony_prod_manufacturing_output(name, profession, COLONIZE_PROF_GUNSMITH, 0);
  }
  return 0;
}

int colony_prod_building_display_output(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int building_type
) {
  if (!pool || !colony || building_type < 0 || building_type >= pool->building_type_count) {
    return 0;
  }
  if (!colony->has_building[building_type]) {
    return 0;
  }
  const char* name = pool->building_types[building_type].name;
  if (!name) {
    return 0;
  }
  int amount = 0;
  if (colony_prod_name_has(name, "Town Hall")) {
    amount += 1; /* building passive liberty bell */
  } else if (
    colony_prod_name_has(name, "Church") || colony_prod_name_has(name, "Cathedral")
  ) {
    amount += colony_prod_church_passive_crosses(name);
  }
  for (int p = 0; p < colony->colonist_count; ++p) {
    const ColonizeColonist* c = &colony->colonists[p];
    if (!c->active || c->building_type != building_type) {
      continue;
    }
    amount += colony_prod_worker_building_output(pool, building_type, c->profession);
  }
  return amount;
}
