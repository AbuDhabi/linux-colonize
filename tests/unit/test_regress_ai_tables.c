/*
 * Regression guards for FIXED bugs.md rows that had no test (gap audit
 * 2026-09-26). Slice: ai_tables.
 */
#include "../common/ai_fixture.h"
#include "../common/test_catalogs.h"
#include "../common/test_runner.h"

#include "core/ai_euro.h"
#include "core/ai_internal.h"
#include "core/ai_euro_internal.h"
#include "core/ai_king.h"
#include "core/ai_king_internal.h"
#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/units.h"

#include <string.h>

/*
 * bugs.md #651 (FUN_521d_20e6 raw 89685-89687 / 89801-89803): the port's
 * ai_euro_20e6_unit_col5 (ai_euro_ship.c:2456) used to return the @UNIT
 * HOLDS column (cargo) instead of DEFENSE — Dragoon scored (0-10)*2 = -20
 * instead of (3-10)*2 = -14. Guard: a hand-built pool with a land type whose
 * defense and cargo differ must read defense.
 */
static int case_651_col5_reads_defense(void) {
  ColonizeUnitPool pool;
  memset(&pool, 0, sizeof(pool));
  pool.type_count = 1;
  pool.types[0].kind_plus1 = (int)UNITS_KIND_DRAGOON + 1;
  pool.types[0].defense = 3;
  pool.types[0].cargo = 99; /* distinct from defense; must NOT be returned */

  const int got = ai_euro_20e6_unit_col5(&pool, (int)UNITS_KIND_DRAGOON);
  if (got != 3) {
    fprintf(stderr, "unit_regress_ai_tables: col5(Dragoon)=%d want 3 (defense)\n", got);
    return 1;
  }
  if (got == pool.types[0].cargo) {
    return 1; /* would only coincidentally pass if reverted to holds */
  }
  return 0;
}

/*
 * bugs.md #668 (FUN_521d_20e6 raw 88586, 88885-88887): the NAMES.TXT-default
 * fallback table k_20e6_type_combat (ai_euro_land.c:57) used to hold the
 * DEFENSE column (Artillery 5, Galleon 10, Colonists 1); every DOS site
 * reads ATTACK. Guard via the public accessor ai_euro_20e6_type_combat,
 * which reads the fallback table whenever the live per-turn cache
 * (ai_euro_s_20e6_type_cache_valid) has no bit set for that row — true here
 * since this test never runs the Euro dispatcher turn.
 */
static int case_668_fallback_table_is_attack(void) {
  ai_euro_reset(); /* zeroes ai_euro_s_20e6_type_cache_valid */

  const int colonist = ai_euro_20e6_type_combat((int)UNITS_KIND_COLONIST);
  const int artillery = ai_euro_20e6_type_combat((int)UNITS_KIND_ARTILLERY);
  const int galleon = ai_euro_20e6_type_combat((int)UNITS_KIND_GALLEON);

  /* Attack column values (current k_20e6_type_combat[]); the old DEFENSE
   * column read Colonists 1, Artillery 5, Galleon 10. */
  if (colonist != 0 || artillery != 7 || galleon != 0) {
    fprintf(
      stderr,
      "unit_regress_ai_tables: fallback combat colonist=%d artillery=%d galleon=%d "
      "want 0/7/0 (attack, not defense 1/5/10)\n",
      colonist, artillery, galleon
    );
    return 1;
  }
  return 0;
}

/*
 * bugs.md #661 (FUN_43f7_0982 raw 74077-74081 / 74091-74094): the REF
 * per-wave landing cap used to be a hardcoded 6/31 pair; DS:0x5333 is the
 * @UNIT row 18 (Man-O-War) HOLDS column. Guard: ai_king_0982_max_landing
 * must track the catalog's Man-O-War cargo value, not a literal.
 */
static int case_661_max_landing_reads_mow_holds(void) {
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units.type_count = (int)UNITS_KIND_MAN_O_WAR + 1;
  units.types[UNITS_KIND_MAN_O_WAR].kind_plus1 = (int)UNITS_KIND_MAN_O_WAR + 1;
  units.types[UNITS_KIND_MAN_O_WAR].cargo = 9; /* deliberately not 6 or 31 */

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.units = &units;

  const int got = ai_king_0982_max_landing(&ctx);
  if (got != 9) {
    fprintf(
      stderr, "unit_regress_ai_tables: max_landing=%d want 9 (catalog MoW holds)\n", got
    );
    return 1;
  }
  return 0;
}

/*
 * bugs.md #874 (FUN_43f7_2022 raw 75007): one if/else over
 * `(0x5382&2)==0 || 0x53e6==0` — the free intervention drain and the
 * mercenary offer are exclusive within the same beat. The port used to run
 * the drain (ai_king_10f0_land) unconditionally first, so a drain that took
 * backup_force[2] 1 -> 0 let ai_king_merc_offer see mow_pool == 0 on its own
 * gate and fire in the same turn. Guard: force backup_force[2] 1 -> 0 via
 * the drain and assert no AI_POPUP_TAG_KING_MERC offer is queued that beat.
 * Deterministic seed 1 makes the merc roll's own 1-in-3 chance (the first
 * dos_rng draw) pass, so a reverted fix (both calls running) would enqueue
 * the offer and this case would catch it.
 */
static int case_874_drain_and_merc_offer_exclusive(void) {
  ColonizeCol1Save col1;
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  EuropeScreen europe;
  ColonizeDosRng rng;
  AiPopupState pop;
  uint16_t year = 1600;
  uint16_t autumn = 0;
  uint32_t turn = 1;
  char status[128];

  units_set_occupancy_map(NULL);
  units_reset_state();
  units_reset_hooks();
  colonies_set_occupancy_map(NULL);
  turn_reset();
  ai_euro_reset();
  ai_native_reset();

  col1_save_init(&col1);
  col1.head.difficulty = 0;
  ai_king_latch_clear(&col1);
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    memset(&col1.nation[i], 0, sizeof(col1.nation[i]));
  }
  col1.head.game_options.woi = 1; /* independence declared */
  col1.head.colony_count = 1;
  col1.colony = calloc(1, sizeof(ColonizeCol1Colony));
  int rc = 0;
  if (!col1.colony) {
    col1_save_free(&col1);
    return 1;
  }
  col1.colony[0].nation_id = 0;
  col1.colony[0].x = 5;
  col1.colony[0].y = 5;
  col1.colony[0].population = 4;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;
  col1.colony[0].flags.coastal = 1;
  snprintf(col1.colony[0].name, sizeof(col1.colony[0].name), "Jamestown");

  if (!fx_map_alloc(&map, 16, 16, 1, false)) {
    col1_save_free(&col1);
    return 1;
  }
  for (int i = 0; i < 16 * 16; ++i) {
    map.layer3[i] = 1; /* region 1 = open ocean, not a lake */
  }
  map.terrain[5 * 16 + 4] = 25; /* ocean tile west of the colony */

  fx_units_init(&units);
  units.type_count = 9;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].attack = 3;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Man-O-War");
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].cargo = 6;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Dragoon");
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Artillery");
  units.types[3].attack = 7;
  units.types[3].defense = 5;
  snprintf(units.types[4].name, sizeof(units.types[4].name), "Soldier");
  units.types[4].attack = 2;
  units.types[4].defense = 2;
  snprintf(units.types[5].name, sizeof(units.types[5].name), "Continental Army");
  units.types[5].attack = 4;
  units.types[5].defense = 4;
  snprintf(units.types[6].name, sizeof(units.types[6].name), "Continental Cavalry");
  units.types[6].attack = 5;
  units.types[6].defense = 5;
  snprintf(units.types[7].name, sizeof(units.types[7].name), "Veteran Soldier");
  units.types[7].attack = 3;
  units.types[7].defense = 3;
  snprintf(units.types[8].name, sizeof(units.types[8].name), "Cavalry");
  units.types[8].attack = 6;
  units.types[8].defense = 6;

  fx_colonies_init(&colonies);
  colonies.colonies[0].id = 0;
  colonies.colonies[0].active = true;
  colonies.colonies[0].nation_id = 0;
  colonies.colonies[0].x = 5;
  colonies.colonies[0].y = 5;
  colonies.colonies[0].population = 4;
  colonies.colonies[0].colonist_count = 4;
  colonies.colonies[0].colony_flags |= COLONIZE_COLONY_FLAG_COASTAL;
  snprintf(colonies.colonies[0].name, sizeof(colonies.colonies[0].name), "Jamestown");
  colonies.colony_count = 1;

  memset(&europe, 0, sizeof(europe));
  /* europe_nation_gold reads EuropeScreen.gold (not col1->nation[].gold) when
   * `nation == europe_purse_nation(eu)`, which a zeroed EuropeScreen resolves
   * to nation 0 — set both so the merc-offer affordability check does not
   * silently starve the probe. */
  europe.gold = 5000000;
  memset(&pop, 0, sizeof(pop));
  status[0] = '\0';

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.messages = test_game_txt();
  ctx.names = test_names_txt();
  ctx.human_nation = 0;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.map = &map;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.turn_number = &turn;
  ctx.status = status;
  ctx.status_size = sizeof(status);
  ctx.ai_popups = &pop;

  dos_rng_seed(&rng, 1u); /* seed 1: first dos_rng_range(rng,0,2) draw == 0 */
  ctx.rng = &rng;

  /* Arm the exclusivity race: announce latch set, MoW pool == 1, and the
   * mobilization gate already cleared so the drain/merc arm actually runs
   * (bugs.md #250: the mobilization turn itself does nothing else). */
  ai_king_latch_set(&col1, AI_KING_INTERVENE_ANNOUNCED_BYTE, 1);
  col1.head.backup_force[2] = 1;
  col1.nation[0].nation_flags = (uint8_t)(col1.nation[0].nation_flags | 0x08u);
  col1.nation[0].gold = 5000000u;

  ai_king_ref_pre_euro_beat(&ctx);

  if (col1.head.backup_force[2] != 0) {
    fprintf(stderr, "unit_regress_ai_tables: #874 drain did not fire (backup_force[2]=%u)\n",
            (unsigned)col1.head.backup_force[2]);
    rc = 1;
  }
  for (int i = 0; i < pop.queue_count; ++i) {
    if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC) {
      fprintf(stderr, "unit_regress_ai_tables: #874 merc offer fired same beat as drain\n");
      rc = 1;
      break;
    }
  }

  col1_save_free(&col1);
  fx_map_free(&map);
  units_set_occupancy_map(NULL);
  colonies_set_occupancy_map(NULL);
  return rc;
}

/* #530: FUN_521d_20e6 raw 89467-89480 clears the accumulated mask
 * BEFORE the continent/latitude gate, and only act state 0x0b overrides it
 * with 0xffff for a land goal. A 0x0c step is not that goal order. */
static int case_530_unload_mask_scan_and_order(void) {
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  ColonizeCol1Save col1;
  const int nation = 1;
  if (!fx_map_alloc(&map, 16, 16, 25, false)) {
    return 1;
  }
  memset(map.layer3, 0xf1, map.tile_count);
  map.terrain[3 * 16 + 6] = 2; /* SE: eligible shore, scanned before NW. */
  map.layer3[3 * 16 + 6] = 0xf2;
  fx_units_init(&units);
  fx_colonies_init(&colonies);
  col1_save_init(&col1);
  ai_goals_reset();
  units.type_count = 2;
  units.types[0].kind_plus1 = UNITS_KIND_SOLDIER + 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
  units.types[0].movement = 1;
  units.types[1].kind_plus1 = UNITS_KIND_CARAVEL + 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[1].movement = 4;
  units.types[1].cargo = 2;
  const int ship_id = units_spawn(&units, 1, 5, 2);
  const int pax_id = units_spawn(&units, 0, 6, 3);
  ColonizeUnit* ship = units_get(&units, ship_id);
  ColonizeUnit* pax = units_get(&units, pax_id);
  int rc = 1;
  if (!ship || !pax) {
    goto done;
  }
  ship->nation_id = pax->nation_id = nation;
  if (!units_board(&units, pax_id, ship_id)) {
    goto done;
  }
  ColonizeTurnContext ctx = {
    .units = &units, .colonies = &colonies, .map = &map,
    .col1 = &col1, .col1_ok = true
  };
  ai_euro_refresh_continent_stance(&ctx, nation);
  if (ai_euro_20e6_unload_mask(&ctx, ship, nation) != 0x10) {
    fprintf(stderr, "#530: isolated eligible shore must allow military unload\n");
    goto done;
  }
  map.terrain[1 * 16 + 4] = 2; /* NW: empty land, but latitude gate fails. */
  map.layer3[1 * 16 + 4] = 0xf2;
  if (ai_euro_20e6_unload_mask(&ctx, ship, nation) != 0) {
    fprintf(stderr, "#530: last empty land tile must clear the earlier mask\n");
    goto done;
  }
  map.terrain[1 * 16 + 4] = 25;
  map.layer3[1 * 16 + 4] = 0xf1;
  ship->goto_x = 6;
  ship->goto_y = 3;
  ship->orders = AI_EURO_ACT_GOAL;
  if (ai_euro_20e6_unload_mask(&ctx, ship, nation) != 0xffff) {
    fprintf(stderr, "#530: goal order must unload all cargo on its continent\n");
    goto done;
  }
  ship->orders = AI_EURO_ACT_STEP;
  if (ai_euro_20e6_unload_mask(&ctx, ship, nation) != 0x10) {
    fprintf(stderr, "#530: step order must not force an all-cargo unload\n");
    goto done;
  }
  rc = 0;
done:
  col1_save_free(&col1);
  fx_map_free(&map);
  return rc;
}

/* FUN_6662_0f74 LAB_1599 (raw 104758), FUN_521d_5b66 5bda:
 * arrival writes facing=-1 and clears the course even with no MP left. */
static int case_530_ship_arrival_facing(void) {
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  ColonizeColonyPool colonies;
  if (!fx_map_alloc(&map, 16, 16, 25, false)) return 1;
  fx_units_init(&units);
  fx_colonies_init(&colonies);
  ai_euro_reset();
  units.type_count = 2;
  units.types[0].kind_plus1 = UNITS_KIND_CARAVEL + 1;
  units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
  units.types[0].movement = 4;
  units.types[1].kind_plus1 = UNITS_KIND_BRAVE + 1;
  units.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
  map.terrain[8 * 16 + 6] = 2;
  ColonizeUnit* brave = units_get(&units, units_spawn(&units, 1, 6, 8));
  if (!brave) { fx_map_free(&map); return 1; }
  brave->nation_id = 4;
  const int id = units_spawn(&units, 0, 7, 7);
  ColonizeUnit* u = units_get(&units, id);
  if (!u) { fx_map_free(&map); return 1; }
  u->nation_id = 1;
  ColonizeTurnContext ctx = {.units=&units, .colonies=&colonies, .map=&map};
  int rc = 0;
  for (int order = AI_EURO_ACT_GOAL; order <= AI_EURO_ACT_STEP; ++order) {
  for (int mp = 1; mp >= 0; --mp) {
    u->moves = mp;
    u->orders = order;
    u->goto_x = u->x;
    u->goto_y = u->y;
    u->last_dir = 7;
    ai_euro_s_euro_last_dir[id] = 7;
    ai_euro_act_ship_dos(&ctx, u, 1);
    if (u->orders != UNITS_ORDER_NONE || u->last_dir != -1 ||
        ai_euro_s_euro_last_dir[id] != -1 || u->moves != mp ||
        u->x != 7 || u->y != 7 || u->goto_x != 7 || u->goto_y != 7) {
      fprintf(stderr, "#530: arrival with %d MP retained order/facing or changed position/MP\n", mp);
      rc = 1;
      break;
    }
  }
  }
  fx_map_free(&map);
  return rc;
}

/* FUN_465b_0000 -> FUN_5bfb_3180, raw 98628-98646: a native step
 * wakes adjacent foreign sentries without erasing their saved goal. */
static int case_530_native_step_wakes_landfall(void) {
  int saw_move = 0;
  for (int seed = 0; seed < 24 && !saw_move; ++seed) {
    ColonizeWorldMap map;
    ColonizeUnitPool units;
    ColonizeCol1Save col1;
    ColonizeCol1Tribe tribe = {.x=5, .y=5, .nation_id=4, .population=3};
    if (!fx_map_alloc(&map, 12, 12, 0, true)) return 1;
    memset(map.layer3, 0xf0, map.tile_count);
    fx_units_init(&units);
    ai_native_reset();
    units.type_count = 2;
    for (int t = 0; t < 2; ++t) {
      units.types[t].kind_plus1 = (t ? UNITS_KIND_SOLDIER : UNITS_KIND_BRAVE) + 1;
      units.types[t].domain = COLONIZE_UNIT_DOMAIN_LAND;
      units.types[t].movement = 1;
      units.types[t].attack = units.types[t].defense = 2;
    }
    memset(&col1, 0, sizeof(col1));
    col1.tribe = &tribe;
    col1.head.tribe_count = 1;
    col1.indian[0].euro_diplo[1] = 1;
    int id = units_spawn(&units, 0, 5, 5);
    ColonizeUnit* b = units_get(&units, id);
    if (!b) { fx_map_free(&map); return 1; }
    b->nation_id = 4;
    b->home_tribe_id = 0;
    b->moves = 0;
    b->last_dir = 8;
    /* Empty immediate neighbours; sentries two tiles away ensure any
     * committed direction approaches a foreign stack without combat. */
    for (int y = 3; y <= 7; ++y) for (int x = 3; x <= 7; ++x) {
      if (x != 3 && x != 7 && y != 3 && y != 7) continue;
      ColonizeUnit* f = units_get(&units, units_spawn(&units, 1, x, y));
      if (!f) { fx_map_free(&map); return 1; }
      f->nation_id = 1;
      f->orders = UNITS_ORDER_SENTRY;
      f->goto_x = 9;
      f->goto_y = 10;
      f->moves = 0;
    }
    ColonizeDosRng rng;
    dos_rng_seed(&rng, (uint32_t)(seed * 12345 + 7));
    int steps = 0;
    (void)ai_native_brave_step(&units, &map, &col1, &rng, 4, false, b,
                              5, 5, 0, 3, 0, &steps);
    int rc = 0;
    if (b->active && (b->x != 5 || b->y != 5)) {
      saw_move = 1;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* f = &units.units[i];
        if (!f->active || f->nation_id != 1) continue;
        const int near = abs(f->x-b->x) <= 1 && abs(f->y-b->y) <= 1;
        if (f->orders != (near ? UNITS_ORDER_NONE : UNITS_ORDER_SENTRY) ||
            f->goto_x != 9 || f->goto_y != 10 || f->moves != 0) {
          fprintf(stderr, "#530: native step must wake only adjacent sentries and preserve goals/MP\n");
          rc = 1;
          break;
        }
      }
    }
    fx_map_free(&map);
    if (rc) return rc;
  }
  if (!saw_move) fprintf(stderr, "#530: native wake test produced no move\n");
  return !saw_move;
}

static const TestCase k_cases[] = {
    {"case_530_ship_arrival_facing", case_530_ship_arrival_facing},
    {"case_530_native_step_wakes_landfall", case_530_native_step_wakes_landfall},
    {"case_530_unload_mask_scan_and_order", case_530_unload_mask_scan_and_order},
    {"case_651_col5_reads_defense", case_651_col5_reads_defense},
    {"case_668_fallback_table_is_attack", case_668_fallback_table_is_attack},
    {"case_661_max_landing_reads_mow_holds", case_661_max_landing_reads_mow_holds},
    {"case_874_drain_and_merc_offer_exclusive", case_874_drain_and_merc_offer_exclusive},
};
TEST_MAIN(k_cases)
