/* Smoke: liberty-bell threshold elects FF with manual/wiki-aligned effects. */
#include "core/ai_diplo.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "unit_founding_fathers: FAIL %s\n", msg);
  return 1;
}

static void seed_unclaimed(ColonizeCol1Save* col1) {
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1->head.founding_father[i] = -1;
  }
}

/* Conquistador @1492: human 1st=40 / 2nd=161 / 3rd=241; AI 1st=48 / 2nd=193. */
static void ff_test_calendar(ColonizeCol1Save* col1) {
  col1->head.year = 1492;
  col1->head.difficulty = 2;
  col1->player[0].control = 0;
}

int main(void) {
  {
    ColonizeCol1Save curve;
    col1_save_init(&curve);
    ff_test_calendar(&curve);
    if (founding_fathers_bells_needed(&curve, 0) != 40u) {
      return fail("bells_needed human 1st");
    }
    curve.nation[0].founding_father_count = 1;
    if (founding_fathers_bells_needed(&curve, 0) != 161u) {
      return fail("bells_needed human 2nd");
    }
    curve.nation[0].founding_father_count = 2;
    if (founding_fathers_bells_needed(&curve, 0) != 241u) {
      return fail("bells_needed human 3rd");
    }
    curve.player[1].control = 1;
    curve.nation[1].founding_father_count = 0;
    if (founding_fathers_bells_needed(&curve, 1) != 48u) {
      return fail("bells_needed AI 1st");
    }
    /* Discoverer human first half-threshold. */
    curve.head.difficulty = 0;
    curve.nation[0].founding_father_count = 0;
    if (founding_fathers_bells_needed(&curve, 0) != 24u) {
      return fail("bells_needed Discoverer 1st");
    }
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  seed_unclaimed(&col1);
  ff_test_calendar(&col1);

  ColonizeCol1Nation* nat = &col1.nation[0];
  memset(nat, 0, sizeof(*nat));
  nat->liberty_bells_total = 40;
  nat->next_founding_father = 0; /* Adam Smith */
  nat->founding_father_count = 0;
  nat->gold = 100;
  nat->current_crosses = 0;

  char status[128];
  status[0] = '\0';

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.human_nation = 0;
  ctx.col1 = &col1;
  ctx.col1_ok = true;
  ctx.status = status;
  ctx.status_size = sizeof(status);

  /* Below threshold: no elect. */
  nat->liberty_bells_total = 39;
  founding_fathers_tick(&ctx);
  if (nat->founding_father_count != 0 || col1.head.founding_father[0] != -1) {
    return fail("no elect below threshold");
  }

  /* At threshold: elect Adam Smith — ownership only (factory gate elsewhere). */
  nat->liberty_bells_total = 40;
  const uint32_t gold_smith = nat->gold;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[0] != 0) {
    return fail("head.founding_father[0] not human");
  }
  if (nat->founding_father_count != 1) {
    return fail("founding_father_count not 1");
  }
  if ((nat->founding_fathers[0] & 1u) == 0) {
    return fail("bitmask bit 0 unset");
  }
  if (!founding_fathers_nation_has(&col1, 0, FF_ADAM_SMITH)) {
    return fail("nation_has Smith false");
  }
  if (nat->gold != gold_smith) {
    return fail("Smith must not invent gold");
  }
  if (nat->next_founding_father != -1) {
    return fail("next_founding_father not cleared to -1 after elect");
  }
  if (strstr(status, "Founding Father elected") == NULL) {
    return fail("status line missing");
  }
  if (nat->liberty_bells_total != 40) {
    return fail("bells were spent (expected gate-only)");
  }

  /* Jakob Fugger: clear ALL boycotts; no gold bump. */
  nat->liberty_bells_total = 161;
  nat->boycott_bitmap = (uint16_t)((1u << 1) | (1u << 4) | (1u << 2));
  col1.head.unknown46[2] = 1;
  const uint32_t gold_before = nat->gold;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[1] != 0 || nat->founding_father_count != 2) {
    return fail("second FF not Jakob Fugger");
  }
  if (nat->gold != gold_before) {
    return fail("Fugger must not invent gold");
  }
  if (nat->boycott_bitmap != 0) {
    return fail("Fugger did not clear all boycott bits");
  }
  if (col1.head.unknown46[2] != 0) {
    return fail("Fugger did not clear human unknown46[2] king refuse");
  }

  /* Brewster: pool filter flag; no crosses / free-colonist spawn fiction. */
  nat->liberty_bells_total = 241;
  nat->next_founding_father = 20;
  EuropeScreen eu_brew;
  memset(&eu_brew, 0, sizeof(eu_brew));
  snprintf(eu_brew.pool[0].name, sizeof(eu_brew.pool[0].name), "Petty Criminals");
  eu_brew.pool[0].profession = 26;
  eu_brew.pool[0].filled = true;
  eu_brew.pool[1].filled = false;
  eu_brew.pool[2].filled = false;
  ctx.europe = &eu_brew;
  const uint16_t crosses_before = nat->current_crosses;
  const uint32_t gold_brew = nat->gold;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[20] != 0 || nat->founding_father_count != 3) {
    return fail("Brewster not elected via next");
  }
  if (nat->current_crosses != crosses_before) {
    return fail("Brewster must not invent crosses");
  }
  if (nat->gold != gold_brew) {
    return fail("Brewster must not invent gold");
  }
  if (!eu_brew.brewster_no_criminals) {
    return fail("Brewster flag not set on Europe");
  }
  if (eu_brew.dock_count != 0) {
    return fail("Brewster must not spawn Free Colonist on dock");
  }
  if (eu_brew.pool[0].filled &&
      (eu_brew.pool[0].profession == 26 || strstr(eu_brew.pool[0].name, "Criminal"))) {
    return fail("Brewster left criminal in pool");
  }
  /* Dock Indentured → Free Colonists (starters may predate elect). */
  {
    EuropeScreen eu_dock;
    memset(&eu_dock, 0, sizeof(eu_dock));
    eu_dock.dock_count = 1;
    eu_dock.dock[0].present = true;
    snprintf(eu_dock.dock[0].name, sizeof(eu_dock.dock[0].name), "Indentured Servants");
    eu_dock.dock[0].profession = COLONIZE_PROF_INDENTURED;
    ColonizeCol1Save bcol1;
    col1_save_init(&bcol1);
    for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
      bcol1.head.founding_father[i] = -1;
    }
    ColonizeCol1Nation* bnat = &bcol1.nation[0];
    memset(bnat, 0, sizeof(*bnat));
    ff_test_calendar(&bcol1);
    bnat->liberty_bells_total = 40;
    bnat->next_founding_father = FF_WILLIAM_BREWSTER;
    ColonizeTurnContext bctx;
    memset(&bctx, 0, sizeof(bctx));
    bctx.human_nation = 0;
    bctx.col1 = &bcol1;
    bctx.col1_ok = true;
    bctx.europe = &eu_dock;
    founding_fathers_tick(&bctx);
    if (!founding_fathers_nation_has(&bcol1, 0, FF_WILLIAM_BREWSTER)) {
      return fail("Brewster dock-filter elect");
    }
    if (eu_dock.dock[0].profession != COLONIZE_PROF_FREE_COLONIST ||
        strstr(eu_dock.dock[0].name, "Free") == NULL) {
      return fail("Brewster must convert Indentured dock to Free Colonists");
    }
  }
  ctx.europe = NULL;

  /* Jefferson: elect only — production +50% on statesmen is turn/prod path. */
  nat->liberty_bells_total = 321;
  nat->next_founding_father = 15;
  const uint16_t bells_before = nat->liberty_bells_total;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[15] != 0 || nat->founding_father_count != 4) {
    return fail("Jefferson not elected via next");
  }
  if (nat->liberty_bells_total != bells_before) {
    return fail("Jefferson must not invent bells");
  }
  if (!founding_fathers_nation_has(&col1, 0, FF_THOMAS_JEFFERSON)) {
    return fail("Jefferson nation_has false");
  }

  /* de Witt: elect only — no tax fiction. */
  nat->tax_rate = 12;
  nat->liberty_bells_total = 401;
  nat->next_founding_father = 4;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[4] != 0 || nat->founding_father_count != 5) {
    return fail("de Witt not elected via next");
  }
  if (nat->tax_rate != 12) {
    return fail("de Witt must not invent tax cut");
  }

  /* Washington: ownership flag only — no mass promote / REF−1. */
  col1.head.expeditionary_force[0] = 5;
  nat->liberty_bells_total = 481;
  nat->next_founding_father = 11;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[11] != 0 || nat->founding_father_count != 6) {
    return fail("Washington not elected via next");
  }
  if (col1.head.expeditionary_force[0] != 5) {
    return fail("Washington must not invent REF−1");
  }

  /* Stuyvesant: elect only (Custom House gate elsewhere) — no gold. */
  nat->liberty_bells_total = 561;
  nat->next_founding_father = 3;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[3] != 0 || nat->founding_father_count != 7) {
      return fail("Stuyvesant not elected via next");
    }
    if (nat->gold != g0) {
      return fail("Stuyvesant must not invent gold");
    }
  }

  /* Drake: ownership flag only — no sea-moves / gold fiction. */
  nat->liberty_bells_total = 641;
  nat->next_founding_father = 13;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[13] != 0 || nat->founding_father_count != 8) {
      return fail("Drake not elected via next");
    }
    if (nat->gold != g0) {
      return fail("Drake must not invent gold");
    }
  }

  /* Revere: ownership flag only on elect — no tools / gold fiction. */
  nat->liberty_bells_total = 721;
  nat->next_founding_father = 12;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[12] != 0 || nat->founding_father_count != 9) {
      return fail("Revere not elected via next");
    }
    if (nat->gold != g0) {
      return fail("Revere must not invent gold");
    }
  }

  /* Bolivar without Col1 colonies: elect only, no bells fiction. */
  nat->liberty_bells_total = 801;
  nat->next_founding_father = 18;
  {
    const uint16_t b0 = nat->liberty_bells_total;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[18] != 0 || nat->founding_father_count != 10) {
      return fail("Bolivar not elected via next");
    }
    if (nat->liberty_bells_total != b0) {
      return fail("Bolivar must not invent bells");
    }
  }

  /* Pocahontas: reset native tension to content; no crosses fiction.
   * Half-rate alarm growth wired in ai_contact (unit_ai_contact). */
  nat->liberty_bells_total = 881;
  nat->next_founding_father = 16;
  {
    ColonizeCol1Tribe tribes[2];
    memset(tribes, 0, sizeof(tribes));
    tribes[0].nation_id = 4;
    tribes[0].alarm[0].friction = 80;
    tribes[0].alarm[0].attacks = 3;
    tribes[0].alarm[1].friction = 40;
    tribes[1].nation_id = 5;
    tribes[1].alarm[0].friction = 55;
    col1.tribe = tribes;
    col1.head.tribe_count = 2;
    col1.indian[0].alarm_by_player[0] = 90;
    col1.indian[0].alarm_by_player[1] = 20;
    col1.indian[1].alarm_by_player[0] = 70;
    const uint16_t c0 = nat->current_crosses;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[16] != 0 || nat->founding_father_count != 11) {
      return fail("Pocahontas not elected via next");
    }
    if (nat->current_crosses != c0) {
      return fail("Pocahontas must not invent crosses");
    }
    if (tribes[0].alarm[0].friction != 0 || tribes[0].alarm[0].attacks != 0 ||
        tribes[1].alarm[0].friction != 0) {
      return fail("Pocahontas must zero own-nation tribe friction");
    }
    if (tribes[0].alarm[1].friction != 40) {
      return fail("Pocahontas must not clear other nations' friction");
    }
    if (col1.indian[0].alarm_by_player[0] != 0 || col1.indian[1].alarm_by_player[0] != 0) {
      return fail("Pocahontas must zero own-nation indian alarm");
    }
    if (col1.indian[0].alarm_by_player[1] != 20) {
      return fail("Pocahontas must not clear other nations' indian alarm");
    }
    col1.tribe = NULL;
    col1.head.tribe_count = 0;
  }

  /* Coronado without map: elect only — no gold fallback. */
  nat->liberty_bells_total = 961;
  nat->next_founding_father = 6;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[6] != 0 || nat->founding_father_count != 12) {
      return fail("Coronado not elected via next");
    }
    if (nat->gold != g0) {
      return fail("Coronado must not invent gold fallback");
    }
  }

  /* Jones without units/map: elect only — no gold fallback. */
  nat->liberty_bells_total = 1041;
  nat->next_founding_father = 14;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[14] != 0 || nat->founding_father_count != 13) {
      return fail("Jones not elected via next");
    }
    if (nat->gold != g0) {
      return fail("Jones must not invent gold fallback");
    }
  }

  /* Brebeuf: ownership gate — no elect crosses fiction. */
  nat->liberty_bells_total = 1121;
  nat->next_founding_father = 22;
  {
    const uint16_t c0 = nat->current_crosses;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[22] != 0 || nat->founding_father_count != 14) {
      return fail("Brebeuf not elected via next");
    }
    if (nat->current_crosses != c0) {
      return fail("Brebeuf must not invent crosses");
    }
    if (!founding_fathers_brebeuf_missionaries_are_experts(&col1, 0)) {
      return fail("Brebeuf ownership gate false after elect");
    }
    if (founding_fathers_brebeuf_missionaries_are_experts(&col1, 1)) {
      return fail("Brebeuf ownership must not leak to other nation");
    }
  }

  /* --- Deeper hooks: Coronado reveal + Magellan permanent + Jones + Bolivar. --- */
  {
    ColonizeCol1Save deep_col1;
    col1_save_init(&deep_col1);
    seed_unclaimed(&deep_col1);

    ColonizeCol1Nation* dnat = &deep_col1.nation[0];
    memset(dnat, 0, sizeof(*dnat));
    dnat->founding_father_count = 0;
    dnat->gold = 100;
    dnat->liberty_bells_total = 40;
    dnat->next_founding_father = 6; /* Coronado */

    char err[64];
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_alloc(&map, 16, 16, err, sizeof(err))) {
      return fail("deep map_alloc");
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = 1; /* land */
    }
    map.terrain[5 * 16 + 4] = 25; /* ocean west of colony */

    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name), "Stockade");
    colonies.building_type_count = 1;
    ColonizeColony* col = &colonies.colonies[0];
    col->id = 0;
    col->active = true;
    col->nation_id = 0;
    col->x = 5;
    col->y = 5;
    col->population = 4;
    col->colonist_count = 4;
    col->stock[COLONIZE_CARGO_TOOLS] = 10;
    col->stock[COLONIZE_CARGO_FURS] = 5;
    colonies.colony_count = 1;

    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 5;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Free Colonist");
    units.types[0].movement = 1;
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(units.types[1].name, sizeof(units.types[1].name), "Frigate");
    units.types[1].movement = 6;
    units.types[1].domain = COLONIZE_UNIT_DOMAIN_SEA;
    units.types[1].cargo = 4;
    snprintf(units.types[2].name, sizeof(units.types[2].name), "Caravel");
    units.types[2].movement = 4;
    units.types[2].domain = COLONIZE_UNIT_DOMAIN_SEA;
    units.types[2].cargo = 2;
    snprintf(units.types[3].name, sizeof(units.types[3].name), "Soldier");
    units.types[3].movement = 1;
    units.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(units.types[4].name, sizeof(units.types[4].name), "Veteran Soldier");
    units.types[4].movement = 1;
    units.types[4].domain = COLONIZE_UNIT_DOMAIN_LAND;

    const int land_id = units_spawn_allow_stack(&units, 0, 8, 8);
    if (land_id < 0) {
      map_free(&map);
      return fail("deep land spawn");
    }
    ColonizeUnit* land = units_get(&units, land_id);
    land->nation_id = 0;
    land->moves_left = 1;

    const int sol_id = units_spawn_allow_stack(&units, 3, 6, 6);
    if (sol_id < 0) {
      map_free(&map);
      return fail("deep Soldier spawn");
    }
    ColonizeUnit* soldier = units_get(&units, sol_id);
    soldier->nation_id = 0;
    soldier->moves_left = 1;
    const int soldier_type = soldier->type_index;

    const int car_id = units_spawn_allow_stack(&units, 2, 4, 5);
    if (car_id < 0) {
      map_free(&map);
      return fail("deep caravel spawn");
    }
    ColonizeUnit* caravel = units_get(&units, car_id);
    caravel->nation_id = 0;
    caravel->moves_left = 4;

    deep_col1.head.colony_count = 1;
    deep_col1.colony = calloc(1, sizeof(ColonizeCol1Colony));
    if (!deep_col1.colony) {
      map_free(&map);
      return fail("deep Col1 colony alloc");
    }
    deep_col1.colony[0].nation_id = 0;
    deep_col1.colony[0].x = 5;
    deep_col1.colony[0].y = 5;
    deep_col1.colony[0].population = 4;
    deep_col1.colony[0].rebel_dividend = 40;
    deep_col1.colony[0].rebel_divisor = 100;

    EuropeScreen europe;
    memset(&europe, 0, sizeof(europe));
    europe.dock_count = 0;
    europe.gold = (int)dnat->gold;

    ColonizeTurnContext deep_ctx;
    memset(&deep_ctx, 0, sizeof(deep_ctx));
    deep_ctx.human_nation = 0;
    deep_ctx.col1 = &deep_col1;
    deep_ctx.col1_ok = true;
    deep_ctx.map = &map;
    deep_ctx.colonies = &colonies;
    deep_ctx.units = &units;
    deep_ctx.europe = &europe;

    const uint32_t gold_pre_cor = dnat->gold;
    if (map_tile_seen_by(&map, 5, 5, 0) || map_tile_seen_by(&map, 7, 5, 0)) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep map unexpectedly seen before Coronado");
    }
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[6] != 0 || dnat->founding_father_count != 1) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Coronado not elected");
    }
    if (dnat->gold != gold_pre_cor) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Coronado should not gold-fallback");
    }
    if (!map_tile_seen_by(&map, 5, 5, 0) || !map_tile_seen_by(&map, 7, 5, 0)) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Coronado reveal missing");
    }
    if (map_tile_seen_by(&map, 8, 5, 0)) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Coronado revealed beyond radius 2");
    }

    /* Magellan: +1 moves_left now; refresh keeps permanent +1. */
    dnat->liberty_bells_total = 161;
    dnat->next_founding_father = 5;
    const int car_moves = caravel->moves_left;
    const uint32_t gold_pre_mag = dnat->gold;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[5] != 0 || dnat->founding_father_count != 2) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Magellan not elected");
    }
    if (caravel->moves_left != car_moves + 1) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Magellan sea moves +1 missing");
    }
    if (dnat->gold != gold_pre_mag) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Magellan should not gold-fallback");
    }
    if (land->moves_left != 1) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Magellan bumped land unit");
    }
    turn_refresh_moves_for_nation(&units, 0, &deep_col1, NULL, NULL, NULL, NULL);
    if (caravel->moves_left != units.types[2].movement + 1) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Magellan permanent refresh +1 missing");
    }

    /* Hudson: ownership only (fur +100% in turn harvest) — no stock dump. */
    dnat->liberty_bells_total = 241;
    dnat->next_founding_father = 8;
    const int tools_h = col->stock[COLONIZE_CARGO_TOOLS];
    const int furs_h = col->stock[COLONIZE_CARGO_FURS];
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[8] != 0 || dnat->founding_father_count != 3) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Hudson not elected");
    }
    if (col->stock[COLONIZE_CARGO_TOOLS] != tools_h || col->stock[COLONIZE_CARGO_FURS] != furs_h) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Hudson must not dump tools/furs stock");
    }
    if (!founding_fathers_nation_has(&deep_col1, 0, FF_HENRY_HUDSON)) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Hudson nation_has false");
    }

    /* de Soto: land reveal; no crosses fallback. */
    dnat->liberty_bells_total = 321;
    dnat->next_founding_father = 7;
    const uint16_t crosses_pre = dnat->current_crosses;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[7] != 0 || dnat->founding_father_count != 4) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep de Soto not elected");
    }
    if (!map_tile_seen_by(&map, 8, 8, 0) || !map_tile_seen_by(&map, 9, 8, 0)) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep de Soto land reveal missing");
    }
    if (dnat->current_crosses != crosses_pre) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep de Soto should not crosses-fallback");
    }

    /* Jones: free Frigate. */
    dnat->liberty_bells_total = 401;
    dnat->next_founding_father = 14;
    const int units_before = units.unit_count;
    const uint32_t gold_pre_jones = dnat->gold;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[14] != 0 || dnat->founding_father_count != 5) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Jones not elected");
    }
    if (dnat->gold != gold_pre_jones) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Jones should not gold-fallback");
    }
    if (units.unit_count != units_before + 1) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Jones did not spawn ship");
    }

    /* Washington: no mass promote. */
    deep_col1.head.expeditionary_force[0] = 3;
    dnat->liberty_bells_total = 481;
    dnat->next_founding_father = 11;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[11] != 0 || dnat->founding_father_count != 6) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Washington not elected");
    }
    if (soldier->type_index != soldier_type) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Washington must not mass-promote on elect");
    }
    if (deep_col1.head.expeditionary_force[0] != 3) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Washington must not REF−1");
    }

    /* Revere: no tools dump. */
    dnat->liberty_bells_total = 561;
    dnat->next_founding_father = 12;
    const int tools_pre_rev = col->stock[COLONIZE_CARGO_TOOLS];
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[12] != 0 || dnat->founding_father_count != 7) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Revere not elected");
    }
    if (col->stock[COLONIZE_CARGO_TOOLS] != tools_pre_rev) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Revere must not dump tools");
    }

    /* Drake: no sea-moves bump. */
    dnat->liberty_bells_total = 641;
    dnat->next_founding_father = 13;
    const int car_moves_pre_drake = caravel->moves_left;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[13] != 0 || dnat->founding_father_count != 8) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Drake not elected");
    }
    if (caravel->moves_left != car_moves_pre_drake) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Drake must not bump sea moves");
    }

    /* Smith: no gold/tools fiction. */
    dnat->liberty_bells_total = 721;
    dnat->next_founding_father = 0;
    {
      const uint32_t g0 = dnat->gold;
      const int tools0 = col->stock[COLONIZE_CARGO_TOOLS];
      founding_fathers_tick(&deep_ctx);
      if (deep_col1.head.founding_father[0] != 0 || dnat->founding_father_count != 9) {
        free(deep_col1.colony);
        map_free(&map);
        return fail("deep Smith not elected");
      }
      if (dnat->gold != g0 || col->stock[COLONIZE_CARGO_TOOLS] != tools0) {
        free(deep_col1.colony);
        map_free(&map);
        return fail("deep Smith must not invent gold/tools");
      }
    }

    /* La Salle: Stockade on pop>=3 colony. */
    dnat->liberty_bells_total = 801;
    dnat->next_founding_father = 9;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[9] != 0 || dnat->founding_father_count != 10) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep La Salle not elected");
    }
    if (!col->has_building[0]) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep La Salle Stockade missing");
    }

    /* Bolivar: display-time SoL +20 (40→60); storage unchanged; no bells fiction. */
    dnat->liberty_bells_total = 881;
    dnat->next_founding_father = 18;
    {
      const uint16_t b0 = dnat->liberty_bells_total;
      const uint32_t div0 = deep_col1.colony[0].rebel_dividend;
      founding_fathers_tick(&deep_ctx);
      if (deep_col1.head.founding_father[18] != 0 || dnat->founding_father_count != 11) {
        free(deep_col1.colony);
        map_free(&map);
        return fail("deep Bolivar not elected");
      }
      if (dnat->liberty_bells_total != b0) {
        free(deep_col1.colony);
        map_free(&map);
        return fail("deep Bolivar must not invent bells");
      }
      if (deep_col1.colony[0].rebel_dividend != div0) {
        free(deep_col1.colony);
        map_free(&map);
        return fail("deep Bolivar must not mutate rebel_dividend");
      }
      if (colony_prod_sol_percent(&deep_col1, col) != 60) {
        free(deep_col1.colony);
        map_free(&map);
        return fail("deep Bolivar display SoL want 60");
      }
    }

    free(deep_col1.colony);
    map_free(&map);
  }

  /* --- AI Euro nation elect (control==1). --- */
  {
    ColonizeCol1Save ai_col1;
    col1_save_init(&ai_col1);
    seed_unclaimed(&ai_col1);
    ai_col1.player[0].control = 0;
    ai_col1.player[1].control = 1;
    ai_col1.player[2].control = 2;
    ai_col1.player[3].control = 1;

    ColonizeCol1Nation* human = &ai_col1.nation[0];
    ColonizeCol1Nation* ai = &ai_col1.nation[1];
    ColonizeCol1Nation* withdrawn = &ai_col1.nation[2];
    memset(human, 0, sizeof(*human));
    memset(ai, 0, sizeof(*ai));
    memset(withdrawn, 0, sizeof(*withdrawn));

    human->liberty_bells_total = 0;
    human->next_founding_father = 0;

    ff_test_calendar(&ai_col1);
    ai_col1.player[1].control = 1;
    ai_col1.player[2].control = 2;
    ai_col1.player[3].control = 1;
    ai->liberty_bells_total = 48;
    ai->next_founding_father = 2; /* Peter Minuit — elect only, no gold invent */
    ai->founding_father_count = 0;
    ai->gold = 10;

    withdrawn->liberty_bells_total = 48;
    withdrawn->next_founding_father = 3;

    ColonizeTurnContext ai_ctx;
    memset(&ai_ctx, 0, sizeof(ai_ctx));
    ai_ctx.human_nation = 0;
    ai_ctx.col1 = &ai_col1;
    ai_ctx.col1_ok = true;

    founding_fathers_tick(&ai_ctx);

    if (human->founding_father_count != 0) {
      return fail("AI tick elected for human below threshold");
    }
    if (ai_col1.head.founding_father[2] != 1) {
      return fail("AI nation did not elect Minuit");
    }
    if (ai->founding_father_count != 1) {
      return fail("AI founding_father_count not 1");
    }
    if (ai->gold != 10u) {
      return fail("AI Minuit must not invent gold");
    }
    if (withdrawn->founding_father_count != 0 || ai_col1.head.founding_father[3] != -1) {
      return fail("withdrawn nation elected FF");
    }

    ai->liberty_bells_total = 193;
    ai->next_founding_father = 1;
    ai->boycott_bitmap = (uint16_t)((1u << 1) | (1u << 4) | (1u << 7));
    ai_col1.head.unknown46[2] = 1;
    const uint32_t ai_gold_before = ai->gold;
    founding_fathers_tick(&ai_ctx);
    if (ai_col1.head.founding_father[1] != 1 || ai->founding_father_count != 2) {
      return fail("AI second elect not Fugger");
    }
    if (ai->gold != ai_gold_before) {
      return fail("AI Fugger must not invent gold");
    }
    if (ai->boycott_bitmap != 0) {
      return fail("AI Fugger did not clear all boycott bits");
    }
    if (ai_col1.head.unknown46[2] != 1) {
      return fail("AI Fugger cleared human unknown46[2]");
    }
  }

  /* --- Combat hooks: Washington promote-on-win, Drake +50%, Revere helper. --- */
  {
    ColonizeCol1Save ccol1;
    col1_save_init(&ccol1);
    seed_unclaimed(&ccol1);
    ccol1.head.founding_father[FF_GEORGE_WASHINGTON] = 0;
    ccol1.head.founding_father[FF_FRANCIS_DRAKE] = 0;
    ccol1.head.founding_father[FF_PAUL_REVERE] = 0;
    ccol1.nation[0].founding_fathers[FF_GEORGE_WASHINGTON / 8] |=
      (uint8_t)(1u << (FF_GEORGE_WASHINGTON % 8));
    ccol1.nation[0].founding_fathers[FF_FRANCIS_DRAKE / 8] |=
      (uint8_t)(1u << (FF_FRANCIS_DRAKE % 8));
    ccol1.nation[0].founding_fathers[FF_PAUL_REVERE / 8] |=
      (uint8_t)(1u << (FF_PAUL_REVERE % 8));

    /* Revere helper: flag + no soldier + muskets stock. */
    if (!founding_fathers_revere_should_auto_arm(&ccol1, 0, false, 50)) {
      return fail("Revere helper should auto-arm with muskets and no soldier");
    }
    if (founding_fathers_revere_should_auto_arm(&ccol1, 0, true, 50)) {
      return fail("Revere helper must not arm when soldier present");
    }
    if (founding_fathers_revere_should_auto_arm(&ccol1, 0, false, 49)) {
      return fail("Revere helper must not arm without equip muskets");
    }
    if (founding_fathers_revere_should_auto_arm(&ccol1, 1, false, 50)) {
      return fail("Revere helper must require owning nation");
    }

    ColonizeUnitPool upool;
    units_reset(&upool);
    upool.type_count = 6;
    snprintf(upool.types[0].name, sizeof(upool.types[0].name), "Soldier");
    upool.types[0].attack = 2;
    upool.types[0].defense = 2;
    upool.types[0].movement = 1;
    upool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(upool.types[1].name, sizeof(upool.types[1].name), "Veteran Soldier");
    upool.types[1].attack = 3;
    upool.types[1].defense = 3;
    upool.types[1].movement = 1;
    upool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(upool.types[2].name, sizeof(upool.types[2].name), "Dragoon");
    upool.types[2].attack = 3;
    upool.types[2].defense = 3;
    upool.types[2].movement = 4;
    upool.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(upool.types[3].name, sizeof(upool.types[3].name), "Veteran Dragoon");
    upool.types[3].attack = 4;
    upool.types[3].defense = 4;
    upool.types[3].movement = 4;
    upool.types[3].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(upool.types[4].name, sizeof(upool.types[4].name), "Privateer");
    upool.types[4].attack = 8;
    upool.types[4].defense = 8;
    upool.types[4].movement = 8;
    upool.types[4].domain = COLONIZE_UNIT_DOMAIN_SEA;
    snprintf(upool.types[5].name, sizeof(upool.types[5].name), "Frigate");
    upool.types[5].attack = 16;
    upool.types[5].defense = 16;
    upool.types[5].movement = 6;
    upool.types[5].domain = COLONIZE_UNIT_DOMAIN_SEA;

    /* Washington: non-vet Soldier win → Veteran Soldier type. */
    {
      const int aid = units_spawn_allow_stack(&upool, 0, 1, 1);
      const int did = units_spawn_allow_stack(&upool, 0, 2, 2);
      if (aid < 0 || did < 0) {
        return fail("Washington combat spawn");
      }
      ColonizeUnit* aw = units_get(&upool, aid);
      ColonizeUnit* dw = units_get(&upool, did);
      aw->nation_id = 0;
      aw->profession = UNITS_JOB_NONE;
      dw->nation_id = 1;
      dw->profession = UNITS_JOB_NONE;
      /* Equal attack/defense; NULL rng → attacker wins when attack >= defense. */
      if (!units_resolve_land_combat_ff(&upool, aid, did, NULL, &ccol1)) {
        return fail("Washington land combat attacker should win");
      }
      aw = units_get(&upool, aid);
      if (!aw || aw->type_index != 1) {
        return fail("Washington should promote Soldier → Veteran Soldier");
      }
      if (aw->profession != UNITS_JOB_SOLDIER) {
        return fail("Washington should set Veteran Soldier profession");
      }
    }

    /* Washington: already Veteran — no further type change. */
    {
      const int aid = units_spawn_allow_stack(&upool, 1, 3, 3);
      const int did = units_spawn_allow_stack(&upool, 0, 4, 4);
      if (aid < 0 || did < 0) {
        return fail("Washington vet spawn");
      }
      ColonizeUnit* aw = units_get(&upool, aid);
      ColonizeUnit* dw = units_get(&upool, did);
      aw->nation_id = 0;
      aw->profession = UNITS_JOB_SOLDIER;
      dw->nation_id = 1;
      if (!units_resolve_land_combat_ff(&upool, aid, did, NULL, &ccol1)) {
        return fail("Washington vet combat should win");
      }
      aw = units_get(&upool, aid);
      if (!aw || aw->type_index != 1) {
        return fail("Washington must not re-promote Veteran");
      }
    }

    /* Washington: Dragoon → Veteran Dragoon. */
    {
      const int aid = units_spawn_allow_stack(&upool, 2, 5, 5);
      const int did = units_spawn_allow_stack(&upool, 0, 6, 6);
      if (aid < 0 || did < 0) {
        return fail("Washington Dragoon spawn");
      }
      ColonizeUnit* aw = units_get(&upool, aid);
      ColonizeUnit* dw = units_get(&upool, did);
      aw->nation_id = 0;
      aw->profession = UNITS_JOB_NONE;
      dw->nation_id = 1;
      if (!units_resolve_land_combat_ff(&upool, aid, did, NULL, &ccol1)) {
        return fail("Washington Dragoon combat should win");
      }
      aw = units_get(&upool, aid);
      if (!aw || aw->type_index != 3) {
        return fail("Washington should promote Dragoon → Veteran Dragoon");
      }
    }

    /* Washington without col1: no promote. */
    {
      const int aid = units_spawn_allow_stack(&upool, 0, 7, 7);
      const int did = units_spawn_allow_stack(&upool, 0, 8, 8);
      if (aid < 0 || did < 0) {
        return fail("Washington NULL-col1 spawn");
      }
      ColonizeUnit* aw = units_get(&upool, aid);
      ColonizeUnit* dw = units_get(&upool, did);
      aw->nation_id = 0;
      aw->profession = UNITS_JOB_NONE;
      dw->nation_id = 1;
      units_set_ff_col1(NULL);
      if (!units_resolve_land_combat_ff(&upool, aid, did, NULL, NULL)) {
        return fail("NULL-col1 land combat should win");
      }
      aw = units_get(&upool, aid);
      if (!aw || aw->type_index != 0) {
        return fail("NULL col1 must not Washington-promote");
      }
    }

    /* Washington via AI wrapper: units_resolve_land_combat uses g_units_ff_col1. */
    {
      const int aid = units_spawn_allow_stack(&upool, 0, 9, 9);
      const int did = units_spawn_allow_stack(&upool, 0, 9, 10);
      if (aid < 0 || did < 0) {
        return fail("Washington wrapper spawn");
      }
      ColonizeUnit* aw = units_get(&upool, aid);
      ColonizeUnit* dw = units_get(&upool, did);
      aw->nation_id = 0;
      aw->profession = UNITS_JOB_NONE;
      dw->nation_id = 1;
      units_set_ff_col1(&ccol1);
      if (!units_resolve_land_combat(&upool, aid, did, NULL)) {
        units_set_ff_col1(NULL);
        return fail("Washington wrapper combat attacker should win");
      }
      units_set_ff_col1(NULL);
      aw = units_get(&upool, aid);
      if (!aw || aw->type_index != 1) {
        return fail("Washington wrapper should promote via g_units_ff_col1");
      }
    }

    /* Drake: Privateer attack 8 → 12 vs Frigate def 16; without Drake loses
     * (8 < 16); with Drake wins (12 vs 16 still loses on >= … wait 12 < 16).
     * Use weaker foe: Frigate def scaled down via custom — give foe Caravel-like
     * defense by using another Privateer without Drake (def 8).
     * With Drake atk 12 >= def 8 → win; without Drake atk 8 >= 8 → also win.
     * Better: attacker Privateer (Drake) vs Frigate: 12 vs 16 → lose on
     * deterministic (>=). Attacker Frigate vs Privateer defender with Drake:
     * atk 16 vs def 12 → win; without Drake atk 16 vs def 8 → win either way.
     * Prove multiplier: Privateer(Drake nation) defends vs equal Privateer
     * (no Drake). Attack 8 vs Drake-def 12 → attacker loses (8 < 12).
     * Same without Drake ownership on defender: 8 >= 8 → attacker wins. */
    {
      ColonizeCol1Save no_drake;
      col1_save_init(&no_drake);
      seed_unclaimed(&no_drake);
      /* Only nation 0 has Drake in ccol1; nation 1 does not. */

      const int atk1 = units_spawn_allow_stack(&upool, 4, 10, 1);
      const int def1 = units_spawn_allow_stack(&upool, 4, 10, 2);
      if (atk1 < 0 || def1 < 0) {
        return fail("Drake naval spawn A");
      }
      ColonizeUnit* a1 = units_get(&upool, atk1);
      ColonizeUnit* d1 = units_get(&upool, def1);
      a1->nation_id = 1; /* no Drake */
      d1->nation_id = 0; /* has Drake → def 8*3/2=12 */
      if (units_resolve_naval_combat_ff(&upool, atk1, def1, NULL, &ccol1)) {
        return fail("Drake defender Privateer +50% should beat equal attacker");
      }
      if (!units_get(&upool, def1) || units_get(&upool, atk1)) {
        return fail("Drake defender should survive, attacker despawned");
      }

      const int atk2 = units_spawn_allow_stack(&upool, 4, 11, 1);
      const int def2 = units_spawn_allow_stack(&upool, 4, 11, 2);
      if (atk2 < 0 || def2 < 0) {
        return fail("Drake naval spawn B");
      }
      ColonizeUnit* a2 = units_get(&upool, atk2);
      ColonizeUnit* d2 = units_get(&upool, def2);
      a2->nation_id = 1;
      d2->nation_id = 0;
      /* No col1 → no Drake bonus → attack 8 >= def 8 → attacker wins. */
      if (!units_resolve_naval_combat_ff(&upool, atk2, def2, NULL, NULL)) {
        return fail("without Drake bonus equal Privateers: attacker should win");
      }
      if (!units_get(&upool, atk2) || units_get(&upool, def2)) {
        return fail("NULL-col1 naval: attacker survives, defender gone");
      }

      /* Attacker Privateer with Drake vs equal foe without: atk 12 >= 8 → win. */
      const int atk3 = units_spawn_allow_stack(&upool, 4, 12, 1);
      const int def3 = units_spawn_allow_stack(&upool, 4, 12, 2);
      if (atk3 < 0 || def3 < 0) {
        return fail("Drake naval spawn C");
      }
      ColonizeUnit* a3 = units_get(&upool, atk3);
      ColonizeUnit* d3 = units_get(&upool, def3);
      a3->nation_id = 0;
      d3->nation_id = 1;
      if (!units_resolve_naval_combat_ff(&upool, atk3, def3, NULL, &ccol1)) {
        return fail("Drake attacker Privateer +50% should win vs equal foe");
      }
      (void)no_drake;
    }

    /* Drake via AI wrapper: units_resolve_naval_combat uses g_units_ff_col1.
     * Same prove as spawn A: equal Privateers, defender nation owns Drake →
     * def 8*3/2=12 > atk 8 → attacker loses (PEDIA/wiki +50%). */
    {
      const int atk = units_spawn_allow_stack(&upool, 4, 13, 1);
      const int def = units_spawn_allow_stack(&upool, 4, 13, 2);
      if (atk < 0 || def < 0) {
        return fail("Drake wrapper spawn");
      }
      ColonizeUnit* a = units_get(&upool, atk);
      ColonizeUnit* d = units_get(&upool, def);
      a->nation_id = 1; /* no Drake */
      d->nation_id = 0; /* has Drake → def 12 */
      units_set_ff_col1(&ccol1);
      if (units_resolve_naval_combat(&upool, atk, def, NULL)) {
        units_set_ff_col1(NULL);
        return fail("Drake wrapper defender Privateer +50% should beat equal attacker");
      }
      units_set_ff_col1(NULL);
      if (!units_get(&upool, def) || units_get(&upool, atk)) {
        return fail("Drake wrapper: defender survives, attacker despawned");
      }
    }

    /* Revere: step onto empty foreign colony → auto-arm + combat. */
    {
      char err[64];
      ColonizeWorldMap rmap;
      memset(&rmap, 0, sizeof(rmap));
      if (!map_alloc(&rmap, 8, 8, err, sizeof(err))) {
        return fail("Revere map_alloc");
      }
      for (size_t i = 0; i < rmap.tile_count; ++i) {
        rmap.terrain[i] = 1;
      }

      ColonizeColonyPool rcol;
      colonies_init(&rcol);
      ColonizeColony* colony = &rcol.colonies[0];
      colony->id = 0;
      colony->active = true;
      colony->nation_id = 0; /* owns Revere */
      colony->x = 6;
      colony->y = 6;
      colony->population = 1;
      colony->colonist_count = 1;
      colony->colonists[0].active = true;
      colony->colonists[0].unit_type_index = 0; /* Soldier fallback */
      colony->stock[COLONIZE_CARGO_MUSKETS] = 50;
      rcol.colony_count = 1;

      /* Ensure Soldiers type exists for eject. */
      if (upool.type_count < 7) {
        snprintf(upool.types[6].name, sizeof(upool.types[6].name), "Soldiers");
        upool.types[6].attack = 2;
        upool.types[6].defense = 2;
        upool.types[6].movement = 1;
        upool.types[6].domain = COLONIZE_UNIT_DOMAIN_LAND;
        upool.type_count = 7;
      }
      /* Attacker type 0: raise attack so 004a×8 beats bare-colony 015e (×1.5). */
      upool.types[0].attack = 4;
      upool.types[0].defense = 2;

      const int atk = units_spawn_allow_stack(&upool, 0, 5, 6);
      if (atk < 0) {
        map_free(&rmap);
        return fail("Revere attacker spawn");
      }
      ColonizeUnit* ra = units_get(&upool, atk);
      ra->nation_id = 1;
      ra->moves_left = 3;

      units_set_ff_col1(&ccol1);
      /* Deterministic: atk 4×8=32 >= bare-colony def ((2+4)*16)>>2=24 → win. */
      if (!units_try_move(&upool, atk, &rmap, 6, 6, &rcol, NULL)) {
        units_set_ff_col1(NULL);
        map_free(&rmap);
        return fail("Revere try_move should win vs auto-armed defender");
      }
      units_set_ff_col1(NULL);
      ra = units_get(&upool, atk);
      if (!ra || ra->x != 6 || ra->y != 6) {
        map_free(&rmap);
        return fail("Revere attacker should occupy colony tile after win");
      }
      if (colony->stock[COLONIZE_CARGO_MUSKETS] != 0) {
        map_free(&rmap);
        return fail("Revere should spend warehouse muskets");
      }
      if (colony->colonist_count != 0) {
        map_free(&rmap);
        return fail("Revere should eject the defending colonist");
      }
      if (units_last_combat_outcome() != 1) {
        map_free(&rmap);
        return fail("Revere combat outcome should be attacker win");
      }

      /* Without FF context: no auto-arm, walk onto colony with muskets intact. */
      colony->population = 1;
      colony->colonist_count = 1;
      colony->colonists[0].active = true;
      colony->colonists[0].unit_type_index = 0;
      colony->stock[COLONIZE_CARGO_MUSKETS] = 50;
      const int atk2 = units_spawn_allow_stack(&upool, 0, 7, 6);
      if (atk2 < 0) {
        map_free(&rmap);
        return fail("Revere no-ff attacker spawn");
      }
      ColonizeUnit* ra2 = units_get(&upool, atk2);
      ra2->nation_id = 1;
      ra2->moves_left = 3;
      units_set_ff_col1(NULL);
      if (!units_try_move(&upool, atk2, &rmap, 6, 6, &rcol, NULL)) {
        map_free(&rmap);
        return fail("without Revere context move onto empty colony should work");
      }
      if (colony->stock[COLONIZE_CARGO_MUSKETS] != 50 || colony->colonist_count != 1) {
        map_free(&rmap);
        return fail("without FF context must not Revere-arm");
      }
      map_free(&rmap);
    }
  }

  /* Arctic / mountain founding rejection. */
  {
    char err[64];
    ColonizeWorldMap map;
    memset(&map, 0, sizeof(map));
    if (!map_alloc(&map, 8, 8, err, sizeof(err))) {
      return fail("arctic map_alloc");
    }
    for (size_t i = 0; i < map.tile_count; ++i) {
      map.terrain[i] = 1;
    }
    map.terrain[3 * 8 + 3] = 24; /* arctic */
    map.terrain[4 * 8 + 4] = (uint8_t)(2 | 0xa0); /* mountain */
    map.terrain[5 * 8 + 5] = (uint8_t)(2 | 0x20); /* hill */
    ColonizeColonyPool pool;
    colonies_init(&pool);
    if (colonies_can_found(&pool, &map, 3, 3)) {
      map_free(&map);
      return fail("can_found allowed arctic");
    }
    if (colonies_can_found(&pool, &map, 4, 4)) {
      map_free(&map);
      return fail("can_found allowed mountain");
    }
    if (!colonies_can_found(&pool, &map, 5, 5)) {
      map_free(&map);
      return fail("can_found rejected hill");
    }
    if (!colonies_can_found(&pool, &map, 2, 2)) {
      map_free(&map);
      return fail("can_found rejected plains");
    }
    map_free(&map);
  }

  /*
   * Jefferson / Paine / Penn production multipliers (fandom_col1994.md):
   *   Jefferson — statesmen (Town Hall workers) bells ×1.5
   *   Paine — colony bells × (100+tax)/100
   *   Penn — colony crosses ×1.5
   */
  {
    ColonizeColonyPool pool;
    colonies_init(&pool);
    snprintf(pool.building_types[0].name, sizeof(pool.building_types[0].name), "Town Hall");
    snprintf(pool.building_types[1].name, sizeof(pool.building_types[1].name), "Church");
    pool.building_type_count = 2;

    ColonizeColony* col = &pool.colonies[0];
    col->id = 0;
    col->active = true;
    col->nation_id = 0;
    col->x = 2;
    col->y = 2;
    col->has_building[0] = true;
    col->has_building[1] = true;
    col->population = 2;
    col->colonist_count = 2;
    col->colonists[0].active = true;
    col->colonists[0].profession = COLONIZE_PROF_STATESMAN;
    col->colonists[0].building_type = 0; /* Town Hall */
    col->colonists[0].field_job = -1;
    col->colonists[1].active = true;
    col->colonists[1].profession = COLONIZE_PROF_PREACHER;
    col->colonists[1].building_type = 1; /* Church */
    col->colonists[1].field_job = -1;
    pool.colony_count = 1;

    /* Expert statesman in Town Hall: base 3×2=6; +Town Hall passive 1 → 7. */
    const int bells_base = colony_prod_colony_bells(&pool, col);
    if (bells_base != 7) {
      return fail("prod baseline bells (statesman+hall) unexpected");
    }
    /* Jefferson +50% on statesmen worker only: 6→9, passive 1 → 10. */
    const int bells_jeff = colony_prod_colony_bells_ff(&pool, col, 50, 0, false, 0);
    if (bells_jeff != 10) {
      return fail("Jefferson statesmen +50% bells");
    }
    /* Paine tax 20%: 7 × 120/100 = 8. */
    const int bells_paine = colony_prod_colony_bells_ff(&pool, col, 0, 20, false, 0);
    if (bells_paine != 8) {
      return fail("Paine tax% bells");
    }
    /* Both: Jefferson then media(none) then Paine: 10 × 120/100 = 12. */
    const int bells_both = colony_prod_colony_bells_ff(&pool, col, 50, 20, false, 0);
    if (bells_both != 12) {
      return fail("Jefferson+Paine combined bells");
    }
    /*
     * AI bells subsidy (FUN_15eb_1f72's flag-0x12 term, player-confirmed
     * 2026-08-15 on Viceroy: AI free-colonist Statesman nets 5 colony bells
     * vs 3 for human, same setup — see manufacturing_worker_calc_1d4c.md).
     * `bells += (pop+3)/5` on the Town Hall passive only, before
     * Jefferson/Paine/media. This fixture: colonist_count=2, so
     * pop=2, (2+3)/5=1. Baseline passive+worker was 7 (bells_base above);
     * +1 subsidy = 8.
     */
    const int bells_ai = colony_prod_colony_bells_ff(&pool, col, 0, 0, true, 0);
    if (bells_ai != 8) {
      return fail("AI bells subsidy");
    }

    /* Base crosses: 1 colony + Church passive 1 (DOS FUN_15eb_1f72: Church and
     * Cathedral are worth the same +1, not manual-sourced +2/+3 — see
     * manufacturing_worker_calc_1d4c.md) + preacher×2 on church 3→6 = 8. */
    const int crosses_base = colony_prod_colony_crosses(&pool, col);
    if (crosses_base != 8) {
      return fail("prod baseline crosses unexpected");
    }
    /* Penn +50% folds in per-Preacher-worker now, not a flat colony-total
     * multiply (DOS FUN_15eb_1d4c's Preacher body falls through into the
     * Penn check unconditionally after the Cathedral branch — see
     * manufacturing_worker_calc_1d4c.md). This fixture has no Cathedral, so
     * only the skilled worker's own 6 gets it: 6+3=9. Base(1)+church
     * passive(1, untouched by Penn)+9 = 11, not the old flat 8×1.5=12. */
    const int crosses_penn = colony_prod_colony_crosses_ff(&pool, col, true, 0);
    if (crosses_penn != 11) {
      return fail("Penn crosses +50%");
    }

    /* Settlement badges: passive + workers; empty buildings still show free output. */
    if (colony_prod_building_display_output(&pool, col, 0) != 7) {
      return fail("Town Hall display should be passive1 + statesman6");
    }
    if (colony_prod_building_display_output(&pool, col, 1) != 7) {
      return fail("Church display should be passive1 + preacher6");
    }
    col->colonists[0].active = false;
    col->colonists[1].active = false;
    if (colony_prod_building_display_output(&pool, col, 0) != 1) {
      return fail("empty Town Hall should still show passive bell");
    }
    if (colony_prod_building_display_output(&pool, col, 1) != 1) {
      return fail("empty Church should still show passive crosses");
    }
    col->colonists[0].active = true;
    col->colonists[1].active = true;

    /* Turn path: elect Jefferson/Paine/Penn and accrue via nation ticks. */
    ColonizeCol1Save pcol1;
    col1_save_init(&pcol1);
    seed_unclaimed(&pcol1);
    ff_test_calendar(&pcol1);
    ColonizeCol1Nation* pnat = &pcol1.nation[0];
    memset(pnat, 0, sizeof(*pnat));
    pnat->tax_rate = 20;
    pnat->founding_father_count = 0;
    pnat->liberty_bells_total = 40;
    pnat->next_founding_father = FF_THOMAS_JEFFERSON;
    pcol1.head.founding_father[FF_THOMAS_JEFFERSON] = -1;

    EuropeScreen peu;
    memset(&peu, 0, sizeof(peu));
    peu.needed_crosses = 100;
    peu.crosses_immigrant_seen = true; /* no idle +2 after first immigrant */

    /* Keep 584a needed above Penn's 13 so the meter is not cleared by a spawn. */
    ColonizeUnitPool punits;
    memset(&punits, 0, sizeof(punits));
    units_reset(&punits);
    punits.type_count = 1;
    snprintf(punits.types[0].name, sizeof(punits.types[0].name), "Colonists");
    for (int ui = 0; ui < 20; ++ui) {
      const int id = units_spawn_allow_stack(&punits, 0, 1, 1);
      ColonizeUnit* uu = units_get(&punits, id);
      if (uu) {
        units_set_nation(uu, 0);
      }
    }

    ColonizeTurnContext pctx;
    memset(&pctx, 0, sizeof(pctx));
    pctx.human_nation = 0;
    pctx.col1 = &pcol1;
    pctx.col1_ok = true;
    pctx.colonies = &pool;
    pctx.europe = &peu;
    pctx.units = &punits;

    founding_fathers_tick(&pctx);
    if (!founding_fathers_nation_has(&pcol1, 0, FF_THOMAS_JEFFERSON)) {
      return fail("prod-path Jefferson elect");
    }
    pnat->liberty_bells_total = 161;
    pnat->next_founding_father = FF_THOMAS_PAINE;
    founding_fathers_tick(&pctx);
    if (!founding_fathers_nation_has(&pcol1, 0, FF_THOMAS_PAINE)) {
      return fail("prod-path Paine elect");
    }
    pnat->liberty_bells_total = 241;
    pnat->next_founding_father = FF_WILLIAM_PENN;
    founding_fathers_tick(&pctx);
    if (!founding_fathers_nation_has(&pcol1, 0, FF_WILLIAM_PENN)) {
      return fail("prod-path Penn elect");
    }

    peu.liberty_bells_total = 0;
    peu.liberty_bells_last_turn = 0;
    peu.current_crosses = 0;
    pnat->liberty_bells_total = 0;
    pnat->current_crosses = 0;
    turn_run_nation_ticks(&pctx, NULL);
    /* Jefferson+Paine: 12 bells; Penn: 11 church crosses (no idle +2) — see
     * the per-worker Penn fold note above crosses_penn. */
    if (peu.liberty_bells_last_turn != 12) {
      return fail("turn Jefferson+Paine bells last_turn");
    }
    if (peu.current_crosses != 11) {
      return fail("turn Penn crosses accrued");
    }
  }

  /* Peter Minuit: FUN_4cc6_07c2 land-buy gold → 0; founding on homeland cheaper/free. */
  {
    ColonizeCol1Save mcol1;
    col1_save_init(&mcol1);
    seed_unclaimed(&mcol1);
    mcol1.head.difficulty = 0;
    mcol1.player[0].control = 0;
    ColonizeCol1Nation* mnat = &mcol1.nation[0];
    memset(mnat, 0, sizeof(*mnat));
    mnat->gold = 500;
    mnat->liberty_bells_total = 40;
    mnat->next_founding_father = FF_PETER_MINUIT;
    mnat->founding_father_count = 0;

    ColonizeCol1Tribe tribe;
    memset(&tribe, 0, sizeof(tribe));
    tribe.x = 5;
    tribe.y = 5;
    tribe.nation_id = 4; /* Arawak */
    tribe.state.capital = 0;
    mcol1.tribe = &tribe;
    mcol1.head.tribe_count = 1;
    memset(&mcol1.indian[0], 0, sizeof(mcol1.indian[0]));

    ColonizeWorldMap mmap;
    memset(&mmap, 0, sizeof(mmap));
    char merr[128];
    if (!map_alloc(&mmap, 12, 12, merr, sizeof(merr))) {
      return fail("Minuit map_alloc");
    }
    /* Plains land tiles (terrain index 1); avoid arctic (pedia 24). */
    for (int yi = 0; yi < (int)mmap.height; ++yi) {
      for (int xi = 0; xi < (int)mmap.width; ++xi) {
        mmap.terrain[yi * (int)mmap.width + xi] = 1;
      }
    }

    const int fx = 5;
    const int fy = 4; /* adjacent to village — homeland radius 1 */
    const int cost_no = colonies_indian_land_purchase_gold(&mcol1, &mmap, fx, fy, 0);
    if (cost_no <= 0) {
      map_free(&mmap);
      return fail("Minuit: homeland land cost without FF must be > 0");
    }
    /* Human Discoverer, dist 1, tech/bought 0: ((0+3)*2+0+0)-1=5; 0x41*5=325; >>1=162. */
    if (cost_no != 162) {
      map_free(&mmap);
      fprintf(stderr, "unit_founding_fathers: cost_no=%d expected 162\n", cost_no);
      return fail("Minuit: FUN_4cc6_07c2 baseline gold");
    }

    ColonizeColonyPool mpool;
    colonies_init(&mpool);
    uint32_t gold_pay = mnat->gold;
    const int cid_pay =
      colonies_found_with_indian_land(&mpool, &mmap, &mcol1, &gold_pay, fx, fy, 0, -1, -1, 0, 0, 0);
    if (cid_pay < 0) {
      map_free(&mmap);
      return fail("Minuit: found without FF should succeed when gold enough");
    }
    if (gold_pay != 500u - (uint32_t)cost_no) {
      map_free(&mmap);
      return fail("Minuit: founding must deduct land-purchase gold");
    }
    if (mcol1.indian[0].lands_bought != 1) {
      map_free(&mmap);
      return fail("Minuit: lands-bought counter must INC");
    }

    /* Elect Minuit — free land on another homeland tile. */
    ColonizeTurnContext mctx;
    memset(&mctx, 0, sizeof(mctx));
    mctx.human_nation = 0;
    mctx.col1 = &mcol1;
    mctx.col1_ok = true;
    founding_fathers_tick(&mctx);
    if (!founding_fathers_nation_has(&mcol1, 0, FF_PETER_MINUIT)) {
      map_free(&mmap);
      return fail("Minuit elect for land-buy smoke");
    }
    const int cost_ff = colonies_indian_land_purchase_gold(&mcol1, &mmap, 6, 5, 0);
    if (cost_ff != 0) {
      map_free(&mmap);
      return fail("Minuit: land cost must be 0 with FF");
    }
    const uint32_t gold_before_free = gold_pay;
    colonies_init(&mpool);
    const int cid_free = colonies_found_with_indian_land(
      &mpool, &mmap, &mcol1, &gold_pay, 6, 5, 0, -1, -1, 0, 0, 0
    );
    if (cid_free < 0) {
      map_free(&mmap);
      return fail("Minuit: free found on homeland failed");
    }
    if (gold_pay != gold_before_free) {
      map_free(&mmap);
      return fail("Minuit: free found must not spend gold");
    }

    /* Insufficient gold without Minuit blocks found.
     * Prior found stamped MAP_LAYER2_PURCHASED on (fx,fy) — clear so cost
     * still applies (WELCOME/found buy must not double-charge same tile). */
    seed_unclaimed(&mcol1);
    mnat->founding_father_count = 0;
    mnat->founding_fathers[0] = 0;
    mcol1.indian[0].lands_bought = 0;
    {
      const size_t idx = (size_t)fy * (size_t)mmap.width + (size_t)fx;
      if (mmap.layer2 && idx < mmap.tile_count) {
        mmap.layer2[idx] = (uint8_t)(mmap.layer2[idx] & (uint8_t)~MAP_LAYER2_PURCHASED);
      }
    }
    uint32_t poor = 10;
    colonies_init(&mpool);
    const int cid_poor =
      colonies_found_with_indian_land(&mpool, &mmap, &mcol1, &poor, fx, fy, 0, -1, -1, 0, 0, 0);
    if (cid_poor >= 0 || poor != 10u) {
      map_free(&mmap);
      return fail("Minuit: short gold must block found and not debit");
    }

    map_free(&mmap);
  }

  /* Las Casas: Convert (@JOB 27) → Free Colonist (19); no gold/crosses.
   * Cite: COLONIZE/PEDIA.TXT @FATHER24; docs/fandom_col1994.md. */
  {
    ColonizeCol1Save lcol1;
    col1_save_init(&lcol1);
    seed_unclaimed(&lcol1);
    ff_test_calendar(&lcol1);

    ColonizeCol1Nation* lnat = &lcol1.nation[0];
    memset(lnat, 0, sizeof(*lnat));
    lnat->founding_father_count = 0;
    lnat->gold = 100;
    lnat->current_crosses = 7;
    lnat->liberty_bells_total = 40;
    lnat->next_founding_father = FF_BARTOLOME_DE_LAS_CASAS;

    ColonizeColonyPool lcolonies;
    colonies_init(&lcolonies);
    ColonizeColony* lcol = &lcolonies.colonies[0];
    lcol->id = 0;
    lcol->active = true;
    lcol->nation_id = 0;
    lcol->x = 3;
    lcol->y = 3;
    lcol->population = 2;
    lcol->colonist_count = 2;
    lcol->colonists[0].active = true;
    lcol->colonists[0].profession = COLONIZE_PROF_CONVERT;
    lcol->colonists[0].unit_type_index = 0;
    lcol->colonists[1].active = true;
    lcol->colonists[1].profession = COLONIZE_PROF_CONVERT;
    lcol->colonists[1].unit_type_index = 0;
    lcolonies.colony_count = 1;

    /* Foreign colony convert must stay Convert. */
    ColonizeColony* fcol = &lcolonies.colonies[1];
    fcol->id = 1;
    fcol->active = true;
    fcol->nation_id = 1;
    fcol->x = 6;
    fcol->y = 6;
    fcol->population = 1;
    fcol->colonist_count = 1;
    fcol->colonists[0].active = true;
    fcol->colonists[0].profession = COLONIZE_PROF_CONVERT;
    lcolonies.colony_count = 2;

    ColonizeUnitPool lunits;
    units_reset(&lunits);
    lunits.type_count = 3;
    snprintf(lunits.types[0].name, sizeof(lunits.types[0].name), "Colonists");
    lunits.types[0].movement = 1;
    lunits.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(lunits.types[1].name, sizeof(lunits.types[1].name), "Indian Converts");
    lunits.types[1].movement = 1;
    lunits.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
    snprintf(lunits.types[2].name, sizeof(lunits.types[2].name), "Free Colonist");
    lunits.types[2].movement = 1;
    lunits.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;

    const int uid = units_spawn_allow_stack(&lunits, 0, 4, 4);
    if (uid < 0) {
      return fail("Las Casas map spawn");
    }
    ColonizeUnit* lu = units_get(&lunits, uid);
    lu->nation_id = 0;
    lu->profession = COLONIZE_PROF_CONVERT;

    const int uid_named = units_spawn_allow_stack(&lunits, 1, 5, 5);
    if (uid_named < 0) {
      return fail("Las Casas named Convert spawn");
    }
    ColonizeUnit* lu_named = units_get(&lunits, uid_named);
    lu_named->nation_id = 0;
    lu_named->profession = COLONIZE_PROF_CONVERT;

    ColonizeTurnContext lctx;
    memset(&lctx, 0, sizeof(lctx));
    lctx.human_nation = 0;
    lctx.col1 = &lcol1;
    lctx.col1_ok = true;
    lctx.colonies = &lcolonies;
    lctx.units = &lunits;
    lctx.status = status;
    lctx.status_size = sizeof(status);

    const uint32_t g0 = lnat->gold;
    const uint16_t c0 = lnat->current_crosses;
    founding_fathers_tick(&lctx);
    if (lcol1.head.founding_father[FF_BARTOLOME_DE_LAS_CASAS] != 0 ||
        lnat->founding_father_count != 1) {
      return fail("Las Casas not elected");
    }
    if (lnat->gold != g0) {
      return fail("Las Casas must not invent gold");
    }
    if (lnat->current_crosses != c0) {
      return fail("Las Casas must not invent crosses");
    }
    if (lcol->colonists[0].profession != COLONIZE_PROF_FREE_COLONIST ||
        lcol->colonists[1].profession != COLONIZE_PROF_FREE_COLONIST) {
      return fail("Las Casas must assimilate owned colony converts");
    }
    if (fcol->colonists[0].profession != COLONIZE_PROF_CONVERT) {
      return fail("Las Casas must not touch foreign colony converts");
    }
    if (lu->profession != COLONIZE_PROF_FREE_COLONIST) {
      return fail("Las Casas must assimilate map Convert profession");
    }
    if (lu_named->profession != COLONIZE_PROF_FREE_COLONIST ||
        lu_named->type_index != 2) {
      return fail("Las Casas must rename Indian Converts unit type");
    }

    /* Ownership tick: late Convert on map assimilates without re-elect. */
    const int late = units_spawn_allow_stack(&lunits, 0, 7, 7);
    if (late < 0) {
      return fail("Las Casas late spawn");
    }
    ColonizeUnit* late_u = units_get(&lunits, late);
    late_u->nation_id = 0;
    late_u->profession = COLONIZE_PROF_CONVERT;
    lnat->liberty_bells_total = 0; /* below next elect threshold */
    founding_fathers_tick(&lctx);
    if (late_u->profession != COLONIZE_PROF_FREE_COLONIST) {
      return fail("Las Casas ownership tick must assimilate late Convert");
    }
    if (lnat->founding_father_count != 1) {
      return fail("Las Casas ownership tick must not invent extra elects");
    }
  }

  /* Cortes ownership gates — no gold invent; cash-in tax stays @KINGGALLEON3. */
  {
    ColonizeCol1Save ccol1;
    col1_save_init(&ccol1);
    seed_unclaimed(&ccol1);
    if (founding_fathers_cortes_guarantees_conquest_treasure(&ccol1, 0) ||
        founding_fathers_cortes_free_king_galleon(&ccol1, 0)) {
      return fail("Cortes gates must be false before elect");
    }
    ccol1.head.founding_father[FF_HERNAN_CORTES] = 0;
    ccol1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] |=
      (uint8_t)(1u << (FF_HERNAN_CORTES % 8));
    if (!founding_fathers_cortes_guarantees_conquest_treasure(&ccol1, 0) ||
        !founding_fathers_cortes_free_king_galleon(&ccol1, 0)) {
      return fail("Cortes gates false after ownership");
    }
    if (founding_fathers_cortes_guarantees_conquest_treasure(&ccol1, 1) ||
        founding_fathers_cortes_free_king_galleon(&ccol1, 1)) {
      return fail("Cortes gates must not leak to other nation");
    }
  }

  /* de Witt foreign-colony cargo transfer (stock only; no gold). */
  {
    ColonizeCol1Save dcol1;
    col1_save_init(&dcol1);
    seed_unclaimed(&dcol1);
    ff_test_calendar(&dcol1);

    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* home = &pool.colonies[0];
    memset(home, 0, sizeof(*home));
    home->active = true;
    home->id = 0;
    home->nation_id = 1; /* foreign French */
    home->x = 3;
    home->y = 3;
    home->building_in_production = -1;
    home->stock[COLONIZE_CARGO_SUGAR] = 40;
    pool.colony_count = 1;

    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Merchantman");
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
    units.types[0].cargo = 4;
    units.types[0].movement = 4;
    const int uid = units_spawn(&units, 0, 3, 3);
    ColonizeUnit* ship = units_get(&units, uid);
    if (!ship) {
      return fail("de Witt ship spawn");
    }
    ship->nation_id = 0; /* English */

    /* Without FF: refuse. */
    if (colonies_de_witt_transfer_from_colony(
          &pool, 0, &units, uid, COLONIZE_CARGO_SUGAR, 10, &dcol1
        ) != 0) {
      return fail("de Witt transfer must refuse without FF");
    }

    dcol1.head.founding_father[FF_JAN_DE_WITT] = 0;
    dcol1.nation[0].founding_fathers[FF_JAN_DE_WITT / 8] |=
      (uint8_t)(1u << (FF_JAN_DE_WITT % 8));
    const int moved = colonies_de_witt_transfer_from_colony(
      &pool, 0, &units, uid, COLONIZE_CARGO_SUGAR, 10, &dcol1
    );
    if (moved != 10 || home->stock[COLONIZE_CARGO_SUGAR] != 30) {
      fprintf(stderr, "de Witt from_colony moved=%d stock=%d\n", moved, home->stock[COLONIZE_CARGO_SUGAR]);
      return fail("de Witt with FF should load sugar from foreign colony");
    }
    /* Unload back into foreign warehouse. */
    int hold = -1;
    for (int h = 0; h < COLONIZE_UNIT_CARGO_MAX; ++h) {
      if (ship->hold_goods_amount[h] > 0 && ship->hold_goods_type[h] == COLONIZE_CARGO_SUGAR) {
        hold = h;
        break;
      }
    }
    if (hold < 0) {
      return fail("de Witt ship should hold sugar");
    }
    const int back = colonies_de_witt_transfer_to_colony(
      &pool, 0, &units, uid, hold, &dcol1, NULL
    );
    if (back != 10 || home->stock[COLONIZE_CARGO_SUGAR] != 40) {
      return fail("de Witt to_colony should unload sugar into foreign stock");
    }
    /* At war: refuse. */
    ai_diplo_declare_war(&dcol1, 0, 1);
    home->stock[COLONIZE_CARGO_SUGAR] = 40;
    if (colonies_de_witt_transfer_from_colony(
          &pool, 0, &units, uid, COLONIZE_CARGO_SUGAR, 5, &dcol1
        ) != 0) {
      return fail("de Witt transfer must refuse while at war");
    }
  }

  /* de Witt: ships may enter foreign Euro colony dock at peace (units_can_enter). */
  {
    ColonizeCol1Save dcol1;
    col1_save_init(&dcol1);
    seed_unclaimed(&dcol1);
    ff_test_calendar(&dcol1);
    dcol1.head.founding_father[FF_JAN_DE_WITT] = 0;
    dcol1.nation[0].founding_fathers[FF_JAN_DE_WITT / 8] |=
      (uint8_t)(1u << (FF_JAN_DE_WITT % 8));

    ColonizeWorldMap dmap;
    memset(&dmap, 0, sizeof(dmap));
    dmap.width = 8;
    dmap.height = 8;
    dmap.tile_count = 64;
    dmap.terrain = calloc(64, 1);
    dmap.layer2 = calloc(64, 1);
    dmap.layer3 = calloc(64, 1);
    if (!dmap.terrain || !dmap.layer2 || !dmap.layer3) {
      return fail("de Witt dock map alloc");
    }
    for (int i = 0; i < 64; ++i) {
      dmap.terrain[i] = 25; /* ocean */
    }
    dmap.terrain[3 * 8 + 3] = 1; /* colony land */

    ColonizeColonyPool pool;
    colonies_init(&pool);
    ColonizeColony* foreign = &pool.colonies[0];
    memset(foreign, 0, sizeof(*foreign));
    foreign->active = true;
    foreign->id = 0;
    foreign->nation_id = 1;
    foreign->x = 3;
    foreign->y = 3;
    foreign->building_in_production = -1;
    pool.colony_count = 1;

    ColonizeUnitPool units;
    units_reset(&units);
    memset(units.types, 0, sizeof(units.types));
    units.type_count = 1;
    snprintf(units.types[0].name, sizeof(units.types[0].name), "Merchantman");
    units.types[0].domain = COLONIZE_UNIT_DOMAIN_SEA;
    units.types[0].cargo = 4;
    units.types[0].movement = 4;
    const int uid = units_spawn(&units, 0, 3, 2);
    ColonizeUnit* ship = units_get(&units, uid);
    if (!ship) {
      map_free(&dmap);
      return fail("de Witt dock ship spawn");
    }
    ship->nation_id = 0;

    units_set_ff_col1(&dcol1);
    if (!units_can_enter(&units, 0, &dmap, 3, 3, uid, &pool)) {
      map_free(&dmap);
      return fail("de Witt ship should enter foreign dock at peace");
    }
    ai_diplo_declare_war(&dcol1, 0, 1);
    if (units_can_enter(&units, 0, &dmap, 3, 3, uid, &pool)) {
      map_free(&dmap);
      return fail("de Witt ship must not enter foreign dock at war");
    }
    units_set_ff_col1(NULL);
    map_free(&dmap);
    fprintf(stderr, "unit_founding_fathers: de Witt ship foreign dock enter ok\n");
  }

  /* Sepulveda / de Soto LCR / de Witt — ownership gates; de Soto LCR wired. */
  {
    ColonizeCol1Save gcol1;
    col1_save_init(&gcol1);
    seed_unclaimed(&gcol1);
    if (founding_fathers_sepulveda_convert_join_bonus(&gcol1, 0) ||
        founding_fathers_de_soto_lcr_always_positive(&gcol1, 0) ||
        founding_fathers_de_witt_allows_foreign_colony_trade(&gcol1, 0)) {
      return fail("Sepulveda/de Soto/de Witt gates must be false before ownership");
    }
    gcol1.head.founding_father[FF_JUAN_DE_SEPULVEDA] = 0;
    gcol1.nation[0].founding_fathers[FF_JUAN_DE_SEPULVEDA / 8] |=
      (uint8_t)(1u << (FF_JUAN_DE_SEPULVEDA % 8));
    gcol1.head.founding_father[FF_HERNANDO_DE_SOTO] = 0;
    gcol1.nation[0].founding_fathers[FF_HERNANDO_DE_SOTO / 8] |=
      (uint8_t)(1u << (FF_HERNANDO_DE_SOTO % 8));
    gcol1.head.founding_father[FF_JAN_DE_WITT] = 0;
    gcol1.nation[0].founding_fathers[FF_JAN_DE_WITT / 8] |=
      (uint8_t)(1u << (FF_JAN_DE_WITT % 8));
    if (!founding_fathers_sepulveda_convert_join_bonus(&gcol1, 0)) {
      return fail("Sepulveda convert-join gate false after ownership");
    }
    if (!founding_fathers_de_soto_lcr_always_positive(&gcol1, 0)) {
      return fail("de Soto LCR gate false after ownership");
    }
    if (!founding_fathers_de_witt_allows_foreign_colony_trade(&gcol1, 0)) {
      return fail("de Witt foreign-trade gate false after ownership");
    }
    if (founding_fathers_sepulveda_convert_join_bonus(&gcol1, 1) ||
        founding_fathers_de_soto_lcr_always_positive(&gcol1, 1) ||
        founding_fathers_de_witt_allows_foreign_colony_trade(&gcol1, 1)) {
      return fail("Sepulveda/de Soto/de Witt gates must not leak");
    }

    /* de Soto LCR resolve wired: clear rumour + reveal (no invented gold). */
    ColonizeWorldMap lmap;
    memset(&lmap, 0, sizeof(lmap));
    lmap.width = 20;
    lmap.height = 20;
    lmap.terrain = calloc(400, 1);
    lmap.layer2 = calloc(400, 1);
    lmap.layer3 = calloc(400, 1);
    lmap.seen = calloc(400, 1);
    if (!lmap.terrain || !lmap.layer2 || !lmap.layer3 || !lmap.seen) {
      map_free(&lmap);
      return fail("de Soto LCR map alloc");
    }
    lmap.terrain[14 * 20 + 8] = 0x08; /* scrub — matches AMER2 rumour fixture class */
    ColonizeUnitPool upool;
    units_reset(&upool);
    upool.type_count = 1;
    snprintf(upool.types[0].name, sizeof(upool.types[0].name), "Scout");
    upool.types[0].movement = 3;
    const int uid = units_spawn_allow_stack(&upool, 0, 8, 14);
    ColonizeUnit* su = units_get(&upool, uid);
    if (!su) {
      map_free(&lmap);
      return fail("de Soto scout spawn");
    }
    su->nation_id = 0;
    if (!map_tile_has_rumour(&lmap, 8, 14)) {
      map_free(&lmap);
      return fail("de Soto LCR fixture tile should have rumour");
    }
    if (!units_resolve_lcr_rumour(&upool, uid, &lmap, &gcol1, NULL, NULL, -1)) {
      map_free(&lmap);
      return fail("units_resolve_lcr_rumour de Soto path");
    }
    if (map_tile_has_rumour(&lmap, 8, 14)) {
      map_free(&lmap);
      return fail("de Soto LCR must clear rumour");
    }
    if (!map_tile_seen_by(&lmap, 8, 14, 0)) {
      map_free(&lmap);
      return fail("de Soto LCR must reveal tile");
    }
    map_free(&lmap);
  }

  /*
   * Slice C: AI combat wrapper + empty-village fallout + Cortes → treasure
   * gold>0 (FUN_5fef_31ea peel). Map Brave death does not destroy the dwelling;
   * treasure comes from units_try_native_settlement_fallout after the tile is
   * clear. turn_refresh arms g_units_ff_col1 for the combat wrapper.
   */
  {
    ColonizeCol1Save ccol1;
    col1_save_init(&ccol1);
    seed_unclaimed(&ccol1);
    ccol1.head.difficulty = 0;
    ccol1.head.tribe_count = 1;
    ccol1.tribe = calloc(1, sizeof(ColonizeCol1Tribe));
    if (!ccol1.tribe) {
      return fail("Cortes AI tribe alloc");
    }
    ccol1.tribe[0].x = 5;
    ccol1.tribe[0].y = 5;
    ccol1.tribe[0].nation_id = 4;
    ccol1.tribe[0].mission = COL1_TRIBE_MISSION_NONE;
    ccol1.head.founding_father[FF_HERNAN_CORTES] = 0;
    ccol1.nation[0].founding_fathers[FF_HERNAN_CORTES / 8] |=
      (uint8_t)(1u << (FF_HERNAN_CORTES % 8));

    ColonizeWorldMap cmap;
    memset(&cmap, 0, sizeof(cmap));
    cmap.width = 16;
    cmap.height = 16;
    cmap.tile_count = 256;
    cmap.terrain = calloc(256, 1);
    cmap.layer2 = calloc(256, 1);
    cmap.layer3 = calloc(256, 1);
    if (!cmap.terrain || !cmap.layer2 || !cmap.layer3) {
      free(ccol1.tribe);
      map_free(&cmap);
      return fail("Cortes AI map alloc");
    }
    for (int i = 0; i < 256; ++i) {
      cmap.terrain[i] = 1;
    }
    cmap.layer3[5 * 16 + 5] = (uint8_t)((4u << 4) | 1u);

    ColonizeUnitPool upool;
    units_reset(&upool);
    memset(upool.types, 0, sizeof(upool.types));
    upool.type_count = 3;
    snprintf(upool.types[0].name, sizeof(upool.types[0].name), "Soldiers");
    upool.types[0].domain = COLONIZE_UNIT_DOMAIN_LAND;
    upool.types[0].movement = 3;
    upool.types[0].attack = 99;
    upool.types[0].defense = 2;
    snprintf(upool.types[1].name, sizeof(upool.types[1].name), "Braves");
    upool.types[1].domain = COLONIZE_UNIT_DOMAIN_LAND;
    upool.types[1].movement = 1;
    upool.types[1].attack = 0;
    upool.types[1].defense = 0;
    snprintf(upool.types[2].name, sizeof(upool.types[2].name), "Treasure");
    upool.types[2].domain = COLONIZE_UNIT_DOMAIN_LAND;
    upool.types[2].movement = 1;

    const int sid = units_spawn_allow_stack(&upool, 0, 5, 5);
    const int bid = units_spawn_allow_stack(&upool, 1, 5, 5);
    ColonizeUnit* soldier = units_get(&upool, sid);
    ColonizeUnit* brave = units_get(&upool, bid);
    if (!soldier || !brave) {
      free(ccol1.tribe);
      map_free(&cmap);
      return fail("Cortes AI spawn");
    }
    soldier->nation_id = 0;
    brave->nation_id = 4;

    ColonizeDosRng rng;
    dos_rng_seed(&rng, 7);
    turn_refresh_moves_for_nation(&upool, 0, &ccol1, &cmap, NULL, NULL, NULL);
    units_set_native_fallout_context(&ccol1, &cmap, -1);
    if (!units_resolve_land_combat(&upool, sid, bid, &rng)) {
      free(ccol1.tribe);
      map_free(&cmap);
      return fail("Cortes AI combat should win");
    }
    /* Dwelling remains after map Brave death; empty-village fallout peels gold. */
    if (!units_try_native_settlement_fallout(
          &ccol1, &upool, &cmap, 0, 4, 5, 5, -1, &rng
        )) {
      free(ccol1.tribe);
      map_free(&cmap);
      return fail("Cortes AI fallout should destroy empty dwelling");
    }
    int gold = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &upool.units[i];
      if (!u->active || u->x != 5 || u->y != 5) {
        continue;
      }
      const ColonizeUnitType* tt = units_type(&upool, u->type_index);
      if (tt && strcmp(tt->name, "Treasure") == 0) {
        gold = u->hold_goods_amount[0] | (u->hold_goods_amount[1] << 8);
        break;
      }
    }
    if (gold <= 0) {
      free(ccol1.tribe);
      map_free(&cmap);
      return fail("Cortes AI path must spawn treasure with peeled gold>0");
    }
    free(ccol1.tribe);
    map_free(&cmap);
    fprintf(stderr, "unit_founding_fathers: Cortes AI treasure gold=%d ok\n", gold);
  }

  /* Human + ai_popups: choose first (next < 0), accumulate, then elect. */
  {
    ColonizeCol1Save dcol1;
    col1_save_init(&dcol1);
    seed_unclaimed(&dcol1);
    ff_test_calendar(&dcol1);
    ColonizeCol1Nation* dnat = &dcol1.nation[0];
    memset(dnat, 0, sizeof(*dnat));
    dnat->liberty_bells_total = 10; /* bells exist, below elect threshold */
    dnat->next_founding_father = -1;
    dnat->founding_father_count = 0;

    AiPopupState pop;
    ai_popup_init(&pop);

    char dstatus[128];
    dstatus[0] = '\0';
    ColonizeTurnContext dctx;
    memset(&dctx, 0, sizeof(dctx));
    dctx.human_nation = 0;
    dctx.col1 = &dcol1;
    dctx.col1_ok = true;
    dctx.status = dstatus;
    dctx.status_size = sizeof(dstatus);
    dctx.ai_popups = &pop;

    founding_fathers_tick(&dctx);
    if (dnat->founding_father_count != 0) {
      return fail("debate path must not elect before CHOICE apply");
    }
    if (dnat->next_founding_father != -1) {
      return fail("debate pending must leave next_founding_father unset");
    }
    if (pop.queue_count < 1 || pop.queue[0].kind != AI_POPUP_KIND_CHOICE ||
        pop.queue[0].tag != AI_POPUP_TAG_FF_CONGRESS || pop.queue[0].choice_count < 2) {
      fprintf(stderr, "unit_founding_fathers: debate queue_count=%d kind=%d choices=%d\n",
              pop.queue_count,
              pop.queue_count > 0 ? (int)pop.queue[0].kind : -1,
              pop.queue_count > 0 ? pop.queue[0].choice_count : -1);
      return fail("expected FF debate CHOICE with ≥2 category candidates");
    }
    const int chosen = pop.queue[0].choice_ids[0];
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_tag = AI_POPUP_TAG_FF_CONGRESS;
    pop.result_choice_id = chosen;
    pop.result_nation_a = 0;
    pop.result_payload = 1;
    pop.queue_count = 0;
    founding_fathers_apply_popup_result(&dctx, &pop);
    if (dnat->founding_father_count != 0) {
      return fail("debate apply below threshold must only lock next, not elect");
    }
    if (dnat->next_founding_father != chosen) {
      return fail("debate apply must lock chosen founding father as next");
    }
    ai_popup_init(&pop);
    dctx.ai_popups = &pop;
    dnat->liberty_bells_total = 40;
    founding_fathers_tick(&dctx);
    if (dnat->founding_father_count != 1 ||
        !founding_fathers_nation_has(&dcol1, 0, chosen)) {
      return fail("threshold tick must elect previously locked founding father");
    }
    if (pop.queue_count > 0 && pop.queue[0].kind == AI_POPUP_KIND_CHOICE) {
      return fail("elect path must not re-open debate CHOICE");
    }
    fprintf(stderr, "unit_founding_fathers: Congress debate CHOICE ok\n");
  }

  printf("unit_founding_fathers: OK\n");
  return 0;
}
