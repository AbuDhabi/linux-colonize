/*
 * Golden fixture for FUN_15eb_28c8's structural reference port
 * (ai_euro_28c8_colonist_job_score_structural, src/core/ai_euro.c).
 * docs/port_plan.md W1.7 — verification only, NOT a wiring test: the port
 * stays reference-only (Tier 3 to flip live, docs/port_plan.md W3.1).
 *
 * No dosbox-x-dumps/* save exercises colonist auto-job-assignment
 * deterministically (checked; the RE doc's own "Not attempted this pass"
 * section already flags this as unbuilt — see
 * original_sources_annotated/turn/colonist_work_plot_28c8.md). Every
 * scenario below is therefore FORMULA-DERIVED from that doc's own
 * Structure §5 write-up, not captured from a live DOS session — expected
 * scores are hand-computed (and, for the multi-tile scenario, cross-checked
 * by an independent from-scratch recompute helper in this file) from the
 * doc's documented terms: field yield (colony_yield_for_tile), the
 * DS:0x2f76+4 labor/travel penalty (map_dos_terr_labor_penalty_byte,
 * scoped to jobs 0/Farmer and 8/Fisherman only per the doc — see the
 * ai_euro.c fix this fixture drove), the population-cap headroom clamp,
 * and the current-job sticky-preference doubling. Every other term the doc
 * lists ("Remaining genuinely open terms") is left at 0 by the port and
 * not modeled here either — this fixture only proves the resolved terms.
 *
 * Every field tile below is marked MAP_LAYER2_SUPPRESS. colony_yield_for_
 * tile's real pipeline folds in a *separate*, already-ported, coordinate-
 * hash-seeded "special resource" term (map_resource_type_for_yield,
 * FUN_12ab_0458 — not part of 28c8's own formula, already covered by
 * colony_yield.c's own tests) that would otherwise make this fixture's
 * expected numbers depend on which absolute (x,y) the test happens to
 * pick — confirmed empirically (a throwaway dump program) to flip a
 * scenario below from its intended outcome. MAP_LAYER2_SUPPRESS forces
 * that unrelated term to 0 so every expected value here reduces to the
 * plain terrain-class base yield (NAMES.TXT table, plus Farmer's
 * documented unconditional +1 non-expert crop bonus).
 */
#include "core/ai_euro.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/map.h"
#include "core/turn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_W 16
#define MAP_H 16

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_euro_28c8_job_score: FAIL %s\n", msg);
  return 1;
}

static void map_init(ColonizeWorldMap* map, uint8_t* terrain, uint8_t* layer2, uint8_t* layer3) {
  memset(map, 0, sizeof(*map));
  map->width = MAP_W;
  map->height = MAP_H;
  map->tile_count = MAP_W * MAP_H;
  map->terrain = terrain;
  map->layer2 = layer2;
  map->layer3 = layer3;
  memset(terrain, 0, MAP_W * MAP_H); /* 0 = Tundra (pedia 0) everywhere by default */
  memset(layer2, 0, MAP_W * MAP_H);
  memset(layer3, 0, MAP_W * MAP_H);
}

/* Suppress the coordinate-hash special-resource term (see top comment) on
 * every one of the colony's 8 field tiles, so expected yields reduce to
 * the plain terrain-class base table. */
static void suppress_field_tile_resources(ColonizeWorldMap* map, int cx, int cy) {
  static const int dx[COLONIZE_COLONY_FIELD_TILES] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[COLONIZE_COLONY_FIELD_TILES] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    const int tx = cx + dx[ti];
    const int ty = cy + dy[ti];
    map->layer2[ty * MAP_W + tx] |= MAP_LAYER2_SUPPRESS;
  }
}

static void colony_init_common(ColonizeColony* c, int nation, int cx, int cy) {
  memset(c, 0, sizeof(*c));
  c->id = 0;
  c->active = true;
  c->nation_id = nation;
  c->x = cx;
  c->y = cy;
  c->population = 1;
  c->colonist_count = 1;
  c->building_in_production = -1;
  c->warehouse_level = 0; /* pop cap floor 100, per FUN_15eb_0a50 */
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES; ++i) {
    c->tiles[i] = -1;
  }
  c->colonists[0].active = true;
  c->colonists[0].unit_type_index = -1;
  c->colonists[0].building_type = -1;
  c->colonists[0].field_job = -1;
}

/*
 * Scenario 1 — the bug the fixture caught, on a single Prairie tile
 * (terr_class 3, labor penalty 15). colony_yield_for_tile's real pipeline
 * (not just the raw NAMES.TXT base table — Farmer gets an unconditional
 * +1 non-expert crop bonus the raw table doesn't show) gives, at Prairie:
 *   Farmer (job0): yld 4 -> base 32, penalty-scoped (job0 IS generalist)
 *                  -> 32-15 = 17.
 *   Cotton Planter (job3): yld 3 -> base 24, NOT penalty-scoped per the
 *                  doc (only jobs 0/8 pay DS:0x2f76+4) -> stays 24.
 * Fixed formula best = Cotton Planter, score 24 (beats Farmer's 17).
 * Before this pass's fix (penalty subtracted from every job
 * unconditionally) Cotton Planter's 24 was wrongly knocked down to 9
 * (24-15), so the port instead picked Farmer at 17 — a real best-pick
 * flip on ONE tile, not just a score-magnitude nit. (Values confirmed
 * against the live colony_yield_for_tile pipeline via a throwaway dump
 * program, not hand-derived from the raw table alone — see this file's
 * top comment.)
 */
static int unit_penalty_scoped_to_generalist_jobs(void) {
  uint8_t terrain[MAP_W * MAP_H];
  uint8_t layer2[MAP_W * MAP_H];
  uint8_t layer3[MAP_W * MAP_H];
  ColonizeWorldMap map;
  map_init(&map, terrain, layer2, layer3);

  const int cx = 8;
  const int cy = 8;
  terrain[(cy - 1) * MAP_W + cx] = 3; /* tile 0 (N, dx=0,dy=-1): Prairie */
  suppress_field_tile_resources(&map, cx, cy);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colony_init_common(&colonies.colonies[0], /*nation=*/1, cx, cy);

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0; /* colony's nation (1) != human -> AI full-search branch */
  ctx.colonies = &colonies;
  ctx.map = &map;

  AiEuro28c8JobCandidate best;
  int ok = ai_euro_28c8_colonist_job_score_structural(&ctx, 0, 0, &best);
  if (!ok) {
    return fail("penalty_scoped: no assignment found");
  }
  if (best.job != COLONIZE_JOB_COTTON_PLANTER) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: got job=%d want=%d (Cotton Planter)\n",
            best.job, COLONIZE_JOB_COTTON_PLANTER);
    return fail("penalty_scoped: wrong job");
  }
  if (best.tile != 0) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: got tile=%d want=0 (Prairie/N)\n", best.tile);
    return fail("penalty_scoped: wrong tile");
  }
  if (best.score != 24) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: got score=%d want=24\n", best.score);
    return fail("penalty_scoped: wrong score");
  }
  return 0;
}

/*
 * Independent recompute of the doc's documented formula (field yield,
 * job-0/8-scoped labor penalty, AI headroom clamp, current-job doubling)
 * over every (tile, job) pair, mirroring the port's own iteration order
 * (tile ascending, job ascending, strict '>' so the first-seen max wins
 * ties) so it can be compared candidate-for-candidate against the port's
 * actual output — not just re-asserting the same hand-picked numbers.
 */
static void recompute_expected(
  const ColonizeWorldMap* map,
  const ColonizeColony* col,
  int current_job,
  AiEuro28c8JobCandidate* out_best
) {
  const int pop_cap = col->warehouse_level == 0 ? 100 : ((int)col->warehouse_level + 1) * 100;
  out_best->job = -1;
  out_best->tile = -1;
  out_best->score = -1000000;
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    int dx = 0, dy = 0;
    colonies_field_tile_delta(ti, &dx, &dy);
    const int tx = col->x + dx;
    const int ty = col->y + dy;
    const int terr = map_dos_terr_class_at(map, tx, ty);
    const int penalty = map_dos_terr_labor_penalty_byte(terr);
    for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
      int yld = colony_yield_for_tile(map, tx, ty, job);
      if (yld <= 0) {
        continue;
      }
      int headcount = 0;
      for (int i = 0; i < col->colonist_count; ++i) {
        if (col->colonists[i].active && col->colonists[i].field_job == job) {
          ++headcount;
        }
      }
      int headroom = pop_cap - headcount;
      if (headroom < 1) {
        headroom = 1;
      }
      if (yld > headroom) {
        yld = headroom;
      }
      int score = yld * 8;
      if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
        score -= penalty;
      }
      if (job == current_job) {
        score *= 2;
      }
      if (score > out_best->score) {
        out_best->score = score;
        out_best->job = job;
        out_best->tile = ti;
      }
    }
  }
}

/*
 * Scenario 2 — full 8-tile matrix, one per pedia terrain class 0..7
 * (Tundra..Swamp), colonist's current job = Ore Miner (job 6, a non-
 * generalist job) to exercise sticky doubling on a job the penalty must
 * NOT touch. Hand-computed best (see docs/port_plan.md W1.7 entry for the
 * per-tile table): Ore Miner @ tile 0 (Tundra), score 32 — Tundra's Ore
 * yield (2) doubled (16*2=32) ties with Desert/Marsh/Swamp's own doubled
 * Ore score, and tile 0 wins the tie as the first-seen max (port's
 * iteration order: tile ascending, job ascending, strict '>').
 * Cross-checked against recompute_expected() above, an independently
 * written re-implementation of the same doc-documented formula.
 */
static int unit_full_matrix_sticky_doubling(void) {
  uint8_t terrain[MAP_W * MAP_H];
  uint8_t layer2[MAP_W * MAP_H];
  uint8_t layer3[MAP_W * MAP_H];
  ColonizeWorldMap map;
  map_init(&map, terrain, layer2, layer3);

  const int cx = 8;
  const int cy = 8;
  /* Field tile order (colony.c k_field_dx/dy): N,NE,E,SE,S,SW,W,NW.
   * Assign pedia terrain class == field tile index for all 8. */
  static const int dx[COLONIZE_COLONY_FIELD_TILES] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[COLONIZE_COLONY_FIELD_TILES] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    terrain[(cy + dy[ti]) * MAP_W + (cx + dx[ti])] = (uint8_t)ti;
  }
  suppress_field_tile_resources(&map, cx, cy);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colony_init_common(&colonies.colonies[0], /*nation=*/1, cx, cy);
  colonies.colonies[0].colonists[0].field_job = COLONIZE_JOB_ORE_MINER; /* sticky */

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0;
  ctx.colonies = &colonies;
  ctx.map = &map;

  AiEuro28c8JobCandidate expected;
  recompute_expected(&map, &colonies.colonies[0], COLONIZE_JOB_ORE_MINER, &expected);
  if (expected.job != COLONIZE_JOB_ORE_MINER || expected.tile != 0 || expected.score != 32) {
    fprintf(stderr,
      "unit_ai_euro_28c8_job_score: recompute sanity mismatch job=%d tile=%d score=%d "
      "(want job=%d tile=0 score=32)\n",
      expected.job, expected.tile, expected.score, COLONIZE_JOB_ORE_MINER);
    return fail("full_matrix: hand-derived expectation and recompute helper disagree");
  }

  AiEuro28c8JobCandidate best;
  int ok = ai_euro_28c8_colonist_job_score_structural(&ctx, 0, 0, &best);
  if (!ok) {
    return fail("full_matrix: no assignment found");
  }
  if (best.job != expected.job || best.tile != expected.tile || best.score != expected.score) {
    fprintf(stderr,
      "unit_ai_euro_28c8_job_score: got job=%d tile=%d score=%d want job=%d tile=%d score=%d\n",
      best.job, best.tile, best.score, expected.job, expected.tile, expected.score);
    return fail("full_matrix: port output doesn't match doc-derived recompute");
  }
  return 0;
}

int main(void) {
  if (unit_penalty_scoped_to_generalist_jobs() != 0) {
    return 1;
  }
  if (unit_full_matrix_sticky_doubling() != 0) {
    return 1;
  }
  fprintf(stderr, "unit_ai_euro_28c8_job_score: ok\n");
  return 0;
}
