#include "core/ai_popup.h"
#include "core/col1_save.h"
#include "core/map.h"
#include "core/units.h"
#include "core/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/test_catalogs.h"
#include "../common/test_runner.h"

/*
 * Shift-F6 Kill Indians = DOS CHEAT case 0x67 -> FUN_4d56_01e2 (raw 81352):
 * FUN_4d56_00e0 for every village of the nation, highest tribe index first.
 * Nothing beyond what 00e0 does per village: no owner-nibble repaint, no
 * indian[] reset, no diplomacy clear, no despawn of units without a home
 * village.
 */

#define KI_W 20

typedef struct KiFix {
  ColonizeCol1Save col1;
  ColonizeWorldMap map;
  ColonizeUnitPool units;
  AiPopupState pops;
} KiFix;

static int ki_setup(KiFix* f, const int* nations, int n) {
  memset(f, 0, sizeof(*f));
  col1_save_init(&f->col1);
  f->col1.tribe = calloc((size_t)n, sizeof(ColonizeCol1Tribe));
  f->col1.owned = true; /* col1_save_free releases the section */
  f->map.layer3 = calloc(KI_W * KI_W, 1);
  if (!f->col1.tribe || !f->map.layer3) {
    return 1;
  }
  f->col1.head.tribe_count = (uint16_t)n;
  f->map.width = KI_W;
  f->map.height = KI_W;
  f->map.tile_count = KI_W * KI_W;
  for (int i = 0; i < n; ++i) {
    f->col1.tribe[i].x = (uint8_t)(4 + 2 * i);
    f->col1.tribe[i].y = 10;
    f->col1.tribe[i].nation_id = (uint8_t)nations[i];
    f->col1.stuff.tribe_village_counts[nations[i] - 4]++;
    f->map.layer3[10 * KI_W + 4 + 2 * i] = (uint8_t)((nations[i] << 4) | 1u);
  }
  units_reset(&f->units);
  units_set_occupancy_map(NULL);
  f->units.type_count = 1;
  snprintf(f->units.types[0].name, sizeof(f->units.types[0].name), "Brave");
  f->units.types[0].movement = 3;
  ai_popup_init(&f->pops);
  units_set_combat_popups(&f->pops, test_game_txt());
  return 0;
}

static void ki_free(KiFix* f) {
  units_set_combat_popups(NULL, NULL);
  free(f->map.layer3);
  col1_save_free(&f->col1);
}

static ColonizeUnit* ki_brave(KiFix* f, int nation, int home, int* out_id) {
  const int id = units_spawn_allow_stack(&f->units, 0, 1, 1);
  ColonizeUnit* u = units_get(&f->units, id);
  if (u) {
    u->nation_id = nation;
    u->home_tribe_id = home;
  }
  *out_id = id;
  return u;
}

#define KI_FAIL(...)            \
  do {                          \
    fprintf(stderr, __VA_ARGS__); \
    ki_free(&f);                \
    return 1;                   \
  } while (0)

/* Inca at 0 and 2, Aztec at 1 and 3: kill Inca. */
static int case_kill_nation_effects(void) {
  if (!test_game_txt()) {
    return TR_SKIP;
  }
  static const int nat[4] = {4, 5, 4, 5};
  KiFix f;
  if (ki_setup(&f, nat, 4)) {
    return 1;
  }
  f.col1.indian[0].tech = 2;
  f.col1.nation[0].relation_by_indian[0] = 0x21;
  int id_home0, id_home2, id_free, id_az3, id_az1, id_euro;
  ki_brave(&f, 4, 0, &id_home0);
  ki_brave(&f, 4, 2, &id_home2);
  ki_brave(&f, 4, -1, &id_free);
  ki_brave(&f, 5, 3, &id_az3);
  ki_brave(&f, 5, 1, &id_az1);
  ColonizeUnit* euro = ki_brave(&f, 0, 0, &id_euro);
  if (!euro) {
    KI_FAIL("spawn failed\n");
  }
  ColonizeWorld w = world_make(&f.units, NULL, &f.map, &f.col1, true, NULL, NULL);
  const int razed = units_destroy_tribe_nation_w(&w, 4);
  if (razed != 2) {
    KI_FAIL("expected 2 villages razed, got %d\n", razed);
  }
  if (f.col1.head.tribe_count != 2 || f.col1.tribe[0].nation_id != 5 ||
      f.col1.tribe[0].x != 6 || f.col1.tribe[1].nation_id != 5 || f.col1.tribe[1].x != 10) {
    KI_FAIL("only the two Aztec villages should remain, in order\n");
  }
  const ColonizeUnit* u;
  if ((u = units_get(&f.units, id_home0)) && u->active) {
    KI_FAIL("brave bound to village 0 must be despawned\n");
  }
  if ((u = units_get(&f.units, id_home2)) && u->active) {
    KI_FAIL("brave bound to village 2 must be despawned\n");
  }
  u = units_get(&f.units, id_free);
  if (!u || !u->active || u->nation_id != 4) {
    KI_FAIL("brave without a home village must survive (DOS 00e0 only walks home-bound units)\n");
  }
  u = units_get(&f.units, id_az3);
  if (!u || !u->active || u->home_tribe_id != 1) {
    KI_FAIL("Aztec brave home 3 must remap to 1 (got %d)\n", u ? u->home_tribe_id : -99);
  }
  u = units_get(&f.units, id_az1);
  if (!u || !u->active || u->home_tribe_id != 0) {
    KI_FAIL("Aztec brave home 1 must remap to 0 (got %d)\n", u ? u->home_tribe_id : -99);
  }
  u = units_get(&f.units, id_euro);
  if (!u || !u->active || u->home_tribe_id != 0) {
    KI_FAIL("Euro units are never destroyed or renumbered by 00e0\n");
  }
  if (f.col1.stuff.tribe_village_counts[0] != 0 || !f.col1.indian[0].extinct) {
    KI_FAIL("Inca must be extinct with a zero village count\n");
  }
  if (f.col1.stuff.tribe_village_counts[1] != 2 || f.col1.indian[1].extinct) {
    KI_FAIL("Aztec count/extinct must be untouched\n");
  }
  if (f.pops.queue_count != 1) {
    KI_FAIL("exactly one @EXTINCT popup expected (last Inca village), got %d\n",
            f.pops.queue_count);
  }
  if ((f.map.layer3[10 * KI_W + 4] >> 4) != 4 || (f.map.layer3[10 * KI_W + 8] >> 4) != 4 ||
      (f.map.layer3[10 * KI_W + 6] >> 4) != 5) {
    KI_FAIL("owner nibbles must be untouched (00e0 clears only the settlement bit)\n");
  }
  if (f.col1.indian[0].tech != 2 || f.col1.nation[0].relation_by_indian[0] != 0x21) {
    KI_FAIL("indian[] slot and diplomacy must be untouched\n");
  }
  ki_free(&f);
  return 0;
}

/* Inca at 0, 2, 3 around an Aztec at 1: descending order razes all three. */
static int case_kill_nation_descending_and_horse_fold(void) {
  if (!test_game_txt()) {
    return TR_SKIP;
  }
  static const int nat[4] = {4, 5, 4, 4};
  KiFix f;
  if (ki_setup(&f, nat, 4)) {
    return 1;
  }
  f.col1.indian[0].horse_herds = 30;
  ColonizeWorld w = world_make(&f.units, NULL, &f.map, &f.col1, true, NULL, NULL);
  if (units_destroy_tribe_nation_w(&w, 4) != 3) {
    KI_FAIL("all three Inca villages must go (ascending compaction would skip one)\n");
  }
  if (f.col1.head.tribe_count != 1 || f.col1.tribe[0].nation_id != 5) {
    KI_FAIL("only the Aztec village should remain\n");
  }
  /* Counts 3 -> 2 -> 1 -> 0: 00e0 folds horses on the first two (div = -1 - left):
   * 30 + 30/-3 = 20, then 20 + 20/-2 = 10; the third goes extinct without a fold. */
  if (f.col1.indian[0].horse_herds != 10 || !f.col1.indian[0].extinct) {
    KI_FAIL("horse fold/extinct mismatch (herds=%d)\n", (int)f.col1.indian[0].horse_herds);
  }
  ki_free(&f);
  return 0;
}

/* The xy form (View-mode Shift-D on a village) razes exactly one village. */
static int case_destroy_village_at(void) {
  if (!test_game_txt()) {
    return TR_SKIP;
  }
  static const int nat[3] = {4, 5, 4};
  KiFix f;
  if (ki_setup(&f, nat, 3)) {
    return 1;
  }
  ColonizeWorld w = world_make(&f.units, NULL, &f.map, &f.col1, true, NULL, NULL);
  if (units_destroy_village_at_w(&w, 5, 10) != -1 || f.col1.head.tribe_count != 3) {
    KI_FAIL("a non-village tile must destroy nothing\n");
  }
  if (units_destroy_village_at_w(&w, 8, 10) != 4 || f.col1.head.tribe_count != 2 ||
      f.col1.tribe[0].x != 4 || f.col1.tribe[1].x != 6) {
    KI_FAIL("village (8,10) must go, the rest shift down\n");
  }
  if (f.col1.stuff.tribe_village_counts[0] != 1 || f.col1.indian[0].extinct ||
      f.pops.queue_count != 0) {
    KI_FAIL("Inca still has a village: not extinct, no popup\n");
  }
  ki_free(&f);
  return 0;
}

static const TestCase k_cases[] = {
    {"case_kill_nation_effects", case_kill_nation_effects},
    {"case_kill_nation_descending_and_horse_fold", case_kill_nation_descending_and_horse_fold},
    {"case_destroy_village_at", case_destroy_village_at},
};

TEST_MAIN(k_cases)
