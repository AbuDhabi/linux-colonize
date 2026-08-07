/* Smoke: liberty-bell threshold elects an unclaimed founding father. */
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/founding_fathers.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
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

  /* At threshold: elect next_founding_father (Adam Smith = 0). */
  nat->liberty_bells_total = 40;
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
  if (nat->next_founding_father != 1) {
    return fail("next_founding_father not advanced to 1");
  }
  if (strstr(status, "Founding Father elected") == NULL) {
    return fail("status line missing");
  }
  /* Bells gated, not spent. */
  if (nat->liberty_bells_total != 40) {
    return fail("bells were spent (expected gate-only)");
  }

  /* Second elect needs 80; Jakob Fugger (1) gold + boycott forgive. */
  nat->liberty_bells_total = 80;
  nat->boycott_bitmap = (uint16_t)((1u << 1) | (1u << 4) | (1u << 2)); /* Sugar+Furs+Tobacco */
  col1.head.unknown46[2] = 1; /* king tax-refuse stand-in */
  const uint32_t gold_before = nat->gold;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[1] != 0 || nat->founding_father_count != 2) {
    return fail("second FF not Jakob Fugger");
  }
  if (nat->gold != gold_before + 50u) {
    return fail("Jakob Fugger gold +50 missing");
  }
  if ((nat->boycott_bitmap & (1u << 1)) != 0) {
    return fail("Fugger did not clear Sugar boycott bit");
  }
  if ((nat->boycott_bitmap & (1u << 4)) != 0) {
    return fail("Fugger did not clear Furs embargo bit");
  }
  if ((nat->boycott_bitmap & (1u << 2)) == 0) {
    return fail("Fugger cleared unrelated Tobacco boycott bit");
  }
  if (col1.head.unknown46[2] != 0) {
    return fail("Fugger did not clear human unknown46[2] king refuse");
  }
  if (nat->next_founding_father != 2) {
    return fail("next after Fugger not 2");
  }

  /* Prefer next_founding_father when still unclaimed. */
  nat->liberty_bells_total = 120;
  nat->next_founding_father = 20; /* William Brewster */
  const uint16_t crosses_before = nat->current_crosses;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[20] != 0 || nat->founding_father_count != 3) {
    return fail("Brewster not elected via next");
  }
  if (nat->current_crosses != (uint16_t)(crosses_before + 8u)) {
    return fail("Brewster crosses +8 missing");
  }

  /* Force Jefferson (15): liberty bells +15. */
  nat->liberty_bells_total = 160;
  nat->next_founding_father = 15;
  const uint16_t bells_before = nat->liberty_bells_total;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[15] != 0 || nat->founding_father_count != 4) {
    return fail("Jefferson not elected via next");
  }
  if (nat->liberty_bells_total != (uint16_t)(bells_before + 15u)) {
    return fail("Jefferson bells +15 missing");
  }

  /* Force Jan de Witt (4): tax_rate -1. */
  nat->tax_rate = 12;
  nat->liberty_bells_total = 200;
  nat->next_founding_father = 4;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[4] != 0 || nat->founding_father_count != 5) {
    return fail("de Witt not elected via next");
  }
  if (nat->tax_rate != 11) {
    return fail("de Witt tax -1 missing");
  }

  /* Force Washington (11): REF regulars -1. */
  col1.head.expeditionary_force[0] = 5;
  nat->liberty_bells_total = 240;
  nat->next_founding_father = 11;
  founding_fathers_tick(&ctx);
  if (col1.head.founding_father[11] != 0 || nat->founding_father_count != 6) {
    return fail("Washington not elected via next");
  }
  if (col1.head.expeditionary_force[0] != 4) {
    return fail("Washington REF regulars -1 missing");
  }

  /* Force Stuyvesant (3): gold +40. */
  nat->liberty_bells_total = 280;
  nat->next_founding_father = 3;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[3] != 0 || nat->founding_father_count != 7) {
      return fail("Stuyvesant not elected via next");
    }
    if (nat->gold != g0 + 40u) {
      return fail("Stuyvesant gold +40 missing");
    }
  }

  /* Force Drake (13): gold +75. */
  nat->liberty_bells_total = 320;
  nat->next_founding_father = 13;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[13] != 0 || nat->founding_father_count != 8) {
      return fail("Drake not elected via next");
    }
    if (nat->gold != g0 + 75u) {
      return fail("Drake gold +75 missing");
    }
  }

  /* Force Pocahontas (16): crosses +10. */
  nat->liberty_bells_total = 360;
  nat->next_founding_father = 16;
  {
    const uint16_t c0 = nat->current_crosses;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[16] != 0 || nat->founding_father_count != 9) {
      return fail("Pocahontas not elected via next");
    }
    if (nat->current_crosses != (uint16_t)(c0 + 10u)) {
      return fail("Pocahontas crosses +10 missing");
    }
  }

  /* Force Coronado (6) without map: gold +20 fallback. */
  nat->liberty_bells_total = 400;
  nat->next_founding_father = 6;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[6] != 0 || nat->founding_father_count != 10) {
      return fail("Coronado not elected via next");
    }
    if (nat->gold != g0 + 20u) {
      return fail("Coronado gold +20 fallback missing");
    }
  }

  /* Force John Paul Jones (14) without units/map: gold +60 fallback. */
  nat->liberty_bells_total = 440;
  nat->next_founding_father = 14;
  {
    const uint32_t g0 = nat->gold;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[14] != 0 || nat->founding_father_count != 11) {
      return fail("Jones not elected via next");
    }
    if (nat->gold != g0 + 60u) {
      return fail("Jones gold +60 fallback missing");
    }
  }

  /* Force Brebeuf (22): crosses +12. */
  nat->liberty_bells_total = 480;
  nat->next_founding_father = 22;
  {
    const uint16_t c0 = nat->current_crosses;
    founding_fathers_tick(&ctx);
    if (col1.head.founding_father[22] != 0 || nat->founding_father_count != 12) {
      return fail("Brebeuf not elected via next");
    }
    if (nat->current_crosses != (uint16_t)(c0 + 12u)) {
      return fail("Brebeuf crosses +12 missing");
    }
  }

  /* --- Deeper hooks: Coronado reveal + Jones Frigate with map/units. --- */
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
    /* Ocean west of colony (5,5) for coastal dock + Jones spawn. */
    map.terrain[5 * 16 + 4] = 25;

    ColonizeColonyPool colonies;
    colonies_init(&colonies);
    ColonizeColony* col = &colonies.colonies[0];
    col->id = 0;
    col->active = true;
    col->nation_id = 0;
    col->x = 5;
    col->y = 5;
    col->population = 2;
    col->colonist_count = 2;
    col->stock[COLONIZE_CARGO_TOOLS] = 10;
    col->stock[COLONIZE_CARGO_FURS] = 5;
    colonies.colony_count = 1;

    ColonizeUnitPool units;
    units_reset(&units);
    units.type_count = 3;
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

    /* Land unit for Magellan (no-op) / de Soto reveal later. */
    const int land_id = units_spawn_allow_stack(&units, 0, 8, 8);
    if (land_id < 0) {
      map_free(&map);
      return fail("deep land spawn");
    }
    ColonizeUnit* land = units_get(&units, land_id);
    land->nation_id = 0;
    land->moves_left = 1;

    /* Existing caravel for Magellan moves bump. */
    const int car_id = units_spawn_allow_stack(&units, 2, 4, 5);
    if (car_id < 0) {
      map_free(&map);
      return fail("deep caravel spawn");
    }
    ColonizeUnit* caravel = units_get(&units, car_id);
    caravel->nation_id = 0;
    caravel->moves_left = 4;

    ColonizeTurnContext deep_ctx;
    memset(&deep_ctx, 0, sizeof(deep_ctx));
    deep_ctx.human_nation = 0;
    deep_ctx.col1 = &deep_col1;
    deep_ctx.col1_ok = true;
    deep_ctx.map = &map;
    deep_ctx.colonies = &colonies;
    deep_ctx.units = &units;

    /* Coronado: reveal around colony, no gold bump. */
    const uint32_t gold_pre_cor = dnat->gold;
    if (map_tile_seen_by(&map, 5, 5, 0) || map_tile_seen_by(&map, 7, 5, 0)) {
      map_free(&map);
      return fail("deep map unexpectedly seen before Coronado");
    }
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[6] != 0 || dnat->founding_father_count != 1) {
      map_free(&map);
      return fail("deep Coronado not elected");
    }
    if (dnat->gold != gold_pre_cor) {
      map_free(&map);
      return fail("deep Coronado should not gold-fallback");
    }
    if (!map_tile_seen_by(&map, 5, 5, 0)) {
      map_free(&map);
      return fail("deep Coronado colony tile unseen");
    }
    if (!map_tile_seen_by(&map, 7, 5, 0)) { /* radius 2 east */
      map_free(&map);
      return fail("deep Coronado radius-2 tile unseen");
    }
    if (map_tile_seen_by(&map, 8, 5, 0)) { /* outside radius 2 */
      map_free(&map);
      return fail("deep Coronado revealed beyond radius 2");
    }

    /* Magellan: +1 moves_left on sea units. */
    dnat->liberty_bells_total = 80;
    dnat->next_founding_father = 5;
    const int car_moves = caravel->moves_left;
    const uint32_t gold_pre_mag = dnat->gold;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[5] != 0 || dnat->founding_father_count != 2) {
      map_free(&map);
      return fail("deep Magellan not elected");
    }
    if (caravel->moves_left != car_moves + 1) {
      map_free(&map);
      return fail("deep Magellan sea moves +1 missing");
    }
    if (dnat->gold != gold_pre_mag) {
      map_free(&map);
      return fail("deep Magellan should not gold-fallback");
    }
    if (land->moves_left != 1) {
      map_free(&map);
      return fail("deep Magellan bumped land unit");
    }

    /* Hudson: +50 tools/+50 furs on owned colony. */
    dnat->liberty_bells_total = 120;
    dnat->next_founding_father = 8;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[8] != 0 || dnat->founding_father_count != 3) {
      map_free(&map);
      return fail("deep Hudson not elected");
    }
    if (col->stock[COLONIZE_CARGO_TOOLS] != 60 || col->stock[COLONIZE_CARGO_FURS] != 55) {
      map_free(&map);
      return fail("deep Hudson stock bump missing");
    }

    /* de Soto: reveal radius 1 around land unit at (8,8). */
    dnat->liberty_bells_total = 160;
    dnat->next_founding_father = 7;
    if (map_tile_seen_by(&map, 8, 8, 0) || map_tile_seen_by(&map, 9, 8, 0)) {
      map_free(&map);
      return fail("deep land tile unexpectedly seen before de Soto");
    }
    const uint16_t crosses_pre = dnat->current_crosses;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[7] != 0 || dnat->founding_father_count != 4) {
      map_free(&map);
      return fail("deep de Soto not elected");
    }
    if (!map_tile_seen_by(&map, 8, 8, 0) || !map_tile_seen_by(&map, 9, 8, 0)) {
      map_free(&map);
      return fail("deep de Soto land reveal missing");
    }
    if (dnat->current_crosses != crosses_pre) {
      map_free(&map);
      return fail("deep de Soto should not crosses-fallback");
    }

    /* Jones: free Frigate on coastal water west of colony. */
    dnat->liberty_bells_total = 200;
    dnat->next_founding_father = 14;
    const int units_before = units.unit_count;
    const uint32_t gold_pre_jones = dnat->gold;
    founding_fathers_tick(&deep_ctx);
    if (deep_col1.head.founding_father[14] != 0 || dnat->founding_father_count != 5) {
      map_free(&map);
      return fail("deep Jones not elected");
    }
    if (dnat->gold != gold_pre_jones) {
      map_free(&map);
      return fail("deep Jones should not gold-fallback");
    }
    if (units.unit_count != units_before + 1) {
      map_free(&map);
      return fail("deep Jones did not spawn ship");
    }
    {
      int found_frigate = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 0) {
          continue;
        }
        const ColonizeUnitType* ty = units_type(&units, u->type_index);
        if (ty && strcmp(ty->name, "Frigate") == 0) {
          found_frigate = 1;
          if (u->x != 4 || u->y != 5) {
            map_free(&map);
            return fail("deep Jones Frigate not on coastal water");
          }
        }
      }
      if (!found_frigate) {
        map_free(&map);
        return fail("deep Jones Frigate type missing");
      }
    }

    map_free(&map);
  }

  /* --- AI Euro nation elect (control==1), same bells threshold. --- */
  {
    ColonizeCol1Save ai_col1;
    col1_save_init(&ai_col1);
    seed_unclaimed(&ai_col1);
    ai_col1.player[0].control = 0; /* human */
    ai_col1.player[1].control = 1; /* AI */
    ai_col1.player[2].control = 2; /* withdrawn — must not elect */
    ai_col1.player[3].control = 1;

    ColonizeCol1Nation* human = &ai_col1.nation[0];
    ColonizeCol1Nation* ai = &ai_col1.nation[1];
    ColonizeCol1Nation* withdrawn = &ai_col1.nation[2];
    memset(human, 0, sizeof(*human));
    memset(ai, 0, sizeof(*ai));
    memset(withdrawn, 0, sizeof(*withdrawn));

    /* Human below threshold so only AI elects this tick. */
    human->liberty_bells_total = 0;
    human->next_founding_father = 0;

    ai->liberty_bells_total = 40;
    ai->next_founding_father = 2; /* Peter Minuit */
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
    if ((ai->founding_fathers[0] & (1u << 2)) == 0) {
      return fail("AI bitmask bit 2 unset");
    }
    if (ai->gold != 40u) {
      return fail("AI Minuit gold +30 missing");
    }
    if (withdrawn->founding_father_count != 0 || ai_col1.head.founding_father[3] != -1) {
      return fail("withdrawn nation elected FF");
    }

    /* One elect per nation per tick: second tick needed for another. */
    ai->liberty_bells_total = 80;
    ai->next_founding_father = 1; /* Fugger still free */
    ai->boycott_bitmap = (uint16_t)((1u << 1) | (1u << 4));
    ai_col1.head.unknown46[2] = 1; /* human refuse flag — AI Fugger must not clear */
    const uint32_t ai_gold_before = ai->gold;
    founding_fathers_tick(&ai_ctx);
    if (ai_col1.head.founding_father[1] != 1 || ai->founding_father_count != 2) {
      return fail("AI second elect not Fugger");
    }
    if (ai->gold != ai_gold_before + 50u) {
      return fail("AI Fugger gold +50 missing");
    }
    if ((ai->boycott_bitmap & ((1u << 1) | (1u << 4))) != 0) {
      return fail("AI Fugger did not clear Sugar/Furs bits");
    }
    if (ai_col1.head.unknown46[2] != 1) {
      return fail("AI Fugger cleared human unknown46[2]");
    }
  }

  printf("smoke_founding_fathers: OK\n");
  return 0;
}
