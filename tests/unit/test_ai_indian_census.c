/*
 * FUN_4d56_1816 §6 regression — the Indian nation-turn tick that the goldens
 * do not cover (the joint field policy compares tribes/units/relations, never
 * the `stuff` census block).
 *
 * The strong evidence here is a real DOS save: `test-saves-ai/TURN7.SAV`
 * stores the census arrays DOS's own `FUN_4962_06b6` (reached from 1816 via
 * `FUN_2a1f_0270`) wrote, so recomputing them from the same save's tribe and
 * unit tables must reproduce them byte for byte:
 *
 *   tribe_village_counts     = 06 01 07 05 06 03 03 03
 *   tribe_population_totals  = 39 09 24 1a 1f 0a 0a 0a
 *   tribe_data_9184          = 30 08 38 28 30 18 18 18   (= 8 x Brave count)
 *
 * It also pins the DOS quirk `village_counts_by_continent` is zeroed on every
 * call and refilled from the current tribe type only, so after the eight
 * 1816 calls it holds the LAST type's villages — which is exactly what
 * TURN7.SAV stores (a single 3 at continent 9, and slot 7's count is 3).
 *
 * Also covers §6a goods decay and §6c horse breeding.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/col1_stuff_census.h"
#include "core/colony.h"
#include "core/colony_production.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"

static int fail(const char* msg) {
  fprintf(stderr, "unit_ai_indian_census: FAIL %s\n", msg);
  return 1;
}

/* layer3 high nibble = tile owner (FUN_137f_0200 / FUN_137f_0228). */
static void set_owner(ColonizeWorldMap* map, int x, int y, int nation) {
  const int i = y * map->width + x;
  map->layer3[i] = (uint8_t)((map->layer3[i] & 0x0fu) | ((unsigned)nation & 0x0fu) << 4);
}

static int owner_of(const ColonizeWorldMap* map, int x, int y) {
  const int hi = (map->layer3[y * map->width + x] >> 4) & 0x0f;
  return hi == 0x0f ? -1 : hi;
}

/*
 * FUN_4d56_1b3a phase 3 — a colonist actually working a tile the natives
 * still own transfers that tile's owner nibble to the colony, unless a
 * settlement or any unit is standing on it (`FUN_137f_0428` layer2 0x03).
 */
static int test_1b3a_phase3(void) {
  char err[128];
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  if (!map_alloc(&map, 32, 32, err, sizeof(err))) {
    return fail("phase3 map_alloc");
  }
  for (int i = 0; i < 32 * 32; ++i) {
    map.terrain[i] = 2; /* plains */
  }
  map_reveal_all(&map, 0);

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  const int cid = colonies_found(&colonies, &map, 10, 10, 0, 0, 0, 0, 0, 0);
  if (cid < 0) {
    return fail("phase3 colonies_found");
  }
  ColonizeColony* c = colonies_get_mut(&colonies, cid);

  /* Three worked plots, three different verdicts. */
  int worked = 0;
  int idx_claim = -1;
  int idx_blocked = -1;
  int idx_unworked = -1;
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES; ++i) {
    c->tiles[i] = -1;
  }
  for (int i = 0; i < COLONIZE_COLONY_FIELD_TILES && worked < 3; ++i) {
    int dx = 0;
    int dy = 0;
    if (!colonies_field_tile_delta(i, &dx, &dy)) {
      continue;
    }
    set_owner(&map, c->x + dx, c->y + dy, 4); /* Indian-claimed */
    if (worked == 0) {
      idx_claim = i;
      c->tiles[i] = 0;
    } else if (worked == 1) {
      idx_blocked = i;
      c->tiles[i] = 1;
      /* A unit (layer2 0x01) parked on the plot vetoes the claim. */
      map_occupancy_set_layer2(&map, c->x + dx, c->y + dy, MAP_OCCUPANCY_HAS_UNIT, true);
    } else {
      idx_unworked = i; /* claimed by natives, but nobody works it */
    }
    worked++;
  }
  if (idx_claim < 0 || idx_blocked < 0 || idx_unworked < 0) {
    return fail("phase3 fixture setup");
  }

  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.map = &map;
  ctx.colonies = &colonies;
  ai_indian_midpass_claim_worked_tiles(&ctx);

  int dx = 0;
  int dy = 0;
  colonies_field_tile_delta(idx_claim, &dx, &dy);
  if (owner_of(&map, c->x + dx, c->y + dy) != 0) {
    return fail("1b3a phase 3 should claim a worked native-owned plot");
  }
  colonies_field_tile_delta(idx_blocked, &dx, &dy);
  if (owner_of(&map, c->x + dx, c->y + dy) != 4) {
    return fail("1b3a phase 3 must not claim a plot with a unit on it");
  }
  colonies_field_tile_delta(idx_unworked, &dx, &dy);
  if (owner_of(&map, c->x + dx, c->y + dy) != 4) {
    return fail("1b3a phase 3 must not claim a plot nobody works");
  }

  map_free(&map);
  return 0;
}


/*
 * smell_audit 2026-09-09 #74 — FUN_4962_0018's EURO unit census (the Indian
 * sibling above is FUN_4962_06b6). Three nation rows, all fed from the
 * FUN_281f_09c8 -> FUN_157e_004a combat VALUE, not from unit counts:
 *
 *   0x9180 land_combat_totals   += 09c8(u, 0)    saturating byte add
 *   0x941c land_combat_strength += 09c8(u, 1)    plain 16-bit word add
 *   0x942c field_combat_totals  += 09c8(u, 1)    saturating byte, gated
 *
 * The gate (asm 4962:022f-026e): a unit standing on a settlement does not
 * count when its nation is human-controlled, nor when its ai_plan letter
 * (+0x314b) is 'A' or 'G' (FUN_521d_0a60 garrison assignment).
 */
static int census_expect(const ColonizeUnitPool* units, int uid, int mode) {
  const ColonizeUnit* u = units_get_const(units, uid);
  const ColonizeUnitType* t = units_type(units, u->type_index);
  int base = (mode == 0) ? t->defense : t->attack;
  if (strstr(t->name, "Artillery") != NULL && (u->col1_unknown15 & 0x80u) != 0) {
    base -= 2;
  }
  if (base < 0) {
    base = 0;
  }
  int v = base * 8;
  if ((strstr(t->name, "Soldier") != NULL || strstr(t->name, "Dragoon") != NULL) &&
      (u->profession == UNITS_JOB_SOLDIER || u->profession == UNITS_JOB_DRAGOON)) {
    v += v >> 1;
  }
  return v;
}

static int test_euro_census_4962_0018(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    return fail("census NAMES.TXT");
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  if (!units_load_types(&units, &names)) {
    return fail("census units_load_types");
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);

  const int ty_colonist = units_find_type(&units, "Colonists");
  const int ty_soldier = units_find_type(&units, "Soldiers");
  const int ty_arty = units_find_type(&units, "Artillery");
  const int ty_ship = units_find_type(&units, "Caravel");
  if (ty_colonist < 0 || ty_soldier < 0 || ty_arty < 0 || ty_ship < 0) {
    return fail("census unit types missing");
  }

  /* An AI colony at (10,10); nothing else on the board. */
  ColonizeColony* col = &colonies.colonies[0];
  memset(col, 0, sizeof(*col));
  col->active = true;
  col->id = 0;
  col->x = 10;
  col->y = 10;
  col->nation_id = 1;
  col->building_in_production = -1;
  if (colonies.colony_count < 1) {
    colonies.colony_count = 1;
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.player[0].control = 0; /* human */
  col1.player[1].control = 1; /* AI */

  /* Nation 1 (AI): open-field colonist, veteran soldier, damaged artillery. */
  const int u_col = units_spawn_allow_stack(&units, ty_colonist, 3, 3);
  const int u_vet = units_spawn_allow_stack(&units, ty_soldier, 4, 3);
  const int u_art = units_spawn_allow_stack(&units, ty_arty, 5, 3);
  /* Nation 1: garrisoned in the colony ('G'), and a plain unit on the same
   * tile — only the first is excluded from field_combat_totals. */
  const int u_gar = units_spawn_allow_stack(&units, ty_soldier, 10, 10);
  const int u_intown = units_spawn_allow_stack(&units, ty_colonist, 10, 10);
  /* Nation 1: a ship contributes to none of the three rows. */
  const int u_ship = units_spawn_allow_stack(&units, ty_ship, 6, 3);
  /* Nation 0 (human): standing on the colony tile — never a field unit. */
  const int u_hum = units_spawn_allow_stack(&units, ty_soldier, 10, 10);
  if (u_col < 0 || u_vet < 0 || u_art < 0 || u_gar < 0 || u_intown < 0 || u_ship < 0 ||
      u_hum < 0) {
    return fail("census spawn");
  }
  const int ai_units[6] = {u_col, u_vet, u_art, u_gar, u_intown, u_ship};
  for (int i = 0; i < 6; ++i) {
    units_get(&units, ai_units[i])->nation_id = 1;
  }
  units_get(&units, u_hum)->nation_id = 0;
  units_get(&units, u_vet)->profession = UNITS_JOB_SOLDIER; /* veteran +50% */
  units_get(&units, u_gar)->profession = UNITS_JOB_SOLDIER;
  units_get(&units, u_hum)->profession = UNITS_JOB_SOLDIER;
  units_get(&units, u_art)->col1_unknown15 |= 0x80u; /* damaged artillery -2 */
  units_get(&units, u_gar)->col1_ai_plan = 0x47u; /* 'G' */
  units_get(&units, u_intown)->col1_ai_plan = 'X';
  units_get(&units, u_hum)->col1_ai_plan = 'X';

  memset(&col1.stuff, 0, sizeof(col1.stuff));
  col1_stuff_census_fill_blank(&col1.stuff, &units, &colonies, &col1);

  int want0 = 0;
  int want1 = 0;
  int want_field = 0;
  const int land_ai[5] = {u_col, u_vet, u_art, u_gar, u_intown};
  for (int i = 0; i < 5; ++i) {
    want0 += census_expect(&units, land_ai[i], 0);
    want1 += census_expect(&units, land_ai[i], 1);
    if (land_ai[i] != u_gar) {
      want_field += census_expect(&units, land_ai[i], 1);
    }
  }
  if (want0 > 255) {
    want0 = 255;
  }
  if (want_field > 255) {
    want_field = 255;
  }

  if ((int)col1.stuff.land_combat_totals[1] != want0) {
    fprintf(
      stderr, "land_combat_totals[1] got %u want %d\n",
      (unsigned)col1.stuff.land_combat_totals[1], want0
    );
    return fail("0x9180 is not the mode-0 value sum");
  }
  if ((int)col1.stuff.land_combat_strength[1] != want1) {
    fprintf(
      stderr, "land_combat_strength[1] got %u want %d\n",
      (unsigned)col1.stuff.land_combat_strength[1], want1
    );
    return fail("0x941c is not the mode-1 value sum");
  }
  if ((int)col1.stuff.field_combat_totals[1] != want_field) {
    fprintf(
      stderr, "field_combat_totals[1] got %u want %d\n",
      (unsigned)col1.stuff.field_combat_totals[1], want_field
    );
    return fail("0x942c gate / value wrong");
  }
  /* The vet soldier really is scaled x8 and +50% — pins the DOS scale. */
  if (census_expect(&units, u_vet, 1) !=
      units_type(&units, units_get_const(&units, u_vet)->type_index)->attack * 12) {
    return fail("veteran mode-1 scale not base*8*1.5");
  }
  /* Human nation: its soldier stands on a settlement, so no field value at
   * all, but it still carries mode-0/mode-1 totals. */
  if (col1.stuff.field_combat_totals[0] != 0) {
    return fail("human unit on a settlement must not count as field combat");
  }
  if ((int)col1.stuff.land_combat_strength[0] != census_expect(&units, u_hum, 1)) {
    return fail("human land_combat_strength wrong");
  }
  /* Ship rows stay out of all three. */
  const int ship_mode1 = census_expect(&units, u_ship, 1);
  if (ship_mode1 > 0 && (int)col1.stuff.land_combat_strength[1] != want1) {
    return fail("ship leaked into the land census");
  }

  col1_save_free(&col1);
  assets_msg_free(&names);
  return 0;
}


/*
 * smell_audit 2026-09-09 #64 — the WoI Tory bells negation must key off
 * head.crown_nation_id (DOS DS:0x53d2, the slot the succession merger
 * vacated), not off a "peer of the human Euro slot (0<->1)" re-derivation
 * that can only ever answer 0 or 1. dutch-reports.SAV carries crown 2.
 */
static int test_crown_nation_bells_negation(void) {
  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    return fail("crown NAMES.TXT");
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&colonies, &names)) {
    return fail("crown colonies_load_buildings");
  }
  const int town_hall = colonies_find_building(&colonies, "Town Hall");
  if (town_hall < 0) {
    return fail("crown Town Hall building missing");
  }

  ColonizeCol1Save col1;
  col1_save_init(&col1);
  col1.head.game_options.woi = 1;
  col1.head.crown_nation_id = 2; /* NOT the 0<->1 peer of the human slot */
  col1.player[0].control = 0; /* human */
  col1.player[1].control = 1;
  col1.player[2].control = 1;
  col1.player[3].control = 1;
  col1.head.colony_count = 2;
  col1.colony = calloc(2, sizeof(ColonizeCol1Colony));
  if (!col1.colony) {
    return fail("crown calloc");
  }

  /* Two identical AI colonies — nation 1 (a rebel peer) and nation 2 (the
   * crown). Town Hall + pop 12 => bells = 1 + (12+3)/5 = 4. */
  for (int k = 0; k < 2; ++k) {
    ColonizeColony* c = &colonies.colonies[k];
    memset(c, 0, sizeof(*c));
    c->active = true;
    c->id = k;
    c->x = 5 + k;
    c->y = 5 + k;
    c->nation_id = 1 + k;
    c->population = 12;
    c->colonist_count = 0;
    c->building_in_production = -1;
    c->has_building[town_hall] = true;
    col1.colony[k].x = (uint8_t)(5 + k);
    col1.colony[k].y = (uint8_t)(5 + k);
    col1.colony[k].nation_id = (uint8_t)(1 + k);
    col1.colony[k].population = 12;
    col1.colony[k].rebel_dividend = 100;
    col1.colony[k].rebel_divisor = 1000;
  }
  if (colonies.colony_count < 2) {
    colonies.colony_count = 2;
  }

  colony_prod_tick_rebel_accumulators(&colonies, &colonies.colonies[0], &col1);
  colony_prod_tick_rebel_accumulators(&colonies, &colonies.colonies[1], &col1);

  /* 100 - (100 >> 6) = 99, then + 4 for the rebel peer, - (4 >> 1) for the
   * crown's own colony. With the old peer-of-human formula the two swapped. */
  if (col1.colony[0].rebel_dividend != 103u) {
    fprintf(
      stderr, "nation 1 rebel_dividend got %u want 103\n",
      (unsigned)col1.colony[0].rebel_dividend
    );
    free(col1.colony);
    return fail("bells negation hit the wrong nation (rebel peer)");
  }
  if (col1.colony[1].rebel_dividend != 97u) {
    fprintf(
      stderr, "crown nation 2 rebel_dividend got %u want 97\n",
      (unsigned)col1.colony[1].rebel_dividend
    );
    free(col1.colony);
    return fail("crown_nation_id 2 did not take the Tory negation");
  }

  free(col1.colony);
  col1.colony = NULL;
  col1.head.colony_count = 0;
  col1_save_free(&col1);
  assets_msg_free(&names);
  return 0;
}

int main(void) {
  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file("test-saves-ai/TURN7.SAV", &save, err, sizeof(err))) {
    fprintf(stderr, "read TURN7.SAV: %s\n", err);
    return 1;
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    fprintf(stderr, "NAMES.TXT load failed\n");
    return 1;
  }
  ColonizeUnitPool units;
  memset(&units, 0, sizeof(units));
  units_reset(&units);
  units_set_occupancy_map(NULL);
  if (!units_load_types(&units, &names)) {
    return fail("units_load_types");
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  colonies_set_occupancy_map(NULL);
  if (!colonies_load_buildings(&colonies, &names)) {
    return fail("colonies_load_buildings");
  }
  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&save, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge apply: %s\n", err);
    return 1;
  }

  /* Keep DOS's own values, then wipe ours so the recount has to earn them. */
  uint8_t want_villages[8];
  uint8_t want_pops[8];
  uint8_t want_value[8];
  uint8_t want_continent[16];
  memcpy(want_villages, save.stuff.tribe_village_counts, 8);
  memcpy(want_pops, save.stuff.tribe_population_totals, 8);
  memcpy(want_value, save.stuff.tribe_data_9184, 8);
  memcpy(want_continent, save.stuff.village_counts_by_continent, 16);

  memset(save.stuff.tribe_village_counts, 0xaa, 8);
  memset(save.stuff.tribe_population_totals, 0xaa, 8);
  memset(save.stuff.tribe_data_9184, 0xaa, 8);
  memset(save.stuff.village_counts_by_continent, 0xaa, 16);

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, 100u);
  ColonizeTurnContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.turn_number = &turn_number;
  ctx.game_year = &year;
  ctx.game_autumn = &autumn;
  ctx.human_nation = br.human_nation;
  ctx.units = &units;
  ctx.colonies = &colonies;
  ctx.europe = &europe;
  ctx.map = &map;
  ctx.col1 = &save;
  ctx.col1_ok = true;
  ctx.rng = &rng;

  /* §6a fixture: a signed ledger that must walk toward zero by tech + 1. */
  save.indian[0].tons[0] = 100;
  save.indian[0].tons[1] = -100;
  save.indian[0].tons[2] = 0;
  const int step0 = (int)save.indian[0].tech + 1;

  /* §6c fixture: herds present, breeding well under the population cap. */
  save.indian[0].horse_herds = 5;
  save.indian[0].horse_breeding = 10;

  for (int n = 4; n <= 11; ++n) {
    ai_contact_indian_relation_tick(&ctx, n);
  }

  for (int i = 0; i < 8; ++i) {
    if (save.stuff.tribe_village_counts[i] != want_villages[i]) {
      fprintf(
        stderr,
        "village_counts[%d] got %u want %u\n",
        i,
        (unsigned)save.stuff.tribe_village_counts[i],
        (unsigned)want_villages[i]
      );
      return fail("tribe_village_counts mismatch vs DOS TURN7.SAV");
    }
    if (save.stuff.tribe_population_totals[i] != want_pops[i]) {
      fprintf(
        stderr,
        "pop_totals[%d] got %u want %u\n",
        i,
        (unsigned)save.stuff.tribe_population_totals[i],
        (unsigned)want_pops[i]
      );
      return fail("tribe_population_totals mismatch vs DOS TURN7.SAV");
    }
    if (save.stuff.tribe_data_9184[i] != want_value[i]) {
      fprintf(
        stderr,
        "tribe_data_9184[%d] got %u want %u\n",
        i,
        (unsigned)save.stuff.tribe_data_9184[i],
        (unsigned)want_value[i]
      );
      return fail("tribe_data_9184 (brave combat sum) mismatch vs DOS TURN7.SAV");
    }
  }
  if (memcmp(save.stuff.village_counts_by_continent, want_continent, 16) != 0) {
    return fail("village_counts_by_continent mismatch — DOS last-type-only quirk not reproduced");
  }

  if (save.indian[0].tons[0] != (int16_t)(100 - step0)) {
    return fail("tons decay: positive entry should fall by tech+1");
  }
  if (save.indian[0].tons[1] != (int16_t)(-100 + step0)) {
    return fail("tons decay: negative entry should rise by tech+1");
  }
  if (save.indian[0].tons[2] != 0) {
    return fail("tons decay: a zero entry must be left alone");
  }

  const int cap0 = ((int)save.stuff.tribe_population_totals[0] + 0x19) * 2;
  const int want_hb = (10 + 5) > cap0 ? cap0 : (10 + 5);
  if ((int)save.indian[0].horse_breeding != want_hb) {
    fprintf(
      stderr,
      "horse_breeding got %u want %d (cap %d)\n",
      (unsigned)save.indian[0].horse_breeding,
      want_hb,
      cap0
    );
    return fail("horse breeding accumulation wrong");
  }

  /* The cap really binds: park breeding above it and it must clamp down. */
  save.indian[0].horse_breeding = (uint16_t)(cap0 + 500);
  ai_contact_indian_relation_tick(&ctx, 4);
  if ((int)save.indian[0].horse_breeding != cap0) {
    return fail("horse breeding cap (pop_total + 25) * 2 not applied");
  }

  /* FUN_4d56_1b3a phase 1 — contact_state is a per-YEAR latch, not forever. */
  for (int s = 0; s < 8; ++s) {
    for (int e = 0; e < 4; ++e) {
      save.indian[s].contact_state[e] = 2;
    }
  }
  ai_indian_midpass_clear_tables(&ctx);
  for (int s = 0; s < 8; ++s) {
    for (int e = 0; e < 4; ++e) {
      if (save.indian[s].contact_state[e] != 0) {
        return fail("1b3a phase 1 should clear every indian contact_state word");
      }
    }
  }

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&save);

  if (test_1b3a_phase3() != 0) {
    return 1;
  }

  if (test_euro_census_4962_0018() != 0) {
    return 1;
  }

  if (test_crown_nation_bells_negation() != 0) {
    return 1;
  }

  printf(
    "unit_ai_indian_census: ok (1816 §6 census/decay/horses + 1b3a phases 1/3 + "
    "4962_0018 euro census + crown bells negation)\n"
  );
  return 0;
}
