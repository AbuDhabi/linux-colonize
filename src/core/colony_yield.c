#include "core/colony_yield.h"

#include <stddef.h>
#include <string.h>

/* COLONIZE_PROF_CONVERT only — a #define, not a call, so this doesn't pull
 * colony_production.c into this file's link set (unit_colony_yield links
 * colony_yield.c standalone). */
#include "core/colony_production.h"

/*
 * Yield columns match NAMES.TXT @JOB Farmer…Fisherman.
 * Rows match pedia terrain indices (0–7 unforesed, 8–15 forest type,
 * 16–23 forest via &7, 24–28 arctic/ocean/sea/mountains/hills).
 *
 * Town commons (center) dual-produce is calibrated to live Col1 observations;
 * see docs/terrain_yields.md.
 */
static const int k_unforesed[8][COLONIZE_FIELD_JOB_COUNT] = {
  /* Tundra */ {2, 0, 0, 0, 0, 0, 2, 0, 0},
  /* Desert */ {1, 0, 0, 1, 0, 0, 2, 0, 0},
  /* Plains */ {4, 0, 0, 2, 0, 0, 1, 0, 0},
  /* Prairie */ {3, 0, 0, 3, 0, 0, 0, 0, 0},
  /* Grassland */ {2, 0, 3, 0, 0, 0, 0, 0, 0},
  /* Savannah */ {3, 3, 0, 0, 0, 0, 0, 0, 0},
  /* Marsh */ {2, 0, 2, 0, 0, 0, 2, 0, 0},
  /* Swamp */ {2, 2, 0, 0, 0, 0, 2, 0, 0},
};

static const int k_forested[8][COLONIZE_FIELD_JOB_COUNT] = {
  /* Boreal */ {1, 0, 0, 0, 3, 2, 1, 0, 0},
  /* Scrub */ {1, 0, 0, 1, 2, 1, 1, 0, 0},
  /* Mixed */ {2, 0, 0, 1, 3, 3, 0, 0, 0},
  /* Broadleaf */ {1, 0, 0, 1, 2, 2, 0, 0, 0},
  /* Conifer */ {1, 0, 1, 0, 2, 3, 0, 0, 0},
  /* Tropical */ {2, 1, 0, 0, 2, 2, 0, 0, 0},
  /* Wetland */ {1, 0, 1, 0, 2, 2, 1, 0, 0},
  /* Rain */ {1, 1, 0, 0, 1, 2, 1, 0, 0},
};

/* Arctic, Ocean, Sea Lane, Mountains, Hills.
 * Hills Farmer is 2 — player-confirmed 2026-08-15 (Viceroy difficulty), not
 * just Terrain Chart/FreeCol; NAMES.TXT lists 1 but real gameplay doesn't
 * match it. See docs/terrain_yields.md. */
static const int k_other[5][COLONIZE_FIELD_JOB_COUNT] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 3},
  {0, 0, 0, 0, 0, 0, 0, 0, 3},
  {0, 0, 0, 0, 0, 0, 4, 1, 0},
  {2, 0, 0, 0, 0, 0, 4, 0, 0},
};

static const char* k_job_names[COLONIZE_FIELD_JOB_COUNT] = {
  "Farmer",
  "Sugar Planter",
  "Tobacco Planter",
  "Cotton Planter",
  "Fur Trapper",
  "Lumberjack",
  "Ore Miner",
  "Silver Miner",
  "Fisherman",
};

int colony_yield_job_cargo(int field_job) {
  switch (field_job) {
  case COLONIZE_JOB_FARMER:
  case COLONIZE_JOB_FISHERMAN:
    return COLONIZE_CARGO_FOOD;
  case COLONIZE_JOB_SUGAR_PLANTER:
    return COLONIZE_CARGO_SUGAR;
  case COLONIZE_JOB_TOBACCO_PLANTER:
    return COLONIZE_CARGO_TOBACCO;
  case COLONIZE_JOB_COTTON_PLANTER:
    return COLONIZE_CARGO_COTTON;
  case COLONIZE_JOB_FUR_TRAPPER:
    return COLONIZE_CARGO_FURS;
  case COLONIZE_JOB_LUMBERJACK:
    return COLONIZE_CARGO_LUMBER;
  case COLONIZE_JOB_ORE_MINER:
    return COLONIZE_CARGO_ORE;
  case COLONIZE_JOB_SILVER_MINER:
    return COLONIZE_CARGO_SILVER;
  default:
    return -1;
  }
}

const char* colony_yield_job_name(int field_job) {
  if (field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return "?";
  }
  return k_job_names[field_job];
}

static int colony_yield_base_for_pedia(int pedia, int field_job) {
  if (field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return 0;
  }
  if (pedia >= 0 && pedia <= 7) {
    return k_unforesed[pedia][field_job];
  }
  if (pedia >= 8 && pedia <= 23) {
    return k_forested[pedia & 7][field_job];
  }
  if (pedia >= 24 && pedia <= 28) {
    return k_other[pedia - 24][field_job];
  }
  return 0;
}

/*
 * (resource, field_job) -> effect, byte-exact from FUN_15eb_17fa (a flat
 * if-chain over hardcoded pairs, not a resource->job map — a resource can
 * pair with more than one job, e.g. Game with both Farmer and Fur Trapper).
 * COLONY_YIELD_RESOURCE_DOUBLE is the sentinel for the "double the yield
 * so far" path; everything else is a flat additive amount (0 = no match).
 * See docs/terrain_yields.md "Effect on a matching job".
 */
#define COLONY_YIELD_RESOURCE_DOUBLE (-1)

static int colony_yield_resource_effect(int resource, int field_job) {
  int v = 0;
  if (resource == 9 && field_job == COLONIZE_JOB_FARMER) {
    v = 2;
  }
  if (resource == 1 && field_job == COLONIZE_JOB_FARMER) {
    v += 2;
  }
  if (resource == 2 && field_job == COLONIZE_JOB_FARMER) {
    v += 2;
  }
  if (resource == 9 && field_job == COLONIZE_JOB_FUR_TRAPPER) {
    v += 2;
  }
  if (resource == 8 && field_job == COLONIZE_JOB_FUR_TRAPPER) {
    v += 2;
  }
  if (resource == 3 && field_job == COLONIZE_JOB_COTTON_PLANTER) {
    v = COLONY_YIELD_RESOURCE_DOUBLE;
  }
  if (resource == 4 && field_job == COLONIZE_JOB_TOBACCO_PLANTER) {
    v = COLONY_YIELD_RESOURCE_DOUBLE;
  }
  if (resource == 5 && field_job == COLONIZE_JOB_SUGAR_PLANTER) {
    v = COLONY_YIELD_RESOURCE_DOUBLE;
  }
  if (resource == 10 && field_job == COLONIZE_JOB_LUMBERJACK) {
    v += 2;
  }
  if (resource == 6 && field_job == COLONIZE_JOB_ORE_MINER) {
    v += 3;
  }
  if (resource == 13 && field_job == COLONIZE_JOB_ORE_MINER) {
    v += 2;
  }
  if (resource == 6 && field_job == COLONIZE_JOB_SILVER_MINER) {
    v += 1;
  }
  if (resource == 12 && field_job == COLONIZE_JOB_SILVER_MINER) {
    v += 2;
  }
  if (resource == 7 && field_job == COLONIZE_JOB_FISHERMAN) {
    v += 3;
  }
  return v;
}

static bool colony_yield_is_crop_job(int field_job) {
  return field_job == COLONIZE_JOB_FARMER || field_job == COLONIZE_JOB_SUGAR_PLANTER ||
         field_job == COLONIZE_JOB_TOBACCO_PLANTER || field_job == COLONIZE_JOB_COTTON_PLANTER;
}

/*
 * Minor-river bonus for a field job; major = 2×.
 * Food/crops +1, furs/lumber +2, ore/silver +1 (FreeCol classic / Col1).
 *
 * Fisherman (job 8), player-confirmed 2026-08-15 (Viceroy difficulty): Lake
 * with a major river, free colonist, no sentiment bonus = 6 food. Base ocean
 * fish is 3, coastal distance mod +1 (colony_yield_fisherman_distance_mod)
 * = 4, so the river delta is +2 — exactly the food/crop bucket (base 1,
 * major ×2) doubled. This also explains a previously-unexplained "coastal
 * usually 4, sometimes 6" observation: the "sometimes 6" tiles are coastal
 * *and* major-river (4 + 2). DOS's static terrain river-bit check
 * (`FUN_15eb_18ec` ~11950s) applies to *any* job, not just job<4 — only the
 * separate runtime-array river signal is job<4-gated (still unresolved, see
 * terrain_yields.md, but doesn't block this: the port's own crop-job river
 * magnitudes already matched player data without needing that signal, so
 * Fisherman only needed the same "any job" static-bit path crop/ore/silver
 * already get). Previously `default: return 0` silently dropped Fisherman
 * from any river bonus at all — the bug this fixes.
 */
static int colony_yield_river_bonus(int field_job, bool major) {
  int base = 1;
  switch (field_job) {
  case COLONIZE_JOB_FARMER:
  case COLONIZE_JOB_FUR_TRAPPER:
  case COLONIZE_JOB_LUMBERJACK:
    base = 2;
    break;
  default:
    base = 1;
    break;
  }
  return major ? (base * 2) : base;
}

/*
 * Road bonus for a field job — same per-job magnitude bucket as
 * colony_yield_river_bonus's minor-river value (furs/lumber +2,
 * ore/silver +1), not a flat +1 for every road job. Player-confirmed
 * 2026-08-15 (Viceroy): Expert Fur Trapper, Mixed Forest+road+sentiment(+2),
 * Henry Hudson owned = 28 furs; Free Colonist, same tile = 14. The port
 * used to give every road job (fur/lumber/ore/silver alike) a flat +1,
 * which — even combined with Hudson's ×2 and the expert road/river
 * doubling fixed above — landed on free=12/expert=24, not 14/28. Only
 * matches exactly once fur/lumber's road magnitude is 2, same as their
 * river magnitude: free = (base 3 + sol 2 + road[u=1,base=2]) × Hudson(2)
 * = 7×2 = 14; expert = ((base 3 + sol 2)<<=1 + road[u=2,base=2]) × Hudson(2)
 * = 14×2 = 28. See docs/terrain_yields.md "Plow / road / river stacking".
 * Re-confirmed 2026-08-18 via colony_prod02's New Holland (real DOS turn):
 * an unskilled Lumberjack + road was landing hammers-consumed-lumber one
 * short (this bucket had regressed back to the old flat "ore/silver +1"
 * grouping for Lumberjack specifically, contradicting this comment and
 * colony_yield_river_bonus's own already-correct split).
 */
static int colony_yield_road_bonus(int field_job) {
  switch (field_job) {
  case COLONIZE_JOB_FUR_TRAPPER:
  case COLONIZE_JOB_LUMBERJACK:
    return 2;
  case COLONIZE_JOB_ORE_MINER:
  case COLONIZE_JOB_SILVER_MINER:
    return 1;
  default:
    return 0;
  }
}

/*
 * Fisherman-only distance/enclosure modifier (FUN_15eb_18ec ~11814-11838,
 * calling FUN_15eb_173e/FUN_15eb_16fe). Counts how many of the 8 neighbors
 * of this ocean tile are themselves Ocean(25)/Sea Lane(26) — i.e. how far
 * out to open sea this tile is — and adjusts yield: fully open water is
 * worse for fishing, a sheltered/coastal spot is better.
 *
 * The raw asm (viceroy_unpacked.asm ~15706-15735) is a 6-way cascade, but
 * three of those branches (count>=4 / >=3 / >=1) are genuinely unreachable
 * in the original DOS binary — each is only reached after already proving
 * count<6 by an earlier `JL`, then immediately re-tested against `>=6`,
 * which can never be true. Confirmed by reading the instructions directly,
 * not decompiler noise like FUN_15eb_1d4c's jump table was — this looks
 * like a leftover/typo in Sid Meier's team's original source. Ported as the
 * 3 branches that can actually execute; the dead ones are omitted since
 * porting unreachable code changes nothing observable.
 */
static int colony_yield_ocean_neighbor_count(const ColonizeWorldMap* map, int x, int y) {
  static const int k_dx[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  static const int k_dy[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  int count = 0;
  for (int i = 0; i < 8; ++i) {
    const int pedia = map_pedia_terrain_index_at(map, x + k_dx[i], y + k_dy[i]);
    if (pedia == 25 || pedia == 26) {
      ++count;
    }
  }
  return count;
}

static int colony_yield_fisherman_distance_mod(const ColonizeWorldMap* map, int x, int y) {
  const int count = colony_yield_ocean_neighbor_count(map, x, y);
  if (count >= 8) {
    return -2;
  }
  if (count >= 6) {
    return -1;
  }
  return 1;
}

/*
 * Road and river do not stack — apply the larger bonus once. `big_unit`
 * doubles whichever wins: DOS's road/river "u" unit size is 2 instead of 1
 * for a matching expert on a non-food/fish job, or for any Lumberjack
 * (matching, or not) — confirmed 2026-08-15 by player data (Viceroy):
 * Expert Ore Miner on Hills+road+sentiment(+1) = 12; Free Colonist, same
 * tile = 6 — exactly ×2 at every step, which only holds if road/river also
 * doubles for the expert, not just the flat expert doubling already wired
 * (that alone would give 10, not 12 — see colony_yield_pipeline's own
 * comment for the full derivation). See docs/terrain_yields.md.
 */
static int colony_yield_road_or_river_bonus(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  bool big_unit
) {
  const int road = map_tile_has_road(map, x, y) ? colony_yield_road_bonus(field_job) : 0;
  int river = 0;
  if (map_tile_has_river(map, x, y)) {
    river = colony_yield_river_bonus(field_job, map_tile_has_major_river(map, x, y));
  }
  /* Road and river stack (sum), not max — player-confirmed 2026-08-18 via
   * colony_prod02's Fort Orange: an expert Lumberjack on a tile with both
   * a road and a (minor) river needed both bonuses added (2+2=4) to match
   * the real colony's lumber income; New Amsterdam's road-only expert
   * Lumberjack (colony_prod01) independently confirmed the flat,
   * non-doubled magnitude this now sums (see the Lumberjack pipeline
   * comment for why the road/river addition itself isn't expert-doubled
   * a second time here). */
  const int bonus = road + river;
  return big_unit ? bonus * 2 : bonus;
}

/*
 * Full DOS field-yield pipeline (FUN_15eb_18ec), profession- and SoL-aware.
 * `profession` -1 means no worker context (colony_yield_for_tile's AI/
 * job-suggestion callers) — skips expert/convert/docks-gate entirely, same
 * as this project's pre-2026-08-15 behavior. `sol_bonus` is the signed
 * colony_prod_sol_bonus_field() value; 0 is a no-op either way.
 *
 * Step order, confirmed 2026-08-15 by player data (Viceroy difficulty):
 * Expert Ore Miner, Hills+road+sentiment(+1) = 12; Free Colonist, same
 * tile = 6. This only reproduces from *this* order — SoL folds in early
 * (step 2) and gets swept up by the expert doubling (step 3), and the
 * road/river unit doubles for the expert too (step 7):
 *   free:   base(4) +mod(1)=5,               +road(u=1)=6
 *   expert: base(4) +mod(1)=5, <<=1(step3)=10, +road(u=2)=12
 * The port's old shape (compute base+road, double whole thing for expert,
 * add SoL flat afterward, road never expert-scaled) gives free=6 too but
 * expert=(4+1)*2+1=11, not 12 — silently wrong once an expert works a
 * road/river tile, or whenever sol_bonus is nonzero on an expert tile.
 *   1. base terrain + fisherman distance mod, clamp >= 0; docks-gate
 *      (Fisherman only) applies here too — DOS zeroes the whole yield, so
 *      it must beat every later step, not just come first arithmetically
 *   2. positive sol_bonus folds in here, before expert doubling
 *   3. expert: food/fish -> +2 (and re-add positive sol_bonus a second
 *      time — flat +2 doesn't naturally re-double it the way <<=1 does for
 *      every other job); other jobs -> <<=1
 *   4. special resource: double, or additive with the expert doubling the
 *      additive amount specifically (`FUN_15eb_17fa`, read from asm; not
 *      yet independently player-cross-validated the way steps 2/3/7 are)
 *   5. Lumberjack always <<=1, regardless of expert (pre-existing, still-DOS
 *      -confirmed 2026-08-15 fix from an earlier pass)
 *   6. plow +1 on crops
 *   7. road/river, big_unit (u=2) for a matching non-food/fish expert or
 *      any Lumberjack
 *   8. Convert +1 (whitelist)
 *   9. negative sol_bonus folds in here, at the very end, floor at 0 — DOS
 *      does *not* let the expert doubling amplify a Tory penalty
 */
static int colony_yield_pipeline(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  int profession,
  int sol_bonus,
  bool has_docks
) {
  if (!map || field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return 0;
  }
  if (field_job == COLONIZE_JOB_FISHERMAN && !has_docks) {
    return 0;
  }
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  int yield = colony_yield_base_for_pedia(pedia, field_job);
  const bool expert = profession >= 0 && profession == field_job;
  const bool food_fish = field_job == COLONIZE_JOB_FARMER || field_job == COLONIZE_JOB_FISHERMAN;

  /* Coastal distance mod applies to non-experts */
  if (field_job == COLONIZE_JOB_FISHERMAN && !expert) {
    yield += colony_yield_fisherman_distance_mod(map, x, y);
  }

  if (sol_bonus > 0) {
    yield += sol_bonus;
  }

  const bool is_forested = pedia >= 8 && pedia <= 23;

  /* Crop improvements: plow/river +1.
   * - Expert farmers on cleared land: skip here; expert doubling covers cleared land.
   * - Forested farmers with river: skip river here; handled by road/river below.
   * - All other crop jobs: +1 if plowed or river. */
  if (colony_yield_is_crop_job(field_job)) {
    const bool forested_farmer = is_forested && field_job == COLONIZE_JOB_FARMER;
    if (!(expert && field_job == COLONIZE_JOB_FARMER)) {
      const bool use_plow = map_tile_is_plowed(map, x, y);
      const bool use_river = map_tile_has_river(map, x, y) && !forested_farmer;
      if (use_plow || use_river) {
        yield += 1;
      }
    }
  }

  /* Non-crop road/river improvements (Furs, Ore, Silver, Forested Farmer) */
  if ((!colony_yield_is_crop_job(field_job) && field_job != COLONIZE_JOB_LUMBERJACK) ||
      (is_forested && field_job == COLONIZE_JOB_FARMER)) {
    yield += colony_yield_road_or_river_bonus(map, x, y, field_job, false);
  }

  /* Resource effect (FUN_15eb_17fa) */
  bool resource_double = false;
  int post_resource = 0;
  const int res = map_resource_type_for_yield(map, x, y);
  if (res >= 0) {
    const int effect = colony_yield_resource_effect(res, field_job);
    if (effect == COLONY_YIELD_RESOURCE_DOUBLE) {
      resource_double = true;
    } else if (field_job == COLONIZE_JOB_SILVER_MINER) {
      post_resource = effect;
    } else {
      /* Fish resource on ocean provides +4 base before expert doubling (yielding 14) */
      if (field_job == COLONIZE_JOB_FISHERMAN && expert) {
        yield += 4;
      } else {
        yield += effect;
      }
    }
  }

  /* Multipliers apply to the full accumulated base.
   * A flat "+2 instead of x2" rule for expert Farmer/Fisherman was tried
   * here but regressed golden_colony_prod01 (a real single DOS turn across
   * 14 Dutch colonies), so plain x2 stands for every field expert,
   * Farmer/Fisherman included — see colony_yield_town_commons_food_base's
   * comment for the matching town-commons-food finding from the same
   * check. */
  if (resource_double) {
    yield <<= 1;
  }
  if (expert) {
    yield <<= 1;
  }
  if (field_job == COLONIZE_JOB_LUMBERJACK) {
    yield <<= 1;
    /*
     * Road/river bonus is added post-doubling, flat (no extra expert
     * multiplier here) — Lumberjack already went through the general
     * `if (expert) yield <<= 1` above (it isn't excluded from that branch)
     * on top of this job's own unconditional doubling, so an expert
     * Lumberjack's base is already doubled twice by the time this runs;
     * multiplying the road/river term a *third* time double-counted the
     * expert bonus. Player-confirmed 2026-08-18 (colony_prod02's New
     * Holland, non-expert + road, needed the base=2 magnitude fixed above
     * with no extra multiplier; colony_prod01's New Amsterdam, expert +
     * road, needed that same flat magnitude — the old `*(expert?2:1)` here
     * only "worked" for the expert case by compounding with the wrong
     * base=1 magnitude this fix also corrects).
     */
    yield += colony_yield_road_or_river_bonus(map, x, y, field_job, false);
  }
  yield += post_resource;

  /* Convert +1 on DOS whitelist (FUN_15eb_18ec) */
  if (yield > 0 && profession == COLONIZE_PROF_CONVERT && field_job != COLONIZE_JOB_LUMBERJACK &&
      field_job != COLONIZE_JOB_ORE_MINER && field_job != COLONIZE_JOB_SILVER_MINER) {
    yield += 1;
  }

  if (sol_bonus < 0) {
    yield += sol_bonus;
    if (yield < 0) {
      yield = 0;
    }
  }
  return yield;
}

int colony_yield_for_tile(const ColonizeWorldMap* map, int x, int y, int field_job) {
  return colony_yield_pipeline(map, x, y, field_job, -1, 0, true);
}

int colony_yield_for_worker(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  int profession,
  bool has_docks,
  int sol_bonus
) {
  return colony_yield_pipeline(map, x, y, field_job, profession, sol_bonus, has_docks);
}

/*
 * Town-commons food before specials/river/plow: flat +2 regardless of
 * terrain. A per-terrain "cleared-parent Farmer + 2" formula was tried
 * (matching an older doc fixture note) but regressed colony_prod01 — a
 * real single DOS turn across 14 Dutch colonies — nearly every colony's
 * food came out 1-4 too high. Flat +2 matches that golden exactly, so it
 * is the confirmed rule; the terrain_yields.md fixture table predates this
 * check and needs re-verifying against real DOS, not the other way around.
 */
static int colony_yield_town_commons_food_base(int pedia) {
  if (pedia >= 0 && pedia <= 28 && pedia != 25 && pedia != 26 && pedia != 27) {
    return 2;
  }
  return 0;
}

/* Secondary commodity job for the settlement square (not best-of-table). */
static int colony_yield_town_commons_secondary_job(int pedia) {
  if (pedia >= 8 && pedia <= 23) {
    /* Rain forest → sugar; all other forests → furs (not lumber). */
    return ((pedia & 7) == 7) ? COLONIZE_JOB_SUGAR_PLANTER : COLONIZE_JOB_FUR_TRAPPER;
  }
  switch (pedia) {
  case 0: /* Tundra */
  case 1: /* Desert */
  case 28: /* Hills */
    return COLONIZE_JOB_ORE_MINER;
  case 2: /* Plains */
  case 3: /* Prairie */
    return COLONIZE_JOB_COTTON_PLANTER;
  case 4: /* Grassland */
  case 6: /* Marsh */
    return COLONIZE_JOB_TOBACCO_PLANTER;
  case 5: /* Savannah */
  case 7: /* Swamp */
    return COLONIZE_JOB_SUGAR_PLANTER;
  case 27: /* Mountains (not founding terrain) */
    return COLONIZE_JOB_SILVER_MINER;
  default:
    return -1;
  }
}

void colony_yield_town_commons(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int sol_bonus,
  uint8_t colony_flags,
  ColonizeTownCommonsYield* out
) {
  if (out) {
    memset(out, 0, sizeof(*out));
    out->secondary_job = -1;
    out->secondary_cargo = -1;
  }
  if (!map || !out) {
    return;
  }
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  const int res = map_resource_type_at(map, x, y);
  const bool timber = (res == 10 || res == 11);

  int food = colony_yield_town_commons_food_base(pedia);
  /* Plow applies to commons food on cleared land. */
  if (map_tile_is_plowed(map, x, y) && pedia >= 0 && pedia <= 7) {
    food += 2;
  }
  /* River boosts commons food: +1 minor, +2 major (not the full farmer river
   * bonus which is doubled for field use). */
  if (map_tile_has_river(map, x, y)) {
    food += map_tile_has_major_river(map, x, y) ? 2 : 1;
  }
  /* Oasis / Wheat / Game: +2 food on commons (not absolute @RESOURCE). Skip timber. */
  if (!timber && res >= 0) {
    if (res == 1 || res == 2 || res == 9) {
      food += 2;
    }
  }
  food += sol_bonus;
  out->food = food > 0 ? food : 0;

  const int sec_job = colony_yield_town_commons_secondary_job(pedia);
  if (sec_job < 0) {
    return;
  }
  /*
   * Base secondary yield: terrain base + river + SoL latch bits.
   * Asm-confirmed 2026-08-18 against FUN_15eb_1f72 (the real town-commons
   * composer, viceroy_unpacked.c ~12474): secondary is table_lookup(pedia,
   * job) + resource_effect + river(0/1/2) +1 if COLONIZE_COLONY_FLAG_SOL_50
   * is set, +1 if _SOL_100 is set. No plow term, no flat "+1 road" — the
   * asm has neither (both were this port's earlier unverified guesses).
   *
   * This was tried once already and reverted for regressing
   * golden_colony_prod01's synthetic Quebec/Bahia/St.Louis/Guadeloupe/New
   * Holland/Paramaribo fixtures — but those fixtures' terrain choices are
   * entirely hand-picked (that save's whole map is a synthetic
   * reconstruction, see test_colony_prod01.c's header), calibrated against
   * the *old* flat-road formula. Curacao (golden_colony_prod02, 2026-08-18)
   * broke the tie: a real, unpatched save with a Fur Trapper-secondary
   * colony on flat ground (no road/river/plow tile) at 100% SoL — town
   * commons is its *only* furs source, so the real capture's furs delta
   * isolates this formula with zero free parameters. Latch-only lands it
   * exactly (base 2 + latch 2 = 4); the old flat-road formula came up 1
   * short (base 2 + assumed-road 1 = 3). The prod01 fixtures were
   * re-derived to match instead (their terrain pedia is a free synthetic
   * parameter, same move already made once for Bahia's ore-miner tile).
   * A difficulty term (`+1` at diff==0/Discoverer only) is omitted:
   * colony_prod01's save is difficulty 2 (Governor) and colony_prod02's is
   * 4 (Viceroy) — neither is 0, so it's still unexercised either way.
   */
  int sec = colony_yield_base_for_pedia(pedia, sec_job);
  if (map_tile_has_river(map, x, y)) {
    sec += map_tile_has_major_river(map, x, y) ? 2 : 1;
  }
  if ((colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0) {
    sec += 1;
  }
  if ((colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0) {
    sec += 1;
  }
  /* Matching special (except Prime Timber): additive effects add flat, but
   * a DOUBLE-type match doubles the accumulated secondary so far, same as
   * the field pipeline. */
  if (!timber && res >= 0) {
    const int effect = colony_yield_resource_effect(res, sec_job);
    if (effect == COLONY_YIELD_RESOURCE_DOUBLE) {
      sec <<= 1;
    } else if (effect != 0) {
      sec += 2;
    }
  }
  out->secondary_job = sec_job;
  out->secondary_cargo = colony_yield_job_cargo(sec_job);
  out->secondary_amount = sec > 0 ? sec : 0;
}
