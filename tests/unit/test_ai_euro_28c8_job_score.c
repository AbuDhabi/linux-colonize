/*
 * FUN_15eb_28c8 work-plot scorer fixture
 * (ai_euro_28c8_colonist_job_score_structural, src/core/ai_euro.c).
 *
 * Rewritten 2026-09-22 for bugs.md #570: the scorer is now a term-for-term
 * port of raw 13012-13130, so this fixture checks the DOS formula, not the
 * old structural sketch (which it used to lock in):
 *
 *   score = yld*8 + (7 - |dx| - |dy|)                   raw 13017-13024
 *   sticky x2 on the colonist's current job, HUMAN colonies only  raw 13025
 *   non-emergency branch: score = (local_38 + local_4) * score   raw 13120
 *     local_4  = 0 (AI) / 1 (human) for jobs 0 and 8 with no AI tick running,
 *                else the DS:0x84bc price byte (0 with no COL1 save bound)
 *     local_38 = local_4 + 1, +1 more when DS:0x2b6[job] names a consumer job
 *                (FUN_15eb_15c6)
 *
 * Both scenarios pin the food-emergency flag (bVar2) OFF so the cargo-weight
 * branch is the one under test: pop 1 (food demand 2) with 50 food in store
 * makes `DS:0x8e32*0x10 < colony+0x9a` true for the AI shape and
 * `DS:0x8dc8 < DS:0x8e0a` false for the human shape. No COL1 save is bound,
 * so there are no Indian claims, no price row and no wealth ranks.
 *
 * Every field tile is marked MAP_LAYER2_SUPPRESS: colony_yield_for_tile folds
 * in a coordinate-hash "special resource" term (map_resource_type_for_yield,
 * FUN_12ab_0458) that is not part of 28c8 and would make expected numbers
 * depend on the absolute (x,y) the test picks.
 */
#include "core/ai_euro.h"
#include "core/colony.h"
#include "core/colony_yield.h"
#include "core/map.h"
#include "core/turn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_runner.h"

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
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES_MAX; ++i) {
    c->tiles[i] = -1;
  }
  c->colonists[0].active = true;
  c->colonists[0].unit_type_index = -1;
  c->colonists[0].building_type = -1;
  c->colonists[0].field_job = -1;
}

/*
 * Scenario 1 — the distance term (raw 13017-13024), which the port used to
 * drop entirely: `score = yld*8 + (7 - |dx| - |dy|)`. Two identical Prairie
 * plots, one orthogonal (tile 0, N, |dx|+|dy| = 1) and one diagonal (tile 1,
 * NE, |dx|+|dy| = 2). Same yields, same weights, so the only thing that can
 * separate them is the distance term — the nearer plot must win, and the
 * winning score must be the odd number `yld*8 + 6` scaled by the cargo
 * weight, never a bare multiple of 8.
 */
static int unit_distance_term_breaks_ties(void) {
  uint8_t terrain[MAP_W * MAP_H];
  uint8_t layer2[MAP_W * MAP_H];
  uint8_t layer3[MAP_W * MAP_H];
  ColonizeWorldMap map;
  map_init(&map, terrain, layer2, layer3);

  const int cx = 8;
  const int cy = 8;
  terrain[(cy - 1) * MAP_W + cx] = 3;       /* tile 0 (N):  Prairie */
  terrain[(cy - 1) * MAP_W + (cx + 1)] = 3; /* tile 1 (NE): Prairie */
  suppress_field_tile_resources(&map, cx, cy);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  colony_init_common(&colonies.colonies[0], /*nation=*/1, cx, cy);
  colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 50; /* bVar2 off */

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0; /* colony nation 1 -> AI shape */
  ctx.colonies = &colonies;
  ctx.map = &map;

  AiEuro28c8JobCandidate best;
  int ok = ai_euro_28c8_colonist_job_score_structural(&ctx, 0, 0, &best);
  if (!ok) {
    return fail("distance_term: no assignment found");
  }
  if (best.tile != 0) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: got tile=%d want=0 (nearer plot)\n",
            best.tile);
    return fail("distance_term: diagonal plot beat the orthogonal one");
  }
  {
    const int cargo = colony_yield_job_cargo(best.job);
    const int yld = colony_yield_for_tile(&map, cx, cy - 1, best.job);
    const int weight = best.score / (yld * 8 + 6);
    if (weight < 1 || best.score != weight * (yld * 8 + 6)) {
      fprintf(stderr,
        "unit_ai_euro_28c8_job_score: score=%d is not a cargo-weight multiple of "
        "yld*8+6 (job=%d cargo=%d yld=%d)\n", best.score, best.job, cargo, yld);
      return fail("distance_term: score has no (7-|dx|-|dy|) term");
    }
  }
  return 0;
}

/*
 * Independent transcription of raw 13012-13130's cargo-weight branch, over
 * every (tile, job) pair in the port's own iteration order (tile ascending,
 * job ascending, strict '>' so the first-seen max wins ties). `human` is
 * DOS's bVar1; no COL1 save is bound, so the price row, the Indian-claim
 * term and the Ore wealth-rank bonus are all 0 and the shortfall/unmet
 * ledgers are empty (pop 1 has no demand beyond food, which is covered).
 */
static void recompute_expected(
  const ColonizeWorldMap* map,
  const ColonizeColony* col,
  int current_job,
  int human,
  AiEuro28c8JobCandidate* out_best
) {
  const int capacity = ((int)col->warehouse_level + 1) * 100;
  out_best->job = -1;
  out_best->tile = -1;
  out_best->score = 0;
  for (int step = 0; step < COLONIZE_COLONY_FIELD_TILES; ++step) {
    const int ti = colonies_field_scan_order(step); /* DS:0xc8/0xde order, #584 */
    int dx = 0, dy = 0;
    colonies_field_tile_delta(ti, &dx, &dy);
    const int tx = col->x + dx;
    const int ty = col->y + dy;
    for (int job = 0; job < COLONIZE_FIELD_JOB_COUNT; ++job) {
      int yld = colony_yield_for_tile(map, tx, ty, job);
      if (yld <= 0) {
        continue;
      }
      const int cargo = job; /* raw 13000/13006 — local_24 is the JOB, #582 */
      int room = capacity - col->stock[cargo];
      if (room < 1) {
        room = 1;
      }
      if (yld > room) {
        yld = room;
      }
      int score = yld * 8 + (7 - (dx < 0 ? -dx : dx) - (dy < 0 ? -dy : dy));
      if (human && job == current_job) {
        score <<= 1;
      }
      int w4 = 0;
      if (job == COLONIZE_JOB_FARMER || job == COLONIZE_JOB_FISHERMAN) {
        w4 = human ? 1 : 0;
      }
      int m = w4 + 1;
      /* DS:0x2b6 consumer job: sugar/tobacco/cotton/furs/ore have one. */
      if (job == 1 || job == 2 || job == 3 || job == 4 || job == COLONIZE_JOB_ORE_MINER) {
        m += 1;
      }
      score = (m + w4) * score;
      if (score > out_best->score) {
        out_best->score = score;
        out_best->job = job;
        out_best->tile = ti;
      }
    }
  }
}

/*
 * Scenario 2 — full 8-tile matrix, one pedia terrain class 0..7 per tile,
 * run twice: once as an AI colony (no sticky doubling, jobs 0/8 weight 0)
 * and once as the human's own colony (sticky x2 on the colonist's current
 * job, jobs 0/8 weight 1). Proves the sticky term is bVar1-gated, which the
 * port used to apply unconditionally.
 */
static int unit_full_matrix_sticky_doubling(void) {
  uint8_t terrain[MAP_W * MAP_H];
  uint8_t layer2[MAP_W * MAP_H];
  uint8_t layer3[MAP_W * MAP_H];
  ColonizeWorldMap map;
  map_init(&map, terrain, layer2, layer3);

  const int cx = 8;
  const int cy = 8;
  static const int dx[COLONIZE_COLONY_FIELD_TILES] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[COLONIZE_COLONY_FIELD_TILES] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    terrain[(cy + dy[ti]) * MAP_W + (cx + dx[ti])] = (uint8_t)ti;
  }
  suppress_field_tile_resources(&map, cx, cy);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  colony_init_common(&colonies.colonies[0], /*nation=*/1, cx, cy);
  colonies.colonies[0].colonists[0].field_job = COLONIZE_JOB_ORE_MINER; /* sticky */

  for (int human = 0; human < 2; ++human) {
    ColonizeTurnContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.human_nation = human ? 1 : 0; /* colony nation is 1 */
    ctx.colonies = &colonies;
    ctx.map = &map;

    AiEuro28c8JobCandidate expected;
    recompute_expected(&map, &colonies.colonies[0], COLONIZE_JOB_ORE_MINER, human, &expected);

    AiEuro28c8JobCandidate best;
    int ok = ai_euro_28c8_colonist_job_score_structural(&ctx, 0, 0, &best);
    if (!ok) {
      return fail("full_matrix: no assignment found");
    }
    if (best.job != expected.job || best.tile != expected.tile ||
        best.score != expected.score) {
      fprintf(stderr,
        "unit_ai_euro_28c8_job_score: human=%d got job=%d tile=%d score=%d "
        "want job=%d tile=%d score=%d\n",
        human, best.job, best.tile, best.score, expected.job, expected.tile,
        expected.score);
      return fail("full_matrix: port output doesn't match the 28c8 transcription");
    }
  }
  return 0;
}

/*
 * bugs.md #562 — the join path. DOS FUN_15eb_3930 -> FUN_15eb_2ea0 ->
 * FUN_15eb_28c8 seats a newly admitted colonist on the best-scoring WORK
 * PLOT (fallback Carpenter, raw 13189-13192); the port used to park every
 * joiner in the Town Hall. One Prairie plot next to the colony, so the
 * newcomer must end up on tile 0 with a field job, not in a building.
 */
static int unit_join_seats_on_work_plot(void) {
  uint8_t terrain[MAP_W * MAP_H];
  uint8_t layer2[MAP_W * MAP_H];
  uint8_t layer3[MAP_W * MAP_H];
  ColonizeWorldMap map;
  map_init(&map, terrain, layer2, layer3);

  const int cx = 8;
  const int cy = 8;
  terrain[(cy - 1) * MAP_W + cx] = 3; /* tile 0 (N): Prairie */
  suppress_field_tile_resources(&map, cx, cy);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(&map);
  colony_init_common(&colonies.colonies[0], /*nation=*/1, cx, cy);
  colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 50;
  colonies.colony_count = 1;

  colonies_seat_new_colonist(&colonies, 0, 0);
  colonies_set_occupancy_map(NULL);

  const ColonizeColony* col = &colonies.colonies[0];
  if (col->colonists[0].field_job < 0 || col->colonists[0].building_type >= 0) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: joiner field_job=%d building=%d\n",
            col->colonists[0].field_job, col->colonists[0].building_type);
    return fail("join_seats: newcomer did not take a work plot");
  }
  if (colonies_colonist_tile(col, 0) != 0) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: joiner tile=%d want=0\n",
            colonies_colonist_tile(col, 0));
    return fail("join_seats: newcomer took the wrong plot");
  }
  return 0;
}

/*
 * bugs.md #582 — the Fisherman's warehouse-room clamp reads cargo slot 8
 * (HORSES), not FOOD. DOS raw 13000 calls `FUN_15eb_18ec(x,y,&local_24,0)`
 * and the fish->food remap at raw 11983 only fires for param_4 != 0, so
 * local_24 stays 8 and raw 13006 clamps against `colony+0x9a+8*2`.
 *
 * All eight field tiles are Ocean and the warehouse holds a full 100 FOOD
 * with 0 HORSES: DOS leaves the fish yield alone (room = 100 - 0), while the
 * old port clamped it to 1 because it indexed FOOD. Scored as an AI colony
 * with the food emergency off (shortfall 2 * 0x10 < stock 100, raw 12978).
 */
static int unit_fisherman_clamps_against_horses(void) {
  uint8_t terrain[MAP_W * MAP_H];
  uint8_t layer2[MAP_W * MAP_H];
  uint8_t layer3[MAP_W * MAP_H];
  ColonizeWorldMap map;
  map_init(&map, terrain, layer2, layer3);

  const int cx = 8;
  const int cy = 8;
  static const int dx[COLONIZE_COLONY_FIELD_TILES] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[COLONIZE_COLONY_FIELD_TILES] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    const int off = (cy + dy[ti]) * MAP_W + (cx + dx[ti]);
    terrain[off] = 0x19; /* Ocean */
    layer3[off] = 1;     /* low nibble 1 = sea, not a lake */
  }
  suppress_field_tile_resources(&map, cx, cy);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  colony_init_common(&colonies.colonies[0], /*nation=*/1, cx, cy);
  /* The 18ec Fisherman gate (raw 11955) is unconditional, so the colony needs
   * real Docks for a water plot to score at all (bugs.md #609). */
  colonies.building_type_count = 1;
  snprintf(
    colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Docks"
  );
  colonies.colonies[0].has_building[0] = true;
  colonies.colonies[0].stock[COLONIZE_CARGO_FOOD] = 100;  /* at capacity */
  colonies.colonies[0].stock[COLONIZE_CARGO_HORSES] = 0;  /* room to spare */

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0; /* colony nation 1 -> AI shape */
  ctx.colonies = &colonies;
  ctx.map = &map;

  AiEuro28c8JobCandidate best;
  if (!ai_euro_28c8_colonist_job_score_structural(&ctx, 0, 0, &best)) {
    return fail("fisherman_clamp: no assignment found");
  }
  if (best.job != COLONIZE_JOB_FISHERMAN) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: job=%d want=%d\n",
            best.job, COLONIZE_JOB_FISHERMAN);
    return fail("fisherman_clamp: an all-Ocean ring elected a land job");
  }
  const int yld = colony_yield_for_tile(&map, cx, cy - 1, COLONIZE_JOB_FISHERMAN);
  if (yld < 2) {
    return fail("fisherman_clamp: fixture Ocean yield is too small to detect a clamp");
  }
  /* w4 = 0 (AI, no tick), m = 1, no consumer chain: score = yld*8 + 6. */
  const int want = yld * 8 + 6;
  if (best.score != want) {
    fprintf(stderr, "unit_ai_euro_28c8_job_score: score=%d want=%d (yld=%d)\n",
            best.score, want, yld);
    return fail("fisherman_clamp: fish yield was clamped against FOOD, not HORSES");
  }
  return 0;
}

/*
 * bugs.md #609 — the 18ec Docks gate (raw 11955) is unconditional, and the
 * structural entry scores with `profession < 0`. The same all-Ocean ring with
 * the Docks bit cleared must yield nothing at all, i.e. no assignment.
 */
static int unit_no_docks_scores_no_water_plot(void) {
  uint8_t terrain[MAP_W * MAP_H];
  uint8_t layer2[MAP_W * MAP_H];
  uint8_t layer3[MAP_W * MAP_H];
  ColonizeWorldMap map;
  map_init(&map, terrain, layer2, layer3);

  const int cx = 8;
  const int cy = 8;
  static const int dx[COLONIZE_COLONY_FIELD_TILES] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[COLONIZE_COLONY_FIELD_TILES] = {-1, -1, 0, 1, 1, 1, 0, -1};
  for (int ti = 0; ti < COLONIZE_COLONY_FIELD_TILES; ++ti) {
    const int off = (cy + dy[ti]) * MAP_W + (cx + dx[ti]);
    terrain[off] = 0x19; /* Ocean */
    layer3[off] = 1;
  }
  suppress_field_tile_resources(&map, cx, cy);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  colony_init_common(&colonies.colonies[0], /*nation=*/1, cx, cy);
  colonies.building_type_count = 1;
  snprintf(
    colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Docks"
  );
  colonies.colonies[0].has_building[0] = false; /* no Docks */

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0;
  ctx.colonies = &colonies;
  ctx.map = &map;

  AiEuro28c8JobCandidate best;
  if (ai_euro_28c8_colonist_job_score_structural(&ctx, 0, 0, &best)) {
    return fail("#609: a dockless colony scored a water plot");
  }
  return 0;
}

static const TestCase k_cases[] = {
    {"unit_distance_term_breaks_ties", unit_distance_term_breaks_ties},
    {"unit_full_matrix_sticky_doubling", unit_full_matrix_sticky_doubling},
    {"unit_join_seats_on_work_plot", unit_join_seats_on_work_plot},
    {"unit_fisherman_clamps_against_horses", unit_fisherman_clamps_against_horses},
    {"unit_no_docks_scores_no_water_plot", unit_no_docks_scores_no_water_plot},
};

TEST_MAIN(k_cases)
