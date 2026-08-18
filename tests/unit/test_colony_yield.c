#include "core/colony.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/map.h"

#include <stdio.h>
#include <string.h>

/*
 * Town-commons secondary is base_for_pedia(job) + river(0/1/2) + SoL latch
 * bits (+1 SOL_50, +1 SOL_100), asm-confirmed against FUN_15eb_1f72
 * (viceroy_unpacked.c ~12474) and player-confirmed via two real captures
 * with zero free parameters (Curacao/Paramaribo, colony_yield_town_commons's
 * own comment) — see docs/terrain_yields.md "Town commons". No plow, no
 * flat road (an earlier reading of this file's own fixtures assumed both;
 * superseded 2026-08-18).
 */

static int find_resource_tile(
  ColonizeWorldMap* map,
  uint8_t terrain,
  int want_res,
  int* out_x,
  int* out_y
) {
  for (int y = 0; y < map->height; ++y) {
    for (int x = 0; x < map->width; ++x) {
      map->terrain[y * map->width + x] = terrain;
      if (map_resource_type_for_yield(map, x, y) == want_res) {
        *out_x = x;
        *out_y = y;
        return 1;
      }
    }
  }
  return 0;
}

static int check_commons(
  ColonizeWorldMap* map,
  int x,
  int y,
  int expect_food,
  int expect_cargo,
  int expect_amt,
  const char* label
) {
  ColonizeTownCommonsYield tc;
  colony_yield_town_commons(map, x, y, 0, 0, &tc);
  if (tc.food != expect_food || tc.secondary_cargo != expect_cargo ||
      tc.secondary_amount != expect_amt) {
    fprintf(
      stderr,
      "town commons %s: expected food=%d cargo=%d amt=%d got food=%d cargo=%d amt=%d\n",
      label,
      expect_food,
      expect_cargo,
      expect_amt,
      tc.food,
      tc.secondary_cargo,
      tc.secondary_amount
    );
    return 1;
  }
  return 0;
}

/* Same as check_commons, but with a colony_flags param (SoL latch bits)
 * and no food check — isolates the secondary-amount SoL-latch behavior. */
static int check_commons_sol(
  ColonizeWorldMap* map,
  int x,
  int y,
  int expect_cargo,
  int expect_amt,
  uint8_t colony_flags,
  const char* label
) {
  ColonizeTownCommonsYield tc;
  colony_yield_town_commons(map, x, y, 0, colony_flags, &tc);
  if (tc.secondary_cargo != expect_cargo || tc.secondary_amount != expect_amt) {
    fprintf(
      stderr,
      "town commons %s: expected cargo=%d amt=%d got cargo=%d amt=%d\n",
      label,
      expect_cargo,
      expect_amt,
      tc.secondary_cargo,
      tc.secondary_amount
    );
    return 1;
  }
  return 0;
}

int main(void) {
  char err[256];
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!map_alloc(&map, 32, 32, err, sizeof(err))) {
    fprintf(stderr, "map_alloc failed: %s\n", err);
    return 1;
  }

  /*
   * Town-commons food is a flat +2 regardless of terrain (plus
   * plow/river/resource on top) — golden_colony_prod01 (a real single DOS
   * turn across 14 Dutch colonies) rules out a per-terrain "cleared-parent
   * Farmer + 2" formula: it over-produced food by 1-4 in nearly every
   * colony. See colony_yield_town_commons_food_base's comment.
   */

  /* Scrub forest (pedia 9) — no special / river / latch. secondary base=2. */
  map.terrain[0] = 9;
  if (check_commons(
        &map,
        0,
        0,
        2,
        COLONIZE_CARGO_FURS,
        2,
        "scrub"
      )) {
    map_free(&map);
    return 1;
  }

  /* Hills (bit 0x20). */
  map.terrain[1] = (uint8_t)(0x20u);
  if (map_pedia_terrain_index_at(&map, 1, 0) != 28) {
    fprintf(stderr, "hills pedia expected 28 got %d\n", map_pedia_terrain_index_at(&map, 1, 0));
    map_free(&map);
    return 1;
  }
  /* amt=6: base(Hills,Ore)=4, +2 from a coincidental Prime Ore hash
   * match at (1,0) with the default seed (100, this synthetic map never
   * sets prime_resource_seed) — not something this fixture set out to
   * test, just a side effect of the 2026-08-18 coordinate-hash fix on this
   * exact coordinate. */
  if (check_commons(&map, 1, 0, 2, COLONIZE_CARGO_ORE, 6, "hills")) {
    map_free(&map);
    return 1;
  }

  /*
   * SoL latch bits on town-commons secondary — asm-confirmed 2026-08-18
   * against FUN_15eb_1f72 (viceroy_unpacked.c ~12474): +1 if
   * COLONIZE_COLONY_FLAG_SOL_50 is set, +1 if _SOL_100 is set (up to +2
   * total). Player-confirmed 2026-08-18 by two real, zero-free-parameter
   * captures (see colony_yield_town_commons's own comment): Curacao
   * (golden_colony_prod02, town commons its only furs source, flat ground,
   * full latch) and Paramaribo (golden_colony_prod01, town commons its
   * only sugar source net of its Rum Distiller's consumption, full latch).
   * Reuses the Hills tile above (base 4, +2 Prime Ore already covered).
   */
  if (check_commons_sol(&map, 1, 0, COLONIZE_CARGO_ORE, 6, 0, "hills, no SoL latch")) {
    map_free(&map);
    return 1;
  }
  if (check_commons_sol(
        &map, 1, 0, COLONIZE_CARGO_ORE, 7, COLONIZE_COLONY_FLAG_SOL_50, "hills, SOL_50 latch"
      )) {
    map_free(&map);
    return 1;
  }
  if (check_commons_sol(
        &map,
        1,
        0,
        COLONIZE_CARGO_ORE,
        8,
        (uint8_t)(COLONIZE_COLONY_FLAG_SOL_50 | COLONIZE_COLONY_FLAG_SOL_100),
        "hills, SOL_50+SOL_100 latch"
      )) {
    map_free(&map);
    return 1;
  }

  /* Broadleaf forest (pedia 11). base(2), no river/latch. */
  map.terrain[2] = 11;
  if (check_commons(&map, 2, 0, 2, COLONIZE_CARGO_FURS, 2, "broadleaf")) {
    map_free(&map);
    return 1;
  }

  /* Prairie (3) + minor river (0x40). */
  map.terrain[3] = (uint8_t)(3u | 0x40u);
  if (!map_tile_has_river(&map, 3, 0) || map_tile_has_major_river(&map, 3, 0)) {
    fprintf(stderr, "prairie tile should be minor river only\n");
    map_free(&map);
    return 1;
  }
  /* amt=4: base(Prairie,Cotton)=3 +1 river(minor). */
  if (check_commons(&map, 3, 0, 3, COLONIZE_CARGO_COTTON, 4, "prairie+minor river")) {
    map_free(&map);
    return 1;
  }

  /* Broadleaf + Game (resource type 9). Find a procedural hit. */
  {
    int gx = -1;
    int gy = -1;
    if (!find_resource_tile(&map, 11, 9, &gx, &gy)) {
      fprintf(stderr, "no broadleaf+Game procedural tile found on 32x32\n");
      map_free(&map);
      return 1;
    }
    /* amt=4: base(Broadleaf,Fur)=2 + Game(+2). */
    if (check_commons(&map, gx, gy, 4, COLONIZE_CARGO_FURS, 4, "broadleaf+Game")) {
      map_free(&map);
      return 1;
    }
    /* Settlement bit hides sprites — a colony's own town square always
     * carries this bit, so town-commons food/secondary correctly drop the
     * Game bonus once it's set (colony_yield_town_commons intentionally
     * uses the settlement-hiding resource lookup, unlike field yields). */
    map.layer2[gy * map.width + gx] = (uint8_t)(map.layer2[gy * map.width + gx] | 2u);
    if (map_resource_type_at(&map, gx, gy) >= 0) {
      fprintf(stderr, "settlement bit should hide resource sprite lookup\n");
      map_free(&map);
      return 1;
    }
    if (map_resource_type_for_yield(&map, gx, gy) != 9) {
      fprintf(stderr, "yield lookup should still see Game under settlement bit\n");
      map_free(&map);
      return 1;
    }
    /* amt=2: base(Broadleaf,Fur)=2, Game hidden. */
    if (check_commons(
          &map,
          gx,
          gy,
          2,
          COLONIZE_CARGO_FURS,
          2,
          "broadleaf+Game (settlement bit)"
        )) {
      map_free(&map);
      return 1;
    }
  }

  /* Field river: prairie cotton +1 with minor river. */
  {
    map.terrain[5] = 3;
    const int cotton_dry = colony_yield_for_tile(&map, 5, 0, COLONIZE_JOB_COTTON_PLANTER);
    map.terrain[5] = (uint8_t)(3u | 0x40u);
    const int cotton_river = colony_yield_for_tile(&map, 5, 0, COLONIZE_JOB_COTTON_PLANTER);
    if (cotton_dry != 3 || cotton_river != 4) {
      fprintf(
        stderr,
        "field prairie cotton dry=%d river=%d expected 3/4\n",
        cotton_dry,
        cotton_river
      );
      map_free(&map);
      return 1;
    }
  }

  /*
   * Fisherman + major river, player-confirmed 2026-08-15 (Viceroy
   * difficulty): Lake with a major river, free colonist, no sentiment
   * bonus = 6 food. Ocean base fish 3, +1 coastal distance mod (few ocean
   * neighbors, matching the "sometimes 6" coastal observation), +2 major
   * river (base 1 × 2, same bucket as Farmer/Ore/Silver) = 6. Previously
   * colony_yield_river_bonus's `default: return 0` silently dropped
   * Fisherman from any river bonus — this is the regression check for that
   * fix (colony_yield.c). Uses colony_yield_for_tile (job-only, matches
   * this test binary's link set) rather than colony_yield_for_worker — no
   * profession/docks gating needed since a free colonist has no skill-match
   * bonus and this check is about the river term specifically.
   */
  {
    const int fx = 20;
    const int fy = 20;
    map.terrain[fy * map.width + fx] = (uint8_t)(25u | 0x40u | 0x80u); /* Ocean, major river */
    if (!map_tile_has_river(&map, fx, fy) || !map_tile_has_major_river(&map, fx, fy)) {
      fprintf(stderr, "fisherman tile should be major river ocean\n");
      map_free(&map);
      return 1;
    }
    const int fish = colony_yield_for_tile(&map, fx, fy, COLONIZE_JOB_FISHERMAN);
    if (fish != 6) {
      fprintf(stderr, "fisherman+major river want 6 got %d\n", fish);
      map_free(&map);
      return 1;
    }
  }

  /*
   * Expert Ore Miner on Hills+road+sentiment. This regression check still
   * covers (a) a positive sol_bonus folding in *before* expert doubling,
   * not as a flat add after, and (b) the road/river unit size doubling for
   * a matching non-food/fish expert (colony_yield_pipeline, colony_yield.c):
   *   free:   base(4) +sol(1)=5,                +road(u=1)=6
   *   expert: base(4) +sol(1)=5, <<=1(expert)=10, +road(u=2)=12
   * Hills Ore base=4 — player-confirmed 2026-08-18 via colony_prod02's Fort
   * Orange: expert Ore Miner, Hills, sentiment +2, no road/river/resource,
   * single colonist (no confound) -> 12 ore = (4+2)x2, and its paired
   * non-specialist Blacksmith's Shop -> 8 tools/8 ore, independently
   * confirming the manufacturing side. A base=3 reading was tried after
   * golden_colony_prod01's synthetic Bahia fixture seemed to need it, but
   * Bahia's terrain there is entirely hand-fabricated (not loaded from a
   * real save) and carried an unconfirmed "+road" guess; base=3 only
   * "worked" by coincidentally cancelling that guess's error. Fixed:
   * Bahia's road flag dropped instead (see test_colony_prod01.c).
   */
  {
    const int hx = 21;
    const int hy = 20;
    map.terrain[hy * map.width + hx] = (uint8_t)(0x20u); /* Hills, no forest/river */
    map_tile_set_road(&map, hx, hy, true);
    const int free_ore = colony_yield_for_worker(
      &map, hx, hy, COLONIZE_JOB_ORE_MINER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 1
    );
    if (free_ore != 6) {
      fprintf(stderr, "free colonist ore+road+sol want 6 got %d\n", free_ore);
      map_free(&map);
      return 1;
    }
    const int expert_ore = colony_yield_for_worker(
      &map, hx, hy, COLONIZE_JOB_ORE_MINER, COLONIZE_JOB_ORE_MINER, /*has_docks=*/true, 1
    );
    if (expert_ore != 12) {
      fprintf(stderr, "expert ore miner+road+sol want 12 got %d\n", expert_ore);
      map_free(&map);
      return 1;
    }
  }

  /*
   * Expert Fur Trapper on Mixed Forest+road+sentiment, player-confirmed
   * 2026-08-15 (Viceroy): 28 furs with Henry Hudson owned, vs. 14 for a
   * Free Colonist — vs. 12/24 the port would have given before this fix.
   * Ruled out a special resource explaining the gap (player-confirmed
   * none present); solved instead to fur/lumber's road bonus needing the
   * same base-2 magnitude bucket river already has (was flat 1 for every
   * road job). This checks that piece alone, via colony_yield_for_worker
   * (job-only pipeline, no Hudson — Hudson's x2 is an external post-hoc
   * step in turn.c/colony_preview.c, not part of colony_yield_pipeline):
   *   free:   base(3) +sol(2)=5,                +road(u=1,base=2)=7
   *   expert: base(3) +sol(2)=5, <<=1(expert)=10, +road(u=2,base=2)=14
   * (x Hudson's external x2 separately gives the full 14/28 the player
   * observed — not re-tested here, that multiply is already covered by
   * the existing "Henry Hudson" tests in test_turn.c.)
   */
  {
    /* Resources are procedurally derived from (terrain, x, y), not stored
     * data — scan for a resource-free Mixed forest cell rather than assume
     * a fixed coordinate has none (an earlier fixed pick landed on one by
     * coincidence, inflating the result and catching this comment's own
     * claim of "no resource involved" out — good, that's what the scan is
     * for). */
    int mx = -1;
    int my = -1;
    for (int y = 0; y < (int)map.height && mx < 0; ++y) {
      for (int x = 0; x < (int)map.width && mx < 0; ++x) {
        map.terrain[y * map.width + x] = 10; /* Mixed forest, pedia 8+2, no river */
        if (map_resource_type_for_yield(&map, x, y) < 0) {
          mx = x;
          my = y;
        }
      }
    }
    if (mx < 0) {
      fprintf(stderr, "no resource-free Mixed forest tile found on 32x32\n");
      map_free(&map);
      return 1;
    }
    map_tile_set_road(&map, mx, my, true);
    const int free_fur = colony_yield_for_worker(
      &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 2
    );
    if (free_fur != 7) {
      fprintf(stderr, "free colonist fur+road+sol want 7 got %d\n", free_fur);
      map_free(&map);
      return 1;
    }
    const int expert_fur = colony_yield_for_worker(
      &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_JOB_FUR_TRAPPER, /*has_docks=*/true, 2
    );
    if (expert_fur != 14) {
      fprintf(stderr, "expert fur trapper+road+sol want 14 got %d\n", expert_fur);
      map_free(&map);
      return 1;
    }
  }

  /*
   * Expert doubling applies to the whole accumulated base (terrain +
   * resource effect together), same as every other field expert — no
   * special-cased resource-only doubling for Farmer. A flat "+2 instead
   * of x2 for food/fish experts" variant was tried and regressed
   * golden_colony_prod01 (see colony_yield_pipeline), so plain x2 stands.
   *   free:   base(1) +farmer(+1, non-expert, asm-confirmed 2026-08-18,
   *            see colony_yield_pipeline) +resource(free,+2)  = 4
   *   expert: (base(1)+resource(+2)) x2  (farmer's +1 doesn't apply —
   *            expert Farmers skip this block entirely)          = 6
   */
  {
    int gx = -1;
    int gy = -1;
    if (!find_resource_tile(&map, 11, 9, &gx, &gy)) {
      fprintf(stderr, "no broadleaf+Game procedural tile found for expert-resource test\n");
      map_free(&map);
      return 1;
    }
    const int free_game = colony_yield_for_worker(
      &map, gx, gy, COLONIZE_JOB_FARMER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0
    );
    if (free_game != 4) {
      fprintf(stderr, "free colonist farmer+Game want 4 got %d\n", free_game);
      map_free(&map);
      return 1;
    }
    const int expert_game = colony_yield_for_worker(
      &map, gx, gy, COLONIZE_JOB_FARMER, COLONIZE_JOB_FARMER, /*has_docks=*/true, 0
    );
    if (expert_game != 6) {
      fprintf(stderr, "expert farmer+Game want 6 got %d\n", expert_game);
      map_free(&map);
      return 1;
    }
  }

  map_free(&map);
  printf("colony_yield town commons tests ok\n");
  return 0;
}
