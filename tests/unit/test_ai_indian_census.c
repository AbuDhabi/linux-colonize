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
#include <string.h>

#include "core/ai.h"
#include "core/ai_contact.h"
#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
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
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    return fail("units_load_types");
  }
  ColonizeColonyPool colonies;
  colonies_init(&colonies);
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

  printf(
    "unit_ai_indian_census: ok (1816 §6 census/decay/horses + 1b3a phases 1/3)\n"
  );
  return 0;
}
