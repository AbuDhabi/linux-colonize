/*
 * Joint late-war golden (T3 Series K).
 * LATE01: load MID02, stamp late-war Indian×Euro fields, write LATE01.SAV,
 * run one full joint turn, then structural pair compare (not TURN XY field-diff).
 * Mutation signals: raid-side (stock/pop/attacks / hotter relation_by_indian /
 * friction) and hunt-side (≥1 Euro military with useful goto / Brave spent).
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

#include <stdio.h>
#include <string.h>

#define MID02_PATH "test-saves-ai/MID02.SAV"
#define LATE01_PATH "test-saves-ai/LATE01.SAV"
#define LATE01_POST_PATH "test-saves-ai/LATE01_POST.SAV"
#define AI_LATE_VR_SEED 100u

static int fail(const char* msg) {
  fprintf(stderr, "smoke_ai_late01: FAIL %s\n", msg);
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
  if (s->head.colony_count == 0) {
    fprintf(stderr, "%s: no colonies\n", tag);
    return 0;
  }
  (void)s->nation[0].indian_hostility_sticky;
  (void)s->nation[0].relation_by_indian[0];
  (void)s->nation[0].euro_relation[1];
  return 1;
}

static int write_late01_from_mid02(void) {
  char err[256];
  ColonizeCol1Save save;
  col1_save_init(&save);
  if (!col1_save_read_file(MID02_PATH, &save, err, sizeof(err))) {
    fprintf(stderr, "read MID02: %s\n", err);
    return fail("load MID02.SAV (run smoke_ai_mid01 first)");
  }

  /* Late-war stamp: calendar + sticky + alarm; keep Linux-derived geometry. */
  if (save.head.year < 1550) {
    save.head.year = 1550;
  }
  for (int e = 0; e < 4; ++e) {
    if (save.nation[e].indian_hostility_sticky < 2) {
      save.nation[e].indian_hostility_sticky = 2;
    }
    for (int i = 0; i < 8; ++i) {
      if (save.nation[e].relation_by_indian[i] == 0) {
        continue;
      }
      /* Hotter late hostility scalar (still readable joint row). */
      if (save.nation[e].relation_by_indian[i] > 35) {
        save.nation[e].relation_by_indian[i] = 30;
      }
    }
  }
  for (int n = 0; n < 8; ++n) {
    ColonizeCol1Indian* ind = &save.indian[n];
    for (int e = 0; e < 4; ++e) {
      if (ind->euro_diplo[e]) {
        if (ind->alarm_by_player[e] < 60) {
          ind->alarm_by_player[e] = 60;
        }
      }
    }
  }
  if (save.tribe) {
    for (uint16_t ti = 0; ti < save.head.tribe_count; ++ti) {
      ColonizeCol1Tribe* t = &save.tribe[ti];
      for (int e = 0; e < 4; ++e) {
        if (t->alarm[e].friction < 55) {
          t->alarm[e].friction = 55;
        }
      }
    }
  }

  if (!col1_save_write_file(LATE01_PATH, &save, err, sizeof(err))) {
    fprintf(stderr, "write LATE01: %s\n", err);
    col1_save_free(&save);
    return fail("write LATE01.SAV");
  }
  col1_save_free(&save);
  return 0;
}

static int run_late01_turn(ColonizeCol1Save* pre_snap) {
  char err[256];
  ColonizeCol1Save late;
  col1_save_init(&late);
  if (!col1_save_read_file(LATE01_PATH, &late, err, sizeof(err))) {
    fprintf(stderr, "read LATE01: %s\n", err);
    return fail("load LATE01 for turn");
  }
  if (pre_snap) {
    /* Shallow snapshot of mutation baselines before turn (after free of late). */
    *pre_snap = late;
    /* Deep-copy pointers we will compare — tribes/units owned by late until free.
     * Instead, capture scalar baselines now. */
  }

  ColonizeMsgCatalog names;
  assets_msg_init(&names);
  if (!assets_msg_load_file(&names, "COLONIZE/NAMES.TXT")) {
    col1_save_free(&late);
    return fail("NAMES.TXT load");
  }

  ColonizeUnitPool units;
  units_reset(&units);
  if (!units_load_types(&units, &names)) {
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("units_load_types");
  }

  ColonizeColonyPool colonies;
  colonies_init(&colonies);
  if (!colonies_load_buildings(&colonies, &names)) {
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("colonies_load_buildings");
  }
  (void)colonies_load_names(&colonies, "COLONIZE/COLONY.TXT");

  ColonizeWorldMap map;
  memset(&map, 0, sizeof(map));
  EuropeScreen europe;
  memset(&europe, 0, sizeof(europe));
  europe.cargo_count = 16;
  ColonizeCol1BridgeResult br;
  if (!col1_bridge_apply(&late, &map, &units, &colonies, &europe, &br, err, sizeof(err))) {
    fprintf(stderr, "bridge apply LATE01: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("bridge apply LATE01");
  }

  /* Baseline mutation signals from live pools (pre-turn). */
  int pre_food = 0;
  int pre_pop = 0;
  int pre_attacks = 0;
  int pre_friction = 0;
  int pre_rel = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies.colonies[i];
    if (!c->active) {
      continue;
    }
    pre_food += c->stock[COLONIZE_CARGO_FOOD];
    pre_pop += c->population;
  }
  if (late.tribe) {
    for (uint16_t ti = 0; ti < late.head.tribe_count; ++ti) {
      for (int e = 0; e < 4; ++e) {
        pre_attacks += (int)late.tribe[ti].alarm[e].attacks;
        pre_friction += (int)late.tribe[ti].alarm[e].friction;
      }
    }
  }
  for (int e = 0; e < 4; ++e) {
    for (int i = 0; i < 8; ++i) {
      pre_rel += (int)late.nation[e].relation_by_indian[i];
    }
  }

  uint32_t turn_number = br.turn_number;
  uint16_t year = br.year;
  uint16_t autumn = br.autumn;
  ColonizeDosRng rng;
  dos_rng_seed(&rng, AI_LATE_VR_SEED);

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
  ctx.col1 = &late;
  ctx.col1_ok = true;
  ctx.rng = &rng;
  ctx.rng_seed = AI_LATE_VR_SEED;

  const uint16_t year0 = year;
  const uint16_t autumn0 = autumn;
  const uint32_t turn0 = turn_number;

  turn_end(&ctx);

  /* Hunt-side: Euro military with useful goto, or Brave spent/moves used. */
  int hunt_signal = 0;
  int brave_spent = 0;
  for (int i = 0; i < COLONIZE_UNITS_MAX; ++i) {
    const ColonizeUnit* u = &units.units[i];
    if (!u->active) {
      continue;
    }
    if (u->nation_id >= 0 && u->nation_id <= 3) {
      const char* nm = units_display_name(&units, u);
      const int mil =
        nm &&
        (strstr(nm, "Soldier") || strstr(nm, "Dragoon") || strstr(nm, "Regular") ||
         strstr(nm, "Continental") || strstr(nm, "Scout"));
      if (mil && (u->orders == UNITS_ORDER_GOTO ||
          (u->goto_x != UNITS_GOTO_NONE && u->goto_y != UNITS_GOTO_NONE))) {
        hunt_signal = 1;
      }
    } else if (u->nation_id >= 4 && u->nation_id <= 11) {
      if (u->moves_left <= 0 || u->orders != UNITS_ORDER_NONE) {
        brave_spent = 1;
      }
    }
  }
  if (!hunt_signal) {
    hunt_signal = brave_spent;
  }

  /* Raid-side: stock/pop/attacks/friction/relation mutation vs pre. */
  int post_food = 0;
  int post_pop = 0;
  int post_attacks = 0;
  int post_friction = 0;
  int post_rel = 0;
  for (int i = 0; i < COLONIZE_COLONIES_MAX; ++i) {
    const ColonizeColony* c = &colonies.colonies[i];
    if (!c->active) {
      continue;
    }
    post_food += c->stock[COLONIZE_CARGO_FOOD];
    post_pop += c->population;
  }
  if (late.tribe) {
    for (uint16_t ti = 0; ti < late.head.tribe_count; ++ti) {
      for (int e = 0; e < 4; ++e) {
        post_attacks += (int)late.tribe[ti].alarm[e].attacks;
        post_friction += (int)late.tribe[ti].alarm[e].friction;
      }
    }
  }
  for (int e = 0; e < 4; ++e) {
    for (int i = 0; i < 8; ++i) {
      post_rel += (int)late.nation[e].relation_by_indian[i];
    }
  }
  const int raid_signal =
    (post_food != pre_food) || (post_pop != pre_pop) || (post_attacks != pre_attacks) ||
    (post_friction != pre_friction) || (post_rel != pre_rel);

  if (!col1_bridge_capture(
        &late,
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
    fprintf(stderr, "bridge capture LATE01 post: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("capture LATE01 post");
  }

  if (!col1_save_write_file(LATE01_POST_PATH, &late, err, sizeof(err))) {
    fprintf(stderr, "write LATE01_POST: %s\n", err);
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("write LATE01_POST.SAV");
  }

  if (!(year > year0 || autumn != autumn0 || turn_number > turn0)) {
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("LATE01 calendar did not advance");
  }
  if (late.nation[0].indian_hostility_sticky < 1) {
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("LATE01 sticky cooled unexpectedly");
  }
  if (!raid_signal) {
    fprintf(
      stderr,
      "smoke_ai_late01: raid-side silent food %d→%d pop %d→%d atk %d→%d fr %d→%d rel %d→%d\n",
      pre_food,
      post_food,
      pre_pop,
      post_pop,
      pre_attacks,
      post_attacks,
      pre_friction,
      post_friction,
      pre_rel,
      post_rel
    );
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("LATE01 expected raid-side mutation signal");
  }
  if (!hunt_signal) {
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("LATE01 expected hunt-side mutation signal");
  }
  if (!assert_joint_fields(&late, "LATE01_POST")) {
    map_free(&map);
    assets_msg_free(&names);
    col1_save_free(&late);
    return fail("LATE01_POST joint fields");
  }

  map_free(&map);
  assets_msg_free(&names);
  col1_save_free(&late);
  (void)pre_snap;
  return 0;
}

int main(void) {
  char err[256];

  if (write_late01_from_mid02() != 0) {
    return 1;
  }

  ColonizeCol1Save late;
  col1_save_init(&late);
  if (!col1_save_read_file(LATE01_PATH, &late, err, sizeof(err))) {
    fprintf(stderr, "reload LATE01: %s\n", err);
    return fail("reload LATE01.SAV");
  }
  if (!assert_joint_fields(&late, "LATE01")) {
    col1_save_free(&late);
    return fail("LATE01 joint field snapshot");
  }
  if (late.head.year < 1550) {
    col1_save_free(&late);
    return fail("LATE01 should be stamped late-war year≥1550");
  }
  if (late.nation[0].indian_hostility_sticky < 2) {
    col1_save_free(&late);
    return fail("LATE01 sticky stamp");
  }
  col1_save_free(&late);

  if (run_late01_turn(NULL) != 0) {
    return 1;
  }

  printf(
    "smoke_ai_late01: ok (LATE01 stamp + turn + structural raid/hunt; %s)\n",
    LATE01_PATH
  );
  return 0;
}
