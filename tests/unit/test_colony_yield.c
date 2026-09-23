#include "core/colony.h"
#include "core/colony_production.h"
#include "core/colony_yield.h"
#include "core/map.h"

#include <stdio.h>
#include <string.h>

#include "../common/test_runner.h"

/*
 * Town-commons secondary is base_for_pedia(job) + river(0/1/2) + SoL latch
 * bits (+1 SOL_50, +1 SOL_100), asm-confirmed against FUN_15eb_1f72
 * (viceroy_unpacked.c ~12474) and player-confirmed via two real captures
 * with zero free parameters (Curacao/Paramaribo, colony_yield_town_commons's
 * own comment) — see docs/terrain_yields.md "Town commons". No plow, no
 * flat road (an earlier reading of this file's own fixtures assumed both;
 * superseded 2026-08-18).
 *
 * Every case below allocates its own fresh 32x32 map rather than sharing
 * one across cases: several original blocks reused a tile another block
 * had just set up (e.g. the SoL-latch checks reuse the "Hills" tile from
 * the preceding block), which would be an order dependency once split into
 * independent cases. Each case now sets up whatever terrain/tiles it needs
 * on its own fresh map instead.
 */

static int map_new(ColonizeWorldMap* map) {
  char err[256];
  memset(map, 0, sizeof(*map));
  if (!map_alloc(map, 32, 32, err, sizeof(err))) {
    fprintf(stderr, "map_alloc failed: %s\n", err);
    return 1;
  }
  return 0;
}

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

/*
 * One town-commons assertion (audit TT-24: this and check_commons_sol were
 * the same body twice). colony_flags carries the SoL latch bits;
 * expect_food < 0 skips the food check, which is how the SoL-latch cases
 * isolate the secondary amount.
 */
static int check_commons_flags(
  ColonizeWorldMap* map,
  int x,
  int y,
  int expect_food,
  int expect_cargo,
  int expect_amt,
  uint8_t colony_flags,
  const char* label
) {
  ColonizeTownCommonsYield tc;
  colony_yield_town_commons(map, x, y, colony_flags, 2, &tc);
  const bool food_bad = expect_food >= 0 && tc.food != expect_food;
  if (food_bad || tc.secondary_cargo != expect_cargo || tc.secondary_amount != expect_amt) {
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

#define check_commons(map, x, y, food, cargo, amt, label) \
  check_commons_flags((map), (x), (y), (food), (cargo), (amt), 0, (label))
#define check_commons_sol(map, x, y, cargo, amt, flags, label) \
  check_commons_flags((map), (x), (y), -1, (cargo), (amt), (flags), (label))

/*
 * Town-commons food is the DOS 4-way pedia-class split (FUN_15eb_1f72 raw
 * 12506-12518: class 24 -> 0; 1/9/17 -> 1; 8..23 and 27/28 -> 2; everything
 * else -> 3), plus difficulty/plow/river/resource on top — see
 * colony_yield_town_commons_food_base. It is NOT a flat +2, and it is not
 * the "cleared-parent Farmer + 2" formula golden_colony_prod01 ruled out
 * (that one over-produced food by 1-4 in nearly every colony). The
 * per-case comments below name the class each fixture lands in.
 */

/* Scrub forest (pedia 9) — food class 1 (Desert/Scrub special case, see
 * colony_yield_town_commons_food_base), no special/river/latch. */
static int case_commons_scrub(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  map.terrain[0] = 9;
  const int rc = check_commons(&map, 0, 0, 1, COLONIZE_CARGO_FURS, 2, "scrub");
  map_free(&map);
  return rc;
}

/* Hills (bit 0x20). */
static int case_commons_hills_base(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
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
  const int rc = check_commons(&map, 1, 0, 2, COLONIZE_CARGO_ORE, 6, "hills");
  map_free(&map);
  return rc;
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
 * Reuses the Hills tile shape from case_commons_hills_base (base 4, +2
 * Prime Ore already covered there).
 */
static int case_commons_hills_sol_latch(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  map.terrain[1] = (uint8_t)(0x20u);
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
  map_free(&map);
  return 0;
}

/* Broadleaf forest (pedia 11). base(2), no river/latch. */
static int case_commons_broadleaf(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  map.terrain[2] = 11;
  const int rc = check_commons(&map, 2, 0, 2, COLONIZE_CARGO_FURS, 2, "broadleaf");
  map_free(&map);
  return rc;
}

/* Prairie (3) + minor river (0x40). */
static int case_commons_prairie_minor_river(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  map.terrain[3] = (uint8_t)(3u | 0x40u);
  if (!map_tile_has_river(&map, 3, 0) || map_tile_has_major_river(&map, 3, 0)) {
    fprintf(stderr, "prairie tile should be minor river only\n");
    map_free(&map);
    return 1;
  }
  /* amt=4: base(Prairie,Cotton)=3 +1 river(minor). Food stays 3 — commons
   * food has NO river term (FUN_15eb_1f72 reads only the runtime plow bit;
   * the river value feeds the secondary alone — see colony_yield.c,
   * 2026-09-03, the Fort Orange plow+river double-count). */
  const int rc = check_commons(&map, 3, 0, 3, COLONIZE_CARGO_COTTON, 4, "prairie+minor river");
  map_free(&map);
  return rc;
}

/* Broadleaf + Game (resource type 9). Find a procedural hit. */
static int case_commons_broadleaf_game(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
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
  /* Settlement bit hides the resource *sprite*, but NOT its yield — a
   * colony's own town square always carries this bit, and DOS's
   * FUN_15eb_1f72 resource read (FUN_137f_04b0) has no settlement gate.
   * Player-confirmed 2026-09-03 (farming saves / golden_colony_prod03):
   * a Swamp town square with Minerals makes 5 ore per turn. So commons
   * food/secondary keep the Game bonus under the settlement bit. */
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
  /* amt=4: base(Broadleaf,Fur)=2 + Game(+2), unchanged by settlement bit. */
  const int rc = check_commons(&map, gx, gy, 4, COLONIZE_CARGO_FURS, 4,
    "broadleaf+Game (settlement bit)");
  map_free(&map);
  return rc;
}

/* Field river: prairie cotton +1 with minor river. */
static int case_field_river_cotton(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  map.terrain[5] = 3;
  const int cotton_dry = colony_yield_for_tile(&map, 5, 0, COLONIZE_JOB_COTTON_PLANTER);
  map.terrain[5] = (uint8_t)(3u | 0x40u);
  const int cotton_river = colony_yield_for_tile(&map, 5, 0, COLONIZE_JOB_COTTON_PLANTER);
  map_free(&map);
  if (cotton_dry != 3 || cotton_river != 4) {
    fprintf(
      stderr,
      "field prairie cotton dry=%d river=%d expected 3/4\n",
      cotton_dry,
      cotton_river
    );
    return 1;
  }
  return 0;
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
static int case_fisherman_major_river(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
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
  /* bugs.md #883: Fisherman falls in the `7 < job` arm of the Convert +1
   * whitelist (FUN_15eb_18ec raw 11973-11979) — Convert gets +1 over free. */
  const int free_fish = colony_yield_for_worker(
    &map, fx, fy, COLONIZE_JOB_FISHERMAN, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    false
  );
  const int convert_fish = colony_yield_for_worker(
    &map, fx, fy, COLONIZE_JOB_FISHERMAN, COLONIZE_PROF_CONVERT, /*has_docks=*/true, 0, 0, false
  );
  map_free(&map);
  if (convert_fish != free_fish + 1) {
    fprintf(
      stderr, "Convert fisherman want free+1 (free=%d) got %d\n", free_fish, convert_fish
    );
    return 1;
  }
  return 0;
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
static int case_expert_ore_miner_hills_road_sol(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  const int hx = 21;
  const int hy = 20;
  map.terrain[hy * map.width + hx] = (uint8_t)(0x20u); /* Hills, no forest/river */
  map_tile_set_road(&map, hx, hy, true);
  const int free_ore = colony_yield_for_worker(
    &map, hx, hy, COLONIZE_JOB_ORE_MINER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 1, 0,
    false
  );
  if (free_ore != 6) {
    fprintf(stderr, "free colonist ore+road+sol want 6 got %d\n", free_ore);
    map_free(&map);
    return 1;
  }
  const int expert_ore = colony_yield_for_worker(
    &map, hx, hy, COLONIZE_JOB_ORE_MINER, COLONIZE_JOB_ORE_MINER, /*has_docks=*/true, 1, 0,
    false
  );
  if (expert_ore != 12) {
    fprintf(stderr, "expert ore miner+road+sol want 12 got %d\n", expert_ore);
    map_free(&map);
    return 1;
  }
  /* bugs.md #883: Ore Miner is excluded from the Convert +1 whitelist
   * (FUN_15eb_18ec raw 11973-11979) — Convert gets no bonus over free. */
  const int convert_ore = colony_yield_for_worker(
    &map, hx, hy, COLONIZE_JOB_ORE_MINER, COLONIZE_PROF_CONVERT, /*has_docks=*/true, 1, 0,
    false
  );
  map_free(&map);
  if (convert_ore != free_ore) {
    fprintf(
      stderr, "Convert ore miner+road+sol want no +1 (== free %d) got %d\n", free_ore, convert_ore
    );
    return 1;
  }
  return 0;
}

/*
 * Expert Fur Trapper on Mixed Forest+road+sentiment, player-confirmed
 * 2026-08-15 (Viceroy): 28 furs with Henry Hudson owned, vs. 14 for a
 * Free Colonist — vs. 12/24 the port would have given before this fix.
 * Ruled out a special resource explaining the gap (player-confirmed
 * none present); solved instead to fur/lumber's road bonus needing the
 * same base-2 magnitude bucket river already has (was flat 1 for every
 * road job). This checks that piece alone, via colony_yield_for_worker:
 *   free:   base(3) +sol(2)=5,                +road(u=1,base=2)=7
 *   expert: base(3) +sol(2)=5, <<=1(expert)=10, +road(u=2,base=2)=14
 * Hudson's x2 is now the pipeline's own `has_hudson` step (DOS
 * FUN_15eb_18ec 11970-11973, smell audit #60), so the 14/28 the player
 * observed comes straight out of these same calls — asserted below.
 *
 * Kept as one case: every sub-assertion reuses the same resource-free
 * Mixed Forest tile the scan at the top finds, building up progressively
 * more elaborate calls (plain -> Hudson -> Convert -> Tory -> non-fur
 * control) rather than independent checks.
 */
static int case_expert_fur_trapper_hudson(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
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
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 2, 0,
    false
  );
  if (free_fur != 7) {
    fprintf(stderr, "free colonist fur+road+sol want 7 got %d\n", free_fur);
    map_free(&map);
    return 1;
  }
  const int expert_fur = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_JOB_FUR_TRAPPER, /*has_docks=*/true, 2, 0,
    false
  );
  if (expert_fur != 14) {
    fprintf(stderr, "expert fur trapper+road+sol want 14 got %d\n", expert_fur);
    map_free(&map);
    return 1;
  }

  /*
   * Henry Hudson (smell audit #60) — DOS doubles the Fur Trapper yield
   * INSIDE FUN_15eb_18ec (11970-11973), between the improvement stack and
   * both the Convert +1 and the negative-SoL subtraction. The port used to
   * apply it at four call sites *after* the whole pipeline, giving
   * `2·(base+1)` / `2·(base−2)` where DOS gives `2·base + 1` /
   * `2·base − 2`.
   *   plain:   7 → 14 and 14 → 28 (the 2026-08-15 player capture)
   *   convert: base 3 +fur-road 1 +stack road 1 = 5, ×2 = 10, +1 = 11
   *            (old post-hoc order: (5+1)×2 = 12)
   *   tory:    same 5, ×2 = 10, sol −2 = 8
   *            (old post-hoc order: (5−2)×2 = 6)
   */
  const int free_fur_hudson = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 2, 0,
    true
  );
  const int expert_fur_hudson = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_JOB_FUR_TRAPPER, /*has_docks=*/true, 2, 0,
    true
  );
  if (free_fur_hudson != 14 || expert_fur_hudson != 28) {
    fprintf(
      stderr,
      "Hudson fur+road+sol want 14/28 got %d/%d\n",
      free_fur_hudson,
      expert_fur_hudson
    );
    map_free(&map);
    return 1;
  }
  const int convert_fur_hudson = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_CONVERT, /*has_docks=*/true, 0, 0, true
  );
  if (convert_fur_hudson != 11) {
    fprintf(
      stderr,
      "Hudson+Convert fur want 11 (2*base+1) got %d\n",
      convert_fur_hudson
    );
    map_free(&map);
    return 1;
  }
  const int tory_fur_hudson = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, -2, 0,
    true
  );
  if (tory_fur_hudson != 8) {
    fprintf(
      stderr,
      "Hudson+negative SoL fur want 8 (2*base-2) got %d\n",
      tory_fur_hudson
    );
    map_free(&map);
    return 1;
  }
  /* Non-fur jobs must be untouched by the flag. */
  const int hudson_lumber = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    true
  );
  const int plain_lumber = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    false
  );
  map_free(&map);
  if (hudson_lumber != plain_lumber) {
    fprintf(
      stderr,
      "Hudson must not touch Lumberjack: %d vs %d\n",
      hudson_lumber,
      plain_lumber
    );
    return 1;
  }
  return 0;
}

/*
 * Expert Farmer gets flat +2 (not ×2) on skill match, plus the colony's
 * SoL latch bits re-added a second time (0 here, no colony context) —
 * asm-confirmed 2026-08-18, see colony_yield_pipeline. Its own resource
 * bonus is deferred past that step and doubled separately, matching the
 * real asm order (not "double the whole accumulated base").
 *   free:   base(1) +farmer(+1, unconditional) +resource(free,+2)      = 4
 *   expert: base(1) +flat(2) +resource(+2 x2 expert) +farmer(+1)       = 8
 * 2026-09-03: the farmer +1 applies to experts too (skill-blind
 * improvement stack, asm 15eb:1c32-1c40) — this exact tile shape is the
 * DOS-save-confirmed farming/case3 value (golden_colony_prod03).
 */
static int case_expert_farmer_game_resource(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int gx = -1;
  int gy = -1;
  if (!find_resource_tile(&map, 11, 9, &gx, &gy)) {
    fprintf(stderr, "no broadleaf+Game procedural tile found for expert-resource test\n");
    map_free(&map);
    return 1;
  }
  const int free_game = colony_yield_for_worker(
    &map, gx, gy, COLONIZE_JOB_FARMER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    false
  );
  if (free_game != 4) {
    fprintf(stderr, "free colonist farmer+Game want 4 got %d\n", free_game);
    map_free(&map);
    return 1;
  }
  const int expert_game = colony_yield_for_worker(
    &map, gx, gy, COLONIZE_JOB_FARMER, COLONIZE_JOB_FARMER, /*has_docks=*/true, 0, 0,
    false
  );
  map_free(&map);
  if (expert_game != 8) {
    fprintf(stderr, "expert farmer+Game want 8 got %d\n", expert_game);
    return 1;
  }
  return 0;
}

/*
 * Hills Farmer — DOS-save-confirmed 2026-09-03 (farming/case3 turn3:
 * expert Farmer on a bare Hill = 4 food; asserted here statically since
 * the player moved the farmer mid-pair, so no golden turn covers it).
 * Pins Hills farmer base back to NAMES.TXT's 1 — the old table 2 was
 * base 1 + the unconditional farmer +1 read into the base — and the
 * skill-blind farmer term: free colonist same tile = 2 (1 + farmer 1),
 * expert = 4 (1 + expert flat 2 + farmer 1).
 */
static int case_hills_farmer(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int hx = -1;
  int hy = -1;
  for (int y = 0; y < (int)map.height && hx < 0; ++y) {
    for (int x = 0; x < (int)map.width && hx < 0; ++x) {
      map.terrain[y * map.width + x] = 0x20u; /* Hills */
      if (map_resource_type_for_yield(&map, x, y) < 0) {
        hx = x;
        hy = y;
      }
    }
  }
  if (hx < 0) {
    fprintf(stderr, "no resource-free Hills tile found on 32x32\n");
    map_free(&map);
    return 1;
  }
  const int free_hill = colony_yield_for_worker(
    &map, hx, hy, COLONIZE_JOB_FARMER, COLONIZE_PROF_FREE_COLONIST, true, 0, 0,
    false
  );
  const int expert_hill = colony_yield_for_worker(
    &map, hx, hy, COLONIZE_JOB_FARMER, COLONIZE_JOB_FARMER, true, 0, 0,
    false
  );
  map_free(&map);
  if (free_hill != 2 || expert_hill != 4) {
    fprintf(stderr, "hills farmer want free=2 expert=4 got %d/%d\n", free_hill, expert_hill);
    return 1;
  }
  return 0;
}

/*
 * 2026-08-24 fix regression: the Farmer/Fisherman expert's second SoL/
 * Tory re-add (colony_yield_pipeline's `is_expert_food_fish` branch) must
 * re-add `sol_bonus` itself, not a value reconstructed from the colony's
 * SoL latch bits (`colony_flags`) — direct read of FUN_15eb_18ec
 * (~11866-11899) shows the re-added variable (`local_1c`) is the *same*
 * one already folded in once earlier in the function (the `sol_bonus`
 * parameter this port already threads through), computed from colonist
 * count/SoL% (byte+0x1f / FUN_15eb_0274, already ported as
 * colony_prod_sol_percent), not freshly derived from latch bits alone.
 * The two only coincide when the formula's Tory-penalty term is exactly
 * 0 (every other test in this file happens to hit that case); this test
 * uses a nonzero sol_bonus with colony_flags=0 (no latch bits) to prove
 * the re-add tracks sol_bonus, not the latch reconstruction the old code
 * used (which would have re-added 0 here instead of 3).
 *   expert: base(2) +sol_fold(3)=5, +flat(2)=7, +sol_readd(3)=10,
 *           +farmer(1, skill-blind improvement stack, 2026-09-03)=11
 */
static int case_tundra_farmer_sol_readd(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int tx = -1;
  int ty = -1;
  for (int y = 0; y < (int)map.height && tx < 0; ++y) {
    for (int x = 0; x < (int)map.width && tx < 0; ++x) {
      map.terrain[y * map.width + x] = 0; /* Tundra, unforested pedia 0 */
      if (map_resource_type_for_yield(&map, x, y) < 0) {
        tx = x;
        ty = y;
      }
    }
  }
  if (tx < 0) {
    fprintf(stderr, "no resource-free Tundra tile found on 32x32\n");
    map_free(&map);
    return 1;
  }
  const int expert_farmer_sol = colony_yield_for_worker(
    &map, tx, ty, COLONIZE_JOB_FARMER, COLONIZE_JOB_FARMER, /*has_docks=*/true, /*sol_bonus=*/3,
    /*colony_flags=*/0,
    false
  );
  map_free(&map);
  if (expert_farmer_sol != 11) {
    fprintf(
      stderr,
      "expert farmer, sol_bonus=3 colony_flags=0 want 11 got %d\n",
      expert_farmer_sol
    );
    return 1;
  }
  return 0;
}

/*
 * Silver Miner collapse on a deposit-less tile — FUN_15eb_18ec's job==7
 * block (viceroy_unpacked.c 11925-11941, smell audit #61). No resource
 * AND runtime mask 0x04 (MAP_LAYER2_SUPPRESS) CLEAR ⇒ a nonzero yield
 * becomes 1 when the tile has road/settlement or the worker is a
 * matching expert, else 0, and the whole improvement stack is skipped.
 * Suppress set (a mined-out mountain) leaves the branch entirely and the
 * ordinary base + expert + road stack applies.
 *
 * Kept as one case: a single mountain tile is progressively re-improved
 * (bare -> +sol -> +road -> +suppress -> reset+deposit -> +other job),
 * each stage's assertion depending on the previous stage's map edits.
 */
static int case_silver_miner_collapse(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int mx = -1;
  int my = -1;
  for (int y = 0; y < (int)map.height && mx < 0; ++y) {
    for (int x = 0; x < (int)map.width && mx < 0; ++x) {
      map.terrain[y * map.width + x] = 0xa0u; /* Mountains (pedia 27) */
      map.improve[y * map.width + x] = 0;
      map.layer2[y * map.width + x] = 0;
      if (map_resource_type_for_yield(&map, x, y) < 0) {
        mx = x;
        my = y;
      }
    }
  }
  if (mx < 0) {
    fprintf(stderr, "no resource-free Mountains tile found on 32x32\n");
    map_free(&map);
    return 1;
  }
  if (map_pedia_terrain_index_at(&map, mx, my) != 27) {
    fprintf(
      stderr, "mountain pedia expected 27 got %d\n", map_pedia_terrain_index_at(&map, mx, my)
    );
    map_free(&map);
    return 1;
  }

  /* Bare rock, no road: free colonist 0, expert 1 (base 1 would have paid
   * 1 and 2 respectively, plus the stack, before the collapse was ported). */
  const int bare_free = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_PROF_FREE_COLONIST, true, 0, 0,
    false
  );
  const int bare_expert = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_JOB_SILVER_MINER, true, 0, 0,
    false
  );
  if (bare_free != 0 || bare_expert != 1) {
    fprintf(
      stderr, "bare mountain silver want free=0 expert=1 got %d/%d\n", bare_free, bare_expert
    );
    map_free(&map);
    return 1;
  }
  /* A positive SoL bonus cannot escape the collapse either — DOS folds it
   * in before this branch, which then overwrites the total outright. */
  if (colony_yield_for_worker(
        &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_JOB_SILVER_MINER, true, 3, 0,
        false
      ) != 1) {
    fprintf(stderr, "bare mountain silver, expert + sol 3, want 1\n");
    map_free(&map);
    return 1;
  }

  /* Road: collapse target is 1 for anyone, and the road's own stack add
   * is suppressed with the rest of the stack (so the expert stays 1). */
  map.improve[my * map.width + mx] |= MAP_IMPROVE_ROAD;
  const int road_free = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_PROF_FREE_COLONIST, true, 0, 0,
    false
  );
  const int road_expert = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_JOB_SILVER_MINER, true, 0, 0,
    false
  );
  if (road_free != 1 || road_expert != 1) {
    fprintf(
      stderr, "roaded bare mountain silver want free=1 expert=1 got %d/%d\n",
      road_free, road_expert
    );
    map_free(&map);
    return 1;
  }
  /* bugs.md #883: Silver Miner is excluded from the Convert +1 whitelist
   * (FUN_15eb_18ec raw 11973-11979) — Convert gets no bonus over free. */
  const int road_convert = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_PROF_CONVERT, true, 0, 0,
    false
  );
  if (road_convert != road_free) {
    fprintf(
      stderr, "roaded bare mountain silver convert want no +1 (== free %d) got %d\n",
      road_free, road_convert
    );
    map_free(&map);
    return 1;
  }

  /* Suppress bit set (mined-out mountain): branch not entered, full
   * pipeline — free 1 + road 1 = 2, expert (1 x2) + road u2 = 4. This is
   * the shape golden_colony_prod01's Vlissingen silver tile needs
   * (with sol_bonus 2: (1+2)x2 + 2 = 8). */
  map.layer2[my * map.width + mx] |= MAP_LAYER2_SUPPRESS;
  const int depl_free = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_PROF_FREE_COLONIST, true, 0, 0,
    false
  );
  const int depl_expert = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_JOB_SILVER_MINER, true, 0, 0,
    false
  );
  const int depl_expert_sol = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_SILVER_MINER, COLONIZE_JOB_SILVER_MINER, true, 2, 0,
    false
  );
  if (depl_free != 2 || depl_expert != 4 || depl_expert_sol != 8) {
    fprintf(
      stderr,
      "suppressed mountain silver want free=2 expert=4 expert+sol2=8 got %d/%d/%d\n",
      depl_free, depl_expert, depl_expert_sol
    );
    map_free(&map);
    return 1;
  }
  map.layer2[my * map.width + mx] = 0;
  map.improve[my * map.width + mx] = 0;

  /* A real Silver Deposit (resource 12) also keeps the branch out — the
   * tile has a resource, so the ordinary base + effect + expert math runs:
   * free 1 + 2 = 3, expert (1 x2) + (2 x2) = 6. */
  int sx = -1;
  int sy = -1;
  if (!find_resource_tile(&map, 0xa0u, 12, &sx, &sy)) {
    fprintf(stderr, "no Mountains tile with a Silver Deposit found on 32x32\n");
    map_free(&map);
    return 1;
  }
  map.improve[sy * map.width + sx] = 0;
  map.layer2[sy * map.width + sx] = 0;
  const int dep_free = colony_yield_for_worker(
    &map, sx, sy, COLONIZE_JOB_SILVER_MINER, COLONIZE_PROF_FREE_COLONIST, true, 0, 0,
    false
  );
  const int dep_expert = colony_yield_for_worker(
    &map, sx, sy, COLONIZE_JOB_SILVER_MINER, COLONIZE_JOB_SILVER_MINER, true, 0, 0,
    false
  );
  if (dep_free != 3 || dep_expert != 6) {
    fprintf(
      stderr, "silver deposit mountain want free=3 expert=6 got %d/%d\n", dep_free, dep_expert
    );
    map_free(&map);
    return 1;
  }

  /* Other mined goods are untouched: the DOS branch tests job == 7 only,
   * so an Ore Miner on the same bare rock keeps base 4 (+expert, +road). */
  map.terrain[my * map.width + mx] = 0xa0u;
  const int ore_free = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_ORE_MINER, COLONIZE_PROF_FREE_COLONIST, true, 0, 0,
    false
  );
  const int ore_expert = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_ORE_MINER, COLONIZE_JOB_ORE_MINER, true, 0, 0,
    false
  );
  map_free(&map);
  if (ore_free != 4 || ore_expert != 8) {
    fprintf(
      stderr, "bare mountain ore want free=4 expert=8 got %d/%d\n", ore_free, ore_expert
    );
    return 1;
  }
  return 0;
}

/*
 * Smell audit #66: the town-commons plow term is unconditional in DOS
 * (FUN_15eb_1f72, viceroy_unpacked.c 12525-12529 — `FUN_137f_0142(x,y) &
 * 0x40` then `+1`, no terrain test). The port carried an invented
 * `pedia >= 0 && pedia <= 7` cleared-land gate, so a plowed forest/hills
 * commons silently lost the +1. Pin the DOS behaviour on a forest tile.
 */
static int case_commons_plow_unconditional(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int fx = -1;
  int fy = -1;
  for (int b = 0; b < 256 && fx < 0; ++b) {
    for (int y = 0; y < (int)map.height && fx < 0; ++y) {
      for (int x = 0; x < (int)map.width && fx < 0; ++x) {
        map.terrain[y * map.width + x] = (uint8_t)b;
        map.improve[y * map.width + x] = 0;
        map.layer2[y * map.width + x] = 0;
        const int pedia = map_pedia_terrain_index_at(&map, x, y);
        if (pedia >= 8 && pedia <= 23 && map_resource_type_for_yield(&map, x, y) < 0) {
          fx = x;
          fy = y;
        }
      }
    }
  }
  if (fx < 0) {
    fprintf(stderr, "no resource-free forest tile found for the commons plow check\n");
    map_free(&map);
    return 1;
  }
  ColonizeTownCommonsYield dry;
  colony_yield_town_commons(&map, fx, fy, 0, 2, &dry);
  map.improve[fy * map.width + fx] |= MAP_IMPROVE_PLOWED;
  ColonizeTownCommonsYield wet;
  colony_yield_town_commons(&map, fx, fy, 0, 2, &wet);
  map.improve[fy * map.width + fx] = 0;
  const int pedia = map_pedia_terrain_index_at(&map, fx, fy);
  map_free(&map);
  if (dry.food != 2 || wet.food != dry.food + 1) {
    fprintf(
      stderr,
      "forest commons plow: expected food %d then %d, got %d then %d (pedia %d)\n",
      2,
      3,
      dry.food,
      wet.food,
      pedia
    );
    return 1;
  }
  return 0;
}

/*
 * Docks gate is a single bit test on @BUILDING row 6 (DOS-LITERAL
 * FUN_15eb_18ec raw 11967 -> FUN_15eb_035e(DS:0x8dc6, 6), raw 9540-9557).
 * A Drydock built on top of Docks keeps row 6 set (the port's completion
 * path only sets bits), so fishing still works; a lone higher tier with the
 * Docks bit clear does not open fishing, matching DOS.
 */
static int case_docks_row6_bit(void) {
  static ColonizeColonyPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.building_type_count = COLONIZE_BUILDING_TYPES_MAX;
  for (int i = 0; i < pool.building_type_count; ++i) {
    pool.building_types[i].row_plus1 = i + 1;
  }
  ColonizeColony col;
  memset(&col, 0, sizeof(col));

  if (colony_yield_colony_has_docks(&pool, &col)) {
    fprintf(stderr, "docks: empty colony reported docks\n");
    return 1;
  }
  col.has_building[COLONY_BUILDING_DOCKS] = true;
  if (!colony_yield_colony_has_docks(&pool, &col)) {
    fprintf(stderr, "docks: row 6 set but not reported\n");
    return 1;
  }
  /* Upgrade: Drydock on top of Docks — row 6 stays set, fishing continues. */
  col.has_building[COLONY_BUILDING_DRYDOCK] = true;
  if (!colony_yield_colony_has_docks(&pool, &col)) {
    fprintf(stderr, "docks: drydock upgrade cleared the fishing gate\n");
    return 1;
  }
  /* Docks bit clear + higher tiers set is not a Docks colony in DOS. */
  col.has_building[COLONY_BUILDING_DOCKS] = false;
  col.has_building[COLONY_BUILDING_SHIPYARD] = true;
  if (colony_yield_colony_has_docks(&pool, &col)) {
    fprintf(stderr, "docks: higher tier substituted for row 6\n");
    return 1;
  }
  return 0;
}

/*
 * Beaver (resource 8) + Fur Trapper = +3, not +2 — FUN_15eb_17fa raw
 * 11736-11738 (`(param_1 == 8) && (param_2 == 4)` -> local_4 + 3). The +3
 * was a 2026-09-03 correction from a wrongly-read +2 and nothing pinned it
 * (only Game +2 was covered), so it could silently regress; the
 * Colonopedia's own copy had in fact drifted back to +2 (bugs.md #855).
 * Mixed forest (class 10) is the Beaver-bearing forest row: fur base 3.
 *   free colonist: 3 + 3 = 6
 *   expert:        (3 + 3) << 1 = 12   (resource add is inside the doubling)
 */
static int case_fur_trapper_beaver(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int bx = -1;
  int by = -1;
  if (!find_resource_tile(&map, 10, 8, &bx, &by)) {
    fprintf(stderr, "no mixed-forest+Beaver procedural tile found on 32x32\n");
    map_free(&map);
    return 1;
  }
  const int free_fur = colony_yield_for_worker(
    &map, bx, by, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    false
  );
  const int expert_fur = colony_yield_for_worker(
    &map, bx, by, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_JOB_FUR_TRAPPER, /*has_docks=*/true, 0, 0,
    false
  );
  map_free(&map);
  if (free_fur != 6 || expert_fur != 12) {
    fprintf(stderr, "mixed+Beaver fur free=%d expert=%d expected 6/12\n", free_fur, expert_fur);
    return 1;
  }
  return 0;
}

/*
 * Furs are the one job that counts a river TWICE: the job-4-only
 * pre-multiplier block (FUN_15eb_18ec raw 11849-11851: +1 minor, +2 major,
 * written as `local_26 + 2` — not 1+1) and then the generic improvement
 * stack's own river `+u`. Mixed forest (base 3), resource-free:
 *   minor river, free colonist: 3 +1 pre +1 stack                     = 5
 *   major river, free colonist: 3 +2 pre +1 stack +1 (major, add==u)  = 7
 *   major river + road, free:   3 +1 pre road +2 pre river
 *                               +1 stack road +1 stack river (add!=u)  = 8
 *     — the road is what disqualifies the stack's major-river extra (the
 *       only place that `add == u` guard is observable for furs), and it
 *       is ALSO counted twice, pre-multiplier and stack, like the river.
 *   major river, expert:        (3 +2) << 1 = 10, +2 river +2 major    = 14
 */
static int case_fur_trapper_river_double_count(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int mx = -1;
  int my = -1;
  for (int y = 0; y < (int)map.height && mx < 0; ++y) {
    for (int x = 0; x < (int)map.width && mx < 0; ++x) {
      map.terrain[y * map.width + x] = 10; /* Mixed forest, no river */
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
  const size_t ti = (size_t)my * (size_t)map.width + (size_t)mx;
  int rc = 0;
  map.terrain[ti] = (uint8_t)(10u | 0x40u); /* minor river */
  const int minor = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    false
  );
  map.terrain[ti] = (uint8_t)(10u | 0x40u | 0x80u); /* major river */
  const int major = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    false
  );
  const int major_expert = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_JOB_FUR_TRAPPER, /*has_docks=*/true, 0, 0,
    false
  );
  map_tile_set_road(&map, mx, my, true);
  const int major_road = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true, 0, 0,
    false
  );
  map_free(&map);
  if (minor != 5 || major != 7 || major_expert != 14 || major_road != 8) {
    fprintf(
      stderr,
      "fur river: minor=%d major=%d major_expert=%d major_road=%d expected 5/7/14/8\n",
      minor,
      major,
      major_expert,
      major_road
    );
    rc = 1;
  }
  return rc;
}

/*
 * Rain forest (pedia 15) is the one forest whose commons secondary is NOT
 * furs: FUN_15eb_1f72 raw 12553-12570 elects the strictly-greatest job over
 * 1..7 skipping 5, and Rain's Fur 1 only TIES Sugar 1, so the earlier job
 * (1, Sugar Planter) keeps it. That is the whole of docs/terrain_yields.md's
 * "Fur Trapper unless Rain" rule, and nothing pinned the tie direction.
 * Food = 2 (class 8..23), difficulty 2 so no handout.
 */
static int case_commons_rain_fur_sugar_tie(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int rx = -1;
  int ry = -1;
  for (int y = 0; y < (int)map.height && rx < 0; ++y) {
    for (int x = 0; x < (int)map.width && rx < 0; ++x) {
      map.terrain[y * map.width + x] = 15; /* Rain forest, no river */
      if (map_resource_type_for_yield(&map, x, y) < 0) {
        rx = x;
        ry = y;
      }
    }
  }
  if (rx < 0) {
    fprintf(stderr, "no resource-free Rain forest tile found on 32x32\n");
    map_free(&map);
    return 1;
  }
  const int rc =
    check_commons(&map, rx, ry, 2, COLONIZE_CARGO_SUGAR, 1, "rain forest commons (Fur/Sugar tie)");
  map_free(&map);
  return rc;
}

/*
 * The base-0 gate on furs: FUN_15eb_18ec's `local_26 != 0` (raw 11813)
 * encloses the fur pre-multiplier block, the positive-SoL fold AND the
 * expert doubling, so an Expert Fur Trapper on any terrain whose fur column
 * is 0 produces nothing at all — no expert flat bonus, no road/river, no
 * Hudson. Checked on Prairie (unforested, fur 0) with a road, a major river
 * and Hudson all on at once.
 *
 * The row's "expert on a base-0 Game tile" variant is unreachable by
 * construction, and that is worth recording rather than asserting: Game
 * (resource 9) is only produced for terrain classes 8/11/16/19 (map.c's
 * mapedit_resource_type_by_terrain), all four of which are forest rows with
 * a fur base of 2 or 3, so no resource-bearing tile can pair Game with a
 * zero fur base. The resource add sitting outside the base-0 gate therefore
 * only shows up for Farmer (Game +2 on cleared land), which
 * case_expert_farmer_game_resource already covers.
 */
static int case_expert_fur_trapper_base_zero(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  map.terrain[7] = (uint8_t)(3u | 0x40u | 0x80u); /* Prairie + major river */
  map_tile_set_road(&map, 7, 0, true);
  const int expert_fur = colony_yield_for_worker(
    &map, 7, 0, COLONIZE_JOB_FUR_TRAPPER, COLONIZE_JOB_FUR_TRAPPER, /*has_docks=*/true, 2, 0,
    /*has_hudson=*/true
  );
  map_free(&map);
  if (expert_fur != 0) {
    fprintf(stderr, "expert fur trapper on prairie (fur base 0) want 0 got %d\n", expert_fur);
    return 1;
  }
  return 0;
}

/*
 * bugs.md #894: FUN_15eb_18ec raw 11913-11923 (DS 0xa896) tallies the
 * ore/silver "depletion units" from (resource, job) alone as it walks the
 * 5x5 field loop, independent of the pipeline's final yield: Minerals(6) +
 * Ore Miner -> +1, Minerals(6) + Silver Miner -> +2, Silver Deposit(12) +
 * Silver Miner -> +1. turn_production.c used to gate the tally behind
 * `yld <= 0 continue`, so a worked deposit driven to a 0-yield turn never
 * depleted; the fix hoists the (resource, job) tally above that gate.
 *
 * unit_colony_yield is a SLIM test target (COLONIZE_SLIM_SOURCES, see
 * CMakeLists.txt) that does not link turn_production.c/turn_colony.c, so
 * this case cannot drive turn_colony_free_production directly. Instead it
 * reproduces turn_production.c's exact (resource, job) -> units decision
 * (mirrored below, comment-linked to the ~594-609 block it must stay in
 * sync with) against the same resource lookup the real tally uses
 * (map_resource_type_for_yield), and proves the "still depletes at yield 0"
 * half of the bug by feeding colony_yield_for_worker a sol_bonus so
 * negative its own `sol_bonus < 0` clamp lands the yield at exactly 0 on
 * the same tile — the tally rule reads only (resource, job), never yield,
 * so it must return the same unit count regardless.
 */
static int depletion_units_for(int resource, int job) {
  /* Mirrors turn_production.c ~594-609 verbatim; keep in sync. */
  if (resource == 6 && job == COLONIZE_JOB_ORE_MINER) {
    return 1;
  }
  if (resource == 6 && job == COLONIZE_JOB_SILVER_MINER) {
    return 2;
  }
  if (resource == 12 && job == COLONIZE_JOB_SILVER_MINER) {
    return 1;
  }
  return 0;
}

static int case_depletion_counter_tally(void) {
  /* Resource placement is coordinate-hash-gated (map_resource_type_at_ex),
   * not a direct function of terrain class, so — same as the file's other
   * resource cases — scan for a hit rather than picking a fixed tile. `map`
   * stays alive for the Minerals/Ore-Miner yield-0 check below; the Silver
   * Deposit lookup only needs its resource id, so its map is scoped and
   * freed immediately. */
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int mx = -1, my = -1;
  if (!find_resource_tile(&map, 0, 6, &mx, &my)) {
    fprintf(stderr, "depletion test setup: no Minerals(6) tile found\n");
    map_free(&map);
    return 1;
  }
  const int res_minerals = map_resource_type_for_yield(&map, mx, my);

  int res_silver_dep = -1;
  {
    ColonizeWorldMap map2;
    if (map_new(&map2) != 0) {
      map_free(&map);
      return 1;
    }
    int sx = -1, sy = -1;
    if (!find_resource_tile(&map2, 27, 12, &sx, &sy)) {
      fprintf(stderr, "depletion test setup: no Silver Deposit(12) tile found\n");
      map_free(&map2);
      map_free(&map);
      return 1;
    }
    res_silver_dep = map_resource_type_for_yield(&map2, sx, sy);
    map_free(&map2);
  }
  if (res_minerals != 6 || res_silver_dep != 12) {
    fprintf(
      stderr, "depletion test setup: resources %d/%d want 6/12\n", res_minerals, res_silver_dep
    );
    map_free(&map);
    return 1;
  }

  /* The 1/2/1 tally itself (bugs.md #894). */
  const int ore_units = depletion_units_for(res_minerals, COLONIZE_JOB_ORE_MINER);
  const int silver_on_minerals_units = depletion_units_for(res_minerals, COLONIZE_JOB_SILVER_MINER);
  const int silver_on_deposit_units = depletion_units_for(res_silver_dep, COLONIZE_JOB_SILVER_MINER);
  if (ore_units != 1 || silver_on_minerals_units != 2 || silver_on_deposit_units != 1) {
    fprintf(
      stderr, "depletion units want 1/2/1 got %d/%d/%d\n",
      ore_units, silver_on_minerals_units, silver_on_deposit_units
    );
    map_free(&map);
    return 1;
  }
  /* An ordinary hills/mountain tile without a matching deposit never
   * depletes (player-confirmed 2026-08-16). */
  if (depletion_units_for(-1, COLONIZE_JOB_ORE_MINER) != 0 ||
      depletion_units_for(13, COLONIZE_JOB_ORE_MINER) != 0) {
    fprintf(stderr, "depletion units: non-deposit tiles must tally 0\n");
    map_free(&map);
    return 1;
  }

  /*
   * Yield-0 case (the bug's actual regression): drive the Ore Miner's
   * final yield on the Minerals tile to exactly 0 via colony_yield_for_
   * worker's own `sol_bonus < 0` clamp (colony_yield.c ~596-599), then
   * confirm the tally rule — read from (resource, job) alone, same as
   * turn_production.c's hoisted block — still counts the unit.
   */
  const int yld0 = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_ORE_MINER, COLONIZE_PROF_FREE_COLONIST, /*has_docks=*/true,
    /*sol_bonus=*/-1000, /*colony_flags=*/0, /*has_hudson=*/false
  );
  if (yld0 != 0) {
    fprintf(stderr, "depletion yield-0 setup: want yld==0 got %d\n", yld0);
    map_free(&map);
    return 1;
  }
  const int units_at_yld0 = depletion_units_for(res_minerals, COLONIZE_JOB_ORE_MINER);
  map_free(&map);
  if (units_at_yld0 != 1) {
    fprintf(
      stderr, "depletion at yield 0 want 1 got %d (bugs.md #894 hoist)\n", units_at_yld0
    );
    return 1;
  }
  return 0;
}

/*
 * bugs.md #895(c): resource-effect deltas from docs/terrain_yields.md's
 * "Resource effect" table, measured as (resource tile) - (resource-free
 * tile of the same terrain class, no road/river/plow, sol=0) so no base
 * table value needs to be hand-derived. Prime Timber(10)+Lumberjack,
 * Minerals(6)+Ore Miner, Minerals(6)+Silver Miner, Ore Deposit(13)+Ore
 * Miner: free delta and expert delta (expert doubles base+effect
 * together, colony_yield.c's own resource-effect comment).
 */
static int resource_delta_case(
  int terrain_class, int want_res, int field_job, int want_free_delta, int want_expert_delta,
  const char* label
) {
  ColonizeWorldMap map_res;
  if (map_new(&map_res) != 0) {
    return 1;
  }
  int rx = -1, ry = -1;
  if (!find_resource_tile(&map_res, (uint8_t)terrain_class, want_res, &rx, &ry)) {
    fprintf(stderr, "%s: no resource tile found\n", label);
    map_free(&map_res);
    return 1;
  }
  const int free_with = colony_yield_for_worker(
    &map_res, rx, ry, field_job, COLONIZE_PROF_FREE_COLONIST, true, 0, 0, false
  );
  const int expert_with =
    colony_yield_for_worker(&map_res, rx, ry, field_job, field_job, true, 0, 0, false);
  map_free(&map_res);

  ColonizeWorldMap map_bare;
  if (map_new(&map_bare) != 0) {
    return 1;
  }
  int bx = -1, by = -1;
  for (int y = 0; y < (int)map_bare.height && bx < 0; ++y) {
    for (int x = 0; x < (int)map_bare.width && bx < 0; ++x) {
      map_bare.terrain[y * map_bare.width + x] = (uint8_t)terrain_class;
      if (map_resource_type_for_yield(&map_bare, x, y) < 0) {
        bx = x;
        by = y;
      }
    }
  }
  if (bx < 0) {
    fprintf(stderr, "%s: no resource-free tile of the same class found\n", label);
    map_free(&map_bare);
    return 1;
  }
  const int free_without = colony_yield_for_worker(
    &map_bare, bx, by, field_job, COLONIZE_PROF_FREE_COLONIST, true, 0, 0, false
  );
  const int expert_without =
    colony_yield_for_worker(&map_bare, bx, by, field_job, field_job, true, 0, 0, false);
  map_free(&map_bare);

  const int free_delta = free_with - free_without;
  const int expert_delta = expert_with - expert_without;
  if (free_delta != want_free_delta || expert_delta != want_expert_delta) {
    fprintf(
      stderr, "%s: free delta want %d got %d, expert delta want %d got %d\n",
      label, want_free_delta, free_delta, want_expert_delta, expert_delta
    );
    return 1;
  }
  return 0;
}

static int case_resource_effect_deltas(void) {
  /*
   * Prime Timber(10): docs/terrain_yields.md's per-terrain special-resource
   * table pins it to Conifer(12)/Tropical(13), not Mixed(10) (Beaver lives
   * there instead).
   *
   * Measured delta is 4 free / 8 expert, not the table's raw "+2" effect
   * value: unlike Ore/Silver Miner (whose resource add sits in the code
   * path that's only doubled once, by the generic `expert ? yield<<=1`
   * branch, and not at all for a free colonist), the resource add for
   * Lumberjack lands in colony_yield.c's plain `else { yield += effect; }`
   * arm, which runs BEFORE the job's own unconditional
   * `if (LUMBERJACK) yield <<= 1`. So the timber bonus rides that
   * doubling too (free: 2*2=4) and, for an expert, ALSO the generic
   * expert doubling ahead of it (2*(2*2)=8) — a double-double the docs'
   * pipeline order (step 7 resource, step 8 Lumberjack double) doesn't
   * predict, and bugs.md #895's "+2 free/+4 expert" assumption missed.
   * Asserting the actual measured numbers per this case's own
   * instructions (report a mismatch rather than editing colony_yield.c,
   * which is out of scope here); flagged to the caller as a possible real
   * discrepancy worth its own bugs.md row.
   */
  if (resource_delta_case(12, 10, COLONIZE_JOB_LUMBERJACK, 4, 8, "prime timber lumberjack")) {
    return 1;
  }
  /* Minerals(6) on a FIELD plot: Tundra class 0 -> resource 6. */
  if (resource_delta_case(0, 6, COLONIZE_JOB_ORE_MINER, 3, 6, "minerals ore miner")) {
    return 1;
  }
  if (resource_delta_case(0, 6, COLONIZE_JOB_SILVER_MINER, 1, 2, "minerals silver miner")) {
    return 1;
  }
  /* Ore Deposit(13): Hills class 28 -> resource 13. */
  if (resource_delta_case(28, 13, COLONIZE_JOB_ORE_MINER, 2, 4, "ore deposit ore miner")) {
    return 1;
  }
  return 0;
}

/*
 * bugs.md #895(c): non-expert Lumberjack's `u` is 2 like a matching expert
 * (colony_yield.c's improvement-stack comment: "u = 2 for a matching
 * non-food/fish expert OR ANY Lumberjack"), and Lumberjack's own `<<= 1`
 * runs before that stack — so a free colonist Lumberjack on a road/river
 * tile gains +2 over the bare tile's (already-doubled) yield, and a major
 * river gains +4 (the stack's own re-add fires because river was the
 * stack's sole contributor). Mixed Forest, no resource (same scan pattern
 * as case_fur_trapper_river_double_count).
 */
static int case_lumberjack_free_u2_stack(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int mx = -1, my = -1;
  for (int y = 0; y < (int)map.height && mx < 0; ++y) {
    for (int x = 0; x < (int)map.width && mx < 0; ++x) {
      map.terrain[y * map.width + x] = 10; /* Mixed forest, no river */
      if (map_resource_type_for_yield(&map, x, y) < 0) {
        mx = x;
        my = y;
      }
    }
  }
  if (mx < 0) {
    fprintf(stderr, "lumberjack u2: no resource-free Mixed forest tile found\n");
    map_free(&map);
    return 1;
  }
  const size_t ti = (size_t)my * (size_t)map.width + (size_t)mx;
  const int bare = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST, true, 0, 0, false
  );
  map_tile_set_road(&map, mx, my, true);
  const int road = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST, true, 0, 0, false
  );
  map_tile_set_road(&map, mx, my, false);
  map.terrain[ti] = (uint8_t)(10u | 0x40u); /* minor river */
  const int minor = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST, true, 0, 0, false
  );
  map.terrain[ti] = (uint8_t)(10u | 0x40u | 0x80u); /* major river */
  const int major = colony_yield_for_worker(
    &map, mx, my, COLONIZE_JOB_LUMBERJACK, COLONIZE_PROF_FREE_COLONIST, true, 0, 0, false
  );
  map_free(&map);
  if (road != bare + 2 || minor != bare + 2 || major != bare + 4) {
    fprintf(
      stderr,
      "lumberjack u2 stack: bare=%d road=%d(want +2) minor=%d(want +2) major=%d(want +4)\n",
      bare, road, minor, major
    );
    return 1;
  }
  return 0;
}

/*
 * bugs.md #895(b)/(c): FUN_15eb_1f72's commons secondary is a strict max
 * over jobs 1..7 (skipping 5, Farmer/Fisherman not eligible). Hills(28)'s
 * own resource is Ore Deposit(13) — Ore-over-Silver there is trivial, both
 * on the bare-terrain base (docs row 27/28's now-fixed table) and with the
 * deposit only helping Ore further.
 *
 * Mountains(27)'s own resource is Silver Deposit(12), and measuring it
 * shows the strict max FLIPS to Silver Miner once the deposit is present
 * (secondary_cargo == COLONIZE_CARGO_SILVER, amount 2) — the Silver
 * Deposit's own +2 resource effect (docs/terrain_yields.md's resource
 * table) overtakes Ore's bare-terrain lead (4 vs 1) once added, unlike the
 * Hills/Ore Deposit case where the deposit only widens Ore's existing
 * lead. bugs.md #895(c)'s "Hills/Mountains with a resource picks Ore over
 * Silver" does not hold on Mountains once its own deposit is on the tile;
 * asserted here as the measured behavior per this file's report-don't-fix
 * convention (colony_yield.c is out of scope for this change).
 */
static int case_commons_hills_mountains_resource_ore_over_silver(void) {
  ColonizeWorldMap map;
  if (map_new(&map) != 0) {
    return 1;
  }
  int hx = -1, hy = -1;
  if (!find_resource_tile(&map, 28, 13, &hx, &hy)) { /* Hills(28) + Ore Deposit(13) */
    fprintf(stderr, "commons hills+ore deposit: no resource tile found\n");
    map_free(&map);
    return 1;
  }
  ColonizeTownCommonsYield tc_hills;
  colony_yield_town_commons(&map, hx, hy, 0, 2, &tc_hills);
  map_free(&map);
  if (tc_hills.secondary_cargo != COLONIZE_CARGO_ORE) {
    fprintf(
      stderr, "commons Hills+Ore Deposit secondary want Ore(%d) got %d\n",
      COLONIZE_CARGO_ORE, tc_hills.secondary_cargo
    );
    return 1;
  }

  ColonizeWorldMap map2;
  if (map_new(&map2) != 0) {
    return 1;
  }
  int mtx = -1, mty = -1;
  if (!find_resource_tile(&map2, 27, 12, &mtx, &mty)) { /* Mountains(27) + Silver Deposit(12) */
    fprintf(stderr, "commons mountains+silver deposit: no resource tile found\n");
    map_free(&map2);
    return 1;
  }
  ColonizeTownCommonsYield tc_mtn;
  colony_yield_town_commons(&map2, mtx, mty, 0, 2, &tc_mtn);
  map_free(&map2);
  if (tc_mtn.secondary_cargo != COLONIZE_CARGO_SILVER || tc_mtn.secondary_amount != 2) {
    fprintf(
      stderr,
      "commons Mountains+Silver Deposit secondary want Silver(%d)/2 got %d/%d\n",
      COLONIZE_CARGO_SILVER, tc_mtn.secondary_cargo, tc_mtn.secondary_amount
    );
    return 1;
  }
  return 0;
}

static const TestCase k_cases[] = {
  {"commons_scrub", case_commons_scrub},
  {"commons_hills_base", case_commons_hills_base},
  {"commons_hills_sol_latch", case_commons_hills_sol_latch},
  {"commons_broadleaf", case_commons_broadleaf},
  {"commons_prairie_minor_river", case_commons_prairie_minor_river},
  {"commons_broadleaf_game", case_commons_broadleaf_game},
  {"field_river_cotton", case_field_river_cotton},
  {"fisherman_major_river", case_fisherman_major_river},
  {"expert_ore_miner_hills_road_sol", case_expert_ore_miner_hills_road_sol},
  {"expert_fur_trapper_hudson", case_expert_fur_trapper_hudson},
  {"fur_trapper_beaver", case_fur_trapper_beaver},
  {"fur_trapper_river_double_count", case_fur_trapper_river_double_count},
  {"commons_rain_fur_sugar_tie", case_commons_rain_fur_sugar_tie},
  {"expert_fur_trapper_base_zero", case_expert_fur_trapper_base_zero},
  {"expert_farmer_game_resource", case_expert_farmer_game_resource},
  {"hills_farmer", case_hills_farmer},
  {"tundra_farmer_sol_readd", case_tundra_farmer_sol_readd},
  {"silver_miner_collapse", case_silver_miner_collapse},
  {"commons_plow_unconditional", case_commons_plow_unconditional},
  {"docks_row6_bit", case_docks_row6_bit},
  {"depletion_counter_tally", case_depletion_counter_tally},
  {"resource_effect_deltas", case_resource_effect_deltas},
  {"lumberjack_free_u2_stack", case_lumberjack_free_u2_stack},
  {"commons_hills_mountains_resource_ore_over_silver",
   case_commons_hills_mountains_resource_ore_over_silver},
};

TEST_MAIN(k_cases)
