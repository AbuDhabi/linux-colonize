/* Smoke: King/REF SoL, tax→REF, boycott, declare, 1528 announce, 10f0, 2244 merc. */
#include "core/ai_king.h"
#include "core/colony.h"
#include "core/col1_save.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_king: FAIL %s\n", msg);
  return 1;
}

static int count_active(const ColonizeUnitPool* units) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (units->units[i].active) {
      n++;
    }
  }
  return n;
}

static int count_nation(const ColonizeUnitPool* units, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    if (units->units[i].active && units->units[i].nation_id == nation_id) {
      n++;
    }
  }
  return n;
}

int main(void) {
  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.difficulty = 0;
  memset(col1.head.unknown46, 0, sizeof(col1.head.unknown46));
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
  for (int i = 0; i < 4; ++i) {
    col1.player[i].control = 0;
    memset(&col1.nation[i], 0, sizeof(col1.nation[i]));
  }

  /* SoL from rebel_dividend/divisor. */
  col1.head.colony_count = 1;
  col1.colony = calloc(1, sizeof(ColonizeCol1Colony));
  if (!col1.colony) {
    return fail("alloc colony");
  }
  col1.colony[0].nation_id = 0;
  col1.colony[0].population = 4;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  map.width = 16;
  map.height = 16;
  map.tile_count = 256;
  map.terrain = calloc(256, 1);
  map.layer2 = calloc(256, 1);
  map.layer3 = calloc(256, 1);
  if (!map.terrain || !map.layer2 || !map.layer3) {
    return fail("alloc map");
  }
  for (int i = 0; i < 256; ++i) {
    map.terrain[i] = 1; /* land */
  }
  map.terrain[5 * 16 + 4] = 25; /* ocean west of colony for MoW */

  ColonizeUnitPool units;
  units_reset(&units);
  units.type_count = 4;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 1;
  units.types[0].attack = 3;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Man-O-War");
  units.types[1].movement = 4;
  units.types[1].domain = 1;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Dragoon");
  units.types[2].movement = 2;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Artillery");
  units.types[3].movement = 1;

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  ColonizeColony* c = &colonies.colonies[0];
  c->id = 0;
  c->active = true;
  c->nation_id = 0;
  c->x = 5;
  c->y = 5;
  c->population = 4;
  c->colonist_count = 4;
  colonies.colony_count = 1;

  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.tax_percent = 0;

  uint16_t year = 1536;
  uint16_t autumn = 0;
  uint32_t turn = 1;
  char status[128];
  status[0] = '\0';
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
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

  const int sol = ai_king_sol_percent(&ctx, 0);
  if (sol != 60) {
    fprintf(stderr, "smoke_ai_king: unexpected SoL %d (want 60)\n", sol);
    return fail("SoL from rebel fields");
  }

  /* Tax hike + REF bump on spring tax year (peacetime; bells low → no declare). */
  const uint16_t pool0 = col1.head.expeditionary_force[0];
  const uint8_t tax0 = col1.nation[0].tax_rate;
  ai_king_nation_turn(&ctx);
  if (col1.nation[0].tax_rate <= tax0) {
    return fail("tax should hike on spring tax year");
  }
  if (col1.head.expeditionary_force[0] <= pool0) {
    return fail("tax should grow REF regulars");
  }
  if (europe.tax_percent != col1.nation[0].tax_rate) {
    return fail("europe tax_percent should sync");
  }
  if (col1.head.unknown46[0] != 0) {
    return fail("tax-only turn should not declare WoI");
  }
  if (col1.head.unknown46[2] != 0) {
    return fail("low tax_rate should not set boycott stand-in");
  }

  /*
   * Boycott/refuse stand-in (38fd_5be8 UI PARKED):
   * tax_rate>=20 + SoL>=30 → unknown46[2], sugar boycott bit, REF grow, no hike.
   * Next tax year while active: skip further hikes (and no extra REF grow).
   */
  year = 1558; /* 1536 + 22 */
  autumn = 0;
  col1.nation[0].tax_rate = 20;
  europe.tax_percent = 20;
  col1.nation[0].boycott_bitmap = 0;
  col1.nation[0].liberty_bells_total = 0; /* keep declare gated */
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  const uint16_t pool_boycott = col1.head.expeditionary_force[0];
  ai_king_nation_turn(&ctx);
  if (col1.head.unknown46[2] == 0) {
    return fail("refuse should set boycott flag unknown46[2]");
  }
  if (col1.nation[0].tax_rate != 20) {
    return fail("refuse should not hike tax_rate");
  }
  if ((col1.nation[0].boycott_bitmap & (1u << 1)) == 0) {
    return fail("refuse should set nation.boycott_bitmap sugar bit");
  }
  if (col1.head.expeditionary_force[0] <= pool_boycott) {
    return fail("refuse should grow REF once without tax hike");
  }
  if (col1.head.unknown46[0] != 0) {
    return fail("boycott turn should not declare WoI");
  }

  year = 1580; /* next tax year; boycott still active */
  autumn = 0;
  const uint8_t tax_held = col1.nation[0].tax_rate;
  const uint16_t pool_held = col1.head.expeditionary_force[0];
  ai_king_nation_turn(&ctx);
  if (col1.nation[0].tax_rate != tax_held) {
    return fail("active boycott should skip further tax hikes");
  }
  if (col1.head.expeditionary_force[0] != pool_held) {
    return fail("active boycott should not grow REF again");
  }
  if (col1.head.unknown46[2] == 0) {
    return fail("boycott flag should remain set");
  }

  /* Declare path: autumn skips tax; SoL≥50 + bells≥100. Wave runs same turn. */
  year = 1600;
  autumn = 1;
  col1.nation[0].liberty_bells_total = 200;
  col1.nation[0].gold = 0; /* merc gated until dedicated 2244 check */
  europe.gold = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  col1.player[1].control = 0;
  col1.player[2].control = 0;
  col1.player[3].control = 0;
  status[0] = '\0';
  const int units_before = count_active(&units);

  ai_king_nation_turn(&ctx);
  if (col1.head.unknown46[0] == 0) {
    return fail("declare should set WoI flag unknown46[0]");
  }
  if (col1.player[1].control != 2 || col1.player[2].control != 2 ||
      col1.player[3].control != 2) {
    return fail("declare should withdraw other Euro control");
  }
  /* Seed then drain: residual +1 regular may leave pools non-zero; require spawn. */
  if (count_nation(&units, 1) < 1) {
    return fail("post-declare wave should spawn crown (nation 1) unit");
  }
  if (count_active(&units) <= units_before) {
    return fail("wave should increase unit count");
  }
  if (count_nation(&units, 0) != 0) {
    return fail("REF/irregular must not spawn as human nation");
  }
  /* Thin 1528: successful 0982 spawn writes arrival status (chrome PARKED). */
  if (!strstr(status, "Expeditionary Force") && !strstr(status, "arrived")) {
    fprintf(stderr, "smoke_ai_king: status after wave: '%s'\n", status);
    return fail("0982 wave should set thin 1528 arrival status");
  }
  /* Pools seeded on declare then drained; still expect REF-present stand-in. */
  if (col1.head.unknown46[1] == 0) {
    return fail("wave should set REF-present unknown46[1]");
  }
  /* Declare should seed thin backup_force (10f0 stand-in). */
  if (col1.head.backup_force[0] == 0 && col1.head.backup_force[1] == 0 &&
      col1.head.backup_force[2] == 0 && col1.head.backup_force[3] == 0) {
    return fail("declare should seed backup_force for 10f0");
  }
  if (col1.head.unknown46[3] != 0) {
    return fail("no gold → 2244 merc flag should stay clear");
  }

  /*
   * 10f0: REF empty + backup_force > 0 → foreign intervention landing
   * (crown-hostile nation 2 when human=0 / crown=1). 06a6 may also fire.
   */
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  /* Crown wave may have captured the port; restore human ownership for landing pick. */
  colonies.colonies[0].nation_id = 0;
  const uint16_t backup0 = col1.head.backup_force[0];
  const int intervene_before = count_nation(&units, 2);
  const int units_mid = count_active(&units);
  ai_king_nation_turn(&ctx);
  if (count_nation(&units, 2) <= intervene_before) {
    return fail("10f0 should spawn intervention (nation 2) when REF empty");
  }
  if (count_active(&units) <= units_mid) {
    return fail("intervention turn should increase unit count");
  }
  if (col1.head.backup_force[0] >= backup0 && backup0 > 0) {
    return fail("10f0 should drain backup_force");
  }
  if (count_nation(&units, 0) != 0) {
    return fail("intervention must not spawn as human nation");
  }

  /*
   * Thin 2244 merc auto-accept (hire dialog PARKED):
   * gold>=300 + SoL>50 + !unknown46[3] → spend 300, spawn human Soldier/Dragoon,
   * set merc-hired flag. Second wartime turn must not hire again.
   */
  colonies.colonies[0].nation_id = 0;
  col1.nation[0].gold = 450;
  europe.gold = 450;
  status[0] = '\0';
  const int human_before = count_nation(&units, 0);
  const uint32_t gold_before = col1.nation[0].gold;
  ai_king_nation_turn(&ctx);
  if (col1.head.unknown46[3] == 0) {
    return fail("2244 should set merc-hired unknown46[3]");
  }
  if (col1.nation[0].gold != gold_before - 300) {
    fprintf(stderr, "smoke_ai_king: gold after merc %u (want %u)\n",
            (unsigned)col1.nation[0].gold, (unsigned)(gold_before - 300));
    return fail("2244 should spend 300 gold");
  }
  if (europe.gold != (int)col1.nation[0].gold) {
    return fail("2244 should sync europe.gold");
  }
  if (count_nation(&units, 0) <= human_before) {
    return fail("2244 should spawn human Continental merc");
  }
  if (!strstr(status, "mercenar") && !strstr(status, "Continental")) {
    fprintf(stderr, "smoke_ai_king: status after merc: '%s'\n", status);
    return fail("2244 should set merc status line");
  }
  const int human_after = count_nation(&units, 0);
  const uint32_t gold_after = col1.nation[0].gold;
  ai_king_nation_turn(&ctx);
  if (col1.nation[0].gold != gold_after) {
    return fail("merc flag should block second 2244 spend");
  }
  if (count_nation(&units, 0) != human_after) {
    return fail("merc flag should block second human merc spawn");
  }

  const uint8_t tax_final = col1.nation[0].tax_rate;
  const int crown_final = count_nation(&units, 1);
  const int intervene_final = count_nation(&units, 2);
  const int boycott_final = col1.head.unknown46[2];
  const int merc_final = col1.head.unknown46[3];
  free(map.terrain);
  free(map.layer2);
  free(map.layer3);
  col1_save_free(&col1);
  fprintf(stderr,
          "smoke_ai_king: ok (sol=%d tax=%u crown=%d intervene=%d boycott=%d merc=%d)\n",
          sol, tax_final, crown_final, intervene_final, boycott_final, merc_final);
  return 0;
}
