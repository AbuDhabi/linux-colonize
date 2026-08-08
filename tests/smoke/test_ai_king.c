/* Smoke: King/REF SoL, tax→REF, boycott audience + Fugger sync, tax SoL≥30 gate
 * (+ SoL-low hike assert), SoL chrome, declare+160a/1528/congress, MoW cargo×3
 * Regular+Dragoon mix + second MoW@diff≥2, 10f0 (dual + third@diff≥2
 * Regular+Dragoon mix + nation pick), REF land hunt/capture+owner-change+status
 * +fortify one, REF stack extras hunt, idle fortify extras hunt, after-capture
 * next colony hunt, idle Regular fortify on crown/captured capital, Artillery
 * after capture / idle on crown colony FORTIFY (Euro pattern), Artillery siege
 * bias (+ adjacent unfortified must not override fortified), Dragoon/Cont. Cav
 * open-land bias (+ Cont. Army stays nearest negative), MoW+cargo AI_SAIL→coast
 * + unload-at-colony (Regular else Dragoon seize), idle empty MoW coastal patrol,
 * 0982 MoW on water adjacent, 2244 merc hire (Soldier type) or cannot-afford once
 * (+ refuse→later gold still blocked via unknown46[3]), 1eca colony-SoL bias +
 * Cont. Army/Cont. Cav capital-rally, REF capital MD hunt bias, congress
 * unknown46[5] on declare, WoI unknown46[0] only when SoL≥50 (bells≥100 alone
 * insufficient). MoW×6 chrome PARKED (structural hold≤3); extra refuse cargos
 * beyond Sugar PARKED. */
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

/* Crown sea vs land (MoW cargo unload stand-in). */
static int count_nation_sea(const ColonizeUnitPool* units, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (units_is_sea(units, u->id)) {
      n++;
    }
  }
  return n;
}

static int count_nation_land(const ColonizeUnitPool* units, int nation_id) {
  int n = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units->units[i];
    if (!u->active || u->nation_id != nation_id) {
      continue;
    }
    if (!units_is_sea(units, u->id)) {
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
  col1.colony[0].x = 5;
  col1.colony[0].y = 5;
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
  units.type_count = 8;
  snprintf(units.types[0].name, sizeof(units.types[0].name), "Regular");
  units.types[0].movement = 1;
  units.types[0].attack = 3;
  units.types[0].defense = 2;
  snprintf(units.types[1].name, sizeof(units.types[1].name), "Man-O-War");
  units.types[1].movement = 4;
  units.types[1].domain = 1;
  units.types[1].cargo = 6;
  snprintf(units.types[2].name, sizeof(units.types[2].name), "Dragoon");
  units.types[2].movement = 2;
  snprintf(units.types[3].name, sizeof(units.types[3].name), "Artillery");
  units.types[3].movement = 1;
  snprintf(units.types[4].name, sizeof(units.types[4].name), "Soldier");
  units.types[4].movement = 1;
  units.types[4].attack = 2;
  units.types[4].defense = 2;
  snprintf(units.types[5].name, sizeof(units.types[5].name), "Continental Army");
  units.types[5].movement = 1;
  units.types[5].attack = 4;
  units.types[5].defense = 4;
  snprintf(units.types[6].name, sizeof(units.types[6].name), "Continental Cavalry");
  units.types[6].movement = 4;
  units.types[6].attack = 5;
  units.types[6].defense = 5;
  snprintf(units.types[7].name, sizeof(units.types[7].name), "Veteran Soldier");
  units.types[7].movement = 1;
  units.types[7].attack = 3;
  units.types[7].defense = 3;
  const int ty_regular = 0;
  const int ty_mow = 1;
  const int ty_soldier = 4;
  const int ty_dragoon = 2;
  const int ty_artillery = 3;
  const int ty_cont_army = 5;
  const int ty_cont_cav = 6;
  const int ty_vet_soldier = 7;

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
  snprintf(c->name, sizeof(c->name), "Jamestown");
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
  status[0] = '\0';
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
  /* 1d42: status buffer must mention the new rate when present. */
  {
    char rate_buf[16];
    snprintf(rate_buf, sizeof(rate_buf), "%u", (unsigned)col1.nation[0].tax_rate);
    if (!strstr(status, "raises taxes") || !strstr(status, rate_buf)) {
      fprintf(stderr, "smoke_ai_king: tax hike status: '%s' (want rate %s)\n", status,
              rate_buf);
      return fail("1d42 tax hike should mention new rate in status");
    }
  }
  if (col1.head.unknown46[0] != 0) {
    return fail("tax-only turn should not declare WoI");
  }
  if (col1.head.unknown46[2] != 0) {
    return fail("low tax_rate should not set boycott stand-in");
  }

  /*
   * Boycott/refuse + thin 38fd_5be8 audience status:
   * tax_rate>=20 + SoL>=30 → unknown46[2], sugar boycott bit, REF grow, no hike,
   * audience status line. Next tax year while active: skip further hikes
   * (hold-audience status; no extra REF grow).
   */
  year = 1558; /* 1536 + 22 */
  autumn = 0;
  col1.nation[0].tax_rate = 20;
  europe.tax_percent = 20;
  col1.nation[0].boycott_bitmap = 0;
  col1.nation[0].liberty_bells_total = 0; /* keep declare gated */
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  const uint16_t pool_boycott = col1.head.expeditionary_force[0];
  status[0] = '\0';
  ai_king_nation_turn(&ctx);
  if (col1.head.unknown46[2] == 0) {
    return fail("refuse should set boycott flag unknown46[2]");
  }
  if (col1.nation[0].tax_rate != 20) {
    return fail("refuse should not hike tax_rate");
  }
  /* Sugar = COLONIZE_CARGO_SUGAR (bit1); extra refuse cargos PARKED. */
  if ((col1.nation[0].boycott_bitmap & (1u << COLONIZE_CARGO_SUGAR)) == 0) {
    return fail("refuse should set nation.boycott_bitmap Sugar bit (COLONIZE_CARGO_SUGAR)");
  }
  if (col1.head.expeditionary_force[0] <= pool_boycott) {
    return fail("refuse should grow REF once without tax hike");
  }
  if (col1.head.unknown46[0] != 0) {
    return fail("boycott turn should not declare WoI");
  }
  if (!strstr(status, "Audience") || !strstr(status, "refuse") ||
      !strstr(status, "tax increase") || !strstr(status, "Tax stays") ||
      !strstr(status, "20")) {
    fprintf(stderr, "smoke_ai_king: refuse audience status: '%s'\n", status);
    return fail("38fd_5be8 refuse should set Audience status with Tax stays rate");
  }

  year = 1580; /* next tax year; boycott still active */
  autumn = 0;
  const uint8_t tax_held = col1.nation[0].tax_rate;
  const uint16_t pool_held = col1.head.expeditionary_force[0];
  status[0] = '\0';
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
  if (!strstr(status, "boycott holds") && !strstr(status, "Audience")) {
    fprintf(stderr, "smoke_ai_king: boycott hold status: '%s'\n", status);
    return fail("active boycott should set hold-audience status");
  }
  /*
   * Audience vs restless: spring refuse at SoL 40..49 must keep Audience status
   * (restless chrome must not clobber 38fd_5be8). Real modal PARKED.
   */
  {
    year = 1602; /* tax year after 1580+22 — wait, interval is 22-diff*2=22 at diff0 */
    /* Use a known tax year: first=1536, interval=22 → 1536+22*k. Pick 1624. */
    year = 1624;
    autumn = 0;
    col1.colony[0].rebel_dividend = 45;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].tax_rate = 20;
    europe.tax_percent = 20;
    col1.nation[0].boycott_bitmap = 0;
    col1.head.unknown46[2] = 0;
    col1.nation[0].liberty_bells_total = 0;
    status[0] = '\0';
    {
      const int sol45a = ai_king_sol_percent(&ctx, 0);
      if (sol45a != 45) {
        fprintf(stderr, "smoke_ai_king: unexpected SoL %d (want 45) for audience polish\n",
                sol45a);
        return fail("audience+restless SoL setup");
      }
    }
    ai_king_nation_turn(&ctx);
    if (col1.head.unknown46[2] == 0) {
      return fail("SoL45 refuse should set boycott unknown46[2]");
    }
    if (!strstr(status, "Audience") || !strstr(status, "refuse") ||
        !strstr(status, "Tax stays")) {
      fprintf(stderr, "smoke_ai_king: audience vs restless status: '%s'\n", status);
      return fail("refuse at SoL 40-49 must keep Audience status (not restless only)");
    }
    if (strstr(status, "Sons of Liberty") && !strstr(status, "refuse")) {
      return fail("restless chrome must not replace Audience refuse status");
    }
    /* Reset SoL for later declare path; clear boycott for clean declare. */
    col1.colony[0].rebel_dividend = 60;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].boycott_bitmap = 0;
    col1.head.unknown46[2] = 0;
  }
  /*
   * External boycott clear (Fugger/diplo — do not touch FF): bitmap==0 →
   * clear unknown46[2] refuse so tax may resume next hike year.
   */
  {
    col1.head.unknown46[2] = 1; /* refuse still set; cargo bits already cleared */
    col1.nation[0].boycott_bitmap = 0;
    year = 1581; /* off-tax year; sync still runs */
    autumn = 0;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    if (col1.head.unknown46[2] != 0) {
      return fail("bitmap==0 should clear unknown46[2] refuse (Fugger sync)");
    }
  }

  /*
   * Tax hike SoL gate (1d42 / AI_KING_BOYCOTT_SOL_MIN=30): tax_rate≥20 but
   * SoL<30 and bells low → hike (refuse requires SoL≥30 or bells≥80).
   * Asserts the existing threshold; real audience modal PARKED.
   */
  {
    year = 1646; /* 1536 + 22*5 spring tax year */
    autumn = 0;
    col1.colony[0].rebel_dividend = 25;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].tax_rate = 20;
    europe.tax_percent = 20;
    col1.nation[0].boycott_bitmap = 0;
    col1.head.unknown46[2] = 0;
    col1.nation[0].liberty_bells_total = 0;
    {
      const int sol25 = ai_king_sol_percent(&ctx, 0);
      if (sol25 != 25) {
        fprintf(stderr, "smoke_ai_king: unexpected SoL %d (want 25) for tax SoL gate\n",
                sol25);
        return fail("tax hike SoL gate setup");
      }
    }
    const uint8_t tax_low_sol = col1.nation[0].tax_rate;
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    if (col1.head.unknown46[2] != 0) {
      return fail("SoL<30 should not refuse tax hike (SoL gate)");
    }
    if (col1.nation[0].tax_rate <= tax_low_sol) {
      return fail("SoL<30 + tax≥20 should still hike (refuse needs SoL≥30)");
    }
    if (!strstr(status, "raises taxes")) {
      fprintf(stderr, "smoke_ai_king: low-SoL hike status: '%s'\n", status);
      return fail("SoL<30 tax year should hike with raises-taxes status");
    }
    /* Restore SoL for later declare path. */
    col1.colony[0].rebel_dividend = 60;
    col1.colony[0].rebel_divisor = 100;
  }

  /*
   * Thin pre-declare SoL chrome: SoL 40..49 → restless status; no congress yet.
   * Autumn skips tax so status is not overwritten by a tax hike line.
   */
  year = 1590;
  autumn = 1;
  col1.colony[0].rebel_dividend = 45;
  col1.colony[0].rebel_divisor = 100;
  col1.nation[0].liberty_bells_total = 50; /* below declare gate */
  status[0] = '\0';
  {
    const int sol45 = ai_king_sol_percent(&ctx, 0);
    if (sol45 != 45) {
      fprintf(stderr, "smoke_ai_king: unexpected SoL %d (want 45)\n", sol45);
      return fail("SoL chrome setup");
    }
  }
  ai_king_nation_turn(&ctx);
  if (col1.head.unknown46[0] != 0) {
    return fail("SoL 45 should not declare WoI");
  }
  if (col1.head.unknown46[5] != 0) {
    return fail("SoL 45 should not set congress confirm unknown46[5]");
  }
  if (!strstr(status, "Sons of Liberty") || !strstr(status, "45")) {
    fprintf(stderr, "smoke_ai_king: SoL chrome status: '%s'\n", status);
    return fail("SoL 40-49 should set restless status line");
  }
  /* unknown46 consistency: restless chrome must not set WoI or congress. */
  if (col1.head.unknown46[0] != 0 || col1.head.unknown46[5] != 0) {
    return fail("restless SoL chrome must leave WoI/congress unknown46 clear");
  }
  /* Optional tax mention when tax_rate already in refuse band (≥20). */
  if (col1.nation[0].tax_rate >= 20 &&
      !strstr(status, "Tax is at") && !strstr(status, "tax")) {
    fprintf(stderr, "smoke_ai_king: restless+high-tax status: '%s'\n", status);
    return fail("SoL restless with high tax_rate should mention tax");
  }

  /*
   * WoI unknown46[0] SoL gate (FUN_43f7_2564 / fandom total SoL ≥ 50%):
   * SoL 49 + bells≥100 must NOT declare — bells alone are insufficient.
   * Restless chrome may fire; congress/WoI stay clear.
   */
  {
    year = 1591;
    autumn = 1;
    col1.colony[0].rebel_dividend = 49;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 200; /* would declare if SoL≥50 */
    status[0] = '\0';
    {
      const int sol49 = ai_king_sol_percent(&ctx, 0);
      if (sol49 != 49) {
        fprintf(stderr, "smoke_ai_king: unexpected SoL %d (want 49)\n", sol49);
        return fail("SoL49 declare-gate setup");
      }
    }
    ai_king_nation_turn(&ctx);
    if (col1.head.unknown46[0] != 0) {
      return fail("SoL 49 + bells≥100 must not set WoI unknown46[0]");
    }
    if (col1.head.unknown46[5] != 0) {
      return fail("SoL 49 + bells≥100 must not set congress unknown46[5]");
    }
  }

  /* Declare path: autumn skips tax; SoL≥50 + bells≥100. Wave runs same turn. */
  year = 1600;
  autumn = 1;
  col1.colony[0].rebel_dividend = 60;
  col1.colony[0].rebel_divisor = 100;
  col1.nation[0].liberty_bells_total = 200;
  col1.nation[0].gold = 0; /* 2244 cannot-afford once; hire cleared later */
  europe.gold = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  col1.player[1].control = 0;
  col1.player[2].control = 0;
  col1.player[3].control = 0;
  status[0] = '\0';
  const int units_before = count_active(&units);

  /* Seed a pre-declare country so 160a rename is observable. */
  snprintf(col1.player[0].country_name, sizeof(col1.player[0].country_name), "England");
  snprintf(europe.nation_name, sizeof(europe.nation_name), "England");

  ai_king_nation_turn(&ctx);
  if (col1.head.unknown46[0] == 0) {
    return fail("declare should set WoI flag unknown46[0]");
  }
  if (col1.head.unknown46[5] == 0) {
    return fail("declare should set congress confirm unknown46[5]");
  }
  /* Thin 160a: rename stand-in (cinematic PARKED). Status may be overwritten by 1528.
   * Thin 2564 congress: unknown46[5] + country_name prove confirm; wave may clobber status. */
  if (strcmp(col1.player[0].country_name, "United Colonies") != 0) {
    fprintf(stderr, "smoke_ai_king: country_name after declare: '%s'\n",
            col1.player[0].country_name);
    return fail("160a declare should rename player.country_name to United Colonies");
  }
  if (strcmp(europe.nation_name, "United Colonies") != 0) {
    return fail("160a declare should sync europe.nation_name");
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
  /*
   * Thin MoW cargo unload (hold size 3 stand-in; full cargo chrome PARKED):
   * declare seeds force[2]>0 + force[0]≥3 → same-beat MoW + ≥3 land Regulars.
   * Intervene/0982 landing water: MoW must spawn on water adjacent to the
   * target colony (fandom REF man-o-war → ports). MoW×6 embark chrome PARKED.
   */
  {
    const int crown_sea = count_nation_sea(&units, 1);
    const int crown_land = count_nation_land(&units, 1);
    if (crown_sea < 1 || crown_land < 3) {
      fprintf(stderr, "smoke_ai_king: post-declare MoW cargo sea=%d land=%d (want ≥1 ship + ≥3 land)\n",
              crown_sea, crown_land);
      return fail("0982 MoW spawn should unload ≥3 land cargo (or ship+land)");
    }
    int mow_on_adj_water = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 1 || !units_is_sea(&units, u->id)) {
        continue;
      }
      if (u->type_index != ty_mow) {
        continue;
      }
      if (!map_tile_is_water(&map, u->x, u->y)) {
        fprintf(stderr, "smoke_ai_king: MoW at (%d,%d) not on water\n", u->x, u->y);
        return fail("0982 MoW must spawn on water tile");
      }
      /* Adjacent to Jamestown (5,5). */
      if (abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1 && !(u->x == 5 && u->y == 5)) {
        mow_on_adj_water = 1;
      }
    }
    if (!mow_on_adj_water) {
      return fail("0982 MoW must spawn on water adjacent to target colony");
    }
  }
  /* Thin 1528: successful 0982 spawn writes arrival status (chrome PARKED).
   * Same-turn war_act may overwrite with capture or 2244 cannot-afford. */
  if (!strstr(status, "Expeditionary Force") && !strstr(status, "arrived") &&
      !strstr(status, "captured") && !strstr(status, "Cannot afford mercenaries")) {
    fprintf(stderr, "smoke_ai_king: status after wave: '%s'\n", status);
    return fail("0982 wave should set thin 1528 arrival (or same-turn capture/merc) status");
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
  if (col1.head.unknown46[3] == 0) {
    return fail("no gold → 2244 cannot-afford should set unknown46[3]");
  }
  if (!strstr(status, "Cannot afford mercenaries") && !strstr(status, "captured") &&
      !strstr(status, "Expeditionary Force") && !strstr(status, "arrived")) {
    fprintf(stderr, "smoke_ai_king: status after no-gold declare: '%s'\n", status);
    return fail("no gold → 2244 should set cannot-afford (or same-turn wave/capture) status");
  }
  /* Capture may overwrite cannot-afford; force a clean refuse check next. */
  {
    status[0] = '\0';
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    col1.head.unknown46[3] = 0; /* re-arm once-gate for refuse probe */
    colonies.colonies[0].nation_id = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0; /* park crown so capture does not clobber status */
      }
    }
    ai_king_nation_turn(&ctx);
    if (col1.head.unknown46[3] == 0) {
      return fail("2244 cannot-afford probe should set unknown46[3]");
    }
    if (!strstr(status, "Cannot afford mercenaries")) {
      fprintf(stderr, "smoke_ai_king: cannot-afford status: '%s'\n", status);
      return fail("2244 gold-insufficient should write Cannot afford mercenaries.");
    }
    /* Second wartime turn: gate blocks refuse-status spam. */
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    if (strstr(status, "Cannot afford mercenaries")) {
      return fail("merc unknown46[3] should block cannot-afford status spam");
    }
    /*
     * Once-per-war across refuse→hire: unknown46[3] from cannot-afford must
     * still block a later gold bump (no spend / spawn / hire status).
     * Source: FUN_43f7_2244 once-gate; real modal PARKED.
     */
    {
      const int human_refuse = count_nation(&units, 0);
      const uint32_t gold_refuse = 500;
      col1.nation[0].gold = gold_refuse;
      europe.gold = (int)gold_refuse;
      status[0] = '\0';
      if (col1.head.unknown46[3] == 0) {
        return fail("refuse→later-hire probe needs unknown46[3] still set");
      }
      ai_king_nation_turn(&ctx);
      if (col1.nation[0].gold != gold_refuse) {
        return fail("unknown46[3] refuse must block later 2244 spend when gold arrives");
      }
      if (count_nation(&units, 0) != human_refuse) {
        return fail("unknown46[3] refuse must block later 2244 merc spawn");
      }
      if (strstr(status, "Mercenaries join")) {
        return fail("unknown46[3] refuse must block later 2244 hire status");
      }
      if (col1.head.unknown46[3] == 0) {
        return fail("refuse→later-hire probe must leave unknown46[3] set");
      }
    }
  }

  /*
   * 10f0 deepen: REF empty + larger backup_force → up to 2 landings
   * (prefer Regular+Dragoon mix). Crown-hostile nation 2 when human=0 /
   * crown=1. 06a6 may also fire. Economy / merc chrome PARKED.
   */
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  /* Crown wave may have captured the port; restore human ownership for landing pick. */
  colonies.colonies[0].nation_id = 0;
  col1.head.backup_force[0] = 4; /* Regular */
  col1.head.backup_force[1] = 3; /* Dragoon — mix with Regular */
  col1.head.backup_force[2] = 0;
  col1.head.backup_force[3] = 2; /* Artillery */
  const int backup_total_before = (int)col1.head.backup_force[0] +
                                  (int)col1.head.backup_force[1] +
                                  (int)col1.head.backup_force[2] +
                                  (int)col1.head.backup_force[3];
  const uint16_t backup_reg_before = col1.head.backup_force[0];
  const uint16_t backup_drg_before = col1.head.backup_force[1];
  const int intervene_before = count_nation(&units, 2);
  const int units_mid = count_active(&units);
  ai_king_nation_turn(&ctx);
  const int intervene_spawned = count_nation(&units, 2) - intervene_before;
  const int backup_total_after = (int)col1.head.backup_force[0] +
                                 (int)col1.head.backup_force[1] +
                                 (int)col1.head.backup_force[2] +
                                 (int)col1.head.backup_force[3];
  const int backup_drained = backup_total_before - backup_total_after;
  if (intervene_spawned < 1) {
    return fail("10f0 should spawn intervention (nation 2) when REF empty");
  }
  if (intervene_spawned < 2 && backup_drained < 2) {
    fprintf(stderr, "smoke_ai_king: 10f0 spawned=%d drained=%d (want >=2 either)\n",
            intervene_spawned, backup_drained);
    return fail("10f0 deepen should spawn >=2 units or drain backup by 2");
  }
  if (count_active(&units) <= units_mid) {
    return fail("intervention turn should increase unit count");
  }
  /* Mix preference: both Regular and Dragoon pools were live → drain one each. */
  if (col1.head.backup_force[0] != backup_reg_before - 1 ||
      col1.head.backup_force[1] != backup_drg_before - 1) {
    return fail("10f0 should prefer Regular+Dragoon mix (drain one of each)");
  }
  if (count_nation(&units, 0) != 0) {
    return fail("intervention must not spawn as human nation");
  }

  /*
   * 10f0 third landing: difficulty≥2 + REF empty + backup pools → up to 3
   * landings (Regular+Dragoon mix then next pool). Assert mix drain + third.
   */
  col1.head.difficulty = 2;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  colonies.colonies[0].nation_id = 0;
  col1.head.backup_force[0] = 3;
  col1.head.backup_force[1] = 3;
  col1.head.backup_force[2] = 0;
  col1.head.backup_force[3] = 2;
  {
    const uint16_t reg_before3 = col1.head.backup_force[0];
    const uint16_t drg_before3 = col1.head.backup_force[1];
    const int backup_before3 = (int)col1.head.backup_force[0] + (int)col1.head.backup_force[1] +
                               (int)col1.head.backup_force[2] + (int)col1.head.backup_force[3];
    const int intervene_before3 = count_nation(&units, 2);
    int reg_land_before = 0;
    int drg_land_before = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 2) {
        continue;
      }
      if (u->type_index == ty_regular) {
        reg_land_before++;
      } else if (u->type_index == ty_dragoon) {
        drg_land_before++;
      }
    }
    ai_king_nation_turn(&ctx);
    const int intervene_spawned3 = count_nation(&units, 2) - intervene_before3;
    const int backup_after3 = (int)col1.head.backup_force[0] + (int)col1.head.backup_force[1] +
                              (int)col1.head.backup_force[2] + (int)col1.head.backup_force[3];
    const int drained3 = backup_before3 - backup_after3;
    if (intervene_spawned3 < 3 && drained3 < 3) {
      fprintf(stderr, "smoke_ai_king: 10f0@diff2 spawned=%d drained=%d (want >=3 either)\n",
              intervene_spawned3, drained3);
      return fail("10f0 difficulty≥2 should spawn/drain up to 3 landings");
    }
    /* Mix: both Regular and Dragoon pools live → drain ≥1 of each. */
    if (col1.head.backup_force[0] > reg_before3 - 1 ||
        col1.head.backup_force[1] > drg_before3 - 1) {
      fprintf(stderr,
              "smoke_ai_king: 10f0@diff2 mix pools reg %u→%u drg %u→%u (want ≥1 each)\n",
              (unsigned)reg_before3, (unsigned)col1.head.backup_force[0],
              (unsigned)drg_before3, (unsigned)col1.head.backup_force[1]);
      return fail("10f0@diff2 third landing should keep Regular+Dragoon mix");
    }
    {
      int reg_land_after = 0;
      int drg_land_after = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 2) {
          continue;
        }
        if (u->type_index == ty_regular) {
          reg_land_after++;
        } else if (u->type_index == ty_dragoon) {
          drg_land_after++;
        }
      }
      if (reg_land_after <= reg_land_before || drg_land_after <= drg_land_before) {
        fprintf(stderr,
                "smoke_ai_king: 10f0@diff2 land types reg %d→%d drg %d→%d\n",
                reg_land_before, reg_land_after, drg_land_before, drg_land_after);
        return fail("10f0@diff2 should spawn both Regular and Dragoon (mix)");
      }
    }
  }
  col1.head.difficulty = 0; /* restore for later checks */

  /*
   * 10f0 intervene nation pick: prefer Euro with most colonies (not first slot).
   * human=0 crown=1 → candidates 2 and 3; seed a nation-3 colony → land as 3.
   * Structural selection only (Foreign intervention / 10f0).
   */
  {
    ColonizeColony* c3 = &colonies.colonies[1];
    c3->id = 1;
    c3->active = true;
    c3->nation_id = 3;
    c3->x = 8;
    c3->y = 8;
    c3->population = 2;
    c3->colonist_count = 2;
    if (colonies.colony_count < 2) {
      colonies.colony_count = 2;
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    colonies.colonies[0].nation_id = 0;
    col1.head.backup_force[0] = 2;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    const int n2_before = count_nation(&units, 2);
    const int n3_before = count_nation(&units, 3);
    ai_king_nation_turn(&ctx);
    const int n3_spawned = count_nation(&units, 3) - n3_before;
    const int n2_spawned = count_nation(&units, 2) - n2_before;
    if (n3_spawned < 1) {
      fprintf(stderr, "smoke_ai_king: intervene nation pick n3=%d n2=%d\n", n3_spawned,
              n2_spawned);
      return fail("10f0 should intervene as Euro with most colonies (nation 3)");
    }
    c3->active = false; /* don't perturb later weakest-port / hunt picks */
  }

  /*
   * REF land hunt + colony capture + fortify Regular (fandom REF AI / conquest):
   * Idle crown Regular with moves → AI_MOVE toward nearest human land unit;
   * REF on human colony tile → colonies_capture (even with 0 moves) then
   * UNITS_ORDER_FORTIFY on the capturing Regular.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    /* Park existing crown movers so hunt/capture probes are stable. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
      }
    }
    /* Capture: REF Regular already on human colony, no moves. */
    const int cap_id = units_spawn_allow_stack(&units, ty_regular, 5, 5);
    if (cap_id < 0) {
      return fail("capture setup should spawn crown Regular on colony");
    }
    {
      ColonizeUnit* cu = units_get(&units, cap_id);
      if (!cu) {
        return fail("capture setup unit lookup");
      }
      cu->nation_id = 1;
      cu->moves_left = 0;
      cu->orders = UNITS_ORDER_NONE;
      cu->goto_x = -1;
      cu->goto_y = -1;
    }
    /* Hunt: human Soldier at (12,5); idle crown Regular at (10,5) with moves. */
    const int prey_id = units_spawn_allow_stack(&units, ty_soldier, 12, 5);
    const int hunter_id = units_spawn_allow_stack(&units, ty_regular, 10, 5);
    if (prey_id < 0 || hunter_id < 0) {
      return fail("land-hunt setup should spawn human Soldier + crown Regular");
    }
    {
      ColonizeUnit* prey = units_get(&units, prey_id);
      ColonizeUnit* hunter = units_get(&units, hunter_id);
      if (!prey || !hunter) {
        return fail("land-hunt setup unit lookup");
      }
      prey->nation_id = 0;
      prey->moves_left = 0;
      hunter->nation_id = 1;
      hunter->moves_left = 1;
      hunter->orders = UNITS_ORDER_NONE;
      hunter->goto_x = -1;
      hunter->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    /* REF capture clears human ownership (conquest — colonies_capture). */
    if (colonies.colonies[0].nation_id != 1) {
      return fail("REF on human colony should colonies_capture (owner → crown)");
    }
    if (colonies.colonies[0].nation_id == 0) {
      return fail("REF capture must clear human colony ownership");
    }
    /* Thin conquest status (full chrome PARKED): exact phrase + colony name. */
    if (!strstr(status, "The King's forces have captured") || !strstr(status, "Jamestown")) {
      fprintf(stderr, "smoke_ai_king: capture status: '%s'\n", status);
      return fail("REF capture should set 'The King's forces have captured %s!' status");
    }
    {
      /* Any crown Regular on the colony may capture first (wave leftovers); one
       * must end FORTIFY after capture. */
      int fortified = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 1 || u->x != 5 || u->y != 5) {
          continue;
        }
        if (u->type_index == ty_regular && u->orders == UNITS_ORDER_FORTIFY) {
          fortified = 1;
          break;
        }
      }
      if (!fortified) {
        const ColonizeUnit* cu = units_get_const(&units, cap_id);
        fprintf(stderr, "smoke_ai_king: post-capture cap orders=%d (want a FORTIFY Regular)\n",
                cu ? cu->orders : -1);
        return fail("capture should fortify one Regular on colony tile");
      }
    }
    {
      const ColonizeUnit* hunter = units_get_const(&units, hunter_id);
      if (!hunter || !hunter->active) {
        return fail("land-hunt Regular should remain active");
      }
      if (hunter->orders != UNITS_ORDER_AI_MOVE || hunter->goto_x != 12 ||
          hunter->goto_y != 5) {
        fprintf(stderr, "smoke_ai_king: hunt goto=(%d,%d) orders=%d (want AI_MOVE→12,5)\n",
                hunter->goto_x, hunter->goto_y, hunter->orders);
        return fail("REF Regular should AI_MOVE toward nearest human land unit");
      }
      /* One step east toward prey (10,5) → (11,5). */
      if (hunter->x != 11 || hunter->y != 5) {
        fprintf(stderr, "smoke_ai_king: hunt pos=(%d,%d) (want 11,5)\n", hunter->x,
                hunter->y);
        return fail("REF land hunt should step toward human land unit");
      }
    }
    /*
     * REF stack: second Regular with moves on captured (crown) colony must not
     * fortify when one Regular is already FORTIFY — extras hunt instead.
     */
    {
      /* Clear the colony tile: only one garrison Regular may remain. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 1) {
          continue;
        }
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
          u->orders = UNITS_ORDER_NONE;
        }
      }
      /* Sole fortified garrison on crown colony. */
      {
        ColonizeUnit* cu = units_get(&units, cap_id);
        if (!cu || !cu->active) {
          return fail("REF stack needs capture Regular as garrison");
        }
        cu->x = 5;
        cu->y = 5;
        cu->nation_id = 1;
        cu->orders = UNITS_ORDER_FORTIFY;
        cu->moves_left = 0;
      }
      colonies.colonies[0].nation_id = 1;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      col1.head.backup_force[0] = 0;
      col1.head.backup_force[1] = 0;
      col1.head.backup_force[2] = 0;
      col1.head.backup_force[3] = 0;
      const int extra_id = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      const int stack_prey = units_spawn_allow_stack(&units, ty_soldier, 12, 8);
      if (extra_id < 0 || stack_prey < 0) {
        return fail("REF stack setup should spawn extra Regular + human prey");
      }
      {
        ColonizeUnit* ex = units_get(&units, extra_id);
        ColonizeUnit* prey = units_get(&units, stack_prey);
        if (!ex || !prey) {
          return fail("REF stack unit lookup");
        }
        ex->nation_id = 1;
        ex->moves_left = 1;
        ex->orders = UNITS_ORDER_NONE;
        ex->goto_x = -1;
        ex->goto_y = -1;
        prey->nation_id = 0;
        prey->moves_left = 0;
      }
      ai_king_nation_turn(&ctx);
      int fortified = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        const ColonizeUnit* u = &units.units[i];
        if (!u->active || u->nation_id != 1 || u->type_index != ty_regular) {
          continue;
        }
        if ((u->x == 5 && u->y == 5) &&
            (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED)) {
          fortified++;
        }
      }
      {
        const ColonizeUnit* ex = units_get_const(&units, extra_id);
        if (!ex || !ex->active) {
          return fail("REF stack extra Regular should remain active");
        }
        if (ex->orders == UNITS_ORDER_FORTIFY || ex->orders == UNITS_ORDER_FORTIFIED) {
          return fail("REF stack: second Regular on colony must not fortify when one already has");
        }
        if (ex->orders != UNITS_ORDER_AI_MOVE) {
          fprintf(stderr, "smoke_ai_king: stack extra orders=%d (want AI_MOVE hunt)\n",
                  ex->orders);
          return fail("REF stack extra Regular should hunt, not fortify");
        }
      }
      if (fortified != 1) {
        fprintf(stderr, "smoke_ai_king: fortified Regulars on colony=%d (want exactly 1)\n",
                fortified);
        return fail("REF stack should keep exactly one fortified Regular on colony");
      }
    }
    /*
     * After-capture next colony (fandom REF uncaptured-port pressure):
     * founding capital captured + fortify stack taken → idle extra Regular must
     * prefer next nearest remaining human colony over a closer human land unit.
     * Capital MD slack must not apply (founding capital is no longer human).
     */
    {
      ColonizeColony* next_col = &colonies.colonies[2];
      next_col->id = 2;
      next_col->active = true;
      next_col->nation_id = 0;
      next_col->x = 12;
      next_col->y = 5;
      next_col->population = 2;
      next_col->colonist_count = 2;
      next_col->has_building[0] = false;
      snprintf(next_col->name, sizeof(next_col->name), "Plymouth");
      if (colonies.colony_count < 3) {
        colonies.colony_count = 3;
      }
      colonies.colonies[0].nation_id = 1; /* captured capital */
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      /* Sole fortified garrison on captured capital. */
      {
        ColonizeUnit* cu = units_get(&units, cap_id);
        if (!cu || !cu->active) {
          return fail("after-capture next-colony needs capture Regular as garrison");
        }
        cu->x = 5;
        cu->y = 5;
        cu->nation_id = 1;
        cu->orders = UNITS_ORDER_FORTIFY;
        cu->moves_left = 0;
      }
      /* Closer human Soldier decoy (MD=2) vs next colony at (12,5) (MD=7). */
      const int decoy_id = units_spawn_allow_stack(&units, ty_soldier, 7, 5);
      const int next_hunter = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      if (decoy_id < 0 || next_hunter < 0) {
        return fail("after-capture next-colony setup spawn");
      }
      {
        ColonizeUnit* decoy = units_get(&units, decoy_id);
        ColonizeUnit* h = units_get(&units, next_hunter);
        if (!decoy || !h) {
          return fail("after-capture next-colony unit lookup");
        }
        decoy->nation_id = 0;
        decoy->moves_left = 0;
        h->nation_id = 1;
        h->moves_left = 1;
        h->orders = UNITS_ORDER_NONE;
        h->goto_x = -1;
        h->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* h = units_get_const(&units, next_hunter);
        if (!h || !h->active) {
          return fail("after-capture next-colony hunter should remain active");
        }
        if (h->orders == UNITS_ORDER_FORTIFY || h->orders == UNITS_ORDER_FORTIFIED) {
          return fail("after-capture extra must not join capital fortify stack");
        }
        if (h->orders != UNITS_ORDER_AI_MOVE || h->goto_x != 12 || h->goto_y != 5) {
          fprintf(stderr,
                  "smoke_ai_king: after-capture next goto=(%d,%d) orders=%d "
                  "(want next colony 12,5 not decoy 7,5)\n",
                  h->goto_x, h->goto_y, h->orders);
          return fail("after-capture idle extra should hunt next nearest human colony");
        }
      }
      next_col->active = false;
    }
    /* Restore human port for later promote / merc checks. */
    colonies.colonies[0].nation_id = 0;
  }

  /*
   * REF idle fortify (heal stand-in): Regular on own (crown) colony with moves
   * and no adjacent human foe/colony → UNITS_ORDER_FORTIFY (not hunt away).
   * Also: idle Regular on captured human capital prefers FORTIFY when stack
   * allows; already-FORTIFIED capital garrison stays put.
   */
  {
    colonies.colonies[0].nation_id = 0;
    /* Isolated crown colony at (8,8) — far from human (5,5). */
    ColonizeColony* crown_col = &colonies.colonies[3];
    crown_col->id = 3;
    crown_col->active = true;
    crown_col->nation_id = 1;
    crown_col->x = 8;
    crown_col->y = 8;
    crown_col->population = 2;
    crown_col->colonist_count = 1;
    if (colonies.colony_count < 4) {
      colonies.colony_count = 4;
    }
    map.terrain[8 * 16 + 8] = 1; /* land */
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
      }
    }
    /* Park human land units so none are adjacent to (8,8). */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        if (abs(u->x - 8) <= 1 && abs(u->y - 8) <= 1) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    const int idle_id = units_spawn_allow_stack(&units, ty_regular, 8, 8);
    if (idle_id < 0) {
      return fail("idle fortify setup should spawn crown Regular on crown colony");
    }
    {
      ColonizeUnit* idle = units_get(&units, idle_id);
      if (!idle) {
        return fail("idle fortify unit lookup");
      }
      idle->nation_id = 1;
      idle->moves_left = 2;
      idle->orders = UNITS_ORDER_NONE;
      idle->goto_x = -1;
      idle->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* idle = units_get_const(&units, idle_id);
      if (!idle || !idle->active) {
        return fail("idle fortify Regular should remain active");
      }
      if (idle->orders != UNITS_ORDER_FORTIFY) {
        fprintf(stderr, "smoke_ai_king: idle fortify orders=%d (want FORTIFY)\n",
                idle->orders);
        return fail("Regular on crown colony with no adjacent foe should fortify");
      }
      if (idle->x != 8 || idle->y != 8) {
        return fail("idle fortify Regular should stay on crown colony");
      }
    }
    /*
     * Idle fortify stack: only one Regular fortifies; extras hunt
     * (fandom REF garrison; same stack rule as post-capture).
     */
    {
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->id != idle_id) {
          u->moves_left = 0;
        }
      }
      /* Keep the fortified garrison; spawn extra with moves + distant prey. */
      {
        ColonizeUnit* idle = units_get(&units, idle_id);
        if (!idle || !idle->active) {
          return fail("idle fortify extras needs fortified Regular");
        }
        idle->orders = UNITS_ORDER_FORTIFY;
        idle->moves_left = 0;
      }
      const int extra_id = units_spawn_allow_stack(&units, ty_regular, 8, 8);
      const int prey_id = units_spawn_allow_stack(&units, ty_soldier, 14, 8);
      if (extra_id < 0 || prey_id < 0) {
        return fail("idle fortify extras setup should spawn Regular + prey");
      }
      {
        ColonizeUnit* ex = units_get(&units, extra_id);
        ColonizeUnit* prey = units_get(&units, prey_id);
        if (!ex || !prey) {
          return fail("idle fortify extras unit lookup");
        }
        ex->nation_id = 1;
        ex->moves_left = 1;
        ex->orders = UNITS_ORDER_NONE;
        ex->goto_x = -1;
        ex->goto_y = -1;
        prey->nation_id = 0;
        prey->moves_left = 0;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* idle = units_get_const(&units, idle_id);
        const ColonizeUnit* ex = units_get_const(&units, extra_id);
        if (!idle || !idle->active ||
            (idle->orders != UNITS_ORDER_FORTIFY && idle->orders != UNITS_ORDER_FORTIFIED)) {
          return fail("idle fortify extras: garrison Regular must stay FORTIFY");
        }
        if (!ex || !ex->active) {
          return fail("idle fortify extras: extra Regular should remain active");
        }
        if (ex->orders == UNITS_ORDER_FORTIFY || ex->orders == UNITS_ORDER_FORTIFIED) {
          return fail("idle fortify extras: second Regular must not fortify");
        }
        if (ex->orders != UNITS_ORDER_AI_MOVE) {
          fprintf(stderr, "smoke_ai_king: idle extras orders=%d (want AI_MOVE hunt)\n",
                  ex->orders);
          return fail("idle fortify extras: second Regular should hunt");
        }
      }
      {
        int fortified = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (!u->active || u->nation_id != 1 || u->type_index != ty_regular) {
            continue;
          }
          if ((u->x == 8 && u->y == 8) &&
              (u->orders == UNITS_ORDER_FORTIFY || u->orders == UNITS_ORDER_FORTIFIED)) {
            fortified++;
          }
        }
        if (fortified != 1) {
          fprintf(stderr, "smoke_ai_king: idle extras fortified=%d (want 1)\n", fortified);
          return fail("idle fortify extras should keep exactly one fortified Regular");
        }
      }
    }
    crown_col->active = false;

    /*
     * Captured human capital garrison: idle Regular on Jamestown (5,5) after
     * crown capture → FORTIFY when stack empty; already-FORTIFIED stays.
     */
    {
      colonies.colonies[0].nation_id = 1; /* captured capital */
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      col1.head.backup_force[0] = 0;
      col1.head.backup_force[1] = 0;
      col1.head.backup_force[2] = 0;
      col1.head.backup_force[3] = 0;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          /* Keep human land clear of capital adjacency. */
          if (abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1) {
            u->x = 1;
            u->y = 14;
          }
        }
      }
      const int cap_garrison = units_spawn_allow_stack(&units, ty_regular, 5, 5);
      if (cap_garrison < 0) {
        return fail("capital garrison setup should spawn Regular on Jamestown");
      }
      {
        ColonizeUnit* g = units_get(&units, cap_garrison);
        if (!g) {
          return fail("capital garrison unit lookup");
        }
        g->nation_id = 1;
        g->moves_left = 2;
        g->orders = UNITS_ORDER_NONE;
        g->goto_x = -1;
        g->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* g = units_get_const(&units, cap_garrison);
        if (!g || !g->active) {
          return fail("capital garrison Regular should remain active");
        }
        if (g->orders != UNITS_ORDER_FORTIFY) {
          fprintf(stderr, "smoke_ai_king: capital garrison orders=%d (want FORTIFY)\n",
                  g->orders);
          return fail("idle Regular on captured human capital should fortify");
        }
        if (g->x != 5 || g->y != 5) {
          return fail("capital garrison Regular should stay on Jamestown");
        }
      }
      /* Already fortified with moves restored → stay, do not hunt away. */
      {
        ColonizeUnit* g = units_get(&units, cap_garrison);
        if (!g) {
          return fail("capital stay setup");
        }
        g->orders = UNITS_ORDER_FORTIFIED;
        g->moves_left = 2;
        g->goto_x = -1;
        g->goto_y = -1;
        /* Distant human prey that would otherwise attract a hunter. */
        const int bait = units_spawn_allow_stack(&units, ty_soldier, 12, 5);
        if (bait < 0) {
          return fail("capital stay bait spawn");
        }
        {
          ColonizeUnit* b = units_get(&units, bait);
          if (b) {
            b->nation_id = 0;
            b->moves_left = 0;
          }
        }
        ai_king_nation_turn(&ctx);
        g = units_get(&units, cap_garrison);
        if (!g || !g->active) {
          return fail("fortified capital garrison should remain active");
        }
        if (g->orders != UNITS_ORDER_FORTIFIED) {
          fprintf(stderr, "smoke_ai_king: capital stay orders=%d (want FORTIFIED)\n",
                  g->orders);
          return fail("already-FORTIFIED Regular on capital must stay garrisoned");
        }
        if (g->x != 5 || g->y != 5) {
          return fail("already-FORTIFIED capital Regular must not leave tile");
        }
      }
      colonies.colonies[0].nation_id = 0;
    }
  }

  /*
   * Artillery after capture (Euro pattern): crown Artillery on human colony →
   * colonies_capture then UNITS_ORDER_FORTIFY on that Artillery (Colonization.pdf
   * fortify defense / euro_unit_act Artillery fortify after siege). Idle
   * Artillery on crown colony with no adjacent foe also FORTIFY.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
          u->orders = UNITS_ORDER_NONE;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        if (abs(u->x - 5) <= 1 && abs(u->y - 5) <= 1) {
          u->x = 1;
          u->y = 14;
        }
      }
    }
    const int art_cap = units_spawn_allow_stack(&units, ty_artillery, 5, 5);
    if (art_cap < 0) {
      return fail("Artillery after-capture setup should spawn Artillery on colony");
    }
    {
      ColonizeUnit* art = units_get(&units, art_cap);
      if (!art) {
        return fail("Artillery after-capture unit lookup");
      }
      art->nation_id = 1;
      art->moves_left = 0;
      art->orders = UNITS_ORDER_NONE;
      art->goto_x = -1;
      art->goto_y = -1;
    }
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    if (colonies.colonies[0].nation_id != 1) {
      return fail("Artillery on human colony should colonies_capture");
    }
    {
      const ColonizeUnit* art = units_get_const(&units, art_cap);
      if (!art || !art->active) {
        return fail("Artillery capturer should remain active");
      }
      if (art->orders != UNITS_ORDER_FORTIFY) {
        fprintf(stderr, "smoke_ai_king: Artillery after-capture orders=%d (want FORTIFY)\n",
                art->orders);
        return fail("Artillery on newly captured colony should FORTIFY (Euro pattern)");
      }
      if (art->x != 5 || art->y != 5) {
        return fail("Artillery after-capture should stay on colony tile");
      }
    }
    /* Idle Artillery on crown colony (moves>0, no adjacent foe) → FORTIFY. */
    {
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->id != art_cap) {
          u->moves_left = 0;
        }
      }
      ColonizeUnit* art = units_get(&units, art_cap);
      if (!art) {
        return fail("Artillery idle fortify setup");
      }
      art->orders = UNITS_ORDER_NONE;
      art->moves_left = 2;
      art->goto_x = -1;
      art->goto_y = -1;
      colonies.colonies[0].nation_id = 1;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      ai_king_nation_turn(&ctx);
      art = units_get(&units, art_cap);
      if (!art || !art->active) {
        return fail("idle Artillery on crown colony should remain active");
      }
      if (art->orders != UNITS_ORDER_FORTIFY) {
        fprintf(stderr, "smoke_ai_king: idle Artillery orders=%d (want FORTIFY)\n",
                art->orders);
        return fail("idle Artillery on crown colony should FORTIFY (Euro pattern)");
      }
      if (art->x != 5 || art->y != 5) {
        return fail("idle Artillery should stay on crown colony");
      }
    }
    colonies.colonies[0].nation_id = 0;
  }

  /*
   * Thin Artillery siege: fortified target + Artillery type in pool + force[3]>0
   * (no MoW/Regular/Dragoon pools) → wave prefers Artillery spawn.
   * Deep multi-step siege scoring PARKED.
   */
  {
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].has_building[0] = true;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.expeditionary_force[3] = 2; /* Artillery only */
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
      }
    }
    const int art_before = col1.head.expeditionary_force[3];
    int art_units_before = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && u->type_index == ty_artillery) {
        art_units_before++;
      }
    }
    ai_king_nation_turn(&ctx);
    int art_units_after = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && u->type_index == ty_artillery) {
        art_units_after++;
      }
    }
    if (art_units_after <= art_units_before) {
      return fail("fortified target should prefer Artillery wave spawn");
    }
    if (col1.head.expeditionary_force[3] >= art_before) {
      return fail("Artillery siege spawn should drain force[3]");
    }
    /* Artillery hunt prefer fortified: closer unfortified unit vs fortified colony. */
    colonies.colonies[0].nation_id = 0;
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].has_building[0] = true;
    if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
      return fail("Artillery hunt setup requires fortified human colony");
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        /* Move prior crown stacks off the fortified colony so hunt is clean. */
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    const int decoy_id = units_spawn_allow_stack(&units, ty_soldier, 7, 5);
    const int art_id = units_spawn_allow_stack(&units, ty_artillery, 9, 5);
    if (decoy_id < 0 || art_id < 0) {
      return fail("Artillery hunt setup should spawn decoy + Artillery");
    }
    {
      ColonizeUnit* decoy = units_get(&units, decoy_id);
      ColonizeUnit* art = units_get(&units, art_id);
      if (!decoy || !art) {
        return fail("Artillery hunt unit lookup");
      }
      decoy->nation_id = 0;
      decoy->moves_left = 0;
      art->nation_id = 1;
      art->moves_left = 1;
      art->orders = UNITS_ORDER_NONE;
      art->goto_x = -1;
      art->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* art = units_get_const(&units, art_id);
      if (!art || !art->active) {
        return fail("Artillery hunter should remain active");
      }
      if (art->orders != UNITS_ORDER_AI_MOVE || art->goto_x != 5 || art->goto_y != 5) {
        fprintf(stderr, "smoke_ai_king: Artillery goto=(%d,%d) orders=%d (want fortified 5,5)\n",
                art->goto_x, art->goto_y, art->orders);
        return fail("Artillery should prefer fortified human colony over nearer unit");
      }
    }
    /*
     * Artillery siege tighten: adjacent unfortified human colony must not
     * override a farther fortified hunt target.
     */
    {
      ColonizeColony* soft = &colonies.colonies[3];
      soft->id = 3;
      soft->active = true;
      soft->nation_id = 0;
      soft->x = 10;
      soft->y = 5; /* adjacent east of Artillery at (9,5) */
      soft->population = 2;
      soft->colonist_count = 2;
      soft->has_building[0] = false;
      if (colonies.colony_count < 4) {
        colonies.colony_count = 4;
      }
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].has_building[0] = true;
      if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
        return fail("Artillery adj-fort tighten requires fortified Jamestown");
      }
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1) {
          u->moves_left = 0;
          if ((u->x == 5 && u->y == 5) || (u->x == 10 && u->y == 5)) {
            u->x = 1;
            u->y = 1;
          }
        }
      }
      {
        ColonizeUnit* art = units_get(&units, art_id);
        if (!art || !art->active) {
          return fail("Artillery adj-fort setup needs live Artillery");
        }
        art->x = 9;
        art->y = 5;
        art->moves_left = 1;
        art->orders = UNITS_ORDER_NONE;
        art->goto_x = -1;
        art->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* art = units_get_const(&units, art_id);
        if (!art || !art->active) {
          return fail("Artillery adj-fort hunter should remain active");
        }
        if (art->orders != UNITS_ORDER_AI_MOVE || art->goto_x != 5 || art->goto_y != 5) {
          fprintf(stderr,
                  "smoke_ai_king: Artillery adj-fort goto=(%d,%d) orders=%d "
                  "(want fortified 5,5 not soft 10,5)\n",
                  art->goto_x, art->goto_y, art->orders);
          return fail("Artillery must not let adjacent unfortified override fortified hunt");
        }
      }
      soft->active = false;
    }
    colonies.colonies[0].has_building[0] = false; /* clear fort for later */
  }

  /*
   * Dragoon open-land bias: when Artillery type exists, prefer farther open
   * human land unit over nearer fortified colony (Artillery owns siege).
   * Deep role-split scoring PARKED.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].has_building[0] = true;
    if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
      return fail("Dragoon open-land setup requires fortified human colony");
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    /* Park crown; sweep human land units off the probe corridor. */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        /* Keep map clear so Dragoon does not bump into leftovers. */
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    /* Nearer fortified colony (5,5); farther open Soldier (14,5); Dragoon at (8,5). */
    const int open_id = units_spawn_allow_stack(&units, ty_soldier, 14, 5);
    const int drg_id = units_spawn_allow_stack(&units, ty_dragoon, 8, 5);
    if (open_id < 0 || drg_id < 0) {
      return fail("Dragoon open-land setup should spawn open Soldier + Dragoon");
    }
    {
      ColonizeUnit* openu = units_get(&units, open_id);
      ColonizeUnit* drg = units_get(&units, drg_id);
      if (!openu || !drg) {
        return fail("Dragoon open-land unit lookup");
      }
      openu->nation_id = 0;
      openu->moves_left = 0;
      drg->nation_id = 1;
      drg->moves_left = 1;
      drg->orders = UNITS_ORDER_NONE;
      drg->goto_x = -1;
      drg->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* drg = units_get_const(&units, drg_id);
      if (!drg || !drg->active) {
        return fail("Dragoon hunter should remain active");
      }
      if (drg->orders != UNITS_ORDER_AI_MOVE || drg->goto_x != 14 || drg->goto_y != 5) {
        fprintf(stderr,
                "smoke_ai_king: Dragoon goto=(%d,%d) orders=%d (want open unit 14,5 not fort 5,5)\n",
                drg->goto_x, drg->goto_y, drg->orders);
        return fail("Dragoon should prefer open land unit over nearer fortified colony");
      }
    }
    colonies.colonies[0].has_building[0] = false;
  }

  /*
   * Cont. Cav open-land bias (same Dragoon role when Artillery exists):
   * prefer farther open human land unit over nearer fortified colony.
   * Cont. Army stays nearest — Cont. Cav only. Deep role-split PARKED.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.building_type_count = 1;
    snprintf(colonies.building_types[0].name, sizeof(colonies.building_types[0].name),
             "Stockade");
    colonies.colonies[0].has_building[0] = true;
    if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
      return fail("Cont. Cav open-land setup requires fortified human colony");
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if (u->x == 5 && u->y == 5) {
          u->x = 1;
          u->y = 1;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    const int open_id = units_spawn_allow_stack(&units, ty_soldier, 14, 5);
    const int cav_id = units_spawn_allow_stack(&units, ty_cont_cav, 8, 5);
    if (open_id < 0 || cav_id < 0) {
      return fail("Cont. Cav open-land setup should spawn open Soldier + Cont. Cav");
    }
    {
      ColonizeUnit* openu = units_get(&units, open_id);
      ColonizeUnit* cav = units_get(&units, cav_id);
      if (!openu || !cav) {
        return fail("Cont. Cav open-land unit lookup");
      }
      openu->nation_id = 0;
      openu->moves_left = 0;
      cav->nation_id = 1; /* crown Cont. Cav hunter */
      cav->moves_left = 1;
      cav->orders = UNITS_ORDER_NONE;
      cav->goto_x = -1;
      cav->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* cav = units_get_const(&units, cav_id);
      if (!cav || !cav->active) {
        return fail("Cont. Cav hunter should remain active");
      }
      if (cav->orders != UNITS_ORDER_AI_MOVE || cav->goto_x != 14 || cav->goto_y != 5) {
        fprintf(stderr,
                "smoke_ai_king: Cont. Cav goto=(%d,%d) orders=%d (want open 14,5 not fort 5,5)\n",
                cav->goto_x, cav->goto_y, cav->orders);
        return fail("Cont. Cav should prefer open land unit over nearer fortified colony");
      }
    }
    /*
     * Cont. Army stays nearest (no open-land bias): same geometry → fortified
     * colony (5,5), not open Soldier (14,5). Cont. Cav only above.
     * Restore human ownership — prior Cont. Cav beat may have captured via a
     * leftover crown stack on the tile (capture runs at 0 moves).
     */
    {
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].has_building[0] = true;
      if (!colonies_has_fortification(&colonies, &colonies.colonies[0])) {
        return fail("Cont. Army open-land negative requires fortified human colony");
      }
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      const int open_id2 = units_spawn_allow_stack(&units, ty_soldier, 14, 5);
      const int army_id = units_spawn_allow_stack(&units, ty_cont_army, 8, 5);
      if (open_id2 < 0 || army_id < 0) {
        return fail("Cont. Army open-land negative setup should spawn open Soldier + Cont. Army");
      }
      {
        ColonizeUnit* openu = units_get(&units, open_id2);
        ColonizeUnit* army = units_get(&units, army_id);
        if (!openu || !army) {
          return fail("Cont. Army open-land negative unit lookup");
        }
        openu->nation_id = 0;
        openu->moves_left = 0;
        army->nation_id = 1;
        army->moves_left = 1;
        army->orders = UNITS_ORDER_NONE;
        army->goto_x = -1;
        army->goto_y = -1;
      }
      ai_king_nation_turn(&ctx);
      {
        const ColonizeUnit* army = units_get_const(&units, army_id);
        if (!army || !army->active) {
          return fail("Cont. Army hunter should remain active");
        }
        if (army->orders != UNITS_ORDER_AI_MOVE || army->goto_x != 5 || army->goto_y != 5) {
          fprintf(stderr,
                  "smoke_ai_king: Cont. Army goto=(%d,%d) orders=%d (want fort 5,5 not open 14,5)\n",
                  army->goto_x, army->goto_y, army->orders);
          return fail("Cont. Army should stay nearest (fort colony), not open-land bias");
        }
      }
    }
    colonies.colonies[0].has_building[0] = false;
  }

  /*
   * Wartime MoW with cargo → AI_SAIL toward water adjacent to human colony.
   * Ship at (2,5); coast water west of colony is (4,5); step east toward coast.
   * Decoy weak port draws 06a6 irregulars so the coastal colony stays human.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].population = 8; /* stronger — not 06a6 pick */
    ColonizeColony* decoy_port = &colonies.colonies[2];
    decoy_port->id = 2;
    decoy_port->active = true;
    decoy_port->nation_id = 0;
    decoy_port->x = 14;
    decoy_port->y = 14;
    decoy_port->population = 1;
    decoy_port->colonist_count = 1;
    if (colonies.colony_count < 3) {
      colonies.colony_count = 3;
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    col1.head.backup_force[0] = 0;
    col1.head.backup_force[1] = 0;
    col1.head.backup_force[2] = 0;
    col1.head.backup_force[3] = 0;
    /* Ocean corridor (2,5)-(4,5) for MoW sail. */
    map.terrain[5 * 16 + 2] = 25;
    map.terrain[5 * 16 + 3] = 25;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
        /* Off human colonies — 0-move capture would clear MoW coast targets. */
        if ((u->x == 5 && u->y == 5) || (u->x == 14 && u->y == 14)) {
          u->x = 1;
          u->y = 1;
        }
      }
    }
    const int mow_id = units_spawn_allow_stack(&units, ty_mow, 2, 5);
    const int pax_id = units_spawn_allow_stack(&units, ty_regular, 2, 5);
    if (mow_id < 0 || pax_id < 0) {
      return fail("MoW sail setup should spawn Man-O-War + Regular");
    }
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      ColonizeUnit* pax = units_get(&units, pax_id);
      if (!mow || !pax) {
        return fail("MoW sail unit lookup");
      }
      mow->nation_id = 1;
      mow->moves_left = 4;
      mow->orders = UNITS_ORDER_NONE;
      mow->goto_x = -1;
      mow->goto_y = -1;
      pax->nation_id = 1;
      if (!units_board_stacked(&units, pax_id, mow_id)) {
        return fail("MoW sail should board Regular as cargo");
      }
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* mow = units_get_const(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW with cargo should remain active");
      }
      if (mow->orders != UNITS_ORDER_AI_SAIL || mow->goto_x != 4 || mow->goto_y != 5) {
        fprintf(stderr, "smoke_ai_king: MoW goto=(%d,%d) orders=%d (want AI_SAIL→4,5)\n",
                mow->goto_x, mow->goto_y, mow->orders);
        return fail("MoW with cargo should AI_SAIL toward water adjacent to human colony");
      }
      if (mow->x < 3) {
        fprintf(stderr, "smoke_ai_king: MoW pos=(%d,%d) (want step toward coast)\n", mow->x,
                mow->y);
        return fail("MoW with cargo should step toward human coast water");
      }
    }
    /* Place MoW on coast water adjacent to human colony → unload onto colony
     * tile (prefer seize/attack path score 100 over adjacent coastal land). */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active || mow->cargo_count <= 0) {
        return fail("MoW unload setup needs MoW still carrying cargo");
      }
      mow->x = 4;
      mow->y = 5;
      mow->moves_left = 2;
      mow->orders = UNITS_ORDER_NONE;
      /* Ensure colony tile is free of blocking foreign units for unload. */
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->x == 5 && u->y == 5 && u->id != mow_id &&
            u->nation_id != 1) {
          u->x = 1;
          u->y = 1;
        }
      }
      /* Soft coastal land (4,4)/(4,6) must not win over colony tile (5,5). */
      map.terrain[4 * 16 + 4] = 1;
      map.terrain[6 * 16 + 4] = 1;
      const int cargo_before = mow->cargo_count;
      const int land_before = count_nation_land(&units, 1);
      colonies.colonies[0].nation_id = 0;
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW should remain after coastal unload");
      }
      if (mow->cargo_count != cargo_before - 1) {
        fprintf(stderr, "smoke_ai_king: MoW cargo after unload %d (want %d)\n",
                mow->cargo_count, cargo_before - 1);
        return fail("MoW adjacent to human colony coast should unload one Regular");
      }
      if (count_nation_land(&units, 1) < land_before + 1) {
        return fail("MoW coastal unload should place one crown land unit ashore");
      }
      /* Unload+seize: passenger prefers the human colony tile itself. */
      {
        int on_colony = 0;
        for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
          const ColonizeUnit* u = &units.units[i];
          if (!u->active || u->nation_id != 1 || units_is_sea(&units, u->id)) {
            continue;
          }
          if (u->type_index == ty_regular && u->x == 5 && u->y == 5 &&
              u->aboard_ship_id < 0) {
            on_colony = 1;
            break;
          }
        }
        if (!on_colony) {
          return fail("MoW unload should prefer human colony tile (seize path)");
        }
      }
      /* Same-beat capture is allowed but not required (unit index order). */
    }
    /*
     * Dragoon coastal unload when cargo allows (no Regular in hold):
     * prefer Regular otherwise; here only Dragoon → unload Dragoon.
     * Prefer colony tile: make (5,5) the only adjacent land so soft coast
     * cannot win. Same-beat hunt may move the Dragoon after seize — assert
     * cargo drop + not-aboard + human colony captured (owner → crown).
     * MoW×6 chrome remains PARKED.
     */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("Dragoon unload setup needs live Man-O-War");
      }
      /* Clear remaining cargo so we can board a Dragoon alone. */
      while (mow->cargo_count > 0) {
        const int cid = mow->cargo_ids[0];
        ColonizeUnit* p = units_get(&units, cid);
        if (p) {
          p->aboard_ship_id = -1;
          p->active = false;
        }
        mow->cargo_count = 0;
      }
      const int drg_pax = units_spawn_allow_stack(&units, ty_dragoon, 4, 5);
      if (drg_pax < 0) {
        return fail("Dragoon unload setup should spawn Dragoon");
      }
      {
        ColonizeUnit* drg = units_get(&units, drg_pax);
        if (!drg) {
          return fail("Dragoon unload unit lookup");
        }
        drg->nation_id = 1;
        if (!units_board_stacked(&units, drg_pax, mow_id)) {
          return fail("Dragoon unload should board Dragoon as cargo");
        }
      }
      mow->x = 4;
      mow->y = 5;
      mow->moves_left = 2;
      mow->orders = UNITS_ORDER_NONE;
      /* Only colony tile (5,5) enterable from ship — soft coast → water. */
      map.terrain[4 * 16 + 4] = 25;
      map.terrain[6 * 16 + 4] = 25;
      map.terrain[4 * 16 + 5] = 25; /* (5,4) */
      map.terrain[6 * 16 + 5] = 25; /* (5,6) */
      map.terrain[4 * 16 + 3] = 25;
      map.terrain[5 * 16 + 3] = 25;
      map.terrain[6 * 16 + 3] = 25;
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (!u->active || u->id == mow_id || u->id == drg_pax) {
          continue;
        }
        if (u->nation_id == 1) {
          u->moves_left = 0;
          if (u->x == 5 && u->y == 5) {
            u->x = 1;
            u->y = 1;
            u->orders = UNITS_ORDER_NONE;
          }
        } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
          u->x = 1;
          u->y = 14;
          u->moves_left = 0;
        }
      }
      const int cargo_before = mow->cargo_count;
      colonies.colonies[0].nation_id = 0;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("MoW should remain after Dragoon coastal unload");
      }
      if (mow->cargo_count != cargo_before - 1) {
        fprintf(stderr, "smoke_ai_king: MoW cargo after Dragoon unload %d (want %d)\n",
                mow->cargo_count, cargo_before - 1);
        return fail("MoW with Dragoon-only cargo should unload one Dragoon");
      }
      {
        const ColonizeUnit* drg = units_get_const(&units, drg_pax);
        if (!drg || !drg->active || drg->aboard_ship_id >= 0) {
          return fail("Dragoon should be ashore after MoW unload");
        }
      }
      if (colonies.colonies[0].nation_id != 1) {
        return fail("Dragoon unload onto colony tile should seize (owner → crown)");
      }
      /* Restore corridor land for later empty-MoW patrol. */
      map.terrain[4 * 16 + 4] = 1;
      map.terrain[6 * 16 + 4] = 1;
      map.terrain[4 * 16 + 5] = 1;
      map.terrain[6 * 16 + 5] = 1;
    }
    /*
     * Idle empty MoW coastal patrol (fandom REF man-o-war → ports):
     * cargo_count==0 → AI_SAIL toward water adjacent to human colony; step
     * toward coast. Redirects existing ship only — no invent spawn.
     * MoW×6 hold chrome remains PARKED (fandom “man-o-war with 6 units”;
     * structural hold-size-3 Regular+Dragoon unload + coastal unload-one +
     * third landing @diff≥2 cover pressure without ×6 embark chrome).
     */
    {
      ColonizeUnit* mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("empty MoW patrol setup needs live Man-O-War");
      }
      /* Drain any remaining cargo so ship is idle-empty. */
      while (mow->cargo_count > 0) {
        const int cid = mow->cargo_ids[0];
        ColonizeUnit* p = units_get(&units, cid);
        if (p) {
          p->aboard_ship_id = -1;
          p->active = false;
        }
        mow->cargo_count = 0;
        break;
      }
      mow->x = 2;
      mow->y = 5;
      mow->moves_left = 4;
      mow->orders = UNITS_ORDER_NONE;
      mow->goto_x = -1;
      mow->goto_y = -1;
      colonies.colonies[0].nation_id = 0;
      colonies.colonies[0].population = 8;
      decoy_port->active = true;
      decoy_port->nation_id = 0;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
        ColonizeUnit* u = &units.units[i];
        if (u->active && u->nation_id == 1 && u->id != mow_id) {
          u->moves_left = 0;
          if ((u->x == 5 && u->y == 5) || (u->x == 14 && u->y == 14)) {
            u->x = 1;
            u->y = 1;
          }
        }
      }
      ai_king_nation_turn(&ctx);
      mow = units_get(&units, mow_id);
      if (!mow || !mow->active) {
        return fail("idle empty MoW should remain active");
      }
      if (mow->orders != UNITS_ORDER_AI_SAIL || mow->goto_x != 4 || mow->goto_y != 5) {
        fprintf(stderr,
                "smoke_ai_king: empty MoW goto=(%d,%d) orders=%d (want AI_SAIL→4,5)\n",
                mow->goto_x, mow->goto_y, mow->orders);
        return fail("idle empty MoW should AI_SAIL toward human coast water");
      }
      if (mow->x < 3) {
        fprintf(stderr, "smoke_ai_king: empty MoW pos=(%d,%d) (want step toward coast)\n",
                mow->x, mow->y);
        return fail("idle empty MoW should step toward human coast water");
      }
    }
    decoy_port->active = false;
    colonies.colonies[0].population = 4;
  }

  /*
   * Thin 2244 merc auto-accept + hire-dialog status (real modal PARKED):
   * clear cannot-afford gate; gold>=300 + SoL>50 + !unknown46[3] → spend 300,
   * spawn human Soldier/Dragoon, set merc-hired flag + Continental-cause status.
   * Second wartime turn must not hire again.
   */
  colonies.colonies[0].nation_id = 0;
  col1.head.unknown46[3] = 0; /* clear prior cannot-afford gate for hire path */
  col1.nation[0].gold = 450;
  europe.gold = 450;
  status[0] = '\0';
  const int human_before = count_nation(&units, 0);
  int soldierish_before = 0;
  int dragoon_before = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active || u->nation_id != 0) {
      continue;
    }
    if (u->type_index == ty_soldier || u->type_index == ty_cont_army) {
      soldierish_before++;
    }
    if (u->type_index == ty_dragoon) {
      dragoon_before++;
    }
  }
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
  /*
   * Primary hire type is Soldier (Dragoon only as alt when Soldier missing).
   * Same-beat 1eca may Cont.-promote the new Soldier when SoL>50 — count either.
   */
  {
    int soldierish_after = 0;
    int dragoon_after = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 0) {
        continue;
      }
      if (u->type_index == ty_soldier || u->type_index == ty_cont_army) {
        soldierish_after++;
      }
      if (u->type_index == ty_dragoon) {
        dragoon_after++;
      }
    }
    if (soldierish_after <= soldierish_before) {
      return fail("2244 merc hire should spawn Soldier type (or same-beat Cont. promote)");
    }
    if (dragoon_after > dragoon_before) {
      return fail("2244 merc hire should prefer Soldier over Dragoon when Soldier exists");
    }
  }
  if (!strstr(status, "Mercenaries join") || !strstr(status, "Continental cause")) {
    fprintf(stderr, "smoke_ai_king: status after merc: '%s'\n", status);
    return fail("2244 should set hire-dialog merc status line");
  }
  if (!strstr(status, "300") && !strstr(status, "gold")) {
    fprintf(stderr, "smoke_ai_king: merc gold status: '%s'\n", status);
    return fail("2244 merc status should mention gold spent");
  }
  const int human_after = count_nation(&units, 0);
  const uint32_t gold_after = col1.nation[0].gold;
  status[0] = '\0'; /* clear so a re-hire spam would be visible */
  ai_king_nation_turn(&ctx);
  if (col1.nation[0].gold != gold_after) {
    return fail("merc flag should block second 2244 spend");
  }
  if (count_nation(&units, 0) != human_after) {
    return fail("merc flag should block second human merc spawn");
  }
  if (col1.head.unknown46[3] == 0) {
    return fail("merc-hired unknown46[3] should remain set");
  }
  /* Once-per-war: unknown46[3] must prevent hire-status rewrite spam. */
  if (strstr(status, "Mercenaries join")) {
    return fail("merc flag should block second 2244 hire-status spam");
  }

  /*
   * Thin 1eca widen (deep colony-SoL table PARKED):
   * WoI + SoL>50 → Soldier → Continental Army; Dragoon → Continental Cavalry;
   * Regular → Veteran Soldier (Continental Army fallback if no vet type).
   */
  colonies.colonies[0].nation_id = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  const int sid = units_spawn_allow_stack(&units, ty_soldier, 6, 5);
  const int did = units_spawn_allow_stack(&units, ty_dragoon, 7, 5);
  const int rid = units_spawn_allow_stack(&units, ty_regular, 8, 5);
  if (sid < 0 || did < 0 || rid < 0) {
    return fail("1eca setup should spawn human Soldier + Dragoon + Regular");
  }
  {
    ColonizeUnit* su = units_get(&units, sid);
    ColonizeUnit* du = units_get(&units, did);
    ColonizeUnit* ru = units_get(&units, rid);
    if (!su || !du || !ru) {
      return fail("1eca setup unit lookup");
    }
    su->nation_id = 0;
    du->nation_id = 0;
    ru->nation_id = 0;
  }
  ai_king_nation_turn(&ctx);
  {
    const ColonizeUnit* su = units_get_const(&units, sid);
    const ColonizeUnit* du = units_get_const(&units, did);
    const ColonizeUnit* ru = units_get_const(&units, rid);
    if (!su || !su->active || su->type_index != ty_cont_army) {
      fprintf(stderr, "smoke_ai_king: Soldier type after 1eca: %d (want %d)\n",
              su ? su->type_index : -1, ty_cont_army);
      return fail("1eca should promote Soldier → Continental Army");
    }
    if (!du || !du->active || du->type_index != ty_cont_cav) {
      fprintf(stderr, "smoke_ai_king: Dragoon type after 1eca: %d (want %d)\n",
              du ? du->type_index : -1, ty_cont_cav);
      return fail("1eca should promote Dragoon → Continental Cavalry");
    }
    if (!ru || !ru->active || ru->type_index != ty_vet_soldier) {
      fprintf(stderr, "smoke_ai_king: Regular type after 1eca: %d (want %d)\n",
              ru ? ru->type_index : -1, ty_vet_soldier);
      return fail("1eca SoL>50 should promote Regular → Veteran Soldier");
    }
  }

  /*
   * SoL 40..50 band: Soldier → Veteran Soldier only (no Continental).
   * Regular stays Regular. WoI already declared.
   */
  col1.colony[0].rebel_dividend = 45;
  col1.colony[0].rebel_divisor = 100;
  {
    const int sol45w = ai_king_sol_percent(&ctx, 0);
    if (sol45w != 45) {
      fprintf(stderr, "smoke_ai_king: unexpected SoL %d (want 45) for mid-band 1eca\n",
              sol45w);
      return fail("1eca mid-band SoL setup");
    }
  }
  colonies.colonies[0].nation_id = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  const int sid2 = units_spawn_allow_stack(&units, ty_soldier, 9, 5);
  const int rid2 = units_spawn_allow_stack(&units, ty_regular, 10, 5);
  if (sid2 < 0 || rid2 < 0) {
    return fail("1eca mid-band setup should spawn Soldier + Regular");
  }
  {
    ColonizeUnit* su = units_get(&units, sid2);
    ColonizeUnit* ru = units_get(&units, rid2);
    if (!su || !ru) {
      return fail("1eca mid-band unit lookup");
    }
    su->nation_id = 0;
    ru->nation_id = 0;
  }
  ai_king_nation_turn(&ctx);
  {
    const ColonizeUnit* su = units_get_const(&units, sid2);
    const ColonizeUnit* ru = units_get_const(&units, rid2);
    if (!su || !su->active || su->type_index != ty_vet_soldier) {
      fprintf(stderr, "smoke_ai_king: Soldier type SoL45: %d (want %d)\n",
              su ? su->type_index : -1, ty_vet_soldier);
      return fail("1eca SoL 40-50 should promote Soldier → Veteran Soldier");
    }
    if (!ru || !ru->active || ru->type_index != ty_regular) {
      fprintf(stderr, "smoke_ai_king: Regular type SoL45: %d (want %d)\n",
              ru ? ru->type_index : -1, ty_regular);
      return fail("1eca SoL 40-50 should leave Regular unpromoted");
    }
  }

  /*
   * 1eca colony-SoL bias (FUN_43f7_1eca / catalog colony SoL>50%):
   * Nation aggregate mid/low, but unit on a high-SoL Col1 colony tile promotes
   * to Continental; unit on low-SoL colony tile does not. King promote path —
   * not FF Washington mass-promote. No treasury bumps.
   */
  {
    ColonizeCol1Colony* grown = calloc(2, sizeof(ColonizeCol1Colony));
    if (!grown) {
      return fail("alloc 1eca colony-SoL colonies");
    }
    grown[0] = col1.colony[0];
    free(col1.colony);
    col1.colony = grown;
    col1.head.colony_count = 2;
    /* Low SoL off the weakest-port tile so crown combat cannot delete the probe. */
    col1.colony[0].x = 3;
    col1.colony[0].y = 3;
    col1.colony[0].nation_id = 0;
    col1.colony[0].population = 4;
    col1.colony[0].rebel_dividend = 30;
    col1.colony[0].rebel_divisor = 100;
    col1.colony[1].x = 11;
    col1.colony[1].y = 5;
    col1.colony[1].nation_id = 0;
    col1.colony[1].population = 1;
    col1.colony[1].rebel_dividend = 70;
    col1.colony[1].rebel_divisor = 100;
  }
  {
    const int sol_nat = ai_king_sol_percent(&ctx, 0);
    /* (30*4 + 70*1)/5 = 38 — nation alone would not Continental-promote. */
    if (sol_nat != 38) {
      fprintf(stderr, "smoke_ai_king: unexpected nation SoL %d (want 38) for colony-SoL\n",
              sol_nat);
      return fail("1eca colony-SoL nation aggregate setup");
    }
  }
  colonies.colonies[0].nation_id = 0;
  memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
  /* Park crown movers so war_act combat cannot delete the SoL probe units. */
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    ColonizeUnit* u = &units.units[i];
    if (u->active && u->nation_id == 1) {
      u->moves_left = 0;
    }
  }
  const int sid_hi = units_spawn_allow_stack(&units, ty_soldier, 11, 5);
  const int sid_lo = units_spawn_allow_stack(&units, ty_soldier, 3, 3);
  if (sid_hi < 0 || sid_lo < 0) {
    return fail("1eca colony-SoL setup should spawn Soldiers on both colonies");
  }
  {
    ColonizeUnit* hi = units_get(&units, sid_hi);
    ColonizeUnit* lo = units_get(&units, sid_lo);
    if (!hi || !lo) {
      return fail("1eca colony-SoL unit lookup");
    }
    hi->nation_id = 0;
    lo->nation_id = 0;
  }
  ai_king_nation_turn(&ctx);
  {
    const ColonizeUnit* hi = units_get_const(&units, sid_hi);
    const ColonizeUnit* lo = units_get_const(&units, sid_lo);
    if (!hi || !hi->active || hi->type_index != ty_cont_army) {
      fprintf(stderr, "smoke_ai_king: high-SoL colony Soldier type: %d (want %d)\n",
              hi ? hi->type_index : -1, ty_cont_army);
      return fail("1eca colony-SoL>50 at tile should promote Soldier → Continental Army");
    }
    if (!lo || !lo->active || lo->type_index != ty_soldier) {
      fprintf(stderr, "smoke_ai_king: low-SoL colony Soldier type: %d (want %d)\n",
              lo ? lo->type_index : -1, ty_soldier);
      return fail("1eca low colony-SoL should leave Soldier unpromoted");
    }
  }

  /*
   * Cont. Army / Cont. Cav capital rally (after 1eca): idle Cont. off colony →
   * AI_MOVE toward founding capital (lowest colony id). Hunter name check
   * includes Continental / Cont. Army / Cont. Cav (fandom Independence).
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
      }
    }
    const int ca_id = units_spawn_allow_stack(&units, ty_cont_army, 10, 5);
    const int cav_id = units_spawn_allow_stack(&units, ty_cont_cav, 12, 5);
    if (ca_id < 0 || cav_id < 0) {
      return fail("Cont. capital-rally setup spawn");
    }
    {
      ColonizeUnit* ca = units_get(&units, ca_id);
      ColonizeUnit* cav = units_get(&units, cav_id);
      if (!ca || !cav) {
        return fail("Cont. capital-rally unit lookup");
      }
      ca->nation_id = 0;
      ca->moves_left = 2;
      ca->orders = UNITS_ORDER_NONE;
      ca->goto_x = -1;
      ca->goto_y = -1;
      cav->nation_id = 0;
      cav->moves_left = 2;
      cav->orders = UNITS_ORDER_NONE;
      cav->goto_x = -1;
      cav->goto_y = -1;
    }
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* ca = units_get_const(&units, ca_id);
      const ColonizeUnit* cav = units_get_const(&units, cav_id);
      if (!ca || !ca->active) {
        return fail("Cont. Army should remain active");
      }
      if (ca->orders != UNITS_ORDER_AI_MOVE || ca->goto_x != 5 || ca->goto_y != 5) {
        fprintf(stderr, "smoke_ai_king: Cont. Army goto=(%d,%d) orders=%d (want capital 5,5)\n",
                ca->goto_x, ca->goto_y, ca->orders);
        return fail("Cont. Army should capital-rally toward founding capital");
      }
      if (!cav || !cav->active) {
        return fail("Cont. Cav should remain active");
      }
      if (cav->orders != UNITS_ORDER_AI_MOVE || cav->goto_x != 5 || cav->goto_y != 5) {
        fprintf(stderr, "smoke_ai_king: Cont. Cav goto=(%d,%d) orders=%d (want capital 5,5)\n",
                cav->goto_x, cav->goto_y, cav->orders);
        return fail("Cont. Cav should capital-rally toward founding capital");
      }
    }
  }

  /*
   * REF capital MD hunt bias (fandom REF main-port pressure):
   * founding capital id0 at (5,5); nearer distant colony at (11,5); Regular at
   * (9,5) → MD capital=4, MD distant=2; slack=2 → prefer capital over distant.
   * Clear human land units so colony bias is observable. MoW×6 / extra boycott
   * cargos remain PARKED.
   */
  {
    colonies.colonies[0].nation_id = 0;
    colonies.colonies[0].x = 5;
    colonies.colonies[0].y = 5;
    colonies.colonies[0].has_building[0] = false;
    ColonizeColony* distant = &colonies.colonies[2];
    distant->id = 2;
    distant->active = true;
    distant->nation_id = 0;
    distant->x = 11;
    distant->y = 5;
    distant->population = 2;
    distant->colonist_count = 2;
    distant->has_building[0] = false;
    snprintf(distant->name, sizeof(distant->name), "Outpost");
    if (colonies.colony_count < 3) {
      colonies.colony_count = 3;
    }
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active) {
        continue;
      }
      if (u->nation_id == 1) {
        u->moves_left = 0;
        if ((u->x == 5 && u->y == 5) || (u->x == 11 && u->y == 5) ||
            (u->x == 9 && u->y == 5)) {
          u->x = 1;
          u->y = 1;
        }
      } else if (u->nation_id == 0 && !units_is_sea(&units, u->id)) {
        /* Sweep human land so hunt compares colonies only. */
        u->x = 1;
        u->y = 14;
        u->moves_left = 0;
      }
    }
    const int cap_hunter = units_spawn_allow_stack(&units, ty_regular, 9, 5);
    if (cap_hunter < 0) {
      return fail("capital MD bias setup should spawn crown Regular");
    }
    {
      ColonizeUnit* h = units_get(&units, cap_hunter);
      if (!h) {
        return fail("capital MD bias unit lookup");
      }
      h->nation_id = 1;
      h->moves_left = 1;
      h->orders = UNITS_ORDER_NONE;
      h->goto_x = -1;
      h->goto_y = -1;
    }
    ai_king_nation_turn(&ctx);
    {
      const ColonizeUnit* h = units_get_const(&units, cap_hunter);
      if (!h || !h->active) {
        return fail("capital MD bias Regular should remain active");
      }
      if (h->orders != UNITS_ORDER_AI_MOVE || h->goto_x != 5 || h->goto_y != 5) {
        fprintf(stderr,
                "smoke_ai_king: capital MD bias goto=(%d,%d) orders=%d "
                "(want capital 5,5 not distant 11,5)\n",
                h->goto_x, h->goto_y, h->orders);
        return fail("REF idle hunter should prefer capital when MD comparable");
      }
    }
    distant->active = false;
  }

  /*
   * Second MoW @ difficulty≥2 when naval pool allows (0982 path).
   */
  {
    colonies.colonies[0].nation_id = 0;
    col1.head.difficulty = 2;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    col1.head.expeditionary_force[0] = 3;
    col1.head.expeditionary_force[2] = 2;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (u->active && u->nation_id == 1 && units_is_sea(&units, u->id)) {
        u->active = false;
      } else if (u->active && u->nation_id == 1) {
        u->moves_left = 0;
      }
    }
    const int sea_before = count_nation_sea(&units, 1);
    status[0] = '\0';
    ai_king_nation_turn(&ctx);
    const int sea_spawned = count_nation_sea(&units, 1) - sea_before;
    if (sea_spawned < 2) {
      fprintf(stderr, "smoke_ai_king: second MoW sea_spawned=%d (want ≥2)\n", sea_spawned);
      return fail("diff≥2 + force[2]≥2 should spawn second MoW");
    }
    col1.head.difficulty = 0;
  }

  /*
   * MoW hold Regular+Dragoon mix (0982 cargo unload): force[0]=1, force[1]=2,
   * force[2]=1 → ship + 1 Regular + 2 Dragoons (hold-size-3; MoW×6 PARKED).
   * Source: fandom REF Men-O-War / Regulars / Cavalry.
   */
  {
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    memset(col1.head.backup_force, 0, sizeof(col1.head.backup_force));
    col1.head.expeditionary_force[0] = 1; /* Regular */
    col1.head.expeditionary_force[1] = 2; /* Dragoon */
    col1.head.expeditionary_force[2] = 1; /* MoW */
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 1) {
        continue;
      }
      u->moves_left = 0;
      if (u->x == 5 && u->y == 5) {
        u->x = 1;
        u->y = 1;
      }
    }
    int reg_before = 0;
    int drg_before = 0;
    int sea_before = count_nation_sea(&units, 1);
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 1 || units_is_sea(&units, u->id)) {
        continue;
      }
      if (u->type_index == ty_regular) {
        reg_before++;
      } else if (u->type_index == ty_dragoon) {
        drg_before++;
      }
    }
    ai_king_nation_turn(&ctx);
    if (count_nation_sea(&units, 1) <= sea_before) {
      return fail("Regular+Dragoon MoW hold should spawn Man-O-War");
    }
    int reg_after = 0;
    int drg_after = 0;
    for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
      const ColonizeUnit* u = &units.units[i];
      if (!u->active || u->nation_id != 1 || units_is_sea(&units, u->id)) {
        continue;
      }
      if (u->type_index == ty_regular) {
        reg_after++;
      } else if (u->type_index == ty_dragoon) {
        drg_after++;
      }
    }
    if (reg_after < reg_before + 1) {
      fprintf(stderr, "smoke_ai_king: MoW hold Regular %d→%d (want +1)\n", reg_before,
              reg_after);
      return fail("MoW hold should unload Regular first when force[0]>0");
    }
    if (drg_after < drg_before + 2) {
      fprintf(stderr, "smoke_ai_king: MoW hold Dragoon %d→%d (want +2)\n", drg_before,
              drg_after);
      return fail("MoW hold should fill remaining slots with Dragoon from force[1]");
    }
    /*
     * MoW×6 PARKED (fandom “man-o-war with 6 units”): structural unload is
     * hold-size-3 only — do not invent a 6-slot embark this beat.
     */
    {
      const int unloaded =
          (reg_after - reg_before) + (drg_after - drg_before);
      if (unloaded > 3) {
        fprintf(stderr, "smoke_ai_king: MoW hold unloaded=%d (want ≤3; ×6 PARKED)\n",
                unloaded);
        return fail("MoW hold must not invent ×6 unload (structural hold≤3 PARKED)");
      }
      if (unloaded < 3) {
        fprintf(stderr, "smoke_ai_king: MoW hold unloaded=%d (want 3 structural)\n",
                unloaded);
        return fail("MoW hold Regular+Dragoon mix should unload exactly 3");
      }
    }
    /* force[1] fully drained; force[0] drained then tax residual +1 (1d42 crumb). */
    if (col1.head.expeditionary_force[1] != 0) {
      fprintf(stderr, "smoke_ai_king: after mix unload force1=%u (want 0)\n",
              (unsigned)col1.head.expeditionary_force[1]);
      return fail("MoW Regular+Dragoon unload should drain force[1] Dragoons");
    }
  }

  /*
   * ai_popup wire (human queue on turn ctx): tax audience CHOICE defers hike;
   * apply Accept finishes 1d42 effect. Without ai_popups, auto path unchanged.
   */
  {
    AiPopupState pop;
    ai_popup_init(&pop);
    ctx.ai_popups = &pop;

    /* Peacetime tax audience: clear WoI so 1d42 runs; spring tax year. */
    col1.head.unknown46[0] = 0;
    col1.head.unknown46[2] = 0;
    col1.head.unknown46[5] = 0;
    col1.nation[0].tax_rate = 10;
    europe.tax_percent = 10;
    col1.nation[0].boycott_bitmap = 0;
    col1.nation[0].liberty_bells_total = 0;
    col1.colony[0].rebel_dividend = 20;
    col1.colony[0].rebel_divisor = 100;
    year = 1536;
    autumn = 0;
    status[0] = '\0';
    ai_popup_clear(&pop);
    const uint8_t tax_before = col1.nation[0].tax_rate;
    ai_king_nation_turn(&ctx);
    if (col1.nation[0].tax_rate != tax_before) {
      return fail("ai_popups tax audience must defer hike until apply");
    }
    if (pop.queue_count < 1 || pop.queue[0].tag != AI_POPUP_TAG_KING_AUDIENCE ||
        pop.queue[0].kind != AI_POPUP_KIND_CHOICE) {
      return fail("ai_popups should enqueue KING_AUDIENCE choice");
    }
    /* Simulate Accept result (game_loop path). */
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* Accept */
    pop.result_tag = AI_POPUP_TAG_KING_AUDIENCE;
    pop.result_nation_a = 0;
    pop.result_nation_b = 1;
    pop.result_payload = (int)tax_before;
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (col1.nation[0].tax_rate != tax_before + 1) {
      return fail("apply Accept should hike tax (1d42)");
    }
    if (col1.head.unknown46[2] != 0) {
      return fail("Accept hike must not set boycott refuse");
    }

    /* Refuse path: enqueue again, apply Refuse → boycott. */
    year = 1558;
    autumn = 0;
    col1.nation[0].tax_rate = 20;
    europe.tax_percent = 20;
    col1.nation[0].boycott_bitmap = 0;
    col1.head.unknown46[2] = 0;
    status[0] = '\0';
    ai_popup_clear(&pop);
    ai_king_nation_turn(&ctx);
    if (col1.head.unknown46[2] != 0 || col1.nation[0].tax_rate != 20) {
      return fail("ai_popups refuse choice must defer boycott until apply");
    }
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 2; /* Refuse */
    pop.result_tag = AI_POPUP_TAG_KING_AUDIENCE;
    pop.result_nation_a = 0;
    pop.result_nation_b = 1;
    pop.result_payload = 20;
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (col1.head.unknown46[2] == 0) {
      return fail("apply Refuse should set boycott unknown46[2]");
    }
    if (col1.nation[0].tax_rate != 20) {
      return fail("apply Refuse must leave tax_rate unchanged");
    }
    if ((col1.nation[0].boycott_bitmap & (1u << 1)) == 0) {
      return fail("apply Refuse should freeze Sugar boycott bit");
    }
    /* R2: Sugar refuse follow-up OK after Refuse apply (FUN_43f7_38fd_5be8). */
    {
      int found_refuse_ok = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_TAX &&
            pop.queue[i].kind == AI_POPUP_KIND_OK &&
            (strstr(pop.queue[i].body, "Sugar") || strstr(pop.queue[i].body, "boycott"))) {
          found_refuse_ok = 1;
          break;
        }
      }
      if (!found_refuse_ok) {
        return fail("apply Refuse should enqueue Sugar boycott follow-up OK");
      }
    }

    /* Congress CHOICE: gate met → enqueue, no WoI until Confirm. */
    col1.head.unknown46[0] = 0;
    col1.head.unknown46[5] = 0;
    col1.colony[0].rebel_dividend = 60;
    col1.colony[0].rebel_divisor = 100;
    col1.nation[0].liberty_bells_total = 200;
    year = 1600;
    autumn = 0;
    status[0] = '\0';
    ai_popup_clear(&pop);
    /* Avoid another tax audience this beat: off tax interval. */
    year = 1537;
    ai_king_nation_turn(&ctx);
    if (col1.head.unknown46[0] != 0) {
      return fail("ai_popups congress must defer WoI until Confirm");
    }
    {
      int found_congress = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_CONGRESS &&
            pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
          found_congress = 1;
          break;
        }
      }
      if (!found_congress) {
        return fail("ai_popups should enqueue KING_CONGRESS choice");
      }
    }
    pop.has_result = true;
    pop.result_cancelled = false;
    pop.result_choice_id = 1; /* Confirm */
    pop.result_tag = AI_POPUP_TAG_KING_CONGRESS;
    pop.result_nation_a = 0;
    pop.result_nation_b = 1;
    pop.result_payload = 60;
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (col1.head.unknown46[0] == 0 || col1.head.unknown46[5] == 0) {
      return fail("apply Confirm should declare WoI + congress unknown46[5]");
    }
    /* R2: Confirm chain → United Colonies rename OK + WoI begins OK (160a / 1a26). */
    {
      int found_rename = 0;
      int found_woi = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].kind != AI_POPUP_KIND_OK) {
          continue;
        }
        if (strstr(pop.queue[i].body, "United Colonies") ||
            strstr(pop.queue[i].title, "United Colonies")) {
          found_rename = 1;
        }
        if (strstr(pop.queue[i].body, "War of Independence")) {
          found_woi = 1;
        }
      }
      if (!found_rename) {
        return fail("apply Confirm should enqueue United Colonies rename OK");
      }
      if (!found_woi) {
        return fail("apply Confirm should enqueue War of Independence begins OK");
      }
    }
    if (strcmp(col1.player[0].country_name, "United Colonies") != 0) {
      return fail("apply Confirm must still rename country_name (choice apply)");
    }

    /* Merc CHOICE: wartime + gold → enqueue Hire; apply spends/spawns. */
    col1.head.unknown46[3] = 0;
    col1.nation[0].gold = 400;
    europe.gold = 400;
    colonies.colonies[0].nation_id = 0;
    memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
    status[0] = '\0';
    ai_popup_clear(&pop);
    const int merc_units_before = count_nation(&units, 0);
    const uint32_t merc_gold_before = col1.nation[0].gold;
    ai_king_nation_turn(&ctx);
    if (col1.nation[0].gold != merc_gold_before) {
      return fail("ai_popups merc must defer spend until Hire apply");
    }
    if (col1.head.unknown46[3] != 0) {
      return fail("ai_popups merc must leave unknown46[3] clear until apply");
    }
    {
      int found_merc = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC &&
            pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
          found_merc = 1;
          break;
        }
      }
      if (!found_merc) {
        return fail("ai_popups should enqueue KING_MERC Hire/Decline");
      }
    }
    /* Preserve offer payload (packed landing); colony may be captured same beat. */
    {
      int merc_payload = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC &&
            pop.queue[i].kind == AI_POPUP_KIND_CHOICE) {
          merc_payload = pop.queue[i].payload;
          break;
        }
      }
      pop.has_result = true;
      pop.result_cancelled = false;
      pop.result_choice_id = 1; /* Hire */
      pop.result_tag = AI_POPUP_TAG_KING_MERC;
      pop.result_nation_a = 0;
      pop.result_nation_b = 1;
      pop.result_payload = merc_payload;
    }
    ai_king_apply_popup_result(&ctx, &pop);
    ai_popup_consume_result(&pop);
    if (col1.head.unknown46[3] == 0) {
      return fail("apply Hire should set merc unknown46[3]");
    }
    if (col1.nation[0].gold != merc_gold_before - 300) {
      return fail("apply Hire should spend 300 gold");
    }
    if (count_nation(&units, 0) <= merc_units_before) {
      return fail("apply Hire should spawn Continental merc");
    }
    {
      int found_hire_ok = 0;
      for (int i = 0; i < pop.queue_count; ++i) {
        if (pop.queue[i].tag == AI_POPUP_TAG_KING_MERC &&
            pop.queue[i].kind == AI_POPUP_KIND_OK &&
            strstr(pop.queue[i].body, "join the Continental")) {
          found_hire_ok = 1;
          break;
        }
      }
      if (!found_hire_ok) {
        return fail("apply Hire should enqueue merc success follow-up OK");
      }
    }

    /*
     * R3: 10f0 intervene landing enqueues KING_ARRIVAL once (REF 1528 already OK).
     * WoI + REF empty + backup; merc flag already set so no Hire CHOICE spam.
     */
    {
      col1.head.unknown46[0] = 1;
      memset(col1.head.expeditionary_force, 0, sizeof(col1.head.expeditionary_force));
      colonies.colonies[0].nation_id = 0;
      col1.head.backup_force[0] = 2;
      col1.head.backup_force[1] = 2;
      col1.head.backup_force[2] = 0;
      col1.head.backup_force[3] = 0;
      status[0] = '\0';
      ai_popup_clear(&pop);
      ai_king_nation_turn(&ctx);
      {
        int arrival_ok = 0;
        for (int i = 0; i < pop.queue_count; ++i) {
          if (pop.queue[i].tag == AI_POPUP_TAG_KING_ARRIVAL &&
              pop.queue[i].kind == AI_POPUP_KIND_OK &&
              strstr(pop.queue[i].body, "Foreign troops")) {
            arrival_ok++;
          }
        }
        if (arrival_ok != 1) {
          fprintf(stderr, "smoke_ai_king: intervene ARRIVAL count=%d (want 1)\n",
                  arrival_ok);
          return fail("10f0 intervene should enqueue KING_ARRIVAL OK once");
        }
      }
      /* Same-turn capture may overwrite status (1528 pattern); popup is canonical. */
    }

    /*
     * R2 new smoke: SoL restless chrome enqueues INFO OK when human queue attached.
     * Autumn + SoL 45 + bells below declare; peacetime (clear WoI).
     * Force single-colony SoL (earlier 1eca block may leave colony_count=2).
     * FUN_43f7_0004 / peacetime restless — must not set WoI/congress.
     */
    {
      col1.head.unknown46[0] = 0;
      col1.head.unknown46[5] = 0;
      col1.head.colony_count = 1;
      col1.colony[0].nation_id = 0;
      col1.colony[0].population = 4;
      col1.colony[0].rebel_dividend = 45;
      col1.colony[0].rebel_divisor = 100;
      col1.nation[0].liberty_bells_total = 50;
      year = 1590;
      autumn = 1;
      status[0] = '\0';
      ai_popup_clear(&pop);
      if (ai_king_sol_percent(&ctx, 0) != 45) {
        return fail("restless+ai_popups SoL setup want 45");
      }
      ai_king_nation_turn(&ctx);
      if (col1.head.unknown46[0] != 0 || col1.head.unknown46[5] != 0) {
        return fail("restless+ai_popups must leave WoI/congress clear");
      }
      if (!strstr(status, "Sons of Liberty") || !strstr(status, "45")) {
        fprintf(stderr, "smoke_ai_king: restless+popups status: '%s'\n", status);
        return fail("restless+ai_popups should still set restless status");
      }
      {
        int found_restless = 0;
        for (int i = 0; i < pop.queue_count; ++i) {
          if (pop.queue[i].kind == AI_POPUP_KIND_OK &&
              (pop.queue[i].tag == AI_POPUP_TAG_INFO ||
               pop.queue[i].tag == AI_POPUP_TAG_KING_TAX) &&
              strstr(pop.queue[i].body, "restless")) {
            found_restless = 1;
            break;
          }
        }
        if (!found_restless) {
          return fail("restless chrome should enqueue INFO OK when ai_popups");
        }
      }
    }

    ctx.ai_popups = NULL;
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
          "smoke_ai_king: ok (sol=%d tax=%u crown=%d intervene=%d boycott=%d merc=%d "
          "1eca=colony-SoL popups)\n",
          sol, tax_final, crown_final, intervene_final, boycott_final, merc_final);
  return 0;
}
