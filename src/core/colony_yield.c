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
 * Hills Farmer is NAMES.TXT's 1 after all — the 2026-08-15 "player-confirmed
 * 2" observation was base 1 + the unconditional farmer +1 from the DOS
 * improvement stack (FUN_15eb_18ec 1c37, see colony_yield_pipeline), which
 * this table had absorbed before that stack was ported literally. Pinned
 * 2026-09-03 by farming/case3's expert Farmer on a bare Hill = 4
 * (1 + expert 2 + farmer 1); base 2 would give 5. */
static const int k_other[5][COLONIZE_FIELD_JOB_COUNT] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 3},
  {0, 0, 0, 0, 0, 0, 0, 0, 3},
  {0, 0, 0, 0, 0, 0, 4, 1, 0},
  {1, 0, 0, 0, 0, 0, 4, 0, 0},
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
    /* +3, not +2 — FUN_15eb_17fa asm: `(param_1 == 8) && (param_2 == 4)`
     * adds 3 (viceroy_unpacked.c 11736-11738); caught 2026-09-03 while
     * re-reading the table against the farming saves. */
    v += 3;
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
  bool has_docks,
  uint8_t colony_flags
) {
  /* Field-job expert-fish/farmer re-add now goes through `sol_bonus`
   * itself (see the is_expert_food_fish block below) rather than
   * reconstructing it from the colony's SoL latch bits directly — the
   * DOS variable it mirrors (`local_1c`) already folds those bits in
   * upstream of both fold points. `colony_flags` stays a parameter for
   * API symmetry with colony_yield_town_commons (a different DOS
   * function, FUN_15eb_1f72, which does read its own latch bits
   * directly), but this function no longer needs it directly. */
  (void)colony_flags;
  if (!map || field_job < 0 || field_job >= COLONIZE_FIELD_JOB_COUNT) {
    return 0;
  }
  if (field_job == COLONIZE_JOB_FISHERMAN && !has_docks) {
    return 0;
  }
  const int pedia = map_pedia_terrain_index_at(map, x, y);
  int yield = colony_yield_base_for_pedia(pedia, field_job);
  const bool expert = profession >= 0 && profession == field_job;
  const bool is_expert_food_fish =
    expert && (field_job == COLONIZE_JOB_FARMER || field_job == COLONIZE_JOB_FISHERMAN);

  /*
   * Coastal distance mod: applies to *every* Fisherman regardless of
   * skill match, added here to the raw base before anything else —
   * asm-confirmed 2026-08-18 (FUN_15eb_18ec ~11814-11838, unconditional
   * on skill). A same-day attempt kept this pre-multiply placement for
   * non-experts only and added a *second*, post-doubling copy for
   * experts instead of unifying the two — it happened to fit two
   * no-resource expert Fisherman tiles (colony_prod02's New Amsterdam/
   * New Holland) by coincidence (base 3 with no resource doesn't expose
   * the ordering difference), but gave the wrong total once a resource
   * was involved (New Amsterdam's *other* expert Fisherman, on a Fishery
   * tile) once the flat-+2 expert formula below was also wired — this
   * single early placement, shared by expert and non-expert alike, is
   * the version that reconciles all of them together.
   */
  if (field_job == COLONIZE_JOB_FISHERMAN && yield != 0) {
    yield += colony_yield_fisherman_distance_mod(map, x, y);
  }

  /*
   * Fur Trapper pre-multiplier road/river add — FUN_15eb_18ec's own job==4
   * block right after the base lookup (viceroy_unpacked.c ~11840-11850,
   * gated on base != 0): +1 for a road (runtime mask 0x0a), +1 for a river
   * with +1 more on a major river (terrain bits 0x40/0x80), all added
   * *before* the SoL fold and expert doubling. Combined with the
   * unit-sized add in the shared improvement
   * stack below this reproduces the old "furs road/river bucket = 2,
   * major river 4, expert-doubled" totals exactly (2026-08-15 Hudson
   * capture: (3 + road 1 + sol 2)×2 + road u2 = 14, ×Hudson = 28).
   */
  if (field_job == COLONIZE_JOB_FUR_TRAPPER && yield != 0) {
    if (map_tile_has_road(map, x, y)) {
      yield += 1;
    }
    if (map_tile_has_river(map, x, y)) {
      yield += map_tile_has_major_river(map, x, y) ? 2 : 1;
    }
  }

  /*
   * Zero-base gate — DOS folds positive SoL only into a nonzero yield
   * (`local_26 != 0 && local_1c > 0`, asm 15eb:1aeb-1afa), and the expert
   * branch below carries the same `local_26 != 0` gate: a terrain whose
   * table base is 0 for this job stays 0 through both. Player-confirmed
   * 2026-09-03 (farming/case4, golden_colony_prod03): expert Farmer on
   * Mountains = 0 food — the ungated port paid the expert flat +2 and
   * then the stack's farmer +1 on top, inventing 3 food from bare rock.
   */
  if (sol_bonus > 0 && yield != 0) {
    yield += sol_bonus;
  }

  /*
   * Resource effect (FUN_15eb_17fa). For a matching Farmer/Fisherman
   * expert, the real asm applies this *after* the flat-+2 expert step
   * below and doubles just this term (not the whole accumulated yield
   * again) — deferred here to match. Every other job keeps the original
   * placement (before the multiplier), which is independently validated
   * elsewhere and untouched; Farmer/Fisherman never have a DOUBLE-type
   * resource in this table (only Cotton/Tobacco/Sugar planters do), so
   * deferring never interacts with that path.
   */
  bool resource_double = false;
  int post_resource = 0;
  int deferred_resource = 0;
  const int res = map_resource_type_for_yield(map, x, y);
  if (res >= 0) {
    const int effect = colony_yield_resource_effect(res, field_job);
    if (field_job == COLONIZE_JOB_FARMER || field_job == COLONIZE_JOB_FISHERMAN) {
      deferred_resource = effect;
    } else if (effect == COLONY_YIELD_RESOURCE_DOUBLE) {
      resource_double = true;
    } else if (field_job == COLONIZE_JOB_SILVER_MINER) {
      post_resource = effect;
    } else {
      yield += effect;
    }
  }

  /*
   * Multipliers apply to the full accumulated base — except a matching
   * Farmer/Fisherman expert, who gets flat +2 plus a second copy of the
   * *same* positive sol_bonus already folded in above (step 2), instead of
   * ×2. Asm-confirmed 2026-08-18 (FUN_15eb_18ec ~11890-11899, `local_16`/
   * `local_18` branch) and player-confirmed via four real, un-synthesized
   * golden_colony_prod02 town-commons-food values (Curacao/Recife/New
   * Holland, plus Fort Orange after subtracting its own confirmed
   * plow+river) that pinned the *sibling* town-commons formula first —
   * solving those together showed the Farmer/Fisherman expert path needed
   * this exact shape too: Fort Orange's expert Farmer (Savannah, no
   * resource) needs base 3 + sol_bonus fold 2 + flat 2 + sol_bonus re-add
   * 2 = 9, not base+sol ×2 = 10; New Amsterdam's expert Fisherman +
   * Fishery resource needs the same shape plus its own doubled resource
   * (3 base + fold 2 + flat 2 + re-add 2 + resource 3×2 = 16, not the old
   * ×2-of-everything). Two earlier same-day attempts at pieces of this
   * (distance mod alone, "+2 not ×2" alone) each looked locally right but
   * were curve-fit against a *wrong* town-commons baseline (flat +2
   * instead of the real per-terrain class) and regressed real colonies
   * when tested in isolation; this version is the one that reconciles
   * both formulas together against every real anchor found so far.
   *
   * 2026-08-24: the re-add term is `sol_bonus` itself, not a fresh
   * reconstruction from the colony's SoL latch bits. Direct read of
   * `FUN_15eb_18ec` (~11866-11889) shows `local_1c` — the value re-added
   * here — is computed *once*, earlier in the function, from
   * `byte[colony+0x1f]` (colonist count/population, already named
   * project-wide: `colonist_work_plot_28c8.md`, `colony_eot_production.md`
   * "+0x1f | Population") times `FUN_15eb_0274()` (colony SoL%, already
   * ported as `colony_prod_sol_percent` — same rebel-dividend/-divisor
   * shape plus the same human-only Bolivar +20), divided by the same
   * 10-or-(10-difficulty) gate, plus the same `+0x1c` bit `0x04`/`0x02`
   * latch adds `colony_prod_sol_bonus` already applies (`COLONIZE_COLONY_
   * FLAG_SOL_50`/`_100` match exactly) — i.e. `local_1c` *is*
   * `colony_prod_sol_bonus_field()`'s return value, already threaded
   * through this pipeline as the `sol_bonus` parameter and already folded
   * in once above. It is the *same* variable, unmodified, re-added a
   * second time here — not a separately-derived latch-only value. The
   * previous latch-bits reconstruction only coincidentally matched
   * `sol_bonus` in the validated examples above because their Tory-penalty
   * term happened to be 0 (`sol_bonus == latch bits` only in that case);
   * whenever the tory-penalty term is nonzero the two diverge and the old
   * code re-added the wrong amount. Fixed: re-add `sol_bonus` itself.
   */
  if (resource_double) {
    yield <<= 1;
  }
  /* `yield != 0` gate: see the zero-base note above (DOS 15eb:1afd-1b25).
   * For non-food experts the gate is a no-op (doubling 0 is 0, and a
   * resource-only yield doubles to the same total DOS reaches by doubling
   * the effect term instead); it only changes the food/fish flat +2. */
  if (yield != 0) {
    if (is_expert_food_fish) {
      yield += 2;
      if (sol_bonus > 0) {
        yield += sol_bonus;
      }
    } else if (expert) {
      yield <<= 1;
    }
  }
  if (field_job == COLONIZE_JOB_LUMBERJACK) {
    yield <<= 1;
  }
  yield += post_resource;
  if (deferred_resource != 0) {
    yield += expert ? deferred_resource * 2 : deferred_resource;
  }

  /*
   * Improvement stack — FUN_15eb_18ec's literal tail block (asm
   * 15eb:1c16-1c9c, verified instruction-by-instruction 2026-09-03; runs
   * after the expert/resource/Lumberjack multipliers, before Hudson/
   * convert/negative-SoL). Unit `u` = 2 for a matching non-food/fish
   * expert or any Lumberjack, else 1. Adds, all stacking:
   *   - Farmer: +u, UNCONDITIONALLY — any skill level, expert included,
   *     no plow gate (asm 1c37-1c40 is a bare `job==0` test). This is
   *     why every "player-confirmed" farmer base sat exactly 1 above its
   *     NAMES.TXT row (Hills 2-vs-1, and the farming/case1-3 criminals):
   *     the +1 was being read into the base table. Player-confirmed
   *     2026-09-03 via farming/case3 (golden_colony_prod03): expert
   *     Farmer, Broadleaf+Game = 8 (1 + expert 2 + Game 2×2 + farmer 1),
   *     and the same expert on a bare Hill = 4 (1 + 2 + 1), which pins
   *     Hills farmer base back to NAMES's 1.
   *   - road (runtime mask 0x0a) on non-crop jobs (>3): +u.
   *   - plow (runtime bit 0x40) on crop jobs (<4): +u.
   *   - river (terrain bit 0x40): +u; major river (terrain 0x80) adds +u
   *     once more ONLY if the stack so far is exactly u — i.e. river was
   *     the sole contributor. A Farmer never qualifies (the farmer +u is
   *     always there first), which is what farming/case1's major-river
   *     expert = 6 was showing; a Fisherman does (Lake+major=6 capture,
   *     2026-08-15).
   * Replaces the port's former crop-improvements block, the non-crop
   * road/river bucket add, and the Lumberjack post-double road/river tail
   * — all curve-fit shapes that matched every prior anchor only because
   * their totals coincide with this stack on those tiles (checked
   * per-anchor: ore/fur/lumber road & river cases decompose identically).
   */
  if (yield > 0) {
    const int u =
      ((expert && field_job != COLONIZE_JOB_FARMER && field_job != COLONIZE_JOB_FISHERMAN) ||
       field_job == COLONIZE_JOB_LUMBERJACK)
        ? 2
        : 1;
    int add = 0;
    if (field_job == COLONIZE_JOB_FARMER) {
      add = u;
    }
    if (field_job > COLONIZE_JOB_COTTON_PLANTER && map_tile_has_road(map, x, y)) {
      add += u;
    }
    if (field_job <= COLONIZE_JOB_COTTON_PLANTER && map_tile_is_plowed(map, x, y)) {
      add += u;
    }
    if (map_tile_has_river(map, x, y)) {
      add += u;
      if (map_tile_has_major_river(map, x, y) && add == u) {
        add += u;
      }
    }
    yield += add;
  }

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
  return colony_yield_pipeline(map, x, y, field_job, -1, 0, true, 0);
}

int colony_yield_for_worker(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int field_job,
  int profession,
  bool has_docks,
  int sol_bonus,
  uint8_t colony_flags
) {
  return colony_yield_pipeline(map, x, y, field_job, profession, sol_bonus, has_docks, colony_flags);
}

/*
 * Town-commons food before specials/river/plow: flat +2 regardless of
 * terrain. A per-terrain "cleared-parent Farmer + 2" formula was tried
 * (matching an older doc fixture note) but regressed colony_prod01 — a
 * real single DOS turn across 14 Dutch colonies — nearly every colony's
 * food came out 1-4 too high. Flat +2 matches that golden exactly, so it
 * is the confirmed rule; the terrain_yields.md fixture table predates this
 * check and needs re-verifying against real DOS, not the other way around.
 *
 * 2026-08-18: FUN_15eb_1f72 (~12506-12518, the same composer secondary
 * already uses) shows a real 4-way class split by pedia instead — class 2
 * (most forest + Hills/Mountains) happens to equal flat +2, which is why
 * this passed golden_colony_prod01/02 for the large majority of colonies
 * (their town centers mostly sit on forest); class 3 (most cleared land,
 * e.g. Savannah) is +1 higher, and class 1 (Desert/Scrub) is -1 lower.
 * Player-confirmed via colony_prod02's Recife (Savannah, class 3 → real
 * food 3, not flat +2's 2) — but tried twice now (once combined with an
 * incorrect stacking version of the Farmer +1 fix, once alone after that
 * fix's real "replaces plow" shape was confirmed) and both times it
 * overshoots real, un-synthesized colonies with cleared-land town centers
 * (Montreal, Fort Orange, Guadeloupe, New Holland, Vlissingen — all real
 * captures, not free to re-derive) by the same +1 class 3 predicts. Since
 * those are real data too and flat +2 already matches them exactly,
 * class 3 is apparently *not* generally right for cleared land — Recife's
 * Savannah is the outlier needing an explanation of its own, not
 * everyone else being wrong. Left as flat +2; Recife's food gap (-1,
 * golden_colony_prod02) stays open pending that explanation.
 */
static int colony_yield_town_commons_food_base(int pedia) {
  if (pedia == 24) {
    return 0;
  }
  if (pedia == 1 || pedia == 9 || pedia == 17) {
    return 1;
  }
  if (pedia == 27 || pedia == 28) {
    return 2;
  }
  if (pedia >= 8 && pedia <= 23) {
    return 2;
  }
  if (pedia >= 0 && pedia <= 23) {
    return 3;
  }
  return 0;
}

/*
 * Secondary commodity job for the settlement square: DOS is a best-of loop,
 * not a fixed per-terrain table — FUN_15eb_1f72 (viceroy_unpacked.c
 * ~12553-12570) iterates jobs 1..7 skipping 5 (Sugar/Tobacco/Cotton/Fur/
 * Ore/Silver — never Farmer, Lumberjack, or Fisherman), scores each as
 * table base + FUN_15eb_17fa resource effect (a DOUBLE-type match doubles
 * the base inside the comparison), and keeps the strictly-greatest (first
 * job wins ties, so riverless Swamp still picks Sugar over equal-base Ore
 * — matches the Guadeloupe capture that calibrated the old fixed table).
 * Player-confirmed 2026-09-03 (farming/case1+2 saves): New Amsterdam's
 * Swamp center with a Minerals resource makes 5 ore per turn (Ore base 2 +
 * Minerals +3 beats Sugar 2), where the old fixed table said 2 sugar.
 * Returns the winning job via *out_job and its base+resource score.
 */
static int colony_yield_town_commons_secondary_pick(int pedia, int res, int* out_job) {
  int best_job = -1;
  int best = 0;
  for (int job = 1; job <= 7; ++job) {
    if (job == COLONIZE_JOB_LUMBERJACK) {
      continue;
    }
    int v = colony_yield_base_for_pedia(pedia, job);
    if (res >= 0) {
      const int effect = colony_yield_resource_effect(res, job);
      if (effect == COLONY_YIELD_RESOURCE_DOUBLE) {
        v <<= 1;
      } else {
        v += effect;
      }
    }
    if (v > best) {
      best = v;
      best_job = job;
    }
  }
  *out_job = best_job;
  return best;
}

void colony_yield_town_commons(
  const ColonizeWorldMap* map,
  int x,
  int y,
  int sol_bonus,
  uint8_t colony_flags,
  int difficulty,
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
  /* _for_yield: the colony center always carries the settlement bit, and
   * plain map_resource_type_at returns -1 for settlement tiles — DOS's
   * FUN_137f_04b0 read in FUN_15eb_1f72 has no settlement gate. Caught
   * 2026-09-03 by the farming saves' Minerals commons (5 ore/turn). */
  const int res = map_resource_type_for_yield(map, x, y);
  const bool timber = (res == 10 || res == 11);

  int food = colony_yield_town_commons_food_base(pedia);
  /*
   * Difficulty handout — FUN_15eb_1f72 ~12519-12522 (DS 0x53a6), right after
   * the class split: +2 on Discoverer, +1 on Explorer, nothing above. No
   * nation gate (AI colonies get it too). Never exercised by the goldens
   * (colony_prod01 is difficulty 2, colony_prod02 is 4); asm-read only.
   */
  if (difficulty == 0) {
    food += 2;
  } else if (difficulty == 1) {
    food += 1;
  }
  /*
   * Plow applies to commons food on cleared land: +1, not +2.
   * Player-confirmed 2026-08-18 via two real, un-synthesized
   * golden_colony_prod02 colonies (Guadeloupe, Vlissingen) whose own
   * plowed-full-latch-class-3 town centers needed food 6, not the +2
   * version's 7 — confirmed only once their real Fisherman tiles
   * (mismatched-skill Ore Miner and Convert respectively) were checked
   * against direct player-observed values and found already exact,
   * isolating the gap to commons alone. This also exposed that Fort
   * Orange's own real, plowed non-expert Farmer field tile had been
   * under-credited by the same +1 elsewhere (the non-expert Farmer plow
   * term was wrongly concluded not to stack — see the crop-improvements
   * comment in colony_yield_pipeline); fixing both together reconciles
   * Fort Orange (and New Amsterdam) exactly again.
   */
  if (map_tile_is_plowed(map, x, y) && pedia >= 0 && pedia <= 7) {
    food += 1;
  }
  /*
   * NO river term on commons food — 2026-09-03. FUN_15eb_1f72's food block
   * reads only `FUN_137f_0142 & 0x40` (+1, the runtime plow bit — the plow
   * term above); the terrain-byte river value (FUN_137f_010e, local_14
   * 1/2) feeds the SECONDARY only. The former "+1 minor / +2 major" here
   * was double-counting Fort Orange's plowed+rivered Savannah center
   * (7 vs DOS 6) — masked until now because the same colony's expert
   * Farmer was undercounted by exactly 1 (the missing unconditional
   * farmer +1, see colony_yield_pipeline's improvement stack), so the
   * colony aggregate matched with both errors in place. farming/case3
   * broke the tie by pinning the farmer +1 on riverless tiles.
   */
  /* Oasis / Wheat / Game: +2 food on commons (not absolute @RESOURCE). Skip timber. */
  if (!timber && res >= 0) {
    if (res == 1 || res == 2 || res == 9) {
      food += 2;
    }
  }
  /*
   * SoL adds via the colony's latch bits here, not the general (live,
   * Tory-adjusted) sol_bonus this function still takes as a parameter for
   * callers that haven't been re-threaded — see FUN_15eb_1f72's food
   * block. Player-confirmed 2026-08-18 across four real, direct
   * town-commons-food values (colony_prod02's Curacao 4/Recife 3/New
   * Holland 5, plus Fort Orange after subtracting its own confirmed
   * plow+river): Curacao (Broadleaf, class 2, full latch) and Recife
   * (Savannah, class 3, no latch) alone are consistent with either a
   * general-sol or latch-based +2, but New Holland (Savannah, class 3,
   * full latch, no plow/river) pins it at +2 specifically from both latch
   * bits, not a coincidence of this colony's live SoL also reading 100%.
   */
  if ((colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0) {
    food += 1;
  }
  if ((colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0) {
    food += 1;
  }
  (void)sol_bonus;
  out->food = food > 0 ? food : 0;

  /*
   * Secondary: best-of loop over jobs (see colony_yield_town_commons_
   * secondary_pick), then FUN_15eb_1f72 adds to the winner only:
   * +1 at Discoverer, river +1/+2 (minor/major, from the terrain byte,
   * FUN_137f_010e bits 0x40/0x80), +1 per SoL latch bit. No plow term, no
   * flat "+1 road" (both were earlier unverified guesses; the asm has
   * neither — Curacao's 100%-SoL Fur-secondary capture pinned the latch
   * form, 2026-08-18). The resource effect lives *inside* the pick loop at
   * its real FUN_15eb_17fa magnitude (e.g. Minerals+Ore = +3), replacing
   * an older flat "+2 on any match" applied after the fact — that undercut
   * New Amsterdam's Minerals commons by 1 and picked the wrong job
   * entirely (player-confirmed 2026-09-03, farming saves: 5 ore, not
   * 2 sugar). Difficulty term stays unexercised by the goldens
   * (prod01 is difficulty 2, prod02/03 are 4); asm-read only.
   */
  (void)timber;
  int sec_job = -1;
  int sec = colony_yield_town_commons_secondary_pick(pedia, res, &sec_job);
  if (sec_job < 0) {
    return;
  }
  if (difficulty == 0) {
    sec += 1;
  }
  if (map_tile_has_river(map, x, y)) {
    sec += map_tile_has_major_river(map, x, y) ? 2 : 1;
  }
  if ((colony_flags & COLONIZE_COLONY_FLAG_SOL_50) != 0) {
    sec += 1;
  }
  if ((colony_flags & COLONIZE_COLONY_FLAG_SOL_100) != 0) {
    sec += 1;
  }
  out->secondary_job = sec_job;
  out->secondary_cargo = colony_yield_job_cargo(sec_job);
  out->secondary_amount = sec > 0 ? sec : 0;
}
