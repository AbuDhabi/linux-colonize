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
  if (profession < 0) {
    return false;
  }
  if (profession == craft_profession) {
    return true;
  }
  /* Map unit-type indices (19..27) to skill indices (9..17):
   * 19=Carpenter(13), 20=Distiller(9), 21=Tobacconist(10), 22=Weaver(11),
   * 23=FurTrader(12), 24=Blacksmith(14), 25=Gunsmith(15), 26=Preacher(16), 27=Statesman(17) */
  switch (craft_profession) {
  case 9:  return profession == 20;
  case 10: return profession == 21;
  case 11: return profession == 22;
  case 12: return profession == 23;
  case 13: return profession == 19;
  case 14: return profession == 24;
  case 15: return profession == 25;
  case 16: return profession == 26;
  case 17: return profession == 27;
  default: return false;
  }
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
  bool colony_has_upgrade,
  bool nation_has_penn
) {
  const bool skilled = colony_prod_craft_skill_matches(profession, craft_profession);
  int v = (skilled ? 6 : colony_prod_scale_by_class(profession, 3)) + sol_bonus;
  if (colony_has_upgrade) {
    v *= 2;
  }
  /*
   * William Penn ("+50% cross production" — Preacher only; Carpenter always
   * passes nation_has_penn=false). DOS's Preacher body (15eb:1e82-1eca)
   * falls through into this check *unconditionally* after the Cathedral
   * branch — it's not an "else": a colony with both Cathedral and Penn
   * stacks ×2 then ×1.5 = ×3 per worker. Confirmed via the exact same
   * FF-index table already matched for Jefferson(15)/Paine(17): the far
   * call here passes flag 0x15 = 21 = FF_WILLIAM_PENN, and 0x1981:0x0000
   * turned out to be FUN_15eb_3960's own overlay-split tail (table base
   * printed as +0x880f, which is -0x77f1 in 16-bit two's complement — the
   * *same* table Jefferson/Paine use), not a separate mystery function.
   * The port previously applied Penn as a flat ×1.5 on the whole colony
   * crosses total (including the base/Church passive, which DOS's
   * passive-crosses composer — FUN_15eb_1f72 — never touches at all) —
   * wrong both in *where* it multiplies and in *what* it multiplies. See
   * manufacturing_worker_calc_1d4c.md.
   */
  if (nation_has_penn) {
    v += v >> 1;
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
  /* In manufacturing, SoL bonus is +2 at 100% SoL, 0 below 100% (or clamped for sol_50).
   * However, negative Tory penalties apply in full. */
  int effective_sol = 0;
  if (sol_bonus >= 2) {
    effective_sol = 2;
  } else if (sol_bonus < 0) {
    effective_sol = sol_bonus;
  }
  int out = tag + effective_sol;
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
  int craft_profession,
  int sol_bonus
) {
  /*
   * Player-confirmed 2026-08-15 (Viceroy): Textile Mill (factory tier), free
   * colonist, +2 sentiment bonus — output 12 cloth/turn, colony-wide cotton
   * accounting showed exactly 8 consumed that turn. `colony_prod_tier_
   * input_for_output(FACTORY, 12) = (12*6+8)/9 = 8` — exact match. The
   * un-modified base output (9, sol_bonus=0) would give `(9*6+8)/9 = 6`,
   * not 8 — wrong. So the 6-for-9 factory discount is real (this also
   * settles the long-open "does DOS really discount factory input, or
   * consume 1:1" question — it discounts), but it applies to the *actual*
   * SoL-adjusted output, not the flat base rate this function used to force
   * via `sol_bonus=0`. Fixed: takes sol_bonus and threads it through to
   * colony_prod_manufacturing_output the same way the output side already
   * does. See docs/building_production.md factory-input fix-log row.
   */
  const int out =
    colony_prod_manufacturing_output(building_name, profession, craft_profession, sol_bonus);
  if (out <= 0) {
    return 0;
  }
  const ColonyProdTier tier = colony_prod_building_tier(building_name);
  return colony_prod_tier_input_for_output(tier, out);
}

bool colony_prod_field_skill_matches(int profession, int field_job) {
  return profession >= 0 && profession == field_job;
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
      thresh = 10 - diff * 2;
    }
  }
  if (thresh < 2) {
    thresh = 2;
  }
  int mod = -(tories / thresh);
  if (mod < -2) {
    mod = -2;
  }

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

int colony_prod_sol_bonus_field(const ColonizeCol1Save* col1, const ColonizeColony* colony) {
  /*
   * FUN_15eb_18ec (~11869-11878, field yields) zeroes the whole SoL/Tory
   * term outright for AI-controlled colonies, gated by the same
   * nation-status table `FUN_15eb_1d4c` (manufacturing/bells/crosses/
   * hammers) only uses to pick a threshold (10 vs 10-difficulty), never to
   * zero the term. That's a real difference between field and building
   * production, not a duplicate of colony_prod_sol_bonus — hence a
   * separate function, used only by field-yield call sites.
   *
   * Confidence: this exact "nation<4 && per-nation table byte==0" gate was
   * independently observed with the same shape in three separate DOS
   * functions this session (1d4c's threshold pick, 18ec's zero-out here,
   * and 1f72's flag-0x12 bells term) — strong, cross-validated, but the
   * table byte's meaning ("is this nation human-controlled") is still a
   * hypothesis, not proven from a source that states it directly. See
   * manufacturing_worker_calc_1d4c.md.
   */
  if (!colony) {
    return 0;
  }
  if (col1 && colony->nation_id >= 0 &&
      colony->nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
      col1->player[colony->nation_id].control != 0) {
    return 0;
  }
  return colony_prod_sol_bonus(col1, colony);
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
  const bool nation_is_ai = nation_id >= 0 && nation_id < (int)COLONIZE_COL1_NATION_COUNT &&
                             col1->player[nation_id].control != 0;
  /* sol_bonus=0: the rebel-accumulator tick must not feed SoL back into itself. */
  int bells =
    colony_prod_colony_bells_ff(pool, colony, statesmen_pct, paine_tax_pct, nation_is_ai, 0);

  /* WoI + crown-occupied: bells feed Tory (negative half). */
  const int woi = col1->head.unknown46[0] != 0;
  if (woi && nation_id == colony_prod_crown_nation(col1)) {
    bells = -(bells >> 1);
  }

  cc->rebel_dividend -= (cc->rebel_dividend >> 6);
  cc->rebel_divisor -= (cc->rebel_divisor >> 6);
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
  bool colony_has_cathedral,
  bool nation_has_penn
) {
  if (!building_name ||
      (!colony_prod_name_has(building_name, "Church") &&
       !colony_prod_name_has(building_name, "Cathedral"))) {
    return 0;
  }
  return colony_prod_carpenter_preacher_shape(
    profession, COLONIZE_PROF_PREACHER, sol_bonus, colony_has_cathedral, nation_has_penn
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
  /* Carpenter has no Penn-shaped second multiplier — confirmed by direct
   * asm read: its body ends with an explicit jump after each branch, no
   * fall-through into a further check (unlike Preacher's). */
  return colony_prod_carpenter_preacher_shape(
    profession, COLONIZE_PROF_CARPENTER, sol_bonus, colony_has_lumber_mill, false
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
  bool nation_has_penn,
  int sol_bonus
) {
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  /*
   * William Penn used to be a flat ×1.5 on the whole colony total, applied
   * here after summing base/passive/workers. Confirmed wrong on both counts
   * by direct asm read of the Preacher body (FUN_15eb_1d4c): Penn folds in
   * per-Preacher-worker, stacking with that worker's own Cathedral ×2 (so
   * Cathedral+Penn together is ×3 for that worker, not ×1.5 of a total that
   * already had Cathedral's ×2 baked in at the colony level) — and DOS's
   * passive-crosses composer (FUN_15eb_1f72, base +1 / Church +1 / Cathedral
   * +1) never touches Penn at all, so the base/passive portion doesn't get
   * the bonus either. See manufacturing_worker_calc_1d4c.md.
   */
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
    crosses +=
      colony_prod_crosses_worker(bn, c->profession, sol_bonus, colony_has_cathedral, nation_has_penn);
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
  return crosses;
}

int colony_prod_colony_crosses(const ColonizeColonyPool* pool, const ColonizeColony* colony) {
  return colony_prod_colony_crosses_ff(pool, colony, false, 0);
}

int colony_prod_colony_bells_ff(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  int statesmen_bonus_pct,
  int all_bells_bonus_pct,
  bool nation_is_ai,
  int sol_bonus
) {
  if (!pool || !colony || !colony->active) {
    return 0;
  }
  int bells = 0;
  const bool has_town_hall = colony_prod_building_built(pool, colony, "Town Hall");
  if (has_town_hall) {
    bells += 1;
    /* AI bells subsidy, player-confirmed 2026-08-15 (Viceroy difficulty): a
     * free-colonist Statesman produces 5 colony bells for an AI nation vs 3
     * for a human, same colony shape, no visible FF/press bonus — exactly
     * the delta this term predicts for a mid-size colony. FUN_15eb_1f72
     * (nation crosses/bells composer) has `bells += (pop+3)/5` gated on
     * flag 0x12 (numerically = 18 = FF_SIMON_BOLIVAR) AND the same
     * AI/non-human table gate used by colony_prod_sol_bonus_field — the
     * index match with Bolivar was flagged as probably coincidental (his
     * real effect is SoL +20%, not bells) since the arithmetic here doesn't
     * fit a Founding Father at all; this is almost certainly reusing the
     * shared per-nation flag-test primitive for an unrelated AI-difficulty
     * bit, not actually reading Bolivar ownership. The player observation
     * confirms *some* AI-only bells advantage exists (ruling out "dead
     * code"/decompiler noise), and the arithmetic itself was already
     * asm-certain (only whether it was real and worth porting was in
     * doubt) — so ported as read, gated on nation_is_ai (caller-computed:
     * `col1->player[nation_id].control != 0`, same primitive as
     * colony_prod_sol_bonus_field). Not re-derived from the single
     * observation (that would be numerically underdetermined from one data
     * point); taken directly from the decompiled bytes. See
     * nation_crosses_bells_1f72.md item 4. */
    if (nation_is_ai) {
      int pop = colony->colonist_count > 0 ? colony->colonist_count : colony->population;
      if (pop < 0) {
        pop = 0;
      }
      bells += (pop + 3) / 5;
    }
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
  return colony_prod_colony_bells_ff(pool, colony, 0, 0, false, 0);
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

const char* colony_prod_highest_manufacturing_tier_name(
  const ColonizeColonyPool* pool,
  const ColonizeColony* colony,
  const char* base_name
) {
  if (!base_name) {
    return NULL;
  }
  if (colony_prod_name_has(base_name, "Weaver") || colony_prod_name_has(base_name, "Textile")) {
    if (colony_prod_building_built(pool, colony, "Textile Mill")) return "Textile Mill";
    if (colony_prod_building_built(pool, colony, "Weaver's Shop")) return "Weaver's Shop";
    return "Weaver's House";
  }
  if (colony_prod_name_has(base_name, "Rum Distill")) {
    if (colony_prod_building_built(pool, colony, "Rum Factory")) return "Rum Factory";
    if (colony_prod_building_built(pool, colony, "Rum Distillery")) return "Rum Distillery";
    return "Rum Distiller's House";
  }
  if (colony_prod_name_has(base_name, "Tobacconist") || colony_prod_name_has(base_name, "Cigar")) {
    if (colony_prod_building_built(pool, colony, "Cigar Factory")) return "Cigar Factory";
    if (colony_prod_building_built(pool, colony, "Tobacconist's Shop")) return "Tobacconist's Shop";
    return "Tobacconist's House";
  }
  if (colony_prod_name_has(base_name, "Fur Trad") || colony_prod_name_has(base_name, "Fur Fact")) {
    if (colony_prod_building_built(pool, colony, "Fur Factory")) return "Fur Factory";
    if (colony_prod_building_built(pool, colony, "Fur Trading Post")) return "Fur Trading Post";
    return "Fur Trader's House";
  }
  if (colony_prod_name_has(base_name, "Blacksmith") || colony_prod_name_has(base_name, "Iron Works")) {
    if (colony_prod_building_built(pool, colony, "Iron Works")) return "Iron Works";
    if (colony_prod_building_built(pool, colony, "Blacksmith's Shop")) return "Blacksmith's Shop";
    return "Blacksmith's House";
  }
  if (colony_prod_name_has(base_name, "Armory") || colony_prod_name_has(base_name, "Magazine") || colony_prod_name_has(base_name, "Arsenal")) {
    if (colony_prod_building_built(pool, colony, "Arsenal")) return "Arsenal";
    if (colony_prod_building_built(pool, colony, "Magazine")) return "Magazine";
    return "Armory";
  }
  return base_name;
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
    return colony_prod_crosses_worker(name, profession, 0, false, false);
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
