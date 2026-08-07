/* Smoke: liberty-bell threshold elects FF with manual/wiki-aligned effects. */
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/europe.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_founding_fathers: FAIL %s\n", msg);
  return 1;
}

static void seed_unclaimed(ColonizeCol1Save* col1) {
  for (int i = 0; i < (int)COLONIZE_COL1_FF_COUNT; ++i) {
    col1->head.founding_father[i] = -1;
  }
}

int main(void) {
  if (founding_fathers_bells_needed(0) != 40u ||
      founding_fathers_bells_needed(1) != 80u ||
      founding_fathers_bells_needed(2) != 120u) {
    return fail("bells_needed curve");
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  seed_unclaimed(&col1);

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
  if (nat->next_founding_father != 1) {
    return fail("next_founding_father not advanced to 1");
  }
  if (strstr(status, "Founding Father elected") == NULL) {
    return fail("status line missing");
  }
  if (nat->liberty_bells_total != 40) {
    return fail("bells were spent (expected gate-only)");
  }

  /* Jakob Fugger: clear ALL boycotts; no gold bump. */
  nat->liberty_bells_total = 80;
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
  nat->liberty_bells_total = 120;
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
  ctx.europe = NULL;

  /* Jefferson: elect only — no liberty-bells fiction. */
  nat->liberty_bells_total = 160;
  nat->next_founding_father = 15;
  const uint16_t bells_before = nat->liberty_bells_total;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[15] != 0 || nat->founding_father_count != 4) {
    return fail("Jefferson not elected via next");
  }
  if (nat->liberty_bells_total != bells_before) {
    return fail("Jefferson must not invent bells");
  }

  /* de Witt: elect only — no tax fiction. */
  nat->tax_rate = 12;
  nat->liberty_bells_total = 200;
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
  nat->liberty_bells_total = 240;
  nat->next_founding_father = 11;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[11] != 0 || nat->founding_father_count != 6) {
    return fail("Washington not elected via next");
  }
  if (col1.head.expeditionary_force[0] != 5) {
    return fail("Washington must not invent REF−1");
  }

  /* Stuyvesant: elect only (Custom House gate elsewhere) — no gold. */
  nat->liberty_bells_total = 280;
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
  nat->liberty_bells_total = 320;
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

  /* Revere: ownership flag only — no tools / gold fiction. */
  nat->liberty_bells_total = 360;
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
  nat->liberty_bells_total = 400;
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

  /* Pocahontas: PARKED — no crosses fiction. */
  nat->liberty_bells_total = 440;
  nat->next_founding_father = 16;
  {
    const uint16_t c0 = nat->current_crosses;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[16] != 0 || nat->founding_father_count != 11) {
      return fail("Pocahontas not elected via next");
    }
    if (nat->current_crosses != c0) {
      return fail("Pocahontas must not invent crosses");
    }
  }

  /* Coronado without map: elect only — no gold fallback. */
  nat->liberty_bells_total = 480;
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
  nat->liberty_bells_total = 520;
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

  /* Brebeuf: PARKED — no crosses fiction. */
  nat->liberty_bells_total = 560;
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
    dnat->liberty_bells_total = 80;
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
    turn_refresh_moves_for_nation(&units, 0, &deep_col1);
    if (caravel->moves_left != units.types[2].movement + 1) {
      free(deep_col1.colony);
      map_free(&map);
      return fail("deep Magellan permanent refresh +1 missing");
    }

    /* Hudson: ownership only (fur +100% in turn harvest) — no stock dump. */
    dnat->liberty_bells_total = 120;
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
    dnat->liberty_bells_total = 160;
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
    dnat->liberty_bells_total = 200;
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
    dnat->liberty_bells_total = 240;
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
    dnat->liberty_bells_total = 280;
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
    dnat->liberty_bells_total = 320;
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
    dnat->liberty_bells_total = 360;
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
    dnat->liberty_bells_total = 400;
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

    /* Bolivar: SoL +20% only (40→60), no bells fiction. */
    dnat->liberty_bells_total = 440;
    dnat->next_founding_father = 18;
    {
      const uint16_t b0 = dnat->liberty_bells_total;
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
      if (deep_col1.colony[0].rebel_dividend != 60u) {
        free(deep_col1.colony);
        map_free(&map);
        return fail("deep Bolivar rebel_dividend +20 missing");
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

    ai->liberty_bells_total = 40;
    ai->next_founding_father = 2; /* Peter Minuit — PARKED, no gold */
    ai->founding_father_count = 0;
    ai->gold = 10;

    withdrawn->liberty_bells_total = 40;
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

    ai->liberty_bells_total = 80;
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

  /* Arctic founding rejection. */
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
    ColonizeColonyPool pool;
    colonies_init(&pool);
    if (colonies_can_found(&pool, &map, 3, 3)) {
      map_free(&map);
      return fail("can_found allowed arctic");
    }
    if (!colonies_can_found(&pool, &map, 2, 2)) {
      map_free(&map);
      return fail("can_found rejected plains");
    }
    map_free(&map);
  }

  printf("smoke_founding_fathers: OK\n");
  return 0;
}
