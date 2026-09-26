/*
 * Regression guards for FIXED bugs.md rows that had no test (gap audit
 * 2026-09-26). Slice: ai_tables.
 */
#include "../common/ai_fixture.h"
#include "../common/test_catalogs.h"
#include "../common/test_runner.h"

#include "core/ai_euro.h"
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

static const TestCase k_cases[] = {
    {"case_651_col5_reads_defense", case_651_col5_reads_defense},
    {"case_668_fallback_table_is_attack", case_668_fallback_table_is_attack},
    {"case_661_max_landing_reads_mow_holds", case_661_max_landing_reads_mow_holds},
    {"case_874_drain_and_merc_offer_exclusive", case_874_drain_and_merc_offer_exclusive},
};
TEST_MAIN(k_cases)
