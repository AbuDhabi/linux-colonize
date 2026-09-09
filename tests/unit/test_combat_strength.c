/*
 * Smoke: combat_strength.c DOS gates that no other harness pins.
 * Smell audit 2026-09-09 items #3 (missing-foe terrain default), #10 (WoI
 * SoL peel needs a real colony record) and #11 (attacker fatigue has no
 * domain gate — it applies at sea).
 */
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/combat_strength.h"
#include "core/map.h"
#include "core/units.h"

#include <stdio.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_combat_strength: FAIL %s\n", msg);
  return 1;
}

/* Minimal @UNIT table: 0 Soldier (land), 1 Frigate (sea). */
static void seed_types(ColonizeUnitPool* pool) {
  pool->type_count = 2;
  snprintf(pool->types[0].name, sizeof(pool->types[0].name), "Soldier");
  pool->types[0].attack = 2;
  pool->types[0].defense = 2;
  pool->types[0].movement = 1;
  pool->types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  snprintf(pool->types[1].name, sizeof(pool->types[1].name), "Frigate");
  pool->types[1].attack = 16;
  pool->types[1].defense = 16;
  pool->types[1].movement = 6;
  pool->types[1].guns = 16;
  pool->types[1].hull = 16;
  pool->types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
}

/*
 * #3: FUN_157e_015e's terrain gate (viceroy_unpacked.c 9015-9021) needs
 * `foe nation < 4`; a European defender probed with NO foe (the AI's own
 * ai_euro_land_foe_toughness passes -1) used to default to nation 0xf, read
 * as a native attacker, and lose the open-terrain bonus outright.
 */
static int test_missing_foe_keeps_terrain(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[256];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    return fail("map_alloc");
  }

  /* Find a terrain id whose DS:0x2f77 founding-score byte is non-zero, and
   * one whose byte is zero — the bonus is the difference between them. */
  int good = -1;
  int flat = -1;
  for (int t = 0; t < 32 && (good < 0 || flat < 0); ++t) {
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = (uint8_t)t;
    }
    const int score = map_dos_terr_found_score_byte(map_dos_terr_class_at(&map, 2, 2));
    if (score > 0 && good < 0) {
      good = t;
    }
    if (score == 0 && flat < 0) {
      flat = t;
    }
  }
  if (good < 0 || flat < 0) {
    map_free(&map);
    return fail("no terrain pair with distinct founding-score bytes");
  }

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  units_reset(&pool);
  units_set_occupancy_map(NULL);
  seed_types(&pool);

  const int def = units_spawn_allow_stack(&pool, 0, 2, 2);
  const int foe = units_spawn_allow_stack(&pool, 0, 3, 2);
  if (def < 0 || foe < 0) {
    map_free(&map);
    return fail("spawn");
  }
  units_get(&pool, def)->nation_id = 0;
  units_get(&pool, foe)->nation_id = 1;

  ColonizeCombatStrengthCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.map = &map;

  for (size_t i = 0; i < map.tile_count; ++i) {
    map.terrain[i] = (uint8_t)good;
  }
  const int on_good_no_foe = combat_unit_toughness(&ctx, def, -1);
  const int on_good_with_foe = combat_engagement_strength(&ctx, def, foe, NULL);

  for (size_t i = 0; i < map.tile_count; ++i) {
    map.terrain[i] = (uint8_t)flat;
  }
  const int on_flat_no_foe = combat_unit_toughness(&ctx, def, -1);

  map_free(&map);

  if (on_good_no_foe != on_good_with_foe) {
    fprintf(
      stderr, "no-foe %d vs euro-foe %d\n", on_good_no_foe, on_good_with_foe
    );
    return fail("missing foe must score like an ordinary European engagement");
  }
  if (on_good_no_foe <= on_flat_no_foe) {
    return fail("open-terrain bonus lost when the foe is missing (#3)");
  }
  return 0;
}

/*
 * #10: the WoI Tory/Rebel peel (FUN_5fef_1b0e ~100494-100527) runs inside
 * `if (-1 < iVar18)` — the colony INDEX on the defended tile — and reads SoL
 * from the record it then binds. With no col1 record the port must apply no
 * support peel at all; the old `liberty_bells_total / 4` fallback invented
 * one, and a crown attacker collected a full +100% Tory bonus off it.
 */
static int test_woi_sol_needs_a_colony_record(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[256];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    return fail("woi map_alloc");
  }
  for (size_t i = 0; i < map.tile_count; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  units_reset(&pool);
  units_set_occupancy_map(NULL);
  seed_types(&pool);

  const int def = units_spawn_allow_stack(&pool, 0, 4, 4);
  const int atk = units_spawn_allow_stack(&pool, 0, 3, 4);
  if (def < 0 || atk < 0) {
    map_free(&map);
    return fail("woi spawn");
  }
  units_get(&pool, def)->nation_id = 0; /* rebel human */
  units_get(&pool, atk)->nation_id = 1; /* crown */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  colonies.colony_count = 1;

  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.game_options.woi = 1;
  col1.head.crown_nation_id = 1;
  col1.player[0].control = 0; /* human rebel */
  col1.player[1].control = 1; /* crown is AI */
  col1.nation[0].liberty_bells_total = 200; /* the old fallback's input */

  ColonizeCombatStrengthCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.map = &map;
  ctx.colonies = &colonies;
  ctx.col1 = &col1;

  /* No col1 colony array at all → the peel must not fire. */
  ColonizeCombatEngageResult no_record;
  combat_land_engage(&ctx, atk, def, &no_record);
  if (no_record.atk_flags.sol_percent != 0) {
    fprintf(stderr, "sol_percent %d with no record\n", no_record.atk_flags.sol_percent);
    return fail("WoI support peel fired without a colony record (#10)");
  }

  /* A real record at 25% SoL → crown gets the 75% Tory share. */
  ColonizeCol1Colony rec;
  memset(&rec, 0, sizeof(rec));
  rec.x = 4;
  rec.y = 4;
  rec.rebel_dividend = 1;
  rec.rebel_divisor = 4;
  col1.colony = &rec;
  col1.head.colony_count = 1;

  ColonizeCombatEngageResult with_record;
  combat_land_engage(&ctx, atk, def, &with_record);
  col1.colony = NULL;
  col1.head.colony_count = 0;
  map_free(&map);

  if (with_record.atk_flags.sol_percent != 75) {
    fprintf(stderr, "sol_percent %d want 75\n", with_record.atk_flags.sol_percent);
    return fail("crown Tory share must be 100 - SoL from the bound record");
  }
  if (with_record.atk_strength <= no_record.atk_strength) {
    return fail("Tory peel must raise crown attacker strength");
  }
  return 0;
}

/*
 * #11: `if (local_98 != 0) local_92 = local_92 * local_98 / 3` (100459) has
 * no is-ship guard, and local_98 is set from remaining thirds at 100388-93
 * with no domain test either — attacker fatigue applies at sea.
 */
static int test_fatigue_applies_at_sea(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  units_reset(&pool);
  units_set_occupancy_map(NULL);
  seed_types(&pool);

  const int atk = units_spawn_allow_stack(&pool, 1, 2, 2);
  const int def = units_spawn_allow_stack(&pool, 1, 3, 2);
  if (atk < 0 || def < 0) {
    return fail("naval spawn");
  }
  units_get(&pool, atk)->nation_id = 0;
  units_get(&pool, def)->nation_id = 1;

  ColonizeCombatStrengthCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;

  ColonizeCombatEngageResult rested;
  combat_naval_engage(&ctx, atk, def, &rested);

  units_get(&pool, atk)->moves_left = 2; /* 2 of 3 thirds left */
  ColonizeCombatEngageResult tired;
  combat_naval_engage(&ctx, atk, def, &tired);

  if (tired.atk_strength >= rested.atk_strength) {
    fprintf(stderr, "tired %d rested %d\n", tired.atk_strength, rested.atk_strength);
    return fail("naval attacker fatigue peel missing (#11)");
  }
  if (tired.atk_strength != rested.atk_strength * 2 / 3) {
    fprintf(
      stderr, "tired %d want %d\n", tired.atk_strength, rested.atk_strength * 2 / 3
    );
    return fail("naval fatigue must scale by rem/3");
  }
  return 0;
}

/*
 * Parked lead (2026-09-09): FUN_5fef_1b0e's WoI gate is
 * `if (((*(byte *)0x5382 & 1) != 0) && (uVar16 < 4))` (raw 100494) — WoI plus a
 * Euro ATTACKER, no is-ship test on either side. The port had bolted `&& land`
 * onto it, so a crown Man-O-War shelling a hull berthed in a rebel colony lost
 * both the +50% crown/REF bombardment bonus (raw 100504-100507) and the Tory
 * share of the colony's SoL (raw 100508-100523). The colony arm is selected by
 * iVar18 = FUN_281f_07be(defended tile) >= 0, which a berthed defender
 * satisfies exactly as a landed one does.
 */
static int test_woi_crown_ship_bombards_colony(void) {
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  char err[256];
  if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
    return fail("woi naval map_alloc");
  }
  for (size_t i = 0; i < map.tile_count; ++i) {
    map.terrain[i] = 1;
  }

  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  units_reset(&pool);
  units_set_occupancy_map(NULL);
  seed_types(&pool);

  /* Type 1 = Frigate on both sides; the defender lies in the rebel port. */
  const int def = units_spawn_allow_stack(&pool, 1, 4, 4);
  const int atk = units_spawn_allow_stack(&pool, 1, 3, 4);
  if (def < 0 || atk < 0) {
    map_free(&map);
    return fail("woi naval spawn");
  }
  units_get(&pool, def)->nation_id = 0; /* rebel human */
  units_get(&pool, atk)->nation_id = 1; /* crown */

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 4;
  c->y = 4;
  c->population = 3;
  colonies.colony_count = 1;

  ColonizeCol1Colony rec;
  memset(&rec, 0, sizeof(rec));
  rec.x = 4;
  rec.y = 4;
  rec.rebel_dividend = 1;
  rec.rebel_divisor = 4; /* 25% SoL → 75% Tory share for the crown */

  static ColonizeCol1Save col1;
  memset(&col1, 0, sizeof(col1));
  col1.head.crown_nation_id = 1;
  col1.player[0].control = 0; /* human rebel */
  col1.player[1].control = 1; /* crown is AI */
  col1.colony = &rec;
  col1.head.colony_count = 1;

  ColonizeCombatStrengthCtx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &pool;
  ctx.map = &map;
  ctx.colonies = &colonies;
  ctx.col1 = &col1;

  ColonizeCombatEngageResult peace;
  col1.head.game_options.woi = 0;
  combat_naval_engage(&ctx, atk, def, &peace);

  ColonizeCombatEngageResult war;
  col1.head.game_options.woi = 1;
  combat_naval_engage(&ctx, atk, def, &war);

  col1.colony = NULL;
  col1.head.colony_count = 0;
  map_free(&map);

  if ((war.atk_flags.flags & COMBAT_FLAG_REF) == 0) {
    return fail("WoI crown ship must raise the 0x8d01|0x80 bombardment row");
  }
  if (war.atk_flags.sol_percent != 75) {
    fprintf(stderr, "naval sol_percent %d want 75\n", war.atk_flags.sol_percent);
    return fail("WoI crown ship must collect the Tory share (100 - SoL)");
  }
  /* +50% crown, then +75% of that — the same two DOS lines the land arm runs. */
  const int want = peace.atk_strength + (peace.atk_strength >> 1);
  if (war.atk_strength != want + (75 * want) / 100) {
    fprintf(stderr, "naval woi atk %d want %d\n", war.atk_strength, want + (75 * want) / 100);
    return fail("WoI naval crown bonus + Tory peel arithmetic");
  }
  return 0;
}

int main(void) {
  if (test_missing_foe_keeps_terrain() != 0) {
    return 1;
  }
  if (test_woi_sol_needs_a_colony_record() != 0) {
    return 1;
  }
  if (test_fatigue_applies_at_sea() != 0) {
    return 1;
  }
  if (test_woi_crown_ship_bombards_colony() != 0) {
    return 1;
  }
  printf("unit_combat_strength: OK\n");
  return 0;
}
