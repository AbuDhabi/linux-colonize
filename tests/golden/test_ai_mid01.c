/*
 * Joint mid-turn goldens (T3 Series D + H).
 * MID01: load TURN7, stamp mid-war Indian×Euro fields, write MID01.SAV.
 * MID02: load MID01, run one full joint turn (Euro + Indian), capture MID02.SAV.
 * Pair compare: joint field list vs golden_ai_turns (units/Braves, tribes,
 * colonies, euro_relation, relation_by_indian, sticky) — structural diff that
 * calendar advanced and both sides still carry Euro+Brave units.
 */
#include "core/assets.h"
#include "core/col1_bridge.h"
#include "core/col1_save.h"
#include "core/colony.h"
#include "core/dos_rng.h"
#include "core/europe.h"
#include "core/map.h"
#include "core/turn.h"
#include "core/units.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#define MID01_PATH "test-saves-ai/MID01.SAV"
#define MID02_PATH "test-saves-ai/MID02.SAV"
#define TURN7_PATH "test-saves-ai/TURN7.SAV"
#define AI_MID_VR_SEED 100u

static int fail(const char* msg) {
  fprintf(stderr, "golden_ai_mid01: FAIL %s\n", msg);
  return 1;
}

static int assert_joint_fields(const ColonizeCol1Save* s, const char* tag) {
  if (!s || s->head.unit_count == 0) {
    fprintf(stderr, "%s: no units\n", tag);
    return 0;
  }
  if (s->head.tribe_count == 0 || !s->tribe) {
    fprintf(stderr, "%s: no tribes\n", tag);
    return 0;
  }
  int euro_u = 0;
  int brave_u = 0;
  for (unsigned i = 0; i < s->head.unit_count; ++i) {
    const ColonizeCol1Unit* u = &s->unit[i];
    if (u->nation_id < 4) {
      euro_u++;
    } else if (u->nation_id <= 11) {
      brave_u++;
    }
  }
  if (euro_u == 0 || brave_u == 0) {
    fprintf(stderr, "%s: euro_u=%d brave_u=%d\n", tag, euro_u, brave_u);
    return 0;
  }
  (void)s->nation[0].indian_hostility_sticky;
  (void)s->nation[0].relation_by_indian[0];
  (void)s->nation[0].euro_relation[1];
  return 1;
}

/*
 * Pair compare MID01→MID02 using the same joint surface as golden_ai_turns:
 * calendar moved, Euro+Brave units remain, tribes/colonies present, diplo rows
 * readable. Exact unit XY golden is Linux-derived (regenerated each run).
 */
static int compare_mid_pair(const ColonizeCol1Save* a, const ColonizeCol1Save* b) {
  if (!assert_joint_fields(a, "MID01") || !assert_joint_fields(b, "MID02")) {
    return 0;
  }
  if (b->head.turn <= a->head.turn && b->head.year <= a->head.year &&
      b->head.autumn == a->head.autumn) {
    fprintf(
      stderr,
      "MID01→MID02 calendar did not advance (a %u/%u/%u b %u/%u/%u)\n",
      a->head.year,
      a->head.autumn,
      a->head.turn,
      b->head.year,
      b->head.autumn,
      b->head.turn
    );
    return 0;
  }
  if (a->nation[0].indian_hostility_sticky < 2 || b->nation[0].indian_hostility_sticky < 1) {
    fprintf(stderr, "MID pair sticky lost (a=%u b=%u)\n",
            (unsigned)a->nation[0].indian_hostility_sticky,
            (unsigned)b->nation[0].indian_hostility_sticky);
    return 0;
  }
  /* Tribe count stable-ish (growth may add; never wipe). */
  if (b->head.tribe_count == 0) {
    fprintf(stderr, "MID02 lost tribes\n");
    return 0;
  }
  if (b->head.colony_count == 0 && a->head.colony_count > 0) {
    fprintf(stderr, "MID02 wiped colonies\n");
    return 0;
  }
  return 1;
}

static int write_mid01_from_turn7(void) {
  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(TURN7_PATH, &save, err, sizeof(err))) {
    fprintf(stderr, "read TURN7: %s\n", err);
    return fail("load TURN7.SAV");
  }

  for (int e = 0; e < 4; ++e) {
    save.nation[e].indian_hostility_sticky = 2;
    for (int i = 0; i < 8; ++i) {
      if (save.nation[e].relation_by_indian[i] == 0) {
        continue;
      }
      if (save.nation[e].relation_by_indian[i] > 40) {
        save.nation[e].relation_by_indian[i] = 35;
      }
    }
  }
  for (int n = 0; n < 8; ++n) {
    ColonizeCol1Indian* ind = &save.indian[n];
    for (int e = 0; e < 4; ++e) {
      if (ind->euro_diplo[e]) {
        ind->alarm_by_player[e] = 60;
      }
    }
  }
  if (save.head.year < 1500) {
    save.head.year = 1505;
  }

  if (!col1_save_write_file(MID01_PATH, &save, err, sizeof(err))) {
    fprintf(stderr, "write MID01: %s\n", err);
    col1_save_free(&save);
    return fail("write MID01.SAV");
  }
  col1_save_free(&save);
  return 0;
}

static int run_mid01_to_mid02(void) {
  char err[256];
  ColonizeCol1Save mid01;
  col1_save_init(&mid01);
  if (!col1_save_read_file(MID01_PATH, &mid01, err, sizeof(err))) {
    fprintf(stderr, "read MID01: %s\n", err);
    return fail("load MID01 for turn");
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    col1_save_free(&mid01);
    return fail("NAMES.TXT load");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    assets_msg_free(&names);
    col1_save_free(&mid01);
    return fail("units_load_types");
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    assets_msg_free(&names);
    col1_save_free(&mid01);
    return fail("colonies_load_buildings");
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&mid01, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge apply MID01: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&mid01);
    return fail("bridge apply MID01");
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, AI_MID_VR_SEED);

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
  ctx.col1 = &mid01;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.rng_seed = AI_MID_VR_SEED;

  turn_end(&ctx);

  if (!col1_bridge_capture(
        &mid01,
        &map,
        &units,
        &colonies,
        &europe,
        year,
        autumn,
        turn_number,
        br.human_nation,
        br.cursor_x,
        br.cursor_y,
        units.selected_id,
        err,
        sizeof(err)
      )) {
    fprintf(stderr, "bridge capture MID02: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&mid01);
    return fail("capture MID02");
  }

  if (!col1_save_write_file(MID02_PATH, &mid01, err, sizeof(err))) {
    fprintf(stderr, "write MID02: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&mid01);
    return fail("write MID02.SAV");
  }

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&mid01);
  return 0;
}

int main(void) {
  char err[256];

  if (write_mid01_from_turn7() != 0) {
    return 1;
  }

  ColonizeCol1Save mid;
  col1_save_init(&mid);
  if (!col1_save_read_file(MID01_PATH, &mid, err, sizeof(err))) {
    fprintf(stderr, "reload MID01: %s\n", err);
    return fail("reload MID01.SAV");
  }
  if (!assert_joint_fields(&mid, "MID01")) {
    col1_save_free(&mid);
    return fail("MID01 joint field snapshot");
  }
  if (mid.head.year < 1500) {
    col1_save_free(&mid);
    return fail("MID01 should be stamped mid-campaign year");
  }
  if (mid.nation[0].indian_hostility_sticky < 2) {
    col1_save_free(&mid);
    return fail("MID01 sticky stamp");
  }
  col1_save_free(&mid);

  if (run_mid01_to_mid02() != 0) {
    return 1;
  }

  ColonizeCol1Save a;
  ColonizeCol1Save b;
  col1_save_init(&a);
  col1_save_init(&b);
  if (!col1_save_read_file(MID01_PATH, &a, err, sizeof(err))) {
    return fail("pair reload MID01");
  }
  if (!col1_save_read_file(MID02_PATH, &b, err, sizeof(err))) {
    col1_save_free(&a);
    return fail("pair reload MID02");
  }
  if (!compare_mid_pair(&a, &b)) {
    col1_save_free(&a);
    col1_save_free(&b);
    return fail("MID01→MID02 joint pair compare");
  }
  col1_save_free(&a);
  col1_save_free(&b);

  printf(
    "golden_ai_mid01: ok (MID01 stamp + MID02 turn + pair compare; %s %s)\n",
    MID01_PATH,
    MID02_PATH
  );
  return 0;
}
